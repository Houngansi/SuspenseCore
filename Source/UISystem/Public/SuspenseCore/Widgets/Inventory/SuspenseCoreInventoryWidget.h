// SuspenseCoreInventoryWidget.h
// SuspenseCore - Grid-based Inventory Widget
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Widgets/Base/SuspenseCoreBaseContainerWidget.h"
#include "SuspenseCoreInventoryWidget.generated.h"

// Forward declarations
class USuspenseCoreInventorySlotWidget;
class UGridPanel;
class UGridSlot;
class UUniformGridPanel;
class UCanvasPanel;
class UTextBlock;

/**
 * FSuspenseCoreGridUpdateBatch
 * Batch structure for efficient grid updates.
 * Collects multiple slot changes and applies them in a single pass.
 */
USTRUCT(BlueprintType)
struct UISYSTEM_API FSuspenseCoreGridUpdateBatch
{
	GENERATED_BODY()

	/** Slots that need visibility updates */
	UPROPERTY(BlueprintReadWrite, Category = "Batch")
	TMap<int32, bool> SlotVisibilityUpdates;

	/** Slots that need span/size updates (for multi-cell items) */
	UPROPERTY(BlueprintReadWrite, Category = "Batch")
	TMap<int32, FIntPoint> SlotSpanUpdates;

	/** Slots that need full content refresh */
	UPROPERTY(BlueprintReadWrite, Category = "Batch")
	TArray<int32> SlotsToRefresh;

	/** Slots with highlight state changes */
	UPROPERTY(BlueprintReadWrite, Category = "Batch")
	TMap<int32, ESuspenseCoreUISlotState> SlotHighlightUpdates;

	/** If true, skip individual updates and do full refresh */
	UPROPERTY(BlueprintReadWrite, Category = "Batch")
	bool bNeedsFullRefresh = false;

	/** Clear all pending updates */
	void Clear()
	{
		SlotVisibilityUpdates.Empty();
		SlotSpanUpdates.Empty();
		SlotsToRefresh.Empty();
		SlotHighlightUpdates.Empty();
		bNeedsFullRefresh = false;
	}

	/** Check if there are any pending updates */
	bool HasUpdates() const
	{
		return bNeedsFullRefresh ||
			SlotVisibilityUpdates.Num() > 0 ||
			SlotSpanUpdates.Num() > 0 ||
			SlotsToRefresh.Num() > 0 ||
			SlotHighlightUpdates.Num() > 0;
	}

	/** Get total number of updates */
	int32 GetUpdateCount() const
	{
		return SlotVisibilityUpdates.Num() + SlotSpanUpdates.Num() +
			SlotsToRefresh.Num() + SlotHighlightUpdates.Num();
	}

	/** Add a slot to refresh */
	void AddSlotToRefresh(int32 SlotIndex)
	{
		SlotsToRefresh.AddUnique(SlotIndex);
	}

	/** Set visibility for a slot */
	void SetSlotVisibility(int32 SlotIndex, bool bVisible)
	{
		SlotVisibilityUpdates.Add(SlotIndex, bVisible);
	}

	/** Set span/size for a slot (multi-cell items) */
	void SetSlotSpan(int32 SlotIndex, const FIntPoint& Span)
	{
		SlotSpanUpdates.Add(SlotIndex, Span);
	}

	/** Set highlight state for a slot */
	void SetSlotHighlightState(int32 SlotIndex, ESuspenseCoreUISlotState State)
	{
		SlotHighlightUpdates.Add(SlotIndex, State);
	}
};

/**
 * USuspenseCoreInventoryWidget
 *
 * Grid-based inventory container widget for Tarkov/Destiny-style inventory.
 * Supports multi-cell items with rotation.
 *
 * FEATURES:
 * - Configurable grid dimensions (e.g., 10x5 = 50 slots)
 * - Multi-cell items (e.g., 2x3 rifle takes 6 cells)
 * - Item rotation (R key during drag)
 * - Visual slot highlighting during drag
 * - Quick-equip and quick-transfer shortcuts
 *
 * LAYOUT:
 * - Uses UniformGridPanel or GridPanel for slot layout
 * - Each slot is a SuspenseCoreInventorySlotWidget
 * - Items can span multiple slots
 *
 * @see USuspenseCoreBaseContainerWidget
 * @see USuspenseCoreInventorySlotWidget
 */
UCLASS(BlueprintType, Blueprintable)
class UISYSTEM_API USuspenseCoreInventoryWidget : public USuspenseCoreBaseContainerWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreInventoryWidget(const FObjectInitializer& ObjectInitializer);

	//==================================================================
	// UUserWidget Interface
	//==================================================================

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent, UDragDropOperation* Operation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	//==================================================================
	// Grid Configuration
	//==================================================================

	/**
	 * Set grid dimensions
	 * @param InGridSize Grid dimensions (columns, rows)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Inventory")
	void SetGridSize(const FIntPoint& InGridSize);

	/**
	 * Get grid dimensions from provider (CachedContainerData)
	 * NOTE: Always returns provider's grid size as single source of truth
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Inventory")
	FIntPoint GetGridSize() const { return CachedContainerData.GridSize; }

	/**
	 * Get slot size in pixels
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Inventory")
	float GetSlotSizePixels() const { return SlotSizePixels; }

	/**
	 * Set slot size
	 * @param InSize Size in pixels
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Inventory")
	void SetSlotSizePixels(float InSize);

	//==================================================================
	// ISuspenseCoreUIContainer Overrides
	//==================================================================

	virtual void RefreshFromProvider() override;
	virtual UWidget* GetSlotWidget(int32 SlotIndex) const override;
	virtual TArray<UWidget*> GetAllSlotWidgets() const override;
	virtual int32 GetSlotAtLocalPosition(const FVector2D& LocalPosition) const override;
	virtual void SetSlotHighlight(int32 SlotIndex, ESuspenseCoreUISlotState State) override;
	virtual void ShowSlotTooltip(int32 SlotIndex) override;
	virtual void HideTooltip() override;

	//==================================================================
	// Grid Utilities
	//==================================================================

	/**
	 * Convert slot index to grid coordinates
	 * @param SlotIndex Slot index
	 * @return Grid position (column, row)
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Inventory")
	FIntPoint SlotIndexToGridPos(int32 SlotIndex) const;

	/**
	 * Convert grid coordinates to slot index
	 * @param GridPos Grid position (column, row)
	 * @return Slot index, or INDEX_NONE if invalid
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Inventory")
	int32 GridPosToSlotIndex(const FIntPoint& GridPos) const;

	/**
	 * Check if grid position is valid
	 * @param GridPos Grid position to check
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Inventory")
	bool IsValidGridPos(const FIntPoint& GridPos) const;

	/**
	 * Get all slots occupied by an item at given position with given size
	 * @param GridPos Top-left grid position
	 * @param ItemSize Item size (width, height)
	 * @return Array of slot indices
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Inventory")
	TArray<int32> GetOccupiedSlots(const FIntPoint& GridPos, const FIntPoint& ItemSize) const;

	//==================================================================
	// Drag Support
	//==================================================================

	/**
	 * Get slot index at current cursor position
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Inventory")
	int32 GetHoveredSlot() const { return HoveredSlotIndex; }

	/**
	 * Highlight slots for potential drop
	 * @param ItemSize Size of item being dragged
	 * @param TargetSlot Target slot index
	 * @param bIsValid Whether drop would be valid
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Inventory")
	void HighlightDropSlots(const FIntPoint& ItemSize, int32 TargetSlot, bool bIsValid);

	//==================================================================
	// Blueprint Events
	//==================================================================

	/** Called when slot is clicked */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Inventory", meta = (DisplayName = "On Slot Clicked"))
	void K2_OnSlotClicked(int32 SlotIndex, bool bRightClick);

	/** Called when slot is double clicked */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Inventory", meta = (DisplayName = "On Slot Double Clicked"))
	void K2_OnSlotDoubleClicked(int32 SlotIndex);

	/** Called when item rotation is toggled during drag */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Inventory", meta = (DisplayName = "On Rotation Toggled"))
	void K2_OnRotationToggled(bool bIsRotated);

protected:
	//==================================================================
	// Override Points from Base
	//==================================================================

	virtual void CreateSlotWidgets_Implementation() override;
	virtual void UpdateSlotWidget_Implementation(int32 SlotIndex, const FSuspenseCoreSlotUIData& SlotData, const FSuspenseCoreItemUIData& ItemData) override;
	virtual void ClearSlotWidgets_Implementation() override;

	//==================================================================
	// Input Handling
	//==================================================================

	/** Handle slot clicked */
	void HandleSlotClicked(int32 SlotIndex, bool bRightClick);

	/** Handle slot double clicked - opens magazine inspection for magazine items */
	void HandleSlotDoubleClicked(int32 SlotIndex);

	/**
	 * Try to handle ammo-to-magazine drop
	 * @param DragData Drag data with ammo item
	 * @param TargetSlot Slot containing magazine
	 * @return True if handled (ammo loading started), false if not applicable
	 */
	bool TryHandleAmmoToMagazineDrop(const FSuspenseCoreDragData& DragData, int32 TargetSlot);

	/** Check if item is ammo */
	bool IsAmmoItem(const FSuspenseCoreItemUIData& ItemData) const;

	/** Handle slot hovered */
	void HandleSlotHovered(int32 SlotIndex);

	/** Handle rotation key */
	void HandleRotationInput();

	//==================================================================
	// Widget References (Bind in Blueprint)
	//==================================================================

	/**
	 * Grid panel for slots (PREFERRED - supports multi-cell spanning)
	 * Use UGridPanel for proper multi-cell item rendering.
	 * GridSlot supports SetColumnSpan/SetRowSpan for item spanning.
	 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UGridPanel> SlotGridPanel;

	/**
	 * Uniform grid panel for slots (FALLBACK - no spanning support)
	 * Only use if GridPanel is not available.
	 * Multi-cell items will render with visual spanning via icon resizing.
	 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UUniformGridPanel> SlotGrid;

	/** Canvas for item overlays */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UCanvasPanel> ItemOverlayCanvas;

	/** Weight display text (format: "X.X / Y.Y kg") */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UTextBlock> WeightText;

	/** Slot count display text (format: "X / Y") */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UTextBlock> SlotCountText;

	//==================================================================
	// Configuration
	//==================================================================

	/**
	 * Grid dimensions (columns x rows)
	 * @deprecated Use CachedContainerData.GridSize instead - this is only kept for BP backwards compatibility
	 * The actual grid size should come from the provider (ISuspenseCoreUIDataProvider)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration",
		meta = (DeprecatedProperty, DeprecationMessage = "Use provider's GridSize via CachedContainerData.GridSize"))
	FIntPoint GridSize;

	/** Size of each slot in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration")
	float SlotSizePixels;

	/** Gap between slots in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration")
	float SlotGapPixels;

	/** Widget class for slots */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
	TSubclassOf<USuspenseCoreInventorySlotWidget> SlotWidgetClass;

	/** Key to rotate item during drag */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
	FKey RotateKey;

	/** Key for quick equip action */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
	FKey QuickEquipKey;

	/** Key for quick transfer action */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
	FKey QuickTransferKey;

private:
	//==================================================================
	// Slot Widgets
	//==================================================================

	/** Array of slot widgets */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USuspenseCoreInventorySlotWidget>> SlotWidgets;

	//==================================================================
	// Slot-to-Anchor Map (for multi-cell items)
	//==================================================================

	/**
	 * Map from any occupied slot to the anchor slot of the item.
	 * For single-cell items, the slot maps to itself.
	 * For multi-cell items, all occupied slots map to the top-left anchor slot.
	 *
	 * Example: 2x2 item at slot 0
	 * - SlotToAnchorMap[0] = 0 (anchor)
	 * - SlotToAnchorMap[1] = 0 (maps to anchor)
	 * - SlotToAnchorMap[10] = 0 (maps to anchor, next row)
	 * - SlotToAnchorMap[11] = 0 (maps to anchor, next row)
	 *
	 * Usage: When clicking any slot, look up anchor to find the actual item.
	 */
	TMap<int32, int32> SlotToAnchorMap;

	/** Update the slot-to-anchor map when inventory content changes */
	void UpdateSlotToAnchorMap();

	//==================================================================
	// Batch Update System
	//==================================================================

	/** Pending batch of updates to apply */
	FSuspenseCoreGridUpdateBatch PendingBatch;

	/**
	 * Apply a batch of slot updates efficiently
	 * Minimizes individual widget updates by processing all changes at once.
	 * @param Batch The batch of updates to apply
	 */
	void ApplyBatchUpdate(const FSuspenseCoreGridUpdateBatch& Batch);

	/**
	 * Begin collecting updates into a batch
	 * Defers slot updates until CommitBatchUpdate is called.
	 */
	void BeginBatchUpdate();

	/**
	 * Commit and apply the pending batch of updates
	 */
	void CommitBatchUpdate();

	/** Whether we're currently batching updates */
	bool bIsBatchingUpdates = false;

	//==================================================================
	// State
	//==================================================================

	/** Currently hovered slot index */
	int32 HoveredSlotIndex;

	/** Last click time for double-click detection */
	double LastClickTime;

	/** Last clicked slot for double-click detection */
	int32 LastClickedSlot;

	/** Slot index where drag started from */
	int32 DragSourceSlot;

	/** Mouse position when drag started (screen space) - needed for correct DragOffset calculation */
	FVector2D DragStartMousePosition;

	/** Double click threshold in seconds */
	static constexpr double DoubleClickThreshold = 0.3;

	//==================================================================
	// Multi-Cell Item Support
	//==================================================================

	/** Whether we're using GridPanel (true) or UniformGridPanel (false) */
	bool bUsingGridPanel = false;

	/**
	 * Get the active grid panel widget for slot management
	 * Returns SlotGridPanel if bound, otherwise SlotGrid
	 */
	UPanelWidget* GetActiveGridPanel() const;

	/**
	 * Update grid slot span for multi-cell item
	 * Sets ColumnSpan and RowSpan on the GridSlot for anchor slots.
	 * Hides non-anchor slots that are part of multi-cell item.
	 * @param AnchorSlotIndex Anchor slot index
	 * @param ItemSize Item size in grid cells
	 * @param bIsRotated Whether item is rotated
	 */
	void UpdateGridSlotSpan(int32 AnchorSlotIndex, const FIntPoint& ItemSize, bool bIsRotated);

	/**
	 * Reset grid slot span to 1x1
	 * @param SlotIndex Slot index to reset
	 */
	void ResetGridSlotSpan(int32 SlotIndex);

	/**
	 * Update visibility for slots occupied by multi-cell item
	 * Anchor slot: visible with span
	 * Non-anchor slots: hidden (covered by anchor's span)
	 * @param AnchorSlotIndex Anchor slot index
	 * @param ItemSize Item size
	 * @param bIsRotated Whether rotated
	 */
	void UpdateMultiCellSlotVisibility(int32 AnchorSlotIndex, const FIntPoint& ItemSize, bool bIsRotated);

	/**
	 * Cache of grid slots for quick access (GridPanel mode only)
	 * Map from slot index to GridSlot
	 */
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UGridSlot>> CachedGridSlots;
};
