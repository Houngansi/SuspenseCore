// GE_HealOverTime.cpp
// Heal Over Time (HoT) GameplayEffect
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Effects/Medical/GE_HealOverTime.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "SuspenseCore/Tags/SuspenseCoreMedicalNativeTags.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "GameplayEffect.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/RemoveOtherGameplayEffectComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogGE_HealOverTime, Log, All);

UGE_HealOverTime::UGE_HealOverTime(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Duration-based effect with SetByCaller duration
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// SetByCaller duration - use assignment style for FNativeGameplayTag compatibility
	{
		FSetByCallerFloat DurationSetByCaller;
		DurationSetByCaller.DataTag = SuspenseCoreMedicalTags::Data::TAG_Data_Medical_HoTDuration;
		DurationMagnitude = FGameplayEffectModifierMagnitude(DurationSetByCaller);
	}

	// Tick every 1 second
	Period = 1.0f;

	// Healing modifier - applies each tick
	{
		FGameplayModifierInfo HealMod;
		HealMod.Attribute = USuspenseCoreAttributeSet::GetIncomingHealingAttribute();
		HealMod.ModifierOp = EGameplayModOp::Additive;

		// SetByCaller magnitude - heal per tick
		FSetByCallerFloat HealSetByCaller;
		HealSetByCaller.DataTag = SuspenseCoreMedicalTags::Data::TAG_Data_Medical_HealPerTick;
		HealMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(HealSetByCaller);

		Modifiers.Add(HealMod);
	}

	// Stacking configuration
	StackingType = EGameplayEffectStackingType::AggregateBySource;
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;

	// Grant regenerating state tag while active using UTargetTagsGameplayEffectComponent
	// IMPORTANT: Use State.Health.Regenerating to integrate with W_DebuffContainer
	// This matches the tag in SuspenseCoreStatusEffectVisuals.json
	UTargetTagsGameplayEffectComponent* TargetTagsComponent =
		ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			this, TEXT("HoTTargetTags"));

	if (TargetTagsComponent)
	{
		FInheritedTagContainer TagContainer;
		TagContainer.Added.AddTag(SuspenseCoreTags::State::Health::Regenerating);
		TargetTagsComponent->SetAndApplyTargetTagChanges(TagContainer);
		GEComponents.Add(TargetTagsComponent);
	}

	// Effect asset tag for identification using UAssetTagsGameplayEffectComponent
	UAssetTagsGameplayEffectComponent* AssetTagsComponent =
		ObjectInitializer.CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(
			this, TEXT("HoTAssetTags"));

	if (AssetTagsComponent)
	{
		FInheritedTagContainer AssetTagContainer;
		AssetTagContainer.Added.AddTag(SuspenseCoreMedicalTags::Effect::TAG_Effect_Medical_HealOverTime);
		AssetTagsComponent->SetAndApplyAssetTagChanges(AssetTagContainer);
		GEComponents.Add(AssetTagsComponent);
	}

	// Create cancel-on-damage component
	// HoT is cancelled when taking damage (Tarkov-style)
	UTargetTagRequirementsGameplayEffectComponent* CancelComponent =
		ObjectInitializer.CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(
			this, TEXT("HoTCancelOnDamage"));

	if (CancelComponent)
	{
		// Effect is removed when State.Damaged tag is present
		CancelComponent->RemovalTagRequirements.RequireTags.AddTag(SuspenseCoreTags::State::Damaged);
		GEComponents.Add(CancelComponent);
	}

	UE_LOG(LogGE_HealOverTime, Log, TEXT("GE_HealOverTime: Configured with SetByCaller HoT, cancels on damage"));
}
