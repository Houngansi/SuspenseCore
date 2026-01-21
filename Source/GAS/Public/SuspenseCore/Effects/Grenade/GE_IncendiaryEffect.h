// GE_IncendiaryEffect.h
// GameplayEffect for incendiary/thermite grenade burn damage
// Copyright Suspense Team. All Rights Reserved.
//
// ARCHITECTURE:
// - Duration-based effect with periodic damage ticks
// - Applies State.Burning tag
// - Periodic damage via Data.Damage.Burn tag
// - Stacking behavior: refresh duration on reapply
//
// USAGE:
// Apply via GrenadeProjectile for incendiary type or when entering fire zone
//   FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UGE_IncendiaryEffect::StaticClass(), 1, Context);
//   Spec.Data->SetSetByCallerMagnitude(Data.Damage.Burn, DamagePerTick);  // POSITIVE!
//   Spec.Data->SetSetByCallerMagnitude(Data.Grenade.BurnDuration, Duration);
//   ASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_IncendiaryEffect.generated.h"

/**
 * UGE_IncendiaryEffect
 *
 * Periodic damage-over-time GameplayEffect for fire/burn damage.
 * Applies damage every tick while active.
 *
 * CONFIGURATION:
 * - DurationPolicy: HasDuration (SetByCaller)
 * - Duration Tag: Data.Grenade.BurnDuration
 * - Period: 0.5s (damage tick interval)
 * - Damage Tag: Data.Damage.Burn (SetByCaller, per tick)
 * - Granted Tag: State.Burning
 * - Asset Tag: Effect.Grenade.Incendiary
 *
 * STACKING:
 * - AggregateBySource: Multiple sources stack
 * - Duration refreshes on reapply from same source
 * - Max 3 stacks from different sources
 *
 * TYPICAL VALUES (Tarkov-style):
 * - Duration: 5-10 seconds
 * - Damage per tick: 5-15 HP
 * - Total damage: 50-300 HP over full duration
 *
 * @see ASuspenseCoreGrenadeProjectile
 * @see UGE_GrenadeDamage
 */
UCLASS()
class GAS_API UGE_IncendiaryEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_IncendiaryEffect();
};

/**
 * UGE_IncendiaryEffect_Zone
 *
 * Effect applied while standing in incendiary zone.
 * Shorter duration but reapplied continuously while in zone.
 */
UCLASS()
class GAS_API UGE_IncendiaryEffect_Zone : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_IncendiaryEffect_Zone();
};
