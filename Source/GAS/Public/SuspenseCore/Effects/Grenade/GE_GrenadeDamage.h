// GE_GrenadeDamage.h
// GameplayEffect for grenade explosion damage
// Copyright Suspense Team. All Rights Reserved.
//
// ARCHITECTURE:
// - Instant damage effect using SetByCaller magnitude
// - Modifies IncomingDamage meta-attribute (processed by PostGameplayEffectExecute)
// - Damage value set via Data.Damage tag (POSITIVE value)
// - Tagged with Effect.Damage.Grenade for GameplayCue support
//
// USAGE:
// Apply via GrenadeProjectile::ApplyExplosionDamage()
//   FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UGE_GrenadeDamage::StaticClass(), 1, Context);
//   Spec.Data->SetSetByCallerMagnitude(SuspenseCoreTags::Data::Damage, DamageAmount);  // POSITIVE!
//   ASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_GrenadeDamage.generated.h"

/**
 * UGE_GrenadeDamage
 *
 * Instant GameplayEffect for grenade explosion damage.
 * Uses SetByCaller magnitude for dynamic damage values based on distance.
 *
 * CONFIGURATION:
 * - DurationPolicy: Instant
 * - Attribute: IncomingDamage (meta-attribute, processed by PostGameplayEffectExecute)
 * - Damage Tag: Data.Damage (SetByCaller, POSITIVE value)
 * - Asset Tag: Effect.Damage.Grenade
 *
 * DAMAGE CALCULATION (done by GrenadeProjectile):
 * - Full damage at InnerRadius
 * - Falloff damage between InnerRadius and OuterRadius
 * - No damage beyond OuterRadius
 * - Line-of-sight check (walls block damage)
 *
 * @see ASuspenseCoreGrenadeProjectile
 * @see GE_FlashbangEffect
 * @see GE_IncendiaryEffect
 */
UCLASS()
class GAS_API UGE_GrenadeDamage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_GrenadeDamage();
};

/**
 * UGE_GrenadeDamage_Shrapnel
 *
 * Variant for shrapnel/fragment damage with different tags.
 * Used for grenades that deal both blast and shrapnel damage.
 */
UCLASS()
class GAS_API UGE_GrenadeDamage_Shrapnel : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_GrenadeDamage_Shrapnel();
};
