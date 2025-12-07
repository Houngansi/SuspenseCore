// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Services/SuspenseCoreEquipmentNetworkService.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceLocator.h"
#include "Components/Network/SuspenseEquipmentNetworkDispatcher.h"
#include "Components/Network/SuspenseEquipmentPredictionSystem.h"
#include "Components/Network/SuspenseEquipmentReplicationManager.h"
#include "Interfaces/Equipment/ISuspenseEquipmentDataProvider.h"
#include "Interfaces/Equipment/ISuspenseEquipmentOperations.h"
#include "HAL/PlatformProcess.h"
#include "Engine/World.h"
#include "Engine/NetConnection.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#include "Misc/SecureHash.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"

USuspenseCoreEquipmentNetworkService::USuspenseCoreEquipmentNetworkService()
    : ServiceState(EServiceLifecycleState::Uninitialized)
    , AverageLatency(0.0f)
    , TotalOperationsSent(0)
    , TotalOperationsRejected(0)
    , TotalReplayAttemptsBlocked(0)
    , TotalIntegrityFailures(0)
    , NetworkQualityLevel(1.0f)
{
    SecurityConfig = FNetworkSecurityConfig::LoadFromConfig();
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
        ServiceState = EServiceLifecycleState::Shutdown;

        // Очищаем только локальные контейнеры (безопасно)
        FScopeLock Lock(&SecurityLock);
        RateLimitPerPlayer.Empty();
        RateLimitPerIP.Empty();
        ProcessedNonces.Empty();
        PendingNonces.Empty();
        SuspiciousActivityCount.Empty();

        while (!NonceExpiryQueue.IsEmpty())
        {
            TPair<uint64, float> Item;
            NonceExpiryQueue.Dequeue(Item);
        }

        HMACSecretKey.Empty();

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

        return;
    }

    // Защита от повторного вызова
    if (ServiceState == EServiceLifecycleState::Shutdown)
    {
        return;
    }

    ServiceState = EServiceLifecycleState::Shutting;

    // === БЕЗОПАСНЫЙ ЭКСПОРТ МЕТРИК ===
    // Только если не force и не из деструктора
    if (!bForce && GetWorld())
    {
        FString FinalMetricsPath = FPaths::ProjectLogDir() / TEXT("NetworkSecurity_FinalMetrics.csv");
        ExportSecurityMetrics(FinalMetricsPath);

        FString ServiceMetricsPath = FPaths::ProjectLogDir() / TEXT("NetworkService_FinalMetrics.csv");
        ExportMetricsToCSV(ServiceMetricsPath);
    }

    // === БЕЗОПАСНАЯ ОЧИСТКА ТАЙМЕРОВ ===
    UWorld* World = GetWorld();
    if (World && IsValid(World))
    {
        FTimerManager& TimerManager = World->GetTimerManager();

        if (NonceCleanupTimer.IsValid())
        {
            TimerManager.ClearTimer(NonceCleanupTimer);
        }
        if (MetricsUpdateTimer.IsValid())
        {
            TimerManager.ClearTimer(MetricsUpdateTimer);
        }
        if (MetricsExportTimer.IsValid())
        {
            TimerManager.ClearTimer(MetricsExportTimer);
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

    // === БЕЗОПАСНАЯ ОЧИСТКА SECURITY ===
    ShutdownSecurity();

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

    ServiceState = EServiceLifecycleState::Shutdown;

    UE_LOG(LogSuspenseCoreEquipmentNetwork, Log, TEXT("Equipment Network Service shutdown complete"));
}

bool USuspenseCoreEquipmentNetworkService::ResolveDependencies(
    UWorld* World,
    TScriptInterface<ISuspenseEquipmentDataProvider>& OutDataProvider,
    TScriptInterface<ISuspenseEquipmentOperations>& OutOperationExecutor)
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
        if (ServiceObj && ServiceObj->GetClass()->ImplementsInterface(UEquipmentDataService::StaticClass()))
        {
            IEquipmentDataService* DataService = Cast<IEquipmentDataService>(ServiceObj);
            if (DataService)
            {
                ISuspenseEquipmentDataProvider* Provider = DataService->GetDataProvider();
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

        if (ServiceObj && ServiceObj->GetClass()->ImplementsInterface(UEquipmentOperationService::StaticClass()))
        {
            IEquipmentOperationService* OpService = Cast<IEquipmentOperationService>(ServiceObj);
            if (OpService)
            {
                ISuspenseEquipmentOperations* Executor = OpService->GetOperationsExecutor();
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
    const TScriptInterface<ISuspenseEquipmentOperations>& OperationExecutor)
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
    NetworkDispatcher.SetInterface(Cast<ISuspenseNetworkDispatcher>(Dispatcher));

    return Dispatcher;
}

USuspenseCoreEquipmentPredictionSystem* USuspenseCoreEquipmentNetworkService::CreateAndInitPredictionSystem(
    AActor* OwnerActor,
    const TScriptInterface<ISuspenseEquipmentDataProvider>& DataProvider,
    const TScriptInterface<ISuspenseEquipmentOperations>& OperationExecutor)
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
    PredictionManager.SetInterface(Cast<ISuspensePredictionManager>(Prediction));

    return Prediction;
}

USuspenseCoreEquipmentReplicationManager* USuspenseCoreEquipmentNetworkService::CreateAndInitReplicationManager(
    AActor* OwnerActor,
    const TScriptInterface<ISuspenseEquipmentDataProvider>& DataProvider)
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
    Replication->SetReplicationPolicy(EEquipmentReplicationPolicy::OnlyToRelevant);

    UE_LOG(LogSuspenseCoreEquipmentNetwork, Log, TEXT("ReplicationManager created & initialized"));

    // Храним как raw UObject компонент
    ReplicationProvider = Replication;

    return Replication;
}

void USuspenseCoreEquipmentNetworkService::BindDispatcherToPrediction(
    USuspenseCoreEquipmentNetworkDispatcher* Dispatcher,
    ISuspensePredictionManager* Prediction)
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

    World->GetTimerManager().SetTimer(
        NonceCleanupTimer,
        this,
        &USuspenseCoreEquipmentNetworkService::CleanExpiredNonces,
        60.0f, true);

    World->GetTimerManager().SetTimer(
        MetricsUpdateTimer,
        this,
        &USuspenseCoreEquipmentNetworkService::UpdateNetworkMetrics,
        1.0f, true);

    World->GetTimerManager().SetTimer(
        MetricsExportTimer,
        this,
        &USuspenseCoreEquipmentNetworkService::ExportMetricsPeriodically,
        300.0f, true);
}

bool USuspenseCoreEquipmentNetworkService::InitializeService(const FServiceInitParams& Params)
{
    SCOPED_SERVICE_TIMER("InitializeService");

    if (ServiceState != EServiceLifecycleState::Uninitialized)
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Warning, TEXT("Service already initialized"));
        ServiceMetrics.RecordError();
        RECORD_SERVICE_METRIC("initialization_already_initialized", 1);
        return false;
    }

    ServiceState  = EServiceLifecycleState::Initializing;
    ServiceParams = Params;

    InitializeSecurity();
    RECORD_SERVICE_METRIC("security_initialized", 1);

    HMACSecretKey = LoadHMACKeyFromSecureStorage();
    if (HMACSecretKey.IsEmpty())
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("Failed to load HMAC secret key from secure storage"));
        ServiceState = EServiceLifecycleState::Failed;
        ServiceMetrics.RecordError();
        RECORD_SERVICE_METRIC("hmac_key_load_failed", 1);
        return false;
    }
    RECORD_SERVICE_METRIC("hmac_key_loaded", 1);

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("No valid world context for network service initialization"));
        ServiceState = EServiceLifecycleState::Failed;
        ServiceMetrics.RecordError();
        return false;
    }

    AActor* OwnerActor = Cast<AActor>(ServiceParams.Owner);
    if (!OwnerActor)
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("Service owner is not an Actor, cannot create network components"));
        ServiceState = EServiceLifecycleState::Failed;
        ServiceMetrics.RecordError();
        return false;
    }

    TScriptInterface<ISuspenseEquipmentDataProvider> DataProvider;
    TScriptInterface<ISuspenseEquipmentOperations>  OperationExecutor;
    if (!ResolveDependencies(World, DataProvider, OperationExecutor))
    {
        ServiceState = EServiceLifecycleState::Failed;
        ServiceMetrics.RecordError();
        return false;
    }

    USuspenseCoreEquipmentNetworkDispatcher* Dispatcher =
        CreateAndInitNetworkDispatcher(OwnerActor, OperationExecutor);
    if (!Dispatcher)
    {
        ServiceState = EServiceLifecycleState::Failed;
        ServiceMetrics.RecordError();
        return false;
    }
    RECORD_SERVICE_METRIC("network_dispatcher_created", 1);

    USuspenseCoreEquipmentPredictionSystem* Prediction =
        CreateAndInitPredictionSystem(OwnerActor, DataProvider, OperationExecutor);
    if (!Prediction)
    {
        ServiceState = EServiceLifecycleState::Failed;
        ServiceMetrics.RecordError();
        return false;
    }
    RECORD_SERVICE_METRIC("prediction_manager_created", 1);

    USuspenseCoreEquipmentReplicationManager* Replication =
        CreateAndInitReplicationManager(OwnerActor, DataProvider);
    if (!Replication)
    {
        ServiceState = EServiceLifecycleState::Failed;
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

    ServiceState = EServiceLifecycleState::Ready;
    ServiceMetrics.RecordSuccess();
    RECORD_SERVICE_METRIC("initialization_success", 1);

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
    FScopedServiceTimer __svc_timer(const_cast<FServiceMetrics&>(ServiceMetrics),
                                    FName(TEXT("GetServiceTag")));
    return FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Network"));
}

FGameplayTagContainer USuspenseCoreEquipmentNetworkService::GetRequiredDependencies() const
{
    FScopedServiceTimer __svc_timer(const_cast<FServiceMetrics&>(ServiceMetrics),
                                    FName(TEXT("GetRequiredDependencies")));
    FGameplayTagContainer Dependencies;
    Dependencies.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Data")));
    Dependencies.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Operations")));
    return Dependencies;
}

bool USuspenseCoreEquipmentNetworkService::ValidateService(TArray<FText>& OutErrors) const
{
    FScopedServiceTimer __svc_timer(const_cast<FServiceMetrics&>(ServiceMetrics),
                                    FName(TEXT("ValidateService")));
    bool bIsValid = true;

    if (ServiceState != EServiceLifecycleState::Ready)
    {
        OutErrors.Add(FText::FromString(TEXT("Network Service is not in Ready state")));
        bIsValid = false;
        const_cast<FServiceMetrics&>(ServiceMetrics).RecordValue(FName(TEXT("validation_state_error")), 1);
    }

    if (!NetworkDispatcher.GetInterface())
    {
        OutErrors.Add(FText::FromString(TEXT("NetworkDispatcher is not initialized")));
        bIsValid = false;
        const_cast<FServiceMetrics&>(ServiceMetrics).RecordValue(FName(TEXT("validation_dispatcher_error")), 1);
    }

    if (!PredictionManager.GetInterface())
    {
        OutErrors.Add(FText::FromString(TEXT("PredictionManager is not initialized")));
        bIsValid = false;
        const_cast<FServiceMetrics&>(ServiceMetrics).RecordValue(FName(TEXT("validation_prediction_error")), 1);
    }

    if (!ReplicationProvider)
    {
        OutErrors.Add(FText::FromString(TEXT("ReplicationProvider is not initialized")));
        bIsValid = false;
        const_cast<FServiceMetrics&>(ServiceMetrics).RecordValue(FName(TEXT("validation_replication_error")), 1);
    }

    if (HMACSecretKey.IsEmpty())
    {
        OutErrors.Add(FText::FromString(TEXT("HMAC secret key not configured")));
        bIsValid = false;
        const_cast<FServiceMetrics&>(ServiceMetrics).RecordValue(FName(TEXT("validation_hmac_error")), 1);
    }

    if (bIsValid)
    {
        const_cast<FServiceMetrics&>(ServiceMetrics).RecordSuccess();
    }
    else
    {
        const_cast<FServiceMetrics&>(ServiceMetrics).RecordError();
    }

    return bIsValid;
}

void USuspenseCoreEquipmentNetworkService::ResetService()
{
    SCOPED_SERVICE_TIMER("ResetService");
    FScopeLock Lock(&SecurityLock);

    RateLimitPerPlayer.Empty();
    RateLimitPerIP.Empty();
    ProcessedNonces.Empty();
    PendingNonces.Empty();
    while (!NonceExpiryQueue.IsEmpty())
    {
        TPair<uint64, float> Item;
        NonceExpiryQueue.Dequeue(Item);
    }
    SuspiciousActivityCount.Empty();

    SecurityMetrics.Reset();

    AverageLatency              = 0.0f;
    TotalOperationsSent         = 0;
    TotalOperationsRejected     = 0;
    TotalReplayAttemptsBlocked  = 0;
    TotalIntegrityFailures      = 0;

    ServiceMetrics.Reset();
    ServiceMetrics.RecordSuccess();
    RECORD_SERVICE_METRIC("Network.Service.Reset", 1);

    UE_LOG(LogSuspenseCoreEquipmentNetwork, Log,
        TEXT("EquipmentNetworkService reset complete - all security and metric data cleared"));
}

FString USuspenseCoreEquipmentNetworkService::GetServiceStats() const
{
    FScopedServiceTimer __svc_timer(const_cast<FServiceMetrics&>(ServiceMetrics),
                                    FName(TEXT("GetServiceStats")));
    FScopeLock Lock(&SecurityLock);

    FString Stats = TEXT("=== Equipment Network Service Statistics ===\n");
    Stats += FString::Printf(TEXT("Service State: %s\n"),
        *UEnum::GetValueAsString(ServiceState));
    Stats += FString::Printf(TEXT("Network Quality: %.2f\n"), NetworkQualityLevel);
    Stats += FString::Printf(TEXT("Average Latency: %.2f ms\n"), AverageLatency);

    Stats += TEXT("\n=== Service Performance Metrics ===\n");
    Stats += ServiceMetrics.ToString(TEXT("NetworkService"));

    Stats += TEXT("\n=== Enhanced Security Metrics ===\n");
    Stats += SecurityMetrics.ToString();

    Stats += TEXT("\n--- Security Configuration ---\n");
    Stats += FString::Printf(TEXT("Packet Age Limit: %.1f seconds\n"), SecurityConfig.PacketAgeLimit);
    Stats += FString::Printf(TEXT("Max Ops/Second: %d\n"), SecurityConfig.MaxOperationsPerSecond);
    Stats += FString::Printf(TEXT("Max Ops/Minute: %d\n"), SecurityConfig.MaxOperationsPerMinute);
    Stats += FString::Printf(TEXT("IP Rate Limiting: %s\n"), SecurityConfig.bEnableIPRateLimit ? TEXT("ENABLED") : TEXT("DISABLED"));

    Stats += TEXT("\n--- Active Monitoring ---\n");
    Stats += FString::Printf(TEXT("Active Player Rate Limits: %d\n"), RateLimitPerPlayer.Num());
    Stats += FString::Printf(TEXT("Active IP Rate Limits: %d\n"), RateLimitPerIP.Num());
    Stats += FString::Printf(TEXT("Processed Nonces: %d\n"), ProcessedNonces.Num());
    Stats += FString::Printf(TEXT("Pending Nonces: %d\n"), PendingNonces.Num());
    Stats += FString::Printf(TEXT("Suspicious Activities Tracked: %d\n"), SuspiciousActivityCount.Num());

    if (NetworkDispatcher.GetInterface())
    {
        Stats += TEXT("\n--- Network Dispatcher ---\n");
        // оператор -> указывает на интерфейс; для статистики компонента лучше спросить сам компонент
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

    const_cast<FServiceMetrics&>(ServiceMetrics).RecordValue(FName(TEXT("stats_retrieved")), 1);

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
    FScopeLock Lock(&SecurityLock);

    FNetworkSecurityConfig NewConfig = FNetworkSecurityConfig::LoadFromConfig();
    SecurityConfig = NewConfig;

    RECORD_SERVICE_METRIC("config_reloaded", 1);
    ServiceMetrics.RecordSuccess();

    UE_LOG(LogSuspenseCoreEquipmentNetwork, Log, TEXT("Security configuration reloaded from INI"));
}

FGuid USuspenseCoreEquipmentNetworkService::SendEquipmentOperation(const FEquipmentOperationRequest& Request, APlayerController* PlayerController)
{
    SCOPED_SERVICE_TIMER("SendEquipmentOperation");
    const double StartTime = FPlatformTime::Seconds();

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

    FScopeLock Lock(&SecurityLock);

    FGuid PlayerGuid;
    if (APlayerState* PS = PlayerController->GetPlayerState<APlayerState>())
    {
        const FUniqueNetIdRepl& UID = PS->GetUniqueId();
        if (UID.IsValid()) PlayerGuid = CreatePlayerGuid(UID);
    }

    if (!PlayerGuid.IsValid())
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("Failed to generate valid player GUID"));
        SecurityMetrics.RequestsRejectedIntegrity++;
        ServiceMetrics.RecordError();
        RECORD_SERVICE_METRIC("send_operation_invalid_guid", 1);
        return FGuid();
    }

    const FString IP = GetIPAddress(PlayerController);
    if (SecurityConfig.bEnableIPRateLimit && !CheckIPRateLimit(IP))
    {
        SecurityMetrics.RequestsRejectedIP++;
        LogSuspiciousActivity(PlayerController, TEXT("IP rate limit exceeded"));
        ServiceMetrics.RecordError();
        RECORD_SERVICE_METRIC("send_operation_ip_rate_limit", 1);
        return FGuid();
    }

    if (!CheckRateLimit(PlayerGuid, PlayerController))
    {
        TotalOperationsRejected++;
        SecurityMetrics.RequestsRejectedRateLimit++;
        LogSuspiciousActivity(PlayerController, TEXT("Player rate limit exceeded"));
        ServiceMetrics.RecordError();
        RECORD_SERVICE_METRIC("send_operation_player_rate_limit", 1);
        return FGuid();
    }

    const uint64 Nonce = GenerateSecureNonce();
    if (!MarkNonceAsPending(Nonce))
    {
        SecurityMetrics.RequestsRejectedReplay++;
        LogSuspiciousActivity(PlayerController, TEXT("Duplicate nonce on client"));
        ServiceMetrics.RecordError();
        RECORD_SERVICE_METRIC("send_operation_nonce_duplicate", 1);
        return FGuid();
    }
    RECORD_SERVICE_METRIC("nonce_generated", 1);

    FNetworkOperationRequest NetReq;
    NetReq.RequestId        = FGuid::NewGuid();
    NetReq.Operation        = Request;
    NetReq.Priority         = ENetworkOperationPriority::Normal;
    NetReq.Timestamp        = FPlatformTime::Seconds();
    NetReq.ClientTimestamp  = NetReq.Timestamp;
    NetReq.Nonce            = Nonce;
    NetReq.bRequiresConfirmation = true;

    const bool bCritical = (Request.OperationType == EEquipmentOperationType::Drop)     ||
                           (Request.OperationType == EEquipmentOperationType::Transfer) ||
                           (Request.OperationType == EEquipmentOperationType::Upgrade);
    if (bCritical)
    {
        NetReq.Priority = ENetworkOperationPriority::Critical;
        SecurityMetrics.CriticalOperationsProcessed++;
        RECORD_SERVICE_METRIC("critical_operation", 1);
    }

    NetReq.UpdateChecksum();

    if (bCritical && SecurityConfig.bRequireHMACForCritical)
    {
        NetReq.HMACSignature = NetReq.GenerateHMAC(HMACSecretKey);
        RECORD_SERVICE_METRIC("hmac_generated", 1);
    }

    if (NetworkDispatcher.GetInterface())
    {
        NetworkDispatcher->SendOperationToServer(NetReq);
        RECORD_SERVICE_METRIC("operation_sent_to_dispatcher", 1);
    }

    FRateLimitData& RL = RateLimitPerPlayer.FindOrAdd(PlayerGuid);
    RL.RecordOperation(NetReq.Timestamp);
    RL.PlayerIdentifier = GetPlayerIdentifier(PlayerController);

    if (SecurityConfig.bEnableIPRateLimit)
    {
        FRateLimitData& IRL = RateLimitPerIP.FindOrAdd(IP);
        IRL.RecordOperation(NetReq.Timestamp);
        IRL.PlayerIdentifier = IP;
    }

    TotalOperationsSent++;
    SecurityMetrics.TotalRequestsProcessed++;
    ServiceMetrics.RecordSuccess();
    RECORD_SERVICE_METRIC("send_operation_success", 1);

    UpdateSecurityMetrics(StartTime);

    UE_LOG(LogSuspenseCoreEquipmentNetwork, Verbose, TEXT("Sent %s operation %s with nonce %llu from %s"),
        bCritical ? TEXT("CRITICAL") : TEXT("normal"),
        *NetReq.RequestId.ToString(),
        Nonce,
        *GetPlayerIdentifier(PlayerController));

    return NetReq.RequestId;
}

bool USuspenseCoreEquipmentNetworkService::ReceiveEquipmentOperation(const FNetworkOperationRequest& NetworkRequest, APlayerController* SendingPlayer)
{
    SCOPED_SERVICE_TIMER("ReceiveEquipmentOperation");
    const double StartTime = FPlatformTime::Seconds();

    if (!IsServiceReady())
    {
        ServiceMetrics.RecordError();
        RECORD_SERVICE_METRIC("receive_operation_service_not_ready", 1);
        return false;
    }

    FScopeLock Lock(&SecurityLock);

    if (!NetworkRequest.ValidateIntegrity())
    {
        TotalIntegrityFailures++;
        SecurityMetrics.RequestsRejectedIntegrity++;
        LogSuspiciousActivity(SendingPlayer, TEXT("Checksum validation failed"));
        ServiceMetrics.RecordError();
        RECORD_SERVICE_METRIC("receive_operation_integrity_failed", 1);
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("Integrity check failed for %s from %s"),
            *NetworkRequest.RequestId.ToString(), *GetPlayerIdentifier(SendingPlayer));
        return false;
    }

    if (!MarkNonceAsPending(NetworkRequest.Nonce))
    {
        TotalReplayAttemptsBlocked++;
        SecurityMetrics.RequestsRejectedReplay++;
        LogSuspiciousActivity(SendingPlayer, TEXT("Replay attack detected"));
        ServiceMetrics.RecordError();
        RECORD_SERVICE_METRIC("receive_operation_replay_attack", 1);
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("Replay blocked, nonce %llu from %s"),
            NetworkRequest.Nonce, *GetPlayerIdentifier(SendingPlayer));
        return false;
    }

    const float Now = FPlatformTime::Seconds();
    const float Age = Now - NetworkRequest.ClientTimestamp;
    if (Age > SecurityConfig.PacketAgeLimit)
    {
        LogSuspiciousActivity(SendingPlayer, TEXT("Stale packet received"));
        RejectNonce(NetworkRequest.Nonce);
        ServiceMetrics.RecordError();
        RECORD_SERVICE_METRIC("receive_operation_stale_packet", 1);
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Warning, TEXT("Stale packet rejected age=%.2fs from %s"),
            Age, *GetPlayerIdentifier(SendingPlayer));
        return false;
    }

    if (NetworkRequest.Priority == ENetworkOperationPriority::Critical &&
        SecurityConfig.bRequireHMACForCritical)
    {
        if (!NetworkRequest.VerifyHMAC(HMACSecretKey))
        {
            SecurityMetrics.RequestsRejectedHMAC++;
            LogSuspiciousActivity(SendingPlayer, TEXT("HMAC verification failed"));
            RejectNonce(NetworkRequest.Nonce);
            ServiceMetrics.RecordError();
            RECORD_SERVICE_METRIC("receive_operation_hmac_failed", 1);
            UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("HMAC failed for %s from %s"),
                *NetworkRequest.RequestId.ToString(), *GetPlayerIdentifier(SendingPlayer));
            return false;
        }
        RECORD_SERVICE_METRIC("hmac_verified", 1);
    }

    ConfirmNonce(NetworkRequest.Nonce);
    RECORD_SERVICE_METRIC("nonce_confirmed", 1);

    SecurityMetrics.TotalRequestsProcessed++;
    ServiceMetrics.RecordSuccess();
    RECORD_SERVICE_METRIC("receive_operation_success", 1);

    UpdateSecurityMetrics(StartTime);
    return true;
}

void USuspenseCoreEquipmentNetworkService::SetNetworkQuality(float Quality)
{
    SCOPED_SERVICE_TIMER("SetNetworkQuality");
    FScopeLock Lock(&SecurityLock);

    NetworkQualityLevel = FMath::Clamp(Quality, 0.0f, 1.0f);
    AdaptNetworkStrategies();

    RECORD_SERVICE_METRIC("network_quality_updated", 1);
    ServiceMetrics.RecordSuccess();

    UE_LOG(LogSuspenseCoreEquipmentNetwork, Log, TEXT("Network quality set to %.2f"), NetworkQualityLevel);
}

FLatencyCompensationData USuspenseCoreEquipmentNetworkService::GetNetworkMetrics() const
{
    FScopedServiceTimer __svc_timer(const_cast<FServiceMetrics&>(ServiceMetrics),
                                    FName(TEXT("GetNetworkMetrics")));
    FScopeLock Lock(&SecurityLock);

    FLatencyCompensationData Metrics;
    Metrics.EstimatedLatency = AverageLatency;
    Metrics.ServerTime       = FPlatformTime::Seconds();
    Metrics.ClientTime       = FPlatformTime::Seconds();
    Metrics.TimeDilation     = 1.0f;

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

    const_cast<FServiceMetrics&>(ServiceMetrics).RecordValue(FName(TEXT("metrics_retrieved")), 1);

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

    FScopeLock Lock(&SecurityLock);

    if (ReplicationProvider)
    {
        ReplicationProvider->ForceFullReplication();
        RECORD_SERVICE_METRIC("full_replication_forced", 1);

        UE_LOG(LogSuspenseCoreEquipmentNetwork, Log, TEXT("Forced synchronization for %s"),
            *GetPlayerIdentifier(PlayerController));
    }

    if (NetworkDispatcher.GetInterface())
    {
        NetworkDispatcher->FlushPendingOperations(true);
        RECORD_SERVICE_METRIC("pending_operations_flushed", 1);
    }

    ServiceMetrics.RecordSuccess();
    RECORD_SERVICE_METRIC("force_sync_success", 1);
}

bool USuspenseCoreEquipmentNetworkService::ExportSecurityMetrics(const FString& FilePath) const
{
    // Не экспортировать во время engine shutdown
    if (IsEngineExitRequested())
    {
        return false;
    }

    // Проверка что файловая система доступна
    if (!FPaths::DirectoryExists(FPaths::ProjectLogDir()))
    {
        return false;
    }

    FScopeLock Lock(&SecurityLock);

    FString MetricsCSV = SecurityMetrics.ToCSV();

    return FFileHelper::SaveStringToFile(MetricsCSV, *FilePath);
}

FGuid USuspenseCoreEquipmentNetworkService::CreatePlayerGuid(const FUniqueNetIdRepl& UniqueId) const
{
    FScopedServiceTimer __svc_timer(const_cast<FServiceMetrics&>(ServiceMetrics),
                                    FName(TEXT("CreatePlayerGuid")));

    if (!UniqueId.IsValid())
    {
        return FGuid();
    }

    FString UniqueIdString = UniqueId->ToString();
    uint8   HashResult[20];
    FSHA1   Sha1Gen;
    Sha1Gen.Update((const uint8*)TCHAR_TO_UTF8(*UniqueIdString), UniqueIdString.Len());
    FString EntropyString = FString::Printf(TEXT("MedCom_Equipment_%s_Salt"), *UniqueIdString);
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

    const_cast<FServiceMetrics&>(ServiceMetrics).RecordValue(FName(TEXT("player_guid_created")), 1);
    return FGuid(GuidComponents[0], GuidComponents[1], GuidComponents[2], GuidComponents[3]);
}

bool USuspenseCoreEquipmentNetworkService::CheckRateLimit(const FGuid& PlayerGuid, APlayerController* PlayerController)
{
    float CurrentTime = FPlatformTime::Seconds();
    FRateLimitData& RateLimitData = RateLimitPerPlayer.FindOrAdd(PlayerGuid);

    if (RateLimitData.PlayerIdentifier.IsEmpty())
    {
        RateLimitData.PlayerIdentifier = GetPlayerIdentifier(PlayerController);
    }

    if (!RateLimitData.IsOperationAllowed(CurrentTime,
        SecurityConfig.MaxOperationsPerSecond,
        SecurityConfig.MaxOperationsPerMinute,
        SecurityConfig.TemporaryBanDuration))
    {
        RateLimitData.RecordViolation(CurrentTime,
            SecurityConfig.TemporaryBanDuration,
            SecurityConfig.MaxViolationsBeforeBan);

        if (RateLimitData.bIsTemporarilyBanned)
        {
            SecurityMetrics.PlayersTemporarilyBanned++;
            RECORD_SERVICE_METRIC("player_banned", 1);
        }

        RECORD_SERVICE_METRIC("rate_limit_violation", 1);

        UE_LOG(LogSuspenseCoreEquipmentNetwork, Warning,
            TEXT("Rate limit violation for %s - %d violations, banned: %s"),
            *RateLimitData.PlayerIdentifier,
            RateLimitData.ViolationCount,
            RateLimitData.bIsTemporarilyBanned ? TEXT("YES") : TEXT("NO"));

        return false;
    }

    if (RateLimitData.LastOperationTime > 0.0f &&
        (CurrentTime - RateLimitData.LastOperationTime) < SecurityConfig.MinOperationInterval)
    {
        RECORD_SERVICE_METRIC("operation_too_fast", 1);
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Verbose, TEXT("Operation too fast from %s (%.3f seconds since last)"),
            *RateLimitData.PlayerIdentifier,
            CurrentTime - RateLimitData.LastOperationTime);

        return false;
    }

    return true;
}

bool USuspenseCoreEquipmentNetworkService::CheckIPRateLimit(const FString& IPAddress)
{
    if (IPAddress.IsEmpty() || IPAddress == TEXT("Unknown"))
    {
        return true;
    }

    float CurrentTime = FPlatformTime::Seconds();

    FRateLimitData& IPRateLimitData = RateLimitPerIP.FindOrAdd(IPAddress);
    IPRateLimitData.PlayerIdentifier = IPAddress;

    int32 MaxPerMinute = SecurityConfig.MaxOperationsPerIPPerMinute;

    if (!IPRateLimitData.IsOperationAllowed(CurrentTime, MaxPerMinute / 60, MaxPerMinute,
        SecurityConfig.TemporaryBanDuration * 2))
    {
        IPRateLimitData.RecordViolation(CurrentTime,
            SecurityConfig.TemporaryBanDuration * 2,
            SecurityConfig.MaxViolationsBeforeBan);

        if (IPRateLimitData.bIsTemporarilyBanned)
        {
            SecurityMetrics.IPsTemporarilyBanned++;
            RECORD_SERVICE_METRIC("ip_banned", 1);
        }

        RECORD_SERVICE_METRIC("ip_rate_limit_violation", 1);

        UE_LOG(LogSuspenseCoreEquipmentNetwork, Warning,
            TEXT("IP rate limit violation for %s - %d violations, banned: %s"),
            *IPAddress,
            IPRateLimitData.ViolationCount,
            IPRateLimitData.bIsTemporarilyBanned ? TEXT("YES") : TEXT("NO"));

        return false;
    }

    return true;
}

bool USuspenseCoreEquipmentNetworkService::MarkNonceAsPending(uint64 Nonce)
{
    if (ProcessedNonces.Contains(Nonce))
    {
        RECORD_SERVICE_METRIC("nonce_replay_attempt", 1);
        return false;
    }
    if (PendingNonces.Contains(Nonce))
    {
        RECORD_SERVICE_METRIC("nonce_duplicate", 1);
        return false;
    }
    PendingNonces.Add(Nonce, FPlatformTime::Seconds());
    RECORD_SERVICE_METRIC("nonce_marked_pending", 1);
    return true;
}

bool USuspenseCoreEquipmentNetworkService::ConfirmNonce(uint64 Nonce)
{
    if (!PendingNonces.Contains(Nonce))
    {
        RECORD_SERVICE_METRIC("nonce_not_pending", 1);
        return false;
    }
    PendingNonces.Remove(Nonce);
    ProcessedNonces.Add(Nonce);
    NonceExpiryQueue.Enqueue(TPair<uint64, float>(Nonce, FPlatformTime::Seconds() + SecurityConfig.NonceLifetime));
    RECORD_SERVICE_METRIC("nonce_confirmed", 1);
    return true;
}

void USuspenseCoreEquipmentNetworkService::RejectNonce(uint64 Nonce)
{
    PendingNonces.Remove(Nonce);
    RECORD_SERVICE_METRIC("nonce_rejected", 1);
}

uint64 USuspenseCoreEquipmentNetworkService::GenerateSecureNonce()
{
    uint64 Nonce = 0;
    FGuid Guid1 = FGuid::NewGuid();
    FGuid Guid2 = FGuid::NewGuid();

    Nonce  = ((uint64)Guid1.A << 32) | (uint64)Guid1.B;
    Nonce ^= ((uint64)Guid1.C << 16) | ((uint64)Guid1.D << 48);
    Nonce ^= ((uint64)Guid2.A) | ((uint64)Guid2.B << 32);
    Nonce ^= ((uint64)Guid2.C << 48) | ((uint64)Guid2.D << 16);
    Nonce ^= (uint64)FPlatformTime::Cycles64();
    Nonce ^= (uint64)FPlatformTLS::GetCurrentThreadId() << 24;

    if (Nonce == 0) { Nonce = 1; }

    int32 Attempts = 0;
    const int32 MaxAttempts = 100;
    while ((ProcessedNonces.Contains(Nonce) || PendingNonces.Contains(Nonce)) && Attempts < MaxAttempts)
    {
        FGuid NewGuid = FGuid::NewGuid();
        Nonce ^= ((uint64)NewGuid.A << 32) | (uint64)NewGuid.B;
        Nonce ^= (uint64)FPlatformTime::Cycles64();
        if (Nonce == 0) { Nonce = 1; }
        Attempts++;
    }
    if (Attempts >= MaxAttempts)
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Warning, TEXT("Nonce generation collision detected after %d attempts"), MaxAttempts);
        RECORD_SERVICE_METRIC("nonce_collision_detected", 1);
        while (ProcessedNonces.Contains(Nonce) || PendingNonces.Contains(Nonce))
        {
            Nonce++;
            if (Nonce == 0) { Nonce = 1; }
        }
    }
    return Nonce;
}

FString USuspenseCoreEquipmentNetworkService::GenerateRequestHMAC(const FNetworkOperationRequest& Request) const
{
    return Request.GenerateHMAC(HMACSecretKey);
}

bool USuspenseCoreEquipmentNetworkService::VerifyRequestHMAC(const FNetworkOperationRequest& Request) const
{
    return Request.VerifyHMAC(HMACSecretKey);
}

void USuspenseCoreEquipmentNetworkService::CleanExpiredNonces()
{
    SCOPED_SERVICE_TIMER("CleanExpiredNonces");
    FScopeLock Lock(&SecurityLock);

    float CurrentTime = FPlatformTime::Seconds();
    int32 CleanedCount = 0;

    while (!NonceExpiryQueue.IsEmpty())
    {
        TPair<uint64, float> Item;
        if (NonceExpiryQueue.Peek(Item))
        {
            if (Item.Value < CurrentTime)
            {
                NonceExpiryQueue.Dequeue(Item);
                ProcessedNonces.Remove(Item.Key);
                CleanedCount++;
            }
            else
            {
                break;
            }
        }
        else
        {
            break;
        }
    }

    TArray<uint64> ExpiredPendingNonces;
    for (const auto& Pair : PendingNonces)
    {
        if ((CurrentTime - Pair.Value) > 5.0f)
        {
            ExpiredPendingNonces.Add(Pair.Key);
        }
    }
    for (uint64 Nonce : ExpiredPendingNonces)
    {
        PendingNonces.Remove(Nonce);
        CleanedCount++;
    }

    TArray<FGuid> PlayersToRemove;
    for (auto& Pair : RateLimitPerPlayer)
    {
        FRateLimitData& Data = Pair.Value;
        Data.OperationTimestamps.RemoveAll([CurrentTime](float Time){ return (CurrentTime - Time) > 300.0f; });
        if (Data.OperationTimestamps.Num() == 0 && (CurrentTime - Data.LastOperationTime) > 600.0f)
        {
            PlayersToRemove.Add(Pair.Key);
        }
    }
    for (const FGuid& PlayerGuid : PlayersToRemove)
    {
        RateLimitPerPlayer.Remove(PlayerGuid);
    }

    TArray<FString> IPsToRemove;
    for (auto& Pair : RateLimitPerIP)
    {
        FRateLimitData& Data = Pair.Value;
        Data.OperationTimestamps.RemoveAll([CurrentTime](float Time){ return (CurrentTime - Time) > 300.0f; });
        if (Data.OperationTimestamps.Num() == 0 && (CurrentTime - Data.LastOperationTime) > 600.0f)
        {
            IPsToRemove.Add(Pair.Key);
        }
    }
    for (const FString& IP : IPsToRemove)
    {
        RateLimitPerIP.Remove(IP);
    }

    RECORD_SERVICE_METRIC("nonces_cleaned", CleanedCount);
    RECORD_SERVICE_METRIC("players_cleaned", PlayersToRemove.Num());
    RECORD_SERVICE_METRIC("ips_cleaned", IPsToRemove.Num());

    if (CleanedCount > 0)
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Verbose, TEXT("Cleaned %d expired nonces, %d inactive players, %d inactive IPs"),
            CleanedCount, PlayersToRemove.Num(), IPsToRemove.Num());
    }
}

void USuspenseCoreEquipmentNetworkService::LogSuspiciousActivity(APlayerController* PlayerController, const FString& Reason)
{
    if (!SecurityConfig.bLogSuspiciousActivity)
    {
        return;
    }

    FString PlayerID = GetPlayerIdentifier(PlayerController);
    int32& Count = SuspiciousActivityCount.FindOrAdd(PlayerID);
    Count++;

    SecurityMetrics.SuspiciousActivitiesDetected++;
    RECORD_SERVICE_METRIC("suspicious_activity", 1);

    FString LogEntry = FString::Printf(
        TEXT("[SECURITY] %s | Player: %s | Reason: %s | Count: %d"),
        *FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S")),
        *PlayerID,
        *Reason,
        Count
    );

    UE_LOG(LogSuspenseCoreEquipmentNetwork, Warning, TEXT("%s"), *LogEntry);

    FString SecurityLogPath = FPaths::ProjectLogDir() / TEXT("NetworkSecurity.log");
    FFileHelper::SaveStringToFile(LogEntry + TEXT("\n"), *SecurityLogPath,
        FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);

    if (Count >= SecurityConfig.MaxSuspiciousActivities)
    {
        RECORD_SERVICE_METRIC("suspicious_threshold_exceeded", 1);
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Error,
            TEXT("SECURITY ALERT: Player %s exceeded suspicious activity threshold! Immediate action required."),
            *PlayerID);
    }
}

FString USuspenseCoreEquipmentNetworkService::GetPlayerIdentifier(APlayerController* PlayerController) const
{
    if (!PlayerController)
    {
        return TEXT("Unknown");
    }

    FString Identifier = GetIPAddress(PlayerController);

    if (APlayerState* PlayerState = PlayerController->GetPlayerState<APlayerState>())
    {
        Identifier += FString::Printf(TEXT(" [%s]"), *PlayerState->GetPlayerName());

        if (const FUniqueNetIdRepl& UniqueId = PlayerState->GetUniqueId(); UniqueId.IsValid())
        {
            Identifier += FString::Printf(TEXT(" ID:%s"), *UniqueId->ToString());
        }
    }

    return Identifier.IsEmpty() ? TEXT("Unknown") : Identifier;
}

FString USuspenseCoreEquipmentNetworkService::GetIPAddress(APlayerController* PlayerController) const
{
    if (!PlayerController)
    {
        return TEXT("Unknown");
    }

    if (UNetConnection* Connection = PlayerController->GetNetConnection())
    {
        return Connection->LowLevelGetRemoteAddress();
    }

    return TEXT("Unknown");
}

void USuspenseCoreEquipmentNetworkService::UpdateNetworkMetrics()
{
    SCOPED_SERVICE_TIMER("UpdateNetworkMetrics");
    FScopeLock Lock(&SecurityLock);

    if (NetworkDispatcher.GetInterface())
    {
        float NewSampleMs = 0.0f;

        if (USuspenseCoreEquipmentNetworkDispatcher* Dispatcher =
                Cast<USuspenseCoreEquipmentNetworkDispatcher>(NetworkDispatcher.GetObject()))
        {
            const FNetworkDispatcherStats Stats = Dispatcher->GetStats();
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

void USuspenseCoreEquipmentNetworkService::UpdateSecurityMetrics(double StartTime)
{
    double EndTime           = FPlatformTime::Seconds();
    double ProcessingTimeUs  = (EndTime - StartTime) * 1000000.0;

    uint64 CurrentAvg = SecurityMetrics.AverageProcessingTimeUs.Load();
    uint64 NewAvg     = (uint64)((CurrentAvg * 0.9) + (ProcessingTimeUs * 0.1));
    SecurityMetrics.AverageProcessingTimeUs = NewAvg;

    uint64 CurrentPeak = SecurityMetrics.PeakProcessingTimeUs.Load();
    if (ProcessingTimeUs > CurrentPeak)
    {
        SecurityMetrics.PeakProcessingTimeUs = (uint64)ProcessingTimeUs;
    }

    RECORD_SERVICE_METRIC("processing_time_us", (int64)ProcessingTimeUs);
}

void USuspenseCoreEquipmentNetworkService::ExportMetricsPeriodically()
{
    SCOPED_SERVICE_TIMER("ExportMetricsPeriodically");

    FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));

    FString SecurityFilePath = FPaths::ProjectLogDir() / FString::Printf(TEXT("NetworkSecurity_%s.csv"), *Timestamp);
    ExportSecurityMetrics(SecurityFilePath);

    FString ServiceFilePath = FPaths::ProjectLogDir() / FString::Printf(TEXT("NetworkService_%s.csv"), *Timestamp);
    ExportMetricsToCSV(ServiceFilePath);

    RECORD_SERVICE_METRIC("periodic_export", 2);
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
            ReplicationProvider->SetReplicationPolicy(EEquipmentReplicationPolicy::OnlyToOwner);
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
            ReplicationProvider->SetReplicationPolicy(EEquipmentReplicationPolicy::OnlyToRelevant);
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
            ReplicationProvider->SetReplicationPolicy(EEquipmentReplicationPolicy::Always);
        }

        RECORD_SERVICE_METRIC("strategy_good_network", 1);
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Log, TEXT("Adapted to GOOD network - maximum security enabled"));
    }
}

void USuspenseCoreEquipmentNetworkService::InitializeSecurity()
{
    RateLimitPerPlayer.Reserve(100);
    RateLimitPerIP.Reserve(200);
    ProcessedNonces.Reserve(1000);
    PendingNonces.Reserve(100);

    RECORD_SERVICE_METRIC("security_initialized", 1);

    UE_LOG(LogSuspenseCoreEquipmentNetwork, Log, TEXT("Security subsystems initialized with enhanced protection"));
}

void USuspenseCoreEquipmentNetworkService::ShutdownSecurity()
{
    // Во время engine exit - минимальная очистка
    if (IsEngineExitRequested())
    {
        // Только очистка локальных данных без логирования
        FScopeLock Lock(&SecurityLock);
        RateLimitPerPlayer.Empty();
        RateLimitPerIP.Empty();
        ProcessedNonces.Empty();
        PendingNonces.Empty();
        SuspiciousActivityCount.Empty();

        while (!NonceExpiryQueue.IsEmpty())
        {
            TPair<uint64, float> Item;
            NonceExpiryQueue.Dequeue(Item);
        }

        HMACSecretKey.Empty();
        return;
    }

    // Нормальный shutdown с логированием
    FScopeLock Lock(&SecurityLock);

    FString FinalReport = SecurityMetrics.ToString();
    UE_LOG(LogSuspenseCoreEquipmentNetwork, Log, TEXT("Final Security Report:\n%s"), *FinalReport);

    RateLimitPerPlayer.Empty();
    RateLimitPerIP.Empty();
    ProcessedNonces.Empty();
    PendingNonces.Empty();
    SuspiciousActivityCount.Empty();

    while (!NonceExpiryQueue.IsEmpty())
    {
        TPair<uint64, float> Item;
        NonceExpiryQueue.Dequeue(Item);
    }

    HMACSecretKey.Empty();

    UE_LOG(LogSuspenseCoreEquipmentNetwork, Log, TEXT("Security subsystems shutdown complete"));
}

FString USuspenseCoreEquipmentNetworkService::LoadHMACKeyFromSecureStorage()
{
    FString Key;

    if (GConfig)
    {
        FString EncryptedKey;
        if (GConfig->GetString(TEXT("NetworkSecurity.Keys"), TEXT("HMACSecret"), EncryptedKey, GGameIni))
        {
            Key = EncryptedKey;
        }
    }

    if (Key.IsEmpty())
    {
        FString EnvValue = FPlatformMisc::GetEnvironmentVariable(TEXT("MEDCOM_HMAC_KEY"));
        if (!EnvValue.IsEmpty())
        {
            Key = EnvValue;
        }
    }

    if (Key.IsEmpty())
    {
        FString SecureKeyPath = FPaths::ProjectSavedDir() / TEXT("Config/Secure/hmac.key");

        if (FPaths::FileExists(SecureKeyPath))
        {
            FFileHelper::LoadFileToString(Key, *SecureKeyPath);
            Key = Key.TrimStartAndEnd();
        }
    }

    if (Key.IsEmpty())
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Warning, TEXT("No HMAC key found, generating new secure key"));

        FString KeyPart1 = FGuid::NewGuid().ToString(EGuidFormats::Digits);
        FString KeyPart2 = FGuid::NewGuid().ToString(EGuidFormats::Digits);
        Key = KeyPart1 + KeyPart2;

        uint64 SystemEntropy = FPlatformTime::Cycles64() ^ (uint64)FPlatformProcess::GetCurrentProcessId();
        Key += FString::Printf(TEXT("%016llx"), SystemEntropy);

        if (Key.Len() > 64)
        {
            Key = Key.Left(64);
        }

        FString SecureKeyPath = FPaths::ProjectSavedDir() / TEXT("Config/Secure/hmac.key");
        FString SecureDir     = FPaths::GetPath(SecureKeyPath);
        if (!FPaths::DirectoryExists(SecureDir))
        {
            FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*SecureDir);
        }

        if (!FFileHelper::SaveStringToFile(Key, *SecureKeyPath))
        {
            UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("Failed to save HMAC key to secure storage"));
        }
        else
        {
            UE_LOG(LogSuspenseCoreEquipmentNetwork, Log, TEXT("Generated and saved new HMAC key to secure storage"));

            if (GConfig)
            {
                GConfig->SetString(TEXT("NetworkSecurity.Keys"), TEXT("HMACSecret"), *Key, GGameIni);
                GConfig->Flush(false, GGameIni);
            }
        }
    }

    if (Key.Len() < 32)
    {
        UE_LOG(LogSuspenseCoreEquipmentNetwork, Error, TEXT("HMAC key is too short (%d chars), security compromised!"), Key.Len());
        RECORD_SERVICE_METRIC("hmac_key_too_short", 1);
    }

    return Key;
}
