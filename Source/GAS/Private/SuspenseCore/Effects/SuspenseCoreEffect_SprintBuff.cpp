// SuspenseCoreEffect_SprintBuff.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Effects/SuspenseCoreEffect_SprintBuff.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

USuspenseCoreEffect_SprintBuff::USuspenseCoreEffect_SprintBuff()
{
	// Infinite duration - active while sprint ability is active
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// Configure speed modifier for 50% increase
	{
		FGameplayModifierInfo SpeedMod;

		// Get MovementSpeed attribute via reflection
		FProperty* Prop = FindFProperty<FProperty>(
			USuspenseCoreAttributeSet::StaticClass(),
			GET_MEMBER_NAME_CHECKED(USuspenseCoreAttributeSet, MovementSpeed)
		);

		SpeedMod.Attribute = FGameplayAttribute(Prop);
		SpeedMod.ModifierOp = EGameplayModOp::MultiplyAdditive;

		// CRITICAL: For 50% increase use 0.5f, NOT 1.5f!
		// MultiplyAdditive formula: Result = Base + (Base * Magnitude)
		// Example: if base speed 300, then:
		// 300 + (300 * 0.5) = 300 + 150 = 450 (50% increase)
		SpeedMod.ModifierMagnitude = FScalableFloat(0.5f);

		Modifiers.Add(SpeedMod);
	}

	// Add State.Sprinting tag
	UTargetTagsGameplayEffectComponent* TagComponent =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("SprintTargetTagsComponent"));

	if (TagComponent)
	{
		FInheritedTagContainer TagContainer;
		TagContainer.Added.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Sprinting")));

		TagComponent->SetAndApplyTargetTagChanges(TagContainer);
		GEComponents.Add(TagComponent);
	}

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreEffect_SprintBuff: Sprint buff created with 50%% speed increase"));
}
