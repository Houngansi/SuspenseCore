// SuspenseCoreDragVisualWidget.cpp
// SuspenseCore - Drag Visual (Ghost) Widget Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/DragDrop/SuspenseCoreDragVisualWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
#include "Engine/Texture2D.h"

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
	// NOTE: Visibility is controlled by UE5's DragDropOperation when used as DefaultDragVisual.
	// Widget is automatically shown during drag and hidden when drag ends.
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

	// NOTE: Do NOT set visibility here!
	// UE5's DefaultDragVisual system manages visibility automatically.
	// Setting Collapsed here prevents UE5 from showing the widget at all.
}

//==================================================================
// Drag Visual Control
//==================================================================

void USuspenseCoreDragVisualWidget::InitializeDrag(const FSuspenseCoreDragData& DragData)
{
	CurrentDragData = DragData;
	DragOffset = DragData.DragOffset;
	bIsRotated = DragData.bIsRotatedDuringDrag;
	bCurrentDropValid = true;

	// Update size based on item
	CurrentSize = DragData.Item.GetEffectiveSize();

	// Update visuals (icon, quantity, border)
	UpdateVisuals();
	UpdateSize();

	// NOTE: Visibility is managed by UE5's DefaultDragVisual system.
	// Do NOT call SetVisibility here - it interferes with UE5's management.

	// Notify Blueprint
	K2_OnDragInitialized(DragData);
}

void USuspenseCoreDragVisualWidget::UpdatePosition(const FVector2D& ScreenPosition)
{
	// NOTE: This function is intentionally empty!
	// UE5's DefaultDragVisual system manages positioning automatically via UDragDropOperation::Pivot and Offset.
	// Calling SetRenderTranslation() here would CONFLICT with UE5's native positioning,
	// causing the visual to appear at wrong location (e.g., stuck at 0,0 or double-offset).
	// See: SuspenseCoreInventoryWidget::NativeOnDragDetected where we set Offset and Pivot.
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
