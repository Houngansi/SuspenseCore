// SuspenseCoreEffect_StaminaRegen.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Effects/SuspenseCoreEffect_StaminaRegen.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"

USuspenseCoreEffect_StaminaRegen::USuspenseCoreEffect_StaminaRegen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Infinite duration, 10 ticks/s
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = 0.1f;

	// +1 STA per tick = +10 STA/s
	{
		FGameplayModifierInfo Mod;
		Mod.Attribute = USuspenseCoreAttributeSet::GetStaminaAttribute();
		Mod.ModifierOp = EGameplayModOp::Additive;
		Mod.ModifierMagnitude = FScalableFloat(0.5f);
		Modifiers.Add(Mod);
	}

	// Create tag filter component
	auto* TagReq = ObjectInitializer.CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(
		this, TEXT("StaminaRegenTagReq"));

	// Block regeneration while sprinting or dead
	TagReq->OngoingTagRequirements.IgnoreTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Sprinting")));
	TagReq->OngoingTagRequirements.IgnoreTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Dead")));

	GEComponents.Add(TagReq);

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreEffect_StaminaRegen: Configured with +5 STA/s regeneration"));
}
