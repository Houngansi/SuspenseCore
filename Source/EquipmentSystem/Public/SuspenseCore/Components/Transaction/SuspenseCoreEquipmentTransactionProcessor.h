// SuspenseCoreEquipmentTransactionProcessor.h
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreTransactionManager.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentDataProvider.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "SuspenseCore/Types/Equipment/SuspenseCoreEquipmentTypes.h"
#include "SuspenseCore/Types/Transaction/SuspenseCoreTransactionTypes.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreEquipmentTransactionProcessor.generated.h"

/**
 * Transaction savepoint
 */
USTRUCT()
struct FSuspenseCoreTransactionSavepoint
{
    GENERATED_BODY()

    /** Savepoint ID */
    UPROPERTY()
    FGuid SavepointId;

    /** Savepoint name */
    UPROPERTY()
    FString Name;

    /** State snapshot at savepoint */
    UPROPERTY()
    FEquipmentStateSnapshot Snapshot;

    /** Creation timestamp */
    UPROPERTY()
    FDateTime CreationTime;

    /** Operation index at savepoint */
    UPROPERTY()
    int32 OperationIndex = 0;
};

/**
 * Internal transaction execution context
 * This is our internal representation with more details than FEquipmentTransaction
 */
USTRUCT()
struct FSuspenseCoreTransactionExecutionContext
{
    GENERATED_BODY()

    /** Transaction data (public interface) */
    UPROPERTY()
    FEquipmentTransaction TransactionData;

    /** Initial state snapshot */
    UPROPERTY()
    FEquipmentStateSnapshot InitialSnapshot;

    /** Current state snapshot */
    UPROPERTY()
    FEquipmentStateSnapshot CurrentSnapshot;

    /** Operations in this transaction */
    UPROPERTY()
    TArray<FSuspenseCoreTransactionOperation> Operations;

    /** Savepoints in this transaction */
    UPROPERTY()
    TArray<FSuspenseCoreTransactionSavepoint> Savepoints;

    /** Transaction metadata */
    UPROPERTY()
    TMap<FString, FString> Metadata;

    /** Is transaction read-only */
    UPROPERTY()
    bool bReadOnly = false;

    /** Transaction isolation level */
    UPROPERTY()
    int32 IsolationLevel = 0;

    /** Generated deltas during this transaction */
    UPROPERTY()
    TArray<FSuspenseCoreEquipmentDelta> GeneratedDeltas;
};

/**
 * Transaction validation result
 */
USTRUCT()
struct FSuspenseCoreTransactionValidationResult
{
    GENERATED_BODY()

    /** Is transaction valid */
    UPROPERTY()
    bool bIsValid = false;

    /** Validation errors */
    UPROPERTY()
    TArray<FText> Errors;

    /** Validation warnings */
    UPROPERTY()
    TArray<FText> Warnings;

    /** Conflicting operations */
    UPROPERTY()
    TArray<FSuspenseCoreTransactionOperation> Conflicts;
};

/** Delegate for transaction delta notifications */
DECLARE_DELEGATE_OneParam(FOnTransactionDelta, const TArray<FEquipmentDelta>&);

/**
 * Equipment Transaction Processor Component
 *
 * Philosophy: Guarantees atomicity of operations through Unit of Work pattern.
 * Provides transaction management, rollback capability, and recovery mechanisms.
 *
 * Key Design Principles:
 * - ACID compliance (Atomicity, Consistency, Isolation, Durability)
 * - Nested transaction support with savepoints
 * - Automatic conflict detection and resolution
 * - Comprehensive audit trail
 * - Recovery from partial failures
 * - Thread-safe transaction management
 * - Optimistic concurrency control
 * - DIFF-based change tracking for all operations
 *
 * =================================================================
 * CRITICAL LOCKING CONTRACT (MANDATORY):
 * =================================================================
 *
 * 1. LOCK HIERARCHY:
 *    TransactionLock (this class) → DataProvider → DataCriticalSection (DataStore)
 *
 *    RULE: When holding TransactionLock and needing to call DataProvider,
 *    we MUST release TransactionLock first to avoid deadlock.
 *
 * 2. FORBIDDEN PATTERNS:
 *    - NO: holding TransactionLock and calling DataProvider methods
 *    - NO: unlock → external call → lock in the middle of an operation
 *    - NO: recursive acquisition of TransactionLock
 *
 * 3. BATCH OPERATIONS:
 *    In CommitAllTransactions/RollbackAllTransactions:
 *    - Collect transaction list under lock
 *    - Release lock COMPLETELY
 *    - Process operations one by one
 *    - Each operation takes lock independently
 *
 * 4. GUARANTEES:
 *    - No partial states on failures
 *    - Atomicity at individual transaction level
 *    - Isolation between parallel transactions
 *
 * 5. NOTIFICATIONS:
 *    - ExecuteCommit calls SetSlotItem with bNotify=false
 *    - Events are sent by DataServiceImpl after commit
 *    - This avoids double notifications
 * =================================================================
 */
UCLASS(ClassGroup=(Equipment), meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentTransactionProcessor : public UActorComponent, public ISuspenseCoreTransactionManager
{
    GENERATED_BODY()

public:
    USuspenseCoreEquipmentTransactionProcessor();
    virtual ~USuspenseCoreEquipmentTransactionProcessor();

    //~ Begin UActorComponent Interface
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    //~ End UActorComponent Interface

    //========================================
    // ISuspenseCoreTransactionManager Implementation
    //========================================

    virtual FGuid BeginTransaction(const FString& Description = TEXT("")) override;
    virtual bool CommitTransaction(const FGuid& TransactionId) override;
    virtual bool RollbackTransaction(const FGuid& TransactionId) override;
    virtual FGuid CreateSavepoint(const FString& SavepointName) override;
    virtual bool RollbackToSavepoint(const FGuid& SavepointId) override;
    virtual FEquipmentTransaction GetCurrentTransaction() const override;
    virtual bool IsTransactionActive() const override;
    virtual FEquipmentTransaction GetTransaction(const FGuid& TransactionId) const override;
    virtual FGuid BeginNestedTransaction(const FString& Description = TEXT("")) override;
    virtual bool RegisterOperation(const FGuid& OperationId) override;
    virtual bool ValidateTransaction(const FGuid& TransactionId) const override;
    virtual TArray<FEquipmentTransaction> GetTransactionHistory(int32 MaxCount = 10) const override;

    // ===== РАСШИРЕННОЕ API ДЛЯ ОПЕРАЦИЙ =====
    virtual bool SupportsExtendedOps() const override;
    virtual bool RegisterOperation(const FGuid& TransactionId, const FTransactionOperation& Operation) override;
    virtual bool ApplyOperation(const FGuid& TransactionId, const FTransactionOperation& Operation) override;
    virtual TArray<FEquipmentDelta> GetTransactionDeltas(const FGuid& TransactionId) const override;

    //========================================
    // Extended Transaction Management
    //========================================

    /**
     * Commit all pending transactions
     * Collects transaction list under lock, then processes WITHOUT lock
     * @return Number of committed transactions
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Transaction")
    int32 CommitAllTransactions();

    /**
     * Rollback all pending transactions
     * Collects transaction list under lock, then processes WITHOUT lock
     * @return Number of rolled back transactions
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Transaction")
    int32 RollbackAllTransactions();

    /**
     * Record operation with details
     * @param Operation Operation to record
     * @return Operation ID
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Transaction")
    FGuid RecordDetailedOperation(const FSuspenseCoreTransactionOperation& Operation);

    /**
     * Release savepoint
     * @param SavepointId Savepoint to release
     * @return True if released
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Transaction")
    bool ReleaseSavepoint(const FGuid& SavepointId);

    /**
     * Clear transaction history
     * @param bKeepActive Keep active transactions
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Transaction")
    void ClearTransactionHistory(bool bKeepActive = true);

    /**
     * Get current transaction ID if active
     * @return Current transaction ID or invalid GUID
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Transaction")
    FGuid GetCurrentTransactionId() const;

    //========================================
    // Delta Notification
    //========================================

    /** Set delta notification callback */
    void SetDeltaCallback(FOnTransactionDelta InCallback) { OnTransactionDelta = InCallback; }

    /** Clear delta notification callback */
    void ClearDeltaCallback() { OnTransactionDelta.Unbind(); }

    //========================================
    // Recovery and Validation
    //========================================

    /**
     * Recover from failed transaction
     * @param TransactionId Transaction to recover
     * @return True if recovered successfully
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Recovery")
    bool RecoverTransaction(const FGuid& TransactionId);

    /**
     * Validate transaction integrity
     * @param TransactionId Transaction to validate
     * @return Validation result
     */
    //UFUNCTION(BlueprintCallable, Category = "Equipment|Validation")
    FSuspenseCoreTransactionValidationResult ValidateTransactionIntegrity(const FGuid& TransactionId) const;

    /**
     * Check for transaction conflicts
     * @param TransactionId Transaction to check
     * @return Array of conflicting operations
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Validation")
    TArray<FSuspenseCoreTransactionOperation> CheckForConflicts(const FGuid& TransactionId) const;

    /**
     * Resolve transaction conflicts
     * @param TransactionId Transaction with conflicts
     * @param ResolutionStrategy Strategy to use (0=Abort, 1=Retry, 2=Force)
     * @return True if resolved
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Recovery")
    bool ResolveConflicts(const FGuid& TransactionId, int32 ResolutionStrategy = 0);

    //========================================
    // Configuration
    //========================================

    /**
     * Initialize processor with data provider
     * @param InDataProvider Data provider interface
     * @return True if initialized
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Configuration")
    bool Initialize(TScriptInterface<ISuspenseCoreEquipmentDataProvider> InDataProvider);

    /**
     * Get data provider
     * @return Current data provider
     */
    ISuspenseCoreEquipmentDataProvider* GetDataProvider() const { return DataProvider.GetInterface(); }

    /**
     * Set transaction timeout
     * @param Seconds Timeout in seconds
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Configuration")
    void SetTransactionTimeout(float Seconds);

    /**
     * Get transaction timeout
     * @return Timeout in seconds
     */
    float GetTransactionTimeout() const { return TransactionTimeout; }

    /**
     * Set maximum nested transaction depth
     * @param MaxDepth Maximum nesting depth
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Configuration")
    void SetMaxNestedDepth(int32 MaxDepth);

    /**
     * Get maximum nested depth
     * @return Maximum depth
     */
    int32 GetMaxNestedDepth() const { return MaxNestedDepth; }

    /**
     * Enable/disable auto-recovery
     * @param bEnable Enable state
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Configuration")
    void SetAutoRecovery(bool bEnable);

    //========================================
    // Statistics and Debugging
    //========================================

    /**
     * Get transaction statistics
     * @return Statistics as string
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Debug")
    FString GetTransactionStatistics() const;

    /**
     * Get active transaction count
     * @return Number of active transactions
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Debug")
    int32 GetActiveTransactionCount() const;

    /**
     * Dump transaction state for debugging
     * @param TransactionId Transaction to dump
     * @return State as string
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Debug")
    FString DumpTransactionState(const FGuid& TransactionId) const;

   /**
  * Commits transaction with explicit delta publication.
  * This is the NEW recommended way to commit transactions with controlled event publication.
  * @param TransactionId Transaction to commit
  * @param Deltas Array of deltas to publish after successful commit
  * @return true if committed successfully, false otherwise
  */
   bool CommitTransaction(const FGuid& TransactionId, const TArray<FEquipmentDelta>& Deltas);

protected:
    //========================================
    // Internal Transaction Operations
    //========================================

    /**
     * Create transaction execution context
     * @param TransactionId Transaction ID
     * @param Description Description
     * @param ParentId Parent transaction ID
     * @return New context
     */
    FSuspenseCoreTransactionExecutionContext CreateExecutionContext(
        const FGuid& TransactionId,
        const FString& Description,
        const FGuid& ParentId = FGuid());

    /**
     * Execute commit for transaction
     * IMPORTANT: Called WITHOUT holding TransactionLock!
     * IMPORTANT: Uses bNotify=false to avoid double notifications!
     * @param Context Transaction context
     * @return True if committed
     */
    bool ExecuteCommit(FSuspenseCoreTransactionExecutionContext& Context);

    /**
     * Execute rollback for transaction
     * IMPORTANT: Called WITHOUT holding TransactionLock!
     * @param Context Transaction context
     * @return True if rolled back
     */
    bool ExecuteRollback(FSuspenseCoreTransactionExecutionContext& Context);

    /**
     * Generate deltas from transaction
     * @param Context Transaction context
     * @return Array of deltas
     */
    TArray<FEquipmentDelta> GenerateDeltasFromTransaction(const FSuspenseCoreTransactionExecutionContext& Context) const;

    /**
     * Apply operation to state
     * @param Operation Operation to apply
     * @param State State to modify
     * @return True if applied
     */
    bool ApplyOperation(const FTransactionOperation& Operation, FEquipmentStateSnapshot& State) const;

    /**
     * Reverse operation
     * @param Operation Operation to reverse
     * @param State State to modify
     * @return True if reversed
     */
    bool ReverseOperation(const FTransactionOperation& Operation, FEquipmentStateSnapshot& State) const;

    /**
     * Capture current state snapshot
     * @return Current state
     */
    FEquipmentStateSnapshot CaptureStateSnapshot() const;

    /**
     * Restore state from snapshot
     * @param Snapshot Snapshot to restore
     * @return True if restored
     */
    bool RestoreStateSnapshot(const FEquipmentStateSnapshot& Snapshot);

    /**
     * Validate state consistency
     * @param State State to validate
     * @return True if consistent
     */
    bool ValidateStateConsistency(const FEquipmentStateSnapshot& State) const;

    /**
     * Find transaction execution context
     * @param TransactionId Transaction ID
     * @return Context pointer or nullptr
     */
    FSuspenseCoreTransactionExecutionContext* FindExecutionContext(const FGuid& TransactionId);
    const FSuspenseCoreTransactionExecutionContext* FindExecutionContext(const FGuid& TransactionId) const;

    /**
     * Find savepoint in transaction
     * @param Context Transaction context
     * @param SavepointId Savepoint ID
     * @return Savepoint pointer or nullptr
     */
    FSuspenseCoreTransactionSavepoint* FindSavepoint(FSuspenseCoreTransactionExecutionContext& Context, const FGuid& SavepointId);

    /**
     * Cleanup expired transactions
     */
    void CleanupExpiredTransactions();

    /**
     * Log transaction event
     * @param TransactionId Transaction ID
     * @param Event Event description
     */
    void LogTransactionEvent(const FGuid& TransactionId, const FString& Event) const;

    /**
     * Notify transaction state change
     * @param TransactionId Transaction ID
     * @param OldState Previous state
     * @param NewState New state
     */
    void NotifyTransactionStateChange(
        const FGuid& TransactionId,
        ETransactionState OldState,
        ETransactionState NewState);

    /**
     * Convert execution context to transaction
     * @param Context Execution context
     * @return Transaction data
     */
    FEquipmentTransaction ConvertToTransaction(const FSuspenseCoreTransactionExecutionContext& Context) const;

private:
    //========================================
    // Transaction Storage
    //========================================

    /** Active transactions */
    UPROPERTY()
    TMap<FGuid, FSuspenseCoreTransactionExecutionContext> ActiveTransactions;

    /** Transaction history */
    UPROPERTY()
    TArray<FEquipmentTransaction> TransactionHistory;

    /** Current transaction stack (for nested transactions) */
    TArray<FGuid> TransactionStack;

    /** Savepoint registry */
    UPROPERTY()
    TMap<FGuid, FGuid> SavepointToTransaction;

    /** Transaction lock for thread safety */
    mutable FCriticalSection TransactionLock;

    /** Delta notification callback */
    FOnTransactionDelta OnTransactionDelta;

    //========================================
    // Configuration
    //========================================

    /** Data provider interface */
    UPROPERTY()
    TScriptInterface<ISuspenseCoreEquipmentDataProvider> DataProvider;

    /** Transaction timeout in seconds */
    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    float TransactionTimeout = 30.0f;

    /** Maximum nested transaction depth */
    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    int32 MaxNestedDepth = 5;

    /** Maximum transaction history size */
    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    int32 MaxHistorySize = 100;

    /** Enable auto-recovery */
    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    bool bAutoRecovery = true;

    /** Enable transaction logging */
    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    bool bEnableLogging = true;

    /** Enable delta generation */
    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    bool bGenerateDeltas = true;

    /** Cleanup interval in seconds */
    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    float CleanupInterval = 60.0f;

    //========================================
    // Statistics
    //========================================

    /** Total transactions started */
    UPROPERTY()
    int32 TotalTransactionsStarted = 0;

    /** Total transactions committed */
    UPROPERTY()
    int32 TotalTransactionsCommitted = 0;

    /** Total transactions rolled back */
    UPROPERTY()
    int32 TotalTransactionsRolledBack = 0;

    /** Total transactions failed */
    UPROPERTY()
    int32 TotalTransactionsFailed = 0;

    /** Total operations processed */
    UPROPERTY()
    int32 TotalOperationsProcessed = 0;

    /** Total conflicts resolved */
    UPROPERTY()
    int32 TotalConflictsResolved = 0;

    /** Total deltas generated */
    UPROPERTY()
    int32 TotalDeltasGenerated = 0;

    /** Last cleanup time */
    float LastCleanupTime = 0.0f;

    //========================================
    // State Management
    //========================================

    /** Is processor initialized */
    UPROPERTY()
    bool bIsInitialized = false;

    /** Is currently processing transaction */
    UPROPERTY()
    bool bIsProcessing = false;

    /** Processor version for compatibility */
    UPROPERTY()
    int32 ProcessorVersion = 1;

    //========================================
    // Private Helper Methods
    //========================================

    /** Internal conflict check that assumes TransactionLock is already held */
    TArray<FTransactionOperation> CheckForConflicts_NoLock(const FGuid& TransactionId) const;

   /** Internal validation that assumes TransactionLock is already held */
   bool ValidateTransaction_NoLock(const FGuid& TransactionId) const;

   /** Create delta from operation */
   FEquipmentDelta CreateDeltaFromOperation(const FTransactionOperation& Operation, const FGuid& TransactionId) const;
};
