// SuspenseCoreEnemyEffect_InitialAttributes.cpp
// SuspenseCore - Enemy System
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Effects/SuspenseCoreEnemyEffect_InitialAttributes.h"
#include "SuspenseCore/Attributes/SuspenseCoreEnemyAttributeSet.h"
#include "SuspenseCore/Types/SuspenseCoreEnemyTypes.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectTypes.h"

USuspenseCoreEnemyEffect_InitialAttributes::USuspenseCoreEnemyEffect_InitialAttributes()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Health - uses SetByCaller
	{
		FGameplayModifierInfo ModifierInfo;
		ModifierInfo.Attribute = USuspenseCoreEnemyAttributeSet::GetHealthAttribute();
		ModifierInfo.ModifierOp = EGameplayModOp::Override;

		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = FGameplayTag::RequestGameplayTag(FName("Enemy.Attribute.Health"));
		ModifierInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

		Modifiers.Add(ModifierInfo);
	}

	// MaxHealth - uses SetByCaller
	{
		FGameplayModifierInfo ModifierInfo;
		ModifierInfo.Attribute = USuspenseCoreEnemyAttributeSet::GetMaxHealthAttribute();
		ModifierInfo.ModifierOp = EGameplayModOp::Override;

		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = FGameplayTag::RequestGameplayTag(FName("Enemy.Attribute.MaxHealth"));
		ModifierInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

		Modifiers.Add(ModifierInfo);
	}

	// Armor - uses SetByCaller
	{
		FGameplayModifierInfo ModifierInfo;
		ModifierInfo.Attribute = USuspenseCoreEnemyAttributeSet::GetArmorAttribute();
		ModifierInfo.ModifierOp = EGameplayModOp::Override;

		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = FGameplayTag::RequestGameplayTag(FName("Enemy.Attribute.Armor"));
		ModifierInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

		Modifiers.Add(ModifierInfo);
	}

	// AttackPower - uses SetByCaller
	{
		FGameplayModifierInfo ModifierInfo;
		ModifierInfo.Attribute = USuspenseCoreEnemyAttributeSet::GetAttackPowerAttribute();
		ModifierInfo.ModifierOp = EGameplayModOp::Override;

		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = FGameplayTag::RequestGameplayTag(FName("Enemy.Attribute.AttackPower"));
		ModifierInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

		Modifiers.Add(ModifierInfo);
	}

	// MovementSpeed - uses SetByCaller
	{
		FGameplayModifierInfo ModifierInfo;
		ModifierInfo.Attribute = USuspenseCoreEnemyAttributeSet::GetMovementSpeedAttribute();
		ModifierInfo.ModifierOp = EGameplayModOp::Override;

		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = FGameplayTag::RequestGameplayTag(FName("Enemy.Attribute.MovementSpeed"));
		ModifierInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

		Modifiers.Add(ModifierInfo);
	}
}

bool USuspenseCoreEnemyEffect_InitialAttributes::ApplyWithPreset(
	UAbilitySystemComponent* ASC,
	const FSuspenseCoreEnemyPresetRow& Preset,
	int32 Level)
{
	if (!ASC)
	{
		return false;
	}

	// Calculate scaled values
	const float ScaledMaxHealth = Preset.GetScaledHealth(Level);
	const float ScaledArmor = Preset.GetScaledArmor(Level);
	const float ScaledAttackPower = Preset.GetScaledAttackPower(Level);
	const float MovementSpeed = Preset.MovementAttributes.RunSpeed;

	return ApplyWithValues(ASC, ScaledMaxHealth, ScaledArmor, ScaledAttackPower, MovementSpeed);
}

bool USuspenseCoreEnemyEffect_InitialAttributes::ApplyWithValues(
	UAbilitySystemComponent* ASC,
	float MaxHealth,
	float Armor,
	float AttackPower,
	float MovementSpeed)
{
	if (!ASC)
	{
		return false;
	}

	// Get CDO of this effect class
	const USuspenseCoreEnemyEffect_InitialAttributes* EffectCDO = GetDefault<USuspenseCoreEnemyEffect_InitialAttributes>();
	if (!EffectCDO)
	{
		return false;
	}

	// Create effect context
	FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
	ContextHandle.AddSourceObject(ASC->GetOwnerActor());

	// Create effect spec
	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(
		USuspenseCoreEnemyEffect_InitialAttributes::StaticClass(),
		1.0f,
		ContextHandle
	);

	if (!SpecHandle.IsValid())
	{
		return false;
	}

	// Set SetByCaller magnitudes
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();

	Spec->SetSetByCallerMagnitude(
		FGameplayTag::RequestGameplayTag(FName("Enemy.Attribute.Health")),
		MaxHealth
	);

	Spec->SetSetByCallerMagnitude(
		FGameplayTag::RequestGameplayTag(FName("Enemy.Attribute.MaxHealth")),
		MaxHealth
	);

	Spec->SetSetByCallerMagnitude(
		FGameplayTag::RequestGameplayTag(FName("Enemy.Attribute.Armor")),
		Armor
	);

	Spec->SetSetByCallerMagnitude(
		FGameplayTag::RequestGameplayTag(FName("Enemy.Attribute.AttackPower")),
		AttackPower
	);

	Spec->SetSetByCallerMagnitude(
		FGameplayTag::RequestGameplayTag(FName("Enemy.Attribute.MovementSpeed")),
		MovementSpeed
	);

	// Apply the effect
	FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectSpecToSelf(*Spec);

	return Handle.IsValid();
}
