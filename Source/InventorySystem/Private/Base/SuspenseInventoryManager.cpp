// Copyright Suspense Team. All Rights Reserved.

#include "Base/SuspenseInventoryManager.h"
#include "ItemSystem/SuspenseItemManager.h"
#include "Components/SuspenseInventoryComponent.h"
#include "Interfaces/Equipment/ISuspenseEquipmentService.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Engine/Engine.h"

// Подключаем InventoryUtils для создания экземпляров предметов
namespace InventoryUtils
{
    extern FSuspenseInventoryItemInstance CreateItemInstance(const FName& ItemID, int32 Quantity);
    extern bool GetUnifiedItemData(const FName& ItemID, FSuspenseUnifiedItemData& OutData);
}

//==================================================================
// Subsystem lifecycle implementation
//==================================================================

USuspenseInventoryManager::USuspenseInventoryManager()
{
    // Initialize empty cache
    LoadoutCache.Empty();
    LoadoutCacheHits = 0;
    LoadoutCacheMisses = 0;
}

void USuspenseInventoryManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UE_LOG(LogTemp, Log, TEXT("USuspenseInventoryManager: Initializing inventory management subsystem"));

    // Initialize default loadout configuration
    InitializeDefaultLoadout();

    // Load default loadout table
    LoadDefaultLoadoutTable();

    UE_LOG(LogTemp, Log, TEXT("USuspenseInventoryManager: Initialization complete with %d cached loadouts"), LoadoutCache.Num());
}

void USuspenseInventoryManager::Deinitialize()
{
    UE_LOG(LogTemp, Log, TEXT("USuspenseInventoryManager: Shutting down inventory management subsystem"));

    // Clear cache and references
    LoadoutCache.Empty();
    LoadoutTable = nullptr;
    LoadoutCacheHits = 0;
    LoadoutCacheMisses = 0;

    Super::Deinitialize();
}

//==================================================================
// Loadout configuration management implementation
//==================================================================

bool USuspenseInventoryManager::LoadLoadoutDataTable(UDataTable* DataTable)
{
    if (!DataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("USuspenseInventoryManager::LoadLoadoutDataTable: DataTable is null"));
        return false;
    }

    // Verify row structure matches our loadout configuration format
    const UScriptStruct* RowStruct = DataTable->GetRowStruct();
    if (RowStruct != FLoadoutConfiguration::StaticStruct())
    {
        UE_LOG(LogTemp, Error, TEXT("USuspenseInventoryManager::LoadLoadoutDataTable: Invalid row structure"));
        UE_LOG(LogTemp, Error, TEXT("  Expected: %s"), *FLoadoutConfiguration::StaticStruct()->GetName());
        UE_LOG(LogTemp, Error, TEXT("  Got: %s"), RowStruct ? *RowStruct->GetName() : TEXT("nullptr"));
        UE_LOG(LogTemp, Error, TEXT("  Please ensure your DataTable uses FLoadoutConfiguration row structure"));
        return false;
    }

    // Save table reference
    LoadoutTable = DataTable;

    // Build cache from table data
    BuildLoadoutCache();

    UE_LOG(LogTemp, Warning, TEXT("USuspenseInventoryManager: Successfully loaded loadout table"));
    UE_LOG(LogTemp, Warning, TEXT("  DataTable Asset: %s"), *DataTable->GetName());
    UE_LOG(LogTemp, Warning, TEXT("  Cached Loadouts: %d"), LoadoutCache.Num());

    return true;
}

bool USuspenseInventoryManager::GetLoadoutConfiguration(const FName& LoadoutID, FLoadoutConfiguration& OutConfiguration) const
{
    const FLoadoutConfiguration* FoundLoadout = GetCachedLoadoutData(LoadoutID);
    if (FoundLoadout)
    {
        OutConfiguration = *FoundLoadout;
        return true;
    }

    UE_LOG(LogTemp, Warning, TEXT("USuspenseInventoryManager::GetLoadoutConfiguration: Loadout '%s' not found"),
        *LoadoutID.ToString());
    return false;
}

FLoadoutConfiguration USuspenseInventoryManager::GetDefaultLoadoutConfiguration() const
{
    return DefaultLoadout;
}

TArray<FName> USuspenseInventoryManager::GetCompatibleLoadouts(const FGameplayTag& CharacterClass) const
{
    TArray<FName> CompatibleLoadouts;

    for (const auto& LoadoutPair : LoadoutCache)
    {
        if (LoadoutPair.Value.IsCompatibleWithClass(CharacterClass))
        {
            CompatibleLoadouts.Add(LoadoutPair.Key);
        }
    }

    UE_LOG(LogTemp, VeryVerbose, TEXT("USuspenseInventoryManager::GetCompatibleLoadouts: Found %d loadouts for class '%s'"),
        CompatibleLoadouts.Num(), *CharacterClass.ToString());

    return CompatibleLoadouts;
}

TArray<FName> USuspenseInventoryManager::GetAllLoadoutIDs() const
{
    TArray<FName> Result;
    LoadoutCache.GetKeys(Result);
    return Result;
}

//==================================================================
// Inventory initialization from loadout implementation
//==================================================================

int32 USuspenseInventoryManager::InitializeInventoryFromLoadout(USuspenseInventoryComponent* InventoryComponent,
                                                       const FName& LoadoutID,
                                                       const FName& InventoryName)
{
    if (!InventoryComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("USuspenseInventoryManager::InitializeInventoryFromLoadout: Inventory component is null"));
        return 0;
    }

    // Get loadout configuration
    FLoadoutConfiguration LoadoutConfig;
    if (!GetLoadoutConfiguration(LoadoutID, LoadoutConfig))
    {
        UE_LOG(LogTemp, Warning, TEXT("USuspenseInventoryManager::InitializeInventoryFromLoadout: Using default loadout due to missing loadout: %s"),
            *LoadoutID.ToString());
        LoadoutConfig = GetDefaultLoadoutConfiguration();
    }

    // Get inventory configuration from loadout
    const FSuspenseInventoryConfig* InventoryConfig = LoadoutConfig.GetInventoryConfig(InventoryName);
    if (!InventoryConfig)
    {
        UE_LOG(LogTemp, Error, TEXT("USuspenseInventoryManager::InitializeInventoryFromLoadout: Inventory '%s' not found in loadout '%s'"),
            *InventoryName.ToString(), *LoadoutID.ToString());
        return 0;
    }

    // Initialize inventory component with configuration
    // NOTE: This assumes the inventory component has a method to set its configuration
    // You'll need to implement this method in USuspenseInventoryComponent

    UE_LOG(LogTemp, Log, TEXT("USuspenseInventoryManager: Initializing inventory '%s' with grid %dx%d, max weight %.1f"),
        *InventoryName.ToString(), InventoryConfig->Width, InventoryConfig->Height, InventoryConfig->MaxWeight);

    // Create starting items
    return CreateStartingItemsFromLoadout(InventoryComponent, LoadoutConfig, InventoryName);
}

int32 USuspenseInventoryManager::CreateStartingItemsFromLoadout(USuspenseInventoryComponent* InventoryComponent,
                                                       const FLoadoutConfiguration& LoadoutConfiguration,
                                                       const FName& InventoryName)
{
    if (!InventoryComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("USuspenseInventoryManager::CreateStartingItemsFromLoadout: Inventory component is null"));
        return 0;
    }

    // Get inventory configuration
    const FSuspenseInventoryConfig* InventoryConfig = LoadoutConfiguration.GetInventoryConfig(InventoryName);
    if (!InventoryConfig)
    {
        UE_LOG(LogTemp, Warning, TEXT("USuspenseInventoryManager::CreateStartingItemsFromLoadout: Inventory '%s' not found in loadout"),
            *InventoryName.ToString());
        return 0;
    }

    int32 SuccessCount = 0;
    USuspenseItemManager* ItemManager = GetItemManager();
    
    if (!ItemManager)
    {
        UE_LOG(LogTemp, Error, TEXT("USuspenseInventoryManager::CreateStartingItemsFromLoadout: ItemManager not available"));
        return 0;
    }

    // Process each starting item
    for (const FSuspensePickupSpawnData& SpawnData : InventoryConfig->StartingItems)
    {
        if (!SpawnData.IsValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("USuspenseInventoryManager::CreateStartingItemsFromLoadout: Invalid spawn data for item: %s"),
                *SpawnData.ItemID.ToString());
            continue;
        }

        // Create item instance using ItemManager
        FSuspenseInventoryItemInstance NewInstance;
        if (ItemManager->CreateItemInstance(SpawnData.ItemID, SpawnData.Quantity, NewInstance))
        {
            // Apply preset runtime properties if any
            for (const auto& PropertyPair : SpawnData.PresetRuntimeProperties)
            {
                NewInstance.SetRuntimeProperty(PropertyPair.Key, PropertyPair.Value);
            }

            // Try to add item to inventory
            // NOTE: This assumes inventory component has AddItemInstance method
            // You'll need to implement this method in USuspenseInventoryComponent
            /*
            if (InventoryComponent->AddItemInstance(NewInstance))
            {
                SuccessCount++;
                UE_LOG(LogTemp, Log, TEXT("USuspenseInventoryManager: Added starting item: %s"),
                    *NewInstance.GetShortDebugString());
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("USuspenseInventoryManager: Failed to add starting item to inventory: %s"),
                    *SpawnData.ItemID.ToString());
            }
            */

            // Temporary success count increment until inventory integration is complete
            SuccessCount++;
            UE_LOG(LogTemp, Log, TEXT("USuspenseInventoryManager: Created starting item: %s"),
                *NewInstance.GetShortDebugString());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("USuspenseInventoryManager: Failed to create starting item: %s"),
                *SpawnData.ItemID.ToString());
        }
    }

    UE_LOG(LogTemp, Log, TEXT("USuspenseInventoryManager: Successfully created %d/%d starting items for inventory '%s'"),
        SuccessCount, InventoryConfig->StartingItems.Num(), *InventoryName.ToString());

    return SuccessCount;
}

int32 USuspenseInventoryManager::InitializeEquipmentFromLoadout(UObject* EquipmentTarget,
                                                       const FLoadoutConfiguration& LoadoutConfiguration)
{
    if (!EquipmentTarget)
    {
        UE_LOG(LogTemp, Error, TEXT("USuspenseInventoryManager::InitializeEquipmentFromLoadout: EquipmentTarget равен null"));
        return 0;
    }

    // Проверяем, что объект реализует интерфейс экипировки
    if (!EquipmentTarget->GetClass()->ImplementsInterface(USuspenseEquipment::StaticClass()))
    {
        UE_LOG(LogTemp, Error, TEXT("USuspenseInventoryManager::InitializeEquipmentFromLoadout: Объект не реализует IMedComEquipmentInterface"));
        UE_LOG(LogTemp, Error, TEXT("  Класс объекта: %s"), *EquipmentTarget->GetClass()->GetName());
        return 0;
    }

    int32 SuccessCount = 0;
    USuspenseItemManager* ItemManager = GetItemManager();
    
    if (!ItemManager)
    {
        UE_LOG(LogTemp, Error, TEXT("USuspenseInventoryManager::InitializeEquipmentFromLoadout: ItemManager недоступен"));
        return 0;
    }

    // Обрабатываем каждый элемент начальной экипировки из конфигурации loadout
    for (const auto& EquipmentPair : LoadoutConfiguration.StartingEquipment)
    {
        EEquipmentSlotType SlotType = EquipmentPair.Key;
        FName ItemID = EquipmentPair.Value;

        if (ItemID.IsNone())
        {
            continue; // Пустой слот - это нормально
        }

        // Создаём экземпляр предмета для экипировки
        FSuspenseInventoryItemInstance EquipmentInstance;
        if (!ItemManager->CreateItemInstance(ItemID, 1, EquipmentInstance))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("USuspenseInventoryManager::InitializeEquipmentFromLoadout: Не удалось создать предмет: %s"),
                *ItemID.ToString());
            continue;
        }

        // Сначала проверяем, можем ли мы экипировать предмет
        bool bCanEquip = ISuspenseEquipment::Execute_CanEquipItemInstance(
            EquipmentTarget,
            EquipmentInstance
        );

        if (!bCanEquip)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("USuspenseInventoryManager::InitializeEquipmentFromLoadout: Предмет %s не может быть экипирован"),
                *ItemID.ToString());
            continue;
        }

        // Пытаемся экипировать предмет через интерфейс
        FSuspenseInventoryOperationResult EquipResult = ISuspenseEquipment::Execute_EquipItemInstance(
            EquipmentTarget,
            EquipmentInstance,
            false // bForceEquip = false
        );

        // Используем правильный метод проверки успешности
        if (EquipResult.IsSuccess())
        {
            SuccessCount++;
            UE_LOG(LogTemp, Log,
                TEXT("USuspenseInventoryManager::InitializeEquipmentFromLoadout: Успешно экипирован %s в слот %d"),
                *ItemID.ToString(),
                static_cast<int32>(SlotType));
        }
        else
        {
            // Детальное логирование ошибки
            UE_LOG(LogTemp, Warning,
                TEXT("USuspenseInventoryManager::InitializeEquipmentFromLoadout: Не удалось экипировать %s в слот %d"),
                *ItemID.ToString(),
                static_cast<int32>(SlotType));

            // Используем GetDetailedString() для полной информации об ошибке
            UE_LOG(LogTemp, Warning, TEXT("  Детали ошибки: %s"),
                *EquipResult.GetDetailedString());

            // Проверяем конкретные типы ошибок из существующего enum
            switch (EquipResult.ErrorCode)
            {
                case ESuspenseInventoryErrorCode::InvalidItem:
                    UE_LOG(LogTemp, Warning, TEXT("  Причина: Недопустимый предмет для этого слота"));
                    break;

                case ESuspenseInventoryErrorCode::SlotOccupied:
                    UE_LOG(LogTemp, Warning, TEXT("  Причина: Слот уже занят другим предметом"));
                    break;

                case ESuspenseInventoryErrorCode::InvalidSlot:
                    UE_LOG(LogTemp, Warning, TEXT("  Причина: Недопустимый слот для экипировки"));
                    break;

                case ESuspenseInventoryErrorCode::NotInitialized:
                    UE_LOG(LogTemp, Warning, TEXT("  Причина: Система экипировки не инициализирована"));
                    break;

                default:
                    UE_LOG(LogTemp, Warning, TEXT("  Причина: %s"),
                        *FSuspenseInventoryOperationResult::GetErrorCodeString(EquipResult.ErrorCode));
                    break;
            }
        }
    }

    // Если хотя бы один предмет был экипирован, применяем эффекты
    if (SuccessCount > 0)
    {
        // Вызываем применение эффектов экипировки
        ISuspenseEquipment::Execute_ApplyEquipmentEffects(EquipmentTarget);

        UE_LOG(LogTemp, Log,
            TEXT("USuspenseInventoryManager::InitializeEquipmentFromLoadout: Применены эффекты экипировки"));
    }

    UE_LOG(LogTemp, Log,
        TEXT("USuspenseInventoryManager::InitializeEquipmentFromLoadout: Итого инициализировано %d/%d элементов экипировки"),
        SuccessCount,
        LoadoutConfiguration.StartingEquipment.Num());

    return SuccessCount;
}
//==================================================================
// Item instance creation (delegates to ItemManager)
//==================================================================

bool USuspenseInventoryManager::CreateItemInstance(const FName& ItemID, int32 Quantity, FSuspenseInventoryItemInstance& OutInstance) const
{
    USuspenseItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        UE_LOG(LogTemp, Error, TEXT("USuspenseInventoryManager::CreateItemInstance: ItemManager not available"));
        return false;
    }

    // Delegate to ItemManager
    return ItemManager->CreateItemInstance(ItemID, Quantity, OutInstance);
}

int32 USuspenseInventoryManager::CreateItemInstancesFromSpawnData(const TArray<FSuspensePickupSpawnData>& SpawnDataArray, 
                                                         TArray<FInventoryItemInstance>& OutInstances) const
{
    USuspenseItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        UE_LOG(LogTemp, Error, TEXT("USuspenseInventoryManager::CreateItemInstancesFromSpawnData: ItemManager not available"));
        return 0;
    }

    // Delegate to ItemManager
    return ItemManager->CreateItemInstancesFromSpawnData(SpawnDataArray, OutInstances);
}

//==================================================================
// Validation and utilities implementation
//==================================================================

bool USuspenseInventoryManager::ValidateLoadoutConfiguration(const FName& LoadoutID, TArray<FString>& OutErrors) const
{
    OutErrors.Empty();

    const FLoadoutConfiguration* LoadoutConfig = GetCachedLoadoutData(LoadoutID);
    if (!LoadoutConfig)
    {
        OutErrors.Add(TEXT("Loadout not found in cache"));
        return false;
    }

    return ValidateLoadoutInternal(LoadoutID, *LoadoutConfig, OutErrors);
}

bool USuspenseInventoryManager::IsLoadoutCompatibleWithClass(const FName& LoadoutID, const FGameplayTag& CharacterClass) const
{
    const FLoadoutConfiguration* LoadoutConfig = GetCachedLoadoutData(LoadoutID);
    if (!LoadoutConfig)
    {
        return false;
    }

    return LoadoutConfig->IsCompatibleWithClass(CharacterClass);
}

const FSuspenseInventoryConfig* USuspenseInventoryManager::GetInventoryConfigFromLoadout(const FName& LoadoutID, const FName& InventoryName) const
{
    const FLoadoutConfiguration* LoadoutConfig = GetCachedLoadoutData(LoadoutID);
    if (!LoadoutConfig)
    {
        return nullptr;
    }

    return LoadoutConfig->GetInventoryConfig(InventoryName);
}

//==================================================================
// Debug and statistics implementation
//==================================================================

void USuspenseInventoryManager::GetLoadoutCacheStatistics(FString& OutStats) const
{
    FString Stats = FString::Printf(
        TEXT("InventoryManager Cache Statistics:\n")
        TEXT("  Total Loadouts: %d\n")
        TEXT("  Cache Hits: %d\n")
        TEXT("  Cache Misses: %d\n")
        TEXT("  Hit Rate: %.2f%%\n")
        TEXT("  DataTable: %s"),
        LoadoutCache.Num(),
        LoadoutCacheHits,
        LoadoutCacheMisses,
        LoadoutCacheHits + LoadoutCacheMisses > 0 ? (float(LoadoutCacheHits) / float(LoadoutCacheHits + LoadoutCacheMisses)) * 100.0f : 0.0f,
        LoadoutTable ? *LoadoutTable->GetName() : TEXT("None")
    );

    OutStats = Stats;
}

bool USuspenseInventoryManager::RefreshLoadoutCache()
{
    if (!LoadoutTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("USuspenseInventoryManager::RefreshLoadoutCache: No DataTable loaded"));
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("USuspenseInventoryManager: Refreshing loadout cache"));

    // Clear existing cache
    LoadoutCache.Empty();
    LoadoutCacheHits = 0;
    LoadoutCacheMisses = 0;

    // Rebuild from current table
    BuildLoadoutCache();

    return true;
}

void USuspenseInventoryManager::LogLoadoutDetails(const FName& LoadoutID) const
{
    const FLoadoutConfiguration* LoadoutConfig = GetCachedLoadoutData(LoadoutID);
    if (!LoadoutConfig)
    {
        UE_LOG(LogTemp, Warning, TEXT("USuspenseInventoryManager::LogLoadoutDetails: Loadout '%s' not found"), *LoadoutID.ToString());
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("====== Loadout Details: %s ======"), *LoadoutID.ToString());
    UE_LOG(LogTemp, Warning, TEXT("  Name: %s"), *LoadoutConfig->LoadoutName.ToString());
    UE_LOG(LogTemp, Warning, TEXT("  Main Inventory: %dx%d (%.1f kg max)"),
        LoadoutConfig->MainInventory.Width, LoadoutConfig->MainInventory.Height, LoadoutConfig->MainInventory.MaxWeight);
    UE_LOG(LogTemp, Warning, TEXT("  Additional Inventories: %d"), LoadoutConfig->AdditionalInventories.Num());
    UE_LOG(LogTemp, Warning, TEXT("  Equipment Slots: %d"), LoadoutConfig->EquipmentSlots.Num());
    UE_LOG(LogTemp, Warning, TEXT("  Starting Equipment: %d pieces"), LoadoutConfig->StartingEquipment.Num());
    UE_LOG(LogTemp, Warning, TEXT("  Max Total Weight: %.1f kg"), LoadoutConfig->MaxTotalWeight);
    UE_LOG(LogTemp, Warning, TEXT("====================================="));
}

//==================================================================
// Internal helper methods implementation
//==================================================================

void USuspenseInventoryManager::LoadDefaultLoadoutTable()
{
    UE_LOG(LogTemp, Log, TEXT("USuspenseInventoryManager: Loading default loadout table from: %s"),
        *DefaultLoadoutTablePath.ToString());

    TSoftObjectPtr<UDataTable> DefaultTablePtr(DefaultLoadoutTablePath);
    UDataTable* DefaultTable = DefaultTablePtr.LoadSynchronous();

    if (DefaultTable)
    {
        LoadLoadoutDataTable(DefaultTable);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("USuspenseInventoryManager: Failed to load default loadout table, using built-in default"));
        UE_LOG(LogTemp, Warning, TEXT("  Path: %s"), *DefaultLoadoutTablePath.ToString());
    }
}

void USuspenseInventoryManager::BuildLoadoutCache()
{
    LoadoutCache.Empty();

    if (!LoadoutTable)
    {
        UE_LOG(LogTemp, Error, TEXT("USuspenseInventoryManager::BuildLoadoutCache: LoadoutTable is null"));
        return;
    }

    TArray<FName> RowNames = LoadoutTable->GetRowNames();

    UE_LOG(LogTemp, Log, TEXT("USuspenseInventoryManager::BuildLoadoutCache: Building cache from %d rows"), RowNames.Num());

    int32 ValidLoadouts = 0;

    for (const FName& RowName : RowNames)
    {
        FLoadoutConfiguration* LoadoutData = LoadoutTable->FindRow<FLoadoutConfiguration>(RowName, TEXT("InventoryManager::BuildLoadoutCache"));
        if (LoadoutData)
        {
            // Use row name as LoadoutID if not set
            if (LoadoutData->LoadoutID.IsNone())
            {
                LoadoutData->LoadoutID = RowName;
                UE_LOG(LogTemp, Warning, TEXT("USuspenseInventoryManager: Row '%s' has empty LoadoutID, using row name"),
                    *RowName.ToString());
            }

            // Validate loadout data
            TArray<FString> ValidationErrors;
            if (ValidateLoadoutInternal(LoadoutData->LoadoutID, *LoadoutData, ValidationErrors))
            {
                ValidLoadouts++;
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("USuspenseInventoryManager: Loadout '%s' has validation errors:"),
                    *LoadoutData->LoadoutID.ToString());
                for (const FString& Error : ValidationErrors)
                {
                    UE_LOG(LogTemp, Warning, TEXT("  - %s"), *Error);
                }
            }

            // Add to cache regardless of validation (allows for debugging invalid loadouts)
            LoadoutCache.Add(LoadoutData->LoadoutID, *LoadoutData);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("USuspenseInventoryManager: Failed to get data for row '%s'"),
                *RowName.ToString());
        }
    }

    // Log detailed statistics
    LogLoadoutCacheStatistics(LoadoutCache.Num(), ValidLoadouts);
}

void USuspenseInventoryManager::InitializeDefaultLoadout()
{
    DefaultLoadout = FLoadoutConfiguration();
    DefaultLoadout.LoadoutID = TEXT("Default");
    DefaultLoadout.LoadoutName = FText::FromString(TEXT("Default Loadout"));
    DefaultLoadout.Description = FText::FromString(TEXT("Standard loadout configuration"));

    // Configure main inventory
    DefaultLoadout.MainInventory = FSuspenseInventoryConfig(
        FText::FromString(TEXT("Main Inventory")),
        10, 5, 100.0f
    );

    // Add a backpack inventory
    FSuspenseInventoryConfig BackpackInventory(
        FText::FromString(TEXT("Backpack")),
        8, 6, 50.0f
    );
    DefaultLoadout.AddAdditionalInventory(TEXT("Backpack"), BackpackInventory);

    UE_LOG(LogTemp, Log, TEXT("USuspenseInventoryManager: Initialized default loadout configuration"));
}

USuspenseItemManager* USuspenseInventoryManager::GetItemManager() const
{
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        return GameInstance->GetSubsystem<USuspenseItemManager>();
    }

    return nullptr;
}

const FLoadoutConfiguration* USuspenseInventoryManager::GetCachedLoadoutData(const FName& LoadoutID) const
{
    const FLoadoutConfiguration* FoundLoadout = LoadoutCache.Find(LoadoutID);

    // Track cache statistics for performance monitoring
    if (FoundLoadout)
    {
        LoadoutCacheHits++;
    }
    else
    {
        LoadoutCacheMisses++;
    }

    return FoundLoadout;
}

bool USuspenseInventoryManager::ValidateLoadoutInternal(const FName& LoadoutID, const FLoadoutConfiguration& Configuration, TArray<FString>& OutErrors) const
{
    OutErrors.Empty();

    // Use built-in validation from loadout configuration
    if (!Configuration.IsValid())
    {
        OutErrors.Add(TEXT("Loadout configuration failed basic validation"));
        return false;
    }

    // Additional validation specific to our use case
    if (Configuration.MainInventory.GetTotalCells() == 0)
    {
        OutErrors.Add(TEXT("Main inventory has zero cells"));
    }

    if (Configuration.MaxTotalWeight <= 0.0f)
    {
        OutErrors.Add(TEXT("Max total weight must be greater than zero"));
    }

    return OutErrors.Num() == 0;
}

void USuspenseInventoryManager::LogLoadoutCacheStatistics(int32 TotalLoadouts, int32 ValidLoadouts) const
{
    UE_LOG(LogTemp, Warning, TEXT("====== USuspenseInventoryManager: Loadout Cache Built ======"));
    UE_LOG(LogTemp, Warning, TEXT("  Total Loadouts: %d"), TotalLoadouts);
    UE_LOG(LogTemp, Warning, TEXT("  Valid Loadouts: %d"), ValidLoadouts);
    UE_LOG(LogTemp, Warning, TEXT("  Invalid Loadouts: %d"), TotalLoadouts - ValidLoadouts);
    UE_LOG(LogTemp, Warning, TEXT("==================================================="));
}

bool USuspenseInventoryManager::GetInventoryConfigFromLoadout_BP(const FName& LoadoutID, const FName& InventoryName, FSuspenseInventoryConfig& OutInventoryConfig) const
{
    // Используем существующий C++ метод для получения указателя
    const FSuspenseInventoryConfig* ConfigPtr = GetInventoryConfigFromLoadout(LoadoutID, InventoryName);

    if (ConfigPtr)
    {
        // Копируем структуру для Blueprint
        OutInventoryConfig = *ConfigPtr;
        return true;
    }

    // Возвращаем пустую конфигурацию при неудаче
    OutInventoryConfig = FSuspenseInventoryConfig();
    return false;
}
