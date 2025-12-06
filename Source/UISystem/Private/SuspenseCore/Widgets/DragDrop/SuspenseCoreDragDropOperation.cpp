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

	// Use provided class or default
	TSubclassOf<USuspenseCoreDragVisualWidget> WidgetClass = VisualWidgetClass;
	if (!WidgetClass)
	{
		// Use default class - should be configured in project
		WidgetClass = USuspenseCoreDragVisualWidget::StaticClass();
	}

	// Create widget
	USuspenseCoreDragVisualWidget* Visual = CreateWidget<USuspenseCoreDragVisualWidget>(PC, WidgetClass);
	if (Visual)
	{
		// Add to viewport at high Z-order
		Visual->AddToViewport(1000);
	}

	return Visual;
}
