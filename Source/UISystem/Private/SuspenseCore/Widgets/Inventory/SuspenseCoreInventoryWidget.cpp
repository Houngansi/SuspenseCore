// SuspenseCoreInventoryWidget.cpp
// SuspenseCore - Grid-based Inventory Widget Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/Inventory/SuspenseCoreInventoryWidget.h"
#include "SuspenseCore/Widgets/Inventory/SuspenseCoreInventorySlotWidget.h"
#include "SuspenseCore/Subsystems/SuspenseCoreUIManager.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/CanvasPanel.h"

//==================================================================
// Constructor
//==================================================================

USuspenseCoreInventoryWidget::USuspenseCoreInventoryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, GridSize(10, 5)
	, SlotSizePixels(64.0f)
	, SlotGapPixels(2.0f)
	, RotateKey(EKeys::R)
	, QuickEquipKey(EKeys::E)
	, QuickTransferKey(EKeys::LeftControl)
	, HoveredSlotIndex(INDEX_NONE)
	, LastClickTime(0.0)
	, LastClickedSlot(INDEX_NONE)
{
}

//==================================================================
// UUserWidget Interface
//==================================================================

void USuspenseCoreInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Create initial slot widgets if we have a bound provider
	if (IsBoundToProvider())
	{
		CreateSlotWidgets();
	}
}

void USuspenseCoreInventoryWidget::NativeDestruct()
{
	ClearSlotWidgets();
	Super::NativeDestruct();
}

FReply USuspenseCoreInventoryWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	FKey Key = InKeyEvent.GetKey();

	// Handle rotation key
	if (Key == RotateKey)
	{
		HandleRotationInput();
		return FReply::Handled();
	}

	// Handle quick equip on hovered slot
	if (Key == QuickEquipKey && HoveredSlotIndex != INDEX_NONE)
	{
		// Request equip via provider
		// This would be implemented based on your action system
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply USuspenseCoreInventoryWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FVector2D LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	int32 SlotIndex = GetSlotAtLocalPosition(LocalPos);

	if (SlotIndex != INDEX_NONE)
	{
		bool bRightClick = InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton;

		// Check for double click
		double CurrentTime = FPlatformTime::Seconds();
		if (SlotIndex == LastClickedSlot && (CurrentTime - LastClickTime) < DoubleClickThreshold)
		{
			K2_OnSlotDoubleClicked(SlotIndex);
			LastClickedSlot = INDEX_NONE;
		}
		else
		{
			HandleSlotClicked(SlotIndex, bRightClick);
			LastClickedSlot = SlotIndex;
			LastClickTime = CurrentTime;
		}

		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply USuspenseCoreInventoryWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply USuspenseCoreInventoryWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FVector2D LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	int32 SlotIndex = GetSlotAtLocalPosition(LocalPos);

	if (SlotIndex != HoveredSlotIndex)
	{
		// Clear previous hover
		if (HoveredSlotIndex != INDEX_NONE)
		{
			SetSlotHighlight(HoveredSlotIndex, ESuspenseCoreUISlotState::Normal);
		}

		HoveredSlotIndex = SlotIndex;

		// Set new hover
		if (HoveredSlotIndex != INDEX_NONE)
		{
			HandleSlotHovered(HoveredSlotIndex);
		}
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void USuspenseCoreInventoryWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
}

void USuspenseCoreInventoryWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	// Clear hover state
	if (HoveredSlotIndex != INDEX_NONE)
	{
		SetSlotHighlight(HoveredSlotIndex, ESuspenseCoreUISlotState::Normal);
		HoveredSlotIndex = INDEX_NONE;
	}

	Super::NativeOnMouseLeave(InMouseEvent);
}

//==================================================================
// Grid Configuration
//==================================================================

void USuspenseCoreInventoryWidget::SetGridSize(const FIntPoint& InGridSize)
{
	if (GridSize != InGridSize)
	{
		GridSize = InGridSize;

		// Recreate slot widgets
		ClearSlotWidgets();
		CreateSlotWidgets();
	}
}

void USuspenseCoreInventoryWidget::SetSlotSizePixels(float InSize)
{
	if (!FMath::IsNearlyEqual(SlotSizePixels, InSize))
	{
		SlotSizePixels = InSize;
		// Update slot widget sizes if needed
	}
}

//==================================================================
// ISuspenseCoreUIContainer Overrides
//==================================================================

UWidget* USuspenseCoreInventoryWidget::GetSlotWidget(int32 SlotIndex) const
{
	if (SlotIndex >= 0 && SlotIndex < SlotWidgets.Num())
	{
		return SlotWidgets[SlotIndex];
	}
	return nullptr;
}

TArray<UWidget*> USuspenseCoreInventoryWidget::GetAllSlotWidgets() const
{
	TArray<UWidget*> Result;
	for (USuspenseCoreInventorySlotWidget* Slot : SlotWidgets)
	{
		Result.Add(Slot);
	}
	return Result;
}

int32 USuspenseCoreInventoryWidget::GetSlotAtLocalPosition(const FVector2D& LocalPosition) const
{
	if (LocalPosition.X < 0 || LocalPosition.Y < 0)
	{
		return INDEX_NONE;
	}

	// Calculate grid position from local coordinates
	float TotalSlotSize = SlotSizePixels + SlotGapPixels;
	int32 Column = FMath::FloorToInt(LocalPosition.X / TotalSlotSize);
	int32 Row = FMath::FloorToInt(LocalPosition.Y / TotalSlotSize);

	FIntPoint GridPos(Column, Row);
	return GridPosToSlotIndex(GridPos);
}

void USuspenseCoreInventoryWidget::SetSlotHighlight(int32 SlotIndex, ESuspenseCoreUISlotState State)
{
	if (SlotIndex >= 0 && SlotIndex < SlotWidgets.Num() && SlotWidgets[SlotIndex])
	{
		SlotWidgets[SlotIndex]->SetHighlightState(State);
	}
}

//==================================================================
// Grid Utilities
//==================================================================

FIntPoint USuspenseCoreInventoryWidget::SlotIndexToGridPos(int32 SlotIndex) const
{
	if (SlotIndex < 0 || GridSize.X <= 0)
	{
		return FIntPoint(-1, -1);
	}

	int32 Column = SlotIndex % GridSize.X;
	int32 Row = SlotIndex / GridSize.X;

	return FIntPoint(Column, Row);
}

int32 USuspenseCoreInventoryWidget::GridPosToSlotIndex(const FIntPoint& GridPos) const
{
	if (!IsValidGridPos(GridPos))
	{
		return INDEX_NONE;
	}

	return GridPos.Y * GridSize.X + GridPos.X;
}

bool USuspenseCoreInventoryWidget::IsValidGridPos(const FIntPoint& GridPos) const
{
	return GridPos.X >= 0 && GridPos.X < GridSize.X &&
		   GridPos.Y >= 0 && GridPos.Y < GridSize.Y;
}

TArray<int32> USuspenseCoreInventoryWidget::GetOccupiedSlots(const FIntPoint& GridPos, const FIntPoint& ItemSize) const
{
	TArray<int32> Result;

	for (int32 DY = 0; DY < ItemSize.Y; ++DY)
	{
		for (int32 DX = 0; DX < ItemSize.X; ++DX)
		{
			FIntPoint CheckPos = GridPos + FIntPoint(DX, DY);
			int32 SlotIndex = GridPosToSlotIndex(CheckPos);
			if (SlotIndex != INDEX_NONE)
			{
				Result.Add(SlotIndex);
			}
		}
	}

	return Result;
}

//==================================================================
// Drag Support
//==================================================================

void USuspenseCoreInventoryWidget::HighlightDropSlots(const FIntPoint& ItemSize, int32 TargetSlot, bool bIsValid)
{
	// Clear all highlights first
	ClearHighlights();

	if (TargetSlot == INDEX_NONE)
	{
		return;
	}

	// Get grid position of target
	FIntPoint GridPos = SlotIndexToGridPos(TargetSlot);

	// Get all slots that would be occupied
	TArray<int32> OccupiedSlots = GetOccupiedSlots(GridPos, ItemSize);

	// Set highlight state
	ESuspenseCoreUISlotState State = bIsValid ? ESuspenseCoreUISlotState::ValidDrop : ESuspenseCoreUISlotState::InvalidDrop;

	for (int32 SlotIndex : OccupiedSlots)
	{
		SetSlotHighlight(SlotIndex, State);
	}
}

//==================================================================
// Override Points from Base
//==================================================================

void USuspenseCoreInventoryWidget::CreateSlotWidgets_Implementation()
{
	if (!SlotGrid || !SlotWidgetClass)
	{
		return;
	}

	// Clear existing
	SlotGrid->ClearChildren();
	SlotWidgets.Empty();

	int32 TotalSlots = GridSize.X * GridSize.Y;
	SlotWidgets.Reserve(TotalSlots);

	// Create slot widget for each grid cell
	for (int32 SlotIndex = 0; SlotIndex < TotalSlots; ++SlotIndex)
	{
		FIntPoint GridPos = SlotIndexToGridPos(SlotIndex);

		USuspenseCoreInventorySlotWidget* SlotWidget = CreateWidget<USuspenseCoreInventorySlotWidget>(GetOwningPlayer(), SlotWidgetClass);
		if (!SlotWidget)
		{
			continue;
		}

		// Configure slot
		SlotWidget->SetSlotIndex(SlotIndex);
		SlotWidget->SetGridPosition(GridPos);
		SlotWidget->SetSlotSize(FVector2D(SlotSizePixels, SlotSizePixels));

		// Add to grid at correct position
		UUniformGridSlot* GridSlot = SlotGrid->AddChildToUniformGrid(SlotWidget, GridPos.Y, GridPos.X);
		if (GridSlot)
		{
			GridSlot->SetHorizontalAlignment(HAlign_Fill);
			GridSlot->SetVerticalAlignment(VAlign_Fill);
		}

		SlotWidgets.Add(SlotWidget);
	}
}

void USuspenseCoreInventoryWidget::UpdateSlotWidget_Implementation(
	int32 SlotIndex,
	const FSuspenseCoreSlotUIData& SlotData,
	const FSuspenseCoreItemUIData& ItemData)
{
	if (SlotIndex < 0 || SlotIndex >= SlotWidgets.Num())
	{
		return;
	}

	USuspenseCoreInventorySlotWidget* SlotWidget = SlotWidgets[SlotIndex];
	if (!SlotWidget)
	{
		return;
	}

	// Update slot with data
	SlotWidget->UpdateSlot(SlotData, ItemData);
}

void USuspenseCoreInventoryWidget::ClearSlotWidgets_Implementation()
{
	if (SlotGrid)
	{
		SlotGrid->ClearChildren();
	}
	SlotWidgets.Empty();
}

//==================================================================
// Input Handling
//==================================================================

void USuspenseCoreInventoryWidget::HandleSlotClicked(int32 SlotIndex, bool bRightClick)
{
	// Select the slot
	SetSelectedSlot(SlotIndex);

	// Notify Blueprint
	K2_OnSlotClicked(SlotIndex, bRightClick);

	// If right click, show context menu
	if (bRightClick)
	{
		ShowContextMenu(SlotIndex, FVector2D::ZeroVector); // Screen position would need to be passed
	}
}

void USuspenseCoreInventoryWidget::HandleSlotHovered(int32 SlotIndex)
{
	// Set hover highlight
	SetSlotHighlight(SlotIndex, ESuspenseCoreUISlotState::Hovered);

	// Show tooltip for slot content
	ShowSlotTooltip(SlotIndex);
}

void USuspenseCoreInventoryWidget::HandleRotationInput()
{
	// If we're dragging, toggle rotation
	USuspenseCoreUIManager* UIManager = USuspenseCoreUIManager::Get(this);
	if (UIManager && UIManager->IsDragging())
	{
		// Toggle rotation on drag data
		// This would need access to modify the drag data
		K2_OnRotationToggled(true);
	}
}
