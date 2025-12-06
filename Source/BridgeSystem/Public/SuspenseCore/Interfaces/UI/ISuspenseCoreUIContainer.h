// ISuspenseCoreUIContainer.h
// SuspenseCore - UI Container Widget Interface
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUITypes.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUIContainerTypes.h"
#include "ISuspenseCoreUIContainer.generated.h"

// Forward declarations
class ISuspenseCoreUIDataProvider;

/**
 * Delegate for container events
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSuspenseCoreContainerEvent, const FGameplayTag& /*EventType*/, int32 /*SlotIndex*/);

/**
 * USuspenseCoreUIContainer
 * UInterface for container widgets
 */
UINTERFACE(MinimalAPI, Blueprintable, BlueprintType)
class USuspenseCoreUIContainer : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISuspenseCoreUIContainer
 *
 * Interface for container widgets (inventory grid, equipment panel, etc.)
 * Widgets implement this to standardize binding to providers.
 *
 * ARCHITECTURE:
 * - Container widgets bind to ISuspenseCoreUIDataProvider
 * - Providers push data updates via OnUIDataChanged
 * - Widgets refresh display from provider data
 * - User actions go back through provider to EventBus
 *
 * IMPLEMENTATIONS:
 * - USuspenseCoreInventoryWidget (grid container)
 * - USuspenseCoreEquipmentWidget (equipment slots) [future]
 * - USuspenseCoreLootWidget (loot container) [future]
 *
 * @see ISuspenseCoreUIDataProvider
 */
class BRIDGESYSTEM_API ISuspenseCoreUIContainer
{
	GENERATED_BODY()

public:
	//==================================================================
	// Provider Binding
	//==================================================================

	/**
	 * Bind container to data provider
	 * Widget will subscribe to provider's OnUIDataChanged and refresh.
	 * @param Provider Provider to bind to
	 */
	virtual void BindToProvider(TScriptInterface<ISuspenseCoreUIDataProvider> Provider) = 0;

	/**
	 * Unbind from current provider
	 * Clears binding and subscription.
	 */
	virtual void UnbindFromProvider() = 0;

	/**
	 * Check if bound to a provider
	 * @return true if currently bound
	 */
	virtual bool IsBoundToProvider() const = 0;

	/**
	 * Get currently bound provider
	 * @return Current provider or nullptr
	 */
	virtual TScriptInterface<ISuspenseCoreUIDataProvider> GetBoundProvider() const = 0;

	//==================================================================
	// Refresh
	//==================================================================

	/**
	 * Refresh entire container from provider data
	 * Call after binding or when data changes.
	 */
	virtual void RefreshFromProvider() = 0;

	/**
	 * Refresh single slot
	 * More efficient than full refresh for single item changes.
	 * @param SlotIndex Slot to refresh
	 */
	virtual void RefreshSlot(int32 SlotIndex) = 0;

	/**
	 * Refresh item by instance ID
	 * @param InstanceID Item to refresh
	 */
	virtual void RefreshItem(const FGuid& InstanceID) = 0;

	//==================================================================
	// Slot Access
	//==================================================================

	/**
	 * Get slot widget at index
	 * @param SlotIndex Slot to get
	 * @return Slot widget or nullptr
	 */
	virtual UWidget* GetSlotWidget(int32 SlotIndex) const = 0;

	/**
	 * Get all slot widgets
	 * @return Array of slot widgets
	 */
	virtual TArray<UWidget*> GetAllSlotWidgets() const = 0;

	/**
	 * Get slot index from screen position
	 * @param ScreenPosition Position in screen space
	 * @return Slot index or INDEX_NONE
	 */
	virtual int32 GetSlotAtPosition(const FVector2D& ScreenPosition) const = 0;

	/**
	 * Get slot index from local position
	 * @param LocalPosition Position in widget space
	 * @return Slot index or INDEX_NONE
	 */
	virtual int32 GetSlotAtLocalPosition(const FVector2D& LocalPosition) const = 0;

	//==================================================================
	// Selection
	//==================================================================

	/**
	 * Set selected slot
	 * @param SlotIndex Slot to select (-1 to clear)
	 */
	virtual void SetSelectedSlot(int32 SlotIndex) = 0;

	/**
	 * Get selected slot
	 * @return Selected slot index or INDEX_NONE
	 */
	virtual int32 GetSelectedSlot() const = 0;

	/**
	 * Clear selection
	 */
	virtual void ClearSelection() = 0;

	//==================================================================
	// Highlighting
	//==================================================================

	/**
	 * Set slot highlight state
	 * @param SlotIndex Slot to highlight
	 * @param State Highlight state
	 */
	virtual void SetSlotHighlight(int32 SlotIndex, ESuspenseCoreUISlotState State) = 0;

	/**
	 * Highlight slots for potential drop
	 * @param DragData Current drag data
	 * @param HoverSlot Slot being hovered
	 */
	virtual void HighlightDropTarget(const FSuspenseCoreDragData& DragData, int32 HoverSlot) = 0;

	/**
	 * Clear all highlights
	 */
	virtual void ClearHighlights() = 0;

	//==================================================================
	// Drag-Drop
	//==================================================================

	/**
	 * Check if container accepts drops
	 * @return true if drops are accepted
	 */
	virtual bool AcceptsDrop() const = 0;

	/**
	 * Start drag from slot
	 * @param SlotIndex Slot to drag from
	 * @param bSplitStack Start split stack operation
	 * @return true if drag started
	 */
	virtual bool StartDragFromSlot(int32 SlotIndex, bool bSplitStack = false) = 0;

	/**
	 * Handle drop on container
	 * @param DragData Drag data
	 * @param TargetSlot Target slot
	 * @return true if drop handled
	 */
	virtual bool HandleDrop(const FSuspenseCoreDragData& DragData, int32 TargetSlot) = 0;

	/**
	 * Handle drag cancelled
	 */
	virtual void HandleDragCancelled() = 0;

	//==================================================================
	// Configuration
	//==================================================================

	/**
	 * Get container type
	 * @return Container type this widget displays
	 */
	virtual ESuspenseCoreContainerType GetContainerType() const = 0;

	/**
	 * Get container type tag
	 * @return Container type as tag
	 */
	virtual FGameplayTag GetContainerTypeTag() const = 0;

	/**
	 * Is container read-only
	 * @return true if modifications not allowed
	 */
	virtual bool IsReadOnly() const = 0;

	/**
	 * Set read-only state
	 * @param bReadOnly New state
	 */
	virtual void SetReadOnly(bool bReadOnly) = 0;

	//==================================================================
	// Events
	//==================================================================

	/**
	 * Get container event delegate
	 * @return Reference to event delegate
	 */
	virtual FOnSuspenseCoreContainerEvent& OnContainerEvent() = 0;

	//==================================================================
	// Tooltip
	//==================================================================

	/**
	 * Show tooltip for slot
	 * @param SlotIndex Slot to show tooltip for
	 */
	virtual void ShowSlotTooltip(int32 SlotIndex) = 0;

	/**
	 * Hide current tooltip
	 */
	virtual void HideTooltip() = 0;

	//==================================================================
	// Context Menu
	//==================================================================

	/**
	 * Show context menu for slot
	 * @param SlotIndex Slot to show menu for
	 * @param ScreenPosition Position for menu
	 */
	virtual void ShowContextMenu(int32 SlotIndex, const FVector2D& ScreenPosition) = 0;

	/**
	 * Hide context menu
	 */
	virtual void HideContextMenu() = 0;
};

/**
 * USuspenseCoreUISlot
 * UInterface for individual slot widgets
 */
UINTERFACE(MinimalAPI, Blueprintable, BlueprintType)
class USuspenseCoreUISlot : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISuspenseCoreUISlot
 *
 * Interface for individual slot widgets within a container.
 * Represents a single grid cell or equipment slot.
 */
class BRIDGESYSTEM_API ISuspenseCoreUISlot
{
	GENERATED_BODY()

public:
	//==================================================================
	// Initialization
	//==================================================================

	/**
	 * Initialize slot with data
	 * @param SlotData Slot configuration data
	 */
	virtual void InitializeSlot(const FSuspenseCoreSlotUIData& SlotData) = 0;

	/**
	 * Update slot display
	 * @param SlotData Updated slot data
	 * @param ItemData Item data (if occupied)
	 */
	virtual void UpdateSlot(const FSuspenseCoreSlotUIData& SlotData, const FSuspenseCoreItemUIData& ItemData) = 0;

	//==================================================================
	// State
	//==================================================================

	/**
	 * Get slot index
	 * @return Slot index in container
	 */
	virtual int32 GetSlotIndex() const = 0;

	/**
	 * Check if slot is occupied
	 * @return true if has item
	 */
	virtual bool IsOccupied() const = 0;

	/**
	 * Get item instance ID if occupied
	 * @return Item ID or invalid GUID
	 */
	virtual FGuid GetItemInstanceID() const = 0;

	//==================================================================
	// Visual State
	//==================================================================

	/**
	 * Set visual state
	 * @param State New visual state
	 */
	virtual void SetState(ESuspenseCoreUISlotState State) = 0;

	/**
	 * Get current visual state
	 * @return Current state
	 */
	virtual ESuspenseCoreUISlotState GetState() const = 0;

	/**
	 * Set selected
	 * @param bSelected Is selected
	 */
	virtual void SetSelected(bool bSelected) = 0;

	/**
	 * Set highlighted
	 * @param bHighlighted Is highlighted
	 * @param Color Highlight color
	 */
	virtual void SetHighlighted(bool bHighlighted, const FLinearColor& Color = FLinearColor::White) = 0;

	/**
	 * Set locked
	 * @param bLocked Is locked
	 */
	virtual void SetLocked(bool bLocked) = 0;
};
