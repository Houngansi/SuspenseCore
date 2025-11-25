// Copyright Suspense Team. All Rights Reserved.

#include "Operations/SuspenseInventoryHistory.h"
#include "Base/SuspenseSuspenseInventoryLogs.h"

UInventoryHistory::UInventoryHistory()
    : CurrentIndex(-1)
    , HistoryLimit(50)
{
}

void UInventoryHistory::AddOperation(const FSuspenseInventoryOperation& Operation)
{
    // Если добавляем новую операцию, но текущий индекс не в конце истории,
    // то нужно удалить все операции после текущего индекса
    if (CurrentIndex < Operations.Num() - 1 && Operations.Num() > 0)
    {
        Operations.RemoveAt(CurrentIndex + 1, Operations.Num() - CurrentIndex - 1);
    }
    
    // Создаем новую операцию в куче (heap) и добавляем в историю
    TSharedPtr<FSuspenseInventoryOperation> NewOperation = MakeShared<FSuspenseInventoryOperation>(Operation);
    Operations.Add(NewOperation);
    
    // Увеличиваем текущий индекс
    CurrentIndex = Operations.Num() - 1;
    
    // Проверяем лимит истории
    EnforceHistoryLimit();
    
    UE_LOG(LogInventory, Verbose, TEXT("[InventoryHistory] Added operation: %s. Total: %d, Current: %d"), 
           *Operation.ToString(), Operations.Num(), CurrentIndex);
}

bool UInventoryHistory::CanUndo() const
{
    return CurrentIndex >= 0 && Operations.IsValidIndex(CurrentIndex) && 
           Operations[CurrentIndex]->CanUndo();
}

bool UInventoryHistory::Undo()
{
    if (!CanUndo())
    {
        UE_LOG(LogInventory, Warning, TEXT("[InventoryHistory] Cannot undo: No valid operation at index %d"), CurrentIndex);
        return false;
    }
    
    // Получаем текущую операцию
    TSharedPtr<FSuspenseInventoryOperation> Operation = Operations[CurrentIndex];
    
    // Выполняем отмену операции
    bool bResult = Operation->Undo();
    
    if (bResult)
    {
        // Уменьшаем текущий индекс
        CurrentIndex--;
        UE_LOG(LogInventory, Log, TEXT("[InventoryHistory] Undone operation: %s. New current index: %d"), 
               *Operation->ToString(), CurrentIndex);
    }
    else
    {
        UE_LOG(LogInventory, Warning, TEXT("[InventoryHistory] Failed to undo operation: %s"), 
               *Operation->ToString());
    }
    
    return bResult;
}

bool UInventoryHistory::CanRedo() const
{
    return Operations.IsValidIndex(CurrentIndex + 1) && 
           Operations[CurrentIndex + 1]->CanRedo();
}

bool UInventoryHistory::Redo()
{
    if (!CanRedo())
    {
        UE_LOG(LogInventory, Warning, TEXT("[InventoryHistory] Cannot redo: No valid operation at index %d"), CurrentIndex + 1);
        return false;
    }
    
    // Увеличиваем текущий индекс
    CurrentIndex++;
    
    // Получаем операцию для повтора
    TSharedPtr<FSuspenseInventoryOperation> Operation = Operations[CurrentIndex];
    
    // Выполняем повтор операции
    bool bResult = Operation->Redo();
    
    if (bResult)
    {
        UE_LOG(LogInventory, Log, TEXT("[InventoryHistory] Redone operation: %s. New current index: %d"), 
               *Operation->ToString(), CurrentIndex);
    }
    else
    {
        // Если повтор не удался, возвращаемся к предыдущему индексу
        CurrentIndex--;
        UE_LOG(LogInventory, Warning, TEXT("[InventoryHistory] Failed to redo operation: %s"), 
               *Operation->ToString());
    }
    
    return bResult;
}

void UInventoryHistory::ClearHistory()
{
    Operations.Empty();
    CurrentIndex = -1;
    UE_LOG(LogInventory, Log, TEXT("[InventoryHistory] History cleared"));
}

TSharedPtr<FSuspenseInventoryOperation> UInventoryHistory::FindLastOperationOfType(EInventoryOperationType OperationType) const
{
    // Ищем операцию, начиная с текущего индекса и двигаясь назад
    for (int32 i = CurrentIndex; i >= 0; i--)
    {
        if (Operations.IsValidIndex(i) && Operations[i]->OperationType == OperationType)
        {
            return Operations[i];
        }
    }
    
    return nullptr;
}

int32 UInventoryHistory::GetOperationCount() const
{
    return Operations.Num();
}

int32 UInventoryHistory::GetCurrentHistoryIndex() const
{
    return CurrentIndex;
}

void UInventoryHistory::SetHistoryLimit(int32 NewLimit)
{
    if (NewLimit > 0)
    {
        HistoryLimit = NewLimit;
        EnforceHistoryLimit();
        UE_LOG(LogInventory, Log, TEXT("[InventoryHistory] History limit set to %d"), HistoryLimit);
    }
    else
    {
        UE_LOG(LogInventory, Warning, TEXT("[InventoryHistory] Invalid history limit: %d. Must be > 0"), NewLimit);
    }
}

void UInventoryHistory::EnforceHistoryLimit()
{
    // Если количество операций превышает лимит, удаляем самые старые
    if (Operations.Num() > HistoryLimit)
    {
        int32 RemoveCount = Operations.Num() - HistoryLimit;
        Operations.RemoveAt(0, RemoveCount);
        
        // Корректируем текущий индекс
        CurrentIndex = FMath::Max(0, CurrentIndex - RemoveCount);
        
        UE_LOG(LogInventory, Verbose, TEXT("[InventoryHistory] Removed %d oldest operations to enforce history limit"), 
               RemoveCount);
    }
}