// SuspenseCoreInventoryTransaction.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryOperationTypes.h"
#include "SuspenseCoreInventoryTransaction.generated.h"

// Forward declarations
class USuspenseCoreInventoryComponent;

/**
 * ESuspenseCoreTransactionState
 *
 * Transaction state machine states.
 */
UENUM(BlueprintType)
enum class ESuspenseCoreTransactionState : uint8
{
	None = 0			UMETA(DisplayName = "None"),
	Active				UMETA(DisplayName = "Active"),
	Committed			UMETA(DisplayName = "Committed"),
	RolledBack			UMETA(DisplayName = "Rolled Back"),
	Failed				UMETA(DisplayName = "Failed")
};

/**
 * FSuspenseCoreTransactionEntry
 *
 * Single entry in transaction log.
 */
USTRUCT(BlueprintType)
struct INVENTORYSYSTEM_API FSuspenseCoreTransactionEntry
{
	GENERATED_BODY()

	/** Operation type */
	UPROPERTY(BlueprintReadOnly, Category = "Transaction")
	ESuspenseCoreOperationType OperationType;

	/** Item ID affected */
	UPROPERTY(BlueprintReadOnly, Category = "Transaction")
	FName ItemID;

	/** Instance ID affected */
	UPROPERTY(BlueprintReadOnly, Category = "Transaction")
	FGuid InstanceID;

	/** Snapshot of item before operation */
	UPROPERTY(BlueprintReadOnly, Category = "Transaction")
	FSuspenseCoreItemInstance BeforeState;

	/** Snapshot of item after operation */
	UPROPERTY(BlueprintReadOnly, Category = "Transaction")
	FSuspenseCoreItemInstance AfterState;

	/** Operation timestamp */
	UPROPERTY(BlueprintReadOnly, Category = "Transaction")
	float Timestamp;

	FSuspenseCoreTransactionEntry()
		: OperationType(ESuspenseCoreOperationType::None)
		, ItemID(NAME_None)
		, Timestamp(0.0f)
	{
	}
};

/**
 * USuspenseCoreInventoryTransaction
 *
 * Transaction manager for atomic inventory operations.
 * Supports commit/rollback for complex multi-step operations.
 *
 * ARCHITECTURE:
 * - Captures inventory state on begin
 * - Logs all operations during transaction
 * - Rollback reverts to captured state
 * - Commit finalizes all changes
 *
 * USAGE:
 * Transaction->Begin(Inventory);
 * // Perform operations...
 * if (AllSucceeded)
 *     Transaction->Commit();
 * else
 *     Transaction->Rollback();
 */
UCLASS(BlueprintType)
class INVENTORYSYSTEM_API USuspenseCoreInventoryTransaction : public UObject
{
	GENERATED_BODY()

public:
	USuspenseCoreInventoryTransaction();

	//==================================================================
	// Transaction Lifecycle
	//==================================================================

	/**
	 * Begin new transaction.
	 * Captures current inventory state.
	 * @param Inventory Target inventory
	 * @return true if transaction started
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Transaction")
	bool Begin(USuspenseCoreInventoryComponent* Inventory);

	/**
	 * Commit transaction.
	 * Finalizes all changes, broadcasts events.
	 * @return true if committed successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Transaction")
	bool Commit();

	/**
	 * Rollback transaction.
	 * Reverts inventory to state at Begin().
	 * @return true if rolled back successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Transaction")
	bool Rollback();

	/**
	 * Cancel transaction without applying changes.
	 * Same as Rollback but doesn't restore state.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Transaction")
	void Cancel();

	//==================================================================
	// Operation Logging
	//==================================================================

	/**
	 * Log operation in transaction.
	 * Called by inventory component during operations.
	 * @param Entry Operation entry
	 */
	void LogOperation(const FSuspenseCoreTransactionEntry& Entry);

	/**
	 * Log add operation.
	 * @param ItemID Item added
	 * @param Instance Instance added
	 */
	void LogAdd(FName ItemID, const FSuspenseCoreItemInstance& Instance);

	/**
	 * Log remove operation.
	 * @param ItemID Item removed
	 * @param Instance Instance before removal
	 */
	void LogRemove(FName ItemID, const FSuspenseCoreItemInstance& Instance);

	/**
	 * Log move operation.
	 * @param Instance Instance before move
	 * @param NewSlot New slot index
	 */
	void LogMove(const FSuspenseCoreItemInstance& Instance, int32 NewSlot);

	/**
	 * Log rotate operation.
	 * @param Instance Instance before rotation
	 * @param NewRotation New rotation value
	 */
	void LogRotate(const FSuspenseCoreItemInstance& Instance, int32 NewRotation);

	//==================================================================
	// State Query
	//==================================================================

	/**
	 * Check if transaction is active.
	 * @return true if in active state
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Transaction")
	bool IsActive() const { return State == ESuspenseCoreTransactionState::Active; }

	/**
	 * Get transaction state.
	 * @return Current state
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Transaction")
	ESuspenseCoreTransactionState GetState() const { return State; }

	/**
	 * Get transaction ID.
	 * @return Unique transaction ID
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Transaction")
	FGuid GetTransactionID() const { return TransactionID; }

	/**
	 * Get operation count.
	 * @return Number of logged operations
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Transaction")
	int32 GetOperationCount() const { return OperationLog.Num(); }

	/**
	 * Get operation log.
	 * @return Array of operation entries
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Transaction")
	const TArray<FSuspenseCoreTransactionEntry>& GetOperationLog() const { return OperationLog; }

	/**
	 * Get start timestamp.
	 * @return When transaction began
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Transaction")
	float GetStartTime() const { return StartTime; }

	/**
	 * Get elapsed time.
	 * @return Seconds since transaction began
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Transaction")
	float GetElapsedTime() const;

	//==================================================================
	// Debug
	//==================================================================

	/**
	 * Get debug string.
	 * @return Transaction debug info
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Transaction")
	FString GetDebugString() const;

protected:
	/** Transaction state */
	UPROPERTY()
	ESuspenseCoreTransactionState State;

	/** Unique transaction ID */
	UPROPERTY()
	FGuid TransactionID;

	/** Target inventory component */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreInventoryComponent> TargetInventory;

	/** Snapshot of inventory at transaction start */
	UPROPERTY()
	FSuspenseCoreInventorySnapshot BeginSnapshot;

	/** Operation log */
	UPROPERTY()
	TArray<FSuspenseCoreTransactionEntry> OperationLog;

	/** Transaction start time */
	float StartTime;

	/** Apply snapshot to inventory */
	bool ApplySnapshot(const FSuspenseCoreInventorySnapshot& Snapshot);

	/** Capture current inventory state */
	FSuspenseCoreInventorySnapshot CaptureSnapshot() const;
};

/**
 * FSuspenseCoreTransactionScope
 *
 * RAII-style transaction scope.
 * Automatically commits on success, rolls back on failure.
 *
 * USAGE:
 * {
 *     FSuspenseCoreTransactionScope Scope(Inventory);
 *     // Operations...
 *     Scope.MarkSuccess(); // Must call for commit
 * } // Auto-commits if marked success, else rolls back
 */
struct INVENTORYSYSTEM_API FSuspenseCoreTransactionScope
{
public:
	FSuspenseCoreTransactionScope(USuspenseCoreInventoryComponent* Inventory);
	~FSuspenseCoreTransactionScope();

	/** Mark transaction as successful. Must call for commit. */
	void MarkSuccess() { bSuccess = true; }

	/** Check if transaction is active */
	bool IsActive() const { return Transaction != nullptr && Transaction->IsActive(); }

	/** Get underlying transaction */
	USuspenseCoreInventoryTransaction* GetTransaction() const { return Transaction; }

private:
	USuspenseCoreInventoryTransaction* Transaction;
	bool bSuccess;
};
