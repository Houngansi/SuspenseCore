// SuspenseCoreDragDropOperation.cpp
// SuspenseCore - Drag-Drop Operation Handler Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/DragDrop/SuspenseCoreDragDropOperation.h"
#include "SuspenseCore/Widgets/DragDrop/SuspenseCoreDragVisualWidget.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreUIContainer.h"
#include "SuspenseCore/Subsystems/SuspenseCoreUIManager.h"
#include "GameFramework/PlayerController.h"

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

		// If hovering over container, refresh highlight for new item shape
		if (CurrentHoverContainer && CurrentHoverSlot != INDEX_NONE)
		{
			ISuspenseCoreUIContainer* Container = CurrentHoverContainer.GetInterface();
			if (Container)
			{
				Container->HighlightDropTarget(DragData, CurrentHoverSlot);
			}
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
	// Optimization: Skip if target hasn't changed
	// Prevents redundant highlight updates when mouse moves within same slot
	if (CurrentHoverContainer == Container && CurrentHoverSlot == SlotIndex)
	{
		return;
	}

	// Clear previous hover highlighting
	if (CurrentHoverContainer)
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
// Operation Lifecycle
//==================================================================

void USuspenseCoreDragDropOperation::FinishOperation()
{
	// Clear all highlights
	if (CurrentHoverContainer)
	{
		ISuspenseCoreUIContainer* Container = CurrentHoverContainer.GetInterface();
		if (Container)
		{
			Container->ClearHighlights();
		}
	}

	// CRITICAL: Remove widget from viewport to prevent memory leak
	// Just hiding (SetVisibility) leaves widget in memory indefinitely
	if (DragVisual)
	{
		DragVisual->RemoveFromParent();
		DragVisual = nullptr;
	}

	CurrentHoverContainer = nullptr;
	CurrentHoverSlot = INDEX_NONE;
}

//==================================================================
// UDragDropOperation Overrides
//==================================================================

void USuspenseCoreDragDropOperation::Dragged_Implementation(const FPointerEvent& PointerEvent)
{
	Super::Dragged_Implementation(PointerEvent);

	// Update visual position
	// This is called by Slate during drag - serves as backup in case NativeTick
	// doesn't run (can happen during Slate mouse capture)
	if (DragVisual)
	{
		DragVisual->UpdatePositionFromCursor();
	}
}

void USuspenseCoreDragDropOperation::DragCancelled_Implementation(const FPointerEvent& PointerEvent)
{
	// Notify UIManager first
	if (APlayerController* PC = OwningPC.Get())
	{
		if (USuspenseCoreUIManager* UIManager = USuspenseCoreUIManager::Get(PC))
		{
			UIManager->CancelDragOperation();
		}
	}

	// Broadcast event
	OnSuspenseCoreDragCancelled.Broadcast(DragData);

	// Cleanup
	FinishOperation();

	Super::DragCancelled_Implementation(PointerEvent);
}

void USuspenseCoreDragDropOperation::Drop_Implementation(const FPointerEvent& PointerEvent)
{
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

	// Broadcast event
	OnDropCompleted.Broadcast(DragData, bSuccess);

	// Cleanup
	FinishOperation();

	Super::Drop_Implementation(PointerEvent);
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

	if (DragVisual)
	{
		// Initialize visual (updates icon, size, etc.)
		DragVisual->InitializeDrag(DragData);
	}

	// Notify UIManager
	if (USuspenseCoreUIManager* UIManager = USuspenseCoreUIManager::Get(PC))
	{
		UIManager->StartDragOperation(DragData);
	}

	// Broadcast event
	OnDragStarted.Broadcast(DragData);
}

USuspenseCoreDragVisualWidget* USuspenseCoreDragDropOperation::CreateDragVisual(
	APlayerController* PC,
	TSubclassOf<USuspenseCoreDragVisualWidget> VisualWidgetClass)
{
	if (!PC || !VisualWidgetClass)
	{
		return nullptr;
	}

	USuspenseCoreDragVisualWidget* Visual = CreateWidget<USuspenseCoreDragVisualWidget>(PC, VisualWidgetClass);
	if (Visual)
	{
		// Add to viewport with high Z-order
		Visual->AddToViewport(9999);

		// Set anchors to top-left (0,0) so SetPositionInViewport works correctly
		Visual->SetAnchorsInViewport(FAnchors(0.f, 0.f, 0.f, 0.f));
		Visual->SetAlignmentInViewport(FVector2D(0.f, 0.f));
	}

	return Visual;
}
