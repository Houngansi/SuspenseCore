// SuspenseCoreMagazineTooltipWidget.h
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Widgets/Tooltip/SuspenseCoreTooltipWidget.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreMagazineTooltipWidget.h"
#include "SuspenseCoreMagazineTooltipWidget.generated.h"

// Forward declarations
class UTextBlock;
class UImage;
class UProgressBar;
class UVerticalBox;
class UHorizontalBox;
class USizeBox;

/**
 * USuspenseCoreMagazineTooltipWidget
 *
 * Tarkov-style magazine tooltip widget extending base tooltip with magazine-specific display:
 * - Magazine name, icon, rarity (inherited from base)
 * - Current/Max rounds with fill bar
 * - Loaded ammo type and stats
 * - Caliber compatibility
 * - Durability
 * - Weight
 * - Reload modifiers
 *
 * INHERITANCE:
 * This widget extends USuspenseCoreTooltipWidget to reuse:
 * - AAA-quality fade animations (Cubic Ease Out)
 * - DPI-aware positioning
 * - Rarity color system
 * - Comparison mode support
 *
 * Features:
 * - Rich visual display for magazine inspection
 * - Comparison mode (vs equipped magazine)
 * - Stat bars for visual representation
 * - NO programmatic color changes - ALL colors from materials in Editor!
 *
 * Layout:
 * ┌────────────────────────────────────────┐
 * │  [ICON]  STANAG 30-round              │  ← Header (inherited styling)
 * │          ★★★ Rare                      │  ← Rarity
 * ├────────────────────────────────────────┤
 * │  Rounds: 28/30                         │
 * │  ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ │  ← Fill bar
 * ├────────────────────────────────────────┤
 * │  Loaded: 5.56x45 M855A1               │  ← Ammo type
 * │  Damage: 45   Pen: 40                 │  ← Ammo stats
 * ├────────────────────────────────────────┤
 * │  Caliber: 5.56x45mm NATO              │
 * │  Weight: 0.45kg                       │
 * │  Durability: 95/100                   │
 * │  Reload: +5%                          │
 * └────────────────────────────────────────┘
 *
 * @see USuspenseCoreTooltipWidget - Base tooltip with animations
 * @see ISuspenseCoreMagazineTooltipWidgetInterface - Magazine-specific API
 */
UCLASS(Blueprintable, BlueprintType)
class UISYSTEM_API USuspenseCoreMagazineTooltipWidget : public USuspenseCoreTooltipWidget, public ISuspenseCoreMagazineTooltipWidgetInterface
{
	GENERATED_BODY()

public:
	USuspenseCoreMagazineTooltipWidget(const FObjectInitializer& ObjectInitializer);

	// ═══════════════════════════════════════════════════════════════════════════
	// UUserWidget Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void NativeConstruct() override;

	// ═══════════════════════════════════════════════════════════════════════════
	// ISuspenseCoreMagazineTooltipWidget Implementation
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void ShowMagazineTooltip_Implementation(const FSuspenseCoreMagazineTooltipData& TooltipData, const FVector2D& ScreenPosition) override;
	virtual void HideMagazineTooltip_Implementation() override;
	virtual void UpdateMagazineTooltip_Implementation(const FSuspenseCoreMagazineTooltipData& TooltipData) override;
	virtual void UpdateTooltipPosition_Implementation(const FVector2D& ScreenPosition) override;
	virtual void SetShowAmmoStats_Implementation(bool bShow) override;
	virtual void SetShowCompatibleWeapons_Implementation(bool bShow) override;
	virtual void SetComparisonMode_Implementation(bool bCompare, const FSuspenseCoreMagazineTooltipData& CompareData) override;
	virtual bool IsMagazineTooltipVisible_Implementation() const override;
	virtual FSuspenseCoreMagazineTooltipData GetCurrentTooltipData_Implementation() const override;

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Convert magazine data to base item UI data for inherited display
	 * @param MagazineData Magazine tooltip data
	 * @return Converted item UI data
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Tooltip|Magazine")
	FSuspenseCoreItemUIData ConvertToItemUIData(const FSuspenseCoreMagazineTooltipData& MagazineData) const;

	/**
	 * Get cached magazine data
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Tooltip|Magazine")
	const FSuspenseCoreMagazineTooltipData& GetMagazineData() const { return CachedMagazineData; }

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when magazine tooltip is shown (after animation starts) */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Tooltip|Magazine|Events")
	void OnMagazineTooltipShown();

	/** Called when magazine tooltip is hidden (after animation starts) */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Tooltip|Magazine|Events")
	void OnMagazineTooltipHidden();

	/** Called when magazine tooltip data is updated */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Tooltip|Magazine|Events")
	void OnMagazineTooltipUpdated();

	/** Called when comparison mode changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Tooltip|Magazine|Events")
	void OnMagazineComparisonChanged(bool bComparing);

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - MAGAZINE-SPECIFIC (BindWidget)
	// These are ADDITIONAL to the base tooltip bindings
	// ═══════════════════════════════════════════════════════════════════════════

	// --- Rounds Section ---

	/** Current rounds text (e.g., "28") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> CurrentRoundsText;

	/** Max capacity text (e.g., "/30") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> MaxCapacityText;

	/** Fill bar showing magazine fill percentage */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UProgressBar> FillBar;

	// --- Ammo Section ---

	/** Ammo section container - can be hidden */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UVerticalBox> AmmoSection;

	/** Loaded ammo type text (e.g., "5.56x45 M855A1") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> LoadedAmmoText;

	/** Loaded ammo icon */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> LoadedAmmoIcon;

	/** Ammo damage text (e.g., "DMG: 45") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> AmmoDamageText;

	/** Ammo penetration text (e.g., "PEN: 40") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> AmmoPenetrationText;

	/** Ammo fragmentation text (e.g., "FRAG: 20%") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> AmmoFragmentationText;

	// --- Stats Section ---

	/** Caliber text (e.g., "5.56x45mm NATO") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> CaliberText;

	/** Durability text (e.g., "95/100") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> DurabilityText;

	/** Durability bar */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UProgressBar> DurabilityBar;

	/** Reload modifier text (e.g., "+5%") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> ReloadModifierText;

	/** Ergonomics penalty text (e.g., "ERGO: -3") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> ErgonomicsText;

	/** Feed reliability text (e.g., "Reliability: 98%") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> ReliabilityText;

	// --- Compatibility Section ---

	/** Compatible weapons container - can be hidden */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UVerticalBox> CompatibleWeaponsSection;

	/** Compatible weapons list text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> CompatibleWeaponsText;

	// --- Comparison Section ---

	/** Comparison container - shown when comparing magazines */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UVerticalBox> ComparisonSection;

	/** Comparison rounds difference text (e.g., "+5") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> CompareRoundsText;

	/** Comparison capacity difference text (e.g., "-10") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> CompareCapacityText;

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Show ammo stats section (damage, penetration, frag)? */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Tooltip|Magazine|Config")
	bool bShowAmmoStats = true;

	/** Show compatible weapons section? */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Tooltip|Magazine|Config")
	bool bShowCompatibleWeapons = true;

	/** Show description section? */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Tooltip|Magazine|Config")
	bool bShowMagazineDescription = true;

	/** Reload modifier format (positive value) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Tooltip|Magazine|Config")
	FText ReloadModifierPositiveFormat = NSLOCTEXT("MagTooltip", "ReloadPos", "+{0}%");

	/** Reload modifier format (negative value) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Tooltip|Magazine|Config")
	FText ReloadModifierNegativeFormat = NSLOCTEXT("MagTooltip", "ReloadNeg", "{0}%");

	/** Max compatible weapons to display before truncating */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Tooltip|Magazine|Config", meta = (ClampMin = "1", ClampMax = "10"))
	int32 MaxCompatibleWeaponsDisplay = 3;

private:
	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL HELPERS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Update header section (name, icon, rarity) */
	void UpdateHeaderUI();

	/** Update rounds section (current/max, fill bar) */
	void UpdateRoundsUI();

	/** Update ammo section (type, stats) */
	void UpdateAmmoUI();

	/** Update stats section (caliber, durability, reload, etc) */
	void UpdateStatsUI();

	/** Update compatibility section (compatible weapons list) */
	void UpdateCompatibilityUI();

	/** Update comparison section (diff values when comparing magazines) */
	void UpdateComparisonUI();

	/** Refresh all magazine-specific UI sections */
	void RefreshMagazineUI();

	// ═══════════════════════════════════════════════════════════════════════════
	// STATE
	// ═══════════════════════════════════════════════════════════════════════════

	/** Cached magazine tooltip data */
	FSuspenseCoreMagazineTooltipData CachedMagazineData;

	/** Comparison magazine data (when comparing) */
	FSuspenseCoreMagazineTooltipData ComparisonMagazineData;

	/** Is comparison mode active? */
	bool bMagazineComparisonMode = false;
};
