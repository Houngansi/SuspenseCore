// GE_FlashbangEffect.h
// GameplayEffect for flashbang blind/deafen effects
// Copyright Suspense Team. All Rights Reserved.
//
// ARCHITECTURE:
// - Duration-based effect (SetByCaller for dynamic duration)
// - Applies State.Blinded and State.Deafened tags
// - Modifies Perception attributes if available
// - Triggers GameplayCue for screen flash effect
//
// USAGE:
// Apply via GrenadeProjectile::ApplyExplosionDamage() for flashbang type
//   FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UGE_FlashbangEffect::StaticClass(), 1, Context);
//   Spec.Data->SetSetByCallerMagnitude(Data.Grenade.FlashDuration, Duration);
//   ASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_FlashbangEffect.generated.h"

/**
 * UGE_FlashbangEffect
 *
 * Duration-based GameplayEffect for flashbang blind/deafen.
 * Applies screen flash, hearing reduction, and movement penalty.
 *
 * CONFIGURATION:
 * - DurationPolicy: HasDuration (SetByCaller)
 * - Duration Tag: Data.Grenade.FlashDuration
 * - Granted Tags: State.Blinded, State.Deafened
 * - Asset Tag: Effect.Grenade.Flashbang
 *
 * EFFECTS:
 * - Full blindness for first 50% of duration
 * - Gradual vision recovery for remaining 50%
 * - Audio muffling throughout duration
 * - 30% movement speed reduction
 *
 * DISTANCE SCALING:
 * Duration is scaled by distance from explosion:
 * - Point blank: Full duration (5s default)
 * - At radius edge: Minimum duration (1s)
 * - Looking away: 50% duration reduction
 *
 * @see ASuspenseCoreGrenadeProjectile
 * @see UGE_GrenadeDamage
 */
UCLASS()
class GAS_API UGE_FlashbangEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_FlashbangEffect();
};

/**
 * UGE_FlashbangEffect_Partial
 *
 * Reduced flashbang effect for targets at edge of radius
 * or looking away from the explosion.
 */
UCLASS()
class GAS_API UGE_FlashbangEffect_Partial : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_FlashbangEffect_Partial();
};
