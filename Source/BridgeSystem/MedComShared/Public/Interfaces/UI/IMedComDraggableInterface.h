// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Types/UI/ContainerUITypes.h"
#include "IMedComDraggableInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UMedComDraggableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for draggable UI elements
 * Этот интерфейс остается без изменений, так как не использует DragDropOperation
 */
class MEDCOMSHARED_API IMedComDraggableInterface
{
	GENERATED_BODY()

public:
	/**
	 * Check if widget can be dragged
	 * @return True if dragging is allowed
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|DragDrop")
	bool CanBeDragged() const;

	/**
	 * Get drag data for this widget
	 * @return Drag drop data
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|DragDrop")
	FDragDropUIData GetDragData() const;

	/**
	 * Called when drag operation starts
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|DragDrop")
	void OnDragStarted();

	/**
	 * Called when drag operation ends
	 * @param bWasDropped Whether item was successfully dropped
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|DragDrop")
	void OnDragEnded(bool bWasDropped);

	/**
	 * Update visual state during drag
	 * @param bIsValidTarget Whether current target is valid
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|DragDrop")
	void UpdateDragVisual(bool bIsValidTarget);
};