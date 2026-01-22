// SuspenseCoreThrowableTypes.h
// Unified types for throwable items (grenades, etc.)
// Copyright Suspense Team. All Rights Reserved.
//
// PURPOSE:
// Provides shared type definitions for the throwable/grenade system.
// These types are used across GAS, EquipmentSystem, and other modules.
//
// USAGE:
// #include "SuspenseCore/Types/Throwable/SuspenseCoreThrowableTypes.h"
//
// MODULES USING THIS:
// - BridgeSystem (owner)
// - EquipmentSystem (GrenadeHandler, GrenadeProjectile)
// - GAS (GrenadeThrowAbility, GrenadeEquipAbility)

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCoreThrowableTypes.generated.h"

/**
 * ESuspenseCoreGrenadeType
 *
 * Unified grenade type enum for the entire grenade system.
 * Determines grenade behavior, damage type, and visual effects.
 *
 * Used by:
 * - USuspenseCoreGrenadeHandler (Handler for equip/throw flow)
 * - ASuspenseCoreGrenadeProjectile (Physics projectile actor)
 * - FSuspenseCoreGrenadeExplosionData (Explosion parameters)
 *
 * Tarkov-style grenade types:
 * - Fragmentation: Standard explosive damage in radius
 * - Smoke: Creates smoke screen, no damage
 * - Flashbang: Blinds and deafens targets
 * - Incendiary: Creates fire zone with DoT
 * - Impact: Explodes on first impact (no fuse)
 */
UENUM(BlueprintType)
enum class ESuspenseCoreGrenadeType : uint8
{
	/** Standard fragmentation grenade - explosive damage with shrapnel */
	Fragmentation   UMETA(DisplayName = "Fragmentation"),

	/** Smoke grenade - creates smoke screen, blocks visibility */
	Smoke           UMETA(DisplayName = "Smoke"),

	/** Flashbang/stun grenade - blinds and deafens targets */
	Flashbang       UMETA(DisplayName = "Flashbang"),

	/** Incendiary/molotov - creates fire zone with damage over time */
	Incendiary      UMETA(DisplayName = "Incendiary"),

	/** Impact grenade - explodes on first impact, no fuse timer */
	Impact          UMETA(DisplayName = "Impact")
};

/**
 * ESuspenseCoreGrenadeThrowType
 *
 * Type of throw for grenade animations and physics.
 * Determines throw angle, force, and animation selection.
 */
UENUM(BlueprintType)
enum class ESuspenseCoreGrenadeThrowType : uint8
{
	/** Standard overhand throw - longest range, arc trajectory */
	Overhand    UMETA(DisplayName = "Overhand"),

	/** Underhand throw - shorter range, lower arc for tight spaces */
	Underhand   UMETA(DisplayName = "Underhand"),

	/** Roll/slide throw - grenade rolls along ground */
	Roll        UMETA(DisplayName = "Roll")
};

/**
 * FSuspenseCoreGrenadeExplosionData
 *
 * Data structure for grenade explosion parameters.
 * Passed to damage calculation and effect spawning systems.
 *
 * Used by:
 * - ASuspenseCoreGrenadeProjectile::ApplyExplosionDamage()
 * - GE_GrenadeDamage (SetByCaller magnitude)
 * - FOnGrenadeExploded delegate
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreGrenadeExplosionData
{
	GENERATED_BODY()

	/** World location of explosion epicenter */
	UPROPERTY(BlueprintReadWrite, Category = "Explosion")
	FVector ExplosionLocation = FVector::ZeroVector;

	/** Inner radius - targets receive full damage */
	UPROPERTY(BlueprintReadWrite, Category = "Explosion|Radius")
	float InnerRadius = 200.0f;

	/** Outer radius - damage falls off to zero at this distance */
	UPROPERTY(BlueprintReadWrite, Category = "Explosion|Radius")
	float OuterRadius = 500.0f;

	/** Base damage at epicenter (before falloff) */
	UPROPERTY(BlueprintReadWrite, Category = "Explosion|Damage")
	float BaseDamage = 250.0f;

	/** Damage falloff exponent (1.0 = linear, 2.0 = quadratic) */
	UPROPERTY(BlueprintReadWrite, Category = "Explosion|Damage")
	float DamageFalloff = 1.0f;

	/** Grenade type - determines effects and damage type */
	UPROPERTY(BlueprintReadWrite, Category = "Explosion")
	ESuspenseCoreGrenadeType GrenadeType = ESuspenseCoreGrenadeType::Fragmentation;

	/** Actor who threw the grenade (for damage attribution) */
	UPROPERTY(BlueprintReadWrite, Category = "Explosion")
	TWeakObjectPtr<AActor> Instigator;

	/** Default constructor */
	FSuspenseCoreGrenadeExplosionData() = default;

	/** Constructor with basic parameters */
	FSuspenseCoreGrenadeExplosionData(
		const FVector& InLocation,
		float InBaseDamage,
		float InInnerRadius,
		float InOuterRadius,
		ESuspenseCoreGrenadeType InType = ESuspenseCoreGrenadeType::Fragmentation)
		: ExplosionLocation(InLocation)
		, InnerRadius(InInnerRadius)
		, OuterRadius(InOuterRadius)
		, BaseDamage(InBaseDamage)
		, DamageFalloff(1.0f)
		, GrenadeType(InType)
	{
	}

	/**
	 * Calculate damage at given distance from explosion
	 * @param Distance Distance from explosion epicenter
	 * @return Calculated damage (0 if outside OuterRadius)
	 */
	float CalculateDamageAtDistance(float Distance) const
	{
		if (Distance <= InnerRadius)
		{
			return BaseDamage;
		}

		if (Distance >= OuterRadius)
		{
			return 0.0f;
		}

		// Linear falloff between inner and outer radius
		const float FalloffRange = OuterRadius - InnerRadius;
		const float FalloffDistance = Distance - InnerRadius;
		const float FalloffAlpha = FMath::Pow(1.0f - (FalloffDistance / FalloffRange), DamageFalloff);

		return BaseDamage * FalloffAlpha;
	}

	/** Check if a point is within explosion radius */
	bool IsInRadius(const FVector& Point) const
	{
		return FVector::Dist(ExplosionLocation, Point) <= OuterRadius;
	}

	/** Check if a point is in full damage zone */
	bool IsInFullDamageZone(const FVector& Point) const
	{
		return FVector::Dist(ExplosionLocation, Point) <= InnerRadius;
	}
};

/**
 * Helper functions for grenade type conversion and utilities
 */
namespace SuspenseCoreGrenadeUtils
{
	/**
	 * Get display name for grenade type
	 * @param Type Grenade type enum value
	 * @return Human-readable display name
	 */
	inline FString GetGrenadeTypeDisplayName(ESuspenseCoreGrenadeType Type)
	{
		switch (Type)
		{
			case ESuspenseCoreGrenadeType::Fragmentation: return TEXT("Fragmentation");
			case ESuspenseCoreGrenadeType::Smoke:         return TEXT("Smoke");
			case ESuspenseCoreGrenadeType::Flashbang:     return TEXT("Flashbang");
			case ESuspenseCoreGrenadeType::Incendiary:    return TEXT("Incendiary");
			case ESuspenseCoreGrenadeType::Impact:        return TEXT("Impact");
			default:                                       return TEXT("Unknown");
		}
	}

	/**
	 * Check if grenade type deals direct damage
	 * @param Type Grenade type
	 * @return true if grenade deals explosive/impact damage
	 */
	inline bool DoesDealDamage(ESuspenseCoreGrenadeType Type)
	{
		switch (Type)
		{
			case ESuspenseCoreGrenadeType::Fragmentation:
			case ESuspenseCoreGrenadeType::Incendiary:
			case ESuspenseCoreGrenadeType::Impact:
				return true;
			case ESuspenseCoreGrenadeType::Smoke:
			case ESuspenseCoreGrenadeType::Flashbang:
				return false;
			default:
				return false;
		}
	}

	/**
	 * Check if grenade type has fuse timer
	 * @param Type Grenade type
	 * @return true if grenade uses fuse timer (false for Impact)
	 */
	inline bool HasFuseTimer(ESuspenseCoreGrenadeType Type)
	{
		return Type != ESuspenseCoreGrenadeType::Impact;
	}

	/**
	 * Get default fuse time for grenade type
	 * @param Type Grenade type
	 * @return Default fuse time in seconds
	 */
	inline float GetDefaultFuseTime(ESuspenseCoreGrenadeType Type)
	{
		switch (Type)
		{
			case ESuspenseCoreGrenadeType::Fragmentation: return 3.5f;
			case ESuspenseCoreGrenadeType::Smoke:         return 2.0f;
			case ESuspenseCoreGrenadeType::Flashbang:     return 2.5f;
			case ESuspenseCoreGrenadeType::Incendiary:    return 3.0f;
			case ESuspenseCoreGrenadeType::Impact:        return 0.0f;  // No fuse
			default:                                       return 3.5f;
		}
	}
}
