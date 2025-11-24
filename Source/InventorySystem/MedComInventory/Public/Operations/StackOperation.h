// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Operations/InventoryOperation.h"
#include "Types/Inventory/InventoryTypes.h"
#include "Types/Loadout/MedComItemDataTable.h"
#include "StackOperation.generated.h"

// Forward declarations
class AMedComInventoryItem;
class UMedComInventoryComponent;
class UMedComItemManager;

/**
 * ПОЛНОСТЬЮ ОБНОВЛЕННАЯ структура операции стакинга предметов
 * 
 * Интеграция с новой DataTable архитектурой:
 * - MaxStackSize получается из DataTable
 * - Валидация совместимости через ItemID и тип
 * - Поддержка runtime свойств (прочность, патроны)
 * - Оптимизированная работа с FInventoryItemInstance
 */
USTRUCT()
struct MEDCOMINVENTORY_API FStackOperation : public FInventoryOperation
{
    GENERATED_BODY()
    
    //==================================================================
    // Core Operation Data
    //==================================================================
    
    /** Исходный предмет (из которого берутся предметы) */
    UPROPERTY()
    AMedComInventoryItem* SourceItem = nullptr;
    
    /** Целевой предмет (в который добавляются предметы) */
    UPROPERTY()
    AMedComInventoryItem* TargetItem = nullptr;
    
    /** Runtime экземпляры предметов */
    UPROPERTY()
    FInventoryItemInstance SourceInstance;
    
    UPROPERTY()
    FInventoryItemInstance TargetInstance;
    
    /** Исходные количества */
    UPROPERTY()
    int32 SourceInitialAmount = 0;
    
    UPROPERTY()
    int32 TargetInitialAmount = 0;
    
    /** Количество для перемещения */
    UPROPERTY()
    int32 AmountToTransfer = 0;
    
    /** Фактически перемещенное количество */
    UPROPERTY()
    int32 ActualTransferred = 0;
    
    /** Позиции предметов */
    UPROPERTY()
    int32 SourceIndex = INDEX_NONE;
    
    UPROPERTY()
    int32 TargetIndex = INDEX_NONE;
    
    /** Целевой инвентарь для cross-inventory операций */
    UPROPERTY()
    UMedComInventoryComponent* TargetInventory = nullptr;
    
    //==================================================================
    // DataTable Integration
    //==================================================================
    
    /** Кэшированные данные из DataTable */
    UPROPERTY()
    FMedComUnifiedItemData CachedItemData;
    
    /** Флаг наличия кэшированных данных */
    UPROPERTY()
    bool bHasCachedData = false;
    
    /** Максимальный размер стека из DataTable */
    UPROPERTY()
    int32 MaxStackSize = 1;
    
    /** Вес одного предмета */
    UPROPERTY()
    float ItemWeight = 0.0f;
    
    //==================================================================
    // Stacking Rules
    //==================================================================
    
    /** Можно ли стакать предметы с разной прочностью */
    UPROPERTY()
    bool bAllowDifferentDurability = false;
    
    /** Можно ли стакать оружие с разными патронами */
    UPROPERTY()
    bool bAllowDifferentAmmo = false;
    
    /** Исходный предмет был полностью израсходован */
    UPROPERTY()
    bool bSourceDepleted = false;
    
    //==================================================================
    // Runtime Properties Handling
    //==================================================================
    
    /** Средняя прочность после стакинга */
    UPROPERTY()
    float AverageDurability = 0.0f;
    
    /** Среднее количество патронов после стакинга */
    UPROPERTY()
    float AverageAmmo = 0.0f;
    
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
    FStackOperation()
        : FInventoryOperation(EInventoryOperationType::Stack, nullptr)
    {
    }
    
    /** 
     * Основной конструктор
     * @param InComponent Исходный компонент инвентаря
     * @param InSourceItem Исходный предмет
     * @param InTargetItem Целевой предмет
     * @param InAmountToTransfer Количество для перемещения
     * @param InTargetInventory Целевой инвентарь (опционально)
     */
    FStackOperation(
        UMedComInventoryComponent* InComponent, 
        AMedComInventoryItem* InSourceItem, 
        AMedComInventoryItem* InTargetItem, 
        int32 InAmountToTransfer,
        UMedComInventoryComponent* InTargetInventory = nullptr
    );
    
    //==================================================================
    // Static Factory Methods
    //==================================================================
    
    /**
     * Создает операцию стакинга с полной валидацией через DataTable
     * @param InComponent Исходный компонент
     * @param InSourceItem Исходный предмет
     * @param InTargetItem Целевой предмет
     * @param InAmountToTransfer Количество для перемещения
     * @param InTargetInventory Целевой инвентарь
     * @param InItemManager ItemManager для доступа к DataTable
     * @return Структура операции стакинга
     */
    static FStackOperation Create(
        UMedComInventoryComponent* InComponent, 
        AMedComInventoryItem* InSourceItem, 
        AMedComInventoryItem* InTargetItem, 
        int32 InAmountToTransfer,
        UMedComInventoryComponent* InTargetInventory,
        UMedComItemManager* InItemManager
    );
    
    /**
     * Создает операцию полного стакинга (все возможное количество)
     * @param InComponent Исходный компонент
     * @param InSourceItem Исходный предмет
     * @param InTargetItem Целевой предмет
     * @param InItemManager ItemManager для доступа к DataTable
     * @return Операция с максимально возможным количеством
     */
    static FStackOperation CreateFullStack(
        UMedComInventoryComponent* InComponent,
        AMedComInventoryItem* InSourceItem,
        AMedComInventoryItem* InTargetItem,
        UMedComItemManager* InItemManager
    );
    
    /**
     * Создает операцию разделения стека
     * @param InComponent Компонент инвентаря
     * @param InSourceItem Исходный стек
     * @param InSplitAmount Количество для отделения
     * @param InTargetIndex Позиция для нового стека
     * @param InItemManager ItemManager для создания нового стека
     * @return Операция разделения
     */
    static FStackOperation CreateSplit(
        UMedComInventoryComponent* InComponent,
        AMedComInventoryItem* InSourceItem,
        int32 InSplitAmount,
        int32 InTargetIndex,
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
     * Вычисляет максимальное количество для перемещения
     * @return Максимально возможное количество с учетом ограничений
     */
    int32 CalculateMaxTransferAmount() const;
    
    //==================================================================
    // Validation Methods
    //==================================================================
    
    /**
     * Полная валидация операции стакинга
     * @param OutErrorCode Код ошибки при неудаче
     * @param OutErrorMessage Детальное описание ошибки
     * @param InItemManager ItemManager для валидации
     * @return true если стакинг возможен
     */
    bool ValidateStacking(
        EInventoryErrorCode& OutErrorCode,
        FString& OutErrorMessage,
        UMedComItemManager* InItemManager
    ) const;
    
    /**
     * Проверяет совместимость предметов для стакинга
     * @return true если предметы можно стакать
     */
    bool AreItemsStackable() const;
    
    /**
     * Проверяет совместимость runtime свойств
     * @return true если свойства совместимы или разрешены различия
     */
    bool AreRuntimePropertiesCompatible() const;
    
    /**
     * Проверяет весовые ограничения для cross-inventory стакинга
     * @return true если вес допустим
     */
    bool ValidateWeightConstraints() const;
    
    //==================================================================
    // State Query Methods
    //==================================================================
    
    /**
     * Проверяет, является ли операция cross-inventory
     * @return true если стакинг между разными инвентарями
     */
    bool IsCrossInventoryStack() const
    {
        return InventoryComponent != TargetInventory && TargetInventory != nullptr;
    }
    
    /**
     * Проверяет, является ли операция разделением стека
     * @return true если это split операция
     */
    bool IsSplitOperation() const
    {
        return TargetItem == nullptr && TargetIndex != INDEX_NONE;
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
     * Выполняет операцию стакинга
     * @param OutErrorCode Код ошибки при неудаче
     * @param InItemManager ItemManager для операций с данными
     * @return true если операция успешна
     */
    bool ExecuteStacking(
        EInventoryErrorCode& OutErrorCode,
        UMedComItemManager* InItemManager
    );
    
    //==================================================================
    // Runtime Properties Handling
    //==================================================================
    
    /**
     * Объединяет runtime свойства после стакинга
     * Вычисляет средние значения для прочности, патронов и т.д.
     */
    void MergeRuntimeProperties();
    
    /**
     * Применяет объединенные свойства к целевому предмету
     */
    void ApplyMergedProperties();
    
    //==================================================================
    // Undo/Redo System
    //==================================================================
    
    virtual bool CanUndo() const override;
    virtual bool Undo() override;
    virtual bool CanRedo() const override;
    virtual bool Redo() override;
    virtual FString ToString() const override;
    virtual ~FStackOperation() = default;
private:
    //==================================================================
    // Internal Helper Methods
    //==================================================================
    
    /**
     * Выполняет перенос количества между предметами
     * @return true если перенос успешен
     */
    bool TransferAmount();
    
    /**
     * Обрабатывает исходный предмет после переноса
     * Удаляет если он был полностью израсходован
     */
    void HandleSourceDepletion();
    
    /**
     * Создает новый стек для split операции
     * @param InItemManager ItemManager для создания
     * @return true если создание успешно
     */
    bool CreateNewStackForSplit(UMedComItemManager* InItemManager);
    
    /**
     * Обновляет веса инвентарей после операции
     */
    void UpdateInventoryWeights();
    
    /**
     * Логирует детали операции
     * @param Message Сообщение для лога
     * @param bIsError Является ли ошибкой
     */
    void LogOperationDetails(const FString& Message, bool bIsError = false) const;
 
};