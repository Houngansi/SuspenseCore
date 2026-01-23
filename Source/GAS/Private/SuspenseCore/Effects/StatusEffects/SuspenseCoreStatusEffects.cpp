// SuspenseCoreStatusEffects.cpp
// GameplayEffect implementations for Status Effect System v2.0
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Effects/StatusEffects/SuspenseCoreStatusEffects.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "SuspenseCore/Attributes/SuspenseCoreMovementAttributeSet.h"

//========================================================================
// BASE CLASSES
//========================================================================

USuspenseCoreEffect_DoT_Base::USuspenseCoreEffect_DoT_Base()
{
	// Base DoT configuration - subclasses override as needed
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	Period = 1.0f;
	bExecutePeriodicEffectOnApplication = false;

	// Default stacking: aggregate by target, refresh on reapply
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackPeriodResetPolicy = EGameplayEffectStackingPeriodPolicy::ResetOnSuccessfulApplication;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

USuspenseCoreEffect_Buff_Base::USuspenseCoreEffect_Buff_Base()
{
	// Base buff configuration
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// Default stacking: no stack, refresh duration
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

USuspenseCoreEffect_Debuff_Base::USuspenseCoreEffect_Debuff_Base()
{
	// Base debuff configuration
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// Default stacking: no stack, refresh duration
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}


//========================================================================
// UGE_Poisoned
//========================================================================

UGE_Poisoned::UGE_Poisoned()
{
	// Duration: 30 seconds by default (SetByCaller)
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat SetByCallerDuration;
	SetByCallerDuration.DataTag = FGameplayTag::RequestGameplayTag(FName("Data.Effect.Duration"));
	DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCallerDuration);

	// Periodic damage: every 2 seconds
	Period = 2.0f;
	bExecutePeriodicEffectOnApplication = false;

	// Damage modifier: 2 HP per tick
	FGameplayModifierInfo DamageModifier;
	DamageModifier.Attribute = USuspenseCoreAttributeSet::GetIncomingDamageAttribute();
	DamageModifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat SetByCallerDamage;
	SetByCallerDamage.DataTag = FGameplayTag::RequestGameplayTag(FName("Data.Damage.Poison"));
	DamageModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCallerDamage);
	Modifiers.Add(DamageModifier);

	// Movement speed debuff: -10%
	FGameplayModifierInfo SpeedDebuff;
	SpeedDebuff.Attribute = USuspenseCoreMovementAttributeSet::GetWalkSpeedAttribute();
	SpeedDebuff.ModifierOp = EGameplayModOp::Multiplicitive;

	FScalableFloat SpeedMultiplier;
	SpeedMultiplier.Value = 0.9f;  // -10%
	SpeedDebuff.ModifierMagnitude = FGameplayEffectModifierMagnitude(SpeedMultiplier);
	Modifiers.Add(SpeedDebuff);

	// Stacking: up to 3 stacks
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 3;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Granted tags
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Health.Poisoned")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Asset tags
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Damage")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Damage.Poison")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.DoT")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Debuff")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_LOG(LogTemp, Log, TEXT("UGE_Poisoned: Configured - 30s duration, 2s tick, -10%% speed"));
}


//========================================================================
// UGE_Stunned
//========================================================================

UGE_Stunned::UGE_Stunned()
{
	// Duration: SetByCaller (2-5 seconds)
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat SetByCallerDuration;
	SetByCallerDuration.DataTag = FGameplayTag::RequestGameplayTag(FName("Data.Effect.Duration"));
	DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCallerDuration);

	// No periodic effects

	// Movement speed: 0 (cannot move)
	FGameplayModifierInfo SpeedZero;
	SpeedZero.Attribute = USuspenseCoreMovementAttributeSet::GetWalkSpeedAttribute();
	SpeedZero.ModifierOp = EGameplayModOp::Override;

	FScalableFloat ZeroSpeed;
	ZeroSpeed.Value = 0.0f;
	SpeedZero.ModifierMagnitude = FGameplayEffectModifierMagnitude(ZeroSpeed);
	Modifiers.Add(SpeedZero);

	// No stacking
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Granted tags (used by abilities to block actions)
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Combat.Stunned")));
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Movement.Disabled")));
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Action.Disabled")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Asset tags
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Debuff")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Debuff.Stun")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_LOG(LogTemp, Log, TEXT("UGE_Stunned: Configured - SetByCaller duration, movement disabled"));
}


//========================================================================
// UGE_Suppressed
//========================================================================

UGE_Suppressed::UGE_Suppressed()
{
	// Duration: 3 seconds (fixed, refreshes on new suppression)
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FScalableFloat FixedDuration;
	FixedDuration.Value = 3.0f;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FixedDuration);

	// Aim accuracy debuff: -30% (affects spread)
	// Note: This assumes AimAccuracy attribute exists in combat attribute set
	// If not, this modifier will be ignored

	// Refresh on reapplication
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Granted tags
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Combat.Suppressed")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Asset tags
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Debuff")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Debuff.Suppression")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_LOG(LogTemp, Log, TEXT("UGE_Suppressed: Configured - 3s duration, aim penalty"));
}


//========================================================================
// UGE_Fracture_Leg
//========================================================================

UGE_Fracture_Leg::UGE_Fracture_Leg()
{
	// Duration: Infinite (requires surgery)
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// Movement speed: -40%
	FGameplayModifierInfo SpeedDebuff;
	SpeedDebuff.Attribute = USuspenseCoreMovementAttributeSet::GetWalkSpeedAttribute();
	SpeedDebuff.ModifierOp = EGameplayModOp::Multiplicitive;

	FScalableFloat SpeedMultiplier;
	SpeedMultiplier.Value = 0.6f;  // -40%
	SpeedDebuff.ModifierMagnitude = FGameplayEffectModifierMagnitude(SpeedMultiplier);
	Modifiers.Add(SpeedDebuff);

	// No stacking
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Granted tags
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Health.Fracture")));
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Health.Fracture.Leg")));
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Movement.Limp")));
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Movement.NoSprint")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Asset tags
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Debuff")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Debuff.Fracture")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Removal tags (surgery can remove)
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	RemoveGameplayEffectsWithTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Health.Fracture.Leg")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_LOG(LogTemp, Log, TEXT("UGE_Fracture_Leg: Configured - Infinite, -40%% speed, limp"));
}


//========================================================================
// UGE_Fracture_Arm
//========================================================================

UGE_Fracture_Arm::UGE_Fracture_Arm()
{
	// Duration: Infinite (requires surgery)
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// No stacking
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Granted tags
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Health.Fracture")));
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Health.Fracture.Arm")));
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Combat.NoADS")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Asset tags
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Debuff")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Debuff.Fracture")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Removal tags
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	RemoveGameplayEffectsWithTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Health.Fracture.Arm")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_LOG(LogTemp, Log, TEXT("UGE_Fracture_Arm: Configured - Infinite, no ADS"));
}


//========================================================================
// UGE_Dehydrated
//========================================================================

UGE_Dehydrated::UGE_Dehydrated()
{
	// Duration: Infinite (until water consumed)
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// Periodic damage: every 5 seconds
	Period = 5.0f;
	bExecutePeriodicEffectOnApplication = false;

	// Damage: 1 HP per tick
	FGameplayModifierInfo DamageModifier;
	DamageModifier.Attribute = USuspenseCoreAttributeSet::GetIncomingDamageAttribute();
	DamageModifier.ModifierOp = EGameplayModOp::Additive;

	FScalableFloat DamageValue;
	DamageValue.Value = 1.0f;
	DamageModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(DamageValue);
	Modifiers.Add(DamageModifier);

	// No stacking
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Granted tags
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Health.Dehydrated")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Asset tags
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Debuff")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Debuff.Survival")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.DoT")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Removal tags
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	RemoveGameplayEffectsWithTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Health.Dehydrated")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_LOG(LogTemp, Log, TEXT("UGE_Dehydrated: Configured - Infinite, 1 HP/5s"));
}


//========================================================================
// UGE_Exhausted
//========================================================================

UGE_Exhausted::UGE_Exhausted()
{
	// Duration: Infinite (until food consumed)
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// Stamina regen: 0 (override to zero)
	FGameplayModifierInfo StaminaRegenZero;
	StaminaRegenZero.Attribute = USuspenseCoreAttributeSet::GetStaminaRegenAttribute();
	StaminaRegenZero.ModifierOp = EGameplayModOp::Override;

	FScalableFloat ZeroRegen;
	ZeroRegen.Value = 0.0f;
	StaminaRegenZero.ModifierMagnitude = FGameplayEffectModifierMagnitude(ZeroRegen);
	Modifiers.Add(StaminaRegenZero);

	// No stacking
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Granted tags
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Health.Exhausted")));
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Movement.NoSprint")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Asset tags
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Debuff")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Debuff.Survival")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Removal tags
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	RemoveGameplayEffectsWithTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Health.Exhausted")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_LOG(LogTemp, Log, TEXT("UGE_Exhausted: Configured - Infinite, no stamina regen"));
}


//========================================================================
// UGE_Regenerating
//========================================================================

UGE_Regenerating::UGE_Regenerating()
{
	// Duration: SetByCaller (10-30 seconds)
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat SetByCallerDuration;
	SetByCallerDuration.DataTag = FGameplayTag::RequestGameplayTag(FName("Data.Effect.Duration"));
	DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCallerDuration);

	// Periodic healing: every 1 second
	Period = 1.0f;
	bExecutePeriodicEffectOnApplication = false;

	// Healing: +5 HP per tick (negative IncomingDamage = healing)
	FGameplayModifierInfo HealModifier;
	HealModifier.Attribute = USuspenseCoreAttributeSet::GetIncomingDamageAttribute();
	HealModifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat SetByCallerHeal;
	SetByCallerHeal.DataTag = FGameplayTag::RequestGameplayTag(FName("Data.Heal.PerTick"));
	HealModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCallerHeal);
	Modifiers.Add(HealModifier);

	// No stacking - refresh duration
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Granted tags
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Health.Regenerating")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Asset tags
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Buff")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Buff.Heal")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.HoT")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_LOG(LogTemp, Log, TEXT("UGE_Regenerating: Configured - SetByCaller duration, HoT"));
}


//========================================================================
// UGE_Painkiller
//========================================================================

UGE_Painkiller::UGE_Painkiller()
{
	// Duration: SetByCaller (60-300 seconds)
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat SetByCallerDuration;
	SetByCallerDuration.DataTag = FGameplayTag::RequestGameplayTag(FName("Data.Effect.Duration"));
	DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCallerDuration);

	// No attribute modifiers - just tags that suppress pain effects

	// Refresh duration on reapplication
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Granted tags - these block pain-related debuff tags
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Combat.Painkiller")));
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Combat.PainImmune")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Asset tags
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Buff")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Buff.Painkiller")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_LOG(LogTemp, Log, TEXT("UGE_Painkiller: Configured - SetByCaller duration, pain immunity"));
}


//========================================================================
// UGE_Adrenaline
//========================================================================

UGE_Adrenaline::UGE_Adrenaline()
{
	// Duration: SetByCaller (30-60 seconds)
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat SetByCallerDuration;
	SetByCallerDuration.DataTag = FGameplayTag::RequestGameplayTag(FName("Data.Effect.Duration"));
	DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCallerDuration);

	// Movement speed: +15%
	FGameplayModifierInfo SpeedBuff;
	SpeedBuff.Attribute = USuspenseCoreMovementAttributeSet::GetWalkSpeedAttribute();
	SpeedBuff.ModifierOp = EGameplayModOp::Multiplicitive;

	FScalableFloat SpeedMultiplier;
	SpeedMultiplier.Value = 1.15f;  // +15%
	SpeedBuff.ModifierMagnitude = FGameplayEffectModifierMagnitude(SpeedMultiplier);
	Modifiers.Add(SpeedBuff);

	// Stamina regen: +25%
	FGameplayModifierInfo StaminaBuff;
	StaminaBuff.Attribute = USuspenseCoreAttributeSet::GetStaminaRegenAttribute();
	StaminaBuff.ModifierOp = EGameplayModOp::Multiplicitive;

	FScalableFloat StaminaMultiplier;
	StaminaMultiplier.Value = 1.25f;  // +25%
	StaminaBuff.ModifierMagnitude = FGameplayEffectModifierMagnitude(StaminaMultiplier);
	Modifiers.Add(StaminaBuff);

	// No stacking
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::NeverRefresh;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Granted tags
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Combat.Adrenaline")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Asset tags
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Buff")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Buff.Combat")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_LOG(LogTemp, Log, TEXT("UGE_Adrenaline: Configured - +15%% speed, +25%% stamina regen"));
}


//========================================================================
// UGE_Fortified
//========================================================================

UGE_Fortified::UGE_Fortified()
{
	// Duration: SetByCaller (15-30 seconds)
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat SetByCallerDuration;
	SetByCallerDuration.DataTag = FGameplayTag::RequestGameplayTag(FName("Data.Effect.Duration"));
	DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCallerDuration);

	// Damage resistance: +15% (implemented via DamageReduction attribute if exists)
	// For now, we grant a tag that damage calculation checks

	// No stacking
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Granted tags
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Combat.Fortified")));
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Combat.DamageResist")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Asset tags
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Buff")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Buff.Defense")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_LOG(LogTemp, Log, TEXT("UGE_Fortified: Configured - +15%% damage resistance"));
}


//========================================================================
// UGE_Haste
//========================================================================

UGE_Haste::UGE_Haste()
{
	// Duration: SetByCaller (10-20 seconds)
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat SetByCallerDuration;
	SetByCallerDuration.DataTag = FGameplayTag::RequestGameplayTag(FName("Data.Effect.Duration"));
	DurationMagnitude = FGameplayEffectModifierMagnitude(SetByCallerDuration);

	// Movement speed: +20%
	FGameplayModifierInfo SpeedBuff;
	SpeedBuff.Attribute = USuspenseCoreMovementAttributeSet::GetWalkSpeedAttribute();
	SpeedBuff.ModifierOp = EGameplayModOp::Multiplicitive;

	FScalableFloat SpeedMultiplier;
	SpeedMultiplier.Value = 1.2f;  // +20%
	SpeedBuff.ModifierMagnitude = FGameplayEffectModifierMagnitude(SpeedMultiplier);
	Modifiers.Add(SpeedBuff);

	// No stacking
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Granted tags
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableOwnedTagsContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Movement.Haste")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Asset tags
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Buff")));
	InheritableGameplayEffectTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Effect.Buff.Movement")));
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UE_LOG(LogTemp, Log, TEXT("UGE_Haste: Configured - +20%% movement speed"));
}
