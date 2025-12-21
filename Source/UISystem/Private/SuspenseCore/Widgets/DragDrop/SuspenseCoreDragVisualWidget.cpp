// SuspenseCoreDragVisualWidget.cpp
// SuspenseCore - Drag Visual (Ghost) Widget Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/DragDrop/SuspenseCoreDragVisualWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SGameLayerManager.h"
#include "Widgets/SViewport.h"

//==================================================================
// Constructor
//==================================================================

USuspenseCoreDragVisualWidget::USuspenseCoreDragVisualWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CellSizePixels(64.0f)
	, DragOpacity(0.7f)
	, ValidDropColor(FLinearColor(0.0f, 1.0f, 0.0f, 0.5f))
	, InvalidDropColor(FLinearColor(1.0f, 0.0f, 0.0f, 0.5f))
	, NeutralColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.3f))
	, DragOffset(FVector2D::ZeroVector)
	, bIsRotated(false)
	, bCurrentDropValid(true)
	, CurrentSize(1, 1)
	, CachedViewportOrigin(FVector2D::ZeroVector)
	, bViewportCached(false)
{
	// Visibility is managed by UE5's DefaultDragVisual system
	// DO NOT set collapsed here - it breaks DefaultDragVisual display
}

//==================================================================
// UUserWidget Interface
//==================================================================

void USuspenseCoreDragVisualWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// CRITICAL: Validate required BindWidget components
	// If any of these are missing, Blueprint is misconfigured
	checkf(SizeContainer, TEXT("USuspenseCoreDragVisualWidget: SizeContainer is REQUIRED! Add USizeBox named 'SizeContainer' to your Blueprint."));
	checkf(ValidityBorder, TEXT("USuspenseCoreDragVisualWidget: ValidityBorder is REQUIRED! Add UBorder named 'ValidityBorder' to your Blueprint."));
	checkf(ItemIcon, TEXT("USuspenseCoreDragVisualWidget: ItemIcon is REQUIRED! Add UImage named 'ItemIcon' to your Blueprint."));

	// Set initial opacity
	SetRenderOpacity(DragOpacity);
}

void USuspenseCoreDragVisualWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Fast visibility check - update position every frame for smooth dragging
	ESlateVisibility CurrentVis = GetVisibility();
	if (CurrentVis == ESlateVisibility::Visible || CurrentVis == ESlateVisibility::HitTestInvisible)
	{
		UpdatePosition(FVector2D::ZeroVector);
	}
}

//==================================================================
// Drag Visual Control
//==================================================================

void USuspenseCoreDragVisualWidget::InitializeDrag(const FSuspenseCoreDragData& InDragData)
{
	CurrentDragData = InDragData;
	bIsRotated = InDragData.bIsRotatedDuringDrag;
	bCurrentDropValid = true;

	// Update size based on item
	CurrentSize = InDragData.Item.GetEffectiveSize();

	// Calculate center offset - cursor grabs item at its center
	// This provides much better UX than grabbing at click point
	float WidthPixels = FMath::Max(1, CurrentSize.X) * CellSizePixels;
	float HeightPixels = FMath::Max(1, CurrentSize.Y) * CellSizePixels;
	DragOffset = FVector2D(-WidthPixels * 0.5f, -HeightPixels * 0.5f);

	// Cache viewport info for fast position updates
	CacheViewportInfo();

	// Update visuals (icon, quantity, border)
	UpdateVisuals();
	UpdateSize();

	// Show the widget
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// Position immediately
	UpdatePosition(FVector2D::ZeroVector);

	// Notify Blueprint
	K2_OnDragInitialized(InDragData);
}

void USuspenseCoreDragVisualWidget::CacheViewportInfo()
{
	bViewportCached = false;
	CachedViewportOrigin = FVector2D::ZeroVector;

	UGameViewportClient* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!ViewportClient)
	{
		return;
	}

	TSharedPtr<SViewport> ViewportWidget = ViewportClient->GetGameViewportWidget();
	if (ViewportWidget.IsValid())
	{
		FGeometry ViewportGeometry = ViewportWidget->GetCachedGeometry();
		CachedViewportOrigin = ViewportGeometry.GetAbsolutePosition();
		bViewportCached = true;
	}
}

void USuspenseCoreDragVisualWidget::UpdatePosition(const FVector2D& ScreenPosition)
{
	// FAST PATH: Direct cursor position with cached viewport offset
	// This is called every frame during drag - must be as fast as possible

	// Get cursor position in screen space (absolute coordinates)
	const FVector2D CursorScreenPos = FSlateApplication::Get().GetCursorPos();

	// Convert to viewport-local coordinates using cached origin
	// This avoids expensive viewport lookups every frame
	FVector2D ViewportLocalPos = CursorScreenPos;
	if (bViewportCached)
	{
		ViewportLocalPos = CursorScreenPos - CachedViewportOrigin;
	}

	// Apply center offset and set position
	// SetPositionInViewport is the standard UMG way - it's optimized internally
	SetPositionInViewport(ViewportLocalPos + DragOffset);
}

void USuspenseCoreDragVisualWidget::SetDropValidity(bool bCanDrop)
{
	if (bCurrentDropValid != bCanDrop)
	{
		bCurrentDropValid = bCanDrop;

		// Update validity border color
		if (ValidityBorder)
		{
			ValidityBorder->SetBrushColor(bCanDrop ? ValidDropColor : InvalidDropColor);
		}

		// Notify Blueprint
		K2_OnDropValidityChanged(bCanDrop);
	}
}

void USuspenseCoreDragVisualWidget::ToggleRotation()
{
	SetRotation(!bIsRotated);
}

void USuspenseCoreDragVisualWidget::RecalculateCenterOffset()
{
	// Recalculate center offset based on current size
	float WidthPixels = FMath::Max(1, CurrentSize.X) * CellSizePixels;
	float HeightPixels = FMath::Max(1, CurrentSize.Y) * CellSizePixels;
	DragOffset = FVector2D(-WidthPixels * 0.5f, -HeightPixels * 0.5f);
}

void USuspenseCoreDragVisualWidget::SetRotation(bool bRotated)
{
	if (bIsRotated != bRotated)
	{
		bIsRotated = bRotated;

		// Swap width and height
		CurrentSize = FIntPoint(CurrentSize.Y, CurrentSize.X);

		// Recalculate center offset for new dimensions
		RecalculateCenterOffset();

		// Update visuals
		UpdateSize();

		// Rotate the icon
		if (ItemIcon)
		{
			float Angle = bIsRotated ? 90.0f : 0.0f;
			ItemIcon->SetRenderTransformAngle(Angle);
		}

		// Notify Blueprint
		K2_OnRotationChanged(bIsRotated);
	}
}

//==================================================================
// Visual Updates
//==================================================================

void USuspenseCoreDragVisualWidget::UpdateVisuals_Implementation()
{
	// Update icon
	if (ItemIcon)
	{
		if (CurrentDragData.Item.IconPath.IsValid())
		{
			if (UTexture2D* IconTexture = Cast<UTexture2D>(CurrentDragData.Item.IconPath.TryLoad()))
			{
				ItemIcon->SetBrushFromTexture(IconTexture);
				ItemIcon->SetVisibility(ESlateVisibility::Visible);
			}
			else
			{
				ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		else
		{
			ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Update quantity text
	if (QuantityText)
	{
		if (CurrentDragData.DragQuantity > 1)
		{
			QuantityText->SetText(FText::AsNumber(CurrentDragData.DragQuantity));
			QuantityText->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			QuantityText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Update validity border
	if (ValidityBorder)
	{
		ValidityBorder->SetBrushColor(NeutralColor);
	}
}

void USuspenseCoreDragVisualWidget::UpdateSize_Implementation()
{
	if (SizeContainer)
	{
		// Ensure minimum size of 1x1
		int32 EffectiveX = FMath::Max(1, CurrentSize.X);
		int32 EffectiveY = FMath::Max(1, CurrentSize.Y);

		float Width = EffectiveX * CellSizePixels;
		float Height = EffectiveY * CellSizePixels;

		SizeContainer->SetWidthOverride(Width);
		SizeContainer->SetHeightOverride(Height);
	}
}
