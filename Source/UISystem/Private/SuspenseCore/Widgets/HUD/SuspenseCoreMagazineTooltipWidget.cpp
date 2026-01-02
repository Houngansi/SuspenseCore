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

	if (AmmoSection)
	{
		AmmoSection->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void USuspenseCoreMagazineTooltipWidget::SetShowCompatibleWeapons_Implementation(bool bShow)
{
	bShowCompatibleWeapons = bShow;

	if (CompatibleWeaponsSection)
	{
		CompatibleWeaponsSection->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void USuspenseCoreMagazineTooltipWidget::SetComparisonMode_Implementation(bool bCompare, const FSuspenseCoreMagazineTooltipData& CompareData)
{
	bComparisonMode = bCompare;
	ComparisonData = CompareData;

	if (ComparisonSection)
	{
		ComparisonSection->SetVisibility(bCompare ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

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
	if (MagazineIcon && CachedTooltipData.Icon)
	{
		MagazineIcon->SetBrushFromTexture(CachedTooltipData.Icon);
		MagazineIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else if (MagazineIcon)
	{
		MagazineIcon->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Magazine name
	if (MagazineNameText)
	{
		MagazineNameText->SetText(CachedTooltipData.DisplayName);
	}

	// Rarity (from tag - Blueprint should handle actual styling)
	if (RarityText)
	{
		FString RarityStr = CachedTooltipData.RarityTag.IsValid()
			? CachedTooltipData.RarityTag.GetTagName().ToString()
			: TEXT("");
		RarityText->SetText(FText::FromString(RarityStr));
	}
}

void USuspenseCoreMagazineTooltipWidget::UpdateRoundsUI()
{
	// Current rounds
	if (CurrentRoundsText)
	{
		CurrentRoundsText->SetText(FText::AsNumber(CachedTooltipData.CurrentRounds));
	}

	// Max capacity
	if (MaxCapacityText)
	{
		MaxCapacityText->SetText(FText::Format(
			NSLOCTEXT("Tooltip", "CapFormat", "/{0}"),
			FText::AsNumber(CachedTooltipData.MaxCapacity)
		));
	}

	// Fill bar
	if (FillBar)
	{
		FillBar->SetPercent(CachedTooltipData.GetFillPercent());
	}
}

void USuspenseCoreMagazineTooltipWidget::UpdateAmmoUI()
{
	if (!bShowAmmoStats)
	{
		if (AmmoSection)
		{
			AmmoSection->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	if (AmmoSection)
	{
		AmmoSection->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	// Loaded ammo type
	if (LoadedAmmoText)
	{
		LoadedAmmoText->SetText(CachedTooltipData.LoadedAmmoName);
	}

	// Loaded ammo icon
	if (LoadedAmmoIcon && CachedTooltipData.LoadedAmmoIcon)
	{
		LoadedAmmoIcon->SetBrushFromTexture(CachedTooltipData.LoadedAmmoIcon);
		LoadedAmmoIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else if (LoadedAmmoIcon)
	{
		LoadedAmmoIcon->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Damage stat
	if (AmmoDamageText)
	{
		AmmoDamageText->SetText(FText::Format(
			NSLOCTEXT("Tooltip", "Damage", "DMG: {0}"),
			FText::AsNumber(FMath::RoundToInt(CachedTooltipData.AmmoDamage))
		));
	}

	// Penetration stat
	if (AmmoPenetrationText)
	{
		AmmoPenetrationText->SetText(FText::Format(
			NSLOCTEXT("Tooltip", "Penetration", "PEN: {0}"),
			FText::AsNumber(FMath::RoundToInt(CachedTooltipData.AmmoArmorPenetration))
		));
	}

	// Fragmentation stat
	if (AmmoFragmentationText)
	{
		int32 FragPercent = FMath::RoundToInt(CachedTooltipData.AmmoFragmentationChance * 100.0f);
		AmmoFragmentationText->SetText(FText::Format(
			NSLOCTEXT("Tooltip", "Frag", "FRAG: {0}%"),
			FText::AsNumber(FragPercent)
		));
	}
}

void USuspenseCoreMagazineTooltipWidget::UpdateStatsUI()
{
	// Caliber
	if (CaliberText)
	{
		CaliberText->SetText(CachedTooltipData.CaliberDisplayName);
	}

	// Weight
	if (WeightText)
	{
		float TotalWeight = CachedTooltipData.GetTotalWeight();
		WeightText->SetText(FText::Format(
			WeightFormat,
			FText::AsNumber(FMath::RoundToFloat(TotalWeight * 100.0f) / 100.0f)
		));
	}

	// Durability
	if (DurabilityText)
	{
		DurabilityText->SetText(FText::Format(
			DurabilityFormat,
			FText::AsNumber(FMath::RoundToInt(CachedTooltipData.Durability)),
			FText::AsNumber(FMath::RoundToInt(CachedTooltipData.MaxDurability))
		));
	}

	if (DurabilityBar)
	{
		DurabilityBar->SetPercent(CachedTooltipData.GetDurabilityPercent());
	}

	// Reload modifier
	if (ReloadModifierText)
	{
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
	}

	// Ergonomics
	if (ErgonomicsText)
	{
		int32 ErgoPenalty = FMath::RoundToInt(CachedTooltipData.ErgonomicsPenalty);
		ErgonomicsText->SetText(FText::Format(
			NSLOCTEXT("Tooltip", "Ergo", "ERGO: -{0}"),
			FText::AsNumber(ErgoPenalty)
		));
	}

	// Feed reliability
	if (ReliabilityText)
	{
		int32 ReliabilityPercent = FMath::RoundToInt(CachedTooltipData.FeedReliability * 100.0f);
		ReliabilityText->SetText(FText::Format(
			NSLOCTEXT("Tooltip", "Reliability", "Reliability: {0}%"),
			FText::AsNumber(ReliabilityPercent)
		));
	}
}

void USuspenseCoreMagazineTooltipWidget::UpdateCompatibilityUI()
{
	if (!bShowCompatibleWeapons)
	{
		if (CompatibleWeaponsSection)
		{
			CompatibleWeaponsSection->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	if (CompatibleWeaponsSection)
	{
		CompatibleWeaponsSection->SetVisibility(
			CachedTooltipData.CompatibleWeaponNames.Num() > 0
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed
		);
	}

	if (CompatibleWeaponsText && CachedTooltipData.CompatibleWeaponNames.Num() > 0)
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
}

void USuspenseCoreMagazineTooltipWidget::UpdateComparisonUI()
{
	if (!bComparisonMode)
	{
		if (ComparisonSection)
		{
			ComparisonSection->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	if (ComparisonSection)
	{
		ComparisonSection->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	// Rounds difference
	if (CompareRoundsText)
	{
		int32 RoundsDiff = CachedTooltipData.CurrentRounds - ComparisonData.CurrentRounds;
		FString DiffStr = RoundsDiff >= 0
			? FString::Printf(TEXT("+%d"), RoundsDiff)
			: FString::Printf(TEXT("%d"), RoundsDiff);

		CompareRoundsText->SetText(FText::Format(
			NSLOCTEXT("Tooltip", "CompareRounds", "Rounds: {0}"),
			FText::FromString(DiffStr)
		));
	}

	// Capacity difference
	if (CompareCapacityText)
	{
		int32 CapDiff = CachedTooltipData.MaxCapacity - ComparisonData.MaxCapacity;
		FString DiffStr = CapDiff >= 0
			? FString::Printf(TEXT("+%d"), CapDiff)
			: FString::Printf(TEXT("%d"), CapDiff);

		CompareCapacityText->SetText(FText::Format(
			NSLOCTEXT("Tooltip", "CompareCapacity", "Capacity: {0}"),
			FText::FromString(DiffStr)
		));
	}
}

void USuspenseCoreMagazineTooltipWidget::UpdateDescriptionUI()
{
	if (!bShowDescription)
	{
		if (DescriptionText)
		{
			DescriptionText->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	if (DescriptionText)
	{
		if (CachedTooltipData.Description.IsEmpty())
		{
			DescriptionText->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			DescriptionText->SetText(CachedTooltipData.Description);
			DescriptionText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
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
