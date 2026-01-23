// SuspenseCoreDoTUITypes.h
// SuspenseCore - Clean Architecture Bridge Layer
// Copyright (c) 2025. All Rights Reserved.
//
// SSOT: DoT UI Data
// Single source of truth for debuff/buff icon display data.
// Used by W_DebuffIcon, W_DebuffContainer, and future buff widgets.
//
// USAGE:
// 1. Create DataTable asset: DT_DoTUIData (row type: FSuspenseCoreDoTUIData)
// 2. Configure via SuspenseCoreDataManager::LoadDoTUIDataTable()
// 3. Query via SuspenseCoreDataManager::GetDoTUIData(DoTType)
//
// @see W_DebuffIcon.h
// @see W_DebuffContainer.h
// @see Documentation/Plans/DebuffWidget_System_Plan.md

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreDoTUITypes.generated.h"

class UTexture2D;

/**
 * DoT UI display data - SSOT for debuff/buff visuals
 *
 * Row Name should match DoT type tag (e.g., "State.Health.Bleeding.Light")
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreDoTUIData : public FTableRowBase
{
	GENERATED_BODY()

	// ═══════════════════════════════════════════════════════════════════
	// IDENTITY
	// ═══════════════════════════════════════════════════════════════════

	/** DoT type tag (e.g., State.Health.Bleeding.Light) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FGameplayTag DoTType;

	/** Localized display name for UI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FText DisplayName;

	/** Localized description/tooltip */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FText Description;

	// ═══════════════════════════════════════════════════════════════════
	// VISUALS
	// ═══════════════════════════════════════════════════════════════════

	/** Icon texture (64x64 recommended) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Normal tint color for icon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	FLinearColor NormalColor = FLinearColor::White;

	/** Critical/warning tint color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	FLinearColor CriticalColor = FLinearColor(1.0f, 0.3f, 0.3f, 1.0f);

	/** Background color for icon frame */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	FLinearColor BackgroundColor = FLinearColor(0.1f, 0.1f, 0.1f, 0.8f);

	// ═══════════════════════════════════════════════════════════════════
	// BEHAVIOR
	// ═══════════════════════════════════════════════════════════════════

	/** Is this an infinite duration effect (e.g., bleeding)? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	bool bIsInfiniteDuration = false;

	/** Should show duration bar? (false for infinite effects) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	bool bShowDurationBar = true;

	/** Should pulse when critical/low duration? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	bool bPulseOnCritical = true;

	/** Priority for display ordering (higher = show first) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior", meta = (ClampMin = "0", ClampMax = "100"))
	int32 DisplayPriority = 50;

	// ═══════════════════════════════════════════════════════════════════
	// CATEGORY
	// ═══════════════════════════════════════════════════════════════════

	/** Is this a debuff (harmful)? False = buff (beneficial) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Category")
	bool bIsDebuff = true;

	/** Category tag for grouping (e.g., Effect.Category.Health, Effect.Category.Movement) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Category")
	FGameplayTag CategoryTag;

	// ═══════════════════════════════════════════════════════════════════
	// HELPERS
	// ═══════════════════════════════════════════════════════════════════

	/** Check if this is a bleeding effect */
	bool IsBleeding() const
	{
		return DoTType.ToString().Contains(TEXT("Bleeding"));
	}

	/** Check if this is a burning effect */
	bool IsBurning() const
	{
		return DoTType.ToString().Contains(TEXT("Burn"));
	}
};

/**
 * Cached DoT UI data for runtime queries
 * Used internally by SuspenseCoreDataManager
 */
USTRUCT()
struct BRIDGESYSTEM_API FSuspenseCoreDoTUICache
{
	GENERATED_BODY()

	/** Map of DoT type → UI data (built from DataTable) */
	UPROPERTY()
	TMap<FGameplayTag, FSuspenseCoreDoTUIData> DataMap;

	/** Is cache populated? */
	bool bIsLoaded = false;

	/** Get UI data for DoT type (returns nullptr if not found) */
	const FSuspenseCoreDoTUIData* Find(FGameplayTag DoTType) const
	{
		return DataMap.Find(DoTType);
	}

	/** Get UI data with fallback to parent tag */
	const FSuspenseCoreDoTUIData* FindWithFallback(FGameplayTag DoTType) const
	{
		// Try exact match first
		if (const FSuspenseCoreDoTUIData* Data = DataMap.Find(DoTType))
		{
			return Data;
		}

		// Try parent tag (e.g., State.Health.Bleeding if State.Health.Bleeding.Light not found)
		FGameplayTag ParentTag = DoTType.RequestDirectParent();
		if (ParentTag.IsValid())
		{
			return DataMap.Find(ParentTag);
		}

		return nullptr;
	}
};
