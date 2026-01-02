// SuspenseCoreDataManager.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Settings/SuspenseCoreSettings.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreItemDataTable.h"
#include "Engine/DataTable.h"
#include "Engine/DataAsset.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreData, Log, All);

//========================================================================
// Static Access
//========================================================================

USuspenseCoreDataManager* USuspenseCoreDataManager::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	const UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		return nullptr;
	}

	return GameInstance->GetSubsystem<USuspenseCoreDataManager>();
}

//========================================================================
// USubsystem Interface
//========================================================================

bool USuspenseCoreDataManager::ShouldCreateSubsystem(UObject* Outer) const
{
	// Always create - this is the core data system
	return true;
}

void USuspenseCoreDataManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogSuspenseCoreData, Log, TEXT("═══════════════════════════════════════════════════════════════"));
	UE_LOG(LogSuspenseCoreData, Log, TEXT("  SUSPENSECORE DATA MANAGER - INITIALIZATION START"));
	UE_LOG(LogSuspenseCoreData, Log, TEXT("═══════════════════════════════════════════════════════════════"));

	// Get settings
	const USuspenseCoreSettings* Settings = USuspenseCoreSettings::Get();
	if (!Settings)
	{
		UE_LOG(LogSuspenseCoreData, Error, TEXT("SuspenseCoreSettings not found! Data Manager cannot initialize."));
		return;
	}

	// Validate settings
	TArray<FString> ConfigErrors;
	if (!Settings->ValidateConfiguration(ConfigErrors))
	{
		UE_LOG(LogSuspenseCoreData, Warning, TEXT("Settings validation found issues:"));
		for (const FString& Error : ConfigErrors)
		{
			UE_LOG(LogSuspenseCoreData, Warning, TEXT("  - %s"), *Error);
		}
	}

	//========================================================================
	// Initialize Systems
	//========================================================================

	// Item System (Primary - most critical)
	bItemSystemReady = InitializeItemSystem();
	if (!bItemSystemReady)
	{
		UE_LOG(LogSuspenseCoreData, Error, TEXT("Item System initialization FAILED!"));
	}
	else
	{
		UE_LOG(LogSuspenseCoreData, Log, TEXT("Item System: READY (%d items cached)"), ItemCache.Num());
	}

	// Character System
	bCharacterSystemReady = InitializeCharacterSystem();
	if (!bCharacterSystemReady)
	{
		UE_LOG(LogSuspenseCoreData, Warning, TEXT("Character System initialization failed (may be optional)"));
	}
	else
	{
		UE_LOG(LogSuspenseCoreData, Log, TEXT("Character System: READY"));
	}

	// Loadout System
	bLoadoutSystemReady = InitializeLoadoutSystem();
	if (!bLoadoutSystemReady)
	{
		UE_LOG(LogSuspenseCoreData, Warning, TEXT("Loadout System initialization failed (may be optional)"));
	}
	else
	{
		UE_LOG(LogSuspenseCoreData, Log, TEXT("Loadout System: READY"));
	}

	//========================================================================
	// GAS Attributes Systems (SSOT)
	//========================================================================

	if (Settings->bUseSSOTAttributes)
	{
		// Weapon Attributes
		bWeaponAttributesSystemReady = InitializeWeaponAttributesSystem();
		if (!bWeaponAttributesSystemReady)
		{
			UE_LOG(LogSuspenseCoreData, Warning, TEXT("Weapon Attributes System initialization failed (may be optional)"));
		}
		else
		{
			UE_LOG(LogSuspenseCoreData, Log, TEXT("Weapon Attributes System: READY (%d rows cached)"), WeaponAttributesCache.Num());
		}

		// Ammo Attributes
		bAmmoAttributesSystemReady = InitializeAmmoAttributesSystem();
		if (!bAmmoAttributesSystemReady)
		{
			UE_LOG(LogSuspenseCoreData, Warning, TEXT("Ammo Attributes System initialization failed (may be optional)"));
		}
		else
		{
			UE_LOG(LogSuspenseCoreData, Log, TEXT("Ammo Attributes System: READY (%d rows cached)"), AmmoAttributesCache.Num());
		}

		// Armor Attributes (future)
		bArmorAttributesSystemReady = InitializeArmorAttributesSystem();
		if (bArmorAttributesSystemReady)
		{
			UE_LOG(LogSuspenseCoreData, Log, TEXT("Armor Attributes System: READY (%d rows cached)"), ArmorAttributesCache.Num());
		}
	}
	else
	{
		UE_LOG(LogSuspenseCoreData, Log, TEXT("SSOT Attributes disabled - using legacy AttributeSet initialization"));
	}

	//========================================================================
	// Validation (if enabled)
	//========================================================================

	if (Settings->bValidateItemsOnStartup && bItemSystemReady)
	{
		UE_LOG(LogSuspenseCoreData, Log, TEXT("Running item validation..."));

		TArray<FString> ValidationErrors;
		int32 ErrorCount = ValidateAllItems(ValidationErrors);

		if (ErrorCount > 0)
		{
			UE_LOG(LogSuspenseCoreData, Warning, TEXT("Validation found %d items with errors:"), ErrorCount);
			for (const FString& Error : ValidationErrors)
			{
				UE_LOG(LogSuspenseCoreData, Warning, TEXT("  - %s"), *Error);
			}

			if (Settings->bStrictItemValidation)
			{
				UE_LOG(LogSuspenseCoreData, Error, TEXT("STRICT VALIDATION ENABLED - Critical items have errors!"));
				BroadcastValidationResult(false, ValidationErrors);
			}
			else
			{
				BroadcastValidationResult(true, ValidationErrors);
			}
		}
		else
		{
			UE_LOG(LogSuspenseCoreData, Log, TEXT("All items validated successfully"));
			BroadcastValidationResult(true, TArray<FString>());
		}
	}

	//========================================================================
	// Complete Initialization
	//========================================================================

	bIsInitialized = bItemSystemReady; // At minimum, items must work

	if (bIsInitialized)
	{
		BroadcastInitialized();
	}

	UE_LOG(LogSuspenseCoreData, Log, TEXT("═══════════════════════════════════════════════════════════════"));
	UE_LOG(LogSuspenseCoreData, Log, TEXT("  SUSPENSECORE DATA MANAGER - INITIALIZATION %s"),
		bIsInitialized ? TEXT("COMPLETE") : TEXT("FAILED"));
	UE_LOG(LogSuspenseCoreData, Log, TEXT("═══════════════════════════════════════════════════════════════"));
}

void USuspenseCoreDataManager::Deinitialize()
{
	UE_LOG(LogSuspenseCoreData, Log, TEXT("SuspenseCoreDataManager shutting down..."));

	// Clear item caches
	ItemCache.Empty();
	UnifiedItemCache.Empty();
	LoadedItemDataTable = nullptr;
	LoadedCharacterClassesDataAsset = nullptr;
	LoadedLoadoutDataTable = nullptr;

	// Clear GAS attribute caches (SSOT)
	WeaponAttributesCache.Empty();
	AmmoAttributesCache.Empty();
	ArmorAttributesCache.Empty();
	LoadedWeaponAttributesDataTable = nullptr;
	LoadedAmmoAttributesDataTable = nullptr;
	LoadedArmorAttributesDataTable = nullptr;

	CachedEventBus.Reset();

	// Reset all flags
	bIsInitialized = false;
	bItemSystemReady = false;
	bCharacterSystemReady = false;
	bLoadoutSystemReady = false;
	bWeaponAttributesSystemReady = false;
	bAmmoAttributesSystemReady = false;
	bArmorAttributesSystemReady = false;

	Super::Deinitialize();
}

//========================================================================
// Initialization Helpers
//========================================================================

bool USuspenseCoreDataManager::InitializeItemSystem()
{
	const USuspenseCoreSettings* Settings = USuspenseCoreSettings::Get();
	if (!Settings)
	{
		return false;
	}

	// Check if ItemDataTable is configured
	if (Settings->ItemDataTable.IsNull())
	{
		UE_LOG(LogSuspenseCoreData, Error,
			TEXT("ItemDataTable not configured in Project Settings → Game → SuspenseCore!"));
		UE_LOG(LogSuspenseCoreData, Error,
			TEXT("Please configure ItemDataTable to enable the item system."));
		return false;
	}

	// Load the DataTable
	UE_LOG(LogSuspenseCoreData, Log, TEXT("Loading ItemDataTable: %s"),
		*Settings->ItemDataTable.ToString());

	LoadedItemDataTable = Settings->ItemDataTable.LoadSynchronous();

	if (!LoadedItemDataTable)
	{
		UE_LOG(LogSuspenseCoreData, Error, TEXT("Failed to load ItemDataTable!"));
		return false;
	}

	UE_LOG(LogSuspenseCoreData, Log, TEXT("ItemDataTable loaded: %s"),
		*LoadedItemDataTable->GetName());

	// Verify row structure
	const UScriptStruct* RowStruct = LoadedItemDataTable->GetRowStruct();
	if (!RowStruct)
	{
		UE_LOG(LogSuspenseCoreData, Error, TEXT("ItemDataTable has no row structure!"));
		return false;
	}

	UE_LOG(LogSuspenseCoreData, Log, TEXT("Row Structure: %s"), *RowStruct->GetName());

	// Build unified item cache (SSOT for all item data)
	if (!BuildItemCache(LoadedItemDataTable))
	{
		UE_LOG(LogSuspenseCoreData, Error, TEXT("Failed to build item cache!"));
		return false;
	}

	// DataManager is now the Single Source of Truth (SSOT) for all item data
	// All systems (VisualizationService, ActorFactory, etc.) should use
	// DataManager::GetUnifiedItemData() instead of legacy ItemManager

	return true;
}

bool USuspenseCoreDataManager::BuildItemCache(UDataTable* DataTable)
{
	if (!DataTable)
	{
		return false;
	}

	// Clear both caches
	UnifiedItemCache.Empty();
	ItemCache.Empty();

	const USuspenseCoreSettings* Settings = USuspenseCoreSettings::Get();
	const bool bVerbose = Settings && Settings->bLogItemOperations;

	// Get all row names
	TArray<FName> RowNames = DataTable->GetRowNames();
	UE_LOG(LogSuspenseCoreData, Log, TEXT("Building cache from %d rows..."), RowNames.Num());

	// Detect row struct type
	const UScriptStruct* RowStruct = DataTable->GetRowStruct();
	const bool bIsUnifiedType = RowStruct && RowStruct->GetName().Contains(TEXT("UnifiedItemData"));

	UE_LOG(LogSuspenseCoreData, Log, TEXT("DataTable row struct: %s (Unified: %s)"),
		RowStruct ? *RowStruct->GetName() : TEXT("None"),
		bIsUnifiedType ? TEXT("Yes") : TEXT("No"));

	if (!bIsUnifiedType)
	{
		UE_LOG(LogSuspenseCoreData, Error,
			TEXT("DataTable must use FSuspenseCoreUnifiedItemData row structure!"));
		UE_LOG(LogSuspenseCoreData, Error,
			TEXT("Equipment system requires full item data including EquipmentActorClass, sockets, etc."));
		return false;
	}

	int32 LoadedCount = 0;
	int32 FailedCount = 0;
	int32 WeaponCount = 0;
	int32 ArmorCount = 0;
	int32 EquippableCount = 0;

	for (const FName& RowName : RowNames)
	{
		FSuspenseCoreUnifiedItemData* UnifiedData = DataTable->FindRow<FSuspenseCoreUnifiedItemData>(RowName, TEXT(""));
		if (!UnifiedData)
		{
			FailedCount++;
			UE_LOG(LogSuspenseCoreData, Warning, TEXT("  Failed to read row: %s"), *RowName.ToString());
			continue;
		}

		// Ensure ItemID is set
		if (UnifiedData->ItemID.IsNone())
		{
			UnifiedData->ItemID = RowName;
		}

		// Store in PRIMARY cache (UnifiedItemCache) - SSOT
		UnifiedItemCache.Add(RowName, *UnifiedData);

		// Store in SECONDARY cache (ItemCache) for legacy access
		FSuspenseCoreItemData SimplifiedData = ConvertUnifiedToItemData(*UnifiedData, RowName);
		ItemCache.Add(RowName, SimplifiedData);

		LoadedCount++;

		// Track statistics
		if (UnifiedData->bIsWeapon) WeaponCount++;
		if (UnifiedData->bIsArmor) ArmorCount++;
		if (UnifiedData->bIsEquippable) EquippableCount++;

		if (bVerbose)
		{
			UE_LOG(LogSuspenseCoreData, Verbose, TEXT("  Cached: %s (%s) [Equippable=%s, ActorClass=%s]"),
				*RowName.ToString(),
				*UnifiedData->DisplayName.ToString(),
				UnifiedData->bIsEquippable ? TEXT("Yes") : TEXT("No"),
				UnifiedData->EquipmentActorClass.IsNull() ? TEXT("None") : TEXT("Set"));
		}
	}

	UE_LOG(LogSuspenseCoreData, Log, TEXT("═══════════════════════════════════════════════════════════════"));
	UE_LOG(LogSuspenseCoreData, Log, TEXT("  ITEM CACHE BUILT"));
	UE_LOG(LogSuspenseCoreData, Log, TEXT("  Total: %d items (Failed: %d)"), LoadedCount, FailedCount);
	UE_LOG(LogSuspenseCoreData, Log, TEXT("  Weapons: %d, Armor: %d, Equippable: %d"), WeaponCount, ArmorCount, EquippableCount);
	UE_LOG(LogSuspenseCoreData, Log, TEXT("═══════════════════════════════════════════════════════════════"));

	return LoadedCount > 0;
}

FSuspenseCoreItemData USuspenseCoreDataManager::ConvertUnifiedToItemData(
	const FSuspenseCoreUnifiedItemData& Unified, FName RowName)
{
	FSuspenseCoreItemData Result;

	// Core Identity
	Result.Identity.ItemID = Unified.ItemID.IsNone() ? RowName : Unified.ItemID;
	Result.Identity.DisplayName = Unified.DisplayName;
	Result.Identity.Description = Unified.Description;
	Result.Identity.Icon = Unified.Icon;

	// Classification
	Result.Classification.ItemType = Unified.ItemType;
	Result.Classification.Rarity = Unified.Rarity;
	Result.Classification.ItemTags = Unified.ItemTags;

	// Debug: Log the tags being parsed from JSON
	UE_LOG(LogSuspenseCoreData, Log, TEXT("ConvertUnified[%s]: ItemType=%s, EquipSlot=%s, IsValid=%s"),
		*RowName.ToString(),
		*Unified.ItemType.ToString(),
		*Unified.EquipmentSlot.ToString(),
		Unified.ItemType.IsValid() ? TEXT("Yes") : TEXT("No"));

	// Inventory Properties
	Result.InventoryProps.GridSize = Unified.GridSize;
	Result.InventoryProps.MaxStackSize = Unified.MaxStackSize;
	Result.InventoryProps.Weight = Unified.Weight;
	Result.InventoryProps.BaseValue = Unified.BaseValue;

	// Behavior Flags
	Result.Behavior.bIsEquippable = Unified.bIsEquippable;
	Result.Behavior.bIsConsumable = Unified.bIsConsumable;
	Result.Behavior.bCanDrop = Unified.bCanDrop;
	Result.Behavior.bCanTrade = Unified.bCanTrade;
	Result.Behavior.bIsQuestItem = Unified.bIsQuestItem;

	// Visual Assets
	Result.Visuals.WorldMesh = Unified.WorldMesh;
	Result.Visuals.SpawnVFX = Unified.PickupSpawnVFX;
	Result.Visuals.PickupVFX = Unified.PickupCollectVFX;

	// Audio Assets
	Result.Audio.PickupSound = Unified.PickupSound;
	Result.Audio.DropSound = Unified.DropSound;
	Result.Audio.UseSound = Unified.UseSound;

	// Weapon Config
	Result.bIsWeapon = Unified.bIsWeapon;
	if (Unified.bIsWeapon)
	{
		Result.WeaponConfig.WeaponArchetype = Unified.WeaponArchetype;
		Result.WeaponConfig.AmmoType = Unified.AmmoType;
		// WeaponConfig.MagazineSize, FireRate, BaseDamage come from AttributeSet
	}

	// Armor Config
	Result.bIsArmor = Unified.bIsArmor;
	if (Unified.bIsArmor)
	{
		Result.ArmorConfig.ArmorType = Unified.ArmorType;
	}

	// Ammo Config
	Result.bIsAmmo = Unified.bIsAmmo;
	if (Unified.bIsAmmo)
	{
		Result.AmmoConfig.AmmoCaliber = Unified.AmmoCaliber;
	}

	// GAS Config
	if (Unified.bIsWeapon)
	{
		Result.GASConfig.AttributeSetClass = Unified.WeaponInitialization.WeaponAttributeSetClass;
		Result.GASConfig.InitializationEffect = Unified.WeaponInitialization.WeaponInitEffect;
	}
	else if (Unified.bIsArmor)
	{
		Result.GASConfig.AttributeSetClass = Unified.ArmorInitialization.ArmorAttributeSetClass;
		Result.GASConfig.InitializationEffect = Unified.ArmorInitialization.ArmorInitEffect;
	}
	else if (Unified.bIsEquippable)
	{
		Result.GASConfig.AttributeSetClass = Unified.EquipmentAttributeSet;
		Result.GASConfig.InitializationEffect = Unified.EquipmentInitEffect;
	}

	// Granted abilities (simplified conversion)
	for (const FGrantedAbilityData& AbilityData : Unified.GrantedAbilities)
	{
		if (AbilityData.AbilityClass)
		{
			Result.GASConfig.GrantedAbilities.Add(AbilityData.AbilityClass);
		}
	}

	return Result;
}

bool USuspenseCoreDataManager::InitializeCharacterSystem()
{
	const USuspenseCoreSettings* Settings = USuspenseCoreSettings::Get();
	if (!Settings)
	{
		return false;
	}

	if (Settings->CharacterClassesDataAsset.IsNull())
	{
		UE_LOG(LogSuspenseCoreData, Verbose, TEXT("CharacterClassesDataAsset not configured (optional)"));
		return false;
	}

	LoadedCharacterClassesDataAsset = Settings->CharacterClassesDataAsset.LoadSynchronous();

	if (!LoadedCharacterClassesDataAsset)
	{
		UE_LOG(LogSuspenseCoreData, Warning, TEXT("Failed to load CharacterClassesDataAsset"));
		return false;
	}

	UE_LOG(LogSuspenseCoreData, Log, TEXT("CharacterClassesDataAsset loaded: %s"),
		*LoadedCharacterClassesDataAsset->GetName());

	return true;
}

bool USuspenseCoreDataManager::InitializeLoadoutSystem()
{
	const USuspenseCoreSettings* Settings = USuspenseCoreSettings::Get();
	if (!Settings)
	{
		return false;
	}

	if (Settings->LoadoutDataTable.IsNull())
	{
		UE_LOG(LogSuspenseCoreData, Verbose, TEXT("LoadoutDataTable not configured (optional)"));
		return false;
	}

	LoadedLoadoutDataTable = Settings->LoadoutDataTable.LoadSynchronous();

	if (!LoadedLoadoutDataTable)
	{
		UE_LOG(LogSuspenseCoreData, Warning, TEXT("Failed to load LoadoutDataTable"));
		return false;
	}

	UE_LOG(LogSuspenseCoreData, Log, TEXT("LoadoutDataTable loaded: %s"),
		*LoadedLoadoutDataTable->GetName());

	return true;
}

//========================================================================
// Item Data Access
//========================================================================

bool USuspenseCoreDataManager::GetItemData(FName ItemID, FSuspenseCoreItemData& OutItemData) const
{
	if (ItemID.IsNone())
	{
		return false;
	}

	const FSuspenseCoreItemData* Found = ItemCache.Find(ItemID);

	if (Found)
	{
		OutItemData = *Found;
		BroadcastItemLoaded(ItemID, OutItemData);
		return true;
	}

	BroadcastItemNotFound(ItemID);
	return false;
}

bool USuspenseCoreDataManager::GetUnifiedItemData(FName ItemID, FSuspenseCoreUnifiedItemData& OutItemData) const
{
	if (ItemID.IsNone())
	{
		return false;
	}

	const FSuspenseCoreUnifiedItemData* Found = UnifiedItemCache.Find(ItemID);

	if (Found)
	{
		OutItemData = *Found;
		return true;
	}

	UE_LOG(LogSuspenseCoreData, Warning, TEXT("GetUnifiedItemData: Item '%s' not found in cache"), *ItemID.ToString());
	return false;
}

bool USuspenseCoreDataManager::HasItem(FName ItemID) const
{
	return UnifiedItemCache.Contains(ItemID);
}

TArray<FName> USuspenseCoreDataManager::GetAllItemIDs() const
{
	TArray<FName> ItemIDs;
	UnifiedItemCache.GetKeys(ItemIDs);
	return ItemIDs;
}

//========================================================================
// Item Instance Creation
//========================================================================

bool USuspenseCoreDataManager::CreateItemInstance(FName ItemID, int32 Quantity, FSuspenseCoreItemInstance& OutInstance) const
{
	// Get item data
	FSuspenseCoreItemData ItemData;
	if (!GetItemData(ItemID, ItemData))
	{
		UE_LOG(LogSuspenseCoreData, Warning, TEXT("CreateItemInstance: Item '%s' not found"), *ItemID.ToString());
		return false;
	}

	// Create instance
	OutInstance = FSuspenseCoreItemInstance(ItemID, FMath::Max(1, Quantity));

	// Initialize default runtime properties based on item type
	if (ItemData.bIsWeapon)
	{
		// Initialize weapon state with full ammo
		OutInstance.WeaponState.bHasState = true;
		OutInstance.WeaponState.CurrentAmmo = ItemData.WeaponConfig.MagazineSize;
		OutInstance.WeaponState.ReserveAmmo = 0.0f; // No reserve by default
		OutInstance.WeaponState.FireModeIndex = 0;
	}

	if (ItemData.bIsArmor)
	{
		// Initialize durability
		OutInstance.SetProperty(FName("Durability"), ItemData.ArmorConfig.MaxDurability);
		OutInstance.SetProperty(FName("MaxDurability"), ItemData.ArmorConfig.MaxDurability);
	}

	// Broadcast event
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (EventBus)
	{
		FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(
			const_cast<USuspenseCoreDataManager*>(this),
			ESuspenseCoreEventPriority::Normal
		);

		EventData.SetString(TEXT("ItemID"), ItemID.ToString());
		EventData.SetInt(TEXT("Quantity"), Quantity);
		EventData.SetString(TEXT("InstanceID"), OutInstance.UniqueInstanceID.ToString());

		static const FGameplayTag InstanceCreatedTag =
			FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Item.InstanceCreated"));

		EventBus->Publish(InstanceCreatedTag, EventData);
	}

	UE_LOG(LogSuspenseCoreData, Verbose, TEXT("CreateItemInstance: Created %s x%d (ID: %s)"),
		*ItemID.ToString(), Quantity, *OutInstance.UniqueInstanceID.ToString());

	return true;
}

//========================================================================
// Item Validation
//========================================================================

bool USuspenseCoreDataManager::ValidateItem(FName ItemID, TArray<FString>& OutErrors) const
{
	OutErrors.Empty();

	const FSuspenseCoreItemData* ItemData = ItemCache.Find(ItemID);
	if (!ItemData)
	{
		OutErrors.Add(FString::Printf(TEXT("Item '%s' not found in cache"), *ItemID.ToString()));
		return false;
	}

	bool bIsValid = true;

	// Validate display name
	if (ItemData->Identity.DisplayName.IsEmpty())
	{
		OutErrors.Add(FString::Printf(TEXT("[%s] DisplayName is empty"), *ItemID.ToString()));
		bIsValid = false;
	}

	// Validate item type
	if (!ItemData->Classification.ItemType.IsValid())
	{
		OutErrors.Add(FString::Printf(TEXT("[%s] ItemType tag is invalid"), *ItemID.ToString()));
		bIsValid = false;
	}
	else
	{
		// Must be in Item.* hierarchy
		static const FGameplayTag BaseItemTag = FGameplayTag::RequestGameplayTag(TEXT("Item"), false);
		if (BaseItemTag.IsValid() && !ItemData->Classification.ItemType.MatchesTag(BaseItemTag))
		{
			OutErrors.Add(FString::Printf(TEXT("[%s] ItemType '%s' is not in Item.* hierarchy"),
				*ItemID.ToString(), *ItemData->Classification.ItemType.ToString()));
			bIsValid = false;
		}
	}

	// Validate weapon-specific
	if (ItemData->bIsWeapon)
	{
		if (!ItemData->WeaponConfig.WeaponArchetype.IsValid())
		{
			OutErrors.Add(FString::Printf(TEXT("[%s] Weapon has no archetype"), *ItemID.ToString()));
			bIsValid = false;
		}

		if (!ItemData->GASConfig.AttributeSetClass)
		{
			OutErrors.Add(FString::Printf(TEXT("[%s] Weapon missing AttributeSetClass"),
				*ItemID.ToString()));
			bIsValid = false;
		}
	}

	// Validate stack size
	if (ItemData->InventoryProps.MaxStackSize <= 0)
	{
		OutErrors.Add(FString::Printf(TEXT("[%s] Invalid MaxStackSize: %d"),
			*ItemID.ToString(), ItemData->InventoryProps.MaxStackSize));
		bIsValid = false;
	}

	return bIsValid;
}

int32 USuspenseCoreDataManager::ValidateAllItems(TArray<FString>& OutErrors) const
{
	OutErrors.Empty();
	int32 ErrorCount = 0;

	for (const auto& ItemPair : ItemCache)
	{
		TArray<FString> ItemErrors;
		if (!ValidateItem(ItemPair.Key, ItemErrors))
		{
			ErrorCount++;
			OutErrors.Append(ItemErrors);
		}
	}

	return ErrorCount;
}

//========================================================================
// Character Data Access
//========================================================================

UDataAsset* USuspenseCoreDataManager::GetCharacterClassesDataAsset() const
{
	return LoadedCharacterClassesDataAsset;
}

FGameplayTag USuspenseCoreDataManager::GetDefaultCharacterClass() const
{
	const USuspenseCoreSettings* Settings = USuspenseCoreSettings::Get();
	return Settings ? Settings->DefaultCharacterClass : FGameplayTag();
}

//========================================================================
// Loadout Data Access
//========================================================================

UDataTable* USuspenseCoreDataManager::GetLoadoutDataTable() const
{
	return LoadedLoadoutDataTable;
}

FName USuspenseCoreDataManager::GetDefaultLoadoutID() const
{
	const USuspenseCoreSettings* Settings = USuspenseCoreSettings::Get();
	return Settings ? Settings->DefaultLoadoutID : NAME_None;
}

//========================================================================
// EventBus Integration
//========================================================================

USuspenseCoreEventBus* USuspenseCoreDataManager::GetEventBus() const
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	USuspenseCoreEventManager* EventManager = USuspenseCoreEventManager::Get(this);
	if (EventManager)
	{
		USuspenseCoreEventBus* EventBus = EventManager->GetEventBus();
		CachedEventBus = EventBus;
		return EventBus;
	}

	return nullptr;
}

//========================================================================
// EventBus Broadcasting
//========================================================================

void USuspenseCoreDataManager::BroadcastInitialized()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(
		const_cast<USuspenseCoreDataManager*>(this),
		ESuspenseCoreEventPriority::High
	);

	EventData.SetInt(TEXT("CachedItemCount"), ItemCache.Num());
	EventData.SetBool(TEXT("ItemSystemReady"), bItemSystemReady);
	EventData.SetBool(TEXT("CharacterSystemReady"), bCharacterSystemReady);
	EventData.SetBool(TEXT("LoadoutSystemReady"), bLoadoutSystemReady);

	static const FGameplayTag InitializedTag =
		FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Data.Initialized"));

	EventBus->Publish(InitializedTag, EventData);

	UE_LOG(LogSuspenseCoreData, Log, TEXT("Broadcast: Data.Initialized (Items: %d)"), ItemCache.Num());
}

void USuspenseCoreDataManager::BroadcastItemLoaded(FName ItemID, const FSuspenseCoreItemData& ItemData) const
{
	const USuspenseCoreSettings* Settings = USuspenseCoreSettings::Get();
	if (!Settings || !Settings->bLogItemOperations)
	{
		return; // Skip event if not logging
	}

	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(
		const_cast<USuspenseCoreDataManager*>(this),
		ESuspenseCoreEventPriority::Low
	);

	EventData.SetString(TEXT("ItemID"), ItemID.ToString());
	EventData.SetString(TEXT("DisplayName"), ItemData.Identity.DisplayName.ToString());
	EventData.SetString(TEXT("ItemType"), ItemData.Classification.ItemType.ToString());

	static const FGameplayTag ItemLoadedTag =
		FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Data.ItemLoaded"));

	EventBus->Publish(ItemLoadedTag, EventData);
}

void USuspenseCoreDataManager::BroadcastItemNotFound(FName ItemID) const
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(
		const_cast<USuspenseCoreDataManager*>(this),
		ESuspenseCoreEventPriority::Normal
	);

	EventData.SetString(TEXT("ItemID"), ItemID.ToString());

	static const FGameplayTag ItemNotFoundTag =
		FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Data.ItemNotFound"));

	EventBus->Publish(ItemNotFoundTag, EventData);

	UE_LOG(LogSuspenseCoreData, Warning, TEXT("Broadcast: Data.ItemNotFound - %s"), *ItemID.ToString());
}

void USuspenseCoreDataManager::BroadcastValidationResult(bool bPassed, const TArray<FString>& Errors) const
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(
		const_cast<USuspenseCoreDataManager*>(this),
		ESuspenseCoreEventPriority::High
	);

	EventData.SetBool(TEXT("Passed"), bPassed);
	EventData.SetInt(TEXT("ErrorCount"), Errors.Num());

	// Concatenate errors for event
	FString ErrorSummary;
	for (int32 i = 0; i < FMath::Min(Errors.Num(), 10); i++)
	{
		ErrorSummary += Errors[i] + TEXT("\n");
	}
	EventData.SetString(TEXT("Errors"), ErrorSummary);

	FGameplayTag ValidationTag = bPassed
		? FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Data.ValidationPassed"))
		: FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Data.ValidationFailed"));

	EventBus->Publish(ValidationTag, EventData);

	UE_LOG(LogSuspenseCoreData, Log, TEXT("Broadcast: Data.Validation%s"),
		bPassed ? TEXT("Passed") : TEXT("Failed"));
}

//========================================================================
// GAS Attributes Initialization (SSOT)
//========================================================================

bool USuspenseCoreDataManager::InitializeWeaponAttributesSystem()
{
	const USuspenseCoreSettings* Settings = USuspenseCoreSettings::Get();
	if (!Settings)
	{
		return false;
	}

	if (Settings->WeaponAttributesDataTable.IsNull())
	{
		UE_LOG(LogSuspenseCoreData, Verbose, TEXT("WeaponAttributesDataTable not configured (optional)"));
		return false;
	}

	UE_LOG(LogSuspenseCoreData, Log, TEXT("Loading WeaponAttributesDataTable: %s"),
		*Settings->WeaponAttributesDataTable.ToString());

	LoadedWeaponAttributesDataTable = Settings->WeaponAttributesDataTable.LoadSynchronous();

	if (!LoadedWeaponAttributesDataTable)
	{
		UE_LOG(LogSuspenseCoreData, Warning, TEXT("Failed to load WeaponAttributesDataTable"));
		return false;
	}

	// Verify row structure
	const UScriptStruct* RowStruct = LoadedWeaponAttributesDataTable->GetRowStruct();
	if (!RowStruct)
	{
		UE_LOG(LogSuspenseCoreData, Error, TEXT("WeaponAttributesDataTable has no row structure!"));
		return false;
	}

	UE_LOG(LogSuspenseCoreData, Log, TEXT("WeaponAttributes Row Structure: %s"), *RowStruct->GetName());

	return BuildWeaponAttributesCache(LoadedWeaponAttributesDataTable);
}

bool USuspenseCoreDataManager::BuildWeaponAttributesCache(UDataTable* DataTable)
{
	if (!DataTable)
	{
		return false;
	}

	WeaponAttributesCache.Empty();

	TArray<FName> RowNames = DataTable->GetRowNames();
	UE_LOG(LogSuspenseCoreData, Log, TEXT("Building weapon attributes cache from %d rows..."), RowNames.Num());

	int32 LoadedCount = 0;

	for (const FName& RowName : RowNames)
	{
		FSuspenseCoreWeaponAttributeRow* RowData = DataTable->FindRow<FSuspenseCoreWeaponAttributeRow>(RowName, TEXT(""));
		if (!RowData)
		{
			UE_LOG(LogSuspenseCoreData, Warning, TEXT("  Failed to read weapon attribute row: %s"), *RowName.ToString());
			continue;
		}

		// Validate row
		if (!RowData->IsValid())
		{
			UE_LOG(LogSuspenseCoreData, Warning, TEXT("  Invalid weapon attribute row: %s"), *RowName.ToString());
			continue;
		}

		// Use WeaponID if set, otherwise use row name as key
		FName CacheKey = RowData->WeaponID.IsNone() ? RowName : RowData->WeaponID;

		WeaponAttributesCache.Add(CacheKey, *RowData);
		LoadedCount++;

		UE_LOG(LogSuspenseCoreData, Verbose, TEXT("  Cached weapon attrs: %s (Damage=%.1f, ROF=%.0f)"),
			*CacheKey.ToString(), RowData->BaseDamage, RowData->RateOfFire);
	}

	UE_LOG(LogSuspenseCoreData, Log, TEXT("Weapon attributes cache built: %d entries"), LoadedCount);
	return LoadedCount > 0;
}

bool USuspenseCoreDataManager::InitializeAmmoAttributesSystem()
{
	const USuspenseCoreSettings* Settings = USuspenseCoreSettings::Get();
	if (!Settings)
	{
		return false;
	}

	if (Settings->AmmoAttributesDataTable.IsNull())
	{
		UE_LOG(LogSuspenseCoreData, Verbose, TEXT("AmmoAttributesDataTable not configured (optional)"));
		return false;
	}

	UE_LOG(LogSuspenseCoreData, Log, TEXT("Loading AmmoAttributesDataTable: %s"),
		*Settings->AmmoAttributesDataTable.ToString());

	LoadedAmmoAttributesDataTable = Settings->AmmoAttributesDataTable.LoadSynchronous();

	if (!LoadedAmmoAttributesDataTable)
	{
		UE_LOG(LogSuspenseCoreData, Warning, TEXT("Failed to load AmmoAttributesDataTable"));
		return false;
	}

	// Verify row structure
	const UScriptStruct* RowStruct = LoadedAmmoAttributesDataTable->GetRowStruct();
	if (!RowStruct)
	{
		UE_LOG(LogSuspenseCoreData, Error, TEXT("AmmoAttributesDataTable has no row structure!"));
		return false;
	}

	UE_LOG(LogSuspenseCoreData, Log, TEXT("AmmoAttributes Row Structure: %s"), *RowStruct->GetName());

	return BuildAmmoAttributesCache(LoadedAmmoAttributesDataTable);
}

bool USuspenseCoreDataManager::BuildAmmoAttributesCache(UDataTable* DataTable)
{
	if (!DataTable)
	{
		return false;
	}

	AmmoAttributesCache.Empty();

	TArray<FName> RowNames = DataTable->GetRowNames();
	UE_LOG(LogSuspenseCoreData, Log, TEXT("Building ammo attributes cache from %d rows..."), RowNames.Num());

	int32 LoadedCount = 0;

	for (const FName& RowName : RowNames)
	{
		FSuspenseCoreAmmoAttributeRow* RowData = DataTable->FindRow<FSuspenseCoreAmmoAttributeRow>(RowName, TEXT(""));
		if (!RowData)
		{
			UE_LOG(LogSuspenseCoreData, Warning, TEXT("  Failed to read ammo attribute row: %s"), *RowName.ToString());
			continue;
		}

		// Validate row
		if (!RowData->IsValid())
		{
			UE_LOG(LogSuspenseCoreData, Warning, TEXT("  Invalid ammo attribute row: %s"), *RowName.ToString());
			continue;
		}

		// Use AmmoID if set, otherwise use row name as key
		FName CacheKey = RowData->AmmoID.IsNone() ? RowName : RowData->AmmoID;

		AmmoAttributesCache.Add(CacheKey, *RowData);
		LoadedCount++;

		UE_LOG(LogSuspenseCoreData, Verbose, TEXT("  Cached ammo attrs: %s (Damage=%.1f, Pen=%.0f)"),
			*CacheKey.ToString(), RowData->BaseDamage, RowData->ArmorPenetration);
	}

	UE_LOG(LogSuspenseCoreData, Log, TEXT("Ammo attributes cache built: %d entries"), LoadedCount);
	return LoadedCount > 0;
}

bool USuspenseCoreDataManager::InitializeArmorAttributesSystem()
{
	const USuspenseCoreSettings* Settings = USuspenseCoreSettings::Get();
	if (!Settings)
	{
		return false;
	}

	if (Settings->ArmorAttributesDataTable.IsNull())
	{
		UE_LOG(LogSuspenseCoreData, Verbose, TEXT("ArmorAttributesDataTable not configured (optional)"));
		return false;
	}

	UE_LOG(LogSuspenseCoreData, Log, TEXT("Loading ArmorAttributesDataTable: %s"),
		*Settings->ArmorAttributesDataTable.ToString());

	LoadedArmorAttributesDataTable = Settings->ArmorAttributesDataTable.LoadSynchronous();

	if (!LoadedArmorAttributesDataTable)
	{
		UE_LOG(LogSuspenseCoreData, Warning, TEXT("Failed to load ArmorAttributesDataTable"));
		return false;
	}

	return BuildArmorAttributesCache(LoadedArmorAttributesDataTable);
}

bool USuspenseCoreDataManager::BuildArmorAttributesCache(UDataTable* DataTable)
{
	if (!DataTable)
	{
		return false;
	}

	ArmorAttributesCache.Empty();

	TArray<FName> RowNames = DataTable->GetRowNames();
	UE_LOG(LogSuspenseCoreData, Log, TEXT("Building armor attributes cache from %d rows..."), RowNames.Num());

	int32 LoadedCount = 0;

	for (const FName& RowName : RowNames)
	{
		FSuspenseCoreArmorAttributeRow* RowData = DataTable->FindRow<FSuspenseCoreArmorAttributeRow>(RowName, TEXT(""));
		if (!RowData)
		{
			UE_LOG(LogSuspenseCoreData, Warning, TEXT("  Failed to read armor attribute row: %s"), *RowName.ToString());
			continue;
		}

		// Validate row
		if (!RowData->IsValid())
		{
			UE_LOG(LogSuspenseCoreData, Warning, TEXT("  Invalid armor attribute row: %s"), *RowName.ToString());
			continue;
		}

		// Use ArmorID if set, otherwise use row name as key
		FName CacheKey = RowData->ArmorID.IsNone() ? RowName : RowData->ArmorID;

		ArmorAttributesCache.Add(CacheKey, *RowData);
		LoadedCount++;

		UE_LOG(LogSuspenseCoreData, Verbose, TEXT("  Cached armor attrs: %s (Class=%d, Durability=%.0f)"),
			*CacheKey.ToString(), RowData->ArmorClass, RowData->MaxDurability);
	}

	UE_LOG(LogSuspenseCoreData, Log, TEXT("Armor attributes cache built: %d entries"), LoadedCount);
	return LoadedCount > 0;
}

//========================================================================
// GAS Attributes Access (SSOT)
//========================================================================

bool USuspenseCoreDataManager::GetWeaponAttributes(FName AttributeKey, FSuspenseCoreWeaponAttributeRow& OutAttributes) const
{
	if (AttributeKey.IsNone())
	{
		return false;
	}

	const FSuspenseCoreWeaponAttributeRow* Found = WeaponAttributesCache.Find(AttributeKey);
	if (Found)
	{
		OutAttributes = *Found;
		return true;
	}

	UE_LOG(LogSuspenseCoreData, Verbose, TEXT("GetWeaponAttributes: '%s' not found in cache"), *AttributeKey.ToString());
	return false;
}

bool USuspenseCoreDataManager::GetAmmoAttributes(FName AttributeKey, FSuspenseCoreAmmoAttributeRow& OutAttributes) const
{
	if (AttributeKey.IsNone())
	{
		return false;
	}

	const FSuspenseCoreAmmoAttributeRow* Found = AmmoAttributesCache.Find(AttributeKey);
	if (Found)
	{
		OutAttributes = *Found;
		return true;
	}

	UE_LOG(LogSuspenseCoreData, Verbose, TEXT("GetAmmoAttributes: '%s' not found in cache"), *AttributeKey.ToString());
	return false;
}

bool USuspenseCoreDataManager::GetArmorAttributes(FName AttributeKey, FSuspenseCoreArmorAttributeRow& OutAttributes) const
{
	if (AttributeKey.IsNone())
	{
		return false;
	}

	const FSuspenseCoreArmorAttributeRow* Found = ArmorAttributesCache.Find(AttributeKey);
	if (Found)
	{
		OutAttributes = *Found;
		return true;
	}

	UE_LOG(LogSuspenseCoreData, Verbose, TEXT("GetArmorAttributes: '%s' not found in cache"), *AttributeKey.ToString());
	return false;
}

bool USuspenseCoreDataManager::HasWeaponAttributes(FName AttributeKey) const
{
	return WeaponAttributesCache.Contains(AttributeKey);
}

bool USuspenseCoreDataManager::HasAmmoAttributes(FName AttributeKey) const
{
	return AmmoAttributesCache.Contains(AttributeKey);
}

TArray<FName> USuspenseCoreDataManager::GetAllWeaponAttributeKeys() const
{
	TArray<FName> Keys;
	WeaponAttributesCache.GetKeys(Keys);
	return Keys;
}

TArray<FName> USuspenseCoreDataManager::GetAllAmmoAttributeKeys() const
{
	TArray<FName> Keys;
	AmmoAttributesCache.GetKeys(Keys);
	return Keys;
}
