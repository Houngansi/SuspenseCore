// SuspenseCoreMagazineTooltipWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/HUD/SuspenseCoreMagazineTooltipWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetLayoutLibrary.h"

USuspenseCoreMagazineTooltipWidget::USuspenseCoreMagazineTooltipWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ═══════════════════════════════════════════════════════════════════════════════
// UUserWidget Interface
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreMagazineTooltipWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Start hidden
	SetVisibility(ESlateVisibility::Collapsed);
}

// ═══════════════════════════════════════════════════════════════════════════════
// ISuspenseCoreMagazineTooltipWidget Implementation
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreMagazineTooltipWidget::ShowMagazineTooltip_Implementation(const FSuspenseCoreMagazineTooltipData& TooltipData, const FVector2D& ScreenPosition)
{
	CachedTooltipData = TooltipData;
	bIsVisible = true;

	// Update all sections
	UpdateHeaderUI();
	UpdateRoundsUI();
	UpdateAmmoUI();
	UpdateStatsUI();
	UpdateCompatibilityUI();
	UpdateDescriptionUI();

	if (bComparisonMode)
	{
		UpdateComparisonUI();
	}

	// Position tooltip
	ApplyPosition(ScreenPosition);

	// Show widget
	SetVisibility(ESlateVisibility::HitTestInvisible);

	OnTooltipShown();
}

void USuspenseCoreMagazineTooltipWidget::HideMagazineTooltip_Implementation()
{
	bIsVisible = false;
	SetVisibility(ESlateVisibility::Collapsed);

	OnTooltipHidden();
}

void USuspenseCoreMagazineTooltipWidget::UpdateMagazineTooltip_Implementation(const FSuspenseCoreMagazineTooltipData& TooltipData)
{
	CachedTooltipData = TooltipData;

	// Update all sections
	UpdateHeaderUI();
	UpdateRoundsUI();
	UpdateAmmoUI();
	UpdateStatsUI();
	UpdateCompatibilityUI();
	UpdateDescriptionUI();

	if (bComparisonMode)
	{
		UpdateComparisonUI();
	}

	OnTooltipUpdated();
}

void USuspenseCoreMagazineTooltipWidget::UpdateTooltipPosition_Implementation(const FVector2D& ScreenPosition)
{
	ApplyPosition(ScreenPosition);
}

void USuspenseCoreMagazineTooltipWidget::SetShowAmmoStats_Implementation(bool bShow)
{
	bShowAmmoStats = bShow;
	AmmoSection->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void USuspenseCoreMagazineTooltipWidget::SetShowCompatibleWeapons_Implementation(bool bShow)
{
	bShowCompatibleWeapons = bShow;
	CompatibleWeaponsSection->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void USuspenseCoreMagazineTooltipWidget::SetComparisonMode_Implementation(bool bCompare, const FSuspenseCoreMagazineTooltipData& CompareData)
{
	bComparisonMode = bCompare;
	ComparisonData = CompareData;
	ComparisonSection->SetVisibility(bCompare ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	if (bCompare && bIsVisible)
	{
		UpdateComparisonUI();
	}
}

bool USuspenseCoreMagazineTooltipWidget::IsMagazineTooltipVisible_Implementation() const
{
	return bIsVisible;
}

FSuspenseCoreMagazineTooltipData USuspenseCoreMagazineTooltipWidget::GetCurrentTooltipData_Implementation() const
{
	return CachedTooltipData;
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreMagazineTooltipWidget::UpdateHeaderUI()
{
	// Magazine icon
	if (CachedTooltipData.Icon)
	{
		MagazineIcon->SetBrushFromTexture(CachedTooltipData.Icon);
		MagazineIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		MagazineIcon->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Magazine name
	MagazineNameText->SetText(CachedTooltipData.DisplayName);

	// Rarity (from tag - Blueprint should handle actual styling)
	FString RarityStr = CachedTooltipData.RarityTag.IsValid()
		? CachedTooltipData.RarityTag.GetTagName().ToString()
		: TEXT("");
	RarityText->SetText(FText::FromString(RarityStr));

	// Rarity icon visibility based on tag validity
	RarityIcon->SetVisibility(CachedTooltipData.RarityTag.IsValid()
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed);
}

void USuspenseCoreMagazineTooltipWidget::UpdateRoundsUI()
{
	// Current rounds
	CurrentRoundsText->SetText(FText::AsNumber(CachedTooltipData.CurrentRounds));

	// Max capacity
	MaxCapacityText->SetText(FText::Format(
		NSLOCTEXT("Tooltip", "CapFormat", "/{0}"),
		FText::AsNumber(CachedTooltipData.MaxCapacity)
	));

	// Fill bar
	FillBar->SetPercent(CachedTooltipData.GetFillPercent());
}

void USuspenseCoreMagazineTooltipWidget::UpdateAmmoUI()
{
	if (!bShowAmmoStats)
	{
		AmmoSection->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	AmmoSection->SetVisibility(ESlateVisibility::HitTestInvisible);

	// Loaded ammo type
	LoadedAmmoText->SetText(CachedTooltipData.LoadedAmmoName);

	// Loaded ammo icon
	if (CachedTooltipData.LoadedAmmoIcon)
	{
		LoadedAmmoIcon->SetBrushFromTexture(CachedTooltipData.LoadedAmmoIcon);
		LoadedAmmoIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		LoadedAmmoIcon->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Damage stat
	AmmoDamageText->SetText(FText::Format(
		NSLOCTEXT("Tooltip", "Damage", "DMG: {0}"),
		FText::AsNumber(FMath::RoundToInt(CachedTooltipData.AmmoDamage))
	));

	// Penetration stat
	AmmoPenetrationText->SetText(FText::Format(
		NSLOCTEXT("Tooltip", "Penetration", "PEN: {0}"),
		FText::AsNumber(FMath::RoundToInt(CachedTooltipData.AmmoArmorPenetration))
	));

	// Fragmentation stat
	int32 FragPercent = FMath::RoundToInt(CachedTooltipData.AmmoFragmentationChance * 100.0f);
	AmmoFragmentationText->SetText(FText::Format(
		NSLOCTEXT("Tooltip", "Frag", "FRAG: {0}%"),
		FText::AsNumber(FragPercent)
	));
}

void USuspenseCoreMagazineTooltipWidget::UpdateStatsUI()
{
	// Caliber
	CaliberText->SetText(CachedTooltipData.CaliberDisplayName);

	// Weight
	float TotalWeight = CachedTooltipData.GetTotalWeight();
	WeightText->SetText(FText::Format(
		WeightFormat,
		FText::AsNumber(FMath::RoundToFloat(TotalWeight * 100.0f) / 100.0f)
	));

	// Durability
	DurabilityText->SetText(FText::Format(
		DurabilityFormat,
		FText::AsNumber(FMath::RoundToInt(CachedTooltipData.Durability)),
		FText::AsNumber(FMath::RoundToInt(CachedTooltipData.MaxDurability))
	));

	DurabilityBar->SetPercent(CachedTooltipData.GetDurabilityPercent());

	// Reload modifier
	float ModifierPercent = (CachedTooltipData.ReloadTimeModifier - 1.0f) * 100.0f;
	int32 ModifierInt = FMath::RoundToInt(ModifierPercent);

	if (ModifierInt >= 0)
	{
		ReloadModifierText->SetText(FText::Format(
			ReloadModifierPositiveFormat,
			FText::AsNumber(ModifierInt)
		));
	}
	else
	{
		ReloadModifierText->SetText(FText::Format(
			ReloadModifierNegativeFormat,
			FText::AsNumber(ModifierInt)
		));
	}

	// Ergonomics
	int32 ErgoPenalty = FMath::RoundToInt(CachedTooltipData.ErgonomicsPenalty);
	ErgonomicsText->SetText(FText::Format(
		NSLOCTEXT("Tooltip", "Ergo", "ERGO: -{0}"),
		FText::AsNumber(ErgoPenalty)
	));

	// Feed reliability
	int32 ReliabilityPercent = FMath::RoundToInt(CachedTooltipData.FeedReliability * 100.0f);
	ReliabilityText->SetText(FText::Format(
		NSLOCTEXT("Tooltip", "Reliability", "Reliability: {0}%"),
		FText::AsNumber(ReliabilityPercent)
	));
}

void USuspenseCoreMagazineTooltipWidget::UpdateCompatibilityUI()
{
	if (!bShowCompatibleWeapons)
	{
		CompatibleWeaponsSection->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	CompatibleWeaponsSection->SetVisibility(
		CachedTooltipData.CompatibleWeaponNames.Num() > 0
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed
	);

	if (CachedTooltipData.CompatibleWeaponNames.Num() > 0)
	{
		// Build compatible weapons list
		FString WeaponsList;
		for (int32 i = 0; i < CachedTooltipData.CompatibleWeaponNames.Num(); ++i)
		{
			if (i > 0)
			{
				WeaponsList += TEXT(", ");
			}
			WeaponsList += CachedTooltipData.CompatibleWeaponNames[i].ToString();

			// Limit to 3 weapons displayed
			if (i >= 2 && CachedTooltipData.CompatibleWeaponNames.Num() > 3)
			{
				WeaponsList += FString::Printf(TEXT(" (+%d more)"), CachedTooltipData.CompatibleWeaponNames.Num() - 3);
				break;
			}
		}

		CompatibleWeaponsText->SetText(FText::FromString(WeaponsList));
	}
	else
	{
		CompatibleWeaponsText->SetText(FText::GetEmpty());
	}
}

void USuspenseCoreMagazineTooltipWidget::UpdateComparisonUI()
{
	if (!bComparisonMode)
	{
		ComparisonSection->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	ComparisonSection->SetVisibility(ESlateVisibility::HitTestInvisible);

	// Rounds difference
	int32 RoundsDiff = CachedTooltipData.CurrentRounds - ComparisonData.CurrentRounds;
	FString RoundsDiffStr = RoundsDiff >= 0
		? FString::Printf(TEXT("+%d"), RoundsDiff)
		: FString::Printf(TEXT("%d"), RoundsDiff);

	CompareRoundsText->SetText(FText::Format(
		NSLOCTEXT("Tooltip", "CompareRounds", "Rounds: {0}"),
		FText::FromString(RoundsDiffStr)
	));

	// Capacity difference
	int32 CapDiff = CachedTooltipData.MaxCapacity - ComparisonData.MaxCapacity;
	FString CapDiffStr = CapDiff >= 0
		? FString::Printf(TEXT("+%d"), CapDiff)
		: FString::Printf(TEXT("%d"), CapDiff);

	CompareCapacityText->SetText(FText::Format(
		NSLOCTEXT("Tooltip", "CompareCapacity", "Capacity: {0}"),
		FText::FromString(CapDiffStr)
	));
}

void USuspenseCoreMagazineTooltipWidget::UpdateDescriptionUI()
{
	if (!bShowDescription || CachedTooltipData.Description.IsEmpty())
	{
		DescriptionText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	DescriptionText->SetText(CachedTooltipData.Description);
	DescriptionText->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void USuspenseCoreMagazineTooltipWidget::ApplyPosition(const FVector2D& ScreenPosition)
{
	// Apply cursor offset and clamp to viewport
	FVector2D AdjustedPosition = ScreenPosition + CursorOffset;

	// Get viewport size for clamping
	if (APlayerController* PC = GetOwningPlayer())
	{
		int32 ViewportX, ViewportY;
		PC->GetViewportSize(ViewportX, ViewportY);

		// Get widget desired size
		FVector2D WidgetSize = GetDesiredSize();

		// Clamp to viewport bounds
		if (AdjustedPosition.X + WidgetSize.X > ViewportX)
		{
			AdjustedPosition.X = ScreenPosition.X - CursorOffset.X - WidgetSize.X;
		}

		if (AdjustedPosition.Y + WidgetSize.Y > ViewportY)
		{
			AdjustedPosition.Y = ScreenPosition.Y - CursorOffset.Y - WidgetSize.Y;
		}

		// Ensure not negative
		AdjustedPosition.X = FMath::Max(0.0f, AdjustedPosition.X);
		AdjustedPosition.Y = FMath::Max(0.0f, AdjustedPosition.Y);
	}

	SetPositionInViewport(AdjustedPosition, false);
}
