// SuspenseCoreDamageEffect.cpp
// SuspenseCore - Weapon Damage GameplayEffect Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Effects/Weapon/SuspenseCoreDamageEffect.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "SuspenseCore/Utils/SuspenseCoreTraceUtils.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffectTypes.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"

//========================================================================
// USuspenseCoreDamageEffect
//========================================================================

USuspenseCoreDamageEffect::USuspenseCoreDamageEffect()
{
	// Instant duration - damage applied immediately
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

	// Asset tags for GameplayCue identification
	// NOTE: Configure in Blueprint by adding Effect.Damage tag to Asset Tags
	// The deprecated InheritableGameplayEffectTags API is used for C++ initialization
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Damage")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

//========================================================================
// USuspenseCoreDamageEffect_WithHitInfo
//========================================================================

USuspenseCoreDamageEffect_WithHitInfo::USuspenseCoreDamageEffect_WithHitInfo()
{
	// Same as base damage effect
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo DamageModifier;
	DamageModifier.Attribute = USuspenseCoreAttributeSet::GetHealthAttribute();
	DamageModifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = SuspenseCoreTags::Data::Damage;

	// Construct magnitude with SetByCaller
	DamageModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

	Modifiers.Add(DamageModifier);

	// Different tag to indicate hit info is available
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Damage.WithHitInfo")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

//========================================================================
// USuspenseCoreDamageEffectLibrary
//========================================================================

bool USuspenseCoreDamageEffectLibrary::ApplyDamageToTarget(
	AActor* Instigator,
	AActor* Target,
	float DamageAmount,
	const FHitResult& HitResult)
{
	if (!Instigator || !Target || DamageAmount <= 0.0f)
	{
		return false;
	}

	// Get target's ASC
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!TargetASC)
	{
		return false;
	}

	// Get instigator's ASC (for context)
	UAbilitySystemComponent* InstigatorASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Instigator);

	// Create effect context
	FGameplayEffectContextHandle Context;
	if (InstigatorASC)
	{
		Context = InstigatorASC->MakeEffectContext();
		Context.AddInstigator(Instigator, Instigator);
		Context.AddHitResult(HitResult);
	}
	else
	{
		Context = TargetASC->MakeEffectContext();
		Context.AddInstigator(Instigator, Instigator);
		Context.AddHitResult(HitResult);
	}

	// Create effect spec
	UAbilitySystemComponent* SourceASC = InstigatorASC ? InstigatorASC : TargetASC;
	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
		USuspenseCoreDamageEffect::StaticClass(),
		1, // Level
		Context
	);

	if (!SpecHandle.IsValid())
	{
		return false;
	}

	// Set damage magnitude (negative for damage)
	SpecHandle.Data->SetSetByCallerMagnitude(SuspenseCoreTags::Data::Damage, -DamageAmount);

	// Apply to target
	FActiveGameplayEffectHandle ActiveHandle = SourceASC->ApplyGameplayEffectSpecToTarget(
		*SpecHandle.Data.Get(),
		TargetASC
	);

	return ActiveHandle.IsValid();
}

bool USuspenseCoreDamageEffectLibrary::ApplyDamageWithHeadshotCheck(
	AActor* Instigator,
	AActor* Target,
	float BaseDamage,
	const FHitResult& HitResult,
	float HeadshotMultiplier)
{
	if (!Instigator || !Target || BaseDamage <= 0.0f)
	{
		return false;
	}

	// Check for headshot
	float FinalDamage = BaseDamage;
	if (USuspenseCoreTraceUtils::IsHeadshot(HitResult.BoneName))
	{
		FinalDamage *= HeadshotMultiplier;
	}
	else
	{
		// Apply hit zone multiplier for other body parts
		FinalDamage *= USuspenseCoreTraceUtils::GetHitZoneDamageMultiplier(HitResult.BoneName);
	}

	return ApplyDamageToTarget(Instigator, Target, FinalDamage, HitResult);
}

FGameplayEffectSpecHandle USuspenseCoreDamageEffectLibrary::CreateDamageSpec(
	AActor* Instigator,
	float DamageAmount,
	const FHitResult& HitResult)
{
	FGameplayEffectSpecHandle InvalidHandle;

	if (!Instigator || DamageAmount <= 0.0f)
	{
		return InvalidHandle;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Instigator);
	if (!ASC)
	{
		return InvalidHandle;
	}

	// Create context
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddInstigator(Instigator, Instigator);
	Context.AddHitResult(HitResult);

	// Create spec
	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(
		USuspenseCoreDamageEffect::StaticClass(),
		1,
		Context
	);

	if (SpecHandle.IsValid())
	{
		// Set damage (negative for health reduction)
		SpecHandle.Data->SetSetByCallerMagnitude(SuspenseCoreTags::Data::Damage, -DamageAmount);
	}

	return SpecHandle;
}
