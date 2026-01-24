// GE_HealOverTime.h
// Heal Over Time (HoT) GameplayEffect
// Copyright Suspense Team. All Rights Reserved.
//
// USAGE:
// Apply this effect for sustained healing over time.
// Duration and heal-per-tick are passed via SetByCaller.
//
// Example:
//   FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UGE_HealOverTime::StaticClass(), 1.0f, Context);
//   Spec.Data->SetSetByCallerMagnitude(SuspenseCoreMedicalTags::Data::TAG_Data_Medical_HealPerTick, 5.0f);
//   Spec.Data->SetSetByCallerMagnitude(SuspenseCoreMedicalTags::Data::TAG_Data_Medical_HoTDuration, 10.0f);
//   ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
//
// MECHANICS (Tarkov-style):
// - HoT is interrupted when taking damage
// - Healing continues while moving/sprinting
// - Does NOT stack (refreshes on reapply)
//
// @see HealingSystem_DesignProposal.md

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Effects/SuspenseCoreEffect.h"
#include "GE_HealOverTime.generated.h"

/**
 * UGE_HealOverTime
 *
 * Heal Over Time effect for medical items.
 * Heals target periodically until duration expires or interrupted.
 *
 * MECHANICS:
 * - Duration: HasDuration (SetByCaller)
 * - Period: 1.0s (configurable in constructor)
 * - Modifies: IncomingHealing meta-attribute
 * - SetByCaller: Data.Medical.HealPerTick, Data.Medical.HoTDuration
 * - Grants: State.Medical.Regenerating tag
 *
 * INTERRUPTION:
 * - Effect is cancelled when State.Damaged tag is applied
 * - Remaining heal time is lost on interruption
 *
 * STACKING:
 * - Does NOT stack (AggregateBySource)
 * - Reapplying refreshes duration
 *
 * @see StatusEffects_DeveloperGuide.md Section 8
 * @see HealingSystem_DesignProposal.md
 */
UCLASS()
class GAS_API UGE_HealOverTime : public USuspenseCoreEffect
{
	GENERATED_BODY()

public:
	UGE_HealOverTime(const FObjectInitializer& ObjectInitializer);
};
