// GE_BleedingEffect.h
// GameplayEffect for bleeding damage over time from grenade shrapnel
// Copyright Suspense Team. All Rights Reserved.
//
// ARCHITECTURE:
// - Infinite duration effect until removed by healing item
// - Periodic damage ticks (1-3 HP/sec configurable via SetByCaller)
// - ONLY applied when target armor is 0 (shrapnel must penetrate)
// - Applies State.Health.Bleeding.Light or State.Health.Bleeding.Heavy tag
//
// USAGE:
// Apply via GrenadeProjectile::ApplyExplosionDamage() for Fragmentation type:
//   // First check if target armor == 0
//   if (TargetASC->GetNumericAttribute(GetArmorAttribute()) <= 0.0f)
//   {
//       FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UGE_BleedingEffect::StaticClass(), 1, Context);
//       Spec.Data->SetSetByCallerMagnitude(Data.Damage.Bleed, DamagePerTick);  // POSITIVE (1-3)
//       ASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
//   }
//
// REMOVAL:
// - Bandage removes GE_BleedingEffect_Light
// - Medkit/Surgery removes both Light and Heavy
// - Use RemoveActiveGameplayEffectBySourceEffect() or tag-based removal

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_BleedingEffect.generated.h"

/**
 * UGE_BleedingEffect_Light
 *
 * Light bleeding effect from minor shrapnel wounds.
 * Infinite duration - ticks until healed with bandage/medkit.
 *
 * CONFIGURATION:
 * - DurationPolicy: Infinite
 * - Period: 1.0s (damage tick interval)
 * - Damage Tag: Data.Damage.Bleed (SetByCaller, 1-2 HP/tick)
 * - Granted Tag: State.Health.Bleeding.Light
 * - Asset Tag: Effect.Damage.Bleed.Light
 *
 * REQUIREMENTS FOR APPLICATION:
 * - Target Armor == 0 (checked by GrenadeProjectile before applying)
 * - Shrapnel must penetrate to cause bleeding
 *
 * VISUAL CUES:
 * - Light blood screen overlay
 * - Periodic blood splatter on ground
 * - Audible heartbeat increase
 *
 * REMOVAL:
 * - Bandage (Item.Consumable.Bandage)
 * - Medkit (Item.Consumable.Medkit)
 * - Surgery Kit (Item.Consumable.Surgery)
 *
 * TYPICAL VALUES (Tarkov-style):
 * - Damage per tick: 1-2 HP
 * - Stack limit: 1 (reapplication refreshes)
 * - Total potential damage: Unlimited until healed
 *
 * @see ASuspenseCoreGrenadeProjectile
 * @see UGE_GrenadeDamage_Shrapnel
 * @see FSuspenseCoreConsumableAttributeRow::bCanHealLightBleed
 */
UCLASS()
class GAS_API UGE_BleedingEffect_Light : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_BleedingEffect_Light();
};

/**
 * UGE_BleedingEffect_Heavy
 *
 * Heavy bleeding effect from severe shrapnel wounds.
 * Infinite duration - requires tourniquet/surgery to stop.
 *
 * CONFIGURATION:
 * - DurationPolicy: Infinite
 * - Period: 1.0s (damage tick interval)
 * - Damage Tag: Data.Damage.Bleed (SetByCaller, 3-5 HP/tick)
 * - Granted Tag: State.Health.Bleeding.Heavy
 * - Asset Tag: Effect.Damage.Bleed.Heavy
 *
 * REQUIREMENTS FOR APPLICATION:
 * - Target Armor == 0
 * - Multiple fragment hits OR critical location hit
 *
 * VISUAL CUES:
 * - Heavy blood screen overlay
 * - Blood trail on movement
 * - Audible labored breathing
 *
 * REMOVAL:
 * - Tourniquet (temporary - 180s)
 * - Medkit (Item.Consumable.Medkit with bCanHealHeavyBleed)
 * - Surgery Kit (Item.Consumable.Surgery)
 *
 * TYPICAL VALUES:
 * - Damage per tick: 3-5 HP
 * - Stack limit: 1
 * - CRITICAL: Can kill if not treated
 *
 * @see ASuspenseCoreGrenadeProjectile
 * @see UGE_GrenadeDamage_Shrapnel
 */
UCLASS()
class GAS_API UGE_BleedingEffect_Heavy : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_BleedingEffect_Heavy();
};
