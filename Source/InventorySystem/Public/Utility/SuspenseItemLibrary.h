// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Types/Loadout/MedComItemDataTable.h"
#include "Types/Inventory/InventoryTypes.h"
#include "GameplayTagContainer.h"
#include "SuspenseItemLibrary.generated.h"

// Forward declarations для чистого разделения модулей
class UMedComItemManager;
class UTexture2D;
class UObject;
struct FInventoryItemInstance;
struct FMedComUnifiedItemData;

/**
 * ПОЛНОСТЬЮ ОБНОВЛЕННАЯ Blueprint библиотека для работы с inventory system
 * 
 * АРХИТЕКТУРНЫЕ ПРИНЦИПЫ НОВОЙ ВЕРСИИ:
 * - Интеграция с DataTable как единым источником истины для статических данных
 * - Поддержка FInventoryItemInstance для runtime состояния предметов
 * - Централизованный доступ через UMedComItemManager subsystem
 * - Backward compatibility с legacy кодом там, где это возможно
 * - Thread-safe операции для multiplayer среды
 * - Расширенная поддержка runtime свойств (прочность, патроны, модификации)
 */
UCLASS()
class INVENTORYSYSTEM_API USuspenseItemLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
    
public:
    //==================================================================
    // Core DataTable Integration Methods - НОВЫЕ ОСНОВНЫЕ МЕТОДЫ
    //==================================================================
    
    /**
     * Получить unified данные предмета из DataTable по ID (ОСНОВНОЙ МЕТОД)
     * Это центральный метод новой архитектуры для доступа к статическим данным
     * @param WorldContext Контекст для доступа к ItemManager subsystem
     * @param ItemID Идентификатор предмета в DataTable
     * @param OutItemData Выходная unified структура данных
     * @return true если данные найдены и загружены
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|DataTable", CallInEditor)
    static bool GetUnifiedItemData(const UObject* WorldContext, FName ItemID, FMedComUnifiedItemData& OutItemData);
    
    /**
     * Получить ItemManager subsystem для прямого доступа к DataTable
     * @param WorldContext Контекст для доступа к subsystem
     * @return Экземпляр ItemManager или nullptr если недоступен
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|DataTable", meta = (WorldContext = "WorldContext"))
    static UMedComItemManager* GetItemManager(const UObject* WorldContext);
    
    /**
     * Проверить существование предмета в DataTable
     * @param WorldContext Контекст для доступа к ItemManager
     * @param ItemID Идентификатор для проверки
     * @return true если предмет существует в DataTable
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|DataTable", meta = (WorldContext = "WorldContext"))
    static bool DoesItemExistInDataTable(const UObject* WorldContext, FName ItemID);
    
    //==================================================================
    // Updated Legacy Support Methods - ОБНОВЛЕННЫЕ LEGACY МЕТОДЫ
    //==================================================================
    
    /**
     * ОБНОВЛЕНО: Получить название предмета из unified данных
     * @param ItemData Unified данные предмета из DataTable
     * @return Локализованное название предмета
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Items")
    static FText GetItemName(const FMedComUnifiedItemData& ItemData);
    
    /**
     * ОБНОВЛЕНО: Получить описание предмета из unified данных
     * @param ItemData Unified данные предмета
     * @return Локализованное описание предмета
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Items")
    static FText GetItemDescription(const FMedComUnifiedItemData& ItemData);
    
    /**
     * ОБНОВЛЕНО: Получить иконку предмета из unified данных
     * @param ItemData Unified данные предмета
     * @return Текстура иконки предмета
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Items")
    static UTexture2D* GetItemIcon(const FMedComUnifiedItemData& ItemData);
    
    //==================================================================
    // Enhanced Runtime Instance Methods - НОВЫЕ МЕТОДЫ ДЛЯ RUNTIME
    //==================================================================
    
    /**
     * Создать runtime экземпляр предмета из DataTable ID
     * @param WorldContext Контекст для доступа к ItemManager
     * @param ItemID Идентификатор предмета в DataTable
     * @param Quantity Количество предметов в стеке
     * @param OutInstance Выходной runtime экземпляр
     * @return true если экземпляр успешно создан
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Runtime", meta = (WorldContext = "WorldContext"))
    static bool CreateItemInstance(const UObject* WorldContext, FName ItemID, int32 Quantity, FInventoryItemInstance& OutInstance);
    
    /**
     * Получить unified данные из runtime экземпляра
     * @param WorldContext Контекст для доступа к ItemManager
     * @param ItemInstance Runtime экземпляр предмета
     * @param OutItemData Выходные unified данные
     * @return true если данные получены успешно
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Runtime", meta = (WorldContext = "WorldContext"))
    static bool GetUnifiedDataFromInstance(const UObject* WorldContext, const FInventoryItemInstance& ItemInstance, FMedComUnifiedItemData& OutItemData);
    
    /**
     * Получить runtime свойство из экземпляра предмета
     * @param ItemInstance Runtime экземпляр предмета
     * @param PropertyName Название свойства (например, "Durability", "Ammo")
     * @param DefaultValue Значение по умолчанию если свойство не найдено
     * @return Текущее значение свойства или значение по умолчанию
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Runtime")
    static float GetRuntimeProperty(const FInventoryItemInstance& ItemInstance, const FString& PropertyName, float DefaultValue = 0.0f);
    
    /**
     * Установить runtime свойство в экземпляре предмета
     * @param ItemInstance Runtime экземпляр предмета (передается по ссылке)
     * @param PropertyName Название свойства для установки
     * @param Value Новое значение свойства
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Runtime")
    static void SetRuntimeProperty(UPARAM(ref) FInventoryItemInstance& ItemInstance, const FString& PropertyName, float Value);
    
    /**
     * Проверить наличие runtime свойства
     * @param ItemInstance Runtime экземпляр предмета
     * @param PropertyName Название свойства для проверки
     * @return true если свойство существует
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Runtime")
    static bool HasRuntimeProperty(const FInventoryItemInstance& ItemInstance, const FString& PropertyName);
    
    //==================================================================
    // Enhanced Display and Formatting Methods - УЛУЧШЕННЫЕ МЕТОДЫ ОТОБРАЖЕНИЯ
    //==================================================================
    
    /**
     * ОБНОВЛЕНО: Форматировать количество предметов с учетом unified данных
     * @param ItemData Unified данные для получения MaxStackSize
     * @param Quantity Текущее количество для отображения
     * @return Форматированный текст количества
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Display")
    static FText FormatItemQuantity(const FMedComUnifiedItemData& ItemData, int32 Quantity);
    
    /**
     * Форматировать количество из runtime экземпляра
     * @param WorldContext Контекст для доступа к ItemManager
     * @param ItemInstance Runtime экземпляр предмета
     * @return Форматированный текст количества
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Display", meta = (WorldContext = "WorldContext"))
    static FText FormatItemQuantityFromInstance(const UObject* WorldContext, const FInventoryItemInstance& ItemInstance);
    
    /**
     * Форматировать вес предмета из unified данных
     * @param ItemData Unified данные предмета
     * @param Quantity Количество для расчета общего веса
     * @param bIncludeUnit Включать ли единицы измерения
     * @return Форматированный текст веса
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Display")
    static FText FormatItemWeight(const FMedComUnifiedItemData& ItemData, int32 Quantity = 1, bool bIncludeUnit = true);
    
    /**
     * Получить цвет редкости предмета для UI
     * @param ItemData Unified данные предмета
     * @return Цвет для отображения редкости в UI
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Display")
    static FLinearColor GetRarityColor(const FMedComUnifiedItemData& ItemData);
    
    /**
     * Форматировать прочность предмета
     * @param ItemInstance Runtime экземпляр предмета
     * @param bAsPercentage Отображать как процент или абсолютное значение
     * @return Форматированный текст прочности
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Display")
    static FText FormatItemDurability(const FInventoryItemInstance& ItemInstance, bool bAsPercentage = true);
    
    //==================================================================
    // Enhanced Search and Filtering Methods - УЛУЧШЕННЫЕ МЕТОДЫ ПОИСКА
    //==================================================================
    
    /**
     * ОБНОВЛЕНО: Фильтровать unified данные по типу с поддержкой иерархии тегов
     * @param ItemDataArray Массив unified данных для фильтрации
     * @param TypeTag Тег типа для фильтрации (поддерживает иерархию)
     * @param bExactMatch Точное совпадение или иерархическое
     * @return Отфильтрованный массив
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Filtering")
    static TArray<FMedComUnifiedItemData> FilterItemsByType(const TArray<FMedComUnifiedItemData>& ItemDataArray, const FGameplayTag& TypeTag, bool bExactMatch = false);
    
    /**
     * Фильтровать по редкости
     * @param ItemDataArray Массив unified данных
     * @param RarityTag Тег редкости для фильтрации
     * @return Предметы указанной редкости
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Filtering")
    static TArray<FMedComUnifiedItemData> FilterItemsByRarity(const TArray<FMedComUnifiedItemData>& ItemDataArray, const FGameplayTag& RarityTag);
    
    /**
     * Фильтровать по multiple тегам с логическими операторами
     * @param ItemDataArray Массив unified данных
     * @param FilterTags Теги для фильтрации
     * @param bRequireAll true для AND логики, false для OR логики
     * @return Отфильтрованный массив
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Filtering")
    static TArray<FMedComUnifiedItemData> FilterItemsByTags(const TArray<FMedComUnifiedItemData>& ItemDataArray, const FGameplayTagContainer& FilterTags, bool bRequireAll = false);
    
    /**
     * ОБНОВЛЕНО: Расширенный поиск в unified данных
     * @param ItemDataArray Массив для поиска
     * @param SearchText Текст для поиска
     * @param bSearchDescription Искать в описании
     * @param bSearchTags Искать в тегах предмета
     * @return Массив найденных предметов
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Search")
    static TArray<FMedComUnifiedItemData> SearchItems(const TArray<FMedComUnifiedItemData>& ItemDataArray, const FString& SearchText, bool bSearchDescription = true, bool bSearchTags = false);
    
    //==================================================================
    // Enhanced Sorting Methods - УЛУЧШЕННЫЕ МЕТОДЫ СОРТИРОВКИ
    //==================================================================
    
    /**
     * ОБНОВЛЕНО: Сортировать unified данные по названию
     * @param ItemDataArray Массив для сортировки
     * @param bAscending Направление сортировки
     * @return Отсортированный массив
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Sorting")
    static TArray<FMedComUnifiedItemData> SortItemsByName(const TArray<FMedComUnifiedItemData>& ItemDataArray, bool bAscending = true);
    
    /**
     * Сортировать по весу
     * @param ItemDataArray Массив unified данных
     * @param bAscending Направление сортировки
     * @return Отсортированный массив
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Sorting")
    static TArray<FMedComUnifiedItemData> SortItemsByWeight(const TArray<FMedComUnifiedItemData>& ItemDataArray, bool bAscending = true);
    
    /**
     * Сортировать по стоимости
     * @param ItemDataArray Массив unified данных
     * @param bAscending Направление сортировки
     * @return Отсортированный массив
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Sorting")
    static TArray<FMedComUnifiedItemData> SortItemsByValue(const TArray<FMedComUnifiedItemData>& ItemDataArray, bool bAscending = true);
    
    /**
     * Сортировать по редкости (с учетом иерархии)
     * @param ItemDataArray Массив unified данных
     * @param bAscending Направление сортировки
     * @return Отсортированный массив
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Sorting")
    static TArray<FMedComUnifiedItemData> SortItemsByRarity(const TArray<FMedComUnifiedItemData>& ItemDataArray, bool bAscending = true);
    
    //==================================================================
    // Enhanced Weight and Calculation Methods - УЛУЧШЕННЫЕ РАСЧЕТНЫЕ МЕТОДЫ
    //==================================================================
    
    /**
     * ОБНОВЛЕНО: Получить общий вес unified предметов
     * @param ItemDataArray Массив unified данных
     * @param QuantityArray Массив количеств (должен соответствовать ItemDataArray)
     * @return Общий вес всех предметов
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Calculations")
    static float GetTotalItemsWeight(const TArray<FMedComUnifiedItemData>& ItemDataArray, const TArray<int32>& QuantityArray);
    
    /**
     * Получить общий вес из runtime экземпляров
     * @param ItemInstances Массив runtime экземпляров
     * @param WorldContext Контекст для доступа к ItemManager
     * @return Общий вес всех экземпляров
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Calculations", meta = (WorldContext = "WorldContext"))
    static float GetTotalInstancesWeight(const TArray<FInventoryItemInstance>& ItemInstances, const UObject* WorldContext);
    
    /**
     * Получить общую стоимость предметов
     * @param ItemDataArray Массив unified данных
     * @param QuantityArray Массив количеств
     * @return Общая стоимость всех предметов
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Calculations")
    static int32 GetTotalItemsValue(const TArray<FMedComUnifiedItemData>& ItemDataArray, const TArray<int32>& QuantityArray);
    
    //==================================================================
    // Grid and UI Helper Methods - МЕТОДЫ ДЛЯ СЕТКИ И UI
    //==================================================================
    
    /**
     * Получить UI позицию предмета в сетке
     * @param GridX X координата в сетке
     * @param GridY Y координата в сетке
     * @param CellSize Размер ячейки в пикселях
     * @param CellSpacing Отступ между ячейками
     * @return UI позиция для виджета
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|UI")
    static FVector2D GetItemUIPosition(int32 GridX, int32 GridY, const FVector2D& CellSize, float CellSpacing = 2.0f);
    
    /**
     * Получить UI размер предмета с учетом поворота
     * @param ItemData Unified данные предмета
     * @param bIsRotated Повернут ли предмет на 90 градусов
     * @param CellSize Размер ячейки в пикселях
     * @param CellSpacing Отступ между ячейками
     * @return UI размер для виджета
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|UI")
    static FVector2D GetItemUISize(const FMedComUnifiedItemData& ItemData, bool bIsRotated, const FVector2D& CellSize, float CellSpacing = 2.0f);
    
    /**
     * Получить координаты из линейного индекса
     * @param LinearIndex Линейный индекс в сетке
     * @param GridWidth Ширина сетки
     * @param OutX Выходная X координата
     * @param OutY Выходная Y координата
     * @return true если индекс валиден
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Grid")
    static bool GetCoordinatesFromIndex(int32 LinearIndex, int32 GridWidth, int32& OutX, int32& OutY);
    
    /**
     * Получить линейный индекс из координат
     * @param X X координата
     * @param Y Y координата
     * @param GridWidth Ширина сетки
     * @return Линейный индекс
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Grid")
    static int32 GetIndexFromCoordinates(int32 X, int32 Y, int32 GridWidth);
    
    /**
     * Получить все занятые слоты для предмета
     * @param ItemData Unified данные предмета
     * @param AnchorIndex Якорный индекс размещения
     * @param bIsRotated Повернут ли предмет
     * @param GridWidth Ширина сетки инвентаря
     * @return Массив всех занятых индексов
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Grid")
    static TArray<int32> GetOccupiedSlots(const FMedComUnifiedItemData& ItemData, int32 AnchorIndex, bool bIsRotated, int32 GridWidth);
    
    //==================================================================
    // Item Type and Classification Helpers - МЕТОДЫ КЛАССИФИКАЦИИ
    //==================================================================
    
    /**
     * Проверить является ли предмет оружием
     * @param ItemData Unified данные предмета
     * @return true если предмет является оружием
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Classification")
    static bool IsWeapon(const FMedComUnifiedItemData& ItemData);
    
    /**
     * Проверить является ли предмет броней
     * @param ItemData Unified данные предмета
     * @return true если предмет является броней
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Classification")
    static bool IsArmor(const FMedComUnifiedItemData& ItemData);
    
    /**
     * Проверить является ли предмет боеприпасами
     * @param ItemData Unified данные предмета
     * @return true если предмет является боеприпасами
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Classification")
    static bool IsAmmo(const FMedComUnifiedItemData& ItemData);
    
    /**
     * Проверить является ли предмет расходным материалом
     * @param ItemData Unified данные предмета
     * @return true если предмет является расходным
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Classification")
    static bool IsConsumable(const FMedComUnifiedItemData& ItemData);
    
    /**
     * Проверить является ли предмет экипируемым
     * @param ItemData Unified данные предмета
     * @return true если предмет можно экипировать
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Classification")
    static bool IsEquippable(const FMedComUnifiedItemData& ItemData);
    
    /**
     * Проверить может ли предмет быть сложен в стеки
     * @param ItemData Unified данные предмета
     * @return true если предмет стакается
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Classification")
    static bool IsStackable(const FMedComUnifiedItemData& ItemData);
    
    //==================================================================
    // Conversion and Compatibility Methods - МЕТОДЫ КОНВЕРСИИ
    //==================================================================
    
    /**
     * ОБНОВЛЕНО: Получить данные предмета из объекта через интерфейс
     * Теперь возвращает unified данные вместо legacy структуры
     * @param ItemObject Объект предмета с интерфейсом
     * @param OutItemData Выходные unified данные
     * @return true если конверсия успешна
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Conversion")
    static bool GetUnifiedDataFromObject(UObject* ItemObject, FMedComUnifiedItemData& OutItemData);
    
    /**
     * Получить runtime экземпляр из объекта предмета
     * @param ItemObject Объект предмета с интерфейсом
     * @param OutInstance Выходной runtime экземпляр
     * @return true если конверсия успешна
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Conversion")
    static bool GetInstanceFromObject(UObject* ItemObject, FInventoryItemInstance& OutInstance);
    
    /**
     * Создать pickup данные из unified данных
     * @param ItemData Unified данные предмета
     * @param Quantity Количество для pickup
     * @param OutPickupData Выходные данные для spawn pickup
     * @return true если данные созданы успешно
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Conversion")
    static bool CreatePickupDataFromUnified(const FMedComUnifiedItemData& ItemData, int32 Quantity, FMCPickupData& OutPickupData);
    
    /**
     * Создать equipment данные из unified данных
     * @param ItemData Unified данные предмета
     * @param OutEquipmentData Выходные данные для экипировки
     * @return true если данные созданы успешно (только для экипируемых предметов)
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Conversion")
    static bool CreateEquipmentDataFromUnified(const FMedComUnifiedItemData& ItemData, FMCEquipmentData& OutEquipmentData);
    
    //==================================================================
    // Debug and Validation Methods - МЕТОДЫ ОТЛАДКИ И ВАЛИДАЦИИ
    //==================================================================
    
    /**
     * Валидировать unified данные предмета
     * @param ItemData Unified данные для проверки
     * @param OutErrors Массив найденных ошибок
     * @return true если данные валидны
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Debug")
    static bool ValidateUnifiedItemData(const FMedComUnifiedItemData& ItemData, TArray<FString>& OutErrors);
    
    /**
     * Получить debug информацию о предмете
     * @param ItemData Unified данные предмета
     * @return Строка с детальной debug информацией
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Debug")
    static FString GetItemDebugInfo(const FMedComUnifiedItemData& ItemData);
    
    /**
     * Валидировать runtime экземпляр предмета
     * @param ItemInstance Runtime экземпляр для проверки
     * @param WorldContext Контекст для доступа к ItemManager
     * @param OutErrors Массив найденных ошибок
     * @return true если экземпляр валиден
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Debug", meta = (WorldContext = "WorldContext"))
    static bool ValidateItemInstance(const FInventoryItemInstance& ItemInstance, const UObject* WorldContext, TArray<FString>& OutErrors);
//==================================================================
// Enhanced Runtime Properties Management - ДОПОЛНИТЕЛЬНЫЕ МЕТОДЫ
//==================================================================

/**
 * Удалить runtime свойство из экземпляра предмета
 * @param ItemInstance Runtime экземпляр предмета (передается по ссылке)
 * @param PropertyName Название свойства для удаления
 */
UFUNCTION(BlueprintCallable, Category = "Inventory|Runtime")
static void ClearRuntimeProperty(UPARAM(ref) FInventoryItemInstance& ItemInstance, const FString& PropertyName);

/**
 * Получить все имена runtime свойств
 * @param ItemInstance Runtime экземпляр предмета
 * @return Массив имен всех runtime свойств
 */
UFUNCTION(BlueprintPure, Category = "Inventory|Runtime")
static TArray<FString> GetAllRuntimePropertyNames(const FInventoryItemInstance& ItemInstance);

/**
 * Получить количество runtime свойств
 * @param ItemInstance Runtime экземпляр предмета
 * @return Количество runtime свойств
 */
UFUNCTION(BlueprintPure, Category = "Inventory|Runtime")
static int32 GetRuntimePropertiesCount(const FInventoryItemInstance& ItemInstance);

/**
 * Очистить все runtime свойства
 * @param ItemInstance Runtime экземпляр предмета (передается по ссылке)
 */
UFUNCTION(BlueprintCallable, Category = "Inventory|Runtime")
static void ClearAllRuntimeProperties(UPARAM(ref) FInventoryItemInstance& ItemInstance);

//==================================================================
// Convenience Methods for Common Properties - CONVENIENCE МЕТОДЫ
//==================================================================

/**
 * Получить прочность предмета
 * @param ItemInstance Runtime экземпляр предмета
 * @return Текущая прочность
 */
UFUNCTION(BlueprintPure, Category = "Inventory|Properties")
static float GetItemDurability(const FInventoryItemInstance& ItemInstance);

/**
 * Установить прочность предмета с автоматическим клампингом
 * @param ItemInstance Runtime экземпляр предмета (передается по ссылке)
 * @param Durability Новое значение прочности
 */
UFUNCTION(BlueprintCallable, Category = "Inventory|Properties")
static void SetItemDurability(UPARAM(ref) FInventoryItemInstance& ItemInstance, float Durability);

/**
 * Получить прочность предмета в процентах
 * @param ItemInstance Runtime экземпляр предмета
 * @return Прочность от 0.0 до 1.0
 */
UFUNCTION(BlueprintPure, Category = "Inventory|Properties")
static float GetItemDurabilityPercent(const FInventoryItemInstance& ItemInstance);

/**
 * Получить количество патронов в оружии
 * @param ItemInstance Runtime экземпляр предмета
 * @return Текущее количество патронов
 */
UFUNCTION(BlueprintPure, Category = "Inventory|Properties")
static int32 GetItemAmmo(const FInventoryItemInstance& ItemInstance);

/**
 * Установить количество патронов с клампингом к максимуму
 * @param ItemInstance Runtime экземпляр предмета (передается по ссылке)
 * @param AmmoCount Новое количество патронов
 */
UFUNCTION(BlueprintCallable, Category = "Inventory|Properties")
static void SetItemAmmo(UPARAM(ref) FInventoryItemInstance& ItemInstance, int32 AmmoCount);

/**
 * Проверить активен ли кулдаун предмета
 * @param ItemInstance Runtime экземпляр предмета
 * @param CurrentTime Текущее время
 * @return true если кулдаун активен
 */
UFUNCTION(BlueprintPure, Category = "Inventory|Properties")
static bool IsItemOnCooldown(const FInventoryItemInstance& ItemInstance, float CurrentTime);

/**
 * Запустить кулдаун предмета
 * @param ItemInstance Runtime экземпляр предмета (передается по ссылке)
 * @param CurrentTime Текущее время
 * @param CooldownDuration Длительность кулдауна в секундах
 */
UFUNCTION(BlueprintCallable, Category = "Inventory|Properties")
static void StartItemCooldown(UPARAM(ref) FInventoryItemInstance& ItemInstance, float CurrentTime, float CooldownDuration);

/**
 * Получить оставшееся время кулдауна
 * @param ItemInstance Runtime экземпляр предмета
 * @param CurrentTime Текущее время
 * @return Оставшееся время в секундах
 */
UFUNCTION(BlueprintPure, Category = "Inventory|Properties")
static float GetRemainingCooldown(const FInventoryItemInstance& ItemInstance, float CurrentTime);
private:
    //==================================================================
    // Internal Helper Methods - ВНУТРЕННИЕ ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ
    //==================================================================
    
    /**
     * Внутренний helper для безопасного получения ItemManager
     * Централизованная логика с proper error handling
     */
    static UMedComItemManager* GetValidatedItemManager(const UObject* WorldContext);
    
    /**
     * Внутренний helper для расчета rarity priority для сортировки
     * Преобразует rarity теги в числовые значения для comparison
     */
    static int32 GetRarityPriority(const FGameplayTag& RarityTag);
    
    /**
     * Внутренний helper для проверки совместимости тегов
     * Поддерживает как точное совпадение, так и иерархическое
     */
    static bool DoesTagMatch(const FGameplayTag& ItemTag, const FGameplayTag& FilterTag, bool bExactMatch);
};