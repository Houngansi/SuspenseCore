// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/DataTable.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreItemDataTable.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryBaseTypes.h"
#include "SuspenseCoreItemManager.generated.h"

/**
 * Item manager subsystem for centralized item data access
 * Works with unified DataTable system as single source of truth
 * 
 * ARCHITECTURE PRINCIPLES:
 * - Single source of truth: DataTable configured at GameInstance level
 * - Explicit configuration: No hidden dependencies on hardcoded paths
 * - Fail-fast validation: Critical items validated during initialization
 * - Backward compatibility: Fallback to default path if not explicitly configured
 * 
 * INITIALIZATION FLOW:
 * 1. GameInstance calls LoadItemDataTable() with configured DataTable
 * 2. ItemManager validates data structure and builds cache
 * 3. Strict validation checks critical items referenced in loadouts
 * 4. Fallback: If not explicitly loaded, tries default path (with warning)
 * 
 * VALIDATION MODES:
 * - Standard: Logs errors but continues operation
 * - Strict: Blocks initialization if critical items fail validation
 */
UCLASS()
class BRIDGESYSTEM_API USuspenseItemManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()
    
public:
    //==================================================================
    // Subsystem lifecycle
    //==================================================================
    
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    
    //==================================================================
    // Core DataTable management - PRIMARY API
    //==================================================================
    
    /**
     * Load item data table and build cache (PRIMARY METHOD)
     * Should be called by GameInstance during initialization
     * 
     * @param ItemDataTable DataTable with FSuspenseUnifiedItemData rows
     * @param bStrictValidation If true, blocks on critical item validation failures
     * @return True if successfully loaded and validated
     */
    UFUNCTION(BlueprintCallable, Category = "Item Manager")
    bool LoadItemDataTable(UDataTable* ItemDataTable, bool bStrictValidation = false);
    
    /**
     * Get unified item data by ID (primary method)
     * @param ItemID Item identifier
     * @param OutItemData Output unified data from DataTable
     * @return True if item found in cache
     */
    UFUNCTION(BlueprintCallable, Category = "Item Manager")
    bool GetUnifiedItemData(const FName& ItemID, FSuspenseUnifiedItemData& OutItemData) const;
    
    /**
     * Check if item exists in the data table
     * @param ItemID Item identifier
     * @return True if item exists in loaded table
     */
    UFUNCTION(BlueprintCallable, Category = "Item Manager")
    bool HasItem(const FName& ItemID) const;
    
    /**
     * Get all registered item IDs
     * @return Array of all item IDs from loaded DataTable
     */
    UFUNCTION(BlueprintCallable, Category = "Item Manager")
    TArray<FName> GetAllItemIDs() const;
    
    //==================================================================
    // Item instance creation
    //==================================================================
    
    /**
     * Create properly initialized inventory item instance
     * This is the recommended way to create item instances
     * 
     * @param ItemID Item identifier from DataTable
     * @param Quantity Amount of items in stack
     * @param OutInstance Output fully initialized item instance
     * @return True if instance created successfully
     */
    UFUNCTION(BlueprintCallable, Category = "Item Manager")
    bool CreateItemInstance(const FName& ItemID, int32 Quantity, FSuspenseInventoryItemInstance& OutInstance) const;
    
    /**
     * Create multiple item instances from spawn data
     * Useful for loot generation and pickup spawning
     * 
     * @param SpawnDataArray Array of pickup spawn configurations
     * @param OutInstances Output array of created instances
     * @return Number of successfully created instances
     */
    UFUNCTION(BlueprintCallable, Category = "Item Manager")
    int32 CreateItemInstancesFromSpawnData(const TArray<FSuspensePickupSpawnData>& SpawnDataArray, TArray<FSuspenseInventoryItemInstance>& OutInstances) const;
    
    //==================================================================
    // Query and filtering methods
    //==================================================================
    
    /**
     * Get items by type tag matching
     * @param ItemType Type tag to filter by (supports hierarchical matching)
     * @return Array of item IDs matching the type
     */
    UFUNCTION(BlueprintCallable, Category = "Item Manager")
    TArray<FName> GetItemsByType(const FGameplayTag& ItemType) const;
    
    /**
     * Get items by tag container matching
     * @param Tags Tags to match against (ANY matching)
     * @return Array of item IDs matching any of the tags
     */
    UFUNCTION(BlueprintCallable, Category = "Item Manager")
    TArray<FName> GetItemsByTags(const FGameplayTagContainer& Tags) const;
    
    /**
     * Get items by rarity
     * @param Rarity Rarity tag to filter by
     * @return Array of item IDs with specified rarity
     */
    UFUNCTION(BlueprintCallable, Category = "Item Manager")
    TArray<FName> GetItemsByRarity(const FGameplayTag& Rarity) const;
    
    /**
     * Get equippable items for specific slot
     * @param SlotType Equipment slot tag
     * @return Array of item IDs that can be equipped in the slot
     */
    UFUNCTION(BlueprintCallable, Category = "Item Manager")
    TArray<FName> GetEquippableItemsForSlot(const FGameplayTag& SlotType) const;
    
    /**
     * Get weapons by archetype
     * @param WeaponArchetype Weapon type tag
     * @return Array of weapon item IDs matching the archetype
     */
    UFUNCTION(BlueprintCallable, Category = "Item Manager")
    TArray<FName> GetWeaponsByArchetype(const FGameplayTag& WeaponArchetype) const;
    
    /**
     * Get ammo compatible with weapon
     * @param WeaponItemID Weapon item identifier
     * @return Array of ammo item IDs compatible with the weapon
     */
    UFUNCTION(BlueprintCallable, Category = "Item Manager")
    TArray<FName> GetCompatibleAmmoForWeapon(const FName& WeaponItemID) const;
    
    //==================================================================
    // Validation and debugging
    //==================================================================
    
    /**
     * Validate all items in the data table
     * Performs comprehensive validation of item data integrity
     * @param OutErrorMessages Optional array to receive detailed error messages
     * @return Number of validation errors found (0 = all valid)
     */
    UFUNCTION(BlueprintCallable, Category = "Item Manager|Debug")
    int32 ValidateAllItems(TArray<FString>& OutErrorMessages) const;
    
    /**
     * Validate specific item
     * @param ItemID Item to validate
     * @param OutErrorMessages Output array of error messages
     * @return True if item passes validation
     */
    UFUNCTION(BlueprintCallable, Category = "Item Manager|Debug")
    bool ValidateItem(const FName& ItemID, TArray<FString>& OutErrorMessages) const;
    
    /**
     * Get cache statistics for debugging
     * @param OutStats Output string with cache information
     */
    UFUNCTION(BlueprintCallable, Category = "Item Manager|Debug")
    void GetCacheStatistics(FString& OutStats) const;
    
    /**
     * Refresh cache from current data table
     * Useful for hot-reloading in development
     * @return True if cache rebuilt successfully
     */
    UFUNCTION(BlueprintCallable, Category = "Item Manager|Debug")
    bool RefreshCache();
    
    /**
     * Check if item system was explicitly configured by GameInstance
     * @return True if LoadItemDataTable was called with valid DataTable
     */
    UFUNCTION(BlueprintCallable, Category = "Item Manager|Debug")
    bool IsExplicitlyConfigured() const { return bIsExplicitlyConfigured; }
    
    /**
     * Check if strict validation mode is enabled
     * @return True if strict validation is active
     */
    UFUNCTION(BlueprintCallable, Category = "Item Manager|Debug")
    bool IsStrictValidationEnabled() const { return bStrictValidationEnabled; }
    
    //==================================================================
    // Asset access
    //==================================================================
    
    /**
     * Get currently loaded DataTable asset
     * @return DataTable reference or nullptr if not loaded
     */
    UFUNCTION(BlueprintCallable, Category = "Item Manager")
    UDataTable* GetItemDataTable() const { return ItemTable; }
    
    /**
     * Get number of items in cache
     * @return Number of cached items
     */
    UFUNCTION(BlueprintCallable, Category = "Item Manager")
    int32 GetCachedItemCount() const { return UnifiedItemCache.Num(); }
    
    /**
     * Get number of valid items (passed validation)
     * @return Number of items that passed validation
     */
    UFUNCTION(BlueprintCallable, Category = "Item Manager")
    int32 GetValidItemCount() const { return ValidItemCount; }
    
    //==================================================================
    // Legacy support methods (deprecated but maintained for compatibility)
    //==================================================================
    
    /**
     * DEPRECATED: Create legacy inventory item data
     * 
     * This method exists only for backwards compatibility during migration.
     * New code should use CreateItemInstance() instead.
     * 
     * @param ItemID Item identifier
     * @param Quantity Amount of items
     * @param OutInstance Output item instance (replaces old inventory data)
     * @return True if instance created successfully
     */
    UFUNCTION(BlueprintCallable, Category = "Item Manager|Legacy", 
              meta = (DeprecatedFunction, DeprecationMessage = "Use CreateItemInstance instead. Legacy inventory data structures have been replaced with FSuspenseInventoryItemInstance."))
    bool CreateInventoryItemData(const FName& ItemID, int32 Quantity, FSuspenseInventoryItemInstance& OutInstance) const;
    
private:
    //==================================================================
    // Internal data members
    //==================================================================
    
    /** Currently loaded DataTable with unified item data */
    UPROPERTY()
    UDataTable* ItemTable;
    
    /** Fast access cache for item data */
    TMap<FName, FSuspenseUnifiedItemData> UnifiedItemCache;
    
    /** Number of items that passed validation */
    int32 ValidItemCount;
    
    /** Cache statistics for debugging */
    mutable int32 CacheHits;
    mutable int32 CacheMisses;
    
    /** Flag indicating DataTable was explicitly set by GameInstance */
    bool bIsExplicitlyConfigured;
    
    /** Flag indicating strict validation mode is enabled */
    bool bStrictValidationEnabled;
    
    /** Default table path for fallback loading (backwards compatibility) */
    static constexpr const TCHAR* DefaultItemTablePath = TEXT("/Game/SuspenseCore/Data/DT_Items");
    
    //==================================================================
    // Internal helper methods
    //==================================================================
    
    /**
     * Try to load fallback table from default path
     * Called only if LoadItemDataTable was not explicitly called
     * Logs warning to direct developers to proper configuration
     * @return True if fallback table loaded successfully
     */
    bool TryLoadFallbackTable();
    
    /**
     * Build cache from currently loaded table
     * Validates items during cache building
     * @param bStrictMode If true, blocks on validation failures
     * @return True if cache built successfully (or with warnings in non-strict mode)
     */
    bool BuildItemCache(bool bStrictMode);
    
    /**
     * Validate single item data (internal)
     * @param ItemID Item identifier for logging
     * @param ItemData Item data to validate
     * @param OutErrors Output array of validation errors
     * @return True if item passes validation
     */
    bool ValidateItemInternal(const FName& ItemID, const FSuspenseUnifiedItemData& ItemData, TArray<FString>& OutErrors) const;
    
    /**
     * Log cache building statistics
     * @param ValidItems Number of items that passed validation
     * @param WeaponItems Number of weapons
     * @param ArmorItems Number of armor pieces
     * @param ConsumableItems Number of consumables
     * @param AmmoItems Number of ammunition types
     */
    void LogCacheStatistics(int32 ValidItems, int32 WeaponItems, int32 ArmorItems, int32 ConsumableItems, int32 AmmoItems) const;
    
    /**
     * Helper to get item data with cache statistics
     * Tracks cache hits/misses for performance monitoring
     * @param ItemID Item identifier
     * @return Pointer to cached item data or nullptr
     */
    const FSuspenseUnifiedItemData* GetCachedItemData(const FName& ItemID) const;

 /**
     * Initialize runtime properties for item instance based on item type
     * This is used internally by ItemManager to avoid circular dependency with InventoryUtils
     * 
     * @param Instance Item instance to initialize (modified in place)
     * @param ItemData Static item data from DataTable
     */
 void InitializeItemRuntimeProperties(FSuspenseInventoryItemInstance& Instance, const FSuspenseUnifiedItemData& ItemData) const;
};
