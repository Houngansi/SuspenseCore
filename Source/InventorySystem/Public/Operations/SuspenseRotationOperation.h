// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Operations/SuspenseInventoryOperation.h"
#include "Types/Inventory/InventoryTypes.h"
#include "Types/Loadout/MedComItemDataTable.h"
#include "RotationOperation.generated.h"

// Forward declarations
class AMedComInventoryItem;
class USuspenseInventoryComponent;
class UMedComItemManager;

/**
 * ПОЛНОСТЬЮ ОБНОВЛЕННАЯ структура операции поворота предмета
 * 
 * Интеграция с новой DataTable архитектурой:
 * - Размеры получаются из DataTable через ItemManager
 * - Поддержка runtime свойств через FInventoryItemInstance
 * - Оптимизированная проверка коллизий
 * - Кэширование данных для производительности
 */
USTRUCT()
struct INVENTORYSYSTEM_API FSuspenseRotationOperation : public FSuspenseInventoryOperation
{
    GENERATED_BODY()
    
    //==================================================================
    // Core Operation Data
    //==================================================================
    
    /** Предмет для поворота */
    UPROPERTY()
    AMedComInventoryItem* Item = nullptr;
    
    /** Runtime экземпляр предмета */
    UPROPERTY()
    FInventoryItemInstance ItemInstance;
    
    /** Исходное состояние поворота */
    UPROPERTY()
    bool bInitialRotation = false;
    
    /** Целевое состояние поворота */
    UPROPERTY()
    bool bTargetRotation = false;
    
    /** Базовый размер из DataTable - ИСПРАВЛЕНО: изменен тип на FIntPoint */
    UPROPERTY()
    FIntPoint BaseGridSize = FIntPoint::ZeroValue;
    
    /** Исходный эффективный размер */
    UPROPERTY()
    FVector2D InitialEffectiveSize = FVector2D::ZeroVector;
    
    /** Целевой эффективный размер */
    UPROPERTY()
    FVector2D TargetEffectiveSize = FVector2D::ZeroVector;
    
    /** Индекс якорной ячейки */
    UPROPERTY()
    int32 AnchorIndex = INDEX_NONE;
    
    //==================================================================
    // DataTable Integration
    //==================================================================
    
    /** Кэшированные данные из DataTable */
    UPROPERTY()
    FMedComUnifiedItemData CachedItemData;
    
    /** Флаг наличия кэшированных данных */
    UPROPERTY()
    bool bHasCachedData = false;
    
    //==================================================================
    // Collision Detection
    //==================================================================
    
    /** Ячейки, которые будут заняты после поворота */
    UPROPERTY()
    TArray<int32> TargetOccupiedCells;
    
    /** Количество проверок коллизий */
    UPROPERTY()
    int32 CollisionChecks = 0;
    
    //==================================================================
    // Performance Tracking
    //==================================================================
    
    /** Время выполнения операции */
    UPROPERTY()
    float ExecutionTime = 0.0f;
    
    //==================================================================
    // Constructors
    //==================================================================
    
    /** Конструктор по умолчанию */
    FSuspenseRotationOperation()
        : FSuspenseInventoryOperation(EInventoryOperationType::Rotate, nullptr)
    {
    }
    
    /**
     * Основной конструктор
     * @param InComponent Компонент инвентаря
     * @param InItem Предмет для поворота
     * @param InTargetRotation Целевое состояние поворота
     */
    FSuspenseRotationOperation(
        USuspenseInventoryComponent* InComponent,
        AMedComInventoryItem* InItem,
        bool InTargetRotation
    );
    
    //==================================================================
    // Static Factory Methods
    //==================================================================
    
    /**
     * Создает операцию поворота с валидацией через DataTable
     * @param InItem Предмет для поворота
     * @param InTargetRotation Целевое состояние
     * @param InItemManager ItemManager для доступа к DataTable
     * @return Структура операции поворота
     */
    static FSuspenseRotationOperation Create(
        AMedComInventoryItem* InItem, 
        bool InTargetRotation,
        UMedComItemManager* InItemManager
    );
    
    /**
     * Создает операцию с компонентом инвентаря
     * @param InComponent Компонент инвентаря
     * @param InItem Предмет для поворота
     * @param InTargetRotation Целевое состояние
     * @param InItemManager ItemManager для доступа к DataTable
     * @return Структура операции поворота
     */
    static FSuspenseRotationOperation Create(
        USuspenseInventoryComponent* InComponent, 
        AMedComInventoryItem* InItem, 
        bool InTargetRotation,
        UMedComItemManager* InItemManager
    );
    
    /**
     * Создает операцию переключения поворота (toggle)
     * @param InComponent Компонент инвентаря
     * @param InItem Предмет для поворота
     * @param InItemManager ItemManager для доступа к DataTable
     * @return Операция с противоположным состоянием поворота
     */
    static FSuspenseRotationOperation CreateToggle(
        USuspenseInventoryComponent* InComponent,
        AMedComInventoryItem* InItem,
        UMedComItemManager* InItemManager
    );
    
    //==================================================================
    // DataTable Integration Methods
    //==================================================================
    
    /**
     * Кэширует данные из DataTable
     * @param InItemManager ItemManager для доступа к данным
     * @return true если данные успешно получены
     */
    bool CacheItemDataFromTable(UMedComItemManager* InItemManager);
    
    /**
     * Вычисляет эффективные размеры для обоих состояний
     */
    void CalculateEffectiveSizes();
    
    //==================================================================
    // Validation Methods
    //==================================================================
    
    /**
     * Полная валидация возможности поворота
     * @param OutErrorCode Код ошибки при неудаче
     * @param OutErrorMessage Детальное описание ошибки
     * @return true если поворот возможен
     */
    bool ValidateRotation(
        EInventoryErrorCode& OutErrorCode,
        FString& OutErrorMessage
    ) const;
    
    /**
     * Проверяет коллизии для нового размещения
     * @return true если нет коллизий
     */
    bool CheckCollisions() const;
    
    /**
     * Вычисляет целевые занимаемые ячейки
     */
    void CalculateTargetCells();
    
    //==================================================================
    // State Query Methods
    //==================================================================
    
    /**
     * Проверяет изменение состояния поворота
     * @return true если операция изменяет поворот
     */
    bool HasRotationChanged() const
    {
        return bInitialRotation != bTargetRotation;
    }
    
    /**
     * Проверяет изменение размера при повороте
     * @return true если размер изменится
     */
    bool HasSizeChanged() const
    {
        return InitialEffectiveSize != TargetEffectiveSize;
    }
    
    /**
     * Получает описание операции
     * @return Строковое описание
     */
    FString GetOperationDescription() const;
    
    //==================================================================
    // Execution Methods
    //==================================================================
    
    /**
     * Выполняет операцию поворота
     * @param OutErrorCode Код ошибки при неудаче
     * @return true если операция успешна
     */
    bool ExecuteRotation(EInventoryErrorCode& OutErrorCode);
    
    //==================================================================
    // Undo/Redo System
    //==================================================================
    
    virtual bool CanUndo() const override;
    virtual bool Undo() override;
    virtual bool CanRedo() const override;
    virtual bool Redo() override;
    virtual FString ToString() const override;
    
    // ИСПРАВЛЕНО: добавляем виртуальный деструктор для устранения предупреждения
    virtual ~FSuspenseRotationOperation() = default;
    
private:
    //==================================================================
    // Internal Helper Methods
    //==================================================================
    
    /**
     * Применяет поворот к предмету
     * @param bRotated Новое состояние поворота
     */
    void ApplyRotation(bool bRotated);
    
    /**
     * Обновляет размещение в сетке после поворота
     */
    void UpdateGridPlacement();
    
    /**
     * Логирует детали операции
     * @param Message Сообщение для лога
     * @param bIsError Является ли ошибкой
     */
    void LogOperationDetails(const FString& Message, bool bIsError = false) const;
};