// SuspenseCoreEffect_JumpCost.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Effects/SuspenseCoreEffect_JumpCost.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"
#include "GameplayTagContainer.h"

USuspenseCoreEffect_JumpCost::USuspenseCoreEffect_JumpCost()
{
	// Instant effect - applied once on activation
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Configure stamina modifier
	{
		FGameplayModifierInfo StaminaCost;
		StaminaCost.Attribute = USuspenseCoreAttributeSet::GetStaminaAttribute();
		StaminaCost.ModifierOp = EGameplayModOp::Additive;

		// Use SetByCaller for dynamic stamina cost
		FSetByCallerFloat SetByCallerValue;
		SetByCallerValue.DataTag = FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Cost.Stamina"));
		SetByCallerValue.DataName = NAME_None;

		StaminaCost.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCallerValue);

		Modifiers.Add(StaminaCost);
	}

	// Add asset tags component for effect identification
	UAssetTagsGameplayEffectComponent* AssetTagsComponent =
		CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("JumpCostAssetTagsComponent"));

	if (AssetTagsComponent)
	{
		FInheritedTagContainer AssetTagContainer;

		// Tags for effect identification
		AssetTagContainer.Added.AddTag(FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Effect.Cost.Jump")));
		AssetTagContainer.Added.AddTag(FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Effect.Cost.Stamina")));

		AssetTagsComponent->SetAndApplyAssetTagChanges(AssetTagContainer);
		GEComponents.Add(AssetTagsComponent);
	}

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreEffect_JumpCost: Configured with SetByCaller stamina cost"));
}
