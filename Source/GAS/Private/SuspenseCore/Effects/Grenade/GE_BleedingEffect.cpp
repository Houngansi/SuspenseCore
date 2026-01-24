// GE_BleedingEffect.cpp
// GameplayEffect for bleeding damage over time from grenade shrapnel
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Effects/Grenade/GE_BleedingEffect.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"

//========================================================================
// UGE_BleedingEffect_Light
//========================================================================

UGE_BleedingEffect_Light::UGE_BleedingEffect_Light()
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
	SetByCaller.DataTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage.Bleed"));

	DamageModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	Modifiers.Add(DamageModifier);

	// ═══════════════════════════════════════════════════════════════════
	// STACKING: Tarkov-style - multiple bleeds stack and multiply damage
	// 3 light bleeds = 3x damage per tick
	// ═══════════════════════════════════════════════════════════════════
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 5;  // Max 5 stacks of light bleeding
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackPeriodResetPolicy = EGameplayEffectStackingPeriodPolicy::ResetOnSuccessfulApplication;

	// ═══════════════════════════════════════════════════════════════════
	// GRANTED TAGS: State.Health.Bleeding.Light
	// UI and medical system check this tag to show bleed indicator
	// Medical items remove effects with this tag
	// ═══════════════════════════════════════════════════════════════════
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Health.Bleeding.Light")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// ═══════════════════════════════════════════════════════════════════
	// ASSET TAGS: For identification and GameplayCue
	// ═══════════════════════════════════════════════════════════════════
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Damage")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Damage.Bleed")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Damage.Bleed.Light")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.DoT")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Grenade.Shrapnel")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// ═══════════════════════════════════════════════════════════════════
	// REMOVAL TAGS: Medical items with these tags can remove this effect
	// ═══════════════════════════════════════════════════════════════════
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	RemoveGameplayEffectsWithTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Health.Bleeding.Light")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_LOG(LogTemp, Log, TEXT("GE_BleedingEffect_Light: Configured - Infinite duration, 1s period, SetByCaller damage"));
}

//========================================================================
// UGE_BleedingEffect_Heavy
//========================================================================

UGE_BleedingEffect_Heavy::UGE_BleedingEffect_Heavy()
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
	SetByCaller.DataTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage.Bleed"));

	DamageModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	Modifiers.Add(DamageModifier);

	// ═══════════════════════════════════════════════════════════════════
	// STACKING: Tarkov-style - multiple heavy bleeds stack and multiply damage
	// 3 heavy bleeds = 3x damage per tick (very dangerous!)
	// ═══════════════════════════════════════════════════════════════════
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 5;  // Max 5 stacks of heavy bleeding
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackPeriodResetPolicy = EGameplayEffectStackingPeriodPolicy::ResetOnSuccessfulApplication;

	// ═══════════════════════════════════════════════════════════════════
	// GRANTED TAGS: State.Health.Bleeding.Heavy
	// More severe indicator, different treatment required
	// ═══════════════════════════════════════════════════════════════════
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Health.Bleeding.Heavy")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// ═══════════════════════════════════════════════════════════════════
	// ASSET TAGS
	// ═══════════════════════════════════════════════════════════════════
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Damage")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Damage.Bleed")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Damage.Bleed.Heavy")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.DoT")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Grenade.Shrapnel")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// ═══════════════════════════════════════════════════════════════════
	// REMOVAL TAGS: Only surgery/medkit can remove heavy bleed
	// Bandages are NOT sufficient
	// ═══════════════════════════════════════════════════════════════════
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	RemoveGameplayEffectsWithTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Health.Bleeding.Heavy")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_LOG(LogTemp, Log, TEXT("GE_BleedingEffect_Heavy: Configured - Infinite duration, 1s period, SetByCaller damage"));
}
