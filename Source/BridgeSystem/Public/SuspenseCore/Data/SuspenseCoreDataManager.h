// SuspenseCoreDataManager.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/Items/SuspenseCoreItemTypes.h"
// Include for FSuspenseCoreUnifiedItemData - needed for cache storage
#include "SuspenseCore/Types/Loadout/SuspenseCoreItemDataTable.h"
// Include for GAS attribute row structures (SSOT)
#include "SuspenseCore/Types/GAS/SuspenseCoreGASAttributeRows.h"
// Include for magazine types (Tarkov-style)
#include "SuspenseCore/Types/Weapon/SuspenseCoreMagazineTypes.h"
#include "SuspenseCoreDataManager.generated.h"


class UDataTable;
class UDataAsset;
class USuspenseCoreEventBus;
class USuspenseCoreSettings;
struct FSuspenseCoreEventData;



/**
 * USuspenseCoreDataManager
 *
 * Central data management subsystem for SuspenseCore.
 * Loads, caches, and provides access to all game data with EventBus integration.
 *
 * ARCHITECTURE NOTES:
 * ═══════════════════════════════════════════════════════════════════════════
 * This subsystem is the RUNTIME interface for all SuspenseCore data.
 * Configuration comes from USuspenseCoreSettings (Project Settings).
 * All operations broadcast events through SuspenseCoreEventBus.
 *
 * LIFECYCLE:
 * ═══════════════════════════════════════════════════════════════════════════
 * 1. GameInstance creates subsystem
 * 2. Initialize() loads data from USuspenseCoreSettings
 * 3. Caches data for fast runtime access
 * 4. Broadcasts SuspenseCore.Event.Data.Initialized
 * 5. Provides data access throughout game session
 * 6. Deinitialize() cleans up on shutdown
 *
 * EVENTBUS EVENTS:
 * ═══════════════════════════════════════════════════════════════════════════
 * - SuspenseCore.Event.Data.Initialized      - Data manager ready
 * - SuspenseCore.Event.Data.ItemLoaded       - Item data loaded from cache
 * - SuspenseCore.Event.Data.ItemNotFound     - Item ID not in database
 * - SuspenseCore.Event.Data.ValidationFailed - Validation error occurred
 * - SuspenseCore.Event.Data.ValidationPassed - All validations passed
 *
 * USAGE:
 * ═══════════════════════════════════════════════════════════════════════════
 * C++:
 *   USuspenseCoreDataManager* DataManager = USuspenseCoreDataManager::Get(WorldContextObject);
 *   if (DataManager)
 *   {
 *       FSuspenseCoreItemData ItemData;
 *       if (DataManager->GetItemData(ItemID, ItemData))
 *       {
 *           // Use item data
 *       }
 *   }
 *
 * Blueprint:
 *   Use "Get SuspenseCore Data Manager" node
 *
 * @see USuspenseCoreSettings - Configuration source
 * @see USuspenseCoreEventManager - EventBus provider
 */
UCLASS()
class BRIDGESYSTEM_API USuspenseCoreDataManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//========================================================================
	// Static Access
	//========================================================================

	/**
	 * Get DataManager from world context
	 * @param WorldContextObject Any object with valid world context
	 * @return DataManager or nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data", meta = (WorldContext = "WorldContextObject", DisplayName = "Get SuspenseCore Data Manager"))
	static USuspenseCoreDataManager* Get(const UObject* WorldContextObject);

	//========================================================================
	// USubsystem Interface
	//========================================================================

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	//========================================================================
	// Initialization Status
	//========================================================================

	/** Check if data manager is fully initialized and ready */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data")
	bool IsInitialized() const { return bIsInitialized; }

	/** Check if item system is ready */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data")
	bool IsItemSystemReady() const { return bItemSystemReady; }

	//========================================================================
	// Item Data Access
	//========================================================================

	/**
	 * Get item data by ID (simplified format)
	 * Broadcasts SuspenseCore.Event.Data.ItemLoaded or ItemNotFound
	 *
	 * @param ItemID The item identifier
	 * @param OutItemData Output item data structure
	 * @return true if item was found
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Data")
	bool GetItemData(FName ItemID, FSuspenseCoreItemData& OutItemData) const;

	/**
	 * Get unified item data by ID (full DataTable format)
	 * This is the PRIMARY method for equipment system - contains all fields
	 * including EquipmentActorClass, AttachmentSocket, etc.
	 *
	 * @param ItemID The item identifier
	 * @param OutItemData Output unified item data structure
	 * @return true if item was found
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Data")
	bool GetUnifiedItemData(FName ItemID, FSuspenseCoreUnifiedItemData& OutItemData) const;

	/**
	 * Check if item exists in database
	 * @param ItemID The item identifier
	 * @return true if item exists
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data")
	bool HasItem(FName ItemID) const;

	/**
	 * Get all cached item IDs
	 * @return Array of all item IDs in cache
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data")
	TArray<FName> GetAllItemIDs() const;

	/**
	 * Get count of cached items
	 * @return Number of items in cache
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data")
	int32 GetCachedItemCount() const { return UnifiedItemCache.Num(); }

	//========================================================================
	// Item Instance Creation
	//========================================================================

	/**
	 * Create runtime item instance from ItemID
	 * @param ItemID Item identifier
	 * @param Quantity Stack quantity
	 * @param OutInstance Output runtime instance
	 * @return true if instance created successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Data")
	bool CreateItemInstance(FName ItemID, int32 Quantity, FSuspenseCoreItemInstance& OutInstance) const;

	//========================================================================
	// Item Validation
	//========================================================================

	/**
	 * Validate single item configuration
	 * @param ItemID Item to validate
	 * @param OutErrors Output validation errors
	 * @return true if item is valid
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Data")
	bool ValidateItem(FName ItemID, TArray<FString>& OutErrors) const;

	/**
	 * Validate all items in cache
	 * @param OutErrors Output validation errors
	 * @return Number of items with errors
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Data")
	int32 ValidateAllItems(TArray<FString>& OutErrors) const;

	//========================================================================
	// Character Data Access
	//========================================================================

	/**
	 * Get character classes data asset
	 * @return Character classes data asset or nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data")
	UDataAsset* GetCharacterClassesDataAsset() const;

	/**
	 * Get default character class tag
	 * @return Default character class gameplay tag
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data")
	FGameplayTag GetDefaultCharacterClass() const;

	//========================================================================
	// Loadout Data Access (Future)
	//========================================================================

	/**
	 * Get loadout DataTable
	 * @return Loadout configurations DataTable or nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data")
	UDataTable* GetLoadoutDataTable() const;

	/**
	 * Get default loadout ID
	 * @return Default loadout identifier
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data")
	FName GetDefaultLoadoutID() const;

	//========================================================================
	// EventBus Integration
	//========================================================================

	/**
	 * Get cached EventBus reference
	 * @return EventBus or nullptr
	 */
	USuspenseCoreEventBus* GetEventBus() const;

	//========================================================================
	// GAS Attributes Access (SSOT)
	//========================================================================

	/**
	 * Get weapon attributes by ItemID or RowName
	 * Uses WeaponAttributesDataTable configured in Settings
	 *
	 * @param AttributeKey ItemID or explicit row name
	 * @param OutAttributes Output weapon attributes structure
	 * @return true if attributes found
	 *
	 * @see FSuspenseCoreWeaponAttributeRow
	 * @see Documentation/Plans/SSOT_AttributeSet_DataTable_Integration.md
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Data|GAS")
	bool GetWeaponAttributes(FName AttributeKey, FSuspenseCoreWeaponAttributeRow& OutAttributes) const;

	/**
	 * Get ammo attributes by AmmoID or RowName
	 * Uses AmmoAttributesDataTable configured in Settings
	 *
	 * @param AttributeKey AmmoID or explicit row name
	 * @param OutAttributes Output ammo attributes structure
	 * @return true if attributes found
	 *
	 * @see FSuspenseCoreAmmoAttributeRow
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Data|GAS")
	bool GetAmmoAttributes(FName AttributeKey, FSuspenseCoreAmmoAttributeRow& OutAttributes) const;

	/**
	 * Get armor attributes by ArmorID or RowName
	 * Uses ArmorAttributesDataTable configured in Settings
	 *
	 * @param AttributeKey ArmorID or explicit row name
	 * @param OutAttributes Output armor attributes structure
	 * @return true if attributes found
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Data|GAS")
	bool GetArmorAttributes(FName AttributeKey, FSuspenseCoreArmorAttributeRow& OutAttributes) const;

	/**
	 * Get throwable attributes by ThrowableID or RowName
	 * Uses ThrowableAttributesDataTable configured in Settings
	 *
	 * @param AttributeKey ThrowableID or explicit row name
	 * @param OutAttributes Output throwable attributes structure
	 * @return true if attributes found
	 *
	 * @see FSuspenseCoreThrowableAttributeRow
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Data|GAS")
	bool GetThrowableAttributes(FName AttributeKey, FSuspenseCoreThrowableAttributeRow& OutAttributes) const;

	/**
	 * Check if weapon attributes exist for given key
	 * @param AttributeKey ItemID or row name
	 * @return true if attributes exist in cache
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data|GAS")
	bool HasWeaponAttributes(FName AttributeKey) const;

	/**
	 * Check if throwable attributes exist for given key
	 * @param AttributeKey ThrowableID or row name
	 * @return true if attributes exist in cache
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data|GAS")
	bool HasThrowableAttributes(FName AttributeKey) const;

	/**
	 * Check if ammo attributes exist for given key
	 * @param AttributeKey AmmoID or row name
	 * @return true if attributes exist in cache
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data|GAS")
	bool HasAmmoAttributes(FName AttributeKey) const;

	/**
	 * Get all cached weapon attribute keys
	 * @return Array of all weapon attribute keys
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data|GAS")
	TArray<FName> GetAllWeaponAttributeKeys() const;

	/**
	 * Get all cached ammo attribute keys
	 * @return Array of all ammo attribute keys
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data|GAS")
	TArray<FName> GetAllAmmoAttributeKeys() const;

	/**
	 * Get count of cached weapon attributes
	 * @return Number of weapon attribute rows in cache
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data|GAS")
	int32 GetCachedWeaponAttributesCount() const { return WeaponAttributesCache.Num(); }

	/**
	 * Get count of cached ammo attributes
	 * @return Number of ammo attribute rows in cache
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data|GAS")
	int32 GetCachedAmmoAttributesCount() const { return AmmoAttributesCache.Num(); }

	/** Check if weapon attributes system is initialized */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data|GAS")
	bool IsWeaponAttributesSystemReady() const { return bWeaponAttributesSystemReady; }

	/** Check if ammo attributes system is initialized */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data|GAS")
	bool IsAmmoAttributesSystemReady() const { return bAmmoAttributesSystemReady; }

	/** Check if armor attributes system is initialized */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data|GAS")
	bool IsArmorAttributesSystemReady() const { return bArmorAttributesSystemReady; }

	//========================================================================
	// Attachment Attributes Access (Tarkov-Style Recoil Modifiers)
	// @see Documentation/Plans/TarkovStyle_Recoil_System_Design.md
	//========================================================================

	/**
	 * Get attachment attributes by AttachmentID
	 * Uses AttachmentAttributesDataTable configured in Settings
	 *
	 * @param AttributeKey AttachmentID or explicit row name
	 * @param OutAttributes Output attachment attributes structure
	 * @return true if attributes found
	 *
	 * @see FSuspenseCoreAttachmentAttributeRow
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Data|GAS")
	bool GetAttachmentAttributes(FName AttributeKey, FSuspenseCoreAttachmentAttributeRow& OutAttributes) const;

	/**
	 * Check if attachment attributes exist for given key
	 * @param AttributeKey AttachmentID or row name
	 * @return true if attributes exist in cache
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data|GAS")
	bool HasAttachmentAttributes(FName AttributeKey) const;

	/**
	 * Get all cached attachment attribute keys
	 * @return Array of all attachment attribute keys
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data|GAS")
	TArray<FName> GetAllAttachmentAttributeKeys() const;

	/**
	 * Get count of cached attachment attributes
	 * @return Number of attachment attribute rows in cache
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data|GAS")
	int32 GetCachedAttachmentAttributesCount() const { return AttachmentAttributesCache.Num(); }

	/** Check if attachment attributes system is initialized */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data|GAS")
	bool IsAttachmentAttributesSystemReady() const { return bAttachmentAttributesSystemReady; }

	//========================================================================
	// Status Effect Attributes Access (SSOT for Buffs/Debuffs)
	// @see Documentation/Plans/StatusEffect_SSOT_System.md
	//========================================================================

	/**
	 * Get status effect attributes by EffectID or RowName
	 * Uses StatusEffectAttributesDataTable configured in Settings
	 *
	 * @param EffectKey EffectID or explicit row name
	 * @param OutAttributes Output status effect attributes structure
	 * @return true if attributes found
	 *
	 * @see FSuspenseCoreStatusEffectAttributeRow
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Data|StatusEffect")
	bool GetStatusEffectAttributes(FName EffectKey, FSuspenseCoreStatusEffectAttributeRow& OutAttributes) const;

	/**
	 * Get status effect attributes by GameplayTag
	 * Searches by EffectTypeTag field
	 *
	 * @param EffectTag Effect type tag (e.g., State.Health.Bleeding.Light)
	 * @param OutAttributes Output status effect attributes structure
	 * @return true if attributes found
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Data|StatusEffect")
	bool GetStatusEffectByTag(FGameplayTag EffectTag, FSuspenseCoreStatusEffectAttributeRow& OutAttributes) const;

	/**
	 * Get all status effects of a specific category
	 * @param Category Buff, Debuff, or Neutral
	 * @return Array of effect IDs matching the category
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Data|StatusEffect")
	TArray<FName> GetStatusEffectsByCategory(ESuspenseCoreStatusEffectCategory Category) const;

	/**
	 * Get all debuff effect IDs
	 * @return Array of all debuff effect IDs
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data|StatusEffect")
	TArray<FName> GetAllDebuffIDs() const;

	/**
	 * Get all buff effect IDs
	 * @return Array of all buff effect IDs
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data|StatusEffect")
	TArray<FName> GetAllBuffIDs() const;

	/**
	 * Check if status effect exists for given key
	 * @param EffectKey EffectID or row name
	 * @return true if effect exists in cache
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data|StatusEffect")
	bool HasStatusEffect(FName EffectKey) const;

	/**
	 * Get all cached status effect keys
	 * @return Array of all status effect keys
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data|StatusEffect")
	TArray<FName> GetAllStatusEffectKeys() const;

	/**
	 * Get count of cached status effects
	 * @return Number of status effect rows in cache
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data|StatusEffect")
	int32 GetCachedStatusEffectCount() const { return StatusEffectAttributesCache.Num(); }

	/** Check if status effect system is initialized */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data|StatusEffect")
	bool IsStatusEffectSystemReady() const { return bStatusEffectSystemReady; }

	//========================================================================
	// Magazine System Access (Tarkov-Style)
	//========================================================================

	/**
	 * Get magazine data by MagazineID
	 * Uses MagazineDataTable configured in Settings
	 *
	 * @param MagazineID Magazine identifier (DataTable row name)
	 * @param OutMagazineData Output magazine data structure
	 * @return true if magazine found
	 *
	 * @see FSuspenseCoreMagazineData
	 * @see Documentation/Plans/TarkovStyle_Ammo_System_Design.md
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Data|Magazine")
	bool GetMagazineData(FName MagazineID, FSuspenseCoreMagazineData& OutMagazineData) const;

	/**
	 * Check if magazine exists
	 * @param MagazineID Magazine identifier
	 * @return true if magazine exists in cache
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data|Magazine")
	bool HasMagazine(FName MagazineID) const;

	/**
	 * Get all cached magazine IDs
	 * @return Array of all magazine IDs
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data|Magazine")
	TArray<FName> GetAllMagazineIDs() const;

	/**
	 * Get magazines compatible with specific weapon
	 * @param WeaponTag Weapon type tag to match
	 * @return Array of compatible magazine IDs
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Data|Magazine")
	TArray<FName> GetMagazinesForWeapon(const FGameplayTag& WeaponTag) const;

	/**
	 * Get magazines compatible with specific caliber
	 * @param CaliberTag Caliber tag to match
	 * @return Array of compatible magazine IDs
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Data|Magazine")
	TArray<FName> GetMagazinesForCaliber(const FGameplayTag& CaliberTag) const;

	/**
	 * Create magazine instance from DataTable data
	 * @param MagazineID Magazine identifier
	 * @param InitialRounds Initial round count (0 = empty)
	 * @param AmmoID Ammo type to load (if InitialRounds > 0)
	 * @param OutInstance Output magazine instance
	 * @return true if instance created successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Data|Magazine")
	bool CreateMagazineInstance(FName MagazineID, int32 InitialRounds, FName AmmoID, FSuspenseCoreMagazineInstance& OutInstance) const;

	/** Get count of cached magazines */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data|Magazine")
	int32 GetCachedMagazineCount() const { return MagazineCache.Num(); }

	/** Check if magazine system is initialized */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Data|Magazine")
	bool IsMagazineSystemReady() const { return bMagazineSystemReady; }

protected:
	//========================================================================
	// Initialization Helpers
	//========================================================================

	/** Load item data from settings */
	bool InitializeItemSystem();

	/** Build item cache from DataTable */
	bool BuildItemCache(UDataTable* DataTable);

	/** Load character data from settings */
	bool InitializeCharacterSystem();

	/** Load loadout data from settings */
	bool InitializeLoadoutSystem();

	/** Convert unified item data to internal item data format */
	static FSuspenseCoreItemData ConvertUnifiedToItemData(
		const FSuspenseCoreUnifiedItemData& Unified, FName RowName);

	//========================================================================
	// GAS Attributes Initialization (SSOT)
	//========================================================================

	/**
	 * Initialize weapon attributes system
	 * Loads WeaponAttributesDataTable from Settings and caches all rows
	 * @return true if initialization successful
	 */
	bool InitializeWeaponAttributesSystem();

	/**
	 * Build weapon attributes cache from DataTable
	 * @param DataTable Weapon attributes DataTable
	 * @return true if cache built successfully
	 */
	bool BuildWeaponAttributesCache(UDataTable* DataTable);

	/**
	 * Initialize ammo attributes system
	 * Loads AmmoAttributesDataTable from Settings and caches all rows
	 * @return true if initialization successful
	 */
	bool InitializeAmmoAttributesSystem();

	/**
	 * Build ammo attributes cache from DataTable
	 * @param DataTable Ammo attributes DataTable
	 * @return true if cache built successfully
	 */
	bool BuildAmmoAttributesCache(UDataTable* DataTable);

	/**
	 * Initialize armor attributes system
	 * Loads ArmorAttributesDataTable from Settings and caches all rows
	 * @return true if initialization successful
	 */
	bool InitializeArmorAttributesSystem();

	/**
	 * Build armor attributes cache from DataTable
	 * @param DataTable Armor attributes DataTable
	 * @return true if cache built successfully
	 */
	bool BuildArmorAttributesCache(UDataTable* DataTable);

	/**
	 * Initialize throwable attributes system
	 * Loads ThrowableAttributesDataTable from Settings and caches all rows
	 * @return true if initialization successful
	 */
	bool InitializeThrowableAttributesSystem();

	/**
	 * Build throwable attributes cache from DataTable
	 * @param DataTable Throwable attributes DataTable
	 * @return true if cache built successfully
	 */
	bool BuildThrowableAttributesCache(UDataTable* DataTable);

	//========================================================================
	// Attachment Attributes Initialization (Tarkov-Style Modifiers)
	//========================================================================

	/**
	 * Initialize attachment attributes system
	 * Loads AttachmentAttributesDataTable from Settings and caches all rows
	 * @return true if initialization successful
	 */
	bool InitializeAttachmentAttributesSystem();

	/**
	 * Build attachment attributes cache from DataTable
	 * @param DataTable Attachment attributes DataTable
	 * @return true if cache built successfully
	 */
	bool BuildAttachmentAttributesCache(UDataTable* DataTable);

	//========================================================================
	// Status Effect Attributes Initialization (SSOT for Buffs/Debuffs)
	//========================================================================

	/**
	 * Initialize status effect attributes system
	 * Loads StatusEffectAttributesDataTable from Settings and caches all rows
	 * Also builds EffectTag → EffectID lookup map
	 * @return true if initialization successful
	 */
	bool InitializeStatusEffectAttributesSystem();

	/**
	 * Build status effect attributes cache from DataTable
	 * @param DataTable Status effect attributes DataTable
	 * @return true if cache built successfully
	 */
	bool BuildStatusEffectAttributesCache(UDataTable* DataTable);

	//========================================================================
	// Magazine System Initialization (Tarkov-Style)
	//========================================================================

	/**
	 * Initialize magazine system
	 * Loads MagazineDataTable from Settings and caches all rows
	 * @return true if initialization successful
	 */
	bool InitializeMagazineSystem();

	/**
	 * Build magazine cache from DataTable
	 * @param DataTable Magazine DataTable
	 * @return true if cache built successfully
	 */
	bool BuildMagazineCache(UDataTable* DataTable);

	//========================================================================
	// EventBus Broadcasting
	//========================================================================

	/** Broadcast initialization complete event */
	void BroadcastInitialized();

	/** Broadcast item loaded event */
	void BroadcastItemLoaded(FName ItemID, const FSuspenseCoreItemData& ItemData) const;

	/** Broadcast item not found event */
	void BroadcastItemNotFound(FName ItemID) const;

	/** Broadcast validation result */
	void BroadcastValidationResult(bool bPassed, const TArray<FString>& Errors) const;

private:
	//========================================================================
	// Cached Data
	//========================================================================

	/**
	 * PRIMARY cache: Unified item data from DataTable
	 * Contains ALL item fields including EquipmentActorClass, sockets, etc.
	 * This is the SINGLE SOURCE OF TRUTH for item data.
	 */
	UPROPERTY()
	TMap<FName, FSuspenseCoreUnifiedItemData> UnifiedItemCache;

	/**
	 * SECONDARY cache: Simplified item data for legacy/convenience access
	 * Derived from UnifiedItemCache, contains subset of fields.
	 */
	UPROPERTY()
	TMap<FName, FSuspenseCoreItemData> ItemCache;

	/** Loaded item DataTable reference */
	UPROPERTY()
	TObjectPtr<UDataTable> LoadedItemDataTable;

	/** Loaded character classes data asset */
	UPROPERTY()
	TObjectPtr<UDataAsset> LoadedCharacterClassesDataAsset;

	/** Loaded loadout DataTable reference */
	UPROPERTY()
	TObjectPtr<UDataTable> LoadedLoadoutDataTable;

	/** Cached EventBus reference */
	mutable TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	//========================================================================
	// GAS Attributes Cached Data (SSOT)
	//========================================================================

	/**
	 * Weapon attributes cache from WeaponAttributesDataTable
	 * Key: WeaponID or explicit row name
	 * Contains all 19 weapon attributes
	 * @see FSuspenseCoreWeaponAttributeRow
	 */
	UPROPERTY()
	TMap<FName, FSuspenseCoreWeaponAttributeRow> WeaponAttributesCache;

	/**
	 * Ammo attributes cache from AmmoAttributesDataTable
	 * Key: AmmoID or explicit row name
	 * Contains all 15 ammo attributes (Tarkov-style)
	 * @see FSuspenseCoreAmmoAttributeRow
	 */
	UPROPERTY()
	TMap<FName, FSuspenseCoreAmmoAttributeRow> AmmoAttributesCache;

	/**
	 * Armor attributes cache from ArmorAttributesDataTable
	 * Key: ArmorID or explicit row name
	 * @see FSuspenseCoreArmorAttributeRow
	 */
	UPROPERTY()
	TMap<FName, FSuspenseCoreArmorAttributeRow> ArmorAttributesCache;

	/**
	 * Throwable attributes cache from ThrowableAttributesDataTable
	 * Key: ThrowableID or explicit row name
	 * @see FSuspenseCoreThrowableAttributeRow
	 */
	UPROPERTY()
	TMap<FName, FSuspenseCoreThrowableAttributeRow> ThrowableAttributesCache;

	/** Loaded weapon attributes DataTable reference */
	UPROPERTY()
	TObjectPtr<UDataTable> LoadedWeaponAttributesDataTable;

	/** Loaded ammo attributes DataTable reference */
	UPROPERTY()
	TObjectPtr<UDataTable> LoadedAmmoAttributesDataTable;

	/** Loaded armor attributes DataTable reference */
	UPROPERTY()
	TObjectPtr<UDataTable> LoadedArmorAttributesDataTable;

	//========================================================================
	// Attachment Attributes Cached Data (Tarkov-Style Modifiers)
	//========================================================================

	/**
	 * Attachment attributes cache from AttachmentAttributesDataTable
	 * Key: AttachmentID or explicit row name
	 * Contains recoil modifiers, ergonomics bonuses, suppression flags
	 * @see FSuspenseCoreAttachmentAttributeRow
	 */
	UPROPERTY()
	TMap<FName, FSuspenseCoreAttachmentAttributeRow> AttachmentAttributesCache;

	/** Loaded attachment attributes DataTable reference */
	UPROPERTY()
	TObjectPtr<UDataTable> LoadedAttachmentAttributesDataTable;

	//========================================================================
	// Status Effect Attributes Cached Data (SSOT for Buffs/Debuffs)
	//========================================================================

	/**
	 * Status effect attributes cache from StatusEffectAttributesDataTable
	 * Key: EffectID
	 * SINGLE SOURCE OF TRUTH for all buffs and debuffs
	 * @see FSuspenseCoreStatusEffectAttributeRow
	 */
	UPROPERTY()
	TMap<FName, FSuspenseCoreStatusEffectAttributeRow> StatusEffectAttributesCache;

	/**
	 * Effect type tag to EffectID lookup map
	 * Enables fast lookup by GameplayTag
	 * Key: EffectTypeTag, Value: EffectID
	 */
	UPROPERTY()
	TMap<FGameplayTag, FName> StatusEffectTagToIDMap;

	/** Loaded status effect attributes DataTable reference */
	UPROPERTY()
	TObjectPtr<UDataTable> LoadedStatusEffectAttributesDataTable;

	//========================================================================
	// Magazine System Cached Data (Tarkov-Style)
	//========================================================================

	/**
	 * Magazine data cache from MagazineDataTable
	 * Key: MagazineID
	 * @see FSuspenseCoreMagazineData
	 */
	UPROPERTY()
	TMap<FName, FSuspenseCoreMagazineData> MagazineCache;

	/** Loaded magazine DataTable reference */
	UPROPERTY()
	TObjectPtr<UDataTable> LoadedMagazineDataTable;

	//========================================================================
	// Status Flags
	//========================================================================

	/** Overall initialization status */
	bool bIsInitialized = false;

	/** Item system ready flag */
	bool bItemSystemReady = false;

	/** Character system ready flag */
	bool bCharacterSystemReady = false;

	/** Loadout system ready flag */
	bool bLoadoutSystemReady = false;

	/** Weapon attributes system ready flag */
	bool bWeaponAttributesSystemReady = false;

	/** Ammo attributes system ready flag */
	bool bAmmoAttributesSystemReady = false;

	/** Armor attributes system ready flag */
	bool bArmorAttributesSystemReady = false;

	/** Attachment attributes system ready flag */
	bool bAttachmentAttributesSystemReady = false;

	/** Status effect attributes system ready flag */
	bool bStatusEffectSystemReady = false;

	/** Magazine system ready flag */
	bool bMagazineSystemReady = false;
};
