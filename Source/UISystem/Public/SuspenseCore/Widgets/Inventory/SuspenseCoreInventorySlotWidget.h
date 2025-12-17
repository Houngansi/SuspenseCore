// SuspenseCoreInventorySlotWidget.h
// SuspenseCore - Individual Inventory Slot Widget
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUITypes.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreSlotUI.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreDraggable.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreDropTarget.h"
#include "SuspenseCoreInventorySlotWidget.generated.h"

// Forward declarations
class UImage;
class UTextBlock;
class UBorder;
class USizeBox;

/**
 * USuspenseCoreInventorySlotWidget
 *
 * Individual slot widget for grid-based inventory.
 * Displays a single cell that can contain part of an item.
 *
 * VISUAL ELEMENTS:
 * - Background (changes based on state)
 * - Item icon (for primary slot of multi-cell items)
 * - Stack count (for stackable items)
 * - Condition indicator (optional)
 * - Highlight border
 *
 * STATES:
 * - Empty: Shows empty slot background
 * - Occupied: Shows item icon (if primary) or occupied indicator
 * - Hovered: Highlight border
 * - Selected: Selection border
 * - Valid drop: Green highlight
 * - Invalid drop: Red highlight
 *
 * @see USuspenseCoreInventoryWidget
 */
UCLASS(BlueprintType, Blueprintable)
class UISYSTEM_API USuspenseCoreInventorySlotWidget : public UUserWidget
	, public ISuspenseCoreSlotUI
	, public ISuspenseCoreDraggable
	, public ISuspenseCoreDropTarget
{
	GENERATED_BODY()

public:
	USuspenseCoreInventorySlotWidget(const FObjectInitializer& ObjectInitializer);

	//==================================================================
	// UUserWidget Interface
	//==================================================================

	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;

	//==================================================================
	// ISuspenseCoreSlotUI Interface (REQUIRED per documentation)
	//==================================================================

	virtual void InitializeSlot_Implementation(const FSlotUIData& SlotData, const FItemUIData& ItemData) override;
	virtual void UpdateSlot_Implementation(const FSlotUIData& SlotData, const FItemUIData& ItemData) override;
	virtual void SetSelected_Implementation(bool bIsSelected) override;
	virtual void SetHighlighted_Implementation(bool bIsHighlighted, const FLinearColor& HighlightColor) override;
	virtual int32 GetSlotIndex_Implementation() const override;
	virtual FGuid GetItemInstanceID_Implementation() const override;
	virtual bool IsOccupied_Implementation() const override;
	virtual void SetLocked_Implementation(bool bIsLocked) override;

	//==================================================================
	// ISuspenseCoreDraggable Interface (REQUIRED per documentation)
	//==================================================================

	virtual bool CanBeDragged_Implementation() const override;
	virtual FDragDropUIData GetDragData_Implementation() const override;
	virtual void OnDragStarted_Implementation() override;
	virtual void OnDragEnded_Implementation(bool bWasDropped) override;
	virtual void UpdateDragVisual_Implementation(bool bIsValidTarget) override;

	//==================================================================
	// ISuspenseCoreDropTarget Interface (REQUIRED per documentation)
	//==================================================================

	virtual FSlotValidationResult CanAcceptDrop_Implementation(const UDragDropOperation* DragOperation) const override;
	virtual bool HandleDrop_Implementation(UDragDropOperation* DragOperation) override;
	virtual void NotifyDragEnter_Implementation(UDragDropOperation* DragOperation) override;
	virtual void NotifyDragLeave_Implementation() override;
	virtual int32 GetDropTargetSlot_Implementation() const override;

	//==================================================================
	// Slot Configuration
	//==================================================================

	/**
	 * Set slot index
	 * @param InIndex Slot index in container
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Slot")
	void SetSlotIndex(int32 InIndex) { SlotIndex = InIndex; }

	/**
	 * Get cached slot index (use interface GetSlotIndex for general access)
	 */
	int32 GetCachedSlotIndex() const { return SlotIndex; }

	/**
	 * Set grid position
	 * @param InPosition Position in grid (column, row)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Slot")
	void SetGridPosition(const FIntPoint& InPosition) { GridPosition = InPosition; }

	/**
	 * Get grid position
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Slot")
	FIntPoint GetGridPosition() const { return GridPosition; }

	/**
	 * Set slot visual size
	 * @param InSize Size in pixels
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Slot")
	void SetSlotSize(const FVector2D& InSize);

	/**
	 * Set base cell size (for multi-cell icon calculations)
	 * @param InSize Cell size in pixels
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Slot")
	void SetCellSize(float InSize) { CellSizePixels = InSize; }

	/**
	 * Get base cell size
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Slot")
	float GetCellSize() const { return CellSizePixels; }

	/**
	 * Set multi-cell item size (for anchor slots)
	 * When this is > 1x1, the icon will be scaled to fit the entire item area.
	 * @param InSize Item size in grid cells
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Slot")
	void SetMultiCellItemSize(const FIntPoint& InSize);

	/**
	 * Get current multi-cell item size
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Slot")
	FIntPoint GetMultiCellItemSize() const { return MultiCellItemSize; }

	/**
	 * Calculate icon size for multi-cell item
	 * @return Icon size in pixels based on cell size and item grid size
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Slot")
	FVector2D CalculateMultiCellIconSize() const;

	//==================================================================
	// Slot Data
	//==================================================================

	/**
	 * Update slot with extended data (internal use, prefer interface UpdateSlot)
	 * @param SlotData Slot state data
	 * @param ItemData Item data (if occupied)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Slot")
	void UpdateSlotData(const FSuspenseCoreSlotUIData& SlotData, const FSuspenseCoreItemUIData& ItemData);

	/**
	 * Clear slot content
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Slot")
	void ClearSlot();

	/**
	 * Get current slot data
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Slot")
	const FSuspenseCoreSlotUIData& GetSlotData() const { return CachedSlotData; }

	/**
	 * Get current item data
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Slot")
	const FSuspenseCoreItemUIData& GetItemData() const { return CachedItemData; }

	/**
	 * Is slot empty
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Slot")
	bool IsEmpty() const { return !CachedSlotData.IsOccupied(); }

	/**
	 * Is this the anchor/primary slot for an item
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Slot")
	bool IsAnchorSlot() const { return CachedSlotData.bIsAnchor; }

	//==================================================================
	// Highlight State
	//==================================================================

	/**
	 * Set highlight state
	 * @param NewState New highlight state
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Slot")
	void SetHighlightState(ESuspenseCoreUISlotState NewState);

	/**
	 * Get current highlight state
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Slot")
	ESuspenseCoreUISlotState GetHighlightState() const { return CurrentHighlightState; }

	//==================================================================
	// Blueprint Events
	//==================================================================

	/** Called when slot data is updated */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Slot", meta = (DisplayName = "On Slot Updated"))
	void K2_OnSlotUpdated(const FSuspenseCoreSlotUIData& SlotData, const FSuspenseCoreItemUIData& ItemData);

	/** Called when highlight state changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Slot", meta = (DisplayName = "On Highlight Changed"))
	void K2_OnHighlightChanged(ESuspenseCoreUISlotState NewState);

protected:
	//==================================================================
	// Visual Updates
	//==================================================================

	/** Update visuals based on slot data */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|UI|Slot")
	void UpdateVisuals();
	virtual void UpdateVisuals_Implementation();

	/** Update highlight border based on state */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|UI|Slot")
	void UpdateHighlightVisual(ESuspenseCoreUISlotState State);
	virtual void UpdateHighlightVisual_Implementation(ESuspenseCoreUISlotState State);

	/** Get color for highlight state */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|UI|Slot")
	FLinearColor GetHighlightColor(ESuspenseCoreUISlotState State);
	virtual FLinearColor GetHighlightColor_Implementation(ESuspenseCoreUISlotState State);

	//==================================================================
	// Widget References (Bind in Blueprint)
	//==================================================================

	/** Size container */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<USizeBox> SlotSizeBox;

	/** Background border */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UBorder> BackgroundBorder;

	/** Highlight border */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UBorder> HighlightBorder;

	/** Item icon image */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UImage> ItemIcon;

	/** Stack count text */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UTextBlock> StackCountText;

	/** Condition indicator */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UImage> ConditionIndicator;

	//==================================================================
	// Configuration
	//==================================================================

	/** Background color for empty slot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration|Colors")
	FLinearColor EmptySlotColor;

	/** Background color for occupied slot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration|Colors")
	FLinearColor OccupiedSlotColor;

	/** Background color for locked slot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration|Colors")
	FLinearColor LockedSlotColor;

	/** Highlight color for normal state */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration|Colors")
	FLinearColor NormalHighlightColor;

	/** Highlight color for hovered state */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration|Colors")
	FLinearColor HoveredHighlightColor;

	/** Highlight color for selected state */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration|Colors")
	FLinearColor SelectedHighlightColor;

	/** Highlight color for valid drop */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration|Colors")
	FLinearColor ValidDropColor;

	/** Highlight color for invalid drop */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration|Colors")
	FLinearColor InvalidDropColor;

	/** Show stack count only above threshold */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration")
	int32 StackCountDisplayThreshold;

private:
	//==================================================================
	// State
	//==================================================================

	/** Slot index in container */
	int32 SlotIndex;

	/** Grid position (column, row) */
	FIntPoint GridPosition;

	/** Cached slot data */
	FSuspenseCoreSlotUIData CachedSlotData;

	/** Cached item data */
	FSuspenseCoreItemUIData CachedItemData;

	/** Current highlight state */
	ESuspenseCoreUISlotState CurrentHighlightState;

	//==================================================================
	// Multi-Cell Support
	//==================================================================

	/** Base cell size in pixels */
	float CellSizePixels = 64.0f;

	/** Multi-cell item size (for anchor slots with items spanning multiple cells) */
	FIntPoint MultiCellItemSize = FIntPoint(1, 1);

	/** Icon scale factor for multi-cell items (0.85 = 85% of area) */
	static constexpr float MultiCellIconScale = 0.85f;
};
