// Copyright Suspense Team. All Rights Reserved.

#include "Operations/SuspenseMoveOperation.h"
#include "Base/SuspenseInventoryItem.h"
#include "Components/SuspenseInventoryComponent.h"
#include "ItemSystem/SuspenseItemManager.h"
#include "Base/SuspenseInventoryLogs.h"

// Конструктор с полной инициализацией
FSuspenseMoveOperation::FSuspenseMoveOperation(
    USuspenseInventoryComponent* InComponent, 
    ASuspenseInventoryItem* InItem, 
    int32 InTargetIndex, 
    bool InTargetRotated, 
    USuspenseInventoryComponent* InTargetInventory)
    : FSuspenseInventoryOperation(EInventoryOperationType::Move, InComponent)
    , Item(InItem)
    , TargetIndex(InTargetIndex)
    , bTargetRotated(InTargetRotated)
    , TargetInventory(InTargetInventory ? InTargetInventory : InComponent)
    , OperationTimestamp(FPlatformTime::Seconds())
    , CollisionChecks(0)
{
    if (Item)
    {
        // Сохраняем исходное состояние через интерфейс предмета
        if (const IMedComInventoryItemInterface* ItemInterface = Cast<IMedComInventoryItemInterface>(Item))
        {
            SourceIndex = ItemInterface->GetAnchorIndex();
            bSourceRotated = ItemInterface->IsRotated();
            
            // Получаем runtime экземпляр
            ItemInstance = ItemInterface->GetItemInstance();
        }
        
        // Отмечаем необходимость кэширования данных из DataTable
        bHasCachedData = false;
    }
}

// Статический метод создания с валидацией
FSuspenseMoveOperation FSuspenseMoveOperation::Create(
    USuspenseInventoryComponent* InComponent, 
    ASuspenseInventoryItem* InItem, 
    int32 InTargetIndex, 
    bool InTargetRotated, 
    USuspenseInventoryComponent* InTargetInventory,
    USuspenseItemManager* InItemManager)
{
    FSuspenseMoveOperation Operation(InComponent, InItem, InTargetIndex, InTargetRotated, InTargetInventory);
    
    // Базовая валидация
    if (!InItem)
    {
        Operation.ErrorCode = EInventoryErrorCode::InvalidItem;
        UE_LOG(LogInventory, Error, TEXT("FSuspenseMoveOperation::Create: Invalid item"));
        return Operation;
    }
    
    if (!InComponent)
    {
        Operation.ErrorCode = EInventoryErrorCode::NotInitialized;
        UE_LOG(LogInventory, Error, TEXT("FSuspenseMoveOperation::Create: Invalid component"));
        return Operation;
    }
    
    if (!InItemManager)
    {
        Operation.ErrorCode = EInventoryErrorCode::NotInitialized;
        UE_LOG(LogInventory, Error, TEXT("FSuspenseMoveOperation::Create: ItemManager not available"));
        return Operation;
    }
    
    // Кэшируем данные из DataTable
    if (!Operation.CacheItemDataFromTable(InItemManager))
    {
        Operation.ErrorCode = EInventoryErrorCode::InvalidItem;
        UE_LOG(LogInventory, Error, TEXT("FSuspenseMoveOperation::Create: Failed to cache item data for %s"), 
            *InItem->GetName());
        return Operation;
    }
    
    // Вычисляем эффективные размеры
    Operation.CalculateEffectiveSizes();
    
    // Предварительная валидация
    FString ErrorMessage;
    if (!Operation.ValidateOperation(Operation.ErrorCode, ErrorMessage, InItemManager))
    {
        UE_LOG(LogInventory, Warning, TEXT("FSuspenseMoveOperation::Create: Validation failed - %s"), *ErrorMessage);
    }
    
    return Operation;
}

// Создание с автоматическим определением оптимального поворота
FSuspenseMoveOperation FSuspenseMoveOperation::CreateWithOptimalRotation(
    USuspenseInventoryComponent* InComponent,
    ASuspenseInventoryItem* InItem,
    int32 InTargetIndex,
    USuspenseItemManager* InItemManager)
{
    if (!InComponent || !InItem || !InItemManager)
    {
        FSuspenseMoveOperation InvalidOp;
        InvalidOp.ErrorCode = EInventoryErrorCode::InvalidItem;
        return InvalidOp;
    }
    
    // Получаем размеры сетки инвентаря через компонент
    FVector2D GridSize = InComponent->GetInventorySize();
    
    // Получаем ItemInterface для работы с предметом
    const IMedComInventoryItemInterface* ItemInterface = Cast<IMedComInventoryItemInterface>(InItem);
    if (!ItemInterface)
    {
        FSuspenseMoveOperation InvalidOp;
        InvalidOp.ErrorCode = EInventoryErrorCode::InvalidItem;
        return InvalidOp;
    }
    
    // Определяем текущий размер предмета
    FVector2D BaseSize = ItemInterface->GetBaseGridSize();
    
    // Определяем оптимальный поворот на основе доступного места
    bool bOptimalRotation = false;
    
    // Если предмет не квадратный, проверяем какая ориентация лучше подходит
    if (BaseSize.X != BaseSize.Y)
    {
        // Проверяем помещается ли в обычной ориентации
        bool bNormalFits = InComponent->CanPlaceItemAtSlot(BaseSize, InTargetIndex, true);
        
        // Проверяем повернутую ориентацию
        FVector2D RotatedSize(BaseSize.Y, BaseSize.X);
        bool bRotatedFits = InComponent->CanPlaceItemAtSlot(RotatedSize, InTargetIndex, true);
        
        // Предпочитаем поворот если обычная ориентация не помещается, а повернутая помещается
        if (!bNormalFits && bRotatedFits)
        {
            bOptimalRotation = true;
        }
    }
    
    return Create(InComponent, InItem, InTargetIndex, bOptimalRotation, nullptr, InItemManager);
}

bool FSuspenseMoveOperation::CacheItemDataFromTable(USuspenseItemManager* InItemManager)
{
    if (!InItemManager || !Item)
    {
        return false;
    }
    
    // Получаем ItemID через интерфейс
    const IMedComInventoryItemInterface* ItemInterface = Cast<IMedComInventoryItemInterface>(Item);
    if (!ItemInterface)
    {
        UE_LOG(LogInventory, Error, TEXT("FSuspenseMoveOperation::CacheItemDataFromTable: Item doesn't implement required interface"));
        return false;
    }
    
    FName ItemID = ItemInterface->GetItemID();
    
    // Получаем данные из DataTable
    if (!InItemManager->GetUnifiedItemData(ItemID, CachedItemData))
    {
        UE_LOG(LogInventory, Error, TEXT("FSuspenseMoveOperation::CacheItemDataFromTable: Failed to get data for %s"), 
            *ItemID.ToString());
        return false;
    }
    
    // ИСПРАВЛЕНО: BaseGridSize теперь FIntPoint, сохраняем как целые числа
    BaseGridSize = CachedItemData.GridSize; // Прямое присваивание FIntPoint
    
    // Вычисляем общий вес
    ItemTotalWeight = CachedItemData.Weight * ItemInstance.Quantity;
    
    bHasCachedData = true;
    
    // ИСПРАВЛЕНО: используем %d для целых чисел вместо %.0f
    UE_LOG(LogInventory, VeryVerbose, TEXT("FSuspenseMoveOperation::CacheItemDataFromTable: Cached data for %s - Size: %dx%d, Weight: %.2f"), 
        *ItemID.ToString(), 
        BaseGridSize.X, BaseGridSize.Y,  // Теперь это int32
        ItemTotalWeight);
    
    return true;
}

// Вычисление эффективных размеров
void FSuspenseMoveOperation::CalculateEffectiveSizes()
{
    if (!bHasCachedData)
    {
        UE_LOG(LogInventory, Warning, TEXT("FSuspenseMoveOperation::CalculateEffectiveSizes: No cached data available"));
        return;
    }
    if (bSourceRotated)
    {
        SourceEffectiveSize = FVector2D(BaseGridSize.Y, BaseGridSize.X);
    }
    else
    {
        SourceEffectiveSize = FVector2D(BaseGridSize.X, BaseGridSize.Y);
    }
    
    // Целевой эффективный размер
    if (bTargetRotated)
    {
        TargetEffectiveSize = FVector2D(BaseGridSize.Y, BaseGridSize.X);
    }
    else
    {
        TargetEffectiveSize = FVector2D(BaseGridSize.X, BaseGridSize.Y);
    }
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("FSuspenseMoveOperation::CalculateEffectiveSizes: Source: %.0fx%.0f, Target: %.0fx%.0f"), 
        SourceEffectiveSize.X, SourceEffectiveSize.Y,
        TargetEffectiveSize.X, TargetEffectiveSize.Y);
}

// Получение веса из кэша
float FSuspenseMoveOperation::GetCachedItemWeight() const
{
    return ItemTotalWeight;
}

// Полная валидация операции
bool FSuspenseMoveOperation::ValidateOperation(
    EInventoryErrorCode& OutErrorCode, 
    FString& OutErrorMessage,
    USuspenseItemManager* InItemManager) const
{
    // Проверка базовых компонентов
    if (!Item || !InventoryComponent || !TargetInventory)
    {
        OutErrorCode = EInventoryErrorCode::NotInitialized;
        OutErrorMessage = TEXT("Invalid operation components");
        return false;
    }
    
    // Проверка наличия кэшированных данных
    if (!bHasCachedData)
    {
        OutErrorCode = EInventoryErrorCode::InvalidItem;
        OutErrorMessage = TEXT("No cached item data available");
        return false;
    }
    
    // Проверка индекса
    if (TargetIndex < 0)
    {
        OutErrorCode = EInventoryErrorCode::InvalidSlot;
        OutErrorMessage = TEXT("Invalid target index");
        return false;
    }
    
    // Проверка границ сетки через компонент
    FVector2D GridSize = TargetInventory->GetInventorySize();
    int32 GridWidth = static_cast<int32>(GridSize.X);
    int32 GridHeight = static_cast<int32>(GridSize.Y);
    
    if (TargetIndex >= GridWidth * GridHeight)
    {
        OutErrorCode = EInventoryErrorCode::InvalidSlot;
        OutErrorMessage = FString::Printf(TEXT("Target index %d out of bounds (max: %d)"), 
            TargetIndex, GridWidth * GridHeight - 1);
        return false;
    }
    
    // Проверка размещения в границах с учетом размера предмета через интерфейс компонента
    if (!TargetInventory->CanPlaceItemAtSlot(TargetEffectiveSize, TargetIndex, false))
    {
        OutErrorCode = EInventoryErrorCode::NoSpace;
        OutErrorMessage = TEXT("Item doesn't fit at target position");
        return false;
    }
    
    // Проверка весовых ограничений
    if (!ValidateWeightConstraints())
    {
        OutErrorCode = EInventoryErrorCode::WeightLimit;
        OutErrorMessage = FString::Printf(TEXT("Weight limit exceeded - item weight: %.2f"), ItemTotalWeight);
        return false;
    }
    
    // Проверка типовых ограничений
    if (!ValidateItemTypeConstraints())
    {
        OutErrorCode = EInventoryErrorCode::InvalidItem;
        OutErrorMessage = FString::Printf(TEXT("Item type %s not allowed in target inventory"), 
            *CachedItemData.ItemType.ToString());
        return false;
    }
    
    OutErrorCode = EInventoryErrorCode::Success;
    return true;
}

// Проверка весовых ограничений
bool FSuspenseMoveOperation::ValidateWeightConstraints() const
{
    if (IsCrossInventoryMove() && TargetInventory)
    {
        // При перемещении между инвентарями проверяем вес в целевом через компонент
        return TargetInventory->HasWeightCapacity_Implementation(ItemTotalWeight);
    }
    
    // При перемещении внутри инвентаря вес не меняется
    return true;
}

// Проверка типовых ограничений
bool FSuspenseMoveOperation::ValidateItemTypeConstraints() const
{
    // Если перемещение внутри одного инвентаря, тип уже проверен
    if (!IsCrossInventoryMove())
    {
        return true;
    }
    
    // Проверяем разрешенные типы предметов в целевом инвентаре
    FGameplayTagContainer AllowedTypes = TargetInventory->GetAllowedItemTypes_Implementation();
    if (!AllowedTypes.IsEmpty() && !AllowedTypes.HasTag(CachedItemData.ItemType))
    {
        return false;
    }
    
    return true;
}

// Получение описания операции
FString FSuspenseMoveOperation::GetOperationTypeDescription() const
{
    if (IsCrossInventoryMove())
    {
        return TEXT("Cross-Inventory Move");
    }
    else if (HasRotationChanged() && HasPositionChanged())
    {
        return TEXT("Move and Rotate");
    }
    else if (HasRotationChanged())
    {
        return TEXT("Rotate Only");
    }
    else if (HasPositionChanged())
    {
        return TEXT("Move Only");
    }
    else
    {
        return TEXT("No Change");
    }
}

// Выполнение операции
bool FSuspenseMoveOperation::ExecuteOperation(
    EInventoryErrorCode& OutErrorCode,
    USuspenseItemManager* InItemManager)
{
    // Начинаем с финальной валидации операции
    // Это критически важно для обеспечения целостности данных
    FString ErrorMessage;
    if (!ValidateOperation(OutErrorCode, ErrorMessage, InItemManager))
    {
        LogOperationDetails(FString::Printf(TEXT("Execution failed validation: %s"), *ErrorMessage), true);
        return false;
    }
    
    // В новой архитектуре мы работаем с экземплярами, но все еще можем иметь Item для обратной совместимости
    // Проверяем, что у нас есть валидный экземпляр
    if (!ItemInstance.IsValid())
    {
        OutErrorCode = EInventoryErrorCode::InvalidItem;
        LogOperationDetails(TEXT("Invalid item instance"), true);
        return false;
    }
    
    // Начинаем транзакции для обеспечения атомарности операции
    InventoryComponent->BeginTransaction();
    if (IsCrossInventoryMove())
    {
        TargetInventory->BeginTransaction();
    }
    
    // Используем try-catch для безопасной обработки исключений
    try
    {
        // Проверяем, занята ли целевая позиция другим предметом
        FInventoryItemInstance BlockingInstance;
        bool bHasBlockingItem = TargetInventory->GetItemInstanceAtSlot(TargetIndex, BlockingInstance);
        
        // Если слот занят и это не тот же самый предмет
        if (bHasBlockingItem && BlockingInstance.InstanceID != ItemInstance.InstanceID)
        {
            // В новой архитектуре HandleSwapOperation работает по-другому
            // Мы передаем nullptr вместо BlockingItem, так как больше не работаем с объектами
            // Вместо этого мы сохраняем информацию о блокирующем экземпляре
            SwappedItemInstance = BlockingInstance;
            SwappedItemOriginalIndex = BlockingInstance.AnchorIndex;
            bSwappedItemOriginalRotated = BlockingInstance.bIsRotated;
            
            // Получаем данные блокирующего предмета для проверок
            FMedComUnifiedItemData SwappedItemData;
            if (!InItemManager->GetUnifiedItemData(BlockingInstance.ItemID, SwappedItemData))
            {
                OutErrorCode = EInventoryErrorCode::InvalidItem;
                LogOperationDetails(TEXT("Failed to get data for blocking item"), true);
                
                InventoryComponent->RollbackTransaction();
                if (IsCrossInventoryMove())
                {
                    TargetInventory->RollbackTransaction();
                }
                return false;
            }
            
            // Проверяем возможность размещения блокирующего предмета в исходной позиции
            FVector2D SwappedItemSize(SwappedItemData.GridSize.X, SwappedItemData.GridSize.Y);
            if (bSwappedItemOriginalRotated)
            {
                SwappedItemSize = FVector2D(SwappedItemSize.Y, SwappedItemSize.X);
            }
            
            if (!InventoryComponent->CanPlaceItemAtSlot(SwappedItemSize, SourceIndex, true))
            {
                OutErrorCode = EInventoryErrorCode::NoSpace;
                LogOperationDetails(TEXT("Swap failed - no space for swapped item in source position"), true);
                
                InventoryComponent->RollbackTransaction();
                if (IsCrossInventoryMove())
                {
                    TargetInventory->RollbackTransaction();
                }
                return false;
            }
            
            bWasSwapOperation = true;
            LogOperationDetails(FString::Printf(TEXT("Swap operation prepared with item %s"), 
                *BlockingInstance.ItemID.ToString()));
        }
        
        // Выполняем фактическое перемещение
        bool bMoveSuccess = false;
        
        if (IsCrossInventoryMove())
        {
            // Перемещение между разными инвентарями
            // Сначала удаляем из источника
            FSuspenseInventoryOperationResult RemoveResult = InventoryComponent->RemoveItemByID(
                ItemInstance.ItemID, ItemInstance.Quantity);
            
            if (RemoveResult.IsSuccess())
            {
                // Создаем обновленный экземпляр для целевого инвентаря
                FInventoryItemInstance TargetInstance = ItemInstance;
                TargetInstance.AnchorIndex = TargetIndex;
                TargetInstance.bIsRotated = bTargetRotated;
                
                // Пытаемся добавить в целевой инвентарь
                FSuspenseInventoryOperationResult AddResult = TargetInventory->AddItemInstance(TargetInstance);
                bMoveSuccess = AddResult.IsSuccess();
                
                if (!bMoveSuccess)
                {
                    // Если добавление не удалось, возвращаем предмет обратно
                    FInventoryItemInstance RestoreInstance = ItemInstance;
                    RestoreInstance.AnchorIndex = SourceIndex;
                    RestoreInstance.bIsRotated = bSourceRotated;
                    InventoryComponent->AddItemInstance(RestoreInstance);
                    
                    OutErrorCode = AddResult.ErrorCode;
                }
            }
            else
            {
                OutErrorCode = RemoveResult.ErrorCode;
            }
        }
        else
        {
            // Внутреннее перемещение в пределах одного инвентаря
            if (bWasSwapOperation)
            {
                // Если это обмен, нам нужно обработать его особым образом
                // Используем SwapItemsInSlots для атомарного обмена
                EInventoryErrorCode SwapError;
                bMoveSuccess = InventoryComponent->SwapItemsInSlots(SourceIndex, TargetIndex, SwapError);
                
                if (!bMoveSuccess)
                {
                    OutErrorCode = SwapError;
                    LogOperationDetails(TEXT("Swap operation failed"), true);
                }
            }
            else
            {
                // Простое перемещение без обмена
                bMoveSuccess = InventoryComponent->MoveItemBySlots_Implementation(
                    SourceIndex, TargetIndex, !HasRotationChanged());
            }
        }
        
        if (bMoveSuccess)
        {
            // Операция успешна - обновляем данные экземпляра
            ItemInstance.AnchorIndex = TargetIndex;
            ItemInstance.bIsRotated = bTargetRotated;
            ItemInstance.LastUsedTime = FPlatformTime::Seconds();
            
            // Если у нас все еще есть Item (для обратной совместимости), обновляем его
            if (Item)
            {
                if (IMedComInventoryItemInterface* ItemInterface = Cast<IMedComInventoryItemInterface>(Item))
                {
                    ItemInterface->SetAnchorIndex(TargetIndex);
                    ItemInterface->SetRotated(bTargetRotated);
                    ItemInterface->SetItemInstance(ItemInstance);
                }
            }
            
            // Коммитим все транзакции
            InventoryComponent->CommitTransaction();
            if (IsCrossInventoryMove())
            {
                TargetInventory->CommitTransaction();
            }
            
            // Отправляем уведомления об изменениях
            if (IsCrossInventoryMove())
            {
                InventoryComponent->BroadcastInventoryUpdated();
                TargetInventory->BroadcastInventoryUpdated();
            }
            else
            {
                InventoryComponent->BroadcastInventoryUpdated();
            }
            
            // Помечаем операцию как успешную
            bSuccess = true;
            OutErrorCode = EInventoryErrorCode::Success;
            
            LogOperationDetails(FString::Printf(TEXT("Operation executed successfully (%s)"), 
                *GetOperationTypeDescription()));
            
            return true;
        }
        else
        {
            // Перемещение не удалось - откатываем все изменения
            InventoryComponent->RollbackTransaction();
            if (IsCrossInventoryMove())
            {
                TargetInventory->RollbackTransaction();
            }
            
            // Если код ошибки еще не установлен, используем общий
            if (OutErrorCode == EInventoryErrorCode::Success)
            {
                OutErrorCode = EInventoryErrorCode::UnknownError;
            }
            
            LogOperationDetails(TEXT("Failed to execute move operation"), true);
            return false;
        }
    }
    catch (...)
    {
        // Обработка непредвиденных исключений
        InventoryComponent->RollbackTransaction();
        if (IsCrossInventoryMove())
        {
            TargetInventory->RollbackTransaction();
        }
        
        OutErrorCode = EInventoryErrorCode::UnknownError;
        LogOperationDetails(TEXT("Exception during operation execution"), true);
        return false;
    }
}

// Обработка операции обмена
bool FSuspenseMoveOperation::HandleSwapOperation(
    ASuspenseInventoryItem* BlockingItem, 
    EInventoryErrorCode& OutErrorCode,
    USuspenseItemManager* InItemManager)
{
    if (!BlockingItem || !InItemManager)
    {
        OutErrorCode = EInventoryErrorCode::InvalidItem;
        return false;
    }
    
    // ИСПРАВЛЕНО: Работаем через FInventoryItemInstance вместо UObject*
    // Получаем экземпляр предмета в целевом слоте
    FInventoryItemInstance TargetInstance;
    if (!TargetInventory->GetItemInstanceAtSlot(TargetIndex, TargetInstance))
    {
        // Нет предмета в целевом слоте - обмен не требуется
        OutErrorCode = EInventoryErrorCode::Success;
        return true;
    }
    
    // Проверяем, что это действительно другой предмет
    if (TargetInstance.InstanceID == ItemInstance.InstanceID)
    {
        // Это тот же самый предмет - обмен не нужен
        OutErrorCode = EInventoryErrorCode::Success;
        return true;
    }
    
    // Сохраняем информацию об обмениваемом предмете
    // В новой архитектуре мы работаем с экземплярами, а не с акторами
    SwappedItem = nullptr; // Больше не используем актор
    SwappedItemInstance = TargetInstance;
    SwappedItemOriginalIndex = TargetIndex;
    bSwappedItemOriginalRotated = TargetInstance.bIsRotated;
    
    // Получаем данные обмениваемого предмета из DataTable
    FMedComUnifiedItemData SwappedItemData;
    if (!InItemManager->GetUnifiedItemData(SwappedItemInstance.ItemID, SwappedItemData))
    {
        OutErrorCode = EInventoryErrorCode::InvalidItem;
        LogOperationDetails(FString::Printf(TEXT("Failed to get data for swapped item %s"), 
            *SwappedItemInstance.ItemID.ToString()), true);
        return false;
    }
    
    // Вычисляем размер обмениваемого предмета с учетом поворота
    FVector2D SwappedItemSize(SwappedItemData.GridSize.X, SwappedItemData.GridSize.Y);
    if (bSwappedItemOriginalRotated)
    {
        SwappedItemSize = FVector2D(SwappedItemSize.Y, SwappedItemSize.X);
    }
    
    // Проверяем возможность размещения обмениваемого предмета в исходной позиции
    // Используем метод CanPlaceItemAtSlot, который все еще доступен
    if (!InventoryComponent->CanPlaceItemAtSlot(SwappedItemSize, SourceIndex, true))
    {
        OutErrorCode = EInventoryErrorCode::NoSpace;
        LogOperationDetails(TEXT("Swap failed - no space for swapped item in source position"), true);
        return false;
    }
    
    // Проверяем весовые ограничения для обмена
    if (IsCrossInventoryMove())
    {
        float SwappedItemWeight = SwappedItemData.Weight * SwappedItemInstance.Quantity;
        float WeightDelta = ItemTotalWeight - SwappedItemWeight;
        
        if (WeightDelta > 0 && !TargetInventory->HasWeightCapacity_Implementation(WeightDelta))
        {
            OutErrorCode = EInventoryErrorCode::WeightLimit;
            LogOperationDetails(TEXT("Swap failed - weight limit exceeded"), true);
            return false;
        }
    }
    
    bWasSwapOperation = true;
    LogOperationDetails(FString::Printf(TEXT("Swap operation prepared with item %s"), 
        *SwappedItemInstance.ItemID.ToString()));
    
    return true;
}

// Применение нового состояния
void FSuspenseMoveOperation::ApplyNewState()
{
    if (!Item)
    {
        return;
    }
    
    // Получаем интерфейс предмета
    IMedComInventoryItemInterface* ItemInterface = Cast<IMedComInventoryItemInterface>(Item);
    if (!ItemInterface)
    {
        return;
    }
    
    // Применяем поворот если нужно
    if (bSourceRotated != bTargetRotated)
    {
        ItemInterface->SetRotated(bTargetRotated);
        UE_LOG(LogInventory, VeryVerbose, TEXT("Applied rotation change: %s -> %s"), 
            bSourceRotated ? TEXT("Rotated") : TEXT("Normal"),
            bTargetRotated ? TEXT("Rotated") : TEXT("Normal"));
    }
    
    // Обновляем anchor index
    ItemInterface->SetAnchorIndex(TargetIndex);
    
    LogOperationDetails(FString::Printf(TEXT("Applied new state - Rotated: %s"), 
        bTargetRotated ? TEXT("Yes") : TEXT("No")));
}

// Восстановление исходного состояния
void FSuspenseMoveOperation::RestoreOriginalState()
{
    if (!Item)
    {
        return;
    }
    
    IMedComInventoryItemInterface* ItemInterface = Cast<IMedComInventoryItemInterface>(Item);
    if (!ItemInterface)
    {
        return;
    }
    
    // Восстанавливаем исходный поворот
    ItemInterface->SetRotated(bSourceRotated);
    
    // Восстанавливаем исходный anchor index
    ItemInterface->SetAnchorIndex(SourceIndex);
}

// Обновление runtime свойств
void FSuspenseMoveOperation::UpdateRuntimeProperties()
{
    if (!Item)
    {
        return;
    }
    
    IMedComInventoryItemInterface* ItemInterface = Cast<IMedComInventoryItemInterface>(Item);
    if (!ItemInterface)
    {
        return;
    }
    
    // Обновляем время последнего перемещения
    ItemInstance.LastUsedTime = FPlatformTime::Seconds();
    ItemInstance.AnchorIndex = TargetIndex;
    ItemInstance.bIsRotated = bTargetRotated;
    
    // Применяем обновленный экземпляр к актору
    ItemInterface->SetItemInstance(ItemInstance);
    
    LogOperationDetails(TEXT("Updated runtime properties"));
}

// Логирование операции
void FSuspenseMoveOperation::LogOperationDetails(const FString& Message, bool bIsError) const
{
    // ИСПРАВЛЕНО: Используем другое имя для локальной переменной, чтобы избежать затенения
    const FString OperationTypeStr = GetOperationTypeDescription();
    FString ItemName = TEXT("None");
    
    if (Item)
    {
        if (const IMedComInventoryItemInterface* ItemInterface = Cast<IMedComInventoryItemInterface>(Item))
        {
            ItemName = ItemInterface->GetItemID().ToString();
        }
        else
        {
            ItemName = Item->GetName();
        }
    }
    
    const FString Details = FString::Printf(
        TEXT("[MoveOp] %s - Item: %s, Source: %d, Target: %d, CrossInv: %s, Swap: %s - %s"),
        *OperationTypeStr,  // Используем переименованную переменную
        *ItemName,
        SourceIndex,
        TargetIndex,
        IsCrossInventoryMove() ? TEXT("Yes") : TEXT("No"),
        bWasSwapOperation ? TEXT("Yes") : TEXT("No"),
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
bool FSuspenseMoveOperation::CanUndo() const
{
    return bSuccess && Item != nullptr && InventoryComponent != nullptr && TargetInventory != nullptr;
}

bool FSuspenseMoveOperation::Undo()
{
    if (!CanUndo())
    {
        return false;
    }
    
    // Получаем item interface
    IMedComInventoryItemInterface* ItemInterface = Cast<IMedComInventoryItemInterface>(Item);
    if (!ItemInterface)
    {
        LogOperationDetails(TEXT("Undo failed - item interface not available"), true);
        return false;
    }
    
    // Начинаем транзакции для отката
    InventoryComponent->BeginTransaction();
    if (IsCrossInventoryMove())
    {
        TargetInventory->BeginTransaction();
    }
    
    try
    {
        bool bUndoSuccess = false;
        
        if (IsCrossInventoryMove())
        {
            // Cross-inventory undo - удаляем из цели и добавляем в источник
            if (TargetInventory->RemoveItem(ItemInstance.ItemID, ItemInstance.Quantity))
            {
                // Восстанавливаем исходное состояние
                RestoreOriginalState();
                
                // Создаем instance для восстановления в источнике
                FInventoryItemInstance RestoreInstance = ItemInstance;
                RestoreInstance.AnchorIndex = SourceIndex;
                RestoreInstance.bIsRotated = bSourceRotated;
                
                FSuspenseInventoryOperationResult AddResult = InventoryComponent->AddItemInstance(RestoreInstance);
                bUndoSuccess = AddResult.IsSuccess();
            }
        }
        else
        {
            // Внутренний undo - перемещаем обратно
            bUndoSuccess = InventoryComponent->MoveItemBySlots_Implementation(
                TargetIndex, SourceIndex, true);
            
            if (bUndoSuccess)
            {
                // Восстанавливаем исходное состояние поворота
                RestoreOriginalState();
            }
        }
        
        // Если был своп, восстанавливаем обмененный предмет
        if (bUndoSuccess && bWasSwapOperation && SwappedItem)
        {
            // Используем SwapItems для восстановления свопа
            EInventoryErrorCode SwapErrorCode;
            if (!InventoryComponent->SwapItemsInSlots(SourceIndex, SwappedItemOriginalIndex, SwapErrorCode))
            {
                LogOperationDetails(TEXT("Warning: Failed to restore swapped item during undo"), false);
            }
        }
        
        if (bUndoSuccess)
        {
            // Коммитим транзакции
            InventoryComponent->CommitTransaction();
            if (IsCrossInventoryMove())
            {
                TargetInventory->CommitTransaction();
            }
            
            LogOperationDetails(TEXT("Operation undone"));
            return true;
        }
        else
        {
            // Откатываем транзакции при неудаче
            InventoryComponent->RollbackTransaction();
            if (IsCrossInventoryMove())
            {
                TargetInventory->RollbackTransaction();
            }
            
            LogOperationDetails(TEXT("Undo failed"), true);
            return false;
        }
    }
    catch (...)
    {
        // Откатываем при исключении
        InventoryComponent->RollbackTransaction();
        if (IsCrossInventoryMove())
        {
            TargetInventory->RollbackTransaction();
        }
        
        LogOperationDetails(TEXT("Exception during undo operation"), true);
        return false;
    }
}

// Реализация Redo
bool FSuspenseMoveOperation::CanRedo() const
{
    return Item != nullptr && InventoryComponent != nullptr && TargetInventory != nullptr;
}

bool FSuspenseMoveOperation::Redo()
{
    if (!CanRedo())
    {
        return false;
    }
    
    // ИСПРАВЛЕНО: используем другое имя для локальной переменной
    EInventoryErrorCode RedoErrorCode;
    USuspenseItemManager* ItemManager = InventoryComponent->GetItemManager();
    
    if (!ItemManager)
    {
        LogOperationDetails(TEXT("Redo failed - ItemManager not available"), true);
        return false;
    }
    
    return ExecuteOperation(RedoErrorCode, ItemManager);
}

// Преобразование в строку
FString FSuspenseMoveOperation::ToString() const
{
    FString ItemName = TEXT("None");
    
    if (Item)
    {
        if (const IMedComInventoryItemInterface* ItemInterface = Cast<IMedComInventoryItemInterface>(Item))
        {
            ItemName = ItemInterface->GetItemID().ToString();
        }
        else
        {
            ItemName = Item->GetName();
        }
    }
    
    return FString::Printf(
        TEXT("MoveOp[%s, Item=%s, Src=%d(%.0fx%.0f), Tgt=%d(%.0fx%.0f), Weight=%.2f, CrossInv=%s, Swap=%s, Success=%s]"),
        *GetOperationTypeDescription(),
        *ItemName,
        SourceIndex,
        SourceEffectiveSize.X, SourceEffectiveSize.Y,
        TargetIndex,
        TargetEffectiveSize.X, TargetEffectiveSize.Y,
        ItemTotalWeight,
        IsCrossInventoryMove() ? TEXT("Yes") : TEXT("No"),
        bWasSwapOperation ? TEXT("Yes") : TEXT("No"),
        bSuccess ? TEXT("Yes") : TEXT("No")
    );
}