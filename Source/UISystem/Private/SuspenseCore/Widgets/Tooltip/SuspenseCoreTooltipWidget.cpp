// SuspenseCoreTooltipWidget.cpp
// SuspenseCore - Item Tooltip Widget Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/Tooltip/SuspenseCoreTooltipWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"

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

	// Set alignment for proper positioning (top-left pivot)
	SetAlignmentInViewport(FVector2D(0.0f, 0.0f));

	// Ensure tooltip doesn't block input
	SetIsFocusable(false);
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

	// Force layout update so GetDesiredSize() returns correct values
	ForceLayoutPrepass();

	// Position the tooltip first
	RepositionTooltip(ScreenPosition);

	// Show tooltip immediately - delay is handled by caller if needed
	// Using HitTestInvisible so tooltip doesn't intercept mouse events
	ShowDelayTimer = 0.0f;
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void USuspenseCoreTooltipWidget::UpdatePosition(const FVector2D& ScreenPosition)
{
	TargetPosition = ScreenPosition;
	RepositionTooltip(ScreenPosition);
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
	bHasComparison = CompareItemData.InstanceID.IsValid();

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
	if (ItemIcon && ItemData.IconPath.IsValid())
	{
		if (UTexture2D* IconTexture = Cast<UTexture2D>(ItemData.IconPath.TryLoad()))
		{
			ItemIcon->SetBrushFromTexture(IconTexture);
			ItemIcon->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
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
	// This is now just a passthrough - actual positioning is done in RepositionTooltip
	return DesiredPosition;
}

void USuspenseCoreTooltipWidget::RepositionTooltip(const FVector2D& ScreenPosition)
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	// Get actual mouse position in viewport space
	float MouseX, MouseY;
	if (!PC->GetMousePosition(MouseX, MouseY))
	{
		// Use provided screen position as fallback
		MouseX = ScreenPosition.X;
		MouseY = ScreenPosition.Y;
	}

	// Get viewport size
	FVector2D ViewportSize = FVector2D::ZeroVector;
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}

	if (ViewportSize.IsZero())
	{
		return;
	}

	// Get DPI scale
	float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);

	// Convert mouse position to slate units (considering DPI)
	FVector2D MousePosition = FVector2D(MouseX, MouseY) / ViewportScale;

	// Get tooltip size
	FVector2D TooltipSize = GetDesiredSize();

	// If size is not ready, try cached geometry
	if (TooltipSize.IsZero())
	{
		const FGeometry& CachedGeometry = GetCachedGeometry();
		if (CachedGeometry.GetLocalSize().X > 0 && CachedGeometry.GetLocalSize().Y > 0)
		{
			TooltipSize = CachedGeometry.GetLocalSize();
		}
		else
		{
			// Use reasonable default size
			TooltipSize = FVector2D(300.0f, 200.0f);
		}
	}

	// Convert viewport size to slate units
	FVector2D ViewportSizeInSlateUnits = ViewportSize / ViewportScale;

	// Calculate tooltip position relative to mouse
	FVector2D TooltipPosition = MousePosition;

	// Position tooltip with offset from cursor
	const float VerticalOffset = CursorOffset.Y > 0 ? CursorOffset.Y : 20.0f;

	// Determine if tooltip should be on the right or left of cursor
	bool bShowOnRight = true;

	// Check if there's enough space on the right
	if (MousePosition.X + CursorOffset.X + TooltipSize.X > ViewportSizeInSlateUnits.X - ScreenEdgePadding)
	{
		bShowOnRight = false;
	}

	// Position horizontally
	if (bShowOnRight)
	{
		// Show on the right of cursor
		TooltipPosition.X = MousePosition.X + CursorOffset.X;
	}
	else
	{
		// Show on the left of cursor
		TooltipPosition.X = MousePosition.X - CursorOffset.X - TooltipSize.X;
	}

	// Position vertically - tooltip below cursor
	TooltipPosition.Y = MousePosition.Y + VerticalOffset;

	// Check if tooltip goes beyond bottom edge
	if (TooltipPosition.Y + TooltipSize.Y > ViewportSizeInSlateUnits.Y - ScreenEdgePadding)
	{
		// Show above cursor instead
		TooltipPosition.Y = MousePosition.Y - VerticalOffset - TooltipSize.Y;
	}

	// Final bounds check
	TooltipPosition.X = FMath::Clamp(
		TooltipPosition.X,
		ScreenEdgePadding,
		ViewportSizeInSlateUnits.X - TooltipSize.X - ScreenEdgePadding
	);

	TooltipPosition.Y = FMath::Clamp(
		TooltipPosition.Y,
		ScreenEdgePadding,
		ViewportSizeInSlateUnits.Y - TooltipSize.Y - ScreenEdgePadding
	);

	// Apply position using SetPositionInViewport (same as legacy tooltip)
	SetPositionInViewport(TooltipPosition, false);
}
