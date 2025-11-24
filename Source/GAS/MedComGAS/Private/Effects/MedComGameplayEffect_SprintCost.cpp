// Copyright MedCom Team. All Rights Reserved.

#include "Effects/MedComGameplayEffect_SprintCost.h"
#include "Attributes/MedComBaseAttributeSet.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UMedComGameplayEffect_SprintCost::UMedComGameplayEffect_SprintCost()
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
		StaminaDrain.Attribute = UMedComBaseAttributeSet::GetStaminaAttribute();
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

	UE_LOG(LogTemp, Log, TEXT("SprintCostEffect: Configured with periodic stamina drain (-10/sec)"));
}