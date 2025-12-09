// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryBaseTypes.h"
#include "SuspenseCore/Types/Equipment/SuspenseCoreEquipmentTypes.h"
#include "ISuspenseCoreEquipmentOperations.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreEquipmentOperations : public UInterface
{
    GENERATED_BODY()
};

/**
 * SuspenseCore Equipment Operations Interface
 *
 * Pure business logic for equipment operations.
 * No knowledge of UI, networking, or persistence.
 *
 * Replaces legacy ISuspenseEquipmentOperations with SuspenseCore naming.
 *
 * Key Design Principles:
 * - Single Responsibility: Execute equipment operations only
 * - No side effects: No network calls, no persistence
 * - Validation: Pre-check operations before execution
 * - History: Track operations for undo/redo
 */
class BRIDGESYSTEM_API ISuspenseCoreEquipmentOperations
{
    GENERATED_BODY()

public:
    //========================================
    // Core Operation Execution
    //========================================

    /**
     * Execute equipment operation
     * @param Request Operation request containing operation type and parameters
     * @return Operation result with success/failure and details
     */
    virtual FEquipmentOperationResult ExecuteOperation(const FEquipmentOperationRequest& Request) = 0;

    /**
     * Validate operation before execution
     * @param Request Operation to validate
     * @return Validation result indicating if operation can proceed
     */
    virtual FSlotValidationResult ValidateOperation(const FEquipmentOperationRequest& Request) const = 0;

    //========================================
    // Specific Operations
    //========================================

    /**
     * Equip item to specified slot
     * @param ItemInstance Item to equip
     * @param SlotIndex Target slot index
     * @return Operation result
     */
    virtual FEquipmentOperationResult EquipItem(
        const FSuspenseInventoryItemInstance& ItemInstance,
        int32 SlotIndex) = 0;

    /**
     * Unequip item from slot
     * @param SlotIndex Slot to clear
     * @return Operation result with unequipped item info
     */
    virtual FEquipmentOperationResult UnequipItem(int32 SlotIndex) = 0;

    /**
     * Swap items between two slots
     * @param SlotIndexA First slot
     * @param SlotIndexB Second slot
     * @return Operation result
     */
    virtual FEquipmentOperationResult SwapItems(int32 SlotIndexA, int32 SlotIndexB) = 0;

    /**
     * Move item from source to target slot
     * @param SourceSlot Source slot index
     * @param TargetSlot Target slot index
     * @return Operation result
     */
    virtual FEquipmentOperationResult MoveItem(int32 SourceSlot, int32 TargetSlot) = 0;

    /**
     * Drop item from slot (remove from equipment)
     * @param SlotIndex Slot to drop item from
     * @return Operation result with dropped item info
     */
    virtual FEquipmentOperationResult DropItem(int32 SlotIndex) = 0;

    /**
     * Quick switch to next weapon slot
     * @return Operation result
     */
    virtual FEquipmentOperationResult QuickSwitchWeapon() = 0;

    //========================================
    // History & Undo
    //========================================

    /**
     * Get recent operation history
     * @param MaxCount Maximum operations to return
     * @return Array of recent operation results
     */
    virtual TArray<FEquipmentOperationResult> GetOperationHistory(int32 MaxCount = 10) const = 0;

    /**
     * Check if last operation can be undone
     * @return True if undo is available
     */
    virtual bool CanUndoLastOperation() const = 0;

    /**
     * Undo the last operation
     * @return Result of the undo operation
     */
    virtual FEquipmentOperationResult UndoLastOperation() = 0;

    //========================================
    // Extended Operations (Optional)
    //========================================

    /**
     * Batch execute multiple operations atomically
     * @param Requests Array of operation requests
     * @return Array of results (all succeed or all fail)
     */
    virtual TArray<FEquipmentOperationResult> ExecuteBatchOperations(const TArray<FEquipmentOperationRequest>& Requests)
    {
        TArray<FEquipmentOperationResult> Results;
        Results.Reserve(Requests.Num());

        for (const FEquipmentOperationRequest& Request : Requests)
        {
            Results.Add(ExecuteOperation(Request));
        }

        return Results;
    }

    /**
     * Check if operation type is supported
     * @param OperationType Type of operation
     * @return True if supported
     */
    virtual bool IsOperationSupported(EEquipmentOperationType OperationType) const
    {
        // Default: support all standard operations
        return OperationType != EEquipmentOperationType::None;
    }
};

