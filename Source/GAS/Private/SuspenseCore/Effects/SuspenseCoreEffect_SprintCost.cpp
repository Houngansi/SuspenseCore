// SuspenseCoreEffect_SprintCost.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Effects/SuspenseCoreEffect_SprintCost.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"

USuspenseCoreEffect_SprintCost::USuspenseCoreEffect_SprintCost()
{
	// Infinite duration with periodic execution
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = 0.1f;

	// Enable periodic execution for stamina drain
	bExecutePeriodicEffectOnApplication = true;

	// Configure how the periodic effect behaves when inhibited
	PeriodicInhibitionPolicy = EGameplayEffectPeriodInhibitionRemovedPolicy::NeverReset;

	// Configure stamina drain via SetByCaller
	// Value is set dynamically by the ability when applying this effect
	{
		FGameplayModifierInfo StaminaDrain;
		StaminaDrain.Attribute = USuspenseCoreAttributeSet::GetStaminaAttribute();
		StaminaDrain.ModifierOp = EGameplayModOp::Additive;

		// Use SetByCaller for configurable stamina cost
		// Tag: Data.Cost.StaminaPerSecond
		// The ability will set this value (e.g., -1.5 for 15 stamina/sec with 0.1s period)
		FSetByCallerFloat SetByCallerValue;
		SetByCallerValue.DataTag = SuspenseCoreTags::Data::Cost::StaminaPerSecond;

		StaminaDrain.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCallerValue);
		Modifiers.Add(StaminaDrain);
	}

	// Add State.Sprinting tag to block stamina regeneration (Native Tag)
	UTargetTagsGameplayEffectComponent* TagComponent =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("SprintingTagComponent"));

	if (TagComponent)
	{
		FInheritedTagContainer TagContainer;
		TagContainer.Added.AddTag(SuspenseCoreTags::State::Sprinting);

		TagComponent->SetAndApplyTargetTagChanges(TagContainer);
		GEComponents.Add(TagComponent);
	}

	// Add asset tag for effect identification (Native Tag)
	UAssetTagsGameplayEffectComponent* AssetTagsComponent =
		CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("SprintCostAssetTags"));

	if (AssetTagsComponent)
	{
		FInheritedTagContainer AssetTagContainer;
		AssetTagContainer.Added.AddTag(SuspenseCoreTags::Effect::Movement::SprintCost);

		AssetTagsComponent->SetAndApplyAssetTagChanges(AssetTagContainer);
		GEComponents.Add(AssetTagsComponent);
	}

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreEffect_SprintCost: Configured with SetByCaller stamina drain"));
}
