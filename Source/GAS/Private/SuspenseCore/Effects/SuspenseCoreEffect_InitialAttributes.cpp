// SuspenseCoreEffect_InitialAttributes.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Effects/SuspenseCoreEffect_InitialAttributes.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"

USuspenseCoreEffect_InitialAttributes::USuspenseCoreEffect_InitialAttributes()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// CRITICAL: This is the ONLY place where initial attribute values should be set!
	// Do not set initial values in AttributeSet constructor

	// Health
	{
		FGameplayModifierInfo ModifierInfo;
		ModifierInfo.Attribute = USuspenseCoreAttributeSet::GetHealthAttribute();
		ModifierInfo.ModifierOp = EGameplayModOp::Override;
		ModifierInfo.ModifierMagnitude = FScalableFloat(100.0f);
		Modifiers.Add(ModifierInfo);
	}

	// MaxHealth
	{
		FGameplayModifierInfo ModifierInfo;
		ModifierInfo.Attribute = USuspenseCoreAttributeSet::GetMaxHealthAttribute();
		ModifierInfo.ModifierOp = EGameplayModOp::Override;
		ModifierInfo.ModifierMagnitude = FScalableFloat(100.0f);
		Modifiers.Add(ModifierInfo);
	}

	// HealthRegen
	{
		FGameplayModifierInfo ModifierInfo;
		ModifierInfo.Attribute = USuspenseCoreAttributeSet::GetHealthRegenAttribute();
		ModifierInfo.ModifierOp = EGameplayModOp::Override;
		ModifierInfo.ModifierMagnitude = FScalableFloat(1.0f);
		Modifiers.Add(ModifierInfo);
	}

	// Armor
	{
		FGameplayModifierInfo ModifierInfo;
		ModifierInfo.Attribute = USuspenseCoreAttributeSet::GetArmorAttribute();
		ModifierInfo.ModifierOp = EGameplayModOp::Override;
		ModifierInfo.ModifierMagnitude = FScalableFloat(0.0f);
		Modifiers.Add(ModifierInfo);
	}

	// AttackPower
	{
		FGameplayModifierInfo ModifierInfo;
		ModifierInfo.Attribute = USuspenseCoreAttributeSet::GetAttackPowerAttribute();
		ModifierInfo.ModifierOp = EGameplayModOp::Override;
		ModifierInfo.ModifierMagnitude = FScalableFloat(10.0f);
		Modifiers.Add(ModifierInfo);
	}

	// MovementSpeed
	{
		FGameplayModifierInfo ModifierInfo;
		ModifierInfo.Attribute = USuspenseCoreAttributeSet::GetMovementSpeedAttribute();
		ModifierInfo.ModifierOp = EGameplayModOp::Override;
		ModifierInfo.ModifierMagnitude = FScalableFloat(300.0f);
		Modifiers.Add(ModifierInfo);
	}

	// Stamina
	{
		FGameplayModifierInfo ModifierInfo;
		ModifierInfo.Attribute = USuspenseCoreAttributeSet::GetStaminaAttribute();
		ModifierInfo.ModifierOp = EGameplayModOp::Override;
		ModifierInfo.ModifierMagnitude = FScalableFloat(100.0f);
		Modifiers.Add(ModifierInfo);
	}

	// MaxStamina
	{
		FGameplayModifierInfo ModifierInfo;
		ModifierInfo.Attribute = USuspenseCoreAttributeSet::GetMaxStaminaAttribute();
		ModifierInfo.ModifierOp = EGameplayModOp::Override;
		ModifierInfo.ModifierMagnitude = FScalableFloat(100.0f);
		Modifiers.Add(ModifierInfo);
	}

	// StaminaRegen
	{
		FGameplayModifierInfo ModifierInfo;
		ModifierInfo.Attribute = USuspenseCoreAttributeSet::GetStaminaRegenAttribute();
		ModifierInfo.ModifierOp = EGameplayModOp::Override;
		ModifierInfo.ModifierMagnitude = FScalableFloat(5.0f);
		Modifiers.Add(ModifierInfo);
	}

	UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreEffect_InitialAttributes: Created with default attribute values"));
}
