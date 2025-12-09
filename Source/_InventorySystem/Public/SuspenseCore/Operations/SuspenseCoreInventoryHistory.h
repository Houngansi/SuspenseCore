// SuspenseCoreInventoryHistory.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryOperationTypes.h"
#include "SuspenseCoreInventoryHistory.generated.h"

// Forward declarations
class USuspenseCoreInventoryComponent;

/**
 * USuspenseCoreInventoryHistory
 *
 * Tracks history of inventory operations for undo/redo.
 * Maintains a fixed-size circular buffer of operations.
 *
 * ARCHITECTURE:
 * - Records all operations with before/after state
 * - Supports multi-step undo/redo
 * - Automatic cleanup of old entries
 * - EventBus integration for notifications
 *
 * USAGE:
 * History->RecordOperation(Record);
 * if (History->CanUndo())
 *     History->Undo();
 */
UCLASS(BlueprintType)
class INVENTORYSYSTEM_API USuspenseCoreInventoryHistory : public UObject
{
	GENERATED_BODY()

public:
	USuspenseCoreInventoryHistory();

	//==================================================================
	// Configuration
	//==================================================================

	/**
	 * Initialize with inventory component.
	 * @param Inventory Target inventory
	 * @param MaxHistorySize Maximum operations to keep
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|History")
	void Initialize(USuspenseCoreInventoryComponent* Inventory, int32 MaxHistorySize = 50);

	/**
	 * Set maximum history size.
	 * @param NewSize New maximum (oldest entries removed if exceeded)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|History")
	void SetMaxHistorySize(int32 NewSize);

	/**
	 * Get maximum history size.
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|History")
	int32 GetMaxHistorySize() const { return MaxSize; }

	//==================================================================
	// Recording
	//==================================================================

	/**
	 * Record operation in history.
	 * @param Record Operation record
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|History")
	void RecordOperation(const FSuspenseCoreOperationRecord& Record);

	/**
	 * Record add operation.
	 * @param ItemID Item added
	 * @param InstanceID Instance ID
	 * @param Slot Slot index
	 * @param Quantity Amount
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|History")
	void RecordAdd(FName ItemID, const FGuid& InstanceID, int32 Slot, int32 Quantity);

	/**
	 * Record remove operation.
	 * @param ItemID Item removed
	 * @param InstanceID Instance ID
	 * @param Slot Previous slot
	 * @param Quantity Amount
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|History")
	void RecordRemove(FName ItemID, const FGuid& InstanceID, int32 Slot, int32 Quantity);

	/**
	 * Record move operation.
	 * @param InstanceID Instance moved
	 * @param FromSlot Source slot
	 * @param ToSlot Target slot
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|History")
	void RecordMove(const FGuid& InstanceID, int32 FromSlot, int32 ToSlot);

	/**
	 * Record swap operation.
	 * @param InstanceID1 First instance
	 * @param InstanceID2 Second instance
	 * @param Slot1 First slot
	 * @param Slot2 Second slot
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|History")
	void RecordSwap(const FGuid& InstanceID1, const FGuid& InstanceID2, int32 Slot1, int32 Slot2);

	/**
	 * Record rotate operation.
	 * @param InstanceID Instance rotated
	 * @param Slot Slot index
	 * @param OldRotation Previous rotation
	 * @param NewRotation New rotation
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|History")
	void RecordRotate(const FGuid& InstanceID, int32 Slot, int32 OldRotation, int32 NewRotation);

	//==================================================================
	// Undo/Redo
	//==================================================================

	/**
	 * Check if can undo.
	 * @return true if undo available
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|History")
	bool CanUndo() const;

	/**
	 * Check if can redo.
	 * @return true if redo available
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|History")
	bool CanRedo() const;

	/**
	 * Undo last operation.
	 * @return true if undone successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|History")
	bool Undo();

	/**
	 * Redo last undone operation.
	 * @return true if redone successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|History")
	bool Redo();

	/**
	 * Undo multiple operations.
	 * @param Count Number to undo
	 * @return Number actually undone
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|History")
	int32 UndoMultiple(int32 Count);

	/**
	 * Redo multiple operations.
	 * @param Count Number to redo
	 * @return Number actually redone
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|History")
	int32 RedoMultiple(int32 Count);

	//==================================================================
	// Query
	//==================================================================

	/**
	 * Get undo stack size.
	 * @return Number of undoable operations
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|History")
	int32 GetUndoCount() const { return UndoStack.Num(); }

	/**
	 * Get redo stack size.
	 * @return Number of redoable operations
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|History")
	int32 GetRedoCount() const { return RedoStack.Num(); }

	/**
	 * Get last operation.
	 * @param OutRecord Output record
	 * @return true if history not empty
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|History")
	bool GetLastOperation(FSuspenseCoreOperationRecord& OutRecord) const;

	/**
	 * Get all undo operations.
	 * @return Array of undoable records (newest first)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|History")
	TArray<FSuspenseCoreOperationRecord> GetUndoHistory() const;

	/**
	 * Get operations by type.
	 * @param Type Operation type to filter
	 * @return Matching operations
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|History")
	TArray<FSuspenseCoreOperationRecord> GetOperationsByType(ESuspenseCoreOperationType Type) const;

	/**
	 * Get operations for item.
	 * @param InstanceID Instance ID
	 * @return Operations affecting this instance
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|History")
	TArray<FSuspenseCoreOperationRecord> GetOperationsForItem(const FGuid& InstanceID) const;

	//==================================================================
	// Management
	//==================================================================

	/**
	 * Clear all history.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|History")
	void ClearHistory();

	/**
	 * Clear redo stack only.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|History")
	void ClearRedoStack();

	/**
	 * Mark history save point.
	 * Used to identify "clean" state.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|History")
	void MarkSavePoint();

	/**
	 * Check if at save point.
	 * @return true if no changes since last save
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|History")
	bool IsAtSavePoint() const;

	//==================================================================
	// Debug
	//==================================================================

	/**
	 * Get debug string.
	 * @return History debug info
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|History")
	FString GetDebugString() const;

protected:
	/** Target inventory */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreInventoryComponent> TargetInventory;

	/** Undo stack (newest at end) */
	UPROPERTY()
	TArray<FSuspenseCoreOperationRecord> UndoStack;

	/** Redo stack (newest at end) */
	UPROPERTY()
	TArray<FSuspenseCoreOperationRecord> RedoStack;

	/** Maximum history size */
	int32 MaxSize;

	/** Save point operation ID */
	FGuid SavePointID;

	/** Execute undo for single operation */
	bool ExecuteUndo(const FSuspenseCoreOperationRecord& Record);

	/** Execute redo for single operation */
	bool ExecuteRedo(const FSuspenseCoreOperationRecord& Record);

	/** Enforce max size by removing oldest entries */
	void EnforceMaxSize();
};
