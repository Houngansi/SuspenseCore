// SuspenseCoreEnemyTypes.h
// SuspenseCore - Enemy System SSOT Types
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "GameplayEffect.h"
#include "Abilities/GameplayAbility.h"
#include "SuspenseCoreEnemyTypes.generated.h"

class USuspenseCoreEnemyStateBase;
class USuspenseCoreEnemyBehaviorData;

// ═══════════════════════════════════════════════════════════════════════════════════
// ENEMY ARCHETYPE - High-level classification
// ═══════════════════════════════════════════════════════════════════════════════════

UENUM(BlueprintType)
enum class ESuspenseCoreEnemyArchetype : uint8
{
	None			UMETA(DisplayName = "None"),
	Grunt			UMETA(DisplayName = "Grunt (Basic foot soldier)"),
	Scout			UMETA(DisplayName = "Scout (Fast, low HP)"),
	Heavy			UMETA(DisplayName = "Heavy (Slow, high HP/Armor)"),
	Sniper			UMETA(DisplayName = "Sniper (Long range)"),
	Berserker		UMETA(DisplayName = "Berserker (Melee, aggressive)"),
	Support			UMETA(DisplayName = "Support (Healer/Buffer)"),
	Elite			UMETA(DisplayName = "Elite (Mini-boss)"),
	Boss			UMETA(DisplayName = "Boss (Major encounter)")
};

// ═══════════════════════════════════════════════════════════════════════════════════
// ENEMY COMBAT STYLE - AI Behavior preferences
// ═══════════════════════════════════════════════════════════════════════════════════

UENUM(BlueprintType)
enum class ESuspenseCoreEnemyCombatStyle : uint8
{
	Balanced		UMETA(DisplayName = "Balanced (Mix of offense/defense)"),
	Aggressive		UMETA(DisplayName = "Aggressive (Push forward)"),
	Defensive		UMETA(DisplayName = "Defensive (Hold position)"),
	Flanking		UMETA(DisplayName = "Flanking (Circle around)"),
	Ambush			UMETA(DisplayName = "Ambush (Wait and strike)"),
	Ranged			UMETA(DisplayName = "Ranged (Keep distance)"),
	Melee			UMETA(DisplayName = "Melee (Close quarters)")
};

// ═══════════════════════════════════════════════════════════════════════════════════
// ENEMY BASE ATTRIBUTES - Core stats before modifiers
// ═══════════════════════════════════════════════════════════════════════════════════

USTRUCT(BlueprintType)
struct ENEMYSYSTEM_API FSuspenseCoreEnemyBaseAttributes
{
	GENERATED_BODY()

	/** Maximum health points */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	/** Health regeneration per second (0 = no regen) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health", meta = (ClampMin = "0.0"))
	float HealthRegen = 0.0f;

	/** Armor/Defense value (flat damage reduction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float Armor = 0.0f;

	/** Attack power multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.1"))
	float AttackPower = 10.0f;

	/** Attack speed multiplier (1.0 = normal) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float AttackSpeed = 1.0f;

	/** Damage taken multiplier (1.0 = normal, 0.5 = 50% damage) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float DamageMultiplier = 1.0f;
};

// ═══════════════════════════════════════════════════════════════════════════════════
// ENEMY MOVEMENT ATTRIBUTES - Movement and navigation
// ═══════════════════════════════════════════════════════════════════════════════════

USTRUCT(BlueprintType)
struct ENEMYSYSTEM_API FSuspenseCoreEnemyMovementAttributes
{
	GENERATED_BODY()

	/** Walking speed (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed", meta = (ClampMin = "0.0"))
	float WalkSpeed = 300.0f;

	/** Running/chase speed (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed", meta = (ClampMin = "0.0"))
	float RunSpeed = 500.0f;

	/** Patrol speed (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed", meta = (ClampMin = "0.0"))
	float PatrolSpeed = 200.0f;

	/** Rotation rate (degrees/sec) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation", meta = (ClampMin = "0.0"))
	float RotationRate = 360.0f;

	/** Can this enemy jump? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation")
	bool bCanJump = false;

	/** Jump height (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation", meta = (ClampMin = "0.0", EditCondition = "bCanJump"))
	float JumpHeight = 400.0f;

	/** Can this enemy fly/hover? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation")
	bool bCanFly = false;
};

// ═══════════════════════════════════════════════════════════════════════════════════
// ENEMY PERCEPTION ATTRIBUTES - Detection and awareness
// ═══════════════════════════════════════════════════════════════════════════════════

USTRUCT(BlueprintType)
struct ENEMYSYSTEM_API FSuspenseCoreEnemyPerceptionAttributes
{
	GENERATED_BODY()

	/** Sight detection range (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sight", meta = (ClampMin = "0.0"))
	float SightRange = 1500.0f;

	/** Sight lose range (cm) - when target is lost */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sight", meta = (ClampMin = "0.0"))
	float SightLoseRange = 2000.0f;

	/** Peripheral vision angle (degrees, half-angle) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sight", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float PeripheralVisionAngle = 60.0f;

	/** Hearing detection range (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hearing", meta = (ClampMin = "0.0"))
	float HearingRange = 1000.0f;

	/** Time to lose target after losing sight (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory", meta = (ClampMin = "0.0"))
	float TargetMemoryDuration = 5.0f;

	/** Auto-detect range (always detects within this range) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sight", meta = (ClampMin = "0.0"))
	float AutoDetectRange = 300.0f;
};

// ═══════════════════════════════════════════════════════════════════════════════════
// ENEMY COMBAT ATTRIBUTES - Combat behavior parameters
// ═══════════════════════════════════════════════════════════════════════════════════

USTRUCT(BlueprintType)
struct ENEMYSYSTEM_API FSuspenseCoreEnemyCombatAttributes
{
	GENERATED_BODY()

	/** Preferred combat distance (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Range", meta = (ClampMin = "0.0"))
	float PreferredCombatRange = 800.0f;

	/** Minimum combat distance (cm) - will back up if closer */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Range", meta = (ClampMin = "0.0"))
	float MinCombatRange = 200.0f;

	/** Maximum combat distance (cm) - will advance if further */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Range", meta = (ClampMin = "0.0"))
	float MaxCombatRange = 1500.0f;

	/** Melee attack range (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee", meta = (ClampMin = "0.0"))
	float MeleeAttackRange = 150.0f;

	/** Attack cooldown (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0.0"))
	float AttackCooldown = 1.5f;

	/** Aggression level (0-1, affects decision making) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AggressionLevel = 0.5f;

	/** Accuracy rating (0-1, affects aim) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accuracy", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Accuracy = 0.7f;
};

// ═══════════════════════════════════════════════════════════════════════════════════
// ENEMY LOOT DROP CONFIG
// ═══════════════════════════════════════════════════════════════════════════════════

USTRUCT(BlueprintType)
struct ENEMYSYSTEM_API FSuspenseCoreEnemyLootConfig
{
	GENERATED_BODY()

	/** Base experience points awarded on kill */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experience", meta = (ClampMin = "0"))
	int32 BaseExperience = 50;

	/** Base currency dropped */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Currency", meta = (ClampMin = "0"))
	int32 BaseCurrency = 10;

	/** Loot table row name for drops */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	FName LootTableRowName;

	/** Drop chance multiplier (1.0 = normal) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (ClampMin = "0.0"))
	float DropChanceMultiplier = 1.0f;
};

// ═══════════════════════════════════════════════════════════════════════════════════
// ENEMY PRESET ROW - SSOT DataTable row for enemy configurations
// ═══════════════════════════════════════════════════════════════════════════════════

/**
 * FSuspenseCoreEnemyPresetRow
 *
 * SSOT (Single Source of Truth) DataTable row for enemy configurations.
 * Each row defines a complete enemy preset that can be referenced by
 * SuspenseCoreEnemyBehaviorData assets.
 *
 * Usage:
 * 1. Create DataTable with this row structure (DT_EnemyPresets)
 * 2. Add rows for each enemy type (e.g., "Scav_Grunt", "Scav_Heavy", "Boss_Killa")
 * 3. Reference row names in SuspenseCoreEnemyBehaviorData.PresetRowName
 * 4. DataManager loads and caches on game start
 */
USTRUCT(BlueprintType)
struct ENEMYSYSTEM_API FSuspenseCoreEnemyPresetRow : public FTableRowBase
{
	GENERATED_BODY()

	// ═══════════════════════════════════════════════════════════════════════════
	// IDENTITY
	// ═══════════════════════════════════════════════════════════════════════════

	/** Unique preset identifier (same as row name) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FName PresetID;

	/** Display name for UI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FText DisplayName;

	/** Description for designers */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity", meta = (MultiLine = true))
	FText Description;

	/** Enemy type tag (e.g., Enemy.Type.Scav, Enemy.Type.Boss) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FGameplayTag EnemyTypeTag;

	/** Archetype classification */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	ESuspenseCoreEnemyArchetype Archetype = ESuspenseCoreEnemyArchetype::Grunt;

	/** Combat style preference */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	ESuspenseCoreEnemyCombatStyle CombatStyle = ESuspenseCoreEnemyCombatStyle::Balanced;

	// ═══════════════════════════════════════════════════════════════════════════
	// ATTRIBUTES
	// ═══════════════════════════════════════════════════════════════════════════

	/** Base combat attributes (HP, Armor, Attack) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	FSuspenseCoreEnemyBaseAttributes BaseAttributes;

	/** Movement attributes (Speed, Jump, Fly) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	FSuspenseCoreEnemyMovementAttributes MovementAttributes;

	/** Perception attributes (Sight, Hearing) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	FSuspenseCoreEnemyPerceptionAttributes PerceptionAttributes;

	/** Combat behavior attributes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	FSuspenseCoreEnemyCombatAttributes CombatAttributes;

	// ═══════════════════════════════════════════════════════════════════════════
	// LEVEL SCALING
	// ═══════════════════════════════════════════════════════════════════════════

	/** Base enemy level (used for scaling) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scaling", meta = (ClampMin = "1"))
	int32 BaseLevel = 1;

	/** Health scaling per level (additive) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scaling")
	float HealthPerLevel = 10.0f;

	/** Armor scaling per level (additive) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scaling")
	float ArmorPerLevel = 1.0f;

	/** Attack power scaling per level (additive) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scaling")
	float AttackPowerPerLevel = 2.0f;

	/** Experience scaling per level (multiplicative) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scaling")
	float ExperiencePerLevelMultiplier = 1.1f;

	// ═══════════════════════════════════════════════════════════════════════════
	// GAS CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Gameplay abilities to grant on spawn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	/** Gameplay effects to apply on spawn (for setting attributes) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
	TArray<TSubclassOf<UGameplayEffect>> StartupEffects;

	/** Passive gameplay effects (always active) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
	TArray<TSubclassOf<UGameplayEffect>> PassiveEffects;

	/** Tags to add to the enemy on spawn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
	FGameplayTagContainer GrantedTags;

	// ═══════════════════════════════════════════════════════════════════════════
	// EQUIPMENT
	// ═══════════════════════════════════════════════════════════════════════════

	/** Default weapon preset tag (references equipment system) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
	FGameplayTag DefaultWeaponPreset;

	/** Alternative weapon presets (random selection) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
	TArray<FGameplayTag> AlternativeWeaponPresets;

	/** Armor preset tag */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
	FGameplayTag ArmorPreset;

	// ═══════════════════════════════════════════════════════════════════════════
	// LOOT
	// ═══════════════════════════════════════════════════════════════════════════

	/** Loot and reward configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	FSuspenseCoreEnemyLootConfig LootConfig;

	// ═══════════════════════════════════════════════════════════════════════════
	// VISUALS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Skeletal mesh to use (if different from default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	TSoftObjectPtr<USkeletalMesh> OverrideMesh;

	/** Animation blueprint to use (if different from default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	TSoftClassPtr<UAnimInstance> OverrideAnimBlueprint;

	/** Material overrides (index -> material) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	TMap<int32, TSoftObjectPtr<UMaterialInterface>> MaterialOverrides;

	// ═══════════════════════════════════════════════════════════════════════════
	// HELPERS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Calculate scaled health for a given level */
	float GetScaledHealth(int32 Level) const
	{
		const int32 LevelDiff = FMath::Max(0, Level - BaseLevel);
		return BaseAttributes.MaxHealth + (HealthPerLevel * LevelDiff);
	}

	/** Calculate scaled armor for a given level */
	float GetScaledArmor(int32 Level) const
	{
		const int32 LevelDiff = FMath::Max(0, Level - BaseLevel);
		return BaseAttributes.Armor + (ArmorPerLevel * LevelDiff);
	}

	/** Calculate scaled attack power for a given level */
	float GetScaledAttackPower(int32 Level) const
	{
		const int32 LevelDiff = FMath::Max(0, Level - BaseLevel);
		return BaseAttributes.AttackPower + (AttackPowerPerLevel * LevelDiff);
	}

	/** Calculate scaled experience for a given level */
	int32 GetScaledExperience(int32 Level) const
	{
		const int32 LevelDiff = FMath::Max(0, Level - BaseLevel);
		return FMath::RoundToInt(LootConfig.BaseExperience * FMath::Pow(ExperiencePerLevelMultiplier, LevelDiff));
	}

	/** Get a random weapon preset from available options */
	FGameplayTag GetRandomWeaponPreset() const
	{
		if (AlternativeWeaponPresets.Num() > 0 && FMath::FRand() > 0.5f)
		{
			const int32 Index = FMath::RandRange(0, AlternativeWeaponPresets.Num() - 1);
			return AlternativeWeaponPresets[Index];
		}
		return DefaultWeaponPreset;
	}
};

// ═══════════════════════════════════════════════════════════════════════════════════
// ENEMY SPAWN CONFIG - For spawning enemies with specific configurations
// ═══════════════════════════════════════════════════════════════════════════════════

USTRUCT(BlueprintType)
struct ENEMYSYSTEM_API FSuspenseCoreEnemySpawnConfig
{
	GENERATED_BODY()

	/** Preset row name from DT_EnemyPresets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	FName PresetRowName;

	/** Override level (0 = use preset's base level) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0"))
	int32 OverrideLevel = 0;

	/** Override weapon preset (empty = use preset default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	FGameplayTag OverrideWeaponPreset;

	/** Additional tags to grant */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	FGameplayTagContainer AdditionalTags;

	/** Spawn as elite variant (increased stats) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	bool bEliteVariant = false;

	/** Elite stat multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "1.0", EditCondition = "bEliteVariant"))
	float EliteMultiplier = 1.5f;
};
