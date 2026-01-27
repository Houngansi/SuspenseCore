// SuspenseCoreEnemyLoadoutTypes.h
// SuspenseCore - Enemy Loadout System (Tarkov-Style)
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreEnemyLoadoutTypes.generated.h"

// ═══════════════════════════════════════════════════════════════════════════════════
// ENUMS
// ═══════════════════════════════════════════════════════════════════════════════════

/**
 * Where a magazine is stored on the enemy
 */
UENUM(BlueprintType)
enum class EEnemyMagazineLocation : uint8
{
	InWeapon		UMETA(DisplayName = "In Weapon (loaded)"),
	InRig			UMETA(DisplayName = "In Tactical Rig"),
	InPockets		UMETA(DisplayName = "In Pockets"),
	InBackpack		UMETA(DisplayName = "In Backpack")
};

/**
 * Equipment slot type for enemy loadout
 */
UENUM(BlueprintType)
enum class EEnemyEquipmentSlot : uint8
{
	PrimaryWeapon		UMETA(DisplayName = "Primary Weapon"),
	SecondaryWeapon		UMETA(DisplayName = "Secondary Weapon (Holster)"),
	MeleeWeapon			UMETA(DisplayName = "Melee Weapon"),
	BodyArmor			UMETA(DisplayName = "Body Armor"),
	Helmet				UMETA(DisplayName = "Helmet"),
	TacticalRig			UMETA(DisplayName = "Tactical Rig"),
	Backpack			UMETA(DisplayName = "Backpack"),
	Headset				UMETA(DisplayName = "Headset"),
	FaceCover			UMETA(DisplayName = "Face Cover"),
	Eyewear				UMETA(DisplayName = "Eyewear")
};

// ═══════════════════════════════════════════════════════════════════════════════════
// MAGAZINE CONFIG - Configuration for a single magazine
// ═══════════════════════════════════════════════════════════════════════════════════

/**
 * FEnemyMagazineConfig
 *
 * Defines a magazine with its ammo load.
 * Used to create actual magazine instances with specific ammo types and counts.
 */
USTRUCT(BlueprintType)
struct ENEMYSYSTEM_API FEnemyMagazineConfig
{
	GENERATED_BODY()

	/** Item ID of the magazine (from DT_Items) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine")
	FName MagazineItemID;

	/** Item ID of the ammo type loaded */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine")
	FName AmmoItemID;

	/** Number of rounds loaded (clamped to magazine capacity at runtime) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine", meta = (ClampMin = "0"))
	int32 LoadedAmmoCount = 30;

	/** Where this magazine is stored */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine")
	EEnemyMagazineLocation Location = EEnemyMagazineLocation::InWeapon;
};

// ═══════════════════════════════════════════════════════════════════════════════════
// LOOSE AMMO CONFIG - Configuration for loose ammunition
// ═══════════════════════════════════════════════════════════════════════════════════

/**
 * FEnemyLooseAmmoConfig
 *
 * Defines loose (non-magazine) ammunition the enemy carries.
 * Supports random quantity range.
 */
USTRUCT(BlueprintType)
struct ENEMYSYSTEM_API FEnemyLooseAmmoConfig
{
	GENERATED_BODY()

	/** Item ID of the ammo type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
	FName AmmoItemID;

	/** Minimum count (for random range) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo", meta = (ClampMin = "0"))
	int32 MinCount = 0;

	/** Maximum count (for random range) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo", meta = (ClampMin = "0"))
	int32 MaxCount = 30;

	/** Spawn chance (0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpawnChance = 1.0f;

	/** Get random count within range */
	int32 GetRandomCount() const
	{
		if (FMath::FRand() > SpawnChance) return 0;
		return FMath::RandRange(MinCount, MaxCount);
	}
};

// ═══════════════════════════════════════════════════════════════════════════════════
// WEAPON LOADOUT - Complete weapon configuration
// ═══════════════════════════════════════════════════════════════════════════════════

/**
 * FEnemyWeaponLoadout
 *
 * Complete configuration for a weapon including attachments,
 * magazines, and ammunition.
 */
USTRUCT(BlueprintType)
struct ENEMYSYSTEM_API FEnemyWeaponLoadout
{
	GENERATED_BODY()

	/** Item ID of the weapon (from DT_Items) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FName WeaponItemID;

	/** Item IDs of attachments/modifications */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TArray<FName> AttachmentItemIDs;

	/** Magazines for this weapon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TArray<FEnemyMagazineConfig> Magazines;

	/** Loose ammunition for this weapon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FEnemyLooseAmmoConfig LooseAmmo;

	/** Equipment slot for this weapon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	EEnemyEquipmentSlot EquipSlot = EEnemyEquipmentSlot::PrimaryWeapon;

	/** Weapon durability (0-1, 1 = factory new) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Durability = 1.0f;

	/** Get the first magazine configured to be in the weapon */
	const FEnemyMagazineConfig* GetLoadedMagazine() const
	{
		for (const FEnemyMagazineConfig& Mag : Magazines)
		{
			if (Mag.Location == EEnemyMagazineLocation::InWeapon)
			{
				return &Mag;
			}
		}
		return nullptr;
	}

	/** Get all spare magazines (not in weapon) */
	TArray<FEnemyMagazineConfig> GetSpareMagazines() const
	{
		TArray<FEnemyMagazineConfig> Spare;
		for (const FEnemyMagazineConfig& Mag : Magazines)
		{
			if (Mag.Location != EEnemyMagazineLocation::InWeapon)
			{
				Spare.Add(Mag);
			}
		}
		return Spare;
	}
};

// ═══════════════════════════════════════════════════════════════════════════════════
// ARMOR LOADOUT - Body protection configuration
// ═══════════════════════════════════════════════════════════════════════════════════

/**
 * FEnemyArmorLoadout
 *
 * Configuration for all protective equipment (armor, helmet, etc.)
 */
USTRUCT(BlueprintType)
struct ENEMYSYSTEM_API FEnemyArmorLoadout
{
	GENERATED_BODY()

	/** Body armor Item ID (empty = no armor) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor")
	FName BodyArmorItemID;

	/** Helmet Item ID (empty = no helmet) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor")
	FName HelmetItemID;

	/** Tactical rig Item ID (empty = no rig) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor")
	FName RigItemID;

	/** Face cover Item ID (empty = none) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor")
	FName FaceCoverItemID;

	/** Eyewear Item ID (empty = none) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor")
	FName EyewearItemID;

	/** Headset Item ID (empty = none) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor")
	FName HeadsetItemID;

	/** Body armor durability (0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Durability", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BodyArmorDurability = 1.0f;

	/** Helmet durability (0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Durability", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HelmetDurability = 1.0f;

	/** Check if enemy has any body armor */
	bool HasBodyArmor() const { return !BodyArmorItemID.IsNone(); }

	/** Check if enemy has any helmet */
	bool HasHelmet() const { return !HelmetItemID.IsNone(); }

	/** Check if enemy has a tactical rig */
	bool HasRig() const { return !RigItemID.IsNone(); }
};

// ═══════════════════════════════════════════════════════════════════════════════════
// INVENTORY ITEM - Generic item in inventory
// ═══════════════════════════════════════════════════════════════════════════════════

/**
 * FEnemyInventoryItem
 *
 * Generic inventory item with quantity and spawn chance.
 * Used for rig contents, backpack contents, pockets, etc.
 */
USTRUCT(BlueprintType)
struct ENEMYSYSTEM_API FEnemyInventoryItem
{
	GENERATED_BODY()

	/** Item ID from database */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FName ItemID;

	/** Fixed quantity (used if MinQuantity == MaxQuantity) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (ClampMin = "1"))
	int32 Quantity = 1;

	/** Minimum quantity for random range */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random", meta = (ClampMin = "0"))
	int32 MinQuantity = 1;

	/** Maximum quantity for random range */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random", meta = (ClampMin = "0"))
	int32 MaxQuantity = 1;

	/** Chance this item spawns (0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpawnChance = 1.0f;

	/** Get actual quantity (handles random range) */
	int32 GetActualQuantity() const
	{
		if (FMath::FRand() > SpawnChance) return 0;
		if (MinQuantity == MaxQuantity) return Quantity;
		return FMath::RandRange(MinQuantity, MaxQuantity);
	}
};

// ═══════════════════════════════════════════════════════════════════════════════════
// LOADOUT PRESET - Complete enemy loadout
// ═══════════════════════════════════════════════════════════════════════════════════

/**
 * FEnemyLoadoutPreset
 *
 * Complete loadout configuration for an enemy.
 * Defines all weapons, armor, and inventory contents.
 *
 * This is the main structure that gets stored in enemy presets
 * and used to initialize enemy inventory on spawn.
 */
USTRUCT(BlueprintType)
struct ENEMYSYSTEM_API FEnemyLoadoutPreset
{
	GENERATED_BODY()

	// ═══════════════════════════════════════════════════════════════════════════
	// WEAPONS
	// ═══════════════════════════════════════════════════════════════════════════

	/** All weapons this enemy carries */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapons")
	TArray<FEnemyWeaponLoadout> Weapons;

	// ═══════════════════════════════════════════════════════════════════════════
	// PROTECTION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Armor and protective equipment */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Protection")
	FEnemyArmorLoadout Armor;

	/** Backpack Item ID (empty = no backpack) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Protection")
	FName BackpackItemID;

	// ═══════════════════════════════════════════════════════════════════════════
	// INVENTORY CONTENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Items stored in tactical rig */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TArray<FEnemyInventoryItem> RigContents;

	/** Items stored in backpack */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TArray<FEnemyInventoryItem> BackpackContents;

	/** Items stored in pockets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TArray<FEnemyInventoryItem> PocketContents;

	// ═══════════════════════════════════════════════════════════════════════════
	// SPECIAL ITEMS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Currency carried by enemy */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Special")
	TArray<FEnemyInventoryItem> Currency;

	/** Quest items (guaranteed drops, separate category) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Special")
	TArray<FEnemyInventoryItem> QuestItems;

	/** Keys/keycards carried */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Special")
	TArray<FEnemyInventoryItem> Keys;

	// ═══════════════════════════════════════════════════════════════════════════
	// HELPERS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Get primary weapon config (first weapon in PrimaryWeapon slot) */
	const FEnemyWeaponLoadout* GetPrimaryWeapon() const
	{
		for (const FEnemyWeaponLoadout& Weapon : Weapons)
		{
			if (Weapon.EquipSlot == EEnemyEquipmentSlot::PrimaryWeapon)
			{
				return &Weapon;
			}
		}
		return nullptr;
	}

	/** Get secondary weapon config */
	const FEnemyWeaponLoadout* GetSecondaryWeapon() const
	{
		for (const FEnemyWeaponLoadout& Weapon : Weapons)
		{
			if (Weapon.EquipSlot == EEnemyEquipmentSlot::SecondaryWeapon)
			{
				return &Weapon;
			}
		}
		return nullptr;
	}

	/** Check if loadout has any weapons */
	bool HasWeapons() const { return Weapons.Num() > 0; }

	/** Check if loadout has backpack */
	bool HasBackpack() const { return !BackpackItemID.IsNone(); }

	/** Get total magazine count for all weapons */
	int32 GetTotalMagazineCount() const
	{
		int32 Total = 0;
		for (const FEnemyWeaponLoadout& Weapon : Weapons)
		{
			Total += Weapon.Magazines.Num();
		}
		return Total;
	}
};

// ═══════════════════════════════════════════════════════════════════════════════════
// LOADOUT VARIATION - For randomization
// ═══════════════════════════════════════════════════════════════════════════════════

/**
 * FEnemyLoadoutVariation
 *
 * Defines a possible variation of a loadout with a selection weight.
 * Used for randomized loadout selection.
 */
USTRUCT(BlueprintType)
struct ENEMYSYSTEM_API FEnemyLoadoutVariation
{
	GENERATED_BODY()

	/** The loadout preset for this variation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variation")
	FEnemyLoadoutPreset Loadout;

	/** Selection weight (higher = more likely) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variation", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	/** Optional name for this variation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variation")
	FName VariationName;
};
