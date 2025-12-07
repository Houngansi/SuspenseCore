// SuspenseCoreInventorySlotWidget.cpp
// SuspenseCore - Individual Inventory Slot Widget Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/Inventory/SuspenseCoreInventorySlotWidget.h"
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
	, CurrentHighlightState(ESuspenseCoreUISlotState::Empty)
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
	UpdateHighlightVisual(ESuspenseCoreUISlotState::Empty);
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
	CachedSlotData.State = ESuspenseCoreUISlotState::Empty;
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

	// Update item icon
	if (ItemIcon)
	{
		// Only show icon on anchor slot (primary slot for multi-cell items)
		if (CachedSlotData.bIsAnchor && CachedItemData.IconPath.IsValid())
		{
			UE_LOG(LogTemp, Log, TEXT("SlotWidget[%d]: Loading icon from path: %s"), SlotIndex, *CachedItemData.IconPath.ToString());

			// First try synchronous load (works if asset is already in memory)
			if (UTexture2D* IconTexture = Cast<UTexture2D>(CachedItemData.IconPath.TryLoad()))
			{
				UE_LOG(LogTemp, Log, TEXT("SlotWidget[%d]: Icon loaded! Texture=%s, Size=%dx%d"),
					SlotIndex,
					*IconTexture->GetName(),
					IconTexture->GetSizeX(),
					IconTexture->GetSizeY());

				// Create brush with multi-cell sizing
				FVector2D IconSize = CalculateMultiCellIconSize();

				FSlateBrush Brush;
				Brush.SetResourceObject(IconTexture);
				Brush.ImageSize = IconSize;
				Brush.DrawAs = ESlateBrushDrawType::Image;
				Brush.Tiling = ESlateBrushTileType::NoTile;

				ItemIcon->SetBrush(Brush);
				ItemIcon->SetVisibility(ESlateVisibility::HitTestInvisible);

				UE_LOG(LogTemp, Log, TEXT("SlotWidget[%d]: Icon set with size %.1fx%.1f (ItemSize=%dx%d, Cell=%.0f)"),
					SlotIndex,
					IconSize.X, IconSize.Y,
					MultiCellItemSize.X, MultiCellItemSize.Y,
					CellSizePixels);

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
				// Async load using StreamableManager
				UE_LOG(LogTemp, Log, TEXT("SlotWidget[%d]: Starting async icon load"), SlotIndex);

				FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
				TWeakObjectPtr<UImage> WeakIcon(ItemIcon);
				TWeakObjectPtr<USuspenseCoreInventorySlotWidget> WeakThis(this);
				FSoftObjectPath IconPath = CachedItemData.IconPath;
				bool bIsRotated = CachedItemData.bIsRotated;

				StreamableManager.RequestAsyncLoad(
					IconPath,
					FStreamableDelegate::CreateLambda([WeakIcon, WeakThis, IconPath, bIsRotated]()
					{
						USuspenseCoreInventorySlotWidget* This = WeakThis.Get();
						UImage* Icon = WeakIcon.Get();

						if (Icon && This)
						{
							if (UTexture2D* LoadedTexture = Cast<UTexture2D>(IconPath.ResolveObject()))
							{
								UE_LOG(LogTemp, Log, TEXT("SlotWidget: Icon loaded async successfully"));

								// Create brush with multi-cell sizing
								FVector2D IconSize = This->CalculateMultiCellIconSize();

								FSlateBrush Brush;
								Brush.SetResourceObject(LoadedTexture);
								Brush.ImageSize = IconSize;
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
							else
							{
								UE_LOG(LogTemp, Warning, TEXT("SlotWidget: Failed to resolve icon after async load"));
							}
						}
					}),
					FStreamableManager::AsyncLoadHighPriority
				);

				// Show loading state or hide until loaded
				ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		else
		{
			ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SlotWidget[%d]: ItemIcon is NULL - Blueprint binding missing!"), SlotIndex);
	}

	// Update stack count
	if (StackCountText)
	{
		if (CachedSlotData.bIsAnchor && CachedItemData.Quantity > StackCountDisplayThreshold)
		{
			StackCountText->SetText(FText::AsNumber(CachedItemData.Quantity));
			StackCountText->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			StackCountText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Update condition/durability indicator
	if (ConditionIndicator)
	{
		if (CachedSlotData.bIsAnchor && CachedItemData.bHasDurability)
		{
			// Color based on durability percentage
			FLinearColor ConditionColor = FLinearColor::LerpUsingHSV(
				FLinearColor::Red,
				FLinearColor::Green,
				CachedItemData.DurabilityPercent);
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

//==================================================================
// Multi-Cell Support
//==================================================================

void USuspenseCoreInventorySlotWidget::SetMultiCellItemSize(const FIntPoint& InSize)
{
	MultiCellItemSize = InSize;

	// Ensure minimum 1x1
	if (MultiCellItemSize.X < 1) MultiCellItemSize.X = 1;
	if (MultiCellItemSize.Y < 1) MultiCellItemSize.Y = 1;

	// Update icon size if we have an item displayed
	if (ItemIcon && CachedSlotData.bIsAnchor && CachedItemData.InstanceID.IsValid())
	{
		FVector2D IconSize = CalculateMultiCellIconSize();

		// Create brush with proper sizing
		FSlateBrush Brush = ItemIcon->GetBrush();
		Brush.ImageSize = IconSize;
		ItemIcon->SetBrush(Brush);

		UE_LOG(LogTemp, Log, TEXT("SlotWidget[%d]: Multi-cell icon size set to %.1fx%.1f (item size %dx%d)"),
			SlotIndex, IconSize.X, IconSize.Y, MultiCellItemSize.X, MultiCellItemSize.Y);
	}
}

FVector2D USuspenseCoreInventorySlotWidget::CalculateMultiCellIconSize() const
{
	// Calculate icon size based on item grid size and cell size
	// For a 2x3 item with 64px cells: (2*64*0.85, 3*64*0.85) = (108.8, 163.2)
	float IconWidth = MultiCellItemSize.X * CellSizePixels * MultiCellIconScale;
	float IconHeight = MultiCellItemSize.Y * CellSizePixels * MultiCellIconScale;

	return FVector2D(IconWidth, IconHeight);
}
