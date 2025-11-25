// Copyright Suspense Team. All Rights Reserved.

#include "Operations/SuspenseRotationOperation.h"
#include "Base/SuspenseInventoryItem.h"
#include "Components/SuspenseInventoryComponent.h"
// ИСПРАВЛЕНО: добавляем полный include для USuspenseInventoryStorage
#include "Storage/SuspenseInventoryStorage.h"
#include "ItemSystem/MedComItemManager.h"
#include "Base/SuspenseSuspenseInventoryLogs.h"

// Основной конструктор
FSuspenseRotationOperation::FSuspenseRotationOperation(
    USuspenseInventoryComponent* InComponent,
    AMedComInventoryItem* InItem,
    bool InTargetRotation)
    : FSuspenseInventoryOperation(EInventoryOperationType::Rotate, InComponent)
    , Item(InItem)
    , bTargetRotation(InTargetRotation)
{
    if (Item)
    {
        // Получаем интерфейс для доступа к свойствам предмета
        if (const IMedComInventoryItemInterface* ItemInterface = Cast<IMedComInventoryItemInterface>(Item))
        {
            // Сохраняем исходное состояние
            bInitialRotation = ItemInterface->IsRotated();
            AnchorIndex = ItemInterface->GetAnchorIndex();
            
            // Получаем runtime экземпляр
            ItemInstance = ItemInterface->GetItemInstance();
        }
    }
}

// Статический метод создания базовый
FSuspenseRotationOperation FSuspenseRotationOperation::Create(
    AMedComInventoryItem* InItem, 
    bool InTargetRotation,
    UMedComItemManager* InItemManager)
{
    FSuspenseRotationOperation Operation;
    
    // Базовая валидация
    if (!InItem)
    {
        Operation.ErrorCode = EInventoryErrorCode::InvalidItem;
        UE_LOG(LogInventory, Error, TEXT("FSuspenseRotationOperation::Create: Invalid item"));
        return Operation;
    }
    
    if (!InItemManager)
    {
        Operation.ErrorCode = EInventoryErrorCode::NotInitialized;
        UE_LOG(LogInventory, Error, TEXT("FSuspenseRotationOperation::Create: ItemManager not available"));
        return Operation;
    }
    
    // Получаем интерфейс предмета
    const IMedComInventoryItemInterface* ItemInterface = Cast<IMedComInventoryItemInterface>(InItem);
    if (!ItemInterface)
    {
        Operation.ErrorCode = EInventoryErrorCode::InvalidItem;
        UE_LOG(LogInventory, Error, TEXT("FSuspenseRotationOperation::Create: Item doesn't implement required interface"));
        return Operation;
    }
    
    // Инициализация базовых полей
    Operation.Item = InItem;
    Operation.bInitialRotation = ItemInterface->IsRotated();
    Operation.bTargetRotation = InTargetRotation;
    Operation.AnchorIndex = ItemInterface->GetAnchorIndex();
    Operation.ItemInstance = ItemInterface->GetItemInstance();
    
    // Кэширование данных из DataTable
    if (!Operation.CacheItemDataFromTable(InItemManager))
    {
        Operation.ErrorCode = EInventoryErrorCode::InvalidItem;
        UE_LOG(LogInventory, Error, TEXT("FSuspenseRotationOperation::Create: Failed to cache item data"));
        return Operation;
    }
    
    // Вычисление размеров
    Operation.CalculateEffectiveSizes();
    
    // Предварительная валидация
    FString ErrorMessage;
    // ИСПРАВЛЕНО: используем временную переменную для избежания конфликта имен
    EInventoryErrorCode ValidationError;
    if (!Operation.ValidateRotation(ValidationError, ErrorMessage))
    {
        Operation.ErrorCode = ValidationError;
        UE_LOG(LogInventory, Warning, TEXT("FSuspenseRotationOperation::Create: Validation failed - %s"), *ErrorMessage);
    }
    
    return Operation;
}

// Статический метод создания с компонентом
FSuspenseRotationOperation FSuspenseRotationOperation::Create(
    USuspenseInventoryComponent* InComponent, 
    AMedComInventoryItem* InItem, 
    bool InTargetRotation,
    UMedComItemManager* InItemManager)
{
    FSuspenseRotationOperation Operation = Create(InItem, InTargetRotation, InItemManager);
    Operation.InventoryComponent = InComponent;
    
    // Дополнительная валидация с учетом компонента
    if (InComponent && Operation.ErrorCode == EInventoryErrorCode::Success)
    {
        Operation.CalculateTargetCells();
    }
    
    return Operation;
}

// Создание операции переключения
FSuspenseRotationOperation FSuspenseRotationOperation::CreateToggle(
    USuspenseInventoryComponent* InComponent,
    AMedComInventoryItem* InItem,
    UMedComItemManager* InItemManager)
{
    if (!InItem)
    {
        FSuspenseRotationOperation InvalidOp;
        InvalidOp.ErrorCode = EInventoryErrorCode::InvalidItem;
        return InvalidOp;
    }
    
    // Получаем интерфейс для доступа к текущему состоянию
    const IMedComInventoryItemInterface* ItemInterface = Cast<IMedComInventoryItemInterface>(InItem);
    if (!ItemInterface)
    {
        FSuspenseRotationOperation InvalidOp;
        InvalidOp.ErrorCode = EInventoryErrorCode::InvalidItem;
        return InvalidOp;
    }
    
    // Создаем операцию с противоположным состоянием
    bool bCurrentRotation = ItemInterface->IsRotated();
    return Create(InComponent, InItem, !bCurrentRotation, InItemManager);
}

// Кэширование данных из DataTable
bool FSuspenseRotationOperation::CacheItemDataFromTable(UMedComItemManager* InItemManager)
{
    if (!InItemManager || !Item)
    {
        return false;
    }
    
    // Получаем интерфейс предмета
    const IMedComInventoryItemInterface* ItemInterface = Cast<IMedComInventoryItemInterface>(Item);
    if (!ItemInterface)
    {
        UE_LOG(LogInventory, Error, TEXT("FSuspenseRotationOperation::CacheItemDataFromTable: Item doesn't implement required interface"));
        return false;
    }
    
    // Получаем ItemID через интерфейс
    FName ItemID = ItemInterface->GetItemID();
    
    // Получаем данные из DataTable
    if (!InItemManager->GetUnifiedItemData(ItemID, CachedItemData))
    {
        UE_LOG(LogInventory, Error, TEXT("FSuspenseRotationOperation::CacheItemDataFromTable: Failed to get data for %s"), 
            *ItemID.ToString());
        return false;
    }
    
    // Сохраняем базовый размер (корректная конвертация FIntPoint в FVector2D)
    BaseGridSize.X = static_cast<int32>(CachedItemData.GridSize.X);
    BaseGridSize.Y = static_cast<int32>(CachedItemData.GridSize.Y);
    
    bHasCachedData = true;
    
    // ИСПРАВЛЕНО: используем %d для int32 вместо %.0f
    UE_LOG(LogInventory, VeryVerbose, TEXT("FSuspenseRotationOperation::CacheItemDataFromTable: Cached data for %s - Size: %dx%d"), 
        *ItemID.ToString(), BaseGridSize.X, BaseGridSize.Y);
    
    return true;
}

// Вычисление эффективных размеров
void FSuspenseRotationOperation::CalculateEffectiveSizes()
{
    if (!bHasCachedData)
    {
        UE_LOG(LogInventory, Warning, TEXT("FSuspenseRotationOperation::CalculateEffectiveSizes: No cached data"));
        return;
    }
    
    // Исходный размер
    if (bInitialRotation)
    {
        InitialEffectiveSize = FVector2D(BaseGridSize.Y, BaseGridSize.X);
    }
    else
    {
        InitialEffectiveSize = FVector2D(BaseGridSize.X, BaseGridSize.Y);
    }
    
    // Целевой размер
    if (bTargetRotation)
    {
        TargetEffectiveSize = FVector2D(BaseGridSize.Y, BaseGridSize.X);
    }
    else
    {
        TargetEffectiveSize = FVector2D(BaseGridSize.X, BaseGridSize.Y);
    }
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("FSuspenseRotationOperation::CalculateEffectiveSizes: Initial: %.0fx%.0f, Target: %.0fx%.0f"), 
        InitialEffectiveSize.X, InitialEffectiveSize.Y,
        TargetEffectiveSize.X, TargetEffectiveSize.Y);
}

// Полная валидация операции
bool FSuspenseRotationOperation::ValidateRotation(
    EInventoryErrorCode& OutErrorCode,
    FString& OutErrorMessage) const
{
    // Проверка базовых компонентов
    if (!Item)
    {
        OutErrorCode = EInventoryErrorCode::InvalidItem;
        OutErrorMessage = TEXT("Invalid item");
        return false;
    }
    
    // Проверка кэшированных данных
    if (!bHasCachedData)
    {
        OutErrorCode = EInventoryErrorCode::InvalidItem;
        OutErrorMessage = TEXT("No cached item data");
        return false;
    }
    
    // Проверка изменения состояния
    if (!HasRotationChanged())
    {
        OutErrorCode = EInventoryErrorCode::Success;
        OutErrorMessage = TEXT("No rotation change needed");
        return true; // Технически это не ошибка
    }
    
    // Проверка размещения в инвентаре
    if (AnchorIndex == INDEX_NONE)
    {
        OutErrorCode = EInventoryErrorCode::InvalidSlot;
        OutErrorMessage = TEXT("Item not placed in inventory");
        return false;
    }
    
    // Проверка компонента инвентаря для детальной валидации
    if (InventoryComponent)
    {
        // Проверка границ после поворота
        FVector2D GridSize = InventoryComponent->GetInventorySize();
        int32 GridWidth = static_cast<int32>(GridSize.X);
        int32 GridHeight = static_cast<int32>(GridSize.Y);
        
        int32 AnchorX = AnchorIndex % GridWidth;
        int32 AnchorY = AnchorIndex / GridWidth;
        
        if (AnchorX + TargetEffectiveSize.X > GridWidth || 
            AnchorY + TargetEffectiveSize.Y > GridHeight)
        {
            OutErrorCode = EInventoryErrorCode::NoSpace;
            OutErrorMessage = FString::Printf(
                TEXT("Item would extend beyond grid bounds after rotation (pos: %d,%d, size: %.0fx%.0f, grid: %dx%d)"),
                AnchorX, AnchorY,
                TargetEffectiveSize.X, TargetEffectiveSize.Y,
                GridWidth, GridHeight
            );
            return false;
        }
        
        // Проверка коллизий
        if (!CheckCollisions())
        {
            OutErrorCode = EInventoryErrorCode::SlotOccupied;
            OutErrorMessage = TEXT("Target cells are occupied");
            return false;
        }
    }
    
    OutErrorCode = EInventoryErrorCode::Success;
    return true;
}

bool FSuspenseRotationOperation::CheckCollisions() const
{
    if (!InventoryComponent || TargetOccupiedCells.Num() == 0)
    {
        return true; // Нет данных для проверки
    }
    
    // ИСПРАВЛЕНО: Используем GetItemInstanceAtSlot вместо GetItemAtSlot
    // Проверяем что все целевые ячейки свободны или заняты текущим предметом
    for (int32 CellIndex : TargetOccupiedCells)
    {
        FInventoryItemInstance InstanceAtCell;
        if (InventoryComponent->GetItemInstanceAtSlot(CellIndex, InstanceAtCell))
        {
            // Ячейка занята - проверяем, не наш ли это предмет
            if (InstanceAtCell.InstanceID != ItemInstance.InstanceID)
            {
                // Ячейка занята другим предметом
                return false;
            }
        }
        // Если GetItemInstanceAtSlot вернул false, ячейка свободна
    }
    
    return true;
}

// Вычисление целевых ячеек
void FSuspenseRotationOperation::CalculateTargetCells()
{
    TargetOccupiedCells.Empty();
    
    if (!InventoryComponent || AnchorIndex == INDEX_NONE)
    {
        return;
    }
    
    FVector2D GridSize = InventoryComponent->GetInventorySize();
    int32 GridWidth = static_cast<int32>(GridSize.X);
    
    int32 AnchorX = AnchorIndex % GridWidth;
    int32 AnchorY = AnchorIndex / GridWidth;
    
    // Резервируем память
    int32 CellCount = static_cast<int32>(TargetEffectiveSize.X * TargetEffectiveSize.Y);
    TargetOccupiedCells.Reserve(CellCount);
    
    // Вычисляем все ячейки которые будут заняты после поворота
    for (int32 Y = 0; Y < TargetEffectiveSize.Y; ++Y)
    {
        for (int32 X = 0; X < TargetEffectiveSize.X; ++X)
        {
            int32 CellIndex = (AnchorY + Y) * GridWidth + (AnchorX + X);
            TargetOccupiedCells.Add(CellIndex);
        }
    }
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("FSuspenseRotationOperation::CalculateTargetCells: %d cells for item at anchor %d"), 
        TargetOccupiedCells.Num(), AnchorIndex);
}

// Получение описания операции
FString FSuspenseRotationOperation::GetOperationDescription() const
{
    if (!HasRotationChanged())
    {
        return TEXT("No Rotation Change");
    }
    
    if (HasSizeChanged())
    {
        return FString::Printf(TEXT("Rotate %s (%.0fx%.0f -> %.0fx%.0f)"),
            bTargetRotation ? TEXT("90°") : TEXT("0°"),
            InitialEffectiveSize.X, InitialEffectiveSize.Y,
            TargetEffectiveSize.X, TargetEffectiveSize.Y);
    }
    else
    {
        return FString::Printf(TEXT("Rotate %s (square item)"),
            bTargetRotation ? TEXT("90°") : TEXT("0°"));
    }
}

// Выполнение операции поворота
bool FSuspenseRotationOperation::ExecuteRotation(EInventoryErrorCode& OutErrorCode)
{
    double StartTime = FPlatformTime::Seconds();
    
    // Финальная валидация
    FString ErrorMessage;
    if (!ValidateRotation(OutErrorCode, ErrorMessage))
    {
        LogOperationDetails(FString::Printf(TEXT("Execution failed validation: %s"), *ErrorMessage), true);
        return false;
    }
    
    // Если нет изменений, считаем операцию успешной
    if (!HasRotationChanged())
    {
        bSuccess = true;
        OutErrorCode = EInventoryErrorCode::Success;
        LogOperationDetails(TEXT("No rotation needed - already in target state"));
        return true;
    }
    
    // Применяем поворот
    ApplyRotation(bTargetRotation);
    
    // Обновляем размещение в сетке если есть компонент
    if (InventoryComponent)
    {
        UpdateGridPlacement();
    }
    
    // Отмечаем успех
    bSuccess = true;
    OutErrorCode = EInventoryErrorCode::Success;
    
    // Записываем время выполнения
    ExecutionTime = static_cast<float>(FPlatformTime::Seconds() - StartTime);
    
    LogOperationDetails(FString::Printf(TEXT("Rotation executed successfully in %.3f ms"), 
        ExecutionTime * 1000.0f));
    
    return true;
}

// Применение поворота к предмету
void FSuspenseRotationOperation::ApplyRotation(bool bRotated)
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
    
    // Применяем новое состояние поворота
    ItemInterface->SetRotated(bRotated);
    
    // Обновляем runtime свойства
    ItemInstance.bIsRotated = bRotated;
    ItemInstance.LastUsedTime = FPlatformTime::Seconds();
    
    // Применяем обновленный экземпляр
    ItemInterface->SetItemInstance(ItemInstance);
    
    LogOperationDetails(FString::Printf(TEXT("Applied rotation state: %s"), 
        bRotated ? TEXT("Rotated") : TEXT("Normal")));
}

void FSuspenseRotationOperation::UpdateGridPlacement()
{
    if (!InventoryComponent || !Item)
    {
        return;
    }
    
    // ИСПРАВЛЕНО: Используем RotateItemAtSlot вместо RotateItem
    // В новой архитектуре работаем со слотами, а не с объектами
    
    // Проверяем возможность поворота через слот
    if (InventoryComponent->CanRotateItemAtSlot(AnchorIndex))
    {
        // Выполняем поворот через API инвентаря
        bool bRotateSuccess = InventoryComponent->RotateItemAtSlot(AnchorIndex);
        
        if (bRotateSuccess)
        {
            LogOperationDetails(TEXT("Grid placement updated via InventoryComponent::RotateItemAtSlot"));
        }
        else
        {
            UE_LOG(LogInventory, Warning, 
                TEXT("FSuspenseRotationOperation::UpdateGridPlacement: RotateItemAtSlot failed for slot %d"),
                AnchorIndex);
        }
    }
    else
    {
        // Если прямой поворот невозможен, логируем причину
        UE_LOG(LogInventory, Warning, 
            TEXT("FSuspenseRotationOperation::UpdateGridPlacement: CanRotateItemAtSlot returned false for slot %d"),
            AnchorIndex);
        
        // Альтернатива: пробуем обновить через refresh
        InventoryComponent->RefreshItemsUI();
    }
    
    // Всегда отправляем уведомление об изменении
    InventoryComponent->BroadcastInventoryUpdated();
}

// Логирование операции
void FSuspenseRotationOperation::LogOperationDetails(const FString& Message, bool bIsError) const
{
    const FString ItemName = Item ? Item->GetItemID().ToString() : TEXT("None");
    const FString Details = FString::Printf(
        TEXT("[RotateOp] %s - Item: %s, Anchor: %d, Initial: %s, Target: %s - %s"),
        *GetOperationDescription(),
        *ItemName,
        AnchorIndex,
        bInitialRotation ? TEXT("Rotated") : TEXT("Normal"),
        bTargetRotation ? TEXT("Rotated") : TEXT("Normal"),
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
bool FSuspenseRotationOperation::CanUndo() const
{
    return bSuccess && Item != nullptr && HasRotationChanged();
}

bool FSuspenseRotationOperation::Undo()
{
    if (!CanUndo())
    {
        return false;
    }
    
    // Применяем исходное состояние поворота
    ApplyRotation(bInitialRotation);
    
    // Обновляем размещение в сетке
    if (InventoryComponent)
    {
        UpdateGridPlacement();
    }
    
    LogOperationDetails(TEXT("Operation undone"));
    
    return true;
}

// Реализация Redo  
bool FSuspenseRotationOperation::CanRedo() const
{
    return Item != nullptr && HasRotationChanged();
}

bool FSuspenseRotationOperation::Redo()
{
    if (!CanRedo())
    {
        return false;
    }
    
    // Повторяем операцию поворота
    // ИСПРАВЛЕНО: используем локальную переменную для избежания конфликта имен
    EInventoryErrorCode RedoErrorCode;
    bool bResult = ExecuteRotation(RedoErrorCode);
    
    if (!bResult)
    {
        LogOperationDetails(FString::Printf(TEXT("Redo failed with error: %d"), 
            static_cast<int32>(RedoErrorCode)), true);
    }
    
    return bResult;
}

// Преобразование в строку
FString FSuspenseRotationOperation::ToString() const
{
    const FString ItemName = Item ? Item->GetItemID().ToString() : TEXT("None");
    return FString::Printf(
        TEXT("RotateOp[%s, Item=%s, Anchor=%d, Base=%dx%d, Initial=%.0fx%.0f(%s), Target=%.0fx%.0f(%s), Cells=%d, Success=%s]"),
        *GetOperationDescription(),
        *ItemName,
        AnchorIndex,
        BaseGridSize.X, BaseGridSize.Y,
        InitialEffectiveSize.X, InitialEffectiveSize.Y,
        bInitialRotation ? TEXT("R") : TEXT("N"),
        TargetEffectiveSize.X, TargetEffectiveSize.Y,
        bTargetRotation ? TEXT("R") : TEXT("N"),
        TargetOccupiedCells.Num(),
        bSuccess ? TEXT("Yes") : TEXT("No")
    );
}