// SuspenseCoreDragDropOperation.cpp
// SuspenseCore - Drag-Drop Operation Handler Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/DragDrop/SuspenseCoreDragDropOperation.h"
#include "SuspenseCore/Widgets/DragDrop/SuspenseCoreDragVisualWidget.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreUIContainer.h"
#include "SuspenseCore/Subsystems/SuspenseCoreUIManager.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"

//==================================================================
// Constructor
//==================================================================

USuspenseCoreDragDropOperation::USuspenseCoreDragDropOperation()
	: CurrentHoverSlot(INDEX_NONE)
{
}

//==================================================================
// Static Creation
//==================================================================

USuspenseCoreDragDropOperation* USuspenseCoreDragDropOperation::CreateDrag(
	APlayerController* PC,
	const FSuspenseCoreDragData& InDragData,
	TSubclassOf<USuspenseCoreDragVisualWidget> VisualWidgetClass)
{
	if (!PC || !InDragData.bIsValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateDrag: Invalid parameters"));
		return nullptr;
	}

	USuspenseCoreDragDropOperation* Operation = NewObject<USuspenseCoreDragDropOperation>();
	if (Operation)
	{
		Operation->Initialize(PC, InDragData, VisualWidgetClass);
	}

	return Operation;
}

//==================================================================
// Drag State
//==================================================================

void USuspenseCoreDragDropOperation::ToggleRotation()
{
	SetRotation(!DragData.bIsRotatedDuringDrag);
}

void USuspenseCoreDragDropOperation::SetRotation(bool bRotated)
{
	if (DragData.bIsRotatedDuringDrag != bRotated)
	{
		DragData.ToggleRotation();

		// Update visual
		if (DragVisual)
		{
			DragVisual->SetRotation(bRotated);
		}
	}
}

FIntPoint USuspenseCoreDragDropOperation::GetEffectiveSize() const
{
	return DragData.GetEffectiveDragSize();
}

//==================================================================
// Drop Validation
//==================================================================

void USuspenseCoreDragDropOperation::SetHoverTarget(TScriptInterface<ISuspenseCoreUIContainer> Container, int32 SlotIndex)
{
	// Clear previous hover highlighting
	if (CurrentHoverContainer && CurrentHoverSlot != INDEX_NONE)
	{
		ISuspenseCoreUIContainer* PrevContainer = CurrentHoverContainer.GetInterface();
		if (PrevContainer)
		{
			PrevContainer->ClearHighlights();
		}
	}

	CurrentHoverContainer = Container;
	CurrentHoverSlot = SlotIndex;

	// Apply new hover highlighting
	if (CurrentHoverContainer && CurrentHoverSlot != INDEX_NONE)
	{
		ISuspenseCoreUIContainer* NewContainer = CurrentHoverContainer.GetInterface();
		if (NewContainer)
		{
			NewContainer->HighlightDropTarget(DragData, CurrentHoverSlot);
		}
	}
}

void USuspenseCoreDragDropOperation::UpdateDropValidity(bool bCanDrop)
{
	if (DragVisual)
	{
		DragVisual->SetDropValidity(bCanDrop);
	}
}

//==================================================================
// UDragDropOperation Overrides
//==================================================================

void USuspenseCoreDragDropOperation::Dragged_Implementation(const FPointerEvent& PointerEvent)
{
	Super::Dragged_Implementation(PointerEvent);

	// Update visual position
	if (DragVisual)
	{
		DragVisual->UpdatePosition(PointerEvent.GetScreenSpacePosition());
	}
}

void USuspenseCoreDragDropOperation::DragCancelled_Implementation(const FPointerEvent& PointerEvent)
{
	Super::DragCancelled_Implementation(PointerEvent);

	// Clear hover state
	SetHoverTarget(nullptr, INDEX_NONE);

	// Hide visual
	if (DragVisual)
	{
		DragVisual->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Notify UIManager
	if (USuspenseCoreUIManager* UIManager = USuspenseCoreUIManager::Get(OwningPC.Get()))
	{
		UIManager->CancelDragOperation();
	}

	// Broadcast event
	OnSuspenseCoreDragCancelled.Broadcast(DragData);

	UE_LOG(LogTemp, Verbose, TEXT("Drag cancelled for item: %s"), *DragData.Item.DisplayName.ToString());
}

void USuspenseCoreDragDropOperation::Drop_Implementation(const FPointerEvent& PointerEvent)
{
	Super::Drop_Implementation(PointerEvent);

	bool bSuccess = false;

	// Try to complete drop on current target
	if (CurrentHoverContainer && CurrentHoverSlot != INDEX_NONE)
	{
		ISuspenseCoreUIContainer* Container = CurrentHoverContainer.GetInterface();
		if (Container)
		{
			bSuccess = Container->HandleDrop(DragData, CurrentHoverSlot);
		}
	}

	// Clear hover state
	SetHoverTarget(nullptr, INDEX_NONE);

	// Hide visual
	if (DragVisual)
	{
		DragVisual->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Broadcast event
	OnDropCompleted.Broadcast(DragData, bSuccess);

	UE_LOG(LogTemp, Verbose, TEXT("Drop %s for item: %s at slot %d"),
		bSuccess ? TEXT("succeeded") : TEXT("failed"),
		*DragData.Item.DisplayName.ToString(),
		CurrentHoverSlot);
}

//==================================================================
// Initialization
//==================================================================

void USuspenseCoreDragDropOperation::Initialize(
	APlayerController* PC,
	const FSuspenseCoreDragData& InDragData,
	TSubclassOf<USuspenseCoreDragVisualWidget> VisualWidgetClass)
{
	OwningPC = PC;
	DragData = InDragData;
	CurrentHoverSlot = INDEX_NONE;

	// Create drag visual
	DragVisual = CreateDragVisual(PC, VisualWidgetClass);

	// Set default visual widget for base DragDropOperation
	if (DragVisual)
	{
		DefaultDragVisual = DragVisual;
		DragVisual->InitializeDrag(DragData);
	}

	// CRITICAL: Set Pivot for cursor-relative positioning
	// (0,0) = top-left corner at cursor, (0.5,0.5) = center at cursor
	// Use top-left so drag visual appears below and right of cursor
	Pivot = FVector2D(0.0f, 0.0f);

	// Set Offset to account for where user clicked within the item slot
	// DragOffset should be negative if we want the visual to appear at original item position
	Offset = DragData.DragOffset;

	// Notify UIManager
	if (USuspenseCoreUIManager* UIManager = USuspenseCoreUIManager::Get(PC))
	{
		UIManager->StartDragOperation(DragData);
	}

	// Broadcast event
	OnDragStarted.Broadcast(DragData);

	UE_LOG(LogTemp, Verbose, TEXT("Drag started for item: %s from slot %d"),
		*DragData.Item.DisplayName.ToString(), DragData.SourceSlot);
}

USuspenseCoreDragVisualWidget* USuspenseCoreDragDropOperation::CreateDragVisual(
	APlayerController* PC,
	TSubclassOf<USuspenseCoreDragVisualWidget> VisualWidgetClass)
{
	if (!PC)
	{
		return nullptr;
	}

	// Use provided class - MUST be a Blueprint with BindWidget components
	TSubclassOf<USuspenseCoreDragVisualWidget> WidgetClass = VisualWidgetClass;
	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateDragVisual: VisualWidgetClass is null! DragVisualWidgetClass must be set in container widget Blueprint defaults."));
		return nullptr;
	}

	// Create widget - DO NOT AddToViewport!
	// UE5's DragDropOperation automatically displays DefaultDragVisual during drag.
	// Adding to viewport manually causes DUPLICATE visuals (one huge at 0,0 and one normal following cursor).
	USuspenseCoreDragVisualWidget* Visual = CreateWidget<USuspenseCoreDragVisualWidget>(PC, WidgetClass);

	return Visual;
}
