// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "Types/Loadout/SuspenseLoadoutSettings.h"
#include "SuspenseLoadoutManager.generated.h"

// Forward declarations
class UDataTable;
class USuspenseEventManager;
class ISuspenseInventory;
class ISuspenseEquipment;
class ISuspenseLoadout;

/**
 * Local loadout change event delegate for LoadoutManager
 * This is separate from the global event in EventDelegateManager
 * Used for local Blueprint bindings specific to this manager
 * 
 * Renamed from FOnLoadoutChanged to avoid naming conflict with EventDelegateManager
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FOnLoadoutManagerChanged,
    const FName&, LoadoutID,
    APlayerState*, PlayerState,
    bool, bSuccess
);

/**
 * Central manager for all loadout configurations
 * Works through interfaces to maintain module independence
 * 
 * Architecture notes:
 * - Uses interfaces instead of concrete classes
 * - Broadcasts events through EventDelegateManager
 * - Thread-safe design for multiplayer
 * - No dependencies on specific component implementations
 */
UCLASS(BlueprintType, Blueprintable, Config=Game, defaultconfig)
class BRIDGESYSTEM_API USuspenseLoadoutManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    //~ Begin USubsystem Interface
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    //~ End USubsystem Interface

    /**
     * Load loadout configurations from specified DataTable
     * @param InLoadoutTable DataTable containing FLoadoutConfiguration rows
     * @return Number of successfully loaded configurations
     */
    UFUNCTION(BlueprintCallable, Category = "Loadout")
    int32 LoadLoadoutTable(UDataTable* InLoadoutTable);

    /**
     * Reload configurations from current DataTable
     * Useful for runtime updates during development
     */
    UFUNCTION(BlueprintCallable, Category = "Loadout")
    void ReloadConfigurations();

    /**
     * Get loadout configuration by ID
     * For C++ use only - returns pointer for efficiency
     * @param LoadoutID Unique identifier of loadout
     * @return Pointer to configuration or nullptr if not found
     */
    const FLoadoutConfiguration* GetLoadoutConfig(const FName& LoadoutID) const;
    
    /**
     * Get loadout configuration by ID (Blueprint safe)
     * @param LoadoutID Unique identifier of loadout
     * @param OutConfig Configuration data (will be filled if found)
     * @return True if loadout was found and OutConfig was filled
     */
    UFUNCTION(BlueprintCallable, Category = "Loadout", DisplayName = "Get Loadout Config")
    bool GetLoadoutConfigBP(const FName& LoadoutID, FLoadoutConfiguration& OutConfig) const;
    
    /**
     * Get inventory configuration from loadout
     * For C++ use only - returns pointer for efficiency
     * @param LoadoutID Loadout identifier
     * @param InventoryName Name of inventory (empty = main inventory)
     * @return Pointer to inventory config or nullptr
     */
    const FSuspenseInventoryConfig* GetInventoryConfig(const FName& LoadoutID, const FName& InventoryName = NAME_None) const;
    
    /**
     * Get inventory configuration from loadout (Blueprint safe)
     * @param LoadoutID Loadout identifier
     * @param InventoryName Name of inventory (empty = main inventory)
     * @param OutConfig Configuration data (will be filled if found)
     * @return True if inventory config was found and OutConfig was filled
     */
    UFUNCTION(BlueprintCallable, Category = "Loadout", DisplayName = "Get Inventory Config")
    bool GetInventoryConfigBP(const FName& LoadoutID, const FName& InventoryName, FSuspenseInventoryConfig& OutConfig) const;

    /**
     * Get all inventory names in a loadout
     * @param LoadoutID Loadout identifier
     * @return Array of inventory names including main
     */
    UFUNCTION(BlueprintCallable, Category = "Loadout")
    TArray<FName> GetInventoryNames(const FName& LoadoutID) const;

    /**
     * Get equipment slots configuration
     * @param LoadoutID Loadout identifier
     * @return Array of equipment slot configurations
     */
    UFUNCTION(BlueprintCallable, Category = "Loadout")
    TArray<FEquipmentSlotConfig> GetEquipmentSlots(const FName& LoadoutID) const;

    /**
     * Check if loadout exists and is valid
     * @param LoadoutID Loadout to check
     * @return True if loadout exists and passes validation
     */
    UFUNCTION(BlueprintCallable, Category = "Loadout")
    bool IsLoadoutValid(const FName& LoadoutID) const;

    /**
     * Get all available loadout IDs
     * @return Array of all cached loadout identifiers
     */
    UFUNCTION(BlueprintCallable, Category = "Loadout")
    TArray<FName> GetAllLoadoutIDs() const;

    /**
     * Get loadouts compatible with character class
     * @param CharacterClass Class tag to filter by
     * @return Array of compatible loadout IDs
     */
    UFUNCTION(BlueprintCallable, Category = "Loadout")
    TArray<FName> GetLoadoutsForClass(const FGameplayTag& CharacterClass) const;

    /**
     * Apply loadout to object implementing inventory interface
     * @param InventoryObject Object implementing ISuspenseInventory
     * @param LoadoutID Loadout to apply
     * @param InventoryName Specific inventory (empty = main)
     * @return True if successfully applied
     */
    UFUNCTION(BlueprintCallable, Category = "Loadout", meta = (CallInEditor = "true"))
    bool ApplyLoadoutToInventory(
        UObject* InventoryObject, 
        const FName& LoadoutID, 
        const FName& InventoryName = NAME_None
    ) const;

    /**
     * Apply loadout to object implementing equipment interface
     * @param EquipmentObject Object implementing ISuspenseEquipment
     * @param LoadoutID Loadout to apply
     * @return True if successfully applied
     */
    UFUNCTION(BlueprintCallable, Category = "Loadout", meta = (CallInEditor = "true"))
    bool ApplyLoadoutToEquipment(
        UObject* EquipmentObject, 
        const FName& LoadoutID
    ) const;

    /**
     * Apply loadout to object implementing loadout interface
     * @param LoadoutObject Object implementing ISuspenseLoadout
     * @param LoadoutID Loadout to apply
     * @param bForceApply Force application even if already using this loadout
     * @return True if successfully applied
     */
    UFUNCTION(BlueprintCallable, Category = "Loadout", meta = (CallInEditor = "true"))
    bool ApplyLoadoutToObject(
        UObject* LoadoutObject,
        const FName& LoadoutID,
        bool bForceApply = false
    ) const;

    /**
     * Get default loadout for character class
     * @param CharacterClass Class to get default for
     * @return Default loadout ID or NAME_None
     */
    UFUNCTION(BlueprintCallable, Category = "Loadout")
    FName GetDefaultLoadoutForClass(const FGameplayTag& CharacterClass) const;

    /**
     * Validate all loaded configurations
     * @param OutErrors Array of validation errors
     * @return True if all configurations are valid
     */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Debug")
    bool ValidateAllConfigurations(TArray<FString>& OutErrors) const;

    /**
     * Get total weight capacity for loadout
     * @param LoadoutID Loadout to check
     * @return Total weight capacity across all inventories
     */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Stats")
    float GetTotalWeightCapacity(const FName& LoadoutID) const;

    /**
     * Get total inventory cells for loadout
     * @param LoadoutID Loadout to check
     * @return Total cell count across all inventories
     */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Stats")
    int32 GetTotalInventoryCells(const FName& LoadoutID) const;

    /**
     * Set the default DataTable path
     * Used for automatic loading on initialize
     */
    UFUNCTION(BlueprintCallable, Category = "Loadout|Config")
    void SetDefaultDataTablePath(const FString& Path);

    /**
     * Broadcast loadout change event
     * @param LoadoutID The loadout that was changed
     * @param PlayerState The player state affected (can be null)
     * @param bSuccess Whether the change was successful
     */
    void BroadcastLoadoutChange(const FName& LoadoutID, APlayerState* PlayerState, bool bSuccess) const;

    /**
     * Local event for Blueprint bindings
     * For global C++ event notifications, use EventDelegateManager
     * This is separate to avoid naming conflicts and provide local Blueprint access
     */
    UPROPERTY(BlueprintAssignable, Category = "Loadout|Events")
    FOnLoadoutManagerChanged OnLoadoutManagerChanged;

protected:
    /** Path to default loadout DataTable */
    UPROPERTY(Config, EditAnywhere, Category = "Loadout|Config")
    FString DefaultLoadoutTablePath;

    /** Currently loaded DataTable */
    UPROPERTY(BlueprintReadOnly, Category = "Loadout")
    UDataTable* LoadedDataTable;

    /** Cached loadout configurations for fast access */
    UPROPERTY()
    TMap<FName, FLoadoutConfiguration> CachedConfigurations;

    /** Map of character class to default loadout */
    UPROPERTY(EditDefaultsOnly, Category = "Loadout|Defaults")
    TMap<FGameplayTag, FName> ClassDefaultLoadouts;

private:
    /**
     * Load configurations from DataTable into cache
     * @return Number of loaded configurations
     */
    int32 CacheConfigurationsFromTable();

    /**
     * Validate single configuration
     * @param Config Configuration to validate
     * @param OutErrors Validation errors
     * @return True if valid
     */
    bool ValidateConfiguration(const FLoadoutConfiguration& Config, TArray<FString>& OutErrors) const;

    /**
     * Clear all cached data
     */
    void ClearCache();

    /**
     * Try to load default DataTable from path
     */
    void TryLoadDefaultTable();

    /**
     * Log loadout statistics for debugging
     */
    void LogLoadoutStatistics() const;

    /**
     * Get event delegate manager
     */
    USuspenseEventManager* GetEventDelegateManager() const;

    /** Flag to prevent multiple initialization */
    bool bIsInitialized;

    /** Critical section for thread-safe cache access */
    mutable FCriticalSection CacheCriticalSection;
};