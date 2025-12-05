// SuspenseCoreEffect_CrouchDebuff.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Effects/SuspenseCoreEffect_CrouchDebuff.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

USuspenseCoreEffect_CrouchDebuff::USuspenseCoreEffect_CrouchDebuff()
{
	// Infinite duration - active while crouch ability is active
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// Configure speed modifier for 50% reduction
	{
		FGameplayModifierInfo SpeedMod;

		// Get MovementSpeed attribute via reflection
		FProperty* Prop = FindFProperty<FProperty>(
			USuspenseCoreAttributeSet::StaticClass(),
			GET_MEMBER_NAME_CHECKED(USuspenseCoreAttributeSet, MovementSpeed)
		);

		SpeedMod.Attribute = FGameplayAttribute(Prop);
		SpeedMod.ModifierOp = EGameplayModOp::MultiplyAdditive;

		// For 50% reduction use -0.5f
		// Formula: current_speed + (current_speed * -0.5) = current_speed * 0.5
		SpeedMod.ModifierMagnitude = FScalableFloat(-0.5f);

		Modifiers.Add(SpeedMod);
	}

	// Add State.Crouching tag
	UTargetTagsGameplayEffectComponent* TagComponent =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("CrouchTargetTagsComponent"));

	if (TagComponent)
	{
		FInheritedTagContainer TagContainer;
		TagContainer.Added.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Crouching")));

		TagComponent->SetAndApplyTargetTagChanges(TagContainer);
		GEComponents.Add(TagComponent);
	}

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreEffect_CrouchDebuff: Crouch debuff created with 50%% speed decrease"));
}
