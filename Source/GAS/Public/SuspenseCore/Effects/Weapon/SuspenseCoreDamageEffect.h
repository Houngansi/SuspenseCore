// SuspenseCoreDamageEffect.h
// SuspenseCore - Weapon Damage GameplayEffect
// Copyright Suspense Team. All Rights Reserved.
//
// Instant damage effect using SetByCaller magnitude.
// Modifies IncomingDamage meta-attribute (processed by PostGameplayEffectExecute).
// Damage value is set at runtime via Data.Damage tag (POSITIVE value).
//
// Usage:
//   FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(USuspenseCoreDamageEffect::StaticClass(), 1, Context);
//   SpecHandle.Data->SetSetByCallerMagnitude(SuspenseCoreTags::Data::Damage, 50.0f);  // POSITIVE!
//   ASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "SuspenseCoreDamageEffect.generated.h"

/**
 * USuspenseCoreDamageEffect
 *
 * Instant GameplayEffect for applying weapon damage.
 * Uses SetByCaller magnitude with Data.Damage tag.
 * Modifies IncomingDamage meta-attribute (processed by PostGameplayEffectExecute).
 *
 * Features:
 * - Instant duration (damage applied immediately)
 * - SetByCaller magnitude for dynamic damage values
 * - IncomingDamage attribute (armor reduction handled in AttributeSet)
 * - Tagged with Effect.Damage for GameplayCue support
 * - Supports headshot multipliers via caller
 *
 * IMPORTANT: Damage values should be POSITIVE (PostGameplayEffectExecute handles reduction).
 * Example: SetSetByCallerMagnitude(Data.Damage, 50.0f) for 50 damage
 *
 * @see SuspenseCoreTags::Data::Damage
 * @see USuspenseCoreAttributeSet::PostGameplayEffectExecute
 */
UCLASS()
class GAS_API USuspenseCoreDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USuspenseCoreDamageEffect();
};

/**
 * USuspenseCoreDamageEffect_WithHitInfo
 *
 * Extended damage effect that preserves hit information.
 * Use when you need to track where damage came from.
 */
UCLASS()
class GAS_API USuspenseCoreDamageEffect_WithHitInfo : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USuspenseCoreDamageEffect_WithHitInfo();
};

/**
 * Utility functions for applying damage effects.
 */
UCLASS()
class GAS_API USuspenseCoreDamageEffectLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Apply instant damage to target.
	 *
	 * @param Instigator Actor causing the damage
	 * @param Target Target actor to damage
	 * @param DamageAmount Amount of damage (positive value)
	 * @param HitResult Optional hit result for context
	 * @return True if damage was applied
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Damage")
	static bool ApplyDamageToTarget(
		AActor* Instigator,
		AActor* Target,
		float DamageAmount,
		const FHitResult& HitResult
	);

	/**
	 * Apply damage with headshot check.
	 * Automatically applies headshot multiplier if hit bone indicates head.
	 *
	 * @param Instigator Actor causing the damage
	 * @param Target Target actor to damage
	 * @param BaseDamage Base damage amount (before multipliers)
	 * @param HitResult Hit result for bone checking
	 * @param HeadshotMultiplier Multiplier for headshots (default 2.0)
	 * @return True if damage was applied
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Damage")
	static bool ApplyDamageWithHeadshotCheck(
		AActor* Instigator,
		AActor* Target,
		float BaseDamage,
		const FHitResult& HitResult,
		float HeadshotMultiplier = 2.0f
	);

	/**
	 * Create damage effect spec with proper context.
	 *
	 * @param Instigator Actor causing the damage
	 * @param DamageAmount Amount of damage (positive value)
	 * @param HitResult Optional hit result for context
	 * @return Effect spec handle ready for application
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Damage")
	static FGameplayEffectSpecHandle CreateDamageSpec(
		AActor* Instigator,
		float DamageAmount,
		const FHitResult& HitResult
	);
};
