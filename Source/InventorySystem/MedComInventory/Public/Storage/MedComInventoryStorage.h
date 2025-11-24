// MedComInventory/Storage/MedComInventoryStorage.h
// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/Inventory/InventoryTypes.h"
#include "Base/InventoryLogs.h"
#include "GameplayTagContainer.h"
#include "MedComInventoryStorage.generated.h"

// Forward declarations для новой архитектуры
class UMedComItemManager;
struct FMedComUnifiedItemData;
struct FInventoryItemInstance;
class UEventDelegateManager;

/**
 * Структура для описания транзакции изменения инвентаря
 * Обеспечивает atomic операции и возможность rollback при ошибках
 */
USTRUCT()
struct FInventoryTransaction
{
    GENERATED_BODY()
    
    /** Снимок состояния ячеек до изменения */
    UPROPERTY()
    TArray<FInventoryCell> CellsSnapshot;
    
    /** Снимок runtime экземпляров до изменения */
    UPROPERTY()
    TArray<FInventoryItemInstance> InstancesSnapshot;
    
    /** Активна ли транзакция */
    UPROPERTY()
    bool bIsActive = false;
    
    /** Время начала транзакции для timeout защиты */
    UPROPERTY()
    float StartTime = 0.0f;
    
    FInventoryTransaction()
    {
        bIsActive = false;
        StartTime = 0.0f;
    }
    
    /** Проверка валидности транзакции */
    bool IsValid(float CurrentTime, float TimeoutSeconds = 30.0f) const
    {
        return bIsActive && (CurrentTime - StartTime) < TimeoutSeconds;
    }
};

/**
 * Компонент отвечающий за хранение предметов инвентаря в grid-based структуре
 * 
 * НОВАЯ АРХИТЕКТУРА:
 * - Полностью интегрирован с ItemManager и DataTable
 * - Работает исключительно с FGuid и FMedComUnifiedItemData
 * - Поддерживает FInventoryItemInstance для runtime данных
 * - Обеспечивает atomic операции через transaction system
 * - Интегрирован с системой событий для UI updates
 */
UCLASS(ClassGroup=(MedCom), meta=(BlueprintSpawnableComponent))
class MEDCOMINVENTORY_API UMedComInventoryStorage : public UActorComponent
{
    GENERATED_BODY()

public:
    //==================================================================
    // Constructor and Lifecycle
    //==================================================================
    
    UMedComInventoryStorage();
    
    /** Component initialization */
    virtual void BeginPlay() override;
    
    /** Component cleanup */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    //==================================================================
    // Core Storage Management
    //==================================================================
    
    /** 
     * Инициализирует storage сетку с указанными размерами
     * Очищает все существующие данные и создает чистую сетку
     * 
     * @param Width Ширина сетки в ячейках
     * @param Height Высота сетки в ячейках
     * @param MaxWeight Максимальный вес для storage (0 = без ограничений)
     * @return true если инициализация прошла успешно
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Storage")
    bool InitializeGrid(int32 Width, int32 Height, float MaxWeight = 0.0f);

    /**
     * Проверяет инициализирован ли storage
     * @return true если storage готов к использованию
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Storage")
    bool IsInitialized() const { return bInitialized; }

    /**
     * Получает размеры сетки инвентаря
     * @return 2D вектор содержащий ширину и высоту
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Storage")
    FVector2D GetGridSize() const { return FVector2D(GridWidth, GridHeight); }
    
    /**
     * Получает общее количество ячеек в сетке
     * @return Общее количество ячеек
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Storage")
    int32 GetTotalCells() const { return GridWidth * GridHeight; }
    
    /**
     * Получает количество свободных ячеек
     * @return Количество незанятых ячеек
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Storage")
    int32 GetFreeCellCount() const;

    //==================================================================
    // Item Instance Management
    //==================================================================
    
    /**
     * Добавляет runtime экземпляр предмета в storage
     * Автоматически находит подходящее место и размещает предмет
     * 
     * @param ItemInstance Runtime экземпляр для размещения
     * @param bAllowRotation Разрешить поворот для оптимального размещения
     * @return true если предмет успешно размещен
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Items")
    bool AddItemInstance(const FInventoryItemInstance& ItemInstance, bool bAllowRotation = true);
    
    /**
     * Удаляет runtime экземпляр предмета из storage
     * 
     * @param InstanceID Уникальный ID экземпляра для удаления
     * @return true если предмет найден и удален
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Items")
    bool RemoveItemInstance(const FGuid& InstanceID);
    
    /**
     * Получает runtime экземпляр предмета по его ID
     * 
     * @param InstanceID Уникальный ID экземпляра
     * @param OutInstance Выходной runtime экземпляр
     * @return true если экземпляр найден
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Items")
    bool GetItemInstance(const FGuid& InstanceID, FInventoryItemInstance& OutInstance) const;
    
    /**
     * Получает все runtime экземпляры в storage
     * @return Массив всех runtime экземпляров
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Items")
    TArray<FInventoryItemInstance> GetAllItemInstances() const;
    
    /**
     * Обновляет runtime экземпляр предмета
     * Полезно для изменения количества, runtime свойств и т.д.
     * 
     * @param UpdatedInstance Обновленный runtime экземпляр
     * @return true если обновление прошло успешно
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Items")
    bool UpdateItemInstance(const FInventoryItemInstance& UpdatedInstance);

    //==================================================================
    // Space Management and Placement
    //==================================================================
    
    /**
     * Находит свободное место для предмета указанного размера
     * Использует intelligent algorithms для оптимального размещения
     * 
     * @param ItemID ID предмета из DataTable (для получения размера)
     * @param bAllowRotation Разрешить поворот для лучшего размещения
     * @param bOptimizeFragmentation Минимизировать фрагментацию
     * @return Индекс якорной ячейки или INDEX_NONE если места нет
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Placement")
    int32 FindFreeSpace(const FName& ItemID, bool bAllowRotation = true, bool bOptimizeFragmentation = true) const;

    /**
     * Проверяет свободны ли ячейки для размещения предмета
     * 
     * @param StartIndex Начальный индекс якорной ячейки
     * @param ItemID ID предмета для получения размера из DataTable
     * @param bIsRotated Повернут ли предмет
     * @return true если все необходимые ячейки свободны
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Placement")
    bool AreCellsFreeForItem(int32 StartIndex, const FName& ItemID, bool bIsRotated = false) const;

    /**
     * Размещает runtime экземпляр в указанной позиции
     * 
     * @param ItemInstance Runtime экземпляр для размещения
     * @param AnchorIndex Индекс якорной ячейки
     * @return true если размещение прошло успешно
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Placement")
    bool PlaceItemInstance(const FInventoryItemInstance& ItemInstance, int32 AnchorIndex);
    
    /**
     * Перемещает предмет из одной позиции в другую
     * Поддерживает atomic операцию с rollback при неудаче
     * 
     * @param InstanceID ID экземпляра для перемещения
     * @param NewAnchorIndex Новая позиция якорной ячейки
     * @param bAllowRotation Разрешить изменение поворота
     * @return true если перемещение прошло успешно
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Placement")
    bool MoveItem(const FGuid& InstanceID, int32 NewAnchorIndex, bool bAllowRotation = false);

    //==================================================================
    // Item Queries and Access
    //==================================================================
    
    /**
     * Получает runtime экземпляр в указанной ячейке
     * @param Index Индекс ячейки
     * @param OutInstance Выходной runtime экземпляр
     * @return true если в ячейке есть предмет
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Query")
    bool GetItemInstanceAt(int32 Index, FInventoryItemInstance& OutInstance) const;
    
    /**
     * Подсчитывает общее количество предметов по ID
     * @param ItemID ID предмета для подсчета
     * @return Общее количество предметов данного типа
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Query")
    int32 GetItemCountByID(const FName& ItemID) const;
    
    /**
     * Находит предметы по типу из DataTable
     * @param ItemType Тип предмета для поиска
     * @return Массив найденных runtime экземпляров
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Query")
    TArray<FInventoryItemInstance> FindItemsByType(const FGameplayTag& ItemType) const;

    //==================================================================
    // Grid Coordinate Utilities
    //==================================================================
    
    /**
     * Конвертирует линейный индекс в координаты сетки
     * @param Index Линейный индекс
     * @param OutX Выходная X координата
     * @param OutY Выходная Y координата
     * @return true если конвертация прошла успешно
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Utilities")
    bool GetGridCoordinates(int32 Index, int32& OutX, int32& OutY) const;

    /**
     * Конвертирует координаты сетки в линейный индекс
     * @param X X координата
     * @param Y Y координата
     * @param OutIndex Выходной линейный индекс
     * @return true если координаты валидны
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Utilities")
    bool GetLinearIndex(int32 X, int32 Y, int32& OutIndex) const;
    
    /**
     * Получает все занятые ячейки для предмета
     * @param InstanceID ID экземпляра предмета
     * @return Массив индексов занятых ячеек
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Utilities")
    TArray<int32> GetOccupiedCells(const FGuid& InstanceID) const;

    //==================================================================
    // Weight Management
    //==================================================================
    
    /**
     * Получает текущий общий вес в storage
     * Вычисляется на основе предметов и их данных из DataTable
     * @return Текущий вес в игровых единицах
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Weight")
    float GetCurrentWeight() const;
    
    /**
     * Получает максимально допустимый вес
     * @return Максимальный вес или 0 если ограничения нет
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Weight")
    float GetMaxWeight() const { return MaxWeight; }
    
    /**
     * Устанавливает максимальный вес
     * @param NewMaxWeight Новое ограничение по весу
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Weight")
    void SetMaxWeight(float NewMaxWeight);
    
    /**
     * Проверяет достаточно ли места по весу для предмета
     * @param ItemID ID предмета для проверки
     * @param Quantity Количество предметов
     * @return true если есть достаточно места по весу
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Weight")
    bool HasWeightCapacity(const FName& ItemID, int32 Quantity = 1) const;

    //==================================================================
    // Transaction Support
    //==================================================================
    
    /**
     * Начинает atomic транзакцию для multiple операций
     * Создает snapshot текущего состояния для возможного rollback
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Transactions")
    void BeginTransaction();
    
    /**
     * Подтверждает все изменения в текущей транзакции
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Transactions")
    void CommitTransaction();
    
    /**
     * Отменяет все изменения в текущей транзакции
     * Восстанавливает состояние на момент начала транзакции
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Transactions")
    void RollbackTransaction();
    
    /**
     * Проверяет активна ли транзакция
     * @return true если транзакция активна
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Transactions")
    bool IsTransactionActive() const;
 
    //==================================================================
    // Maintenance and Utilities
    //==================================================================
    
    /**
     * Очищает все предметы из storage
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Maintenance")
    void ClearAllItems();
    
    /**
     * Валидирует целостность storage
     * Проверяет consistency между ячейками и runtime экземплярами
     * @param OutErrors Массив найденных ошибок
     * @param bAutoFix Автоматически исправить найденные проблемы
     * @return true если storage в валидном состоянии
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    bool ValidateStorageIntegrity(TArray<FString>& OutErrors, bool bAutoFix = false);
    
    /**
     * Получает детальную debug информацию о storage
     * @return Подробная строка с состоянием storage
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    FString GetStorageDebugInfo() const;
    
    /**
     * Дефрагментирует storage для оптимизации использования места
     * Перемещает предметы для уменьшения фрагментации
     * @return Количество перемещенных предметов
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Optimization")
    int32 DefragmentStorage();

protected:
    //==================================================================
    // Storage State
    //==================================================================
    
    /** Ширина сетки в ячейках */
    UPROPERTY(VisibleAnywhere, Category = "Inventory|State")
    int32 GridWidth = 0;

    /** Высота сетки в ячейках */
    UPROPERTY(VisibleAnywhere, Category = "Inventory|State")
    int32 GridHeight = 0;
    
    /** Максимальный вес для storage (0 = без ограничений) */
    UPROPERTY(EditAnywhere, Category = "Inventory|Configuration")
    float MaxWeight = 0.0f;

    /** Состояние инициализации */
    UPROPERTY(VisibleAnywhere, Category = "Inventory|State")
    bool bInitialized = false;

    /** Ячейки сетки с информацией о размещении */
    UPROPERTY()
    TArray<FInventoryCell> Cells;

    /** Runtime экземпляры предметов в storage */
    UPROPERTY()
    TArray<FInventoryItemInstance> StoredInstances;

    /** Битовая карта для быстрого поиска свободных ячеек */
    TBitArray<> FreeCellsBitmap;
    
    /** Текущая транзакция для atomic операций */
    UPROPERTY()
    FInventoryTransaction ActiveTransaction;

    //==================================================================
    // Internal Helper Methods
    //==================================================================
    
    /**
     * Валидирует индекс на принадлежность к сетке
     * @param Index Индекс для проверки
     * @return true если индекс валиден
     */
    bool IsValidIndex(int32 Index) const;
    
    /**
     * Пересчитывает битовую карту свободных ячеек
     */
    void UpdateFreeCellsBitmap();
    
    /**
     * Получает ItemManager для доступа к DataTable
     * @return ItemManager или nullptr если недоступен
     */
    UMedComItemManager* GetItemManager() const;
    
    /**
     * Получает unified данные предмета из DataTable
     * @param ItemID ID предмета
     * @param OutData Выходные данные
     * @return true если данные получены
     */
    bool GetItemData(const FName& ItemID, FMedComUnifiedItemData& OutData) const;
    
    /**
     * Размещает runtime экземпляр в ячейках сетки
     * @param ItemInstance Экземпляр для размещения
     * @param AnchorIndex Якорная ячейка
     * @return true если размещение прошло успешно
     */
    bool PlaceInstanceInCells(const FInventoryItemInstance& ItemInstance, int32 AnchorIndex);
    
    /**
     * Удаляет runtime экземпляр из ячеек сетки
     * @param InstanceID ID экземпляра для удаления
     * @return true если удаление прошло успешно
     */
    bool RemoveInstanceFromCells(const FGuid& InstanceID);
    
    /**
     * Находит runtime экземпляр по ID
     * @param InstanceID ID для поиска
     * @return Указатель на экземпляр или nullptr
     */
    FInventoryItemInstance* FindStoredInstance(const FGuid& InstanceID);
    
    /**
     * Находит runtime экземпляр по ID (const версия)
     */
    const FInventoryItemInstance* FindStoredInstance(const FGuid& InstanceID) const;
    
    /**
     * Создает snapshot текущего состояния для транзакции
     */
    void CreateTransactionSnapshot();
    
    /**
     * Восстанавливает состояние из snapshot транзакции
     */
    void RestoreFromTransactionSnapshot();
    
    /**
     * Вычисляет оптимальные координаты для размещения предмета
     * @param ItemSize Размер предмета
     * @param bOptimizeFragmentation Минимизировать фрагментацию
     * @return Индекс оптимальной позиции или INDEX_NONE
     */
    int32 FindOptimalPlacement(const FVector2D& ItemSize, bool bOptimizeFragmentation = true) const;
    
    /**
     * Проверяет свободны ли ячейки для указанного размера
     * @param StartIndex Начальный индекс  
     * @param Size Размер области для проверки
     * @return true если все ячейки в области свободны
     */
    bool AreCellsFree(int32 StartIndex, const FVector2D& Size) const;
};