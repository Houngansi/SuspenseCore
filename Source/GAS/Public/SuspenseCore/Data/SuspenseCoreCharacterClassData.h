// SuspenseCoreCharacterClassData.h
// SuspenseCore - Character Class System
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpec.h"
#include "SuspenseCoreCharacterClassData.generated.h"

class UGameplayAbility;
class UGameplayEffect;
class UTexture2D;
class USkeletalMesh;
class UAnimInstance;
class UAnimSequence;

/**
 * ESuspenseCoreClassRole
 *
 * Defines the primary combat role of a character class.
 * Used for matchmaking balance and team composition.
 */
UENUM(BlueprintType)
enum class ESuspenseCoreClassRole : uint8
{
	Assault		UMETA(DisplayName = "Assault", ToolTip = "Front-line damage dealer"),
	Tank		UMETA(DisplayName = "Tank", ToolTip = "High survivability, crowd control"),
	Support		UMETA(DisplayName = "Support", ToolTip = "Healing, buffs, utility"),
	Recon		UMETA(DisplayName = "Recon", ToolTip = "Scouting, long-range, mobility"),
	Engineer	UMETA(DisplayName = "Engineer", ToolTip = "Deployables, repairs, area denial")
};

/**
 * FSuspenseCoreAttributeModifier
 *
 * Defines how a class modifies base attribute values.
 * Uses multipliers for scalability across progression.
 */
USTRUCT(BlueprintType)
struct GAS_API FSuspenseCoreAttributeModifier
{
	GENERATED_BODY()

	// ═══════════════════════════════════════════════════════════════════════════
	// HEALTH & SURVIVABILITY
	// ═══════════════════════════════════════════════════════════════════════════

	/** MaxHealth multiplier (1.0 = 100 base) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float MaxHealthMultiplier = 1.0f;

	/** HealthRegen multiplier (1.0 = base regen rate) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	float HealthRegenMultiplier = 1.0f;

	/** Armor bonus (additive, 0 = no bonus) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float ArmorBonus = 0.0f;

	// ═══════════════════════════════════════════════════════════════════════════
	// SHIELD
	// ═══════════════════════════════════════════════════════════════════════════

	/** MaxShield multiplier */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shield", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float MaxShieldMultiplier = 1.0f;

	/** ShieldRegen multiplier */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shield", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	float ShieldRegenMultiplier = 1.0f;

	/** ShieldRegenDelay multiplier (lower = faster start) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shield", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float ShieldRegenDelayMultiplier = 1.0f;

	// ═══════════════════════════════════════════════════════════════════════════
	// COMBAT
	// ═══════════════════════════════════════════════════════════════════════════

	/** AttackPower multiplier (damage output) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float AttackPowerMultiplier = 1.0f;

	/** Weapon accuracy multiplier (spread reduction) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.5", ClampMax = "1.5"))
	float AccuracyMultiplier = 1.0f;

	/** Reload speed multiplier (higher = faster) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float ReloadSpeedMultiplier = 1.0f;

	// ═══════════════════════════════════════════════════════════════════════════
	// STAMINA & MOVEMENT
	// ═══════════════════════════════════════════════════════════════════════════

	/** MaxStamina multiplier */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stamina", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float MaxStaminaMultiplier = 1.0f;

	/** StaminaRegen multiplier */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stamina", meta = (ClampMin = "0.5", ClampMax = "3.0"))
	float StaminaRegenMultiplier = 1.0f;

	/** Base movement speed multiplier */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.7", ClampMax = "1.5"))
	float MovementSpeedMultiplier = 1.0f;

	/** Sprint speed multiplier */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.7", ClampMax = "1.5"))
	float SprintSpeedMultiplier = 1.0f;

	/** Jump height multiplier */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float JumpHeightMultiplier = 1.0f;

	// ═══════════════════════════════════════════════════════════════════════════
	// WEIGHT & CARRY
	// ═══════════════════════════════════════════════════════════════════════════

	/** MaxWeight multiplier (carry capacity) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weight", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float MaxWeightMultiplier = 1.0f;
};

/**
 * FSuspenseCoreClassAbilitySlot
 *
 * Defines an ability granted by the class with its unlock requirements.
 */
USTRUCT(BlueprintType)
struct GAS_API FSuspenseCoreClassAbilitySlot
{
	GENERATED_BODY()

	/** The ability class to grant */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	TSubclassOf<UGameplayAbility> AbilityClass;

	/** Level required to unlock (0 = available immediately) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability", meta = (ClampMin = "0", ClampMax = "100"))
	int32 RequiredLevel = 0;

	/** Input action tag for binding */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	FGameplayTag InputTag;

	/** Is this a passive ability (always active)? */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	bool bIsPassive = false;

	/** Slot index for UI display (0-3 for active, 4+ for passive) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability", meta = (ClampMin = "0", ClampMax = "10"))
	int32 SlotIndex = 0;
};

/**
 * USuspenseCoreCharacterClassData
 *
 * Primary Data Asset defining a character class.
 * Contains all configuration for attributes, abilities, effects, and visuals.
 *
 * Usage:
 * 1. Create Blueprint child: BP_CharacterClass_Assault
 * 2. Configure in Editor
 * 3. Reference in PlayerState or CharacterManager
 *
 * Design Philosophy:
 * - Data-driven for designer iteration
 * - Multiplier-based for balanced scaling
 * - Supports progression unlocks
 */
UCLASS(BlueprintType, Blueprintable)
class GAS_API USuspenseCoreCharacterClassData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	USuspenseCoreCharacterClassData();

	// ═══════════════════════════════════════════════════════════════════════════
	// IDENTITY
	// ═══════════════════════════════════════════════════════════════════════════

	/** Internal class identifier */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	FName ClassID;

	/** Localized display name */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	FText DisplayName;

	/** Short description for selection screen */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
	FText ShortDescription;

	/** Full description with lore */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
	FText FullDescription;

	/** Gameplay tag for this class */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	FGameplayTag ClassTag;

	/** Combat role */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	ESuspenseCoreClassRole Role = ESuspenseCoreClassRole::Assault;

	// ═══════════════════════════════════════════════════════════════════════════
	// VISUALS - UI
	// ═══════════════════════════════════════════════════════════════════════════

	/** Class icon for UI */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals|UI")
	TSoftObjectPtr<UTexture2D> ClassIcon;

	/** Large portrait for selection screen */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals|UI")
	TSoftObjectPtr<UTexture2D> ClassPortrait;

	/** Primary class color for UI theming */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals|UI")
	FLinearColor PrimaryColor = FLinearColor(0.2f, 0.6f, 1.0f, 1.0f);

	/** Secondary accent color */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals|UI")
	FLinearColor SecondaryColor = FLinearColor(0.1f, 0.3f, 0.5f, 1.0f);

	// ═══════════════════════════════════════════════════════════════════════════
	// VISUALS - CHARACTER MESH
	// ═══════════════════════════════════════════════════════════════════════════

	/** Character skeletal mesh for preview and gameplay */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals|Character")
	TSoftObjectPtr<USkeletalMesh> CharacterMesh;

	/** First person arms mesh (if different from third person) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals|Character")
	TSoftObjectPtr<USkeletalMesh> FirstPersonArmsMesh;

	/** Animation Blueprint for the character */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals|Character")
	TSoftClassPtr<UAnimInstance> AnimationBlueprint;

	/** Preview animation to play in character selection */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals|Character")
	TSoftObjectPtr<UAnimSequence> PreviewIdleAnimation;

	// ═══════════════════════════════════════════════════════════════════════════
	// ATTRIBUTES
	// ═══════════════════════════════════════════════════════════════════════════

	/** Attribute modifiers for this class */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attributes")
	FSuspenseCoreAttributeModifier AttributeModifiers;

	// ═══════════════════════════════════════════════════════════════════════════
	// ABILITIES
	// ═══════════════════════════════════════════════════════════════════════════

	/** Abilities granted by this class */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TArray<FSuspenseCoreClassAbilitySlot> ClassAbilities;

	// ═══════════════════════════════════════════════════════════════════════════
	// EFFECTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** GameplayEffect applied on class selection to set initial attributes */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	TSubclassOf<UGameplayEffect> InitialAttributesEffect;

	/** Passive effects always active for this class */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	TArray<TSubclassOf<UGameplayEffect>> PassiveEffects;

	// ═══════════════════════════════════════════════════════════════════════════
	// LOADOUT
	// ═══════════════════════════════════════════════════════════════════════════

	/** Default primary weapon ID */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout")
	FName DefaultPrimaryWeapon;

	/** Default secondary weapon ID */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout")
	FName DefaultSecondaryWeapon;

	/** Default equipment/gadget IDs */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout")
	TArray<FName> DefaultEquipment;

	// ═══════════════════════════════════════════════════════════════════════════
	// BALANCE METADATA
	// ═══════════════════════════════════════════════════════════════════════════

	/** Difficulty rating for new players (1-5) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "1", ClampMax = "5"))
	int32 DifficultyRating = 2;

	/** Is this class available to all players? */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance")
	bool bIsStarterClass = true;

	/** Level required to unlock (if not starter) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance", meta = (ClampMin = "0", ClampMax = "100", EditCondition = "!bIsStarterClass"))
	int32 UnlockLevel = 0;

	// ═══════════════════════════════════════════════════════════════════════════
	// UPrimaryDataAsset Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// ═══════════════════════════════════════════════════════════════════════════
	// UTILITY
	// ═══════════════════════════════════════════════════════════════════════════

	/** Get abilities available at given level */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Class")
	TArray<TSubclassOf<UGameplayAbility>> GetAbilitiesForLevel(int32 Level) const;

	/** Check if class is unlocked for player */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Class")
	bool IsUnlockedForLevel(int32 PlayerLevel) const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
