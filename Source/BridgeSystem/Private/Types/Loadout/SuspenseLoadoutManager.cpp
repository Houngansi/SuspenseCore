// Copyright Suspense Team. All Rights Reserved.

#include "Types/Loadout/SuspenseLoadoutManager.h"
#include "Interfaces/Inventory/ISuspenseInventory.h"
#include "Interfaces/Equipment/ISuspenseEquipment.h"
#include "Interfaces/Core/ISuspenseLoadout.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Delegates/SuspenseEventManager.h"

// Logging category
DEFINE_LOG_CATEGORY_STATIC(LogLoadoutManager, Log, All);

void USuspenseLoadoutManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (bIsInitialized)
    {
        return;
    }

    UE_LOG(LogLoadoutManager, Log, TEXT("Initializing LoadoutManager"));

    // Try to load default DataTable if path is set
    if (!DefaultLoadoutTablePath.IsEmpty())
    {
        TryLoadDefaultTable();
    }

    bIsInitialized = true;
}

void USuspenseLoadoutManager::Deinitialize()
{
    UE_LOG(LogLoadoutManager, Log, TEXT("Deinitializing LoadoutManager"));
    
    ClearCache();
    LoadedDataTable = nullptr;
    bIsInitialized = false;

    Super::Deinitialize();
}

int32 USuspenseLoadoutManager::LoadLoadoutTable(UDataTable* InLoadoutTable)
{
    if (!InLoadoutTable)
    {
        UE_LOG(LogLoadoutManager, Error, TEXT("LoadLoadoutTable: Invalid DataTable"));
        return 0;
    }

    // Verify row struct type
    if (!InLoadoutTable->GetRowStruct()->IsChildOf(FLoadoutConfiguration::StaticStruct()))
    {
        UE_LOG(LogLoadoutManager, Error, TEXT("LoadLoadoutTable: DataTable row struct is not FLoadoutConfiguration"));
        return 0;
    }

    FScopeLock Lock(&CacheCriticalSection);

    // Clear existing cache
    ClearCache();

    // Store new table
    LoadedDataTable = InLoadoutTable;

    // Cache configurations
    int32 LoadedCount = CacheConfigurationsFromTable();

    UE_LOG(LogLoadoutManager, Log, TEXT("LoadLoadoutTable: Loaded %d configurations from %s"), 
        LoadedCount, *InLoadoutTable->GetName());

    // Log statistics in verbose mode
    if (LoadedCount > 0)
    {
        LogLoadoutStatistics();
    }

    // Broadcast through EventDelegateManager for global C++ listeners
    if (USuspenseEventManager* EventManager = GetEventDelegateManager())
    {
        EventManager->NotifyLoadoutTableLoaded(InLoadoutTable, LoadedCount);
    }

    // Also broadcast through local delegate for Blueprint listeners
    OnLoadoutManagerChanged.Broadcast(NAME_None, nullptr, true);

    return LoadedCount;
}

void USuspenseLoadoutManager::ReloadConfigurations()
{
    if (!LoadedDataTable)
    {
        UE_LOG(LogLoadoutManager, Warning, TEXT("ReloadConfigurations: No DataTable loaded"));
        return;
    }

    FScopeLock Lock(&CacheCriticalSection);

    ClearCache();
    int32 ReloadedCount = CacheConfigurationsFromTable();

    UE_LOG(LogLoadoutManager, Log, TEXT("ReloadConfigurations: Reloaded %d configurations"), ReloadedCount);
}

const FLoadoutConfiguration* USuspenseLoadoutManager::GetLoadoutConfig(const FName& LoadoutID) const
{
    if (LoadoutID.IsNone())
    {
        return nullptr;
    }

    FScopeLock Lock(&CacheCriticalSection);
    return CachedConfigurations.Find(LoadoutID);
}

const FSuspenseInventoryConfig* USuspenseLoadoutManager::GetInventoryConfig(const FName& LoadoutID, const FName& InventoryName) const
{
    const FLoadoutConfiguration* LoadoutConfig = GetLoadoutConfig(LoadoutID);
    if (!LoadoutConfig)
    {
        return nullptr;
    }

    return LoadoutConfig->GetInventoryConfig(InventoryName);
}

bool USuspenseLoadoutManager::GetLoadoutConfigBP(const FName& LoadoutID, FLoadoutConfiguration& OutConfig) const
{
    const FLoadoutConfiguration* ConfigPtr = GetLoadoutConfig(LoadoutID);
    if (ConfigPtr)
    {
        OutConfig = *ConfigPtr;
        return true;
    }
    
    // Возвращаем дефолтную конфигурацию если не найдена
    OutConfig = FLoadoutConfiguration();
    return false;
}

bool USuspenseLoadoutManager::GetInventoryConfigBP(const FName& LoadoutID, const FName& InventoryName, FSuspenseInventoryConfig& OutConfig) const
{
    const FSuspenseInventoryConfig* ConfigPtr = GetInventoryConfig(LoadoutID, InventoryName);
    if (ConfigPtr)
    {
        OutConfig = *ConfigPtr;
        return true;
    }
    
    // Возвращаем дефолтную конфигурацию если не найдена
    OutConfig = FSuspenseInventoryConfig();
    return false;
}

TArray<FName> USuspenseLoadoutManager::GetInventoryNames(const FName& LoadoutID) const
{
    TArray<FName> InventoryNames;
    
    const FLoadoutConfiguration* LoadoutConfig = GetLoadoutConfig(LoadoutID);
    if (!LoadoutConfig)
    {
        return InventoryNames;
    }

    // Add main inventory
    InventoryNames.Add(NAME_None); // Main inventory uses NAME_None

    // Add additional inventories
    for (const auto& Pair : LoadoutConfig->AdditionalInventories)
    {
        InventoryNames.Add(Pair.Key);
    }

    return InventoryNames;
}

TArray<FEquipmentSlotConfig> USuspenseLoadoutManager::GetEquipmentSlots(const FName& LoadoutID) const
{
    const FLoadoutConfiguration* LoadoutConfig = GetLoadoutConfig(LoadoutID);
    if (!LoadoutConfig)
    {
        return TArray<FEquipmentSlotConfig>();
    }

    return LoadoutConfig->EquipmentSlots;
}

bool USuspenseLoadoutManager::IsLoadoutValid(const FName& LoadoutID) const
{
    const FLoadoutConfiguration* LoadoutConfig = GetLoadoutConfig(LoadoutID);
    return LoadoutConfig && LoadoutConfig->IsValid();
}

TArray<FName> USuspenseLoadoutManager::GetAllLoadoutIDs() const
{
    FScopeLock Lock(&CacheCriticalSection);

    TArray<FName> LoadoutIDs;
    CachedConfigurations.GetKeys(LoadoutIDs);
    return LoadoutIDs;
}

TArray<FName> USuspenseLoadoutManager::GetLoadoutsForClass(const FGameplayTag& CharacterClass) const
{
    TArray<FName> CompatibleLoadouts;

    FScopeLock Lock(&CacheCriticalSection);

    for (const auto& Pair : CachedConfigurations)
    {
        if (Pair.Value.IsCompatibleWithClass(CharacterClass))
        {
            CompatibleLoadouts.Add(Pair.Key);
        }
    }

    return CompatibleLoadouts;
}

bool USuspenseLoadoutManager::ApplyLoadoutToInventory(
    UObject* InventoryObject, 
    const FName& LoadoutID, 
    const FName& InventoryName) const
{
    if (!InventoryObject)
    {
        UE_LOG(LogLoadoutManager, Error, TEXT("ApplyLoadoutToInventory: Invalid inventory object"));
        return false;
    }

    // Check if object implements inventory interface
    ISuspenseInventory* InventoryInterface = Cast<ISuspenseInventory>(InventoryObject);
    if (!InventoryInterface)
    {
        UE_LOG(LogLoadoutManager, Error, TEXT("ApplyLoadoutToInventory: Object does not implement ISuspenseInventory"));
        return false;
    }

    const FSuspenseInventoryConfig* Config = GetInventoryConfig(LoadoutID, InventoryName);
    if (!Config)
    {
        UE_LOG(LogLoadoutManager, Error, TEXT("ApplyLoadoutToInventory: Config not found for %s/%s"), 
            *LoadoutID.ToString(), *InventoryName.ToString());
        return false;
    }

    // Initialize inventory through interface
    InventoryInterface->InitializeInventory(*Config);

    // Create starting items if configured
    if (Config->StartingItems.Num() > 0)
    {
        int32 CreatedCount = InventoryInterface->CreateItemsFromSpawnData(Config->StartingItems);
            
        UE_LOG(LogLoadoutManager, Log, TEXT("ApplyLoadoutToInventory: Created %d starting items"), CreatedCount);
    }

    // Broadcast change through EventDelegateManager
    BroadcastLoadoutChange(LoadoutID, nullptr, true);

    // Also broadcast the component-specific event
    if (USuspenseEventManager* EventManager = GetEventDelegateManager())
    {
        FGameplayTag ComponentType = FGameplayTag::RequestGameplayTag(TEXT("Loadout.Component.Inventory"));
        EventManager->NotifyLoadoutApplied(LoadoutID, InventoryObject, ComponentType, true);
    }

    UE_LOG(LogLoadoutManager, Log, TEXT("ApplyLoadoutToInventory: Applied %s to inventory"), 
        *LoadoutID.ToString());

    return true;
}

bool USuspenseLoadoutManager::ApplyLoadoutToEquipment(
    UObject* EquipmentObject, 
    const FName& LoadoutID) const
{
    if (!EquipmentObject)
    {
        UE_LOG(LogLoadoutManager, Error, TEXT("ApplyLoadoutToEquipment: Invalid equipment object"));
        return false;
    }

    const FLoadoutConfiguration* LoadoutConfig = GetLoadoutConfig(LoadoutID);
    if (!LoadoutConfig)
    {
        UE_LOG(LogLoadoutManager, Error, TEXT("ApplyLoadoutToEquipment: Loadout %s not found"), 
            *LoadoutID.ToString());
        return false;
    }

    bool bSuccess = false;

    // Path 1: Check if object implements ISuspenseLoadout (preferred)
    if (EquipmentObject->GetClass()->ImplementsInterface(USuspenseLoadoutInterface::StaticClass()))
    {
        FLoadoutApplicationResult Result = ISuspenseLoadout::Execute_ApplyLoadoutConfiguration(
            EquipmentObject, LoadoutID, const_cast<USuspenseLoadoutManager*>(this), false);
        
        bSuccess = Result.bSuccess;
        
        if (!bSuccess)
        {
            UE_LOG(LogLoadoutManager, Error, TEXT("ApplyLoadoutToEquipment: Loadout interface failed: %s"), 
                *Result.GetSummary());
        }
    }
    // Path 2: Check if object implements ISuspenseEquipment
    else if (EquipmentObject->GetClass()->ImplementsInterface(USuspenseEquipmentInterface::StaticClass()))
    {
        UE_LOG(LogLoadoutManager, Log, TEXT("ApplyLoadoutToEquipment: Using equipment interface for %s"), 
            *LoadoutID.ToString());
        
        // Apply equipment slots configuration
        for (const FEquipmentSlotConfig& SlotConfig : LoadoutConfig->EquipmentSlots)
        {
            UE_LOG(LogLoadoutManager, Verbose, TEXT("  Equipment slot: %s (Type: %s)"), 
                *SlotConfig.SlotTag.ToString(), 
                *UEnum::GetValueAsString(SlotConfig.SlotType));
        }
        
        // Apply starting equipment
        int32 EquippedCount = 0;
        for (const auto& EquipPair : LoadoutConfig->StartingEquipment)
        {
            if (!EquipPair.Value.IsNone())
            {
                // Create item instance for equipment
                FSuspenseInventoryItemInstance ItemInstance;
                ItemInstance.ItemID = EquipPair.Value;
                ItemInstance.InstanceID = FGuid::NewGuid();
                ItemInstance.Quantity = 1;
                
                // Try to equip through interface
                FSuspenseInventoryOperationResult EquipResult = ISuspenseEquipment::Execute_EquipItemInstance(
                    EquipmentObject, ItemInstance, true);
                
                if (EquipResult.bSuccess)
                {
                    EquippedCount++;
                    UE_LOG(LogLoadoutManager, Log, TEXT("  Equipped %s in slot %s"), 
                        *EquipPair.Value.ToString(), 
                        *UEnum::GetValueAsString(EquipPair.Key));
                }
                else
                {
                    UE_LOG(LogLoadoutManager, Warning, TEXT("  Failed to equip %s: %s"), 
                        *EquipPair.Value.ToString(), 
                        *EquipResult.ErrorMessage.ToString());
                }
            }
        }
        
        bSuccess = (EquippedCount > 0 || LoadoutConfig->StartingEquipment.Num() == 0);
        
        UE_LOG(LogLoadoutManager, Log, TEXT("ApplyLoadoutToEquipment: Equipped %d/%d items"), 
            EquippedCount, LoadoutConfig->StartingEquipment.Num());
    }
    else
    {
        UE_LOG(LogLoadoutManager, Error, TEXT("ApplyLoadoutToEquipment: Object does not implement required interfaces"));
        return false;
    }

    if (bSuccess)
    {
        // Broadcast change through EventDelegateManager
        BroadcastLoadoutChange(LoadoutID, nullptr, true);

        // Also broadcast the component-specific event
        if (USuspenseEventManager* EventManager = GetEventDelegateManager())
        {
            FGameplayTag ComponentType = FGameplayTag::RequestGameplayTag(TEXT("Loadout.Component.Equipment"));
            EventManager->NotifyLoadoutApplied(LoadoutID, EquipmentObject, ComponentType, true);
        }

        UE_LOG(LogLoadoutManager, Log, TEXT("ApplyLoadoutToEquipment: Successfully applied %s to equipment"), 
            *LoadoutID.ToString());
    }
    else
    {
        UE_LOG(LogLoadoutManager, Error, TEXT("ApplyLoadoutToEquipment: Failed to apply %s to equipment"), 
            *LoadoutID.ToString());
    }

    return bSuccess;
}

bool USuspenseLoadoutManager::ApplyLoadoutToObject(
    UObject* LoadoutObject,
    const FName& LoadoutID,
    bool bForceApply) const
{
    if (!LoadoutObject)
    {
        UE_LOG(LogLoadoutManager, Error, TEXT("ApplyLoadoutToObject: Invalid object"));
        return false;
    }

    // Check if object implements loadout interface
    if (!LoadoutObject->GetClass()->ImplementsInterface(USuspenseLoadoutInterface::StaticClass()))
    {
        UE_LOG(LogLoadoutManager, Error, TEXT("ApplyLoadoutToObject: Object does not implement ISuspenseLoadout"));
        return false;
    }

    // Cast to interface and apply
    TScriptInterface<ISuspenseLoadout> LoadoutInterface;
    LoadoutInterface.SetObject(LoadoutObject);
    LoadoutInterface.SetInterface(Cast<ISuspenseLoadout>(LoadoutObject));

    FLoadoutApplicationResult Result = ISuspenseLoadout::Execute_ApplyLoadoutConfiguration(
        LoadoutObject, LoadoutID, const_cast<USuspenseLoadoutManager*>(this), bForceApply);

    if (Result.bSuccess)
    {
        UE_LOG(LogLoadoutManager, Log, TEXT("ApplyLoadoutToObject: Successfully applied loadout %s"), 
            *LoadoutID.ToString());
    }
    else
    {
        UE_LOG(LogLoadoutManager, Warning, TEXT("ApplyLoadoutToObject: Failed to apply loadout %s: %s"), 
            *LoadoutID.ToString(), *Result.GetSummary());
    }

    return Result.bSuccess;
}

FName USuspenseLoadoutManager::GetDefaultLoadoutForClass(const FGameplayTag& CharacterClass) const
{
    const FName* DefaultLoadout = ClassDefaultLoadouts.Find(CharacterClass);
    return DefaultLoadout ? *DefaultLoadout : NAME_None;
}

bool USuspenseLoadoutManager::ValidateAllConfigurations(TArray<FString>& OutErrors) const
{
    OutErrors.Empty();
    bool bAllValid = true;

    FScopeLock Lock(&CacheCriticalSection);

    for (const auto& Pair : CachedConfigurations)
    {
        TArray<FString> ConfigErrors;
        if (!ValidateConfiguration(Pair.Value, ConfigErrors))
        {
            bAllValid = false;
            for (const FString& Error : ConfigErrors)
            {
                OutErrors.Add(FString::Printf(TEXT("[%s] %s"), *Pair.Key.ToString(), *Error));
            }
        }
    }

    return bAllValid;
}

float USuspenseLoadoutManager::GetTotalWeightCapacity(const FName& LoadoutID) const
{
    const FLoadoutConfiguration* LoadoutConfig = GetLoadoutConfig(LoadoutID);
    if (!LoadoutConfig)
    {
        return 0.0f;
    }

    return LoadoutConfig->GetTotalInventoryWeight();
}

int32 USuspenseLoadoutManager::GetTotalInventoryCells(const FName& LoadoutID) const
{
    const FLoadoutConfiguration* LoadoutConfig = GetLoadoutConfig(LoadoutID);
    if (!LoadoutConfig)
    {
        return 0;
    }

    return LoadoutConfig->GetTotalInventoryCells();
}

void USuspenseLoadoutManager::SetDefaultDataTablePath(const FString& Path)
{
    DefaultLoadoutTablePath = Path;
    
    // If already initialized, try to load the new table
    if (bIsInitialized && !Path.IsEmpty())
    {
        TryLoadDefaultTable();
    }
}

void USuspenseLoadoutManager::BroadcastLoadoutChange(const FName& LoadoutID, APlayerState* PlayerState, bool bSuccess) const
{
    // Broadcast through EventDelegateManager for global C++ listeners
    if (USuspenseEventManager* EventManager = GetEventDelegateManager())
    {
        EventManager->NotifyLoadoutChanged(LoadoutID, PlayerState, bSuccess);
    }
    
    // Also broadcast through local delegate for Blueprint listeners
    // Note: We need a non-const cast here because Blueprint delegates require non-const
    const_cast<USuspenseLoadoutManager*>(this)->OnLoadoutManagerChanged.Broadcast(LoadoutID, PlayerState, bSuccess);
}

int32 USuspenseLoadoutManager::CacheConfigurationsFromTable()
{
    if (!LoadedDataTable)
    {
        return 0;
    }

    int32 LoadedCount = 0;
    const TMap<FName, uint8*>& RowMap = LoadedDataTable->GetRowMap();

    for (const auto& Row : RowMap)
    {
        const FLoadoutConfiguration* LoadoutConfig = reinterpret_cast<const FLoadoutConfiguration*>(Row.Value);
        if (LoadoutConfig && LoadoutConfig->IsValid())
        {
            CachedConfigurations.Add(Row.Key, *LoadoutConfig);
            LoadedCount++;
        }
        else
        {
            UE_LOG(LogLoadoutManager, Warning, TEXT("Invalid loadout configuration: %s"), *Row.Key.ToString());
        }
    }

    return LoadedCount;
}

bool USuspenseLoadoutManager::ValidateConfiguration(const FLoadoutConfiguration& Config, TArray<FString>& OutErrors) const
{
    bool bIsValid = true;

    // Basic validation
    if (!Config.IsValid())
    {
        OutErrors.Add(TEXT("Configuration failed basic validation"));
        bIsValid = false;
    }

    // Check weight limits
    float TotalInventoryWeight = Config.GetTotalInventoryWeight();
    if (TotalInventoryWeight > Config.MaxTotalWeight)
    {
        OutErrors.Add(FString::Printf(TEXT("Total inventory weight (%.1f) exceeds max total weight (%.1f)"), 
            TotalInventoryWeight, Config.MaxTotalWeight));
        bIsValid = false;
    }

    // Validate equipment slots
    TSet<EEquipmentSlotType> UniqueSlots;
    for (const FEquipmentSlotConfig& SlotConfig : Config.EquipmentSlots)
    {
        if (UniqueSlots.Contains(SlotConfig.SlotType))
        {
            OutErrors.Add(FString::Printf(TEXT("Duplicate equipment slot type: %s"), 
                *UEnum::GetValueAsString(SlotConfig.SlotType)));
            bIsValid = false;
        }
        UniqueSlots.Add(SlotConfig.SlotType);
    }

    // Validate starting equipment references
    for (const auto& EquipPair : Config.StartingEquipment)
    {
        if (!Config.GetEquipmentSlotConfig(EquipPair.Key))
        {
            OutErrors.Add(FString::Printf(TEXT("Starting equipment references non-existent slot: %s"), 
                *UEnum::GetValueAsString(EquipPair.Key)));
            bIsValid = false;
        }
    }

    return bIsValid;
}

void USuspenseLoadoutManager::ClearCache()
{
    CachedConfigurations.Empty();
}

void USuspenseLoadoutManager::TryLoadDefaultTable()
{
    if (DefaultLoadoutTablePath.IsEmpty())
    {
        return;
    }

    UDataTable* DefaultTable = LoadObject<UDataTable>(nullptr, *DefaultLoadoutTablePath);
    if (DefaultTable)
    {
        LoadLoadoutTable(DefaultTable);
        UE_LOG(LogLoadoutManager, Log, TEXT("Loaded default DataTable from: %s"), *DefaultLoadoutTablePath);
    }
    else
    {
        UE_LOG(LogLoadoutManager, Warning, TEXT("Failed to load default DataTable from: %s"), *DefaultLoadoutTablePath);
    }
}

void USuspenseLoadoutManager::LogLoadoutStatistics() const
{
    FScopeLock Lock(&CacheCriticalSection);

    UE_LOG(LogLoadoutManager, Verbose, TEXT("=== Loadout Manager Statistics ==="));
    UE_LOG(LogLoadoutManager, Verbose, TEXT("Total Loadouts: %d"), CachedConfigurations.Num());

    for (const auto& Pair : CachedConfigurations)
    {
        const FLoadoutConfiguration& Config = Pair.Value;
        UE_LOG(LogLoadoutManager, Verbose, TEXT("Loadout '%s':"), *Pair.Key.ToString());
        UE_LOG(LogLoadoutManager, Verbose, TEXT("  - Inventories: %d"), 1 + Config.AdditionalInventories.Num());
        UE_LOG(LogLoadoutManager, Verbose, TEXT("  - Total Cells: %d"), Config.GetTotalInventoryCells());
        UE_LOG(LogLoadoutManager, Verbose, TEXT("  - Total Weight: %.1f"), Config.GetTotalInventoryWeight());
        UE_LOG(LogLoadoutManager, Verbose, TEXT("  - Equipment Slots: %d"), Config.EquipmentSlots.Num());
    }
}

USuspenseEventManager* USuspenseLoadoutManager::GetEventDelegateManager() const
{
    if (const UGameInstance* GameInstance = GetGameInstance())
    {
        return GameInstance->GetSubsystem<USuspenseEventManager>();
    }
    
    return nullptr;
}