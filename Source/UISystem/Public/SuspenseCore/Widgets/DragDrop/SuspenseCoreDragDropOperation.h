// SuspenseCoreDragDropOperation.h
// SuspenseCore - Drag-Drop Operation Handler
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUIContainerTypes.h"
#include "SuspenseCoreDragDropOperation.generated.h"

// Forward declarations
class USuspenseCoreDragVisualWidget;
class ISuspenseCoreUIContainer;

/**
 * Delegate for drag events
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSuspenseCoreDragEvent, const FSuspenseCoreDragData&, DragData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSuspenseCoreDropEvent, const FSuspenseCoreDragData&, DragData, bool, bSuccess);

/**
 * USuspenseCoreDragDropOperation
 *
 * Custom drag-drop operation for inventory items.
 * This is the "brain" of the drag-drop system - it manages:
 * - Drag visual lifecycle (creation, display, cleanup)
 * - Hover target tracking (which container/slot is under cursor)
 * - Rotation during drag (R key)
 * - Drop validation and execution
 *
 * USAGE:
 * ```cpp
 * USuspenseCoreDragDropOperation* Op = USuspenseCoreDragDropOperation::CreateDrag(PC, DragData);
 * if (Op)
 * {
 *     // Drag started - visual is created and tracking cursor
 * }
 * ```
 *
 * @see USuspenseCoreDragVisualWidget
 * @see FSuspenseCoreDragData
 */
UCLASS(BlueprintType, Blueprintable)
class UISYSTEM_API USuspenseCoreDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	USuspenseCoreDragDropOperation();

	//==================================================================
	// Static Creation
	//==================================================================

	/**
	 * Create and start a drag operation
	 * @param PC Player controller
	 * @param DragData Drag data
	 * @param VisualWidgetClass Optional custom visual widget class
	 * @return Created operation or nullptr if failed
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|DragDrop")
	static USuspenseCoreDragDropOperation* CreateDrag(
		APlayerController* PC,
		const FSuspenseCoreDragData& DragData,
		TSubclassOf<USuspenseCoreDragVisualWidget> VisualWidgetClass = nullptr);

	//==================================================================
	// Drag State
	//==================================================================

	/**
	 * Get current drag data
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|DragDrop")
	const FSuspenseCoreDragData& GetDragData() const { return DragData; }

	/**
	 * Get drag data (mutable for rotation updates)
	 */
	FSuspenseCoreDragData& GetMutableDragData() { return DragData; }

	/**
	 * Is item currently rotated
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|DragDrop")
	bool IsRotated() const { return DragData.bIsRotatedDuringDrag; }

	/**
	 * Toggle item rotation
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|DragDrop")
	void ToggleRotation();

	/**
	 * Set rotation state
	 * @param bRotated New rotation state
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|DragDrop")
	void SetRotation(bool bRotated);

	/**
	 * Get effective item size (accounting for rotation)
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|DragDrop")
	FIntPoint GetEffectiveSize() const;

	//==================================================================
	// Drop Validation
	//==================================================================

	/**
	 * Set current hover target
	 * @param Container Target container
	 * @param SlotIndex Target slot
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|DragDrop")
	void SetHoverTarget(TScriptInterface<ISuspenseCoreUIContainer> Container, int32 SlotIndex);

	/**
	 * Update drop validity visual
	 * @param bCanDrop True if drop is valid
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|DragDrop")
	void UpdateDropValidity(bool bCanDrop);

	/**
	 * Get current hover slot
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|DragDrop")
	int32 GetHoverSlot() const { return CurrentHoverSlot; }

	//==================================================================
	// Events
	//==================================================================

	/** Fired when drag starts */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|UI|DragDrop")
	FOnSuspenseCoreDragEvent OnDragStarted;

	/** Fired when drag ends (cancelled or dropped) */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|UI|DragDrop")
	FOnSuspenseCoreDragEvent OnSuspenseCoreDragCancelled;

	/** Fired when drop completes */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|UI|DragDrop")
	FOnSuspenseCoreDropEvent OnDropCompleted;

	//==================================================================
	// UDragDropOperation Overrides
	//==================================================================

	virtual void Dragged_Implementation(const FPointerEvent& PointerEvent) override;
	virtual void DragCancelled_Implementation(const FPointerEvent& PointerEvent) override;
	virtual void Drop_Implementation(const FPointerEvent& PointerEvent) override;

protected:
	//==================================================================
	// Initialization & Cleanup
	//==================================================================

	/** Initialize the operation */
	void Initialize(APlayerController* PC, const FSuspenseCoreDragData& InDragData, TSubclassOf<USuspenseCoreDragVisualWidget> VisualWidgetClass);

	/** Create drag visual widget */
	USuspenseCoreDragVisualWidget* CreateDragVisual(APlayerController* PC, TSubclassOf<USuspenseCoreDragVisualWidget> VisualWidgetClass);

	/** Cleanup operation - clear highlights and remove visual from viewport */
	void FinishOperation();

private:
	//==================================================================
	// State
	//==================================================================

	/** Complete drag data */
	UPROPERTY()
	FSuspenseCoreDragData DragData;

	/** Drag visual widget */
	UPROPERTY()
	TObjectPtr<USuspenseCoreDragVisualWidget> DragVisual;

	/** Current hover slot index */
	int32 CurrentHoverSlot;

	/** Current hover container */
	TScriptInterface<ISuspenseCoreUIContainer> CurrentHoverContainer;

	/** Owning player controller */
	UPROPERTY()
	TWeakObjectPtr<APlayerController> OwningPC;
};
