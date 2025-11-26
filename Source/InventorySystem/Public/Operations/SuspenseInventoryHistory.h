// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Operations/SuspenseInventoryOperation.h"
#include "SuspenseInventoryHistory.generated.h"

/**
 * Класс для управления историей операций с инвентарем.
 * Поддерживает отмену и повтор операций, а также фильтрацию по типам.
 */
UCLASS(BlueprintType)
class INVENTORYSYSTEM_API UInventoryHistory : public UObject
{
    GENERATED_BODY()

public:
    UInventoryHistory();

    /**
     * Добавляет операцию в историю
     * @param Operation Операция для добавления
     */
    void AddOperation(const FSuspenseInventoryOperation& Operation);

    /**
     * Проверяет, можно ли отменить последнюю операцию
     * @return true если есть операция для отмены
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|History")
    bool CanUndo() const;

    /**
     * Отменяет последнюю операцию
     * @return true если операция успешно отменена
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|History")
    bool Undo();

    /**
     * Проверяет, можно ли повторить отмененную операцию
     * @return true если есть операция для повтора
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|History")
    bool CanRedo() const;

    /**
     * Повторяет отмененную операцию
     * @return true если операция успешно повторена
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|History")
    bool Redo();

    /**
     * Очищает всю историю операций
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|History")
    void ClearHistory();

    /**
     * Возвращает последнюю выполненную операцию определенного типа
     * @param OperationType Тип искомой операции
     * @return Указатель на последнюю операцию или nullptr
     */
    TSharedPtr<FSuspenseInventoryOperation> FindLastOperationOfType(EInventoryOperationType OperationType) const;

    /**
     * Возвращает количество операций в истории
     * @return Общее количество операций (выполненных и отмененных)
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|History")
    int32 GetOperationCount() const;

    /**
     * Возвращает текущий индекс в истории
     * @return Индекс текущей операции
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|History")
    int32 GetCurrentHistoryIndex() const;

    /**
     * Устанавливает максимальный размер истории
     * @param NewLimit Новый предел количества операций
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|History")
    void SetHistoryLimit(int32 NewLimit);

private:
    /** Массив операций в истории */
    TArray<TSharedPtr<FSuspenseInventoryOperation>> Operations;

    /** Индекс текущей операции в истории */
    int32 CurrentIndex;

    /** Максимальное количество операций в истории */
    int32 HistoryLimit;

    /** Обеспечивает соблюдение лимита истории */
    void EnforceHistoryLimit();
};
