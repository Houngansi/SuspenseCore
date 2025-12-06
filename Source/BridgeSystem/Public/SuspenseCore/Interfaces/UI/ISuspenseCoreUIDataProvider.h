// ISuspenseCoreUIDataProvider.h
// SuspenseCore - UI Data Provider Interface
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUITypes.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUIContainerTypes.h"
#include "ISuspenseCoreUIDataProvider.generated.h"

// Forward declarations
class USuspenseCoreEventBus;

/**
 * Delegate for UI data changes
 * Broadcast when provider data changes (items added/removed/moved, etc.)
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSuspenseCoreUIDataChanged, const FGameplayTag& /*ChangeType*/, const FGuid& /*AffectedItemID*/);

/**
 * Delegate for drop validation requests
 */
DECLARE_DELEGATE_RetVal_ThreeParams(FSuspenseCoreDropValidation, FOnSuspenseCoreValidateDrop, const FSuspenseCoreDragData& /*DragData*/, int32 /*TargetSlot*/, bool /*bRotated*/);

/**
 * USuspenseCoreUIDataProvider
 * UInterface for UI data provider
 */
UINTERFACE(MinimalAPI, Blueprintable, BlueprintType)
class USuspenseCoreUIDataProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISuspenseCoreUIDataProvider
 *
 * Interface for providing UI-ready data to widgets.
 * Any component that wants to display items in UI should implement this interface.
 *
 * ARCHITECTURE:
 * - Widgets bind to providers via this interface
 * - Providers convert internal data to UI-friendly format
 * - Changes are notified via OnUIDataChanged delegate
 * - All operations go through EventBus for decoupling
 *
 * IMPLEMENTATIONS:
 * - USuspenseCoreInventoryComponent (grid inventory)
 * - USuspenseCoreEquipmentComponent (equipment slots) [future]
 * - USuspenseCoreStashComponent (stash storage) [future]
 * - USuspenseCoreLootComponent (loot containers) [future]
 *
 * USAGE:
 * ```cpp
 * // Widget binding
 * ISuspenseCoreUIDataProvider* Provider = FindProvider();
 * Provider->OnUIDataChanged().AddUObject(this, &ThisClass::HandleDataChanged);
 *
 * // Getting data
 * FSuspenseCoreContainerUIData ContainerData = Provider->GetContainerUIData();
 * TArray<FSuspenseCoreItemUIData> Items = Provider->GetAllItemUIData();
 * ```
 *
 * @see FSuspenseCoreContainerUIData
 * @see FSuspenseCoreItemUIData
 */
class BRIDGESYSTEM_API ISuspenseCoreUIDataProvider
{
	GENERATED_BODY()

public:
	//==================================================================
	// Identity
	//==================================================================

	/**
	 * Get unique provider ID
	 * @return GUID identifying this provider instance
	 */
	virtual FGuid GetProviderID() const = 0;

	/**
	 * Get container type
	 * @return Container type enum
	 */
	virtual ESuspenseCoreContainerType GetContainerType() const = 0;

	/**
	 * Get container type as gameplay tag
	 * @return Tag like SuspenseCore.UIProvider.Type.Inventory
	 */
	virtual FGameplayTag GetContainerTypeTag() const = 0;

	/**
	 * Get owning actor
	 * @return Actor that owns this provider (typically PlayerState)
	 */
	virtual AActor* GetOwningActor() const = 0;

	//==================================================================
	// Container Data
	//==================================================================

	/**
	 * Get complete container UI data
	 * Includes all slots and items, weight info, restrictions.
	 * @return Container data for UI display
	 */
	virtual FSuspenseCoreContainerUIData GetContainerUIData() const = 0;

	/**
	 * Get grid size (for grid-based containers)
	 * @return Grid dimensions (width, height)
	 */
	virtual FIntPoint GetGridSize() const = 0;

	/**
	 * Get total slot count
	 * @return Number of slots in container
	 */
	virtual int32 GetSlotCount() const = 0;

	//==================================================================
	// Slot Data
	//==================================================================

	/**
	 * Get all slot UI data
	 * @return Array of slot data
	 */
	virtual TArray<FSuspenseCoreSlotUIData> GetAllSlotUIData() const = 0;

	/**
	 * Get slot UI data at specific index
	 * @param SlotIndex Slot to query
	 * @return Slot data or default if invalid
	 */
	virtual FSuspenseCoreSlotUIData GetSlotUIData(int32 SlotIndex) const = 0;

	/**
	 * Check if slot is valid
	 * @param SlotIndex Slot to check
	 * @return true if slot exists
	 */
	virtual bool IsSlotValid(int32 SlotIndex) const = 0;

	//==================================================================
	// Item Data
	//==================================================================

	/**
	 * Get all item UI data
	 * @return Array of all items in container
	 */
	virtual TArray<FSuspenseCoreItemUIData> GetAllItemUIData() const = 0;

	/**
	 * Get item UI data at specific slot
	 * @param SlotIndex Slot to query
	 * @param OutItem Output item data
	 * @return true if slot has item
	 */
	virtual bool GetItemUIDataAtSlot(int32 SlotIndex, FSuspenseCoreItemUIData& OutItem) const = 0;

	/**
	 * Find item UI data by instance ID
	 * @param InstanceID Item instance ID
	 * @param OutItem Output item data
	 * @return true if found
	 */
	virtual bool FindItemUIData(const FGuid& InstanceID, FSuspenseCoreItemUIData& OutItem) const = 0;

	/**
	 * Get item count (unique item stacks)
	 * @return Number of unique item stacks
	 */
	virtual int32 GetItemCount() const = 0;

	//==================================================================
	// Weight System
	//==================================================================

	/**
	 * Check if container has weight limit
	 * @return true if weight is limited
	 */
	virtual bool HasWeightLimit() const = 0;

	/**
	 * Get current weight
	 * @return Current total weight
	 */
	virtual float GetCurrentWeight() const = 0;

	/**
	 * Get maximum weight
	 * @return Max weight capacity
	 */
	virtual float GetMaxWeight() const = 0;

	/**
	 * Get weight as percentage (0-1)
	 * @return Current weight / max weight
	 */
	virtual float GetWeightPercent() const
	{
		float MaxW = GetMaxWeight();
		return MaxW > 0.0f ? GetCurrentWeight() / MaxW : 0.0f;
	}

	//==================================================================
	// Validation
	//==================================================================

	/**
	 * Validate drop operation
	 * @param DragData Drag data with item info
	 * @param TargetSlot Target slot index
	 * @param bRotated Is item rotated
	 * @return Validation result with reason
	 */
	virtual FSuspenseCoreDropValidation ValidateDrop(
		const FSuspenseCoreDragData& DragData,
		int32 TargetSlot,
		bool bRotated = false) const = 0;

	/**
	 * Check if container can accept item type
	 * @param ItemType Item type tag
	 * @return true if allowed
	 */
	virtual bool CanAcceptItemType(const FGameplayTag& ItemType) const = 0;

	/**
	 * Find best slot for item
	 * @param ItemSize Item grid size
	 * @param bAllowRotation Can rotate to fit
	 * @return Slot index or INDEX_NONE
	 */
	virtual int32 FindBestSlotForItem(FIntPoint ItemSize, bool bAllowRotation = true) const = 0;

	//==================================================================
	// Grid Position Calculations (moved from UI widget)
	//==================================================================

	/**
	 * Calculate slot index from local position within the grid
	 * Used by UI widgets to convert mouse position to slot index.
	 * NOTE: This is a UI-assisting method - calculations stay in provider as single source of truth.
	 *
	 * @param LocalPos Position in local widget space
	 * @param CellSize Size of each cell in pixels
	 * @param CellGap Gap between cells in pixels
	 * @return Slot index at position, or INDEX_NONE if outside grid
	 */
	virtual int32 GetSlotAtLocalPosition(const FVector2D& LocalPos, float CellSize, float CellGap) const = 0;

	/**
	 * Get all slots occupied by an item instance
	 * For multi-cell items, returns the anchor slot plus all additional cells.
	 *
	 * @param ItemInstanceID The item instance GUID
	 * @return Array of all slot indices occupied by this item
	 */
	virtual TArray<int32> GetOccupiedSlotsForItem(const FGuid& ItemInstanceID) const = 0;

	/**
	 * Get the anchor slot for any slot that might be part of a multi-cell item
	 * If slot is empty or is the anchor, returns the same slot index.
	 *
	 * @param AnySlotIndex Any slot index that might be part of a multi-cell item
	 * @return The anchor slot index, or the same index if empty/is anchor
	 */
	virtual int32 GetAnchorSlotForPosition(int32 AnySlotIndex) const = 0;

	/**
	 * Check if an item can be placed at a specific slot
	 * Validates bounds, occupancy, and rotation.
	 *
	 * @param ItemID The item's instance ID (or empty GUID for new items)
	 * @param SlotIndex Target anchor slot
	 * @param bRotated Whether item would be rotated
	 * @return true if placement is valid
	 */
	virtual bool CanPlaceItemAtSlot(const FGuid& ItemID, int32 SlotIndex, bool bRotated) const = 0;

	//==================================================================
	// Operations (via EventBus)
	//==================================================================

	/**
	 * Request item move within container
	 * Actually sends request via EventBus to inventory component.
	 * @param FromSlot Source slot
	 * @param ToSlot Target slot
	 * @param bRotate Rotate during move
	 * @return true if request sent (not if succeeded)
	 */
	virtual bool RequestMoveItem(int32 FromSlot, int32 ToSlot, bool bRotate = false) = 0;

	/**
	 * Request item rotation
	 * @param SlotIndex Slot with item
	 * @return true if request sent
	 */
	virtual bool RequestRotateItem(int32 SlotIndex) = 0;

	/**
	 * Request item use/consume
	 * @param SlotIndex Slot with item
	 * @return true if request sent
	 */
	virtual bool RequestUseItem(int32 SlotIndex) = 0;

	/**
	 * Request item drop to world
	 * @param SlotIndex Slot with item
	 * @param Quantity Amount to drop (0 = all)
	 * @return true if request sent
	 */
	virtual bool RequestDropItem(int32 SlotIndex, int32 Quantity = 0) = 0;

	/**
	 * Request stack split
	 * @param SlotIndex Slot with stack
	 * @param SplitQuantity Amount to split off
	 * @param TargetSlot Where to place split (-1 for auto)
	 * @return true if request sent
	 */
	virtual bool RequestSplitStack(int32 SlotIndex, int32 SplitQuantity, int32 TargetSlot = INDEX_NONE) = 0;

	/**
	 * Request item transfer to another container
	 * @param SlotIndex Slot with item
	 * @param TargetProviderID Target container provider ID
	 * @param TargetSlot Target slot (-1 for auto)
	 * @param Quantity Amount to transfer (0 = all)
	 * @return true if request sent
	 */
	virtual bool RequestTransferItem(
		int32 SlotIndex,
		const FGuid& TargetProviderID,
		int32 TargetSlot = INDEX_NONE,
		int32 Quantity = 0) = 0;

	//==================================================================
	// Context Menu Actions
	//==================================================================

	/**
	 * Get available context menu actions for item
	 * @param SlotIndex Slot with item
	 * @return Array of action tags
	 */
	virtual TArray<FGameplayTag> GetItemContextActions(int32 SlotIndex) const = 0;

	/**
	 * Execute context menu action
	 * @param SlotIndex Slot with item
	 * @param ActionTag Action to execute
	 * @return true if executed
	 */
	virtual bool ExecuteContextAction(int32 SlotIndex, const FGameplayTag& ActionTag) = 0;

	//==================================================================
	// Delegate Access
	//==================================================================

	/**
	 * Get data changed delegate
	 * Subscribe to receive notifications when data changes.
	 * @return Reference to delegate
	 */
	virtual FOnSuspenseCoreUIDataChanged& OnUIDataChanged() = 0;

	//==================================================================
	// EventBus Integration
	//==================================================================

	/**
	 * Get EventBus for this provider
	 * @return EventBus instance
	 */
	virtual USuspenseCoreEventBus* GetEventBus() const = 0;
};

/**
 * USuspenseCoreUIDataProviderLibrary
 * Blueprint function library for UI data provider operations
 */
UCLASS()
class BRIDGESYSTEM_API USuspenseCoreUIDataProviderLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Find UI data provider on actor
	 * @param Actor Actor to search
	 * @param ContainerType Type of container to find
	 * @return Provider interface or nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI", meta = (DisplayName = "Find UI Data Provider"))
	static TScriptInterface<ISuspenseCoreUIDataProvider> FindDataProviderOnActor(
		AActor* Actor,
		ESuspenseCoreContainerType ContainerType);

	/**
	 * Find all UI data providers on actor
	 * @param Actor Actor to search
	 * @return Array of providers
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI", meta = (DisplayName = "Find All UI Data Providers"))
	static TArray<TScriptInterface<ISuspenseCoreUIDataProvider>> FindAllDataProvidersOnActor(AActor* Actor);

	/**
	 * Get provider from component directly
	 * @param Component Component that might implement provider
	 * @return Provider interface or nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI", meta = (DisplayName = "Get Provider From Component"))
	static TScriptInterface<ISuspenseCoreUIDataProvider> GetProviderFromComponent(UActorComponent* Component);
};
