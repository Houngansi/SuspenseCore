// GE_BleedingEffect.cpp
// GameplayEffect for bleeding damage over time from grenade shrapnel
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Effects/Grenade/GE_BleedingEffect.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogGE_BleedingEffect, Log, All);

//========================================================================
// UGE_BleedingEffect_Light
//========================================================================

UGE_BleedingEffect_Light::UGE_BleedingEffect_Light(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// ═══════════════════════════════════════════════════════════════════
	// DURATION: Infinite until healed
	// Bleeding continues until bandage/medkit removes it
	// ═══════════════════════════════════════════════════════════════════
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// ═══════════════════════════════════════════════════════════════════
	// PERIODIC DAMAGE: Every 1 second
	// Tarkov-style bleeding ticks at regular intervals
	// ═══════════════════════════════════════════════════════════════════
	Period = 1.0f;
	bExecutePeriodicEffectOnApplication = false;  // First tick after 1s

	// ═══════════════════════════════════════════════════════════════════
	// DAMAGE MODIFIER: Uses IncomingDamage meta-attribute
	// PostGameplayEffectExecute will process this with armor/resistances
	// IMPORTANT: Bleeding bypasses armor (already penetrated)
	// ═══════════════════════════════════════════════════════════════════
	FGameplayModifierInfo DamageModifier;
	DamageModifier.Attribute = USuspenseCoreAttributeSet::GetIncomingDamageAttribute();
	DamageModifier.ModifierOp = EGameplayModOp::Additive;

	// SetByCaller magnitude for configurable damage per tick
	// Default: 1-2 HP per tick for light bleed
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = SuspenseCoreTags::Data::DoT::Bleed;

	DamageModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	Modifiers.Add(DamageModifier);

	// ═══════════════════════════════════════════════════════════════════
	// STACKING: Tarkov-style - multiple bleeds stack and multiply damage
	// 3 light bleeds = 3x damage per tick
	// NOTE: Stacking API will be made private in future UE versions.
	//       Epic has not yet provided a component-based alternative.
	//       @see GE_FlashbangEffect.cpp for consistent project pattern
	// ═══════════════════════════════════════════════════════════════════
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 5;  // Max 5 stacks of light bleeding
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackPeriodResetPolicy = EGameplayEffectStackingPeriodPolicy::ResetOnSuccessfulApplication;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// ═══════════════════════════════════════════════════════════════════
	// GRANTED TAGS: State.Health.Bleeding.Light
	// Using UTargetTagsGameplayEffectComponent for proper ASC registration
	// This allows RemoveActiveEffectsWithGrantedTags() to find this effect
	// ═══════════════════════════════════════════════════════════════════
	UTargetTagsGameplayEffectComponent* TargetTagsComponent =
		ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			this, TEXT("LightBleedTargetTags"));

	if (TargetTagsComponent)
	{
		FInheritedTagContainer TagContainer;
		TagContainer.Added.AddTag(SuspenseCoreTags::State::Health::BleedingLight);
		TargetTagsComponent->SetAndApplyTargetTagChanges(TagContainer);
		GEComponents.Add(TargetTagsComponent);
	}

	// ═══════════════════════════════════════════════════════════════════
	// ASSET TAGS: For identification and GameplayCue
	// Using UAssetTagsGameplayEffectComponent
	// ═══════════════════════════════════════════════════════════════════
	UAssetTagsGameplayEffectComponent* AssetTagsComponent =
		ObjectInitializer.CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(
			this, TEXT("LightBleedAssetTags"));

	if (AssetTagsComponent)
	{
		FInheritedTagContainer AssetTagContainer;
		AssetTagContainer.Added.AddTag(SuspenseCoreTags::Effect::Damage);
		AssetTagContainer.Added.AddTag(SuspenseCoreTags::Effect::DamageBleed);
		AssetTagContainer.Added.AddTag(SuspenseCoreTags::Effect::DamageBleedLight);
		AssetTagContainer.Added.AddTag(SuspenseCoreTags::Effect::DoT::Root);
		AssetTagContainer.Added.AddTag(SuspenseCoreTags::Effect::GrenadeShrapnel);
		AssetTagsComponent->SetAndApplyAssetTagChanges(AssetTagContainer);
		GEComponents.Add(AssetTagsComponent);
	}

	UE_LOG(LogGE_BleedingEffect, Log, TEXT("GE_BleedingEffect_Light: Configured - Infinite duration, 1s period, SetByCaller damage, grants State.Health.Bleeding.Light"));
}

//========================================================================
// UGE_BleedingEffect_Heavy
//========================================================================

UGE_BleedingEffect_Heavy::UGE_BleedingEffect_Heavy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// ═══════════════════════════════════════════════════════════════════
	// DURATION: Infinite until healed
	// Heavy bleeding is more dangerous - requires surgery/medkit
	// ═══════════════════════════════════════════════════════════════════
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// ═══════════════════════════════════════════════════════════════════
	// PERIODIC DAMAGE: Every 1 second
	// Higher damage per tick than light bleeding
	// ═══════════════════════════════════════════════════════════════════
	Period = 1.0f;
	bExecutePeriodicEffectOnApplication = false;

	// ═══════════════════════════════════════════════════════════════════
	// DAMAGE MODIFIER: Higher damage for heavy bleed
	// Default: 3-5 HP per tick
	// ═══════════════════════════════════════════════════════════════════
	FGameplayModifierInfo DamageModifier;
	DamageModifier.Attribute = USuspenseCoreAttributeSet::GetIncomingDamageAttribute();
	DamageModifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = SuspenseCoreTags::Data::DoT::Bleed;

	DamageModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	Modifiers.Add(DamageModifier);

	// ═══════════════════════════════════════════════════════════════════
	// STACKING: Tarkov-style - multiple heavy bleeds stack and multiply damage
	// 3 heavy bleeds = 3x damage per tick (very dangerous!)
	// NOTE: Stacking API will be made private in future UE versions.
	//       Epic has not yet provided a component-based alternative.
	//       @see GE_FlashbangEffect.cpp for consistent project pattern
	// ═══════════════════════════════════════════════════════════════════
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 5;  // Max 5 stacks of heavy bleeding
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackPeriodResetPolicy = EGameplayEffectStackingPeriodPolicy::ResetOnSuccessfulApplication;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// ═══════════════════════════════════════════════════════════════════
	// GRANTED TAGS: State.Health.Bleeding.Heavy
	// Using UTargetTagsGameplayEffectComponent for proper ASC registration
	// This allows RemoveActiveEffectsWithGrantedTags() to find this effect
	// ═══════════════════════════════════════════════════════════════════
	UTargetTagsGameplayEffectComponent* TargetTagsComponent =
		ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(
			this, TEXT("HeavyBleedTargetTags"));

	if (TargetTagsComponent)
	{
		FInheritedTagContainer TagContainer;
		TagContainer.Added.AddTag(SuspenseCoreTags::State::Health::BleedingHeavy);
		TargetTagsComponent->SetAndApplyTargetTagChanges(TagContainer);
		GEComponents.Add(TargetTagsComponent);
	}

	// ═══════════════════════════════════════════════════════════════════
	// ASSET TAGS: For identification and GameplayCue
	// Using UAssetTagsGameplayEffectComponent
	// ═══════════════════════════════════════════════════════════════════
	UAssetTagsGameplayEffectComponent* AssetTagsComponent =
		ObjectInitializer.CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(
			this, TEXT("HeavyBleedAssetTags"));

	if (AssetTagsComponent)
	{
		FInheritedTagContainer AssetTagContainer;
		AssetTagContainer.Added.AddTag(SuspenseCoreTags::Effect::Damage);
		AssetTagContainer.Added.AddTag(SuspenseCoreTags::Effect::DamageBleed);
		AssetTagContainer.Added.AddTag(SuspenseCoreTags::Effect::DamageBleedHeavy);
		AssetTagContainer.Added.AddTag(SuspenseCoreTags::Effect::DoT::Root);
		AssetTagContainer.Added.AddTag(SuspenseCoreTags::Effect::GrenadeShrapnel);
		AssetTagsComponent->SetAndApplyAssetTagChanges(AssetTagContainer);
		GEComponents.Add(AssetTagsComponent);
	}

	UE_LOG(LogGE_BleedingEffect, Log, TEXT("GE_BleedingEffect_Heavy: Configured - Infinite duration, 1s period, SetByCaller damage, grants State.Health.Bleeding.Heavy"));
}
