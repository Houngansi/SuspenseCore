// SuspenseCoreEffect_SprintBuff.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Effects/SuspenseCoreEffect_SprintBuff.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"

USuspenseCoreEffect_SprintBuff::USuspenseCoreEffect_SprintBuff()
{
	// Infinite duration - active while sprint ability is active
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// Configure speed modifier via SetByCaller
	{
		FGameplayModifierInfo SpeedMod;

		// Get MovementSpeed attribute via reflection
		FProperty* Prop = FindFProperty<FProperty>(
			USuspenseCoreAttributeSet::StaticClass(),
			GET_MEMBER_NAME_CHECKED(USuspenseCoreAttributeSet, MovementSpeed)
		);

		SpeedMod.Attribute = FGameplayAttribute(Prop);
		SpeedMod.ModifierOp = EGameplayModOp::MultiplyAdditive;

		// Use SetByCaller for configurable speed multiplier
		// Tag: Data.Cost.SpeedMultiplier
		// The ability will set this value (e.g., 0.5 for +50% speed)
		// Formula: Result = Base + (Base * Magnitude)
		FSetByCallerFloat SetByCallerValue;
		SetByCallerValue.DataTag = SuspenseCoreTags::Data::Cost::SpeedMultiplier;

		SpeedMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCallerValue);
		Modifiers.Add(SpeedMod);
	}

	// Add State.Sprinting tag (Native Tag)
	UTargetTagsGameplayEffectComponent* TagComponent =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("SprintTargetTagsComponent"));

	if (TagComponent)
	{
		FInheritedTagContainer TagContainer;
		TagContainer.Added.AddTag(SuspenseCoreTags::State::Sprinting);

		TagComponent->SetAndApplyTargetTagChanges(TagContainer);
		GEComponents.Add(TagComponent);
	}

	// Add asset tag for effect identification (Native Tag)
	UAssetTagsGameplayEffectComponent* AssetTagsComponent =
		CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("SprintBuffAssetTags"));

	if (AssetTagsComponent)
	{
		FInheritedTagContainer AssetTagContainer;
		AssetTagContainer.Added.AddTag(SuspenseCoreTags::Effect::Movement::SprintBuff);

		AssetTagsComponent->SetAndApplyAssetTagChanges(AssetTagContainer);
		GEComponents.Add(AssetTagsComponent);
	}

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreEffect_SprintBuff: Configured with SetByCaller speed multiplier"));
}
