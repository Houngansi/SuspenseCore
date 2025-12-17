// SuspenseCoreBaseSlotWidget.h
// SuspenseCore - Base Slot Widget for Inventory and Equipment
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUITypes.h"
#include "SuspenseCoreBaseSlotWidget.generated.h"

// Forward declarations
class UImage;
class UTextBlock;
class UBorder;
class USizeBox;

/**
 * USuspenseCoreBaseSlotWidget
 *
 * Abstract base class for all slot widgets (inventory slots, equipment slots).
 * Provides common functionality for displaying items, highlighting, and drag-drop.
 *
 * BINDWIDGET COMPONENTS (create in Blueprint):
 * - SlotSizeBox: USizeBox - Controls slot dimensions
 * - BackgroundBorder: UBorder - Background with state-based colors
 * - HighlightBorder: UBorder - Highlight overlay for selection/hover
 * - ItemIcon: UImage - Item icon display
 * - StackCountText: UTextBlock - Stack quantity (optional)
 *
 * INHERITANCE:
 * - USuspenseCoreInventorySlotWidget (grid-based inventory)
 * - USuspenseCoreEquipmentSlotWidget (named equipment slots)
 *
 * @see USuspenseCoreInventorySlotWidget
 * @see USuspenseCoreEquipmentSlotWidget
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class UISYSTEM_API USuspenseCoreBaseSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreBaseSlotWidget(const FObjectInitializer& ObjectInitializer);

	//==================================================================
	// UUserWidget Interface
	//==================================================================

	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;

	//==================================================================
	// Common Slot Interface
	//==================================================================

	/**
	 * Update slot with data
	 * @param SlotData Slot state data
	 * @param ItemData Item data (if occupied)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Slot")
	virtual void UpdateSlot(const FSuspenseCoreSlotUIData& SlotData, const FSuspenseCoreItemUIData& ItemData);

	/**
	 * Clear slot content
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Slot")
	virtual void ClearSlot();

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

	//==================================================================
	// Slot Size
	//==================================================================

	/**
	 * Set slot visual size
	 * @param InSize Size in pixels
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Slot")
	virtual void SetSlotSize(const FVector2D& InSize);

	/**
	 * Get slot visual size
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Slot")
	FVector2D GetSlotSize() const { return SlotSize; }

	//==================================================================
	// Highlight State
	//==================================================================

	/**
	 * Set highlight state
	 * @param NewState New highlight state
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Slot")
	virtual void SetHighlightState(ESuspenseCoreUISlotState NewState);

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
	// Visual Updates (Override in subclasses)
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

	/** Update item icon display */
	virtual void UpdateItemIcon();

	/** Update stack count text */
	virtual void UpdateStackCount();

	//==================================================================
	// Widget References (Bind in Blueprint)
	// Create widgets with EXACTLY these names in your BP_SlotWidget
	//==================================================================

	/** Size container - controls slot dimensions */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Widgets")
	TObjectPtr<USizeBox> SlotSizeBox;

	/** Background border - state-based background color */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Widgets")
	TObjectPtr<UBorder> BackgroundBorder;

	/** Highlight border - selection/hover overlay */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Widgets")
	TObjectPtr<UBorder> HighlightBorder;

	/** Item icon image */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Widgets")
	TObjectPtr<UImage> ItemIcon;

	/** Stack count text (optional - only for stackable items) */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UTextBlock> StackCountText;

	//==================================================================
	// Configuration (EditAnywhere for Blueprint defaults)
	//==================================================================

	/** Background color for empty slot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|Colors")
	FLinearColor EmptySlotColor;

	/** Background color for occupied slot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|Colors")
	FLinearColor OccupiedSlotColor;

	/** Background color for locked slot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|Colors")
	FLinearColor LockedSlotColor;

	/** Highlight color for normal state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|Colors")
	FLinearColor NormalHighlightColor;

	/** Highlight color for hovered state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|Colors")
	FLinearColor HoveredHighlightColor;

	/** Highlight color for selected state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|Colors")
	FLinearColor SelectedHighlightColor;

	/** Highlight color for valid drop */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|Colors")
	FLinearColor ValidDropColor;

	/** Highlight color for invalid drop */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration|Colors")
	FLinearColor InvalidDropColor;

	/** Show stack count only above threshold */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
	int32 StackCountDisplayThreshold;

	//==================================================================
	// Cached Data
	//==================================================================

	/** Cached slot data */
	UPROPERTY(BlueprintReadOnly, Category = "State")
	FSuspenseCoreSlotUIData CachedSlotData;

	/** Cached item data */
	UPROPERTY(BlueprintReadOnly, Category = "State")
	FSuspenseCoreItemUIData CachedItemData;

	/** Current highlight state */
	UPROPERTY(BlueprintReadOnly, Category = "State")
	ESuspenseCoreUISlotState CurrentHighlightState;

	/** Slot visual size */
	UPROPERTY(BlueprintReadOnly, Category = "State")
	FVector2D SlotSize;
};
