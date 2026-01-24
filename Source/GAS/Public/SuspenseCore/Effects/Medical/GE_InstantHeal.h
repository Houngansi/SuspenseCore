// GE_InstantHeal.h
// Instant Healing GameplayEffect
// Copyright Suspense Team. All Rights Reserved.
//
// USAGE:
// Apply this effect to instantly heal the target.
// Heal amount is passed via SetByCaller with tag Data.Medical.InstantHeal.
//
// Example:
//   FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UGE_InstantHeal::StaticClass(), 1.0f, Context);
//   Spec.Data->SetSetByCallerMagnitude(SuspenseCoreTags::Data::Medical::InstantHeal, 50.0f);
//   ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Effects/SuspenseCoreEffect.h"
#include "GE_InstantHeal.generated.h"

/**
 * UGE_InstantHeal
 *
 * Instant healing effect for medical items.
 * Heals the target immediately by a SetByCaller amount.
 *
 * MECHANICS:
 * - Duration: Instant
 * - Modifies: IncomingHealing meta-attribute
 * - SetByCaller: Data.Medical.InstantHeal
 *
 * The IncomingHealing meta-attribute is processed in
 * USuspenseCoreAttributeSet::PostGameplayEffectExecute() to clamp health.
 *
 * @see HealingSystem_DesignProposal.md
 * @see StatusEffects_DeveloperGuide.md Section 8
 */
UCLASS()
class GAS_API UGE_InstantHeal : public USuspenseCoreEffect
{
	GENERATED_BODY()

public:
	UGE_InstantHeal(const FObjectInitializer& ObjectInitializer);
};
