// GE_IncendiaryEffect.cpp
// GameplayEffect for incendiary/thermite grenade burn damage
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Effects/Grenade/GE_IncendiaryEffect.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"

//========================================================================
// UGE_IncendiaryEffect
//========================================================================

UGE_IncendiaryEffect::UGE_IncendiaryEffect()
{
	// Duration set by ability via SetByCaller
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// Use SetByCaller for burn duration
	// Tag: Data.Grenade.BurnDuration
	FSetByCallerFloat SetByCallerDuration;
	SetByCallerDuration.DataTag = FGameplayTag::RequestGameplayTag(FName("Data.Grenade.BurnDuration"));
	DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCallerDuration);

	// Periodic damage - tick every 0.5 seconds
	Period = 0.5f;
	bExecutePeriodicEffectOnApplication = true;  // Apply damage immediately on first application

	// Create periodic damage modifier
	// Use IncomingDamage meta-attribute (processed by PostGameplayEffectExecute)
	FGameplayModifierInfo BurnDamageModifier;
	BurnDamageModifier.Attribute = USuspenseCoreAttributeSet::GetIncomingDamageAttribute();
	BurnDamageModifier.ModifierOp = EGameplayModOp::Additive;

	// Use SetByCaller for damage per tick
	// Tag: Data.Damage.Burn (POSITIVE value)
	FSetByCallerFloat SetByCallerDamage;
	SetByCallerDamage.DataTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage.Burn"));

	BurnDamageModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCallerDamage);

	Modifiers.Add(BurnDamageModifier);

	// Stacking: multiple sources can stack (up to 3)
	// Duration refreshes when same source reapplies
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateBySource;
	StackLimitCount = 3;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackExpirationPolicy = EGameplayEffectStackingExpirationPolicy::RemoveSingleStackAndRefreshDuration;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Add State.Burning tag
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Burning")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Asset tags for GameplayCue support (fire VFX, burning sound)
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Damage")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Damage.Burn")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Grenade.Incendiary")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("GameplayCue.Grenade.Burn")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_LOG(LogTemp, Log, TEXT("GE_IncendiaryEffect: Configured with periodic burn damage (0.5s tick)"));
}

//========================================================================
// UGE_IncendiaryEffect_Zone
//========================================================================

UGE_IncendiaryEffect_Zone::UGE_IncendiaryEffect_Zone()
{
	// Short duration - reapplied while in zone
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// Fixed short duration (will be reapplied while in zone)
	FScalableFloat FixedDuration;
	FixedDuration.Value = 1.0f;  // 1 second
	DurationMagnitude = FGameplayEffectModifierMagnitude(FixedDuration);

	// Periodic damage - tick every 0.25 seconds (faster than direct hit)
	Period = 0.25f;
	bExecutePeriodicEffectOnApplication = true;

	// Create periodic damage modifier
	// Use IncomingDamage meta-attribute (processed by PostGameplayEffectExecute)
	FGameplayModifierInfo BurnDamageModifier;
	BurnDamageModifier.Attribute = USuspenseCoreAttributeSet::GetIncomingDamageAttribute();
	BurnDamageModifier.ModifierOp = EGameplayModOp::Additive;

	// Use SetByCaller for damage per tick (POSITIVE value)
	FSetByCallerFloat SetByCallerDamage;
	SetByCallerDamage.DataTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage.Burn"));

	BurnDamageModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCallerDamage);

	Modifiers.Add(BurnDamageModifier);

	// Stacking: only one zone effect at a time, refresh on reapply
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackExpirationPolicy = EGameplayEffectStackingExpirationPolicy::ClearEntireStack;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Add State.InFireZone tag (different from direct burn)
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.InFireZone")));
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Burning")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Asset tags
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Damage")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Damage.Burn")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Grenade.Incendiary.Zone")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_LOG(LogTemp, Log, TEXT("GE_IncendiaryEffect_Zone: Configured for fire zone (0.25s tick, 1s duration)"));
}

//========================================================================
// UGE_IncendiaryEffect_ArmorBypass
//========================================================================

UGE_IncendiaryEffect_ArmorBypass::UGE_IncendiaryEffect_ArmorBypass()
{
	// ═══════════════════════════════════════════════════════════════════
	// DURATION: SetByCaller from grenade data
	// ═══════════════════════════════════════════════════════════════════
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat SetByCallerDuration;
	SetByCallerDuration.DataTag = FGameplayTag::RequestGameplayTag(FName("Data.Grenade.BurnDuration"));
	DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCallerDuration);

	// ═══════════════════════════════════════════════════════════════════
	// PERIODIC DAMAGE: Every 0.5 seconds
	// ═══════════════════════════════════════════════════════════════════
	Period = 0.5f;
	bExecutePeriodicEffectOnApplication = true;

	// ═══════════════════════════════════════════════════════════════════
	// MODIFIER 1: ARMOR/SHIELD DAMAGE
	// Directly reduces Armor attribute (bypasses normal damage pipeline)
	// NOTE: Caller passes NEGATIVE value (-5.0f) to reduce armor
	// ═══════════════════════════════════════════════════════════════════
	FGameplayModifierInfo ArmorDamageModifier;
	ArmorDamageModifier.Attribute = USuspenseCoreAttributeSet::GetArmorAttribute();
	ArmorDamageModifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat SetByCallerArmorDamage;
	SetByCallerArmorDamage.DataTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage.Burn.Armor"));

	ArmorDamageModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCallerArmorDamage);
	Modifiers.Add(ArmorDamageModifier);

	// ═══════════════════════════════════════════════════════════════════
	// MODIFIER 2: HEALTH DAMAGE (DIRECT - BYPASSES ARMOR!)
	// Directly reduces Health attribute (NOT IncomingDamage)
	// This is the key difference - fire burns through everything
	// NOTE: Caller passes NEGATIVE value (-5.0f) to reduce health
	// ═══════════════════════════════════════════════════════════════════
	FGameplayModifierInfo HealthDamageModifier;
	HealthDamageModifier.Attribute = USuspenseCoreAttributeSet::GetHealthAttribute();
	HealthDamageModifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat SetByCallerHealthDamage;
	SetByCallerHealthDamage.DataTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage.Burn.Health"));

	HealthDamageModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCallerHealthDamage);
	Modifiers.Add(HealthDamageModifier);

	// ═══════════════════════════════════════════════════════════════════
	// STACKING: Only one burn effect per source
	// ═══════════════════════════════════════════════════════════════════
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateBySource;
	StackLimitCount = 3;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackExpirationPolicy = EGameplayEffectStackingExpirationPolicy::RemoveSingleStackAndRefreshDuration;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// ═══════════════════════════════════════════════════════════════════
	// GRANTED TAGS: State.Burning
	// ═══════════════════════════════════════════════════════════════════
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Burning")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// ═══════════════════════════════════════════════════════════════════
	// ASSET TAGS: For GameplayCue and identification
	// ═══════════════════════════════════════════════════════════════════
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Damage")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Damage.Burn")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Grenade.Incendiary")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Grenade.Incendiary.ArmorBypass")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.DoT")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("GameplayCue.Grenade.Burn")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_LOG(LogTemp, Log, TEXT("GE_IncendiaryEffect_ArmorBypass: Configured with DUAL damage (Armor + Health bypass)"));
}
