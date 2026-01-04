// SuspenseCoreMagazineTooltipWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.
//
// Extends SuspenseCoreTooltipWidget with magazine-specific functionality.
// Uses inherited animations, positioning, and rarity system.

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
	// Magazine tooltip specific defaults
	// Base class handles CursorOffset, FadeInDuration, FadeOutDuration, etc.
}

// ═══════════════════════════════════════════════════════════════════════════════
// UUserWidget Interface
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreMagazineTooltipWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Validate magazine-specific BindWidget components
	checkf(CurrentRoundsText, TEXT("USuspenseCoreMagazineTooltipWidget: CurrentRoundsText is REQUIRED!"));
	checkf(MaxCapacityText, TEXT("USuspenseCoreMagazineTooltipWidget: MaxCapacityText is REQUIRED!"));
	checkf(FillBar, TEXT("USuspenseCoreMagazineTooltipWidget: FillBar is REQUIRED!"));
	checkf(AmmoSection, TEXT("USuspenseCoreMagazineTooltipWidget: AmmoSection is REQUIRED!"));
	checkf(CaliberText, TEXT("USuspenseCoreMagazineTooltipWidget: CaliberText is REQUIRED!"));
	checkf(DurabilityText, TEXT("USuspenseCoreMagazineTooltipWidget: DurabilityText is REQUIRED!"));
	checkf(DurabilityBar, TEXT("USuspenseCoreMagazineTooltipWidget: DurabilityBar is REQUIRED!"));
	checkf(ComparisonSection, TEXT("USuspenseCoreMagazineTooltipWidget: ComparisonSection is REQUIRED!"));
	checkf(CompatibleWeaponsSection, TEXT("USuspenseCoreMagazineTooltipWidget: CompatibleWeaponsSection is REQUIRED!"));

	// Start with comparison section hidden
	if (ComparisonSection)
	{
		ComparisonSection->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Initialize section visibility based on config
	if (AmmoSection)
	{
		AmmoSection->SetVisibility(bShowAmmoStats ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (CompatibleWeaponsSection)
	{
		CompatibleWeaponsSection->SetVisibility(bShowCompatibleWeapons ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// ISuspenseCoreMagazineTooltipWidget Implementation
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreMagazineTooltipWidget::ShowMagazineTooltip_Implementation(const FSuspenseCoreMagazineTooltipData& TooltipData, const FVector2D& ScreenPosition)
{
	// Cache magazine data
	CachedMagazineData = TooltipData;

	// Convert to base item UI data for inherited header display
	FSuspenseCoreItemUIData ItemData = ConvertToItemUIData(TooltipData);

	// Refresh magazine-specific UI sections BEFORE showing (so size is calculated correctly)
	RefreshMagazineUI();

	// Use base class method for fade-in animation and positioning
	// This handles: visibility, animations, DPI-aware positioning
	ShowForItem(ItemData, ScreenPosition);

	// Notify Blueprint
	OnMagazineTooltipShown();
}

void USuspenseCoreMagazineTooltipWidget::HideMagazineTooltip_Implementation()
{
	// Use base class method for fade-out animation
	Hide();

	// Notify Blueprint
	OnMagazineTooltipHidden();
}

void USuspenseCoreMagazineTooltipWidget::UpdateMagazineTooltip_Implementation(const FSuspenseCoreMagazineTooltipData& TooltipData)
{
	CachedMagazineData = TooltipData;

	// Update all magazine-specific sections
	RefreshMagazineUI();

	// Also update base header with converted data
	FSuspenseCoreItemUIData ItemData = ConvertToItemUIData(TooltipData);
	PopulateContent(ItemData);

	// Notify Blueprint
	OnMagazineTooltipUpdated();
}

void USuspenseCoreMagazineTooltipWidget::UpdateTooltipPosition_Implementation(const FVector2D& ScreenPosition)
{
	// Use inherited positioning method (DPI-aware, bounds-checked)
	UpdatePosition(ScreenPosition);
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
	bMagazineComparisonMode = bCompare;
	ComparisonMagazineData = CompareData;

	if (ComparisonSection)
	{
		ComparisonSection->SetVisibility(bCompare ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (bCompare && IsTooltipVisible())
	{
		UpdateComparisonUI();
	}

	// Notify Blueprint
	OnMagazineComparisonChanged(bCompare);
}

bool USuspenseCoreMagazineTooltipWidget::IsMagazineTooltipVisible_Implementation() const
{
	// Use inherited visibility check (includes animation state)
	return IsTooltipVisible();
}

FSuspenseCoreMagazineTooltipData USuspenseCoreMagazineTooltipWidget::GetCurrentTooltipData_Implementation() const
{
	return CachedMagazineData;
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════════

FSuspenseCoreItemUIData USuspenseCoreMagazineTooltipWidget::ConvertToItemUIData(const FSuspenseCoreMagazineTooltipData& MagazineData) const
{
	FSuspenseCoreItemUIData ItemData;

	// Identity
	ItemData.ItemID = MagazineData.MagazineID;
	ItemData.DisplayName = MagazineData.DisplayName;
	ItemData.Description = MagazineData.Description;
	ItemData.RarityTag = MagazineData.RarityTag;

	// Set item type tag for magazines
	ItemData.ItemType = FGameplayTag::RequestGameplayTag(FName("Item.Magazine"), false);

	// Icon - use TSoftObjectPtr path if available
	if (MagazineData.Icon)
	{
		ItemData.IconPath = FSoftObjectPath(MagazineData.Icon);
	}

	// Weight - calculate total weight (empty + loaded ammo)
	ItemData.TotalWeight = MagazineData.GetTotalWeight();

	// Grid size - magazines are typically 1x2 or 1x3
	ItemData.GridSize = FIntPoint(1, 2);

	// No value set - magazines don't typically show price in tooltip

	return ItemData;
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreMagazineTooltipWidget::RefreshMagazineUI()
{
	UpdateHeaderUI();
	UpdateRoundsUI();
	UpdateAmmoUI();
	UpdateStatsUI();
	UpdateCompatibilityUI();

	if (bMagazineComparisonMode)
	{
		UpdateComparisonUI();
	}
}

void USuspenseCoreMagazineTooltipWidget::UpdateHeaderUI()
{
	// Base class handles ItemNameText, ItemIcon, RarityBorder through PopulateContent
	// Magazine-specific header updates can go here if needed

	// Note: If you need magazine-specific header elements (like magazine icon separate from item icon),
	// add BindWidget properties and update them here
}

void USuspenseCoreMagazineTooltipWidget::UpdateRoundsUI()
{
	// Current rounds
	if (CurrentRoundsText)
	{
		CurrentRoundsText->SetText(FText::AsNumber(CachedMagazineData.CurrentRounds));
	}

	// Max capacity
	if (MaxCapacityText)
	{
		MaxCapacityText->SetText(FText::Format(
			NSLOCTEXT("MagTooltip", "CapFormat", "/{0}"),
			FText::AsNumber(CachedMagazineData.MaxCapacity)
		));
	}

	// Fill bar
	if (FillBar)
	{
		FillBar->SetPercent(CachedMagazineData.GetFillPercent());
	}
}

void USuspenseCoreMagazineTooltipWidget::UpdateAmmoUI()
{
	if (!bShowAmmoStats || !AmmoSection)
	{
		if (AmmoSection)
		{
			AmmoSection->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	AmmoSection->SetVisibility(ESlateVisibility::HitTestInvisible);

	// Loaded ammo type
	if (LoadedAmmoText)
	{
		LoadedAmmoText->SetText(CachedMagazineData.LoadedAmmoName);
	}

	// Loaded ammo icon
	if (LoadedAmmoIcon)
	{
		if (CachedMagazineData.LoadedAmmoIcon)
		{
			LoadedAmmoIcon->SetBrushFromTexture(CachedMagazineData.LoadedAmmoIcon);
			LoadedAmmoIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			LoadedAmmoIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Damage stat
	if (AmmoDamageText)
	{
		AmmoDamageText->SetText(FText::Format(
			NSLOCTEXT("MagTooltip", "Damage", "DMG: {0}"),
			FText::AsNumber(FMath::RoundToInt(CachedMagazineData.AmmoDamage))
		));
	}

	// Penetration stat
	if (AmmoPenetrationText)
	{
		AmmoPenetrationText->SetText(FText::Format(
			NSLOCTEXT("MagTooltip", "Penetration", "PEN: {0}"),
			FText::AsNumber(FMath::RoundToInt(CachedMagazineData.AmmoArmorPenetration))
		));
	}

	// Fragmentation stat
	if (AmmoFragmentationText)
	{
		int32 FragPercent = FMath::RoundToInt(CachedMagazineData.AmmoFragmentationChance * 100.0f);
		AmmoFragmentationText->SetText(FText::Format(
			NSLOCTEXT("MagTooltip", "Frag", "FRAG: {0}%"),
			FText::AsNumber(FragPercent)
		));
	}
}

void USuspenseCoreMagazineTooltipWidget::UpdateStatsUI()
{
	// Caliber
	if (CaliberText)
	{
		CaliberText->SetText(CachedMagazineData.CaliberDisplayName);
	}

	// Weight - use inherited FormatWeight for consistency
	// Note: WeightText is in base class, so base PopulateContent handles it

	// Durability text
	if (DurabilityText)
	{
		DurabilityText->SetText(FText::Format(
			NSLOCTEXT("MagTooltip", "DurabilityFormat", "{0}/{1}"),
			FText::AsNumber(FMath::RoundToInt(CachedMagazineData.Durability)),
			FText::AsNumber(FMath::RoundToInt(CachedMagazineData.MaxDurability))
		));
	}

	// Durability bar
	if (DurabilityBar)
	{
		DurabilityBar->SetPercent(CachedMagazineData.GetDurabilityPercent());
	}

	// Reload modifier
	if (ReloadModifierText)
	{
		float ModifierPercent = (CachedMagazineData.ReloadTimeModifier - 1.0f) * 100.0f;
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

	// Ergonomics penalty
	if (ErgonomicsText)
	{
		int32 ErgoPenalty = FMath::RoundToInt(CachedMagazineData.ErgonomicsPenalty);
		ErgonomicsText->SetText(FText::Format(
			NSLOCTEXT("MagTooltip", "Ergo", "ERGO: -{0}"),
			FText::AsNumber(ErgoPenalty)
		));
	}

	// Feed reliability
	if (ReliabilityText)
	{
		int32 ReliabilityPercent = FMath::RoundToInt(CachedMagazineData.FeedReliability * 100.0f);
		ReliabilityText->SetText(FText::Format(
			NSLOCTEXT("MagTooltip", "Reliability", "Reliability: {0}%"),
			FText::AsNumber(ReliabilityPercent)
		));
	}
}

void USuspenseCoreMagazineTooltipWidget::UpdateCompatibilityUI()
{
	if (!bShowCompatibleWeapons || !CompatibleWeaponsSection)
	{
		if (CompatibleWeaponsSection)
		{
			CompatibleWeaponsSection->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	const bool bHasCompatibleWeapons = CachedMagazineData.CompatibleWeaponNames.Num() > 0;
	CompatibleWeaponsSection->SetVisibility(bHasCompatibleWeapons
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed);

	if (bHasCompatibleWeapons && CompatibleWeaponsText)
	{
		// Build compatible weapons list with truncation
		FString WeaponsList;
		const int32 NumWeapons = CachedMagazineData.CompatibleWeaponNames.Num();
		const int32 MaxDisplay = FMath::Min(NumWeapons, MaxCompatibleWeaponsDisplay);

		for (int32 i = 0; i < MaxDisplay; ++i)
		{
			if (i > 0)
			{
				WeaponsList += TEXT(", ");
			}
			WeaponsList += CachedMagazineData.CompatibleWeaponNames[i].ToString();
		}

		// Show "+X more" if truncated
		if (NumWeapons > MaxCompatibleWeaponsDisplay)
		{
			WeaponsList += FString::Printf(TEXT(" (+%d more)"), NumWeapons - MaxCompatibleWeaponsDisplay);
		}

		CompatibleWeaponsText->SetText(FText::FromString(WeaponsList));
	}
}

void USuspenseCoreMagazineTooltipWidget::UpdateComparisonUI()
{
	if (!bMagazineComparisonMode || !ComparisonSection)
	{
		if (ComparisonSection)
		{
			ComparisonSection->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	ComparisonSection->SetVisibility(ESlateVisibility::HitTestInvisible);

	// Rounds difference
	if (CompareRoundsText)
	{
		int32 RoundsDiff = CachedMagazineData.CurrentRounds - ComparisonMagazineData.CurrentRounds;
		FString RoundsDiffStr = RoundsDiff >= 0
			? FString::Printf(TEXT("+%d"), RoundsDiff)
			: FString::Printf(TEXT("%d"), RoundsDiff);

		CompareRoundsText->SetText(FText::Format(
			NSLOCTEXT("MagTooltip", "CompareRounds", "Rounds: {0}"),
			FText::FromString(RoundsDiffStr)
		));
	}

	// Capacity difference
	if (CompareCapacityText)
	{
		int32 CapDiff = CachedMagazineData.MaxCapacity - ComparisonMagazineData.MaxCapacity;
		FString CapDiffStr = CapDiff >= 0
			? FString::Printf(TEXT("+%d"), CapDiff)
			: FString::Printf(TEXT("%d"), CapDiff);

		CompareCapacityText->SetText(FText::Format(
			NSLOCTEXT("MagTooltip", "CompareCapacity", "Capacity: {0}"),
			FText::FromString(CapDiffStr)
		));
	}
}
