// SuspenseCoreItemTypes.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "SuspenseCoreItemTypes.generated.h"

// Forward declarations
class UStaticMesh;
class UNiagaraSystem;
class USoundBase;
class UTexture2D;
class UGameplayEffect;
class UAttributeSet;
class UGameplayAbility;

/**
 * FSuspenseCoreItemIdentity
 *
 * Core identification data for an item.
 * This is the minimum required data for any item.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreItemIdentity
{
	GENERATED_BODY()

	/** Unique identifier - DataTable row name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FName ItemID;

	/** Localized display name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FText DisplayName;

	/** Localized description */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FText Description;

	/** UI icon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	TSoftObjectPtr<UTexture2D> Icon;

	FSuspenseCoreItemIdentity()
		: ItemID(NAME_None)
	{
	}

	bool IsValid() const { return !ItemID.IsNone(); }
};

/**
 * FSuspenseCoreItemClassification
 *
 * Classification and categorization data.
 * Uses GameplayTags for flexible item taxonomy.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreItemClassification
{
	GENERATED_BODY()

	/** Primary item type (Item.Weapon, Item.Armor, Item.Consumable, etc) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Classification",
		meta = (Categories = "Item"))
	FGameplayTag ItemType;

	/** Item rarity (Item.Rarity.Common, etc) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Classification",
		meta = (Categories = "Item.Rarity"))
	FGameplayTag Rarity;

	/** Additional classification tags */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Classification")
	FGameplayTagContainer ItemTags;

	FSuspenseCoreItemClassification()
	{
	}
};

/**
 * FSuspenseCoreInventoryProperties
 *
 * Inventory-related properties.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreInventoryProperties
{
	GENERATED_BODY()

	/** Grid size (width, height) for grid-based inventory */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory",
		meta = (ClampMin = "1", ClampMax = "10"))
	FIntPoint GridSize;

	/** Maximum stack size (1 = not stackable) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory",
		meta = (ClampMin = "1"))
	int32 MaxStackSize;

	/** Weight per unit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory",
		meta = (ClampMin = "0.0"))
	float Weight;

	/** Base value for trading/selling */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory",
		meta = (ClampMin = "0"))
	int32 BaseValue;

	FSuspenseCoreInventoryProperties()
		: GridSize(1, 1)
		, MaxStackSize(1)
		, Weight(0.1f)
		, BaseValue(100)
	{
	}

	bool IsStackable() const { return MaxStackSize > 1; }
};

/**
 * FSuspenseCoreItemBehavior
 *
 * Behavior flags determining how the item can be used.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreItemBehavior
{
	GENERATED_BODY()

	/** Can be equipped to equipment slots */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	bool bIsEquippable;

	/** Can be consumed/used */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	bool bIsConsumable;

	/** Can be dropped on ground */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	bool bCanDrop;

	/** Can be traded with other players */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	bool bCanTrade;

	/** Quest item - cannot be discarded */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	bool bIsQuestItem;

	FSuspenseCoreItemBehavior()
		: bIsEquippable(false)
		, bIsConsumable(false)
		, bCanDrop(true)
		, bCanTrade(true)
		, bIsQuestItem(false)
	{
	}
};

/**
 * FSuspenseCoreItemVisuals
 *
 * Visual asset references.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreItemVisuals
{
	GENERATED_BODY()

	/** Mesh displayed in world (pickup) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	TSoftObjectPtr<UStaticMesh> WorldMesh;

	/** VFX on spawn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	TSoftObjectPtr<UNiagaraSystem> SpawnVFX;

	/** VFX on pickup */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
	TSoftObjectPtr<UNiagaraSystem> PickupVFX;

	FSuspenseCoreItemVisuals()
	{
	}
};

/**
 * FSuspenseCoreItemAudio
 *
 * Audio asset references.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreItemAudio
{
	GENERATED_BODY()

	/** Sound on pickup */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<USoundBase> PickupSound;

	/** Sound on drop */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<USoundBase> DropSound;

	/** Sound on use/equip */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<USoundBase> UseSound;

	FSuspenseCoreItemAudio()
	{
	}
};

/**
 * FSuspenseCoreWeaponConfig
 *
 * Weapon-specific configuration.
 * Only valid when item is a weapon.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreWeaponConfig
{
	GENERATED_BODY()

	/** Weapon archetype (Weapon.Archetype.Rifle, etc) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon",
		meta = (Categories = "Weapon.Archetype"))
	FGameplayTag WeaponArchetype;

	/** Compatible ammo type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon",
		meta = (Categories = "Ammo"))
	FGameplayTag AmmoType;

	/** Magazine capacity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon",
		meta = (ClampMin = "1"))
	int32 MagazineSize;

	/** Fire rate (rounds per minute) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon",
		meta = (ClampMin = "1"))
	float FireRate;

	/** Base damage per hit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon",
		meta = (ClampMin = "0"))
	float BaseDamage;

	FSuspenseCoreWeaponConfig()
		: MagazineSize(30)
		, FireRate(600.0f)
		, BaseDamage(25.0f)
	{
	}
};

/**
 * FSuspenseCoreArmorConfig
 *
 * Armor-specific configuration.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreArmorConfig
{
	GENERATED_BODY()

	/** Armor type (Armor.Type.Head, Armor.Type.Body, etc) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor",
		meta = (Categories = "Armor.Type"))
	FGameplayTag ArmorType;

	/** Armor class (1-6 Tarkov style) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor",
		meta = (ClampMin = "1", ClampMax = "6"))
	int32 ArmorClass;

	/** Maximum durability */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor",
		meta = (ClampMin = "1"))
	float MaxDurability;

	FSuspenseCoreArmorConfig()
		: ArmorClass(1)
		, MaxDurability(100.0f)
	{
	}
};

/**
 * FSuspenseCoreAmmoConfig
 *
 * Ammunition-specific configuration.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreAmmoConfig
{
	GENERATED_BODY()

	/** Ammo caliber (Ammo.Caliber.556x45, etc) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo",
		meta = (Categories = "Ammo.Caliber"))
	FGameplayTag AmmoCaliber;

	/** Penetration value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo",
		meta = (ClampMin = "0"))
	float Penetration;

	/** Armor damage multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo",
		meta = (ClampMin = "0"))
	float ArmorDamage;

	/** Flesh damage multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo",
		meta = (ClampMin = "0"))
	float FleshDamage;

	FSuspenseCoreAmmoConfig()
		: Penetration(20.0f)
		, ArmorDamage(1.0f)
		, FleshDamage(1.0f)
	{
	}
};

/**
 * FSuspenseCoreGASConfig
 *
 * GAS integration configuration.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreGASConfig
{
	GENERATED_BODY()

	/** AttributeSet class to instantiate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
	TSubclassOf<UAttributeSet> AttributeSetClass;

	/** Initialization effect to apply on equip */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
	TSubclassOf<UGameplayEffect> InitializationEffect;

	/** Abilities granted on equip */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
	TArray<TSubclassOf<UGameplayAbility>> GrantedAbilities;

	FSuspenseCoreGASConfig()
	{
	}
};

/**
 * FSuspenseCoreItemData
 *
 * Complete item data structure for SuspenseCore DataTable.
 * Combines all sub-structures into a single row type.
 *
 * ARCHITECTURE:
 * - Modular structure with separate concerns
 * - Optional configs for specialized item types
 * - GAS-ready with ability/effect integration
 * - EventBus-compatible
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreItemData : public FTableRowBase
{
	GENERATED_BODY()

	//==================================================================
	// Core Data (Required)
	//==================================================================

	/** Item identity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Core")
	FSuspenseCoreItemIdentity Identity;

	/** Item classification */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Core")
	FSuspenseCoreItemClassification Classification;

	/** Inventory properties */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Core")
	FSuspenseCoreInventoryProperties InventoryProps;

	/** Behavior flags */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Core")
	FSuspenseCoreItemBehavior Behavior;

	//==================================================================
	// Assets
	//==================================================================

	/** Visual assets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assets")
	FSuspenseCoreItemVisuals Visuals;

	/** Audio assets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assets")
	FSuspenseCoreItemAudio Audio;

	//==================================================================
	// Type-Specific Configs (Optional)
	//==================================================================

	/** Is this a weapon? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type Flags")
	bool bIsWeapon;

	/** Weapon configuration (valid when bIsWeapon) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon",
		meta = (EditCondition = "bIsWeapon"))
	FSuspenseCoreWeaponConfig WeaponConfig;

	/** Is this armor? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type Flags")
	bool bIsArmor;

	/** Armor configuration (valid when bIsArmor) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor",
		meta = (EditCondition = "bIsArmor"))
	FSuspenseCoreArmorConfig ArmorConfig;

	/** Is this ammunition? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type Flags")
	bool bIsAmmo;

	/** Ammo configuration (valid when bIsAmmo) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo",
		meta = (EditCondition = "bIsAmmo"))
	FSuspenseCoreAmmoConfig AmmoConfig;

	//==================================================================
	// GAS Integration
	//==================================================================

	/** GAS configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
	FSuspenseCoreGASConfig GASConfig;

	//==================================================================
	// Constructor & Helpers
	//==================================================================

	FSuspenseCoreItemData()
		: bIsWeapon(false)
		, bIsArmor(false)
		, bIsAmmo(false)
	{
	}

	/** Get effective item type considering special flags */
	FGameplayTag GetEffectiveItemType() const
	{
		if (bIsWeapon && WeaponConfig.WeaponArchetype.IsValid())
		{
			return WeaponConfig.WeaponArchetype;
		}
		if (bIsArmor && ArmorConfig.ArmorType.IsValid())
		{
			return ArmorConfig.ArmorType;
		}
		if (bIsAmmo && AmmoConfig.AmmoCaliber.IsValid())
		{
			return AmmoConfig.AmmoCaliber;
		}
		return Classification.ItemType;
	}

	bool IsValid() const { return Identity.IsValid(); }
	bool IsStackable() const { return InventoryProps.IsStackable(); }
};
