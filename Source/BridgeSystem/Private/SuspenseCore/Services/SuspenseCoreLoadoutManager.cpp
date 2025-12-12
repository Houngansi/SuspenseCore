// SuspenseCoreLoadoutManager.cpp
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Services/SuspenseCoreLoadoutManager.h"
#include "SuspenseCore/Interfaces/Inventory/ISuspenseCoreInventory.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipment.h"
#include "SuspenseCore/Interfaces/Core/ISuspenseCoreLoadout.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreLoadout, Log, All);

void USuspenseCoreLoadoutManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (bIsInitialized) return;

    UE_LOG(LogSuspenseCoreLoadout, Log, TEXT("Initializing SuspenseCoreLoadoutManager"));

    if (!DefaultLoadoutTablePath.IsEmpty())
    {
        TryLoadDefaultTable();
    }

    // ALWAYS register default loadout to ensure equipment slots are available
    // even without a DataTable being loaded. This creates all 17 Tarkov-style
    // equipment slots with proper tags.
    RegisterDefaultLoadout(FName(TEXT("Default_Soldier")));

    bIsInitialized = true;
}

void USuspenseCoreLoadoutManager::Deinitialize()
{
    UE_LOG(LogSuspenseCoreLoadout, Log, TEXT("Deinitializing SuspenseCoreLoadoutManager"));

    ClearCache();
    LoadedDataTable = nullptr;
    bIsInitialized = false;

    Super::Deinitialize();
}

int32 USuspenseCoreLoadoutManager::LoadLoadoutTable(UDataTable* InLoadoutTable)
{
    if (!InLoadoutTable)
    {
        UE_LOG(LogSuspenseCoreLoadout, Error, TEXT("LoadLoadoutTable: Invalid DataTable"));
        return 0;
    }

    if (!InLoadoutTable->GetRowStruct()->IsChildOf(FLoadoutConfiguration::StaticStruct()))
    {
        UE_LOG(LogSuspenseCoreLoadout, Error, TEXT("LoadLoadoutTable: DataTable row struct is not FLoadoutConfiguration"));
        return 0;
    }

    FScopeLock Lock(&CacheCriticalSection);
    ClearCache();
    LoadedDataTable = InLoadoutTable;
    int32 LoadedCount = CacheConfigurationsFromTable();

    UE_LOG(LogSuspenseCoreLoadout, Log, TEXT("LoadLoadoutTable: Loaded %d configurations from %s"),
        LoadedCount, *InLoadoutTable->GetName());

    if (LoadedCount > 0)
    {
        LogLoadoutStatistics();
    }

    OnLoadoutChanged.Broadcast(NAME_None, nullptr, true);
    return LoadedCount;
}

void USuspenseCoreLoadoutManager::ReloadConfigurations()
{
    if (!LoadedDataTable)
    {
        UE_LOG(LogSuspenseCoreLoadout, Warning, TEXT("ReloadConfigurations: No DataTable loaded"));
        return;
    }

    FScopeLock Lock(&CacheCriticalSection);
    ClearCache();
    int32 ReloadedCount = CacheConfigurationsFromTable();
    UE_LOG(LogSuspenseCoreLoadout, Log, TEXT("ReloadConfigurations: Reloaded %d configurations"), ReloadedCount);
}

const FLoadoutConfiguration* USuspenseCoreLoadoutManager::GetLoadoutConfig(const FName& LoadoutID) const
{
    if (LoadoutID.IsNone()) return nullptr;
    FScopeLock Lock(&CacheCriticalSection);
    return CachedConfigurations.Find(LoadoutID);
}

const FSuspenseCoreLoadoutInventoryConfig* USuspenseCoreLoadoutManager::GetInventoryConfig(const FName& LoadoutID, const FName& InventoryName) const
{
    const FLoadoutConfiguration* LoadoutConfig = GetLoadoutConfig(LoadoutID);
    if (!LoadoutConfig) return nullptr;
    return LoadoutConfig->GetInventoryConfig(InventoryName);
}

bool USuspenseCoreLoadoutManager::GetLoadoutConfigBP(const FName& LoadoutID, FLoadoutConfiguration& OutConfig) const
{
    const FLoadoutConfiguration* ConfigPtr = GetLoadoutConfig(LoadoutID);
    if (ConfigPtr)
    {
        OutConfig = *ConfigPtr;
        return true;
    }
    OutConfig = FLoadoutConfiguration();
    return false;
}

bool USuspenseCoreLoadoutManager::GetInventoryConfigBP(const FName& LoadoutID, const FName& InventoryName, FSuspenseCoreLoadoutInventoryConfig& OutConfig) const
{
    const FSuspenseCoreLoadoutInventoryConfig* ConfigPtr = GetInventoryConfig(LoadoutID, InventoryName);
    if (ConfigPtr)
    {
        OutConfig = *ConfigPtr;
        return true;
    }
    OutConfig = FSuspenseCoreLoadoutInventoryConfig();
    return false;
}

TArray<FName> USuspenseCoreLoadoutManager::GetInventoryNames(const FName& LoadoutID) const
{
    TArray<FName> InventoryNames;
    const FLoadoutConfiguration* LoadoutConfig = GetLoadoutConfig(LoadoutID);
    if (!LoadoutConfig) return InventoryNames;

    InventoryNames.Add(NAME_None);
    for (const auto& Pair : LoadoutConfig->AdditionalInventories)
    {
        InventoryNames.Add(Pair.Key);
    }
    return InventoryNames;
}

TArray<FEquipmentSlotConfig> USuspenseCoreLoadoutManager::GetEquipmentSlots(const FName& LoadoutID) const
{
    const FLoadoutConfiguration* LoadoutConfig = GetLoadoutConfig(LoadoutID);
    if (!LoadoutConfig) return TArray<FEquipmentSlotConfig>();
    return LoadoutConfig->EquipmentSlots;
}

bool USuspenseCoreLoadoutManager::IsLoadoutValid(const FName& LoadoutID) const
{
    const FLoadoutConfiguration* LoadoutConfig = GetLoadoutConfig(LoadoutID);
    return LoadoutConfig && LoadoutConfig->IsValid();
}

TArray<FName> USuspenseCoreLoadoutManager::GetAllLoadoutIDs() const
{
    FScopeLock Lock(&CacheCriticalSection);
    TArray<FName> LoadoutIDs;
    CachedConfigurations.GetKeys(LoadoutIDs);
    return LoadoutIDs;
}

TArray<FName> USuspenseCoreLoadoutManager::GetLoadoutsForClass(const FGameplayTag& CharacterClass) const
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

bool USuspenseCoreLoadoutManager::ApplyLoadoutToInventory(UObject* InventoryObject, const FName& LoadoutID, const FName& InventoryName) const
{
    if (!InventoryObject)
    {
        UE_LOG(LogSuspenseCoreLoadout, Error, TEXT("ApplyLoadoutToInventory: Invalid inventory object"));
        return false;
    }

    ISuspenseCoreInventory* InventoryInterface = Cast<ISuspenseCoreInventory>(InventoryObject);
    if (!InventoryInterface)
    {
        UE_LOG(LogSuspenseCoreLoadout, Error, TEXT("ApplyLoadoutToInventory: Object does not implement ISuspenseCoreInventory"));
        return false;
    }

    const FSuspenseCoreLoadoutInventoryConfig* Config = GetInventoryConfig(LoadoutID, InventoryName);
    if (!Config)
    {
        UE_LOG(LogSuspenseCoreLoadout, Error, TEXT("ApplyLoadoutToInventory: Config not found for %s/%s"),
            *LoadoutID.ToString(), *InventoryName.ToString());
        return false;
    }

    // Initialize inventory with grid size and weight
    InventoryInterface->Initialize(Config->Width, Config->Height, Config->MaxWeight);

    // Set allowed item types if specified
    if (!Config->AllowedItemTypes.IsEmpty())
    {
        InventoryInterface->SetAllowedItemTypes(Config->AllowedItemTypes);
    }

    // Add starting items
    int32 CreatedCount = 0;
    if (Config->StartingItems.Num() > 0)
    {
        for (const FSuspenseCorePickupSpawnData& SpawnData : Config->StartingItems)
        {
            if (InventoryInterface->AddItemByID(SpawnData.ItemID, SpawnData.Quantity))
            {
                CreatedCount++;
            }
        }
        UE_LOG(LogSuspenseCoreLoadout, Log, TEXT("ApplyLoadoutToInventory: Created %d starting items"), CreatedCount);
    }

    BroadcastLoadoutChange(LoadoutID, nullptr, true);
    return true;
}

bool USuspenseCoreLoadoutManager::ApplyLoadoutToEquipment(UObject* EquipmentObject, const FName& LoadoutID) const
{
    if (!EquipmentObject)
    {
        UE_LOG(LogSuspenseCoreLoadout, Error, TEXT("ApplyLoadoutToEquipment: Invalid equipment object"));
        return false;
    }

    const FLoadoutConfiguration* LoadoutConfig = GetLoadoutConfig(LoadoutID);
    if (!LoadoutConfig)
    {
        UE_LOG(LogSuspenseCoreLoadout, Error, TEXT("ApplyLoadoutToEquipment: Loadout %s not found"), *LoadoutID.ToString());
        return false;
    }

    bool bSuccess = false;

    if (EquipmentObject->GetClass()->ImplementsInterface(USuspenseCoreEquipment::StaticClass()))
    {
        int32 EquippedCount = 0;
        for (const auto& EquipPair : LoadoutConfig->StartingEquipment)
        {
            if (!EquipPair.Value.IsNone())
            {
                FSuspenseCoreInventoryItemInstance ItemInstance;
                ItemInstance.ItemID = EquipPair.Value;
                ItemInstance.InstanceID = FGuid::NewGuid();
                ItemInstance.Quantity = 1;

                FSuspenseInventoryOperationResult EquipResult = ISuspenseCoreEquipment::Execute_EquipItemInstance(
                    EquipmentObject, ItemInstance, true);

                if (EquipResult.bSuccess) EquippedCount++;
            }
        }
        bSuccess = (EquippedCount > 0 || LoadoutConfig->StartingEquipment.Num() == 0);
    }

    if (bSuccess)
    {
        BroadcastLoadoutChange(LoadoutID, nullptr, true);
    }
    return bSuccess;
}

bool USuspenseCoreLoadoutManager::ApplyLoadoutToObject(UObject* LoadoutObject, const FName& LoadoutID, bool bForceApply) const
{
    if (!LoadoutObject)
    {
        UE_LOG(LogSuspenseCoreLoadout, Error, TEXT("ApplyLoadoutToObject: Invalid object"));
        return false;
    }

    if (!LoadoutObject->GetClass()->ImplementsInterface(USuspenseCoreLoadout::StaticClass()))
    {
        UE_LOG(LogSuspenseCoreLoadout, Error, TEXT("ApplyLoadoutToObject: Object does not implement ISuspenseCoreLoadout"));
        return false;
    }

    FSuspenseCoreLoadoutResult Result = ISuspenseCoreLoadout::Execute_ApplyLoadoutConfiguration(
        LoadoutObject, LoadoutID, const_cast<USuspenseCoreLoadoutManager*>(this), bForceApply);

    return Result.bSuccess;
}

FName USuspenseCoreLoadoutManager::GetDefaultLoadoutForClass(const FGameplayTag& CharacterClass) const
{
    const FName* DefaultLoadout = ClassDefaultLoadouts.Find(CharacterClass);
    return DefaultLoadout ? *DefaultLoadout : NAME_None;
}

bool USuspenseCoreLoadoutManager::ValidateAllConfigurations(TArray<FString>& OutErrors) const
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

float USuspenseCoreLoadoutManager::GetTotalWeightCapacity(const FName& LoadoutID) const
{
    const FLoadoutConfiguration* LoadoutConfig = GetLoadoutConfig(LoadoutID);
    return LoadoutConfig ? LoadoutConfig->GetTotalInventoryWeight() : 0.0f;
}

int32 USuspenseCoreLoadoutManager::GetTotalInventoryCells(const FName& LoadoutID) const
{
    const FLoadoutConfiguration* LoadoutConfig = GetLoadoutConfig(LoadoutID);
    return LoadoutConfig ? LoadoutConfig->GetTotalInventoryCells() : 0;
}

void USuspenseCoreLoadoutManager::SetDefaultDataTablePath(const FString& Path)
{
    DefaultLoadoutTablePath = Path;
    if (bIsInitialized && !Path.IsEmpty())
    {
        TryLoadDefaultTable();
    }
}

bool USuspenseCoreLoadoutManager::RegisterDefaultLoadout(const FName& LoadoutID)
{
    FScopeLock Lock(&CacheCriticalSection);

    // Check if already exists
    if (CachedConfigurations.Contains(LoadoutID))
    {
        UE_LOG(LogSuspenseCoreLoadout, Verbose, TEXT("RegisterDefaultLoadout: Loadout %s already exists"), *LoadoutID.ToString());
        return true;
    }

    // Create default configuration - this automatically calls SetupDefaultEquipmentSlots()
    FLoadoutConfiguration DefaultConfig;
    DefaultConfig.LoadoutID = LoadoutID;
    DefaultConfig.LoadoutName = FText::FromString(TEXT("Default Soldier Loadout"));
    DefaultConfig.Description = FText::FromString(TEXT("Standard PMC equipment configuration with all 17 slots"));

    // The constructor already calls SetupDefaultEquipmentSlots() which creates all slots:
    // - 4 Weapon slots (PrimaryWeapon, SecondaryWeapon, Holster, Scabbard)
    // - 4 Head gear slots (Headwear, Earpiece, Eyewear, FaceCover)
    // - 2 Body gear slots (BodyArmor, TacticalRig)
    // - 2 Storage slots (Backpack, SecureContainer)
    // - 4 Quick slots (QuickSlot1-4)
    // - 1 Special slot (Armband)

    if (!DefaultConfig.IsValid())
    {
        UE_LOG(LogSuspenseCoreLoadout, Error, TEXT("RegisterDefaultLoadout: Failed to create valid default configuration"));
        return false;
    }

    CachedConfigurations.Add(LoadoutID, DefaultConfig);

    UE_LOG(LogSuspenseCoreLoadout, Log, TEXT("RegisterDefaultLoadout: Registered %s with %d equipment slots"),
        *LoadoutID.ToString(), DefaultConfig.EquipmentSlots.Num());

    // Log slot details for debugging
    for (const FEquipmentSlotConfig& SlotConfig : DefaultConfig.EquipmentSlots)
    {
        UE_LOG(LogSuspenseCoreLoadout, Verbose, TEXT("  - Slot: %s (Type: %d, Tag: %s)"),
            *SlotConfig.DisplayName.ToString(),
            static_cast<int32>(SlotConfig.SlotType),
            *SlotConfig.SlotTag.ToString());
    }

    return true;
}

bool USuspenseCoreLoadoutManager::HasLoadoutsConfigured() const
{
    FScopeLock Lock(&CacheCriticalSection);
    return CachedConfigurations.Num() > 0;
}

void USuspenseCoreLoadoutManager::BroadcastLoadoutChange(const FName& LoadoutID, APlayerState* PlayerState, bool bSuccess) const
{
    const_cast<USuspenseCoreLoadoutManager*>(this)->OnLoadoutChanged.Broadcast(LoadoutID, PlayerState, bSuccess);
}

int32 USuspenseCoreLoadoutManager::CacheConfigurationsFromTable()
{
    if (!LoadedDataTable) return 0;

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
    }
    return LoadedCount;
}

bool USuspenseCoreLoadoutManager::ValidateConfiguration(const FLoadoutConfiguration& Config, TArray<FString>& OutErrors) const
{
    bool bIsValid = true;

    if (!Config.IsValid())
    {
        OutErrors.Add(TEXT("Configuration failed basic validation"));
        bIsValid = false;
    }

    float TotalInventoryWeight = Config.GetTotalInventoryWeight();
    if (TotalInventoryWeight > Config.MaxTotalWeight)
    {
        OutErrors.Add(FString::Printf(TEXT("Total weight (%.1f) exceeds max (%.1f)"), TotalInventoryWeight, Config.MaxTotalWeight));
        bIsValid = false;
    }

    TSet<EEquipmentSlotType> UniqueSlots;
    for (const FEquipmentSlotConfig& SlotConfig : Config.EquipmentSlots)
    {
        if (UniqueSlots.Contains(SlotConfig.SlotType))
        {
            OutErrors.Add(FString::Printf(TEXT("Duplicate slot: %s"), *UEnum::GetValueAsString(SlotConfig.SlotType)));
            bIsValid = false;
        }
        UniqueSlots.Add(SlotConfig.SlotType);
    }

    return bIsValid;
}

void USuspenseCoreLoadoutManager::ClearCache()
{
    CachedConfigurations.Empty();
}

void USuspenseCoreLoadoutManager::TryLoadDefaultTable()
{
    if (DefaultLoadoutTablePath.IsEmpty()) return;

    UDataTable* DefaultTable = LoadObject<UDataTable>(nullptr, *DefaultLoadoutTablePath);
    if (DefaultTable)
    {
        LoadLoadoutTable(DefaultTable);
    }
}

void USuspenseCoreLoadoutManager::LogLoadoutStatistics() const
{
    FScopeLock Lock(&CacheCriticalSection);
    UE_LOG(LogSuspenseCoreLoadout, Verbose, TEXT("Total Loadouts: %d"), CachedConfigurations.Num());
}

USuspenseCoreEventManager* USuspenseCoreLoadoutManager::GetEventManager() const
{
    if (const UGameInstance* GameInstance = GetGameInstance())
    {
        return GameInstance->GetSubsystem<USuspenseCoreEventManager>();
    }
    return nullptr;
}
