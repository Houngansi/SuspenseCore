// ISuspenseCoreInventory.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "ISuspenseCoreInventory.generated.h"

// Forward declarations
struct FSuspenseCoreItemData;
struct FSuspenseCoreItemInstance;
class USuspenseCoreEventBus;
enum class ESuspenseCoreInventoryResult : uint8;

/**
 * USuspenseCoreInventory
 *
 * UInterface for SuspenseCore inventory system.
 */
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreInventory : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISuspenseCoreInventory
 *
 * SuspenseCore inventory interface using EventBus architecture.
 *
 * ARCHITECTURE PRINCIPLES:
 * - Uses FSuspenseCoreItemData (static) and FSuspenseCoreItemInstance (runtime)
 * - EventBus for all notifications (no legacy delegates)
 * - DataTable as single source of truth
 * - Clear separation of concerns
 *
 * EVENTBUS INTEGRATION:
 * All operations broadcast events via SuspenseCore.Event.Inventory.*
 * - SuspenseCore.Event.Inventory.ItemAdded
 * - SuspenseCore.Event.Inventory.ItemRemoved
 * - SuspenseCore.Event.Inventory.ItemMoved
 * - SuspenseCore.Event.Inventory.Updated
 * - SuspenseCore.Event.Inventory.Error
 *
 * TRANSACTION SUPPORT:
 * Supports atomic transactions with rollback capability.
 *
 * @see FSuspenseCoreItemData
 * @see FSuspenseCoreItemInstance
 * @see USuspenseCoreEventBus
 */
class BRIDGESYSTEM_API ISuspenseCoreInventory
{
	GENERATED_BODY()

public:
	//==================================================================
	// Core Operations - Add
	//==================================================================

	/**
	 * Add item by ItemID from DataTable.
	 * Creates new FSuspenseCoreItemInstance internally.
	 * @param ItemID DataTable row name
	 * @param Quantity Amount to add
	 * @return true if successful
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Inventory")
	bool AddItemByID(FName ItemID, int32 Quantity);

	/**
	 * Add existing item instance.
	 * Used for transfers between inventories.
	 * @param ItemInstance Runtime instance to add
	 * @return true if successful
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Inventory")
	bool AddItemInstance(const FSuspenseCoreItemInstance& ItemInstance);

	/**
	 * Add item instance to specific slot.
	 * @param ItemInstance Instance to add
	 * @param TargetSlot Target slot index (-1 for auto-place)
	 * @return true if successful
	 */
	virtual bool AddItemInstanceToSlot(const FSuspenseCoreItemInstance& ItemInstance, int32 TargetSlot) = 0;

	//==================================================================
	// Core Operations - Remove
	//==================================================================

	/**
	 * Remove item by ID.
	 * @param ItemID Item to remove
	 * @param Quantity Amount to remove
	 * @return true if successful
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Inventory")
	bool RemoveItemByID(FName ItemID, int32 Quantity);

	/**
	 * Remove specific item instance.
	 * @param InstanceID Unique instance ID
	 * @return true if successful
	 */
	virtual bool RemoveItemInstance(const FGuid& InstanceID) = 0;

	/**
	 * Remove item from slot and return it.
	 * @param SlotIndex Slot to remove from
	 * @param OutRemovedInstance Removed instance (for undo/transfer)
	 * @return true if successful
	 */
	virtual bool RemoveItemFromSlot(int32 SlotIndex, FSuspenseCoreItemInstance& OutRemovedInstance) = 0;

	//==================================================================
	// Query Operations
	//==================================================================

	/**
	 * Get all item instances in inventory.
	 * @return Array of all instances
	 */
	virtual TArray<FSuspenseCoreItemInstance> GetAllItemInstances() const = 0;

	/**
	 * Get item instance at slot.
	 * @param SlotIndex Slot to query
	 * @param OutInstance Output instance
	 * @return true if slot contains item
	 */
	virtual bool GetItemInstanceAtSlot(int32 SlotIndex, FSuspenseCoreItemInstance& OutInstance) const = 0;

	/**
	 * Find item instance by ID.
	 * @param InstanceID Unique instance ID
	 * @param OutInstance Output instance
	 * @return true if found
	 */
	virtual bool FindItemInstance(const FGuid& InstanceID, FSuspenseCoreItemInstance& OutInstance) const = 0;

	/**
	 * Count items by ItemID.
	 * @param ItemID Item type to count
	 * @return Total quantity
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Inventory")
	int32 GetItemCountByID(FName ItemID) const;

	/**
	 * Check if has item.
	 * @param ItemID Item to check
	 * @param Quantity Minimum quantity
	 * @return true if has at least Quantity
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Inventory")
	bool HasItem(FName ItemID, int32 Quantity) const;

	/**
	 * Get total unique item count.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Inventory")
	int32 GetTotalItemCount() const;

	/**
	 * Find items by type tag.
	 * @param ItemType Type tag to filter by
	 * @return Matching instances
	 */
	virtual TArray<FSuspenseCoreItemInstance> FindItemsByType(FGameplayTag ItemType) const = 0;

	//==================================================================
	// Grid Operations
	//==================================================================

	/**
	 * Get inventory grid size.
	 * @return Grid dimensions (width, height)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Inventory|Grid")
	FIntPoint GetGridSize() const;

	/**
	 * Move item between slots.
	 * @param FromSlot Source slot
	 * @param ToSlot Target slot
	 * @return true if successful
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Inventory|Grid")
	bool MoveItem(int32 FromSlot, int32 ToSlot);

	/**
	 * Swap items between slots.
	 * @param Slot1 First slot
	 * @param Slot2 Second slot
	 * @return true if successful
	 */
	virtual bool SwapItems(int32 Slot1, int32 Slot2) = 0;

	/**
	 * Rotate item at slot.
	 * @param SlotIndex Slot containing item
	 * @return true if successful
	 */
	virtual bool RotateItemAtSlot(int32 SlotIndex) = 0;

	/**
	 * Check if slot is occupied.
	 * @param SlotIndex Slot to check
	 * @return true if slot has item
	 */
	virtual bool IsSlotOccupied(int32 SlotIndex) const = 0;

	/**
	 * Find free slot for item.
	 * @param ItemGridSize Size of item in grid
	 * @param bAllowRotation Allow rotation to fit
	 * @return Slot index or INDEX_NONE if no space
	 */
	virtual int32 FindFreeSlot(FIntPoint ItemGridSize, bool bAllowRotation = true) const = 0;

	/**
	 * Check if item can be placed at slot.
	 * @param ItemGridSize Item size
	 * @param SlotIndex Target slot
	 * @param bRotated Is item rotated
	 * @return true if placement is valid
	 */
	virtual bool CanPlaceItemAtSlot(FIntPoint ItemGridSize, int32 SlotIndex, bool bRotated = false) const = 0;

	//==================================================================
	// Weight System
	//==================================================================

	/**
	 * Get current total weight.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Inventory|Weight")
	float GetCurrentWeight() const;

	/**
	 * Get maximum weight capacity.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Inventory|Weight")
	float GetMaxWeight() const;

	/**
	 * Get remaining weight capacity.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Inventory|Weight")
	float GetRemainingWeight() const;

	/**
	 * Check if can carry additional weight.
	 * @param AdditionalWeight Weight to check
	 * @return true if has capacity
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Inventory|Weight")
	bool HasWeightCapacity(float AdditionalWeight) const;

	/**
	 * Set maximum weight.
	 * @param NewMaxWeight New weight limit
	 */
	virtual void SetMaxWeight(float NewMaxWeight) = 0;

	//==================================================================
	// Validation
	//==================================================================

	/**
	 * Check if can receive item.
	 * Validates space, weight, and restrictions.
	 * @param ItemID Item to check
	 * @param Quantity Amount to check
	 * @return true if can receive
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Inventory|Validation")
	bool CanReceiveItem(FName ItemID, int32 Quantity) const;

	/**
	 * Get allowed item types.
	 * Empty = all types allowed.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Inventory|Validation")
	FGameplayTagContainer GetAllowedItemTypes() const;

	/**
	 * Set allowed item types.
	 * @param AllowedTypes Types that can be stored
	 */
	virtual void SetAllowedItemTypes(const FGameplayTagContainer& AllowedTypes) = 0;

	/**
	 * Validate inventory integrity.
	 * @param OutErrors Array of error messages
	 * @return true if valid
	 */
	virtual bool ValidateIntegrity(TArray<FString>& OutErrors) const = 0;

	//==================================================================
	// Transaction System
	//==================================================================

	/**
	 * Begin atomic transaction.
	 * All operations until Commit/Rollback are grouped.
	 */
	virtual void BeginTransaction() = 0;

	/**
	 * Commit transaction.
	 * Applies all pending changes.
	 */
	virtual void CommitTransaction() = 0;

	/**
	 * Rollback transaction.
	 * Reverts all pending changes.
	 */
	virtual void RollbackTransaction() = 0;

	/**
	 * Check if transaction is active.
	 */
	virtual bool IsTransactionActive() const = 0;

	//==================================================================
	// Stack Operations
	//==================================================================

	/**
	 * Split stack at slot.
	 * @param SourceSlot Slot with stack
	 * @param SplitQuantity Amount to split off
	 * @param TargetSlot Where to place split (-1 for auto)
	 * @return true if successful
	 */
	virtual bool SplitStack(int32 SourceSlot, int32 SplitQuantity, int32 TargetSlot) = 0;

	/**
	 * Consolidate stacks of same item type.
	 * @param ItemID Item to consolidate (NAME_None = all)
	 * @return Number of stacks consolidated
	 */
	virtual int32 ConsolidateStacks(FName ItemID = NAME_None) = 0;

	//==================================================================
	// Initialization
	//==================================================================

	/**
	 * Initialize from loadout configuration.
	 * @param LoadoutID Loadout row name
	 * @return true if successful
	 */
	virtual bool InitializeFromLoadout(FName LoadoutID) = 0;

	/**
	 * Initialize with grid size and weight.
	 * @param GridWidth Grid width
	 * @param GridHeight Grid height
	 * @param MaxWeight Maximum weight capacity
	 */
	virtual void Initialize(int32 GridWidth, int32 GridHeight, float MaxWeight) = 0;

	/**
	 * Check if inventory is initialized.
	 */
	virtual bool IsInitialized() const = 0;

	/**
	 * Clear all items.
	 */
	virtual void Clear() = 0;

	//==================================================================
	// EventBus Integration
	//==================================================================

	/**
	 * Get EventBus for this inventory.
	 * Used for subscribing to inventory events.
	 */
	virtual USuspenseCoreEventBus* GetEventBus() const = 0;

	/**
	 * Force broadcast inventory updated event.
	 */
	virtual void BroadcastInventoryUpdated() = 0;

	//==================================================================
	// Debug
	//==================================================================

	/**
	 * Get debug info string.
	 */
	virtual FString GetDebugString() const = 0;

	/**
	 * Log inventory contents.
	 */
	virtual void LogContents() const = 0;
};

