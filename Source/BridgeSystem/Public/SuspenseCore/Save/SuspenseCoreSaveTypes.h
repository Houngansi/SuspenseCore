// SuspenseCoreSaveTypes.h
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Data/SuspenseCorePlayerData.h"
#include "SuspenseCoreSaveTypes.generated.h"

// ═══════════════════════════════════════════════════════════════════════════════
// SAVE HEADER
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * FSuspenseCoreSaveHeader
 *
 * Metadata for save slots - displayed in save/load UI.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreSaveHeader
{
	GENERATED_BODY()

	/** Save format version for migration */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	int32 SaveVersion = 1;

	/** When this save was created */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FDateTime SaveTimestamp;

	/** Total play time in seconds */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	int64 TotalPlayTimeSeconds = 0;

	/** Slot name (user-defined or auto) */
	UPROPERTY(BlueprintReadWrite, Category = "Save")
	FString SlotName;

	/** Description for display */
	UPROPERTY(BlueprintReadWrite, Category = "Save")
	FString Description;

	/** Character name */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FString CharacterName;

	/** Character level */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	int32 CharacterLevel = 1;

	/** Current location name */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FString LocationName;

	/** Is this an auto-save slot */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	bool bIsAutoSave = false;

	/** Slot index */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	int32 SlotIndex = -1;

	/** Is slot empty */
	bool IsEmpty() const { return SlotName.IsEmpty() && CharacterName.IsEmpty(); }
};

// ═══════════════════════════════════════════════════════════════════════════════
// ACTIVE EFFECT
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * FSuspenseCoreActiveEffect
 *
 * Represents an active gameplay effect for saving.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreActiveEffect
{
	GENERATED_BODY()

	/** Effect class name or ID */
	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	FString EffectId;

	/** Remaining duration (0 = infinite) */
	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	float RemainingDuration = 0.0f;

	/** Stack count */
	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	int32 StackCount = 1;

	/** Who applied this effect */
	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	FString SourceId;

	/** Effect level/magnitude */
	UPROPERTY(BlueprintReadWrite, Category = "Effect")
	float Level = 1.0f;
};

// ═══════════════════════════════════════════════════════════════════════════════
// CHARACTER STATE
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * FSuspenseCoreCharacterState
 *
 * Runtime character state for saving mid-game.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreCharacterState
{
	GENERATED_BODY()

	// ═══════════════════════════════════════════════════════════════
	// ATTRIBUTES
	// ═══════════════════════════════════════════════════════════════

	/** Current health */
	UPROPERTY(BlueprintReadWrite, Category = "Attributes")
	float CurrentHealth = 100.0f;

	/** Maximum health */
	UPROPERTY(BlueprintReadWrite, Category = "Attributes")
	float MaxHealth = 100.0f;

	/** Current stamina */
	UPROPERTY(BlueprintReadWrite, Category = "Attributes")
	float CurrentStamina = 100.0f;

	/** Maximum stamina */
	UPROPERTY(BlueprintReadWrite, Category = "Attributes")
	float MaxStamina = 100.0f;

	/** Current mana/energy */
	UPROPERTY(BlueprintReadWrite, Category = "Attributes")
	float CurrentMana = 100.0f;

	/** Maximum mana/energy */
	UPROPERTY(BlueprintReadWrite, Category = "Attributes")
	float MaxMana = 100.0f;

	/** Current armor */
	UPROPERTY(BlueprintReadWrite, Category = "Attributes")
	float CurrentArmor = 0.0f;

	/** Current shield */
	UPROPERTY(BlueprintReadWrite, Category = "Attributes")
	float CurrentShield = 0.0f;

	// ═══════════════════════════════════════════════════════════════
	// POSITION
	// ═══════════════════════════════════════════════════════════════

	/** World position */
	UPROPERTY(BlueprintReadWrite, Category = "Position")
	FVector WorldPosition = FVector::ZeroVector;

	/** World rotation */
	UPROPERTY(BlueprintReadWrite, Category = "Position")
	FRotator WorldRotation = FRotator::ZeroRotator;

	/** Current map/level name */
	UPROPERTY(BlueprintReadWrite, Category = "Position")
	FName CurrentMapName;

	/** Current checkpoint ID */
	UPROPERTY(BlueprintReadWrite, Category = "Position")
	FString CurrentCheckpointId;

	// ═══════════════════════════════════════════════════════════════
	// EFFECTS & ABILITIES
	// ═══════════════════════════════════════════════════════════════

	/** Active gameplay effects */
	UPROPERTY(BlueprintReadWrite, Category = "Effects")
	TArray<FSuspenseCoreActiveEffect> ActiveEffects;

	/** Ability cooldowns: AbilityId -> RemainingCooldown */
	UPROPERTY(BlueprintReadWrite, Category = "Abilities")
	TMap<FString, float> AbilityCooldowns;

	/** Active ability tags (as strings) */
	UPROPERTY(BlueprintReadWrite, Category = "Abilities")
	TArray<FString> ActiveAbilityTags;

	// ═══════════════════════════════════════════════════════════════
	// STATE FLAGS
	// ═══════════════════════════════════════════════════════════════

	/** Is character in combat */
	UPROPERTY(BlueprintReadWrite, Category = "State")
	bool bIsInCombat = false;

	/** Is character dead */
	UPROPERTY(BlueprintReadWrite, Category = "State")
	bool bIsDead = false;

	/** Is character crouching */
	UPROPERTY(BlueprintReadWrite, Category = "State")
	bool bIsCrouching = false;

	/** Is character sprinting */
	UPROPERTY(BlueprintReadWrite, Category = "State")
	bool bIsSprinting = false;

	// ═══════════════════════════════════════════════════════════════
	// HELPERS
	// ═══════════════════════════════════════════════════════════════

	/** Check if state is valid */
	bool IsValid() const
	{
		return MaxHealth > 0.0f && !CurrentMapName.IsNone();
	}

	/** Reset to defaults */
	void Reset()
	{
		*this = FSuspenseCoreCharacterState();
	}
};

// ═══════════════════════════════════════════════════════════════════════════════
// RUNTIME ITEM
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * FSuspenseCoreRuntimeItem
 *
 * Item instance for runtime inventory saving.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreRuntimeItem
{
	GENERATED_BODY()

	/** Item definition ID (references DataAsset) */
	UPROPERTY(BlueprintReadWrite, Category = "Item")
	FString DefinitionId;

	/** Unique instance ID */
	UPROPERTY(BlueprintReadWrite, Category = "Item")
	FString InstanceId;

	/** Stack quantity */
	UPROPERTY(BlueprintReadWrite, Category = "Item")
	int32 Quantity = 1;

	/** Slot index in inventory (-1 = not assigned) */
	UPROPERTY(BlueprintReadWrite, Category = "Item")
	int32 SlotIndex = -1;

	/** Durability (0-1) */
	UPROPERTY(BlueprintReadWrite, Category = "Item")
	float Durability = 1.0f;

	/** Upgrade level */
	UPROPERTY(BlueprintReadWrite, Category = "Item")
	int32 UpgradeLevel = 0;

	/** Attachment IDs */
	UPROPERTY(BlueprintReadWrite, Category = "Item")
	TArray<FString> AttachmentIds;

	/** Custom data (JSON) */
	UPROPERTY(BlueprintReadWrite, Category = "Item")
	FString CustomData;
};

// ═══════════════════════════════════════════════════════════════════════════════
// INVENTORY STATE
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * FSuspenseCoreInventoryState
 *
 * Full inventory state for saving.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreInventoryState
{
	GENERATED_BODY()

	/** All items */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory")
	TArray<FSuspenseCoreRuntimeItem> Items;

	/** Currencies: CurrencyId -> Amount */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory")
	TMap<FString, int64> Currencies;

	/** Inventory size */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory")
	int32 InventorySize = 50;
};

// ═══════════════════════════════════════════════════════════════════════════════
// EQUIPMENT STATE
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * FSuspenseCoreEquipmentState
 *
 * Equipped items state for saving.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreEquipmentState
{
	GENERATED_BODY()

	/** Equipped slots: SlotName -> ItemInstanceId */
	UPROPERTY(BlueprintReadWrite, Category = "Equipment")
	TMap<FString, FString> EquippedSlots;

	/** Active loadout index */
	UPROPERTY(BlueprintReadWrite, Category = "Equipment")
	int32 ActiveLoadoutIndex = 0;

	/** Quick slots (hotbar): SlotIndex -> ItemInstanceId */
	UPROPERTY(BlueprintReadWrite, Category = "Equipment")
	TMap<int32, FString> QuickSlots;

	/** Current weapon ammo: WeaponInstanceId -> CurrentAmmo */
	UPROPERTY(BlueprintReadWrite, Category = "Equipment")
	TMap<FString, int32> WeaponAmmo;
};

// ═══════════════════════════════════════════════════════════════════════════════
// FULL SAVE DATA
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * FSuspenseCoreSaveData
 *
 * Complete save data structure.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreSaveData
{
	GENERATED_BODY()

	/** Metadata header */
	UPROPERTY(BlueprintReadWrite, Category = "Save")
	FSuspenseCoreSaveHeader Header;

	/** Profile data (account, XP, stats, settings) */
	UPROPERTY(BlueprintReadWrite, Category = "Save")
	FSuspenseCorePlayerData ProfileData;

	/** Character runtime state */
	UPROPERTY(BlueprintReadWrite, Category = "Save")
	FSuspenseCoreCharacterState CharacterState;

	/** Inventory state */
	UPROPERTY(BlueprintReadWrite, Category = "Save")
	FSuspenseCoreInventoryState InventoryState;

	/** Equipment state */
	UPROPERTY(BlueprintReadWrite, Category = "Save")
	FSuspenseCoreEquipmentState EquipmentState;

	// ═══════════════════════════════════════════════════════════════
	// VERSION
	// ═══════════════════════════════════════════════════════════════

	static constexpr int32 CURRENT_VERSION = 1;

	// ═══════════════════════════════════════════════════════════════
	// HELPERS
	// ═══════════════════════════════════════════════════════════════

	/** Check if save data is valid */
	bool IsValid() const
	{
		return ProfileData.IsValid() && Header.SaveVersion > 0;
	}

	/** Create empty save data with current timestamp */
	static FSuspenseCoreSaveData CreateEmpty()
	{
		FSuspenseCoreSaveData Data;
		Data.Header.SaveVersion = CURRENT_VERSION;
		Data.Header.SaveTimestamp = FDateTime::UtcNow();
		return Data;
	}
};

// ═══════════════════════════════════════════════════════════════════════════════
// SAVE RESULT
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * ESuspenseCoreSaveResult
 *
 * Result of save/load operations.
 */
UENUM(BlueprintType)
enum class ESuspenseCoreSaveResult : uint8
{
	Success,
	Failed,
	SlotNotFound,
	CorruptedData,
	VersionMismatch,
	DiskFull,
	PermissionDenied,
	InProgress
};

