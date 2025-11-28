#include "Utility/SuspenseItemLibrary.h"
#include "Interfaces/Inventory/ISuspenseInventoryItem.h"
#include "ItemSystem/SuspenseItemManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameplayTagsManager.h"
#include "UObject/SoftObjectPtr.h"

// Определяем log категорию для consistent logging
DEFINE_LOG_CATEGORY_STATIC(LogMedComItemLibrary, Log, All);

//==================================================================
// Core DataTable Integration Methods - ОСНОВНЫЕ МЕТОДЫ DATATABLE
//==================================================================

bool USuspenseItemLibrary::GetUnifiedItemData(const UObject* WorldContext, FName ItemID, FSuspenseUnifiedItemData& OutItemData)
{
    // Основной метод новой архитектуры - получение статических данных из DataTable
    // Заменяет все legacy методы получения item data
    
    USuspenseItemManager* ItemManager = GetValidatedItemManager(WorldContext);
    if (!ItemManager)
    {
        UE_LOG(LogMedComItemLibrary, Warning, TEXT("GetUnifiedItemData: ItemManager недоступен"));
        return false;
    }
    
    if (ItemID.IsNone())
    {
        UE_LOG(LogMedComItemLibrary, Warning, TEXT("GetUnifiedItemData: Пустой ItemID"));
        return false;
    }
    
    bool bSuccess = ItemManager->GetUnifiedItemData(ItemID, OutItemData);
    if (!bSuccess)
    {
        UE_LOG(LogMedComItemLibrary, Warning, TEXT("GetUnifiedItemData: Предмет '%s' не найден в DataTable"), *ItemID.ToString());
    }
    
    return bSuccess;
}

USuspenseItemManager* USuspenseItemLibrary::GetItemManager(const UObject* WorldContext)
{
    // Публичный метод для прямого доступа к ItemManager из Blueprint
    return GetValidatedItemManager(WorldContext);
}

bool USuspenseItemLibrary::DoesItemExistInDataTable(const UObject* WorldContext, FName ItemID)
{
    // ИСПРАВЛЕНО: Используем правильное имя метода HasItem вместо DoesItemExist
    USuspenseItemManager* ItemManager = GetValidatedItemManager(WorldContext);
    if (!ItemManager || ItemID.IsNone())
    {
        return false;
    }
    
    // Используем существующий метод HasItem из ItemManager
    return ItemManager->HasItem(ItemID);
}


//==================================================================
// Updated Legacy Support Methods - ОБНОВЛЕННЫЕ LEGACY МЕТОДЫ
//==================================================================

FText USuspenseItemLibrary::GetItemName(const FSuspenseUnifiedItemData& ItemData)
{
    // ОБНОВЛЕНО: Теперь работает с unified структурой вместо legacy FMCInventoryItemData
    if (!ItemData.DisplayName.IsEmpty())
    {
        return ItemData.DisplayName;
    }
    
    // Fallback на ItemID если DisplayName пуст
    return FText::FromName(ItemData.ItemID);
}

FText USuspenseItemLibrary::GetItemDescription(const FSuspenseUnifiedItemData& ItemData)
{
    // ОБНОВЛЕНО: Используем Description поле из unified структуры
    if (!ItemData.Description.IsEmpty())
    {
        return ItemData.Description;
    }
    
    return FText::GetEmpty();
}

UTexture2D* USuspenseItemLibrary::GetItemIcon(const FSuspenseUnifiedItemData& ItemData)
{
    // ОБНОВЛЕНО: Обрабатываем soft object pointer и загружаем синхронно для immediate use
    if (!ItemData.Icon.IsNull())
    {
        return ItemData.Icon.LoadSynchronous();
    }
    
    return nullptr;
}

//==================================================================
// Enhanced Runtime Instance Methods - НОВЫЕ МЕТОДЫ ДЛЯ RUNTIME
//==================================================================

bool USuspenseItemLibrary::CreateItemInstance(const UObject* WorldContext, FName ItemID, int32 Quantity, FInventoryItemInstance& OutInstance)
{
    // Создание runtime экземпляра из DataTable ID - центральный метод новой архитектуры
    
    USuspenseItemManager* ItemManager = GetValidatedItemManager(WorldContext);
    if (!ItemManager)
    {
        UE_LOG(LogMedComItemLibrary, Warning, TEXT("CreateItemInstance: ItemManager недоступен"));
        return false;
    }
    
    // Проверяем валидность входных параметров
    if (ItemID.IsNone())
    {
        UE_LOG(LogMedComItemLibrary, Warning, TEXT("CreateItemInstance: Пустой ItemID"));
        return false;
    }
    
    if (Quantity <= 0)
    {
        UE_LOG(LogMedComItemLibrary, Warning, TEXT("CreateItemInstance: Некорректное количество %d"), Quantity);
        return false;
    }
    
    // Получаем unified данные для валидации и ограничений
    FSuspenseUnifiedItemData UnifiedData;
    if (!ItemManager->GetUnifiedItemData(ItemID, UnifiedData))
    {
        UE_LOG(LogMedComItemLibrary, Error, TEXT("CreateItemInstance: Предмет '%s' не найден в DataTable"), *ItemID.ToString());
        return false;
    }
    
    // Создаем новый runtime экземпляр
    OutInstance = FInventoryItemInstance();
    OutInstance.ItemID = ItemID;
    OutInstance.Quantity = FMath::Clamp(Quantity, 1, UnifiedData.MaxStackSize);
    OutInstance.InstanceID = FGuid::NewGuid(); // Уникальный GUID для tracking в multiplayer
    OutInstance.AnchorIndex = INDEX_NONE; // Не размещен в инвентаре
    OutInstance.bIsRotated = false;
    
    // Инициализируем runtime свойства на основе типа предмета
    if (UnifiedData.bIsWeapon)
    {
        // Для оружия инициализируем ammo свойства
        OutInstance.RuntimeProperties.Add(TEXT("Ammo"), 0.0f);
        OutInstance.RuntimeProperties.Add(TEXT("MaxAmmo"), 30.0f); // Default magazine size
        OutInstance.RuntimeProperties.Add(TEXT("Durability"), 100.0f);
        OutInstance.RuntimeProperties.Add(TEXT("MaxDurability"), 100.0f);
    }
    else if (UnifiedData.bIsArmor)
    {
        // Для брони инициализируем durability
        OutInstance.RuntimeProperties.Add(TEXT("Durability"), 100.0f);
        OutInstance.RuntimeProperties.Add(TEXT("MaxDurability"), 100.0f);
    }
    else if (UnifiedData.bIsConsumable)
    {
        // Для расходников устанавливаем effectiveness
        OutInstance.RuntimeProperties.Add(TEXT("Effectiveness"), 100.0f);
    }
    
    // Логируем успешное создание
    UE_LOG(LogMedComItemLibrary, VeryVerbose, TEXT("CreateItemInstance: Создан экземпляр %s (x%d) [%s]"), 
        *ItemID.ToString(), OutInstance.Quantity, *OutInstance.InstanceID.ToString().Left(8));
    
    return true;
}

bool USuspenseItemLibrary::GetUnifiedDataFromInstance(const UObject* WorldContext, const FInventoryItemInstance& ItemInstance, FSuspenseUnifiedItemData& OutItemData)
{
    // Получение unified данных из runtime экземпляра
    if (!ItemInstance.IsValid())
    {
        UE_LOG(LogMedComItemLibrary, Warning, TEXT("GetUnifiedDataFromInstance: Некорректный runtime экземпляр"));
        return false;
    }
    
    return GetUnifiedItemData(WorldContext, ItemInstance.ItemID, OutItemData);
}

float USuspenseItemLibrary::GetRuntimeProperty(const FInventoryItemInstance& ItemInstance, const FString& PropertyName, float DefaultValue)
{
    // Получение runtime свойства с корректной конверсией типов
    if (!ItemInstance.IsValid() || PropertyName.IsEmpty())
    {
        return DefaultValue;
    }
    
    // Конвертируем FString в FName для поиска в мапе
    FName PropertyKey(*PropertyName);
    const float* FoundValue = ItemInstance.RuntimeProperties.Find(PropertyKey);
    return FoundValue ? *FoundValue : DefaultValue;
}

void USuspenseItemLibrary::SetRuntimeProperty(FInventoryItemInstance& ItemInstance, const FString& PropertyName, float Value)
{
    // Установка runtime свойства с корректной конверсией типов
    if (!ItemInstance.IsValid() || PropertyName.IsEmpty())
    {
        UE_LOG(LogMedComItemLibrary, Warning, TEXT("SetRuntimeProperty: Некорректные параметры"));
        return;
    }
    
    // Конвертируем FString в FName для хранения в мапе
    FName PropertyKey(*PropertyName);
    ItemInstance.RuntimeProperties.Add(PropertyKey, Value);
    
    UE_LOG(LogMedComItemLibrary, VeryVerbose, TEXT("SetRuntimeProperty: %s.%s = %.2f [%s]"), 
        *ItemInstance.ItemID.ToString(), *PropertyName, Value, *ItemInstance.InstanceID.ToString().Left(8));
}

bool USuspenseItemLibrary::HasRuntimeProperty(const FInventoryItemInstance& ItemInstance, const FString& PropertyName)
{
    // Проверка существования runtime свойства с корректной конверсией типов
    if (!ItemInstance.IsValid() || PropertyName.IsEmpty())
    {
        return false;
    }
    
    // Конвертируем FString в FName для поиска в мапе
    FName PropertyKey(*PropertyName);
    return ItemInstance.RuntimeProperties.Contains(PropertyKey);
}

//==================================================================
// Enhanced Display and Formatting Methods - УЛУЧШЕННЫЕ МЕТОДЫ ОТОБРАЖЕНИЯ
//==================================================================

FText USuspenseItemLibrary::FormatItemQuantity(const FSuspenseUnifiedItemData& ItemData, int32 Quantity)
{
    // ОБНОВЛЕНО: Используем MaxStackSize из unified данных вместо параметра
    if (ItemData.MaxStackSize <= 1)
    {
        return FText::GetEmpty(); // Не показываем количество для нестакуемых предметов
    }
    
    if (ItemData.MaxStackSize > 999)
    {
        // Для больших стеков показываем только текущее количество
        return FText::Format(NSLOCTEXT("Inventory", "QuantityLarge", "{0}"), FText::AsNumber(Quantity));
    }
    
    // Стандартный формат: текущее/максимальное
    return FText::Format(NSLOCTEXT("Inventory", "QuantityWithMax", "{0}/{1}"), 
        FText::AsNumber(Quantity), FText::AsNumber(ItemData.MaxStackSize));
}

FText USuspenseItemLibrary::FormatItemQuantityFromInstance(const UObject* WorldContext, const FInventoryItemInstance& ItemInstance)
{
    // Форматирование количества из runtime экземпляра
    if (!ItemInstance.IsValid())
    {
        return FText::GetEmpty();
    }
    
    FSuspenseUnifiedItemData UnifiedData;
    if (!GetUnifiedDataFromInstance(WorldContext, ItemInstance, UnifiedData))
    {
        // Fallback форматирование без DataTable данных
        return FText::AsNumber(ItemInstance.Quantity);
    }
    
    return FormatItemQuantity(UnifiedData, ItemInstance.Quantity);
}

FText USuspenseItemLibrary::FormatItemWeight(const FSuspenseUnifiedItemData& ItemData, int32 Quantity, bool bIncludeUnit)
{
    // Форматирование веса с учетом количества и локализации
    float TotalWeight = ItemData.Weight * Quantity;
    
    if (bIncludeUnit)
    {
        return FText::Format(NSLOCTEXT("Inventory", "WeightWithUnit", "{0} кг"), 
            FText::AsNumber(TotalWeight, &FNumberFormattingOptions::DefaultNoGrouping()));
    }
    
    return FText::AsNumber(TotalWeight, &FNumberFormattingOptions::DefaultNoGrouping());
}

FLinearColor USuspenseItemLibrary::GetRarityColor(const FSuspenseUnifiedItemData& ItemData)
{
    // Получение цвета редкости из unified данных
    // Использует встроенный метод unified структуры
    return ItemData.GetRarityColor();
}

FText USuspenseItemLibrary::FormatItemDurability(const FInventoryItemInstance& ItemInstance, bool bAsPercentage)
{
    // Форматирование прочности из runtime свойств
    if (!ItemInstance.IsValid())
    {
        return FText::GetEmpty();
    }
    
    float CurrentDurability = GetRuntimeProperty(ItemInstance, TEXT("Durability"), 100.0f);
    float MaxDurability = GetRuntimeProperty(ItemInstance, TEXT("MaxDurability"), 100.0f);
    
    if (MaxDurability <= 0.0f)
    {
        return FText::GetEmpty(); // Предмет не имеет системы прочности
    }
    
    if (bAsPercentage)
    {
        float Percentage = FMath::Clamp((CurrentDurability / MaxDurability) * 100.0f, 0.0f, 100.0f);
        return FText::Format(NSLOCTEXT("Inventory", "DurabilityPercent", "{0}%"), 
            FText::AsNumber(FMath::RoundToInt(Percentage)));
    }
    else
    {
        return FText::Format(NSLOCTEXT("Inventory", "DurabilityAbsolute", "{0}/{1}"), 
            FText::AsNumber(FMath::RoundToInt(CurrentDurability)), 
            FText::AsNumber(FMath::RoundToInt(MaxDurability)));
    }
}

//==================================================================
// Enhanced Search and Filtering Methods - УЛУЧШЕННЫЕ МЕТОДЫ ПОИСКА
//==================================================================

TArray<FSuspenseUnifiedItemData> USuspenseItemLibrary::FilterItemsByType(const TArray<FSuspenseUnifiedItemData>& ItemDataArray, const FGameplayTag& TypeTag, bool bExactMatch)
{
    // ОБНОВЛЕНО: Поддержка иерархических тегов для гибкой фильтрации
    TArray<FSuspenseUnifiedItemData> FilteredItems;
    
    if (!TypeTag.IsValid())
    {
        return ItemDataArray; // Пустой фильтр возвращает все предметы
    }
    
    for (const FSuspenseUnifiedItemData& Item : ItemDataArray)
    {
        if (DoesTagMatch(Item.ItemType, TypeTag, bExactMatch))
        {
            FilteredItems.Add(Item);
        }
    }
    
    UE_LOG(LogMedComItemLibrary, VeryVerbose, TEXT("FilterItemsByType: Отфильтровано %d из %d предметов по тегу %s"), 
        FilteredItems.Num(), ItemDataArray.Num(), *TypeTag.ToString());
    
    return FilteredItems;
}

TArray<FSuspenseUnifiedItemData> USuspenseItemLibrary::FilterItemsByRarity(const TArray<FSuspenseUnifiedItemData>& ItemDataArray, const FGameplayTag& RarityTag)
{
    // Фильтрация по редкости с точным совпадением
    TArray<FSuspenseUnifiedItemData> FilteredItems;
    
    if (!RarityTag.IsValid())
    {
        return ItemDataArray;
    }
    
    for (const FSuspenseUnifiedItemData& Item : ItemDataArray)
    {
        if (Item.Rarity.MatchesTagExact(RarityTag))
        {
            FilteredItems.Add(Item);
        }
    }
    
    return FilteredItems;
}

TArray<FSuspenseUnifiedItemData> USuspenseItemLibrary::FilterItemsByTags(const TArray<FSuspenseUnifiedItemData>& ItemDataArray, const FGameplayTagContainer& FilterTags, bool bRequireAll)
{
    // Расширенная фильтрация с логическими операторами
    TArray<FSuspenseUnifiedItemData> FilteredItems;
    
    if (FilterTags.IsEmpty())
    {
        return ItemDataArray;
    }
    
    for (const FSuspenseUnifiedItemData& Item : ItemDataArray)
    {
        // Собираем все теги предмета для проверки
        FGameplayTagContainer ItemAllTags;
        ItemAllTags.AddTag(Item.ItemType);
        ItemAllTags.AddTag(Item.Rarity);
        ItemAllTags.AppendTags(Item.ItemTags);
        
        // Добавляем специализированные теги
        if (Item.bIsWeapon && Item.WeaponArchetype.IsValid())
        {
            ItemAllTags.AddTag(Item.WeaponArchetype);
        }
        if (Item.bIsArmor && Item.ArmorType.IsValid())
        {
            ItemAllTags.AddTag(Item.ArmorType);
        }
        if (Item.bIsAmmo && Item.AmmoCaliber.IsValid())
        {
            ItemAllTags.AddTag(Item.AmmoCaliber);
        }
        
        // Применяем логику фильтрации
        bool bMatches = false;
        if (bRequireAll)
        {
            bMatches = ItemAllTags.HasAll(FilterTags);
        }
        else
        {
            bMatches = ItemAllTags.HasAny(FilterTags);
        }
        
        if (bMatches)
        {
            FilteredItems.Add(Item);
        }
    }
    
    return FilteredItems;
}

TArray<FSuspenseUnifiedItemData> USuspenseItemLibrary::SearchItems(const TArray<FSuspenseUnifiedItemData>& ItemDataArray, const FString& SearchText, bool bSearchDescription, bool bSearchTags)
{
    // ОБНОВЛЕНО: Расширенный поиск с дополнительными опциями
    if (SearchText.IsEmpty())
    {
        return ItemDataArray;
    }
    
    TArray<FSuspenseUnifiedItemData> MatchingItems;
    
    for (const FSuspenseUnifiedItemData& Item : ItemDataArray)
    {
        bool bFound = false;
        
        // Поиск в названии (приоритет)
        if (Item.DisplayName.ToString().Contains(SearchText, ESearchCase::IgnoreCase))
        {
            bFound = true;
        }
        
        // Поиск в ID предмета
        if (!bFound && Item.ItemID.ToString().Contains(SearchText, ESearchCase::IgnoreCase))
        {
            bFound = true;
        }
        
        // Поиск в описании если включен
        if (!bFound && bSearchDescription && Item.Description.ToString().Contains(SearchText, ESearchCase::IgnoreCase))
        {
            bFound = true;
        }
        
        // Поиск в тегах если включен
        if (!bFound && bSearchTags)
        {
            // Проверяем основные теги
            if (Item.ItemType.ToString().Contains(SearchText, ESearchCase::IgnoreCase) ||
                Item.Rarity.ToString().Contains(SearchText, ESearchCase::IgnoreCase))
            {
                bFound = true;
            }
            
            // Проверяем дополнительные теги
            if (!bFound)
            {
                for (const FGameplayTag& Tag : Item.ItemTags)
                {
                    if (Tag.ToString().Contains(SearchText, ESearchCase::IgnoreCase))
                    {
                        bFound = true;
                        break;
                    }
                }
            }
        }
        
        if (bFound)
        {
            MatchingItems.Add(Item);
        }
    }
    
    UE_LOG(LogMedComItemLibrary, VeryVerbose, TEXT("SearchItems: Найдено %d предметов по запросу '%s'"), 
        MatchingItems.Num(), *SearchText);
    
    return MatchingItems;
}

//==================================================================
// Enhanced Sorting Methods - УЛУЧШЕННЫЕ МЕТОДЫ СОРТИРОВКИ
//==================================================================

TArray<FSuspenseUnifiedItemData> USuspenseItemLibrary::SortItemsByName(const TArray<FSuspenseUnifiedItemData>& ItemDataArray, bool bAscending)
{
    // ОБНОВЛЕНО: Сортировка по DisplayName из unified структуры
    TArray<FSuspenseUnifiedItemData> SortedItems = ItemDataArray;
    
    SortedItems.Sort([bAscending](const FSuspenseUnifiedItemData& A, const FSuspenseUnifiedItemData& B) {
        const FString NameA = A.DisplayName.ToString();
        const FString NameB = B.DisplayName.ToString();
        
        return bAscending ? NameA < NameB : NameA > NameB;
    });
    
    return SortedItems;
}

TArray<FSuspenseUnifiedItemData> USuspenseItemLibrary::SortItemsByWeight(const TArray<FSuspenseUnifiedItemData>& ItemDataArray, bool bAscending)
{
    // Сортировка по весу из unified данных
    TArray<FSuspenseUnifiedItemData> SortedItems = ItemDataArray;
    
    SortedItems.Sort([bAscending](const FSuspenseUnifiedItemData& A, const FSuspenseUnifiedItemData& B) {
        return bAscending ? A.Weight < B.Weight : A.Weight > B.Weight;
    });
    
    return SortedItems;
}

TArray<FSuspenseUnifiedItemData> USuspenseItemLibrary::SortItemsByValue(const TArray<FSuspenseUnifiedItemData>& ItemDataArray, bool bAscending)
{
    // Сортировка по базовой стоимости
    TArray<FSuspenseUnifiedItemData> SortedItems = ItemDataArray;
    
    SortedItems.Sort([bAscending](const FSuspenseUnifiedItemData& A, const FSuspenseUnifiedItemData& B) {
        return bAscending ? A.BaseValue < B.BaseValue : A.BaseValue > B.BaseValue;
    });
    
    return SortedItems;
}

TArray<FSuspenseUnifiedItemData> USuspenseItemLibrary::SortItemsByRarity(const TArray<FSuspenseUnifiedItemData>& ItemDataArray, bool bAscending)
{
    // Сортировка по редкости с учетом иерархии
    TArray<FSuspenseUnifiedItemData> SortedItems = ItemDataArray;
    
    SortedItems.Sort([bAscending](const FSuspenseUnifiedItemData& A, const FSuspenseUnifiedItemData& B) {
        int32 PriorityA = GetRarityPriority(A.Rarity);
        int32 PriorityB = GetRarityPriority(B.Rarity);
        
        return bAscending ? PriorityA < PriorityB : PriorityA > PriorityB;
    });
    
    return SortedItems;
}

//==================================================================
// Enhanced Weight and Calculation Methods - УЛУЧШЕННЫЕ РАСЧЕТНЫЕ МЕТОДЫ
//==================================================================

float USuspenseItemLibrary::GetTotalItemsWeight(const TArray<FSuspenseUnifiedItemData>& ItemDataArray, const TArray<int32>& QuantityArray)
{
    // ОБНОВЛЕНО: Корректная работа с unified данными и массивом количеств
    if (ItemDataArray.Num() != QuantityArray.Num())
    {
        UE_LOG(LogMedComItemLibrary, Warning, TEXT("GetTotalItemsWeight: Несоответствие размеров массивов (%d != %d)"), 
            ItemDataArray.Num(), QuantityArray.Num());
        return 0.0f;
    }
    
    float TotalWeight = 0.0f;
    
    for (int32 i = 0; i < ItemDataArray.Num(); ++i)
    {
        const FSuspenseUnifiedItemData& Item = ItemDataArray[i];
        int32 Quantity = FMath::Max(0, QuantityArray[i]); // Защита от отрицательных значений
        
        TotalWeight += Item.Weight * Quantity;
    }
    
    return TotalWeight;
}

float USuspenseItemLibrary::GetTotalInstancesWeight(const TArray<FInventoryItemInstance>& ItemInstances, const UObject* WorldContext)
{
    // Расчет веса из runtime экземпляров с получением данных из DataTable
    USuspenseItemManager* ItemManager = GetValidatedItemManager(WorldContext);
    if (!ItemManager)
    {
        UE_LOG(LogMedComItemLibrary, Warning, TEXT("GetTotalInstancesWeight: ItemManager недоступен"));
        return 0.0f;
    }
    
    float TotalWeight = 0.0f;
    
    for (const FInventoryItemInstance& Instance : ItemInstances)
    {
        if (!Instance.IsValid())
        {
            continue;
        }
        
        FSuspenseUnifiedItemData UnifiedData;
        if (ItemManager->GetUnifiedItemData(Instance.ItemID, UnifiedData))
        {
            TotalWeight += UnifiedData.Weight * Instance.Quantity;
        }
        else
        {
            UE_LOG(LogMedComItemLibrary, Warning, TEXT("GetTotalInstancesWeight: Не найдены данные для предмета '%s'"), 
                *Instance.ItemID.ToString());
        }
    }
    
    return TotalWeight;
}

int32 USuspenseItemLibrary::GetTotalItemsValue(const TArray<FSuspenseUnifiedItemData>& ItemDataArray, const TArray<int32>& QuantityArray)
{
    // Расчет общей стоимости предметов
    if (ItemDataArray.Num() != QuantityArray.Num())
    {
        UE_LOG(LogMedComItemLibrary, Warning, TEXT("GetTotalItemsValue: Несоответствие размеров массивов"));
        return 0;
    }
    
    int32 TotalValue = 0;
    
    for (int32 i = 0; i < ItemDataArray.Num(); ++i)
    {
        const FSuspenseUnifiedItemData& Item = ItemDataArray[i];
        int32 Quantity = FMath::Max(0, QuantityArray[i]);
        
        TotalValue += Item.BaseValue * Quantity;
    }
    
    return TotalValue;
}

//==================================================================
// Grid and UI Helper Methods - МЕТОДЫ ДЛЯ СЕТКИ И UI
//==================================================================

FVector2D USuspenseItemLibrary::GetItemUIPosition(int32 GridX, int32 GridY, const FVector2D& CellSize, float CellSpacing)
{
    // Расчет UI позиции в пикселях - без изменений, stable метод
    return FVector2D(
        GridX * (CellSize.X + CellSpacing),
        GridY * (CellSize.Y + CellSpacing)
    );
}

FVector2D USuspenseItemLibrary::GetItemUISize(const FSuspenseUnifiedItemData& ItemData, bool bIsRotated, const FVector2D& CellSize, float CellSpacing)
{
    // ОБНОВЛЕНО: Использует GridSize из unified данных с учетом поворота
    FVector2D EffectiveSize = bIsRotated ? 
        FVector2D(ItemData.GridSize.Y, ItemData.GridSize.X) : 
        FVector2D(ItemData.GridSize.X, ItemData.GridSize.Y);
    
    return FVector2D(
        EffectiveSize.X * CellSize.X + (EffectiveSize.X - 1) * CellSpacing,
        EffectiveSize.Y * CellSize.Y + (EffectiveSize.Y - 1) * CellSpacing
    );
}

bool USuspenseItemLibrary::GetCoordinatesFromIndex(int32 LinearIndex, int32 GridWidth, int32& OutX, int32& OutY)
{
    // Конверсия линейного индекса в координаты - без изменений, stable метод
    if (LinearIndex < 0 || GridWidth <= 0)
    {
        OutX = -1;
        OutY = -1;
        return false;
    }
    
    OutX = LinearIndex % GridWidth;
    OutY = LinearIndex / GridWidth;
    
    return true;
}

int32 USuspenseItemLibrary::GetIndexFromCoordinates(int32 X, int32 Y, int32 GridWidth)
{
    // Конверсия координат в линейный индекс - без изменений, stable метод
    if (X < 0 || Y < 0 || GridWidth <= 0)
    {
        return INDEX_NONE;
    }
    
    return Y * GridWidth + X;
}

TArray<int32> USuspenseItemLibrary::GetOccupiedSlots(const FSuspenseUnifiedItemData& ItemData, int32 AnchorIndex, bool bIsRotated, int32 GridWidth)
{
    // ОБНОВЛЕНО: Расчет занятых слотов из unified данных
    TArray<int32> OccupiedSlots;
    
    if (AnchorIndex == INDEX_NONE || GridWidth <= 0)
    {
        return OccupiedSlots;
    }
    
    // Получаем эффективный размер с учетом поворота
    FVector2D EffectiveSize = bIsRotated ? 
        FVector2D(ItemData.GridSize.Y, ItemData.GridSize.X) : 
        FVector2D(ItemData.GridSize.X, ItemData.GridSize.Y);
    
    // Вычисляем базовые координаты
    int32 AnchorX, AnchorY;
    if (!GetCoordinatesFromIndex(AnchorIndex, GridWidth, AnchorX, AnchorY))
    {
        return OccupiedSlots;
    }
    
    // Генерируем все занятые слоты
    for (int32 Y = 0; Y < EffectiveSize.Y; ++Y)
    {
        for (int32 X = 0; X < EffectiveSize.X; ++X)
        {
            int32 SlotIndex = GetIndexFromCoordinates(AnchorX + X, AnchorY + Y, GridWidth);
            if (SlotIndex != INDEX_NONE)
            {
                OccupiedSlots.Add(SlotIndex);
            }
        }
    }
    
    return OccupiedSlots;
}

//==================================================================
// Item Type and Classification Helpers - МЕТОДЫ КЛАССИФИКАЦИИ
//==================================================================

bool USuspenseItemLibrary::IsWeapon(const FSuspenseUnifiedItemData& ItemData)
{
    // Проверка является ли предмет оружием
    return ItemData.bIsWeapon;
}

bool USuspenseItemLibrary::IsArmor(const FSuspenseUnifiedItemData& ItemData)
{
    // Проверка является ли предмет броней
    return ItemData.bIsArmor;
}

bool USuspenseItemLibrary::IsAmmo(const FSuspenseUnifiedItemData& ItemData)
{
    // Проверка является ли предмет боеприпасами
    return ItemData.bIsAmmo;
}

bool USuspenseItemLibrary::IsConsumable(const FSuspenseUnifiedItemData& ItemData)
{
    // Проверка является ли предмет расходным материалом
    return ItemData.bIsConsumable;
}

bool USuspenseItemLibrary::IsEquippable(const FSuspenseUnifiedItemData& ItemData)
{
    // Проверка может ли предмет быть экипирован
    return ItemData.bIsEquippable;
}

bool USuspenseItemLibrary::IsStackable(const FSuspenseUnifiedItemData& ItemData)
{
    // Проверка может ли предмет стакаться
    return ItemData.MaxStackSize > 1;
}

//==================================================================
// Conversion and Compatibility Methods - МЕТОДЫ КОНВЕРСИИ
//==================================================================

bool USuspenseItemLibrary::GetUnifiedDataFromObject(UObject* ItemObject, FSuspenseUnifiedItemData& OutItemData)
{
    // ОБНОВЛЕНО: Конверсия из объекта в unified данные через новый интерфейс
    if (!ItemObject)
    {
        return false;
    }
    
    // Проверяем интерфейс инвентарного предмета
    IMedComInventoryItemInterface* ItemInterface = Cast<IMedComInventoryItemInterface>(ItemObject);
    if (!ItemInterface || !ItemInterface->IsInitialized())
    {
        return false;
    }
    
    // Получаем unified данные через интерфейс (новый метод)
    return ItemInterface->GetItemData(OutItemData);
}

bool USuspenseItemLibrary::GetInstanceFromObject(UObject* ItemObject, FInventoryItemInstance& OutInstance)
{
    // Получение runtime экземпляра из объекта предмета
    if (!ItemObject)
    {
        return false;
    }
    
    IMedComInventoryItemInterface* ItemInterface = Cast<IMedComInventoryItemInterface>(ItemObject);
    if (!ItemInterface || !ItemInterface->IsInitialized())
    {
        return false;
    }
    
    // Получаем runtime экземпляр через новый интерфейс
    OutInstance = ItemInterface->GetItemInstance();
    return OutInstance.IsValid();
}

bool USuspenseItemLibrary::CreatePickupDataFromUnified(const FSuspenseUnifiedItemData& ItemData, int32 Quantity, FMCPickupData& OutPickupData)
{
    // Создание pickup данных из unified структуры
    if (ItemData.ItemID.IsNone() || Quantity <= 0)
    {
        return false;
    }
    
    // Используем встроенный метод unified структуры
    OutPickupData = ItemData.ToPickupData(Quantity);
    return true;
}

bool USuspenseItemLibrary::CreateEquipmentDataFromUnified(const FSuspenseUnifiedItemData& ItemData, FMCEquipmentData& OutEquipmentData)
{
    // Создание equipment данных из unified структуры
    if (ItemData.ItemID.IsNone() || !ItemData.bIsEquippable)
    {
        return false;
    }
    
    // Используем встроенный метод unified структуры
    OutEquipmentData = ItemData.ToEquipmentData();
    return true;
}

//==================================================================
// Debug and Validation Methods - МЕТОДЫ ОТЛАДКИ И ВАЛИДАЦИИ
//==================================================================

bool USuspenseItemLibrary::ValidateUnifiedItemData(const FSuspenseUnifiedItemData& ItemData, TArray<FString>& OutErrors)
{
    // Валидация unified данных с детальными ошибками
    OutErrors.Empty();
    
    // Используем встроенный метод валидации
    TArray<FText> ValidationErrors = ItemData.GetValidationErrors();
    
    // Конвертируем FText ошибки в FString для Blueprint compatibility
    for (const FText& Error : ValidationErrors)
    {
        OutErrors.Add(Error.ToString());
    }
    
    return OutErrors.Num() == 0;
}

FString USuspenseItemLibrary::GetItemDebugInfo(const FSuspenseUnifiedItemData& ItemData)
{
    // Генерация подробной debug информации
    FString DebugInfo;
    
    DebugInfo += FString::Printf(TEXT("=== ITEM DEBUG INFO ===\n"));
    DebugInfo += FString::Printf(TEXT("ItemID: %s\n"), *ItemData.ItemID.ToString());
    DebugInfo += FString::Printf(TEXT("DisplayName: %s\n"), *ItemData.DisplayName.ToString());
    DebugInfo += FString::Printf(TEXT("ItemType: %s\n"), *ItemData.ItemType.ToString());
    DebugInfo += FString::Printf(TEXT("Rarity: %s\n"), *ItemData.Rarity.ToString());
    DebugInfo += FString::Printf(TEXT("GridSize: %dx%d\n"), ItemData.GridSize.X, ItemData.GridSize.Y);
    DebugInfo += FString::Printf(TEXT("MaxStackSize: %d\n"), ItemData.MaxStackSize);
    DebugInfo += FString::Printf(TEXT("Weight: %.2f\n"), ItemData.Weight);
    DebugInfo += FString::Printf(TEXT("BaseValue: %d\n"), ItemData.BaseValue);
    DebugInfo += FString::Printf(TEXT("IsEquippable: %s\n"), ItemData.bIsEquippable ? TEXT("Yes") : TEXT("No"));
    DebugInfo += FString::Printf(TEXT("IsWeapon: %s\n"), ItemData.bIsWeapon ? TEXT("Yes") : TEXT("No"));
    DebugInfo += FString::Printf(TEXT("IsArmor: %s\n"), ItemData.bIsArmor ? TEXT("Yes") : TEXT("No"));
    DebugInfo += FString::Printf(TEXT("IsAmmo: %s\n"), ItemData.bIsAmmo ? TEXT("Yes") : TEXT("No"));
    DebugInfo += FString::Printf(TEXT("IsConsumable: %s\n"), ItemData.bIsConsumable ? TEXT("Yes") : TEXT("No"));
    DebugInfo += FString::Printf(TEXT("CanDrop: %s\n"), ItemData.bCanDrop ? TEXT("Yes") : TEXT("No"));
    DebugInfo += FString::Printf(TEXT("CanTrade: %s\n"), ItemData.bCanTrade ? TEXT("Yes") : TEXT("No"));
    
    if (!ItemData.ItemTags.IsEmpty())
    {
        DebugInfo += FString::Printf(TEXT("AdditionalTags: %s\n"), *ItemData.ItemTags.ToString());
    }
    
    DebugInfo += FString::Printf(TEXT("======================"));
    
    return DebugInfo;
}

bool USuspenseItemLibrary::ValidateItemInstance(const FInventoryItemInstance& ItemInstance, const UObject* WorldContext, TArray<FString>& OutErrors)
{
    // Валидация runtime экземпляра с исправленной обработкой типов
    OutErrors.Empty();
    
    // Базовая валидация экземпляра
    if (!ItemInstance.IsValid())
    {
        OutErrors.Add(TEXT("Runtime экземпляр невалиден"));
        return false;
    }
    
    // Проверяем существование в DataTable
    USuspenseItemManager* ItemManager = GetValidatedItemManager(WorldContext);
    if (!ItemManager)
    {
        OutErrors.Add(TEXT("ItemManager недоступен для валидации"));
        return false;
    }
    
    FSuspenseUnifiedItemData UnifiedData;
    if (!ItemManager->GetUnifiedItemData(ItemInstance.ItemID, UnifiedData))
    {
        OutErrors.Add(FString::Printf(TEXT("Предмет '%s' не найден в DataTable"), *ItemInstance.ItemID.ToString()));
        return false;
    }
    
    // Проверяем количество
    if (ItemInstance.Quantity <= 0)
    {
        OutErrors.Add(TEXT("Некорректное количество"));
    }
    
    if (ItemInstance.Quantity > UnifiedData.MaxStackSize)
    {
        OutErrors.Add(FString::Printf(TEXT("Количество (%d) превышает максимальный размер стека (%d)"), 
            ItemInstance.Quantity, UnifiedData.MaxStackSize));
    }
    
    // Проверяем GUID
    if (!ItemInstance.InstanceID.IsValid())
    {
        OutErrors.Add(TEXT("Некорректный InstanceID GUID"));
    }
    
    // ИСПРАВЛЕНО: Корректная валидация runtime свойств с FName ключами
    for (const auto& PropertyPair : ItemInstance.RuntimeProperties)
    {
        // FName использует IsNone() вместо IsEmpty()
        if (PropertyPair.Key.IsNone())
        {
            OutErrors.Add(TEXT("Найдено runtime свойство с пустым именем"));
        }
        
        // Проверяем валидность числового значения
        if (!FMath::IsFinite(PropertyPair.Value))
        {
            OutErrors.Add(FString::Printf(TEXT("Некорректное значение для свойства '%s'"), *PropertyPair.Key.ToString()));
        }
    }
    
    return OutErrors.Num() == 0;
}

//==================================================================
// Internal Helper Methods - ВНУТРЕННИЕ ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ
//==================================================================

USuspenseItemManager* USuspenseItemLibrary::GetValidatedItemManager(const UObject* WorldContext)
{
    // Централизованная логика получения ItemManager с proper error handling
    if (!WorldContext)
    {
        UE_LOG(LogMedComItemLibrary, Warning, TEXT("GetValidatedItemManager: Отсутствует WorldContext"));
        return nullptr;
    }
    
    UWorld* World = WorldContext->GetWorld();
    if (!World)
    {
        UE_LOG(LogMedComItemLibrary, Warning, TEXT("GetValidatedItemManager: Не удалось получить World"));
        return nullptr;
    }
    
    UGameInstance* GameInstance = World->GetGameInstance();
    if (!GameInstance)
    {
        UE_LOG(LogMedComItemLibrary, Warning, TEXT("GetValidatedItemManager: Отсутствует GameInstance"));
        return nullptr;
    }
    
    USuspenseItemManager* ItemManager = GameInstance->GetSubsystem<USuspenseItemManager>();
    if (!ItemManager)
    {
        UE_LOG(LogMedComItemLibrary, Error, TEXT("GetValidatedItemManager: ItemManager subsystem не найден"));
        UE_LOG(LogMedComItemLibrary, Error, TEXT("Убедитесь что USuspenseItemManager зарегистрирован как subsystem"));
    }
    
    return ItemManager;
}

int32 USuspenseItemLibrary::GetRarityPriority(const FGameplayTag& RarityTag)
{
    // Преобразование rarity тегов в числовые значения для сортировки
    static const TMap<FString, int32> RarityPriorities = {
        {TEXT("Item.Rarity.Common"), 0},
        {TEXT("Item.Rarity.Uncommon"), 1},
        {TEXT("Item.Rarity.Rare"), 2},
        {TEXT("Item.Rarity.Epic"), 3},
        {TEXT("Item.Rarity.Legendary"), 4},
        {TEXT("Item.Rarity.Mythic"), 5}
    };
    
    if (const int32* Priority = RarityPriorities.Find(RarityTag.ToString()))
    {
        return *Priority;
    }
    
    return -1; // Неизвестная редкость получает низший приоритет
}

bool USuspenseItemLibrary::DoesTagMatch(const FGameplayTag& ItemTag, const FGameplayTag& FilterTag, bool bExactMatch)
{
    // Универсальная логика сравнения тегов
    if (!ItemTag.IsValid() || !FilterTag.IsValid())
    {
        return false;
    }
    
    if (bExactMatch)
    {
        return ItemTag.MatchesTagExact(FilterTag);
    }
    else
    {
        return ItemTag.MatchesTag(FilterTag); // Поддерживает иерархию
    }
}

void USuspenseItemLibrary::ClearRuntimeProperty(FInventoryItemInstance& ItemInstance, const FString& PropertyName)
{
    // Удаление runtime свойства с корректной обработкой типов
    if (!ItemInstance.IsValid() || PropertyName.IsEmpty())
    {
        UE_LOG(LogMedComItemLibrary, Warning, TEXT("ClearRuntimeProperty: Некорректные параметры"));
        return;
    }
    
    FName PropertyKey(*PropertyName);
    int32 RemovedCount = ItemInstance.RuntimeProperties.Remove(PropertyKey);
    
    if (RemovedCount > 0)
    {
        UE_LOG(LogMedComItemLibrary, VeryVerbose, TEXT("ClearRuntimeProperty: Удалено свойство %s.%s [%s]"), 
            *ItemInstance.ItemID.ToString(), *PropertyName, *ItemInstance.InstanceID.ToString().Left(8));
    }
    else
    {
        UE_LOG(LogMedComItemLibrary, VeryVerbose, TEXT("ClearRuntimeProperty: Свойство %s.%s не найдено [%s]"), 
            *ItemInstance.ItemID.ToString(), *PropertyName, *ItemInstance.InstanceID.ToString().Left(8));
    }
}

TArray<FString> USuspenseItemLibrary::GetAllRuntimePropertyNames(const FInventoryItemInstance& ItemInstance)
{
    // Получение всех имен runtime свойств для debugging и UI
    TArray<FString> PropertyNames;
    
    if (!ItemInstance.IsValid())
    {
        return PropertyNames;
    }
    
    // Конвертируем FName ключи в FString для Blueprint compatibility
    for (const auto& PropertyPair : ItemInstance.RuntimeProperties)
    {
        PropertyNames.Add(PropertyPair.Key.ToString());
    }
    
    return PropertyNames;
}

int32 USuspenseItemLibrary::GetRuntimePropertiesCount(const FInventoryItemInstance& ItemInstance)
{
    // Получение количества runtime свойств
    if (!ItemInstance.IsValid())
    {
        return 0;
    }
    
    return ItemInstance.RuntimeProperties.Num();
}

void USuspenseItemLibrary::ClearAllRuntimeProperties(FInventoryItemInstance& ItemInstance)
{
    // Очистка всех runtime свойств для переинициализации
    if (!ItemInstance.IsValid())
    {
        UE_LOG(LogMedComItemLibrary, Warning, TEXT("ClearAllRuntimeProperties: Некорректный экземпляр"));
        return;
    }
    
    int32 ClearedCount = ItemInstance.RuntimeProperties.Num();
    ItemInstance.RuntimeProperties.Empty();
    
    UE_LOG(LogMedComItemLibrary, VeryVerbose, TEXT("ClearAllRuntimeProperties: Очищено %d свойств для %s [%s]"), 
        ClearedCount, *ItemInstance.ItemID.ToString(), *ItemInstance.InstanceID.ToString().Left(8));
}

//==================================================================
// Convenience методы для часто используемых runtime свойств - ОБНОВЛЕННЫЕ
//==================================================================

float USuspenseItemLibrary::GetItemDurability(const FInventoryItemInstance& ItemInstance)
{
    // Получение текущей прочности через unified interface
    return GetRuntimeProperty(ItemInstance, TEXT("Durability"), 0.0f);
}

void USuspenseItemLibrary::SetItemDurability(FInventoryItemInstance& ItemInstance, float Durability)
{
    // Установка прочности с автоматическим клампингом к максимуму
    float MaxDurability = GetRuntimeProperty(ItemInstance, TEXT("MaxDurability"), 100.0f);
    float ClampedDurability = FMath::Clamp(Durability, 0.0f, MaxDurability);
    SetRuntimeProperty(ItemInstance, TEXT("Durability"), ClampedDurability);
    
    UE_LOG(LogMedComItemLibrary, VeryVerbose, TEXT("SetItemDurability: %s durability set to %.1f/%.1f [%s]"), 
        *ItemInstance.ItemID.ToString(), ClampedDurability, MaxDurability, *ItemInstance.InstanceID.ToString().Left(8));
}

float USuspenseItemLibrary::GetItemDurabilityPercent(const FInventoryItemInstance& ItemInstance)
{
    // Получение прочности в процентах от 0.0 до 1.0
    float MaxDurability = GetRuntimeProperty(ItemInstance, TEXT("MaxDurability"), 100.0f);
    if (MaxDurability <= 0.0f) 
    {
        return 1.0f; // Предмет не имеет системы прочности
    }
    
    float CurrentDurability = GetItemDurability(ItemInstance);
    return FMath::Clamp(CurrentDurability / MaxDurability, 0.0f, 1.0f);
}

int32 USuspenseItemLibrary::GetItemAmmo(const FInventoryItemInstance& ItemInstance)
{
    // Получение текущих патронов в оружии
    return FMath::RoundToInt(GetRuntimeProperty(ItemInstance, TEXT("Ammo"), 0.0f));
}

void USuspenseItemLibrary::SetItemAmmo(FInventoryItemInstance& ItemInstance, int32 AmmoCount)
{
    // Установка патронов с клампингом к максимуму
    int32 MaxAmmo = FMath::RoundToInt(GetRuntimeProperty(ItemInstance, TEXT("MaxAmmo"), 30.0f));
    int32 ClampedAmmo = FMath::Clamp(AmmoCount, 0, MaxAmmo);
    SetRuntimeProperty(ItemInstance, TEXT("Ammo"), static_cast<float>(ClampedAmmo));
    
    UE_LOG(LogMedComItemLibrary, VeryVerbose, TEXT("SetItemAmmo: %s ammo set to %d/%d [%s]"), 
        *ItemInstance.ItemID.ToString(), ClampedAmmo, MaxAmmo, *ItemInstance.InstanceID.ToString().Left(8));
}

bool USuspenseItemLibrary::IsItemOnCooldown(const FInventoryItemInstance& ItemInstance, float CurrentTime)
{
    // Проверка активного кулдауна
    float CooldownEndTime = GetRuntimeProperty(ItemInstance, TEXT("CooldownEnd"), 0.0f);
    return CurrentTime < CooldownEndTime;
}

void USuspenseItemLibrary::StartItemCooldown(FInventoryItemInstance& ItemInstance, float CurrentTime, float CooldownDuration)
{
    // Запуск кулдауна предмета
    float CooldownEndTime = CurrentTime + CooldownDuration;
    SetRuntimeProperty(ItemInstance, TEXT("CooldownEnd"), CooldownEndTime);
    
    UE_LOG(LogMedComItemLibrary, VeryVerbose, TEXT("StartItemCooldown: %s cooldown started for %.1fs [%s]"), 
        *ItemInstance.ItemID.ToString(), CooldownDuration, *ItemInstance.InstanceID.ToString().Left(8));
}

float USuspenseItemLibrary::GetRemainingCooldown(const FInventoryItemInstance& ItemInstance, float CurrentTime)
{
    // Получение оставшегося времени кулдауна
    float CooldownEndTime = GetRuntimeProperty(ItemInstance, TEXT("CooldownEnd"), 0.0f);
    return FMath::Max(0.0f, CooldownEndTime - CurrentTime);
}