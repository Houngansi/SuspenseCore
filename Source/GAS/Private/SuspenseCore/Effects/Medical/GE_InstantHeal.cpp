// GE_InstantHeal.cpp
// Instant Healing GameplayEffect
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Effects/Medical/GE_InstantHeal.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "SuspenseCore/Tags/SuspenseCoreMedicalNativeTags.h"
#include "GameplayEffect.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogGE_InstantHeal, Log, All);

UGE_InstantHeal::UGE_InstantHeal(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Instant effect - applies once and completes
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Modify IncomingHealing meta-attribute
	// The AttributeSet's PostGameplayEffectExecute will process this
	// and apply it to Health with proper clamping
	{
		FGameplayModifierInfo HealMod;
		HealMod.Attribute = USuspenseCoreAttributeSet::GetIncomingHealingAttribute();
		HealMod.ModifierOp = EGameplayModOp::Additive;

		// SetByCaller magnitude - caller provides heal amount
		FSetByCallerFloat HealSetByCaller;
		HealSetByCaller.DataTag = SuspenseCoreMedicalTags::Data::TAG_Data_Medical_InstantHeal;
		HealMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(HealSetByCaller);

		Modifiers.Add(HealMod);
	}

	// Effect asset tag for identification using UAssetTagsGameplayEffectComponent
	UAssetTagsGameplayEffectComponent* AssetTagsComponent =
		ObjectInitializer.CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(
			this, TEXT("InstantHealAssetTags"));

	if (AssetTagsComponent)
	{
		FInheritedTagContainer AssetTagContainer;
		AssetTagContainer.Added.AddTag(SuspenseCoreMedicalTags::Effect::TAG_Effect_Medical_InstantHeal);
		AssetTagsComponent->SetAndApplyAssetTagChanges(AssetTagContainer);
		GEComponents.Add(AssetTagsComponent);
	}

	UE_LOG(LogGE_InstantHeal, Log, TEXT("GE_InstantHeal: Configured with SetByCaller healing"));
}
