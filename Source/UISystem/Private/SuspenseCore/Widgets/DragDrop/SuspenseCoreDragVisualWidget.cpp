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

	// Update position every tick
	ESlateVisibility CurrentVis = GetVisibility();
	bool bShouldUpdate = (CurrentVis == ESlateVisibility::Visible || CurrentVis == ESlateVisibility::HitTestInvisible);

	// Debug log once to verify tick is running
	static bool bLoggedOnce = false;
	if (!bLoggedOnce && bShouldUpdate)
	{
		UE_LOG(LogTemp, Log, TEXT("DragVisual NativeTick: Visibility=%d, Updating position"), (int32)CurrentVis);
		bLoggedOnce = true;
	}

	if (bShouldUpdate)
	{
		UpdatePosition(FVector2D::ZeroVector); // Parameter not used anymore
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

	// Update size based on item
	CurrentSize = InDragData.Item.GetEffectiveSize();

	// Update visuals (icon, quantity, border)
	UpdateVisuals();
	UpdateSize();

	// Show the widget
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// Position immediately
	UpdatePosition(FVector2D::ZeroVector);

	// Notify Blueprint
	K2_OnDragInitialized(InDragData);

	UE_LOG(LogTemp, Log, TEXT("InitializeDrag: DragOffset=(%.1f, %.1f)"), DragOffset.X, DragOffset.Y);
}

void USuspenseCoreDragVisualWidget::UpdatePosition(const FVector2D& ScreenPosition)
{
	// Use FSlateApplication to get cursor position - works during drag when PC->GetMousePosition fails
	// GetCursorPos returns SCREEN coordinates (absolute), need to convert to viewport
	FVector2D CursorScreenPos = FSlateApplication::Get().GetCursorPos();

	// Convert screen position to viewport position
	// Get the game viewport
	UGameViewportClient* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!ViewportClient)
	{
		// Fallback: use screen position directly with offset
		FVector2D FinalPos = CursorScreenPos + DragOffset;
		SetPositionInViewport(FinalPos);
		return;
	}

	// Get viewport origin in screen space
	FVector2D ViewportOrigin;
	FVector2D ViewportSize;
	ViewportClient->GetViewportSize(ViewportSize);

	// Get the viewport widget to find its screen position
	TSharedPtr<SViewport> ViewportWidget = ViewportClient->GetGameViewportWidget();
	if (ViewportWidget.IsValid())
	{
		FGeometry ViewportGeometry = ViewportWidget->GetCachedGeometry();
		ViewportOrigin = ViewportGeometry.GetAbsolutePosition();
	}

	// Convert cursor screen position to viewport-local position
	FVector2D ViewportLocalPos = CursorScreenPos - ViewportOrigin;

	// Apply drag offset and set position
	FVector2D FinalPos = ViewportLocalPos + DragOffset;
	SetPositionInViewport(FinalPos);

	// Debug log every ~60 frames
	static int32 DebugCounter = 0;
	if (++DebugCounter % 60 == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("UpdatePosition: CursorScreen=(%.1f, %.1f), ViewportOrigin=(%.1f, %.1f), Local=(%.1f, %.1f), Offset=(%.1f, %.1f), Final=(%.1f, %.1f)"),
			CursorScreenPos.X, CursorScreenPos.Y,
			ViewportOrigin.X, ViewportOrigin.Y,
			ViewportLocalPos.X, ViewportLocalPos.Y,
			DragOffset.X, DragOffset.Y,
			FinalPos.X, FinalPos.Y);
	}
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
