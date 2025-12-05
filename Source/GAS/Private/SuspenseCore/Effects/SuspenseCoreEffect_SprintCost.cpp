// SuspenseCoreEffect_SprintCost.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Effects/SuspenseCoreEffect_SprintCost.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

USuspenseCoreEffect_SprintCost::USuspenseCoreEffect_SprintCost()
{
	// Infinite duration with periodic execution
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = 0.1f;

	// Enable periodic execution for stamina drain
	bExecutePeriodicEffectOnApplication = true;

	// Configure how the periodic effect behaves when inhibited
	PeriodicInhibitionPolicy = EGameplayEffectPeriodInhibitionRemovedPolicy::NeverReset;

	// Drain 1 stamina every 0.1 seconds (10 stamina per second)
	{
		FGameplayModifierInfo StaminaDrain;
		StaminaDrain.Attribute = USuspenseCoreAttributeSet::GetStaminaAttribute();
		StaminaDrain.ModifierOp = EGameplayModOp::Additive;
		StaminaDrain.ModifierMagnitude = FScalableFloat(-1.0f);
		Modifiers.Add(StaminaDrain);
	}

	// Add State.Sprinting tag to block stamina regeneration
	UTargetTagsGameplayEffectComponent* TagComponent =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("SprintingTagComponent"));

	if (TagComponent)
	{
		FInheritedTagContainer TagContainer;
		TagContainer.Added.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Sprinting")));

		TagComponent->SetAndApplyTargetTagChanges(TagContainer);
		GEComponents.Add(TagComponent);
	}

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreEffect_SprintCost: Configured with periodic stamina drain (-10/sec)"));
}
