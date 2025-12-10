// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Services/SuspenseCoreEquipmentNetworkService.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceLocator.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentSecurityService.h"
#include "SuspenseCore/Services/SuspenseCoreServiceProvider.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "SuspenseCore/Components/Network/SuspenseCoreEquipmentNetworkDispatcher.h"
#include "SuspenseCore/Components/Network/SuspenseCoreEquipmentPredictionSystem.h"
#include "SuspenseCore/Components/Network/SuspenseCoreEquipmentReplicationManager.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentDataProvider.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentOperations.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentService.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreNetworkInterfaces.h"
#include "HAL/PlatformProcess.h"
#include "Engine/World.h"
#include "Engine/NetConnection.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceLocator.h"
#include "HAL/PlatformTime.h"
#include "Misc/SecureHash.h"
#include "Misc/Paths.h"

USuspenseCoreEquipmentNetworkService::USuspenseCoreEquipmentNetworkService()
    : ServiceState(ESuspenseCoreServiceLifecycleState::Uninitialized)
    , AverageLatency(0.0f)
    , TotalOperationsSent(0)
    , TotalOperationsRejected(0)
    , NetworkQualityLevel(1.0f)
{
    // SecurityService will be created in InitializeService
}

USuspenseCoreEquipmentNetworkService::~USuspenseCoreEquipmentNetworkService()
{
    // Вызываем невиртуальный метод cleanup
    // bForce=true, bFromDestructor=true для минимальной очистки
    InternalShutdown(true, true);
}

bool USuspenseCoreEquipmentNetworkService::ShutdownService(bool bForce)
{
    // Вызываем невиртуальный метод cleanup
    // bFromDestructor=false - нормальный shutdown с метриками
    InternalShutdown(bForce, false);
    return true;
}

void USuspenseCoreEquipmentNetworkService::InternalShutdown(bool bForce, bool bFromDestructor)
{
    // КРИТИЧНО: минимальная очистка при engine exit или деструкторе
    if (IsEngineExitRequested() || bFromDestructor)
    {
        // Только обнуляем состояние, никакого I/O
        ServiceState = ESuspenseCoreServiceLifecycleState::Shutdown;

        // Delegate security cleanup to SecurityService
        if (SecurityService && IsValid(SecurityService))
        {
            SecurityService->ShutdownService(true);
        }

        // Очищаем интерфейсы
        if (NetworkDispatcher.GetInterface())
        {
            NetworkDispatcher.SetObject(nullptr);
            NetworkDispatcher.SetInterface(nullptr);
        }
        if (PredictionManager.GetInterface())
        {
            PredictionManager.SetObject(nullptr);
            PredictionManager.SetInterface(nullptr);
        }
        ReplicationProvider = nullptr;
        SecurityService = nullptr;

        return;
    }

    // Защита от повторного вызова
    if (ServiceState == ESuspenseCoreServiceLifecycleState::Shutdown)
    {
        return;
    }

    ServiceState = ESuspenseCoreServiceLifecycleState::Shutting;

    // === TEARDOWN EVENTBUS SUBSCRIPTIONS ===
    TeardownEventSubscriptions();

    // === БЕЗОПАСНЫЙ ЭКСПОРТ МЕТРИК ===
    // Только если не force и не из деструктора
    if (!bForce && GetWorld())
    {
        ExportSecurityMetrics(FPaths::ProjectLogDir() / TEXT("NetworkSecurity_FinalMetrics.csv"));
        ExportMetricsToCSV(FPaths::ProjectLogDir() / TEXT("NetworkService_FinalMetrics.csv"));
    }

    // === БЕЗОПАСНАЯ ОЧИСТКА ТАЙМЕРОВ ===
    UWorld* World = GetWorld();
    if (World && IsValid(World))
    {
        FTimerManager& TimerManager = World->GetTimerManager();

        if (MetricsUpdateTimer.IsValid())
        {
            TimerManager.ClearTimer(MetricsUpdateTimer);
        }
    }

    // === БЕЗОПАСНАЯ ОТПИСКА ОТ DELEGATES ===
    if (NetworkDispatcher.GetInterface())
    {
        USuspenseCoreEquipmentNetworkDispatcher* Dispatcher =
            Cast<USuspenseCoreEquipmentNetworkDispatcher>(NetworkDispatcher.GetObject());

        if (Dispatcher && IsValid(Dispatcher))
        {
            if (DispatcherSuccessHandle.IsValid())
            {
                Dispatcher->OnOperationSuccess.Remove(DispatcherSuccessHandle);
                DispatcherSuccessHandle.Reset();
            }
            if (DispatcherFailureHandle.IsValid())
            {
                Dispatcher->OnOperationFailure.Remove(DispatcherFailureHandle);
                DispatcherFailureHandle.Reset();
            }
            if (DispatcherTimeoutHandle.IsValid())
            {
                Dispatcher->OnOperationTimeout.Remove(DispatcherTimeoutHandle);
                DispatcherTimeoutHandle.Reset();
            }
        }
    }

    // === SHUTDOWN SECURITY SERVICE ===
    if (SecurityService && IsValid(SecurityService))
    {
        SecurityService->ShutdownService(bForce);
    }

    // === БЕЗОПАСНАЯ ОЧИСТКА ИНТЕРФЕЙСОВ ===
    if (NetworkDispatcher.GetInterface())
    {
        NetworkDispatcher.SetObject(nullptr);
        NetworkDispatcher.SetInterface(nullptr);
    }

    if (PredictionManager.GetInterface())
    {
        PredictionManager.SetObject(nullptr);
        PredictionManager.SetInterface(nullptr);
    }

    ReplicationProvider = nullptr;
    SecurityService = nullptr;

    ServiceState = ESuspenseCoreServiceLifecycleState::Shutdown;

    UE_LOG(LogSuspenseCoreEquipmentNetwork, Log, TEXT("Equipment Network Service shutdown complete"));
}

bool USuspenseCoreEquipmentNetworkService::ResolveDependencies(
    UWorld* World,
    TScriptInterface<ISuspenseCoreEquipmentDataProvider>& OutDataProvider,
    TScriptInterface<ISuspenseCoreEquipmentOperations>& OutOperationExecutor)
{
    if (!World)
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("ResolveDependencies: invalid World"));
        return false;
    }

    USuspenseCoreEquipmentServiceLocator* ServiceLocator = USuspenseCoreEquipmentServiceLocator::Get(World);
    if (!ServiceLocator)
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("ResolveDependencies: ServiceLocator not available"));
        return false;
    }

    // Resolve Data Provider
    {
        const FGameplayTag DataServiceTag = FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Data"));
        UObject* ServiceObj = ServiceLocator->GetService(DataServiceTag);

        // NB: нижеследующие интерфейсы приведены как пример; используем безопасные проверки
        if (ServiceObj && ServiceObj->GetClass()->ImplementsInterface(USuspenseCoreEquipmentDataServiceInterface::StaticClass()))
        {
            ISuspenseCoreEquipmentDataServiceInterface* DataService = Cast<ISuspenseCoreEquipmentDataServiceInterface>(ServiceObj);
            if (DataService)
            {
                ISuspenseCoreEquipmentDataProvider* Provider = DataService->GetDataProvider();
                if (Provider)
                {
                    if (UObject* ProviderObj = Cast<UObject>(Provider))
                    {
                        OutDataProvider.SetObject(ProviderObj);
                        OutDataProvider.SetInterface(Provider);
                    }
                }
            }
        }

        if (!OutDataProvider.GetInterface())
        {
            UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("ResolveDependencies: Failed to resolve DataProvider"));
            return false;
        }
    }

    // Resolve Operation Executor
    {
        const FGameplayTag OpServiceTag = FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Operations"));
        UObject* ServiceObj = ServiceLocator->GetService(OpServiceTag);

        if (ServiceObj && ServiceObj->GetClass()->ImplementsInterface(USuspenseCoreEquipmentOperationServiceInterface::StaticClass()))
        {
            ISuspenseCoreEquipmentOperationServiceInterface* OpService = Cast<ISuspenseCoreEquipmentOperationServiceInterface>(ServiceObj);
            if (OpService)
            {
                ISuspenseCoreEquipmentOperations* Executor = OpService->GetOperationsExecutor();
                if (Executor)
                {
                    if (UObject* ExecutorObj = Cast<UObject>(Executor))
                    {
                        OutOperationExecutor.SetObject(ExecutorObj);
                        OutOperationExecutor.SetInterface(Executor);
                    }
                }
            }
        }

        if (!OutOperationExecutor.GetInterface())
        {
            UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("ResolveDependencies: Failed to resolve OperationExecutor"));
            return false;
        }
    }

    return true;
}

USuspenseCoreEquipmentNetworkDispatcher* USuspenseCoreEquipmentNetworkService::CreateAndInitNetworkDispatcher(
    AActor* OwnerActor,
    const TScriptInterface<ISuspenseCoreEquipmentOperations>& OperationExecutor)
{
    if (!OwnerActor)
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("CreateAndInitNetworkDispatcher: OwnerActor is null"));
        return nullptr;
    }

    USuspenseCoreEquipmentNetworkDispatcher* Dispatcher = NewObject<USuspenseCoreEquipmentNetworkDispatcher>(
        OwnerActor,
        USuspenseCoreEquipmentNetworkDispatcher::StaticClass(),
        TEXT("EquipmentNetworkDispatcher"));

    if (!Dispatcher)
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("Failed to create EquipmentNetworkDispatcher"));
        return nullptr;
    }

    Dispatcher->RegisterComponent();
    Dispatcher->SetOperationExecutor(OperationExecutor);
    Dispatcher->ConfigureRetryPolicy(3, 0.5f, 2.0f, 0.5f);
    Dispatcher->ConfigureBatching(10, 0.1f);
    Dispatcher->SetOperationTimeout(2.0f);
    Dispatcher->SetSecurityService(this);

    UE_LOG(LogSuspenseCoreEquipmentNetwork, Log, TEXT("NetworkDispatcher created & configured"));

    // ВАЖНО: храним как TScriptInterface, без TSharedPtr
    NetworkDispatcher.SetObject(Dispatcher);
    NetworkDispatcher.SetInterface(Cast<ISuspenseCoreNetworkDispatcher>(Dispatcher));

    return Dispatcher;
}

USuspenseCoreEquipmentPredictionSystem* USuspenseCoreEquipmentNetworkService::CreateAndInitPredictionSystem(
    AActor* OwnerActor,
    const TScriptInterface<ISuspenseCoreEquipmentDataProvider>& DataProvider,
    const TScriptInterface<ISuspenseCoreEquipmentOperations>& OperationExecutor)
{
    if (!OwnerActor)
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("CreateAndInitPredictionSystem: OwnerActor is null"));
        return nullptr;
    }

    USuspenseCoreEquipmentPredictionSystem* Prediction = NewObject<USuspenseCoreEquipmentPredictionSystem>(
        OwnerActor,
        USuspenseCoreEquipmentPredictionSystem::StaticClass(),
        TEXT("EquipmentPredictionSystem"));

    if (!Prediction)
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("Failed to create EquipmentPredictionSystem"));
        return nullptr;
    }

    Prediction->RegisterComponent();

    if (!Prediction->Initialize(DataProvider, OperationExecutor))
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("PredictionSystem Initialize failed"));
        Prediction->DestroyComponent();
        return nullptr;
    }

    Prediction->SetMaxActivePredictions(10);
    Prediction->SetPredictionTimeout(2.0f);
    Prediction->SetPredictionEnabled(true);

    UE_LOG(LogSuspenseCoreEquipmentNetwork, Log, TEXT("PredictionSystem created & initialized"));

    // НЕ TSharedPtr — используем TScriptInterface (GC-safe)
    PredictionManager.SetObject(Prediction);
    PredictionManager.SetInterface(Cast<ISuspenseCorePredictionManager>(Prediction));

    return Prediction;
}

USuspenseCoreEquipmentReplicationManager* USuspenseCoreEquipmentNetworkService::CreateAndInitReplicationManager(
    AActor* OwnerActor,
    const TScriptInterface<ISuspenseCoreEquipmentDataProvider>& DataProvider)
{
    if (!OwnerActor)
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("CreateAndInitReplicationManager: OwnerActor is null"));
        return nullptr;
    }

    USuspenseCoreEquipmentReplicationManager* Replication = NewObject<USuspenseCoreEquipmentReplicationManager>(
        OwnerActor,
        USuspenseCoreEquipmentReplicationManager::StaticClass(),
        TEXT("EquipmentReplicationManager"));

    if (!Replication)
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("Failed to create EquipmentReplicationManager"));
        return nullptr;
    }

    Replication->RegisterComponent();

    if (!Replication->Initialize(DataProvider))
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("ReplicationManager Initialize failed"));
        return nullptr;
    }

    // Base parameters
    Replication->SetUpdateRate(10.0f);
    Replication->SetRelevancyDistance(5000.0f);
    Replication->SetCompressionEnabled(true);
    Replication->SetReplicationPolicy(ESuspenseCoreReplicationPolicy::OnlyToRelevant);

    UE_LOG(LogSuspenseCoreEquipmentNetwork, Log, TEXT("ReplicationManager created & initialized"));

    // Храним как raw UObject компонент
    ReplicationProvider = Replication;

    return Replication;
}

void USuspenseCoreEquipmentNetworkService::BindDispatcherToPrediction(
    USuspenseCoreEquipmentNetworkDispatcher* Dispatcher,
    ISuspenseCorePredictionManager* Prediction)
{
    if (!Dispatcher || !Prediction)
    {
        return;
    }

    DispatcherSuccessHandle = Dispatcher->OnOperationSuccess.AddLambda(
        [this](const FGuid& OperationId, const FEquipmentOperationResult& Result)
        {
            if (PredictionManager.GetInterface())
            {
                PredictionManager->ConfirmPrediction(OperationId, Result);
            }
        });

    DispatcherFailureHandle = Dispatcher->OnOperationFailure.AddLambda(
        [this](const FGuid& OperationId, const FText& Reason)
        {
            if (PredictionManager.GetInterface())
            {
                PredictionManager->RollbackPrediction(OperationId, Reason);
            }
        });

    DispatcherTimeoutHandle = Dispatcher->OnOperationTimeout.AddLambda(
        [this](const FGuid& OperationId)
        {
            if (PredictionManager.GetInterface())
            {
                PredictionManager->RollbackPrediction(OperationId, FText::FromString(TEXT("Timeout")));
            }
        });
}

void USuspenseCoreEquipmentNetworkService::StartMonitoringTimers(UWorld* World)
{
    if (!World) return;

    // Network metrics update timer - security timers are handled by SecurityService
    World->GetTimerManager().SetTimer(
        MetricsUpdateTimer,
        this,
        &USuspenseCoreEquipmentNetworkService::UpdateNetworkMetrics,
        1.0f, true);
}

bool USuspenseCoreEquipmentNetworkService::InitializeService(const FSuspenseCoreServiceInitParams& Params)
{
    SCOPED_SERVICE_TIMER("InitializeService");

    if (ServiceState != ESuspenseCoreServiceLifecycleState::Uninitialized)
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Warning, TEXT("Service already initialized"));
        ServiceMetrics.RecordError();
        RECORD_SERVICE_METRIC("initialization_already_initialized", 1);
        return false;
    }

    ServiceState  = ESuspenseCoreServiceLifecycleState::Initializing;
    ServiceParams = Params;

    // Create and initialize SecurityService (delegated security responsibility)
    SecurityService = NewObject<USuspenseCoreEquipmentSecurityService>(this);
    if (!SecurityService)
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("Failed to create SecurityService"));
        ServiceState = ESuspenseCoreServiceLifecycleState::Failed;
        ServiceMetrics.RecordError();
        return false;
    }

    if (!SecurityService->InitializeService(Params))
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("Failed to initialize SecurityService"));
        ServiceState = ESuspenseCoreServiceLifecycleState::Failed;
        ServiceMetrics.RecordError();
        RECORD_SERVICE_METRIC("security_service_init_failed", 1);
        return false;
    }
    RECORD_SERVICE_METRIC("security_service_initialized", 1);

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("No valid world context for network service initialization"));
        ServiceState = ESuspenseCoreServiceLifecycleState::Failed;
        ServiceMetrics.RecordError();
        return false;
    }

    AActor* OwnerActor = Cast<AActor>(ServiceParams.Owner);
    if (!OwnerActor)
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("Service owner is not an Actor, cannot create network components"));
        ServiceState = ESuspenseCoreServiceLifecycleState::Failed;
        ServiceMetrics.RecordError();
        return false;
    }

    TScriptInterface<ISuspenseCoreEquipmentDataProvider> DataProvider;
    TScriptInterface<ISuspenseCoreEquipmentOperations>  OperationExecutor;
    if (!ResolveDependencies(World, DataProvider, OperationExecutor))
    {
        ServiceState = ESuspenseCoreServiceLifecycleState::Failed;
        ServiceMetrics.RecordError();
        return false;
    }

    USuspenseCoreEquipmentNetworkDispatcher* Dispatcher =
        CreateAndInitNetworkDispatcher(OwnerActor, OperationExecutor);
    if (!Dispatcher)
    {
        ServiceState = ESuspenseCoreServiceLifecycleState::Failed;
        ServiceMetrics.RecordError();
        return false;
    }
    RECORD_SERVICE_METRIC("network_dispatcher_created", 1);

    USuspenseCoreEquipmentPredictionSystem* Prediction =
        CreateAndInitPredictionSystem(OwnerActor, DataProvider, OperationExecutor);
    if (!Prediction)
    {
        ServiceState = ESuspenseCoreServiceLifecycleState::Failed;
        ServiceMetrics.RecordError();
        return false;
    }
    RECORD_SERVICE_METRIC("prediction_manager_created", 1);

    USuspenseCoreEquipmentReplicationManager* Replication =
        CreateAndInitReplicationManager(OwnerActor, DataProvider);
    if (!Replication)
    {
        ServiceState = ESuspenseCoreServiceLifecycleState::Failed;
        ServiceMetrics.RecordError();
        return false;
    }
    RECORD_SERVICE_METRIC("replication_provider_created", 1);

    if (World->GetNetMode() != NM_Client)
    {
        BindDispatcherToPrediction(Dispatcher, PredictionManager.GetInterface());
    }

    float InitialUpdateRate = 10.0f;
    if (AverageLatency > 0.0f)
    {
        if      (AverageLatency <  50.0f) InitialUpdateRate = 20.0f;
        else if (AverageLatency < 100.0f) InitialUpdateRate = 15.0f;
        else if (AverageLatency < 150.0f) InitialUpdateRate = 10.0f;
        else                              InitialUpdateRate =  5.0f;
    }
    Replication->SetUpdateRate(InitialUpdateRate);

    StartMonitoringTimers(World);
    RECORD_SERVICE_METRIC("timers_started", 3);

    // Setup EventBus subscriptions for inter-service communication
    SetupEventSubscriptions();
    RECORD_SERVICE_METRIC("eventbus_subscriptions", EventSubscriptions.Num());

    ServiceState = ESuspenseCoreServiceLifecycleState::Ready;
    ServiceMetrics.RecordSuccess();
    RECORD_SERVICE_METRIC("initialization_success", 1);

    const FSecurityServiceConfig& SecurityConfig = SecurityService->GetConfiguration();
    UE_LOG(LogSuspenseCoreEquipmentNetwork, Log, TEXT("========================================"));
    UE_LOG(LogSuspenseCoreEquipmentNetwork, Log, TEXT("Equipment Network Service Initialized"));
    UE_LOG(LogSuspenseCoreEquipmentNetwork, Log, TEXT("Security: Strict=%s, HMACCritical=%s, IPRateLimit=%s"),
        SecurityConfig.bEnableStrictSecurity ? TEXT("ON") : TEXT("OFF"),
        SecurityConfig.bRequireHMACForCritical ? TEXT("ON") : TEXT("OFF"),
        SecurityConfig.bEnableIPRateLimit ? TEXT("ON") : TEXT("OFF"));
    UE_LOG(LogSuspenseCoreEquipmentNetwork, Log, TEXT("========================================"));
    return true;
}

FGameplayTag USuspenseCoreEquipmentNetworkService::GetServiceTag() const
{
    FSuspenseCoreScopedServiceTimer __svc_timer(const_cast<FSuspenseCoreServiceMetrics&>(ServiceMetrics),
                                    FName(TEXT("GetServiceTag")));
    return FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Network"));
}

FGameplayTagContainer USuspenseCoreEquipmentNetworkService::GetRequiredDependencies() const
{
    FSuspenseCoreScopedServiceTimer __svc_timer(const_cast<FSuspenseCoreServiceMetrics&>(ServiceMetrics),
                                    FName(TEXT("GetRequiredDependencies")));
    FGameplayTagContainer Dependencies;
    Dependencies.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Data")));
    Dependencies.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Operations")));
    return Dependencies;
}

bool USuspenseCoreEquipmentNetworkService::ValidateService(TArray<FText>& OutErrors) const
{
    FSuspenseCoreScopedServiceTimer __svc_timer(const_cast<FSuspenseCoreServiceMetrics&>(ServiceMetrics),
                                    FName(TEXT("ValidateService")));
    bool bIsValid = true;

    if (ServiceState != ESuspenseCoreServiceLifecycleState::Ready)
    {
        OutErrors.Add(FText::FromString(TEXT("Network Service is not in Ready state")));
        bIsValid = false;
        const_cast<FSuspenseCoreServiceMetrics&>(ServiceMetrics).RecordValue(FName(TEXT("validation_state_error")), 1);
    }

    if (!NetworkDispatcher.GetInterface())
    {
        OutErrors.Add(FText::FromString(TEXT("NetworkDispatcher is not initialized")));
        bIsValid = false;
        const_cast<FSuspenseCoreServiceMetrics&>(ServiceMetrics).RecordValue(FName(TEXT("validation_dispatcher_error")), 1);
    }

    if (!PredictionManager.GetInterface())
    {
        OutErrors.Add(FText::FromString(TEXT("PredictionManager is not initialized")));
        bIsValid = false;
        const_cast<FSuspenseCoreServiceMetrics&>(ServiceMetrics).RecordValue(FName(TEXT("validation_prediction_error")), 1);
    }

    if (!ReplicationProvider)
    {
        OutErrors.Add(FText::FromString(TEXT("ReplicationProvider is not initialized")));
        bIsValid = false;
        const_cast<FSuspenseCoreServiceMetrics&>(ServiceMetrics).RecordValue(FName(TEXT("validation_replication_error")), 1);
    }

    // Delegate security validation to SecurityService
    if (!SecurityService || !SecurityService->IsServiceReady())
    {
        OutErrors.Add(FText::FromString(TEXT("SecurityService is not initialized or ready")));
        bIsValid = false;
        const_cast<FSuspenseCoreServiceMetrics&>(ServiceMetrics).RecordValue(FName(TEXT("validation_security_error")), 1);
    }
    else
    {
        // Validate SecurityService
        TArray<FText> SecurityErrors;
        if (!SecurityService->ValidateService(SecurityErrors))
        {
            OutErrors.Append(SecurityErrors);
            bIsValid = false;
        }
    }

    if (bIsValid)
    {
        const_cast<FSuspenseCoreServiceMetrics&>(ServiceMetrics).RecordSuccess();
    }
    else
    {
        const_cast<FSuspenseCoreServiceMetrics&>(ServiceMetrics).RecordError();
    }

    return bIsValid;
}

void USuspenseCoreEquipmentNetworkService::ResetService()
{
    SCOPED_SERVICE_TIMER("ResetService");

    // Delegate security reset to SecurityService
    if (SecurityService)
    {
        SecurityService->ResetService();
    }

    // Reset network-specific metrics
    AverageLatency          = 0.0f;
    TotalOperationsSent     = 0;
    TotalOperationsRejected = 0;

    ServiceMetrics.Reset();
    ServiceMetrics.RecordSuccess();
    RECORD_SERVICE_METRIC("Network.Service.Reset", 1);

    UE_LOG(LogSuspenseCoreEquipmentNetwork, Log,
        TEXT("EquipmentNetworkService reset complete"));
}

FString USuspenseCoreEquipmentNetworkService::GetServiceStats() const
{
    FSuspenseCoreScopedServiceTimer __svc_timer(const_cast<FSuspenseCoreServiceMetrics&>(ServiceMetrics),
                                    FName(TEXT("GetServiceStats")));

    FString Stats = TEXT("=== Equipment Network Service Statistics ===\n");
    Stats += FString::Printf(TEXT("Service State: %s\n"),
        *UEnum::GetValueAsString(ServiceState));
    Stats += FString::Printf(TEXT("Network Quality: %.2f\n"), NetworkQualityLevel);
    Stats += FString::Printf(TEXT("Average Latency: %.2f ms\n"), AverageLatency);
    Stats += FString::Printf(TEXT("Operations Sent: %d, Rejected: %d\n"),
        TotalOperationsSent, TotalOperationsRejected);

    Stats += TEXT("\n=== Service Performance Metrics ===\n");
    Stats += ServiceMetrics.ToString(TEXT("NetworkService"));

    // Delegate security stats to SecurityService
    if (SecurityService)
    {
        Stats += TEXT("\n=== Security Service Statistics ===\n");
        Stats += SecurityService->GetServiceStats();
    }

    if (NetworkDispatcher.GetInterface())
    {
        Stats += TEXT("\n--- Network Dispatcher ---\n");
        if (USuspenseCoreEquipmentNetworkDispatcher* Dispatcher =
                Cast<USuspenseCoreEquipmentNetworkDispatcher>(NetworkDispatcher.GetObject()))
        {
            Stats += Dispatcher->GetNetworkStatistics();
        }
        else
        {
            Stats += TEXT("Interface active (no component stats available)\n");
        }
    }

    const_cast<FSuspenseCoreServiceMetrics&>(ServiceMetrics).RecordValue(FName(TEXT("stats_retrieved")), 1);

    return Stats;
}

bool USuspenseCoreEquipmentNetworkService::ExportMetricsToCSV(const FString& FilePath) const
{
    if (IsEngineExitRequested())
    {
        return false;
    }

    return ServiceMetrics.ExportToCSV(FilePath, TEXT("NetworkService"));
}

void USuspenseCoreEquipmentNetworkService::ReloadSecurityConfig()
{
    SCOPED_SERVICE_TIMER("ReloadSecurityConfig");

    // Delegate to SecurityService
    if (SecurityService)
    {
        SecurityService->ReloadConfiguration();
    }

    RECORD_SERVICE_METRIC("config_reloaded", 1);
    ServiceMetrics.RecordSuccess();

    UE_LOG(LogSuspenseCoreEquipmentNetwork, Log, TEXT("Security configuration reloaded via SecurityService"));
}

const FSecurityServiceMetrics* USuspenseCoreEquipmentNetworkService::GetSecurityMetrics() const
{
    if (SecurityService)
    {
        return &SecurityService->GetMetrics();
    }
    return nullptr;
}

bool USuspenseCoreEquipmentNetworkService::ExportSecurityMetrics(const FString& FilePath) const
{
    if (IsEngineExitRequested())
    {
        return false;
    }

    // Delegate to SecurityService
    if (SecurityService)
    {
        return SecurityService->ExportMetrics(FilePath);
    }
    return false;
}

FGuid USuspenseCoreEquipmentNetworkService::SendEquipmentOperation(const FEquipmentOperationRequest& Request, APlayerController* PlayerController)
{
    SCOPED_SERVICE_TIMER("SendEquipmentOperation");

    if (!IsServiceReady())
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("Network service not ready"));
        ServiceMetrics.RecordError();
        RECORD_SERVICE_METRIC("send_operation_service_not_ready", 1);
        return FGuid();
    }

    if (!PlayerController)
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("Invalid player controller"));
        ServiceMetrics.RecordError();
        RECORD_SERVICE_METRIC("send_operation_invalid_controller", 1);
        return FGuid();
    }

    if (!SecurityService || !SecurityService->IsServiceReady())
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("SecurityService not ready"));
        ServiceMetrics.RecordError();
        return FGuid();
    }

    // Get player GUID for security validation
    FGuid PlayerGuid;
    if (APlayerState* PS = PlayerController->GetPlayerState<APlayerState>())
    {
        const FUniqueNetIdRepl& UID = PS->GetUniqueId();
        if (UID.IsValid())
        {
            PlayerGuid = CreatePlayerGuid(UID);
        }
    }

    if (!PlayerGuid.IsValid())
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("Failed to generate valid player GUID"));
        ServiceMetrics.RecordError();
        RECORD_SERVICE_METRIC("send_operation_invalid_guid", 1);
        return FGuid();
    }

    // Determine if critical operation
    const bool bCritical = (Request.OperationType == EEquipmentOperationType::Drop) ||
                           (Request.OperationType == EEquipmentOperationType::Transfer) ||
                           (Request.OperationType == EEquipmentOperationType::Upgrade);

    // Generate nonce via SecurityService
    const uint64 Nonce = SecurityService->GenerateNonce();

    // Validate request via SecurityService (rate limiting, nonce, IP checks)
    FSecurityValidationResponse ValidationResult = SecurityService->ValidateRequest(
        PlayerGuid, PlayerController, Nonce, bCritical);

    if (!ValidationResult.IsValid())
    {
        TotalOperationsRejected++;
        ServiceMetrics.RecordError();

        if (ValidationResult.bShouldLogSuspicious)
        {
            SecurityService->ReportSuspiciousActivity(PlayerController, ValidationResult.ErrorMessage);
        }

        UE_LOG(LogSuspenseCoreEquipmentNetwork, Warning, TEXT("Security validation failed: %s"),
            *ValidationResult.ErrorMessage);
        RECORD_SERVICE_METRIC("send_operation_security_rejected", 1);
        return FGuid();
    }

    // Build network request
    FNetworkOperationRequest NetReq;
    NetReq.RequestId = FGuid::NewGuid();
    NetReq.Operation = Request;
    NetReq.Priority = bCritical ? ENetworkOperationPriority::Critical : ENetworkOperationPriority::Normal;
    NetReq.Timestamp = FPlatformTime::Seconds();
    NetReq.ClientTimestamp = NetReq.Timestamp;
    NetReq.Nonce = Nonce;
    NetReq.bRequiresConfirmation = true;

    NetReq.UpdateChecksum();

    // Generate HMAC for critical operations via SecurityService
    const FSecurityServiceConfig& SecurityConfig = SecurityService->GetConfiguration();
    if (bCritical && SecurityConfig.bRequireHMACForCritical)
    {
        NetReq.HMACSignature = SecurityService->GenerateHMAC(NetReq);
        if (NetReq.HMACSignature.IsEmpty())
        {
            UE_LOG(LogSuspenseCoreEquipmentNetwork, Error,
                TEXT("Failed to generate HMAC for critical operation"));
            ServiceMetrics.RecordError();
            return FGuid();
        }
        RECORD_SERVICE_METRIC("hmac_generated", 1);
    }

    // Send via dispatcher
    if (NetworkDispatcher.GetInterface())
    {
        NetworkDispatcher->SendOperationToServer(NetReq);
        RECORD_SERVICE_METRIC("operation_sent_to_dispatcher", 1);
    }

    // Mark nonce as used after successful send
    SecurityService->MarkNonceUsed(Nonce);

    TotalOperationsSent++;
    ServiceMetrics.RecordSuccess();
    RECORD_SERVICE_METRIC("send_operation_success", 1);

    UE_LOG(LogSuspenseCoreEquipmentNetwork, Verbose, TEXT("Sent %s operation %s with nonce %llu"),
        bCritical ? TEXT("CRITICAL") : TEXT("normal"),
        *NetReq.RequestId.ToString(),
        Nonce);

    return NetReq.RequestId;
}

bool USuspenseCoreEquipmentNetworkService::ReceiveEquipmentOperation(const FNetworkOperationRequest& NetworkRequest, APlayerController* SendingPlayer)
{
    SCOPED_SERVICE_TIMER("ReceiveEquipmentOperation");

    // SECURITY: Only server can receive and process network operations
    // This is the authoritative entry point for equipment operations
    UWorld* World = GetWorld();
    if (World && World->GetNetMode() == NM_Client)
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Warning,
            TEXT("ReceiveEquipmentOperation rejected - server authority required"));
        return false;
    }

    if (!IsServiceReady())
    {
        ServiceMetrics.RecordError();
        RECORD_SERVICE_METRIC("receive_operation_service_not_ready", 1);
        return false;
    }

    if (!SecurityService || !SecurityService->IsServiceReady())
    {
        ServiceMetrics.RecordError();
        RECORD_SERVICE_METRIC("receive_operation_security_not_ready", 1);
        return false;
    }

    // Validate packet integrity (checksum)
    if (!NetworkRequest.ValidateIntegrity())
    {
        TotalOperationsRejected++;
        SecurityService->ReportSuspiciousActivity(SendingPlayer, TEXT("Checksum validation failed"));
        ServiceMetrics.RecordError();
        RECORD_SERVICE_METRIC("receive_operation_integrity_failed", 1);
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("Integrity check failed for %s"),
            *NetworkRequest.RequestId.ToString());
        return false;
    }

    // Get player GUID for validation
    FGuid PlayerGuid;
    if (SendingPlayer)
    {
        if (APlayerState* PS = SendingPlayer->GetPlayerState<APlayerState>())
        {
            const FUniqueNetIdRepl& UID = PS->GetUniqueId();
            if (UID.IsValid())
            {
                PlayerGuid = CreatePlayerGuid(UID);
            }
        }
    }

    const bool bCritical = (NetworkRequest.Priority == ENetworkOperationPriority::Critical);

    // Validate via SecurityService (nonce, rate limiting, timing)
    FSecurityValidationResponse ValidationResult = SecurityService->ValidateRequest(
        PlayerGuid, SendingPlayer, NetworkRequest.Nonce, bCritical);

    if (!ValidationResult.IsValid())
    {
        TotalOperationsRejected++;
        ServiceMetrics.RecordError();

        if (ValidationResult.bShouldLogSuspicious)
        {
            SecurityService->ReportSuspiciousActivity(SendingPlayer, ValidationResult.ErrorMessage);
        }

        UE_LOG(LogSuspenseCoreEquipmentNetwork, Warning, TEXT("Receive validation failed: %s"),
            *ValidationResult.ErrorMessage);
        return false;
    }

    // Check packet age
    const FSecurityServiceConfig& SecurityConfig = SecurityService->GetConfiguration();
    const float Now = FPlatformTime::Seconds();
    const float Age = Now - NetworkRequest.ClientTimestamp;
    if (Age > SecurityConfig.PacketAgeLimit)
    {
        SecurityService->ReportSuspiciousActivity(SendingPlayer, TEXT("Stale packet received"));
        ServiceMetrics.RecordError();
        RECORD_SERVICE_METRIC("receive_operation_stale_packet", 1);
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Warning, TEXT("Stale packet rejected age=%.2fs"),
            Age);
        return false;
    }

    // Verify HMAC for critical operations via SecurityService
    if (bCritical && SecurityConfig.bRequireHMACForCritical)
    {
        if (!SecurityService->VerifyHMAC(NetworkRequest))
        {
            SecurityService->ReportSuspiciousActivity(SendingPlayer, TEXT("HMAC verification failed"));
            ServiceMetrics.RecordError();
            RECORD_SERVICE_METRIC("receive_operation_hmac_failed", 1);
            UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("HMAC failed for %s"),
                *NetworkRequest.RequestId.ToString());
            return false;
        }
        RECORD_SERVICE_METRIC("hmac_verified", 1);
    }

    // Mark nonce as confirmed
    SecurityService->MarkNonceUsed(NetworkRequest.Nonce);
    RECORD_SERVICE_METRIC("nonce_confirmed", 1);

    ServiceMetrics.RecordSuccess();
    RECORD_SERVICE_METRIC("receive_operation_success", 1);

    return true;
}

void USuspenseCoreEquipmentNetworkService::SetNetworkQuality(float Quality)
{
    SCOPED_SERVICE_TIMER("SetNetworkQuality");

    NetworkQualityLevel = FMath::Clamp(Quality, 0.0f, 1.0f);
    AdaptNetworkStrategies();

    RECORD_SERVICE_METRIC("network_quality_updated", 1);
    ServiceMetrics.RecordSuccess();

    UE_LOG(LogSuspenseCoreEquipmentNetwork, Log, TEXT("Network quality set to %.2f"), NetworkQualityLevel);
}

FLatencyCompensationData USuspenseCoreEquipmentNetworkService::GetNetworkMetrics() const
{
    FSuspenseCoreScopedServiceTimer __svc_timer(const_cast<FSuspenseCoreServiceMetrics&>(ServiceMetrics),
                                    FName(TEXT("GetNetworkMetrics")));

    FLatencyCompensationData Metrics;
    Metrics.EstimatedLatency = AverageLatency;
    Metrics.ServerTime       = FPlatformTime::Seconds();
    Metrics.ClientTime       = FPlatformTime::Seconds();
    Metrics.TimeDilation     = 1.0f;

    // Calculate packet loss from SecurityService metrics
    if (SecurityService)
    {
        const FSecurityServiceMetrics& SecurityMetrics = SecurityService->GetMetrics();
        uint64 TotalProcessed = SecurityMetrics.TotalRequestsProcessed.Load();
        uint64 TotalRejected  = SecurityMetrics.RequestsRejectedRateLimit.Load() +
                                SecurityMetrics.RequestsRejectedReplay.Load() +
                                SecurityMetrics.RequestsRejectedIntegrity.Load() +
                                SecurityMetrics.RequestsRejectedHMAC.Load() +
                                SecurityMetrics.RequestsRejectedIP.Load();

        if (TotalProcessed > 0)
        {
            float LossRate = (float)TotalRejected / (float)(TotalProcessed + TotalRejected);
            Metrics.PacketLoss = FMath::RoundToInt(LossRate * 100.0f);
        }
    }

    const_cast<FSuspenseCoreServiceMetrics&>(ServiceMetrics).RecordValue(FName(TEXT("metrics_retrieved")), 1);

    return Metrics;
}

void USuspenseCoreEquipmentNetworkService::ForceSynchronization(APlayerController* PlayerController)
{
    SCOPED_SERVICE_TIMER("ForceSynchronization");

    if (!IsServiceReady() || !PlayerController)
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Warning, TEXT("Cannot force sync - service not ready or invalid controller"));
        ServiceMetrics.RecordError();
        RECORD_SERVICE_METRIC("force_sync_failed", 1);
        return;
    }

    if (ReplicationProvider)
    {
        ReplicationProvider->ForceFullReplication();
        RECORD_SERVICE_METRIC("full_replication_forced", 1);

        UE_LOG(LogSuspenseCoreEquipmentNetwork, Log, TEXT("Forced synchronization for %s"),
            *GetNameSafe(PlayerController));
    }

    if (NetworkDispatcher.GetInterface())
    {
        NetworkDispatcher->FlushPendingOperations(true);
        RECORD_SERVICE_METRIC("pending_operations_flushed", 1);
    }

    ServiceMetrics.RecordSuccess();
    RECORD_SERVICE_METRIC("force_sync_success", 1);
}

FGuid USuspenseCoreEquipmentNetworkService::CreatePlayerGuid(const FUniqueNetIdRepl& UniqueId) const
{
    FSuspenseCoreScopedServiceTimer __svc_timer(const_cast<FSuspenseCoreServiceMetrics&>(ServiceMetrics),
                                    FName(TEXT("CreatePlayerGuid")));

    if (!UniqueId.IsValid())
    {
        return FGuid();
    }

    FString UniqueIdString = UniqueId->ToString();
    uint8   HashResult[20];
    FSHA1   Sha1Gen;
    Sha1Gen.Update((const uint8*)TCHAR_TO_UTF8(*UniqueIdString), UniqueIdString.Len());
    FString EntropyString = FString::Printf(TEXT("SuspenseCore_Equipment_%s_Salt"), *UniqueIdString);
    Sha1Gen.Update((const uint8*)TCHAR_TO_UTF8(*EntropyString), EntropyString.Len());
    Sha1Gen.Final();
    Sha1Gen.GetHash(HashResult);

    uint8  SecondPassHash[20];
    FSHA1  SecondPass;
    SecondPass.Update(HashResult, sizeof(HashResult));
    FString SecondSalt = FString::Printf(TEXT("SecondPass_%d"), GetTypeHash(UniqueIdString));
    SecondPass.Update((const uint8*)TCHAR_TO_UTF8(*SecondSalt), SecondSalt.Len());
    SecondPass.Final();
    SecondPass.GetHash(SecondPassHash);

    uint32 GuidComponents[4];
    GuidComponents[0] = (*reinterpret_cast<uint32*>(&HashResult[0]))  ^ (*reinterpret_cast<uint32*>(&SecondPassHash[16]));
    GuidComponents[1] = (*reinterpret_cast<uint32*>(&HashResult[4]))  ^ (*reinterpret_cast<uint32*>(&SecondPassHash[12]));
    GuidComponents[2] = (*reinterpret_cast<uint32*>(&HashResult[8]))  ^ (*reinterpret_cast<uint32*>(&SecondPassHash[8]));
    GuidComponents[3] = (*reinterpret_cast<uint32*>(&HashResult[12])) ^ (*reinterpret_cast<uint32*>(&SecondPassHash[4]));

    const_cast<FSuspenseCoreServiceMetrics&>(ServiceMetrics).RecordValue(FName(TEXT("player_guid_created")), 1);
    return FGuid(GuidComponents[0], GuidComponents[1], GuidComponents[2], GuidComponents[3]);
}

// ============================================================================
// Network Metrics - kept in NetworkService as network-specific logic
// ============================================================================

void USuspenseCoreEquipmentNetworkService::UpdateNetworkMetrics()
{
    SCOPED_SERVICE_TIMER("UpdateNetworkMetrics");

    if (NetworkDispatcher.GetInterface())
    {
        float NewSampleMs = 0.0f;

        if (USuspenseCoreEquipmentNetworkDispatcher* Dispatcher =
                Cast<USuspenseCoreEquipmentNetworkDispatcher>(NetworkDispatcher.GetObject()))
        {
            const FSuspenseCoreNetworkDispatcherStats Stats = Dispatcher->GetStats();
            NewSampleMs = Stats.AverageResponseTime * 1000.0f;
        }
        else
        {
            NewSampleMs = FMath::RandRange(20.0f, 100.0f);
        }

        AverageLatency = (AverageLatency * 0.9f) + (NewSampleMs * 0.1f);
        RECORD_SERVICE_METRIC("latency_sample_ms", (int64)NewSampleMs);

        if (USuspenseCoreEquipmentReplicationManager* ReplicationMgr = ReplicationProvider)
        {
            float NewHz = 10.0f;
            if      (AverageLatency <  50.0f) NewHz = 20.0f;
            else if (AverageLatency < 100.0f) NewHz = 15.0f;
            else if (AverageLatency < 150.0f) NewHz = 10.0f;
            else                               NewHz =  5.0f;

            static float LastHz = 10.0f;
            if (FMath::Abs(NewHz - LastHz) > 2.0f)
            {
                ReplicationMgr->SetUpdateRate(NewHz);
                LastHz = NewHz;

                UE_LOG(LogSuspenseCoreEquipmentNetwork, Verbose,
                    TEXT("Adjusted replication rate to %.1f Hz based on %.1fms average latency"),
                    NewHz, AverageLatency);
            }

            const float NetworkQuality = 1.0f - FMath::Clamp(AverageLatency / 200.0f, 0.0f, 1.0f);
            ReplicationMgr->OnNetworkQualityUpdated(NetworkQuality);
        }
    }
}

void USuspenseCoreEquipmentNetworkService::AdaptNetworkStrategies()
{
    if (NetworkQualityLevel < 0.3f)
    {
        if (PredictionManager.GetInterface())
        {
            PredictionManager->SetPredictionEnabled(true);
        }

        if (ReplicationProvider)
        {
            ReplicationProvider->SetReplicationPolicy(ESuspenseCoreReplicationPolicy::OnlyToOwner);
        }

        RECORD_SERVICE_METRIC("strategy_poor_network", 1);
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Log, TEXT("Adapted to POOR network - optimized for playability"));
    }
    else if (NetworkQualityLevel < 0.7f)
    {
        if (PredictionManager.GetInterface())
        {
            PredictionManager->SetPredictionEnabled(true);
        }

        if (ReplicationProvider)
        {
            ReplicationProvider->SetReplicationPolicy(ESuspenseCoreReplicationPolicy::OnlyToRelevant);
        }

        RECORD_SERVICE_METRIC("strategy_medium_network", 1);
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Log, TEXT("Adapted to MEDIUM network - balanced security"));
    }
    else
    {
        if (PredictionManager.GetInterface())
        {
            PredictionManager->SetPredictionEnabled(false);
        }

        if (ReplicationProvider)
        {
            ReplicationProvider->SetReplicationPolicy(ESuspenseCoreReplicationPolicy::Always);
        }

        RECORD_SERVICE_METRIC("strategy_good_network", 1);
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Log, TEXT("Adapted to GOOD network - maximum security enabled"));
    }
}

// ============================================================================
// EventBus Integration - Phase 4
// ============================================================================

void USuspenseCoreEquipmentNetworkService::SetupEventSubscriptions()
{
    using namespace SuspenseCoreEquipmentTags;

    // Get EventBus from ServiceProvider (SuspenseCore architecture)
    if (USuspenseCoreServiceProvider* Provider = USuspenseCoreServiceProvider::Get(this))
    {
        EventBus = Provider->GetEventBus();
    }

    if (!EventBus)
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Warning,
            TEXT("SetupEventSubscriptions: EventBus not available from ServiceProvider"));
        return;
    }

    // Initialize event tags using native compile-time tags (SuspenseCore.Event.* format)
    Tag_NetworkResult = Event::TAG_Equipment_Event_Network_Result;
    Tag_NetworkTimeout = Event::TAG_Equipment_Event_Network_Timeout;
    Tag_SecurityViolation = Event::TAG_Equipment_Event_Network_SecurityViolation;
    Tag_OperationCompleted = Event::TAG_Equipment_Event_Operation_Completed;

    // Subscribe to operation completed events to track network-related operations
    if (Tag_OperationCompleted.IsValid())
    {
        EventSubscriptions.Add(EventBus->SubscribeNative(
            Tag_OperationCompleted,
            this,
            FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreEquipmentNetworkService::OnOperationCompleted),
            ESuspenseCoreEventPriority::Normal));

        UE_LOG(LogSuspenseCoreEquipmentNetwork, Verbose,
            TEXT("Subscribed to SuspenseCore.Event.Equipment.Operation.Completed events"));
    }

    UE_LOG(LogSuspenseCoreEquipmentNetwork, Log,
        TEXT("EventBus subscriptions setup complete: %d subscriptions"),
        EventSubscriptions.Num());
}

void USuspenseCoreEquipmentNetworkService::TeardownEventSubscriptions()
{
    if (EventBus)
    {
        for (const FSuspenseCoreSubscriptionHandle& Handle : EventSubscriptions)
        {
            EventBus->Unsubscribe(Handle);
        }
    }
    EventSubscriptions.Empty();
    EventBus = nullptr;

    UE_LOG(LogSuspenseCoreEquipmentNetwork, Verbose,
        TEXT("EventBus subscriptions torn down"));
}

void USuspenseCoreEquipmentNetworkService::BroadcastNetworkResult(
    bool bSuccess,
    const FGuid& OperationId,
    const FString& ErrorMessage)
{
    if (!EventBus || !Tag_NetworkResult.IsValid())
    {
        return;
    }

    // Create event data using SuspenseCore types
    FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
    EventData.SetString(FName("Success"), bSuccess ? TEXT("true") : TEXT("false"));
    EventData.SetString(FName("OperationId"), OperationId.ToString());
    if (!bSuccess && !ErrorMessage.IsEmpty())
    {
        EventData.SetString(FName("ErrorMessage"), ErrorMessage);
    }

    EventBus->Publish(Tag_NetworkResult, EventData);

    UE_LOG(LogSuspenseCoreEquipmentNetwork, Verbose,
        TEXT("Broadcast SuspenseCore.Event.Equipment.Network.Result: Success=%s, OpId=%s"),
        bSuccess ? TEXT("true") : TEXT("false"),
        *OperationId.ToString());
}

void USuspenseCoreEquipmentNetworkService::BroadcastSecurityViolation(
    const FString& ViolationType,
    APlayerController* PlayerController,
    const FString& Details)
{
    if (!EventBus || !Tag_SecurityViolation.IsValid())
    {
        return;
    }

    // Create event data using SuspenseCore types
    FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
    EventData.SetObject(FName("Target"), PlayerController);
    EventData.SetString(FName("ViolationType"), ViolationType);
    EventData.SetString(FName("Details"), Details);

    if (PlayerController)
    {
        if (APlayerState* PS = PlayerController->GetPlayerState<APlayerState>())
        {
            EventData.SetString(FName("PlayerName"), PS->GetPlayerName());
        }
    }

    EventBus->Publish(Tag_SecurityViolation, EventData);

    UE_LOG(LogSuspenseCoreEquipmentNetwork, Warning,
        TEXT("Broadcast SuspenseCore.Event.Equipment.Network.SecurityViolation: Type=%s, Details=%s"),
        *ViolationType, *Details);
}

void USuspenseCoreEquipmentNetworkService::OnOperationCompleted(
    FGameplayTag EventTag,
    const FSuspenseCoreEventData& EventData)
{
    // Handle operation completed events - update network metrics
    const FString OpIdStr = EventData.GetString(FName("OperationId"), TEXT(""));
    const FString SuccessStr = EventData.GetString(FName("Success"), TEXT("false"));
    const bool bSuccess = SuccessStr.Equals(TEXT("true"), ESearchCase::IgnoreCase);

    FGuid OperationId;
    if (FGuid::Parse(OpIdStr, OperationId))
    {
        // Track network-specific metrics only
        if (!bSuccess)
        {
            TotalOperationsRejected++;
        }
        else
        {
            TotalOperationsSent++;
        }
    }

    UE_LOG(LogSuspenseCoreEquipmentNetwork, Verbose,
        TEXT("OnOperationCompleted [%s]: OpId=%s, Success=%s"),
        *EventTag.ToString(), *OpIdStr, bSuccess ? TEXT("true") : TEXT("false"));
}
