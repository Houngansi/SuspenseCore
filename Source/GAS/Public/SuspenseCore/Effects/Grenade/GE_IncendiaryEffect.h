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

/**
 * UGE_IncendiaryEffect_ArmorBypass
 *
 * SPECIAL: Incendiary effect that BYPASSES ARMOR and damages BOTH
 * shield/armor AND health simultaneously.
 *
 * DESIGN RATIONALE (Tarkov-style):
 * Fire doesn't care about armor - it burns through and damages everything.
 * Each tick deals damage to BOTH Armor AND Health:
 * - Example: 5 damage to Armor + 5 damage to Health per tick
 *
 * CONFIGURATION:
 * - DurationPolicy: HasDuration (SetByCaller)
 * - Duration Tag: Data.Grenade.BurnDuration
 * - Period: 0.5s (damage tick interval)
 * - Armor Damage Tag: Data.Damage.Burn.Armor (SetByCaller, per tick)
 * - Health Damage Tag: Data.Damage.Burn.Health (SetByCaller, per tick)
 * - Granted Tag: State.Burning
 * - Asset Tag: Effect.Grenade.Incendiary.ArmorBypass
 *
 * DUAL DAMAGE MODIFIERS:
 * 1. Modifier 1: Reduces Armor directly (no resistance)
 * 2. Modifier 2: Reduces Health directly (bypasses IncomingDamage)
 *
 * TYPICAL VALUES:
 * - Duration: 5-8 seconds
 * - Armor damage per tick: 5 HP
 * - Health damage per tick: 5 HP
 * - Total: 10 HP equivalent per tick (5 armor + 5 health)
 *
 * USAGE:
 * Apply via GrenadeProjectile for incendiary type:
 *   FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(
 *       UGE_IncendiaryEffect_ArmorBypass::StaticClass(), 1, Context);
 *   Spec.Data->SetSetByCallerMagnitude(Data.Damage.Burn.Armor, 5.0f);
 *   Spec.Data->SetSetByCallerMagnitude(Data.Damage.Burn.Health, 5.0f);
 *   Spec.Data->SetSetByCallerMagnitude(Data.Grenade.BurnDuration, Duration);
 *   ASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
 *
 * @see ASuspenseCoreGrenadeProjectile
 * @see UGE_IncendiaryEffect (standard version with armor)
 */
UCLASS()
class GAS_API UGE_IncendiaryEffect_ArmorBypass : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_IncendiaryEffect_ArmorBypass();
};
