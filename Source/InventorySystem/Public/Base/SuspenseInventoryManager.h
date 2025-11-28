// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Types/Loadout/SuspenseLoadoutSettings.h"
#include "SuspenseInventoryManager.generated.h"

// Forward declarations
class USuspenseItemManager;
struct FLoadoutConfiguration;

/**
 * Subsystem for inventory management and loadout configuration
 *
 * ОБНОВЛЕНО ДЛЯ НОВОЙ АРХИТЕКТУРЫ:
 * - Интеграция с LoadoutSettings вместо устаревших FInventorySettings
 * - Использование FSuspenseInventoryItemInstance вместо FMCInventoryItemData
 * - Делегирование создания предметов в ItemManager и InventoryUtils
 * - Удаление собственного кэша - теперь используем DataTable как источник истины
 *
 * Основные функции:
 * - Получение конфигураций loadout из DataTable
 * - Создание и инициализация инвентарей на основе loadout
 * - Управление starting items для новых персонажей
 * - Валидация совместимости loadout с классами персонажей
 */
UCLASS()
class INVENTORYSYSTEM_API USuspenseInventoryManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    //==================================================================
    // Subsystem lifecycle
    //==================================================================

    USuspenseInventoryManager();

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    //==================================================================
    // Loadout configuration management
    //==================================================================

    /**
     * Load loadout configurations from DataTable
     * @param LoadoutDataTable DataTable with FLoadoutConfiguration rows
     * @return True if successfully loaded
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Loadout")
    bool LoadLoadoutDataTable(UDataTable* LoadoutDataTable);

    /**
     * Get loadout configuration by ID
     * @param LoadoutID Identifier of the loadout
     * @param OutConfiguration Output loadout configuration
     * @return True if loadout found
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Loadout")
    bool GetLoadoutConfiguration(const FName& LoadoutID, FLoadoutConfiguration& OutConfiguration) const;

    /**
     * Get default loadout configuration
     * Used when no specific loadout is requested
     * @return Default loadout configuration
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Loadout")
    FLoadoutConfiguration GetDefaultLoadoutConfiguration() const;

    /**
     * Get loadout configurations compatible with character class
     * @param CharacterClass Class tag to filter by
     * @return Array of compatible loadout IDs
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Loadout")
    TArray<FName> GetCompatibleLoadouts(const FGameplayTag& CharacterClass) const;

    /**
     * Get all available loadout IDs
     * @return Array of all registered loadout IDs
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Loadout")
    TArray<FName> GetAllLoadoutIDs() const;

    //==================================================================
    // Inventory initialization from loadout
    //==================================================================

    /**
     * Initialize inventory component from loadout configuration
     * @param InventoryComponent Target inventory component
     * @param LoadoutID Loadout configuration to use
     * @param InventoryName Specific inventory name (NAME_None for main inventory)
     * @return Number of successfully created starting items
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Initialization")
    int32 InitializeInventoryFromLoadout(class USuspenseInventoryComponent* InventoryComponent,
                                        const FName& LoadoutID,
                                        const FName& InventoryName = NAME_None);

    /**
     * Create starting items from loadout configuration
     * @param InventoryComponent Target inventory component
     * @param LoadoutConfiguration Loadout configuration containing starting items
     * @param InventoryName Specific inventory name (NAME_None for main inventory)
     * @return Number of successfully added items
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Initialization")
    int32 CreateStartingItemsFromLoadout(class USuspenseInventoryComponent* InventoryComponent,
                                        const FLoadoutConfiguration& LoadoutConfiguration,
                                        const FName& InventoryName = NAME_None);

 /**
      * Инициализировать слоты экипировки из конфигурации loadout
      * Работает через интерфейс IMedComEquipmentInterface для слабой связанности модулей
      *
      * @param EquipmentTarget Объект, реализующий IMedComEquipmentInterface
      * @param LoadoutConfiguration Конфигурация loadout с настройками экипировки
      * @return Количество успешно экипированных предметов
      */
 UFUNCTION(BlueprintCallable, Category = "Inventory|Initialization")
 int32 InitializeEquipmentFromLoadout(UObject* EquipmentTarget,
                                     const FLoadoutConfiguration& LoadoutConfiguration);

    //==================================================================
    // Item instance creation (delegates to ItemManager)
    //==================================================================

    /**
     * Create item instance using ItemManager
     * This is a convenience method that delegates to ItemManager
     * @param ItemID Item identifier from DataTable
     * @param Quantity Amount of items in stack
     * @param OutInstance Output item instance
     * @return True if instance created successfully
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Items")
    bool CreateItemInstance(const FName& ItemID, int32 Quantity, FSuspenseInventoryItemInstance& OutInstance) const;

    /**
     * Create multiple item instances from spawn data
     * @param SpawnDataArray Array of pickup spawn configurations
     * @param OutInstances Output array of created instances
     * @return Number of successfully created instances
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Items")
    int32 CreateItemInstancesFromSpawnData(const TArray<FSuspensePickupSpawnData>& SpawnDataArray,
                                          TArray<FSuspenseInventoryItemInstance>& OutInstances) const;

    //==================================================================
    // Validation and utilities
    //==================================================================

    /**
     * Validate loadout configuration integrity
     * @param LoadoutID Loadout to validate
     * @param OutErrors Output array of validation errors
     * @return True if loadout is valid
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    bool ValidateLoadoutConfiguration(const FName& LoadoutID, TArray<FString>& OutErrors) const;

    /**
     * Check if loadout is compatible with character class
     * @param LoadoutID Loadout to check
     * @param CharacterClass Character class tag
     * @return True if compatible
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    bool IsLoadoutCompatibleWithClass(const FName& LoadoutID, const FGameplayTag& CharacterClass) const;

    /**
     * Get inventory configuration from loadout (C++ version - returns pointer for efficiency)
     * @param LoadoutID Loadout identifier
     * @param InventoryName Specific inventory name (NAME_None for main)
     * @return Inventory configuration pointer or nullptr if not found
     */
    const FSuspenseInventoryConfig* GetInventoryConfigFromLoadout(const FName& LoadoutID, const FName& InventoryName = NAME_None) const;

    /**
     * Get inventory configuration from loadout (Blueprint version - returns copy)
     * @param LoadoutID Loadout identifier
     * @param InventoryName Specific inventory name (NAME_None for main)
     * @param OutInventoryConfig Output inventory configuration
     * @return True if configuration found
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Configuration", DisplayName = "Get Inventory Config From Loadout")
    bool GetInventoryConfigFromLoadout_BP(const FName& LoadoutID, const FName& InventoryName, FSuspenseInventoryConfig& OutInventoryConfig) const;

    //==================================================================
    // Debug and statistics
    //==================================================================

    /**
     * Get loadout cache statistics for debugging
     * @param OutStats Output statistics string
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    void GetLoadoutCacheStatistics(FString& OutStats) const;

    /**
     * Refresh loadout cache from current DataTable
     * @return True if cache refreshed successfully
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    bool RefreshLoadoutCache();

    /**
     * Log detailed loadout information for debugging
     * @param LoadoutID Loadout to analyze
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    void LogLoadoutDetails(const FName& LoadoutID) const;

private:
    //==================================================================
    // Internal data
    //==================================================================

    /** Currently loaded loadout DataTable */
    UPROPERTY()
    UDataTable* LoadoutTable;

    /** Default loadout table path */
    FSoftObjectPath DefaultLoadoutTablePath = FSoftObjectPath(TEXT("/Game/MEDCOM/Data/DT_LoadoutConfigurations"));

    /** Cache for fast access to loadout configurations */
    TMap<FName, FLoadoutConfiguration> LoadoutCache;

    /** Default loadout configuration */
    FLoadoutConfiguration DefaultLoadout;

    /** Cache access statistics */
    mutable int32 LoadoutCacheHits = 0;
    mutable int32 LoadoutCacheMisses = 0;

    //==================================================================
    // Internal helper methods
    //==================================================================

    /** Load default loadout table during initialization */
    void LoadDefaultLoadoutTable();

    /** Build cache from currently loaded table */
    void BuildLoadoutCache();

    /** Initialize default loadout configuration */
    void InitializeDefaultLoadout();

    /** Get ItemManager subsystem reference */
    USuspenseItemManager* GetItemManager() const;

    /** Helper to get loadout data with cache statistics */
    const FLoadoutConfiguration* GetCachedLoadoutData(const FName& LoadoutID) const;

    /** Validate single loadout configuration */
    bool ValidateLoadoutInternal(const FName& LoadoutID, const FLoadoutConfiguration& Configuration, TArray<FString>& OutErrors) const;

    /** Log cache building statistics */
    void LogLoadoutCacheStatistics(int32 TotalLoadouts, int32 ValidLoadouts) const;
};
