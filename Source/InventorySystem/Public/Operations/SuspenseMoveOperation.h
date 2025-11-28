// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Operations/SuspenseInventoryOperation.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Types/Loadout/SuspenseItemDataTable.h"
#include "SuspenseMoveOperation.generated.h"

// Forward declarations
class ASuspenseInventoryItem;
class USuspenseInventoryComponent;
class USuspenseItemManager;

/**
 * ОБНОВЛЕННАЯ структура для операции перемещения предмета
 * 
 * Полностью интегрирована с новой DataTable архитектурой:
 * - Размеры предметов получаются из DataTable через ItemManager
 * - Поддержка runtime свойств через FSuspenseInventoryItemInstance
 * - Улучшенная валидация с учетом весовых ограничений
 * - Оптимизированное кэширование данных из DataTable
 */
USTRUCT()
struct INVENTORYSYSTEM_API FSuspenseMoveOperation : public FSuspenseInventoryOperation
{
    GENERATED_BODY()
    
    //==================================================================
    // Core Operation Data
    //==================================================================
    
    /** Актор предмета для перемещения */
    UPROPERTY()
    ASuspenseInventoryItem* Item = nullptr;
    
    /** Runtime экземпляр предмета (для доступа к свойствам) */
    UPROPERTY()
    FSuspenseInventoryItemInstance ItemInstance;
    
    /** Исходный индекс ячейки */
    UPROPERTY()
    int32 SourceIndex = INDEX_NONE;
    
    /** Целевой индекс ячейки */
    UPROPERTY()
    int32 TargetIndex = INDEX_NONE;
    
    /** Исходное состояние поворота */
    UPROPERTY()
    bool bSourceRotated = false;
    
    /** Целевое состояние поворота */
    UPROPERTY()
    bool bTargetRotated = false;
    
    /** Кэшированный размер из DataTable (базовый) */
    UPROPERTY()
    FIntPoint BaseGridSize = FIntPoint::ZeroValue;
    
    /** Эффективный размер при исходном размещении */
    UPROPERTY()
    FVector2D SourceEffectiveSize = FVector2D::ZeroVector;
    
    /** Эффективный размер при целевом размещении */
    UPROPERTY()
    FVector2D TargetEffectiveSize = FVector2D::ZeroVector;
    
    /** Целевой компонент инвентаря */
    UPROPERTY()
    USuspenseInventoryComponent* TargetInventory = nullptr;
    
    //==================================================================
    // DataTable Integration
    //==================================================================
    
    /** Кэшированные данные из DataTable */
    UPROPERTY()
    FSuspenseUnifiedItemData CachedItemData;
    
    /** Флаг наличия валидных данных из DataTable */
    UPROPERTY()
    bool bHasCachedData = false;
    
    /** Вес предмета для валидации */
    UPROPERTY()
    float ItemTotalWeight = 0.0f;
    
    //==================================================================
    // Swap Operation Support
    //==================================================================
    
    /** Была ли выполнена операция обмена */
    UPROPERTY()
    bool bWasSwapOperation = false;
    
    /** Предмет с которым был выполнен обмен */
    UPROPERTY()
    ASuspenseInventoryItem* SwappedItem = nullptr;
    
    /** Экземпляр обмененного предмета */
    UPROPERTY()
    FSuspenseInventoryItemInstance SwappedItemInstance;
    
    /** Исходная позиция обмененного предмета */
    UPROPERTY()
    int32 SwappedItemOriginalIndex = INDEX_NONE;
    
    /** Исходное состояние поворота обмененного предмета */
    UPROPERTY()
    bool bSwappedItemOriginalRotated = false;
    
    //==================================================================
    // Performance Tracking
    //==================================================================
    
    /** Время создания операции */
    UPROPERTY()
    float OperationTimestamp = 0.0f;
    
    /** Количество проверок коллизий */
    UPROPERTY()
    int32 CollisionChecks = 0;
    
    //==================================================================
    // Constructors
    //==================================================================
    
    /** Конструктор по умолчанию */
    FSuspenseMoveOperation()
        : FSuspenseInventoryOperation(EInventoryOperationType::Move, nullptr)
    {
    }
    
    /** 
     * Основной конструктор с полной инициализацией
     * Автоматически кэширует данные из DataTable
     */
    FSuspenseMoveOperation(
        USuspenseInventoryComponent* InComponent, 
        ASuspenseInventoryItem* InItem, 
        int32 InTargetIndex, 
        bool InTargetRotated, 
        USuspenseInventoryComponent* InTargetInventory = nullptr
    );
    
    //==================================================================
    // Static Factory Methods
    //==================================================================
    
    /**
     * Создает операцию перемещения с валидацией через DataTable
     * @param InComponent Исходный компонент инвентаря
     * @param InItem Предмет для перемещения
     * @param InTargetIndex Целевой индекс
     * @param InTargetRotated Желаемое состояние поворота
     * @param InTargetInventory Целевой инвентарь (опционально)
     * @param InItemManager ItemManager для доступа к DataTable
     * @return Созданная операция с кэшированными данными
     */
    static FSuspenseMoveOperation Create(
        USuspenseInventoryComponent* InComponent, 
        ASuspenseInventoryItem* InItem, 
        int32 InTargetIndex, 
        bool InTargetRotated, 
        USuspenseInventoryComponent* InTargetInventory,
        USuspenseItemManager* InItemManager
    );
    
    /**
     * Создает операцию с автоматическим определением оптимального поворота
     * @param InComponent Компонент инвентаря
     * @param InItem Предмет для анализа
     * @param InTargetIndex Новая позиция
     * @param InItemManager ItemManager для доступа к DataTable
     * @return Операция с оптимальными параметрами
     */
    static FSuspenseMoveOperation CreateWithOptimalRotation(
        USuspenseInventoryComponent* InComponent,
        ASuspenseInventoryItem* InItem,
        int32 InTargetIndex,
        USuspenseItemManager* InItemManager
    );
    
    //==================================================================
    // DataTable Integration Methods
    //==================================================================
    
    /**
     * Кэширует данные из DataTable для предмета
     * @param InItemManager ItemManager для доступа к DataTable
     * @return true если данные успешно получены
     */
    bool CacheItemDataFromTable(USuspenseItemManager* InItemManager);
    
    /**
     * Вычисляет эффективные размеры с учетом поворота
     */
    void CalculateEffectiveSizes();
    
    /**
     * Получает вес предмета из кэшированных данных
     * @return Общий вес стека предметов
     */
    float GetCachedItemWeight() const;
    
    //==================================================================
    // Validation Methods
    //==================================================================
    
    /**
     * Полная валидация операции с учетом DataTable constraints
     * @param OutErrorCode Код ошибки при неудаче
     * @param OutErrorMessage Детальное описание ошибки
     * @param InItemManager ItemManager для валидации
     * @return true если операция возможна
     */
    bool ValidateOperation(
        ESuspenseInventoryErrorCode& OutErrorCode, 
        FString& OutErrorMessage,
        USuspenseItemManager* InItemManager
    ) const;
    
    /**
     * Проверяет весовые ограничения целевого инвентаря
     * @return true если вес допустим
     */
    bool ValidateWeightConstraints() const;
    
    /**
     * Проверяет совместимость типа предмета с целевым инвентарем
     * @return true если тип разрешен
     */
    bool ValidateItemTypeConstraints() const;
    
    //==================================================================
    // State Query Methods
    //==================================================================
    
    bool HasPositionChanged() const
    {
        return SourceIndex != TargetIndex || InventoryComponent != TargetInventory;
    }
    
    bool HasRotationChanged() const
    {
        return bSourceRotated != bTargetRotated;
    }
    
    bool IsCrossInventoryMove() const
    {
        return InventoryComponent != TargetInventory && TargetInventory != nullptr;
    }
    
    bool HasAnyChanges() const
    {
        return HasPositionChanged() || HasRotationChanged();
    }
    
    FString GetOperationTypeDescription() const;
    
    //==================================================================
    // Execution Methods
    //==================================================================
    
    /**
     * Выполняет операцию перемещения с полной интеграцией DataTable
     * @param OutErrorCode Код ошибки при неудаче
     * @param InItemManager ItemManager для операций с данными
     * @return true если операция успешна
     */
    bool ExecuteOperation(
        ESuspenseInventoryErrorCode& OutErrorCode,
        USuspenseItemManager* InItemManager
    );
    
    //==================================================================
    // Undo/Redo System
    //==================================================================
    
    virtual bool CanUndo() const override;
    virtual bool Undo() override;
    virtual bool CanRedo() const override;
    virtual bool Redo() override;
    virtual FString ToString() const override;
	virtual ~FSuspenseMoveOperation() = default;
private:
    //==================================================================
    // Internal Helper Methods
    //==================================================================
    
    /**
     * Обрабатывает операцию обмена если целевая позиция занята
     * @param BlockingItem Предмет блокирующий позицию
     * @param OutErrorCode Код ошибки при неудаче
     * @param InItemManager ItemManager для валидации
     * @return true если обмен успешен
     */
    bool HandleSwapOperation(
        ASuspenseInventoryItem* BlockingItem, 
        ESuspenseInventoryErrorCode& OutErrorCode,
        USuspenseItemManager* InItemManager
    );
    
    /**
     * Применяет новое состояние к предмету
     */
    void ApplyNewState();
    
    /**
     * Восстанавливает исходное состояние при ошибке
     */
    void RestoreOriginalState();
    
    /**
     * Логирует детали операции
     * @param Message Сообщение для лога
     * @param bIsError Является ли сообщение ошибкой
     */
    void LogOperationDetails(const FString& Message, bool bIsError = false) const;
    
    /**
     * Обновляет runtime свойства после перемещения
     */
    void UpdateRuntimeProperties();
};