// GE_InstantHeal.cpp
// Instant Healing GameplayEffect
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Effects/Medical/GE_InstantHeal.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "SuspenseCore/Tags/SuspenseCoreMedicalNativeTags.h"
#include "GameplayEffect.h"

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
		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = SuspenseCoreMedicalTags::Data::TAG_Data_Medical_InstantHeal;
		HealMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

		Modifiers.Add(HealMod);
	}

	// Effect asset tag for identification
	InheritableOwnedTagsContainer.AddTag(
		FGameplayTag::RequestGameplayTag(FName("Effect.Medical.InstantHeal")));

	UE_LOG(LogGE_InstantHeal, Log, TEXT("GE_InstantHeal: Configured with SetByCaller healing"));
}
