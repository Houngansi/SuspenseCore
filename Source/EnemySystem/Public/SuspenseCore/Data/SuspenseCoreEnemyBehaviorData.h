// SuspenseCoreEnemyBehaviorData.h
// SuspenseCore - Enemy System
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreEnemyBehaviorData.generated.h"

class USuspenseCoreEnemyStateBase;
class UGameplayAbility;
class UGameplayEffect;
struct FSuspenseCoreEnemyPresetRow;

// ═══════════════════════════════════════════════════════════════════════════════════
// FSM STATE CONFIG
// ═══════════════════════════════════════════════════════════════════════════════════

USTRUCT(BlueprintType)
struct ENEMYSYSTEM_API FSuspenseCoreEnemyStateConfig
{
	GENERATED_BODY()

	/** GameplayTag identifying this state */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag StateTag;

	/** C++ or Blueprint class implementing this state */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<USuspenseCoreEnemyStateBase> StateClass;
};

// ═══════════════════════════════════════════════════════════════════════════════════
// FSM TRANSITION CONFIG
// ═══════════════════════════════════════════════════════════════════════════════════

USTRUCT(BlueprintType)
struct ENEMYSYSTEM_API FSuspenseCoreEnemyTransitionConfig
{
	GENERATED_BODY()

	/** State to transition FROM (empty = any state) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag FromState;

	/** Event that triggers this transition */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag OnEvent;

	/** State to transition TO */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag ToState;
};

// ═══════════════════════════════════════════════════════════════════════════════════
// ENEMY BEHAVIOR DATA ASSET
// ═══════════════════════════════════════════════════════════════════════════════════

/**
 * USuspenseCoreEnemyBehaviorData
 *
 * DataAsset defining enemy behavior configuration.
 * Combines FSM (Finite State Machine) setup with attribute configuration.
 *
 * Supports two modes:
 * 1. SSOT Mode: Set PresetRowName to load attributes from DT_EnemyPresets
 * 2. Inline Mode: Configure attributes directly in this asset
 *
 * If PresetRowName is set, it takes priority over inline values.
 */
UCLASS(BlueprintType)
class ENEMYSYSTEM_API USuspenseCoreEnemyBehaviorData : public UDataAsset
{
	GENERATED_BODY()

public:
	// ═══════════════════════════════════════════════════════════════════════════
	// IDENTITY
	// ═══════════════════════════════════════════════════════════════════════════

	/** Unique identifier for this behavior configuration */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FName BehaviorID;

	/** Display name for UI/debugging */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FText DisplayName;

	/** Enemy type classification tag */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FGameplayTag EnemyTypeTag;

	// ═══════════════════════════════════════════════════════════════════════════
	// SSOT INTEGRATION
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Row name in DT_EnemyPresets DataTable.
	 * If set, attributes are loaded from the DataTable (SSOT mode).
	 * If empty, inline values below are used.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SSOT",
		meta = (DisplayName = "Preset Row Name (SSOT)"))
	FName PresetRowName;

	/**
	 * Reference to the enemy presets DataTable.
	 * Optional - if not set, uses the one from Project Settings.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SSOT")
	TSoftObjectPtr<UDataTable> EnemyPresetsDataTable;

	// ═══════════════════════════════════════════════════════════════════════════
	// FSM CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Initial state when enemy spawns */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FSM")
	FGameplayTag InitialState;

	/** Available states for this enemy */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FSM")
	TArray<FSuspenseCoreEnemyStateConfig> States;

	/** State transition rules */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FSM")
	TArray<FSuspenseCoreEnemyTransitionConfig> Transitions;

	// ═══════════════════════════════════════════════════════════════════════════
	// INLINE ATTRIBUTES (used when PresetRowName is empty)
	// ═══════════════════════════════════════════════════════════════════════════

	/** Maximum health (inline mode) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Inline",
		meta = (EditCondition = "PresetRowName.IsEmpty()", ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	/** Attack power (inline mode) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Inline",
		meta = (EditCondition = "PresetRowName.IsEmpty()", ClampMin = "0.0"))
	float AttackPower = 15.0f;

	/** Armor/defense (inline mode) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Inline",
		meta = (EditCondition = "PresetRowName.IsEmpty()", ClampMin = "0.0"))
	float Armor = 0.0f;

	/** Walking speed cm/s (inline mode) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Inline",
		meta = (EditCondition = "PresetRowName.IsEmpty()", ClampMin = "0.0"))
	float WalkSpeed = 200.0f;

	/** Running/chase speed cm/s (inline mode) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Inline",
		meta = (EditCondition = "PresetRowName.IsEmpty()", ClampMin = "0.0"))
	float RunSpeed = 400.0f;

	/** Sight detection range cm (inline mode) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perception|Inline",
		meta = (EditCondition = "PresetRowName.IsEmpty()", ClampMin = "0.0"))
	float SightRange = 1500.0f;

	/** Hearing detection range cm (inline mode) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perception|Inline",
		meta = (EditCondition = "PresetRowName.IsEmpty()", ClampMin = "0.0"))
	float HearingRange = 1000.0f;

	// ═══════════════════════════════════════════════════════════════════════════
	// GAS CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Abilities to grant on spawn.
	 * Note: If using SSOT mode, these are MERGED with preset abilities.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	/**
	 * Effects to apply on spawn (for attribute initialization).
	 * Note: If using SSOT mode, these are MERGED with preset effects.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS")
	TArray<TSubclassOf<UGameplayEffect>> StartupEffects;

	// ═══════════════════════════════════════════════════════════════════════════
	// HELPERS
	// ═══════════════════════════════════════════════════════════════════════════

	USuspenseCoreEnemyBehaviorData();

	/** Check if this behavior uses SSOT mode (DataTable reference) */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Behavior")
	bool UsesSSOTMode() const { return !PresetRowName.IsNone(); }

	/**
	 * Get the preset row from the DataTable.
	 * @param OutPreset - Output preset data
	 * @return True if preset was found
	 */
	bool GetPresetRow(FSuspenseCoreEnemyPresetRow& OutPreset) const;

	/**
	 * Get effective max health (from preset or inline).
	 * @param Level - Enemy level for scaling
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Behavior")
	float GetEffectiveMaxHealth(int32 Level = 1) const;

	/**
	 * Get effective armor (from preset or inline).
	 * @param Level - Enemy level for scaling
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Behavior")
	float GetEffectiveArmor(int32 Level = 1) const;

	/**
	 * Get effective attack power (from preset or inline).
	 * @param Level - Enemy level for scaling
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Behavior")
	float GetEffectiveAttackPower(int32 Level = 1) const;

	/**
	 * Get effective walk speed (from preset or inline).
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Behavior")
	float GetEffectiveWalkSpeed() const;

	/**
	 * Get effective run speed (from preset or inline).
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Behavior")
	float GetEffectiveRunSpeed() const;

	/**
	 * Get effective sight range (from preset or inline).
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Behavior")
	float GetEffectiveSightRange() const;

	/**
	 * Get effective hearing range (from preset or inline).
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Behavior")
	float GetEffectiveHearingRange() const;

	/**
	 * Get all startup abilities (merged from preset and inline).
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Behavior")
	TArray<TSubclassOf<UGameplayAbility>> GetAllStartupAbilities() const;

	/**
	 * Get all startup effects (merged from preset and inline).
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Behavior")
	TArray<TSubclassOf<UGameplayEffect>> GetAllStartupEffects() const;
};
