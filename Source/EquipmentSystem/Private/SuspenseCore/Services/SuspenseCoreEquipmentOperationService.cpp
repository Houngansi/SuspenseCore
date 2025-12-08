// EquipmentOperationServiceImpl.cpp
// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Services/SuspenseCoreEquipmentOperationService.h"
#include "Core/Services/SuspenseEquipmentServiceLocator.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "SuspenseCore/Components/Transaction/SuspenseCoreEquipmentTransactionProcessor.h"
#include "SuspenseCore/Components/Core/SuspenseCoreEquipmentDataStore.h"
#include "Interfaces/Equipment/ISuspenseEquipmentService.h"
#include "Interfaces/Equipment/ISuspenseNetworkDispatcher.h"
#include "SuspenseCore/Components/Core/SuspenseCoreEquipmentOperationExecutor.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentValidationService.h"
#include "Types/Network/SuspenseNetworkTypes.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "Async/Async.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentDataService.h"

DEFINE_LOG_CATEGORY(LogSuspenseCoreEquipmentOperations);

// Service identification tags - using native compile-time tags
namespace ServiceTags
{
    using namespace SuspenseCoreEquipmentTags::Service;

    const FGameplayTag& Operations = TAG_Service_Equipment_Operations;
    const FGameplayTag& Data = TAG_Service_Equipment_Data;
    const FGameplayTag& Validation = TAG_Service_Equipment_Validation;
    const FGameplayTag& Network = TAG_Service_Equipment_Network;
    const FGameplayTag& Prediction = TAG_Service_Equipment_Prediction;
}

// Event tags - using native compile-time tags
namespace EventTags
{
    using namespace SuspenseCoreEquipmentTags::Event;

    const FGameplayTag& OperationQueued = TAG_Equipment_Event_Operation_Queued;
    const FGameplayTag& OperationStarted = TAG_Equipment_Event_Operation_Started;
    const FGameplayTag& OperationCompleted = TAG_Equipment_Event_Operation_Completed;
    const FGameplayTag& ValidationChanged = TAG_Equipment_Event_Validation_Changed;
    const FGameplayTag& DataChanged = TAG_Equipment_Event_Data_Changed;
    const FGameplayTag& NetworkResult = TAG_Equipment_Event_Network_Result;
}

USuspenseCoreEquipmentOperationService::USuspenseCoreEquipmentOperationService()
{
    ValidationCache = MakeShareable(new FSuspenseCoreEquipmentCacheManager<uint32, FSlotValidationResult>(500));
    ResultCache = MakeShareable(new FSuspenseCoreEquipmentCacheManager<FGuid, FEquipmentOperationResult>(100));
    InitializationTime = FDateTime::Now();
}

USuspenseCoreEquipmentOperationService::~USuspenseCoreEquipmentOperationService()
{
    if (ServiceState == ESuspenseCoreServiceLifecycleState::Ready)
    {
        ShutdownService(true);
    }
    CleanupObjectPools();
}

//========================================
// IEquipmentService Implementation
//========================================

bool USuspenseCoreEquipmentOperationService::InitializeService(const FSuspenseCoreServiceInitParams& Params)
{
    if (ServiceState != ESuspenseCoreServiceLifecycleState::Uninitialized)
    {
        UE_LOG(LogSuspenseCoreEquipmentOperations, Warning,
            TEXT("InitializeService: already initialized (state=%s)"),
            *UEnum::GetValueAsString(ServiceState));
        return ServiceState == ESuspenseCoreServiceLifecycleState::Ready;
    }

    ServiceState = ESuspenseCoreServiceLifecycleState::Initializing;
    InitializationTime = FDateTime::Now();

    // CRITICAL FIX: Store ServiceLocator reference from params
    CachedServiceLocator = Cast<USuspenseCoreEquipmentServiceLocator>(Params.ServiceLocator);

    if (!CachedServiceLocator.IsValid())
    {
        UE_LOG(LogSuspenseCoreEquipmentOperations, Error,
            TEXT("InitializeService: ServiceLocator not provided in init params"));
        ServiceState = ESuspenseCoreServiceLifecycleState::Failed;
        return false;
    }

    UE_LOG(LogSuspenseCoreEquipmentOperations, Log,
        TEXT("InitializeService: ServiceLocator cached successfully"));

    // Validate and sanitize configuration
    EnsureValidConfig();

    // Initialize object pools for memory optimization
    if (bEnableObjectPooling)
    {
        InitializeObjectPools();
        UE_LOG(LogSuspenseCoreEquipmentOperations, Log,
            TEXT("Initialized object pools: %d operations, %d results"),
            InitialPoolSize, InitialPoolSize);
    }

    // Initialize dependency graph
    if (!InitializeDependencies())
    {
        UE_LOG(LogSuspenseCoreEquipmentOperations, Error,
            TEXT("Failed to initialize dependencies"));
        ServiceState = ESuspenseCoreServiceLifecycleState::Failed;
        return false;
    }

    // Initialize caching systems with proper constructor signature
    // Format: FEquipmentCacheManager(float DefaultTTL, int32 MaxEntries)
    ValidationCache = MakeShared<FSuspenseCoreEquipmentCacheManager<uint32, FSlotValidationResult>>(
        ValidationCacheTTL,  // Default TTL: 5.0s
        1000);               // Max entries: 1000

    ResultCache = MakeShared<FSuspenseCoreEquipmentCacheManager<FGuid, FEquipmentOperationResult>>(
        ResultCacheTTL,      // Default TTL: 2.0s
        500);                // Max entries: 500

    UE_LOG(LogSuspenseCoreEquipmentOperations, Log,
        TEXT("Initialized caches: Validation(TTL=%.1fs, Cap=%d), Result(TTL=%.1fs, Cap=%d)"),
        ValidationCacheTTL, 1000, ResultCacheTTL, 500);

    // Setup event subscriptions
    SetupEventSubscriptions();

    // Start async queue processing
    if (bQueueProcessingEnabled)
    {
        StartQueueProcessing();
    }

    ServiceState = ESuspenseCoreServiceLifecycleState::Ready;

    UE_LOG(LogSuspenseCoreEquipmentOperations, Log,
        TEXT("EquipmentOperationService initialized successfully"));
    UE_LOG(LogSuspenseCoreEquipmentOperations, Log,
        TEXT("  - Mode: %s"),
        bServerAuthority ? TEXT("Server Authority") : TEXT("Client Predicted"));
    UE_LOG(LogSuspenseCoreEquipmentOperations, Log,
        TEXT("  - Queue Processing: %s (interval=%.3fs)"),
        bQueueProcessingEnabled ? TEXT("Enabled") : TEXT("Disabled"),
        QueueProcessInterval);
    UE_LOG(LogSuspenseCoreEquipmentOperations, Log,
        TEXT("  - Object Pooling: %s"),
        bEnableObjectPooling ? TEXT("Enabled") : TEXT("Disabled"));
    UE_LOG(LogSuspenseCoreEquipmentOperations, Log,
        TEXT("  - Transaction Plans: %s"),
        bUseTransactionPlans ? TEXT("Enabled") : TEXT("Disabled"));

    return true;
}

USuspenseCoreEquipmentServiceLocator* USuspenseCoreEquipmentOperationService::GetServiceLocator() const
{
    if (!CachedServiceLocator.IsValid())
    {
        UE_LOG(LogSuspenseCoreEquipmentOperations, Error,
            TEXT("GetServiceLocator: cached locator is invalid"));
        return nullptr;
    }
    return CachedServiceLocator.Get();
}
bool USuspenseCoreEquipmentOperationService::ShutdownService(bool bForce)
{
    SCOPED_SERVICE_TIMER("ShutdownService");

    if (ServiceState == ESuspenseCoreServiceLifecycleState::Shutdown)
    {
        return true;
    }

    ServiceState = ESuspenseCoreServiceLifecycleState::Shutting;
    StopQueueProcessing();

    if (!bForce && OperationQueue.Num() > 0)
    {
        UE_LOG(LogSuspenseCoreEquipmentOperations, Warning,
            TEXT("Processing %d remaining operations before shutdown"),
            OperationQueue.Num());

        while (OperationQueue.Num() > 0 && !bForce)
        {
            ProcessOperationQueue();
        }
    }

    {
        FRWScopeLock Lock(QueueLock, SLT_Write);

        for (FSuspenseCoreQueuedOperation* Op : OperationQueue)
        {
            ReleaseOperation(Op);
        }
        OperationQueue.Empty();

        for (auto& BatchPair : ActiveBatches)
        {
            for (FSuspenseCoreQueuedOperation* Op : BatchPair.Value)
            {
                ReleaseOperation(Op);
            }
        }
        ActiveBatches.Empty();
    }

    {
        FRWScopeLock Lock(HistoryLock, SLT_Write);
        OperationHistory.Empty();
        RedoStack.Empty();
    }

    ValidationCache->Clear();
    ResultCache->Clear();

    FSuspenseGlobalCacheRegistry::Get().UnregisterCache(TEXT("Operations.ValidationCache"));
    FSuspenseGlobalCacheRegistry::Get().UnregisterCache(TEXT("Operations.ResultCache"));

    // MEMORY LEAK FIX: Properly unsubscribe all EventHandles before clearing
    // EventScope.UnsubscribeAll() only cleans EventScope subscriptions (which is empty),
    // but actual subscriptions are stored in EventHandles array and must be unsubscribed explicitly
    if (auto Bus = EventBus.Pin())
    {
        for (const FEventSubscriptionHandle& Handle : EventHandles)
        {
            Bus->Unsubscribe(Handle);
        }
    }
    EventHandles.Empty();
    EventScope.UnsubscribeAll(); // Keep for safety if EventScope used elsewhere

    {
        FRWScopeLock Lock(ExecutorLock, SLT_Write);
        OperationsExecutor = nullptr;
        DataProvider = nullptr;
        TransactionManager = nullptr;
        RulesEngine = nullptr;
        NetworkServiceObject = nullptr;
        PredictionManager = nullptr;
    }

    CleanupObjectPools();
    ServiceState = ESuspenseCoreServiceLifecycleState::Shutdown;
    ServiceMetrics.RecordSuccess();

    UE_LOG(LogSuspenseCoreEquipmentOperations, Log,
        TEXT("Service shutdown - Total executed: %d, Success rate: %.1f%%, Pool efficiency: %.1f%%"),
        TotalOperationsExecuted.load(),
        TotalOperationsExecuted.load() > 0 ?
            (float)SuccessfulOperations.load() / TotalOperationsExecuted.load() * 100.0f : 0.0f,
        GetPoolEfficiency());

    return true;
}

ESuspenseCoreServiceLifecycleState USuspenseCoreEquipmentOperationService::GetServiceState() const
{
    SCOPED_SERVICE_TIMER("GetServiceState");
    return ServiceState;
}

bool USuspenseCoreEquipmentOperationService::IsServiceReady() const
{
    SCOPED_SERVICE_TIMER("IsServiceReady");
    return ServiceState == ESuspenseCoreServiceLifecycleState::Ready;
}

FGameplayTag USuspenseCoreEquipmentOperationService::GetServiceTag() const
{
    SCOPED_SERVICE_TIMER("GetServiceTag");
    return ServiceTags::Operations;
}

FGameplayTagContainer USuspenseCoreEquipmentOperationService::GetRequiredDependencies() const
{
    SCOPED_SERVICE_TIMER("GetRequiredDependencies");
    FGameplayTagContainer Dependencies;
    Dependencies.AddTag(ServiceTags::Data);
    Dependencies.AddTag(ServiceTags::Validation);
    return Dependencies;
}

bool USuspenseCoreEquipmentOperationService::ValidateService(TArray<FText>& OutErrors) const
{
    SCOPED_SERVICE_TIMER("ValidateService");

    OutErrors.Empty();
    bool bIsValid = true;

    if (ServiceState != ESuspenseCoreServiceLifecycleState::Ready)
    {
        OutErrors.Add(FText::Format(
            NSLOCTEXT("Equipment", "ServiceNotReady", "Service not ready: {0}"),
            FText::FromString(UEnum::GetValueAsString(ServiceState))
        ));
        bIsValid = false;
    }

    {
        FRWScopeLock Lock(ExecutorLock, SLT_ReadOnly);

        if (!DataProvider.GetInterface())
        {
            OutErrors.Add(NSLOCTEXT("Equipment", "NoDataProvider", "Data provider not available"));
            bIsValid = false;
        }

        if (!TransactionManager.GetInterface())
        {
            OutErrors.Add(NSLOCTEXT("Equipment", "NoTransactionManager", "Transaction manager not available"));
            bIsValid = false;
        }

        if (!RulesEngine.GetInterface())
        {
            OutErrors.Add(NSLOCTEXT("Equipment", "NoRulesEngine", "Rules engine not available"));
            bIsValid = false;
        }
    }

    {
        FRWScopeLock Lock(QueueLock, SLT_ReadOnly);

        if (OperationQueue.Num() > MaxQueueSize * 0.9f)
        {
            OutErrors.Add(FText::Format(
                NSLOCTEXT("Equipment", "QueueNearFull", "Queue near capacity: {0}/{1}"),
                OperationQueue.Num(),
                MaxQueueSize
            ));
        }
    }

    if (bEnableObjectPooling)
    {
        float PoolEfficiency = GetPoolEfficiency();

        if (PoolEfficiency < 0.5f && TotalOperationsExecuted.load() > 100)
        {
            OutErrors.Add(FText::Format(
                NSLOCTEXT("Equipment", "LowPoolEfficiency", "Low pool efficiency: {0}%"),
                FText::AsNumber(FMath::RoundToInt(PoolEfficiency * 100.0f))
            ));
        }
    }

    ServiceMetrics.Inc(TEXT("ValidateServiceCalls"));
    return bIsValid;
}

void USuspenseCoreEquipmentOperationService::ResetService()
{
    SCOPED_SERVICE_TIMER("ResetService");

    // Clear operation queue and active batches
    {
        FRWScopeLock Lock(QueueLock, SLT_Write);

        // Release all queued operations back to pool
        for (FSuspenseCoreQueuedOperation* Op : OperationQueue)
        {
            ReleaseOperation(Op);
        }
        OperationQueue.Empty();

        // Release all active batch operations
        for (auto& BatchPair : ActiveBatches)
        {
            for (FSuspenseCoreQueuedOperation* Op : BatchPair.Value)
            {
                ReleaseOperation(Op);
            }
        }
        ActiveBatches.Empty();

        // Reset processing flags
        bIsProcessingQueue = false;
        bClearQueueAfterProcessing = false;
    }

    // Clear operation history and redo stack
    {
        FRWScopeLock Lock(HistoryLock, SLT_Write);
        OperationHistory.Empty();
        RedoStack.Empty();
    }

    // Clear validation and result caches
    if (ValidationCache.IsValid())
    {
        ValidationCache->Clear();
    }
    if (ResultCache.IsValid())
    {
        ResultCache->Clear();
    }

    // Reset operation statistics
    {
        FRWScopeLock Lock(StatsLock, SLT_Write);

        // Reset atomic counters
        TotalOperationsQueued.store(0);
        TotalOperationsExecuted.store(0);
        SuccessfulOperations.store(0);
        FailedOperations.store(0);
        CancelledOperations.store(0);
        TotalBatchesProcessed.store(0);

        // Reset calculated metrics
        CacheHitRate = 0.0f;
        AverageQueueTime = 0.0f;
        AverageExecutionTime = 0.0f;
        PeakQueueSize = 0;
    }

    // Reset pool statistics
    OperationPoolHits.store(0);
    OperationPoolMisses.store(0);
    ResultPoolHits.store(0);
    ResultPoolMisses.store(0);
    PoolOverflows.store(0);

    // Reset service metrics to clean state
    ServiceMetrics.Reset();

    // Now record the reset event itself as the first new metric
    ServiceMetrics.RecordSuccess();
    RECORD_SERVICE_METRIC("Operations.Service.Reset", 1);

    UE_LOG(LogSuspenseCoreEquipmentOperations, Log, TEXT("EquipmentOperationService reset complete"));
}

FString USuspenseCoreEquipmentOperationService::GetServiceStats() const
{
    SCOPED_SERVICE_TIMER("GetServiceStats");

    FRWScopeLock StatsGuard(StatsLock, SLT_ReadOnly);

    FString Stats = TEXT("=== Equipment Operation Service Statistics ===\n");

    Stats += FString::Printf(TEXT("State: %s\n"), *UEnum::GetValueAsString(ServiceState));
    Stats += FString::Printf(TEXT("Transaction Plans: %s\n"), bUseTransactionPlans ? TEXT("Enabled") : TEXT("Disabled"));
    FTimespan Uptime = FDateTime::Now() - InitializationTime;
    Stats += FString::Printf(TEXT("Uptime: %.1f hours\n"), Uptime.GetTotalHours());

    Stats += TEXT("\n--- Queue ---\n");
    Stats += FString::Printf(TEXT("Current: %d/%d\n"), GetQueueSize(), MaxQueueSize);
    Stats += FString::Printf(TEXT("Peak: %d\n"), PeakQueueSize);
    Stats += FString::Printf(TEXT("Total Queued: %d\n"), TotalOperationsQueued.load());
    Stats += FString::Printf(TEXT("Avg Queue Time: %.3fms\n"), AverageQueueTime * 1000.0f);

    Stats += TEXT("\n--- Execution ---\n");
    Stats += FString::Printf(TEXT("Total Executed: %d\n"), TotalOperationsExecuted.load());
    float SuccessRate = TotalOperationsExecuted.load() > 0 ?
        (float)SuccessfulOperations.load() / TotalOperationsExecuted.load() * 100.0f : 0.0f;
    Stats += FString::Printf(TEXT("Success Rate: %.1f%%\n"), SuccessRate);
    Stats += FString::Printf(TEXT("Failed: %d\n"), FailedOperations.load());
    Stats += FString::Printf(TEXT("Cancelled: %d\n"), CancelledOperations.load());
    Stats += FString::Printf(TEXT("Avg Execution: %.3fms\n"), AverageExecutionTime * 1000.0f);

    Stats += TEXT("\n--- Cache ---\n");
    Stats += FString::Printf(TEXT("Hit Rate: %.1f%%\n"), CacheHitRate * 100.0f);
    Stats += ValidationCache->GetStatistics().ToString() + TEXT("\n");
    Stats += ResultCache->GetStatistics().ToString() + TEXT("\n");

    if (bEnableObjectPooling)
    {
        Stats += TEXT("\n--- Object Pools ---\n");
        Stats += GetPoolStatistics();
    }

    // Add unified metrics
    Stats += ServiceMetrics.ToString(TEXT("OperationService"));

    return Stats;
}

//========================================
// Ownership and Authority
//========================================

void USuspenseCoreEquipmentOperationService::InitializeWithOwner(APlayerState* InOwnerPS, bool bInServerAuthority)
{
    SCOPED_SERVICE_TIMER("InitializeWithOwner");

    OwnerPlayerState = InOwnerPS;
    bServerAuthority = bInServerAuthority;

    if (InOwnerPS)
    {
        const FUniqueNetIdRepl& UniqueId = InOwnerPS->GetUniqueId();
        if (UniqueId.IsValid())
        {
            // Избегаем перегрузок GetTypeHash(FString): считаем CRC напрямую по строке
            const FString IdStr = UniqueId->ToString();
            const uint32 H = FCrc::StrCrc32(*IdStr);

            // Дет-микширование для FGuid (стабильно при одинаковом UniqueId)
            const uint32 A = H;
            const uint32 B = H ^ 0xA5A55A5A;
            const uint32 C = (H << 1);      // простая вариация
            const uint32 D = (H >> 1);

            OwnerPlayerGuid = FGuid(A, B, C, D);
        }
        else
        {
            OwnerPlayerGuid = FGuid(); // invalid
        }
    }
    else
    {
        OwnerPlayerGuid = FGuid(); // invalid
    }

    ServiceMetrics.RecordSuccess();
    UE_LOG(LogSuspenseCoreEquipmentOperations, Log,
        TEXT("Initialized with owner: %s, Authority: %s"),
        InOwnerPS ? *InOwnerPS->GetPlayerName() : TEXT("None"),
        bInServerAuthority ? TEXT("Server") : TEXT("Client"));
}


//========================================
// IEquipmentOperationService Implementation
//========================================

ISuspenseCoreEquipmentOperations* USuspenseCoreEquipmentOperationService::GetOperationsExecutor()
{
    SCOPED_SERVICE_TIMER("GetOperationsExecutor");
    FRWScopeLock Lock(ExecutorLock, SLT_ReadOnly);
    return OperationsExecutor.GetInterface();
}

bool USuspenseCoreEquipmentOperationService::QueueOperation(const FEquipmentOperationRequest& Request)
{
    SCOPED_SERVICE_TIMER("QueueOperation");
    return QueueOperationWithPriority(Request, static_cast<int32>(Request.Priority)) != INDEX_NONE;
}

void USuspenseCoreEquipmentOperationService::ProcessOperationQueue()
{
    SCOPED_SERVICE_TIMER("ProcessOperationQueue");

    if (!bQueueProcessingEnabled)
    {
        return;
    }

    if (bIsProcessingQueue)
    {
        return;
    }

    bIsProcessingQueue = true;

    TArray<FSuspenseCoreQueuedOperation*> BatchToProcess;
    {
        FRWScopeLock Lock(QueueLock, SLT_Write);

        if (bClearQueueAfterProcessing)
        {
            for (FSuspenseCoreQueuedOperation* Op : OperationQueue)
            {
                ReleaseOperation(Op);
            }
            OperationQueue.Empty();
            bClearQueueAfterProcessing = false;
            bIsProcessingQueue = false;
            return;
        }

        // ВАЖНО: для TArray<T*> предикат принимает (const T&, const T&)
        OperationQueue.Sort([](const FSuspenseCoreQueuedOperation& A, const FSuspenseCoreQueuedOperation& B)
        {
            if (A.Priority != B.Priority)
            {
                // выше приоритет — раньше в очереди
                return A.Priority > B.Priority;
            }
            // более раннее время постановки — раньше
            return A.QueueTime < B.QueueTime;
        });

        const int32 BatchCount = FMath::Min(BatchSize, OperationQueue.Num());
        for (int32 i = 0; i < BatchCount; i++)
        {
            BatchToProcess.Add(OperationQueue[0]);
            OperationQueue.RemoveAt(0);
        }
    }

    for (FSuspenseCoreQueuedOperation* QueuedOp : BatchToProcess)
    {
        const float QueueTimeSec = FPlatformTime::Seconds() - QueuedOp->QueueTime;
        AverageQueueTime = AverageQueueTime * 0.9f + QueueTimeSec * 0.1f;
        ServiceMetrics.AddDurationMs(TEXT("QueueLatency"), QueueTimeSec * 1000.0f);

        const FEquipmentOperationResult Result = ProcessSingleOperation(QueuedOp);
        UpdateStatistics(Result);

        ReleaseOperation(QueuedOp);
    }

    bIsProcessingQueue = false;
    ServiceMetrics.Inc(TEXT("QueueProcessingCycles"));
}

//========================================
// Operation Execution
//========================================

FEquipmentOperationResult USuspenseCoreEquipmentOperationService::ExecuteImmediate(const FEquipmentOperationRequest& Request)
{
    SCOPED_SERVICE_TIMER("ExecuteImmediate");

    if (!IsServiceReady())
    {
        ServiceMetrics.RecordError();
        return FEquipmentOperationResult::CreateFailure(
            Request.OperationId,
            NSLOCTEXT("Equipment", "ServiceNotReady", "Service not ready"),
            EEquipmentValidationFailure::SystemError
        );
    }

    // Ensure operation has valid ID
    FEquipmentOperationRequest LocalRequest = Request;
    if (!LocalRequest.OperationId.IsValid())
    {
        LocalRequest.OperationId = FGuid::NewGuid();
    }

    // Check if should delegate to server
    if (ShouldDelegateToServer(LocalRequest))
    {
        ServiceMetrics.Inc(TEXT("DelegatedToServer"));
        return DelegateOperationToServer(LocalRequest);
    }

    FSuspenseCoreQueuedOperation* QueuedOp = AcquireOperation();
    QueuedOp->Request = LocalRequest;
    QueuedOp->QueueTime = FPlatformTime::Seconds();
    QueuedOp->Priority = static_cast<int32>(EEquipmentOperationPriority::Critical);

    FEquipmentOperationResult Result = ProcessSingleOperation(QueuedOp);

    ReleaseOperation(QueuedOp);

    if (Result.bSuccess)
    {
        ServiceMetrics.RecordSuccess();
    }
    else
    {
        ServiceMetrics.RecordError();
    }

    return Result;
}

int32 USuspenseCoreEquipmentOperationService::QueueOperationWithPriority(const FEquipmentOperationRequest& Request, int32 Priority)
{
    SCOPED_SERVICE_TIMER("QueueOperationWithPriority");

    if (!IsServiceReady())
    {
        ServiceMetrics.RecordError();
        return INDEX_NONE;
    }

    // Ensure operation has valid ID
    FEquipmentOperationRequest LocalRequest = Request;
    if (!LocalRequest.OperationId.IsValid())
    {
        LocalRequest.OperationId = FGuid::NewGuid();
    }

    FRWScopeLock Lock(QueueLock, SLT_Write);

    if (OperationQueue.Num() >= MaxQueueSize)
    {
        UE_LOG(LogSuspenseCoreEquipmentOperations, Warning,
            TEXT("Queue full - rejecting operation %s"),
            *LocalRequest.OperationId.ToString());
        ServiceMetrics.Inc(TEXT("QueueRejections"));
        ServiceMetrics.RecordError();
        return INDEX_NONE;
    }

    FSuspenseCoreQueuedOperation* QueuedOp = AcquireOperation();
    QueuedOp->Request = LocalRequest;
    QueuedOp->QueueTime = FPlatformTime::Seconds();
    QueuedOp->Priority = Priority;

    // Try coalescing if enabled
    if (bEnableQueueCoalescing)
    {
        const int32 CoalescedIndex = TryCoalesceOperation(QueuedOp);
        if (CoalescedIndex != INDEX_NONE)
        {
            ReleaseOperation(QueuedOp);
            ServiceMetrics.Inc(TEXT("OperationsCoalesced"));
            return CoalescedIndex; // Возвращаем реальный индекс объединённой операции
        }
    }

    int32 Position = OperationQueue.Add(QueuedOp);

    TotalOperationsQueued.fetch_add(1);
    PeakQueueSize = FMath::Max(PeakQueueSize, OperationQueue.Num());
    ServiceMetrics.Inc(TEXT("OperationsQueued"));

    OnOperationQueued.Broadcast(LocalRequest.OperationId);

    if (bEnableDetailedLogging)
    {
        UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose,
            TEXT("Queued operation %s at position %d"),
            *LocalRequest.GetDescription(),
            Position);
    }

    ServiceMetrics.RecordSuccess();
    return Position;
}

FGuid USuspenseCoreEquipmentOperationService::BatchOperations(const TArray<FEquipmentOperationRequest>& Requests, bool bAtomic)
{
    SCOPED_SERVICE_TIMER("BatchOperations");

    if (Requests.Num() == 0)
    {
        return FGuid();
    }

    FGuid BatchId = FGuid::NewGuid();
    TArray<FSuspenseCoreQueuedOperation*> BatchOps;

    for (FEquipmentOperationRequest Request : Requests)
    {
        if (!Request.OperationId.IsValid())
        {
            Request.OperationId = FGuid::NewGuid();
        }

        FSuspenseCoreQueuedOperation* QueuedOp = AcquireOperation();
        QueuedOp->Request = Request;
        QueuedOp->QueueTime = FPlatformTime::Seconds();
        QueuedOp->Priority = static_cast<int32>(EEquipmentOperationPriority::High);
        QueuedOp->TransactionId = bAtomic ? BatchId : FGuid();

        BatchOps.Add(QueuedOp);
    }

    {
        FRWScopeLock Lock(QueueLock, SLT_Write);
        ActiveBatches.Add(BatchId, BatchOps);
    }

    bool bSuccess = ProcessBatch(BatchOps, bAtomic);

    {
        FRWScopeLock Lock(QueueLock, SLT_Write);

        TArray<FSuspenseCoreQueuedOperation*>* StoredBatch = ActiveBatches.Find(BatchId);
        if (StoredBatch)
        {
            for (FSuspenseCoreQueuedOperation* Op : *StoredBatch)
            {
                ReleaseOperation(Op);
            }
        }

        ActiveBatches.Remove(BatchId);
    }

    OnBatchCompleted.Broadcast(BatchId, bSuccess);
    ServiceMetrics.Inc(TEXT("BatchesProcessed"));

    if (bSuccess)
    {
        ServiceMetrics.RecordSuccess();
    }
    else
    {
        ServiceMetrics.RecordError();
    }

    return BatchId;
}

FGuid USuspenseCoreEquipmentOperationService::BatchOperationsEx(const TArray<FEquipmentOperationRequest>& Requests,
                                                         bool bAtomic,
                                                         TArray<FEquipmentOperationResult>& OutResults)
{
    SCOPED_SERVICE_TIMER("BatchOperationsEx");

    OutResults.Empty();

    if (Requests.Num() == 0)
    {
        return FGuid();
    }

    FGuid BatchId = FGuid::NewGuid();
    TArray<FSuspenseCoreQueuedOperation*> BatchOps;

    for (FEquipmentOperationRequest Request : Requests)
    {
        if (!Request.OperationId.IsValid())
        {
            Request.OperationId = FGuid::NewGuid();
        }

        FSuspenseCoreQueuedOperation* QueuedOp = AcquireOperation();
        QueuedOp->Request = Request;
        QueuedOp->QueueTime = FPlatformTime::Seconds();
        QueuedOp->Priority = static_cast<int32>(EEquipmentOperationPriority::High);
        QueuedOp->TransactionId = bAtomic ? BatchId : FGuid();

        BatchOps.Add(QueuedOp);
    }

    {
        FRWScopeLock Lock(QueueLock, SLT_Write);
        ActiveBatches.Add(BatchId, BatchOps);
    }

    bool bSuccess = ProcessBatch(BatchOps, bAtomic, &OutResults);

    {
        FRWScopeLock Lock(QueueLock, SLT_Write);

        TArray<FSuspenseCoreQueuedOperation*>* StoredBatch = ActiveBatches.Find(BatchId);
        if (StoredBatch)
        {
            for (FSuspenseCoreQueuedOperation* Op : *StoredBatch)
            {
                ReleaseOperation(Op);
            }
        }

        ActiveBatches.Remove(BatchId);
    }

    OnBatchCompleted.Broadcast(BatchId, bSuccess);
    ServiceMetrics.Inc(TEXT("BatchesProcessedEx"));

    return BatchId;
}

//========================================
// Queue Management
//========================================

bool USuspenseCoreEquipmentOperationService::CancelQueuedOperation(const FGuid& OperationId)
{
    SCOPED_SERVICE_TIMER("CancelQueuedOperation");

    FRWScopeLock Lock(QueueLock, SLT_Write);

    for (int32 i = 0; i < OperationQueue.Num(); i++)
    {
        if (OperationQueue[i]->Request.OperationId == OperationId)
        {
            FSuspenseCoreQueuedOperation* Op = OperationQueue[i];
            OperationQueue.RemoveAt(i);

            ReleaseOperation(Op);

            CancelledOperations.fetch_add(1);
            ServiceMetrics.Inc(TEXT("OperationsCancelled"));

            UE_LOG(LogSuspenseCoreEquipmentOperations, Log,
                TEXT("Cancelled operation %s"),
                *OperationId.ToString());

            return true;
        }
    }

    return false;
}

int32 USuspenseCoreEquipmentOperationService::GetQueueSize() const
{
    SCOPED_SERVICE_TIMER("GetQueueSize");
    FRWScopeLock Lock(QueueLock, SLT_ReadOnly);
    return OperationQueue.Num();
}

void USuspenseCoreEquipmentOperationService::ClearQueue(bool bForce)
{
    SCOPED_SERVICE_TIMER("ClearQueue");

    FRWScopeLock Lock(QueueLock, SLT_Write);

    if (!bForce && bIsProcessingQueue)
    {
        bClearQueueAfterProcessing = true;
        UE_LOG(LogSuspenseCoreEquipmentOperations, Warning,
            TEXT("Queue will be cleared after current processing cycle"));
        return;
    }

    int32 ClearedCount = OperationQueue.Num();

    for (FSuspenseCoreQueuedOperation* Op : OperationQueue)
    {
        ReleaseOperation(Op);
    }

    OperationQueue.Empty();
    bClearQueueAfterProcessing = false;

    CancelledOperations.fetch_add(ClearedCount);
    ServiceMetrics.Inc(TEXT("QueueClears"));

    // Усадка пулов после очистки очереди
    TrimPools(InitialPoolSize);

    UE_LOG(LogSuspenseCoreEquipmentOperations, Log,
        TEXT("Cleared %d operations from queue and trimmed pools"),
        ClearedCount);
}

void USuspenseCoreEquipmentOperationService::SetQueueProcessingEnabled(bool bEnabled)
{
    SCOPED_SERVICE_TIMER("SetQueueProcessingEnabled");

    bQueueProcessingEnabled = bEnabled;

    if (bEnabled)
    {
        StartQueueProcessing();
    }
    else
    {
        StopQueueProcessing();
    }
}

//========================================
// History and Undo/Redo
//========================================

FEquipmentOperationResult USuspenseCoreEquipmentOperationService::UndoLastOperation()
{
    SCOPED_SERVICE_TIMER("UndoLastOperation");

    FRWScopeLock Lock(HistoryLock, SLT_Write);

    if (OperationHistory.Num() == 0)
    {
        ServiceMetrics.RecordError();
        return FEquipmentOperationResult::CreateFailure(
            FGuid::NewGuid(),
            NSLOCTEXT("Equipment", "NoUndoHistory", "No operations to undo"),
            EEquipmentValidationFailure::SystemError
        );
    }

    for (int32 i = OperationHistory.Num() - 1; i >= 0; i--)
    {
        if (OperationHistory[i].bCanUndo)
        {
            FSuspenseCoreOperationHistoryEntry& Entry = OperationHistory[i];

            if (DataProvider.GetInterface())
            {
                DataProvider->RestoreSnapshot(Entry.StateBefore);
            }

            RedoStack.Add(Entry);
            OperationHistory.RemoveAt(i);

            FEquipmentOperationResult Result;
            Result.bSuccess = true;
            Result.OperationId = Entry.Request.OperationId;

            OnOperationCompleted.Broadcast(Result);
            ServiceMetrics.Inc(TEXT("UndoOperations"));
            ServiceMetrics.RecordSuccess();

            return Result;
        }
    }

    ServiceMetrics.RecordError();
    return FEquipmentOperationResult::CreateFailure(
        FGuid::NewGuid(),
        NSLOCTEXT("Equipment", "NoUndoableOps", "No undoable operations"),
        EEquipmentValidationFailure::SystemError
    );
}

FEquipmentOperationResult USuspenseCoreEquipmentOperationService::RedoLastOperation()
{
    SCOPED_SERVICE_TIMER("RedoLastOperation");

    FRWScopeLock Lock(HistoryLock, SLT_Write);

    if (RedoStack.Num() == 0)
    {
        ServiceMetrics.RecordError();
        return FEquipmentOperationResult::CreateFailure(
            FGuid::NewGuid(),
            NSLOCTEXT("Equipment", "NoRedoHistory", "No operations to redo"),
            EEquipmentValidationFailure::SystemError
        );
    }

    FSuspenseCoreOperationHistoryEntry Entry = RedoStack.Pop();

    if (DataProvider.GetInterface())
    {
        DataProvider->RestoreSnapshot(Entry.StateAfter);
    }

    OperationHistory.Add(Entry);

    FEquipmentOperationResult Result;
    Result.bSuccess = true;
    Result.OperationId = Entry.Request.OperationId;

    OnOperationCompleted.Broadcast(Result);
    ServiceMetrics.Inc(TEXT("RedoOperations"));
    ServiceMetrics.RecordSuccess();

    return Result;
}

TArray<FSuspenseCoreOperationHistoryEntry> USuspenseCoreEquipmentOperationService::GetOperationHistory(int32 MaxCount) const
{
    SCOPED_SERVICE_TIMER("GetOperationHistory");

    FRWScopeLock Lock(HistoryLock, SLT_ReadOnly);

    TArray<FSuspenseCoreOperationHistoryEntry> Result;
    int32 StartIndex = FMath::Max(0, OperationHistory.Num() - MaxCount);

    for (int32 i = StartIndex; i < OperationHistory.Num(); i++)
    {
        Result.Add(OperationHistory[i]);
    }

    return Result;
}

void USuspenseCoreEquipmentOperationService::ClearHistory()
{
    SCOPED_SERVICE_TIMER("ClearHistory");

    FRWScopeLock Lock(HistoryLock, SLT_Write);
    OperationHistory.Empty();
    RedoStack.Empty();

    ServiceMetrics.Inc(TEXT("HistoryClears"));
    UE_LOG(LogSuspenseCoreEquipmentOperations, Log, TEXT("Operation history cleared"));
}

bool USuspenseCoreEquipmentOperationService::CanUndo() const
{
    SCOPED_SERVICE_TIMER("CanUndo");

    FRWScopeLock Lock(HistoryLock, SLT_ReadOnly);

    for (const FSuspenseCoreOperationHistoryEntry& Entry : OperationHistory)
    {
        if (Entry.bCanUndo)
        {
            return true;
        }
    }

    return false;
}

bool USuspenseCoreEquipmentOperationService::CanRedo() const
{
    SCOPED_SERVICE_TIMER("CanRedo");

    FRWScopeLock Lock(HistoryLock, SLT_ReadOnly);
    return RedoStack.Num() > 0;
}

//========================================
// Metrics and Telemetry
//========================================

bool USuspenseCoreEquipmentOperationService::ExportMetricsToCSV(const FString& FilePath) const
{
    SCOPED_SERVICE_TIMER("ExportMetricsToCSV");

    FString AbsolutePath = FPaths::ProjectSavedDir() / TEXT("Metrics") / FilePath;
    bool bSuccess = ServiceMetrics.ExportToCSV(AbsolutePath, TEXT("OperationService"));

    if (bSuccess)
    {
        UE_LOG(LogSuspenseCoreEquipmentOperations, Log, TEXT("Metrics exported to: %s"), *AbsolutePath);
    }
    else
    {
        UE_LOG(LogSuspenseCoreEquipmentOperations, Error, TEXT("Failed to export metrics to: %s"), *AbsolutePath);
    }

    return bSuccess;
}

void USuspenseCoreEquipmentOperationService::ResetMetrics()
{
    SCOPED_SERVICE_TIMER("ResetMetrics");

    ServiceMetrics.Reset();

    // Сбрасываем legacy статистику
    {
        FRWScopeLock Lock(StatsLock, SLT_Write);
        TotalOperationsQueued.store(0);
        TotalOperationsExecuted.store(0);
        SuccessfulOperations.store(0);
        FailedOperations.store(0);
        CancelledOperations.store(0);
        TotalBatchesProcessed.store(0);
        CacheHitRate = 0.0f;
        AverageQueueTime = 0.0f;
        AverageExecutionTime = 0.0f;
        PeakQueueSize = 0;
    }

    // Сбрасываем статистику пулов
    OperationPoolHits.store(0);
    OperationPoolMisses.store(0);
    ResultPoolHits.store(0);
    ResultPoolMisses.store(0);
    PoolOverflows.store(0);

    // Очищаем кэши для чистого старта
    if (ValidationCache.IsValid())
    {
        ValidationCache->Clear();
    }
    if (ResultCache.IsValid())
    {
        ResultCache->Clear();
    }

    UE_LOG(LogSuspenseCoreEquipmentOperations, Log, TEXT("All metrics have been reset"));
}

//========================================
// NEW: Transaction Plan Support Methods (UPDATED)
//========================================

FTransactionOperation USuspenseCoreEquipmentOperationService::MakeTxnOpFromStep(const FSuspenseCoreTransactionPlanStep& Step) const
{
    FTransactionOperation Op;
    Op.OperationId = Step.Request.OperationId; // Guaranteed by plan step constructor
    Op.OperationType = MapOperationTypeToTag(Step.Request.OperationType);

    // Choose primary slot for the operation
    Op.SlotIndex = (Step.Request.TargetSlotIndex != INDEX_NONE)
        ? Step.Request.TargetSlotIndex
        : Step.Request.SourceSlotIndex;

    // Capture current state if we have access to DataProvider
    if (DataProvider.GetInterface() && Op.SlotIndex != INDEX_NONE)
    {
        Op.ItemBefore = DataProvider->GetSlotItem(Op.SlotIndex);
        Op.ItemAfter = Step.Request.ItemInstance; // Expected after state
    }

    Op.Timestamp = Step.Request.Timestamp;
    Op.bReversible = Step.bReversible;

    // Transfer any metadata from request parameters if available
    for (const auto& Param : Step.Request.Parameters)
    {
        Op.Metadata.Add(Param.Key, Param.Value);
    }

    return Op;
}

FGameplayTag USuspenseCoreEquipmentOperationService::MapOperationTypeToTag(EEquipmentOperationType OpType) const
{
    // Map operation types to gameplay tags used by transaction system
    switch (OpType)
    {
        case EEquipmentOperationType::Equip:
            return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Equip"));
        case EEquipmentOperationType::Unequip:
            return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Unequip"));
        case EEquipmentOperationType::Move:
            return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Move"));
        case EEquipmentOperationType::Swap:
            return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Swap"));
        case EEquipmentOperationType::Drop:
            return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Drop"));
        case EEquipmentOperationType::QuickSwitch:
            return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.QuickSwitch"));
        case EEquipmentOperationType::Transfer:
            return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Transfer"));
        case EEquipmentOperationType::Reload:
            return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Reload"));
        case EEquipmentOperationType::Repair:
            return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Repair"));
        case EEquipmentOperationType::Upgrade:
            return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Upgrade"));
        case EEquipmentOperationType::Modify:
            return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Modify"));
        case EEquipmentOperationType::Combine:
            return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Combine"));
        case EEquipmentOperationType::Split:
            return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Split"));
        default:
            return FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Unknown"));
    }
}

bool USuspenseCoreEquipmentOperationService::BatchValidatePlan(const FSuspenseCoreTransactionPlan& Plan, FText& OutError) const
{
    OutError = FText::GetEmpty();

    // Вариант 1: доверяем бизнес-валидатору (Executor.ValidatePlan)
    {
        FRWScopeLock Lock(ExecutorLock, SLT_ReadOnly);

        if (!OperationsExecutor.GetInterface())
        {
            OutError = NSLOCTEXT("EquipmentService", "NoExecutor", "No operations executor available");
            return false;
        }

        if (const USuspenseCoreEquipmentOperationExecutor* Exec = Cast<USuspenseCoreEquipmentOperationExecutor>(OperationsExecutor.GetObject()))
        {
            FText ExecError;
            if (!Exec->ValidatePlan(Plan, ExecError))
            {
                OutError = ExecError;
                return false;
            }
        }
        else
        {
            OutError = NSLOCTEXT("EquipmentService", "InvalidExecutor", "Executor doesn't support plan validation");
            return false;
        }
    }

    // Вариант 2 (на будущее): RulesEngine.BatchValidate(Plan) — сюда вставите батч-валидацию,
    // когда добавите соответствующий контракт. Сейчас — считаем, что проверки Executor'а достаточно.
    return true;
}

bool USuspenseCoreEquipmentOperationService::ExecutePlanTransactional(
    const FSuspenseCoreTransactionPlan& Plan,
    const FGuid& OuterTxnId,
    TArray<FEquipmentDelta>& OutDeltas)
{
    OutDeltas.Reset();

    FRWScopeLock Lock(ExecutorLock, SLT_ReadOnly);

    if (!TransactionManager.GetInterface())
    {
        UE_LOG(LogSuspenseCoreEquipmentOperations, Error, TEXT("No transaction manager available for plan execution"));
        return false;
    }

    // 1) Begin transaction (supports nesting)
    const FString TxnDescription = !Plan.DebugLabel.IsEmpty()
        ? Plan.DebugLabel
        : FString::Printf(TEXT("Plan_%s"), *Plan.PlanId.ToString());

    const FGuid TxnId = OuterTxnId.IsValid()
        ? OuterTxnId
        : TransactionManager->BeginTransaction(TxnDescription);

    const bool bOwnTxn = !OuterTxnId.IsValid();

    // === ПУТЬ 1: Расширенное API транзактора (идеальный) ===
    if (TransactionManager->SupportsExtendedOps())
    {
        for (int32 i = 0; i < Plan.Steps.Num(); ++i)
        {
            const FSuspenseCoreTransactionPlanStep& Step = Plan.Steps[i];
            FTransactionOperation Op = MakeTxnOpFromStep(Step);

            // Зарегистрировать → применить к рабочему снапшоту
            if (!TransactionManager->RegisterOperation(TxnId, Op))
            {
                UE_LOG(LogSuspenseCoreEquipmentOperations, Warning,
                    TEXT("Txn(Register) failed at step %d for plan %s"), i + 1, *Plan.PlanId.ToString());
                if (bOwnTxn) { TransactionManager->RollbackTransaction(TxnId); }
                return false;
            }

            if (!TransactionManager->ApplyOperation(TxnId, Op))
            {
                UE_LOG(LogSuspenseCoreEquipmentOperations, Warning,
                    TEXT("Txn(Apply) failed at step %d for plan %s"), i + 1, *Plan.PlanId.ToString());
                if (bOwnTxn) { TransactionManager->RollbackTransaction(TxnId); }
                return false;
            }
        }

        if (bOwnTxn)
        {
            // Get deltas BEFORE commit
            OutDeltas = TransactionManager->GetTransactionDeltas(TxnId);

            // Prefer explicit-delta commit when processor is available
            if (USuspenseCoreEquipmentTransactionProcessor* Processor = Cast<USuspenseCoreEquipmentTransactionProcessor>(TransactionManager.GetObject()))
            {
                if (!Processor->CommitTransaction(TxnId, OutDeltas))
                {
                    UE_LOG(LogSuspenseCoreEquipmentOperations, Error, TEXT("CommitWithDeltas failed for plan %s"), *Plan.PlanId.ToString());
                    return false;
                }
            }
            else
            {
                if (!TransactionManager->CommitTransaction(TxnId))
                {
                    UE_LOG(LogSuspenseCoreEquipmentOperations, Error, TEXT("Legacy commit failed for plan %s"), *Plan.PlanId.ToString());
                    return false;
                }
            }

            ServiceMetrics.Inc(TEXT("TransactionsCommitted"));
        }

        if (bEnableDetailedLogging)
        {
            UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose,
                TEXT("Plan %s executed via Extended TM (steps=%d, deltas=%d)"),
                *Plan.PlanId.ToString(), Plan.Steps.Num(), OutDeltas.Num());
        }

        return true;
    }

    // === ПУТЬ 2: Legacy fallback (прямые вызовы DataProvider без уведомлений) ===
    for (int32 i = 0; i < Plan.Steps.Num(); i++)
    {
        const FSuspenseCoreTransactionPlanStep& Step = Plan.Steps[i];

        FTransactionOperation Op = MakeTxnOpFromStep(Step);
        if (!TransactionManager->RegisterOperation(TxnId, Op))
        {
            UE_LOG(LogSuspenseCoreEquipmentOperations, Warning,
                TEXT("Fallback: Register failed at step %d for plan %s"), i + 1, *Plan.PlanId.ToString());
            if (bOwnTxn) { TransactionManager->RollbackTransaction(TxnId); }
            return false;
        }

        bool bApplied = false;

        // Применяем операцию через DataProvider с bNotify=false и формируем OutDeltas вручную
        switch (Step.Request.OperationType)
        {
            case EEquipmentOperationType::Equip:
            {
                FSuspenseInventoryItemInstance OldItem = DataProvider->GetSlotItem(Step.Request.TargetSlotIndex);
                bApplied = DataProvider->SetSlotItem(Step.Request.TargetSlotIndex, Step.Request.ItemInstance, /*bNotify=*/false);

                if (bApplied)
                {
                    // Создаём дельту вручную
                    FEquipmentDelta Delta;
                    Delta.ChangeType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Change.Equip"));
                    Delta.SlotIndex = Step.Request.TargetSlotIndex;
                    Delta.ItemBefore = OldItem;
                    Delta.ItemAfter = Step.Request.ItemInstance;
                    Delta.SourceTransactionId = TxnId;
                    Delta.OperationId = Step.Request.OperationId;
                    Delta.ReasonTag = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Reason.Transaction"));
                    Delta.Timestamp = FDateTime::Now();
                    OutDeltas.Add(Delta);
                }
                break;
            }

            case EEquipmentOperationType::Unequip:
            {
                FSuspenseInventoryItemInstance OldItem = DataProvider->GetSlotItem(Step.Request.SourceSlotIndex);
                FSuspenseInventoryItemInstance ClearedItem = DataProvider->ClearSlot(Step.Request.SourceSlotIndex, /*bNotify=*/false);
                bApplied = ClearedItem.IsValid();

                if (bApplied)
                {
                    FEquipmentDelta Delta;
                    Delta.ChangeType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Change.Unequip"));
                    Delta.SlotIndex = Step.Request.SourceSlotIndex;
                    Delta.ItemBefore = OldItem;
                    Delta.ItemAfter = FSuspenseInventoryItemInstance(); // Empty
                    Delta.SourceTransactionId = TxnId;
                    Delta.OperationId = Step.Request.OperationId;
                    Delta.ReasonTag = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Reason.Transaction"));
                    Delta.Timestamp = FDateTime::Now();
                    OutDeltas.Add(Delta);
                }
                break;
            }

            case EEquipmentOperationType::Move:
            {
                FSuspenseInventoryItemInstance SourceItem = DataProvider->GetSlotItem(Step.Request.SourceSlotIndex);
                FSuspenseInventoryItemInstance TargetItem = DataProvider->GetSlotItem(Step.Request.TargetSlotIndex);

                // Clear source
                DataProvider->ClearSlot(Step.Request.SourceSlotIndex, /*bNotify=*/false);
                // Set target
                bApplied = DataProvider->SetSlotItem(Step.Request.TargetSlotIndex, SourceItem, /*bNotify=*/false);

                if (bApplied)
                {
                    // Создаём две дельты для move
                    FEquipmentDelta Delta1;
                    Delta1.ChangeType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Change.Move"));
                    Delta1.SlotIndex = Step.Request.SourceSlotIndex;
                    Delta1.ItemBefore = SourceItem;
                    Delta1.ItemAfter = FSuspenseInventoryItemInstance();
                    Delta1.SourceTransactionId = TxnId;
                    Delta1.OperationId = Step.Request.OperationId;
                    Delta1.ReasonTag = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Reason.Transaction"));
                    Delta1.Timestamp = FDateTime::Now();
                    OutDeltas.Add(Delta1);

                    FEquipmentDelta Delta2;
                    Delta2.ChangeType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Change.Move"));
                    Delta2.SlotIndex = Step.Request.TargetSlotIndex;
                    Delta2.ItemBefore = TargetItem;
                    Delta2.ItemAfter = SourceItem;
                    Delta2.SourceTransactionId = TxnId;
                    Delta2.OperationId = Step.Request.OperationId;
                    Delta2.ReasonTag = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Reason.Transaction"));
                    Delta2.Timestamp = FDateTime::Now();
                    OutDeltas.Add(Delta2);
                }
                break;
            }

            case EEquipmentOperationType::Swap:
            {
                FSuspenseInventoryItemInstance ItemA = DataProvider->GetSlotItem(Step.Request.SourceSlotIndex);
                FSuspenseInventoryItemInstance ItemB = DataProvider->GetSlotItem(Step.Request.TargetSlotIndex);

                // Perform swap
                DataProvider->SetSlotItem(Step.Request.SourceSlotIndex, ItemB, /*bNotify=*/false);
                bApplied = DataProvider->SetSlotItem(Step.Request.TargetSlotIndex, ItemA, /*bNotify=*/false);

                if (bApplied)
                {
                    // Создаём дельты для обоих слотов
                    FEquipmentDelta Delta1;
                    Delta1.ChangeType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Change.Swap"));
                    Delta1.SlotIndex = Step.Request.SourceSlotIndex;
                    Delta1.ItemBefore = ItemA;
                    Delta1.ItemAfter = ItemB;
                    Delta1.SourceTransactionId = TxnId;
                    Delta1.OperationId = Step.Request.OperationId;
                    Delta1.ReasonTag = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Reason.Transaction"));
                    Delta1.Timestamp = FDateTime::Now();
                    OutDeltas.Add(Delta1);

                    FEquipmentDelta Delta2;
                    Delta2.ChangeType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Change.Swap"));
                    Delta2.SlotIndex = Step.Request.TargetSlotIndex;
                    Delta2.ItemBefore = ItemB;
                    Delta2.ItemAfter = ItemA;
                    Delta2.SourceTransactionId = TxnId;
                    Delta2.OperationId = Step.Request.OperationId;
                    Delta2.ReasonTag = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Reason.Transaction"));
                    Delta2.Timestamp = FDateTime::Now();
                    OutDeltas.Add(Delta2);
                }
                break;
            }

            default:
                // Для других операций просто регистрируем без применения
                // Они могут требовать специальной обработки
                bApplied = true;
                break;
        }

        if (!bApplied)
        {
            UE_LOG(LogSuspenseCoreEquipmentOperations, Warning,
                TEXT("Fallback: Apply failed at step %d for plan %s"), i + 1, *Plan.PlanId.ToString());
            if (bOwnTxn) { TransactionManager->RollbackTransaction(TxnId); }
            return false;
        }
    }

    if (bOwnTxn)
    {
        if (USuspenseCoreEquipmentTransactionProcessor* Processor = Cast<USuspenseCoreEquipmentTransactionProcessor>(TransactionManager.GetObject()))
        {
            if (!Processor->CommitTransaction(TxnId, OutDeltas))
            {
                UE_LOG(LogSuspenseCoreEquipmentOperations, Error, TEXT("Fallback commit with deltas failed for plan %s"), *Plan.PlanId.ToString());
                return false;
            }
        }
        else
        {
            if (!TransactionManager->CommitTransaction(TxnId))
            {
                UE_LOG(LogSuspenseCoreEquipmentOperations, Error, TEXT("Fallback legacy commit failed for plan %s"), *Plan.PlanId.ToString());
                return false;
            }
        }
        ServiceMetrics.Inc(TEXT("TransactionsCommitted"));
    }

    if (bEnableDetailedLogging)
    {
        UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose,
            TEXT("Plan %s executed via FALLBACK (steps=%d, deltas=%d)"),
            *Plan.PlanId.ToString(), Plan.Steps.Num(), OutDeltas.Num());
    }

    return true;
}

bool USuspenseCoreEquipmentOperationService::CommitTransactionWithDeltas(const FGuid& TxnId, const TArray<FEquipmentDelta>& Deltas)
{
    FRWScopeLock Lock(ExecutorLock, SLT_ReadOnly);

    if (!TransactionManager.GetInterface())
    {
        return false;
    }

    // Попытка использовать новую перегрузку если доступна
    if (USuspenseCoreEquipmentTransactionProcessor* Processor = Cast<USuspenseCoreEquipmentTransactionProcessor>(TransactionManager.GetObject()))
    {
        // Используем новую перегрузку с явным указанием дельт
        return Processor->CommitTransaction(TxnId, Deltas);
    }
    else
    {
        // Fallback на старый метод если новая перегрузка недоступна
        UE_LOG(LogSuspenseCoreEquipmentOperations, Warning,
            TEXT("CommitTransactionWithDeltas: Using legacy commit (deltas will not be controlled)"));
        return TransactionManager->CommitTransaction(TxnId);
    }
}

// ========================================
// Legacy Compatibility Methods
// ========================================

FSuspenseCoreTransactionPlanStep USuspenseCoreEquipmentOperationService::MakePlanStepFromRequest(const FEquipmentOperationRequest& Request) const
{
    // Human-readable description for logs/metrics
    FString Description = FString::Printf(TEXT("Direct operation: %s"), *Request.GetDescription());
    FSuspenseCoreTransactionPlanStep Step(Request, Description);
    Step.StepPriority = static_cast<int32>(Request.Priority);

    // Estimate reversibility based on operation type
    switch (Request.OperationType)
    {
        case EEquipmentOperationType::Equip:
        case EEquipmentOperationType::Unequip:
        case EEquipmentOperationType::Move:
        case EEquipmentOperationType::Swap:
        case EEquipmentOperationType::Modify:
        case EEquipmentOperationType::Split:
            Step.bReversible = true;
            break;
        case EEquipmentOperationType::Drop:
        case EEquipmentOperationType::Repair:
        case EEquipmentOperationType::Upgrade:
        case EEquipmentOperationType::Combine:
        case EEquipmentOperationType::Reload:
            Step.bReversible = false;
            break;
        default:
            Step.bReversible = true;
            break;
    }
    return Step;
}

bool USuspenseCoreEquipmentOperationService::BuildSingleStepPlanFromRequest(const FEquipmentOperationRequest& Request, FSuspenseCoreTransactionPlan& OutPlan) const
{
    // Build minimal compatible plan from single request
    OutPlan = FSuspenseCoreTransactionPlan();
    OutPlan.DebugLabel = FString::Printf(TEXT("CompatPlan-%s-%s"),
        *Request.OperationId.ToString(),
        *UEnum::GetValueAsString(Request.OperationType));

    const FSuspenseCoreTransactionPlanStep Step = MakePlanStepFromRequest(Request);
    OutPlan.Add(Step);

    // Compatible plan is atomic with reversibility from the step
    OutPlan.bAtomic = true;
    OutPlan.bReversible = Step.bReversible;
    OutPlan.Metadata.Add(TEXT("Compat"), TEXT("true"));

    return true;
}

bool USuspenseCoreEquipmentOperationService::ProcessBatchUsingPlans(
    const TArray<FSuspenseCoreQueuedOperation*>& BatchOps,
    bool bAtomic,
    TArray<FEquipmentOperationResult>* OutResults)
{
    if (BatchOps.Num() == 0)
    {
        if (OutResults) OutResults->Empty();
        return true;
    }

    // For non-atomic batches, fall back to sequential processing
    if (!bAtomic)
    {
        return false; // Signal to use existing branch
    }

    // Get executor
    FRWScopeLock ExecLock(ExecutorLock, SLT_ReadOnly);
    if (!OperationsExecutor.GetInterface())
    {
        if (OutResults) OutResults->Empty();
        return false;
    }

    USuspenseCoreEquipmentOperationExecutor* Executor = Cast<USuspenseCoreEquipmentOperationExecutor>(OperationsExecutor.GetObject());
    if (!Executor)
    {
        if (OutResults) OutResults->Empty();
        return false;
    }

    // 1) Build combined plan (preserving operation order)
    FSuspenseCoreTransactionPlan CombinedPlan;
    CombinedPlan.DebugLabel = FString::Printf(TEXT("Batch-%dOps"), BatchOps.Num());
    CombinedPlan.bAtomic = true;
    CombinedPlan.bReversible = true;

    TArray<FEquipmentOperationRequest> Requests;
    Requests.Reserve(BatchOps.Num());
    for (FSuspenseCoreQueuedOperation* Op : BatchOps)
    {
        Requests.Add(Op->Request);
    }

    // Build individual plans and merge steps
    for (const FEquipmentOperationRequest& Req : Requests)
    {
        FSuspenseCoreTransactionPlan LocalPlan;
        FText Err;

        if (bUseTransactionPlans)
        {
            if (!Executor->BuildPlan(Req, LocalPlan, Err))
            {
                // Fill OutResults with failures
                if (OutResults)
                {
                    OutResults->Empty();
                    for (const FSuspenseCoreQueuedOperation* Op : BatchOps)
                    {
                        FEquipmentOperationResult R = FEquipmentOperationResult::CreateFailure(
                            Op->Request.OperationId, Err, EEquipmentValidationFailure::SystemError);
                        OutResults->Add(R);
                    }
                }
                return false;
            }
        }
        else
        {
            if (!BuildSingleStepPlanFromRequest(Req, LocalPlan))
            {
                if (OutResults)
                {
                    OutResults->Empty();
                    for (const FSuspenseCoreQueuedOperation* Op : BatchOps)
                    {
                        FEquipmentOperationResult R = FEquipmentOperationResult::CreateFailure(
                            Op->Request.OperationId,
                            NSLOCTEXT("EquipmentService", "CompatPlanFailed", "Failed to build compatible plan"),
                            EEquipmentValidationFailure::SystemError);
                        OutResults->Add(R);
                    }
                }
                return false;
            }
        }

        // Merge steps into combined plan
        for (const FSuspenseCoreTransactionPlanStep& Step : LocalPlan.Steps)
        {
            CombinedPlan.Add(Step);
        }
    }

    // 2) Pre-validate combined sequence
    {
        FText ValidationError;
        if (!BatchValidatePlan(CombinedPlan, ValidationError))
        {
            if (OutResults)
            {
                OutResults->Empty();
                for (const FSuspenseCoreQueuedOperation* Op : BatchOps)
                {
                    FEquipmentOperationResult R = FEquipmentOperationResult::CreateFailure(
                        Op->Request.OperationId,
                        ValidationError,
                        EEquipmentValidationFailure::RequirementsNotMet);
                    OutResults->Add(R);
                }
            }
            return false;
        }
    }

    // 3) Single transaction for all operations
    FGuid BatchTxnId;
    if (TransactionManager.GetInterface())
    {
        BatchTxnId = TransactionManager->BeginTransaction(TEXT("Batch Combined Plan"));
    }

    // Snapshot before batch
    FEquipmentStateSnapshot StateBefore;
    if (DataProvider.GetInterface())
    {
        StateBefore = DataProvider->CreateSnapshot();
    }

    // Send start events for UI consistency
    for (const FSuspenseCoreQueuedOperation* Op : BatchOps)
    {
        OnOperationStarted.Broadcast(Op->Request);
    }

    // Execute combined plan within the pre-started transaction
    TArray<FEquipmentDelta> Deltas;
    const bool bExecOk = ExecutePlanTransactional(CombinedPlan, BatchTxnId, Deltas);

    if (!TransactionManager.GetInterface() || !BatchTxnId.IsValid())
    {
        // No transaction manager - protocol failure
        if (OutResults)
        {
            OutResults->Empty();
            for (const FSuspenseCoreQueuedOperation* Op : BatchOps)
            {
                FEquipmentOperationResult R = FEquipmentOperationResult::CreateFailure(
                    Op->Request.OperationId,
                    NSLOCTEXT("EquipmentService", "NoTxnManager", "No transaction manager available"),
                    EEquipmentValidationFailure::SystemError);
                OutResults->Add(R);
            }
        }
        return false;
    }

    bool bCommitOk = false;
    if (bExecOk)
    {
        TArray<FEquipmentDelta> BatchDeltas = TransactionManager->GetTransactionDeltas(BatchTxnId);

        if (USuspenseCoreEquipmentTransactionProcessor* Processor = Cast<USuspenseCoreEquipmentTransactionProcessor>(TransactionManager.GetObject()))
        {
            bCommitOk = Processor->CommitTransaction(BatchTxnId, BatchDeltas);
        }
        else
        {
            bCommitOk = TransactionManager->CommitTransaction(BatchTxnId);
        }

        if (!bCommitOk)
        {
            UE_LOG(LogSuspenseCoreEquipmentOperations, Error, TEXT("Batch commit failed (%s)"), *BatchTxnId.ToString());
            return false;
        }

        ServiceMetrics.Inc(TEXT("BatchTransactionsCommitted"));
    }
    else
    {
        TransactionManager->RollbackTransaction(BatchTxnId);
        ServiceMetrics.Inc(TEXT("BatchTransactionsRolledBack"));
    }

    // Generate per-operation results
    if (OutResults)
    {
        OutResults->Empty();
        for (const FSuspenseCoreQueuedOperation* Op : BatchOps)
        {
            if (bExecOk && bCommitOk)
            {
                FEquipmentOperationResult R = FEquipmentOperationResult::CreateSuccess(Op->Request.OperationId);
                R.ResultMetadata.Add(TEXT("CombinedPlan"), TEXT("true"));
                R.ResultMetadata.Add(TEXT("PlanId"), CombinedPlan.PlanId.ToString());
                R.ResultMetadata.Add(TEXT("PlanSteps"), FString::FromInt(CombinedPlan.Num()));
                OutResults->Add(R);

                // Cache, events and logging - same as single operation
                ResultCache->Set(Op->Request.OperationId, R, ResultCacheTTL);
                OnOperationCompleted.Broadcast(R);
                PublishOperationEvent(R);
                LogOperation(Op->Request, R);
            }
            else
            {
                FEquipmentOperationResult R = FEquipmentOperationResult::CreateFailure(
                    Op->Request.OperationId,
                    NSLOCTEXT("EquipmentService", "BatchFailed", "Batch failed"),
                    EEquipmentValidationFailure::SystemError);
                OutResults->Add(R);

                ResultCache->Set(Op->Request.OperationId, R, ResultCacheTTL);
                OnOperationCompleted.Broadcast(R);
                PublishOperationEvent(R);
                LogOperation(Op->Request, R);
            }
        }
    }

    // Record history for successful batch
    if (bExecOk && bCommitOk)
    {
        for (const FSuspenseCoreQueuedOperation* Op : BatchOps)
        {
            FEquipmentOperationResult Dummy = FEquipmentOperationResult::CreateSuccess(Op->Request.OperationId);
            RecordOperation(Op->Request, Dummy, StateBefore);
        }
    }

    return bExecOk && bCommitOk;
}

//========================================
// Protected Methods - Core Implementation
//========================================

bool USuspenseCoreEquipmentOperationService::InitializeDependencies()
{
    USuspenseCoreEquipmentServiceLocator* Locator = GetServiceLocator();
    if (!Locator)
    {
        UE_LOG(LogSuspenseCoreEquipmentOperations, Error,
            TEXT("InitializeDependencies: ServiceLocator not available"));
        return false;
    }

    UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose,
        TEXT("InitializeDependencies: Starting dependency resolution"));

    // 1) OperationsExecutor — НЕ обязательный на старте (инжектится из PlayerState позже)
    {
        FRWScopeLock R(ExecutorLock, SLT_ReadOnly);
        if (!OperationsExecutor.GetObject() || !OperationsExecutor.GetInterface())
        {
            UE_LOG(LogSuspenseCoreEquipmentOperations, Warning,
                TEXT("InitializeDependencies: OperationsExecutor not injected yet (will accept late injection from PlayerState)"));
        }
        else
        {
            UE_LOG(LogSuspenseCoreEquipmentOperations, Log,
                TEXT("InitializeDependencies: ✅ OperationsExecutor is present (%s)"),
                *OperationsExecutor.GetObject()->GetName());
        }
    }

    // 2) DataProvider — в STATELESS-режиме может отсутствовать на старте (НЕ падать)
{
    const FGameplayTag DataTag = FGameplayTag::RequestGameplayTag(FName("Service.Equipment.Data"));
    UObject* DataSvcObj = Locator->GetService(DataTag);
    if (!DataSvcObj)
    {
        UE_LOG(LogSuspenseCoreEquipmentOperations, Error,
            TEXT("InitializeDependencies: Data service not found (tag=%s)"), *DataTag.ToString());
        return false; // отсутствие самого сервиса — критично
    }

    bool bResolvedProvider = false;

    // Вариант A: сам сервис реализует ISuspenseEquipmentDataProvider
    if (DataSvcObj->GetClass()->ImplementsInterface(USuspenseCoreEquipmentDataProvider::StaticClass()))
    {
        DataProvider.SetObject(DataSvcObj);
        DataProvider.SetInterface(Cast<ISuspenseEquipmentDataProvider>(DataSvcObj));
        bResolvedProvider = (DataProvider.GetObject() && DataProvider.GetInterface());
        if (bResolvedProvider)
        {
            UE_LOG(LogSuspenseCoreEquipmentOperations, Log,
                TEXT("InitializeDependencies: ✅ DataProvider resolved directly from DataService (%s)"),
                *DataSvcObj->GetName());
        }
    }

    // Вариант B: USuspenseCoreEquipmentDataService::GetDataProvider()
    if (!bResolvedProvider)
    {
        if (USuspenseCoreEquipmentDataService* DataSvc = Cast<USuspenseCoreEquipmentDataService>(DataSvcObj))
        {
            if (ISuspenseEquipmentDataProvider* Provider = DataSvc->GetDataProvider())
            {
                DataProvider.SetObject(DataSvcObj); // держим сервис как владелец жизненного цикла
                DataProvider.SetInterface(Provider);
                bResolvedProvider = true;

                UE_LOG(LogSuspenseCoreEquipmentOperations, Log,
                    TEXT("InitializeDependencies: ✅ DataProvider resolved via DataService::GetDataProvider() (%s)"),
                    *DataSvcObj->GetName());
            }
        }
    }

    // <<< КЛЮЧЕВОЕ: НЕ падать, если провайдера ещё нет >>>
    if (!bResolvedProvider)
    {
        UE_LOG(LogSuspenseCoreEquipmentOperations, Warning,
            TEXT("InitializeDependencies: DataProvider not available at startup (STATELESS). ")
            TEXT("Operations will require a provider via per-call context or later injection."));
        // НЕ возвращаем false — продолжаем init
    }
}

    // 3) TransactionManager — НЕ обязательный на старте (перс-компонент, может отсутствовать глобально)
    {
        const FGameplayTag TxnTag = FGameplayTag::RequestGameplayTag(FName("Service.Equipment.Transaction"));
        UObject* TxnObj = Locator->TryGetService(TxnTag);
        if (TxnObj && TxnObj->GetClass()->ImplementsInterface(USuspenseTransactionManager::StaticClass()))
        {
            TransactionManager.SetObject(TxnObj);
            TransactionManager.SetInterface(Cast<ISuspenseTransactionManager>(TxnObj));
            UE_LOG(LogSuspenseCoreEquipmentOperations, Log,
                TEXT("InitializeDependencies: ✅ TransactionManager resolved (GLOBAL)"));
        }
        else
        {
            UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose,
                TEXT("InitializeDependencies: TransactionManager will be supplied per-player via context (STATELESS)"));
        }
    }

// 4) Rules (опционально): пробуем получить ISuspenseEquipmentRules через сервис валидации
{
    const FGameplayTag ValidationTag = FGameplayTag::RequestGameplayTag(FName("Service.Equipment.Validation"));
    UObject* ValidationObj = Locator->TryGetService(ValidationTag);
    if (!ValidationObj)
    {
        UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose,
            TEXT("InitializeDependencies: Validation service not found (rules binding skipped)"));
    }
    else
    {
        bool bBoundRules = false;

        // Путь A: сам сервис реализует ISuspenseEquipmentRules
        if (ValidationObj->GetClass()->ImplementsInterface(USuspenseCoreEquipmentRules::StaticClass()))
        {
            RulesEngine.SetObject(ValidationObj);
            RulesEngine.SetInterface(Cast<ISuspenseEquipmentRules>(ValidationObj));
            bBoundRules = (RulesEngine.GetObject() && RulesEngine.GetInterface());
            if (bBoundRules)
            {
                UE_LOG(LogSuspenseCoreEquipmentOperations, Log,
                    TEXT("InitializeDependencies: ✅ RulesEngine resolved directly from ValidationService (%s)"),
                    *ValidationObj->GetName());
            }
        }

        // Путь B: ValidationServiceImpl может быть не интерфейсным — оставляем мягкий fallback
        if (!bBoundRules)
        {
            if (USuspenseCoreEquipmentValidationService* VS = Cast<USuspenseCoreEquipmentValidationService>(ValidationObj))
            {
                // Если в будущем появится геттер движка правил — можно дернуть здесь:
                // ISuspenseEquipmentRules* Rules = VS->GetRulesEngine();
                // if (Rules) { RulesEngine.SetObject(VS); RulesEngine.SetInterface(Rules); bBoundRules = true; }
                UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose,
                    TEXT("InitializeDependencies: Validation service present, but ISuspenseEquipmentRules not exposed (skip binding)"));
            }
            else
            {
                UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose,
                    TEXT("InitializeDependencies: Validation service has unexpected class (%s)"),
                    *ValidationObj->GetClass()->GetName());
            }
        }

        if (!bBoundRules)
        {
            UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose,
                TEXT("InitializeDependencies: RulesEngine not bound (stateless validation path only)"));
        }
    }
}


    UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose,
        TEXT("InitializeDependencies: Dependency resolution completed"));
    return true;
}

void USuspenseCoreEquipmentOperationService::SetOperationsExecutor(TScriptInterface<ISuspenseCoreEquipmentOperations> InExecutor)
{
    if (!InExecutor.GetObject() || !InExecutor.GetInterface())
    {
        // Разрешаем очистку, но предупреждаем
        {
            FRWScopeLock W(ExecutorLock, SLT_Write);
            OperationsExecutor = TScriptInterface<ISuspenseCoreEquipmentOperations>();
        }
        UE_LOG(LogSuspenseCoreEquipmentOperations, Warning, TEXT("SetOperationsExecutor: cleared executor (null injected)"));
        return;
    }

    if (!InExecutor.GetObject()->GetClass()->ImplementsInterface(USuspenseCoreEquipmentOperations::StaticClass()))
    {
        UE_LOG(LogSuspenseCoreEquipmentOperations, Error,
            TEXT("SetOperationsExecutor: Provided object doesn't implement ISuspenseEquipmentOperations"));
        return;
    }

    {
        FRWScopeLock W(ExecutorLock, SLT_Write);
        OperationsExecutor = InExecutor;
    }

    UE_LOG(LogSuspenseCoreEquipmentOperations, Log,
        TEXT("SetOperationsExecutor: executor injected (%s)"),
        *InExecutor.GetObject()->GetName());
}


void USuspenseCoreEquipmentOperationService::SetupEventSubscriptions()
{
    auto EventBus = FSuspenseCoreEquipmentEventBus::Get();
    if (!EventBus.IsValid())
    {
        return;
    }

    EventHandles.Add(EventBus->Subscribe(
        EventTags::ValidationChanged,
        FEventHandlerDelegate::CreateUObject(this, &USuspenseCoreEquipmentOperationService::OnValidationRulesChanged)
    ));

    EventHandles.Add(EventBus->Subscribe(
        EventTags::DataChanged,
        FEventHandlerDelegate::CreateUObject(this, &USuspenseCoreEquipmentOperationService::OnDataStateChanged)
    ));

    EventHandles.Add(EventBus->Subscribe(
        EventTags::NetworkResult,
        FEventHandlerDelegate::CreateUObject(this, &USuspenseCoreEquipmentOperationService::OnNetworkOperationResult)
    ));
}

void USuspenseCoreEquipmentOperationService::StartQueueProcessing()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            QueueProcessTimer,
            this,
            &USuspenseCoreEquipmentOperationService::ProcessQueueAsync,
            QueueProcessInterval,
            true
        );
    }
    else
    {
        // Fallback to ticker
        TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateUObject(this, &USuspenseCoreEquipmentOperationService::TickQueueFallback),
            QueueProcessInterval
        );
    }
}

void USuspenseCoreEquipmentOperationService::StopQueueProcessing()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(QueueProcessTimer);
    }

    if (TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
        TickerHandle.Reset();
    }

    // Аккуратная усадка пулов до стартового размера
    TrimPools(InitialPoolSize);
}

bool USuspenseCoreEquipmentOperationService::ShouldDelegateToServer(const FEquipmentOperationRequest& Request) const
{
    // Server always executes locally
    if (bServerAuthority)
    {
        return false;
    }

    // Check if operation needs server authority
    bool bNeedsServer = (Request.OperationType == EEquipmentOperationType::Equip ||
                         Request.OperationType == EEquipmentOperationType::Unequip ||
                         Request.OperationType == EEquipmentOperationType::Move ||
                         Request.OperationType == EEquipmentOperationType::Swap ||
                         Request.OperationType == EEquipmentOperationType::Drop);

    return bNeedsServer && NetworkServiceObject.IsValid();
}

FEquipmentOperationResult USuspenseCoreEquipmentOperationService::DelegateOperationToServer(const FEquipmentOperationRequest& Request)
{
    if (!NetworkServiceObject.IsValid())
    {
        return FEquipmentOperationResult::CreateFailure(
            Request.OperationId,
            NSLOCTEXT("Equipment", "NoNetworkService", "Network service not available"),
            EEquipmentValidationFailure::SystemError
        );
    }

    // Пытаемся получить PlayerController, но не требуем его обязательно
    APlayerController* PC = nullptr;

    // Сначала пробуем через OwnerPlayerState
    if (OwnerPlayerState.IsValid())
    {
        PC = Cast<APlayerController>(OwnerPlayerState->GetOwner());
    }

    // Фоллбек на первый доступный PlayerController
    if (!PC && GetWorld())
    {
        PC = GetWorld()->GetFirstPlayerController();
    }

    // Логируем отсутствие PC, но не падаем
    if (!PC)
    {
        UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose,
            TEXT("PlayerController not found for operation %s; proceeding without it"),
            *Request.OperationId.ToString());
    }

    // Используем NetworkDispatcher через интерфейс IEquipmentNetworkService
    if (IEquipmentNetworkService* NetService = Cast<IEquipmentNetworkService>(NetworkServiceObject.Get()))
    {
        if (ISuspenseNetworkDispatcher* Dispatcher = NetService->GetNetworkDispatcher())
        {
            // Создаём сетевой запрос для диспетчера
            FNetworkOperationRequest NetRequest;
            NetRequest.RequestId = FGuid::NewGuid(); // Транспортный ID
            NetRequest.Operation = Request;          // Бизнес-операция с OperationId внутри
            NetRequest.Priority = ENetworkOperationPriority::Normal;
            NetRequest.Timestamp = FPlatformTime::Seconds();
            NetRequest.bRequiresConfirmation = true;
            NetRequest.RetryCount = 0;

            // Добавляем информацию о владельце в метаданные запроса
            if (OwnerPlayerGuid.IsValid())
            {
                NetRequest.Operation.Parameters.Add(TEXT("OwnerPlayerGuid"), OwnerPlayerGuid.ToString());
            }

            // Добавляем PC в инстигатор если есть
            if (PC && PC->GetPawn())
            {
                NetRequest.Operation.Instigator = PC->GetPawn();
            }

            // Отправляем через диспетчер
            const FGuid NetworkRequestId = Dispatcher->SendOperationToServer(NetRequest);

            if (NetworkRequestId.IsValid())
            {
                // Запускаем предсказание если доступно
                if (PredictionManager.GetInterface())
                {
                    StartPrediction(Request);
                }

                // Создаём pending результат
                FEquipmentOperationResult PendingResult;
                PendingResult.bSuccess = true;
                PendingResult.OperationId = Request.OperationId;

                // Добавляем метаданные о сетевой отправке
                PendingResult.ResultMetadata.Add(TEXT("NetworkRequestId"), NetworkRequestId.ToString());
                PendingResult.ResultMetadata.Add(TEXT("Status"), TEXT("Pending"));
                PendingResult.ResultMetadata.Add(TEXT("HasPlayerController"), PC ? TEXT("Yes") : TEXT("No"));

                // Добавляем предупреждение если PC отсутствует
                if (!PC)
                {
                    PendingResult.Warnings.Add(
                        NSLOCTEXT("Equipment", "NoPlayerControllerWarning",
                                 "Operation sent without PlayerController context")
                    );
                }

                ServiceMetrics.Inc(TEXT("OperationsDelegated"));
                if (!PC)
                {
                    ServiceMetrics.Inc(TEXT("OperationsDelegatedWithoutPC"));
                }

                UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose,
                    TEXT("Operation %s delegated to server with network request %s (PC: %s)"),
                    *Request.OperationId.ToString(),
                    *NetworkRequestId.ToString(),
                    PC ? TEXT("Present") : TEXT("Absent"));

                return PendingResult;
            }
            else
            {
                UE_LOG(LogSuspenseCoreEquipmentOperations, Warning,
                    TEXT("Failed to send operation %s to server - dispatcher returned invalid ID"),
                    *Request.OperationId.ToString());
            }
        }
        else
        {
            UE_LOG(LogSuspenseCoreEquipmentOperations, Warning,
                TEXT("Network service available but dispatcher is null"));
        }
    }
    else
    {
        UE_LOG(LogSuspenseCoreEquipmentOperations, Warning,
            TEXT("Network service object doesn't implement IEquipmentNetworkService interface"));
    }

    return FEquipmentOperationResult::CreateFailure(
        Request.OperationId,
        NSLOCTEXT("Equipment", "NetworkDelegationFailed", "Failed to delegate operation to server"),
        EEquipmentValidationFailure::NetworkError
    );
}

void USuspenseCoreEquipmentOperationService::StartPrediction(const FEquipmentOperationRequest& Request)
{
    if (!PredictionManager.GetInterface())
    {
        return;
    }

    FGuid PredictionId = PredictionManager->CreatePrediction(Request);
    PredictionManager->ApplyPrediction(PredictionId);

    OperationToPredictionMap.Add(Request.OperationId, PredictionId);
    ServiceMetrics.Inc(TEXT("PredictionsStarted"));
}

void USuspenseCoreEquipmentOperationService::ConfirmPrediction(const FGuid& OperationId, const FEquipmentOperationResult& ServerResult)
{
    if (!PredictionManager.GetInterface())
    {
        return;
    }

    FGuid* PredictionId = OperationToPredictionMap.Find(OperationId);
    if (!PredictionId)
    {
        return;
    }

    if (ServerResult.bSuccess)
    {
        PredictionManager->ConfirmPrediction(*PredictionId, ServerResult);
        ServiceMetrics.Inc(TEXT("PredictionsConfirmed"));
    }
    else
    {
        PredictionManager->RollbackPrediction(*PredictionId, ServerResult.ErrorMessage);
        ServiceMetrics.Inc(TEXT("PredictionsRolledBack"));
    }

    OperationToPredictionMap.Remove(OperationId);
}

int32 USuspenseCoreEquipmentOperationService::TryCoalesceOperation(FSuspenseCoreQueuedOperation* NewOp)
{
    if (!NewOp)
    {
        return INDEX_NONE;
    }

    // Смотрим хвост очереди — только свежие операции
    const int32 StartIdx = FMath::Max(0, OperationQueue.Num() - CoalescingLookback);

    for (int32 i = OperationQueue.Num() - 1; i >= StartIdx; --i)
    {
        FSuspenseCoreQueuedOperation* ExistingOp = OperationQueue[i];
        if (!ExistingOp)
        {
            continue;
        }

        // Условие коалесинга: тот же тип, тот же предмет, тот же источник
        if (ExistingOp->Request.OperationType      == NewOp->Request.OperationType &&
            ExistingOp->Request.ItemInstance.ItemID == NewOp->Request.ItemInstance.ItemID &&
            ExistingOp->Request.SourceSlotIndex     == NewOp->Request.SourceSlotIndex)
        {
            // Обновляем целевой слот/приоритет у существующей операции
            ExistingOp->Request.TargetSlotIndex = NewOp->Request.TargetSlotIndex;
            ExistingOp->Priority = FMath::Max(ExistingOp->Priority, NewOp->Priority);

            if (bEnableDetailedLogging)
            {
                UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose,
                    TEXT("Coalesced op %s into existing op at index %d"),
                    *NewOp->Request.OperationId.ToString(), i);
            }

            return i;
        }
    }

    return INDEX_NONE;
}

void USuspenseCoreEquipmentOperationService::OptimizeQueue()
{
    // TODO: Implement more advanced queue optimization
    // - Remove redundant operations
    // - Merge compatible operations
    // - Reorder for optimal execution
}

FEquipmentOperationResult USuspenseCoreEquipmentOperationService::ProcessSingleOperation(
    FSuspenseCoreQueuedOperation* QueuedOp,
    const FGuid& OuterTransactionId)
{
    if (!QueuedOp)
    {
        return FEquipmentOperationResult::CreateFailure(
            FGuid::NewGuid(),
            NSLOCTEXT("Equipment", "InvalidOperation", "Invalid operation"),
            EEquipmentValidationFailure::SystemError
        );
    }

    const double StartTime = FPlatformTime::Seconds();

    // === Idempotency: check cache ===
    FEquipmentOperationResult CachedResult;
    if (ResultCache->Get(QueuedOp->Request.OperationId, CachedResult))
    {
        CacheHitRate = CacheHitRate * 0.9f + 0.1f;
        ServiceMetrics.Inc(TEXT("CacheHits"));
        return CachedResult;
    }
    CacheHitRate = CacheHitRate * 0.9f;
    ServiceMetrics.Inc(TEXT("CacheMisses"));

    // === Get Executor ===
    FRWScopeLock ExecLock(ExecutorLock, SLT_ReadOnly);
    if (!OperationsExecutor.GetInterface())
    {
        FEquipmentOperationResult Fail = FEquipmentOperationResult::CreateFailure(
            QueuedOp->Request.OperationId,
            NSLOCTEXT("EquipmentService", "NoExecutor", "Executor is not available"),
            EEquipmentValidationFailure::SystemError
        );
        ResultCache->Set(QueuedOp->Request.OperationId, Fail, ResultCacheTTL);
        return Fail;
    }

    USuspenseCoreEquipmentOperationExecutor* Executor = Cast<USuspenseCoreEquipmentOperationExecutor>(OperationsExecutor.GetObject());
    if (!Executor)
    {
        FEquipmentOperationResult Fail = FEquipmentOperationResult::CreateFailure(
            QueuedOp->Request.OperationId,
            NSLOCTEXT("EquipmentService", "InvalidExecutor", "Executor doesn't support plans"),
            EEquipmentValidationFailure::SystemError
        );
        ResultCache->Set(QueuedOp->Request.OperationId, Fail, ResultCacheTTL);
        return Fail;
    }

    // === 1) Build Plan (unified path) ===
    FSuspenseCoreTransactionPlan Plan;
    FText PlanError;

    if (bUseTransactionPlans)
    {
        if (!Executor->BuildPlan(QueuedOp->Request, Plan, PlanError))
        {
            FEquipmentOperationResult Fail = FEquipmentOperationResult::CreateFailure(
                QueuedOp->Request.OperationId,
                PlanError,
                EEquipmentValidationFailure::SystemError
            );
            ResultCache->Set(QueuedOp->Request.OperationId, Fail, ResultCacheTTL);
            return Fail;
        }
    }
    else
    {
        // Legacy compatibility: build single-step plan from request
        if (!BuildSingleStepPlanFromRequest(QueuedOp->Request, Plan))
        {
            FEquipmentOperationResult Fail = FEquipmentOperationResult::CreateFailure(
                QueuedOp->Request.OperationId,
                NSLOCTEXT("EquipmentService", "CompatPlanFailed", "Failed to build compatible plan"),
                EEquipmentValidationFailure::SystemError
            );
            ResultCache->Set(QueuedOp->Request.OperationId, Fail, ResultCacheTTL);
            return Fail;
        }
    }

    // === 2) Batch/Sequential Validate (in one place) ===
    FText ValidationError;
    if (!BatchValidatePlan(Plan, ValidationError))
    {
        FEquipmentOperationResult Fail = FEquipmentOperationResult::CreateFailure(
            QueuedOp->Request.OperationId,
            ValidationError,
            EEquipmentValidationFailure::RequirementsNotMet
        );
        ResultCache->Set(QueuedOp->Request.OperationId, Fail, ResultCacheTTL);
        return Fail;
    }

    // === 3) Snapshot for history ===
    FEquipmentStateSnapshot StateBefore;
    if (DataProvider.GetInterface())
    {
        StateBefore = DataProvider->CreateSnapshot();
    }

    OnOperationStarted.Broadcast(QueuedOp->Request);

    // === 4) Execute plan through transaction system ===
    TArray<FEquipmentDelta> Deltas;
    if (!ExecutePlanTransactional(Plan, OuterTransactionId, Deltas))
    {
        FEquipmentOperationResult Fail = FEquipmentOperationResult::CreateFailure(
            QueuedOp->Request.OperationId,
            NSLOCTEXT("EquipmentService", "TransactionFailed", "Transaction failed"),
            EEquipmentValidationFailure::SystemError
        );
        ResultCache->Set(QueuedOp->Request.OperationId, Fail, ResultCacheTTL);
        return Fail;
    }

    // === 5) Success with plan metadata ===
    FEquipmentOperationResult Success = FEquipmentOperationResult::CreateSuccess(QueuedOp->Request.OperationId);
    Success.ResultMetadata.Add(TEXT("PlanId"), Plan.PlanId.ToString());
    Success.ResultMetadata.Add(TEXT("PlanSteps"), FString::FromInt(Plan.Num()));
    Success.ResultMetadata.Add(TEXT("EstimatedMs"), FString::SanitizeFloat(Plan.EstimatedExecutionTimeMs));
    Success.ResultMetadata.Add(TEXT("Idempotent"), Plan.bIdempotent ? TEXT("true") : TEXT("false"));

    Success.AffectedSlots.Reserve(Deltas.Num());
    for (const FEquipmentDelta& Delta : Deltas)
    {
        Success.AffectedSlots.AddUnique(Delta.SlotIndex);
    }

    // === 6) Record history ===
    RecordOperation(QueuedOp->Request, Success, StateBefore);

    // === 7) Timing/metrics ===
    const double ExecutionTime = FPlatformTime::Seconds() - StartTime;
    Success.ExecutionTime = static_cast<float>(ExecutionTime);
    AverageExecutionTime = AverageExecutionTime * 0.9f + static_cast<float>(ExecutionTime) * 0.1f;
    ServiceMetrics.AddDurationMs(TEXT("OperationExecution"), static_cast<float>(ExecutionTime * 1000.0));

    // === 8) Cache/events/logging ===
    ResultCache->Set(QueuedOp->Request.OperationId, Success, ResultCacheTTL);
    OnOperationCompleted.Broadcast(Success);
    PublishOperationEvent(Success);
    LogOperation(QueuedOp->Request, Success);

    return Success;
}

bool USuspenseCoreEquipmentOperationService::PreflightRequests(
    const TArray<FSuspenseCoreQueuedOperation*>& BatchOps,
    TArray<FEquipmentOperationResult>* OutResults)
{
    // Early validation: check if ValidationService is available for batch preflight
    if (!ValidationServiceObject.IsValid())
    {
        UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose,
            TEXT("PreflightRequests: ValidationService not available, skipping batch preflight"));
        return true; // Not an error, just skip preflight
    }

    USuspenseCoreEquipmentValidationService* ValidationService = ValidationServiceObject.Get();
    if (!ValidationService)
    {
        UE_LOG(LogSuspenseCoreEquipmentOperations, Warning,
            TEXT("PreflightRequests: ValidationService became invalid"));
        return true; // Degrade gracefully
    }

    // Build request array from queued operations
    TArray<FEquipmentOperationRequest> Requests;
    Requests.Reserve(BatchOps.Num());

    for (const FSuspenseCoreQueuedOperation* QOp : BatchOps)
    {
        if (QOp)
        {
            Requests.Add(QOp->Request);
        }
    }

    if (Requests.Num() == 0)
    {
        UE_LOG(LogSuspenseCoreEquipmentOperations, Warning,
            TEXT("PreflightRequests: No valid requests in batch"));
        return false;
    }

    // Call batch validation - BatchValidate returns TArray<FSlotValidationResult>
    // NOT a boolean, so we need to check the array contents
    TArray<FSlotValidationResult> ValidationResults = ValidationService->BatchValidate(Requests);

    // Verify we got results for all requests
    if (ValidationResults.Num() != Requests.Num())
    {
        UE_LOG(LogSuspenseCoreEquipmentOperations, Error,
            TEXT("PreflightRequests: Batch validation returned inconsistent results (Expected=%d, Got=%d)"),
            Requests.Num(), ValidationResults.Num());

        // Populate failure results if caller wants them
        if (OutResults)
        {
            OutResults->Reserve(Requests.Num());
            for (int32 i = 0; i < Requests.Num(); ++i)
            {
                FEquipmentOperationResult Result;
                Result.bSuccess = false;
                Result.OperationId = Requests[i].OperationId;
                Result.ErrorMessage = NSLOCTEXT("Operations", "PreflightInconsistent",
                    "Batch validation returned inconsistent number of results");
                Result.FailureType = EEquipmentValidationFailure::SystemError;
                OutResults->Add(Result);
            }
        }

        return false;
    }

    // Check each validation result
    bool bAllValid = true;
    for (int32 i = 0; i < ValidationResults.Num(); ++i)
    {
        const FSlotValidationResult& ValidationResult = ValidationResults[i];

        if (!ValidationResult.bIsValid)
        {
            UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose,
                TEXT("PreflightRequests: Request %d failed validation: %s"),
                i, *ValidationResult.ErrorMessage.ToString());

            bAllValid = false;

            // Populate failure result if requested
            if (OutResults)
            {
                FEquipmentOperationResult Result;
                Result.bSuccess = false;
                Result.OperationId = Requests[i].OperationId;
                Result.ErrorMessage = ValidationResult.ErrorMessage;
                Result.FailureType = ValidationResult.FailureType;

                // Copy warnings if any
                for (const FText& Warning : ValidationResult.Warnings)
                {
                    Result.Warnings.Add(Warning);
                }

                OutResults->Add(Result);
            }
        }
        else if (OutResults)
        {
            // Even if valid, add success result if caller wants detailed results
            FEquipmentOperationResult Result;
            Result.bSuccess = true;
            Result.OperationId = Requests[i].OperationId;

            // Include validation warnings even for successful validations
            for (const FText& Warning : ValidationResult.Warnings)
            {
                Result.Warnings.Add(Warning);
            }

            OutResults->Add(Result);
        }
    }

    if (!bAllValid)
    {
        UE_LOG(LogSuspenseCoreEquipmentOperations, Log,
            TEXT("PreflightRequests: Batch contains %d invalid requests out of %d total"),
            Requests.Num() - (bAllValid ? Requests.Num() : 0), Requests.Num());
    }
    else
    {
        UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose,
            TEXT("PreflightRequests: ✅ All %d requests passed preflight validation"),
            Requests.Num());
    }

    return bAllValid;
}

bool USuspenseCoreEquipmentOperationService::ProcessBatch(
    const TArray<FSuspenseCoreQueuedOperation*>& BatchOps,
    bool bAtomic,
    TArray<FEquipmentOperationResult>* OutResults)
{
    // S8: preflight validation (fail-fast)
    if (!PreflightRequests(BatchOps, OutResults))
    {
        return false;
    }

    if (BatchOps.Num() == 0)
    {
        return true;
    }
    if (bUseTransactionPlans && bAtomic)
    {
        const bool bOk = ProcessBatchUsingPlans(BatchOps, /*bAtomic=*/true, OutResults);

        // Update metrics
        TotalBatchesProcessed.fetch_add(1);
        ServiceMetrics.Inc(TEXT("BatchesCompleted"));
        if (bOk)
        {
            ServiceMetrics.Inc(TEXT("BatchesSucceeded"));
        }
        else
        {
            ServiceMetrics.Inc(TEXT("BatchesFailed"));
        }
        ServiceMetrics.RecordValue(TEXT("BatchSize"), BatchOps.Num());

        return bOk;
    }
    bool bAllSuccess = true;
    TArray<FEquipmentOperationResult> Results;
    Results.Reserve(BatchOps.Num());

    // Begin outer batch transaction if atomic
    FGuid BatchTransactionId;
    if (bAtomic && TransactionManager.GetInterface())
    {
        BatchTransactionId = TransactionManager->BeginTransaction(TEXT("Batch Operation"));

        UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose,
            TEXT("Started batch transaction %s for %d operations"),
            *BatchTransactionId.ToString(),
            BatchOps.Num());
    }

    // Process each operation with shared transaction
    int32 ProcessedCount = 0;
    for (FSuspenseCoreQueuedOperation* Op : BatchOps)
    {
        const FEquipmentOperationResult R = ProcessSingleOperation(Op, BatchTransactionId);
        Results.Add(R);
        ProcessedCount++;

        if (!R.bSuccess)
        {
            bAllSuccess = false;

            UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose,
                TEXT("Batch operation %d/%d failed: %s"),
                ProcessedCount,
                BatchOps.Num(),
                *R.ErrorMessage.ToString());

            if (bAtomic)
            {
                // Stop at first failure in atomic batch
                UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose,
                    TEXT("Stopping atomic batch processing due to failure"));
                break;
            }
        }
    }

    // Complete batch transaction
    if (bAtomic && BatchTransactionId.IsValid() && TransactionManager.GetInterface())
    {
        if (bAllSuccess)
        {
            TArray<FEquipmentDelta> BatchDeltas = TransactionManager->GetTransactionDeltas(BatchTransactionId);

            if (USuspenseCoreEquipmentTransactionProcessor* Processor = Cast<USuspenseCoreEquipmentTransactionProcessor>(TransactionManager.GetObject()))
            {
                if (!Processor->CommitTransaction(BatchTransactionId, BatchDeltas))
                {
                    UE_LOG(LogSuspenseCoreEquipmentOperations, Error, TEXT("Batch commit with deltas failed (%s)"), *BatchTransactionId.ToString());
                    TransactionManager->RollbackTransaction(BatchTransactionId);
                    return false;
                }
            }
            else
            {
                if (!TransactionManager->CommitTransaction(BatchTransactionId))
                {
                    UE_LOG(LogSuspenseCoreEquipmentOperations, Error, TEXT("Batch legacy commit failed (%s)"), *BatchTransactionId.ToString());
                    TransactionManager->RollbackTransaction(BatchTransactionId);
                    return false;
                }
            }

            ServiceMetrics.Inc(TEXT("BatchTransactionsCommitted"));
            UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose, TEXT("Committed batch transaction %s - %d operations succeeded"),
                *BatchTransactionId.ToString(), ProcessedCount);
        }
        else
        {
            TransactionManager->RollbackTransaction(BatchTransactionId);
            UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose, TEXT("Rolled back batch transaction %s"), *BatchTransactionId.ToString());
        }
    }

    // Return results if requested
    if (OutResults)
    {
        *OutResults = MoveTemp(Results);
    }

    // Update metrics
    TotalBatchesProcessed.fetch_add(1);
    ServiceMetrics.Inc(TEXT("BatchesCompleted"));

    if (bAllSuccess)
    {
        ServiceMetrics.Inc(TEXT("BatchesSucceeded"));
    }
    else
    {
        ServiceMetrics.Inc(TEXT("BatchesFailed"));
    }

    ServiceMetrics.RecordValue(TEXT("BatchSize"), BatchOps.Num());

    return bAllSuccess;
}

void USuspenseCoreEquipmentOperationService::ProcessQueueAsync()
{
    if (GetQueueSize() > 0 && !bIsProcessingQueue)
    {
        ProcessOperationQueue();
    }
}

bool USuspenseCoreEquipmentOperationService::TickQueueFallback(float DeltaTime)
{
    ProcessQueueAsync();
    return true; // Continue ticking
}

uint32 USuspenseCoreEquipmentOperationService::GenerateValidationCacheKey(const FEquipmentOperationRequest& Request) const
{
    uint32 CacheKey = GetTypeHash(Request.OperationType);
    CacheKey = HashCombine(CacheKey, GetTypeHash(Request.SourceSlotIndex));
    CacheKey = HashCombine(CacheKey, GetTypeHash(Request.TargetSlotIndex));

    // Используем существующие поля из FEquipmentOperationRequest
    CacheKey = HashCombine(CacheKey, GetTypeHash(Request.ItemInstance.ItemID));
    CacheKey = HashCombine(CacheKey, GetTypeHash(Request.ItemInstance.Quantity));
    CacheKey = HashCombine(CacheKey, GetTypeHash(Request.Priority));
    CacheKey = HashCombine(CacheKey, GetTypeHash(Request.bForceOperation));

    // Контекст владельца для исключения пересечений между игроками
    if (OwnerPlayerGuid.IsValid())
    {
        CacheKey = HashCombine(CacheKey, GetTypeHash(OwnerPlayerGuid));
    }

    // Хэш параметров операции если есть
    if (Request.Parameters.Num() > 0)
    {
        // Создаём стабильный хэш из параметров
        TArray<FString> Keys;
        Request.Parameters.GetKeys(Keys);
        Keys.Sort(); // Сортируем для стабильности хэша

        for (const FString& Key : Keys)
        {
            CacheKey = HashCombine(CacheKey, GetTypeHash(Key));
            const FString* Value = Request.Parameters.Find(Key);
            if (Value)
            {
                CacheKey = HashCombine(CacheKey, GetTypeHash(*Value));
            }
        }
    }

    // Добавляем инстигатора если есть (важно для контекстно-зависимых проверок)
    if (Request.Instigator.IsValid())
    {
        CacheKey = HashCombine(CacheKey, GetTypeHash(Request.Instigator->GetUniqueID()));
    }

    return CacheKey;
}

FSlotValidationResult USuspenseCoreEquipmentOperationService::ValidateOperationCached(const FEquipmentOperationRequest& Request) const
{
    // Skip cache for forced operations
    if (Request.bForceOperation)
    {
        return FSlotValidationResult::Success();
    }

    uint32 CacheKey = GenerateValidationCacheKey(Request);

    FSlotValidationResult CachedResult;
    if (ValidationCache->Get(CacheKey, CachedResult))
    {
        ServiceMetrics.Inc(TEXT("ValidationCacheHits"));
        return CachedResult;
    }

    ServiceMetrics.Inc(TEXT("ValidationCacheMisses"));

    FSlotValidationResult Result;

    FRWScopeLock Lock(ExecutorLock, SLT_ReadOnly);

    if (RulesEngine.GetInterface())
    {
        FSuspenseCoreRuleEvaluationResult RuleResult = RulesEngine->EvaluateRules(Request);

        Result.bIsValid = RuleResult.bPassed;
        Result.ErrorMessage = RuleResult.FailureReason;
        Result.ConfidenceScore = RuleResult.ConfidenceScore;

        if (!RuleResult.bPassed)
        {
            Result.FailureType = EEquipmentValidationFailure::RequirementsNotMet;
        }
    }
    else
    {
        Result = FSlotValidationResult::Success();
    }

    const_cast<USuspenseCoreEquipmentOperationService*>(this)->ValidationCache->Set(
        CacheKey, Result, ValidationCacheTTL);

    return Result;
}

void USuspenseCoreEquipmentOperationService::InvalidateValidationCache()
{
    ValidationCache->Clear();
    ServiceMetrics.Inc(TEXT("ValidationCacheInvalidations"));
}

FGuid USuspenseCoreEquipmentOperationService::BeginOperationTransaction(
    const FEquipmentOperationRequest& Request,
    const FGuid& OuterTransactionId)
{
    // Use outer transaction if provided
    if (OuterTransactionId.IsValid())
    {
        return OuterTransactionId;
    }

    FRWScopeLock Lock(ExecutorLock, SLT_ReadOnly);
    if (!TransactionManager.GetInterface())
    {
        // Return invalid GUID to show no transaction
        return FGuid();
    }

    const FString Description = FString::Printf(TEXT("Operation %s"), *Request.GetDescription());
    return TransactionManager->BeginTransaction(Description);
}

void USuspenseCoreEquipmentOperationService::CompleteTransaction(
    const FGuid& TransactionId,
    bool bSuccess,
    bool bIsOuter)
{
    // Don't complete invalid or outer transactions
    if (!TransactionId.IsValid() || bIsOuter)
    {
        return;
    }

    FRWScopeLock Lock(ExecutorLock, SLT_ReadOnly);
    if (!TransactionManager.GetInterface())
    {
        return;
    }

    if (bSuccess)
    {
        TArray<FEquipmentDelta> TxnDeltas = TransactionManager->GetTransactionDeltas(TransactionId);

        if (USuspenseCoreEquipmentTransactionProcessor* Processor = Cast<USuspenseCoreEquipmentTransactionProcessor>(TransactionManager.GetObject()))
        {
            if (!Processor->CommitTransaction(TransactionId, TxnDeltas))
            {
                UE_LOG(LogSuspenseCoreEquipmentOperations, Error, TEXT("Commit with deltas failed (%s)"), *TransactionId.ToString());
                TransactionManager->RollbackTransaction(TransactionId);
                ServiceMetrics.Inc(TEXT("TransactionsRolledBack"));
                return;
            }
        }
        else
        {
            if (!TransactionManager->CommitTransaction(TransactionId))
            {
                UE_LOG(LogSuspenseCoreEquipmentOperations, Error, TEXT("Legacy commit failed (%s)"), *TransactionId.ToString());
                TransactionManager->RollbackTransaction(TransactionId);
                ServiceMetrics.Inc(TEXT("TransactionsRolledBack"));
                return;
            }
        }

        ServiceMetrics.Inc(TEXT("TransactionsCommitted"));
    }
    else
    {
        TransactionManager->RollbackTransaction(TransactionId);
        ServiceMetrics.Inc(TEXT("TransactionsRolledBack"));
    }
}

void USuspenseCoreEquipmentOperationService::RecordOperation(const FEquipmentOperationRequest& Request,
                                                     const FEquipmentOperationResult& Result,
                                                     const FEquipmentStateSnapshot& StateBefore)
{
    FRWScopeLock Lock(HistoryLock, SLT_Write);

    RedoStack.Empty();

    FSuspenseCoreOperationHistoryEntry Entry;
    Entry.Request = Request;
    Entry.Result = Result;
    Entry.ExecutionTime = FDateTime::Now();
    Entry.StateBefore = StateBefore;

    if (DataProvider.GetInterface())
    {
        Entry.StateAfter = DataProvider->CreateSnapshot();
    }

    Entry.bCanUndo = (Request.OperationType == EEquipmentOperationType::Equip ||
                      Request.OperationType == EEquipmentOperationType::Unequip ||
                      Request.OperationType == EEquipmentOperationType::Swap ||
                      Request.OperationType == EEquipmentOperationType::Move);

    OperationHistory.Add(Entry);

    if (OperationHistory.Num() > MaxHistorySize)
    {
        PruneHistory();
    }

    ServiceMetrics.Inc(TEXT("HistoryEntries"));
}

void USuspenseCoreEquipmentOperationService::PruneHistory()
{
    while (OperationHistory.Num() > MaxHistorySize)
    {
        OperationHistory.RemoveAt(0);
    }
}

void USuspenseCoreEquipmentOperationService::PublishOperationEvent(const FEquipmentOperationResult& Result)
{
    auto EventBus = FSuspenseCoreEquipmentEventBus::Get();
    if (!EventBus.IsValid())
    {
        return;
    }

    FSuspenseCoreEquipmentEventData EventData;
    EventData.EventType = EventTags::OperationCompleted;
    EventData.Source = this;
    EventData.Payload = Result.OperationId.ToString();
    EventData.Timestamp = FPlatformTime::Seconds();

    if (!Result.bSuccess)
    {
        EventData.Metadata.Add(TEXT("Error"), Result.ErrorMessage.ToString());
        EventData.Metadata.Add(TEXT("FailureType"), UEnum::GetValueAsString(Result.FailureType));
    }

    EventData.Metadata.Add(TEXT("ExecutionTime"), FString::Printf(TEXT("%.3f"), Result.ExecutionTime));
    EventData.Metadata.Add(TEXT("AffectedSlots"), FString::Printf(TEXT("%d"), Result.AffectedSlots.Num()));

    EventBus->Broadcast(EventData);
}

void USuspenseCoreEquipmentOperationService::OnValidationRulesChanged(const FSuspenseCoreEquipmentEventData& EventData)
{
    InvalidateValidationCache();

    UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose,
        TEXT("Validation rules changed - cache invalidated"));
}

void USuspenseCoreEquipmentOperationService::OnDataStateChanged(const FSuspenseCoreEquipmentEventData& EventData)
{
    ResultCache->Clear();
    ServiceMetrics.Inc(TEXT("ResultCacheInvalidations"));

    UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose,
        TEXT("Data state changed - result cache cleared"));
}

void USuspenseCoreEquipmentOperationService::OnNetworkOperationResult(const FSuspenseCoreEquipmentEventData& EventData)
{
    // Достаём OperationId: сначала из метаданных, потом из payload
    FGuid OperationId;

    if (EventData.HasMetadata(TEXT("OperationId")))
    {
        FGuid::Parse(EventData.GetMetadata(TEXT("OperationId")), OperationId);
    }
    if (!OperationId.IsValid())
    {
        FGuid::Parse(EventData.Payload, OperationId);
    }
    if (!OperationId.IsValid())
    {
        // Нечего подтверждать
        return;
    }

    // Строим результат из события
    FEquipmentOperationResult ServerResult;
    ServerResult.OperationId = OperationId;
    ServerResult.bSuccess = !EventData.HasMetadata(TEXT("Error"));

    if (!ServerResult.bSuccess)
    {
        ServerResult.ErrorMessage = FText::FromString(
            EventData.HasMetadata(TEXT("Error")) ?
            EventData.GetMetadata(TEXT("Error")) :
            TEXT("Unknown network error")
        );
    }

    // Подтверждаем/откатываем prediction
    ConfirmPrediction(OperationId, ServerResult);

    // Пробрасываем событие как обычное завершение операции для единого пути UI/логики
    OnOperationCompleted.Broadcast(ServerResult);

    // Кешируем успешный результат или инвалидируем кеш при ошибке
    if (ServerResult.bSuccess)
    {
        ResultCache->Set(OperationId, ServerResult, ResultCacheTTL);
    }
    else
    {
        ResultCache->Invalidate(OperationId);
    }

    ServiceMetrics.Inc(TEXT("NetworkResultsProcessed"));
}

void USuspenseCoreEquipmentOperationService::UpdateStatistics(const FEquipmentOperationResult& Result)
{
    FRWScopeLock Lock(StatsLock, SLT_Write);

    TotalOperationsExecuted.fetch_add(1);

    if (Result.bSuccess)
    {
        SuccessfulOperations.fetch_add(1);
    }
    else
    {
        FailedOperations.fetch_add(1);
    }
}

void USuspenseCoreEquipmentOperationService::LogOperation(const FEquipmentOperationRequest& Request,
                                                  const FEquipmentOperationResult& Result) const
{
    if (!bEnableDetailedLogging)
    {
        return;
    }

    if (Result.bSuccess)
    {
        UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose,
            TEXT("Operation completed: %s (Time: %.3fms)"),
            *Request.GetDescription(),
            Result.ExecutionTime * 1000.0f);
    }
    else
    {
        UE_LOG(LogSuspenseCoreEquipmentOperations, Warning,
            TEXT("Operation failed: %s - %s"),
            *Request.GetDescription(),
            *Result.ErrorMessage.ToString());
    }
}

//========================================
// Object Pool Management
//========================================

void USuspenseCoreEquipmentOperationService::InitializeObjectPools()
{
    {
        FScopeLock Lock(&OperationPoolLock);
        for (int32 i = 0; i < InitialPoolSize; i++)
        {
            FSuspenseCoreQueuedOperation* NewOp = new FSuspenseCoreQueuedOperation();
            NewOp->bIsFromPool = true;
            OperationPool.Enqueue(NewOp);
            OperationPoolSize.fetch_add(1);
        }
    }

    {
        FScopeLock Lock(&ResultPoolLock);
        for (int32 i = 0; i < InitialPoolSize; i++)
        {
            FEquipmentOperationResult* NewResult = new FEquipmentOperationResult();
            ResultPool.Enqueue(NewResult);
            ResultPoolSize.fetch_add(1);
        }
    }

    ServiceMetrics.Inc(TEXT("PoolsInitialized"), InitialPoolSize * 2);

    UE_LOG(LogSuspenseCoreEquipmentOperations, Log,
        TEXT("Initialized object pools: %d operations, %d results"),
        InitialPoolSize, InitialPoolSize);
}

void USuspenseCoreEquipmentOperationService::CleanupObjectPools()
{
    {
        FScopeLock Lock(&OperationPoolLock);
        FSuspenseCoreQueuedOperation* Op = nullptr;
        while (OperationPool.Dequeue(Op))
        {
            delete Op;
            OperationPoolSize.fetch_sub(1);
        }
    }

    {
        FScopeLock Lock(&ResultPoolLock);
        FEquipmentOperationResult* Result = nullptr;
        while (ResultPool.Dequeue(Result))
        {
            delete Result;
            ResultPoolSize.fetch_sub(1);
        }
    }

    ServiceMetrics.Inc(TEXT("PoolsCleaned"));

    UE_LOG(LogSuspenseCoreEquipmentOperations, Log,
        TEXT("Cleaned up object pools - Total allocations avoided: Operation=%d, Result=%d"),
        OperationPoolHits.load(), ResultPoolHits.load());
}

FSuspenseCoreQueuedOperation* USuspenseCoreEquipmentOperationService::AcquireOperation()
{
    if (!bEnableObjectPooling)
    {
        return new FSuspenseCoreQueuedOperation();
    }

    FSuspenseCoreQueuedOperation* Operation = nullptr;

    {
        FScopeLock Lock(&OperationPoolLock);

        if (OperationPool.Dequeue(Operation))
        {
            OperationPoolHits.fetch_add(1);
            OperationPoolSize.fetch_sub(1);
            ServiceMetrics.Inc(TEXT("OperationPoolHits"));
        }
        else
        {
            OperationPoolMisses.fetch_add(1);
            ServiceMetrics.Inc(TEXT("OperationPoolMisses"));
        }
    }

    if (!Operation)
    {
        Operation = new FSuspenseCoreQueuedOperation();
        Operation->bIsFromPool = false;

        if (bEnableDetailedLogging)
        {
            UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose,
                TEXT("Operation pool miss - allocated new (Total misses: %d)"),
                OperationPoolMisses.load());
        }
    }
    else
    {
        Operation->Reset();
        Operation->bIsFromPool = true;
    }

    return Operation;
}

void USuspenseCoreEquipmentOperationService::ReleaseOperation(FSuspenseCoreQueuedOperation* Operation)
{
    if (!Operation)
    {
        return;
    }

    if (!bEnableObjectPooling)
    {
        delete Operation;
        return;
    }

    FScopeLock Lock(&OperationPoolLock);

    if (OperationPoolSize.load() >= MaxPoolSize)
    {
        PoolOverflows.fetch_add(1);
        ServiceMetrics.Inc(TEXT("PoolOverflows"));
        delete Operation;

        if (bEnableDetailedLogging)
        {
            UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose,
                TEXT("Operation pool overflow - deleting (Total overflows: %d)"),
                PoolOverflows.load());
        }
    }
    else
    {
        Operation->Reset();
        Operation->bIsFromPool = true;
        OperationPool.Enqueue(Operation);
        OperationPoolSize.fetch_add(1);
    }
}

FEquipmentOperationResult* USuspenseCoreEquipmentOperationService::AcquireResult()
{
    if (!bEnableObjectPooling)
    {
        return new FEquipmentOperationResult();
    }

    FEquipmentOperationResult* Result = nullptr;

    {
        FScopeLock Lock(&ResultPoolLock);

        if (ResultPool.Dequeue(Result))
        {
            ResultPoolHits.fetch_add(1);
            ResultPoolSize.fetch_sub(1);
            ServiceMetrics.Inc(TEXT("ResultPoolHits"));
        }
        else
        {
            ResultPoolMisses.fetch_add(1);
            ServiceMetrics.Inc(TEXT("ResultPoolMisses"));
        }
    }

    if (!Result)
    {
        Result = new FEquipmentOperationResult();

        if (bEnableDetailedLogging)
        {
            UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose,
                TEXT("Result pool miss - allocated new (Total misses: %d)"),
                ResultPoolMisses.load());
        }
    }
    else
    {
        *Result = FEquipmentOperationResult();
    }

    return Result;
}

void USuspenseCoreEquipmentOperationService::ReleaseResult(FEquipmentOperationResult* Result)
{
    if (!Result)
    {
        return;
    }

    if (!bEnableObjectPooling)
    {
        delete Result;
        return;
    }

    FScopeLock Lock(&ResultPoolLock);

    if (ResultPoolSize.load() >= MaxPoolSize)
    {
        PoolOverflows.fetch_add(1);
        ServiceMetrics.Inc(TEXT("ResultPoolOverflows"));
        delete Result;

        if (bEnableDetailedLogging)
        {
            UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose,
                TEXT("Result pool overflow - deleting (Total overflows: %d)"),
                PoolOverflows.load());
        }
    }
    else
    {
        *Result = FEquipmentOperationResult();
        ResultPool.Enqueue(Result);
        ResultPoolSize.fetch_add(1);
    }
}

float USuspenseCoreEquipmentOperationService::GetPoolEfficiency() const
{
    int32 TotalOperationAccesses = OperationPoolHits.load() + OperationPoolMisses.load();
    int32 TotalResultAccesses = ResultPoolHits.load() + ResultPoolMisses.load();
    int32 TotalAccesses = TotalOperationAccesses + TotalResultAccesses;
    int32 TotalHits = OperationPoolHits.load() + ResultPoolHits.load();

    return TotalAccesses > 0 ? (float)TotalHits / TotalAccesses : 0.0f;
}

void USuspenseCoreEquipmentOperationService::EnsureValidConfig()
{
    // Минимумы/максимумы под MMO-нагрузку
    MaxQueueSize = FMath::Clamp(MaxQueueSize, 32, 100000);
    BatchSize = FMath::Clamp(BatchSize, 1, 1024);
    QueueProcessInterval = FMath::Clamp(QueueProcessInterval, 0.01f, 5.0f);
    ValidationCacheTTL = FMath::Clamp(ValidationCacheTTL, 0.1f, 60.0f);
    ResultCacheTTL = FMath::Clamp(ResultCacheTTL, 0.05f, 30.0f);
    CoalescingLookback = FMath::Clamp(CoalescingLookback, 0, 1000);
    MaxHistorySize = FMath::Clamp(MaxHistorySize, 10, 1000);

    UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose,
        TEXT("Config sanitized: MaxQueue=%d, Batch=%d, Interval=%.2f, ValidationTTL=%.1f, ResultTTL=%.1f, TransactionPlans=%s"),
        MaxQueueSize, BatchSize, QueueProcessInterval, ValidationCacheTTL, ResultCacheTTL,
        bUseTransactionPlans ? TEXT("Enabled") : TEXT("Disabled"));
}

void USuspenseCoreEquipmentOperationService::TrimPools(int32 KeepPerPool)
{
    int32 OperationsDropped = 0;
    int32 ResultsDropped = 0;
    int32 OriginalOperationPoolSize = 0;
    int32 OriginalResultPoolSize = 0;

    // Обрезаем operation pool
    {
        FScopeLock Lock(&OperationPoolLock);
        OriginalOperationPoolSize = OperationPoolSize.load();
        int32 ToDrop = FMath::Max(0, OriginalOperationPoolSize - KeepPerPool);
        OperationsDropped = ToDrop;

        FSuspenseCoreQueuedOperation* Op = nullptr;
        while (ToDrop > 0 && OperationPool.Dequeue(Op))
        {
            delete Op;
            OperationPoolSize.fetch_sub(1);
            --ToDrop;
        }
    }

    // Обрезаем result pool
    {
        FScopeLock Lock(&ResultPoolLock);
        OriginalResultPoolSize = ResultPoolSize.load();
        int32 ToDrop = FMath::Max(0, OriginalResultPoolSize - KeepPerPool);
        ResultsDropped = ToDrop;

        FEquipmentOperationResult* Res = nullptr;
        while (ToDrop > 0 && ResultPool.Dequeue(Res))
        {
            delete Res;
            ResultPoolSize.fetch_sub(1);
            --ToDrop;
        }
    }

    ServiceMetrics.Inc(TEXT("PoolsTrimmed"));

    if (bEnableDetailedLogging)
    {
        UE_LOG(LogSuspenseCoreEquipmentOperations, Verbose,
            TEXT("Pools trimmed to %d items each (was: Operations=%d, Results=%d, dropped: %d/%d)"),
            KeepPerPool,
            OriginalOperationPoolSize,
            OriginalResultPoolSize,
            OperationsDropped,
            ResultsDropped);
    }
}

FString USuspenseCoreEquipmentOperationService::GetPoolStatistics() const
{
    FString Stats;

    float PoolEfficiency = GetPoolEfficiency() * 100.0f;

    Stats += FString::Printf(TEXT("Overall Pool Efficiency: %.1f%%\n"), PoolEfficiency);

    Stats += TEXT("\n-- Operation Pool --\n");
    int32 OpAccesses = OperationPoolHits.load() + OperationPoolMisses.load();
    float OpEfficiency = OpAccesses > 0 ?
        (float)OperationPoolHits.load() / OpAccesses * 100.0f : 0.0f;
    Stats += FString::Printf(TEXT("Efficiency: %.1f%%\n"), OpEfficiency);
    Stats += FString::Printf(TEXT("Hits: %d, Misses: %d\n"),
        OperationPoolHits.load(), OperationPoolMisses.load());
    Stats += FString::Printf(TEXT("Current Size: %d/%d\n"),
        OperationPoolSize.load(), MaxPoolSize);

    Stats += TEXT("\n-- Result Pool --\n");
    int32 ResAccesses = ResultPoolHits.load() + ResultPoolMisses.load();
    float ResEfficiency = ResAccesses > 0 ?
        (float)ResultPoolHits.load() / ResAccesses * 100.0f : 0.0f;
    Stats += FString::Printf(TEXT("Efficiency: %.1f%%\n"), ResEfficiency);
    Stats += FString::Printf(TEXT("Hits: %d, Misses: %d\n"),
        ResultPoolHits.load(), ResultPoolMisses.load());
    Stats += FString::Printf(TEXT("Current Size: %d/%d\n"),
        ResultPoolSize.load(), MaxPoolSize);

    Stats += TEXT("\n-- Common --\n");
    Stats += FString::Printf(TEXT("Total Overflows: %d\n"), PoolOverflows.load());

    int32 AllocationsSaved = OperationPoolHits.load() + ResultPoolHits.load();
    int32 OperationBytes = OperationPoolHits.load() * sizeof(FSuspenseCoreQueuedOperation);
    int32 ResultBytes = ResultPoolHits.load() * sizeof(FEquipmentOperationResult);
    int32 TotalBytesSaved = OperationBytes + ResultBytes;

    Stats += FString::Printf(TEXT("Allocations Avoided: %d\n"), AllocationsSaved);
    Stats += FString::Printf(TEXT("Estimated Memory Saved: %.2f KB\n"),
        TotalBytesSaved / 1024.0f);

    return Stats;
}
