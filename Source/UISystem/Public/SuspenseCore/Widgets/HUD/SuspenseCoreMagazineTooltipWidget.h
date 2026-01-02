// SuspenseCoreMagazineTooltipWidget.h
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
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
 * Tarkov-style magazine tooltip widget displaying detailed information:
 * - Magazine name, icon, rarity
 * - Current/Max rounds with fill bar
 * - Loaded ammo type and stats
 * - Caliber compatibility
 * - Durability
 * - Weight
 * - Reload modifiers
 *
 * Features:
 * - Rich visual display
 * - Comparison mode (vs equipped magazine)
 * - Stat bars for visual representation
 * - NO programmatic color changes - ALL colors from materials in Editor!
 *
 * Layout:
 * ┌────────────────────────────────────────┐
 * │  [ICON]  STANAG 30-round              │  ← Header
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
 */
UCLASS(Blueprintable, BlueprintType)
class UISYSTEM_API USuspenseCoreMagazineTooltipWidget : public UUserWidget, public ISuspenseCoreMagazineTooltipWidget
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
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when tooltip is shown */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Tooltip|Events")
	void OnTooltipShown();

	/** Called when tooltip is hidden */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Tooltip|Events")
	void OnTooltipHidden();

	/** Called when tooltip data is updated */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Tooltip|Events")
	void OnTooltipUpdated();

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Header
	// ═══════════════════════════════════════════════════════════════════════════

	/** Magazine icon */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> MagazineIcon;

	/** Magazine display name */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> MagazineNameText;

	/** Rarity text/stars */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RarityText;

	/** Rarity icon */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> RarityIcon;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Rounds Section
	// ═══════════════════════════════════════════════════════════════════════════

	/** Current rounds text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> CurrentRoundsText;

	/** Max capacity text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> MaxCapacityText;

	/** Fill bar */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UProgressBar> FillBar;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Ammo Section
	// ═══════════════════════════════════════════════════════════════════════════

	/** Ammo section container */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> AmmoSection;

	/** Loaded ammo type text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LoadedAmmoText;

	/** Loaded ammo icon */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> LoadedAmmoIcon;

	/** Ammo damage text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AmmoDamageText;

	/** Ammo penetration text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AmmoPenetrationText;

	/** Ammo fragmentation text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AmmoFragmentationText;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Stats Section
	// ═══════════════════════════════════════════════════════════════════════════

	/** Caliber text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> CaliberText;

	/** Weight text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WeightText;

	/** Durability text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DurabilityText;

	/** Durability bar */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> DurabilityBar;

	/** Reload modifier text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReloadModifierText;

	/** Ergonomics penalty text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ErgonomicsText;

	/** Feed reliability text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReliabilityText;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Compatibility Section
	// ═══════════════════════════════════════════════════════════════════════════

	/** Compatible weapons container */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> CompatibleWeaponsSection;

	/** Compatible weapons list text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CompatibleWeaponsText;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Comparison Section
	// ═══════════════════════════════════════════════════════════════════════════

	/** Comparison container */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> ComparisonSection;

	/** Comparison rounds diff text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CompareRoundsText;

	/** Comparison capacity diff text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CompareCapacityText;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Description
	// ═══════════════════════════════════════════════════════════════════════════

	/** Description text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DescriptionText;

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Show ammo stats section? */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Tooltip|Config")
	bool bShowAmmoStats = true;

	/** Show compatible weapons? */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Tooltip|Config")
	bool bShowCompatibleWeapons = true;

	/** Show description? */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Tooltip|Config")
	bool bShowDescription = true;

	/** Offset from cursor position */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Tooltip|Config")
	FVector2D CursorOffset = FVector2D(15.0f, 15.0f);

	/** Weight display format */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Tooltip|Config")
	FText WeightFormat = NSLOCTEXT("Tooltip", "Weight", "{0} kg");

	/** Durability display format */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Tooltip|Config")
	FText DurabilityFormat = NSLOCTEXT("Tooltip", "Durability", "{0}/{1}");

	/** Reload modifier format (positive) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Tooltip|Config")
	FText ReloadModifierPositiveFormat = NSLOCTEXT("Tooltip", "ReloadPos", "+{0}%");

	/** Reload modifier format (negative) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Tooltip|Config")
	FText ReloadModifierNegativeFormat = NSLOCTEXT("Tooltip", "ReloadNeg", "{0}%");

private:
	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL HELPERS
	// ═══════════════════════════════════════════════════════════════════════════

	void UpdateHeaderUI();
	void UpdateRoundsUI();
	void UpdateAmmoUI();
	void UpdateStatsUI();
	void UpdateCompatibilityUI();
	void UpdateComparisonUI();
	void UpdateDescriptionUI();
	void ApplyPosition(const FVector2D& ScreenPosition);

	// ═══════════════════════════════════════════════════════════════════════════
	// STATE
	// ═══════════════════════════════════════════════════════════════════════════

	FSuspenseCoreMagazineTooltipData CachedTooltipData;
	FSuspenseCoreMagazineTooltipData ComparisonData;

	bool bIsVisible = false;
	bool bComparisonMode = false;
};
