// SuspenseCoreBaseSlotWidget.cpp
// SuspenseCore - Base Slot Widget Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/Base/SuspenseCoreBaseSlotWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Styling/SlateBrush.h"

//==================================================================
// Constructor
//==================================================================

USuspenseCoreBaseSlotWidget::USuspenseCoreBaseSlotWidget(const FObjectInitializer& ObjectInitializer)
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
	, CurrentHighlightState(ESuspenseCoreUISlotState::Empty)
	, SlotSize(64.0f, 64.0f)
{
}

//==================================================================
// UUserWidget Interface
//==================================================================

void USuspenseCoreBaseSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Initialize visual state
	UpdateVisuals();
	UpdateHighlightVisual(ESuspenseCoreUISlotState::Empty);
}

void USuspenseCoreBaseSlotWidget::NativePreConstruct()
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
// Common Slot Interface
//==================================================================

void USuspenseCoreBaseSlotWidget::UpdateSlot(const FSuspenseCoreSlotUIData& SlotData, const FSuspenseCoreItemUIData& ItemData)
{
	CachedSlotData = SlotData;
	CachedItemData = ItemData;

	// Update visuals
	UpdateVisuals();

	// Notify Blueprint
	K2_OnSlotUpdated(SlotData, ItemData);
}

void USuspenseCoreBaseSlotWidget::ClearSlot()
{
	CachedSlotData = FSuspenseCoreSlotUIData();
	CachedSlotData.State = ESuspenseCoreUISlotState::Empty;
	CachedItemData = FSuspenseCoreItemUIData();

	UpdateVisuals();
}

void USuspenseCoreBaseSlotWidget::SetSlotSize(const FVector2D& InSize)
{
	SlotSize = InSize;

	if (SlotSizeBox)
	{
		SlotSizeBox->SetWidthOverride(InSize.X);
		SlotSizeBox->SetHeightOverride(InSize.Y);
	}
}

//==================================================================
// Highlight State
//==================================================================

void USuspenseCoreBaseSlotWidget::SetHighlightState(ESuspenseCoreUISlotState NewState)
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

void USuspenseCoreBaseSlotWidget::UpdateVisuals_Implementation()
{
	// Update background color based on state
	if (BackgroundBorder)
	{
		FLinearColor BgColor = EmptySlotColor;

		if (CachedSlotData.State == ESuspenseCoreUISlotState::Locked)
		{
			BgColor = LockedSlotColor;
		}
		else if (CachedSlotData.IsOccupied())
		{
			BgColor = OccupiedSlotColor;
		}

		BackgroundBorder->SetBrushColor(BgColor);
	}

	// Update icon and stack count in subclass implementations
	UpdateItemIcon();
	UpdateStackCount();
}

void USuspenseCoreBaseSlotWidget::UpdateItemIcon()
{
	// Base implementation - show/hide icon based on item data
	if (!ItemIcon)
	{
		return;
	}

	if (CachedItemData.IconPath.IsValid() && CachedSlotData.IsOccupied())
	{
		// Try synchronous load first
		if (UTexture2D* IconTexture = Cast<UTexture2D>(CachedItemData.IconPath.TryLoad()))
		{
			FSlateBrush Brush;
			Brush.SetResourceObject(IconTexture);
			Brush.ImageSize = SlotSize * 0.85f; // 85% of slot size
			Brush.DrawAs = ESlateBrushDrawType::Image;
			Brush.Tiling = ESlateBrushTileType::NoTile;

			ItemIcon->SetBrush(Brush);
			ItemIcon->SetVisibility(ESlateVisibility::HitTestInvisible);

			// Handle rotation
			if (CachedItemData.bIsRotated)
			{
				ItemIcon->SetRenderTransformAngle(90.0f);
				ItemIcon->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			}
			else
			{
				ItemIcon->SetRenderTransformAngle(0.0f);
			}
		}
		else
		{
			// Async load
			FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
			TWeakObjectPtr<UImage> WeakIcon(ItemIcon);
			TWeakObjectPtr<USuspenseCoreBaseSlotWidget> WeakThis(this);
			FSoftObjectPath IconPath = CachedItemData.IconPath;
			bool bIsRotated = CachedItemData.bIsRotated;
			FVector2D CapturedSize = SlotSize;

			StreamableManager.RequestAsyncLoad(
				IconPath,
				FStreamableDelegate::CreateLambda([WeakIcon, WeakThis, IconPath, bIsRotated, CapturedSize]()
				{
					USuspenseCoreBaseSlotWidget* This = WeakThis.Get();
					UImage* Icon = WeakIcon.Get();

					if (Icon && This)
					{
						if (UTexture2D* LoadedTexture = Cast<UTexture2D>(IconPath.ResolveObject()))
						{
							FSlateBrush Brush;
							Brush.SetResourceObject(LoadedTexture);
							Brush.ImageSize = CapturedSize * 0.85f;
							Brush.DrawAs = ESlateBrushDrawType::Image;
							Brush.Tiling = ESlateBrushTileType::NoTile;

							Icon->SetBrush(Brush);
							Icon->SetVisibility(ESlateVisibility::HitTestInvisible);

							if (bIsRotated)
							{
								Icon->SetRenderTransformAngle(90.0f);
								Icon->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
							}
							else
							{
								Icon->SetRenderTransformAngle(0.0f);
							}
						}
					}
				}),
				FStreamableManager::AsyncLoadHighPriority
			);

			ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	else
	{
		ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void USuspenseCoreBaseSlotWidget::UpdateStackCount()
{
	if (!StackCountText)
	{
		return;
	}

	if (CachedSlotData.IsOccupied() && CachedItemData.Quantity > StackCountDisplayThreshold)
	{
		StackCountText->SetText(FText::AsNumber(CachedItemData.Quantity));
		StackCountText->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		StackCountText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void USuspenseCoreBaseSlotWidget::UpdateHighlightVisual_Implementation(ESuspenseCoreUISlotState State)
{
	if (!HighlightBorder)
	{
		return;
	}

	FLinearColor Color = GetHighlightColor(State);
	HighlightBorder->SetBrushColor(Color);
}

FLinearColor USuspenseCoreBaseSlotWidget::GetHighlightColor_Implementation(ESuspenseCoreUISlotState State)
{
	switch (State)
	{
	case ESuspenseCoreUISlotState::Empty:
		return NormalHighlightColor;

	case ESuspenseCoreUISlotState::Highlighted:
		return HoveredHighlightColor;

	case ESuspenseCoreUISlotState::Selected:
		return SelectedHighlightColor;

	case ESuspenseCoreUISlotState::DropTargetValid:
		return ValidDropColor;

	case ESuspenseCoreUISlotState::DropTargetInvalid:
		return InvalidDropColor;

	case ESuspenseCoreUISlotState::Locked:
	case ESuspenseCoreUISlotState::Invalid:
		return LockedSlotColor;

	default:
		return NormalHighlightColor;
	}
}
