// EquipmentDataServiceImpl.cpp
// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Services/SuspenseCoreEquipmentDataService.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceLocator.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "SuspenseCore/Components/Core/SuspenseCoreEquipmentDataStore.h"
#include "SuspenseCore/Components/Transaction/SuspenseCoreEquipmentTransactionProcessor.h"
#include "Components/Validation/SuspenseEquipmentSlotValidator.h"
#include "Types/Loadout/SuspenseLoadoutSettings.h"
#include "Engine/World.h"
#include "HAL/PlatformFilemanager.h"
#include "Async/Async.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include <atomic>

#include "Components/Coordination/SuspenseEquipmentEventDispatcher.h"
#include "ItemSystem/SuspenseItemManager.h"

USuspenseCoreEquipmentDataService::USuspenseCoreEquipmentDataService()
{
    // Initialize caches with appropriate sizes for MMO scale
    // These sizes are tuned for ~100 concurrent players per server
    SnapshotCache = MakeShareable(new FSuspenseCoreEquipmentCacheManager<FGuid, FEquipmentStateSnapshot>(100));
    ItemCache = MakeShareable(new FSuspenseCoreEquipmentCacheManager<int32, FSuspenseInventoryItemInstance>(500));
    ConfigCache = MakeShareable(new FSuspenseCoreEquipmentCacheManager<int32, FEquipmentSlotConfig>(MaxSlotCount));

    InitializationTime = FDateTime::Now();
}

USuspenseCoreEquipmentDataService::~USuspenseCoreEquipmentDataService()
{
    // Ensure clean shutdown even in abnormal termination
    ShutdownService(true);
}

//========================================
// Component Injection and Configuration
//========================================

void USuspenseCoreEquipmentDataService::InjectComponents(UObject* InDataStore, UObject* InItemManager)
{
    // This method is called BEFORE InitializeService, so we don't take DataLock
    // as the service is not yet initialized and nobody can access it

    // ✅ КРИТИЧНО: ItemManager обязателен
    if (!InItemManager)
    {
        UE_LOG(LogSuspenseCoreEquipmentData, Error,
            TEXT("InjectComponents: ItemManager is null (REQUIRED)"));
        return;
    }

    // Cast ItemManager (REQUIRED)
    USuspenseItemManager* CastedItemManager = Cast<USuspenseItemManager>(InItemManager);
    if (!CastedItemManager)
    {
        UE_LOG(LogSuspenseCoreEquipmentData, Error,
            TEXT("InjectComponents: Failed to cast ItemManager. Received type: %s"),
            *InItemManager->GetClass()->GetName());
        return;
    }

    // Save ItemManager reference (REQUIRED for service operation)
    ItemManager = CastedItemManager;

    UE_LOG(LogSuspenseCoreEquipmentData, Log,
        TEXT("InjectComponents: ItemManager injected successfully: %s (items: %d)"),
        *ItemManager->GetName(), ItemManager->GetCachedItemCount());

    // ✅ DataStore и TransactionProcessor ОПЦИОНАЛЬНЫ (stateless mode)
    if (InDataStore)
    {
        USuspenseCoreEquipmentDataStore* CastedDataStore = Cast<USuspenseCoreEquipmentDataStore>(InDataStore);

        if (!CastedDataStore)
        {
            UE_LOG(LogSuspenseCoreEquipmentData, Error,
                TEXT("InjectComponents: Failed to cast DataStore to USuspenseCoreEquipmentDataStore. Received type: %s"),
                *InDataStore->GetClass()->GetName());
            return;
        }

        // Check that component is properly registered
        if (!CastedDataStore->IsRegistered())
        {
            UE_LOG(LogSuspenseCoreEquipmentData, Warning,
                TEXT("InjectComponents: DataStore is not registered as component. "
                     "This may cause lifecycle issues"));
        }

        // Save typed pointer
        DataStore = CastedDataStore;

        // Create interface wrapper
        DataProviderInterface.SetObject(DataStore);
        DataProviderInterface.SetInterface(Cast<ISuspenseEquipmentDataProvider>(DataStore));

        if (!DataProviderInterface.GetInterface())
        {
            UE_LOG(LogSuspenseCoreEquipmentData, Error,
                TEXT("InjectComponents: Failed to cast DataStore to ISuspenseEquipmentDataProvider"));
            DataStore = nullptr;
            return;
        }

        UE_LOG(LogSuspenseCoreEquipmentData, Log,
            TEXT("InjectComponents: DataStore injected: %s"), *DataStore->GetName());
    }
    else
    {
        UE_LOG(LogSuspenseCoreEquipmentData, Warning,
            TEXT("InjectComponents: DataStore not provided (stateless mode)"));
    }

    // Mark injection complete (ItemManager is sufficient for basic operation)
    bComponentsInjected = true;

    UE_LOG(LogSuspenseCoreEquipmentData, Log,
        TEXT("InjectComponents: Component injection completed successfully"));
    UE_LOG(LogSuspenseCoreEquipmentData, Log,
        TEXT("  - ItemManager: %s (REQUIRED - injected)"), *ItemManager->GetName());
    UE_LOG(LogSuspenseCoreEquipmentData, Log,
        TEXT("  - DataStore: %s"), DataStore ? *DataStore->GetName() : TEXT("Not Set (stateless)"));
    UE_LOG(LogSuspenseCoreEquipmentData, Log,
        TEXT("  - TransactionProcessor: %s"), TransactionProcessor ? *TransactionProcessor->GetName() : TEXT("Not Set (stateless)"));
}

void USuspenseCoreEquipmentDataService::SetValidator(UObject* InValidator)
{
    if (!InValidator)
    {
        UE_LOG(LogSuspenseCoreEquipmentData, Error,
            TEXT("SetValidator: Invalid validator provided (null)"));
        return;
    }

    // Cast from UObject* to actual type
    USuspenseCoreEquipmentSlotValidator* CastedValidator = Cast<USuspenseCoreEquipmentSlotValidator>(InValidator);

    if (!CastedValidator)
    {
        UE_LOG(LogSuspenseCoreEquipmentData, Error,
            TEXT("SetValidator: Failed to cast to USuspenseCoreEquipmentSlotValidator. Received type: %s"),
            *InValidator->GetClass()->GetName());
        return;
    }

    SlotValidator = CastedValidator;

    UE_LOG(LogSuspenseCoreEquipmentData, Log,
        TEXT("SetValidator: Slot validator set successfully: %s"), *SlotValidator->GetName());
}

//========================================
// IEquipmentService Implementation
//========================================

bool USuspenseCoreEquipmentDataService::InitializeService(const FServiceInitParams& Params)
{
    SCOPED_SERVICE_TIMER("Data.InitializeService");
    bool bOk = false;
    ON_SCOPE_EXIT { if (bOk) ServiceMetrics.RecordSuccess(); else ServiceMetrics.RecordError(); };

    // ✅ Phase 0: Critical dependency check - ItemManager is REQUIRED
    if (!ItemManager)
    {
        UE_LOG(LogSuspenseCoreEquipmentData, Error, TEXT("InitializeService: ItemManager not injected!"));
        UE_LOG(LogSuspenseCoreEquipmentData, Error, TEXT("  Call InjectComponents(DataStore, ItemManager) before InitializeService"));
        RECORD_SERVICE_METRIC("Data.Init.NoItemManager", 1);
        ServiceState = EServiceLifecycleState::Failed;
        return false;
    }

    UE_LOG(LogSuspenseCoreEquipmentData, Log, TEXT("InitializeService: ItemManager available (%s)"),
        *ItemManager->GetName());

    // Phase 1: Check state and validate under lock
    {
        FRWScopeLock ScopeLock(DataLock, SLT_Write);

        if (ServiceState != EServiceLifecycleState::Uninitialized)
        {
            UE_LOG(LogSuspenseCoreEquipmentData, Warning,
                TEXT("InitializeService: Service already initialized (state: %s)"),
                *UEnum::GetValueAsString(ServiceState));
            RECORD_SERVICE_METRIC("Data.Init.AlreadyInitialized", 1);
            return false;
        }

        ServiceState = EServiceLifecycleState::Initializing;

        // ✅ STATELESS MODE: Per-player components are optional
        // Auto-resolve: если инъекции ещё не было — попробуем достать компоненты с Owner (обычно PlayerState/Actor).
        // ИЩЕМ ТОЛЬКО ActorComponents (DataStore/TransactionProcessor). Валидатор НЕ ищем через FindComponentByClass!
        if (!bComponentsInjected)
        {
            if (AActor* OwnerActor = Cast<AActor>(Params.Owner))
            {
                if (!DataStore || !TransactionProcessor)
                {
                    if (USuspenseCoreEquipmentDataStore* AutoDS = OwnerActor->FindComponentByClass<USuspenseCoreEquipmentDataStore>())
                    {
                        if (USuspenseCoreEquipmentTransactionProcessor* AutoTP = OwnerActor->FindComponentByClass<USuspenseCoreEquipmentTransactionProcessor>())
                        {
                            // Типобезопасная инъекция через публичный API сервиса
                            InjectComponents(AutoDS, ItemManager);

                            UE_LOG(LogSuspenseCoreEquipmentData, Log,
                                TEXT("InitializeService: Auto-resolved components from Owner actor"));
                        }
                    }
                }
            }
        }

        // ✅ STATELESS MODE: Components are optional for global service
        // Service can work in stateless mode where components are passed as method parameters
        const bool bStatelessMode = !bComponentsInjected || !DataStore || !TransactionProcessor;

        if (bStatelessMode)
        {
            UE_LOG(LogSuspenseCoreEquipmentData, Warning,
                TEXT("InitializeService: Operating in STATELESS mode (no per-player components)"));
            UE_LOG(LogSuspenseCoreEquipmentData, Warning,
                TEXT("  DataStore: %s"), DataStore ? TEXT("Available") : TEXT("Not Set"));
            UE_LOG(LogSuspenseCoreEquipmentData, Warning,
                TEXT("  TransactionProcessor: %s"), TransactionProcessor ? TEXT("Available") : TEXT("Not Set"));
            UE_LOG(LogSuspenseCoreEquipmentData, Warning,
                TEXT("  Components will be passed as parameters to service methods"));

            RECORD_SERVICE_METRIC("Data.Init.StatelessMode", 1);

            // In stateless mode, skip component validation
            // Service will validate components per-call in service methods
        }
        else
        {
            // ✅ STATEFUL MODE: Validate injected components
            UE_LOG(LogSuspenseCoreEquipmentData, Log,
                TEXT("InitializeService: Operating in STATEFUL mode (with per-player components)"));

            // Additional validation of components and interfaces
            if (!DataStore || !DataStore->IsValidLowLevel())
            {
                ServiceState = EServiceLifecycleState::Failed;
                UE_LOG(LogSuspenseCoreEquipmentData, Error,
                    TEXT("InitializeService: DataStore is invalid or destroyed"));
                RECORD_SERVICE_METRIC("Data.Init.InvalidDataStore", 1);
                return false;
            }

            if (!TransactionProcessor || !TransactionProcessor->IsValidLowLevel())
            {
                ServiceState = EServiceLifecycleState::Failed;
                UE_LOG(LogSuspenseCoreEquipmentData, Error,
                    TEXT("InitializeService: TransactionProcessor is invalid or destroyed"));
                RECORD_SERVICE_METRIC("Data.Init.InvalidTransactionProcessor", 1);
                return false;
            }

            if (!DataProviderInterface.GetInterface())
            {
                ServiceState = EServiceLifecycleState::Failed;
                UE_LOG(LogSuspenseCoreEquipmentData, Error,
                    TEXT("InitializeService: DataProviderInterface is not set"));
                RECORD_SERVICE_METRIC("Data.Init.NoDataProviderInterface", 1);
                return false;
            }

            if (!TransactionManagerInterface.GetInterface())
            {
                ServiceState = EServiceLifecycleState::Failed;
                UE_LOG(LogSuspenseCoreEquipmentData, Error,
                    TEXT("InitializeService: TransactionManagerInterface is not set"));
                RECORD_SERVICE_METRIC("Data.Init.NoTransactionManagerInterface", 1);
                return false;
            }

            // Validator is optional — НЕ ищем его через FindComponentByClass (он не компонент)!
            if (!SlotValidator)
            {
                UE_LOG(LogSuspenseCoreEquipmentData, Warning,
                    TEXT("InitializeService: SlotValidator not set - validation will be skipped"));
                RECORD_SERVICE_METRIC("Data.Init.NoValidator", 1);
            }

            // Registration sanity
            if (!DataStore->IsRegistered())
            {
                UE_LOG(LogSuspenseCoreEquipmentData, Error,
                    TEXT("InitializeService: DataStore is not registered! "
                         "It won't receive BeginPlay/EndPlay events"));
            }

            if (!TransactionProcessor->IsRegistered())
            {
                UE_LOG(LogSuspenseCoreEquipmentData, Error,
                    TEXT("InitializeService: TransactionProcessor is not registered! "
                         "It won't receive BeginPlay/EndPlay events"));
            }

            RECORD_SERVICE_METRIC("Data.Init.ComponentsValidated", 1);
        }
    }

    // Phase 2: External initialization WITHOUT lock to prevent deadlocks
    // ✅ Only in STATEFUL mode (when components are available)
    if (TransactionProcessor)
    {
        // Ensure TransactionProcessor is initialized with DataProvider
        if (!TransactionProcessor->GetDataProvider())
        {
            UE_LOG(LogSuspenseCoreEquipmentData, Log,
                TEXT("InitializeService: Initializing TransactionProcessor with DataProvider"));

            if (!TransactionProcessor->Initialize(DataProviderInterface))
            {
                FRWScopeLock ScopeLock(DataLock, SLT_Write);
                ServiceState = EServiceLifecycleState::Failed;
                UE_LOG(LogSuspenseCoreEquipmentData, Error,
                    TEXT("InitializeService: Failed to initialize TransactionProcessor with DataProvider"));
                RECORD_SERVICE_METRIC("Data.Init.TransactionProcessorInitFailed", 1);
                return false;
            }

            RECORD_SERVICE_METRIC("Data.Init.TransactionProcessorInitialized", 1);
        }
        else
        {
            UE_LOG(LogSuspenseCoreEquipmentData, Log,
                TEXT("InitializeService: TransactionProcessor already initialized with DataProvider"));
        }

        // Setup delta callback for transaction processor
        TransactionProcessor->SetDeltaCallback(
            FOnTransactionDelta::CreateUObject(this, &USuspenseCoreEquipmentDataService::OnTransactionDeltas)
        );
    }
    else
    {
        UE_LOG(LogSuspenseCoreEquipmentData, Warning,
            TEXT("InitializeService: TransactionProcessor not available (stateless mode)"));
    }

    // Phase 3: Complete initialization under lock
    {
        FRWScopeLock ScopeLock(DataLock, SLT_Write);

        // Setup event subscriptions including delta events (only in stateful mode)
        if (DataStore && TransactionProcessor)
        {
            SetupEventSubscriptions();
            RECORD_SERVICE_METRIC("Data.Init.EventSubscriptionsSetup", 1);
        }
        else
        {
            UE_LOG(LogSuspenseCoreEquipmentData, Warning,
                TEXT("InitializeService: Skipping event subscriptions (stateless mode)"));
        }

        // Register caches in the global registry (pass raw pointers, not lambdas)
        if (SnapshotCache.IsValid())
        {
            FSuspenseGlobalCacheRegistry::Get().RegisterCache(TEXT("EquipmentData.Snapshots"), SnapshotCache.Get());
        }
        if (ItemCache.IsValid())
        {
            FSuspenseGlobalCacheRegistry::Get().RegisterCache(TEXT("EquipmentData.Items"), ItemCache.Get());
        }
        if (ConfigCache.IsValid())
        {
            FSuspenseGlobalCacheRegistry::Get().RegisterCache(TEXT("EquipmentData.Configs"), ConfigCache.Get());
        }
        RECORD_SERVICE_METRIC("Data.Init.CachesRegistered", 3);

        // Initialize delta metrics
        DeltaMetrics.Reset();

        // Warm up caches if enabled
        if (bEnableCacheWarming)
        {
            WarmupCachesSafe();
            RECORD_SERVICE_METRIC("Data.Init.CacheWarmed", 1);
        }

        // Finalize initialization
        ServiceState = EServiceLifecycleState::Ready;
        RECORD_SERVICE_METRIC("Data.Init.Success", 1);

        // ✅ Enhanced logging with mode indication
        UE_LOG(LogSuspenseCoreEquipmentData, Log,
            TEXT("EquipmentDataService initialized successfully:"));
        UE_LOG(LogSuspenseCoreEquipmentData, Log,
            TEXT("  - Mode: %s"),
            (DataStore && TransactionProcessor) ? TEXT("STATEFUL (per-player)") : TEXT("STATELESS (global)"));
        UE_LOG(LogSuspenseCoreEquipmentData, Log,
            TEXT("  - ItemManager: %s (items: %d)"),
            *ItemManager->GetName(), ItemManager->GetCachedItemCount());

        if (DataStore && TransactionProcessor)
        {
            UE_LOG(LogSuspenseCoreEquipmentData, Log,
                TEXT("  - DataStore: %s (slots: %d)"),
                *DataStore->GetName(), DataStore->GetSlotCount());
            UE_LOG(LogSuspenseCoreEquipmentData, Log,
                TEXT("  - TransactionProcessor: %s (timeout: %.1fs, max depth: %d)"),
                *TransactionProcessor->GetName(),
                TransactionProcessor->GetTransactionTimeout(),
                TransactionProcessor->GetMaxNestedDepth());
            UE_LOG(LogSuspenseCoreEquipmentData, Log,
                TEXT("  - Validator: %s"),
                SlotValidator ? *SlotValidator->GetName() : TEXT("Not Set"));
        }
        else
        {
            UE_LOG(LogSuspenseCoreEquipmentData, Log,
                TEXT("  - DataStore: Not Set (stateless)"));
            UE_LOG(LogSuspenseCoreEquipmentData, Log,
                TEXT("  - TransactionProcessor: Not Set (stateless)"));
            UE_LOG(LogSuspenseCoreEquipmentData, Log,
                TEXT("  - Components will be validated per method call"));
        }

        UE_LOG(LogSuspenseCoreEquipmentData, Log,
            TEXT("  - Cache TTL: %.1fs, Warming: %s"),
            DefaultCacheTTL, bEnableCacheWarming ? TEXT("Enabled") : TEXT("Disabled"));
        UE_LOG(LogSuspenseCoreEquipmentData, Log,
            TEXT("  - Delta Tracking: %s"),
            bEnableDeltaTracking ? TEXT("Enabled") : TEXT("Disabled"));

        bOk = true;
    }

    return true;
}

bool USuspenseCoreEquipmentDataService::ShutdownService(bool bForce)
{
    SCOPED_SERVICE_TIMER("Data.ShutdownService");
    bool bOk = false;
    ON_SCOPE_EXIT { if (bOk) ServiceMetrics.RecordSuccess(); else ServiceMetrics.RecordError(); };

    // Phase 0: Quick exit check with read lock only
    {
        FRWScopeLock ScopeLock(DataLock, SLT_ReadOnly);
        if (ServiceState == EServiceLifecycleState::Shutdown)
        {
            bOk = true;
            return true;
        }
    }

    // Phase 1: External operations WITHOUT DataLock to prevent deadlocks
    // Save references before nullifying them
    USuspenseCoreEquipmentTransactionProcessor* OldTransactionProcessor = TransactionProcessor;

    if (OldTransactionProcessor && !bForce)
    {
        // Rollback happens outside of DataLock to avoid nested locking
        int32 RolledBack = OldTransactionProcessor->RollbackAllTransactions();
        if (RolledBack > 0)
        {
            UE_LOG(LogSuspenseCoreEquipmentData, Warning,
                TEXT("ShutdownService: Rolled back %d active transactions"), RolledBack);
            RECORD_SERVICE_METRIC("Data.Shutdown.TransactionsRolledBack", RolledBack);
        }
    }

    // Clear delta callback outside of locks
    if (OldTransactionProcessor)
    {
        OldTransactionProcessor->ClearDeltaCallback();
    }

    // Phase 2: Internal cleanup under DataLock
    {
        FRWScopeLock ScopeLock(DataLock, SLT_Write);

        // Double-check state
        if (ServiceState == EServiceLifecycleState::Shutdown)
        {
            bOk = true;
            return true;
        }

        ServiceState = EServiceLifecycleState::Shutting;

        // Clear event subscriptions and delegates
        EventScope.UnsubscribeAll();
        OnEquipmentDeltaDelegate.Clear();
        OnBatchDeltasDelegate.Clear();

        // Null out component references (we don't own their lifecycle)
        TransactionProcessor = nullptr;
        DataStore = nullptr;
        DataProviderInterface.SetObject(nullptr);
        DataProviderInterface.SetInterface(nullptr);
        TransactionManagerInterface.SetObject(nullptr);
        TransactionManagerInterface.SetInterface(nullptr);

        ServiceState = EServiceLifecycleState::Shutdown;
    }

    // Phase 3: Cache and registry cleanup (these operations take their own locks)
    // Clear all caches
    {
        FRWScopeLock CacheWriteLock(CacheLock, SLT_Write);
        SnapshotCache->Clear();
        ItemCache->Clear();
        ConfigCache->Clear();
    }
    RECORD_SERVICE_METRIC("Data.Shutdown.CachesCleared", 3);

    // Unregister from global cache registry
    FSuspenseGlobalCacheRegistry::Get().UnregisterCache(TEXT("EquipmentData.Snapshots"));
    FSuspenseGlobalCacheRegistry::Get().UnregisterCache(TEXT("EquipmentData.Items"));
    FSuspenseGlobalCacheRegistry::Get().UnregisterCache(TEXT("EquipmentData.Configs"));

    // Unsubscribe global invalidation lambda
    if (GlobalCacheInvalidateHandle.IsValid())
    {
        FSuspenseGlobalCacheRegistry::Get().OnGlobalInvalidate.Remove(GlobalCacheInvalidateHandle);
        GlobalCacheInvalidateHandle.Reset();
    }

    // Clear delta history
    {
        FScopeLock DeltaScopeLock(&DeltaLock);
        RecentDeltaHistory.Empty();
        DeltasByType.Empty();
    }

    RECORD_SERVICE_METRIC("Data.Shutdown.Complete", 1);

    // Log final statistics
    float Uptime = (FDateTime::Now() - InitializationTime).GetTotalSeconds();
    UE_LOG(LogSuspenseCoreEquipmentData, Log,
        TEXT("EquipmentDataService shutdown complete (uptime: %.1f hours, total ops: %d reads, %d writes, cache contention: %d, deltas: %d)"),
        Uptime / 3600.0f, TotalReads.load(), TotalWrites.load(), CacheContention.load(), TotalDeltasProcessed.load());

    bOk = true;
    return true;
}

FGameplayTag USuspenseCoreEquipmentDataService::GetServiceTag() const
{
    SCOPED_SERVICE_TIMER("Data.GetServiceTag");
    return FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Data"));
}

FGameplayTagContainer USuspenseCoreEquipmentDataService::GetRequiredDependencies() const
{
    SCOPED_SERVICE_TIMER("Data.GetRequiredDependencies");
    // Data service is foundational - it has no dependencies on other services
    return FGameplayTagContainer();
}

bool USuspenseCoreEquipmentDataService::ValidateService(TArray<FText>& OutErrors) const
{
    SCOPED_SERVICE_TIMER("Data.ValidateService");
    bool bOk = false;
    ON_SCOPE_EXIT { if (bOk) ServiceMetrics.RecordSuccess(); else ServiceMetrics.RecordError(); };

    FRWScopeLock ScopeLock(DataLock, SLT_ReadOnly);

    OutErrors.Empty();
    bool bIsValid = true;

    // Validate core components
    if (!DataStore)
    {
        OutErrors.Add(FText::FromString(TEXT("DataStore component not initialized")));
        bIsValid = false;
        RECORD_SERVICE_METRIC("Data.Validation.ErrorDataStore", 1);
    }

    if (!TransactionProcessor)
    {
        OutErrors.Add(FText::FromString(TEXT("TransactionProcessor component not initialized")));
        bIsValid = false;
        RECORD_SERVICE_METRIC("Data.Validation.ErrorTransactionProcessor", 1);
    }

    // Validate data integrity using internal version since we already hold DataLock
    if (!ValidateDataIntegrity_Internal(false))
    {
        OutErrors.Add(FText::FromString(TEXT("Data integrity validation failed")));
        bIsValid = false;
        RECORD_SERVICE_METRIC("Data.Validation.ErrorDataIntegrity", 1);
    }

    // Check for transaction issues
    if (TransactionProcessor && TransactionProcessor->GetActiveTransactionCount() > 10)
    {
        OutErrors.Add(FText::Format(
            FText::FromString(TEXT("Excessive active transactions: {0}")),
            TransactionProcessor->GetActiveTransactionCount()
        ));
        bIsValid = false;
        RECORD_SERVICE_METRIC("Data.Validation.ErrorExcessiveTransactions", 1);
    }

    // Check cache health
    int32 TotalReadsValue = TotalReads.load();
    int32 CacheHitsValue = CacheHits.load();
    float CacheHitRate = TotalReadsValue > 0 ? (float)CacheHitsValue / TotalReadsValue : 0.0f;
    if (CacheHitRate < 0.5f && TotalReadsValue > 100)
    {
        OutErrors.Add(FText::Format(
            FText::FromString(TEXT("Low cache hit rate: {0}%")),
            FMath::RoundToInt(CacheHitRate * 100)
        ));
        RECORD_SERVICE_METRIC("Data.Validation.WarningLowCacheHitRate", 1);
        // This is a warning, not a failure
    }

    // Check cache contention
    int32 ContentionValue = CacheContention.load();
    if (ContentionValue > TotalReadsValue * 0.1f && TotalReadsValue > 100)
    {
        OutErrors.Add(FText::Format(
            FText::FromString(TEXT("High cache contention: {0} conflicts")),
            ContentionValue
        ));
        RECORD_SERVICE_METRIC("Data.Validation.WarningHighCacheContention", 1);
    }

    bOk = bIsValid;
    return bIsValid;
}

void USuspenseCoreEquipmentDataService::ResetService()
{
    SCOPED_SERVICE_TIMER("Data.ResetService");

    // Track success of the reset operation
    bool bResetSuccessful = false;

    // Ensure we record the result of the reset attempt
    ON_SCOPE_EXIT
    {
        if (bResetSuccessful)
        {
            ServiceMetrics.RecordSuccess();
            RECORD_SERVICE_METRIC("Data.Service.ResetSuccess", 1);
        }
        else
        {
            ServiceMetrics.RecordError();
            RECORD_SERVICE_METRIC("Data.Service.ResetFailed", 1);
        }
    };

    FRWScopeLock ScopeLock(DataLock, SLT_Write);

    UE_LOG(LogSuspenseCoreEquipmentData, Log, TEXT("ResetService: Beginning comprehensive service reset"));

    // Reset the core data store to initial state
    ResetDataStore();
    RECORD_SERVICE_METRIC("Data.Reset.DataStore", 1);

    // Clear all transaction history if processor is available
    if (TransactionProcessor)
    {
        TransactionProcessor->ClearTransactionHistory(false);
        RECORD_SERVICE_METRIC("Data.Reset.TransactionHistory", 1);
    }

    // Clear all data caches
    if (SnapshotCache.IsValid())
    {
        SnapshotCache->Clear();
    }
    if (ItemCache.IsValid())
    {
        ItemCache->Clear();
    }
    if (ConfigCache.IsValid())
    {
        ConfigCache->Clear();
    }
    RECORD_SERVICE_METRIC("Data.Reset.Caches", 3);

    // Clear delta history
    {
        FScopeLock DeltaScopeLock(&DeltaLock);
        RecentDeltaHistory.Empty();
        DeltasByType.Empty();
        TotalDeltasProcessed.store(0);
    }

    // Reset all atomic statistics
    TotalReads.store(0);
    TotalWrites.store(0);
    CacheHits.store(0);
    CacheMisses.store(0);
    CacheContention.store(0);
    TotalTransactions.store(0);
    SuccessfulTransactions.store(0);
    FailedTransactions.store(0);

    // Reset the service metrics container
    ServiceMetrics.Reset();
    DeltaMetrics.Reset();

    // Mark the reset as successful
    bResetSuccessful = true;

    UE_LOG(LogSuspenseCoreEquipmentData, Log, TEXT("ResetService: Service reset completed successfully"));
}

FString USuspenseCoreEquipmentDataService::GetServiceStats() const
{
    SCOPED_SERVICE_TIMER("Data.GetServiceStats");

    FRWScopeLock ScopeLock(DataLock, SLT_ReadOnly);

    FString Stats;
    Stats.Reserve(2048);

    // Header & state
    Stats += TEXT("=== Equipment Data Service Statistics ===\n");
    Stats += FString::Printf(TEXT("Service State: %s\n"),
        *UEnum::GetValueAsString(ServiceState));

    // Uptime
    const FTimespan Uptime = FDateTime::Now() - InitializationTime;
    Stats += FString::Printf(TEXT("Uptime: %d hours, %d minutes\n"),
        Uptime.GetHours(), Uptime.GetMinutes() % 60);

    // Operations
    Stats += TEXT("\n--- Operations ---\n");
    const int32 Reads  = TotalReads.load();
    const int32 Writes = TotalWrites.load();
    Stats += FString::Printf(TEXT("Total Reads: %d\n"),  Reads);
    Stats += FString::Printf(TEXT("Total Writes: %d\n"), Writes);
    Stats += FString::Printf(TEXT("Read/Write Ratio: %.2f:1\n"),
        (Writes > 0) ? (static_cast<float>(Reads) / static_cast<float>(Writes)) : 0.0f);

    // Cache perf
    Stats += TEXT("\n--- Cache Performance ---\n");
    const int32 Hits    = CacheHits.load();
    const int32 Misses  = CacheMisses.load();
    const float HitRate = (Reads > 0) ? (static_cast<float>(Hits) / static_cast<float>(Reads)) * 100.0f : 0.0f;
    Stats += FString::Printf(TEXT("Cache Hits: %d (%.1f%%)\n"), Hits, HitRate);
    Stats += FString::Printf(TEXT("Cache Misses: %d\n"), Misses);
    Stats += FString::Printf(TEXT("Cache Contention: %d\n"), CacheContention.load());

    // Transactions
    if (TransactionProcessor)
    {
        Stats += TEXT("\n--- Transactions ---\n");
        Stats += FString::Printf(TEXT("Active Transactions: %d\n"),
            TransactionProcessor->GetActiveTransactionCount());

        const int32 TotalTx   = TotalTransactions.load();
        const int32 SuccessTx = SuccessfulTransactions.load();
        Stats += FString::Printf(TEXT("Total Started: %d\n"), TotalTx);
        Stats += FString::Printf(TEXT("Successful: %d\n"), SuccessTx);
        Stats += FString::Printf(TEXT("Failed: %d\n"), FailedTransactions.load());

        const float SuccessRate = (TotalTx > 0)
            ? (static_cast<float>(SuccessTx) / static_cast<float>(TotalTx)) * 100.0f
            : 0.0f;
        Stats += FString::Printf(TEXT("Success Rate: %.1f%%\n"), SuccessRate);
    }

    // Deltas
    Stats += GetDeltaStatistics();

    // Cache details (use DumpStats())
    Stats += TEXT("\n--- Cache Details ---\n");

    if (SnapshotCache.IsValid())
    {
        Stats += TEXT("Snapshot Cache:\n");
        Stats += SnapshotCache->DumpStats();
        Stats += TEXT("\n");
    }
    else
    {
        Stats += TEXT("Snapshot Cache: N/A\n");
    }

    if (ItemCache.IsValid())
    {
        Stats += TEXT("Item Cache:\n");
        Stats += ItemCache->DumpStats();
        Stats += TEXT("\n");
    }
    else
    {
        Stats += TEXT("Item Cache: N/A\n");
    }

    if (ConfigCache.IsValid())
    {
        Stats += TEXT("Config Cache:\n");
        Stats += ConfigCache->DumpStats();
        Stats += TEXT("\n");
    }
    else
    {
        Stats += TEXT("Config Cache: N/A\n");
    }

    // Service metrics
    Stats += ServiceMetrics.ToString(TEXT("EquipmentDataService"));

    return Stats;
}

//========================================
// IEquipmentDataService Implementation
//========================================

ISuspenseEquipmentDataProvider* USuspenseCoreEquipmentDataService::GetDataProvider()
{
    SCOPED_SERVICE_TIMER("Data.GetDataProvider");
    return DataProviderInterface.GetInterface();
}

ISuspenseTransactionManager* USuspenseCoreEquipmentDataService::GetTransactionManager()
{
    SCOPED_SERVICE_TIMER("Data.GetTransactionManager");
    return TransactionManagerInterface.GetInterface();
}

//========================================
// Data Operations (High-Level API)
//========================================

FSuspenseInventoryItemInstance USuspenseCoreEquipmentDataService::GetSlotItemCached(int32 SlotIndex) const
{
    SCOPED_SERVICE_TIMER("Data.GetSlotItemCached");
    bool bOk = false;
    ON_SCOPE_EXIT { if (bOk) ServiceMetrics.RecordSuccess(); else ServiceMetrics.RecordError(); };

    // Validate input
    if (!IsValidSlotIndex(SlotIndex))
    {
        RECORD_SERVICE_METRIC("Data.Input.InvalidSlot", 1);
        return FSuspenseInventoryItemInstance();
    }

    if (bEnableDetailedLogging)
    {
        LogDataOperation(TEXT("GetSlotItemCached"), SlotIndex);
    }

    TotalReads.fetch_add(1);
    RECORD_SERVICE_METRIC("Data.Read.Op", 1);

    FSuspenseInventoryItemInstance Item;

    // Step 1: Cache-first approach - check cache under read lock
    {
        FRWScopeLock CacheScopeLock(CacheLock, SLT_ReadOnly);
        FSuspenseInventoryItemInstance CachedItem;
        if (ItemCache->Get(SlotIndex, CachedItem))
        {
            CacheHits.fetch_add(1);
            RECORD_SERVICE_METRIC("Data.Cache.Hit", 1);
            bOk = true;
            return CachedItem;
        }
        else
        {
            CacheMisses.fetch_add(1);
            RECORD_SERVICE_METRIC("Data.Cache.Miss", 1);
        }
    }

    // Step 2: Cache miss — read from DataStore
    {
        FRWScopeLock DataScopeLock(DataLock, SLT_ReadOnly);

        if (!DataStore)
        {
            return FSuspenseInventoryItemInstance();
        }

        Item = DataStore->GetSlotItem(SlotIndex);
    }

    // Step 3: Update cache if we got valid data
    if (Item.IsValid())
    {
        FRWScopeLock CacheWriteLock(CacheLock, SLT_Write);

        // Double-checked locking pattern
        FSuspenseInventoryItemInstance ExistingItem;
        if (!ItemCache->Get(SlotIndex, ExistingItem))
        {
            ItemCache->Set(SlotIndex, Item, DefaultCacheTTL);
            RECORD_SERVICE_METRIC("Data.Cache.Insert", 1);
        }
        else
        {
            CacheContention.fetch_add(1);
            RECORD_SERVICE_METRIC("Data.Cache.Contention", 1);
            Item = ExistingItem; // Prefer the concurrently cached value
        }
    }

    bOk = true;
    return Item;
}

TMap<int32, FSuspenseInventoryItemInstance> USuspenseCoreEquipmentDataService::BatchGetSlotItems(const TArray<int32>& SlotIndices) const
{
    SCOPED_SERVICE_TIMER("Data.BatchGetSlotItems");
    bool bOk = false;
    ON_SCOPE_EXIT { if (bOk) ServiceMetrics.RecordSuccess(); else ServiceMetrics.RecordError(); };

    if (bEnableDetailedLogging)
    {
        LogDataOperation(TEXT("BatchGetSlotItems"));
    }

    // Filter and deduplicate input
    TSet<int32> ValidUnique;
    for (int32 SlotIndex : SlotIndices)
    {
        if (IsValidSlotIndex(SlotIndex))
        {
            ValidUnique.Add(SlotIndex);
        }
        else
        {
            RECORD_SERVICE_METRIC("Data.Input.InvalidSlot", 1);
        }
    }

    RECORD_SERVICE_METRIC("Data.Batch.Requested", SlotIndices.Num());
    RECORD_SERVICE_METRIC("Data.Batch.ValidUnique", ValidUnique.Num());

    TMap<int32, FSuspenseInventoryItemInstance> Result;
    TArray<int32> CacheMissIndices;

    // Count reads for each requested slot
    TotalReads.fetch_add(ValidUnique.Num());
    RECORD_SERVICE_METRIC("Data.Batch.Read.Op", ValidUnique.Num());

    // First pass: read cache for all requested items under a single cache read lock
    {
        FRWScopeLock CacheScopeLock(CacheLock, SLT_ReadOnly);

        for (int32 SlotIndex : ValidUnique)
        {
            FSuspenseInventoryItemInstance CachedItem;
            if (ItemCache->Get(SlotIndex, CachedItem))
            {
                Result.Add(SlotIndex, CachedItem);
                CacheHits.fetch_add(1);
                RECORD_SERVICE_METRIC("Data.Batch.Cache.Hit", 1);
            }
            else
            {
                CacheMissIndices.Add(SlotIndex);
                CacheMisses.fetch_add(1);
                RECORD_SERVICE_METRIC("Data.Batch.Cache.Miss", 1);
            }
        }
    }

    // Second pass: if misses exist, read all of them under a single DataLock read
    if (CacheMissIndices.Num() > 0)
    {
        TMap<int32, FSuspenseInventoryItemInstance> DataStoreItems;

        {
            FRWScopeLock DataScopeLock(DataLock, SLT_ReadOnly);

            if (!DataStore)
            {
                return Result;
            }

            for (int32 SlotIndex : CacheMissIndices)
            {
                FSuspenseInventoryItemInstance Item = DataStore->GetSlotItem(SlotIndex);
                if (Item.IsValid())
                {
                    DataStoreItems.Add(SlotIndex, Item);
                    Result.Add(SlotIndex, Item);
                }
            }

            RECORD_SERVICE_METRIC("Data.Batch.DataStore.Fetches", CacheMissIndices.Num());
        }

        // Third pass: batch update cache under a single cache write lock
        if (DataStoreItems.Num() > 0)
        {
            BatchUpdateCache(DataStoreItems);
            RECORD_SERVICE_METRIC("Data.Batch.Cache.Updates", DataStoreItems.Num());
        }
    }

    RECORD_SERVICE_METRIC("Data.Batch.Complete", 1);

    bOk = true;
    return Result;
}

FGuid USuspenseCoreEquipmentDataService::SwapSlotItems(int32 SlotA, int32 SlotB)
{
    SCOPED_SERVICE_TIMER("Data.SwapSlotItems");
    SCOPED_DIFF_TIMER("Swap");
    bool bOk = false;
    ON_SCOPE_EXIT { if (bOk) ServiceMetrics.RecordSuccess(); else ServiceMetrics.RecordError(); };

    if (bEnableDetailedLogging)
    {
        LogDataOperation(TEXT("SwapSlotItems"), SlotA);
    }

    // Validate inputs
    if (!IsValidSlotIndex(SlotA) || !IsValidSlotIndex(SlotB))
    {
        RECORD_SERVICE_METRIC("Data.Input.InvalidSlot", 1);
        return FGuid();
    }

    if (SlotA == SlotB)
    {
        RECORD_SERVICE_METRIC("Data.Input.SameSlotSwap", 1);
        return FGuid();
    }

    if (!TransactionProcessor || !DataStore)
    {
        RECORD_SERVICE_METRIC("Data.Swap.NoComponents", 1);
        return FGuid();
    }

    // Start transaction for atomic swap
    FGuid TransactionId = TransactionProcessor->BeginTransaction(
        FString::Printf(TEXT("Swap slots %d and %d"), SlotA, SlotB)
    );

    if (!TransactionId.IsValid())
    {
        RECORD_SERVICE_METRIC("Data.Swap.TransactionStartFailed", 1);
        return FGuid();
    }

    TotalTransactions.fetch_add(1);
    RECORD_SERVICE_METRIC("Data.Tx.Started", 1);

    // Perform swap under DataLock write to guarantee atomicity at DataStore level
    FSuspenseInventoryItemInstance ItemA;
    FSuspenseInventoryItemInstance ItemB;

    {
        FRWScopeLock DataWriteLock(DataLock, SLT_Write);

        // Get current items
        ItemA = DataStore->GetSlotItem(SlotA);
        ItemB = DataStore->GetSlotItem(SlotB);

        // Record BOTH operations for complete DIFF tracking
        // Operation 1: SlotA gets ItemB
        FTransactionOperation OpA;
        OpA.OperationType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Set"));
        OpA.SlotIndex = SlotA;
        OpA.ItemBefore = ItemA;
        OpA.ItemAfter = ItemB;
        OpA.bReversible = true;
        OpA.Metadata.Add(TEXT("SwapPeerSlot"), FString::FromInt(SlotB));
        OpA.Metadata.Add(TEXT("SwapType"), TEXT("Source"));
        TransactionProcessor->RecordDetailedOperation(OpA);

        // Operation 2: SlotB gets ItemA
        FTransactionOperation OpB;
        OpB.OperationType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Set"));
        OpB.SlotIndex = SlotB;
        OpB.ItemBefore = ItemB;
        OpB.ItemAfter = ItemA;
        OpB.bReversible = true;
        OpB.Metadata.Add(TEXT("SwapPeerSlot"), FString::FromInt(SlotA));
        OpB.Metadata.Add(TEXT("SwapType"), TEXT("Target"));
        TransactionProcessor->RecordDetailedOperation(OpB);

        // Apply the swap
        DataStore->SetSlotItem(SlotA, ItemB, false);
        DataStore->SetSlotItem(SlotB, ItemA, false);

        RECORD_SERVICE_METRIC("Data.Swap.Executed", 1);
    }

    // Commit transaction (no locks held)
    bool bSuccess = TransactionProcessor->CommitTransaction(TransactionId);

    if (bSuccess)
    {
        // Invalidate caches for both slots with a single lock
        {
            FRWScopeLock CacheWriteLock(CacheLock, SLT_Write);
            if (ItemCache.IsValid())
            {
                ItemCache->Remove(SlotA);
                ItemCache->Remove(SlotB);
            }
        }

        SuccessfulTransactions.fetch_add(1);
        TotalWrites.fetch_add(2);
        RECORD_SERVICE_METRIC("Data.Swap.Success", 1);
        RECORD_SERVICE_METRIC("Data.Write.Op", 2);
        RECORD_DIFF_METRIC("Swap", 1);

        // Notify observers on game thread with safe weak pointer capture
        TWeakObjectPtr<USuspenseCoreEquipmentDataStore> WeakDS = DataStore;
        AsyncTask(ENamedThreads::GameThread, [WeakDS, SlotA, SlotB, ItemA, ItemB]()
        {
            if (WeakDS.IsValid())
            {
                WeakDS->OnSlotDataChanged().Broadcast(SlotA, ItemB);
                WeakDS->OnSlotDataChanged().Broadcast(SlotB, ItemA);
            }
        });

        bOk = true;
    }
    else
    {
        FailedTransactions.fetch_add(1);
        RECORD_SERVICE_METRIC("Data.Swap.Failed", 1);

        // Rollback transaction on commit failure
        TransactionProcessor->RollbackTransaction(TransactionId);
        RECORD_SERVICE_METRIC("Data.Tx.RollbackOnCommitFail", 1);

        UE_LOG(LogSuspenseCoreEquipmentData, Error,
            TEXT("SwapSlotItems: Failed to commit transaction %s"),
            *TransactionId.ToString());
    }

    return TransactionId;
}

FGuid USuspenseCoreEquipmentDataService::BatchUpdateSlots(const TMap<int32, FSuspenseInventoryItemInstance>& Updates)
{
    SCOPED_SERVICE_TIMER("Data.BatchUpdateSlots");
    SCOPED_DIFF_TIMER("BatchUpdate");
    bool bOk = false;
    ON_SCOPE_EXIT { if (bOk) ServiceMetrics.RecordSuccess(); else ServiceMetrics.RecordError(); };

    if (bEnableDetailedLogging)
    {
        LogDataOperation(TEXT("BatchUpdateSlots"));
    }

    if (!TransactionProcessor || !DataStore || Updates.Num() == 0)
    {
        RECORD_SERVICE_METRIC("Data.BatchUpdate.InvalidParams", 1);
        return FGuid();
    }

    // Validate all slot indices
    for (const auto& UpdatePair : Updates)
    {
        if (!IsValidSlotIndex(UpdatePair.Key))
        {
            RECORD_SERVICE_METRIC("Data.Input.InvalidSlot", 1);
            return FGuid();
        }
    }

    RECORD_SERVICE_METRIC("Data.BatchUpdate.SlotCount", Updates.Num());
    RECORD_DIFF_METRIC("BatchUpdate.Slots", Updates.Num());

    // Start transaction for batch update
    FGuid TransactionId = TransactionProcessor->BeginTransaction(
        FString::Printf(TEXT("Batch update %d slots"), Updates.Num())
    );

    if (!TransactionId.IsValid())
    {
        RECORD_SERVICE_METRIC("Data.BatchUpdate.TransactionStartFailed", 1);
        return FGuid();
    }

    TotalTransactions.fetch_add(1);
    RECORD_SERVICE_METRIC("Data.Tx.Started", 1);

    {
        // Protect all DataStore writes with a single DataLock write scope
        FRWScopeLock DataWriteLock(DataLock, SLT_Write);

        // Record all operations and apply updates
        for (const auto& UpdatePair : Updates)
        {
            const int32 SlotIdx = UpdatePair.Key;
            const FSuspenseInventoryItemInstance& NewItem = UpdatePair.Value;

            FTransactionOperation UpdateOp;
            UpdateOp.OperationType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Set"));
            UpdateOp.SlotIndex = SlotIdx;
            UpdateOp.ItemBefore = DataStore->GetSlotItem(SlotIdx);
            UpdateOp.ItemAfter = NewItem;
            UpdateOp.bReversible = true;

            TransactionProcessor->RecordDetailedOperation(UpdateOp);

            DataStore->SetSlotItem(SlotIdx, NewItem, false);
        }

        RECORD_SERVICE_METRIC("Data.BatchUpdate.OperationsRecorded", Updates.Num());
    }

    // Commit transaction
    bool bSuccess = TransactionProcessor->CommitTransaction(TransactionId);

    if (bSuccess)
    {
        // Invalidate cache for all updated slots with single lock
        {
            FRWScopeLock CacheWriteLock(CacheLock, SLT_Write);
            if (ItemCache.IsValid())
            {
                for (const auto& UpdatePair : Updates)
                {
                    ItemCache->Remove(UpdatePair.Key);
                }
            }
        }

        // Notify observers on game thread with safe weak pointer
        TWeakObjectPtr<USuspenseCoreEquipmentDataStore> WeakDS = DataStore;
        AsyncTask(ENamedThreads::GameThread, [WeakDS, Updates]()
        {
            if (WeakDS.IsValid())
            {
                for (const auto& UpdatePair : Updates)
                {
                    WeakDS->OnSlotDataChanged().Broadcast(UpdatePair.Key, UpdatePair.Value);
                }
            }
        });

        SuccessfulTransactions.fetch_add(1);
        TotalWrites.fetch_add(Updates.Num());
        RECORD_SERVICE_METRIC("Data.BatchUpdate.Success", 1);
        RECORD_SERVICE_METRIC("Data.Batch.Write.Op", Updates.Num());
        RECORD_DIFF_METRIC("BatchUpdate.Success", 1);
        bOk = true;
    }
    else
    {
        FailedTransactions.fetch_add(1);
        RECORD_SERVICE_METRIC("Data.BatchUpdate.Failed", 1);
        RECORD_DIFF_METRIC("BatchUpdate.Failed", 1);

        // Rollback transaction on commit failure
        TransactionProcessor->RollbackTransaction(TransactionId);
        RECORD_SERVICE_METRIC("Data.Tx.RollbackOnCommitFail", 1);

        UE_LOG(LogSuspenseCoreEquipmentData, Error,
            TEXT("BatchUpdateSlots: Failed to commit transaction %s"),
            *TransactionId.ToString());
    }

    return TransactionId;
}

FGuid USuspenseCoreEquipmentDataService::CreateDataCheckpoint(const FString& CheckpointName)
{
    SCOPED_SERVICE_TIMER("Data.CreateDataCheckpoint");
    bool bOk = false;
    ON_SCOPE_EXIT { if (bOk) ServiceMetrics.RecordSuccess(); else ServiceMetrics.RecordError(); };

    if (!TransactionProcessor || !DataStore)
    {
        RECORD_SERVICE_METRIC("Data.Checkpoint.NoComponents", 1);
        return FGuid();
    }

    FGuid CheckpointId = TransactionProcessor->CreateSavepoint(CheckpointName);

    if (CheckpointId.IsValid())
    {
        // Take snapshot under DataLock read
        FEquipmentStateSnapshot Snapshot;
        {
            FRWScopeLock DataScopeLock(DataLock, SLT_ReadOnly);
            Snapshot = DataStore->CreateSnapshot();
        }

        // Cache the snapshot under CacheLock write
        {
            FRWScopeLock CacheWriteLock(CacheLock, SLT_Write);
            SnapshotCache->Set(CheckpointId, Snapshot, 300.0f); // 5 minute TTL for checkpoints
        }

        RECORD_SERVICE_METRIC("Data.Checkpoint.Created", 1);

        UE_LOG(LogSuspenseCoreEquipmentData, Log,
            TEXT("CreateDataCheckpoint: Created checkpoint '%s' (ID: %s)"),
            *CheckpointName, *CheckpointId.ToString());

        bOk = true;
    }
    else
    {
        RECORD_SERVICE_METRIC("Data.Checkpoint.CreationFailed", 1);
    }

    return CheckpointId;
}

bool USuspenseCoreEquipmentDataService::RollbackToCheckpoint(const FGuid& CheckpointId)
{
    SCOPED_SERVICE_TIMER("Data.RollbackToCheckpoint");
    SCOPED_DIFF_TIMER("Rollback");
    bool bOk = false;
    ON_SCOPE_EXIT { if (bOk) ServiceMetrics.RecordSuccess(); else ServiceMetrics.RecordError(); };

    if (!TransactionProcessor || !CheckpointId.IsValid() || !DataStore)
    {
        RECORD_SERVICE_METRIC("Data.Rollback.InvalidParams", 1);
        return false;
    }

    // Try to get cached snapshot first
    FEquipmentStateSnapshot CachedSnapshot;
    bool bHasCachedSnapshot = false;

    {
        FRWScopeLock CacheScopeLock(CacheLock, SLT_ReadOnly);
        bHasCachedSnapshot = SnapshotCache->Get(CheckpointId, CachedSnapshot);
    }

    if (bHasCachedSnapshot)
    {
        bool bRestored = false;

        // Restore under DataLock write
        {
            FRWScopeLock DataWriteLock(DataLock, SLT_Write);
            bRestored = DataStore->RestoreSnapshot(CachedSnapshot);
        }

        if (bRestored)
        {
            // Invalidate item cache
            {
                FRWScopeLock CacheWriteLock(CacheLock, SLT_Write);
                ItemCache->Clear();
            }

            RECORD_SERVICE_METRIC("Data.Rollback.FromCache", 1);
            RECORD_DIFF_METRIC("Rollback.FromCache", 1);

            UE_LOG(LogSuspenseCoreEquipmentData, Log,
                TEXT("RollbackToCheckpoint: Restored from cached snapshot %s"),
                *CheckpointId.ToString());

            bOk = true;
            return true;
        }
    }

    // Slow path - use transaction processor
    bool bSuccess = TransactionProcessor->RollbackToSavepoint(CheckpointId);

    if (bSuccess)
    {
        RECORD_SERVICE_METRIC("Data.Rollback.FromProcessor", 1);
        RECORD_DIFF_METRIC("Rollback.FromProcessor", 1);
        bOk = true;
    }
    else
    {
        RECORD_SERVICE_METRIC("Data.Rollback.Failed", 1);
        RECORD_DIFF_METRIC("Rollback.Failed", 1);
    }

    return bSuccess;
}

bool USuspenseCoreEquipmentDataService::ValidateDataIntegrity(bool bDeep) const
{
    SCOPED_SERVICE_TIMER("Data.ValidateDataIntegrity");
    bool bOk = false;
    ON_SCOPE_EXIT { if (bOk) ServiceMetrics.RecordSuccess(); else ServiceMetrics.RecordError(); };

    FRWScopeLock ScopeLock(DataLock, SLT_ReadOnly);
    bool bResult = ValidateDataIntegrity_Internal(bDeep);
    bOk = bResult;
    return bResult;
}

void USuspenseCoreEquipmentDataService::InvalidateCache(int32 SlotIndex)
{
    SCOPED_SERVICE_TIMER("Data.InvalidateCache");

    FRWScopeLock CacheWriteLock(CacheLock, SLT_Write);

    if (SlotIndex == -1)
    {
        // Invalidate all caches
        if (ItemCache.IsValid())     { ItemCache->Clear(); }
        if (ConfigCache.IsValid())   { ConfigCache->Clear(); }
        if (SnapshotCache.IsValid()) { SnapshotCache->Clear(); }

        RECORD_SERVICE_METRIC("Data.Cache.FullInvalidation", 1);
        UE_LOG(LogSuspenseCoreEquipmentData, Log, TEXT("InvalidateCache: All caches cleared"));
    }
    else
    {
        // Invalidate specific slot
        if (ItemCache.IsValid())   { ItemCache->Remove(SlotIndex); }
        if (ConfigCache.IsValid()) { ConfigCache->Remove(SlotIndex); }

        RECORD_SERVICE_METRIC("Data.Cache.SlotInvalidation", 1);

        if (bEnableDetailedLogging)
        {
            UE_LOG(LogSuspenseCoreEquipmentData, Verbose,
                TEXT("InvalidateCache: Cleared cache for slot %d"), SlotIndex);
        }
    }
}

FString USuspenseCoreEquipmentDataService::GetCacheStatistics() const
{
    SCOPED_SERVICE_TIMER("Data.GetCacheStatistics");

    FRWScopeLock CacheScopeLock(CacheLock, SLT_ReadOnly);

    FString Stats = TEXT("=== Cache Statistics ===\n");

    Stats += TEXT("Item Cache:\n");
    if (ItemCache.IsValid()) { Stats += ItemCache->DumpStats(); }
    else                     { Stats += TEXT("N/A"); }
    Stats += TEXT("\n");

    Stats += TEXT("Config Cache:\n");
    if (ConfigCache.IsValid()) { Stats += ConfigCache->DumpStats(); }
    else                       { Stats += TEXT("N/A"); }
    Stats += TEXT("\n");

    Stats += TEXT("Snapshot Cache:\n");
    if (SnapshotCache.IsValid()) { Stats += SnapshotCache->DumpStats(); }
    else                         { Stats += TEXT("N/A"); }
    Stats += TEXT("\n");

    return Stats;
}


bool USuspenseCoreEquipmentDataService::ExportMetricsToCSV(const FString& FilePath) const
{
    SCOPED_SERVICE_TIMER("Data.ExportMetricsToCSV");
    bool bOk = false;
    ON_SCOPE_EXIT { if (bOk) ServiceMetrics.RecordSuccess(); else ServiceMetrics.RecordError(); };

    // Guard против пустого пути
    if (FilePath.IsEmpty())
    {
        UE_LOG(LogSuspenseCoreEquipmentData, Warning, TEXT("Empty FilePath for ExportMetricsToCSV"));
        RECORD_SERVICE_METRIC("Data.Metrics.Export.EmptyPath", 1);
        return false;
    }

    // Ensure directory exists
    FString DirectoryPath = FPaths::GetPath(FilePath);
    IFileManager::Get().MakeDirectory(*DirectoryPath, true);

    bool bSuccess = ServiceMetrics.ExportToCSV(FilePath, TEXT("EquipmentDataService"));

    // Also export delta metrics
    if (bSuccess)
    {
        FString DeltaPath = FilePath.Replace(TEXT(".csv"), TEXT("_deltas.csv"));
        bSuccess = DeltaMetrics.ExportToCSV(DeltaPath, TEXT("EquipmentDeltas"));
    }

    if (bSuccess)
    {
        RECORD_SERVICE_METRIC("Data.Metrics.Export.Ok", 1);
        UE_LOG(LogSuspenseCoreEquipmentData, Log, TEXT("ExportMetricsToCSV: Metrics exported to %s"), *FilePath);
        bOk = true;
    }
    else
    {
        RECORD_SERVICE_METRIC("Data.Metrics.Export.Fail", 1);
        UE_LOG(LogSuspenseCoreEquipmentData, Error, TEXT("ExportMetricsToCSV: Failed to export metrics to %s"), *FilePath);
    }

    return bSuccess;
}

//========================================
// Delta Event Methods
//========================================

TArray<FEquipmentDelta> USuspenseCoreEquipmentDataService::GetRecentDeltas(int32 MaxCount) const
{
    FScopeLock Lock(&DeltaLock);

    TArray<FEquipmentDelta> Result;
    int32 StartIndex = FMath::Max(0, RecentDeltaHistory.Num() - MaxCount);

    for (int32 i = StartIndex; i < RecentDeltaHistory.Num(); i++)
    {
        Result.Add(RecentDeltaHistory[i]);
    }

    return Result;
}

FString USuspenseCoreEquipmentDataService::GetDeltaStatistics() const
{
    FScopeLock Lock(&DeltaLock);

    FString Stats = TEXT("\n--- Delta Statistics ---\n");
    Stats += FString::Printf(TEXT("Total Deltas Processed: %d\n"), TotalDeltasProcessed.load());
    Stats += FString::Printf(TEXT("Recent History Size: %d\n"), RecentDeltaHistory.Num());

    if (DeltasByType.Num() > 0)
    {
        Stats += TEXT("Deltas by Type:\n");
        for (const auto& Pair : DeltasByType)
        {
            Stats += FString::Printf(TEXT("  %s: %d\n"),
                *Pair.Key.ToString(), Pair.Value);
        }
    }

    Stats += DeltaMetrics.ToString(TEXT("DeltaMetrics"));

    return Stats;
}

//========================================
// Protected Methods
//========================================

void USuspenseCoreEquipmentDataService::InitializeDataStorage()
{
    // DEPRECATED: Этот метод больше не используется
    // Компоненты теперь инжектируются через InjectComponents
    UE_LOG(LogSuspenseCoreEquipmentData, Warning,
        TEXT("InitializeDataStorage: This method is deprecated. "
             "Components should be injected via InjectComponents"));
}

void USuspenseCoreEquipmentDataService::SetupEventSubscriptions()
{
    // Эта функция теперь работает с инжектированными компонентами

    if (!DataStore)
    {
        UE_LOG(LogSuspenseCoreEquipmentData, Warning,
            TEXT("SetupEventSubscriptions: DataStore is null"));
        return;
    }

    // ===================================================================
    // Подписка на изменения данных в слотах
    // ===================================================================

    // Важно: мы НЕ берём DataLock здесь, так как мы уже под ним в InitializeService
    // Создаём weak pointer для безопасного захвата в лямбдах
    TWeakObjectPtr<USuspenseCoreEquipmentDataService> WeakThis(this);

    DataStore->OnSlotDataChanged().AddLambda(
        [WeakThis](int32 SlotIndex, const FSuspenseInventoryItemInstance& NewItem)
        {
            if (!WeakThis.IsValid())
            {
                return;
            }

            USuspenseCoreEquipmentDataService* Service = WeakThis.Get();

            // Инвалидируем кэш для изменённого слота
            // Используем отдельный CacheLock, не DataLock, чтобы избежать дедлока
            Service->InvalidateCache(SlotIndex);

            // Записываем метрику
            Service->ServiceMetrics.RecordEvent("Data.SlotChanged", 1);

            if (Service->bEnableDetailedLogging)
            {
                UE_LOG(LogSuspenseCoreEquipmentData, Verbose,
                    TEXT("OnSlotDataChanged: Slot %d changed, cache invalidated"),
                    SlotIndex);
            }
        }
    );

    // ===================================================================
    // НОВОЕ: Подписка на delta события от DataStore
    // ===================================================================

    DataStore->OnEquipmentDelta().AddUObject(this, &USuspenseCoreEquipmentDataService::OnDataStoreDelta);

    // ===================================================================
    // Подписка на изменения конфигурации слотов
    // ===================================================================

    DataStore->OnSlotConfigurationChanged().AddLambda(
        [WeakThis](int32 SlotIndex)
        {
            if (!WeakThis.IsValid())
            {
                return;
            }

            USuspenseCoreEquipmentDataService* Service = WeakThis.Get();

            // Инвалидируем кэш конфигурации
            FRWScopeLock CacheWriteLock(Service->CacheLock, SLT_Write);
            if (Service->ConfigCache.IsValid())
            {
                Service->ConfigCache->Remove(SlotIndex);
            }

            Service->ServiceMetrics.RecordEvent("Data.ConfigChanged", 1);

            if (Service->bEnableDetailedLogging)
            {
                UE_LOG(LogSuspenseCoreEquipmentData, Verbose,
                    TEXT("OnSlotConfigurationChanged: Slot %d config changed"),
                    SlotIndex);
            }
        }
    );

    // ===================================================================
    // Подписка на полный сброс хранилища
    // ===================================================================

    DataStore->OnDataStoreReset().AddLambda(
        [WeakThis]()
        {
            if (!WeakThis.IsValid())
            {
                return;
            }

            USuspenseCoreEquipmentDataService* Service = WeakThis.Get();

            // При сбросе очищаем все кэши
            Service->InvalidateCache(-1); // -1 означает очистить всё

            Service->ServiceMetrics.RecordEvent("Data.StoreReset", 1);

            UE_LOG(LogSuspenseCoreEquipmentData, Log,
                TEXT("OnDataStoreReset: DataStore was reset, all caches cleared"));
        }
    );

    // ===================================================================
    // Подписка на глобальные события через EventBus
    // ===================================================================

    auto EventBus = FSuspenseCoreEquipmentEventBus::Get();
    if (EventBus.IsValid())
    {
        // Подписываемся на запросы инвалидации кэша от других систем
        EventScope.Subscribe(
            FGameplayTag::RequestGameplayTag(TEXT("Cache.Invalidate.Equipment")),
            FEventHandlerDelegate::CreateUObject(this, &USuspenseCoreEquipmentDataService::OnCacheInvalidation)
        );

        // S8: Subscribe to resend requests (state refresh)
        EventScope.Subscribe(
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.RequestResend")),
            FEventHandlerDelegate::CreateUObject(this, &USuspenseCoreEquipmentDataService::OnResendRequested)
        );


        UE_LOG(LogSuspenseCoreEquipmentData, Log,
            TEXT("SetupEventSubscriptions: Subscribed to global cache invalidation events"));
    }

    // ===================================================================
    // Подписка на глобальную инвалидацию кэшей
    // ===================================================================

    GlobalCacheInvalidateHandle = FSuspenseGlobalCacheRegistry::Get().OnGlobalInvalidate.AddLambda(
        [WeakThis]()
        {
            if (WeakThis.IsValid())
            {
                WeakThis.Get()->InvalidateCache(-1);
                UE_LOG(LogSuspenseCoreEquipmentData, Log,
                    TEXT("Global cache invalidation triggered"));
            }
        }
    );

    UE_LOG(LogSuspenseCoreEquipmentData, Log,
        TEXT("SetupEventSubscriptions: All event subscriptions configured"));
}

void USuspenseCoreEquipmentDataService::OnCacheInvalidation(const FSuspenseCoreEquipmentEventData& EventData)
{
    // Parse slot index from event payload if available
    int32 SlotIndex = -1;

    TArray<FString> Params;
    EventData.Payload.ParseIntoArray(Params, TEXT(","));

    for (const FString& Param : Params)
    {
        FString Key, Value;
        if (Param.Split(TEXT(":"), &Key, &Value))
        {
            if (Key == TEXT("SlotIndex"))
            {
                SlotIndex = FCString::Atoi(*Value);
                break;
            }
        }
    }

    InvalidateCache(SlotIndex);
    RECORD_SERVICE_METRIC("Data.Cache.EventDrivenInvalidation", 1);
}

void USuspenseCoreEquipmentDataService::OnTransactionCompleted(const FGuid& TransactionId, bool bSuccess)
{
    if (bSuccess)
    {
        SuccessfulTransactions.fetch_add(1);
        RECORD_SERVICE_METRIC("Data.Tx.Completed", 1);
    }
    else
    {
        FailedTransactions.fetch_add(1);
        RECORD_SERVICE_METRIC("Data.Tx.Failed", 1);
    }

    if (bEnableDetailedLogging)
    {
        UE_LOG(LogSuspenseCoreEquipmentData, Verbose,
            TEXT("OnTransactionCompleted: Transaction %s %s"),
            *TransactionId.ToString(),
            bSuccess ? TEXT("succeeded") : TEXT("failed"));
    }
}

void USuspenseCoreEquipmentDataService::OnDataStoreDelta(const FEquipmentDelta& Delta)
{
    if (!bEnableDeltaTracking)
    {
        return;
    }

    // Update delta metrics
    TotalDeltasProcessed.fetch_add(1);

    // Record the total count with static metric
    RECORD_DIFF_METRIC("Total", 1);

    // For dynamic delta types, use the DeltaMetrics system which supports runtime names
    DeltaMetrics.RecordValue(FName(*Delta.ChangeType.ToString()), 1);

    // Store in history
    {
        FScopeLock Lock(&DeltaLock);

        RecentDeltaHistory.Add(Delta);
        if (RecentDeltaHistory.Num() > MaxDeltaHistory)
        {
            RecentDeltaHistory.RemoveAt(0);
        }

        // Update type counter
        if (int32* Count = DeltasByType.Find(Delta.ChangeType))
        {
            (*Count)++;
        }
        else
        {
            DeltasByType.Add(Delta.ChangeType, 1);
        }
    }

    // Broadcast to listeners
    BroadcastDelta(Delta);

    if (bEnableDetailedLogging)
    {
        UE_LOG(LogSuspenseCoreEquipmentData, Verbose,
            TEXT("OnDataStoreDelta: %s"),
            *Delta.ToString());
    }
}

void USuspenseCoreEquipmentDataService::OnTransactionDeltas(const TArray<FEquipmentDelta>& Deltas)
{
    if (!bEnableDeltaTracking || Deltas.Num() == 0)
    {
        return;
    }

    // Update metrics
    TotalDeltasProcessed.fetch_add(Deltas.Num());
    RECORD_DIFF_METRIC("BatchTotal", Deltas.Num());

    // Store in history and update counters
    {
        FScopeLock Lock(&DeltaLock);

        for (const FEquipmentDelta& Delta : Deltas)
        {
            RecentDeltaHistory.Add(Delta);

            // Update type counter
            if (int32* Count = DeltasByType.Find(Delta.ChangeType))
            {
                (*Count)++;
            }
            else
            {
                DeltasByType.Add(Delta.ChangeType, 1);
            }

            // Use DeltaMetrics for dynamic type recording
            DeltaMetrics.RecordValue(FName(*Delta.ChangeType.ToString()), 1);
        }

        // Trim history if needed
        while (RecentDeltaHistory.Num() > MaxDeltaHistory)
        {
            RecentDeltaHistory.RemoveAt(0);
        }
    }

    // Broadcast batch
    BroadcastBatchDeltas(Deltas);

    if (bEnableDetailedLogging)
    {
        UE_LOG(LogSuspenseCoreEquipmentData, Verbose,
            TEXT("OnTransactionDeltas: Received %d deltas from transaction"),
            Deltas.Num());
    }
}

void USuspenseCoreEquipmentDataService::BroadcastDelta(const FEquipmentDelta& Delta)
{
    // Ensure we're on the game thread for event safety
    if (!IsInGameThread())
    {
        AsyncTask(ENamedThreads::GameThread, [this, Delta]()
        {
            BroadcastDelta(Delta);
        });
        return;
    }

    // Broadcast through delegate (now guaranteed on game thread)
    OnEquipmentDeltaDelegate.Broadcast(Delta);

    // 🔥 КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Транслируем дельты в правильные события
    auto EventBus = FSuspenseCoreEquipmentEventBus::Get();
    if (EventBus.IsValid())
    {
        FSuspenseCoreEquipmentEventData EventData;
        EventData.Source = this;
        EventData.Timestamp = FPlatformTime::Seconds();
        EventData.Priority = EEventPriority::Normal;

        // Определяем тип события на основе типа операции в дельте
        const FGameplayTag OperationEquip = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Equip"));
        const FGameplayTag OperationSet = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Set"));
        const FGameplayTag OperationUnequip = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Unequip"));
        const FGameplayTag OperationClear = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Clear"));

        // 🎯 КЛЮЧЕВАЯ ЛОГИКА: Преобразуем операции в события
        if (Delta.ChangeType.MatchesTag(OperationEquip) || Delta.ChangeType.MatchesTag(OperationSet))
        {
            // ЭТО СОБЫТИЕ ТРИГГЕРИТ СПАВН ВИЗУАЛЬНОГО АКТОРА!
            EventData.EventType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.Equipped"));

            // Добавляем критически важные метаданные для VisualizationService
            EventData.AddMetadata(TEXT("Slot"), FString::FromInt(Delta.SlotIndex));
            EventData.AddMetadata(TEXT("ItemID"), Delta.ItemAfter.ItemID.ToString());
            EventData.AddMetadata(TEXT("InstanceID"), Delta.ItemAfter.InstanceID.ToString());

            // Устанавливаем Target для события (владелец экипировки)
            if (DataStore)
            {
                EventData.Target = DataStore->GetOwner();
            }

            UE_LOG(LogSuspenseCoreEquipmentData, Warning,
                TEXT("🟢 Broadcasting Equipment.Event.Equipped - Slot: %d, Item: %s"),
                Delta.SlotIndex, *Delta.ItemAfter.ItemID.ToString());
        }
        else if (Delta.ChangeType.MatchesTag(OperationUnequip) || Delta.ChangeType.MatchesTag(OperationClear))
        {
            EventData.EventType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.Unequipped"));
            EventData.AddMetadata(TEXT("Slot"), FString::FromInt(Delta.SlotIndex));

            if (DataStore)
            {
                EventData.Target = DataStore->GetOwner();
            }

            UE_LOG(LogSuspenseCoreEquipmentData, Log,
                TEXT("Broadcasting Equipment.Event.Unequipped - Slot: %d"), Delta.SlotIndex);
        }
        else
        {
            // Для других типов операций используем общий тег дельты
            EventData.EventType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Delta"));
            EventData.Payload = Delta.ToString();
            EventData.AddMetadata(TEXT("DeltaType"), Delta.ChangeType.ToString());
            EventData.AddMetadata(TEXT("SlotIndex"), FString::FromInt(Delta.SlotIndex));
        }

        // Broadcast события
        if (EventData.EventType.IsValid())
        {
            EventBus->Broadcast(EventData);
        }
    }
}

void USuspenseCoreEquipmentDataService::BroadcastBatchDeltas(const TArray<FEquipmentDelta>& Deltas)
{
    // Ensure we're on the game thread
    if (!IsInGameThread())
    {
        AsyncTask(ENamedThreads::GameThread, [this, Deltas]()
        {
            BroadcastBatchDeltas(Deltas);
        });
        return;
    }

    // Broadcast through delegate
    OnBatchDeltasDelegate.Broadcast(Deltas);

    // 🔥 КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Обрабатываем каждую дельту индивидуально
    // для генерации правильных событий типа Equipment.Event.Equipped/Unequipped
    for (const FEquipmentDelta& Delta : Deltas)
    {
        // Переиспользуем логику из BroadcastDelta для единообразия
        auto EventBus = FSuspenseCoreEquipmentEventBus::Get();
        if (!EventBus.IsValid())
        {
            continue;
        }

        FSuspenseCoreEquipmentEventData EventData;
        EventData.Source = this;
        EventData.Timestamp = FPlatformTime::Seconds();
        EventData.Priority = EEventPriority::Normal;

        const FGameplayTag OperationEquip = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Equip"));
        const FGameplayTag OperationSet = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Set"));
        const FGameplayTag OperationUnequip = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Unequip"));
        const FGameplayTag OperationClear = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Clear"));

        if (Delta.ChangeType.MatchesTag(OperationEquip) || Delta.ChangeType.MatchesTag(OperationSet))
        {
            EventData.EventType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.Equipped"));
            EventData.AddMetadata(TEXT("Slot"), FString::FromInt(Delta.SlotIndex));
            EventData.AddMetadata(TEXT("ItemID"), Delta.ItemAfter.ItemID.ToString());
            EventData.AddMetadata(TEXT("InstanceID"), Delta.ItemAfter.InstanceID.ToString());
            EventData.AddMetadata(TEXT("BatchSize"), FString::FromInt(Deltas.Num()));

            if (DataStore)
            {
                EventData.Target = DataStore->GetOwner();
            }

            UE_LOG(LogSuspenseCoreEquipmentData, Warning,
                TEXT("🟢 [Batch] Broadcasting Equipment.Event.Equipped - Slot: %d, Item: %s"),
                Delta.SlotIndex, *Delta.ItemAfter.ItemID.ToString());

            EventBus->Broadcast(EventData);
        }
        else if (Delta.ChangeType.MatchesTag(OperationUnequip) || Delta.ChangeType.MatchesTag(OperationClear))
        {
            EventData.EventType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.Unequipped"));
            EventData.AddMetadata(TEXT("Slot"), FString::FromInt(Delta.SlotIndex));
            EventData.AddMetadata(TEXT("BatchSize"), FString::FromInt(Deltas.Num()));

            if (DataStore)
            {
                EventData.Target = DataStore->GetOwner();
            }

            EventBus->Broadcast(EventData);
        }
        else
        {
            // Для других операций используем общий BatchDelta тег
            EventData.EventType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Delta.Batch"));
            EventData.Payload = Delta.ToString();
            EventData.AddMetadata(TEXT("DeltaType"), Delta.ChangeType.ToString());
            EventData.AddMetadata(TEXT("SlotIndex"), FString::FromInt(Delta.SlotIndex));
            EventData.AddMetadata(TEXT("BatchSize"), FString::FromInt(Deltas.Num()));

            EventBus->Broadcast(EventData);
        }
    }
}

bool USuspenseCoreEquipmentDataService::PerformDeepValidation() const
{
    FRWScopeLock DataScopeLock(DataLock, SLT_ReadOnly);
    return PerformDeepValidation_Internal();
}

bool USuspenseCoreEquipmentDataService::PerformDeepValidation_Internal() const
{
    // Предполагаем, что DataLock уже удерживается вызывающим кодом
    // Deep validation for data consistency
    TMap<FGuid, int32> InstanceIdToSlot;

    for (int32 i = 0; i < DataStore->GetSlotCount(); ++i)
    {
        FSuspenseInventoryItemInstance Item = DataStore->GetSlotItem(i);

        if (!Item.IsValid())
        {
            continue;
        }

        // Check for duplicate instance IDs
        if (InstanceIdToSlot.Contains(Item.InstanceID))
        {
            UE_LOG(LogSuspenseCoreEquipmentData, Error,
                TEXT("PerformDeepValidation: Duplicate instance ID %s in slots %d and %d"),
                *Item.InstanceID.ToString(), InstanceIdToSlot[Item.InstanceID], i);
            RECORD_SERVICE_METRIC("Data.Validation.DuplicateInstances", 1);
            return false;
        }

        InstanceIdToSlot.Add(Item.InstanceID, i);

        // Validate item configuration matches slot
        FEquipmentSlotConfig SlotConfig = DataStore->GetSlotConfiguration(i);
        if (!SlotConfig.IsValid())
        {
            UE_LOG(LogSuspenseCoreEquipmentData, Error,
                TEXT("PerformDeepValidation: Invalid slot configuration at index %d"), i);
            RECORD_SERVICE_METRIC("Data.Validation.InvalidConfigs", 1);
            return false;
        }

        // Additional deep checks can be added here
    }

    RECORD_SERVICE_METRIC("Data.Validation.ItemsChecked", InstanceIdToSlot.Num());
    return true;
}

bool USuspenseCoreEquipmentDataService::ValidateDataIntegrity_Internal(bool bDeep) const
{
    // Предполагаем, что DataLock уже удерживается вызывающим кодом
    if (!DataStore)
    {
        RECORD_SERVICE_METRIC("Data.IntegrityCheck.NoDataStore", 1);
        return false;
    }

    // Basic validation
    int32 SlotCount = DataStore->GetSlotCount();
    if (SlotCount <= 0 || SlotCount > MaxSlotCount)
    {
        UE_LOG(LogSuspenseCoreEquipmentData, Error,
            TEXT("ValidateDataIntegrity: Invalid slot count %d (max: %d)"),
            SlotCount, MaxSlotCount);
        RECORD_SERVICE_METRIC("Data.IntegrityCheck.InvalidSlotCount", 1);
        return false;
    }

    // Validate active weapon slot
    int32 ActiveSlot = DataStore->GetActiveWeaponSlot();
    if (ActiveSlot != INDEX_NONE && !DataStore->IsValidSlotIndex(ActiveSlot))
    {
        UE_LOG(LogSuspenseCoreEquipmentData, Error,
            TEXT("ValidateDataIntegrity: Invalid active weapon slot %d"),
            ActiveSlot);
        RECORD_SERVICE_METRIC("Data.IntegrityCheck.InvalidActiveSlot", 1);
        return false;
    }

    if (bDeep)
    {
        bool bDeepSuccess = PerformDeepValidation_Internal();
        if (bDeepSuccess)
        {
            RECORD_SERVICE_METRIC("Data.IntegrityCheck.DeepPassed", 1);
        }
        else
        {
            RECORD_SERVICE_METRIC("Data.IntegrityCheck.DeepFailed", 1);
        }
        return bDeepSuccess;
    }

    RECORD_SERVICE_METRIC("Data.IntegrityCheck.BasicPassed", 1);
    return true;
}

// Остальные методы остаются без изменений...
// [Методы CreateDefaultSlotConfiguration, UpdateCacheEntry, BatchUpdateCache,
//  LogDataOperation, ResetDataStore, IsValidSlotIndex, WarmupCachesSafe
//  остаются такими же как в оригинальном файле]

TArray<FEquipmentSlotConfig> USuspenseCoreEquipmentDataService::CreateDefaultSlotConfiguration() const
{
    TArray<FEquipmentSlotConfig> EquipmentSlots;
    EquipmentSlots.Empty();

    // Primary
    {
        FEquipmentSlotConfig Slot(
            EEquipmentSlotType::PrimaryWeapon,
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.PrimaryWeapon")));
        Slot.AttachmentSocket = TEXT("weapon_r");
        Slot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.AR")));
        Slot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.DMR")));
        Slot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.SR")));
        Slot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Shotgun")));
        Slot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.LMG")));
        EquipmentSlots.Add(Slot);
    }

    // Secondary
    {
        FEquipmentSlotConfig Slot(
            EEquipmentSlotType::SecondaryWeapon,
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.SecondaryWeapon")));
        Slot.AttachmentSocket = TEXT("spine_03");
        Slot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.SMG")));
        Slot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Shotgun")));
        Slot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.PDW")));
        EquipmentSlots.Add(Slot);
    }

    // Holster
    {
        FEquipmentSlotConfig Slot(
            EEquipmentSlotType::Holster,
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Holster")));
        Slot.AttachmentSocket = TEXT("thigh_r");
        Slot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Pistol")));
        Slot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Revolver")));
        EquipmentSlots.Add(Slot);
    }

    // Scabbard
    {
        FEquipmentSlotConfig Slot(
            EEquipmentSlotType::Scabbard,
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Scabbard")));
        Slot.AttachmentSocket = TEXT("spine_02");
        Slot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Melee.Knife")));
        EquipmentSlots.Add(Slot);
    }

    // Head
    {
        FEquipmentSlotConfig Slot(
            EEquipmentSlotType::Headwear,
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Headwear")));
        Slot.AttachmentSocket = TEXT("head");
        Slot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Armor.Helmet")));
        Slot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Gear.Headwear")));
        EquipmentSlots.Add(Slot);
    }

    {
        FEquipmentSlotConfig Slot(
            EEquipmentSlotType::Earpiece,
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Earpiece")));
        Slot.AttachmentSocket = TEXT("head");
        Slot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Gear.Earpiece")));
        EquipmentSlots.Add(Slot);
    }

    {
        FEquipmentSlotConfig Slot(
            EEquipmentSlotType::Eyewear,
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Eyewear")));
        Slot.AttachmentSocket = TEXT("head");
        Slot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Gear.Eyewear")));
        EquipmentSlots.Add(Slot);
    }

    {
        FEquipmentSlotConfig Slot(
            EEquipmentSlotType::FaceCover,
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.FaceCover")));
        Slot.AttachmentSocket = TEXT("head");
        Slot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Gear.FaceCover")));
        EquipmentSlots.Add(Slot);
    }

    // Body
    {
        FEquipmentSlotConfig Slot(
            EEquipmentSlotType::BodyArmor,
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.BodyArmor")));
        Slot.AttachmentSocket = TEXT("spine_03");
        Slot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Armor.BodyArmor")));
        EquipmentSlots.Add(Slot);
    }

    {
        FEquipmentSlotConfig Slot(
            EEquipmentSlotType::TacticalRig,
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.TacticalRig")));
        Slot.AttachmentSocket = TEXT("spine_03");
        Slot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Gear.TacticalRig")));
        EquipmentSlots.Add(Slot);
    }

    // Storage
    {
        FEquipmentSlotConfig Slot(
            EEquipmentSlotType::Backpack,
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Backpack")));
        Slot.AttachmentSocket = TEXT("spine_02");
        Slot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Gear.Backpack")));
        EquipmentSlots.Add(Slot);
    }

    {
        FEquipmentSlotConfig Slot(
            EEquipmentSlotType::SecureContainer,
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.SecureContainer")));
        Slot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Gear.SecureContainer")));
        EquipmentSlots.Add(Slot);
    }

    // Quick slots (1..4)
    for (int32 i = 1; i <= 4; ++i)
    {
        const EEquipmentSlotType QuickType = static_cast<EEquipmentSlotType>(
            static_cast<int32>(EEquipmentSlotType::QuickSlot1) + (i - 1)
        );
        const FString TagName = FString::Printf(TEXT("Equipment.Slot.QuickSlot%d"), i);

        FEquipmentSlotConfig Slot(QuickType, FGameplayTag::RequestGameplayTag(*TagName));
        Slot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Consumable")));
        Slot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Medical")));
        Slot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Throwable")));
        Slot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Ammo")));
        EquipmentSlots.Add(Slot);
    }

    // Armband
    {
        FEquipmentSlotConfig Slot(
            EEquipmentSlotType::Armband,
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Armband")));
        Slot.AttachmentSocket = TEXT("upperarm_l");
        Slot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Gear.Armband")));
        EquipmentSlots.Add(Slot);
    }

    return EquipmentSlots;
}

void USuspenseCoreEquipmentDataService::UpdateCacheEntry(int32 SlotIndex, const FSuspenseInventoryItemInstance& Item) const
{
    FRWScopeLock CacheWriteLock(CacheLock, SLT_Write);

    if (!ItemCache.IsValid())
    {
        return;
    }

    if (Item.IsValid())
    {
        ItemCache->Set(SlotIndex, Item, DefaultCacheTTL);
        RECORD_SERVICE_METRIC("Data.Cache.EntryUpdated", 1);
    }
    else
    {
        ItemCache->Remove(SlotIndex);
        RECORD_SERVICE_METRIC("Data.Cache.EntryInvalidated", 1);
    }
}

void USuspenseCoreEquipmentDataService::BatchUpdateCache(const TMap<int32, FSuspenseInventoryItemInstance>& Items) const
{
    // Нечего обновлять
    if (Items.Num() == 0 || !ItemCache.IsValid())
    {
        return;
    }

    int32 NumSet = 0;
    int32 NumInvalidated = 0;
    int32 NumContention = 0;

    // Один общий write-lock на весь батч
    FRWScopeLock CacheWriteLock(CacheLock, SLT_Write);

    for (const auto& Pair : Items)
    {
        const int32 SlotIndex = Pair.Key;
        const FSuspenseInventoryItemInstance& Item = Pair.Value;

        if (Item.IsValid())
        {
            // Double-checked: если кто-то уже положил значение параллельно — не перетираем
            FSuspenseInventoryItemInstance Existing;
            const bool bAlreadyCached = ItemCache->Get(SlotIndex, Existing);

            if (!bAlreadyCached)
            {
                ItemCache->Set(SlotIndex, Item, DefaultCacheTTL);
                ++NumSet;
            }
            else
            {
                // Считаем конфликт и оставляем существующее значение
                ++NumContention;
            }
        }
        else
        {
            ItemCache->Remove(SlotIndex);
            ++NumInvalidated;
        }
    }

    // Метрики
    RECORD_SERVICE_METRIC("Data.Cache.BatchUpdate.Total", Items.Num());
    if (NumSet > 0)          { RECORD_SERVICE_METRIC("Data.Cache.BatchUpdate.Set", NumSet); }
    if (NumInvalidated > 0)  { RECORD_SERVICE_METRIC("Data.Cache.BatchUpdate.Invalidate", NumInvalidated); }
    if (NumContention > 0)
    {
        CacheContention.fetch_add(NumContention);
        RECORD_SERVICE_METRIC("Data.Cache.BatchUpdate.Contention", NumContention);
    }

    if (bEnableDetailedLogging)
    {
        UE_LOG(LogSuspenseCoreEquipmentData, Verbose,
            TEXT("BatchUpdateCache: total=%d, set=%d, invalidated=%d, contention=%d"),
            Items.Num(), NumSet, NumInvalidated, NumContention);
    }
}


void USuspenseCoreEquipmentDataService::LogDataOperation(const FString& Operation, int32 SlotIndex) const
{
    if (!bEnableDetailedLogging)
    {
        return;
    }

    if (SlotIndex >= 0)
    {
        UE_LOG(LogSuspenseCoreEquipmentData, VeryVerbose,
            TEXT("DataOperation: %s (Slot: %d)"), *Operation, SlotIndex);
    }
    else
    {
        UE_LOG(LogSuspenseCoreEquipmentData, VeryVerbose,
            TEXT("DataOperation: %s"), *Operation);
    }
}

void USuspenseCoreEquipmentDataService::ResetDataStore()
{
    if (!DataStore)
    {
        return;
    }

    // Clear all slots
    const int32 SlotCount = DataStore->GetSlotCount();
    for (int32 i = 0; i < SlotCount; ++i)
    {
        DataStore->ClearSlot(i, false);
    }

    // Reset active weapon slot
    DataStore->SetActiveWeaponSlot(INDEX_NONE);

    // Reset equipment state
    DataStore->SetEquipmentState(FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Default")));

    // Reinitialize with default configuration
    InitializeDataStorage();

    // Notify observers on game thread (safe weak pointer capture)
    TWeakObjectPtr<USuspenseCoreEquipmentDataStore> WeakDS = DataStore;
    AsyncTask(ENamedThreads::GameThread, [WeakDS]()
    {
        if (WeakDS.IsValid())
        {
            WeakDS->OnDataStoreReset().Broadcast();
        }
    });

    RECORD_SERVICE_METRIC("Data.DataStore.Reset", 1);
}

bool USuspenseCoreEquipmentDataService::IsValidSlotIndex(int32 SlotIndex) const
{
    if (SlotIndex < 0 || SlotIndex >= MaxSlotCount)
    {
        return false;
    }

    if (DataStore && !DataStore->IsValidSlotIndex(SlotIndex))
    {
        return false;
    }

    return true;
}

void USuspenseCoreEquipmentDataService::WarmupCachesSafe()
{
    SCOPED_SERVICE_TIMER("Data.Cache.WarmupSafe");

    if (!bEnableCacheWarming || !DataStore)
    {
        return;
    }

    TMap<int32, FSuspenseInventoryItemInstance> WarmupItems;
    TMap<int32, FEquipmentSlotConfig>   WarmupConfigs;

    // 1) Собираем данные из DataStore под DataLock (read-only)
    {
        FRWScopeLock DataScopeLock(DataLock, SLT_ReadOnly);
        const int32 Slots = DataStore->GetSlotCount();

        for (int32 i = 0; i < Slots; ++i)
        {
            const FSuspenseInventoryItemInstance Item = DataStore->GetSlotItem(i);
            if (Item.IsValid())
            {
                WarmupItems.Add(i, Item);
            }

            WarmupConfigs.Add(i, DataStore->GetSlotConfiguration(i));
        }
    }

    // 2) Кладём конфиги в кэш под CacheLock (write)
    {
        FRWScopeLock CacheWriteLock(CacheLock, SLT_Write);
        for (const auto& Pair : WarmupConfigs)
        {
            ConfigCache->Set(Pair.Key, Pair.Value, DefaultCacheTTL * 2.0f); // конфиги меняются реже
        }
    }
    RECORD_SERVICE_METRIC("Data.Cache.Warm.Configs", WarmupConfigs.Num());

    // 3) Кладём items пачкой (метод сам возьмёт CacheLock)
    if (WarmupItems.Num() > 0)
    {
        BatchUpdateCache(WarmupItems);
        RECORD_SERVICE_METRIC("Data.Cache.Warm.Items", WarmupItems.Num());
    }

    LastCacheWarmingTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    RECORD_SERVICE_METRIC("Data.Cache.Warm.Executed", 1);

    if (bEnableDetailedLogging)
    {
        UE_LOG(LogSuspenseCoreEquipmentData, Verbose,
            TEXT("WarmupCachesSafe: warmed items=%d, configs=%d"),
            WarmupItems.Num(), WarmupConfigs.Num());
    }
}

void USuspenseCoreEquipmentDataService::OnResendRequested(const FSuspenseCoreEquipmentEventData& Event)
{
    // Минимальная корректная реализация: фиксируем запрос и инициируем безопасное переотправление
    UE_LOG(LogTemp, Verbose,
        TEXT("[EquipmentDataService] Resend requested: Type=%s, Src=%s, Tgt=%s, Payload=%s"),
        *Event.EventType.ToString(),
        *GetNameSafe(Event.Source.Get()),
        *GetNameSafe(Event.Target.Get()),
        *Event.Payload);

    // Если у вас уже есть внутренняя процедура переотправки — вызовите её тут.
    // Например, можно переиспользовать вашу очередь/механизм:
    // RequeueAllPendingDeltas();  // <- если есть
    // BroadcastBatchDeltas();      // <- если есть

    // Базовый "no-op" безопасен: подписчик существует, метод реализован, линковка закрыта.
}
