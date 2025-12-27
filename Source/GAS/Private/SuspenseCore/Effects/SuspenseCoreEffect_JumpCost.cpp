// SuspenseCoreEffect_JumpCost.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Effects/SuspenseCoreEffect_JumpCost.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"
#include "GameplayTagContainer.h"

USuspenseCoreEffect_JumpCost::USuspenseCoreEffect_JumpCost()
{
	// Instant effect - applied once on activation
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Configure stamina cost via SetByCaller
	{
		FGameplayModifierInfo StaminaCost;
		StaminaCost.Attribute = USuspenseCoreAttributeSet::GetStaminaAttribute();
		StaminaCost.ModifierOp = EGameplayModOp::Additive;

		// Use SetByCaller for dynamic stamina cost
		// Tag: Data.Cost.Stamina
		// The ability will set this value (e.g., -10 for 10 stamina cost)
		FSetByCallerFloat SetByCallerValue;
		SetByCallerValue.DataTag = SuspenseCoreTags::Data::Cost::Stamina;
		SetByCallerValue.DataName = NAME_None;

		StaminaCost.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCallerValue);
		Modifiers.Add(StaminaCost);
	}

	// Add asset tags for effect identification (Native Tags)
	UAssetTagsGameplayEffectComponent* AssetTagsComponent =
		CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("JumpCostAssetTagsComponent"));

	if (AssetTagsComponent)
	{
		FInheritedTagContainer AssetTagContainer;
		AssetTagContainer.Added.AddTag(SuspenseCoreTags::Effect::Movement::JumpCost);

		AssetTagsComponent->SetAndApplyAssetTagChanges(AssetTagContainer);
		GEComponents.Add(AssetTagsComponent);
	}

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreEffect_JumpCost: Configured with SetByCaller stamina cost"));
}
