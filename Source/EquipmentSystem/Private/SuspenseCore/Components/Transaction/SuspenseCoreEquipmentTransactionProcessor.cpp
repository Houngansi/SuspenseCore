// SuspenseCoreEquipmentTransactionProcessor.cpp
// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Components/Transaction/SuspenseCoreEquipmentTransactionProcessor.h"
#include "SuspenseCore/Components/Core/SuspenseCoreEquipmentDataStore.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceMacros.h"
#include "SuspenseCore/Types/Items/SuspenseCoreItemTypes.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"

//========================================
// Helper: Convert FSuspenseCoreInventoryItemInstance to FSuspenseCoreItemInstance
//========================================

static FSuspenseCoreItemInstance ConvertToItemInstance(const FSuspenseCoreInventoryItemInstance& Source)
{
    FSuspenseCoreItemInstance Result;
    Result.ItemID = Source.ItemID;
    Result.UniqueInstanceID = Source.InstanceID;
    Result.Quantity = Source.Quantity;

    // Note: We don't copy all runtime properties as FSuspenseCoreItemInstance uses a different format
    // This is a minimal conversion for operation request purposes
    return Result;
}

//========================================
// Constructor/Destructor
//========================================

USuspenseCoreEquipmentTransactionProcessor::USuspenseCoreEquipmentTransactionProcessor()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f;
    
    // Initialize configuration with defaults
    TransactionTimeout = 30.0f;
    MaxNestedDepth = 5;
    MaxHistorySize = 100;
    bAutoRecovery = true;
    bEnableLogging = true;
    CleanupInterval = 60.0f;
    bGenerateDeltas = true;
    
    // Initialize statistics counters
    TotalTransactionsStarted = 0;
    TotalTransactionsCommitted = 0;
    TotalTransactionsRolledBack = 0;
    TotalTransactionsFailed = 0;
    TotalOperationsProcessed = 0;
    TotalConflictsResolved = 0;
    TotalDeltasGenerated = 0;
    
    // Initialize state flags
    bIsInitialized = false;
    bIsProcessing = false;
    ProcessorVersion = 1;
    LastCleanupTime = 0.0f;
}

USuspenseCoreEquipmentTransactionProcessor::~USuspenseCoreEquipmentTransactionProcessor()
{
    // Ensure cleanup of active transactions
    if (ActiveTransactions.Num() > 0)
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
            TEXT("Destructor: %d active transactions will be forcibly rolled back"),
            ActiveTransactions.Num());
    }
}

//========================================
// UActorComponent Interface
//========================================

void USuspenseCoreEquipmentTransactionProcessor::BeginPlay()
{
    Super::BeginPlay();
    
    // Enable tick for cleanup if configured
    if (CleanupInterval > 0.0f)
    {
        SetComponentTickEnabled(true);
        SetComponentTickInterval(CleanupInterval);
    }
    
    UE_LOG(LogSuspenseCoreEquipmentTransaction, Log, 
        TEXT("Transaction Processor initialized on %s with timeout: %.1fs, max depth: %d"), 
        GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"),
        TransactionTimeout, MaxNestedDepth);
}

void USuspenseCoreEquipmentTransactionProcessor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Rollback all active transactions before shutdown
    if (ActiveTransactions.Num() > 0)
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
            TEXT("EndPlay: Rolling back %d active transactions"), 
            ActiveTransactions.Num());
        RollbackAllTransactions();
    }
    
    // Clear all data structures
    ActiveTransactions.Empty();
    TransactionHistory.Empty();
    TransactionStack.Empty();
    SavepointToTransaction.Empty();
    
    UE_LOG(LogSuspenseCoreEquipmentTransaction, Log, 
        TEXT("Transaction Processor shutdown (reason: %s)"), 
        *UEnum::GetValueAsString(EndPlayReason));
    
    Super::EndPlay(EndPlayReason);
}

void USuspenseCoreEquipmentTransactionProcessor::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    // Periodic cleanup of expired transactions
    if (GetWorld())
    {
        float CurrentTime = GetWorld()->GetTimeSeconds();
        if (CurrentTime - LastCleanupTime > CleanupInterval)
        {
            CleanupExpiredTransactions();
            LastCleanupTime = CurrentTime;
        }
    }
}

//========================================
// ISuspenseCoreTransactionManager Implementation (Basic API)
//========================================

FGuid USuspenseCoreEquipmentTransactionProcessor::BeginTransaction(const FString& Description)
{
    // Phase 1: Validate and prepare under lock
    FGuid TransactionId = FGuid::NewGuid();
    FGuid ParentId;
    
    {
        FScopeLock Lock(&TransactionLock);
        
        // Check initialization
        if (!bIsInitialized || !DataProvider.GetInterface())
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Error, 
                TEXT("BeginTransaction: Processor not initialized"));
            return FGuid();
        }
        
        // Check nesting depth
        if (TransactionStack.Num() >= MaxNestedDepth)
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Error,
                TEXT("BeginTransaction: Maximum nesting depth %d exceeded"), 
                MaxNestedDepth);
            return FGuid();
        }
        
        // Get parent if nested
        if (TransactionStack.Num() > 0)
        {
            ParentId = TransactionStack.Last();
        }
    }
    
    // Phase 2: Capture initial snapshot WITHOUT lock
    FEquipmentStateSnapshot InitialSnapshot = CaptureStateSnapshot();
    
    // Phase 3: Create and register transaction under lock
    {
        FScopeLock Lock(&TransactionLock);
        
        // Double-check depth (could have changed)
        if (TransactionStack.Num() >= MaxNestedDepth)
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Error,
                TEXT("BeginTransaction: Maximum nesting depth %d exceeded (post-capture)"), 
                MaxNestedDepth);
            return FGuid();
        }
        
        // Create execution context
        FTransactionExecutionContext Context = CreateExecutionContext(TransactionId, Description, ParentId);
        Context.InitialSnapshot = InitialSnapshot;
        Context.CurrentSnapshot = InitialSnapshot;
        Context.TransactionData.State = ETransactionState::Active;
        Context.TransactionData.StateBefore = InitialSnapshot;
        
        // Register transaction
        ActiveTransactions.Add(TransactionId, Context);
        TransactionStack.Add(TransactionId);
        
        // Update statistics
        TotalTransactionsStarted++;
        
        if (bEnableLogging)
        {
            LogTransactionEvent(TransactionId, FString::Printf(
                TEXT("Transaction started: %s (Nested: %s)"), 
                *Description,
                ParentId.IsValid() ? TEXT("Yes") : TEXT("No")));
        }
    }
    
    // Phase 4: Notify DataStore WITHOUT lock
    if (USuspenseCoreEquipmentDataStore* DataStore = Cast<USuspenseCoreEquipmentDataStore>(DataProvider.GetObject()))
    {
        DataStore->SetActiveTransaction(TransactionId);
    }
    
    return TransactionId;
}

bool USuspenseCoreEquipmentTransactionProcessor::CommitTransaction(const FGuid& TransactionId)
{
    // Phase 1: Validate and prepare under lock
    FTransactionExecutionContext ContextCopy;
    bool bCanCommit = false;
    ETransactionState OldState = ETransactionState::None;
    
    {
        FScopeLock Lock(&TransactionLock);
        
        if (!TransactionId.IsValid())
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
                TEXT("CommitTransaction: Invalid transaction ID"));
            return false;
        }
        
        // Find transaction
        FTransactionExecutionContext* Context = FindExecutionContext(TransactionId);
        if (!Context)
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
                TEXT("CommitTransaction: Transaction %s not found"), 
                *TransactionId.ToString());
            return false;
        }
        
        // Check state
        OldState = Context->TransactionData.State;
        if (Context->TransactionData.State != ETransactionState::Active)
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
                TEXT("CommitTransaction: Transaction %s is not active (state: %d)"), 
                *TransactionId.ToString(), (int32)Context->TransactionData.State);
            return false;
        }
        
        // Check if current transaction (must be top of stack for proper nesting)
        if (TransactionStack.Num() > 0 && TransactionStack.Last() != TransactionId)
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
                TEXT("CommitTransaction: Transaction %s is not the current transaction"), 
                *TransactionId.ToString());
            return false;
        }
        
        // Validate transaction
        if (!ValidateTransaction_NoLock(TransactionId))
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Error,
                TEXT("CommitTransaction: Transaction %s validation failed"),
                *TransactionId.ToString());
            Context->TransactionData.State = ETransactionState::Failed;
            TotalTransactionsFailed++;
            return false;
        }
        
        // Copy context for execution outside lock
        ContextCopy = *Context;
        Context->TransactionData.State = ETransactionState::Committing;
        bCanCommit = true;
        
        // Notify state change
        NotifyTransactionStateChange(TransactionId, OldState, ETransactionState::Committing);
    }
    
    // Phase 2: Execute commit WITHOUT lock
    bool bSuccess = false;
    FEquipmentStateSnapshot AfterSnapshot;
    TArray<FEquipmentDelta> GeneratedDeltas;
    
    if (bCanCommit)
    {
        bSuccess = ExecuteCommit(ContextCopy);
        if (bSuccess)
        {
            // Capture state after commit
            AfterSnapshot = CaptureStateSnapshot();
            
            // Generate deltas if enabled
            if (bGenerateDeltas)
            {
                GeneratedDeltas = GenerateDeltasFromTransaction(ContextCopy);
                TotalDeltasGenerated += GeneratedDeltas.Num();
            }
        }
    }
    
    // Phase 3: Update state under lock
    {
        FScopeLock Lock(&TransactionLock);
        
        FTransactionExecutionContext* Context = FindExecutionContext(TransactionId);
        if (!Context)
        {
            // Transaction was removed while committing
            return false;
        }
        
        if (bSuccess)
        {
            Context->TransactionData.State = ETransactionState::Committed;
            Context->TransactionData.bIsCommitted = true;
            Context->TransactionData.EndTime = FDateTime::Now();
            Context->TransactionData.StateAfter = AfterSnapshot;
            Context->GeneratedDeltas = GeneratedDeltas;
            
            // Remove from stack
            TransactionStack.Remove(TransactionId);
            
            // Move to history
            if (TransactionHistory.Num() >= MaxHistorySize)
            {
                TransactionHistory.RemoveAt(0);
            }
            TransactionHistory.Add(Context->TransactionData);
            
            // Remove from active
            ActiveTransactions.Remove(TransactionId);
            
            // Update statistics
            TotalTransactionsCommitted++;
            
            if (bEnableLogging)
            {
                LogTransactionEvent(TransactionId, 
                    FString::Printf(TEXT("Transaction committed successfully (%d operations, %d deltas)"),
                        Context->Operations.Num(), GeneratedDeltas.Num()));
            }
            
            // Notify state change
            NotifyTransactionStateChange(TransactionId, ETransactionState::Committing, ETransactionState::Committed);
        }
        else
        {
            Context->TransactionData.State = ETransactionState::Failed;
            TotalTransactionsFailed++;
            
            // Auto-recovery attempt
            if (bAutoRecovery)
            {
                UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
                    TEXT("CommitTransaction: Attempting auto-recovery for transaction %s"), 
                    *TransactionId.ToString());
                
                // Remove from stack and active
                TransactionStack.Remove(TransactionId);
                ActiveTransactions.Remove(TransactionId);
            }
            
            // Notify state change
            NotifyTransactionStateChange(TransactionId, ETransactionState::Committing, ETransactionState::Failed);
        }
    }
    
    // Phase 4: Clear active transaction on DataStore WITHOUT lock
    if (USuspenseCoreEquipmentDataStore* DataStore = Cast<USuspenseCoreEquipmentDataStore>(DataProvider.GetObject()))
    {
        DataStore->ClearActiveTransaction();
    }
    
    // Phase 5: Broadcast deltas if successful
    if (bSuccess && OnTransactionDelta.IsBound() && GeneratedDeltas.Num() > 0)
    {
        OnTransactionDelta.Execute(GeneratedDeltas);
    }
    
    return bSuccess;
}

bool USuspenseCoreEquipmentTransactionProcessor::RollbackTransaction(const FGuid& TransactionId)
{
    // Phase 1: Validate and prepare under lock
    FTransactionExecutionContext ContextCopy;
    bool bCanRollback = false;
    ETransactionState OldState = ETransactionState::None;
    
    {
        FScopeLock Lock(&TransactionLock);
        
        if (!TransactionId.IsValid())
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
                TEXT("RollbackTransaction: Invalid transaction ID"));
            return false;
        }
        
        // Find transaction
        FTransactionExecutionContext* Context = FindExecutionContext(TransactionId);
        if (!Context)
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
                TEXT("RollbackTransaction: Transaction %s not found"), 
                *TransactionId.ToString());
            return false;
        }
        
        // Check state
        OldState = Context->TransactionData.State;
        if (Context->TransactionData.State == ETransactionState::Committed || 
            Context->TransactionData.State == ETransactionState::RolledBack)
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
                TEXT("RollbackTransaction: Transaction %s already finalized (state: %d)"), 
                *TransactionId.ToString(), (int32)Context->TransactionData.State);
            return false;
        }
        
        // Copy context for execution outside lock
        ContextCopy = *Context;
        Context->TransactionData.State = ETransactionState::RollingBack;
        bCanRollback = true;
        
        // Notify state change
        NotifyTransactionStateChange(TransactionId, OldState, ETransactionState::RollingBack);
    }
    
    // Phase 2: Execute rollback WITHOUT lock
    bool bSuccess = false;
    TArray<FEquipmentDelta> RevertDeltas;
    
    if (bCanRollback)
    {
        bSuccess = ExecuteRollback(ContextCopy);
        
        // Generate revert deltas if successful
        if (bSuccess && bGenerateDeltas)
        {
            for (const FTransactionOperation& Op : ContextCopy.Operations)
            {
                FEquipmentDelta Delta = CreateDeltaFromOperation(Op, TransactionId);
                
                // Mark as revert and swap before/after
                Delta.ChangeType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Delta.Revert"));
                Delta.ReasonTag = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Reason.Rollback"));
                FSuspenseCoreInventoryItemInstance Temp = Delta.ItemBefore;
                Delta.ItemBefore = Delta.ItemAfter;
                Delta.ItemAfter = Temp;
                
                RevertDeltas.Add(Delta);
            }
            TotalDeltasGenerated += RevertDeltas.Num();
        }
    }
    
    // Phase 3: Update state under lock
    {
        FScopeLock Lock(&TransactionLock);
        
        FTransactionExecutionContext* Context = FindExecutionContext(TransactionId);
        if (!Context)
        {
            // Transaction was removed
            return false;
        }
        
        if (bSuccess)
        {
            Context->TransactionData.State = ETransactionState::RolledBack;
            Context->TransactionData.bIsRolledBack = true;
            Context->TransactionData.EndTime = FDateTime::Now();
            Context->GeneratedDeltas = RevertDeltas;
            
            // Remove from stack
            TransactionStack.Remove(TransactionId);
            
            // Move to history
            if (TransactionHistory.Num() >= MaxHistorySize)
            {
                TransactionHistory.RemoveAt(0);
            }
            TransactionHistory.Add(Context->TransactionData);
            
            // Clean up savepoints
            TArray<FGuid> SavepointsToRemove;
            for (const auto& Pair : SavepointToTransaction)
            {
                if (Pair.Value == TransactionId)
                {
                    SavepointsToRemove.Add(Pair.Key);
                }
            }
            for (const FGuid& SavepointId : SavepointsToRemove)
            {
                SavepointToTransaction.Remove(SavepointId);
            }
            
            // Remove from active
            ActiveTransactions.Remove(TransactionId);
            
            // Update statistics
            TotalTransactionsRolledBack++;
            
            if (bEnableLogging)
            {
                LogTransactionEvent(TransactionId, 
                    FString::Printf(TEXT("Transaction rolled back successfully (%d revert deltas)"),
                        RevertDeltas.Num()));
            }
            
            // Notify state change
            NotifyTransactionStateChange(TransactionId, ETransactionState::RollingBack, ETransactionState::RolledBack);
        }
        else
        {
            Context->TransactionData.State = ETransactionState::Failed;
            TotalTransactionsFailed++;
            
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Error, 
                TEXT("RollbackTransaction: Failed to rollback transaction %s"), 
                *TransactionId.ToString());
            
            // Notify state change
            NotifyTransactionStateChange(TransactionId, ETransactionState::RollingBack, ETransactionState::Failed);
        }
    }
    
    // Phase 4: Clear active transaction on DataStore WITHOUT lock
    if (USuspenseCoreEquipmentDataStore* DataStore = Cast<USuspenseCoreEquipmentDataStore>(DataProvider.GetObject()))
    {
        DataStore->ClearActiveTransaction();
    }
    
    // Phase 5: Broadcast revert deltas if successful
    if (bSuccess && OnTransactionDelta.IsBound() && RevertDeltas.Num() > 0)
    {
        OnTransactionDelta.Execute(RevertDeltas);
    }
    
    return bSuccess;
}

FGuid USuspenseCoreEquipmentTransactionProcessor::BeginNestedTransaction(const FString& Description)
{
    // Phase 1: Get parent ID under lock
    FGuid ParentId;
    {
        FScopeLock Lock(&TransactionLock);
        
        if (TransactionStack.Num() == 0)
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
                TEXT("BeginNestedTransaction: No parent transaction active"));
            return FGuid();
        }
        
        ParentId = TransactionStack.Last();
    }
    
    // Phase 2: Create nested transaction WITHOUT lock
    FGuid NewTransactionId = BeginTransaction(Description);
    
    // Phase 3: Mark as nested under lock
    if (NewTransactionId.IsValid())
    {
        FScopeLock Lock(&TransactionLock);
        
        if (FTransactionExecutionContext* Context = FindExecutionContext(NewTransactionId))
        {
            Context->TransactionData.bIsNested = true;
            Context->TransactionData.ParentTransactionId = ParentId;
        }
    }
    
    return NewTransactionId;
}

bool USuspenseCoreEquipmentTransactionProcessor::RegisterOperation(const FGuid& OperationId)
{
    FScopeLock Lock(&TransactionLock);
    
    if (TransactionStack.Num() == 0)
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
            TEXT("RegisterOperation: No active transaction"));
        return false;
    }
    
    FGuid CurrentTransactionId = TransactionStack.Last();
    FTransactionExecutionContext* Context = FindExecutionContext(CurrentTransactionId);
    
    if (!Context)
    {
        return false;
    }
    
    // Add operation ID to transaction
    Context->TransactionData.OperationIds.Add(OperationId);
    
    // Create minimal operation record
    FTransactionOperation Op;
    Op.OperationId = OperationId;
    Op.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    Context->Operations.Add(Op);
    
    TotalOperationsProcessed++;
    
    return true;
}

bool USuspenseCoreEquipmentTransactionProcessor::ValidateTransaction(const FGuid& TransactionId) const
{
    FScopeLock Lock(&TransactionLock);
    return ValidateTransaction_NoLock(TransactionId);
}

TArray<FEquipmentTransaction> USuspenseCoreEquipmentTransactionProcessor::GetTransactionHistory(int32 MaxCount) const
{
    FScopeLock Lock(&TransactionLock);
    
    TArray<FEquipmentTransaction> History;
    
    int32 StartIndex = FMath::Max(0, TransactionHistory.Num() - MaxCount);
    for (int32 i = StartIndex; i < TransactionHistory.Num(); i++)
    {
        History.Add(TransactionHistory[i]);
    }
    
    return History;
}

//========================================
// ISuspenseCoreTransactionManager Implementation (Extended API)
//========================================

bool USuspenseCoreEquipmentTransactionProcessor::SupportsExtendedOps() const
{
    // Этот процессор реализует расширенное API
    return true;
}

bool USuspenseCoreEquipmentTransactionProcessor::RegisterOperation(
    const FGuid& TransactionId,
    const FTransactionOperation& Operation)
{
    FScopeLock Lock(&TransactionLock);

    if (!TransactionId.IsValid())
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, TEXT("RegisterOperation(Txn,Op): invalid TxnId"));
        return false;
    }

    FTransactionExecutionContext* Ctx = FindExecutionContext(TransactionId);
    if (!Ctx)
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, TEXT("RegisterOperation(Txn,Op): transaction not found"));
        return false;
    }

    if (Ctx->TransactionData.State != ETransactionState::Active)
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, TEXT("RegisterOperation(Txn,Op): transaction is not active"));
        return false;
    }

    // Сохраняем копию операции, гарантирующую корректный OperationId
    FTransactionOperation Op = Operation;
    if (!Op.OperationId.IsValid())
    {
        Op.OperationId = FGuid::NewGuid();
    }

    // Если не задан ItemBefore, попробуем его заполнить из текущего снапшота
    if (Op.SlotIndex >= 0 && !Op.ItemBefore.IsValid())
    {
        for (const FEquipmentSlotSnapshot& Slot : Ctx->CurrentSnapshot.SlotSnapshots)
        {
            if (Slot.SlotIndex == Op.SlotIndex)
            {
                Op.ItemBefore = Slot.ItemInstance;
                break;
            }
        }
    }

    // Регистрируем
    Ctx->Operations.Add(Op);
    Ctx->TransactionData.OperationIds.Add(Op.OperationId);
    TotalOperationsProcessed++;

    if (bEnableLogging)
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Verbose,
            TEXT("RegisterOperation: Txn=%s, Op=%s, Slot=%d, Type=%s"),
            *TransactionId.ToString().Left(8),
            *Op.OperationId.ToString().Left(8),
            Op.SlotIndex,
            *Op.OperationType.ToString());
    }

    return true;
}

bool USuspenseCoreEquipmentTransactionProcessor::ApplyOperation(
    const FGuid& TransactionId,
    const FTransactionOperation& Operation)
{
    // Фаза 1: найти контекст и ссылку на хранимую операцию (под блокировкой)
    FTransactionOperation OpForApply;
    {
        FScopeLock Lock(&TransactionLock);

        if (!TransactionId.IsValid())
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, TEXT("ApplyOperation(Txn,Op): invalid TxnId"));
            return false;
        }

        FTransactionExecutionContext* Ctx = FindExecutionContext(TransactionId);
        if (!Ctx)
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, TEXT("ApplyOperation(Txn,Op): transaction not found"));
            return false;
        }

        if (Ctx->TransactionData.State != ETransactionState::Active)
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, TEXT("ApplyOperation(Txn,Op): transaction is not active"));
            return false;
        }

        // Найдём зарегистрированную запись по OperationId (последняя победит)
        int32 Index = INDEX_NONE;
        for (int32 i = Ctx->Operations.Num() - 1; i >= 0; --i)
        {
            if (Ctx->Operations[i].OperationId == Operation.OperationId)
            {
                Index = i;
                break;
            }
        }

        if (Index == INDEX_NONE)
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, TEXT("ApplyOperation: op not registered (id=%s)"),
                *Operation.OperationId.ToString());
            return false;
        }

        // Обновим хранившийся Op полями из входного Operation (ItemAfter/Metadata могут быть новее)
        FTransactionOperation& Stored = Ctx->Operations[Index];

        // Обновим базовые поля (если переданы)
        if (Operation.OperationType.IsValid()) Stored.OperationType = Operation.OperationType;
        if (Operation.SlotIndex != INDEX_NONE)  Stored.SlotIndex     = Operation.SlotIndex;

        // Если нет корректного ItemBefore — подтянем из снапшота
        if (Stored.SlotIndex >= 0 && !Stored.ItemBefore.IsValid())
        {
            for (const FEquipmentSlotSnapshot& Slot : Ctx->CurrentSnapshot.SlotSnapshots)
            {
                if (Slot.SlotIndex == Stored.SlotIndex)
                {
                    Stored.ItemBefore = Slot.ItemInstance;
                    break;
                }
            }
        }

        // Запомним ItemAfter/Metadata из аргумента
        if (Operation.ItemAfter.IsValid())
        {
            Stored.ItemAfter = Operation.ItemAfter;
        }
        for (const auto& Kvp : Operation.Metadata)
        {
            Stored.Metadata.Add(Kvp.Key, Kvp.Value);
        }

        // Снимем копию для применения вне секции
        OpForApply = Stored;
    }

    // Фаза 2: применяем к рабочему снапшоту БЕЗ глобального лока
    // (обновим CurrentSnapshot; реальные Set/Clear делаются при Commit)
    {
        FScopeLock Lock(&TransactionLock);
        FTransactionExecutionContext* Ctx = FindExecutionContext(TransactionId);
        if (!Ctx) return false;

        // Обновим текущий снапшот детерминированно
        const bool bOk = ApplyOperation(OpForApply, Ctx->CurrentSnapshot);
        if (!bOk)
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning,
                TEXT("ApplyOperation: failed to update working snapshot for op %s"),
                *OpForApply.OperationId.ToString());
            return false;
        }

        // Перепишем ItemAfter обратно в хранимую операцию (на случай, если helper вычислил его)
        for (FTransactionOperation& OpRef : Ctx->Operations)
        {
            if (OpRef.OperationId == OpForApply.OperationId)
            {
                OpRef.ItemAfter = OpForApply.ItemAfter;
                break;
            }
        }
    }

    if (bEnableLogging)
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Verbose,
            TEXT("ApplyOperation: Txn=%s, Op=%s applied to working snapshot"),
            *TransactionId.ToString().Left(8),
            *Operation.OperationId.ToString().Left(8));
    }

    return true;
}

TArray<FEquipmentDelta> USuspenseCoreEquipmentTransactionProcessor::GetTransactionDeltas(
    const FGuid& TransactionId) const
{
    FScopeLock Lock(&TransactionLock);

    const FTransactionExecutionContext* Ctx = FindExecutionContext(TransactionId);
    if (Ctx)
    {
        return Ctx->GeneratedDeltas; // будут заполнены при Commit
    }
    return {};
}

//========================================
// Savepoint Management
//========================================

FGuid USuspenseCoreEquipmentTransactionProcessor::CreateSavepoint(const FString& SavepointName)
{
    // Phase 1: Validate and prepare under lock
    FGuid CurrentTransactionId;
    int32 OperationIndex = 0;
    
    {
        FScopeLock Lock(&TransactionLock);
        
        if (TransactionStack.Num() == 0)
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
                TEXT("CreateSavepoint: No active transaction"));
            return FGuid();
        }
        
        CurrentTransactionId = TransactionStack.Last();
        FTransactionExecutionContext* Context = FindExecutionContext(CurrentTransactionId);
        
        if (!Context)
        {
            return FGuid();
        }
        
        OperationIndex = Context->Operations.Num();
    }
    
    // Phase 2: Capture snapshot WITHOUT lock
    FEquipmentStateSnapshot Snapshot = CaptureStateSnapshot();
    
    // Phase 3: Store savepoint under lock
    FGuid SavepointId = FGuid::NewGuid();
    
    {
        FScopeLock Lock(&TransactionLock);
        
        // Re-find context (could have changed)
        FTransactionExecutionContext* Context = FindExecutionContext(CurrentTransactionId);
        if (!Context)
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
                TEXT("CreateSavepoint: Transaction disappeared while creating savepoint"));
            return FGuid();
        }
        
        // Create and store savepoint
        FTransactionSavepoint Savepoint;
        Savepoint.SavepointId = SavepointId;
        Savepoint.Name = SavepointName;
        Savepoint.CreationTime = FDateTime::Now();
        Savepoint.OperationIndex = OperationIndex;
        Savepoint.Snapshot = Snapshot;
        
        Context->Savepoints.Add(Savepoint);
        SavepointToTransaction.Add(SavepointId, CurrentTransactionId);
        
        if (bEnableLogging)
        {
            LogTransactionEvent(CurrentTransactionId, 
                FString::Printf(TEXT("Savepoint '%s' created (ID: %s)"), 
                    *SavepointName, *SavepointId.ToString()));
        }
    }
    
    return SavepointId;
}

bool USuspenseCoreEquipmentTransactionProcessor::RollbackToSavepoint(const FGuid& SavepointId)
{
    // Phase 1: Collect data under lock
    FGuid TransactionId;
    FTransactionSavepoint SavepointCopy;
    bool bCanRollback = false;
    
    {
        FScopeLock Lock(&TransactionLock);
        
        if (!SavepointId.IsValid())
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
                TEXT("RollbackToSavepoint: Invalid savepoint ID"));
            return false;
        }
        
        // Find transaction for savepoint
        const FGuid* TransactionIdPtr = SavepointToTransaction.Find(SavepointId);
        if (!TransactionIdPtr)
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
                TEXT("RollbackToSavepoint: Savepoint %s not found"), 
                *SavepointId.ToString());
            return false;
        }
        
        TransactionId = *TransactionIdPtr;
        FTransactionExecutionContext* Context = FindExecutionContext(TransactionId);
        
        if (!Context)
        {
            return false;
        }
        
        // Find savepoint
        FTransactionSavepoint* Savepoint = FindSavepoint(*Context, SavepointId);
        if (!Savepoint)
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
                TEXT("RollbackToSavepoint: Savepoint %s not found in transaction"), 
                *SavepointId.ToString());
            return false;
        }
        
        SavepointCopy = *Savepoint;
        bCanRollback = true;
    }
    
    if (!bCanRollback)
    {
        return false;
    }
    
    // Phase 2: Restore snapshot WITHOUT lock
    bool bSuccess = RestoreStateSnapshot(SavepointCopy.Snapshot);
    
    if (!bSuccess)
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Error, 
            TEXT("RollbackToSavepoint: Failed to restore snapshot"));
        return false;
    }
    
    // Phase 3: Update context under lock
    {
        FScopeLock Lock(&TransactionLock);
        
        FTransactionExecutionContext* Context = FindExecutionContext(TransactionId);
        if (!Context)
        {
            return false;
        }
        
        // Truncate operations to savepoint
        if (SavepointCopy.OperationIndex < Context->Operations.Num())
        {
            Context->Operations.SetNum(SavepointCopy.OperationIndex);
        }
        
        // Update operation IDs
        Context->TransactionData.OperationIds.Empty();
        for (const FTransactionOperation& Op : Context->Operations)
        {
            Context->TransactionData.OperationIds.Add(Op.OperationId);
        }
        
        // Remove later savepoints
        Context->Savepoints.RemoveAll([&SavepointCopy](const FTransactionSavepoint& SP)
        {
            return SP.CreationTime > SavepointCopy.CreationTime;
        });
        
        // Restore snapshot
        Context->CurrentSnapshot = SavepointCopy.Snapshot;
        
        if (bEnableLogging)
        {
            LogTransactionEvent(TransactionId, 
                FString::Printf(TEXT("Rolled back to savepoint '%s'"), 
                    *SavepointCopy.Name));
        }
    }
    
    return true;
}

//========================================
// Query Methods
//========================================

bool USuspenseCoreEquipmentTransactionProcessor::IsTransactionActive() const
{
    FScopeLock Lock(&TransactionLock);
    return TransactionStack.Num() > 0;
}

FEquipmentTransaction USuspenseCoreEquipmentTransactionProcessor::GetCurrentTransaction() const
{
    FScopeLock Lock(&TransactionLock);
    
    if (TransactionStack.Num() == 0)
    {
        return FEquipmentTransaction();
    }
    
    FGuid CurrentId = TransactionStack.Last();
    const FTransactionExecutionContext* Context = FindExecutionContext(CurrentId);
    
    if (Context)
    {
        return ConvertToTransaction(*Context);
    }
    
    return FEquipmentTransaction();
}

FGuid USuspenseCoreEquipmentTransactionProcessor::GetCurrentTransactionId() const
{
    FScopeLock Lock(&TransactionLock);
    
    if (TransactionStack.Num() > 0)
    {
        return TransactionStack.Last();
    }
    
    return FGuid();
}

FEquipmentTransaction USuspenseCoreEquipmentTransactionProcessor::GetTransaction(const FGuid& TransactionId) const
{
    FScopeLock Lock(&TransactionLock);
    
    // Check active transactions first
    const FTransactionExecutionContext* Context = FindExecutionContext(TransactionId);
    if (Context)
    {
        return ConvertToTransaction(*Context);
    }
    
    // Check history
    for (const FEquipmentTransaction& Transaction : TransactionHistory)
    {
        if (Transaction.TransactionId == TransactionId)
        {
            return Transaction;
        }
    }
    
    return FEquipmentTransaction();
}

//========================================
// Extended Transaction Management
//========================================

int32 USuspenseCoreEquipmentTransactionProcessor::CommitAllTransactions()
{
    // Phase 1: Collect transaction list under lock
    TArray<FGuid> TransactionsToCommit;
    {
        FScopeLock Lock(&TransactionLock);
        
        // Copy stack in reverse order (from nested to parent)
        for (int32 i = TransactionStack.Num() - 1; i >= 0; i--)
        {
            TransactionsToCommit.Add(TransactionStack[i]);
        }
        
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Log, 
            TEXT("CommitAllTransactions: Preparing to commit %d transactions"), 
            TransactionsToCommit.Num());
    }
    
    // Phase 2: Process transactions WITHOUT global lock
    int32 CommittedCount = 0;
    
    for (const FGuid& TransactionId : TransactionsToCommit)
    {
        // Each CommitTransaction call takes lock independently
        bool bSuccess = CommitTransaction(TransactionId);
        
        if (bSuccess)
        {
            CommittedCount++;
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Verbose, 
                TEXT("CommitAllTransactions: Successfully committed transaction %s"), 
                *TransactionId.ToString());
        }
        else
        {
            // Stop on first failure
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
                TEXT("CommitAllTransactions: Failed to commit transaction %s, stopping"), 
                *TransactionId.ToString());
            break;
        }
    }
    
    UE_LOG(LogSuspenseCoreEquipmentTransaction, Log, 
        TEXT("CommitAllTransactions: Committed %d of %d transactions"), 
        CommittedCount, TransactionsToCommit.Num());
    
    return CommittedCount;
}

int32 USuspenseCoreEquipmentTransactionProcessor::RollbackAllTransactions()
{
    // Phase 1: Collect transaction list under lock
    TArray<FGuid> TransactionsToRollback;
    {
        FScopeLock Lock(&TransactionLock);
        
        // Copy stack in reverse order (from nested to parent)
        for (int32 i = TransactionStack.Num() - 1; i >= 0; i--)
        {
            TransactionsToRollback.Add(TransactionStack[i]);
        }
        
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Log, 
            TEXT("RollbackAllTransactions: Preparing to rollback %d transactions"), 
            TransactionsToRollback.Num());
    }
    
    // Phase 2: Process transactions WITHOUT global lock
    int32 RolledBackCount = 0;
    
    for (const FGuid& TransactionId : TransactionsToRollback)
    {
        // Each RollbackTransaction call takes lock independently
        bool bSuccess = RollbackTransaction(TransactionId);
        
        if (bSuccess)
        {
            RolledBackCount++;
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Verbose, 
                TEXT("RollbackAllTransactions: Successfully rolled back transaction %s"), 
                *TransactionId.ToString());
        }
        else
        {
            // Try to force cleanup on failure
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Error, 
                TEXT("RollbackAllTransactions: Failed to rollback transaction %s, forcing cleanup"), 
                *TransactionId.ToString());
            
            // Force cleanup under lock
            {
                FScopeLock Lock(&TransactionLock);
                
                // Remove from stack
                TransactionStack.Remove(TransactionId);
                
                // Mark as failed
                if (FTransactionExecutionContext* Context = FindExecutionContext(TransactionId))
                {
                    Context->TransactionData.State = ETransactionState::Failed;
                    Context->TransactionData.EndTime = FDateTime::Now();
                    
                    // Move to history
                    if (TransactionHistory.Num() >= MaxHistorySize)
                    {
                        TransactionHistory.RemoveAt(0);
                    }
                    TransactionHistory.Add(Context->TransactionData);
                    
                    // Remove from active
                    ActiveTransactions.Remove(TransactionId);
                }
            }
        }
    }
    
    // Phase 3: Final cleanup check
    {
        FScopeLock Lock(&TransactionLock);
        
        if (TransactionStack.Num() > 0)
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Error, 
                TEXT("RollbackAllTransactions: %d transactions remain in stack after rollback"), 
                TransactionStack.Num());
            
            // Force clear stack
            TransactionStack.Empty();
        }
    }
    
    UE_LOG(LogSuspenseCoreEquipmentTransaction, Log, 
        TEXT("RollbackAllTransactions: Rolled back %d of %d transactions"), 
        RolledBackCount, TransactionsToRollback.Num());
    
    return RolledBackCount;
}

FGuid USuspenseCoreEquipmentTransactionProcessor::RecordDetailedOperation(const FTransactionOperation& Operation)
{
    FScopeLock Lock(&TransactionLock);
    
    if (TransactionStack.Num() == 0)
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
            TEXT("RecordDetailedOperation: No active transaction"));
        return FGuid();
    }
    
    FGuid CurrentTransactionId = TransactionStack.Last();
    FTransactionExecutionContext* Context = FindExecutionContext(CurrentTransactionId);
    
    if (!Context)
    {
        return FGuid();
    }
    
    // Add operation with generated ID if not provided
    FTransactionOperation OpWithId = Operation;
    if (!OpWithId.OperationId.IsValid())
    {
        OpWithId.OperationId = FGuid::NewGuid();
    }
    OpWithId.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    
    // Store operation
    Context->Operations.Add(OpWithId);
    Context->TransactionData.OperationIds.Add(OpWithId.OperationId);
    
    // Apply operation to current snapshot
    ApplyOperation(OpWithId, Context->CurrentSnapshot);
    
    // Update statistics
    TotalOperationsProcessed++;
    
    if (bEnableLogging)
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Verbose,
            TEXT("Operation %s recorded for transaction %s"),
            *OpWithId.OperationId.ToString(),
            *CurrentTransactionId.ToString());
    }
    
    return OpWithId.OperationId;
}

bool USuspenseCoreEquipmentTransactionProcessor::ReleaseSavepoint(const FGuid& SavepointId)
{
    FScopeLock Lock(&TransactionLock);
    
    if (!SavepointId.IsValid())
    {
        return false;
    }
    
    // Find transaction for savepoint
    const FGuid* TransactionIdPtr = SavepointToTransaction.Find(SavepointId);
    if (!TransactionIdPtr)
    {
        return false;
    }
    
    FGuid TransactionId = *TransactionIdPtr;
    FTransactionExecutionContext* Context = FindExecutionContext(TransactionId);
    
    if (!Context)
    {
        return false;
    }
    
    // Remove savepoint
    int32 RemovedCount = Context->Savepoints.RemoveAll([SavepointId](const FTransactionSavepoint& SP)
    {
        return SP.SavepointId == SavepointId;
    });
    
    if (RemovedCount > 0)
    {
        SavepointToTransaction.Remove(SavepointId);
        
        if (bEnableLogging)
        {
            LogTransactionEvent(TransactionId, 
                FString::Printf(TEXT("Savepoint %s released"), 
                    *SavepointId.ToString()));
        }
    }
    
    return RemovedCount > 0;
}

void USuspenseCoreEquipmentTransactionProcessor::ClearTransactionHistory(bool bKeepActive)
{
    // Clear history under lock
    {
        FScopeLock Lock(&TransactionLock);
        TransactionHistory.Empty();
    }
    
    if (!bKeepActive)
    {
        // Rollback all WITHOUT lock
        RollbackAllTransactions();
        
        // Clear remaining data under lock
        FScopeLock Lock(&TransactionLock);
        ActiveTransactions.Empty();
        TransactionStack.Empty();
        SavepointToTransaction.Empty();
    }
    
    UE_LOG(LogSuspenseCoreEquipmentTransaction, Log, 
        TEXT("Transaction history cleared (keep active: %s)"),
        bKeepActive ? TEXT("true") : TEXT("false"));
}

//========================================
// Validation and Recovery
//========================================

FTransactionValidationResult USuspenseCoreEquipmentTransactionProcessor::ValidateTransactionIntegrity(const FGuid& TransactionId) const
{
    FScopeLock Lock(&TransactionLock);
    
    FTransactionValidationResult Result;
    
    const FTransactionExecutionContext* Context = FindExecutionContext(TransactionId);
    if (!Context)
    {
        Result.Errors.Add(FText::FromString(TEXT("Transaction not found")));
        return Result;
    }
    
    // Validate operation sequence
    FEquipmentStateSnapshot TestSnapshot = Context->InitialSnapshot;
    for (const FTransactionOperation& Op : Context->Operations)
    {
        // Check operation validity
        if (!Op.OperationId.IsValid())
        {
            Result.Errors.Add(FText::FromString(
                TEXT("Invalid operation ID in transaction")));
        }
        
        if (Op.SlotIndex < 0 && Op.OperationType.IsValid())
        {
            Result.Warnings.Add(FText::FromString(
                FString::Printf(TEXT("Operation %s has invalid slot index"), 
                    *Op.OperationId.ToString())));
        }
        
        // Apply operation manually here since ApplyOperation modifies state
        // We need a const-correct version for validation
        bool bCanApply = false;
        for (FEquipmentSlotSnapshot& SlotSnapshot : TestSnapshot.SlotSnapshots)
        {
            if (SlotSnapshot.SlotIndex == Op.SlotIndex)
            {
                SlotSnapshot.ItemInstance = Op.ItemAfter;
                TestSnapshot.Version++;
                TestSnapshot.Timestamp = FDateTime::Now();
                bCanApply = true;
                break;
            }
        }
        
        // Handle global operations
        if (!bCanApply && Op.OperationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Global"))))
        {
            TestSnapshot.Version++;
            TestSnapshot.Timestamp = FDateTime::Now();
            bCanApply = true;
        }
        
        if (!bCanApply && Op.SlotIndex >= 0)
        {
            Result.Errors.Add(FText::FromString(
                FString::Printf(TEXT("Operation %s cannot be applied"), 
                    *Op.OperationId.ToString())));
        }
    }
    
    // Check final state consistency
    if (!ValidateStateConsistency(TestSnapshot))
    {
        Result.Errors.Add(FText::FromString(TEXT("State inconsistency detected")));
    }
    
    // Check for conflicts
    TArray<FTransactionOperation> Conflicts = CheckForConflicts_NoLock(TransactionId);
    if (Conflicts.Num() > 0)
    {
        Result.Conflicts = Conflicts;
        Result.Warnings.Add(FText::Format(
            FText::FromString(TEXT("{0} conflicting operations detected")),
            FText::AsNumber(Conflicts.Num())));
    }
    
    Result.bIsValid = Result.Errors.Num() == 0;
    
    return Result;
}

TArray<FTransactionOperation> USuspenseCoreEquipmentTransactionProcessor::CheckForConflicts(const FGuid& TransactionId) const
{
    FScopeLock Lock(&TransactionLock);
    return CheckForConflicts_NoLock(TransactionId);
}

bool USuspenseCoreEquipmentTransactionProcessor::ResolveConflicts(const FGuid& TransactionId, int32 ResolutionStrategy)
{
    // Phase 1: Collect data under lock
    TArray<FTransactionOperation> Conflicts;
    TArray<FTransactionOperation> SavedOps;
    FString Description;
    bool bHasConflicts = false;
    
    {
        FScopeLock Lock(&TransactionLock);
        
        FTransactionExecutionContext* Context = FindExecutionContext(TransactionId);
        if (!Context)
        {
            return false;
        }
        
        Conflicts = CheckForConflicts_NoLock(TransactionId);
        bHasConflicts = Conflicts.Num() > 0;
        
        if (!bHasConflicts)
        {
            return true; // No conflicts to resolve
        }
        
        if (ResolutionStrategy == 2) // Force
        {
            Context->Metadata.Add(TEXT("ForcedCommit"), TEXT("true"));
            TotalConflictsResolved++;
            
            if (bEnableLogging)
            {
                LogTransactionEvent(TransactionId,
                    FString::Printf(TEXT("Resolved %d conflicts using force strategy"), 
                        Conflicts.Num()));
            }
            
            return true;
        }
        
        // For Abort/Retry save data
        SavedOps = Context->Operations;
        Description = Context->TransactionData.Description;
    }
    
    // Phase 2: External actions WITHOUT lock
    bool bResolved = false;
    
    switch (ResolutionStrategy)
    {
        case 0: // Abort
            bResolved = RollbackTransaction(TransactionId);
            break;
            
        case 1: // Retry
        {
            // Rollback current transaction
            if (RollbackTransaction(TransactionId))
            {
                // Start new transaction
                FGuid NewTx = BeginTransaction(Description + TEXT(" (Retry)"));
                if (NewTx.IsValid())
                {
                    // Replay operations into new transaction
                    for (FTransactionOperation Op : SavedOps)
                    {
                        Op.OperationId = FGuid::NewGuid();
                        RecordDetailedOperation(Op);
                    }
                    
                    // Attempt to commit new transaction
                    bResolved = CommitTransaction(NewTx);
                }
            }
            break;
        }
        
        default:
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning,
                TEXT("ResolveConflicts: Unknown resolution strategy %d"), 
                ResolutionStrategy);
            return false;
    }
    
    // Phase 3: Update statistics under lock
    if (bResolved)
    {
        FScopeLock Lock(&TransactionLock);
        TotalConflictsResolved++;
        
        if (bEnableLogging)
        {
            LogTransactionEvent(TransactionId,
                FString::Printf(TEXT("Resolved %d conflicts using strategy %d"),
                    Conflicts.Num(), ResolutionStrategy));
        }
    }
    
    return bResolved;
}

bool USuspenseCoreEquipmentTransactionProcessor::RecoverTransaction(const FGuid& TransactionId)
{
    // Phase 1: Collect data under lock
    FEquipmentStateSnapshot InitialSnapshot;
    TArray<FTransactionOperation> Operations;
    
    {
        FScopeLock Lock(&TransactionLock);
        
        FTransactionExecutionContext* Context = FindExecutionContext(TransactionId);
        if (!Context)
        {
            return false;
        }
        
        InitialSnapshot = Context->InitialSnapshot;
        Operations = Context->Operations;
    }
    
    UE_LOG(LogSuspenseCoreEquipmentTransaction, Log, 
        TEXT("RecoverTransaction: Attempting recovery for transaction %s"), 
        *TransactionId.ToString());
    
    // Phase 2: Restore initial state WITHOUT lock
    bool bRecovered = RestoreStateSnapshot(InitialSnapshot);
    
    // Phase 3: Update context and replay operations under lock
    {
        FScopeLock Lock(&TransactionLock);
        
        FTransactionExecutionContext* Context = FindExecutionContext(TransactionId);
        if (!Context)
        {
            return false;
        }
        
        if (bRecovered)
        {
            // Replay operations on the snapshot
            Context->CurrentSnapshot = InitialSnapshot;
            
            for (const FTransactionOperation& Op : Operations)
            {
                if (!ApplyOperation(Op, Context->CurrentSnapshot))
                {
                    UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
                        TEXT("RecoverTransaction: Failed to replay operation %s"), 
                        *Op.OperationId.ToString());
                    bRecovered = false;
                    break;
                }
            }
        }
        
        if (bRecovered)
        {
            Context->TransactionData.State = ETransactionState::Active;
            
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Log, 
                TEXT("RecoverTransaction: Successfully recovered transaction %s"), 
                *TransactionId.ToString());
        }
        else
        {
            Context->TransactionData.State = ETransactionState::Failed;
            TotalTransactionsFailed++;
        }
    }
    
    return bRecovered;
}

//========================================
// Configuration
//========================================

bool USuspenseCoreEquipmentTransactionProcessor::Initialize(TScriptInterface<ISuspenseCoreEquipmentDataProvider> InDataProvider)
{
    FScopeLock Lock(&TransactionLock);
    
    if (!InDataProvider.GetInterface())
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Error, 
            TEXT("Initialize: Invalid data provider"));
        return false;
    }
    
    DataProvider = InDataProvider;
    bIsInitialized = true;
    
    UE_LOG(LogSuspenseCoreEquipmentTransaction, Log, 
        TEXT("Transaction processor initialized with data provider"));
    
    return true;
}

void USuspenseCoreEquipmentTransactionProcessor::SetTransactionTimeout(float Seconds)
{
    TransactionTimeout = FMath::Max(0.0f, Seconds);
    
    if (bEnableLogging)
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Log, 
            TEXT("Transaction timeout set to %.1f seconds"), TransactionTimeout);
    }
}

void USuspenseCoreEquipmentTransactionProcessor::SetMaxNestedDepth(int32 MaxDepth)
{
    MaxNestedDepth = FMath::Clamp(MaxDepth, 1, 10);
    
    if (bEnableLogging)
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Log, 
            TEXT("Maximum nested depth set to %d"), MaxNestedDepth);
    }
}

void USuspenseCoreEquipmentTransactionProcessor::SetAutoRecovery(bool bEnable)
{
    bAutoRecovery = bEnable;
    
    if (bEnableLogging)
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Log, 
            TEXT("Auto-recovery %s"), bAutoRecovery ? TEXT("enabled") : TEXT("disabled"));
    }
}

//========================================
// Statistics and Debugging
//========================================

FString USuspenseCoreEquipmentTransactionProcessor::GetTransactionStatistics() const
{
    FScopeLock Lock(&TransactionLock);
    
    float SuccessRate = TotalTransactionsStarted > 0 ? 
        (float)TotalTransactionsCommitted / (float)TotalTransactionsStarted * 100.0f : 0.0f;
    
    float FailureRate = TotalTransactionsStarted > 0 ? 
        (float)TotalTransactionsFailed / (float)TotalTransactionsStarted * 100.0f : 0.0f;
    
    return FString::Printf(TEXT("=== Transaction Statistics ===\n"
                               "Started: %d\n"
                               "Committed: %d (%.1f%%)\n"
                               "Rolled Back: %d\n"
                               "Failed: %d (%.1f%%)\n"
                               "Active: %d\n"
                               "Operations Processed: %d\n"
                               "Conflicts Resolved: %d\n"
                               "Deltas Generated: %d\n"
                               "History Size: %d/%d"),
                          TotalTransactionsStarted,
                          TotalTransactionsCommitted, SuccessRate,
                          TotalTransactionsRolledBack,
                          TotalTransactionsFailed, FailureRate,
                          ActiveTransactions.Num(),
                          TotalOperationsProcessed,
                          TotalConflictsResolved,
                          TotalDeltasGenerated,
                          TransactionHistory.Num(), MaxHistorySize);
}

int32 USuspenseCoreEquipmentTransactionProcessor::GetActiveTransactionCount() const
{
    FScopeLock Lock(&TransactionLock);
    return ActiveTransactions.Num();
}

FString USuspenseCoreEquipmentTransactionProcessor::DumpTransactionState(const FGuid& TransactionId) const
{
    FScopeLock Lock(&TransactionLock);
    
    const FTransactionExecutionContext* Context = FindExecutionContext(TransactionId);
    if (!Context)
    {
        return TEXT("Transaction not found");
    }
    
    FString StateStr;
    switch (Context->TransactionData.State)
    {
        case ETransactionState::None: StateStr = TEXT("None"); break;
        case ETransactionState::Active: StateStr = TEXT("Active"); break;
        case ETransactionState::Committing: StateStr = TEXT("Committing"); break;
        case ETransactionState::Committed: StateStr = TEXT("Committed"); break;
        case ETransactionState::RollingBack: StateStr = TEXT("RollingBack"); break;
        case ETransactionState::RolledBack: StateStr = TEXT("RolledBack"); break;
        case ETransactionState::Failed: StateStr = TEXT("Failed"); break;
        default: StateStr = TEXT("Unknown"); break;
    }
    
    return FString::Printf(TEXT("=== Transaction %s ===\n"
                               "State: %s\n"
                               "Description: %s\n"
                               "Operations: %d\n"
                               "Savepoints: %d\n"
                               "Deltas: %d\n"
                               "Start Time: %s\n"
                               "Is Nested: %s\n"
                               "Parent: %s\n"
                               "Isolation Level: %d"),
                          *TransactionId.ToString(),
                          *StateStr,
                          *Context->TransactionData.Description,
                          Context->Operations.Num(),
                          Context->Savepoints.Num(),
                          Context->GeneratedDeltas.Num(),
                          *Context->TransactionData.StartTime.ToString(),
                          Context->TransactionData.bIsNested ? TEXT("Yes") : TEXT("No"),
                          *Context->TransactionData.ParentTransactionId.ToString(),
                          Context->IsolationLevel);
}

//========================================
// Protected Helper Methods
//========================================

FTransactionExecutionContext USuspenseCoreEquipmentTransactionProcessor::CreateExecutionContext(
    const FGuid& TransactionId,
    const FString& Description,
    const FGuid& ParentId)
{
    FTransactionExecutionContext Context;
    
    // Initialize transaction data
    Context.TransactionData.TransactionId = TransactionId;
    Context.TransactionData.State = ETransactionState::None;
    Context.TransactionData.StartTime = FDateTime::Now();
    Context.TransactionData.Description = Description;
    Context.TransactionData.bIsNested = ParentId.IsValid();
    Context.TransactionData.ParentTransactionId = ParentId;
    
    // Initialize execution context
    Context.Metadata.Add(TEXT("Description"), Description);
    Context.bReadOnly = false;
    Context.IsolationLevel = 0;
    
    return Context;
}

bool USuspenseCoreEquipmentTransactionProcessor::ExecuteCommit(FTransactionExecutionContext& Context)
{
    // ВАЖНО: здесь НЕТ глобального лока; можно звать DataProvider.
    if (!DataProvider.GetInterface())
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Error, TEXT("ExecuteCommit: No data provider"));
        return false;
    }

    // Все операции применяем с bNotify=false — события публикует сервис после коммита.
    for (const FTransactionOperation& Op : Context.Operations)
    {
        const bool bIsSetLike =
            Op.OperationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Set"))) ||
            Op.OperationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Equip"))) ||
            Op.OperationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.MoveTarget"))) ||
            Op.OperationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Upgrade"))) ||
            Op.OperationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Modify")));

        const bool bIsClearLike =
            Op.OperationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Clear"))) ||
            Op.OperationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Unequip"))) ||
            Op.OperationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Drop"))) ||
            Op.OperationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.MoveSource")));

        const bool bIsSwap =
            Op.OperationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Swap")));

        if (bIsSetLike)
        {
            if (Op.SlotIndex >= 0)
            {
                if (!DataProvider->SetSlotItem(Op.SlotIndex, Op.ItemAfter, /*bNotify=*/false))
                {
                    UE_LOG(LogSuspenseCoreEquipmentTransaction, Error,
                        TEXT("ExecuteCommit: SetSlotItem failed (slot=%d)"), Op.SlotIndex);
                    return false;
                }
            }
            continue;
        }

        if (bIsClearLike)
        {
            if (Op.SlotIndex >= 0)
            {
                DataProvider->ClearSlot(Op.SlotIndex, /*bNotify=*/false);
            }
            continue;
        }

        if (bIsSwap)
        {
            // Ожидаем метадату TargetSlot
            int32 TargetSlot = INDEX_NONE;
            if (const FString* V = Op.Metadata.Find(TEXT("TargetSlot")))
            {
                TargetSlot = FCString::Atoi(**V);
            }
            if (Op.SlotIndex >= 0 && TargetSlot >= 0)
            {
                FSuspenseCoreInventoryItemInstance A = DataProvider->GetSlotItem(Op.SlotIndex);
                FSuspenseCoreInventoryItemInstance B = DataProvider->GetSlotItem(TargetSlot);
                DataProvider->SetSlotItem(Op.SlotIndex, B, /*bNotify=*/false);
                DataProvider->SetSlotItem(TargetSlot, A, /*bNotify=*/false);
            }
            continue;
        }

        // Глобальные/прочие
        if (Op.OperationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Global"))))
        {
            // При необходимости — обрабатывайте специфические глобальные действия
            continue;
        }

        // Неизвестные тэги — не валим транзакцию, только логируем (расширяйте по мере добавления типов)
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Verbose,
            TEXT("ExecuteCommit: Unknown op tag %s; skipping"), *Op.OperationType.ToString());
    }

    UE_LOG(LogSuspenseCoreEquipmentTransaction, Verbose,
        TEXT("ExecuteCommit: applied %d ops (notify=false)"), Context.Operations.Num());
    return true;
}

bool USuspenseCoreEquipmentTransactionProcessor::ExecuteRollback(FTransactionExecutionContext& Context)
{
    // IMPORTANT: This method is called WITHOUT holding TransactionLock!
    // We can safely call DataProvider methods
    
    if (!DataProvider.GetInterface())
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Error, 
            TEXT("ExecuteRollback: No data provider"));
        return false;
    }
    
    // Restore initial state
    bool bSuccess = RestoreStateSnapshot(Context.InitialSnapshot);
    
    if (!bSuccess)
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Error, 
            TEXT("ExecuteRollback: Failed to restore initial snapshot"));
        return false;
    }
    
    UE_LOG(LogSuspenseCoreEquipmentTransaction, Verbose, 
        TEXT("ExecuteRollback: Successfully restored initial state"));
    
    return true;
}

bool USuspenseCoreEquipmentTransactionProcessor::ApplyOperation(
    const FTransactionOperation& Operation,
    FEquipmentStateSnapshot& State) const
{
    auto TouchSnapshot = [&State]()
    {
        State.Version++;
        State.Timestamp = FDateTime::Now();
    };

    const bool bIsSetLike =
        Operation.OperationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Set")))   ||
        Operation.OperationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Equip"))) ||
        Operation.OperationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.MoveTarget"))) ||
        Operation.OperationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Upgrade"))) ||
        Operation.OperationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Modify")));

    const bool bIsClearLike =
        Operation.OperationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Clear")))   ||
        Operation.OperationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Unequip"))) ||
        Operation.OperationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Drop")))    ||
        Operation.OperationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.MoveSource")));

    const bool bIsSwap =
        Operation.OperationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Swap")));

    // SET/Equip/MoveTarget
    if (bIsSetLike && Operation.SlotIndex >= 0)
    {
        for (FEquipmentSlotSnapshot& Slot : State.SlotSnapshots)
        {
            if (Slot.SlotIndex == Operation.SlotIndex)
            {
                Slot.ItemInstance = Operation.ItemAfter;
                TouchSnapshot();
                return true;
            }
        }
        // если слот не найден — можно добавить предупреждение; возвращаем false
        return false;
    }

    // CLEAR/Unequip/Drop/MoveSource
    if (bIsClearLike && Operation.SlotIndex >= 0)
    {
        for (FEquipmentSlotSnapshot& Slot : State.SlotSnapshots)
        {
            if (Slot.SlotIndex == Operation.SlotIndex)
            {
                Slot.ItemInstance = FSuspenseCoreInventoryItemInstance(); // пусто
                TouchSnapshot();
                return true;
            }
        }
        return false;
    }

    // SWAP — ожидаем метадату TargetSlot
    if (bIsSwap)
    {
        int32 TargetSlot = INDEX_NONE;
        if (const FString* V = Operation.Metadata.Find(TEXT("TargetSlot")))
        {
            TargetSlot = FCString::Atoi(**V);
        }
        if (Operation.SlotIndex >= 0 && TargetSlot >= 0)
        {
            FEquipmentSlotSnapshot* A = nullptr;
            FEquipmentSlotSnapshot* B = nullptr;

            for (FEquipmentSlotSnapshot& S : State.SlotSnapshots)
            {
                if (S.SlotIndex == Operation.SlotIndex) A = &S;
                if (S.SlotIndex == TargetSlot)          B = &S;
            }

            if (A && B)
            {
                Swap(A->ItemInstance, B->ItemInstance);
                TouchSnapshot();
                return true;
            }
        }
        return false;
    }

    if (Operation.OperationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Global"))))
    {
        // Глобальный «touch»
        TouchSnapshot();
        return true;
    }

    UE_LOG(LogSuspenseCoreEquipmentTransaction, Verbose,
        TEXT("ApplyOperation: unknown tag %s"), *Operation.OperationType.ToString());
    return false;
}

bool USuspenseCoreEquipmentTransactionProcessor::ReverseOperation(const FTransactionOperation& Operation, FEquipmentStateSnapshot& State) const
{
    if (!Operation.bReversible)
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
            TEXT("ReverseOperation: Operation %s is not reversible"), 
            *Operation.OperationId.ToString());
        return false;
    }
    
    // Find and restore slot snapshot
    for (FEquipmentSlotSnapshot& SlotSnapshot : State.SlotSnapshots)
    {
        if (SlotSnapshot.SlotIndex == Operation.SlotIndex)
        {
            SlotSnapshot.ItemInstance = Operation.ItemBefore;
            State.Version++;
            State.Timestamp = FDateTime::Now();
            return true;
        }
    }
    
    return false;
}

FEquipmentStateSnapshot USuspenseCoreEquipmentTransactionProcessor::CaptureStateSnapshot() const
{
    FEquipmentStateSnapshot Snapshot;
    
    if (DataProvider.GetInterface())
    {
        // Use unified API name
        Snapshot = DataProvider->CreateSnapshot();
    }
    
    return Snapshot;
}

bool USuspenseCoreEquipmentTransactionProcessor::RestoreStateSnapshot(const FEquipmentStateSnapshot& Snapshot)
{
    if (!DataProvider.GetInterface())
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Error, 
            TEXT("RestoreStateSnapshot: No data provider"));
        return false;
    }
    
    // Use unified API name
    return DataProvider->RestoreSnapshot(Snapshot);
}

bool USuspenseCoreEquipmentTransactionProcessor::ValidateStateConsistency(const FEquipmentStateSnapshot& State) const
{
    // Basic validation
    if (!State.IsValid())
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
            TEXT("ValidateStateConsistency: Invalid snapshot"));
        return false;
    }
    
    // Check for duplicate instance IDs
    TSet<FGuid> InstanceIds;
    for (const FEquipmentSlotSnapshot& SlotSnapshot : State.SlotSnapshots)
    {
        if (SlotSnapshot.ItemInstance.IsValid())
        {
            if (InstanceIds.Contains(SlotSnapshot.ItemInstance.InstanceID))
            {
                UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
                    TEXT("ValidateStateConsistency: Duplicate instance ID %s found"), 
                    *SlotSnapshot.ItemInstance.InstanceID.ToString());
                return false;
            }
            InstanceIds.Add(SlotSnapshot.ItemInstance.InstanceID);
        }
    }
    
    // Validate slot configurations
    for (const FEquipmentSlotSnapshot& SlotSnapshot : State.SlotSnapshots)
    {
        if (!SlotSnapshot.Configuration.SlotTag.IsValid())
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
                TEXT("ValidateStateConsistency: Invalid slot tag at index %d"), 
                SlotSnapshot.SlotIndex);
            return false;
        }
    }
    
    return true;
}

FTransactionExecutionContext* USuspenseCoreEquipmentTransactionProcessor::FindExecutionContext(const FGuid& TransactionId)
{
    return ActiveTransactions.Find(TransactionId);
}

const FTransactionExecutionContext* USuspenseCoreEquipmentTransactionProcessor::FindExecutionContext(const FGuid& TransactionId) const
{
    return ActiveTransactions.Find(TransactionId);
}

FTransactionSavepoint* USuspenseCoreEquipmentTransactionProcessor::FindSavepoint(
    FTransactionExecutionContext& Context, 
    const FGuid& SavepointId)
{
    for (FTransactionSavepoint& Savepoint : Context.Savepoints)
    {
        if (Savepoint.SavepointId == SavepointId)
        {
            return &Savepoint;
        }
    }
    
    return nullptr;
}

void USuspenseCoreEquipmentTransactionProcessor::CleanupExpiredTransactions()
{
    // Collect expired transactions under lock
    TArray<FGuid> ExpiredTransactions;
    {
        FScopeLock Lock(&TransactionLock);
        
        if (TransactionTimeout <= 0)
        {
            return;
        }
        
        FDateTime Now = FDateTime::Now();
        
        for (const auto& Pair : ActiveTransactions)
        {
            const FTransactionExecutionContext& Context = Pair.Value;
            
            // Skip transactions being processed
            if (Context.TransactionData.State == ETransactionState::Committing ||
                Context.TransactionData.State == ETransactionState::RollingBack)
            {
                continue;
            }
            
            // Check timeout
            FTimespan Duration = Now - Context.TransactionData.StartTime;
            
            if (Duration.GetTotalSeconds() > TransactionTimeout)
            {
                ExpiredTransactions.Add(Pair.Key);
            }
        }
    }
    
    // Rollback expired transactions WITHOUT lock
    for (const FGuid& TransactionId : ExpiredTransactions)
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
            TEXT("CleanupExpiredTransactions: Rolling back expired transaction %s"), 
            *TransactionId.ToString());
        
        RollbackTransaction(TransactionId);
    }
    
    if (ExpiredTransactions.Num() > 0)
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Log, 
            TEXT("CleanupExpiredTransactions: Cleaned up %d expired transactions"), 
            ExpiredTransactions.Num());
    }
}

void USuspenseCoreEquipmentTransactionProcessor::LogTransactionEvent(
    const FGuid& TransactionId, 
    const FString& Event) const
{
    if (bEnableLogging)
    {
        UE_LOG(LogSuspenseCoreEquipmentTransaction, Verbose, 
            TEXT("[Transaction %s] %s"), 
            *TransactionId.ToString().Left(8), 
            *Event);
    }
}

void USuspenseCoreEquipmentTransactionProcessor::NotifyTransactionStateChange(
    const FGuid& TransactionId,
    ETransactionState OldState,
    ETransactionState NewState)
{
    // Log state change
    if (bEnableLogging)
    {
        FString OldStateStr = UEnum::GetValueAsString(OldState);
        FString NewStateStr = UEnum::GetValueAsString(NewState);
        
        LogTransactionEvent(TransactionId, 
            FString::Printf(TEXT("State changed from %s to %s"), 
                *OldStateStr, *NewStateStr));
    }
    
    // Generate state change delta if enabled
    if (bGenerateDeltas && OnTransactionDelta.IsBound())
    {
        FEquipmentDelta StateDelta;
        StateDelta.ChangeType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Delta.StateChange"));
        StateDelta.SlotIndex = INDEX_NONE; // Global change
        StateDelta.ReasonTag = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Reason.StateTransition"));
        StateDelta.SourceTransactionId = TransactionId;
        StateDelta.Timestamp = FDateTime::Now();
        StateDelta.Metadata.Add(TEXT("OldState"), UEnum::GetValueAsString(OldState));
        StateDelta.Metadata.Add(TEXT("NewState"), UEnum::GetValueAsString(NewState));
        
        TArray<FEquipmentDelta> StateDeltas;
        StateDeltas.Add(StateDelta);
        OnTransactionDelta.Execute(StateDeltas);
    }
}

FEquipmentTransaction USuspenseCoreEquipmentTransactionProcessor::ConvertToTransaction(
    const FTransactionExecutionContext& Context) const
{
    FEquipmentTransaction Transaction = Context.TransactionData;
    
    // Copy operations array to transaction (already exists in FEquipmentTransaction)
    Transaction.Operations.Empty();
    for (const FTransactionOperation& Op : Context.Operations)
    {
        FEquipmentOperationRequest Request;
        Request.OperationId = Op.OperationId;
        Request.TargetSlotIndex = Op.SlotIndex;
        Request.ItemInstance = ConvertToItemInstance(Op.ItemAfter);
        Request.Timestamp = Op.Timestamp;
        Request.Parameters = Op.Metadata;
        
        // Determine operation type from tag
        if (Op.OperationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Set"))))
        {
            Request.OperationType = EEquipmentOperationType::Equip;
        }
        else if (Op.OperationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Clear"))))
        {
            Request.OperationType = EEquipmentOperationType::Unequip;
        }
        else if (Op.OperationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Swap"))))
        {
            Request.OperationType = EEquipmentOperationType::Swap;
        }
        else
        {
            Request.OperationType = EEquipmentOperationType::None;
        }
        
        Transaction.Operations.Add(Request);
    }
    
    // Store counts in metadata for debugging if needed
    Transaction.Description = FString::Printf(TEXT("%s [Ops:%d, Saves:%d, Deltas:%d]"),
        *Context.TransactionData.Description,
        Context.Operations.Num(),
        Context.Savepoints.Num(),
        Context.GeneratedDeltas.Num());
    
    return Transaction;
}

bool USuspenseCoreEquipmentTransactionProcessor::CommitTransaction(const FGuid& TransactionId, const TArray<FEquipmentDelta>& Deltas)
{
    // Phase 1: Validate and prepare under lock
    FTransactionExecutionContext ContextCopy;
    bool bCanCommit = false;
    ETransactionState OldState = ETransactionState::None;
    
    {
        FScopeLock Lock(&TransactionLock);
        
        if (!TransactionId.IsValid())
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
                TEXT("CommitTransaction(Deltas): Invalid transaction ID"));
            return false;
        }
        
        // Find transaction
        FTransactionExecutionContext* Context = FindExecutionContext(TransactionId);
        if (!Context)
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
                TEXT("CommitTransaction(Deltas): Transaction %s not found"), 
                *TransactionId.ToString());
            return false;
        }
        
        // Check state
        OldState = Context->TransactionData.State;
        if (Context->TransactionData.State != ETransactionState::Active)
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
                TEXT("CommitTransaction(Deltas): Transaction %s is not active (state: %d)"), 
                *TransactionId.ToString(), (int32)Context->TransactionData.State);
            return false;
        }
        
        // Check if current transaction (must be top of stack for proper nesting)
        if (TransactionStack.Num() > 0 && TransactionStack.Last() != TransactionId)
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
                TEXT("CommitTransaction(Deltas): Transaction %s is not the current transaction"), 
                *TransactionId.ToString());
            return false;
        }
        
        // Validate transaction
        if (!ValidateTransaction_NoLock(TransactionId))
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Error,
                TEXT("CommitTransaction(Deltas): Transaction %s validation failed"),
                *TransactionId.ToString());
            Context->TransactionData.State = ETransactionState::Failed;
            TotalTransactionsFailed++;
            return false;
        }
        
        // Copy context for execution outside lock
        ContextCopy = *Context;
        Context->TransactionData.State = ETransactionState::Committing;
        bCanCommit = true;
        
        // Notify state change
        NotifyTransactionStateChange(TransactionId, OldState, ETransactionState::Committing);
    }
    
    // Phase 2: Execute commit WITHOUT lock
    bool bSuccess = false;
    FEquipmentStateSnapshot AfterSnapshot;
    
    if (bCanCommit)
    {
        bSuccess = ExecuteCommit(ContextCopy);
        if (bSuccess)
        {
            // Capture state after commit
            AfterSnapshot = CaptureStateSnapshot();
        }
    }
    
    // Phase 3: Update state and publish deltas under lock
    {
        FScopeLock Lock(&TransactionLock);
        
        FTransactionExecutionContext* Context = FindExecutionContext(TransactionId);
        if (!Context)
        {
            // Transaction was removed while committing
            return false;
        }
        
        if (bSuccess)
        {
            Context->TransactionData.State = ETransactionState::Committed;
            Context->TransactionData.bIsCommitted = true;
            Context->TransactionData.EndTime = FDateTime::Now();
            Context->TransactionData.StateAfter = AfterSnapshot;
            
            // Store provided deltas instead of generating them
            Context->GeneratedDeltas = Deltas;
            
            // Remove from stack
            TransactionStack.Remove(TransactionId);
            
            // Move to history
            if (TransactionHistory.Num() >= MaxHistorySize)
            {
                TransactionHistory.RemoveAt(0);
            }
            TransactionHistory.Add(Context->TransactionData);
            
            // Remove from active
            ActiveTransactions.Remove(TransactionId);
            
            // Update statistics
            TotalTransactionsCommitted++;
            TotalDeltasGenerated += Deltas.Num();
            
            if (bEnableLogging)
            {
                LogTransactionEvent(TransactionId, 
                    FString::Printf(TEXT("Transaction committed with %d explicit deltas (%d operations)"),
                        Deltas.Num(), Context->Operations.Num()));
            }
            
            // Notify state change
            NotifyTransactionStateChange(TransactionId, ETransactionState::Committing, ETransactionState::Committed);
        }
        else
        {
            Context->TransactionData.State = ETransactionState::Failed;
            TotalTransactionsFailed++;
            
            // Auto-recovery attempt
            if (bAutoRecovery)
            {
                UE_LOG(LogSuspenseCoreEquipmentTransaction, Warning, 
                    TEXT("CommitTransaction(Deltas): Attempting auto-recovery for transaction %s"), 
                    *TransactionId.ToString());
                
                // Remove from stack and active
                TransactionStack.Remove(TransactionId);
                ActiveTransactions.Remove(TransactionId);
            }
            
            // Notify state change
            NotifyTransactionStateChange(TransactionId, ETransactionState::Committing, ETransactionState::Failed);
        }
    }
    
    // Phase 4: Clear active transaction on DataStore WITHOUT lock
    if (USuspenseCoreEquipmentDataStore* DataStore = Cast<USuspenseCoreEquipmentDataStore>(DataProvider.GetObject()))
    {
        DataStore->ClearActiveTransactionIfMatches(TransactionId);
    }
    
    // Phase 5: Broadcast deltas if successful - КРИТИЧЕСКИ ВАЖНО: публикуем ровно один раз!
    if (bSuccess && OnTransactionDelta.IsBound() && Deltas.Num() > 0)
    {
        double DeltaPublishStartTime = FPlatformTime::Seconds();
        
        // Единственная точка публикации дельт в системе
        OnTransactionDelta.Execute(Deltas);
        
        double DeltaPublishMs = (FPlatformTime::Seconds() - DeltaPublishStartTime) * 1000.0;
        
        if (bEnableLogging)
        {
            UE_LOG(LogSuspenseCoreEquipmentTransaction, Verbose,
                TEXT("CommitTransaction(Deltas): Published %d deltas in %.2fms"),
                Deltas.Num(), DeltaPublishMs);
        }
        
        // Record metrics
        // ServiceMetrics.AddDurationMs(TEXT("DeltaPublishMs"), DeltaPublishMs);
        // ServiceMetrics.RecordValue(TEXT("DeltasPerTransaction"), Deltas.Num());
    }
    
    return bSuccess;
}

//========================================
// DIFF/Delta Generation
//========================================

TArray<FEquipmentDelta> USuspenseCoreEquipmentTransactionProcessor::GenerateDeltasFromTransaction(
    const FTransactionExecutionContext& Context) const
{
    TArray<FEquipmentDelta> Deltas;
    
    for (const FTransactionOperation& Op : Context.Operations)
    {
        FEquipmentDelta Delta = CreateDeltaFromOperation(Op, Context.TransactionData.TransactionId);
        Deltas.Add(Delta);
    }
    
    return Deltas;
}

FEquipmentDelta USuspenseCoreEquipmentTransactionProcessor::CreateDeltaFromOperation(
    const FTransactionOperation& Operation, 
    const FGuid& TransactionId) const
{
    FEquipmentDelta Delta;
    
    // Set basic delta properties
    Delta.ChangeType = Operation.OperationType;
    Delta.SlotIndex = Operation.SlotIndex;
    Delta.ItemBefore = Operation.ItemBefore;
    Delta.ItemAfter = Operation.ItemAfter;
    
    // Use operation type as reason if no specific reason is provided
    // Check metadata for custom reason tag
    if (Operation.Metadata.Contains(TEXT("ReasonTag")))
    {
        Delta.ReasonTag = FGameplayTag::RequestGameplayTag(FName(*Operation.Metadata[TEXT("ReasonTag")]));
    }
    else
    {
        // Default reason based on operation type
        Delta.ReasonTag = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Reason.Transaction"));
    }
    
    Delta.SourceTransactionId = TransactionId;
    Delta.OperationId = Operation.OperationId;
    Delta.Timestamp = FDateTime::Now();
    Delta.Metadata = Operation.Metadata;
    
    return Delta;
}

//========================================
// Private Helper Methods
//========================================

TArray<FTransactionOperation> USuspenseCoreEquipmentTransactionProcessor::CheckForConflicts_NoLock(
    const FGuid& TransactionId) const
{
    TArray<FTransactionOperation> Conflicts;
    
    const FTransactionExecutionContext* Context = ActiveTransactions.Find(TransactionId);
    if (!Context)
    {
        return Conflicts;
    }
    
    // Check conflicts with other active transactions
    for (const auto& Pair : ActiveTransactions)
    {
        if (Pair.Key == TransactionId)
        {
            continue;
        }
        
        const FTransactionExecutionContext& OtherContext = Pair.Value;
        
        // Skip non-active transactions
        if (OtherContext.TransactionData.State != ETransactionState::Active)
        {
            continue;
        }
        
        // Check slot conflicts
        for (const FTransactionOperation& Op : Context->Operations)
        {
            for (const FTransactionOperation& OtherOp : OtherContext.Operations)
            {
                if (Op.SlotIndex == OtherOp.SlotIndex && Op.SlotIndex != INDEX_NONE)
                {
                    // Same slot modified in both transactions
                    Conflicts.Add(Op);
                    break;
                }
            }
        }
    }
    
    return Conflicts;
}

bool USuspenseCoreEquipmentTransactionProcessor::ValidateTransaction_NoLock(const FGuid& TransactionId) const
{
    const FTransactionExecutionContext* Context = FindExecutionContext(TransactionId);
    if (!Context)
    {
        return false;
    }
    
    // Check transaction state
    if (Context->TransactionData.State != ETransactionState::Active)
    {
        return false;
    }
    
    // Check state consistency
    if (!ValidateStateConsistency(Context->CurrentSnapshot))
    {
        return false;
    }
    
    // Check timeout
    if (TransactionTimeout > 0)
    {
        const FTimespan Duration = FDateTime::Now() - Context->TransactionData.StartTime;
        if (Duration.GetTotalSeconds() > TransactionTimeout)
        {
            return false;
        }
    }
    
    // Check operations validity
    for (const FTransactionOperation& Op : Context->Operations)
    {
        if (!Op.OperationId.IsValid())
        {
            return false;
        }
    }
    
    // Check for conflicts - we're already under lock
    const TArray<FTransactionOperation> Conflicts = CheckForConflicts_NoLock(TransactionId);
    
    return Conflicts.Num() == 0;
}