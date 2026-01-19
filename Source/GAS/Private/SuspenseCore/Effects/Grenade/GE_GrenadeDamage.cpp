// GE_GrenadeDamage.cpp
// GameplayEffect for grenade explosion damage
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Effects/Grenade/GE_GrenadeDamage.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"

//========================================================================
// UGE_GrenadeDamage
//========================================================================

UGE_GrenadeDamage::UGE_GrenadeDamage()
{
	// Instant duration - damage applied immediately on explosion
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Create modifier for Health attribute
	FGameplayModifierInfo DamageModifier;
	DamageModifier.Attribute = USuspenseCoreAttributeSet::GetHealthAttribute();
	DamageModifier.ModifierOp = EGameplayModOp::Additive;

	// Use SetByCaller for dynamic damage values (based on distance from explosion)
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = SuspenseCoreTags::Data::Damage;

	// Construct magnitude with SetByCaller
	DamageModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

	Modifiers.Add(DamageModifier);

	// Asset tags for GameplayCue identification
	// Effect.Damage.Grenade triggers explosion damage cue
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Damage")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Damage.Grenade")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Damage.Explosion")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_LOG(LogTemp, Log, TEXT("GE_GrenadeDamage: Configured with SetByCaller damage"));
}

//========================================================================
// UGE_GrenadeDamage_Shrapnel
//========================================================================

UGE_GrenadeDamage_Shrapnel::UGE_GrenadeDamage_Shrapnel()
{
	// Instant duration - shrapnel damage applied immediately
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Create modifier for Health attribute
	FGameplayModifierInfo DamageModifier;
	DamageModifier.Attribute = USuspenseCoreAttributeSet::GetHealthAttribute();
	DamageModifier.ModifierOp = EGameplayModOp::Additive;

	// Use SetByCaller for dynamic damage values
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = SuspenseCoreTags::Data::Damage;

	// Construct magnitude with SetByCaller
	DamageModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

	Modifiers.Add(DamageModifier);

	// Asset tags - shrapnel is a subtype of grenade damage
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Damage")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Damage.Grenade")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Damage.Shrapnel")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_LOG(LogTemp, Log, TEXT("GE_GrenadeDamage_Shrapnel: Configured with SetByCaller damage"));
}
