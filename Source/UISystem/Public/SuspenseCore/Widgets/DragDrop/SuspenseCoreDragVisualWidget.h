// SuspenseCoreDragVisualWidget.h
// SuspenseCore - Drag Visual (Ghost) Widget
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUITypes.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUIContainerTypes.h"
#include "SuspenseCoreDragVisualWidget.generated.h"

// Forward declarations
class UImage;
class UTextBlock;
class USizeBox;
class UBorder;

/**
 * USuspenseCoreDragVisualWidget
 *
 * Visual representation of item being dragged.
 * Follows cursor during drag operation with DPI-aware positioning.
 *
 * FEATURES:
 * - Semi-transparent item icon
 * - Shows item size (for multi-cell items)
 * - Visual rotation indicator (R key support)
 * - Stack quantity display
 * - Valid/invalid drop color indicator
 * - DPI-safe cursor tracking (works on 4K, ultrawide, windowed mode)
 *
 * @see USuspenseCoreUIManager
 * @see FSuspenseCoreDragData
 */
UCLASS(BlueprintType, Blueprintable)
class UISYSTEM_API USuspenseCoreDragVisualWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreDragVisualWidget(const FObjectInitializer& ObjectInitializer);

	//==================================================================
	// UUserWidget Interface
	//==================================================================

	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	//==================================================================
	// Drag Visual Control
	//==================================================================

	/**
	 * Initialize drag visual with item data
	 * @param DragData Complete drag operation data
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|DragDrop")
	void InitializeDrag(const FSuspenseCoreDragData& DragData);

	/**
	 * Update visual position based on current cursor location
	 * Uses DPI-aware positioning via GetMousePositionScaledByDPI
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|DragDrop")
	void UpdatePositionFromCursor();

	/**
	 * Set drop validity state (changes visual indicator)
	 * @param bCanDrop True if drop is valid
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|DragDrop")
	void SetDropValidity(bool bCanDrop);

	/**
	 * Toggle rotation visual
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|DragDrop")
	void ToggleRotation();

	/**
	 * Set rotation state directly
	 * @param bRotated New rotation state
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|DragDrop")
	void SetRotation(bool bRotated);

	/**
	 * Get current rotation state
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|DragDrop")
	bool IsRotated() const { return bIsRotated; }

	/**
	 * Get current drag data
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|DragDrop")
	const FSuspenseCoreDragData& GetDragData() const { return CurrentDragData; }

	//==================================================================
	// Blueprint Events
	//==================================================================

	/** Called when drag is initialized */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|DragDrop", meta = (DisplayName = "On Drag Initialized"))
	void K2_OnDragInitialized(const FSuspenseCoreDragData& DragData);

	/** Called when rotation changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|DragDrop", meta = (DisplayName = "On Rotation Changed"))
	void K2_OnRotationChanged(bool bNewRotated);

	/** Called when drop validity changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|DragDrop", meta = (DisplayName = "On Drop Validity Changed"))
	void K2_OnDropValidityChanged(bool bCanDrop);

protected:
	//==================================================================
	// Visual Updates
	//==================================================================

	/** Update visuals based on drag data */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|UI|DragDrop")
	void UpdateVisuals();
	virtual void UpdateVisuals_Implementation();

	/** Update size based on item dimensions */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|UI|DragDrop")
	void UpdateSize();
	virtual void UpdateSize_Implementation();

	//==================================================================
	// Widget References (Bind in Blueprint) - REQUIRED components
	// Blueprint MUST have widgets with these EXACT names or compilation will fail
	//==================================================================

	/** Size container - REQUIRED for multi-cell item sizing */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Widgets")
	TObjectPtr<USizeBox> SizeContainer;

	/** Border for drop validity indicator - REQUIRED for valid/invalid drop feedback */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Widgets")
	TObjectPtr<UBorder> ValidityBorder;

	/** Item icon - REQUIRED for visual representation */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Widgets")
	TObjectPtr<UImage> ItemIcon;

	/** Quantity text - optional (not all items have quantity) */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UTextBlock> QuantityText;

	//==================================================================
	// Configuration
	//==================================================================

	/** Size per grid cell in pixels */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
	float CellSizePixels;

	/** Opacity when dragging */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
	float DragOpacity;

	/** Color when drop is valid */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
	FLinearColor ValidDropColor;

	/** Color when drop is invalid */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
	FLinearColor InvalidDropColor;

	/** Color when no drop target */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
	FLinearColor NeutralColor;

private:
	//==================================================================
	// State
	//==================================================================

	/** Current drag data */
	FSuspenseCoreDragData CurrentDragData;

	/** Drag offset from cursor - calculated to center item under cursor */
	FVector2D DragOffset;

	/** Is item currently rotated */
	bool bIsRotated;

	/** Current drop validity */
	bool bCurrentDropValid;

	/** Current item size accounting for rotation */
	FIntPoint CurrentSize;

	/** Recalculate center offset when size changes (e.g., rotation) */
	void RecalculateCenterOffset();
};
