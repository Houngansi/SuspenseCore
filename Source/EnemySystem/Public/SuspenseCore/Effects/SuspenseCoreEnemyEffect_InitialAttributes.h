// SuspenseCoreEnemyEffect_InitialAttributes.h
// SuspenseCore - Enemy System
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "SuspenseCoreEnemyEffect_InitialAttributes.generated.h"

struct FSuspenseCoreEnemyPresetRow;

/**
 * USuspenseCoreEnemyEffect_InitialAttributes
 *
 * Enemy attribute initialization effect using SetByCaller magnitudes.
 * Values are set at runtime from FSuspenseCoreEnemyPresetRow data.
 *
 * SetByCaller Tags:
 * - Enemy.Attribute.MaxHealth
 * - Enemy.Attribute.Health
 * - Enemy.Attribute.Armor
 * - Enemy.Attribute.AttackPower
 * - Enemy.Attribute.MovementSpeed
 *
 * Usage:
 * 1. Create effect spec from this class
 * 2. Set magnitudes using SetSetByCallerMagnitude() with preset values
 * 3. Apply to enemy's ASC
 */
UCLASS()
class ENEMYSYSTEM_API USuspenseCoreEnemyEffect_InitialAttributes : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USuspenseCoreEnemyEffect_InitialAttributes();

	/**
	 * Apply this effect with values from a preset row.
	 * @param ASC - The AbilitySystemComponent to apply to
	 * @param Preset - The enemy preset data
	 * @param Level - Enemy level for scaling
	 * @return True if successfully applied
	 */
	static bool ApplyWithPreset(
		class UAbilitySystemComponent* ASC,
		const FSuspenseCoreEnemyPresetRow& Preset,
		int32 Level = 1);

	/**
	 * Apply this effect with explicit values.
	 * @param ASC - The AbilitySystemComponent to apply to
	 * @param MaxHealth - Maximum health value
	 * @param Armor - Armor value
	 * @param AttackPower - Attack power value
	 * @param MovementSpeed - Movement speed value
	 * @return True if successfully applied
	 */
	static bool ApplyWithValues(
		class UAbilitySystemComponent* ASC,
		float MaxHealth,
		float Armor,
		float AttackPower,
		float MovementSpeed);
};
