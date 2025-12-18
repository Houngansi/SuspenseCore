// SuspenseCoreDragDropOperation.cpp
// SuspenseCore - Drag-Drop Operation Handler Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/DragDrop/SuspenseCoreDragDropOperation.h"
#include "SuspenseCore/Widgets/DragDrop/SuspenseCoreDragVisualWidget.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreUIContainer.h"
#include "SuspenseCore/Subsystems/SuspenseCoreUIManager.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Slate/SlateBrushAsset.h"
#include "Components/CanvasPanelSlot.h"

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

	// Position is now managed automatically by UE5's DefaultDragVisual system
	// No manual position update needed
}

void USuspenseCoreDragDropOperation::DragCancelled_Implementation(const FPointerEvent& PointerEvent)
{
	Super::DragCancelled_Implementation(PointerEvent);

	// Clear hover state
	SetHoverTarget(nullptr, INDEX_NONE);

	// Visibility is managed by UE5's DefaultDragVisual system
	// No manual hide needed - UE5 handles cleanup

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

	// Visibility is managed by UE5's DefaultDragVisual system
	// No manual hide needed - UE5 handles cleanup

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

	// Create drag visual - uses UE5's DefaultDragVisual system for automatic positioning
	DragVisual = CreateDragVisual(PC, VisualWidgetClass);

	if (DragVisual)
	{
		// Set offset from DragData - this is the offset from cursor to widget top-left
		// DragData.DragOffset was calculated as: SlotAbsolutePos - InitialClickPos
		// UE5's DefaultDragVisual will position widget at: CursorPos + Offset
		Offset = DragData.DragOffset;

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

	UE_LOG(LogTemp, Log, TEXT("Drag started for item: %s from slot %d, Offset: (%.1f, %.1f)"),
		*DragData.Item.DisplayName.ToString(), DragData.SourceSlot, Offset.X, Offset.Y);
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

	// Create widget - DO NOT add to viewport manually!
	// We'll use UE5's DefaultDragVisual system which handles positioning automatically
	USuspenseCoreDragVisualWidget* Visual = CreateWidget<USuspenseCoreDragVisualWidget>(PC, WidgetClass);
	if (Visual)
	{
		// Use UE5's built-in DefaultDragVisual system instead of manual positioning
		// This properly handles all coordinate systems and DPI scaling
		DefaultDragVisual = Visual;

		// Pivot determines where on the widget the cursor attaches
		// (0,0) = top-left, (0.5, 0.5) = center, (1,1) = bottom-right
		Pivot = EDragPivot::TopLeft;

		// Offset from cursor position (in local widget coordinates)
		// We'll update this in Initialize after we have DragData with the calculated offset
	}

	return Visual;
}
