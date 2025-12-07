// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Services/SuspenseCoreEquipmentTransactionService.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/ConfigCacheIni.h"

DEFINE_LOG_CATEGORY(LogSuspenseCoreEquipmentTransaction);

//========================================
// FTransactionServiceConfig
//========================================

FTransactionServiceConfig FTransactionServiceConfig::LoadFromConfig(const FString& ConfigSection)
{
    FTransactionServiceConfig OutConfig;

    if (!GConfig) return OutConfig;

    GConfig->GetFloat(*ConfigSection, TEXT("TransactionTimeout"), OutConfig.TransactionTimeout, GGameIni);
    GConfig->GetInt(*ConfigSection, TEXT("MaxNestedDepth"), OutConfig.MaxNestedDepth, GGameIni);
    GConfig->GetInt(*ConfigSection, TEXT("MaxHistorySize"), OutConfig.MaxHistorySize, GGameIni);
    GConfig->GetBool(*ConfigSection, TEXT("bAutoRecovery"), OutConfig.bAutoRecovery, GGameIni);
    GConfig->GetBool(*ConfigSection, TEXT("bEnableLogging"), OutConfig.bEnableLogging, GGameIni);
    GConfig->GetBool(*ConfigSection, TEXT("bGenerateDeltas"), OutConfig.bGenerateDeltas, GGameIni);
    GConfig->GetBool(*ConfigSection, TEXT("bBroadcastTransactionEvents"), OutConfig.bBroadcastTransactionEvents, GGameIni);
    GConfig->GetFloat(*ConfigSection, TEXT("CleanupInterval"), OutConfig.CleanupInterval, GGameIni);

    return OutConfig;
}

//========================================
// FTransactionServiceMetrics
//========================================

FString FTransactionServiceMetrics::ToString() const
{
    return FString::Printf(
        TEXT("=== Transaction Service Metrics ===\n")
        TEXT("Started: %llu\n")
        TEXT("Committed: %llu (%.1f%%)\n")
        TEXT("Rolled Back: %llu\n")
        TEXT("Failed: %llu\n")
        TEXT("Operations: %llu\n")
        TEXT("Conflicts Resolved: %llu\n")
        TEXT("Deltas Generated: %llu\n")
        TEXT("Active: %llu\n")
        TEXT("Avg Time: %llu us\n")
        TEXT("Peak Time: %llu us"),
        TotalTransactionsStarted.Load(),
        TotalTransactionsCommitted.Load(),
        GetCommitRate(),
        TotalTransactionsRolledBack.Load(),
        TotalTransactionsFailed.Load(),
        TotalOperationsProcessed.Load(),
        TotalConflictsResolved.Load(),
        TotalDeltasGenerated.Load(),
        ActiveTransactionCount.Load(),
        AverageTransactionTimeUs.Load(),
        PeakTransactionTimeUs.Load()
    );
}

void FTransactionServiceMetrics::Reset()
{
    TotalTransactionsStarted.Store(0);
    TotalTransactionsCommitted.Store(0);
    TotalTransactionsRolledBack.Store(0);
    TotalTransactionsFailed.Store(0);
    TotalOperationsProcessed.Store(0);
    TotalConflictsResolved.Store(0);
    TotalDeltasGenerated.Store(0);
    ActiveTransactionCount.Store(0);
    AverageTransactionTimeUs.Store(0);
    PeakTransactionTimeUs.Store(0);
}

//========================================
// USuspenseCoreEquipmentTransactionService
//========================================

USuspenseCoreEquipmentTransactionService::USuspenseCoreEquipmentTransactionService()
{
    Config = FTransactionServiceConfig::LoadFromConfig();
}

USuspenseCoreEquipmentTransactionService::~USuspenseCoreEquipmentTransactionService()
{
    ShutdownService(true);
}

//========================================
// IEquipmentService Implementation
//========================================

bool USuspenseCoreEquipmentTransactionService::InitializeService(const FServiceInitParams& Params)
{
    SCOPED_SERVICE_TIMER("TransactionService::Initialize");

    if (ServiceState != EServiceLifecycleState::Uninitialized)
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, TEXT("Service already initialized"));
        return ServiceState == EServiceLifecycleState::Ready;
    }

    ServiceState = EServiceLifecycleState::Initializing;
    ServiceParams = Params;

    UE_LOG(LogSuspenseCoreEquipmentTransaction, Log, TEXT(">>> TransactionService: Initializing..."));

    // Load configuration
    Config = FTransactionServiceConfig::LoadFromConfig();

    // Setup EventBus integration
    SetupEventBus();

    ServiceState = EServiceLifecycleState::Ready;
    UE_LOG(LogSuspenseCoreEquipmentTransaction, Log,
        TEXT("<<< TransactionService: Initialized (Timeout=%.1fs, MaxDepth=%d, Events=%s)"),
        Config.TransactionTimeout,
        Config.MaxNestedDepth,
        Config.bBroadcastTransactionEvents ? TEXT("ON") : TEXT("OFF"));

    return true;
}

bool USuspenseCoreEquipmentTransactionService::ShutdownService(bool bForce)
{
    if (ServiceState == EServiceLifecycleState::Shutdown)
    {
        return true;
    }

    UE_LOG(LogSuspenseCoreEquipmentTransaction, Log, TEXT(">>> TransactionService: Shutting down..."));

    // Rollback all pending transactions
    if (!bForce)
    {
        RollbackAllTransactions();
    }

    // Teardown EventBus
    TeardownEventBus();

    // Clear data
    {
        FScopeLock Lock(&TransactionLock);
        PendingTransactions.Empty();
        TransactionStack.Empty();
        TransactionHistory.Empty();
        TransactionStartTimes.Empty();
    }

    ServiceState = EServiceLifecycleState::Shutdown;
    UE_LOG(LogSuspenseCoreEquipmentTransaction, Log, TEXT("<<< TransactionService: Shutdown complete"));

    return true;
}

FGameplayTag USuspenseCoreEquipmentTransactionService::GetServiceTag() const
{
    using namespace SuspenseCoreEquipmentTags;
    return Service::TAG_Service_Equipment_Operations; // Transaction is part of operations
}

FGameplayTagContainer USuspenseCoreEquipmentTransactionService::GetRequiredDependencies() const
{
    using namespace SuspenseCoreEquipmentTags;
    FGameplayTagContainer Dependencies;
    Dependencies.AddTag(Service::TAG_Service_Equipment_Data); // Needs DataService
    return Dependencies;
}

bool USuspenseCoreEquipmentTransactionService::ValidateService(TArray<FText>& OutErrors) const
{
    bool bValid = true;

    if (!DataProvider.GetInterface())
    {
        OutErrors.Add(FText::FromString(TEXT("DataProvider not injected")));
        bValid = false;
    }

    return bValid;
}

void USuspenseCoreEquipmentTransactionService::ResetService()
{
    FScopeLock Lock(&TransactionLock);

    // Rollback all pending
    for (auto& Pair : PendingTransactions)
    {
        Pair.Value.State = ETransactionState::RolledBack;
    }
    PendingTransactions.Empty();
    TransactionStack.Empty();
    TransactionStartTimes.Empty();

    // Keep history but reset metrics
    Metrics.Reset();

    UE_LOG(LogSuspenseCoreEquipmentTransaction, Log, TEXT("TransactionService: Reset complete"));
}

FString USuspenseCoreEquipmentTransactionService::GetServiceStats() const
{
    FScopeLock Lock(&TransactionLock);

    return FString::Printf(
        TEXT("TransactionService Stats:\n")
        TEXT("  Pending Transactions: %d\n")
        TEXT("  Stack Depth: %d\n")
        TEXT("  History Size: %d\n")
        TEXT("%s"),
        PendingTransactions.Num(),
        TransactionStack.Num(),
        TransactionHistory.Num(),
        *Metrics.ToString()
    );
}

//========================================
// ISuspenseCoreTransactionManager Implementation
//========================================

FGuid USuspenseCoreEquipmentTransactionService::BeginTransaction(const FString& Description)
{
    if (!IsServiceReady())
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, TEXT("BeginTransaction failed - service not ready"));
        return FGuid();
    }

    FScopeLock Lock(&TransactionLock);

    // Check nesting limit
    if (TransactionStack.Num() >= Config.MaxNestedDepth)
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning,
            TEXT("BeginTransaction failed - max nesting depth (%d) exceeded"),
            Config.MaxNestedDepth);
        return FGuid();
    }

    // Create new transaction
    FGuid TransactionId = FGuid::NewGuid();
    FEquipmentTransaction& NewTransaction = PendingTransactions.Add(TransactionId);
    NewTransaction.TransactionId = TransactionId;
    NewTransaction.Description = Description;
    NewTransaction.State = ETransactionState::Active;
    NewTransaction.StartTime = FDateTime::Now();

    // Set parent if nested
    if (TransactionStack.Num() > 0)
    {
        NewTransaction.ParentTransactionId = TransactionStack.Last();
    }

    // Push to stack
    TransactionStack.Add(TransactionId);

    // Record start time for metrics
    TransactionStartTimes.Add(TransactionId, FPlatformTime::Seconds());

    // Update metrics
    Metrics.TotalTransactionsStarted++;
    Metrics.ActiveTransactionCount++;

    // Broadcast event
    if (Config.bBroadcastTransactionEvents)
    {
        BroadcastTransactionStarted(TransactionId, Description);
    }

    if (Config.bEnableLogging)
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Log,
            TEXT("Transaction started: %s (%s)"),
            *TransactionId.ToString(), *Description);
    }

    return TransactionId;
}

bool USuspenseCoreEquipmentTransactionService::CommitTransaction(const FGuid& TransactionId)
{
    if (!IsServiceReady())
    {
        return false;
    }

    FScopeLock Lock(&TransactionLock);

    FEquipmentTransaction* Transaction = FindTransaction(TransactionId);
    if (!Transaction)
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning,
            TEXT("CommitTransaction failed - transaction %s not found"),
            *TransactionId.ToString());
        return false;
    }

    if (Transaction->State != ETransactionState::Active)
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning,
            TEXT("CommitTransaction failed - transaction %s not active (state=%d)"),
            *TransactionId.ToString(), static_cast<int32>(Transaction->State));
        return false;
    }

    // Must commit in stack order (LIFO)
    if (TransactionStack.Num() > 0 && TransactionStack.Last() != TransactionId)
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning,
            TEXT("CommitTransaction failed - must commit in LIFO order"));
        return false;
    }

    // Mark as committed
    Transaction->State = ETransactionState::Committed;
    Transaction->EndTime = FDateTime::Now();

    // Remove from stack
    TransactionStack.Remove(TransactionId);

    // Move to history
    TransactionHistory.Add(*Transaction);
    if (TransactionHistory.Num() > Config.MaxHistorySize)
    {
        TransactionHistory.RemoveAt(0);
    }

    // Remove from pending
    PendingTransactions.Remove(TransactionId);

    // Update metrics
    UpdateMetrics(TransactionId, true);
    Metrics.TotalTransactionsCommitted++;
    Metrics.ActiveTransactionCount--;

    // Broadcast event
    if (Config.bBroadcastTransactionEvents)
    {
        BroadcastTransactionCommitted(TransactionId);
    }

    if (Config.bEnableLogging)
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Log,
            TEXT("Transaction committed: %s"),
            *TransactionId.ToString());
    }

    return true;
}

bool USuspenseCoreEquipmentTransactionService::RollbackTransaction(const FGuid& TransactionId)
{
    if (!IsServiceReady())
    {
        return false;
    }

    FScopeLock Lock(&TransactionLock);

    FEquipmentTransaction* Transaction = FindTransaction(TransactionId);
    if (!Transaction)
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning,
            TEXT("RollbackTransaction failed - transaction %s not found"),
            *TransactionId.ToString());
        return false;
    }

    if (Transaction->State != ETransactionState::Active)
    {
        return false;
    }

    // Mark as rolled back
    Transaction->State = ETransactionState::RolledBack;
    Transaction->EndTime = FDateTime::Now();

    // Remove from stack
    TransactionStack.Remove(TransactionId);

    // Move to history
    TransactionHistory.Add(*Transaction);
    if (TransactionHistory.Num() > Config.MaxHistorySize)
    {
        TransactionHistory.RemoveAt(0);
    }

    // Remove from pending
    PendingTransactions.Remove(TransactionId);

    // Update metrics
    UpdateMetrics(TransactionId, false);
    Metrics.TotalTransactionsRolledBack++;
    Metrics.ActiveTransactionCount--;

    // Broadcast event
    if (Config.bBroadcastTransactionEvents)
    {
        BroadcastTransactionRolledBack(TransactionId);
    }

    if (Config.bEnableLogging)
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Log,
            TEXT("Transaction rolled back: %s"),
            *TransactionId.ToString());
    }

    return true;
}

FGuid USuspenseCoreEquipmentTransactionService::CreateSavepoint(const FString& SavepointName)
{
    FScopeLock Lock(&TransactionLock);

    if (TransactionStack.Num() == 0)
    {
        return FGuid();
    }

    FGuid CurrentTransactionId = TransactionStack.Last();
    FEquipmentTransaction* Transaction = FindTransaction(CurrentTransactionId);
    if (!Transaction)
    {
        return FGuid();
    }

    FGuid SavepointId = FGuid::NewGuid();
    // Note: Savepoints would be stored in transaction metadata
    // For now, return the ID as confirmation
    return SavepointId;
}

bool USuspenseCoreEquipmentTransactionService::RollbackToSavepoint(const FGuid& SavepointId)
{
    // Savepoint rollback implementation
    // Would restore state to savepoint snapshot
    return false;
}

FEquipmentTransaction USuspenseCoreEquipmentTransactionService::GetCurrentTransaction() const
{
    FScopeLock Lock(&TransactionLock);

    if (TransactionStack.Num() == 0)
    {
        return FEquipmentTransaction();
    }

    const FEquipmentTransaction* Transaction = FindTransaction(TransactionStack.Last());
    return Transaction ? *Transaction : FEquipmentTransaction();
}

bool USuspenseCoreEquipmentTransactionService::IsTransactionActive() const
{
    FScopeLock Lock(&TransactionLock);
    return TransactionStack.Num() > 0;
}

FEquipmentTransaction USuspenseCoreEquipmentTransactionService::GetTransaction(const FGuid& TransactionId) const
{
    FScopeLock Lock(&TransactionLock);

    const FEquipmentTransaction* Transaction = FindTransaction(TransactionId);
    return Transaction ? *Transaction : FEquipmentTransaction();
}

FGuid USuspenseCoreEquipmentTransactionService::BeginNestedTransaction(const FString& Description)
{
    return BeginTransaction(Description);
}

bool USuspenseCoreEquipmentTransactionService::RegisterOperation(const FGuid& OperationId)
{
    FScopeLock Lock(&TransactionLock);

    if (TransactionStack.Num() == 0)
    {
        return false;
    }

    FEquipmentTransaction* Transaction = FindTransaction(TransactionStack.Last());
    if (!Transaction)
    {
        return false;
    }

    Transaction->OperationIds.Add(OperationId);
    Metrics.TotalOperationsProcessed++;
    return true;
}

bool USuspenseCoreEquipmentTransactionService::ValidateTransaction(const FGuid& TransactionId) const
{
    FScopeLock Lock(&TransactionLock);

    const FEquipmentTransaction* Transaction = FindTransaction(TransactionId);
    return Transaction && Transaction->State == ETransactionState::Active;
}

TArray<FEquipmentTransaction> USuspenseCoreEquipmentTransactionService::GetTransactionHistory(int32 MaxCount) const
{
    FScopeLock Lock(&TransactionLock);

    if (MaxCount <= 0 || MaxCount >= TransactionHistory.Num())
    {
        return TransactionHistory;
    }

    TArray<FEquipmentTransaction> Result;
    const int32 StartIndex = TransactionHistory.Num() - MaxCount;
    for (int32 i = StartIndex; i < TransactionHistory.Num(); ++i)
    {
        Result.Add(TransactionHistory[i]);
    }
    return Result;
}

bool USuspenseCoreEquipmentTransactionService::RegisterOperation(const FGuid& TransactionId, const FTransactionOperation& Operation)
{
    FScopeLock Lock(&TransactionLock);

    FEquipmentTransaction* Transaction = FindTransaction(TransactionId);
    if (!Transaction || Transaction->State != ETransactionState::Active)
    {
        return false;
    }

    // Store operation ID
    Transaction->OperationIds.Add(Operation.OperationId);
    Metrics.TotalOperationsProcessed++;
    return true;
}

bool USuspenseCoreEquipmentTransactionService::ApplyOperation(const FGuid& TransactionId, const FTransactionOperation& Operation)
{
    return RegisterOperation(TransactionId, Operation);
}

TArray<FEquipmentDelta> USuspenseCoreEquipmentTransactionService::GetTransactionDeltas(const FGuid& TransactionId) const
{
    // Delta generation would be implemented here
    return TArray<FEquipmentDelta>();
}

//========================================
// Extended API
//========================================

int32 USuspenseCoreEquipmentTransactionService::CommitAllTransactions()
{
    TArray<FGuid> ToCommit;

    {
        FScopeLock Lock(&TransactionLock);
        // Copy stack in reverse order (LIFO)
        for (int32 i = TransactionStack.Num() - 1; i >= 0; --i)
        {
            ToCommit.Add(TransactionStack[i]);
        }
    }

    int32 CommittedCount = 0;
    for (const FGuid& TransactionId : ToCommit)
    {
        if (CommitTransaction(TransactionId))
        {
            CommittedCount++;
        }
    }

    return CommittedCount;
}

int32 USuspenseCoreEquipmentTransactionService::RollbackAllTransactions()
{
    TArray<FGuid> ToRollback;

    {
        FScopeLock Lock(&TransactionLock);
        // Copy stack in reverse order (LIFO)
        for (int32 i = TransactionStack.Num() - 1; i >= 0; --i)
        {
            ToRollback.Add(TransactionStack[i]);
        }
    }

    int32 RolledBackCount = 0;
    for (const FGuid& TransactionId : ToRollback)
    {
        if (RollbackTransaction(TransactionId))
        {
            RolledBackCount++;
        }
    }

    return RolledBackCount;
}

FGuid USuspenseCoreEquipmentTransactionService::GetCurrentTransactionId() const
{
    FScopeLock Lock(&TransactionLock);
    return TransactionStack.Num() > 0 ? TransactionStack.Last() : FGuid();
}

int32 USuspenseCoreEquipmentTransactionService::GetActiveTransactionCount() const
{
    FScopeLock Lock(&TransactionLock);
    return PendingTransactions.Num();
}

void USuspenseCoreEquipmentTransactionService::ClearTransactionHistory(bool bKeepActive)
{
    FScopeLock Lock(&TransactionLock);

    if (bKeepActive)
    {
        TransactionHistory.Empty();
    }
    else
    {
        TransactionHistory.Empty();
        PendingTransactions.Empty();
        TransactionStack.Empty();
        TransactionStartTimes.Empty();
    }
}

bool USuspenseCoreEquipmentTransactionService::InjectDataProvider(TScriptInterface<ISuspenseCoreEquipmentDataProvider> InDataProvider)
{
    DataProvider = InDataProvider;
    return DataProvider.GetInterface() != nullptr;
}

//========================================
// Private Helpers
//========================================

FEquipmentTransaction* USuspenseCoreEquipmentTransactionService::FindTransaction(const FGuid& TransactionId)
{
    return PendingTransactions.Find(TransactionId);
}

const FEquipmentTransaction* USuspenseCoreEquipmentTransactionService::FindTransaction(const FGuid& TransactionId) const
{
    return PendingTransactions.Find(TransactionId);
}

void USuspenseCoreEquipmentTransactionService::SetupEventBus()
{
    EventBus = FSuspenseCoreEquipmentEventBus::Get();

    using namespace SuspenseCoreEquipmentTags;
    Tag_Transaction_Started = Event::TAG_Equipment_Event_Operation_Started;
    Tag_Transaction_Committed = Event::TAG_Equipment_Event_Operation_Completed;
    Tag_Transaction_RolledBack = Event::TAG_Equipment_Event_Operation_Cancelled;
    Tag_Transaction_Failed = Event::TAG_Equipment_Event_Validation_Failed;
}

void USuspenseCoreEquipmentTransactionService::TeardownEventBus()
{
    auto Bus = EventBus.Pin();
    if (Bus.IsValid())
    {
        for (const FEventSubscriptionHandle& Handle : EventSubscriptions)
        {
            Bus->Unsubscribe(Handle);
        }
    }
    EventSubscriptions.Empty();
}

void USuspenseCoreEquipmentTransactionService::BroadcastTransactionStarted(const FGuid& TransactionId, const FString& Description) const
{
    auto Bus = EventBus.Pin();
    if (!Bus.IsValid() || !Tag_Transaction_Started.IsValid()) return;

    FSuspenseCoreEquipmentEventData EventData;
    EventData.EventType = Tag_Transaction_Started;
    EventData.AddMetadata(TEXT("TransactionId"), TransactionId.ToString());
    EventData.AddMetadata(TEXT("Description"), Description);

    Bus->Broadcast(EventData);
}

void USuspenseCoreEquipmentTransactionService::BroadcastTransactionCommitted(const FGuid& TransactionId) const
{
    auto Bus = EventBus.Pin();
    if (!Bus.IsValid() || !Tag_Transaction_Committed.IsValid()) return;

    FSuspenseCoreEquipmentEventData EventData;
    EventData.EventType = Tag_Transaction_Committed;
    EventData.AddMetadata(TEXT("TransactionId"), TransactionId.ToString());
    EventData.AddMetadata(TEXT("Result"), TEXT("Committed"));

    Bus->Broadcast(EventData);
}

void USuspenseCoreEquipmentTransactionService::BroadcastTransactionRolledBack(const FGuid& TransactionId) const
{
    auto Bus = EventBus.Pin();
    if (!Bus.IsValid() || !Tag_Transaction_RolledBack.IsValid()) return;

    FSuspenseCoreEquipmentEventData EventData;
    EventData.EventType = Tag_Transaction_RolledBack;
    EventData.AddMetadata(TEXT("TransactionId"), TransactionId.ToString());
    EventData.AddMetadata(TEXT("Result"), TEXT("RolledBack"));

    Bus->Broadcast(EventData);
}

void USuspenseCoreEquipmentTransactionService::BroadcastTransactionFailed(const FGuid& TransactionId, const FString& Reason) const
{
    auto Bus = EventBus.Pin();
    if (!Bus.IsValid() || !Tag_Transaction_Failed.IsValid()) return;

    FSuspenseCoreEquipmentEventData EventData;
    EventData.EventType = Tag_Transaction_Failed;
    EventData.AddMetadata(TEXT("TransactionId"), TransactionId.ToString());
    EventData.AddMetadata(TEXT("Reason"), Reason);

    Bus->Broadcast(EventData);
}

void USuspenseCoreEquipmentTransactionService::UpdateMetrics(const FGuid& TransactionId, bool bCommitted) const
{
    const double* StartTime = TransactionStartTimes.Find(TransactionId);
    if (!StartTime)
    {
        return;
    }

    const double EndTime = FPlatformTime::Seconds();
    const uint64 TransactionTimeUs = static_cast<uint64>((EndTime - *StartTime) * 1000000.0);

    // Update average (simple moving average)
    const uint64 CurrentAvg = Metrics.AverageTransactionTimeUs.Load();
    const uint64 NewAvg = (CurrentAvg * 9 + TransactionTimeUs) / 10;
    Metrics.AverageTransactionTimeUs.Store(NewAvg);

    // Update peak
    uint64 CurrentPeak = Metrics.PeakTransactionTimeUs.Load();
    while (TransactionTimeUs > CurrentPeak)
    {
        if (Metrics.PeakTransactionTimeUs.CompareExchange(CurrentPeak, TransactionTimeUs))
        {
            break;
        }
        CurrentPeak = Metrics.PeakTransactionTimeUs.Load();
    }

    // Remove from timing map
    const_cast<USuspenseCoreEquipmentTransactionService*>(this)->TransactionStartTimes.Remove(TransactionId);
}

void USuspenseCoreEquipmentTransactionService::CleanupExpiredTransactions()
{
    FScopeLock Lock(&TransactionLock);

    const FDateTime Now = FDateTime::Now();
    TArray<FGuid> ToRemove;

    for (auto& Pair : PendingTransactions)
    {
        const FTimespan Age = Now - Pair.Value.StartTime;
        if (Age.GetTotalSeconds() > Config.TransactionTimeout)
        {
            ToRemove.Add(Pair.Key);
        }
    }

    for (const FGuid& TransactionId : ToRemove)
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning,
            TEXT("Transaction %s timed out - rolling back"),
            *TransactionId.ToString());

        // Mark as failed and remove
        FEquipmentTransaction* Transaction = FindTransaction(TransactionId);
        if (Transaction)
        {
            Transaction->State = ETransactionState::Failed;
            TransactionHistory.Add(*Transaction);
            Metrics.TotalTransactionsFailed++;
        }

        TransactionStack.Remove(TransactionId);
        PendingTransactions.Remove(TransactionId);
        TransactionStartTimes.Remove(TransactionId);
        Metrics.ActiveTransactionCount--;
    }
}
