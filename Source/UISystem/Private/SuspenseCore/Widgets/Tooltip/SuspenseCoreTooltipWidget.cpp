// SuspenseCoreTooltipWidget.cpp
// SuspenseCore - Item Tooltip Widget Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/Tooltip/SuspenseCoreTooltipWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Blueprint/WidgetLayoutLibrary.h"

//==================================================================
// Constructor
//==================================================================

USuspenseCoreTooltipWidget::USuspenseCoreTooltipWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CursorOffset(FVector2D(15.0f, 15.0f))
	, ScreenEdgePadding(10.0f)
	, ShowDelay(0.3f)
	, bHasComparison(false)
	, TargetPosition(FVector2D::ZeroVector)
	, ShowDelayTimer(0.0f)
{
	// Start hidden
	SetVisibility(ESlateVisibility::Collapsed);
}

//==================================================================
// UUserWidget Interface
//==================================================================

void USuspenseCoreTooltipWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void USuspenseCoreTooltipWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Handle show delay
	if (ShowDelayTimer > 0.0f)
	{
		ShowDelayTimer -= InDeltaTime;
		if (ShowDelayTimer <= 0.0f)
		{
			SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}

	// Follow mouse position if visible
	if (IsVisible())
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			float MouseX, MouseY;
			if (PC->GetMousePosition(MouseX, MouseY))
			{
				UpdatePosition(FVector2D(MouseX, MouseY));
			}
		}
	}
}

//==================================================================
// Tooltip Control
//==================================================================

void USuspenseCoreTooltipWidget::ShowForItem(const FSuspenseCoreItemUIData& ItemData, const FVector2D& ScreenPosition)
{
	CurrentItemData = ItemData;
	TargetPosition = ScreenPosition;

	// Populate content
	PopulateContent(ItemData);

	// Notify Blueprint
	K2_OnPopulateTooltip(ItemData);

	// Start show delay timer
	if (ShowDelay > 0.0f)
	{
		ShowDelayTimer = ShowDelay;
		// Position but keep hidden during delay
		FVector2D BestPosition = CalculateBestPosition(ScreenPosition + CursorOffset);
		SetRenderTranslation(BestPosition);
	}
	else
	{
		// Show immediately
		FVector2D BestPosition = CalculateBestPosition(ScreenPosition + CursorOffset);
		SetRenderTranslation(BestPosition);
		SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void USuspenseCoreTooltipWidget::UpdatePosition(const FVector2D& ScreenPosition)
{
	TargetPosition = ScreenPosition;
	FVector2D BestPosition = CalculateBestPosition(ScreenPosition + CursorOffset);
	SetRenderTranslation(BestPosition);
}

void USuspenseCoreTooltipWidget::Hide()
{
	SetVisibility(ESlateVisibility::Collapsed);
	ShowDelayTimer = 0.0f;
	CurrentItemData = FSuspenseCoreItemUIData();
}

void USuspenseCoreTooltipWidget::SetComparisonItem(const FSuspenseCoreItemUIData& CompareItemData)
{
	ComparisonItemData = CompareItemData;
	bHasComparison = CompareItemData.ItemInstanceID.IsValid();

	// Notify Blueprint
	K2_OnComparisonChanged(bHasComparison, ComparisonItemData);

	// Refresh content if visible
	if (IsVisible())
	{
		PopulateContent(CurrentItemData);
	}
}

void USuspenseCoreTooltipWidget::ClearComparison()
{
	ComparisonItemData = FSuspenseCoreItemUIData();
	bHasComparison = false;

	// Notify Blueprint
	K2_OnComparisonChanged(false, ComparisonItemData);

	// Refresh content if visible
	if (IsVisible())
	{
		PopulateContent(CurrentItemData);
	}
}

//==================================================================
// Content Population
//==================================================================

void USuspenseCoreTooltipWidget::PopulateContent_Implementation(const FSuspenseCoreItemUIData& ItemData)
{
	// Set icon
	if (ItemIcon && ItemData.Icon)
	{
		ItemIcon->SetBrushFromTexture(ItemData.Icon);
		ItemIcon->SetVisibility(ESlateVisibility::Visible);
	}
	else if (ItemIcon)
	{
		ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Set name
	if (ItemNameText)
	{
		ItemNameText->SetText(ItemData.DisplayName);
	}

	// Set description
	if (DescriptionText)
	{
		DescriptionText->SetText(ItemData.Description);
	}

	// Stats would be populated here
	// In a full implementation, you'd iterate ItemData.Stats and create stat widgets
}

FVector2D USuspenseCoreTooltipWidget::CalculateBestPosition_Implementation(const FVector2D& DesiredPosition)
{
	FVector2D Result = DesiredPosition;

	// Get viewport size
	if (APlayerController* PC = GetOwningPlayer())
	{
		FVector2D ViewportSize;
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->GetViewportSize(ViewportSize);
		}

		// Get tooltip size
		FVector2D TooltipSize = GetDesiredSize();

		// Clamp X to stay on screen
		if (Result.X + TooltipSize.X > ViewportSize.X - ScreenEdgePadding)
		{
			// Flip to left side of cursor
			Result.X = DesiredPosition.X - TooltipSize.X - CursorOffset.X * 2.0f;
		}
		Result.X = FMath::Max(Result.X, ScreenEdgePadding);

		// Clamp Y to stay on screen
		if (Result.Y + TooltipSize.Y > ViewportSize.Y - ScreenEdgePadding)
		{
			// Flip to above cursor
			Result.Y = DesiredPosition.Y - TooltipSize.Y - CursorOffset.Y * 2.0f;
		}
		Result.Y = FMath::Max(Result.Y, ScreenEdgePadding);
	}

	return Result;
}
