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
class UUniformGridPanel;
class UCanvasPanel;

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
	 * Get grid dimensions
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Inventory")
	FIntPoint GetGridSize() const { return GridSize; }

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

	virtual UWidget* GetSlotWidget(int32 SlotIndex) const override;
	virtual TArray<UWidget*> GetAllSlotWidgets() const override;
	virtual int32 GetSlotAtLocalPosition(const FVector2D& LocalPosition) const override;
	virtual void SetSlotHighlight(int32 SlotIndex, ESuspenseCoreUISlotState State) override;

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

	/** Handle slot hovered */
	void HandleSlotHovered(int32 SlotIndex);

	/** Handle rotation key */
	void HandleRotationInput();

	//==================================================================
	// Widget References (Bind in Blueprint)
	//==================================================================

	/** Grid panel containing slots */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UUniformGridPanel> SlotGrid;

	/** Canvas for item overlays */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UCanvasPanel> ItemOverlayCanvas;

	//==================================================================
	// Configuration
	//==================================================================

	/** Grid dimensions (columns x rows) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration")
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
	// State
	//==================================================================

	/** Currently hovered slot index */
	int32 HoveredSlotIndex;

	/** Last click time for double-click detection */
	double LastClickTime;

	/** Last clicked slot for double-click detection */
	int32 LastClickedSlot;

	/** Double click threshold in seconds */
	static constexpr double DoubleClickThreshold = 0.3;
};
