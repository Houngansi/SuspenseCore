// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "ISuspenseCoreMagazineTooltipWidget.generated.h"

// Forward declarations
class UTexture2D;

/**
 * Data structure for magazine tooltip display (Tarkov-style)
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreMagazineTooltipData
{
	GENERATED_BODY()

	//================================================
	// Magazine Identity
	//================================================

	/** Magazine ID */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Identity")
	FName MagazineID;

	/** Magazine display name */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Identity")
	FText DisplayName;

	/** Magazine description */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Identity")
	FText Description;

	/** Magazine icon */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Identity")
	TObjectPtr<UTexture2D> Icon = nullptr;

	/** Rarity tag */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Identity")
	FGameplayTag RarityTag;

	//================================================
	// Magazine Stats
	//================================================

	/** Current round count */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Stats")
	int32 CurrentRounds = 0;

	/** Maximum capacity */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Stats")
	int32 MaxCapacity = 0;

	/** Current durability (0-100) */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Stats")
	float Durability = 100.0f;

	/** Max durability */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Stats")
	float MaxDurability = 100.0f;

	//================================================
	// Compatibility
	//================================================

	/** Caliber tag */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Compatibility")
	FGameplayTag CaliberTag;

	/** Caliber display name */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Compatibility")
	FText CaliberDisplayName;

	/** Compatible weapons list */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Compatibility")
	TArray<FText> CompatibleWeaponNames;

	//================================================
	// Loaded Ammo
	//================================================

	/** Loaded ammo type ID */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Ammo")
	FName LoadedAmmoID;

	/** Loaded ammo display name */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Ammo")
	FText LoadedAmmoName;

	/** Loaded ammo icon */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Ammo")
	TObjectPtr<UTexture2D> LoadedAmmoIcon = nullptr;

	//================================================
	// Ammo Stats (from loaded ammo type)
	//================================================

	/** Base damage of loaded ammo */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|AmmoStats")
	float AmmoDamage = 0.0f;

	/** Armor penetration of loaded ammo */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|AmmoStats")
	float AmmoArmorPenetration = 0.0f;

	/** Fragmentation chance of loaded ammo */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|AmmoStats")
	float AmmoFragmentationChance = 0.0f;

	//================================================
	// Physical Properties
	//================================================

	/** Empty magazine weight (kg) */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Physical")
	float EmptyWeight = 0.0f;

	/** Weight per round (kg) */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Physical")
	float WeightPerRound = 0.0f;

	/** Ergonomics penalty when attached */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Physical")
	float ErgonomicsPenalty = 0.0f;

	//================================================
	// Reload Stats
	//================================================

	/** Load time per round (seconds) */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Reload")
	float LoadTimePerRound = 0.0f;

	/** Reload time modifier (1.0 = normal) */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Reload")
	float ReloadTimeModifier = 1.0f;

	/** Feed reliability (0-1, affects jam chance) */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Reload")
	float FeedReliability = 1.0f;

	//================================================
	// Helpers
	//================================================

	/** Get fill percentage (0-1) */
	float GetFillPercent() const
	{
		if (MaxCapacity <= 0) return 0.0f;
		return FMath::Clamp((float)CurrentRounds / (float)MaxCapacity, 0.0f, 1.0f);
	}

	/** Get durability percentage (0-1) */
	float GetDurabilityPercent() const
	{
		if (MaxDurability <= 0.0f) return 0.0f;
		return FMath::Clamp(Durability / MaxDurability, 0.0f, 1.0f);
	}

	/** Get total weight (empty + ammo) */
	float GetTotalWeight() const
	{
		return EmptyWeight + (WeightPerRound * CurrentRounds);
	}

	/** Is magazine empty? */
	bool IsEmpty() const { return CurrentRounds <= 0; }

	/** Is magazine full? */
	bool IsFull() const { return CurrentRounds >= MaxCapacity; }

	/** Is durability low? */
	bool IsDurabilityLow() const { return GetDurabilityPercent() < 0.25f; }
};

UINTERFACE(MinimalAPI, BlueprintType)
class UISuspenseCoreMagazineTooltipWidget : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for Magazine Tooltip widget (Tarkov-style)
 *
 * Displays detailed magazine information:
 * - Magazine name, icon, rarity
 * - Current/Max rounds with fill bar
 * - Loaded ammo type and stats
 * - Caliber compatibility
 * - Durability
 * - Weight
 * - Reload modifiers
 *
 * Used when hovering over magazines in:
 * - Inventory
 * - Equipment slots
 * - QuickSlots
 */
class BRIDGESYSTEM_API ISuspenseCoreMagazineTooltipWidget
{
	GENERATED_BODY()

public:
	//================================================
	// Show/Hide
	//================================================

	/**
	 * Show tooltip with magazine data
	 * @param TooltipData Magazine data to display
	 * @param ScreenPosition Screen position for tooltip
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Tooltip|Magazine")
	void ShowMagazineTooltip(const FSuspenseCoreMagazineTooltipData& TooltipData, const FVector2D& ScreenPosition);

	/**
	 * Hide the tooltip
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Tooltip|Magazine")
	void HideMagazineTooltip();

	/**
	 * Update tooltip data (while showing)
	 * @param TooltipData Updated magazine data
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Tooltip|Magazine")
	void UpdateMagazineTooltip(const FSuspenseCoreMagazineTooltipData& TooltipData);

	/**
	 * Update tooltip position
	 * @param ScreenPosition New screen position
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Tooltip|Magazine")
	void UpdateTooltipPosition(const FVector2D& ScreenPosition);

	//================================================
	// Display Options
	//================================================

	/**
	 * Set whether to show ammo stats section
	 * @param bShow Should show ammo stats?
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Tooltip|Magazine")
	void SetShowAmmoStats(bool bShow);

	/**
	 * Set whether to show compatible weapons
	 * @param bShow Should show compatible weapons?
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Tooltip|Magazine")
	void SetShowCompatibleWeapons(bool bShow);

	/**
	 * Set comparison mode (compare with currently loaded magazine)
	 * @param bCompare Enable comparison mode?
	 * @param CompareData Data of magazine to compare against
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Tooltip|Magazine")
	void SetComparisonMode(bool bCompare, const FSuspenseCoreMagazineTooltipData& CompareData);

	//================================================
	// State
	//================================================

	/**
	 * Check if tooltip is currently visible
	 * @return True if visible
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Tooltip|Magazine")
	bool IsMagazineTooltipVisible() const;

	/**
	 * Get current tooltip data
	 * @return Current magazine tooltip data
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Tooltip|Magazine")
	FSuspenseCoreMagazineTooltipData GetCurrentTooltipData() const;
};
