// Copyright Suspense Team. All Rights Reserved.

#include "Operations/SuspenseStackOperation.h"
#include "Base/SuspenseInventoryItem.h"
#include "Components/SuspenseInventoryComponent.h"
#include "ItemSystem/MedComItemManager.h"
#include "Base/SuspenseSuspenseInventoryLogs.h"

// Основной конструктор
FSuspenseStackOperation::FSuspenseStackOperation(
    USuspenseInventoryComponent* InComponent, 
    AMedComInventoryItem* InSourceItem, 
    AMedComInventoryItem* InTargetItem, 
    int32 InAmountToTransfer,
    USuspenseInventoryComponent* InTargetInventory)
    : FSuspenseInventoryOperation(EInventoryOperationType::Stack, InComponent)
    , SourceItem(InSourceItem)
    , TargetItem(InTargetItem)
    , AmountToTransfer(InAmountToTransfer)
    , TargetInventory(InTargetInventory ? InTargetInventory : InComponent)
{
    if (SourceItem)
    {
        // Получаем интерфейс для доступа к свойствам источника
        if (const IMedComInventoryItemInterface* SourceInterface = Cast<IMedComInventoryItemInterface>(SourceItem))
        {
            SourceInstance = SourceInterface->GetItemInstance();
            SourceInitialAmount = SourceInterface->GetAmount();
            SourceIndex = SourceInterface->GetAnchorIndex();
        }
    }
    
    if (TargetItem)
    {
        // Получаем интерфейс для доступа к свойствам цели
        if (const IMedComInventoryItemInterface* TargetInterface = Cast<IMedComInventoryItemInterface>(TargetItem))
        {
            TargetInstance = TargetInterface->GetItemInstance();
            TargetInitialAmount = TargetInterface->GetAmount();
            TargetIndex = TargetInterface->GetAnchorIndex();
        }
    }
}

// Статический метод создания с валидацией
FSuspenseStackOperation FSuspenseStackOperation::Create(
    USuspenseInventoryComponent* InComponent, 
    AMedComInventoryItem* InSourceItem, 
    AMedComInventoryItem* InTargetItem, 
    int32 InAmountToTransfer,
    USuspenseInventoryComponent* InTargetInventory,
    UMedComItemManager* InItemManager)
{
    FSuspenseStackOperation Operation(InComponent, InSourceItem, InTargetItem, InAmountToTransfer, InTargetInventory);
    
    // Базовая валидация
    if (!InSourceItem)
    {
        Operation.ErrorCode = EInventoryErrorCode::InvalidItem;
        UE_LOG(LogInventory, Error, TEXT("FSuspenseStackOperation::Create: Invalid source item"));
        return Operation;
    }
    
    if (!InTargetItem && Operation.TargetIndex == INDEX_NONE)
    {
        Operation.ErrorCode = EInventoryErrorCode::InvalidItem;
        UE_LOG(LogInventory, Error, TEXT("FSuspenseStackOperation::Create: Invalid target item and no target index"));
        return Operation;
    }
    
    if (!InComponent)
    {
        Operation.ErrorCode = EInventoryErrorCode::NotInitialized;
        UE_LOG(LogInventory, Error, TEXT("FSuspenseStackOperation::Create: Invalid component"));
        return Operation;
    }
    
    if (!InItemManager)
    {
        Operation.ErrorCode = EInventoryErrorCode::NotInitialized;
        UE_LOG(LogInventory, Error, TEXT("FSuspenseStackOperation::Create: ItemManager not available"));
        return Operation;
    }
    
    // Кэшируем данные из DataTable
    if (!Operation.CacheItemDataFromTable(InItemManager))
    {
        Operation.ErrorCode = EInventoryErrorCode::InvalidItem;
        UE_LOG(LogInventory, Error, TEXT("FSuspenseStackOperation::Create: Failed to cache item data"));
        return Operation;
    }
    
    // Предварительная валидация
    FString ErrorMessage;
    if (!Operation.ValidateStacking(Operation.ErrorCode, ErrorMessage, InItemManager))
    {
        UE_LOG(LogInventory, Warning, TEXT("FSuspenseStackOperation::Create: Validation failed - %s"), *ErrorMessage);
    }
    
    return Operation;
}

// Создание операции полного стакинга
FSuspenseStackOperation FSuspenseStackOperation::CreateFullStack(
    USuspenseInventoryComponent* InComponent,
    AMedComInventoryItem* InSourceItem,
    AMedComInventoryItem* InTargetItem,
    UMedComItemManager* InItemManager)
{
    if (!InSourceItem || !InTargetItem)
    {
        FSuspenseStackOperation InvalidOp;
        InvalidOp.ErrorCode = EInventoryErrorCode::InvalidItem;
        return InvalidOp;
    }
    
    // Получаем интерфейс источника для определения количества
    const IMedComInventoryItemInterface* SourceInterface = Cast<IMedComInventoryItemInterface>(InSourceItem);
    if (!SourceInterface)
    {
        FSuspenseStackOperation InvalidOp;
        InvalidOp.ErrorCode = EInventoryErrorCode::InvalidItem;
        return InvalidOp;
    }
    
    // Создаем операцию с полным количеством исходного предмета
    FSuspenseStackOperation Operation = Create(
        InComponent, 
        InSourceItem, 
        InTargetItem, 
        SourceInterface->GetAmount(),
        nullptr,
        InItemManager
    );
    
    // Вычисляем фактическое максимальное количество для переноса
    if (Operation.bHasCachedData)
    {
        Operation.AmountToTransfer = Operation.CalculateMaxTransferAmount();
    }
    
    return Operation;
}

// Создание операции разделения стека
FSuspenseStackOperation FSuspenseStackOperation::CreateSplit(
    USuspenseInventoryComponent* InComponent,
    AMedComInventoryItem* InSourceItem,
    int32 InSplitAmount,
    int32 InTargetIndex,
    UMedComItemManager* InItemManager)
{
    FSuspenseStackOperation Operation;
    Operation.InventoryComponent = InComponent;
    Operation.SourceItem = InSourceItem;
    Operation.TargetItem = nullptr; // Split создает новый предмет
    Operation.AmountToTransfer = InSplitAmount;
    Operation.TargetIndex = InTargetIndex;
    Operation.TargetInventory = InComponent;
    
    if (InSourceItem)
    {
        // Получаем интерфейс источника
        if (const IMedComInventoryItemInterface* SourceInterface = Cast<IMedComInventoryItemInterface>(InSourceItem))
        {
            Operation.SourceInstance = SourceInterface->GetItemInstance();
            Operation.SourceInitialAmount = SourceInterface->GetAmount();
            Operation.SourceIndex = SourceInterface->GetAnchorIndex();
        }
    }
    
    // Валидация split операции
    if (!InSourceItem || InSplitAmount <= 0 || InSplitAmount >= Operation.SourceInitialAmount)
    {
        Operation.ErrorCode = EInventoryErrorCode::InsufficientQuantity;
        UE_LOG(LogInventory, Error, TEXT("FSuspenseStackOperation::CreateSplit: Invalid split amount"));
        return Operation;
    }
    
    if (!InItemManager)
    {
        Operation.ErrorCode = EInventoryErrorCode::NotInitialized;
        return Operation;
    }
    
    // Кэшируем данные
    if (!Operation.CacheItemDataFromTable(InItemManager))
    {
        Operation.ErrorCode = EInventoryErrorCode::InvalidItem;
        return Operation;
    }
    
    return Operation;
}

// Кэширование данных из DataTable
bool FSuspenseStackOperation::CacheItemDataFromTable(UMedComItemManager* InItemManager)
{
    if (!InItemManager || !SourceItem)
    {
        return false;
    }
    
    // Получаем интерфейс источника
    const IMedComInventoryItemInterface* SourceInterface = Cast<IMedComInventoryItemInterface>(SourceItem);
    if (!SourceInterface)
    {
        UE_LOG(LogInventory, Error, TEXT("FSuspenseStackOperation::CacheItemDataFromTable: Source item doesn't implement required interface"));
        return false;
    }
    
    // Получаем ItemID через интерфейс
    FName ItemID = SourceInterface->GetItemID();
    
    // Получаем данные из DataTable
    if (!InItemManager->GetUnifiedItemData(ItemID, CachedItemData))
    {
        UE_LOG(LogInventory, Error, TEXT("FSuspenseStackOperation::CacheItemDataFromTable: Failed to get data for %s"), 
            *ItemID.ToString());
        return false;
    }
    
    // Сохраняем важные параметры
    MaxStackSize = CachedItemData.MaxStackSize;
    ItemWeight = CachedItemData.Weight;
    bHasCachedData = true;
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("FSuspenseStackOperation::CacheItemDataFromTable: Cached data for %s - MaxStack: %d, Weight: %.2f"), 
        *ItemID.ToString(), MaxStackSize, ItemWeight);
    
    return true;
}

// Вычисление максимального количества для переноса
int32 FSuspenseStackOperation::CalculateMaxTransferAmount() const
{
    if (!bHasCachedData || !TargetItem)
    {
        return AmountToTransfer;
    }
    
    // Вычисляем доступное место в целевом стеке
    int32 TargetAvailableSpace = MaxStackSize - TargetInitialAmount;
    
    // Возвращаем минимум из запрошенного, доступного в источнике и места в цели
    return FMath::Min3(AmountToTransfer, SourceInitialAmount, TargetAvailableSpace);
}

// Полная валидация операции
bool FSuspenseStackOperation::ValidateStacking(
    EInventoryErrorCode& OutErrorCode,
    FString& OutErrorMessage,
    UMedComItemManager* InItemManager) const
{
    // Проверка базовых компонентов
    if (!SourceItem || !InventoryComponent)
    {
        OutErrorCode = EInventoryErrorCode::NotInitialized;
        OutErrorMessage = TEXT("Invalid operation components");
        return false;
    }
    
    // Проверка кэшированных данных
    if (!bHasCachedData)
    {
        OutErrorCode = EInventoryErrorCode::InvalidItem;
        OutErrorMessage = TEXT("No cached item data");
        return false;
    }
    
    // Проверка количества
    if (AmountToTransfer <= 0)
    {
        OutErrorCode = EInventoryErrorCode::InsufficientQuantity;
        OutErrorMessage = TEXT("Invalid transfer amount");
        return false;
    }
    
    if (AmountToTransfer > SourceInitialAmount)
    {
        OutErrorCode = EInventoryErrorCode::InsufficientQuantity;
        OutErrorMessage = FString::Printf(TEXT("Transfer amount %d exceeds source amount %d"), 
            AmountToTransfer, SourceInitialAmount);
        return false;
    }
    
    // Валидация для обычного стакинга
    if (TargetItem)
    {
        // Проверка совместимости предметов
        if (!AreItemsStackable())
        {
            OutErrorCode = EInventoryErrorCode::InvalidItem;
            OutErrorMessage = TEXT("Items are not stackable");
            return false;
        }
        
        // Проверка лимита стека
        if (TargetInitialAmount >= MaxStackSize)
        {
            OutErrorCode = EInventoryErrorCode::NoSpace;
            OutErrorMessage = FString::Printf(TEXT("Target stack is full (%d/%d)"), 
                TargetInitialAmount, MaxStackSize);
            return false;
        }
        
        // Проверка runtime свойств
        if (!AreRuntimePropertiesCompatible())
        {
            OutErrorCode = EInventoryErrorCode::InvalidItem;
            OutErrorMessage = TEXT("Runtime properties are not compatible for stacking");
            return false;
        }
    }
    // Валидация для split операции
    else if (IsSplitOperation())
    {
        // Проверка что исходный стек достаточно большой
        if (SourceInitialAmount <= 1)
        {
            OutErrorCode = EInventoryErrorCode::InsufficientQuantity;
            OutErrorMessage = TEXT("Cannot split single item");
            return false;
        }
        
        // Проверка целевой позиции
        if (TargetIndex < 0)
        {
            OutErrorCode = EInventoryErrorCode::InvalidSlot;
            OutErrorMessage = TEXT("Invalid target index for split");
            return false;
        }
    }
    
    // Проверка весовых ограничений для cross-inventory
    if (!ValidateWeightConstraints())
    {
        OutErrorCode = EInventoryErrorCode::WeightLimit;
        float TransferWeight = ItemWeight * AmountToTransfer;
        OutErrorMessage = FString::Printf(TEXT("Weight limit exceeded - transfer weight: %.2f"), 
            TransferWeight);
        return false;
    }
    
    OutErrorCode = EInventoryErrorCode::Success;
    return true;
}

// Проверка совместимости предметов
bool FSuspenseStackOperation::AreItemsStackable() const
{
    if (!SourceItem || !TargetItem)
    {
        return false;
    }
    
    // Получаем интерфейсы
    const IMedComInventoryItemInterface* SourceInterface = Cast<IMedComInventoryItemInterface>(SourceItem);
    const IMedComInventoryItemInterface* TargetInterface = Cast<IMedComInventoryItemInterface>(TargetItem);
    
    if (!SourceInterface || !TargetInterface)
    {
        return false;
    }
    
    // Базовая проверка - одинаковый ItemID
    if (SourceInterface->GetItemID() != TargetInterface->GetItemID())
    {
        return false;
    }
    
    // Проверка возможности стакинга из DataTable
    if (MaxStackSize <= 1)
    {
        return false;
    }
    
    return true;
}

// Проверка совместимости runtime свойств
bool FSuspenseStackOperation::AreRuntimePropertiesCompatible() const
{
    if (!SourceItem || !TargetItem)
    {
        return true;
    }
    
    // Проверка прочности для экипируемых предметов
    if (CachedItemData.bIsEquippable && !bAllowDifferentDurability)
    {
        float SourceDurability = SourceInstance.GetDurabilityPercent();
        float TargetDurability = TargetInstance.GetDurabilityPercent();
        
        // Разрешаем стакинг только с близкой прочностью (разница < 10%)
        if (FMath::Abs(SourceDurability - TargetDurability) > 0.1f)
        {
            UE_LOG(LogInventory, VeryVerbose, TEXT("FSuspenseStackOperation: Durability mismatch - Source: %.2f, Target: %.2f"), 
                SourceDurability, TargetDurability);
            return false;
        }
    }
    
    // Проверка патронов для оружия
    if (CachedItemData.bIsWeapon && !bAllowDifferentAmmo)
    {
        int32 SourceAmmo = SourceInstance.GetCurrentAmmo();
        int32 TargetAmmo = TargetInstance.GetCurrentAmmo();
        
        if (SourceAmmo != TargetAmmo)
        {
            UE_LOG(LogInventory, VeryVerbose, TEXT("FSuspenseStackOperation: Ammo mismatch - Source: %d, Target: %d"), 
                SourceAmmo, TargetAmmo);
            return false;
        }
    }
    
    return true;
}

// Проверка весовых ограничений
bool FSuspenseStackOperation::ValidateWeightConstraints() const
{
    if (!IsCrossInventoryStack())
    {
        return true; // Вес не меняется при стакинге в одном инвентаре
    }
    
    float TransferWeight = ItemWeight * AmountToTransfer;
    // Используем правильный метод из интерфейса IMedComInventoryInterface
    return TargetInventory->HasWeightCapacity_Implementation(TransferWeight);
}

// Получение описания операции
FString FSuspenseStackOperation::GetOperationDescription() const
{
    if (IsSplitOperation())
    {
        return FString::Printf(TEXT("Split %d items to new stack"), AmountToTransfer);
    }
    else if (IsCrossInventoryStack())
    {
        return FString::Printf(TEXT("Cross-inventory stack %d items"), AmountToTransfer);
    }
    else
    {
        return FString::Printf(TEXT("Stack %d items"), AmountToTransfer);
    }
}

// Выполнение операции стакинга
bool FSuspenseStackOperation::ExecuteStacking(
    EInventoryErrorCode& OutErrorCode,
    UMedComItemManager* InItemManager)
{
    double StartTime = FPlatformTime::Seconds();
    
    // Финальная валидация
    FString ErrorMessage;
    if (!ValidateStacking(OutErrorCode, ErrorMessage, InItemManager))
    {
        LogOperationDetails(FString::Printf(TEXT("Execution failed validation: %s"), *ErrorMessage), true);
        return false;
    }
    
    // Выполнение для split операции
    if (IsSplitOperation())
    {
        if (!CreateNewStackForSplit(InItemManager))
        {
            OutErrorCode = EInventoryErrorCode::UnknownError;
            LogOperationDetails(TEXT("Failed to create new stack for split"), true);
            return false;
        }
    }
    // Выполнение для обычного стакинга
    else if (TargetItem)
    {
        // Вычисляем фактическое количество для переноса
        ActualTransferred = CalculateMaxTransferAmount();
        
        // Объединяем runtime свойства
        MergeRuntimeProperties();
        
        // Переносим количество
        if (!TransferAmount())
        {
            OutErrorCode = EInventoryErrorCode::UnknownError;
            LogOperationDetails(TEXT("Failed to transfer amount"), true);
            return false;
        }
        
        // Применяем объединенные свойства
        ApplyMergedProperties();
    }
    
    // Обрабатываем истощение источника
    HandleSourceDepletion();
    
    // Обновляем веса инвентарей
    UpdateInventoryWeights();
    
    // Отправляем уведомления
    InventoryComponent->BroadcastInventoryUpdated();
    if (IsCrossInventoryStack() && TargetInventory)
    {
        TargetInventory->BroadcastInventoryUpdated();
    }
    
    bSuccess = true;
    OutErrorCode = EInventoryErrorCode::Success;
    
    ExecutionTime = static_cast<float>(FPlatformTime::Seconds() - StartTime);
    
    LogOperationDetails(FString::Printf(TEXT("Stacking executed successfully in %.3f ms"), 
        ExecutionTime * 1000.0f));
    
    return true;
}

// Объединение runtime свойств
void FSuspenseStackOperation::MergeRuntimeProperties()
{
    if (!SourceItem || !TargetItem || ActualTransferred <= 0)
    {
        return;
    }
    
    // Вычисляем взвешенное среднее для прочности
    if (CachedItemData.bIsEquippable)
    {
        float SourceDurability = SourceInstance.GetCurrentDurability();
        float TargetDurability = TargetInstance.GetCurrentDurability();
        
        float TotalItems = static_cast<float>(TargetInitialAmount + ActualTransferred);
        float SourceWeight = static_cast<float>(ActualTransferred) / TotalItems;
        float TargetWeight = static_cast<float>(TargetInitialAmount) / TotalItems;
        
        AverageDurability = (SourceDurability * SourceWeight) + (TargetDurability * TargetWeight);
        
        UE_LOG(LogInventory, VeryVerbose, TEXT("FSuspenseStackOperation::MergeRuntimeProperties: Average durability: %.2f"), 
            AverageDurability);
    }
    
    // Для оружия берем минимальное количество патронов (безопасный подход)
    if (CachedItemData.bIsWeapon)
    {
        AverageAmmo = static_cast<float>(FMath::Min(
            SourceInstance.GetCurrentAmmo(), 
            TargetInstance.GetCurrentAmmo()
        ));
        
        UE_LOG(LogInventory, VeryVerbose, TEXT("FSuspenseStackOperation::MergeRuntimeProperties: Using minimum ammo: %.0f"), 
            AverageAmmo);
    }
}

// Применение объединенных свойств
void FSuspenseStackOperation::ApplyMergedProperties()
{
    if (!TargetItem || ActualTransferred <= 0)
    {
        return;
    }
    
    // Получаем интерфейс цели
    IMedComInventoryItemInterface* TargetInterface = Cast<IMedComInventoryItemInterface>(TargetItem);
    if (!TargetInterface)
    {
        return;
    }
    
    // Применяем среднюю прочность
    if (CachedItemData.bIsEquippable && AverageDurability > 0)
    {
        TargetInstance.SetCurrentDurability(AverageDurability);
    }
    
    // Применяем минимальные патроны
    if (CachedItemData.bIsWeapon && AverageAmmo >= 0)
    {
        TargetInstance.SetCurrentAmmo(static_cast<int32>(AverageAmmo));
    }
    
    // Обновляем время использования
    TargetInstance.LastUsedTime = FPlatformTime::Seconds();
    
    // Применяем обновленный экземпляр
    TargetInterface->SetItemInstance(TargetInstance);
}

// Перенос количества
bool FSuspenseStackOperation::TransferAmount()
{
    if (!SourceItem || !TargetItem || ActualTransferred <= 0)
    {
        return false;
    }
    
    // Получаем интерфейсы
    IMedComInventoryItemInterface* SourceInterface = Cast<IMedComInventoryItemInterface>(SourceItem);
    IMedComInventoryItemInterface* TargetInterface = Cast<IMedComInventoryItemInterface>(TargetItem);
    
    if (!SourceInterface || !TargetInterface)
    {
        return false;
    }
    
    // Обновляем количества
    int32 NewSourceAmount = SourceInitialAmount - ActualTransferred;
    int32 NewTargetAmount = TargetInitialAmount + ActualTransferred;
    
    // Применяем новые количества
    bool bSourceSuccess = SourceInterface->TrySetAmount(NewSourceAmount);
    bool bTargetSuccess = TargetInterface->TrySetAmount(NewTargetAmount);
    
    if (!bSourceSuccess || !bTargetSuccess)
    {
        // Откатываем изменения при ошибке
        SourceInterface->TrySetAmount(SourceInitialAmount);
        TargetInterface->TrySetAmount(TargetInitialAmount);
        return false;
    }
    
    // Отмечаем истощение источника
    bSourceDepleted = (NewSourceAmount <= 0);
    
    LogOperationDetails(FString::Printf(TEXT("Transferred %d items: Source %d->%d, Target %d->%d"), 
        ActualTransferred, SourceInitialAmount, NewSourceAmount, 
        TargetInitialAmount, NewTargetAmount));
    
    return true;
}

// Обработка истощения источника
void FSuspenseStackOperation::HandleSourceDepletion()
{
    if (!bSourceDepleted || !SourceItem || !InventoryComponent)
    {
        return;
    }
    
    // Получаем ItemID для удаления
    const IMedComInventoryItemInterface* SourceInterface = Cast<IMedComInventoryItemInterface>(SourceItem);
    if (!SourceInterface)
    {
        return;
    }
    
    FName ItemID = SourceInterface->GetItemID();
    
    // Удаляем предмет из инвентаря через правильный метод интерфейса
    // Поскольку количество уже 0, RemoveItem удалит предмет полностью
    InventoryComponent->RemoveItem(ItemID, 0);
    
    // Уничтожаем актор
    if (AActor* SourceActor = Cast<AActor>(SourceItem))
    {
        SourceActor->Destroy();
        LogOperationDetails(TEXT("Source item depleted and destroyed"));
    }
}

// Создание нового стека для split
bool FSuspenseStackOperation::CreateNewStackForSplit(UMedComItemManager* InItemManager)
{
    if (!SourceItem || !InventoryComponent || !InItemManager)
    {
        return false;
    }
    
    // TODO: Implement split stack creation
    // Это требует создания нового актора предмета, что выходит за рамки операции
    // Должно быть реализовано через систему транзакций
    
    LogOperationDetails(TEXT("Split operation not fully implemented"), true);
    return false;
}

// Обновление весов инвентарей
void FSuspenseStackOperation::UpdateInventoryWeights()
{
    if (IsCrossInventoryStack() && ActualTransferred > 0)
    {
        // Вес автоматически обновляется при изменении предметов через интерфейс
        // Но мы можем форсировать обновление UI через BroadcastInventoryUpdated
        InventoryComponent->BroadcastInventoryUpdated();
        if (TargetInventory)
        {
            TargetInventory->BroadcastInventoryUpdated();
        }
    }
}

// Логирование операции
void FSuspenseStackOperation::LogOperationDetails(const FString& Message, bool bIsError) const
{
    FString ItemName = TEXT("None");
    if (SourceItem)
    {
        if (const IMedComInventoryItemInterface* SourceInterface = Cast<IMedComInventoryItemInterface>(SourceItem))
        {
            ItemName = SourceInterface->GetItemID().ToString();
        }
    }
    
    const FString Details = FString::Printf(
        TEXT("[StackOp] %s - Item: %s, Transfer: %d/%d, CrossInv: %s, Split: %s - %s"),
        *GetOperationDescription(),
        *ItemName,
        ActualTransferred,
        AmountToTransfer,
        IsCrossInventoryStack() ? TEXT("Yes") : TEXT("No"),
        IsSplitOperation() ? TEXT("Yes") : TEXT("No"),
        *Message
    );
    
    if (bIsError)
    {
        UE_LOG(LogInventory, Error, TEXT("%s"), *Details);
    }
    else
    {
        UE_LOG(LogInventory, Log, TEXT("%s"), *Details);
    }
}

// Реализация Undo
bool FSuspenseStackOperation::CanUndo() const
{
    return bSuccess && SourceItem != nullptr && !bSourceDepleted;
}

bool FSuspenseStackOperation::Undo()
{
    if (!CanUndo())
    {
        return false;
    }
    
    // Получаем интерфейсы
    IMedComInventoryItemInterface* SourceInterface = Cast<IMedComInventoryItemInterface>(SourceItem);
    if (!SourceInterface)
    {
        return false;
    }
    
    // Восстанавливаем исходные количества
    bool bSourceSuccess = SourceInterface->TrySetAmount(SourceInitialAmount);
    bool bTargetSuccess = true;
    
    if (TargetItem)
    {
        IMedComInventoryItemInterface* TargetInterface = Cast<IMedComInventoryItemInterface>(TargetItem);
        if (TargetInterface)
        {
            bTargetSuccess = TargetInterface->TrySetAmount(TargetInitialAmount);
            
            // Восстанавливаем исходные runtime свойства
            TargetInterface->SetItemInstance(TargetInstance);
        }
    }
    
    // Обновляем веса если нужно
    UpdateInventoryWeights();
    
    LogOperationDetails(TEXT("Operation undone"));
    
    return bSourceSuccess && bTargetSuccess;
}

// Реализация Redo
bool FSuspenseStackOperation::CanRedo() const
{
    return SourceItem != nullptr && (TargetItem != nullptr || IsSplitOperation());
}

bool FSuspenseStackOperation::Redo()
{
    if (!CanRedo())
    {
        return false;
    }
    
    // Получаем ItemManager
    UMedComItemManager* ItemManager = nullptr;
    if (UWorld* World = InventoryComponent->GetWorld())
    {
        if (UGameInstance* GameInstance = World->GetGameInstance())
        {
            ItemManager = GameInstance->GetSubsystem<UMedComItemManager>();
        }
    }
    
    if (!ItemManager)
    {
        LogOperationDetails(TEXT("Redo failed - ItemManager not available"), true);
        return false;
    }
    
    // Повторяем операцию с уникальным именем для локальной переменной
    EInventoryErrorCode RedoErrorCode;
    return ExecuteStacking(RedoErrorCode, ItemManager);
}
// Преобразование в строку
FString FSuspenseStackOperation::ToString() const
{
    FString ItemName = TEXT("None");
    int32 CurrentSourceAmount = 0;
    int32 CurrentTargetAmount = 0;
    
    // Получаем информацию о источнике
    if (SourceItem)
    {
        if (const IMedComInventoryItemInterface* SourceInterface = Cast<IMedComInventoryItemInterface>(SourceItem))
        {
            ItemName = SourceInterface->GetItemID().ToString();
            CurrentSourceAmount = SourceInterface->GetAmount();
        }
    }
    
    // Получаем информацию о цели
    if (TargetItem)
    {
        if (const IMedComInventoryItemInterface* TargetInterface = Cast<IMedComInventoryItemInterface>(TargetItem))
        {
            CurrentTargetAmount = TargetInterface->GetAmount();
        }
    }
    
    return FString::Printf(
        TEXT("StackOp[%s, Item=%s, Source=%d->%d, Target=%d->%d, Transfer=%d(%d), MaxStack=%d, CrossInv=%s, Success=%s]"),
        *GetOperationDescription(),
        *ItemName,
        SourceInitialAmount,
        CurrentSourceAmount,
        TargetInitialAmount,
        CurrentTargetAmount,
        AmountToTransfer,
        ActualTransferred,
        MaxStackSize,
        IsCrossInventoryStack() ? TEXT("Yes") : TEXT("No"),
        bSuccess ? TEXT("Yes") : TEXT("No")
    );
}