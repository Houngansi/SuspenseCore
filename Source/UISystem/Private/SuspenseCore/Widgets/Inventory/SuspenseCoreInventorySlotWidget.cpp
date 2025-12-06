// SuspenseCoreInventorySlotWidget.cpp
// SuspenseCore - Individual Inventory Slot Widget Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/Inventory/SuspenseCoreInventorySlotWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"

//==================================================================
// Constructor
//==================================================================

USuspenseCoreInventorySlotWidget::USuspenseCoreInventorySlotWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, EmptySlotColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f))
	, OccupiedSlotColor(FLinearColor(0.15f, 0.15f, 0.15f, 0.9f))
	, LockedSlotColor(FLinearColor(0.3f, 0.1f, 0.1f, 0.8f))
	, NormalHighlightColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f))
	, HoveredHighlightColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.3f))
	, SelectedHighlightColor(FLinearColor(1.0f, 0.8f, 0.0f, 0.5f))
	, ValidDropColor(FLinearColor(0.0f, 1.0f, 0.0f, 0.4f))
	, InvalidDropColor(FLinearColor(1.0f, 0.0f, 0.0f, 0.4f))
	, StackCountDisplayThreshold(1)
	, SlotIndex(INDEX_NONE)
	, GridPosition(FIntPoint::ZeroValue)
	, CurrentHighlightState(ESuspenseCoreUISlotState::Normal)
{
}

//==================================================================
// UUserWidget Interface
//==================================================================

void USuspenseCoreInventorySlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Initialize visual state
	UpdateVisuals();
	UpdateHighlightVisual(ESuspenseCoreUISlotState::Normal);
}

void USuspenseCoreInventorySlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Set default visuals in editor
	if (BackgroundBorder)
	{
		BackgroundBorder->SetBrushColor(EmptySlotColor);
	}

	if (HighlightBorder)
	{
		HighlightBorder->SetBrushColor(NormalHighlightColor);
	}
}

//==================================================================
// Slot Configuration
//==================================================================

void USuspenseCoreInventorySlotWidget::SetSlotSize(const FVector2D& InSize)
{
	if (SlotSizeBox)
	{
		SlotSizeBox->SetWidthOverride(InSize.X);
		SlotSizeBox->SetHeightOverride(InSize.Y);
	}
}

//==================================================================
// Slot Data
//==================================================================

void USuspenseCoreInventorySlotWidget::UpdateSlot(const FSuspenseCoreSlotUIData& SlotData, const FSuspenseCoreItemUIData& ItemData)
{
	CachedSlotData = SlotData;
	CachedItemData = ItemData;

	// Update visuals
	UpdateVisuals();

	// Notify Blueprint
	K2_OnSlotUpdated(SlotData, ItemData);
}

void USuspenseCoreInventorySlotWidget::ClearSlot()
{
	CachedSlotData = FSuspenseCoreSlotUIData();
	CachedSlotData.SlotIndex = SlotIndex;
	CachedSlotData.bIsEmpty = true;
	CachedItemData = FSuspenseCoreItemUIData();

	UpdateVisuals();
}

//==================================================================
// Highlight State
//==================================================================

void USuspenseCoreInventorySlotWidget::SetHighlightState(ESuspenseCoreUISlotState NewState)
{
	if (CurrentHighlightState != NewState)
	{
		CurrentHighlightState = NewState;
		UpdateHighlightVisual(NewState);
		K2_OnHighlightChanged(NewState);
	}
}

//==================================================================
// Visual Updates
//==================================================================

void USuspenseCoreInventorySlotWidget::UpdateVisuals_Implementation()
{
	// Update background color based on state
	if (BackgroundBorder)
	{
		FLinearColor BgColor = EmptySlotColor;

		if (CachedSlotData.bIsLocked)
		{
			BgColor = LockedSlotColor;
		}
		else if (!CachedSlotData.bIsEmpty)
		{
			BgColor = OccupiedSlotColor;
		}

		BackgroundBorder->SetBrushColor(BgColor);
	}

	// Update item icon
	if (ItemIcon)
	{
		if (CachedSlotData.bIsPrimarySlot && CachedItemData.Icon)
		{
			ItemIcon->SetBrushFromTexture(CachedItemData.Icon);
			ItemIcon->SetVisibility(ESlateVisibility::Visible);

			// Handle rotation
			if (CachedItemData.bIsRotated)
			{
				ItemIcon->SetRenderTransformAngle(90.0f);
			}
			else
			{
				ItemIcon->SetRenderTransformAngle(0.0f);
			}
		}
		else
		{
			ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Update stack count
	if (StackCountText)
	{
		if (CachedSlotData.bIsPrimarySlot && CachedItemData.Quantity > StackCountDisplayThreshold)
		{
			StackCountText->SetText(FText::AsNumber(CachedItemData.Quantity));
			StackCountText->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			StackCountText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Update condition indicator
	if (ConditionIndicator)
	{
		if (CachedSlotData.bIsPrimarySlot && CachedItemData.bShowCondition)
		{
			// Color based on condition percentage
			FLinearColor ConditionColor = FLinearColor::LerpUsingHSV(
				FLinearColor::Red,
				FLinearColor::Green,
				CachedItemData.ConditionPercent);
			ConditionIndicator->SetColorAndOpacity(ConditionColor);
			ConditionIndicator->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			ConditionIndicator->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void USuspenseCoreInventorySlotWidget::UpdateHighlightVisual_Implementation(ESuspenseCoreUISlotState State)
{
	if (!HighlightBorder)
	{
		return;
	}

	FLinearColor Color = GetHighlightColor(State);
	HighlightBorder->SetBrushColor(Color);
}

FLinearColor USuspenseCoreInventorySlotWidget::GetHighlightColor_Implementation(ESuspenseCoreUISlotState State)
{
	switch (State)
	{
	case ESuspenseCoreUISlotState::Normal:
		return NormalHighlightColor;

	case ESuspenseCoreUISlotState::Hovered:
		return HoveredHighlightColor;

	case ESuspenseCoreUISlotState::Selected:
		return SelectedHighlightColor;

	case ESuspenseCoreUISlotState::ValidDrop:
		return ValidDropColor;

	case ESuspenseCoreUISlotState::InvalidDrop:
		return InvalidDropColor;

	case ESuspenseCoreUISlotState::Disabled:
		return LockedSlotColor;

	default:
		return NormalHighlightColor;
	}
}
