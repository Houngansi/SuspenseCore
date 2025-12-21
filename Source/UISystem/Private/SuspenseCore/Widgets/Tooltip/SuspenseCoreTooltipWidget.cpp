// SuspenseCoreTooltipWidget.cpp
// SuspenseCore - Item Tooltip Widget Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/Tooltip/SuspenseCoreTooltipWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/Border.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"
#include "Internationalization/Text.h"
#include "Slate/WidgetTransform.h"

//==================================================================
// Constructor
//==================================================================

USuspenseCoreTooltipWidget::USuspenseCoreTooltipWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CursorOffset(FVector2D(20.0f, 20.0f))
	, ScreenEdgePadding(12.0f)
	, FadeInDuration(0.15f)   // Slightly slower for smooth feel
	, FadeOutDuration(0.1f)
	// Rarity colors (Tarkov-style)
	, CommonColor(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f))        // Gray
	, UncommonColor(FLinearColor(0.12f, 0.85f, 0.25f, 1.0f))   // Green
	, RareColor(FLinearColor(0.0f, 0.5f, 1.0f, 1.0f))          // Blue
	, EpicColor(FLinearColor(0.7f, 0.25f, 1.0f, 1.0f))         // Purple
	, LegendaryColor(FLinearColor(1.0f, 0.55f, 0.0f, 1.0f))    // Orange
	// State
	, bHasComparison(false)
	, TargetPosition(FVector2D::ZeroVector)
	, bIsShowing(false)
	, bIsFading(false)
	, bFadingIn(false)
	, CurrentOpacity(0.0f)
	, AnimProgress(0.0f)
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

	// CRITICAL: Validate required BindWidget components
	checkf(RootBorder, TEXT("USuspenseCoreTooltipWidget: RootBorder is REQUIRED! Add UBorder named 'RootBorder' to your Blueprint."));
	checkf(ItemNameText, TEXT("USuspenseCoreTooltipWidget: ItemNameText is REQUIRED! Add UTextBlock named 'ItemNameText' to your Blueprint."));

	// Set alignment for proper positioning (top-left pivot)
	SetAlignmentInViewport(FVector2D(0.0f, 0.0f));

	// Ensure tooltip doesn't block input
	SetIsFocusable(false);

	// Initialize animation state
	SetRenderOpacity(0.0f);
	FWidgetTransform InitTransform;
	InitTransform.Scale = FVector2D(StartScale, StartScale);
	SetRenderTransform(InitTransform);
}

void USuspenseCoreTooltipWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Update fade animation
	if (bIsFading)
	{
		UpdateFadeAnimation(InDeltaTime);
	}

	// Follow mouse position if visible (also during fade animation)
	if (bIsShowing && (CurrentOpacity > 0.0f || bIsFading))
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

	// Position the tooltip
	RepositionTooltip(ScreenPosition);

	// Make visible and start fade-in animation
	SetVisibility(ESlateVisibility::HitTestInvisible);
	bIsShowing = true;
	bIsFading = true;
	bFadingIn = true;
	// AnimProgress preserves current state for seamless reverse animation

	// Notify Blueprint
	K2_OnFadeStarted(true);
}

void USuspenseCoreTooltipWidget::UpdatePosition(const FVector2D& ScreenPosition)
{
	TargetPosition = ScreenPosition;
	RepositionTooltip(ScreenPosition);
}

void USuspenseCoreTooltipWidget::Hide()
{
	if (!bIsShowing && AnimProgress <= 0.0f)
	{
		return; // Already hidden
	}

	// Start fade-out animation (AnimProgress continues from current state)
	bIsShowing = false;
	bIsFading = true;
	bFadingIn = false;

	// Notify Blueprint
	K2_OnFadeStarted(false);
}

void USuspenseCoreTooltipWidget::HideImmediate()
{
	bIsShowing = false;
	bIsFading = false;
	AnimProgress = 0.0f;
	CurrentOpacity = 0.0f;
	SetRenderOpacity(0.0f);
	FWidgetTransform ResetTransform;
	ResetTransform.Scale = FVector2D(StartScale, StartScale);
	ResetTransform.Translation = FVector2D::ZeroVector;
	SetRenderTransform(ResetTransform);
	SetVisibility(ESlateVisibility::Collapsed);
}

void USuspenseCoreTooltipWidget::SetComparisonItem(const FSuspenseCoreItemUIData& CompareItemData)
{
	ComparisonItemData = CompareItemData;
	bHasComparison = CompareItemData.InstanceID.IsValid();

	// Notify Blueprint
	K2_OnComparisonChanged(bHasComparison, ComparisonItemData);

	// Refresh content if visible
	if (bIsShowing)
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
	if (bIsShowing)
	{
		PopulateContent(CurrentItemData);
	}
}

//==================================================================
// Animation (AAA-quality with Cubic Ease Out)
//==================================================================

void USuspenseCoreTooltipWidget::UpdateFadeAnimation(float DeltaTime)
{
	if (!bIsFading)
	{
		return;
	}

	const float Duration = bFadingIn ? FadeInDuration : FadeOutDuration;

	// Calculate linear progress step
	if (Duration > 0.0f)
	{
		const float Change = DeltaTime / Duration;
		AnimProgress = bFadingIn ? (AnimProgress + Change) : (AnimProgress - Change);
	}
	else
	{
		AnimProgress = bFadingIn ? 1.0f : 0.0f;
	}

	// Clamp progress
	AnimProgress = FMath::Clamp(AnimProgress, 0.0f, 1.0f);

	// Apply Cubic Ease Out for smooth deceleration ("sticky" landing effect)
	const float EasedValue = FMath::InterpEaseOut(0.0f, 1.0f, AnimProgress, 3.0f);

	// 1. Opacity
	CurrentOpacity = EasedValue;
	SetRenderOpacity(CurrentOpacity);

	// 2. Build combined transform (scale + translation)
	FWidgetTransform AnimTransform;

	// Dynamic scale (from StartScale to 1.0)
	const float CurrentScale = FMath::Lerp(StartScale, 1.0f, EasedValue);
	AnimTransform.Scale = FVector2D(CurrentScale, CurrentScale);

	// Vertical drift (float-up effect - starts offset, ends at 0)
	const float CurrentTranslationY = FMath::Lerp(VerticalDrift, 0.0f, EasedValue);
	AnimTransform.Translation = FVector2D(0.0f, CurrentTranslationY);

	SetRenderTransform(AnimTransform);

	// Check if animation complete
	if ((bFadingIn && AnimProgress >= 1.0f) || (!bFadingIn && AnimProgress <= 0.0f))
	{
		bIsFading = false;
		CurrentOpacity = bFadingIn ? 1.0f : 0.0f;

		if (!bFadingIn)
		{
			SetVisibility(ESlateVisibility::Collapsed);
		}

		// Notify Blueprint
		K2_OnFadeCompleted(bFadingIn);
	}
}

//==================================================================
// Content Population
//==================================================================

void USuspenseCoreTooltipWidget::PopulateContent_Implementation(const FSuspenseCoreItemUIData& ItemData)
{
	// Get rarity color
	FLinearColor RarityColor = GetRarityColor(ItemData.RarityTag);

	// Set item name with rarity color
	if (ItemNameText)
	{
		ItemNameText->SetText(ItemData.DisplayName);
		ItemNameText->SetColorAndOpacity(FSlateColor(RarityColor));
	}

	// Set item type
	if (ItemTypeText)
	{
		ItemTypeText->SetText(GetItemTypeDisplayName(ItemData.ItemType));
	}

	// Set description
	if (DescriptionText)
	{
		DescriptionText->SetText(ItemData.Description);
	}

	// Set weight
	if (WeightText)
	{
		WeightText->SetText(FormatWeight(ItemData.TotalWeight));
	}

	// Set value
	if (ValueText)
	{
		ValueText->SetText(FormatValue(ItemData.TotalValue));
	}

	// Set grid size
	if (SizeText)
	{
		FText SizeFormatted = FText::Format(
			NSLOCTEXT("SuspenseCore", "GridSizeFormat", "{0}x{1}"),
			FText::AsNumber(ItemData.GridSize.X),
			FText::AsNumber(ItemData.GridSize.Y)
		);
		SizeText->SetText(SizeFormatted);
	}

	// Set icon
	if (ItemIcon)
	{
		if (ItemData.IconPath.IsValid())
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
		else
		{
			ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Set rarity border color
	if (RarityBorder)
	{
		RarityBorder->SetBrushColor(RarityColor);
	}
}

//==================================================================
// Helper Functions
//==================================================================

FLinearColor USuspenseCoreTooltipWidget::GetRarityColor(const FGameplayTag& RarityTag) const
{
	FString TagString = RarityTag.ToString();

	if (TagString.Contains(TEXT("Legendary")))
	{
		return LegendaryColor;
	}
	else if (TagString.Contains(TEXT("Epic")))
	{
		return EpicColor;
	}
	else if (TagString.Contains(TEXT("Rare")))
	{
		return RareColor;
	}
	else if (TagString.Contains(TEXT("Uncommon")))
	{
		return UncommonColor;
	}

	return CommonColor;
}

FText USuspenseCoreTooltipWidget::FormatWeight(float Weight) const
{
	// Format weight with 2 decimal places and "kg" suffix
	FNumberFormattingOptions Options = FNumberFormattingOptions::DefaultNoGrouping();
	Options.MaximumFractionalDigits = 2;
	Options.MinimumFractionalDigits = 1;

	return FText::Format(
		NSLOCTEXT("SuspenseCore", "WeightFormat", "{0} kg"),
		FText::AsNumber(Weight, &Options)
	);
}

FText USuspenseCoreTooltipWidget::FormatValue(int32 Value) const
{
	// Format value with thousands separator
	FNumberFormattingOptions Options;
	Options.UseGrouping = true;

	return FText::AsNumber(Value, &Options);
}

FText USuspenseCoreTooltipWidget::GetItemTypeDisplayName(const FGameplayTag& ItemTypeTag) const
{
	FString TagString = ItemTypeTag.ToString();

	// Extract the last part of the tag (e.g., "Item.Weapon.AR" -> "AR")
	FString TypeName;
	TagString.Split(TEXT("."), nullptr, &TypeName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);

	// Map common abbreviations to readable names
	if (TypeName == TEXT("AR"))
	{
		return NSLOCTEXT("SuspenseCore", "ItemType_AR", "Assault Rifle");
	}
	else if (TypeName == TEXT("SMG"))
	{
		return NSLOCTEXT("SuspenseCore", "ItemType_SMG", "Submachine Gun");
	}
	else if (TypeName == TEXT("Pistol"))
	{
		return NSLOCTEXT("SuspenseCore", "ItemType_Pistol", "Pistol");
	}
	else if (TypeName == TEXT("Helmet"))
	{
		return NSLOCTEXT("SuspenseCore", "ItemType_Helmet", "Helmet");
	}
	else if (TypeName == TEXT("BodyArmor"))
	{
		return NSLOCTEXT("SuspenseCore", "ItemType_BodyArmor", "Body Armor");
	}
	else if (TypeName == TEXT("Backpack"))
	{
		return NSLOCTEXT("SuspenseCore", "ItemType_Backpack", "Backpack");
	}
	else if (TypeName == TEXT("TacticalRig"))
	{
		return NSLOCTEXT("SuspenseCore", "ItemType_TacticalRig", "Tactical Rig");
	}
	else if (TypeName == TEXT("Medical"))
	{
		return NSLOCTEXT("SuspenseCore", "ItemType_Medical", "Medical");
	}
	else if (TypeName == TEXT("Throwable"))
	{
		return NSLOCTEXT("SuspenseCore", "ItemType_Throwable", "Throwable");
	}
	else if (TypeName == TEXT("Knife") || TagString.Contains(TEXT("Melee")))
	{
		return NSLOCTEXT("SuspenseCore", "ItemType_Melee", "Melee Weapon");
	}
	else if (TagString.Contains(TEXT("Ammo")))
	{
		return NSLOCTEXT("SuspenseCore", "ItemType_Ammo", "Ammunition");
	}
	else if (TagString.Contains(TEXT("Gear")))
	{
		return NSLOCTEXT("SuspenseCore", "ItemType_Gear", "Gear");
	}

	// Fallback: return the type name as-is
	return FText::FromString(TypeName);
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
