// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Types/Loadout/SuspenseItemDataTable.h"
#include "Types/Loadout/SuspenseLoadoutSettings.h"
#include "GameplayTagContainer.h"
#include "Operations/SuspenseInventoryResult.h"
#include "SuspenseInventoryLibrary.generated.h"

// Forward declarations для чистого разделения модулей
class USuspenseItemManager;
class UTexture2D;
class UWorld;
class AActor;

/**
 * Modern inventory function library for DataTable-based architecture
 * 
 * АРХИТЕКТУРНЫЕ ПРИНЦИПЫ:
 * Эта библиотека полностью интегрирована с новой DataTable архитектурой и обеспечивает:
 * 
 * - Единый источник истины через DataTable
 * - Разделение статических данных (FSuspenseUnifiedItemData) и runtime состояния (FSuspenseInventoryItemInstance)
 * - Thread-safe операции для multiplayer среды
 * - Производительные методы с кэшированием
 * - Comprehensive validation и error handling
 * 
 * МИГРАЦИЯ ОТ LEGACY:
 * Все методы обновлены для использования FSuspenseUnifiedItemData вместо устаревшей FMCInventoryItemData.
 * Runtime данные теперь управляются через FSuspenseInventoryItemInstance с расширенной системой свойств.
 */
UCLASS(meta = (BlueprintThreadSafe))
class INVENTORYSYSTEM_API USuspenseInventoryLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    //==================================================================
    // Core Item Creation and Management (DataTable-Based)
    //==================================================================
    
    /**
     * Создает runtime экземпляр предмета из DataTable ID
     * Это основной метод создания предметов в новой архитектуре
     * 
     * @param ItemID Идентификатор предмета из DataTable
     * @param Quantity Количество предметов в стеке
     * @param WorldContext Контекст для получения ItemManager
     * @return Валидный runtime экземпляр или пустой при ошибке
     */
 UFUNCTION(BlueprintCallable, Category = "Inventory|Item Creation", CallInEditor)
    static FSuspenseInventoryItemInstance CreateItemInstance(const FName& ItemID, int32 Quantity, const UObject* WorldContext);
    
    /**
     * Создает множественные экземпляры предметов из spawn data
     * Эффективный batch creation для loadouts и loot generation
     *
     * @param SpawnDataArray Массив конфигураций для создания
     * @param WorldContext Контекст для получения ItemManager
     * @param OutInstances Выходной массив созданных экземпляров
     * @return Количество успешно созданных экземпляров
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Item Creation")
    static int32 CreateItemInstancesFromSpawnData(const TArray<FSuspensePickupSpawnData>& SpawnDataArray,
                                                  const UObject* WorldContext,
                                                  TArray<FSuspenseInventoryItemInstance>& OutInstances);
    
    /**
     * Получает unified данные предмета из DataTable
     * Основной метод доступа к статическим свойствам предметов
     * 
     * @param ItemID Идентификатор предмета
     * @param WorldContext Контекст для получения ItemManager
     * @param OutItemData Выходные unified данные
     * @return true если данные найдены в DataTable
     */
 UFUNCTION(BlueprintCallable, Category = "Inventory|Item Data", CallInEditor)
    static bool GetUnifiedItemData(const FName& ItemID, const UObject* WorldContext, FSuspenseUnifiedItemData& OutItemData);
    
    /**
     * Создает предмет в мире для pickup системы
     * Интегрирован с новой архитектурой для консистентности данных
     * 
     * @param ItemInstance Runtime экземпляр для создания
     * @param World Мир для создания предмета
     * @param Transform Трансформация для размещения
     * @return Созданный актор предмета или nullptr при ошибке
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|World Interaction")
    static AActor* SpawnItemInWorld(const FSuspenseInventoryItemInstance& ItemInstance,
                                   UWorld* World,
                                   const FTransform& Transform);
    
    //==================================================================
    // Enhanced Item Validation and Analysis
    //==================================================================
    
    /**
     * Проверяет валидность runtime экземпляра предмета
     * Включает проверку существования в DataTable и integrity
     * 
     * @param ItemInstance Runtime экземпляр для проверки
     * @param WorldContext Контекст для получения ItemManager
     * @param OutErrors Детальные ошибки валидации
     * @return true если экземпляр полностью валиден
     */
 UFUNCTION(BlueprintCallable, Category = "Inventory|Validation", CallInEditor)
    static bool ValidateItemInstance(const FSuspenseInventoryItemInstance& ItemInstance,
                                   const UObject* WorldContext,
                                   TArray<FString>& OutErrors);
    
    /**
     * Проверяет существование предмета в DataTable
     * Quick validation без создания экземпляра
     * 
     * @param ItemID Идентификатор для проверки
     * @param WorldContext Контекст для получения ItemManager
     * @return true если предмет существует в DataTable
     */
 UFUNCTION(BlueprintPure, Category = "Inventory|Validation", CallInEditor)
    static bool IsValidItemID(const FName& ItemID, const UObject* WorldContext);
    
    /**
     * Проверяет совместимость количества с максимальным размером стека
     * 
     * @param ItemID Идентификатор предмета
     * @param Quantity Проверяемое количество
     * @param WorldContext Контекст для получения данных
     * @return true если количество допустимо
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Validation")
    static bool IsValidQuantityForItem(const FName& ItemID, int32 Quantity, const UObject* WorldContext);
    
    //==================================================================
    // Advanced Grid and Placement Operations
    //==================================================================
    
    /**
     * Конвертирует linear index в grid coordinates с error checking
     * Обновленная версия с improved validation
     * 
     * @param Index Линейный индекс ячейки
     * @param GridWidth Ширина сетки
     * @param OutX Выходная X координата
     * @param OutY Выходная Y координата
     * @return true если конвертация успешна и индекс валиден
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Grid", meta = (CompactNodeTitle = "Index->XY"))
    static bool IndexToGridCoords(int32 Index, int32 GridWidth, int32& OutX, int32& OutY);
    
    /**
     * Конвертирует grid coordinates в linear index с boundary checking
     * 
     * @param X X координата в сетке
     * @param Y Y координата в сетке
     * @param GridWidth Ширина сетки
     * @param GridHeight Высота сетки (для boundary checking)
     * @return Линейный индекс или INDEX_NONE при invalid coordinates
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Grid", meta = (CompactNodeTitle = "XY->Index"))
    static int32 GridCoordsToIndex(int32 X, int32 Y, int32 GridWidth, int32 GridHeight = -1);
    
    /**
     * Проверяет размещение предмета в сетке с rotation support
     * Интегрирован с DataTable для получения размеров предметов
     * 
     * @param ItemID Идентификатор предмета
     * @param AnchorX X координата якоря размещения
     * @param AnchorY Y координата якоря размещения
     * @param GridWidth Ширина сетки
     * @param GridHeight Высота сетки
     * @param bIsRotated Учитывать поворот предмета
     * @param WorldContext Контекст для получения размеров из DataTable
     * @return true если предмет помещается в указанное место
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Grid")
    static bool CanItemFitAtPosition(const FName& ItemID, 
                                   int32 AnchorX, int32 AnchorY,
                                   int32 GridWidth, int32 GridHeight,
                                   bool bIsRotated,
                                   const UObject* WorldContext);
    
    /**
     * Находит оптимальное свободное место для предмета
     * Использует intelligent algorithms для best fit positioning
     * 
     * @param ItemID Идентификатор размещаемого предмета
     * @param GridWidth Ширина сетки
     * @param GridHeight Высота сетки
     * @param OccupiedSlots Массив занятых слотов
     * @param bAllowRotation Разрешить поворот для лучшего размещения
     * @param WorldContext Контекст для получения размеров
     * @param OutRotated Будет ли предмет повернут в найденной позиции
     * @return Индекс якорного слота или INDEX_NONE если места нет
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Grid")
    static int32 FindOptimalPlacementForItem(const FName& ItemID,
                                           int32 GridWidth, int32 GridHeight,
                                           const TArray<int32>& OccupiedSlots,
                                           bool bAllowRotation,
                                           const UObject* WorldContext,
                                           bool& OutRotated);
    
    /**
     * Получает все слоты, занимаемые предметом
     * Учитывает размер из DataTable и rotation
     * 
     * @param ItemID Идентификатор предмета
     * @param AnchorIndex Индекс якорного слота
     * @param GridWidth Ширина сетки
     * @param bIsRotated Повернут ли предмет
     * @param WorldContext Контекст для получения размеров
     * @return Массив всех занимаемых слотов
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Grid")
    static TArray<int32> GetOccupiedSlots(const FName& ItemID,
                                        int32 AnchorIndex,
                                        int32 GridWidth,
                                        bool bIsRotated,
                                        const UObject* WorldContext);
    
    /**
     * Рассчитывает размер предмета с учетом поворота
     * Получает данные из DataTable для точности
     * 
     * @param ItemID Идентификатор предмета
     * @param bIsRotated Повернут ли предмет
     * @param WorldContext Контекст для получения данных
     * @return Эффективный размер с учетом поворота
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Grid")
    static FIntPoint GetEffectiveItemSize(const FName& ItemID, bool bIsRotated, const UObject* WorldContext);
    
    //==================================================================
    // Enhanced Weight and Resource Management
    //==================================================================
    
    /**
     * Рассчитывает общий вес массива runtime экземпляров
     * Учитывает quantity и получает weight из DataTable
     * 
     * @param ItemInstances Массив runtime экземпляров
     * @param WorldContext Контекст для получения весов из DataTable
     * @return Общий вес всех предметов
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Weight")
    static float CalculateTotalWeightFromInstances(const TArray<FSuspenseInventoryItemInstance>& ItemInstances,
                                                  const UObject* WorldContext);
    
    /**
     * Проверяет возможность добавления предметов по весу
     * Enhanced версия с support для runtime экземпляров
     * 
     * @param CurrentWeight Текущий вес инвентаря
     * @param ItemInstances Добавляемые runtime экземпляры
     * @param MaxWeight Максимальный допустимый вес
     * @param WorldContext Контекст для получения весов
     * @return true если добавление не превысит лимит веса
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Weight")
    static bool CanAddItemsByWeight(float CurrentWeight,
                                  const TArray<FSuspenseInventoryItemInstance>& ItemInstances,
                                  float MaxWeight,
                                  const UObject* WorldContext);
    
    /**
     * Получает вес одного предмета из DataTable
     * 
     * @param ItemID Идентификатор предмета
     * @param WorldContext Контекст для получения ItemManager
     * @return Вес одной единицы предмета
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Weight")
    static float GetItemWeight(const FName& ItemID, const UObject* WorldContext);
    
    //==================================================================
    // Enhanced Tag System and Compatibility
    //==================================================================
    
    /**
     * Проверяет совместимость типа предмета и слота оборудования
     * Использует hierarchical tag matching для гибкости
     * 
     * @param ItemID Идентификатор предмета
     * @param SlotType Тег типа слота оборудования
     * @param WorldContext Контекст для получения типа предмета
     * @return true если предмет совместим со слотом
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Compatibility")
    static bool IsItemCompatibleWithSlot(const FName& ItemID, 
                                       const FGameplayTag& SlotType, 
                                       const UObject* WorldContext);
    
    /**
     * Проверяет разрешенность типа предмета в инвентаре
     * Supports complex tag hierarchies и restrictions
     * 
     * @param ItemID Идентификатор предмета
     * @param AllowedTypes Контейнер разрешенных типов
     * @param WorldContext Контекст для получения типа предмета
     * @return true если тип предмета разрешен
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Compatibility")
    static bool IsItemTypeAllowed(const FName& ItemID, 
                                const FGameplayTagContainer& AllowedTypes, 
                                const UObject* WorldContext);
    
    /**
     * Получает primary тип предмета из DataTable
     * 
     * @param ItemID Идентификатор предмета
     * @param WorldContext Контекст для получения данных
     * @return Primary gameplay tag типа предмета
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Item Properties")
    static FGameplayTag GetItemType(const FName& ItemID, const UObject* WorldContext);
    
    /**
     * Получает все теги предмета включая type и additional tags
     * 
     * @param ItemID Идентификатор предмета
     * @param WorldContext Контекст для получения данных
     * @return Полный контейнер тегов предмета
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Item Properties")
    static FGameplayTagContainer GetItemTags(const FName& ItemID, const UObject* WorldContext);
    
    //==================================================================
    // Enhanced Stacking and Quantity Management
    //==================================================================
    
    /**
     * Проверяет возможность объединения двух runtime экземпляров
     * Учитывает ID, runtime properties и stack limits
     * 
     * @param Instance1 Первый runtime экземпляр
     * @param Instance2 Второй runtime экземпляр
     * @param WorldContext Контекст для получения stack limits
     * @return true если экземпляры можно объединить
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Stacking")
    static bool CanStackInstances(const FSuspenseInventoryItemInstance& Instance1,
                                const FSuspenseInventoryItemInstance& Instance2,
                                const UObject* WorldContext);
    
    /**
     * Объединяет совместимые runtime экземпляры
     * Возвращает результат объединения и remainder если необходимо
     * 
     * @param SourceInstance Исходный экземпляр (будет изменен)
     * @param TargetInstance Целевой экземпляр для объединения
     * @param WorldContext Контекст для получения stack limits
     * @param OutRemainder Остаток если объединение частичное
     * @return true если объединение выполнено (частично или полностью)
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Stacking")
    static bool StackInstances(UPARAM(ref) FSuspenseInventoryItemInstance& SourceInstance,
                             const FSuspenseInventoryItemInstance& TargetInstance,
                             const UObject* WorldContext,
                             FSuspenseInventoryItemInstance& OutRemainder);
    
    /**
     * Разделяет runtime экземпляр на два стека
     * 
     * @param SourceInstance Исходный экземпляр (будет уменьшен)
     * @param SplitQuantity Количество для отделения
     * @param OutNewInstance Новый экземпляр с отделенным количеством
     * @return true если разделение успешно
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Stacking")
    static bool SplitInstance(UPARAM(ref) FSuspenseInventoryItemInstance& SourceInstance,
                            int32 SplitQuantity,
                            FSuspenseInventoryItemInstance& OutNewInstance);
    
    /**
     * Получает максимальный размер стека из DataTable
     * 
     * @param ItemID Идентификатор предмета
     * @param WorldContext Контекст для получения данных
     * @return Максимальный размер стека для предмета
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Stacking")
    static int32 GetMaxStackSize(const FName& ItemID, const UObject* WorldContext);
    
    //==================================================================
    // Enhanced UI Support and Display
    //==================================================================
    
    /**
     * Рассчитывает позицию предмета в UI сетке
     * Enhanced версия с support для padding и borders
     * 
     * @param GridX X координата в сетке
     * @param GridY Y координата в сетке
     * @param CellSize Размер ячейки в пикселях
     * @param CellPadding Отступ между ячейками
     * @param GridBorderSize Размер границы сетки
     * @return Позиция в пикселях с учетом всех отступов
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|UI")
    static FVector2D CalculateItemPositionInUI(int32 GridX, int32 GridY, 
                                             const FVector2D& CellSize, 
                                             float CellPadding = 2.0f,
                                             float GridBorderSize = 5.0f);
    
    /**
     * Рассчитывает размер предмета в UI
     * Enhanced версия с учетом размеров из DataTable
     * 
     * @param ItemID Идентификатор предмета
     * @param CellSize Размер ячейки в пикселях
     * @param CellPadding Отступ между ячейками
     * @param bIsRotated Повернут ли предмет
     * @param WorldContext Контекст для получения размеров
     * @return Размер предмета в пикселях
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|UI")
    static FVector2D CalculateItemSizeInUI(const FName& ItemID,
                                         const FVector2D& CellSize,
                                         float CellPadding,
                                         bool bIsRotated,
                                         const UObject* WorldContext);
    
    /**
     * Получает display имя предмета из DataTable
     * 
     * @param ItemID Идентификатор предмета
     * @param WorldContext Контекст для получения данных
     * @return Локализованное display имя
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|UI")
    static FText GetItemDisplayName(const FName& ItemID, const UObject* WorldContext);
    
    /**
     * Получает описание предмета из DataTable
     * 
     * @param ItemID Идентификатор предмета
     * @param WorldContext Контекст для получения данных
     * @return Локализованное описание предмета
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|UI")
    static FText GetItemDescription(const FName& ItemID, const UObject* WorldContext);
    
    /**
     * Получает иконку предмета из DataTable
     * 
     * @param ItemID Идентификатор предмета
     * @param WorldContext Контекст для получения данных
     * @return Текстура иконки или nullptr если не найдена
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
    static UTexture2D* GetItemIcon(const FName& ItemID, const UObject* WorldContext);
    
    /**
     * Форматирует вес для отображения в UI
     * Enhanced версия с localization support
     * 
     * @param Weight Вес в числовом формате
     * @param bShowUnit Показывать ли единицу измерения
     * @param DecimalPlaces Количество знаков после запятой
     * @return Форматированная локализованная строка
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|UI")
    static FText FormatWeightForDisplay(float Weight, bool bShowUnit = true, int32 DecimalPlaces = 1);
    
    /**
     * Получает цвет редкости предмета для UI
     * 
     * @param ItemID Идентификатор предмета
     * @param WorldContext Контекст для получения данных
     * @return Цвет соответствующий редкости предмета
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|UI")
    static FLinearColor GetItemRarityColor(const FName& ItemID, const UObject* WorldContext);
    
    //==================================================================
    // Enhanced Error Handling and Operations
    //==================================================================
    
    /**
     * Получает локализованное сообщение для кода ошибки
     * Enhanced версия с context-sensitive messages
     * 
     * @param ErrorCode Код ошибки операции
     * @param Context Контекст операции для детализации сообщения
     * @return Локализованное сообщение об ошибке
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Operations")
    static FText GetErrorMessage(ESuspenseInventoryErrorCode ErrorCode, const FString& Context = TEXT(""));
    
    /**
     * Создает успешный результат операции с enhanced данными
     * 
     * @param Context Контекст операции
     * @param ResultObject Объект-результат операции
     * @param AdditionalData Дополнительные данные результата
     * @return Структура успешного результата
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Operations")
    static FSuspenseInventoryOperationResult CreateSuccessResult(const FName& Context,
                                                        UObject* ResultObject = nullptr,
                                                        const FString& AdditionalData = TEXT(""));
    
    /**
     * Создает результат ошибки операции с enhanced данными
     * 
     * @param ErrorCode Код ошибки
     * @param ErrorMessage Детальное сообщение об ошибке
     * @param Context Контекст операции
     * @param ResultObject Объект связанный с ошибкой
     * @return Структура результата ошибки
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Operations")
    static FSuspenseInventoryOperationResult CreateFailureResult(ESuspenseInventoryErrorCode ErrorCode,
                                                        const FText& ErrorMessage,
                                                        const FName& Context,
                                                        UObject* ResultObject = nullptr);
    
    //==================================================================
    // Runtime Properties and Gameplay Integration
    //==================================================================
    
    /**
     * Получает runtime свойство из экземпляра предмета
     * 
     * @param ItemInstance Runtime экземпляр
     * @param PropertyName Имя свойства
     * @param DefaultValue Значение по умолчанию
     * @return Значение runtime свойства
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Runtime Properties")
    static float GetItemRuntimeProperty(const FSuspenseInventoryItemInstance& ItemInstance,
                                      const FName& PropertyName,
                                      float DefaultValue = 0.0f);
    
    /**
     * Устанавливает runtime свойство экземпляра предмета
     * 
     * @param ItemInstance Runtime экземпляр (by reference)
     * @param PropertyName Имя свойства
     * @param Value Новое значение
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Runtime Properties")
    static void SetItemRuntimeProperty(UPARAM(ref) FSuspenseInventoryItemInstance& ItemInstance,
                                     const FName& PropertyName,
                                     float Value);
    
    /**
     * Проверяет наличие runtime свойства
     * 
     * @param ItemInstance Runtime экземпляр
     * @param PropertyName Имя свойства
     * @return true если свойство существует
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Runtime Properties")
    static bool HasItemRuntimeProperty(const FSuspenseInventoryItemInstance& ItemInstance,
                                     const FName& PropertyName);
    
    //==================================================================
    // Debug and Development Utilities
    //==================================================================
    
    /**
     * Получает детальную debug информацию о runtime экземпляре
     * 
     * @param ItemInstance Runtime экземпляр для анализа
     * @param WorldContext Контекст для получения дополнительных данных
     * @return Детальная debug строка
     */
 UFUNCTION(BlueprintCallable, Category = "Inventory|Debug", CallInEditor)
    static FString GetItemInstanceDebugInfo(const FSuspenseInventoryItemInstance& ItemInstance,
                                          const UObject* WorldContext);
    
    /**
     * Получает статистику использования ItemManager
     * 
     * @param WorldContext Контекст для получения ItemManager
     * @return Статистика производительности и кэширования
     */
 UFUNCTION(BlueprintCallable, Category = "Inventory|Debug", CallInEditor)
    static FString GetItemManagerStatistics(const UObject* WorldContext);
    
    //==================================================================
    // Internal Helper Methods
    //==================================================================

private:
    /**
     * Получает ItemManager из world context с error handling
     *
     * @param WorldContext Объект с world context
     * @return ItemManager или nullptr при ошибке
     */
    static USuspenseItemManager* GetItemManager(const UObject* WorldContext);
    
    /**
     * Логирует ошибку с unified форматированием
     * 
     * @param FunctionName Имя функции где произошла ошибка
     * @param ErrorMessage Сообщение об ошибке
     * @param ItemID Связанный ItemID (опционально)
     */
    static void LogError(const FString& FunctionName, const FString& ErrorMessage, const FName& ItemID = NAME_None);
    
    /**
     * Валидирует world context для ItemManager операций
     * 
     * @param WorldContext Контекст для проверки
     * @param FunctionName Имя вызывающей функции для логирования
     * @return true если контекст валиден
     */
    static bool ValidateWorldContext(const UObject* WorldContext, const FString& FunctionName);
};