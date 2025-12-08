// SuspenseCoreItemTypes.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "AttributeSet.h"
#include "SuspenseCoreItemTypes.generated.h"

// Forward declarations
class UStaticMesh;
class UNiagaraSystem;
class USoundBase;
class UTexture2D;
class UGameplayEffect;
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

/**
 * FSuspenseCoreRuntimeProperty
 *
 * Key-value pair for runtime item properties.
 * Replicated as TArray since TMap doesn't support network replication.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreRuntimeProperty
{
	GENERATED_BODY()

	/** Property name (e.g., "Durability", "Charge", "Temperature") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime")
	FName PropertyName;

	/** Property value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime")
	float Value;

	FSuspenseCoreRuntimeProperty()
		: PropertyName(NAME_None)
		, Value(0.0f)
	{
	}

	FSuspenseCoreRuntimeProperty(FName InName, float InValue)
		: PropertyName(InName)
		, Value(InValue)
	{
	}

	bool operator==(const FSuspenseCoreRuntimeProperty& Other) const
	{
		return PropertyName == Other.PropertyName;
	}
};

/**
 * FSuspenseCoreWeaponState
 *
 * Weapon-specific runtime state.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreWeaponState
{
	GENERATED_BODY()

	/** Whether weapon state is set */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	bool bHasState;

	/** Current ammo in magazine */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon",
		meta = (EditCondition = "bHasState"))
	float CurrentAmmo;

	/** Reserve ammo */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon",
		meta = (EditCondition = "bHasState"))
	float ReserveAmmo;

	/** Current fire mode index */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon",
		meta = (EditCondition = "bHasState"))
	int32 FireModeIndex;

	FSuspenseCoreWeaponState()
		: bHasState(false)
		, CurrentAmmo(0.0f)
		, ReserveAmmo(0.0f)
		, FireModeIndex(0)
	{
	}

	void SetAmmoState(float InCurrent, float InReserve)
	{
		bHasState = true;
		CurrentAmmo = InCurrent;
		ReserveAmmo = InReserve;
	}

	void Clear()
	{
		bHasState = false;
		CurrentAmmo = 0.0f;
		ReserveAmmo = 0.0f;
		FireModeIndex = 0;
	}
};

/**
 * FSuspenseCoreItemInstance
 *
 * Runtime item instance for SuspenseCore.
 * Contains all runtime state for an item in inventory/world.
 *
 * ARCHITECTURE:
 * - ItemID references static data in FSuspenseCoreItemData (DataTable)
 * - RuntimeProperties stores dynamic state (durability, modifications)
 * - WeaponState stores weapon-specific runtime data
 * - Supports network replication via TArray instead of TMap
 * - UniqueInstanceID for tracking across save/load
 *
 * USAGE:
 * - Created by SuspenseCoreDataManager::CreateItemInstance()
 * - Used in inventory, equipment, and pickup systems
 * - Broadcasts SuspenseCore.Event.Item.* events
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreItemInstance
{
	GENERATED_BODY()

	//==================================================================
	// Identification
	//==================================================================

	/** Unique runtime instance ID (for tracking across save/load) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance")
	FGuid UniqueInstanceID;

	/** Item ID for DataTable lookup */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instance")
	FName ItemID;

	//==================================================================
	// Stack Data
	//==================================================================

	/** Current quantity in stack */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stack",
		meta = (ClampMin = "1"))
	int32 Quantity;

	//==================================================================
	// Runtime State
	//==================================================================

	/** Runtime properties (durability, charge, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime",
		meta = (TitleProperty = "PropertyName"))
	TArray<FSuspenseCoreRuntimeProperty> RuntimeProperties;

	/** Weapon-specific state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FSuspenseCoreWeaponState WeaponState;

	//==================================================================
	// Inventory Position (Optional)
	//==================================================================

	/** Slot index in inventory (-1 = not in inventory) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	int32 SlotIndex;

	/** Grid position for grid-based inventory */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	FIntPoint GridPosition;

	/** Rotation state for grid inventory (0, 90, 180, 270) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	int32 Rotation;

	//==================================================================
	// Constructor
	//==================================================================

	FSuspenseCoreItemInstance()
		: UniqueInstanceID(FGuid())
		, ItemID(NAME_None)
		, Quantity(1)
		, SlotIndex(-1)
		, GridPosition(FIntPoint::NoneValue)
		, Rotation(0)
	{
	}

	FSuspenseCoreItemInstance(FName InItemID, int32 InQuantity = 1)
		: UniqueInstanceID(FGuid::NewGuid())
		, ItemID(InItemID)
		, Quantity(InQuantity)
		, SlotIndex(-1)
		, GridPosition(FIntPoint::NoneValue)
		, Rotation(0)
	{
	}

	//==================================================================
	// Validation
	//==================================================================

	bool IsValid() const { return !ItemID.IsNone() && Quantity > 0; }

	//==================================================================
	// Runtime Property Helpers
	//==================================================================

	/** Get runtime property value */
	float GetProperty(FName PropertyName, float DefaultValue = 0.0f) const
	{
		for (const FSuspenseCoreRuntimeProperty& Prop : RuntimeProperties)
		{
			if (Prop.PropertyName == PropertyName)
			{
				return Prop.Value;
			}
		}
		return DefaultValue;
	}

	/** Set runtime property value */
	void SetProperty(FName PropertyName, float Value)
	{
		for (FSuspenseCoreRuntimeProperty& Prop : RuntimeProperties)
		{
			if (Prop.PropertyName == PropertyName)
			{
				Prop.Value = Value;
				return;
			}
		}
		RuntimeProperties.Add(FSuspenseCoreRuntimeProperty(PropertyName, Value));
	}

	/** Check if property exists */
	bool HasProperty(FName PropertyName) const
	{
		for (const FSuspenseCoreRuntimeProperty& Prop : RuntimeProperties)
		{
			if (Prop.PropertyName == PropertyName)
			{
				return true;
			}
		}
		return false;
	}

	/** Remove property */
	bool RemoveProperty(FName PropertyName)
	{
		return RuntimeProperties.RemoveAll([PropertyName](const FSuspenseCoreRuntimeProperty& Prop)
		{
			return Prop.PropertyName == PropertyName;
		}) > 0;
	}

	/** Convert properties to TMap (for convenience, not for replication) */
	TMap<FName, float> GetPropertiesAsMap() const
	{
		TMap<FName, float> Result;
		for (const FSuspenseCoreRuntimeProperty& Prop : RuntimeProperties)
		{
			Result.Add(Prop.PropertyName, Prop.Value);
		}
		return Result;
	}

	/** Set properties from TMap */
	void SetPropertiesFromMap(const TMap<FName, float>& Properties)
	{
		RuntimeProperties.Empty();
		for (const auto& Pair : Properties)
		{
			RuntimeProperties.Add(FSuspenseCoreRuntimeProperty(Pair.Key, Pair.Value));
		}
	}

	//==================================================================
	// Comparison
	//==================================================================

	bool operator==(const FSuspenseCoreItemInstance& Other) const
	{
		return UniqueInstanceID == Other.UniqueInstanceID;
	}

	bool operator!=(const FSuspenseCoreItemInstance& Other) const
	{
		return !(*this == Other);
	}

	/** Check if can stack with another instance (same ItemID, no unique properties) */
	bool CanStackWith(const FSuspenseCoreItemInstance& Other) const
	{
		// Same item type
		if (ItemID != Other.ItemID)
		{
			return false;
		}

		// Weapons don't stack (they have unique state)
		if (WeaponState.bHasState || Other.WeaponState.bHasState)
		{
			return false;
		}

		// Items with runtime properties don't stack
		if (RuntimeProperties.Num() > 0 || Other.RuntimeProperties.Num() > 0)
		{
			return false;
		}

		return true;
	}
};

