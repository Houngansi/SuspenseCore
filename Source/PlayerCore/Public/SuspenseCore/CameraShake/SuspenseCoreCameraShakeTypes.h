// SuspenseCoreCameraShakeTypes.h
// SuspenseCore - Layered Camera Shake Types
// Copyright Suspense Team. All Rights Reserved.
//
// Priority-based camera shake layer system for AAA-quality shake management.
// Supports concurrent shakes with priority blending (similar to Battlefield/CoD).

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "SuspenseCoreCameraShakeTypes.generated.h"

/**
 * Camera shake priority levels.
 * Higher priority shakes can override or blend with lower priority.
 */
UENUM(BlueprintType)
enum class ESuspenseCoreShakePriority : uint8
{
	/** Ambient shakes (wind, idle sway) - always running, lowest priority */
	Ambient = 0,
	/** Movement shakes (walking, running, landing) */
	Movement = 1,
	/** Weapon shakes (firing, reloading) */
	Weapon = 2,
	/** Combat shakes (taking damage) */
	Combat = 3,
	/** Environmental shakes (explosions nearby) */
	Environmental = 4,
	/** Cinematic/scripted shakes - highest priority */
	Cinematic = 5
};

/**
 * Shake blend mode for concurrent shakes.
 */
UENUM(BlueprintType)
enum class ESuspenseCoreShakeBlendMode : uint8
{
	/** Add shake to existing (cumulative) */
	Additive,
	/** Replace shakes of same or lower priority */
	Replace,
	/** Blend based on weight with existing shakes */
	Weighted,
	/** Override everything - exclusive shake */
	Override
};

/**
 * Individual camera shake layer.
 * Tracks a single shake instance with its priority and blend settings.
 */
USTRUCT(BlueprintType)
struct PLAYERCORE_API FSuspenseCoreShakeLayer
{
	GENERATED_BODY()

	/** The active shake instance */
	UPROPERTY()
	TWeakObjectPtr<UCameraShakeBase> ShakeInstance;

	/** Shake class for identification */
	UPROPERTY()
	TSubclassOf<UCameraShakeBase> ShakeClass;

	/** Priority level */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
	ESuspenseCoreShakePriority Priority = ESuspenseCoreShakePriority::Movement;

	/** Blend mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
	ESuspenseCoreShakeBlendMode BlendMode = ESuspenseCoreShakeBlendMode::Additive;

	/** Blend weight (0.0 - 1.0) for Weighted mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BlendWeight = 1.0f;

	/** Category tag for grouping (e.g., "Weapon.Rifle", "Movement.Landing") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layer")
	FName Category;

	/** Time when shake started */
	float StartTime = 0.0f;

	/** Is this layer currently active? */
	bool IsActive() const
	{
		return ShakeInstance.IsValid() && !ShakeInstance->IsFinished();
	}

	FSuspenseCoreShakeLayer()
		: Priority(ESuspenseCoreShakePriority::Movement)
		, BlendMode(ESuspenseCoreShakeBlendMode::Additive)
		, BlendWeight(1.0f)
		, StartTime(0.0f)
	{
	}

	FSuspenseCoreShakeLayer(
		TSubclassOf<UCameraShakeBase> InClass,
		ESuspenseCoreShakePriority InPriority,
		ESuspenseCoreShakeBlendMode InBlendMode = ESuspenseCoreShakeBlendMode::Additive,
		float InWeight = 1.0f)
		: ShakeClass(InClass)
		, Priority(InPriority)
		, BlendMode(InBlendMode)
		, BlendWeight(InWeight)
		, StartTime(0.0f)
	{
	}
};

/**
 * Camera shake layer configuration.
 * Used to define shake behavior when starting a new shake.
 */
USTRUCT(BlueprintType)
struct PLAYERCORE_API FSuspenseCoreShakeConfig
{
	GENERATED_BODY()

	/** Shake class to play */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	TSubclassOf<UCameraShakeBase> ShakeClass;

	/** Priority level */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	ESuspenseCoreShakePriority Priority = ESuspenseCoreShakePriority::Movement;

	/** Blend mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	ESuspenseCoreShakeBlendMode BlendMode = ESuspenseCoreShakeBlendMode::Additive;

	/** Scale multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (ClampMin = "0.0"))
	float Scale = 1.0f;

	/** Blend weight for Weighted mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BlendWeight = 1.0f;

	/** Category for grouping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	FName Category;

	/** Should this shake stop others of same category? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	bool bStopSameCategory = false;

	/** Max concurrent shakes of this category (0 = unlimited) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (ClampMin = "0"))
	int32 MaxConcurrent = 0;

	FSuspenseCoreShakeConfig()
		: Priority(ESuspenseCoreShakePriority::Movement)
		, BlendMode(ESuspenseCoreShakeBlendMode::Additive)
		, Scale(1.0f)
		, BlendWeight(1.0f)
		, bStopSameCategory(false)
		, MaxConcurrent(0)
	{
	}
};

/**
 * Static helper for shake layer management.
 */
struct PLAYERCORE_API FSuspenseCoreShakeLayerUtils
{
	/** Get priority as integer for comparison */
	static int32 GetPriorityValue(ESuspenseCoreShakePriority Priority)
	{
		return static_cast<int32>(Priority);
	}

	/** Check if NewPriority should override OldPriority */
	static bool ShouldOverride(ESuspenseCoreShakePriority NewPriority, ESuspenseCoreShakePriority OldPriority)
	{
		return GetPriorityValue(NewPriority) >= GetPriorityValue(OldPriority);
	}

	/** Calculate effective blend weight based on priorities */
	static float CalculateBlendWeight(
		ESuspenseCoreShakePriority ShakePriority,
		ESuspenseCoreShakePriority HighestActivePriority,
		float BaseWeight = 1.0f)
	{
		const int32 PriorityDiff = GetPriorityValue(HighestActivePriority) - GetPriorityValue(ShakePriority);

		if (PriorityDiff <= 0)
		{
			// Same or higher priority - full weight
			return BaseWeight;
		}

		// Lower priority - reduce weight per priority level (20% reduction per level)
		const float ReductionPerLevel = 0.2f;
		const float Reduction = FMath::Min(PriorityDiff * ReductionPerLevel, 0.8f);
		return BaseWeight * (1.0f - Reduction);
	}

	/** Get recommended blend mode for shake category */
	static ESuspenseCoreShakeBlendMode GetRecommendedBlendMode(FName Category)
	{
		// Weapon shakes should replace each other
		if (Category.ToString().StartsWith(TEXT("Weapon")))
		{
			return ESuspenseCoreShakeBlendMode::Replace;
		}

		// Environmental/explosion shakes are additive
		if (Category.ToString().StartsWith(TEXT("Explosion")) ||
			Category.ToString().StartsWith(TEXT("Environmental")))
		{
			return ESuspenseCoreShakeBlendMode::Additive;
		}

		// Movement shakes replace within category
		if (Category.ToString().StartsWith(TEXT("Movement")))
		{
			return ESuspenseCoreShakeBlendMode::Replace;
		}

		// Default to additive
		return ESuspenseCoreShakeBlendMode::Additive;
	}
};
