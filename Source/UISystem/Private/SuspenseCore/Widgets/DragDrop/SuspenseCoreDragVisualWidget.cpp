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
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"

//==================================================================
// Constructor
//==================================================================

USuspenseCoreDragVisualWidget::USuspenseCoreDragVisualWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CellSizePixels(64.0f)
	, DragOpacity(0.7f)
	, ValidDropColor(FLinearColor(0.0f, 1.0f, 0.0f, 0.4f))
	, InvalidDropColor(FLinearColor(1.0f, 0.0f, 0.0f, 0.4f))
	, NeutralColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.2f))
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
	checkf(SizeContainer, TEXT("USuspenseCoreDragVisualWidget: SizeContainer is REQUIRED! Add USizeBox named 'SizeContainer' to your Blueprint."));
	checkf(ValidityBorder, TEXT("USuspenseCoreDragVisualWidget: ValidityBorder is REQUIRED! Add UBorder named 'ValidityBorder' to your Blueprint."));
	checkf(ItemIcon, TEXT("USuspenseCoreDragVisualWidget: ItemIcon is REQUIRED! Add UImage named 'ItemIcon' to your Blueprint."));

	// Set alignment for proper positioning (top-left pivot)
	SetAlignmentInViewport(FVector2D(0.0f, 0.0f));

	// Ensure widget doesn't block input
	SetIsFocusable(false);

	// Set initial opacity
	SetRenderOpacity(DragOpacity);
}

void USuspenseCoreDragVisualWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Update position every frame for smooth cursor tracking
	// Check visibility to avoid unnecessary updates when collapsed
	ESlateVisibility CurrentVis = GetVisibility();
	if (CurrentVis != ESlateVisibility::Collapsed && CurrentVis != ESlateVisibility::Hidden)
	{
		UpdatePositionFromCursor();
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

	// Get effective size (accounts for rotation in drag data)
	CurrentSize = InDragData.Item.GetEffectiveSize();

	// IMPORTANT: Update visuals BEFORE calculating offset
	// This ensures size is correct when calculating center
	UpdateVisuals();
	UpdateSize();

	// Calculate center offset - cursor grabs item at its center
	// This provides much better UX than grabbing at arbitrary click point
	RecalculateCenterOffset();

	// Show the widget
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// Position immediately to prevent first-frame jump
	UpdatePositionFromCursor();

	// Notify Blueprint
	K2_OnDragInitialized(InDragData);
}

void USuspenseCoreDragVisualWidget::UpdatePositionFromCursor()
{
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	// ============================================================================
	// WINDOWED MODE SAFE CURSOR TRACKING
	//
	// Problem: In windowed mode, FSlateApplication::GetCursorPos() returns
	// absolute screen coordinates (e.g., cursor at 1500,800 on a 1920x1080 monitor).
	// But SetPositionInViewport expects viewport-local coordinates.
	//
	// Solution: Get viewport's absolute position on screen and subtract it.
	// ============================================================================

	// Get ABSOLUTE cursor position (screen coordinates)
	FVector2D AbsoluteCursorPos = FSlateApplication::Get().GetCursorPos();

	// Get viewport scale for DPI correction
	float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);

	// Try to get viewport-local position via PlayerController (preferred method)
	FVector2D LocalMousePos;
	APlayerController* PC = GetOwningPlayer();

	if (PC)
	{
		float MouseX, MouseY;
		if (UWidgetLayoutLibrary::GetMousePositionScaledByDPI(PC, MouseX, MouseY))
		{
			// This works correctly in both fullscreen and windowed mode
			LocalMousePos = FVector2D(MouseX, MouseY);
		}
		else
		{
			// Fallback: manually convert absolute to viewport-local
			// We need to get the viewport's position on screen
			FVector2D ViewportOrigin = FVector2D::ZeroVector;

			UWorld* World = GetWorld();
			if (World)
			{
				UGameViewportClient* ViewportClient = World->GetGameViewport();
				if (ViewportClient)
				{
					TSharedPtr<SViewport> ViewportWidget = ViewportClient->GetGameViewportWidget();
					if (ViewportWidget.IsValid())
					{
						// Get viewport's absolute position on screen
						FGeometry ViewportGeometry = ViewportWidget->GetCachedGeometry();
						ViewportOrigin = ViewportGeometry.GetAbsolutePosition();
					}
				}
			}

			// Subtract viewport origin to get viewport-local coordinates
			FVector2D ViewportLocalPos = AbsoluteCursorPos - ViewportOrigin;

			// Apply DPI scale
			LocalMousePos = ViewportLocalPos / ViewportScale;
		}
	}
	else
	{
		// No PC - try to get viewport origin anyway
		FVector2D ViewportOrigin = FVector2D::ZeroVector;

		UWorld* World = GetWorld();
		if (World)
		{
			UGameViewportClient* ViewportClient = World->GetGameViewport();
			if (ViewportClient)
			{
				TSharedPtr<SViewport> ViewportWidget = ViewportClient->GetGameViewportWidget();
				if (ViewportWidget.IsValid())
				{
					FGeometry ViewportGeometry = ViewportWidget->GetCachedGeometry();
					ViewportOrigin = ViewportGeometry.GetAbsolutePosition();
				}
			}
		}

		FVector2D ViewportLocalPos = AbsoluteCursorPos - ViewportOrigin;
		LocalMousePos = ViewportLocalPos / ViewportScale;
	}

	// Convert DragOffset from pixels to slate units
	FVector2D ScaledOffset = DragOffset / ViewportScale;

	// Apply center offset and set position
	SetPositionInViewport(LocalMousePos + ScaledOffset, false);
}

void USuspenseCoreDragVisualWidget::RecalculateCenterOffset()
{
	// Calculate center offset based on current size
	// This centers the item under the cursor
	float WidthPixels = FMath::Max(1, CurrentSize.X) * CellSizePixels;
	float HeightPixels = FMath::Max(1, CurrentSize.Y) * CellSizePixels;
	DragOffset = FVector2D(-WidthPixels * 0.5f, -HeightPixels * 0.5f);
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

		// CRITICAL: Recalculate offset immediately after size change
		// Otherwise item will visually "jump" away from cursor
		RecalculateCenterOffset();

		// Update size container
		UpdateSize();

		// Rotate the icon
		if (ItemIcon)
		{
			ItemIcon->SetRenderTransformAngle(bIsRotated ? 90.0f : 0.0f);
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
		// ============================================================================
		// ICON LOADING
		// NOTE: TryLoad() is synchronous and may cause hitches if icon isn't cached.
		// For production, consider:
		// 1. Pre-loading icons via IconManager subsystem
		// 2. Passing already-loaded UTexture2D* in DragData
		// 3. Using async loading with placeholder
		// ============================================================================

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

	// Set neutral border color initially
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
