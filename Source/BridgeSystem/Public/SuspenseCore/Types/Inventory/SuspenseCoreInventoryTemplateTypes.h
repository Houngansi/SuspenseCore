// SuspenseCoreInventoryTemplateTypes.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "SuspenseCoreInventoryTemplateTypes.generated.h"

// Forward declarations
struct FSuspenseCoreItemInstance;

/**
 * ESuspenseCoreTemplateType
 *
 * Types of inventory templates.
 */
UENUM(BlueprintType)
enum class ESuspenseCoreTemplateType : uint8
{
	/** Empty inventory */
	Empty = 0				UMETA(DisplayName = "Empty"),
	/** Predefined loadout */
	Loadout					UMETA(DisplayName = "Loadout"),
	/** Loot table (random) */
	LootTable				UMETA(DisplayName = "Loot Table"),
	/** Container preset */
	Container				UMETA(DisplayName = "Container"),
	/** Vendor inventory */
	Vendor					UMETA(DisplayName = "Vendor"),
	/** Quest rewards */
	QuestReward				UMETA(DisplayName = "Quest Reward")
};

/**
 * FSuspenseCoreTemplateItem
 *
 * Single item entry in a template.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreTemplateItem
{
	GENERATED_BODY()

	/** Item ID from DataTable */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	FName ItemID;

	/** Quantity to add */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template",
		meta = (ClampMin = "1"))
	int32 Quantity;

	/** Preferred slot (-1 for auto) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	int32 PreferredSlot;

	/** Spawn chance (0-1) for loot tables */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpawnChance;

	/** Min quantity for random range */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template",
		meta = (ClampMin = "0"))
	int32 MinQuantity;

	/** Max quantity for random range (0 = use Quantity) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template",
		meta = (ClampMin = "0"))
	int32 MaxQuantity;

	/** Initial durability (0-1, 0 = default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InitialDurability;

	/** Spawn with full ammo if weapon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	bool bFullAmmo;

	FSuspenseCoreTemplateItem()
		: ItemID(NAME_None)
		, Quantity(1)
		, PreferredSlot(-1)
		, SpawnChance(1.0f)
		, MinQuantity(0)
		, MaxQuantity(0)
		, InitialDurability(0.0f)
		, bFullAmmo(true)
	{
	}

	FSuspenseCoreTemplateItem(FName InItemID, int32 InQuantity = 1)
		: ItemID(InItemID)
		, Quantity(InQuantity)
		, PreferredSlot(-1)
		, SpawnChance(1.0f)
		, MinQuantity(0)
		, MaxQuantity(0)
		, InitialDurability(0.0f)
		, bFullAmmo(true)
	{
	}

	bool IsValid() const { return !ItemID.IsNone() && Quantity > 0; }

	/** Get random quantity within range */
	int32 GetRandomQuantity() const
	{
		if (MaxQuantity > MinQuantity)
		{
			return FMath::RandRange(MinQuantity, MaxQuantity);
		}
		return Quantity;
	}

	/** Check spawn chance */
	bool ShouldSpawn() const
	{
		return FMath::FRand() <= SpawnChance;
	}
};

/**
 * FSuspenseCoreInventoryTemplate
 *
 * Template for initializing inventories with predefined items.
 * Can be used for loadouts, loot tables, containers, etc.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreInventoryTemplate : public FTableRowBase
{
	GENERATED_BODY()

	/** Template identifier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	FName TemplateID;

	/** Display name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	FText DisplayName;

	/** Template type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	ESuspenseCoreTemplateType TemplateType;

	/** Items in this template */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template",
		meta = (TitleProperty = "ItemID"))
	TArray<FSuspenseCoreTemplateItem> Items;

	/** Grid size override (0,0 = use default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	FIntPoint GridSizeOverride;

	/** Max weight override (0 = use default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	float MaxWeightOverride;

	/** Tags for filtering templates */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Template")
	FGameplayTagContainer TemplateTags;

	/** Min items to spawn from loot table */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LootTable",
		meta = (EditCondition = "TemplateType == ESuspenseCoreTemplateType::LootTable"))
	int32 MinLootItems;

	/** Max items to spawn from loot table */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LootTable",
		meta = (EditCondition = "TemplateType == ESuspenseCoreTemplateType::LootTable"))
	int32 MaxLootItems;

	FSuspenseCoreInventoryTemplate()
		: TemplateID(NAME_None)
		, TemplateType(ESuspenseCoreTemplateType::Empty)
		, GridSizeOverride(FIntPoint::ZeroValue)
		, MaxWeightOverride(0.0f)
		, MinLootItems(1)
		, MaxLootItems(5)
	{
	}

	bool IsValid() const { return !TemplateID.IsNone(); }

	bool HasGridOverride() const
	{
		return GridSizeOverride.X > 0 && GridSizeOverride.Y > 0;
	}

	bool HasWeightOverride() const
	{
		return MaxWeightOverride > 0.0f;
	}

	/** Get random loot item count */
	int32 GetRandomLootCount() const
	{
		return FMath::RandRange(MinLootItems, MaxLootItems);
	}
};

/**
 * FSuspenseCoreTemplateLoadoutSlot
 *
 * Equipment slot in an inventory template loadout.
 * Note: Different from FSuspenseCoreLoadoutSlot in SuspenseCorePlayerData.h
 * which is used for player save data.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreTemplateLoadoutSlot
{
	GENERATED_BODY()

	/** Slot name (e.g., "PrimaryWeapon", "Helmet") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FName SlotName;

	/** Item to equip in this slot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FName ItemID;

	/** Attachments for this item */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	TArray<FName> AttachmentIDs;

	FSuspenseCoreTemplateLoadoutSlot()
		: SlotName(NAME_None)
		, ItemID(NAME_None)
	{
	}

	bool IsValid() const { return !SlotName.IsNone() && !ItemID.IsNone(); }
};

/**
 * FSuspenseCoreTemplateLoadout
 *
 * Complete loadout definition for inventory templates.
 * Note: Different from FSuspenseCoreLoadout in SuspenseCorePlayerData.h
 * which is used for player save data.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreTemplateLoadout : public FTableRowBase
{
	GENERATED_BODY()

	/** Loadout identifier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FName LoadoutID;

	/** Display name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FText DisplayName;

	/** Character class this loadout is for */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout",
		meta = (Categories = "SuspenseCore.Class"))
	FGameplayTag CharacterClass;

	/** Equipment slots */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout",
		meta = (TitleProperty = "SlotName"))
	TArray<FSuspenseCoreTemplateLoadoutSlot> EquipmentSlots;

	/** Inventory template to apply (optional) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FName InventoryTemplateID;

	//========================================================================
	// Inventory Configuration (Direct - Single Source of Truth)
	//========================================================================

	/** Inventory grid width */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory",
		meta = (ClampMin = "1", ClampMax = "20"))
	int32 InventoryWidth = 10;

	/** Inventory grid height */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory",
		meta = (ClampMin = "1", ClampMax = "20"))
	int32 InventoryHeight = 6;

	/** Maximum inventory weight */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory",
		meta = (ClampMin = "0.0"))
	float MaxWeight = 50.0f;

	/** Is default loadout for class */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	bool bIsDefault;

	/** Tags for filtering loadouts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FGameplayTagContainer LoadoutTags;

	FSuspenseCoreTemplateLoadout()
		: LoadoutID(NAME_None)
		, InventoryTemplateID(NAME_None)
		, InventoryWidth(10)
		, InventoryHeight(6)
		, MaxWeight(50.0f)
		, bIsDefault(false)
	{
	}

	bool IsValid() const { return !LoadoutID.IsNone(); }
};
