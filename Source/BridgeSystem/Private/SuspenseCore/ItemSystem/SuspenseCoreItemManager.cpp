// Copyright Suspense Team. All Rights Reserved.

#include "ItemSystem/SuspenseItemManager.h"
#include "Engine/AssetManager.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"

// Include InventoryUtils for item instance creation
// This ensures proper initialization of runtime properties
namespace InventoryUtils
{
    extern FSuspenseInventoryItemInstance CreateItemInstance(const FName& ItemID, int32 Quantity);
    extern bool GetUnifiedItemData(const FName& ItemID, FSuspenseUnifiedItemData& OutData);
}

DEFINE_LOG_CATEGORY_STATIC(LogMedComItemManager, Log, All);

//==================================================================
// Subsystem lifecycle implementation
//==================================================================

void USuspenseItemManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    UE_LOG(LogMedComItemManager, Log, TEXT("=== ItemManager: Subsystem initialization START ==="));
    
    // Initialize internal state only - NO data loading here
    // Data loading must be done explicitly by GameInstance via LoadItemDataTable()
    ItemTable = nullptr;
    ValidItemCount = 0;
    CacheHits = 0;
    CacheMisses = 0;
    bIsExplicitlyConfigured = false;
    bStrictValidationEnabled = false;
    UnifiedItemCache.Empty();
    
    UE_LOG(LogMedComItemManager, Log, TEXT("ItemManager: Internal state initialized"));
    UE_LOG(LogMedComItemManager, Log, TEXT("ItemManager: Waiting for explicit LoadItemDataTable() call from GameInstance"));
    UE_LOG(LogMedComItemManager, Log, TEXT("=== ItemManager: Subsystem initialization COMPLETE ==="));
}

void USuspenseItemManager::Deinitialize()
{
    UE_LOG(LogMedComItemManager, Log, TEXT("ItemManager: Shutting down subsystem"));
    
    // Clear cache and references
    UnifiedItemCache.Empty();
    ItemTable = nullptr;
    ValidItemCount = 0;
    CacheHits = 0;
    CacheMisses = 0;
    bIsExplicitlyConfigured = false;
    bStrictValidationEnabled = false;
    
    Super::Deinitialize();
}

//==================================================================
// Core DataTable management implementation - PRIMARY API
//==================================================================

bool USuspenseItemManager::LoadItemDataTable(UDataTable* ItemDataTable, bool bStrictValidation)
{
    UE_LOG(LogMedComItemManager, Warning, TEXT("=== ItemManager: LoadItemDataTable START ==="));
    
    if (!ItemDataTable)
    {
        UE_LOG(LogMedComItemManager, Error, TEXT("LoadItemDataTable: DataTable is null"));
        UE_LOG(LogMedComItemManager, Error, TEXT("ItemManager cannot function without item data"));
        return false;
    }
    
    // Verify row structure matches our unified data format
    const UScriptStruct* RowStruct = ItemDataTable->GetRowStruct();
    if (RowStruct != FSuspenseUnifiedItemData::StaticStruct())
    {
        UE_LOG(LogMedComItemManager, Error, TEXT("LoadItemDataTable: Invalid row structure"));
        UE_LOG(LogMedComItemManager, Error, TEXT("  Expected: %s"), *FSuspenseUnifiedItemData::StaticStruct()->GetName());
        UE_LOG(LogMedComItemManager, Error, TEXT("  Got: %s"), RowStruct ? *RowStruct->GetName() : TEXT("nullptr"));
        UE_LOG(LogMedComItemManager, Error, TEXT("  Please ensure your DataTable uses FSuspenseUnifiedItemData row structure"));
        return false;
    }
    
    // Clear existing cache before loading new data
    UnifiedItemCache.Empty();
    ValidItemCount = 0;
    CacheHits = 0;
    CacheMisses = 0;
    
    // Save table reference
    ItemTable = ItemDataTable;
    bIsExplicitlyConfigured = true;
    bStrictValidationEnabled = bStrictValidation;
    
    UE_LOG(LogMedComItemManager, Log, TEXT("ItemManager: Explicitly configured with DataTable: %s"), *ItemDataTable->GetName());
    UE_LOG(LogMedComItemManager, Log, TEXT("ItemManager: Strict validation mode: %s"), bStrictValidation ? TEXT("ENABLED") : TEXT("DISABLED"));
    
    // Build cache from table data
    bool bBuildSuccess = BuildItemCache(bStrictValidation);
    
    if (!bBuildSuccess)
    {
        UE_LOG(LogMedComItemManager, Error, TEXT("LoadItemDataTable: Failed to build item cache"));
        
        if (bStrictValidation)
        {
            UE_LOG(LogMedComItemManager, Error, TEXT("CRITICAL: Strict validation failed - ItemManager initialization blocked"));
            UE_LOG(LogMedComItemManager, Error, TEXT("Game cannot start with invalid critical items"));
            
            // Clear the cache to prevent using invalid data
            UnifiedItemCache.Empty();
            ItemTable = nullptr;
            ValidItemCount = 0;
            bIsExplicitlyConfigured = false;
            
            return false;
        }
        else
        {
            UE_LOG(LogMedComItemManager, Warning, TEXT("Cache building completed with warnings"));
            UE_LOG(LogMedComItemManager, Warning, TEXT("Some items may not work correctly, but system will continue"));
        }
    }
    
    UE_LOG(LogMedComItemManager, Warning, TEXT("ItemManager: Successfully loaded and cached item data"));
    UE_LOG(LogMedComItemManager, Warning, TEXT("  DataTable Asset: %s"), *ItemDataTable->GetName());
    UE_LOG(LogMedComItemManager, Warning, TEXT("  Total Items Cached: %d"), UnifiedItemCache.Num());
    UE_LOG(LogMedComItemManager, Warning, TEXT("  Valid Items: %d"), ValidItemCount);
    
    UE_LOG(LogMedComItemManager, Warning, TEXT("=== ItemManager: LoadItemDataTable COMPLETE ==="));
    
    return true;
}

bool USuspenseItemManager::GetUnifiedItemData(const FName& ItemID, FSuspenseUnifiedItemData& OutItemData) const
{
    // Use helper method for cache statistics tracking
    const FSuspenseUnifiedItemData* FoundItem = GetCachedItemData(ItemID);
    if (FoundItem)
    {
        OutItemData = *FoundItem;
        return true;
    }
    
    UE_LOG(LogMedComItemManager, Warning, TEXT("GetUnifiedItemData: Item '%s' not found in cache"), 
        *ItemID.ToString());
    
    // If not explicitly configured, suggest fallback
    if (!bIsExplicitlyConfigured)
    {
        UE_LOG(LogMedComItemManager, Warning, TEXT("  ItemManager was not explicitly configured by GameInstance"));
        UE_LOG(LogMedComItemManager, Warning, TEXT("  Make sure ItemDataTable is set in BP_MedComGameInstance"));
    }
    
    return false;
}

bool USuspenseItemManager::HasItem(const FName& ItemID) const
{
    return UnifiedItemCache.Contains(ItemID);
}

TArray<FName> USuspenseItemManager::GetAllItemIDs() const
{
    TArray<FName> Result;
    UnifiedItemCache.GetKeys(Result);
    return Result;
}

//==================================================================
// Item instance creation implementation
//==================================================================

bool USuspenseItemManager::CreateItemInstance(const FName& ItemID, int32 Quantity, FSuspenseInventoryItemInstance& OutInstance) const
{
    // Verify item exists in our cache
    if (!HasItem(ItemID))
    {
        UE_LOG(LogMedComItemManager, Warning, TEXT("CreateItemInstance: Item not found: %s"), 
               *ItemID.ToString());
        return false;
    }
    
    // АРХИТЕКТУРНОЕ УЛУЧШЕНИЕ:
    // ItemManager НЕ использует InventoryUtils для создания инстансов
    // Вместо этого мы делаем всё сами, потому что у нас уже есть доступ к данным
    // Это устраняет циклическую зависимость и делает код более эффективным
    
    // Get item data directly from our cache
    FSuspenseUnifiedItemData ItemData;
    if (!GetUnifiedItemData(ItemID, ItemData))
    {
        UE_LOG(LogMedComItemManager, Error, TEXT("CreateItemInstance: Failed to get item data for: %s"), 
               *ItemID.ToString());
        return false;
    }
    
    // Create base instance with ID and quantity using factory method
    OutInstance = FSuspenseInventoryItemInstance::Create(ItemID, Quantity);
    
    // Initialize runtime properties based on item type
    // This logic mirrors what InventoryUtils does but without the circular dependency
    InitializeItemRuntimeProperties(OutInstance, ItemData);
    
    // Verify the instance was created successfully
    if (!OutInstance.IsValid())
    {
        UE_LOG(LogMedComItemManager, Error, TEXT("CreateItemInstance: Failed to create valid instance for: %s"), 
               *ItemID.ToString());
        return false;
    }
    
    UE_LOG(LogMedComItemManager, VeryVerbose, TEXT("ItemManager: Created item instance: %s"), 
        *OutInstance.GetShortDebugString());
    
    return true;
}
int32 USuspenseItemManager::CreateItemInstancesFromSpawnData(const TArray<FSuspensePickupSpawnData>& SpawnDataArray, TArray<FSuspenseInventoryItemInstance>& OutInstances) const
{
    OutInstances.Empty();
    OutInstances.Reserve(SpawnDataArray.Num());
    
    int32 SuccessfulCreations = 0;
    
    for (const FSuspensePickupSpawnData& SpawnData : SpawnDataArray)
    {
        if (!SpawnData.IsValid())
        {
            UE_LOG(LogMedComItemManager, Warning, TEXT("CreateItemInstancesFromSpawnData: Invalid spawn data for item: %s"), 
                *SpawnData.ItemID.ToString());
            continue;
        }
        
        FSuspenseInventoryItemInstance NewInstance;
        if (CreateItemInstance(SpawnData.ItemID, SpawnData.Quantity, NewInstance))
        {
            // Apply any preset runtime properties from spawn data
            for (const auto& PropertyPair : SpawnData.PresetRuntimeProperties)
            {
                NewInstance.SetRuntimeProperty(PropertyPair.Key, PropertyPair.Value);
            }
            
            OutInstances.Add(NewInstance);
            SuccessfulCreations++;
        }
    }
    
    UE_LOG(LogMedComItemManager, Log, TEXT("ItemManager: Created %d/%d item instances from spawn data"), 
        SuccessfulCreations, SpawnDataArray.Num());
    
    return SuccessfulCreations;
}

//==================================================================
// Query and filtering methods implementation
//==================================================================

TArray<FName> USuspenseItemManager::GetItemsByType(const FGameplayTag& ItemType) const
{
    TArray<FName> Result;
    
    for (const auto& Pair : UnifiedItemCache)
    {
        // Use hierarchical tag matching - this allows filtering by parent tags
        // For example, "Item.Type.Weapon" will match "Item.Type.Weapon.Rifle"
        if (Pair.Value.GetEffectiveItemType().MatchesTag(ItemType))
        {
            Result.Add(Pair.Key);
        }
    }
    
    UE_LOG(LogMedComItemManager, VeryVerbose, TEXT("GetItemsByType: Found %d items of type '%s'"), 
        Result.Num(), *ItemType.ToString());
    
    return Result;
}

TArray<FName> USuspenseItemManager::GetItemsByTags(const FGameplayTagContainer& Tags) const
{
    TArray<FName> Result;
    
    if (Tags.IsEmpty())
    {
        UE_LOG(LogMedComItemManager, Warning, TEXT("GetItemsByTags: Empty tag container provided"));
        return Result;
    }
    
    for (const auto& Pair : UnifiedItemCache)
    {
        if (Pair.Value.MatchesTags(Tags))
        {
            Result.Add(Pair.Key);
        }
    }
    
    return Result;
}

TArray<FName> USuspenseItemManager::GetItemsByRarity(const FGameplayTag& Rarity) const
{
    TArray<FName> Result;
    
    for (const auto& Pair : UnifiedItemCache)
    {
        if (Pair.Value.Rarity.MatchesTagExact(Rarity))
        {
            Result.Add(Pair.Key);
        }
    }
    
    return Result;
}

TArray<FName> USuspenseItemManager::GetEquippableItemsForSlot(const FGameplayTag& SlotType) const
{
    TArray<FName> Result;
    
    for (const auto& Pair : UnifiedItemCache)
    {
        const FSuspenseUnifiedItemData& ItemData = Pair.Value;
        if (ItemData.bIsEquippable && ItemData.EquipmentSlot.MatchesTagExact(SlotType))
        {
            Result.Add(Pair.Key);
        }
    }
    
    return Result;
}

TArray<FName> USuspenseItemManager::GetWeaponsByArchetype(const FGameplayTag& WeaponArchetype) const
{
    TArray<FName> Result;
    
    for (const auto& Pair : UnifiedItemCache)
    {
        const FSuspenseUnifiedItemData& ItemData = Pair.Value;
        if (ItemData.bIsWeapon && ItemData.WeaponArchetype.MatchesTag(WeaponArchetype))
        {
            Result.Add(Pair.Key);
        }
    }
    
    return Result;
}

TArray<FName> USuspenseItemManager::GetCompatibleAmmoForWeapon(const FName& WeaponItemID) const
{
    TArray<FName> Result;
    
    // First get the weapon data to find its ammo type
    const FSuspenseUnifiedItemData* WeaponData = GetCachedItemData(WeaponItemID);
    if (!WeaponData || !WeaponData->bIsWeapon)
    {
        UE_LOG(LogMedComItemManager, Warning, TEXT("GetCompatibleAmmoForWeapon: Invalid weapon ID: %s"), 
            *WeaponItemID.ToString());
        return Result;
    }
    
    // Find ammo that matches the weapon's ammo type
    for (const auto& Pair : UnifiedItemCache)
    {
        const FSuspenseUnifiedItemData& ItemData = Pair.Value;
        if (ItemData.bIsAmmo)
        {
            // Check if ammo caliber matches weapon's ammo type
            if (ItemData.AmmoCaliber.MatchesTagExact(WeaponData->AmmoType))
            {
                Result.Add(Pair.Key);
            }
            // Also check if weapon type is in ammo's compatible weapons list
            else if (ItemData.CompatibleWeapons.HasTag(WeaponData->WeaponArchetype))
            {
                Result.Add(Pair.Key);
            }
        }
    }
    
    return Result;
}

//==================================================================
// Validation and debugging implementation
//==================================================================

int32 USuspenseItemManager::ValidateAllItems(TArray<FString>& OutErrorMessages) const
{
    OutErrorMessages.Empty();
    int32 TotalErrors = 0;
    
    for (const auto& Pair : UnifiedItemCache)
    {
        TArray<FString> ItemErrors;
        if (!ValidateItemInternal(Pair.Key, Pair.Value, ItemErrors))
        {
            TotalErrors++;
            
            // Add item header to error messages
            OutErrorMessages.Add(FString::Printf(TEXT("Item '%s' validation errors:"), *Pair.Key.ToString()));
            
            // Add individual errors with indentation
            for (const FString& Error : ItemErrors)
            {
                OutErrorMessages.Add(FString::Printf(TEXT("  - %s"), *Error));
            }
        }
    }
    
    if (TotalErrors > 0)
    {
        UE_LOG(LogMedComItemManager, Warning, TEXT("ValidateAllItems: Found %d items with validation errors"), TotalErrors);
    }
    else
    {
        UE_LOG(LogMedComItemManager, Log, TEXT("ValidateAllItems: All items passed validation"));
    }
    
    return TotalErrors;
}

bool USuspenseItemManager::ValidateItem(const FName& ItemID, TArray<FString>& OutErrorMessages) const
{
    OutErrorMessages.Empty();
    
    const FSuspenseUnifiedItemData* ItemData = GetCachedItemData(ItemID);
    if (!ItemData)
    {
        OutErrorMessages.Add(TEXT("Item not found in cache"));
        return false;
    }
    
    return ValidateItemInternal(ItemID, *ItemData, OutErrorMessages);
}

void USuspenseItemManager::GetCacheStatistics(FString& OutStats) const
{
    float HitRate = 0.0f;
    int32 TotalAccesses = CacheHits + CacheMisses;
    
    if (TotalAccesses > 0)
    {
        HitRate = (float(CacheHits) / float(TotalAccesses)) * 100.0f;
    }
    
    FString Stats = FString::Printf(
        TEXT("ItemManager Cache Statistics:\n")
        TEXT("  Configuration Mode: %s\n")
        TEXT("  Strict Validation: %s\n")
        TEXT("  Total Items: %d\n")
        TEXT("  Valid Items: %d\n")
        TEXT("  Cache Hits: %d\n")
        TEXT("  Cache Misses: %d\n")
        TEXT("  Hit Rate: %.2f%%\n")
        TEXT("  DataTable: %s"),
        bIsExplicitlyConfigured ? TEXT("Explicit (GameInstance)") : TEXT("Fallback (Default Path)"),
        bStrictValidationEnabled ? TEXT("Enabled") : TEXT("Disabled"),
        UnifiedItemCache.Num(),
        ValidItemCount,
        CacheHits,
        CacheMisses,
        HitRate,
        ItemTable ? *ItemTable->GetName() : TEXT("None")
    );
    
    OutStats = Stats;
}

bool USuspenseItemManager::RefreshCache()
{
    if (!ItemTable)
    {
        UE_LOG(LogMedComItemManager, Warning, TEXT("RefreshCache: No DataTable loaded"));
        return false;
    }
    
    UE_LOG(LogMedComItemManager, Log, TEXT("ItemManager: Refreshing item cache"));
    
    // Clear existing cache
    UnifiedItemCache.Empty();
    ValidItemCount = 0;
    CacheHits = 0;
    CacheMisses = 0;
    
    // Rebuild from current table with current validation mode
    bool bSuccess = BuildItemCache(bStrictValidationEnabled);
    
    if (bSuccess)
    {
        UE_LOG(LogMedComItemManager, Log, TEXT("ItemManager: Cache refreshed successfully"));
    }
    else
    {
        UE_LOG(LogMedComItemManager, Warning, TEXT("ItemManager: Cache refresh completed with warnings"));
    }
    
    return bSuccess;
}

//==================================================================
// Legacy support implementation
//==================================================================

bool USuspenseItemManager::CreateInventoryItemData(const FName& ItemID, int32 Quantity, FSuspenseInventoryItemInstance& OutInstance) const
{
    UE_LOG(LogMedComItemManager, Warning, TEXT("CreateInventoryItemData: Using deprecated method. Please migrate to CreateItemInstance()."));
    
    // Forward to new method - this maintains compatibility while encouraging migration
    return CreateItemInstance(ItemID, Quantity, OutInstance);
}

//==================================================================
// Internal helper methods implementation
//==================================================================

bool USuspenseItemManager::TryLoadFallbackTable()
{
    UE_LOG(LogMedComItemManager, Warning, TEXT("=== ItemManager: TryLoadFallbackTable START ==="));
    UE_LOG(LogMedComItemManager, Warning, TEXT("WARNING: ItemManager was not explicitly configured by GameInstance"));
    UE_LOG(LogMedComItemManager, Warning, TEXT("WARNING: Attempting fallback to default path: %s"), DefaultItemTablePath);
    UE_LOG(LogMedComItemManager, Warning, TEXT("WARNING: This is NOT recommended for production"));
    UE_LOG(LogMedComItemManager, Warning, TEXT("RECOMMENDED: Set ItemDataTable in BP_MedComGameInstance instead"));
    
    // Try to load from default path
    UDataTable* FallbackTable = LoadObject<UDataTable>(nullptr, DefaultItemTablePath);
    
    if (FallbackTable)
    {
        UE_LOG(LogMedComItemManager, Warning, TEXT("ItemManager: Fallback table loaded from default path"));
        
        // Use LoadItemDataTable with non-strict validation for fallback
        // We don't want to block startup for fallback scenarios
        bool bLoadSuccess = LoadItemDataTable(FallbackTable, false);
        
        if (bLoadSuccess)
        {
            UE_LOG(LogMedComItemManager, Log, TEXT("ItemManager: Fallback initialization successful"));
            UE_LOG(LogMedComItemManager, Warning, TEXT("=== ItemManager: TryLoadFallbackTable COMPLETE (SUCCESS) ==="));
            return true;
        }
        else
        {
            UE_LOG(LogMedComItemManager, Error, TEXT("ItemManager: Fallback table loaded but failed validation"));
        }
    }
    else
    {
        UE_LOG(LogMedComItemManager, Error, TEXT("ItemManager: Failed to load fallback table from: %s"), DefaultItemTablePath);
    }
    
    UE_LOG(LogMedComItemManager, Error, TEXT("=== ItemManager: TryLoadFallbackTable COMPLETE (FAILED) ==="));
    return false;
}

bool USuspenseItemManager::BuildItemCache(bool bStrictMode)
{
    UnifiedItemCache.Empty();
    ValidItemCount = 0;
    
    if (!ItemTable)
    {
        UE_LOG(LogMedComItemManager, Error, TEXT("BuildItemCache: ItemTable is null"));
        return false;
    }
    
    TArray<FName> RowNames = ItemTable->GetRowNames();
    
    UE_LOG(LogMedComItemManager, Log, TEXT("ItemManager: Building cache from %d rows (Strict mode: %s)"), 
        RowNames.Num(), bStrictMode ? TEXT("ON") : TEXT("OFF"));
    
    int32 WeaponItems = 0;
    int32 ArmorItems = 0;
    int32 ConsumableItems = 0;
    int32 AmmoItems = 0;
    int32 ValidationErrors = 0;
    
    for (const FName& RowName : RowNames)
    {
        FSuspenseUnifiedItemData* ItemData = ItemTable->FindRow<FSuspenseUnifiedItemData>(RowName, TEXT("ItemManager::BuildItemCache"));
        if (ItemData)
        {
            // Use row name as ItemID if not set
            if (ItemData->ItemID.IsNone())
            {
                ItemData->ItemID = RowName;
                UE_LOG(LogMedComItemManager, Warning, TEXT("ItemManager: Row '%s' has empty ItemID, using row name"), 
                    *RowName.ToString());
            }
            
            // Validate item data
            TArray<FString> ItemValidationErrors;
            bool bItemValid = ValidateItemInternal(ItemData->ItemID, *ItemData, ItemValidationErrors);
            
            if (bItemValid)
            {
                ValidItemCount++;
                
                // Count item types for statistics
                if (ItemData->bIsWeapon) WeaponItems++;
                if (ItemData->bIsArmor) ArmorItems++;
                if (ItemData->bIsConsumable) ConsumableItems++;
                if (ItemData->bIsAmmo) AmmoItems++;
            }
            else
            {
                ValidationErrors++;
                
                UE_LOG(LogMedComItemManager, Warning, TEXT("ItemManager: Item '%s' has %d validation errors:"), 
                    *ItemData->ItemID.ToString(), ItemValidationErrors.Num());
                    
                for (const FString& Error : ItemValidationErrors)
                {
                    UE_LOG(LogMedComItemManager, Warning, TEXT("  - %s"), *Error);
                }
                
                // In strict mode, validation failure is critical
                if (bStrictMode)
                {
                    UE_LOG(LogMedComItemManager, Error, TEXT("STRICT MODE: Item '%s' failed validation"), 
                        *ItemData->ItemID.ToString());
                }
            }
            
            // Add to cache regardless of validation (allows for debugging invalid items)
            // but mark them internally so they can be filtered out in production
            UnifiedItemCache.Add(ItemData->ItemID, *ItemData);
        }
        else
        {
            UE_LOG(LogMedComItemManager, Error, TEXT("ItemManager: Failed to get data for row '%s'"), 
                *RowName.ToString());
            ValidationErrors++;
            
            if (bStrictMode)
            {
                UE_LOG(LogMedComItemManager, Error, TEXT("STRICT MODE: Cannot read row '%s'"), *RowName.ToString());
            }
        }
    }
    
    // Log detailed statistics
    LogCacheStatistics(ValidItemCount, WeaponItems, ArmorItems, ConsumableItems, AmmoItems);
    
    // In strict mode, any validation errors = failure
    if (bStrictMode && ValidationErrors > 0)
    {
        UE_LOG(LogMedComItemManager, Error, TEXT("STRICT MODE FAILURE: %d items failed validation"), ValidationErrors);
        UE_LOG(LogMedComItemManager, Error, TEXT("Cache building blocked due to strict validation requirements"));
        return false;
    }
    
    // In non-strict mode, we continue but log warnings
    if (ValidationErrors > 0)
    {
        UE_LOG(LogMedComItemManager, Warning, TEXT("Cache built with %d validation errors"), ValidationErrors);
        UE_LOG(LogMedComItemManager, Warning, TEXT("Some items may not function correctly"));
    }
    
    return true;
}

bool USuspenseItemManager::ValidateItemInternal(const FName& ItemID, const FSuspenseUnifiedItemData& ItemData, TArray<FString>& OutErrors) const
{
    OutErrors.Empty();
    
    // Use the built-in validation from the item data structure
    TArray<FText> ValidationErrors = ItemData.GetValidationErrors();
    
    // Convert FText errors to FString for our interface
    for (const FText& ErrorText : ValidationErrors)
    {
        OutErrors.Add(ErrorText.ToString());
    }
    
    return ValidationErrors.Num() == 0;
}

void USuspenseItemManager::LogCacheStatistics(int32 TotalItems, int32 WeaponItems, int32 ArmorItems, int32 ConsumableItems, int32 AmmoItems) const
{
    UE_LOG(LogMedComItemManager, Warning, TEXT("====== ItemManager: Cache Statistics ======"));
    UE_LOG(LogMedComItemManager, Warning, TEXT("  Configuration: %s"), 
        bIsExplicitlyConfigured ? TEXT("Explicit (via GameInstance)") : TEXT("Fallback (default path)"));
    UE_LOG(LogMedComItemManager, Warning, TEXT("  Validation Mode: %s"), 
        bStrictValidationEnabled ? TEXT("STRICT") : TEXT("Standard"));
    UE_LOG(LogMedComItemManager, Warning, TEXT("  Total Items Cached: %d"), UnifiedItemCache.Num());
    UE_LOG(LogMedComItemManager, Warning, TEXT("  Valid Items: %d"), TotalItems);
    UE_LOG(LogMedComItemManager, Warning, TEXT("  Weapons: %d"), WeaponItems);
    UE_LOG(LogMedComItemManager, Warning, TEXT("  Armor: %d"), ArmorItems);
    UE_LOG(LogMedComItemManager, Warning, TEXT("  Consumables: %d"), ConsumableItems);
    UE_LOG(LogMedComItemManager, Warning, TEXT("  Ammunition: %d"), AmmoItems);
    UE_LOG(LogMedComItemManager, Warning, TEXT("=========================================="));
}

const FSuspenseUnifiedItemData* USuspenseItemManager::GetCachedItemData(const FName& ItemID) const
{
    const FSuspenseUnifiedItemData* FoundItem = UnifiedItemCache.Find(ItemID);
    
    // Track cache statistics for performance monitoring
    if (FoundItem)
    {
        CacheHits++;
    }
    else
    {
        CacheMisses++;
    }
    
    return FoundItem;
}

//==================================================================
// Internal Item Instance Initialization
//==================================================================

void USuspenseItemManager::InitializeItemRuntimeProperties(FSuspenseInventoryItemInstance& Instance, const FSuspenseUnifiedItemData& ItemData) const
{
    // This method contains the SAME logic as InventoryUtils initialization functions
    // but is implemented directly in ItemManager to avoid circular dependencies
    
    // Initialize durability for equippable items
    if (ItemData.bIsEquippable)
    {
        // STUB: Get default max durability based on item type
        // TODO: Replace with AttributeSet values after GAS integration
        float MaxDurability = 100.0f;
        
        if (ItemData.bIsWeapon)
        {
            MaxDurability = 150.0f; // Weapons have medium durability
        }
        else if (ItemData.bIsArmor)
        {
            MaxDurability = 200.0f; // Armor has higher durability
        }
        
        Instance.SetRuntimeProperty(TEXT("MaxDurability"), MaxDurability);
        Instance.SetRuntimeProperty(TEXT("Durability"), MaxDurability); // Start at full durability
        
        UE_LOG(LogMedComItemManager, VeryVerbose, 
            TEXT("InitializeItemRuntimeProperties: Set durability for %s: %.1f/%.1f"), 
            *ItemData.ItemID.ToString(), MaxDurability, MaxDurability);
    }
    
    // Initialize ammo for weapons
    if (ItemData.bIsWeapon)
    {
        // STUB: Get default ammo capacity based on weapon archetype
        // TODO: Replace with AmmoAttributeSet values after GAS integration
        int32 MaxAmmo = 30; // Default capacity
        
        if (ItemData.WeaponArchetype.IsValid())
        {
            FString ArchetypeString = ItemData.WeaponArchetype.ToString();
            
            // Simple logic based on weapon type name (will be replaced with AttributeSet)
            if (ArchetypeString.Contains(TEXT("Rifle")))
            {
                MaxAmmo = 30;
            }
            else if (ArchetypeString.Contains(TEXT("Pistol")))
            {
                MaxAmmo = 15;
            }
            else if (ArchetypeString.Contains(TEXT("Shotgun")))
            {
                MaxAmmo = 8;
            }
            else if (ArchetypeString.Contains(TEXT("Sniper")))
            {
                MaxAmmo = 5;
            }
            else if (ArchetypeString.Contains(TEXT("SMG")) || ArchetypeString.Contains(TEXT("Submachine")))
            {
                MaxAmmo = 25;
            }
            else if (ArchetypeString.Contains(TEXT("LMG")) || ArchetypeString.Contains(TEXT("Machine")))
            {
                MaxAmmo = 100;
            }
        }
        
        Instance.SetRuntimeProperty(TEXT("MaxAmmo"), static_cast<float>(MaxAmmo));
        Instance.SetRuntimeProperty(TEXT("Ammo"), static_cast<float>(MaxAmmo)); // Start with full magazine
        
        UE_LOG(LogMedComItemManager, VeryVerbose, 
            TEXT("InitializeItemRuntimeProperties: Set ammo for %s: %d/%d"), 
            *ItemData.ItemID.ToString(), MaxAmmo, MaxAmmo);
    }
    
    // Initialize charges for consumables
    if (ItemData.bIsConsumable)
    {
        // Consumables start with number of charges equal to quantity
        float InitialCharges = static_cast<float>(Instance.Quantity);
        Instance.SetRuntimeProperty(TEXT("Charges"), InitialCharges);
        
        UE_LOG(LogMedComItemManager, VeryVerbose, 
            TEXT("InitializeItemRuntimeProperties: Set charges for %s: %.0f"), 
            *ItemData.ItemID.ToString(), InitialCharges);
    }
    
    // STUB: Initialize item condition
    // In future this may be replaced with more complex logic
    Instance.SetRuntimeProperty(TEXT("Condition"), 1.0f); // 1.0 = perfect condition
    
    UE_LOG(LogMedComItemManager, VeryVerbose, 
        TEXT("InitializeItemRuntimeProperties: Initialized %d runtime properties for %s"),
        Instance.RuntimeProperties.Num(), *ItemData.ItemID.ToString());
}