// Copyright Suspense Team. All Rights Reserved.

#include "Core/SuspenseGameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "UObject/UObjectIterator.h"
#include "Misc/App.h"
#include "Subsystems/SuspenseSystemCoordinator.h"
#include "Types/Loadout/SuspenseLoadoutManager.h"
#include "Subsystems/WeaponAnimationSubsystem.h"
#include "ItemSystem/SuspenseItemManager.h"
#include "Types/Loadout/SuspenseLoadoutSettings.h"
#include "Types/Animation/SuspenseAnimationState.h"
#include "Types/Loadout/SuspenseItemDataTable.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseGameInstance, Log, All);

//========================================
// Construction
//========================================

USuspenseGameInstance::USuspenseGameInstance()
    : bIsShuttingDown(false)
    , bLoadoutSystemInitialized(false)
    , bAnimationSystemInitialized(false)
    , bItemSystemInitialized(false)
{
    // Version will be cached in Init() when FApp is ready
    CachedGameVersion = TEXT("Unknown Version");

    // Default configuration
    DefaultLoadoutID = TEXT("Default_Soldier");
    bValidateLoadoutsOnStartup = true;
    bLogLoadoutOperations = true;
    bValidateAnimationsOnStartup = true;
    bLogAnimationOperations = true;
    bValidateItemsOnStartup = true;
    bStrictItemValidation = true;
    bLogItemOperations = true;
}

//========================================
// UGameInstance Interface
//========================================

void USuspenseGameInstance::Init()
{
    Super::Init();

    UE_LOG(LogSuspenseGameInstance, Log, TEXT("=== GAME INSTANCE INITIALIZATION START ==="));

    // Cache version info
    CacheGameVersion();

    // Initialize Item system FIRST - loadouts depend on valid item data
    if (!InitializeItemSystem())
    {
        UE_LOG(LogSuspenseGameInstance, Error, TEXT("Failed to initialize item system!"));
        // Continue initialization but log critical error
    }

    // Initialize Loadout system - depends on ItemManager for validation
    if (!InitializeLoadoutSystem())
    {
        UE_LOG(LogSuspenseGameInstance, Error, TEXT("Failed to initialize loadout system!"));
    }

    // Initialize Animation system
    if (!InitializeAnimationSystem())
    {
        UE_LOG(LogSuspenseGameInstance, Error, TEXT("Failed to initialize animation system!"));
    }

    USuspenseSystemCoordinator* CoordinatorSubsystem = GetSubsystem<USuspenseSystemCoordinator>();
    if (CoordinatorSubsystem)
    {
        if (CoordinatorSubsystem->AreGlobalServicesReady())
        {
            UE_LOG(LogSuspenseGameInstance, Log, TEXT("Equipment services already initialized"));
        }
        else
        {
            UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Equipment services not ready yet - will initialize on world load"));
        }
    }
    else
    {
        UE_LOG(LogSuspenseGameInstance, Error, TEXT("SystemCoordinatorSubsystem not available!"));
    }

    // Register global event handlers
    RegisterGlobalEventHandlers();

    UE_LOG(LogSuspenseGameInstance, Log, TEXT("=== GAME INSTANCE INITIALIZATION COMPLETE ==="));
    UE_LOG(LogSuspenseGameInstance, Log, TEXT("Game Version: %s"), *CachedGameVersion);
    UE_LOG(LogSuspenseGameInstance, Log, TEXT("Network Mode: %s"), *GetNetworkMode());
}

void USuspenseGameInstance::Shutdown()
{
    UE_LOG(LogSuspenseGameInstance, Log, TEXT("=== GAME INSTANCE SHUTDOWN START ==="));

    // Set shutdown flag to prevent error handlers from running
    bIsShuttingDown = true;

    // Unregister event handlers
    UnregisterGlobalEventHandlers();

    Super::Shutdown();

    UE_LOG(LogSuspenseGameInstance, Log, TEXT("=== GAME INSTANCE SHUTDOWN COMPLETE ==="));
}

void USuspenseGameInstance::OnStart()
{
    Super::OnStart();

    UE_LOG(LogSuspenseGameInstance, Log, TEXT("SuspenseGameInstance OnStart called"));

    // Double-check system readiness (fallback if Init() failed)
    if (!bItemSystemInitialized)
    {
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Item system not initialized on game start, attempting initialization..."));
        InitializeItemSystem();
    }

    if (!bLoadoutSystemInitialized)
    {
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Loadout system not initialized on game start, attempting initialization..."));
        InitializeLoadoutSystem();
    }

    if (!bAnimationSystemInitialized)
    {
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Animation system not initialized on game start, attempting initialization..."));
        InitializeAnimationSystem();
    }
}

//========================================
// Static Accessors
//========================================

USuspenseGameInstance* USuspenseGameInstance::GetSuspenseGameInstance(const UObject* WorldContextObject)
{
    if (!WorldContextObject)
    {
        return nullptr;
    }

    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (!World)
    {
        return nullptr;
    }

    return Cast<USuspenseGameInstance>(World->GetGameInstance());
}

//========================================
// Core Status API
//========================================

bool USuspenseGameInstance::IsOfflineMode() const
{
    UWorld* World = GetWorld();
    return !World || World->GetNetMode() == NM_Standalone;
}

FString USuspenseGameInstance::GetNetworkMode() const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return TEXT("Unknown");
    }

    switch (World->GetNetMode())
    {
        case NM_Standalone:
            return TEXT("Standalone");
        case NM_DedicatedServer:
            return TEXT("DedicatedServer");
        case NM_ListenServer:
            return TEXT("ListenServer");
        case NM_Client:
            return TEXT("Client");
        default:
            return TEXT("Unknown");
    }
}

FString USuspenseGameInstance::GetGameVersion() const
{
    return CachedGameVersion;
}

//========================================
// Subsystem Access
//========================================

USuspenseLoadoutManager* USuspenseGameInstance::GetLoadoutManager() const
{
    return GetSubsystem<USuspenseLoadoutManager>();
}

UWeaponAnimationSubsystem* USuspenseGameInstance::GetWeaponAnimationSubsystem() const
{
    return GetSubsystem<UWeaponAnimationSubsystem>();
}

USuspenseItemManager* USuspenseGameInstance::GetItemManager() const
{
    return GetSubsystem<USuspenseItemManager>();
}

//========================================
// Item System Initialization (NEW)
//========================================

bool USuspenseGameInstance::InitializeItemSystem()
{
    UE_LOG(LogSuspenseGameInstance, Warning, TEXT("=== ITEM SYSTEM INITIALIZATION START ==="));

    if (bItemSystemInitialized)
    {
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Item system already initialized"));
        return true;
    }

    //========================================
    // Step 1: Get ItemManager Subsystem
    //========================================
    {
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Step 1: Getting ItemManager subsystem..."));
        USuspenseItemManager* ItemManager = GetItemManager();
        if (!ItemManager)
        {
            UE_LOG(LogSuspenseGameInstance, Error, TEXT("ItemManager subsystem not found!"));
            return false;
        }
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Step 1: SUCCESS - ItemManager found at %p"), ItemManager);
    }

    //========================================
    // Step 2: Verify DataTable Configured
    //========================================
    {
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Step 2: Checking ItemDataTable..."));
        if (!ItemDataTable)
        {
            UE_LOG(LogSuspenseGameInstance, Error, TEXT("ItemDataTable not set in GameInstance!"));
            UE_LOG(LogSuspenseGameInstance, Error, TEXT("CRITICAL: Make sure BP_SuspenseGameInstance has ItemDataTable field set!"));
            UE_LOG(LogSuspenseGameInstance, Error, TEXT("ItemManager will attempt fallback to default path, but this is not recommended."));

            // Allow ItemManager to try fallback initialization
            // This maintains backwards compatibility but is not the preferred path
            bItemSystemInitialized = false;
            return false;
        }
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Step 2: SUCCESS - DataTable found: %s"), *ItemDataTable->GetName());
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("  - DataTable Path: %s"), *ItemDataTable->GetPathName());
    }

    //========================================
    // Step 3: Verify DataTable Structure
    //========================================
    {
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Step 3: Verifying DataTable structure..."));
        const UScriptStruct* RowStruct = ItemDataTable->GetRowStruct();
        if (!RowStruct)
        {
            UE_LOG(LogSuspenseGameInstance, Error, TEXT("ItemDataTable has no row structure!"));
            return false;
        }
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("  - Row Structure Name: %s"), *RowStruct->GetName());
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("  - Expected Structure: FSuspenseUnifiedItemData"));

        // Verify correct structure
        if (RowStruct != FSuspenseUnifiedItemData::StaticStruct())
        {
            UE_LOG(LogSuspenseGameInstance, Error,
                TEXT("ItemDataTable has incorrect row structure! Expected: FSuspenseUnifiedItemData, Got: %s"),
                *RowStruct->GetName());
            return false;
        }
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Step 3: SUCCESS - Row structure is correct"));
    }

    //========================================
    // Step 4: Examine DataTable Contents
    //========================================
    {
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Step 4: Examining DataTable contents..."));
        TArray<FName> RowNames = ItemDataTable->GetRowNames();
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("  - Total rows in DataTable: %d"), RowNames.Num());

        if (bLogItemOperations && RowNames.Num() > 0)
        {
            for (int32 i = 0; i < FMath::Min(RowNames.Num(), 10); i++)
            {
                UE_LOG(LogSuspenseGameInstance, Warning, TEXT("  - Row[%d]: %s"), i, *RowNames[i].ToString());
            }
            if (RowNames.Num() > 10)
            {
                UE_LOG(LogSuspenseGameInstance, Warning, TEXT("  - ... and %d more rows"), RowNames.Num() - 10);
            }
        }
    }

    //========================================
    // Step 5: Load Data into ItemManager
    //========================================
    {
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Step 5: Loading item data into ItemManager..."));
        USuspenseItemManager* ItemManager = GetItemManager();

        bool bLoadSuccess = ItemManager->LoadItemDataTable(ItemDataTable);

        if (!bLoadSuccess)
        {
            UE_LOG(LogSuspenseGameInstance, Error, TEXT("Failed to load ItemDataTable into ItemManager!"));
            UE_LOG(LogSuspenseGameInstance, Error, TEXT("Check ItemManager logs for specific errors"));
            return false;
        }

        int32 LoadedCount = ItemManager->GetCachedItemCount();
        UE_LOG(LogSuspenseGameInstance, Log, TEXT("Step 5: SUCCESS - Loaded %d items into cache"), LoadedCount);

        if (LoadedCount == 0)
        {
            UE_LOG(LogSuspenseGameInstance, Error, TEXT("No items were loaded from DataTable!"));
            UE_LOG(LogSuspenseGameInstance, Error, TEXT("This usually means the DataTable is empty or has structural issues"));
            return false;
        }
    }

    //========================================
    // Step 6: Validate Items (Optional but Recommended)
    //========================================
    if (bValidateItemsOnStartup)
    {
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Step 6: Validating item configurations..."));

        bool bValidationPassed = ValidateItemConfigurations();

        if (!bValidationPassed)
        {
            if (bStrictItemValidation)
            {
                UE_LOG(LogSuspenseGameInstance, Error, TEXT("CRITICAL: Item validation failed with strict mode enabled!"));
                UE_LOG(LogSuspenseGameInstance, Error, TEXT("Game cannot start with invalid critical items."));
                UE_LOG(LogSuspenseGameInstance, Error, TEXT("Please fix the validation errors in the DataTable."));

                // In production, this could trigger a fatal error or graceful shutdown
                // For now, we log the error but allow continuation for debugging
                return false;
            }
            else
            {
                UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Item validation found issues, but strict mode is disabled"));
                UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Game will continue, but some items may not work correctly"));
            }
        }
        else
        {
            UE_LOG(LogSuspenseGameInstance, Log, TEXT("Step 6: SUCCESS - All items validated successfully"));
        }
    }

    //========================================
    // Step 7: Log Final Summary
    //========================================
    if (bLogItemOperations)
    {
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Step 7: Final item system summary:"));
        USuspenseItemManager* ItemManager = GetItemManager();

        TArray<FName> AllItems = ItemManager->GetAllItemIDs();
        UE_LOG(LogSuspenseGameInstance, Log, TEXT("Total items available: %d"), AllItems.Num());

        // Count by type for statistics
        int32 WeaponCount = 0;
        int32 ArmorCount = 0;
        int32 AmmoCount = 0;
        int32 ConsumableCount = 0;

        for (const FName& ItemID : AllItems)
        {
            FSuspenseUnifiedItemData ItemData;
            if (ItemManager->GetUnifiedItemData(ItemID, ItemData))
            {
                if (ItemData.bIsWeapon) WeaponCount++;
                if (ItemData.bIsArmor) ArmorCount++;
                if (ItemData.bIsAmmo) AmmoCount++;
                if (ItemData.bIsConsumable) ConsumableCount++;
            }
        }

        UE_LOG(LogSuspenseGameInstance, Log, TEXT("  - Weapons: %d"), WeaponCount);
        UE_LOG(LogSuspenseGameInstance, Log, TEXT("  - Armor: %d"), ArmorCount);
        UE_LOG(LogSuspenseGameInstance, Log, TEXT("  - Ammunition: %d"), AmmoCount);
        UE_LOG(LogSuspenseGameInstance, Log, TEXT("  - Consumables: %d"), ConsumableCount);
    }

    bItemSystemInitialized = true;
    UE_LOG(LogSuspenseGameInstance, Warning, TEXT("=== ITEM SYSTEM INITIALIZATION COMPLETE ==="));
    return true;
}

bool USuspenseGameInstance::ValidateItemConfigurations()
{
    UE_LOG(LogSuspenseGameInstance, Warning, TEXT("=== ITEM VALIDATION START ==="));

    USuspenseItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        UE_LOG(LogSuspenseGameInstance, Error, TEXT("ItemManager not available for validation"));
        return false;
    }

    USuspenseLoadoutManager* LoadoutManager = GetLoadoutManager();
    if (!LoadoutManager)
    {
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("LoadoutManager not available for critical item validation"));
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Will perform basic item validation only"));

        // Fallback to basic validation if loadout manager not available
        TArray<FString> ValidationErrors;
        int32 ErrorCount = ItemManager->ValidateAllItems(ValidationErrors);

        if (ErrorCount > 0)
        {
            UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Basic validation found %d items with errors"), ErrorCount);
            for (const FString& Error : ValidationErrors)
            {
                UE_LOG(LogSuspenseGameInstance, Warning, TEXT("  %s"), *Error);
            }
        }

        return ErrorCount == 0;
    }

    //========================================
    // Phase 1: Basic Validation of All Items
    //========================================
    UE_LOG(LogSuspenseGameInstance, Log, TEXT("Phase 1: Validating all items in DataTable..."));

    TArray<FString> ValidationErrors;
    int32 ErrorCount = ItemManager->ValidateAllItems(ValidationErrors);

    if (ErrorCount > 0)
    {
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Phase 1: Found %d items with validation errors:"), ErrorCount);

        // Log errors but continue to phase 2 for critical item analysis
        for (const FString& Error : ValidationErrors)
        {
            UE_LOG(LogSuspenseGameInstance, Warning, TEXT("  %s"), *Error);
        }
    }
    else
    {
        UE_LOG(LogSuspenseGameInstance, Log, TEXT("Phase 1: All items passed basic validation"));
    }

    //========================================
    // Phase 2: Critical Items Validation
    //========================================
    UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Phase 2: Validating critical items referenced in loadouts..."));

    TArray<FString> CriticalErrors;
    bool bCriticalItemsValid = ValidateCriticalItems(LoadoutManager, ItemManager, CriticalErrors);

    if (!bCriticalItemsValid)
    {
        UE_LOG(LogSuspenseGameInstance, Error, TEXT("=== CRITICAL ITEM VALIDATION FAILURES ==="));
        UE_LOG(LogSuspenseGameInstance, Error, TEXT("The following items are used in loadouts but have critical errors:"));
        UE_LOG(LogSuspenseGameInstance, Error, TEXT(""));

        for (const FString& CriticalError : CriticalErrors)
        {
            // Log each line of the detailed report
            TArray<FString> Lines;
            CriticalError.ParseIntoArray(Lines, TEXT("\n"), false);
            for (const FString& Line : Lines)
            {
                UE_LOG(LogSuspenseGameInstance, Error, TEXT("%s"), *Line);
            }
            UE_LOG(LogSuspenseGameInstance, Error, TEXT(""));
        }

        UE_LOG(LogSuspenseGameInstance, Error, TEXT("=== END CRITICAL VALIDATION FAILURES ==="));
        UE_LOG(LogSuspenseGameInstance, Error, TEXT(""));
        UE_LOG(LogSuspenseGameInstance, Error, TEXT("GAME CANNOT START WITH INVALID CRITICAL ITEMS"));
        UE_LOG(LogSuspenseGameInstance, Error, TEXT("Please fix the above errors in your item DataTable"));
        UE_LOG(LogSuspenseGameInstance, Error, TEXT(""));

        if (bStrictItemValidation)
        {
            UE_LOG(LogSuspenseGameInstance, Error, TEXT("Strict validation mode is ENABLED - blocking startup"));
            return false;
        }
        else
        {
            UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Strict validation mode is DISABLED - continuing with warnings"));
            UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Loadouts using invalid items will NOT function correctly!"));
        }
    }
    else
    {
        UE_LOG(LogSuspenseGameInstance, Log, TEXT("Phase 2: All critical items validated successfully"));
    }

    //========================================
    // Phase 3: Summary
    //========================================
    UE_LOG(LogSuspenseGameInstance, Warning, TEXT("=== ITEM VALIDATION SUMMARY ==="));
    UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Total Items in DataTable: %d"), ItemManager->GetCachedItemCount());
    UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Valid Items: %d"), ItemManager->GetValidItemCount());
    UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Items with Errors: %d"), ErrorCount);
    UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Critical Items Validated: %s"), CriticalErrors.Num() == 0 ? TEXT("All passed") : *FString::Printf(TEXT("%d failed"), CriticalErrors.Num()));
    UE_LOG(LogSuspenseGameInstance, Warning, TEXT("================================"));

    // Return true only if:
    // 1. No basic validation errors OR we're in non-strict mode
    // 2. All critical items passed validation OR we're in non-strict mode
    bool bOverallSuccess = (ErrorCount == 0 || !bStrictItemValidation) &&
                          (bCriticalItemsValid || !bStrictItemValidation);

    UE_LOG(LogSuspenseGameInstance, Warning, TEXT("=== ITEM VALIDATION %s ==="),
        bOverallSuccess ? TEXT("COMPLETE") : TEXT("FAILED"));

    return bOverallSuccess;
}

bool USuspenseGameInstance::ValidateCriticalItems(
    USuspenseLoadoutManager* LoadoutManager,
    USuspenseItemManager* ItemManager,
    TArray<FString>& OutCriticalErrors)
{
    OutCriticalErrors.Empty();

    if (!LoadoutManager || !ItemManager)
    {
        return false;
    }

    //========================================
    // Step 1: Collect all items referenced in loadouts
    //========================================
    UE_LOG(LogSuspenseGameInstance, Log, TEXT("ValidateCriticalItems: Collecting items from all loadouts..."));

    TArray<FName> AllLoadouts = LoadoutManager->GetAllLoadoutIDs();
    TSet<FName> CriticalItems; // Use Set to avoid duplicates
    TMap<FName, TArray<FString>> ItemUsageMap; // Track where each item is used

    for (const FName& LoadoutID : AllLoadouts)
    {
        FLoadoutConfiguration LoadoutConfig;
        if (LoadoutManager->GetLoadoutConfigBP(LoadoutID, LoadoutConfig))
        {
            // Collect starting equipment items
            for (const auto& EquipPair : LoadoutConfig.StartingEquipment)
            {
                const FName& ItemID = EquipPair.Value;
                if (!ItemID.IsNone())
                {
                    CriticalItems.Add(ItemID);

                    // Track usage
                    FString UsageInfo = FString::Printf(TEXT("Loadout '%s': Equipment slot %s"),
                        *LoadoutID.ToString(),
                        *UEnum::GetValueAsString(EquipPair.Key));

                    ItemUsageMap.FindOrAdd(ItemID).Add(UsageInfo);
                }
            }

            // Collect starting items from main inventory
            for (const FSuspensePickupSpawnData& SpawnData : LoadoutConfig.MainInventory.StartingItems)
            {
                const FName& ItemID = SpawnData.ItemID;
                if (!ItemID.IsNone())
                {
                    CriticalItems.Add(ItemID);

                    FString UsageInfo = FString::Printf(TEXT("Loadout '%s': Main inventory (qty: %d)"),
                        *LoadoutID.ToString(),
                        SpawnData.Quantity);

                    ItemUsageMap.FindOrAdd(ItemID).Add(UsageInfo);
                }
            }

            // Collect starting items from additional inventories
            for (const auto& InvPair : LoadoutConfig.AdditionalInventories)
            {
                for (const FSuspensePickupSpawnData& SpawnData : InvPair.Value.StartingItems)
                {
                    const FName& ItemID = SpawnData.ItemID;
                    if (!ItemID.IsNone())
                    {
                        CriticalItems.Add(ItemID);

                        FString UsageInfo = FString::Printf(TEXT("Loadout '%s': %s inventory (qty: %d)"),
                            *LoadoutID.ToString(),
                            *InvPair.Key.ToString(),
                            SpawnData.Quantity);

                        ItemUsageMap.FindOrAdd(ItemID).Add(UsageInfo);
                    }
                }
            }
        }
    }

    UE_LOG(LogSuspenseGameInstance, Log, TEXT("ValidateCriticalItems: Found %d unique critical items"), CriticalItems.Num());

    if (CriticalItems.Num() == 0)
    {
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("ValidateCriticalItems: No items found in any loadout"));
        return true; // No items to validate
    }

    //========================================
    // Step 2: Validate each critical item
    //========================================
    int32 CriticalItemErrors = 0;

    for (const FName& ItemID : CriticalItems)
    {
        // First check if item exists at all
        if (!ItemManager->HasItem(ItemID))
        {
            CriticalItemErrors++;

            FString ErrorReport;
            ErrorReport += FString::Printf(TEXT("CRITICAL ERROR: Item '%s' NOT FOUND in ItemDataTable\n"), *ItemID.ToString());
            ErrorReport += TEXT("This item is referenced in the following loadouts but does not exist:\n");

            // Show all usages
            const TArray<FString>* Usages = ItemUsageMap.Find(ItemID);
            if (Usages)
            {
                for (const FString& Usage : *Usages)
                {
                    ErrorReport += FString::Printf(TEXT("  - %s\n"), *Usage);
                }
            }

            ErrorReport += TEXT("\nREQUIRED ACTION:\n");
            ErrorReport += FString::Printf(TEXT("  1. Add item '%s' to your ItemDataTable (DT_MedComItems)\n"), *ItemID.ToString());
            ErrorReport += TEXT("  2. OR remove this item from the affected loadouts\n");

            OutCriticalErrors.Add(ErrorReport);

            continue; // Skip further validation for missing items
        }

        // Validate the item
        TArray<FString> ItemErrors;
        if (!ItemManager->ValidateItem(ItemID, ItemErrors))
        {
            CriticalItemErrors++;

            // Build detailed error report
            FString DetailedReport;
            BuildCriticalItemErrorReport(ItemID, ItemErrors, LoadoutManager, DetailedReport);
            OutCriticalErrors.Add(DetailedReport);
        }
    }

    //========================================
    // Step 3: Log summary
    //========================================
    if (CriticalItemErrors > 0)
    {
        UE_LOG(LogSuspenseGameInstance, Error,
            TEXT("ValidateCriticalItems: %d critical items failed validation"),
            CriticalItemErrors);
        return false;
    }
    else
    {
        UE_LOG(LogSuspenseGameInstance, Log,
            TEXT("ValidateCriticalItems: All %d critical items validated successfully"),
            CriticalItems.Num());
        return true;
    }
}

void USuspenseGameInstance::BuildCriticalItemErrorReport(
    const FName& ItemID,
    const TArray<FString>& ItemErrors,
    USuspenseLoadoutManager* LoadoutManager,
    FString& OutReport)
{
    OutReport.Empty();

    // Get item data to provide context
    USuspenseItemManager* ItemManager = GetItemManager();
    FSuspenseUnifiedItemData ItemData;
    bool bHasItemData = ItemManager && ItemManager->GetUnifiedItemData(ItemID, ItemData);

    //========================================
    // Header
    //========================================
    OutReport += FString::Printf(TEXT("========================================\n"));
    OutReport += FString::Printf(TEXT("CRITICAL ITEM VALIDATION FAILURE\n"));
    OutReport += FString::Printf(TEXT("========================================\n"));
    OutReport += FString::Printf(TEXT("Item ID: %s\n"), *ItemID.ToString());

    if (bHasItemData)
    {
        OutReport += FString::Printf(TEXT("Display Name: %s\n"), *ItemData.DisplayName.ToString());
        OutReport += FString::Printf(TEXT("Item Type: %s\n"), *ItemData.ItemType.ToString());

        if (ItemData.bIsWeapon)
        {
            OutReport += TEXT("Category: WEAPON\n");
            OutReport += FString::Printf(TEXT("Weapon Type: %s\n"), *ItemData.WeaponArchetype.ToString());
        }
        else if (ItemData.bIsArmor)
        {
            OutReport += TEXT("Category: ARMOR\n");
            OutReport += FString::Printf(TEXT("Armor Type: %s\n"), *ItemData.ArmorType.ToString());
        }
        else if (ItemData.bIsAmmo)
        {
            OutReport += TEXT("Category: AMMUNITION\n");
        }
        else if (ItemData.bIsConsumable)
        {
            OutReport += TEXT("Category: CONSUMABLE\n");
        }
    }

    OutReport += TEXT("\n");

    //========================================
    // Validation Errors
    //========================================
    OutReport += FString::Printf(TEXT("VALIDATION ERRORS (%d):\n"), ItemErrors.Num());
    for (int32 i = 0; i < ItemErrors.Num(); i++)
    {
        OutReport += FString::Printf(TEXT("  %d. %s\n"), i + 1, *ItemErrors[i]);
    }
    OutReport += TEXT("\n");

    //========================================
    // Usage in Loadouts
    //========================================
    OutReport += TEXT("USED IN LOADOUTS:\n");

    TArray<FName> AllLoadouts = LoadoutManager->GetAllLoadoutIDs();
    bool bFoundUsage = false;

    for (const FName& LoadoutID : AllLoadouts)
    {
        FLoadoutConfiguration LoadoutConfig;
        if (LoadoutManager->GetLoadoutConfigBP(LoadoutID, LoadoutConfig))
        {
            // Check equipment slots
            for (const auto& EquipPair : LoadoutConfig.StartingEquipment)
            {
                if (EquipPair.Value == ItemID)
                {
                    bFoundUsage = true;
                    OutReport += FString::Printf(TEXT("  - Loadout '%s'\n"), *LoadoutID.ToString());
                    OutReport += FString::Printf(TEXT("    Equipment Slot: %s\n"),
                        *UEnum::GetValueAsString(EquipPair.Key));
                    OutReport += TEXT("    WARNING: This loadout will NOT function without a valid item!\n");
                }
            }

            // Check inventory items
            bool bInMainInventory = false;
            for (const FSuspensePickupSpawnData& SpawnData : LoadoutConfig.MainInventory.StartingItems)
            {
                if (SpawnData.ItemID == ItemID)
                {
                    if (!bInMainInventory)
                    {
                        bFoundUsage = true;
                        bInMainInventory = true;
                        OutReport += FString::Printf(TEXT("  - Loadout '%s' (Main Inventory)\n"), *LoadoutID.ToString());
                    }
                }
            }
        }
    }

    if (!bFoundUsage)
    {
        OutReport += TEXT("  (No loadouts found - this shouldn't happen!)\n");
    }

    OutReport += TEXT("\n");

    //========================================
    // Specific Guidance Based on Item Type
    //========================================
    OutReport += TEXT("REQUIRED ACTIONS TO FIX:\n");

    if (bHasItemData && ItemData.bIsWeapon)
    {
        // Weapon-specific guidance
        OutReport += TEXT("For WEAPONS, ensure the following are configured:\n");
        OutReport += TEXT("  1. WeaponInitialization section:\n");
        OutReport += TEXT("     - WeaponAttributeSetClass must be set\n");
        OutReport += TEXT("       (e.g., UMedComWeaponAttributeSet or custom weapon AttributeSet)\n");
        OutReport += TEXT("     - WeaponInitEffect must be set\n");
        OutReport += TEXT("       (GameplayEffect that initializes weapon stats like damage, accuracy)\n");
        OutReport += TEXT("  2. FireModes array:\n");
        OutReport += TEXT("     - At least ONE fire mode must be defined\n");
        OutReport += TEXT("     - Each fire mode needs:\n");
        OutReport += TEXT("       * FireModeTag (e.g., Weapon.FireMode.Single)\n");
        OutReport += TEXT("       * FireModeAbility (GameplayAbility class implementing the fire behavior)\n");
        OutReport += TEXT("  3. DefaultFireMode must match one of the FireModes\n");

        // Check specific missing fields
        if (!ItemData.WeaponInitialization.WeaponAttributeSetClass)
        {
            OutReport += TEXT("\n  MISSING: WeaponAttributeSetClass\n");
            OutReport += TEXT("  → Open DataTable, find this weapon row\n");
            OutReport += TEXT("  → Expand 'Weapon Initialization' section\n");
            OutReport += TEXT("  → Set WeaponAttributeSetClass to your weapon AttributeSet\n");
        }

        if (!ItemData.WeaponInitialization.WeaponInitEffect)
        {
            OutReport += TEXT("\n  MISSING: WeaponInitEffect\n");
            OutReport += TEXT("  → Create a GameplayEffect Blueprint (or use existing one)\n");
            OutReport += TEXT("  → Configure it to set weapon attributes (damage, fire rate, etc.)\n");
            OutReport += TEXT("  → Assign it to WeaponInitEffect field\n");
        }

        if (ItemData.FireModes.Num() == 0)
        {
            OutReport += TEXT("\n  MISSING: Fire Modes\n");
            OutReport += TEXT("  → Add at least one entry to FireModes array\n");
            OutReport += TEXT("  → For each mode, set FireModeTag and FireModeAbility\n");
            OutReport += TEXT("  → Common fire modes: Single, Burst, Auto\n");
        }
        else
        {
            // Check if fire modes have abilities
            for (int32 i = 0; i < ItemData.FireModes.Num(); i++)
            {
                if (!ItemData.FireModes[i].FireModeAbility)
                {
                    OutReport += FString::Printf(TEXT("\n  MISSING: FireMode[%d] ability class\n"), i);
                    OutReport += FString::Printf(TEXT("  → Fire mode '%s' needs a GameplayAbility class\n"),
                        *ItemData.FireModes[i].DisplayName.ToString());
                }
            }
        }
    }
    else if (bHasItemData && ItemData.bIsArmor)
    {
        // Armor-specific guidance
        OutReport += TEXT("For ARMOR, ensure the following are configured:\n");
        OutReport += TEXT("  1. ArmorInitialization section:\n");
        OutReport += TEXT("     - ArmorAttributeSetClass must be set\n");
        OutReport += TEXT("     - ArmorInitEffect must be set\n");
        OutReport += TEXT("  2. ArmorType tag must be valid\n");

        if (!ItemData.ArmorInitialization.ArmorAttributeSetClass)
        {
            OutReport += TEXT("\n  MISSING: ArmorAttributeSetClass\n");
        }

        if (!ItemData.ArmorInitialization.ArmorInitEffect)
        {
            OutReport += TEXT("\n  MISSING: ArmorInitEffect\n");
        }
    }
    else if (bHasItemData && ItemData.bIsAmmo)
    {
        // Ammo-specific guidance
        OutReport += TEXT("For AMMUNITION, ensure the following are configured:\n");
        OutReport += TEXT("  1. AmmoInitialization section:\n");
        OutReport += TEXT("     - AmmoAttributeSetClass must be set\n");
        OutReport += TEXT("     - AmmoInitEffect must be set\n");
        OutReport += TEXT("  2. AmmoCaliber tag must match weapon AmmoType\n");
        OutReport += TEXT("  3. CompatibleWeapons should list weapon archetypes\n");

        if (!ItemData.AmmoInitialization.AmmoAttributeSetClass)
        {
            OutReport += TEXT("\n  MISSING: AmmoAttributeSetClass\n");
        }

        if (!ItemData.AmmoInitialization.AmmoInitEffect)
        {
            OutReport += TEXT("\n  MISSING: AmmoInitEffect\n");
        }
    }
    else if (bHasItemData && ItemData.bIsEquippable)
    {
        // General equipment guidance
        OutReport += TEXT("For EQUIPPABLE ITEMS, ensure:\n");
        OutReport += TEXT("  1. EquipmentSlot tag is set\n");
        OutReport += TEXT("  2. EquipmentActorClass is set\n");
        OutReport += TEXT("  3. EquipmentAttributeSet is set (if not weapon/armor)\n");
        OutReport += TEXT("  4. EquipmentInitEffect is set (if not weapon/armor)\n");
    }

    OutReport += TEXT("\n");
    OutReport += TEXT("========================================\n");
}

//========================================
// Loadout System Initialization
//========================================

bool USuspenseGameInstance::InitializeLoadoutSystem()
{
    UE_LOG(LogSuspenseGameInstance, Warning, TEXT("=== LOADOUT SYSTEM INITIALIZATION START ==="));

    if (bLoadoutSystemInitialized)
    {
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Loadout system already initialized"));
        return true;
    }

    //========================================
    // Step 1: Get LoadoutManager Subsystem
    //========================================
    {
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Step 1: Getting LoadoutManager subsystem..."));
        USuspenseLoadoutManager* LoadoutManager = GetLoadoutManager();
        if (!LoadoutManager)
        {
            UE_LOG(LogSuspenseGameInstance, Error, TEXT("LoadoutManager subsystem not found!"));
            return false;
        }
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Step 1: SUCCESS - LoadoutManager found at %p"), LoadoutManager);
    }

    //========================================
    // Step 2: Verify DataTable Configured
    //========================================
    {
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Step 2: Checking LoadoutConfigurationsTable..."));
        if (!LoadoutConfigurationsTable)
        {
            UE_LOG(LogSuspenseGameInstance, Error, TEXT("LoadoutConfigurationsTable not set in GameInstance!"));
            UE_LOG(LogSuspenseGameInstance, Error, TEXT("CRITICAL: Make sure BP_SuspenseGameInstance has LoadoutConfigurationsTable field set!"));
            return false;
        }
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Step 2: SUCCESS - DataTable found: %s"), *LoadoutConfigurationsTable->GetName());
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("  - DataTable Path: %s"), *LoadoutConfigurationsTable->GetPathName());
    }

    //========================================
    // Step 3: Verify DataTable Structure
    //========================================
    {
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Step 3: Verifying DataTable structure..."));
        const UScriptStruct* RowStruct = LoadoutConfigurationsTable->GetRowStruct();
        if (!RowStruct)
        {
            UE_LOG(LogSuspenseGameInstance, Error, TEXT("LoadoutConfigurationsTable has no row structure!"));
            return false;
        }
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("  - Row Structure Name: %s"), *RowStruct->GetName());
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("  - Expected Structure: FLoadoutConfiguration"));

        // Verify correct structure
        if (RowStruct != FLoadoutConfiguration::StaticStruct() &&
            !RowStruct->IsChildOf(FLoadoutConfiguration::StaticStruct()))
        {
            UE_LOG(LogSuspenseGameInstance, Error,
                TEXT("LoadoutConfigurationsTable has incorrect row structure! Expected: FLoadoutConfiguration, Got: %s"),
                *RowStruct->GetName());
            return false;
        }
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Step 3: SUCCESS - Row structure is correct"));
    }

    //========================================
    // Step 4: Examine DataTable Contents
    //========================================
    {
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Step 4: Examining DataTable contents..."));
        TArray<FName> RowNames = LoadoutConfigurationsTable->GetRowNames();
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("  - Total rows in DataTable: %d"), RowNames.Num());
        for (int32 i = 0; i < RowNames.Num(); i++)
        {
            UE_LOG(LogSuspenseGameInstance, Warning, TEXT("  - Row[%d]: %s"), i, *RowNames[i].ToString());
        }
    }

    //========================================
    // Step 5: Load Configurations into LoadoutManager
    //========================================
    {
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Step 5: Loading configurations into LoadoutManager..."));
        USuspenseLoadoutManager* LoadoutManager = GetLoadoutManager();

        int32 LoadedCount = LoadoutManager->LoadLoadoutTable(LoadoutConfigurationsTable);
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("  - LoadLoadoutTable returned: %d configurations"), LoadedCount);

        if (LoadedCount == 0)
        {
            UE_LOG(LogSuspenseGameInstance, Error, TEXT("No loadout configurations were loaded from DataTable!"));
            UE_LOG(LogSuspenseGameInstance, Error, TEXT("Check LoadoutManager->LoadLoadoutTable implementation"));
            return false;
        }

        UE_LOG(LogSuspenseGameInstance, Log, TEXT("Step 5: SUCCESS - Loaded %d loadout configurations"), LoadedCount);
    }

    //========================================
    // Step 6: Validate Configurations (Optional)
    //========================================
    if (bValidateLoadoutsOnStartup)
    {
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Step 6: Validating loadout configurations..."));
        ValidateLoadoutConfigurations();
    }

    //========================================
    // Step 7: Check Default Loadout
    //========================================
    {
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Step 7: Checking default loadout..."));
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("  - DefaultLoadoutID: %s"), *DefaultLoadoutID.ToString());

        USuspenseLoadoutManager* LoadoutManager = GetLoadoutManager();

        if (!LoadoutManager->IsLoadoutValid(DefaultLoadoutID))
        {
            UE_LOG(LogSuspenseGameInstance, Warning, TEXT("  - Default loadout '%s' not found in LoadoutManager"), *DefaultLoadoutID.ToString());

            TArray<FName> AllLoadouts = LoadoutManager->GetAllLoadoutIDs();
            UE_LOG(LogSuspenseGameInstance, Warning, TEXT("  - Available loadouts in manager: %d"), AllLoadouts.Num());

            if (AllLoadouts.Num() > 0)
            {
                for (int32 i = 0; i < AllLoadouts.Num(); i++)
                {
                    UE_LOG(LogSuspenseGameInstance, Warning, TEXT("    [%d]: %s"), i, *AllLoadouts[i].ToString());
                }

                UE_LOG(LogSuspenseGameInstance, Warning, TEXT("  - Using first available loadout: '%s'"),
                    *AllLoadouts[0].ToString());
                DefaultLoadoutID = AllLoadouts[0];
            }
            else
            {
                UE_LOG(LogSuspenseGameInstance, Error, TEXT("No valid loadouts available in LoadoutManager!"));
                return false;
            }
        }
        else
        {
            UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Step 7: SUCCESS - Default loadout '%s' is valid"), *DefaultLoadoutID.ToString());
        }
    }

    //========================================
    // Step 8: Log Final Summary
    //========================================
    if (bLogLoadoutOperations)
    {
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Step 8: Final loadout summary:"));
        USuspenseLoadoutManager* LoadoutManager = GetLoadoutManager();
        TArray<FName> AllLoadouts = LoadoutManager->GetAllLoadoutIDs();
        UE_LOG(LogSuspenseGameInstance, Log, TEXT("Available loadouts (%d total):"), AllLoadouts.Num());
        for (const FName& LoadoutID : AllLoadouts)
        {
            UE_LOG(LogSuspenseGameInstance, Log, TEXT("  - %s"), *LoadoutID.ToString());
        }
    }

    bLoadoutSystemInitialized = true;
    UE_LOG(LogSuspenseGameInstance, Warning, TEXT("=== LOADOUT SYSTEM INITIALIZATION COMPLETE ==="));
    return true;
}

void USuspenseGameInstance::ValidateLoadoutConfigurations()
{
    USuspenseLoadoutManager* LoadoutManager = GetLoadoutManager();
    if (!LoadoutManager)
    {
        return;
    }

    TArray<FString> ValidationErrors;
    bool bAllValid = LoadoutManager->ValidateAllConfigurations(ValidationErrors);

    if (!bAllValid)
    {
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Loadout validation found %d issues:"), ValidationErrors.Num());
        for (const FString& Error : ValidationErrors)
        {
            UE_LOG(LogSuspenseGameInstance, Warning, TEXT("  - %s"), *Error);
        }
    }
    else
    {
        UE_LOG(LogSuspenseGameInstance, Log, TEXT("All loadout configurations validated successfully"));
    }
}

//========================================
// Animation System Initialization
//========================================

bool USuspenseGameInstance::InitializeAnimationSystem()
{
    UE_LOG(LogSuspenseGameInstance, Log, TEXT("=== ANIMATION SYSTEM INITIALIZATION START ==="));

    if (bAnimationSystemInitialized)
    {
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Animation system already initialized"));
        return true;
    }

    //========================================
    // Step 1: Get WeaponAnimationSubsystem
    //========================================
    {
        UWeaponAnimationSubsystem* AnimationSubsystem = GetWeaponAnimationSubsystem();
        if (!AnimationSubsystem)
        {
            UE_LOG(LogSuspenseGameInstance, Error, TEXT("WeaponAnimationSubsystem not found!"));
            return false;
        }
    }

    //========================================
    // Step 2: Verify DataTable Configured
    //========================================
    {
        if (!WeaponAnimationsTable)
        {
            UE_LOG(LogSuspenseGameInstance, Error, TEXT("WeaponAnimationsTable not set in GameInstance!"));
            UE_LOG(LogSuspenseGameInstance, Error, TEXT("Please configure it in BP_SuspenseGameInstance"));

            // Attempt to load default
            WeaponAnimationsTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/DT_Anims"));
            if (WeaponAnimationsTable)
            {
                UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Loaded DT_Anims from default path"));
            }
            else
            {
                return false;
            }
        }
    }

    //========================================
    // Step 3: Verify DataTable Structure
    //========================================
    {
        const UScriptStruct* RowStruct = WeaponAnimationsTable->GetRowStruct();
        if (!RowStruct)
        {
            UE_LOG(LogSuspenseGameInstance, Error, TEXT("WeaponAnimationsTable has no row structure!"));
            return false;
        }

        // Verify correct structure (FAnimationStateData)
        if (RowStruct->GetName() != TEXT("AnimationStateData"))
        {
            UE_LOG(LogSuspenseGameInstance, Error,
                TEXT("WeaponAnimationsTable has incorrect row structure! Expected: FAnimationStateData, Got: %s"),
                *RowStruct->GetName());
            return false;
        }
    }

    //========================================
    // Step 4: Load DataTable into Subsystem
    //========================================
    {
        UWeaponAnimationSubsystem* AnimationSubsystem = GetWeaponAnimationSubsystem();

        if (!AnimationSubsystem->LoadAnimationDataTable(WeaponAnimationsTable))
        {
            UE_LOG(LogSuspenseGameInstance, Error, TEXT("Failed to load animation DataTable into subsystem"));
            return false;
        }

        // Get loaded weapon types
        TArray<FGameplayTag> LoadedWeaponTypes = AnimationSubsystem->GetAvailableWeaponTypes_Implementation();
        UE_LOG(LogSuspenseGameInstance, Log, TEXT("Loaded %d weapon animation configurations:"), LoadedWeaponTypes.Num());

        if (bLogAnimationOperations)
        {
            for (const FGameplayTag& WeaponType : LoadedWeaponTypes)
            {
                UE_LOG(LogSuspenseGameInstance, Log, TEXT("  - %s"), *WeaponType.ToString());
            }
        }
    }

    //========================================
    // Step 5: Validate Configurations (Optional)
    //========================================
    if (bValidateAnimationsOnStartup)
    {
        ValidateAnimationConfigurations();
    }

    bAnimationSystemInitialized = true;
    UE_LOG(LogSuspenseGameInstance, Log, TEXT("=== ANIMATION SYSTEM INITIALIZATION COMPLETE ==="));
    return true;
}

void USuspenseGameInstance::ValidateAnimationConfigurations()
{
    UWeaponAnimationSubsystem* AnimationSubsystem = GetWeaponAnimationSubsystem();
    if (!AnimationSubsystem)
    {
        return;
    }

    TArray<FGameplayTag> WeaponTypes = AnimationSubsystem->GetAvailableWeaponTypes_Implementation();
    int32 ErrorCount = 0;

    for (const FGameplayTag& WeaponType : WeaponTypes)
    {
        TArray<FString> ValidationErrors;
        if (!AnimationSubsystem->ValidateAnimationData_Implementation(WeaponType, ValidationErrors))
        {
            ErrorCount++;
            UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Validation errors for %s:"), *WeaponType.ToString());
            for (const FString& Error : ValidationErrors)
            {
                UE_LOG(LogSuspenseGameInstance, Warning, TEXT("  - %s"), *Error);
            }
        }
    }

    if (ErrorCount == 0)
    {
        UE_LOG(LogSuspenseGameInstance, Log, TEXT("All animation configurations validated successfully"));
    }
    else
    {
        UE_LOG(LogSuspenseGameInstance, Warning, TEXT("Found validation issues in %d weapon configurations"), ErrorCount);
    }
}

//========================================
// System Event Handlers
//========================================

void USuspenseGameInstance::RegisterGlobalEventHandlers()
{
    if (GEngine)
    {
        // Register network error handlers
        GEngine->OnNetworkFailure().AddUObject(this, &USuspenseGameInstance::HandleNetworkError);

        // Register travel error handlers
        GEngine->OnTravelFailure().AddUObject(this, &USuspenseGameInstance::HandleTravelFailure);

        UE_LOG(LogSuspenseGameInstance, Verbose, TEXT("Global event handlers registered"));
    }
}

void USuspenseGameInstance::UnregisterGlobalEventHandlers()
{
    if (GEngine)
    {
        GEngine->OnNetworkFailure().RemoveAll(this);
        GEngine->OnTravelFailure().RemoveAll(this);

        UE_LOG(LogSuspenseGameInstance, Verbose, TEXT("Global event handlers unregistered"));
    }
}

void USuspenseGameInstance::HandleSystemMessage(const FString& Message, float Duration)
{
    if (bIsShuttingDown)
    {
        return;
    }

    UE_LOG(LogSuspenseGameInstance, Warning, TEXT("System Message: %s"), *Message);

    // Display on screen
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, Duration, FColor::Yellow, Message);
    }
}

void USuspenseGameInstance::HandleNetworkError(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
    if (bIsShuttingDown || !World)
    {
        return;
    }

    // Convert failure type to string
    FString FailureTypeString;
    switch (FailureType)
    {
        case ENetworkFailure::NetDriverAlreadyExists:
            FailureTypeString = TEXT("NetDriverAlreadyExists");
            break;
        case ENetworkFailure::NetDriverCreateFailure:
            FailureTypeString = TEXT("NetDriverCreateFailure");
            break;
        case ENetworkFailure::NetDriverListenFailure:
            FailureTypeString = TEXT("NetDriverListenFailure");
            break;
        case ENetworkFailure::ConnectionLost:
            FailureTypeString = TEXT("ConnectionLost");
            break;
        case ENetworkFailure::ConnectionTimeout:
            FailureTypeString = TEXT("ConnectionTimeout");
            break;
        case ENetworkFailure::FailureReceived:
            FailureTypeString = TEXT("FailureReceived");
            break;
        case ENetworkFailure::OutdatedClient:
            FailureTypeString = TEXT("OutdatedClient");
            break;
        case ENetworkFailure::OutdatedServer:
            FailureTypeString = TEXT("OutdatedServer");
            break;
        case ENetworkFailure::PendingConnectionFailure:
            FailureTypeString = TEXT("PendingConnectionFailure");
            break;
        case ENetworkFailure::NetGuidMismatch:
            FailureTypeString = TEXT("NetGuidMismatch");
            break;
        case ENetworkFailure::NetChecksumMismatch:
            FailureTypeString = TEXT("NetChecksumMismatch");
            break;
        default:
            FailureTypeString = TEXT("Unknown");
            break;
    }

    FString ErrorMessage = FString::Printf(TEXT("Network Error: %s - %s"), *FailureTypeString, *ErrorString);
    UE_LOG(LogSuspenseGameInstance, Error, TEXT("%s"), *ErrorMessage);

    HandleSystemMessage(ErrorMessage, 10.0f);
}

void USuspenseGameInstance::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
    if (bIsShuttingDown || !World)
    {
        return;
    }

    // Convert failure type to string
    FString FailureTypeString;
    switch (FailureType)
    {
        case ETravelFailure::NoLevel:
            FailureTypeString = TEXT("NoLevel");
            break;
        case ETravelFailure::LoadMapFailure:
            FailureTypeString = TEXT("LoadMapFailure");
            break;
        case ETravelFailure::InvalidURL:
            FailureTypeString = TEXT("InvalidURL");
            break;
        case ETravelFailure::PackageMissing:
            FailureTypeString = TEXT("PackageMissing");
            break;
        case ETravelFailure::PackageVersion:
            FailureTypeString = TEXT("PackageVersion");
            break;
        case ETravelFailure::NoDownload:
            FailureTypeString = TEXT("NoDownload");
            break;
        case ETravelFailure::TravelFailure:
            FailureTypeString = TEXT("TravelFailure");
            break;
        case ETravelFailure::CheatCommands:
            FailureTypeString = TEXT("CheatCommands");
            break;
        case ETravelFailure::PendingNetGameCreateFailure:
            FailureTypeString = TEXT("PendingNetGameCreateFailure");
            break;
        case ETravelFailure::CloudSaveFailure:
            FailureTypeString = TEXT("CloudSaveFailure");
            break;
        case ETravelFailure::ServerTravelFailure:
            FailureTypeString = TEXT("ServerTravelFailure");
            break;
        case ETravelFailure::ClientTravelFailure:
            FailureTypeString = TEXT("ClientTravelFailure");
            break;
        default:
            FailureTypeString = TEXT("Unknown");
            break;
    }

    FString ErrorMessage = FString::Printf(TEXT("Travel Error: %s - %s"), *FailureTypeString, *ErrorString);
    UE_LOG(LogSuspenseGameInstance, Error, TEXT("%s"), *ErrorMessage);

    HandleSystemMessage(ErrorMessage, 10.0f);
}

void USuspenseGameInstance::CacheGameVersion()
{
    // Safely get game version from project settings
    FString ProjectName = FApp::GetProjectName();
    if (ProjectName.IsEmpty())
    {
        ProjectName = TEXT("MedCom");
    }

    FString BuildVersion = FApp::GetBuildVersion();
    if (BuildVersion.IsEmpty())
    {
        BuildVersion = TEXT("0.0.0");
    }

    CachedGameVersion = FString::Printf(TEXT("%s - %s"), *ProjectName, *BuildVersion);
}
