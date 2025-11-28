// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Types/Loadout/SuspenseLoadoutSettings.h"
#include "SuspenseGameInstance.generated.h"

// Forward declarations
class UDataTable;
class USuspenseLoadoutManager;
class UWeaponAnimationSubsystem;
class USuspenseItemManager;

/**
 * Base GameInstance class for MedCom
 *
 * RESPONSIBILITIES:
 * - Manages persistent functionality between levels
 * - Provides access to subsystems
 * - Configures LoadoutManager with DataTables on initialization
 * - Configures AnimationSubsystem with DataTables on initialization
 * - Configures ItemManager with DataTables on initialization (NEW)
 * - Handles global network and travel errors
 *
 * ARCHITECTURE NOTE:
 * Equipment Services are NO LONGER initialized here.
 * They are managed by UMedComSystemCoordinatorSubsystem (GameInstance-level Subsystem).
 * This ensures proper lifecycle management across seamless/non-seamless travel.
 *
 * SUBSYSTEMS:
 * - USuspenseLoadoutManager: Loadout configurations
 * - UWeaponAnimationSubsystem: Weapon animations
 * - USuspenseItemManager: Item data management (NEW)
 * - USuspenseSystemCoordinator: Equipment services (auto-initialized by UE)
 */
UCLASS()
class PLAYERCORE_API USuspenseGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    USuspenseGameInstance();

    //========================================
    // UGameInstance Interface
    //========================================

    virtual void Init() override;
    virtual void Shutdown() override;
    virtual void OnStart() override;

    //========================================
    // Static Accessors
    //========================================

    /**
     * Get SuspenseGameInstance from world context
     * @param WorldContextObject - Any UObject with valid world context
     * @return SuspenseGameInstance or nullptr
     */
    UFUNCTION(BlueprintPure, Category = "MedCom|Core", meta=(WorldContext="WorldContextObject"))
    static USuspenseGameInstance* GetSuspenseGameInstance(const UObject* WorldContextObject);

    /**
     * Generic subsystem getter (C++ template)
     * @return Subsystem of type T or nullptr
     */
    template<typename T>
    T* GetSubsystem() const
    {
        return UGameInstance::GetSubsystem<T>();
    }

    //========================================
    // Core Status API
    //========================================

    /** Check if running in offline/standalone mode */
    UFUNCTION(BlueprintPure, Category = "MedCom|Core")
    bool IsOfflineMode() const;

    /** Get current network mode as string (Standalone/DedicatedServer/ListenServer/Client) */
    UFUNCTION(BlueprintPure, Category = "MedCom|Core")
    FString GetNetworkMode() const;

    /** Get cached game version string */
    UFUNCTION(BlueprintPure, Category = "MedCom|Core")
    FString GetGameVersion() const;

    //========================================
    // Subsystem Access
    //========================================

    /** Get default loadout ID for new players */
    UFUNCTION(BlueprintPure, Category = "MedCom|Loadout")
    FName GetDefaultLoadoutID() const { return DefaultLoadoutID; }

    /** Get LoadoutManager subsystem */
    UFUNCTION(BlueprintPure, Category = "MedCom|Loadout")
    USuspenseLoadoutManager* GetLoadoutManager() const;

    /** Get WeaponAnimationSubsystem */
    UFUNCTION(BlueprintPure, Category = "MedCom|Animation")
    UWeaponAnimationSubsystem* GetWeaponAnimationSubsystem() const;

    /** Get ItemManager subsystem */
    UFUNCTION(BlueprintPure, Category = "MedCom|Items")
    USuspenseItemManager* GetItemManager() const;

protected:
    //========================================
    // Loadout System Configuration
    //========================================

 /**
      * DataTable containing all loadout configurations
      * Row Structure must be FLoadoutConfiguration
      *
      * REQUIRED: Must be set in BP_SuspenseGameInstance
      * Each row defines a complete loadout with inventory and equipment slots
      */
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout System",
     meta = (RequiredAssetDataTags = "RowStructure=/Script/MedComShared.LoadoutConfiguration",
             ToolTip = "DataTable with loadout configurations. Row Structure: FLoadoutConfiguration"))
 TObjectPtr<UDataTable> LoadoutConfigurationsTable;

    /** Default loadout ID for new players */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout System")
    FName DefaultLoadoutID = TEXT("Default_Soldier");

    /** Whether to validate loadouts on startup (recommended for development) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout System")
    bool bValidateLoadoutsOnStartup = true;

    /** Whether to log loadout operations (verbose logging) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loadout System")
    bool bLogLoadoutOperations = true;

 //========================================
 // Animation System Configuration
 //========================================

 /**
  * DataTable containing weapon animation states
  * Row Structure must be FAnimationStateData
  *
  * REQUIRED: Must be set in BP_SuspenseGameInstance
  * Row Name should match weapon type GameplayTag (e.g., Weapon.Type.Rifle.AK47)
  * Each row contains animation montages for all weapon states (Idle, Fire, Reload, etc.)
  */
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation System",
     meta = (RequiredAssetDataTags = "RowStructure=/Script/MedComShared.AnimationStateData",
             ToolTip = "DataTable with weapon animations. Row names = weapon type GameplayTags"))
 TObjectPtr<UDataTable> WeaponAnimationsTable;

 /** Whether to validate animation data on startup (recommended for development) */
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation System")
 bool bValidateAnimationsOnStartup = true;

 /** Whether to log animation system operations (verbose logging) */
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation System")
 bool bLogAnimationOperations = true;

 //========================================
 // Item System Configuration
 //========================================

 /**
  * DataTable containing all item definitions
  * Row Structure must be FSuspenseUnifiedItemData
  *
  * REQUIRED: Must be set in BP_SuspenseGameInstance
  * Each row defines a complete item with all properties, stats, and GAS integration
  * Items referenced in loadouts MUST exist in this table and pass validation
  */
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item System",
     meta = (RequiredAssetDataTags = "RowStructure=/Script/MedComShared.MedComUnifiedItemData",
             ToolTip = "DataTable with item definitions. Row Structure: FSuspenseUnifiedItemData"))
 TObjectPtr<UDataTable> ItemDataTable;

 /** Whether to validate items on startup (recommended for development) */
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item System")
 bool bValidateItemsOnStartup = true;

 /** Whether to use strict validation mode (blocks startup if critical items fail) */
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item System")
 bool bStrictItemValidation = true;

 /** Whether to log item system operations (verbose logging) */
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item System")
 bool bLogItemOperations = true;

    //========================================
    // System Event Handlers
    //========================================

    /**
     * Register global event handlers (network errors, travel failures)
     * Called during Init()
     */
    virtual void RegisterGlobalEventHandlers();

    /**
     * Unregister global event handlers
     * Called during Shutdown()
     */
    virtual void UnregisterGlobalEventHandlers();

    /**
     * Display system message to player
     * @param Message - Message text
     * @param Duration - How long to display (seconds)
     */
    void HandleSystemMessage(const FString& Message, float Duration);

    /**
     * Handle network errors
     * @param World - World context
     * @param NetDriver - Network driver
     * @param FailureType - Type of network failure
     * @param ErrorString - Error description
     */
    void HandleNetworkError(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);

    /**
     * Handle travel failures
     * @param World - World context
     * @param FailureType - Type of travel failure
     * @param ErrorString - Error description
     */
    void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

    /**
     * Cache game version string from project settings
     * Called during Init()
     */
    void CacheGameVersion();

    //========================================
    // System Initialization
    //========================================

    /**
     * Initialize LoadoutManager with configured DataTable
     * Loads all loadout configurations and validates default loadout
     *
     * @return true if initialization successful
     */
    bool InitializeLoadoutSystem();

    /**
     * Initialize WeaponAnimationSubsystem with configured DataTable
     * Loads all weapon animation configurations
     *
     * @return true if initialization successful
     */
    bool InitializeAnimationSystem();

    /**
     * Initialize ItemManager with configured DataTable
     * Loads all item definitions and performs critical validation
     *
     * CRITICAL: This validates items referenced in loadouts
     * If strict validation enabled, startup will be blocked on validation failure
     *
     * @return true if initialization successful
     */
    bool InitializeItemSystem();

    /**
     * Validate all loadout configurations
     * Logs warnings for any issues found
     */
    void ValidateLoadoutConfigurations();

    /**
     * Validate all animation configurations
     * Logs warnings for any issues found
     */
    void ValidateAnimationConfigurations();

    /**
     * Validate all item configurations
     * Performs strict validation of critical items referenced in loadouts
     *
     * @return true if all critical items passed validation
     */
    bool ValidateItemConfigurations();

 /**
     * Validate critical items referenced in loadouts with detailed reporting
     * This is the core of the validation system that ensures game-breaking items
     * are caught at startup rather than discovered during gameplay
     *
     * @param LoadoutManager Manager containing all loadout configurations
     * @param ItemManager Manager containing all item data
     * @param OutCriticalErrors Output array of detailed error descriptions
     * @return true if all critical items passed validation
     */
 bool ValidateCriticalItems(
     USuspenseLoadoutManager* LoadoutManager,
     USuspenseItemManager* ItemManager,
     TArray<FString>& OutCriticalErrors);

 /**
  * Build detailed validation report for a single critical item
  * Identifies which loadouts use the item and in what capacity
  *
  * @param ItemID Item being validated
  * @param ItemErrors Array of validation errors for this item
  * @param LoadoutManager Manager to query loadout usage
  * @param OutReport Output detailed report string
  */
 void BuildCriticalItemErrorReport(
     const FName& ItemID,
     const TArray<FString>& ItemErrors,
     USuspenseLoadoutManager* LoadoutManager,
     FString& OutReport);


private:
    //========================================
    // Cached State
    //========================================

    /** Cached game version string (ProjectName - BuildVersion) */
    FString CachedGameVersion;

    /** Flag indicating system is shutting down (prevents error handlers from running) */
    bool bIsShuttingDown;

    /** Flag indicating loadout system has been initialized */
    bool bLoadoutSystemInitialized;

    /** Flag indicating animation system has been initialized */
    bool bAnimationSystemInitialized;

    /** Flag indicating item system has been initialized */
    bool bItemSystemInitialized;
};
