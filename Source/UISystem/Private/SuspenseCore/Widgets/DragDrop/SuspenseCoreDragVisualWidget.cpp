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
	, CachedViewportOrigin(FVector2D::ZeroVector)
	, bDragInitialized(false)
	, bIsRotated(false)
	, bCurrentDropValid(true)
	, CurrentSize(1, 1)
{
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

	// Update position every tick if drag is active
	if (bDragInitialized)
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
	DragOffset = InDragData.DragOffset;
	bIsRotated = InDragData.bIsRotatedDuringDrag;
	bCurrentDropValid = true;
	bDragInitialized = true;

	// Update size based on item
	CurrentSize = InDragData.Item.GetEffectiveSize();

	// Cache viewport origin ONCE at drag start (expensive lookup)
	CachedViewportOrigin = FVector2D::ZeroVector;
	UGameViewportClient* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (ViewportClient)
	{
		TSharedPtr<SViewport> ViewportWidget = ViewportClient->GetGameViewportWidget();
		if (ViewportWidget.IsValid())
		{
			FGeometry ViewportGeometry = ViewportWidget->GetCachedGeometry();
			CachedViewportOrigin = ViewportGeometry.GetAbsolutePosition();
		}
	}

	// Update visuals (icon, quantity, border)
	UpdateVisuals();
	UpdateSize();

	// Position widget at (0,0) - we'll use RenderTranslation to move it
	SetPositionInViewport(FVector2D::ZeroVector);

	// Show the widget
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// Position immediately using RenderTranslation
	UpdatePosition(FVector2D::ZeroVector);

	// Notify Blueprint
	K2_OnDragInitialized(InDragData);

	UE_LOG(LogTemp, Log, TEXT("InitializeDrag: DragOffset=(%.1f, %.1f), ViewportOrigin=(%.1f, %.1f)"),
		DragOffset.X, DragOffset.Y, CachedViewportOrigin.X, CachedViewportOrigin.Y);
}

void USuspenseCoreDragVisualWidget::UpdatePosition(const FVector2D& ScreenPosition)
{
	if (!bDragInitialized)
	{
		return;
	}

	// Get cursor position from Slate - works during drag operations
	FVector2D CursorScreenPos = FSlateApplication::Get().GetCursorPos();

	// Convert to viewport-local using cached viewport origin (no expensive lookups!)
	FVector2D ViewportLocalPos = CursorScreenPos - CachedViewportOrigin;

	// Final position = cursor position + drag offset
	FVector2D FinalPos = ViewportLocalPos + DragOffset;

	// Use SetRenderTranslation for INSTANT updates (no layout recalculation!)
	// Widget is positioned at (0,0) in viewport, RenderTranslation moves it visually
	SetRenderTranslation(FinalPos);
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

void USuspenseCoreDragVisualWidget::SetRotation(bool bRotated)
{
	if (bIsRotated != bRotated)
	{
		bIsRotated = bRotated;

		// Swap width and height
		CurrentSize = FIntPoint(CurrentSize.Y, CurrentSize.X);

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
