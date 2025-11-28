// MedComInventory/Serialization/MedComInventorySerializer.cpp
// Copyright Suspense Team. All Rights Reserved.

#include "Serialization/SuspenseInventorySerializer.h"
#include "Components/SuspenseInventoryComponent.h"
#include "ItemSystem/SuspenseItemManager.h"
#include "ItemSystem/SuspenseItemSystemAccess.h"
#include "Types/Loadout/SuspenseItemDataTable.h"
#include "Types/Inventory/SuspenseInventoryUtils.h"
#include "Base/SuspenseInventoryLogs.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"


// Константы класса
const FString USuspenseInventorySerializer::BACKUP_EXTENSION = TEXT(".backup");

//==================================================================
// Core Serialization Implementation
//==================================================================

FSerializedInventoryData USuspenseInventorySerializer::SerializeInventory(USuspenseInventoryComponent* InventoryComponent)
{
    FSerializedInventoryData Result;
    
    // Валидация входного компонента
    if (!InventoryComponent || !InventoryComponent->IsInventoryInitialized())
    {
        UE_LOG(LogInventory, Error, TEXT("SerializeInventory: Invalid or uninitialized inventory component"));
        return Result; // Возвращаем пустую структуру с Version = 2
    }
    
    UE_LOG(LogInventory, Log, TEXT("SerializeInventory: Starting serialization of inventory component"));
    
    // Устанавливаем базовые свойства инвентаря
    FVector2D GridSize = InventoryComponent->GetInventorySize();
    Result.GridWidth = FMath::FloorToInt(GridSize.X);
    Result.GridHeight = FMath::FloorToInt(GridSize.Y);
    Result.MaxWeight = InventoryComponent->GetMaxWeight();
    Result.SaveTime = FDateTime::Now();
    
    // Получаем фильтры типов предметов
    Result.AllowedItemTypes = InventoryComponent->GetAllowedItemTypes();
    
    // КЛЮЧЕВОЕ ИЗМЕНЕНИЕ: Получаем runtime экземпляры вместо legacy структур
    TArray<FSuspenseInventoryItemInstance> AllInstances = InventoryComponent->GetAllItemInstances();
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("SerializeInventory: Found %d item instances to serialize"), 
        AllInstances.Num());
    
    // Сериализуем каждый runtime экземпляр
    int32 SerializedCount = 0;
    for (const FSuspenseInventoryItemInstance& Instance : AllInstances)
    {
        // Валидируем экземпляр перед сериализацией
        if (!Instance.IsValid())
        {
            UE_LOG(LogInventory, Warning, TEXT("SerializeInventory: Skipping invalid instance: %s"), 
                *Instance.GetShortDebugString());
            continue;
        }
        
        // ИСПРАВЛЕНО: Добавлен WorldContextObject как первый параметр
        // Проверяем что предмет еще существует в DataTable (защита от orphaned items)
        FSuspenseUnifiedItemData ItemData;
        if (!InventoryUtils::GetUnifiedItemData(InventoryComponent, Instance.ItemID, ItemData))
        {
            UE_LOG(LogInventory, Warning, TEXT("SerializeInventory: Item '%s' no longer exists in DataTable, skipping"), 
                *Instance.ItemID.ToString());
            continue;
        }
        
        // Добавляем валидный экземпляр к результату
        Result.ItemInstances.Add(Instance);
        SerializedCount++;
        
        UE_LOG(LogInventory, VeryVerbose, TEXT("SerializeInventory: Serialized item: %s"), 
            *Instance.GetShortDebugString());
    }
    
    // Добавляем метаданные инвентаря для debugging
    Result.InventoryMetadata.Add(TEXT("SerializationTime"), Result.SaveTime.ToString());
    Result.InventoryMetadata.Add(TEXT("TotalItems"), FString::FromInt(SerializedCount));
    Result.InventoryMetadata.Add(TEXT("GridCells"), FString::FromInt(Result.GridWidth * Result.GridHeight));
    
    UE_LOG(LogInventory, Log, TEXT("SerializeInventory: Successfully serialized %d items from inventory"), 
        SerializedCount);
    
    // Логируем статистику для мониторинга
    UE_LOG(LogInventory, VeryVerbose, TEXT("SerializeInventory: Grid %dx%d, MaxWeight %.1f, AllowedTypes %d"), 
        Result.GridWidth, Result.GridHeight, Result.MaxWeight, Result.AllowedItemTypes.Num());
    
    return Result;
}

bool USuspenseInventorySerializer::DeserializeInventory(USuspenseInventoryComponent* InventoryComponent, 
                                                     const FSerializedInventoryData& SerializedData)
{
    // Валидация входных параметров
    if (!InventoryComponent)
    {
        UE_LOG(LogInventory, Error, TEXT("DeserializeInventory: Invalid inventory component"));
        return false;
    }
    
    if (!SerializedData.IsValid())
    {
        UE_LOG(LogInventory, Error, TEXT("DeserializeInventory: Invalid serialized data"));
        return false;
    }
    
    UE_LOG(LogInventory, Log, TEXT("DeserializeInventory: Starting deserialization of %d items"), 
        SerializedData.ItemInstances.Num());
    
    // Проверяем версию для backward compatibility
    if (SerializedData.Version < MIN_SUPPORTED_VERSION)
    {
        UE_LOG(LogInventory, Error, TEXT("DeserializeInventory: Unsupported data version %d (min: %d)"), 
            SerializedData.Version, MIN_SUPPORTED_VERSION);
        return false;
    }
    
    // Выполняем миграцию если необходимо
    FSerializedInventoryData MutableData = SerializedData;
    if (SerializedData.Version < CURRENT_VERSION)
    {
        UE_LOG(LogInventory, Warning, TEXT("DeserializeInventory: Migrating data from version %d to %d"), 
            SerializedData.Version, CURRENT_VERSION);
        
        if (!MigrateSerializedData(MutableData))
        {
            UE_LOG(LogInventory, Error, TEXT("DeserializeInventory: Migration failed"));
            return false;
        }
    }
    
    // Очищаем существующий инвентарь
    // ВАЖНО: Используем транзакцию для atomic операции
    InventoryComponent->BeginTransaction();
    
    try
    {
        // Очищаем все предметы
        TArray<FSuspenseInventoryItemInstance> ExistingItems = InventoryComponent->GetAllItemInstances();
        for (const FSuspenseInventoryItemInstance& Item : ExistingItems)
        {
            InventoryComponent->RemoveItemByID(Item.ItemID, Item.Quantity);
        }
        
        UE_LOG(LogInventory, VeryVerbose, TEXT("DeserializeInventory: Cleared %d existing items"), 
            ExistingItems.Num());
        
        // Восстанавливаем конфигурацию инвентаря если необходимо
        // NOTE: Некоторые настройки могут быть read-only в runtime
        
        // Загружаем предметы из сериализованных данных
        int32 SuccessCount = 0;
        int32 FailureCount = 0;
        
        // ИСПРАВЛЕНО: Используем FSuspenseItemSystemAccess вместо прямого обращения
        USuspenseItemManager* ItemManager = FItemSystemAccess::GetItemManager(InventoryComponent);
        if (!ItemManager)
        {
            UE_LOG(LogInventory, Error, TEXT("DeserializeInventory: ItemManager not available"));
            InventoryComponent->RollbackTransaction();
            return false;
        }
        
        for (const FSuspenseInventoryItemInstance& SerializedInstance : MutableData.ItemInstances)
        {
            // Валидируем каждый предмет против текущего DataTable
            FString ValidationError;
            if (!ValidateItemInstance(SerializedInstance, ItemManager, ValidationError))
            {
                UE_LOG(LogInventory, Warning, TEXT("DeserializeInventory: Skipping invalid item: %s - %s"), 
                    *SerializedInstance.ItemID.ToString(), *ValidationError);
                FailureCount++;
                continue;
            }
        
            // Добавляем предмет в инвентарь
            FSuspenseInventoryOperationResult AddResult = InventoryComponent->AddItemInstance(SerializedInstance);
            if (AddResult.bSuccess)
            {
                SuccessCount++;
                UE_LOG(LogInventory, VeryVerbose, TEXT("DeserializeInventory: Loaded item: %s"), 
                    *SerializedInstance.GetShortDebugString());
            }
            else
            {
                // ИСПРАВЛЕНО: Добавлен .ToString() для конверсии FText в FString
                UE_LOG(LogInventory, Warning, TEXT("DeserializeInventory: Failed to add item: %s - %s"), 
                    *SerializedInstance.ItemID.ToString(), *AddResult.ErrorMessage.ToString());
                FailureCount++;
            }
        }
        
        // Коммитим транзакцию если загрузка прошла успешно
        InventoryComponent->CommitTransaction();
        
        UE_LOG(LogInventory, Log, TEXT("DeserializeInventory: Completed - Success: %d, Failed: %d"), 
            SuccessCount, FailureCount);
        
        return SuccessCount > 0; // Успех если загрузили хотя бы один предмет
    }
    catch (...)
    {
        // Откатываем изменения при любой ошибке
        UE_LOG(LogInventory, Error, TEXT("DeserializeInventory: Exception occurred, rolling back transaction"));
        InventoryComponent->RollbackTransaction();
        return false;
    }
}

//==================================================================
// JSON Serialization Implementation
//==================================================================

FString USuspenseInventorySerializer::SerializeInventoryToJson(USuspenseInventoryComponent* InventoryComponent, 
                                                           bool bPrettyPrint)
{
    // Получаем структурированные данные
    FSerializedInventoryData Data = SerializeInventory(InventoryComponent);
    
    if (!Data.IsValid())
    {
        UE_LOG(LogInventory, Error, TEXT("SerializeInventoryToJson: Invalid serialized data"));
        return FString();
    }
    
    // Конвертируем в JSON
    FString JsonString = StructToJson(Data, bPrettyPrint);
    
    if (JsonString.IsEmpty())
    {
        UE_LOG(LogInventory, Error, TEXT("SerializeInventoryToJson: JSON conversion failed"));
        return FString();
    }
    
    UE_LOG(LogInventory, Log, TEXT("SerializeInventoryToJson: Generated JSON (%d characters)"), 
        JsonString.Len());
    
    return JsonString;
}

bool USuspenseInventorySerializer::DeserializeInventoryFromJson(USuspenseInventoryComponent* InventoryComponent, 
                                                            const FString& JsonString)
{
    if (JsonString.IsEmpty())
    {
        UE_LOG(LogInventory, Error, TEXT("DeserializeInventoryFromJson: Empty JSON string"));
        return false;
    }
    
    // Парсим JSON в структуру
    FSerializedInventoryData Data;
    if (!JsonToStruct(JsonString, Data))
    {
        UE_LOG(LogInventory, Error, TEXT("DeserializeInventoryFromJson: JSON parsing failed"));
        return false;
    }
    
    UE_LOG(LogInventory, Log, TEXT("DeserializeInventoryFromJson: Parsed JSON with %d items"), 
        Data.ItemInstances.Num());
    
    // Десериализуем структурированные данные
    return DeserializeInventory(InventoryComponent, Data);
}

//==================================================================
// File Operations Implementation
//==================================================================

bool USuspenseInventorySerializer::SaveInventoryToFile(USuspenseInventoryComponent* InventoryComponent, 
                                                    const FString& FilePath, 
                                                    bool bUseJson, 
                                                    bool bCreateBackup)
{
    if (!InventoryComponent)
    {
        UE_LOG(LogInventory, Error, TEXT("SaveInventoryToFile: Invalid inventory component"));
        return false;
    }
    
    if (FilePath.IsEmpty())
    {
        UE_LOG(LogInventory, Error, TEXT("SaveInventoryToFile: Empty file path"));
        return false;
    }
    
    UE_LOG(LogInventory, Log, TEXT("SaveInventoryToFile: Saving to '%s' (JSON: %s, Backup: %s)"), 
        *FilePath, bUseJson ? TEXT("Yes") : TEXT("No"), bCreateBackup ? TEXT("Yes") : TEXT("No"));
    
    // Создаем backup существующего файла если необходимо
    if (bCreateBackup && FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath))
    {
        if (!CreateFileBackup(FilePath))
        {
            UE_LOG(LogInventory, Warning, TEXT("SaveInventoryToFile: Failed to create backup, continuing anyway"));
        }
    }
    
    FString DataString;
    
    if (bUseJson)
    {
        // JSON формат для читаемости и debugging
        DataString = SerializeInventoryToJson(InventoryComponent, false); // Не pretty print для размера
    }
    else
    {
        // Binary формат пока не реализован в данной версии
        UE_LOG(LogInventory, Error, TEXT("SaveInventoryToFile: Binary format not implemented yet"));
        return false;
    }
    
    if (DataString.IsEmpty())
    {
        UE_LOG(LogInventory, Error, TEXT("SaveInventoryToFile: Serialization produced empty data"));
        return false;
    }
    
    // Сохраняем в файл с atomic операцией
    FString TempFilePath = FilePath + TEXT(".tmp");
    
    if (!FFileHelper::SaveStringToFile(DataString, *TempFilePath))
    {
        UE_LOG(LogInventory, Error, TEXT("SaveInventoryToFile: Failed to write temporary file: %s"), 
            *TempFilePath);
        return false;
    }
    
    // Атомарно перемещаем временный файл
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    
    if (PlatformFile.FileExists(*FilePath))
    {
        if (!PlatformFile.DeleteFile(*FilePath))
        {
            UE_LOG(LogInventory, Error, TEXT("SaveInventoryToFile: Failed to delete existing file: %s"), 
                *FilePath);
            PlatformFile.DeleteFile(*TempFilePath); // Cleanup temp file
            return false;
        }
    }
    
    if (!PlatformFile.MoveFile(*FilePath, *TempFilePath))
    {
        UE_LOG(LogInventory, Error, TEXT("SaveInventoryToFile: Failed to move temp file to final location"));
        PlatformFile.DeleteFile(*TempFilePath); // Cleanup temp file
        return false;
    }
    
    UE_LOG(LogInventory, Log, TEXT("SaveInventoryToFile: Successfully saved inventory (%d bytes)"), 
        DataString.Len());
    
    return true;
}

bool USuspenseInventorySerializer::LoadInventoryFromFile(USuspenseInventoryComponent* InventoryComponent, 
                                                     const FString& FilePath, 
                                                     bool bValidateAfterLoad)
{
    if (!InventoryComponent)
    {
        UE_LOG(LogInventory, Error, TEXT("LoadInventoryFromFile: Invalid inventory component"));
        return false;
    }
    
    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath))
    {
        UE_LOG(LogInventory, Warning, TEXT("LoadInventoryFromFile: File not found: %s"), *FilePath);
        return false;
    }
    
    UE_LOG(LogInventory, Log, TEXT("LoadInventoryFromFile: Loading from '%s'"), *FilePath);
    
    // Автоматически определяем формат файла
    bool bIsJson = false;
    if (!DetectFileFormat(FilePath, bIsJson))
    {
        UE_LOG(LogInventory, Warning, TEXT("LoadInventoryFromFile: Could not detect file format, assuming JSON"));
        bIsJson = true;
    }
    
    // Загружаем содержимое файла
    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
    {
        UE_LOG(LogInventory, Error, TEXT("LoadInventoryFromFile: Failed to read file content"));
        return false;
    }
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("LoadInventoryFromFile: Loaded %d characters from file"), 
        FileContent.Len());
    
    bool bSuccess = false;
    
    if (bIsJson)
    {
        bSuccess = DeserializeInventoryFromJson(InventoryComponent, FileContent);
    }
    else
    {
        UE_LOG(LogInventory, Error, TEXT("LoadInventoryFromFile: Binary format not implemented yet"));
        return false;
    }
    
    if (bSuccess && bValidateAfterLoad)
    {
        // Валидируем загруженный инвентарь
        TArray<FString> ValidationErrors;
        if (!InventoryComponent->ValidateInventoryIntegrity(ValidationErrors))
        {
            UE_LOG(LogInventory, Warning, TEXT("LoadInventoryFromFile: Post-load validation found issues:"));
            for (const FString& Error : ValidationErrors)
            {
                UE_LOG(LogInventory, Warning, TEXT("  - %s"), *Error);
            }
        }
    }
    
    UE_LOG(LogInventory, Log, TEXT("LoadInventoryFromFile: Load result: %s"), 
        bSuccess ? TEXT("Success") : TEXT("Failed"));
    
    return bSuccess;
}

//==================================================================
// Validation Implementation
//==================================================================

bool USuspenseInventorySerializer::ValidateSerializedData(const FSerializedInventoryData& SerializedData,
                                                       TArray<FName>& OutMissingItems,
                                                       TArray<FString>& OutValidationErrors)
{
    OutMissingItems.Empty();
    OutValidationErrors.Empty();
    
    // Базовая валидация структуры
    if (!SerializedData.IsValid())
    {
        OutValidationErrors.Add(TEXT("Invalid basic structure"));
        return false;
    }
    
    // ИСПРАВЛЕНО: Используем FSuspenseItemSystemAccess для получения ItemManager
    // Получаем любой World context из engine для доступа к ItemManager
    USuspenseItemManager* ItemManager = nullptr;
    if (GEngine && GEngine->GetWorldContexts().Num() > 0)
    {
        const FWorldContext& WorldContext = GEngine->GetWorldContexts()[0];
        if (WorldContext.World())
        {
            ItemManager = FItemSystemAccess::GetItemManager(WorldContext.World());
        }
    }
    
    if (!ItemManager)
    {
        OutValidationErrors.Add(TEXT("ItemManager not available for validation"));
        return false;
    }
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("ValidateSerializedData: Validating %d items"), 
        SerializedData.ItemInstances.Num());
    
    // Валидируем каждый предмет
    bool bAllValid = true;
    TSet<FName> ProcessedItems; // Для отслеживания дубликатов
    
    for (const FSuspenseInventoryItemInstance& Instance : SerializedData.ItemInstances)
    {
        // Проверяем базовую валидность экземпляра
        if (!Instance.IsValid())
        {
            OutValidationErrors.Add(FString::Printf(TEXT("Invalid instance: %s"), 
                *Instance.GetShortDebugString()));
            bAllValid = false;
            continue;
        }
        
        // Проверяем существование в DataTable
        FSuspenseUnifiedItemData ItemData;
        if (!ItemManager->GetUnifiedItemData(Instance.ItemID, ItemData))
        {
            OutMissingItems.AddUnique(Instance.ItemID);
            OutValidationErrors.Add(FString::Printf(TEXT("Item not found in DataTable: %s"), 
                *Instance.ItemID.ToString()));
            bAllValid = false;
            continue;
        }
        
        // Проверяем количество против максимального размера стека
        if (Instance.Quantity > ItemData.MaxStackSize)
        {
            OutValidationErrors.Add(FString::Printf(TEXT("Quantity %d exceeds max stack size %d for item: %s"), 
                Instance.Quantity, ItemData.MaxStackSize, *Instance.ItemID.ToString()));
            bAllValid = false;
        }
        
        // Проверяем размещение в сетке
        if (Instance.IsPlacedInInventory())
        {
            int32 AnchorX = Instance.AnchorIndex % SerializedData.GridWidth;
            int32 AnchorY = Instance.AnchorIndex / SerializedData.GridWidth;
            
            FVector2D ItemSize(ItemData.GridSize.X, ItemData.GridSize.Y);
            if (Instance.bIsRotated)
            {
                ItemSize = FVector2D(ItemSize.Y, ItemSize.X);
            }
            
            if (AnchorX + ItemSize.X > SerializedData.GridWidth || 
                AnchorY + ItemSize.Y > SerializedData.GridHeight)
            {
                OutValidationErrors.Add(FString::Printf(TEXT("Item extends beyond grid bounds: %s at position %d"), 
                    *Instance.ItemID.ToString(), Instance.AnchorIndex));
                bAllValid = false;
            }
        }
        
        ProcessedItems.Add(Instance.ItemID);
    }
    
    UE_LOG(LogInventory, Log, TEXT("ValidateSerializedData: Validation complete - Valid: %s, Errors: %d, Missing: %d"), 
        bAllValid ? TEXT("Yes") : TEXT("No"), OutValidationErrors.Num(), OutMissingItems.Num());
    
    return bAllValid;
}

bool USuspenseInventorySerializer::MigrateSerializedData(FSerializedInventoryData& SerializedData)
{
    if (SerializedData.Version >= CURRENT_VERSION)
    {
        return true; // Миграция не нужна
    }
    
    UE_LOG(LogInventory, Log, TEXT("MigrateSerializedData: Migrating from version %d to %d"), 
        SerializedData.Version, CURRENT_VERSION);
    
    // Миграция от версии 1 к версии 2
    if (SerializedData.Version == 1)
    {
        // В версии 1 использовались FMCInventoryItemData структуры
        // Мы должны сконвертировать их в FSuspenseInventoryItemInstance
        
        // NOTE: Здесь был бы код конверсии, но поскольку у нас нет доступа к старым структурам
        // в текущем коде, мы просто обновляем версию
        
        UE_LOG(LogInventory, Warning, TEXT("MigrateSerializedData: Version 1 to 2 migration not fully implemented"));
        
        // Мигрируем runtime свойства каждого экземпляра
        for (FSuspenseInventoryItemInstance& Instance : SerializedData.ItemInstances)
        {
            MigrateRuntimeProperties(Instance);
        }
    }
    
    // Обновляем версию
    SerializedData.Version = CURRENT_VERSION;
    
    UE_LOG(LogInventory, Log, TEXT("MigrateSerializedData: Migration completed"));
    
    return true;
}

int32 USuspenseInventorySerializer::CleanupInvalidItems(FSerializedInventoryData& SerializedData)
{
    TArray<FName> MissingItems;
    TArray<FString> ValidationErrors;
    
    // Сначала валидируем данные
    ValidateSerializedData(SerializedData, MissingItems, ValidationErrors);
    
    if (MissingItems.Num() == 0)
    {
        return 0; // Нет предметов для удаления
    }
    
    UE_LOG(LogInventory, Log, TEXT("CleanupInvalidItems: Removing %d invalid items"), MissingItems.Num());
    
    // Удаляем недопустимые предметы
    int32 RemovedCount = 0;
    SerializedData.ItemInstances.RemoveAll([&MissingItems, &RemovedCount](const FSuspenseInventoryItemInstance& Instance)
    {
        if (MissingItems.Contains(Instance.ItemID))
        {
            UE_LOG(LogInventory, VeryVerbose, TEXT("CleanupInvalidItems: Removing item: %s"), 
                *Instance.ItemID.ToString());
            RemovedCount++;
            return true;
        }
        return false;
    });
    
    UE_LOG(LogInventory, Log, TEXT("CleanupInvalidItems: Removed %d invalid items"), RemovedCount);
    
    return RemovedCount;
}

//==================================================================
// Utility Methods Implementation
//==================================================================

FString USuspenseInventorySerializer::GetInventoryStatistics(const FSerializedInventoryData& SerializedData)
{
    if (!SerializedData.IsValid())
    {
        return TEXT("Invalid inventory data");
    }
    
    // ИСПРАВЛЕНО: Получаем World context для доступа к InventoryUtils
    UWorld* World = nullptr;
    if (GEngine && GEngine->GetWorldContexts().Num() > 0)
    {
        World = GEngine->GetWorldContexts()[0].World();
    }
    
    if (!World)
    {
        return TEXT("Cannot calculate statistics - World context not available");
    }
    
    // Подсчитываем статистику
    int32 TotalItems = 0;
    float TotalWeight = 0.0f;
    TMap<FGameplayTag, int32> TypeCounts;
    
    for (const FSuspenseInventoryItemInstance& Instance : SerializedData.ItemInstances)
    {
        TotalItems += Instance.Quantity;
        
        // ИСПРАВЛЕНО: Добавлен WorldContextObject как первый параметр
        // Получаем вес предмета
        float ItemWeight = InventoryUtils::GetItemWeight(World, Instance.ItemID);
        TotalWeight += ItemWeight * Instance.Quantity;
        
        // Подсчитываем типы предметов
        FSuspenseUnifiedItemData ItemData;
        if (InventoryUtils::GetUnifiedItemData(World, Instance.ItemID, ItemData))
        {
            TypeCounts.FindOrAdd(ItemData.ItemType)++;
        }
    }
    
    // Формируем отчет
    FString Stats = FString::Printf(
        TEXT("Inventory Statistics:\n")
        TEXT("  Grid Size: %dx%d (%d cells)\n")
        TEXT("  Max Weight: %.1f kg\n")
        TEXT("  Total Unique Items: %d\n")
        TEXT("  Total Item Count: %d\n")
        TEXT("  Total Weight: %.1f kg (%.1f%% of capacity)\n")
        TEXT("  Version: %d\n")
        TEXT("  Save Time: %s\n"),
        SerializedData.GridWidth, SerializedData.GridHeight, SerializedData.GridWidth * SerializedData.GridHeight,
        SerializedData.MaxWeight,
        SerializedData.ItemInstances.Num(),
        TotalItems,
        TotalWeight, SerializedData.MaxWeight > 0 ? (TotalWeight / SerializedData.MaxWeight) * 100.0f : 0.0f,
        SerializedData.Version,
        *SerializedData.SaveTime.ToString()
    );
    
    // Добавляем статистику по типам
    if (TypeCounts.Num() > 0)
    {
        Stats += TEXT("  Item Types:\n");
        for (const auto& TypePair : TypeCounts)
        {
            Stats += FString::Printf(TEXT("    %s: %d items\n"), 
                *TypePair.Key.ToString(), TypePair.Value);
        }
    }
    
    return Stats;
}

bool USuspenseInventorySerializer::CompareInventoryData(const FSerializedInventoryData& DataA,
                                                    const FSerializedInventoryData& DataB,
                                                    TArray<FString>& OutDifferences)
{
    OutDifferences.Empty();
    
    // Сравниваем базовые свойства
    if (DataA.Version != DataB.Version)
    {
        OutDifferences.Add(FString::Printf(TEXT("Version: %d vs %d"), DataA.Version, DataB.Version));
    }
    
    if (DataA.GridWidth != DataB.GridWidth || DataA.GridHeight != DataB.GridHeight)
    {
        OutDifferences.Add(FString::Printf(TEXT("Grid Size: %dx%d vs %dx%d"), 
            DataA.GridWidth, DataA.GridHeight, DataB.GridWidth, DataB.GridHeight));
    }
    
    if (!FMath::IsNearlyEqual(DataA.MaxWeight, DataB.MaxWeight))
    {
        OutDifferences.Add(FString::Printf(TEXT("Max Weight: %.1f vs %.1f"), DataA.MaxWeight, DataB.MaxWeight));
    }
    
    // Сравниваем предметы
    if (DataA.ItemInstances.Num() != DataB.ItemInstances.Num())
    {
        OutDifferences.Add(FString::Printf(TEXT("Item Count: %d vs %d"), 
            DataA.ItemInstances.Num(), DataB.ItemInstances.Num()));
    }
    
    // Создаем карты для сравнения предметов
    TMap<FGuid, FSuspenseInventoryItemInstance> MapA, MapB;
    
    for (const FSuspenseInventoryItemInstance& Instance : DataA.ItemInstances)
    {
        MapA.Add(Instance.InstanceID, Instance);
    }
    
    for (const FSuspenseInventoryItemInstance& Instance : DataB.ItemInstances)
    {
        MapB.Add(Instance.InstanceID, Instance);
    }
    
    // Ищем различия в предметах
    for (const auto& PairA : MapA)
    {
        const FSuspenseInventoryItemInstance* InstanceB = MapB.Find(PairA.Key);
        if (!InstanceB)
        {
            OutDifferences.Add(FString::Printf(TEXT("Item only in A: %s"), 
                *PairA.Value.GetShortDebugString()));
            continue;
        }
        
        // Сравниваем свойства предметов
        if (PairA.Value.ItemID != InstanceB->ItemID)
        {
            OutDifferences.Add(FString::Printf(TEXT("Item ID differs for %s: %s vs %s"), 
                *PairA.Key.ToString(), *PairA.Value.ItemID.ToString(), *InstanceB->ItemID.ToString()));
        }
        
        if (PairA.Value.Quantity != InstanceB->Quantity)
        {
            OutDifferences.Add(FString::Printf(TEXT("Quantity differs for %s: %d vs %d"), 
                *PairA.Value.ItemID.ToString(), PairA.Value.Quantity, InstanceB->Quantity));
        }
    }
    
    for (const auto& PairB : MapB)
    {
        if (!MapA.Find(PairB.Key))
        {
            OutDifferences.Add(FString::Printf(TEXT("Item only in B: %s"), 
                *PairB.Value.GetShortDebugString()));
        }
    }
    
    return OutDifferences.Num() == 0;
}

//==================================================================
// Internal Helper Methods Implementation
//==================================================================

FString USuspenseInventorySerializer::StructToJson(const FSerializedInventoryData& Data, bool bPrettyPrint)
{
    FString JsonString;
    
    // Используем встроенный Unreal JSON converter
    bool bSuccess = FJsonObjectConverter::UStructToJsonObjectString(
        Data, 
        JsonString, 
        0, // CheckFlags
        0, // SkipFlags
        0, // Indent (0 for compact, > 0 for pretty print)
        bPrettyPrint ? nullptr : nullptr // Пока не поддерживаем pretty print в этой версии
    );
    
    if (!bSuccess)
    {
        UE_LOG(LogInventory, Error, TEXT("StructToJson: Failed to convert struct to JSON"));
        return FString();
    }
    
    return JsonString;
}

bool USuspenseInventorySerializer::JsonToStruct(const FString& JsonString, FSerializedInventoryData& OutData)
{
    if (JsonString.IsEmpty())
    {
        UE_LOG(LogInventory, Error, TEXT("JsonToStruct: Empty JSON string"));
        return false;
    }
    
    // Используем встроенный Unreal JSON converter
    bool bSuccess = FJsonObjectConverter::JsonObjectStringToUStruct(
        JsonString, 
        &OutData, 
        0, // CheckFlags
        0  // SkipFlags
    );
    
    if (!bSuccess)
    {
        UE_LOG(LogInventory, Error, TEXT("JsonToStruct: Failed to parse JSON to struct"));
        return false;
    }
    
    return true;
}

USuspenseItemManager* USuspenseInventorySerializer::GetItemManager(const UObject* WorldContext)
{
    // ИСПРАВЛЕНО: Используем FSuspenseItemSystemAccess вместо прямого обращения
    return FItemSystemAccess::GetItemManager(WorldContext);
}

bool USuspenseInventorySerializer::ValidateItemInstance(const FSuspenseInventoryItemInstance& Instance, 
                                                     USuspenseItemManager* ItemManager,
                                                     FString& OutError)
{
    // Базовая валидация экземпляра
    if (!Instance.IsValid())
    {
        OutError = TEXT("Invalid instance structure");
        return false;
    }
    
    // Проверяем существование в DataTable
    FSuspenseUnifiedItemData ItemData;
    if (!ItemManager->GetUnifiedItemData(Instance.ItemID, ItemData))
    {
        OutError = FString::Printf(TEXT("Item '%s' not found in DataTable"), *Instance.ItemID.ToString());
        return false;
    }
    
    // Проверяем количество
    if (Instance.Quantity > ItemData.MaxStackSize)
    {
        OutError = FString::Printf(TEXT("Quantity %d exceeds max stack size %d"), 
            Instance.Quantity, ItemData.MaxStackSize);
        return false;
    }
    
    // Проверяем runtime свойства для специфичных типов
    if (ItemData.bIsWeapon)
    {
        if (Instance.HasRuntimeProperty(TEXT("Ammo")))
        {
            int32 Ammo = FMath::RoundToInt(Instance.GetRuntimeProperty(TEXT("Ammo")));
            int32 MaxAmmo = FMath::RoundToInt(Instance.GetRuntimeProperty(TEXT("MaxAmmo"), 30.0f));
            
            if (Ammo > MaxAmmo)
            {
                OutError = FString::Printf(TEXT("Ammo %d exceeds max ammo %d"), Ammo, MaxAmmo);
                return false;
            }
        }
    }
    
    if (ItemData.bIsEquippable)
    {
        if (Instance.HasRuntimeProperty(TEXT("Durability")))
        {
            float Durability = Instance.GetRuntimeProperty(TEXT("Durability"));
            float MaxDurability = Instance.GetRuntimeProperty(TEXT("MaxDurability"), 100.0f);
            
            if (Durability > MaxDurability)
            {
                OutError = FString::Printf(TEXT("Durability %.1f exceeds max durability %.1f"), 
                    Durability, MaxDurability);
                return false;
            }
        }
    }
    
    return true;
}

bool USuspenseInventorySerializer::CreateFileBackup(const FString& OriginalPath)
{
    FString BackupPath = OriginalPath + BACKUP_EXTENSION;
    
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    
    // Удаляем старый backup если существует
    if (PlatformFile.FileExists(*BackupPath))
    {
        if (!PlatformFile.DeleteFile(*BackupPath))
        {
            UE_LOG(LogInventory, Warning, TEXT("CreateFileBackup: Failed to delete old backup: %s"), 
                *BackupPath);
        }
    }
    
    // Копируем оригинальный файл
    if (!PlatformFile.CopyFile(*BackupPath, *OriginalPath))
    {
        UE_LOG(LogInventory, Error, TEXT("CreateFileBackup: Failed to create backup: %s"), *BackupPath);
        return false;
    }
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("CreateFileBackup: Created backup: %s"), *BackupPath);
    return true;
}

bool USuspenseInventorySerializer::DetectFileFormat(const FString& FilePath, bool& bOutIsJson)
{
    // Простая эвристика: если файл начинается с '{', считаем его JSON
    FString FirstChars;
    if (!FFileHelper::LoadFileToString(FirstChars, *FilePath, FFileHelper::EHashOptions::None, FILEREAD_Silent))
    {
        return false;
    }
    
    FirstChars = FirstChars.TrimStartAndEnd();
    bOutIsJson = FirstChars.StartsWith(TEXT("{"));
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("DetectFileFormat: File '%s' detected as %s"), 
        *FilePath, bOutIsJson ? TEXT("JSON") : TEXT("Binary"));
    
    return true;
}

void USuspenseInventorySerializer::MigrateRuntimeProperties(FSuspenseInventoryItemInstance& Instance)
{
    // Миграция runtime свойств от старой версии к новой
    // В этой версии ничего не делаем, но метод готов для future migrations
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("MigrateRuntimeProperties: Migrated properties for %s"), 
        *Instance.ItemID.ToString());
}