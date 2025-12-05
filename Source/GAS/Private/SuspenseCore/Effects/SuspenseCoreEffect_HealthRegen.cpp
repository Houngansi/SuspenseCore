// SuspenseCoreEffect_HealthRegen.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Effects/SuspenseCoreEffect_HealthRegen.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"

USuspenseCoreEffect_HealthRegen::USuspenseCoreEffect_HealthRegen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Infinite duration, 10 ticks/s
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = 0.1f;

	// +0.5 HP per tick = +5 HP/s
	{
		FGameplayModifierInfo Mod;
		Mod.Attribute = USuspenseCoreAttributeSet::GetHealthAttribute();
		Mod.ModifierOp = EGameplayModOp::Additive;
		Mod.ModifierMagnitude = FScalableFloat(0.5f);
		Modifiers.Add(Mod);
	}

	// Create tag filter component
	auto* TagReq = ObjectInitializer.CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(
		this, TEXT("HealthRegenTagReq"));

	// Block regeneration while sprinting or dead
	TagReq->OngoingTagRequirements.IgnoreTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Sprinting")));
	TagReq->OngoingTagRequirements.IgnoreTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Dead")));

	GEComponents.Add(TagReq);

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreEffect_HealthRegen: Configured with +5 HP/s regeneration"));
}
