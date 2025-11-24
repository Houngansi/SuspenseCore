// Copyright MedCom Team. All Rights Reserved.

#include "UI/MedComInventoryUIConnector.h"
#include "Components/MedComInventoryComponent.h"
#include "Interfaces/UI/IMedComInventoryUIBridgeWidget.h"
#include "Interfaces/Inventory/IMedComInventoryItemInterface.h"
#include "Interfaces/Inventory/IMedComInventoryInterface.h"
#include "ItemSystem/MedComItemManager.h"
#include "Delegates/EventDelegateManager.h"
#include "Types/Loadout/MedComItemDataTable.h"
#include "Base/InventoryLogs.h"
#include "Engine/GameInstance.h"
#include "Internationalization/Text.h"

UMedComInventoryUIConnector::UMedComInventoryUIConnector()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UMedComInventoryUIConnector::BeginPlay()
{
    Super::BeginPlay();
    
    // Cache managers
    if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
    {
        CachedItemManager = GameInstance->GetSubsystem<UMedComItemManager>();
        CachedDelegateManager = GameInstance->GetSubsystem<UEventDelegateManager>();
    }
}

void UMedComInventoryUIConnector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Unsubscribe from all events
    UnsubscribeFromEvents();
    
    // Clear caches
    IconCache.Empty();
    CurrentDragOperation.Reset();
    
    Super::EndPlay(EndPlayReason);
}

//==================================================================
// Core Setup
//==================================================================

void UMedComInventoryUIConnector::SetInventoryComponent(UMedComInventoryComponent* InInventoryComponent)
{
    // Unsubscribe from previous component
    if (InventoryComponent != InInventoryComponent)
    {
        UnsubscribeFromEvents();
    }
    
    InventoryComponent = InInventoryComponent;
    
    // Subscribe to new component
    if (InventoryComponent)
    {
        SubscribeToEvents();
        
        // Initial UI update
        RefreshUI();
    }
}

void UMedComInventoryUIConnector::SetUIBridge(TScriptInterface<IMedComInventoryUIBridgeWidget> InBridge)
{
    UIBridge = InBridge;
}

//==================================================================
// UI Display Data
//==================================================================

TArray<FInventoryCellUI> UMedComInventoryUIConnector::GetAllCellsForUI()
{
    TArray<FInventoryCellUI> Result;
    
    if (!InventoryComponent || !InventoryComponent->IsInventoryInitialized())
    {
        return Result;
    }
    
    // Получаем размеры сетки
    FVector2D GridSize = InventoryComponent->GetInventorySize();
    int32 TotalCells = static_cast<int32>(GridSize.X * GridSize.Y);
    
    // Резервируем память для производительности
    Result.Reserve(TotalCells);
    
    // Получаем все экземпляры предметов из инвентаря
    TArray<FInventoryItemInstance> AllInstances = InventoryComponent->GetAllItemInstances();
    
    // Получаем ItemManager для доступа к DataTable
    UMedComItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        return Result;
    }
    
    // Создаём карту якорных индексов к экземплярам для быстрого поиска
    TMap<int32, const FInventoryItemInstance*> AnchorToInstance;
    
    // Также нужна карта всех занятых ячеек
    TSet<int32> OccupiedCells;
    
    // Первый проход: мапим предметы и их занятые ячейки
    for (const FInventoryItemInstance& Instance : AllInstances)
    {
        if (Instance.AnchorIndex == INDEX_NONE)
        {
            continue;
        }
        
        AnchorToInstance.Add(Instance.AnchorIndex, &Instance);
        
        // Получаем данные предмета из DataTable
        FMedComUnifiedItemData ItemData;
        if (ItemManager->GetUnifiedItemData(Instance.ItemID, ItemData))
        {
            // Вычисляем эффективный размер с учётом поворота
            FVector2D BaseSize(ItemData.GridSize.X, ItemData.GridSize.Y);
            FVector2D ItemSize = Instance.bIsRotated ? 
                FVector2D(BaseSize.Y, BaseSize.X) : BaseSize;
                
            TArray<int32> ItemOccupiedCells = InventoryComponent->GetOccupiedSlots(
                Instance.AnchorIndex, ItemSize, Instance.bIsRotated);
                
            for (int32 CellIndex : ItemOccupiedCells)
            {
                OccupiedCells.Add(CellIndex);
            }
        }
    }
    
    // Второй проход: создаём данные ячеек для всей сетки
    for (int32 CellIndex = 0; CellIndex < TotalCells; CellIndex++)
    {
        FInventoryCellUI CellUI;
        CellUI.Index = CellIndex;
        
        // Вычисляем позицию в сетке
        int32 X, Y;
        if (InventoryComponent->GetInventoryCoordinates(CellIndex, X, Y))
        {
            CellUI.Position = FVector2D(X, Y);
        }
        
        // Проверяем, является ли это якорной позицией с предметом
        if (const FInventoryItemInstance** InstancePtr = AnchorToInstance.Find(CellIndex))
        {
            // Конвертируем в UI ячейку с полными данными
            // Передаём nullptr вместо ItemObject, так как мы больше не работаем с объектами
            CellUI = ConvertItemToUICell(**InstancePtr, nullptr, CellIndex);
        }
        
        Result.Add(CellUI);
    }
    
    return Result;
}

FInventoryCellUI UMedComInventoryUIConnector::GetCellData(int32 CellIndex) const
{
    FInventoryCellUI CellUI;
    CellUI.Index = CellIndex;
    
    if (!InventoryComponent || CellIndex < 0)
    {
        return CellUI;
    }
    
    // Получаем позицию в сетке
    int32 X, Y;
    if (InventoryComponent->GetInventoryCoordinates(CellIndex, X, Y))
    {
        CellUI.Position = FVector2D(X, Y);
    }
    
    // Проверяем, есть ли предмет в этой позиции
    FInventoryItemInstance Instance;
    if (!InventoryComponent->GetItemInstanceAtSlot(CellIndex, Instance))
    {
        return CellUI; // Пустая ячейка
    }
    
    // Возвращаем полные данные только если это якорная ячейка
    if (Instance.AnchorIndex == CellIndex)
    {
        CellUI = ConvertItemToUICell(Instance, nullptr, CellIndex);
    }
    
    return CellUI;
}

FVector2D UMedComInventoryUIConnector::GetInventoryGridSize() const
{
    if (InventoryComponent)
    {
        return InventoryComponent->GetInventorySize();
    }
    
    return FVector2D::ZeroVector;
}

void UMedComInventoryUIConnector::GetWeightInfo(float& OutCurrentWeight, float& OutMaxWeight, float& OutPercentUsed) const
{
    if (InventoryComponent)
    {
        OutCurrentWeight = InventoryComponent->GetCurrentWeight_Implementation();
        OutMaxWeight = InventoryComponent->GetMaxWeight_Implementation();
        OutPercentUsed = (OutMaxWeight > 0.0f) ? (OutCurrentWeight / OutMaxWeight) : 0.0f;
    }
    else
    {
        OutCurrentWeight = 0.0f;
        OutMaxWeight = 0.0f;
        OutPercentUsed = 0.0f;
    }
}

//==================================================================
// UI Actions
//==================================================================

void UMedComInventoryUIConnector::ShowInventory()
{
    if (UIBridge.GetInterface())
    {
        IMedComInventoryUIBridgeWidget::Execute_ShowInventoryUI(UIBridge.GetObject());
    }
}

void UMedComInventoryUIConnector::HideInventory()
{
    if (UIBridge.GetInterface())
    {
        IMedComInventoryUIBridgeWidget::Execute_HideInventoryUI(UIBridge.GetObject());
    }
}

void UMedComInventoryUIConnector::ToggleInventory()
{
    if (UIBridge.GetInterface())
    {
        bool bIsVisible = IMedComInventoryUIBridgeWidget::Execute_IsInventoryUIVisible(UIBridge.GetObject());
        if (bIsVisible)
        {
            HideInventory();
        }
        else
        {
            ShowInventory();
        }
    }
}

void UMedComInventoryUIConnector::RefreshUI()
{
    OnInventoryUpdated();
}

//==================================================================
// Drag & Drop Operations
//==================================================================

bool UMedComInventoryUIConnector::StartDragOperation(UObject* ItemObject, int32 FromCellIndex)
{
    if (!InventoryComponent || !ItemObject)
    {
        return false;
    }
    
    // Validate item
    const IMedComInventoryItemInterface* ItemInterface = Cast<IMedComInventoryItemInterface>(ItemObject);
    if (!ItemInterface || !ItemInterface->IsInitialized())
    {
        return false;
    }
    
    // Store drag operation data
    CurrentDragOperation.DraggedItem = ItemObject;
    CurrentDragOperation.OriginalCellIndex = FromCellIndex;
    CurrentDragOperation.bIsActive = true;
    
    UE_LOG(LogInventory, Log, TEXT("UIConnector: Started drag operation for %s from cell %d"), 
        *ItemInterface->GetItemID().ToString(), FromCellIndex);
    
    return true;
}

bool UMedComInventoryUIConnector::PreviewDrop(UObject* ItemObject, int32 TargetCellIndex, bool bWantRotate)
{
    if (!InventoryComponent || !ItemObject)
    {
        return false;
    }
    
    const IMedComInventoryItemInterface* ItemInterface = Cast<IMedComInventoryItemInterface>(ItemObject);
    if (!ItemInterface)
    {
        return false;
    }
    
    // Calculate effective size with rotation
    FVector2D BaseSize = ItemInterface->GetBaseGridSize();
    FVector2D EffectiveSize = bWantRotate ? 
        FVector2D(BaseSize.Y, BaseSize.X) : BaseSize;
    
    // Check if item can be placed at target
    return InventoryComponent->CanPlaceItemAtSlot(EffectiveSize, TargetCellIndex, false);
}

bool UMedComInventoryUIConnector::CompleteDrop(UObject* ItemObject, int32 TargetCellIndex, bool bWantRotate)
{
    if (!InventoryComponent || !CurrentDragOperation.bIsActive)
    {
        return false;
    }
    
    // В новой архитектуре мы работаем только со слотами, не с объектами
    // ItemObject здесь используется только для валидации drag операции
    if (CurrentDragOperation.DraggedItem.Get() != ItemObject)
    {
        UE_LOG(LogInventory, Warning, TEXT("UIConnector: Drop item mismatch"));
        CurrentDragOperation.Reset();
        return false;
    }
    
    // Получаем экземпляр из исходного слота
    FInventoryItemInstance SourceInstance;
    if (!InventoryComponent->GetItemInstanceAtSlot(CurrentDragOperation.OriginalCellIndex, SourceInstance))
    {
        UE_LOG(LogInventory, Warning, TEXT("UIConnector: No item at source slot %d"), 
            CurrentDragOperation.OriginalCellIndex);
        CurrentDragOperation.Reset();
        return false;
    }
    
    // Определяем, это просто поворот или перемещение
    bool bCurrentRotation = SourceInstance.bIsRotated;
    bool bIsRotationOnly = (CurrentDragOperation.OriginalCellIndex == TargetCellIndex) && 
                          (bCurrentRotation != bWantRotate);
    
    // Выполняем соответствующую операцию
    bool bSuccess = false;
    
    if (bIsRotationOnly)
    {
        // Просто поворачиваем предмет
        bSuccess = InventoryComponent->RotateItemAtSlot(CurrentDragOperation.OriginalCellIndex);
    }
    else
    {
        // Перемещаем предмет (с опциональным поворотом)
        // bMaintainRotation инвертирован: если bWantRotate == true, мы НЕ хотим сохранять текущую ротацию
        bSuccess = InventoryComponent->MoveItemBySlots_Implementation(
            CurrentDragOperation.OriginalCellIndex,
            TargetCellIndex,
            !bWantRotate
        );
    }
    
    // Очищаем операцию перетаскивания
    CurrentDragOperation.Reset();
    
    if (bSuccess)
    {
        UE_LOG(LogInventory, Log, TEXT("UIConnector: Drop completed successfully"));
    }
    
    return bSuccess;
}

void UMedComInventoryUIConnector::CancelDrag()
{
    CurrentDragOperation.Reset();
    UE_LOG(LogInventory, Log, TEXT("UIConnector: Drag operation cancelled"));
}

//==================================================================
// Stack Operations
//==================================================================

bool UMedComInventoryUIConnector::TryStackItems(UObject* SourceItem, UObject* TargetItem, int32 Amount)
{
    if (!InventoryComponent || !SourceItem || !TargetItem)
    {
        return false;
    }
    
    const IMedComInventoryItemInterface* SourceInterface = Cast<IMedComInventoryItemInterface>(SourceItem);
    const IMedComInventoryItemInterface* TargetInterface = Cast<IMedComInventoryItemInterface>(TargetItem);
    
    if (!SourceInterface || !TargetInterface)
    {
        return false;
    }
    
    // Validate stackability
    if (!CanItemsStack(SourceItem, TargetItem))
    {
        return false;
    }
    
    // Calculate amount to transfer
    int32 SourceAmount = SourceInterface->GetAmount();
    int32 TargetAmount = TargetInterface->GetAmount();
    int32 MaxStack = TargetInterface->GetMaxStackSize();
    
    if (Amount <= 0)
    {
        Amount = SourceAmount; // Transfer all
    }
    
    int32 AvailableSpace = MaxStack - TargetAmount;
    int32 ToTransfer = FMath::Min3(Amount, SourceAmount, AvailableSpace);
    
    if (ToTransfer <= 0)
    {
        return false;
    }
    
    // Use inventory component's swap operation for stacking
    // This ensures proper transaction handling
    int32 SourceSlot = SourceInterface->GetAnchorIndex();
    int32 TargetSlot = TargetInterface->GetAnchorIndex();
    
    EInventoryErrorCode ErrorCode;
    bool bSuccess = InventoryComponent->SwapItemsInSlots(SourceSlot, TargetSlot, ErrorCode);
    
    if (bSuccess)
    {
        UE_LOG(LogInventory, Log, TEXT("UIConnector: Stacked %d items"), ToTransfer);
    }
    else
    {
        UE_LOG(LogInventory, Warning, TEXT("UIConnector: Stack failed with error %d"), 
            static_cast<int32>(ErrorCode));
    }
    
    return bSuccess;
}

bool UMedComInventoryUIConnector::SplitItemStack(UObject* SourceItem, int32 SplitAmount, int32 TargetCellIndex)
{
    if (!InventoryComponent || !SourceItem)
    {
        return false;
    }
    
    const IMedComInventoryItemInterface* SourceInterface = Cast<IMedComInventoryItemInterface>(SourceItem);
    if (!SourceInterface || !SourceInterface->IsStackable())
    {
        return false;
    }
    
    // Use inventory component's split stack operation
    int32 SourceSlot = SourceInterface->GetAnchorIndex();
    FInventoryOperationResult Result = InventoryComponent->SplitStack(SourceSlot, SplitAmount, TargetCellIndex);
    
    if (Result.IsSuccess())
    {
        UE_LOG(LogInventory, Log, TEXT("UIConnector: Split %d items to cell %d"), 
            SplitAmount, TargetCellIndex);
    }
    else
    {
        UE_LOG(LogInventory, Warning, TEXT("UIConnector: Split failed - %s"), 
            *Result.ErrorMessage.ToString());
    }
    
    return Result.IsSuccess();
}

bool UMedComInventoryUIConnector::CanItemsStack(UObject* Item1, UObject* Item2) const
{
    if (!Item1 || !Item2 || Item1 == Item2)
    {
        return false;
    }
    
    const IMedComInventoryItemInterface* Interface1 = Cast<IMedComInventoryItemInterface>(Item1);
    const IMedComInventoryItemInterface* Interface2 = Cast<IMedComInventoryItemInterface>(Item2);
    
    if (!Interface1 || !Interface2)
    {
        return false;
    }
    
    // Must be same item type
    if (Interface1->GetItemID() != Interface2->GetItemID())
    {
        return false;
    }
    
    // Must be stackable
    if (!Interface1->IsStackable() || !Interface2->IsStackable())
    {
        return false;
    }
    
    // Target must have space
    return Interface2->GetAmount() < Interface2->GetMaxStackSize();
}

//==================================================================
// Item Information from DataTable
//==================================================================

UTexture2D* UMedComInventoryUIConnector::GetItemIcon(const FName& ItemID) const
{
    if (ItemID.IsNone())
    {
        return nullptr;
    }
    
    // Check cache first
    if (const TWeakObjectPtr<UTexture2D>* CachedIcon = IconCache.Find(ItemID))
    {
        if (CachedIcon->IsValid())
        {
            return CachedIcon->Get();
        }
    }
    
    // Get from item manager
    UMedComItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        return nullptr;
    }
    
    FMedComUnifiedItemData ItemData;
    if (ItemManager->GetUnifiedItemData(ItemID, ItemData))
    {
        if (!ItemData.Icon.IsNull())
        {
            UTexture2D* LoadedIcon = ItemData.Icon.LoadSynchronous();
            if (LoadedIcon)
            {
                IconCache.Add(ItemID, LoadedIcon);
                return LoadedIcon;
            }
        }
    }
    
    return nullptr;
}

bool UMedComInventoryUIConnector::GetItemDisplayInfo(const FName& ItemID, FText& OutDisplayName, 
    FText& OutDescription, FLinearColor& OutRarityColor) const
{
    if (ItemID.IsNone())
    {
        return false;
    }
    
    UMedComItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        return false;
    }
    
    FMedComUnifiedItemData ItemData;
    if (ItemManager->GetUnifiedItemData(ItemID, ItemData))
    {
        OutDisplayName = ItemData.DisplayName;
        OutDescription = ItemData.Description;
        OutRarityColor = ItemData.GetRarityColor();
        return true;
    }
    
    return false;
}

bool UMedComInventoryUIConnector::GetItemTooltip(UObject* ItemObject, FText& OutTooltipText, 
    FLinearColor& OutRarityColor) const
{
    if (!ItemObject)
    {
        return false;
    }
    
    const IMedComInventoryItemInterface* ItemInterface = Cast<IMedComInventoryItemInterface>(ItemObject);
    if (!ItemInterface)
    {
        return false;
    }
    
    // Get item data from DataTable
    FMedComUnifiedItemData ItemData;
    if (!ItemInterface->GetItemData(ItemData))
    {
        return false;
    }
    
    // Get runtime instance
    FInventoryItemInstance Instance = ItemInterface->GetItemInstance();
    
    // Build tooltip
    OutTooltipText = BuildItemTooltip(Instance, ItemData);
    OutRarityColor = ItemData.GetRarityColor();
    
    return true;
}

//==================================================================
// Utility Functions
//==================================================================

FText UMedComInventoryUIConnector::FormatWeight(float Weight) const
{
    FNumberFormattingOptions Options;
    Options.MinimumFractionalDigits = 1;
    Options.MaximumFractionalDigits = 1;
    
    return FText::Format(
        NSLOCTEXT("Inventory", "WeightFormat", "{0} kg"),
        FText::AsNumber(Weight, &Options)
    );
}

FText UMedComInventoryUIConnector::FormatStackQuantity(int32 Current, int32 Max) const
{
    if (Max <= 1)
    {
        return FText::GetEmpty();
    }
    
    return FText::Format(
        NSLOCTEXT("Inventory", "StackFormat", "{0}/{1}"),
        FText::AsNumber(Current),
        FText::AsNumber(Max)
    );
}

TArray<int32> UMedComInventoryUIConnector::GetItemOccupiedCells(UObject* ItemObject) const
{
    TArray<int32> Result;
    
    if (!InventoryComponent || !ItemObject)
    {
        return Result;
    }
    
    const IMedComInventoryItemInterface* ItemInterface = Cast<IMedComInventoryItemInterface>(ItemObject);
    if (!ItemInterface)
    {
        return Result;
    }
    
    return InventoryComponent->GetOccupiedSlots(
        ItemInterface->GetAnchorIndex(),
        ItemInterface->GetEffectiveGridSize(),
        ItemInterface->IsRotated()
    );
}

//==================================================================
// Internal Helpers
//==================================================================

UMedComItemManager* UMedComInventoryUIConnector::GetItemManager() const
{
    if (CachedItemManager.IsValid())
    {
        return CachedItemManager.Get();
    }
    
    // Try to get from inventory component
    if (InventoryComponent)
    {
        UMedComItemManager* Manager = InventoryComponent->GetItemManager();
        if (Manager)
        {
            CachedItemManager = Manager;
            return Manager;
        }
    }
    
    // Get from game instance
    if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
    {
        UMedComItemManager* Manager = GameInstance->GetSubsystem<UMedComItemManager>();
        CachedItemManager = Manager;
        return Manager;
    }
    
    return nullptr;
}

UEventDelegateManager* UMedComInventoryUIConnector::GetDelegateManager() const
{
    if (CachedDelegateManager.IsValid())
    {
        return CachedDelegateManager.Get();
    }
    
    // Try to get from inventory component
    if (InventoryComponent)
    {
        UEventDelegateManager* Manager = InventoryComponent->GetDelegateManager();
        if (Manager)
        {
            CachedDelegateManager = Manager;
            return Manager;
        }
    }
    
    // Get from game instance
    if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
    {
        UEventDelegateManager* Manager = GameInstance->GetSubsystem<UEventDelegateManager>();
        CachedDelegateManager = Manager;
        return Manager;
    }
    
    return nullptr;
}

FInventoryCellUI UMedComInventoryUIConnector::ConvertItemToUICell(
    const FInventoryItemInstance& Instance, 
    UObject* ItemObject, 
    int32 CellIndex) const
{
    FInventoryCellUI CellUI;
    
    // Базовые свойства из экземпляра
    CellUI.Index = CellIndex;
    CellUI.ItemID = Instance.ItemID;
    CellUI.Quantity = Instance.Quantity;
    CellUI.AnchorIndex = Instance.AnchorIndex;
    CellUI.bIsRotated = Instance.bIsRotated;
    
    // КРИТИЧЕСКИ ВАЖНО: Правильная обработка InstanceID
    // Всегда используем InstanceID из экземпляра, а не из временного объекта
    CellUI.InstanceID = Instance.InstanceID;
    
    // Проверяем валидность InstanceID и логируем предупреждение если он невалиден
    if (!CellUI.InstanceID.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[UIConnector] ConvertItemToUICell: Invalid InstanceID for item %s at slot %d"), 
            *Instance.ItemID.ToString(), CellIndex);
        // НЕ генерируем новый ID здесь - это ответственность Storage компонента
        // Невалидный InstanceID приведет к проблемам с drag&drop
    }
    else
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("[UIConnector] ConvertItemToUICell: Item %s has valid InstanceID %s"), 
            *Instance.ItemID.ToString(), *CellUI.InstanceID.ToString());
    }
    
    // ItemObject может быть nullptr в новой системе, где мы работаем напрямую с экземплярами
    CellUI.ItemObject = ItemObject;
    
    // Вычисляем позицию в сетке
    if (InventoryComponent && CellIndex != INDEX_NONE)
    {
        int32 X, Y;
        if (InventoryComponent->GetInventoryCoordinates(CellIndex, X, Y))
        {
            CellUI.Position = FVector2D(X, Y);
        }
    }
    
    // Получаем унифицированные данные из DataTable через ItemManager
    UMedComItemManager* ItemManager = GetItemManager();
    if (ItemManager)
    {
        FMedComUnifiedItemData ItemData;
        if (ItemManager->GetUnifiedItemData(Instance.ItemID, ItemData))
        {
            // Свойства отображения
            CellUI.ItemName = ItemData.DisplayName;
            CellUI.Weight = ItemData.Weight * Instance.Quantity; // Учитываем количество
            CellUI.RarityColor = ItemData.GetRarityColor();
            CellUI.bIsStackable = ItemData.MaxStackSize > 1;
            CellUI.MaxStackSize = ItemData.MaxStackSize;
            
            // Размер в сетке с учетом поворота
            FVector2D BaseSize(ItemData.GridSize.X, ItemData.GridSize.Y);
            CellUI.GridSize = Instance.bIsRotated ? 
                FVector2D(BaseSize.Y, BaseSize.X) : BaseSize;
            
            // Загружаем иконку
            CellUI.ItemIcon = GetItemIcon(Instance.ItemID);
            
            // Информация о прочности для экипировки
            if (ItemData.bIsEquippable)
            {
                CellUI.DurabilityPercent = Instance.GetDurabilityPercent();
                
                // Добавляем визуальную индикацию низкой прочности
                if (CellUI.DurabilityPercent < 0.3f) // Менее 30%
                {
                    UE_LOG(LogTemp, Verbose, TEXT("[UIConnector] Item %s has low durability: %.1f%%"), 
                        *Instance.ItemID.ToString(), CellUI.DurabilityPercent * 100.0f);
                }
            }
            
            // Информация о патронах для оружия
            // Поскольку в FInventoryCellUI нет поля AmmoText, мы просто логируем эту информацию
            // UI виджет должен будет самостоятельно получить эти данные при необходимости
            if (ItemData.bIsWeapon && Instance.HasRuntimeProperty(TEXT("Ammo")))
            {
                int32 CurrentAmmo = Instance.GetCurrentAmmo();
                int32 MaxAmmo = FMath::RoundToInt(Instance.GetRuntimeProperty(TEXT("MaxAmmo"), 30.0f));
                
                // Логируем информацию о патронах для отладки
                UE_LOG(LogTemp, VeryVerbose, TEXT("[UIConnector] Weapon %s has ammo: %d/%d"), 
                    *Instance.ItemID.ToString(), CurrentAmmo, MaxAmmo);
                
                // Добавляем визуальную индикацию низкого боезапаса
                float AmmoPercent = (MaxAmmo > 0) ? (float(CurrentAmmo) / float(MaxAmmo)) : 0.0f;
                if (AmmoPercent < 0.3f) // Менее 30%
                {
                    UE_LOG(LogTemp, Verbose, TEXT("[UIConnector] Weapon %s has low ammo: %d/%d"), 
                        *Instance.ItemID.ToString(), CurrentAmmo, MaxAmmo);
                }
            }
            
            // Финальное логирование для отладки
            UE_LOG(LogTemp, VeryVerbose, TEXT("[UIConnector] ConvertItemToUICell completed for %s:"), 
                *Instance.ItemID.ToString());
            UE_LOG(LogTemp, VeryVerbose, TEXT("  - Name: %s"), *CellUI.ItemName.ToString());
            UE_LOG(LogTemp, VeryVerbose, TEXT("  - Quantity: %d"), CellUI.Quantity);
            UE_LOG(LogTemp, VeryVerbose, TEXT("  - Weight: %.2f"), CellUI.Weight);
            UE_LOG(LogTemp, VeryVerbose, TEXT("  - Grid Size: %.0fx%.0f"), CellUI.GridSize.X, CellUI.GridSize.Y);
            UE_LOG(LogTemp, VeryVerbose, TEXT("  - Position: (%d, %d)"), 
                FMath::FloorToInt(CellUI.Position.X), FMath::FloorToInt(CellUI.Position.Y));
            UE_LOG(LogTemp, VeryVerbose, TEXT("  - Rotated: %s"), CellUI.bIsRotated ? TEXT("Yes") : TEXT("No"));
        }
        else
        {
            // Не удалось получить данные из ItemManager
            UE_LOG(LogTemp, Warning, TEXT("[UIConnector] Failed to get unified data for item %s"), 
                *Instance.ItemID.ToString());
            
            // Используем минимальные данные для отображения
            CellUI.ItemName = FText::FromName(Instance.ItemID);
            CellUI.Weight = 1.0f * Instance.Quantity;
            CellUI.MaxStackSize = 1;
            CellUI.GridSize = FVector2D(1.0f, 1.0f); // Исправлено: используем FVector2D вместо FIntPoint
            CellUI.RarityColor = FLinearColor::Gray;
        }
    }
    else
    {
        // ItemManager недоступен
        UE_LOG(LogTemp, Error, TEXT("[UIConnector] ItemManager not available for item conversion"));
        
        // Fallback данные
        CellUI.ItemName = FText::FromName(Instance.ItemID);
        CellUI.Weight = 1.0f * Instance.Quantity;
        CellUI.MaxStackSize = 1;
        CellUI.GridSize = FVector2D(1.0f, 1.0f); // Исправлено: используем FVector2D вместо FIntPoint
        CellUI.RarityColor = FLinearColor::Gray;
    }
    
    return CellUI;
}

FText UMedComInventoryUIConnector::BuildItemTooltip(
    const FInventoryItemInstance& Instance, 
    const FMedComUnifiedItemData& ItemData) const
{
    TArray<FString> TooltipLines;
    
    // Name with quantity
    if (Instance.Quantity > 1)
    {
        TooltipLines.Add(FString::Printf(TEXT("%s (x%d)"), 
            *ItemData.DisplayName.ToString(), Instance.Quantity));
    }
    else
    {
        TooltipLines.Add(ItemData.DisplayName.ToString());
    }
    
    // Type and rarity
    if (ItemData.Rarity.IsValid())
    {
        FString RarityName = ItemData.Rarity.GetTagName().ToString();
        RarityName.RemoveFromStart("Item.Rarity.");
        TooltipLines.Add(RarityName);
    }
    
    // Description
    if (!ItemData.Description.IsEmpty())
    {
        TooltipLines.Add(TEXT("")); // Empty line
        TooltipLines.Add(ItemData.Description.ToString());
    }
    
    // Weight
    float TotalWeight = ItemData.Weight * Instance.Quantity;
    TooltipLines.Add(TEXT("")); // Empty line
    TooltipLines.Add(FString::Printf(TEXT("Weight: %s"), 
        *FormatWeight(TotalWeight).ToString()));
    
    // Durability for equipment
    if (ItemData.bIsEquippable && Instance.HasRuntimeProperty(TEXT("Durability")))
    {
        float DurabilityPercent = Instance.GetDurabilityPercent();
        TooltipLines.Add(FString::Printf(TEXT("Durability: %.0f%%"), 
            DurabilityPercent * 100.0f));
    }
    
    // Ammo for weapons
    if (ItemData.bIsWeapon && Instance.HasRuntimeProperty(TEXT("Ammo")))
    {
        int32 CurrentAmmo = Instance.GetCurrentAmmo();
        // GetMaxAmmo() doesn't exist in FInventoryItemInstance
        // We need to get it from RuntimeProperties directly
        int32 MaxAmmo = FMath::RoundToInt(Instance.GetRuntimeProperty(TEXT("MaxAmmo"), 30.0f));
        TooltipLines.Add(FString::Printf(TEXT("Ammo: %d/%d"), 
            CurrentAmmo, MaxAmmo));
    }
    
    // Value
    if (ItemData.BaseValue > 0)
    {
        TooltipLines.Add(FString::Printf(TEXT("Value: %d"), 
            ItemData.BaseValue * Instance.Quantity));
    }
    
    // Join all lines
    return FText::FromString(FString::Join(TooltipLines, TEXT("\n")));
}

void UMedComInventoryUIConnector::SubscribeToEvents()
{
    if (!InventoryComponent)
    {
        return;
    }
    
    // Get EventDelegateManager for centralized event subscription
    UEventDelegateManager* DelegateManager = GetDelegateManager();
    if (!DelegateManager)
    {
        UE_LOG(LogInventory, Warning, TEXT("UIConnector: EventDelegateManager not available for subscription"));
        return;
    }
    
    // Subscribe to inventory refresh events through EventDelegateManager
    // This is the proper way to handle inventory updates in your architecture
    InventoryUpdateHandle = DelegateManager->SubscribeToUIEvent(
        [this](UObject* Source, const FGameplayTag& EventTag, const FString& EventData)
        {
            // Filter for inventory update events
            if (EventTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Inventory.Event.Updated"))))
            {
                OnInventoryUpdated();
            }
        }
    );
    
    // Subscribe to specific inventory UI refresh events
    if (InventoryUpdateHandle.IsValid())
    {
        UE_LOG(LogInventory, Log, TEXT("UIConnector: Successfully subscribed to inventory events"));
    }
    
    // Additional subscriptions for specific inventory events if needed
    // For example, item added/removed events
    ItemAddedHandle = DelegateManager->SubscribeToUIEvent(
        [this](UObject* Source, const FGameplayTag& EventTag, const FString& EventData)
        {
            if (EventTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Inventory.Event.ItemAdded"))))
            {
                // Parse EventData to get item info if needed
                // Format from BroadcastItemAdded: "Item:ID,DisplayName:Name,Quantity:N,Slot:N,..."
                OnInventoryUpdated(); // For now, just refresh
            }
        }
    );
    
    ItemRemovedHandle = DelegateManager->SubscribeToUIEvent(
        [this](UObject* Source, const FGameplayTag& EventTag, const FString& EventData)
        {
            if (EventTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Inventory.Event.ItemRemoved"))))
            {
                OnInventoryUpdated();
            }
        }
    );
    
    ItemMovedHandle = DelegateManager->SubscribeToUIEvent(
        [this](UObject* Source, const FGameplayTag& EventTag, const FString& EventData)
        {
            if (EventTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Inventory.Event.ItemMoved"))))
            {
                OnInventoryUpdated();
            }
        }
    );
    
    // Alternative: If you need to work with the component's delegate directly
    // and it's exposed as a public property, you can do this:
    if (UMedComInventoryComponent* MedComInvComp = Cast<UMedComInventoryComponent>(InventoryComponent))
    {
        // If OnInventoryUpdated is a public UPROPERTY on the component
        // Note: This requires the delegate to be public in the component class
        // MedComInvComp->OnInventoryUpdated.AddDynamic(this, &UMedComInventoryUIConnector::OnInventoryUpdated);
        
        // But since OnInventoryUpdated is likely protected/private, we use EventDelegateManager
    }
}

void UMedComInventoryUIConnector::UnsubscribeFromEvents()
{
    // Get EventDelegateManager for unsubscription
    UEventDelegateManager* DelegateManager = GetDelegateManager();
    if (DelegateManager)
    {
        // Use the universal unsubscribe method for all handles
        if (InventoryUpdateHandle.IsValid())
        {
            DelegateManager->UniversalUnsubscribe(InventoryUpdateHandle);
            InventoryUpdateHandle.Reset();
        }
        
        if (ItemAddedHandle.IsValid())
        {
            DelegateManager->UniversalUnsubscribe(ItemAddedHandle);
            ItemAddedHandle.Reset();
        }
        
        if (ItemRemovedHandle.IsValid())
        {
            DelegateManager->UniversalUnsubscribe(ItemRemovedHandle);
            ItemRemovedHandle.Reset();
        }
        
        if (ItemMovedHandle.IsValid())
        {
            DelegateManager->UniversalUnsubscribe(ItemMovedHandle);
            ItemMovedHandle.Reset();
        }
        
        UE_LOG(LogInventory, Log, TEXT("UIConnector: Unsubscribed from all inventory events"));
    }
    
    // Alternative: If working with component's delegate directly
    if (InventoryComponent)
    {
        if (UMedComInventoryComponent* MedComInvComp = Cast<UMedComInventoryComponent>(InventoryComponent))
        {
            // If you added with AddDynamic, remove with RemoveDynamic
            // MedComInvComp->OnInventoryUpdated.RemoveDynamic(this, &UMedComInventoryUIConnector::OnInventoryUpdated);
        }
    }
}

void UMedComInventoryUIConnector::OnInventoryUpdated()
{
    // Notify UI bridge about update using the correct method
    if (UIBridge.GetInterface())
    {
        IMedComInventoryUIBridgeWidget::Execute_RefreshInventoryUI(UIBridge.GetObject());
    }
}

void UMedComInventoryUIConnector::OnItemAdded(const FInventoryItemInstance& Instance, int32 SlotIndex)
{
    // Specific handling for item added if needed
    OnInventoryUpdated();
}

void UMedComInventoryUIConnector::OnItemRemoved(const FName& ItemID, int32 Quantity, int32 SlotIndex)
{
    // Specific handling for item removed if needed
    OnInventoryUpdated();
}

void UMedComInventoryUIConnector::OnItemMoved(UObject* Item, int32 OldSlot, int32 NewSlot, bool bRotated)
{
    // Specific handling for item moved if needed
    OnInventoryUpdated();
}