// MedComInventory/Validation/SuspenseInventoryValidator.h
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Types/Inventory/InventoryTypes.h"
#include "Types/Loadout/MedComItemDataTable.h"
#include "Operations/InventoryResult.h"
#include "GameplayTagContainer.h"
#include "SuspenseInventoryValidator.generated.h"

// Forward declarations для clean separation между модулями
class UMedComItemManager;
struct FInventoryItemInstance;
struct FMedComUnifiedItemData;

/**
 * ПОЛНОСТЬЮ ОБНОВЛЕННЫЙ класс для валидации операций инвентаря
 * 
 * Архитектурные принципы новой версии:
 * - Интеграция с DataTable как источником истины для статических данных
 * - Поддержка FInventoryItemInstance для runtime валидации экземпляров
 * - Централизованный доступ к данным через UMedComItemManager
 * - Enhanced error reporting с детальной диагностикой
 * - Backward compatibility с legacy структурами во время миграции
 * - Thread-safe операции для multiplayer среды
 */
UCLASS(BlueprintType)
class INVENTORYSYSTEM_API USuspenseInventoryValidator : public UObject
{
    GENERATED_BODY()

public:
    //==================================================================
    // Lifecycle and Initialization - ЖИЗНЕННЫЙ ЦИКЛ
    //==================================================================
    
    USuspenseInventoryValidator();

    /**
     * Инициализирует constraints с конкретными настройками
     * Теперь может работать как с legacy параметрами, так и с LoadoutSettings
     * 
     * @param InMaxWeight Максимальный лимит веса
     * @param InAllowedTypes Контейнер разрешенных типов предметов
     * @param InGridWidth Ширина сетки инвентаря
     * @param InGridHeight Высота сетки инвентаря
     * @param InItemManager Опциональная ссылка на ItemManager для DataTable доступа
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Constraints")
    void Initialize(float InMaxWeight, const FGameplayTagContainer& InAllowedTypes, int32 InGridWidth, int32 InGridHeight, UMedComItemManager* InItemManager = nullptr);

    /**
     * НОВЫЙ МЕТОД: Инициализация из LoadoutSettings (рекомендуемый способ)
     * Автоматически получает все необходимые параметры из конфигурации
     * 
     * @param LoadoutID Идентификатор loadout конфигурации
     * @param InventoryName Имя конкретного инвентаря в loadout
     * @param WorldContext Контекст для доступа к ItemManager subsystem
     * @return true если инициализация прошла успешно
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Constraints")
    bool InitializeFromLoadout(const FName& LoadoutID, const FName& InventoryName, const UObject* WorldContext);

    /**
     * Проверяет готовность constraints к использованию
     * @return true если constraints полностью инициализированы
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Constraints")
    bool IsInitialized() const { return bInitialized; }

    //==================================================================
    // Enhanced Unified Data Validation - ВАЛИДАЦИЯ UNIFIED ДАННЫХ
    //==================================================================

    /**
     * ОСНОВНОЙ МЕТОД: Валидация unified данных предмета из DataTable
     * Заменяет legacy ValidateItemParams для новой архитектуры
     * 
     * @param ItemData Unified данные предмета из DataTable
     * @param Amount Количество предметов
     * @param FunctionName Контекст для логирования
     * @return Результат валидации с детальной информацией об ошибках
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    FInventoryOperationResult ValidateUnifiedItemData(const FMedComUnifiedItemData& ItemData, int32 Amount, const FName& FunctionName) const;

    /**
     * Валидация unified данных с проверкой типовых ограничений
     * Включает все базовые проверки плюс item type restrictions
     * 
     * @param ItemData Unified данные предмета
     * @param Amount Количество предметов
     * @param FunctionName Контекст для логирования
     * @return Результат валидации
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    FInventoryOperationResult ValidateUnifiedItemDataWithRestrictions(const FMedComUnifiedItemData& ItemData, int32 Amount, const FName& FunctionName) const;

    //==================================================================
    // Runtime Instance Validation - ВАЛИДАЦИЯ RUNTIME ЭКЗЕМПЛЯРОВ
    //==================================================================

    /**
     * НОВЫЙ МЕТОД: Валидация runtime экземпляра предмета
     * Проверяет как статические данные из DataTable, так и runtime состояние
     * 
     * @param ItemInstance Runtime экземпляр предмета
     * @param FunctionName Контекст для логирования
     * @return Результат валидации экземпляра
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    FInventoryOperationResult ValidateItemInstance(const FInventoryItemInstance& ItemInstance, const FName& FunctionName) const;

    /**
     * Валидация массива runtime экземпляров
     * Полезно для bulk операций и batch validation
     * 
     * @param ItemInstances Массив экземпляров для проверки
     * @param FunctionName Контекст для логирования
     * @param OutFailedInstances Выходной массив экземпляров, не прошедших валидацию
     * @return Количество успешно прошедших валидацию экземпляров
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    int32 ValidateItemInstances(const TArray<FInventoryItemInstance>& ItemInstances, const FName& FunctionName, TArray<FInventoryItemInstance>& OutFailedInstances) const;

    /**
     * Валидация runtime свойств экземпляра
     * Проверяет consistency и валидность динамических свойств
     * 
     * @param ItemInstance Runtime экземпляр для проверки
     * @param FunctionName Контекст для логирования
     * @return Результат валидации runtime свойств
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    FInventoryOperationResult ValidateRuntimeProperties(const FInventoryItemInstance& ItemInstance, const FName& FunctionName) const;

    //==================================================================
    // Enhanced Grid and Spatial Validation - ПРОСТРАНСТВЕННАЯ ВАЛИДАЦИЯ
    //==================================================================

    /**
     * Валидация индекса слота против границ сетки
     * @param SlotIndex Индекс для проверки
     * @param FunctionName Контекст для логирования
     * @return Результат валидации
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    FInventoryOperationResult ValidateSlotIndex(int32 SlotIndex, const FName& FunctionName) const;

    /**
     * ОБНОВЛЕНО: Валидация границ сетки для unified данных
     * Работает с размерами предметов из DataTable с учетом поворота
     * 
     * @param ItemData Unified данные предмета
     * @param AnchorIndex Начальный индекс размещения
     * @param bIsRotated Повернут ли предмет на 90 градусов
     * @param FunctionName Контекст для логирования
     * @return Результат валидации границ
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    FInventoryOperationResult ValidateGridBoundsForUnified(const FMedComUnifiedItemData& ItemData, int32 AnchorIndex, bool bIsRotated, const FName& FunctionName) const;

    /**
     * Валидация границ для runtime экземпляра
     * Получает размеры из DataTable автоматически
     * 
     * @param ItemInstance Runtime экземпляр предмета
     * @param AnchorIndex Начальный индекс размещения  
     * @param FunctionName Контекст для логирования
     * @return Результат валидации границ
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    FInventoryOperationResult ValidateGridBoundsForInstance(const FInventoryItemInstance& ItemInstance, int32 AnchorIndex, const FName& FunctionName) const;

    /**
     * НОВЫЙ МЕТОД: Валидация размещения предмета с collision detection
     * Проверяет не только границы, но и столкновения с другими предметами
     * 
     * @param ItemData Unified данные предмета
     * @param AnchorIndex Позиция для размещения
     * @param bIsRotated Поворот предмета
     * @param OccupiedSlots Массив уже занятых слотов
     * @param FunctionName Контекст для логирования
     * @return Результат валидации размещения
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    FInventoryOperationResult ValidateItemPlacement(const FMedComUnifiedItemData& ItemData, int32 AnchorIndex, bool bIsRotated, const TArray<bool>& OccupiedSlots, const FName& FunctionName) const;

    //==================================================================
    // Enhanced Weight Validation - ВАЛИДАЦИЯ ВЕСА
    //==================================================================

    /**
     * ОБНОВЛЕНО: Валидация веса для unified данных
     * Использует вес из DataTable с учетом количества
     * 
     * @param ItemData Unified данные предмета
     * @param Amount Количество предметов
     * @param CurrentWeight Текущий вес инвентаря
     * @param FunctionName Контекст для логирования
     * @return Результат валидации веса
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    FInventoryOperationResult ValidateWeightForUnified(const FMedComUnifiedItemData& ItemData, int32 Amount, float CurrentWeight, const FName& FunctionName) const;

    /**
     * Валидация веса для runtime экземпляра
     * Автоматически получает вес из DataTable
     * 
     * @param ItemInstance Runtime экземпляр предмета
     * @param CurrentWeight Текущий вес инвентаря
     * @param FunctionName Контекст для логирования
     * @return Результат валидации веса
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    FInventoryOperationResult ValidateWeightForInstance(const FInventoryItemInstance& ItemInstance, float CurrentWeight, const FName& FunctionName) const;

    /**
     * Проверка превышения лимита веса для unified данных
     * @param ItemData Unified данные предмета
     * @param Amount Количество предметов
     * @param CurrentWeight Текущий вес
     * @return true если лимит будет превышен
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Validation")
    bool WouldExceedWeightLimitUnified(const FMedComUnifiedItemData& ItemData, int32 Amount, float CurrentWeight) const;

    //==================================================================
    // Item and Object Validation - ВАЛИДАЦИЯ ОБЪЕКТОВ
    //==================================================================

    /**
     * Валидация объекта предмета для операций
     * Проверяет interface implementation и initialization
     * 
     * @param ItemObject Объект предмета для проверки
     * @param FunctionName Контекст для логирования
     * @return Результат валидации объекта
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    FInventoryOperationResult ValidateItemForOperation(UObject* ItemObject, const FName& FunctionName) const;

    /**
     * НОВЫЙ МЕТОД: Валидация совместимости предмета с инвентарем
     * Комплексная проверка всех ограничений для конкретного предмета
     * 
     * @param ItemData Unified данные предмета
     * @param Amount Количество предметов
     * @param CurrentWeight Текущий вес инвентаря
     * @param FunctionName Контекст для логирования
     * @return Результат комплексной валидации
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Validation")
    FInventoryOperationResult ValidateItemCompatibility(const FMedComUnifiedItemData& ItemData, int32 Amount, float CurrentWeight, const FName& FunctionName) const;

    //==================================================================
    // Type and Restriction Checking - ПРОВЕРКА ТИПОВ И ОГРАНИЧЕНИЙ
    //==================================================================

    /**
     * Проверка разрешен ли тип предмета в constraints
     * Поддерживает иерархические теги для гибкой конфигурации
     * 
     * @param ItemType Тег типа предмета для проверки
     * @return true если тип разрешен
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Validation")
    bool IsItemTypeAllowed(const FGameplayTag& ItemType) const;

    /**
     * НОВЫЙ МЕТОД: Проверка разрешен ли предмет по множественным критериям
     * Учитывает тип, теги, специальные ограничения
     * 
     * @param ItemData Unified данные предмета
     * @return true если предмет разрешен по всем критериям
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Validation")
    bool IsItemAllowedByAllCriteria(const FMedComUnifiedItemData& ItemData) const;

    /**
     * Проверка превышения лимита веса (legacy метод)
     * @param CurrentWeight Текущий вес
     * @param ItemWeight Вес предмета
     * @param Amount Количество предметов
     * @return true если лимит будет превышен
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Validation")
    bool WouldExceedWeightLimit(float CurrentWeight, float ItemWeight, int32 Amount) const;

    //==================================================================
    // Configuration Access and Modification - ДОСТУП К КОНФИГУРАЦИИ
    //==================================================================

    /**
     * Получить максимальный лимит веса
     * @return Максимальный лимит веса
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Configuration")
    float GetMaxWeight() const { return MaxWeight; }

    /**
     * Установить новый лимит веса
     * @param NewMaxWeight Новый лимит веса
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Configuration")
    void SetMaxWeight(float NewMaxWeight);

    /**
     * Получить разрешенные типы предметов
     * @return Контейнер разрешенных типов
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Configuration")
    const FGameplayTagContainer& GetAllowedItemTypes() const { return AllowedItemTypes; }

    /**
     * Установить новые разрешенные типы предметов
     * @param NewAllowedTypes Новые разрешенные типы
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Configuration")
    void SetAllowedItemTypes(const FGameplayTagContainer& NewAllowedTypes);

    /**
     * Получить размеры сетки инвентаря
     * @return Размеры сетки (ширина x высота)
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Configuration")
    FVector2D GetGridSize() const { return FVector2D(GridWidth, GridHeight); }

    /**
     * Получить общее количество слотов в сетке
     * @return Общее количество слотов
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Configuration")
    int32 GetTotalSlots() const { return GridWidth * GridHeight; }

    //==================================================================
    // Debug and Diagnostic Methods - ОТЛАДКА И ДИАГНОСТИКА
    //==================================================================

    /**
     * НОВЫЙ МЕТОД: Получить подробную диагностическую информацию
     * Полезно для debugging и performance monitoring
     * 
     * @return Строка с детальной информацией о constraints
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    FString GetDetailedDiagnosticInfo() const;

    /**
     * Валидация всех constraints настроек
     * Проверяет внутреннюю consistency конфигурации
     * 
     * @param OutErrors Выходной массив найденных ошибок
     * @return true если все настройки валидны
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    bool ValidateConstraintsConfiguration(TArray<FString>& OutErrors) const;

private:
    //==================================================================
    // Internal Data Members - ВНУТРЕННИЕ ДАННЫЕ
    //==================================================================

    /** Максимальный вес, который может содержать инвентарь */
    UPROPERTY()
    float MaxWeight;

    /** Разрешенные типы предметов для этого инвентаря */
    UPROPERTY()
    FGameplayTagContainer AllowedItemTypes;

    /** Ширина сетки для проверки границ */
    UPROPERTY()
    int32 GridWidth;

    /** Высота сетки для проверки границ */
    UPROPERTY()
    int32 GridHeight;

    /** Инициализирован ли объект constraints */
    UPROPERTY()
    bool bInitialized;

    /** Слабая ссылка на ItemManager для доступа к DataTable */
    UPROPERTY()
    TWeakObjectPtr<UMedComItemManager> ItemManagerRef;

    //==================================================================
    // Internal Helper Methods - ВНУТРЕННИЕ ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ
    //==================================================================

    /**
     * Получить validated ItemManager для доступа к DataTable
     * Централизованная логика с proper error handling
     * 
     * @return Валидный ItemManager или nullptr
     */
    UMedComItemManager* GetValidatedItemManager() const;

    /**
     * Получить unified данные для runtime экземпляра
     * Internal helper для получения DataTable данных из экземпляра
     * 
     * @param ItemInstance Runtime экземпляр
     * @param OutUnifiedData Выходные unified данные
     * @return true если данные получены успешно
     */
    bool GetUnifiedDataForInstance(const FInventoryItemInstance& ItemInstance, FMedComUnifiedItemData& OutUnifiedData) const;

    /**
     * Валидация базовых параметров unified данных
     * Internal helper для проверки фундаментальных свойств
     * 
     * @param ItemData Unified данные для проверки
     * @param Amount Количество предметов
     * @param FunctionName Контекст для логирования
     * @return Результат базовой валидации
     */
    FInventoryOperationResult ValidateUnifiedDataBasics(const FMedComUnifiedItemData& ItemData, int32 Amount, const FName& FunctionName) const;

    /**
     * Рассчитать эффективный размер предмета с учетом поворота
     * @param BaseSize Базовый размер из DataTable
     * @param bIsRotated Повернут ли предмет
     * @return Эффективный размер для размещения
     */
    FVector2D CalculateEffectiveItemSize(const FIntPoint& BaseSize, bool bIsRotated) const;

    /**
     * Получить все слоты, которые займет предмет
     * @param AnchorIndex Якорный индекс
     * @param ItemSize Размер предмета
     * @return Массив всех занимаемых слотов
     */
    TArray<int32> GetOccupiedSlots(int32 AnchorIndex, const FVector2D& ItemSize) const;

    /**
     * Логирование результата валидации для debugging
     * @param Result Результат валидации
     * @param Context Дополнительный контекст
     */
    void LogValidationResult(const FInventoryOperationResult& Result, const FString& Context) const;
};