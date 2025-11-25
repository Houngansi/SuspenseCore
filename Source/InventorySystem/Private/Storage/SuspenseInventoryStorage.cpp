// MedComInventory/Storage/MedComInventoryStorage.cpp
// Copyright Suspense Team. All Rights Reserved.

#include "Storage/SuspenseInventoryStorage.h"
#include "ItemSystem/MedComItemManager.h"
#include "Types/Loadout/MedComItemDataTable.h"
#include "Interfaces/Inventory/IMedComInventoryItemInterface.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Base/SuspenseSuspenseInventoryLogs.h"

//==================================================================
// Constructor and Lifecycle Implementation
//==================================================================

USuspenseInventoryStorage::USuspenseInventoryStorage()
{
    // Настройка базовых свойств компонента
    PrimaryComponentTick.bCanEverTick = false; // Storage не требует tick
    bWantsInitializeComponent = true; // Нужна инициализация
    
    // Инициализация состояния
    GridWidth = 0;
    GridHeight = 0;
    MaxWeight = 0.0f;
    bInitialized = false;
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("USuspenseInventoryStorage: Constructor called"));
}

void USuspenseInventoryStorage::BeginPlay()
{
    Super::BeginPlay();
    
    // Валидируем состояние если уже инициализирован
    if (bInitialized)
    {
        TArray<FString> ValidationErrors;
        if (!ValidateStorageIntegrity(ValidationErrors))
        {
            UE_LOG(LogInventory, Warning, TEXT("USuspenseInventoryStorage: Storage integrity validation failed"));
            for (const FString& Error : ValidationErrors)
            {
                UE_LOG(LogInventory, Warning, TEXT("  - %s"), *Error);
            }
        }
    }
}

void USuspenseInventoryStorage::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Откатываем активную транзакцию если есть
    if (IsTransactionActive())
    {
        UE_LOG(LogInventory, Warning, TEXT("USuspenseInventoryStorage: Rolling back active transaction during EndPlay"));
        RollbackTransaction();
    }
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("USuspenseInventoryStorage: EndPlay cleanup completed"));
    
    Super::EndPlay(EndPlayReason);
}

//==================================================================
// Core Storage Management Implementation
//==================================================================

bool USuspenseInventoryStorage::InitializeGrid(int32 Width, int32 Height, float InMaxWeight)
{
    // Валидация параметров
    if (Width <= 0 || Height <= 0)
    {
        UE_LOG(LogInventory, Error, TEXT("InitializeGrid: Invalid grid dimensions: %dx%d"), Width, Height);
        return false;
    }
    
    // Проверка на переполнение int32
    int64 TotalCells64 = static_cast<int64>(Width) * static_cast<int64>(Height);
    if (TotalCells64 > INT32_MAX)
    {
        UE_LOG(LogInventory, Error, TEXT("InitializeGrid: Grid size would overflow int32"));
        return false;
    }
    
    // Проверка разумных ограничений
    constexpr int32 MAX_GRID_SIZE = 100; // Предотвращаем создание слишком больших сеток
    if (Width > MAX_GRID_SIZE || Height > MAX_GRID_SIZE)
    {
        UE_LOG(LogInventory, Error, TEXT("InitializeGrid: Grid dimensions too large: %dx%d (max: %dx%d)"), 
            Width, Height, MAX_GRID_SIZE, MAX_GRID_SIZE);
        return false;
    }
    
    UE_LOG(LogInventory, Log, TEXT("InitializeGrid: Initializing storage with dimensions %dx%d, max weight %.1f"), 
        Width, Height, InMaxWeight);
    
    // Если уже инициализирован, сначала очищаем
    if (bInitialized)
    {
        UE_LOG(LogInventory, Warning, TEXT("InitializeGrid: Storage already initialized, clearing existing data"));
        ClearAllItems();
    }
    
    // Устанавливаем размеры сетки
    GridWidth = Width;
    GridHeight = Height;
    MaxWeight = InMaxWeight;
    
    // Инициализируем массив ячеек
    const int32 TotalCells = Width * Height;
    Cells.SetNum(TotalCells);
    
    // Инициализируем каждую ячейку с правильным индексом
    for (int32 i = 0; i < TotalCells; ++i)
    {
        Cells[i] = FInventoryCell(i);
    }
    
    // Инициализируем битовую карту свободных ячеек
    FreeCellsBitmap.Init(true, TotalCells);
    
    // Очищаем runtime данные
    StoredInstances.Empty();
    
    // Сбрасываем транзакцию если была активна
    ActiveTransaction = FInventoryTransaction();
    
    // Помечаем как инициализированный
    bInitialized = true;
    
    UE_LOG(LogInventory, Log, TEXT("InitializeGrid: Successfully initialized %dx%d grid (%d total cells)"), 
        Width, Height, TotalCells);
    
    return true;
}

int32 USuspenseInventoryStorage::GetFreeCellCount() const
{
    if (!bInitialized)
    {
        return 0;
    }
    
    // Подсчитываем свободные ячейки в битовой карте
    int32 FreeCount = 0;
    for (int32 i = 0; i < FreeCellsBitmap.Num(); ++i)
    {
        if (FreeCellsBitmap[i])
        {
            FreeCount++;
        }
    }
    
    return FreeCount;
}

//==================================================================
// Item Instance Management Implementation
//==================================================================

bool USuspenseInventoryStorage::AddItemInstance(const FInventoryItemInstance& ItemInstance, bool bAllowRotation)
{
    // Базовая валидация
    if (!bInitialized)
    {
        UE_LOG(LogInventory, Error, TEXT("AddItemInstance: Storage not initialized"));
        return false;
    }
    
    if (!ItemInstance.IsValid())
    {
        UE_LOG(LogInventory, Error, TEXT("AddItemInstance: Invalid item instance"));
        return false;
    }
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("AddItemInstance: Adding %s"), *ItemInstance.GetShortDebugString());
    
    // Проверяем ограничения по весу
    if (MaxWeight > 0.0f)
    {
        float ItemWeight = 0.0f;
        FMedComUnifiedItemData ItemData;
        if (GetItemData(ItemInstance.ItemID, ItemData))
        {
            ItemWeight = ItemData.Weight * ItemInstance.Quantity;
        }
        
        if (GetCurrentWeight() + ItemWeight > MaxWeight)
        {
            UE_LOG(LogInventory, Warning, TEXT("AddItemInstance: Weight limit exceeded - Current: %.1f, Adding: %.1f, Max: %.1f"), 
                GetCurrentWeight(), ItemWeight, MaxWeight);
            return false;
        }
    }
    
    // Ищем подходящее место
    int32 PlacementIndex = FindFreeSpace(ItemInstance.ItemID, bAllowRotation);
    if (PlacementIndex == INDEX_NONE)
    {
        UE_LOG(LogInventory, Warning, TEXT("AddItemInstance: No free space found for %s"), 
            *ItemInstance.ItemID.ToString());
        return false;
    }
    
    // Размещаем предмет
    if (PlaceItemInstance(ItemInstance, PlacementIndex))
    {
        UE_LOG(LogInventory, Log, TEXT("AddItemInstance: Successfully added %s at index %d"), 
            *ItemInstance.GetShortDebugString(), PlacementIndex);
        return true;
    }
    
    return false;
}

bool USuspenseInventoryStorage::RemoveItemInstance(const FGuid& InstanceID)
{
    if (!bInitialized)
    {
        UE_LOG(LogInventory, Error, TEXT("RemoveItemInstance: Storage not initialized"));
        return false;
    }
    
    if (!InstanceID.IsValid())
    {
        UE_LOG(LogInventory, Error, TEXT("RemoveItemInstance: Invalid instance ID"));
        return false;
    }
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("RemoveItemInstance: Removing instance %s"), *InstanceID.ToString());
    
    // Находим экземпляр
    FInventoryItemInstance* FoundInstance = FindStoredInstance(InstanceID);
    if (!FoundInstance)
    {
        UE_LOG(LogInventory, Warning, TEXT("RemoveItemInstance: Instance not found: %s"), *InstanceID.ToString());
        return false;
    }
    
    // Удаляем из ячеек сетки
    if (!RemoveInstanceFromCells(InstanceID))
    {
        UE_LOG(LogInventory, Error, TEXT("RemoveItemInstance: Failed to remove from grid cells"));
        return false;
    }
    
    // Удаляем из массива runtime экземпляров
    StoredInstances.RemoveAll([&InstanceID](const FInventoryItemInstance& Instance)
    {
        return Instance.InstanceID == InstanceID;
    });
    
    UE_LOG(LogInventory, Log, TEXT("RemoveItemInstance: Successfully removed instance %s"), 
        *InstanceID.ToString());
    
    return true;
}

bool USuspenseInventoryStorage::GetItemInstance(const FGuid& InstanceID, FInventoryItemInstance& OutInstance) const
{
    if (!bInitialized || !InstanceID.IsValid())
    {
        return false;
    }
    
    const FInventoryItemInstance* FoundInstance = FindStoredInstance(InstanceID);
    if (FoundInstance)
    {
        OutInstance = *FoundInstance;
        return true;
    }
    
    return false;
}

TArray<FInventoryItemInstance> USuspenseInventoryStorage::GetAllItemInstances() const
{
    return StoredInstances;
}

bool USuspenseInventoryStorage::UpdateItemInstance(const FInventoryItemInstance& UpdatedInstance)
{
    if (!bInitialized || !UpdatedInstance.IsValid())
    {
        return false;
    }
    
    // Находим существующий экземпляр
    FInventoryItemInstance* ExistingInstance = FindStoredInstance(UpdatedInstance.InstanceID);
    if (!ExistingInstance)
    {
        UE_LOG(LogInventory, Warning, TEXT("UpdateItemInstance: Instance not found: %s"), 
            *UpdatedInstance.InstanceID.ToString());
        return false;
    }
    
    // Проверяем что критические поля не изменились
    if (ExistingInstance->ItemID != UpdatedInstance.ItemID)
    {
        UE_LOG(LogInventory, Error, TEXT("UpdateItemInstance: Cannot change ItemID of existing instance"));
        return false;
    }
    
    // Обновляем экземпляр
    *ExistingInstance = UpdatedInstance;
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("UpdateItemInstance: Updated %s"), 
        *UpdatedInstance.GetShortDebugString());
    
    return true;
}

//==================================================================
// Space Management and Placement Implementation
//==================================================================

int32 USuspenseInventoryStorage::FindFreeSpace(const FName& ItemID, bool bAllowRotation, bool bOptimizeFragmentation) const
{
    if (!bInitialized || ItemID.IsNone())
    {
        return INDEX_NONE;
    }
    
    // Получаем данные предмета через ItemManager
    FVector2D BaseSize(1, 1); // Размер по умолчанию
    
    FMedComUnifiedItemData ItemData;
    if (GetItemData(ItemID, ItemData))
    {
        BaseSize = FVector2D(ItemData.GridSize.X, ItemData.GridSize.Y);
        UE_LOG(LogInventory, VeryVerbose, TEXT("FindFreeSpace: Looking for space for %s (size: %.0fx%.0f)"), 
            *ItemID.ToString(), BaseSize.X, BaseSize.Y);
    }
    else
    {
        UE_LOG(LogInventory, Warning, TEXT("FindFreeSpace: Item data not found for %s, using default size"), 
            *ItemID.ToString());
    }
    
    // Сначала пробуем обычную ориентацию
    int32 PlacementIndex = FindOptimalPlacement(BaseSize, bOptimizeFragmentation);
    
    // Если разрешен поворот и обычная ориентация не подошла, пробуем повернутую
    if (PlacementIndex == INDEX_NONE && bAllowRotation && BaseSize.X != BaseSize.Y)
    {
        FVector2D RotatedSize(BaseSize.Y, BaseSize.X);
        PlacementIndex = FindOptimalPlacement(RotatedSize, bOptimizeFragmentation);
        
        if (PlacementIndex != INDEX_NONE)
        {
            UE_LOG(LogInventory, VeryVerbose, TEXT("FindFreeSpace: Found space for %s with rotation at index %d"), 
                *ItemID.ToString(), PlacementIndex);
        }
    }
    
    if (PlacementIndex == INDEX_NONE)
    {
        UE_LOG(LogInventory, VeryVerbose, TEXT("FindFreeSpace: No space found for %s"), *ItemID.ToString());
    }
    
    return PlacementIndex;
}

bool USuspenseInventoryStorage::AreCellsFreeForItem(int32 StartIndex, const FName& ItemID, bool bIsRotated) const
{
    if (!bInitialized || !IsValidIndex(StartIndex) || ItemID.IsNone())
    {
        return false;
    }
    
    // Получаем размер через ItemManager
    FVector2D ItemSize(1, 1); // Размер по умолчанию
    
    FMedComUnifiedItemData ItemData;
    if (GetItemData(ItemID, ItemData))
    {
        ItemSize = FVector2D(ItemData.GridSize.X, ItemData.GridSize.Y);
    }
    else
    {
        UE_LOG(LogInventory, Warning, TEXT("AreCellsFreeForItem: Failed to get item data for %s, using default size"), 
            *ItemID.ToString());
    }
    
    // Применяем ротацию если нужно
    if (bIsRotated)
    {
        ItemSize = FVector2D(ItemSize.Y, ItemSize.X);
    }
    
    return AreCellsFree(StartIndex, ItemSize);
}

bool USuspenseInventoryStorage::AreCellsFree(int32 StartIndex, const FVector2D& Size) const
{
    if (!bInitialized || !IsValidIndex(StartIndex))
    {
        return false;
    }
    
    // Convert float size to integer grid cells
    int32 ItemWidth = FMath::CeilToInt(Size.X);
    int32 ItemHeight = FMath::CeilToInt(Size.Y);
    
    // Get grid coordinates of start position
    int32 StartX, StartY;
    if (!GetGridCoordinates(StartIndex, StartX, StartY))
    {
        return false;
    }
    
    // Check if item would extend beyond grid boundaries
    if (StartX + ItemWidth > GridWidth || StartY + ItemHeight > GridHeight)
    {
        return false;
    }
    
    // Check each cell that would be occupied by the item
    for (int32 Y = 0; Y < ItemHeight; ++Y)
    {
        for (int32 X = 0; X < ItemWidth; ++X)
        {
            int32 CellIndex = (StartY + Y) * GridWidth + (StartX + X);
            
            // Validate cell index
            if (!IsValidIndex(CellIndex))
            {
                return false;
            }
            
            // Check if cell is free using bitmap
            if (!FreeCellsBitmap[CellIndex])
            {
                return false; // Cell is occupied
            }
        }
    }
    
    return true; // All cells are free
}

bool USuspenseInventoryStorage::PlaceItemInstance(const FInventoryItemInstance& ItemInstance, int32 AnchorIndex)
{
    if (!bInitialized || !ItemInstance.IsValid() || !IsValidIndex(AnchorIndex))
    {
        return false;
    }
    
    // Проверяем что место свободно
    if (!AreCellsFreeForItem(AnchorIndex, ItemInstance.ItemID, ItemInstance.bIsRotated))
    {
        UE_LOG(LogInventory, Warning, TEXT("PlaceItemInstance: Cells not free at index %d"), AnchorIndex);
        return false;
    }
    
    // Создаем копию экземпляра с правильным размещением
    FInventoryItemInstance PlacedInstance = ItemInstance;
    PlacedInstance.AnchorIndex = AnchorIndex;
    
    // Добавляем в массив runtime экземпляров
    StoredInstances.Add(PlacedInstance);
    
    // Размещаем в ячейках сетки
    if (!PlaceInstanceInCells(PlacedInstance, AnchorIndex))
    {
        // Откатываем изменения при неудаче
        StoredInstances.RemoveAll([&PlacedInstance](const FInventoryItemInstance& Instance)
        {
            return Instance.InstanceID == PlacedInstance.InstanceID;
        });
        return false;
    }
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("PlaceItemInstance: Placed %s at index %d"), 
        *ItemInstance.GetShortDebugString(), AnchorIndex);
    
    return true;
}

bool USuspenseInventoryStorage::MoveItem(const FGuid& InstanceID, int32 NewAnchorIndex, bool bAllowRotation)
{
    if (!bInitialized || !InstanceID.IsValid() || !IsValidIndex(NewAnchorIndex))
    {
        UE_LOG(LogInventory, Warning, TEXT("MoveItem: Invalid parameters - Init:%d, ID:%s, Index:%d"), 
            bInitialized, *InstanceID.ToString(), NewAnchorIndex);
        return false;
    }
    
    // Find existing instance
    FInventoryItemInstance* ExistingInstance = FindStoredInstance(InstanceID);
    if (!ExistingInstance)
    {
        UE_LOG(LogInventory, Warning, TEXT("MoveItem: Instance not found: %s"), *InstanceID.ToString());
        return false;
    }
    
    // Save old state for rollback
    int32 OldAnchorIndex = ExistingInstance->AnchorIndex;
    bool bOldRotation = ExistingInstance->bIsRotated;
    
    // If already at target position, nothing to do
    if (OldAnchorIndex == NewAnchorIndex && !bAllowRotation)
    {
        UE_LOG(LogInventory, VeryVerbose, TEXT("MoveItem: Already at target position"));
        return true;
    }
    
    // CRITICAL: Create complete backup BEFORE any modifications
    FInventoryItemInstance BackupInstance = *ExistingInstance;
    TArray<FInventoryCell> BackupCells;
    
    // Backup all cells occupied by this item
    TArray<int32> OccupiedCells = GetOccupiedCells(InstanceID);
    for (int32 CellIdx : OccupiedCells)
    {
        if (IsValidIndex(CellIdx))
        {
            BackupCells.Add(Cells[CellIdx]);
        }
    }
    
    // Create temp instance for safe movement
    FInventoryItemInstance TempInstance = *ExistingInstance;
    
    // Begin transaction
    BeginTransaction();
    
    // Step 1: Remove from current position
    if (!RemoveInstanceFromCells(InstanceID))
    {
        UE_LOG(LogInventory, Error, TEXT("MoveItem: Failed to remove from current position"));
        RollbackTransaction();
        return false;
    }
    
    // Step 2: Update position in temp instance
    TempInstance.AnchorIndex = NewAnchorIndex;
    
    // Step 3: Check if we can place at new position
    bool bCanPlace = AreCellsFreeForItem(NewAnchorIndex, TempInstance.ItemID, TempInstance.bIsRotated);
    
    // Step 4: Try with rotation if allowed and normal placement fails
    if (!bCanPlace && bAllowRotation)
    {
        TempInstance.bIsRotated = !TempInstance.bIsRotated;
        bCanPlace = AreCellsFreeForItem(NewAnchorIndex, TempInstance.ItemID, TempInstance.bIsRotated);
    }
    
    // Step 5: Attempt placement
    if (bCanPlace)
    {
        if (PlaceInstanceInCells(TempInstance, NewAnchorIndex))
        {
            // Success - update the stored instance
            *ExistingInstance = TempInstance;
            
            // Update bitmap
            UpdateFreeCellsBitmap();
            
            // Commit transaction
            CommitTransaction();
            
            UE_LOG(LogInventory, Log, TEXT("MoveItem: Successfully moved %s from %d to %d (rotated: %s)"), 
                *TempInstance.ItemID.ToString(), 
                OldAnchorIndex, 
                NewAnchorIndex,
                TempInstance.bIsRotated ? TEXT("Yes") : TEXT("No"));
            
            return true;
        }
    }
    
    // Step 6: CRITICAL - Placement failed, restore to original position
    UE_LOG(LogInventory, Warning, TEXT("MoveItem: Failed to place at new position, restoring to original"));
    
    // First, try to rollback transaction
    RollbackTransaction();
    
    // Double-check that item is properly restored
    FInventoryItemInstance* RestoredInstance = FindStoredInstance(InstanceID);
    if (!RestoredInstance)
    {
        // CRITICAL ERROR - Item lost during move, recreate from backup
        UE_LOG(LogInventory, Error, TEXT("MoveItem: CRITICAL - Item lost during move, recreating from backup"));
        
        // Restore backup instance to storage
        StoredInstances.Add(BackupInstance);
        
        // Restore cells state using CellIndex instead of Index
        for (const FInventoryCell& BackupCell : BackupCells)
        {
            if (IsValidIndex(BackupCell.CellIndex))  // ИСПРАВЛЕНО: используем CellIndex
            {
                Cells[BackupCell.CellIndex] = BackupCell;  // ИСПРАВЛЕНО: используем CellIndex
            }
        }
        
        // Force bitmap update
        UpdateFreeCellsBitmap();
    }
    else
    {
        // Verify item is at original position
        if (RestoredInstance->AnchorIndex != OldAnchorIndex)
        {
            UE_LOG(LogInventory, Warning, TEXT("MoveItem: Item restored but at wrong position, fixing"));
            RestoredInstance->AnchorIndex = OldAnchorIndex;
            RestoredInstance->bIsRotated = bOldRotation;
            
            // Re-place at original position
            PlaceInstanceInCells(*RestoredInstance, OldAnchorIndex);
            UpdateFreeCellsBitmap();
        }
    }
    
    return false;
}

//==================================================================
// Item Queries and Access Implementation
//==================================================================

bool USuspenseInventoryStorage::GetItemInstanceAt(int32 Index, FInventoryItemInstance& OutInstance) const
{
    if (!bInitialized || !IsValidIndex(Index))
    {
        return false;
    }
    
    // Проверяем ячейку
    const FInventoryCell& Cell = Cells[Index];
    if (!Cell.IsOccupied() || !Cell.OccupyingInstanceID.IsValid())
    {
        return false;
    }
    
    // Находим runtime экземпляр по ID
    const FInventoryItemInstance* FoundInstance = FindStoredInstance(Cell.OccupyingInstanceID);
    if (FoundInstance)
    {
        OutInstance = *FoundInstance;
        return true;
    }
    
    return false;
}

int32 USuspenseInventoryStorage::GetItemCountByID(const FName& ItemID) const
{
    if (!bInitialized || ItemID.IsNone())
    {
        return 0;
    }
    
    int32 TotalCount = 0;
    
    // Подсчитываем в runtime экземплярах
    for (const FInventoryItemInstance& Instance : StoredInstances)
    {
        if (Instance.ItemID == ItemID)
        {
            TotalCount += Instance.Quantity;
        }
    }
    
    return TotalCount;
}

TArray<FInventoryItemInstance> USuspenseInventoryStorage::FindItemsByType(const FGameplayTag& ItemType) const
{
    TArray<FInventoryItemInstance> FoundItems;
    
    if (!bInitialized || !ItemType.IsValid())
    {
        return FoundItems;
    }
    
    UMedComItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        return FoundItems;
    }
    
    // Проверяем каждый runtime экземпляр
    for (const FInventoryItemInstance& Instance : StoredInstances)
    {
        FMedComUnifiedItemData ItemData;
        if (ItemManager->GetUnifiedItemData(Instance.ItemID, ItemData))
        {
            // Проверяем соответствие типа (поддерживаем иерархические теги)
            if (ItemData.ItemType.MatchesTag(ItemType))
            {
                FoundItems.Add(Instance);
            }
        }
    }
    
    return FoundItems;
}

TArray<int32> USuspenseInventoryStorage::GetOccupiedCells(const FGuid& InstanceID) const
{
    TArray<int32> OccupiedIndices;
    
    if (!bInitialized || !InstanceID.IsValid())
    {
        return OccupiedIndices;
    }
    
    // Ищем все ячейки занятые данным экземпляром
    for (int32 i = 0; i < Cells.Num(); ++i)
    {
        if (Cells[i].IsOccupied() && Cells[i].OccupyingInstanceID == InstanceID)
        {
            OccupiedIndices.Add(i);
        }
    }
    
    return OccupiedIndices;
}

//==================================================================
// Weight Management Implementation
//==================================================================

float USuspenseInventoryStorage::GetCurrentWeight() const
{
    if (!bInitialized)
    {
        return 0.0f;
    }
    
    float TotalWeight = 0.0f;
    UMedComItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        return 0.0f;
    }
    
    // Суммируем вес всех runtime экземпляров
    for (const FInventoryItemInstance& Instance : StoredInstances)
    {
        FMedComUnifiedItemData ItemData;
        if (ItemManager->GetUnifiedItemData(Instance.ItemID, ItemData))
        {
            TotalWeight += ItemData.Weight * Instance.Quantity;
        }
    }
    
    return TotalWeight;
}

void USuspenseInventoryStorage::SetMaxWeight(float NewMaxWeight)
{
    MaxWeight = FMath::Max(0.0f, NewMaxWeight);
    
    UE_LOG(LogInventory, Log, TEXT("SetMaxWeight: Updated max weight to %.1f"), MaxWeight);
}

bool USuspenseInventoryStorage::HasWeightCapacity(const FName& ItemID, int32 Quantity) const
{
    if (MaxWeight <= 0.0f) // Нет ограничений по весу
    {
        return true;
    }
    
    FMedComUnifiedItemData ItemData;
    if (!GetItemData(ItemID, ItemData))
    {
        return false;
    }
    
    float RequiredWeight = ItemData.Weight * Quantity;
    float AvailableWeight = MaxWeight - GetCurrentWeight();
    
    return AvailableWeight >= RequiredWeight;
}

//==================================================================
// Transaction Support Implementation
//==================================================================

void USuspenseInventoryStorage::BeginTransaction()
{
    if (!bInitialized)
    {
        UE_LOG(LogInventory, Error, TEXT("BeginTransaction: Storage not initialized"));
        return;
    }
    
    if (ActiveTransaction.bIsActive)
    {
        UE_LOG(LogInventory, Warning, TEXT("BeginTransaction: Transaction already active, committing previous"));
        CommitTransaction();
    }
    
    // Создаем snapshot текущего состояния
    CreateTransactionSnapshot();
    
    ActiveTransaction.bIsActive = true;
    ActiveTransaction.StartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("BeginTransaction: Started new transaction"));
}

void USuspenseInventoryStorage::CommitTransaction()
{
    if (!ActiveTransaction.bIsActive)
    {
        UE_LOG(LogInventory, Warning, TEXT("CommitTransaction: No active transaction"));
        return;
    }
    
    // Просто очищаем snapshot - изменения остаются
    ActiveTransaction = FInventoryTransaction();
    
    UE_LOG(LogInventory, VeryVerbose, TEXT("CommitTransaction: Transaction committed"));
}

void USuspenseInventoryStorage::RollbackTransaction()
{
    if (!ActiveTransaction.bIsActive)
    {
        UE_LOG(LogInventory, Warning, TEXT("RollbackTransaction: No active transaction"));
        return;
    }
    
    // Восстанавливаем состояние из snapshot
    RestoreFromTransactionSnapshot();
    
    // Очищаем транзакцию
    ActiveTransaction = FInventoryTransaction();
    
    UE_LOG(LogInventory, Log, TEXT("RollbackTransaction: Transaction rolled back"));
}

bool USuspenseInventoryStorage::IsTransactionActive() const
{
    if (!ActiveTransaction.bIsActive)
    {
        return false;
    }
    
    // Проверяем timeout
    float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    return ActiveTransaction.IsValid(CurrentTime);
}

//==================================================================
// Maintenance and Utilities Implementation
//==================================================================

void USuspenseInventoryStorage::ClearAllItems()
{
    if (!bInitialized)
    {
        return;
    }
    
    UE_LOG(LogInventory, Log, TEXT("ClearAllItems: Clearing %d items from storage"), StoredInstances.Num());
    
    // Очищаем все ячейки
    for (FInventoryCell& Cell : Cells)
    {
        Cell.Clear();
    }
    
    // Сбрасываем битовую карту
    FreeCellsBitmap.Init(true, GridWidth * GridHeight);
    
    // Очищаем runtime экземпляры
    StoredInstances.Empty();
    
    // Сбрасываем активную транзакцию
    ActiveTransaction = FInventoryTransaction();
    
    UE_LOG(LogInventory, Log, TEXT("ClearAllItems: Storage cleared"));
}

bool USuspenseInventoryStorage::ValidateStorageIntegrity(TArray<FString>& OutErrors, bool bAutoFix)
{
    OutErrors.Empty();
    
    if (!bInitialized)
    {
        OutErrors.Add(TEXT("Storage not initialized"));
        return false;
    }
    
    bool bIsValid = true;
    
    // Проверяем соответствие между ячейками и runtime экземплярами
    TSet<FGuid> CellInstanceIDs;
    TSet<FGuid> StoredInstanceIDs;
    
    // Собираем ID из ячеек
    for (const FInventoryCell& Cell : Cells)
    {
        if (Cell.IsOccupied() && Cell.OccupyingInstanceID.IsValid())
        {
            CellInstanceIDs.Add(Cell.OccupyingInstanceID);
        }
    }
    
    // Собираем ID из runtime экземпляров
    for (const FInventoryItemInstance& Instance : StoredInstances)
    {
        StoredInstanceIDs.Add(Instance.InstanceID);
        
        // Проверяем что экземпляр размещен в сетке
        if (Instance.IsPlacedInInventory())
        {
            if (!IsValidIndex(Instance.AnchorIndex))
            {
                OutErrors.Add(FString::Printf(TEXT("Instance %s has invalid anchor index %d"), 
                    *Instance.InstanceID.ToString(), Instance.AnchorIndex));
                bIsValid = false;
            }
        }
    }
    
    // Проверяем orphaned ячейки (ячейки ссылающиеся на несуществующие экземпляры)
    for (const FGuid& CellInstanceID : CellInstanceIDs)
    {
        if (!StoredInstanceIDs.Contains(CellInstanceID))
        {
            OutErrors.Add(FString::Printf(TEXT("Orphaned cell references instance %s"), 
                *CellInstanceID.ToString()));
            bIsValid = false;
            
            if (bAutoFix)
            {
                // Очищаем orphaned ячейки
                for (FInventoryCell& Cell : Cells)
                {
                    if (Cell.OccupyingInstanceID == CellInstanceID)
                    {
                        Cell.Clear();
                    }
                }
            }
        }
    }
    
    // Проверяем что все размещенные экземпляры имеют соответствующие ячейки
    for (const FGuid& StoredInstanceID : StoredInstanceIDs)
    {
        const FInventoryItemInstance* Instance = FindStoredInstance(StoredInstanceID);
        if (Instance && Instance->IsPlacedInInventory())
        {
            if (!CellInstanceIDs.Contains(StoredInstanceID))
            {
                OutErrors.Add(FString::Printf(TEXT("Instance %s claims to be placed but has no cells"), 
                    *StoredInstanceID.ToString()));
                bIsValid = false;
            }
        }
    }
    
    // Проверяем соответствие битовой карты
    for (int32 i = 0; i < Cells.Num() && i < FreeCellsBitmap.Num(); ++i)
    {
        bool bCellOccupied = Cells[i].IsOccupied();
        bool bBitmapSaysFree = FreeCellsBitmap[i];
        
        if (bCellOccupied && bBitmapSaysFree)
        {
            OutErrors.Add(FString::Printf(TEXT("Bitmap inconsistency: Cell %d is occupied but bitmap shows free"), i));
            bIsValid = false;
        }
        else if (!bCellOccupied && !bBitmapSaysFree)
        {
            OutErrors.Add(FString::Printf(TEXT("Bitmap inconsistency: Cell %d is free but bitmap shows occupied"), i));
            bIsValid = false;
        }
    }
    
    // Автоматическое исправление битовой карты если запрошено
    if (bAutoFix && !bIsValid)
    {
        UpdateFreeCellsBitmap();
        UE_LOG(LogInventory, Log, TEXT("ValidateStorageIntegrity: Auto-fixed bitmap inconsistencies"));
    }
    
    return bIsValid;
}

FString USuspenseInventoryStorage::GetStorageDebugInfo() const
{
    if (!bInitialized)
    {
        return TEXT("Storage not initialized");
    }
    
    FString DebugInfo = FString::Printf(
        TEXT("=== Storage Debug Info ===\n")
        TEXT("Grid Size: %dx%d (%d total cells)\n")
        TEXT("Free Cells: %d\n")
        TEXT("Stored Instances: %d\n")
        TEXT("Current Weight: %.1f / %.1f\n")
        TEXT("Transaction Active: %s\n"),
        GridWidth, GridHeight, GetTotalCells(),
        GetFreeCellCount(),
        StoredInstances.Num(),
        GetCurrentWeight(), MaxWeight,
        IsTransactionActive() ? TEXT("Yes") : TEXT("No")
    );
    
    // Добавляем информацию о каждом runtime экземпляре
    if (StoredInstances.Num() > 0)
    {
        DebugInfo += TEXT("\nRuntime Instances:\n");
        for (const FInventoryItemInstance& Instance : StoredInstances)
        {
            DebugInfo += FString::Printf(TEXT("  %s\n"), *Instance.GetDebugString());
        }
    }
    
    return DebugInfo;
}

int32 USuspenseInventoryStorage::DefragmentStorage()
{
    if (!bInitialized || StoredInstances.Num() == 0)
    {
        return 0;
    }
    
    UE_LOG(LogInventory, Log, TEXT("DefragmentStorage: Starting defragmentation"));
    
    // Начинаем транзакцию для atomic операции
    BeginTransaction();
    
    // Собираем все экземпляры для переразмещения
    TArray<FInventoryItemInstance> AllInstances = StoredInstances;
    
    // Очищаем storage
    ClearAllItems();
    
    // Сортируем по размеру (сначала большие предметы)
    AllInstances.Sort([this](const FInventoryItemInstance& A, const FInventoryItemInstance& B)
    {
        FMedComUnifiedItemData DataA, DataB;
        float AreaA = 1.0f, AreaB = 1.0f;
        
        if (GetItemData(A.ItemID, DataA))
        {
            FVector2D SizeA(DataA.GridSize.X, DataA.GridSize.Y);
            if (A.bIsRotated)
            {
                SizeA = FVector2D(SizeA.Y, SizeA.X);
            }
            AreaA = SizeA.X * SizeA.Y;
        }
        
        if (GetItemData(B.ItemID, DataB))
        {
            FVector2D SizeB(DataB.GridSize.X, DataB.GridSize.Y);
            if (B.bIsRotated)
            {
                SizeB = FVector2D(SizeB.Y, SizeB.X);
            }
            AreaB = SizeB.X * SizeB.Y;
        }
        
        return AreaA > AreaB;
    });
    
    // Переразмещаем предметы оптимально
    int32 MovedCount = 0;
    for (const FInventoryItemInstance& Instance : AllInstances)
    {
        if (AddItemInstance(Instance, true)) // Разрешаем поворот для оптимизации
        {
            MovedCount++;
        }
        else
        {
            UE_LOG(LogInventory, Warning, TEXT("DefragmentStorage: Failed to place item %s during defragmentation"), 
                *Instance.ItemID.ToString());
        }
    }
    
    // Коммитим изменения
    CommitTransaction();
    
    UE_LOG(LogInventory, Log, TEXT("DefragmentStorage: Successfully moved %d items"), MovedCount);
    return MovedCount;
}

//==================================================================
// Internal Helper Methods Implementation
//==================================================================

bool USuspenseInventoryStorage::IsValidIndex(int32 Index) const
{
    return bInitialized && Index >= 0 && Index < (GridWidth * GridHeight);
}

void USuspenseInventoryStorage::UpdateFreeCellsBitmap()
{
    if (!bInitialized)
    {
        return;
    }
    
    // Обновляем битовую карту на основе занятости ячеек
    for (int32 i = 0; i < Cells.Num() && i < FreeCellsBitmap.Num(); ++i)
    {
        FreeCellsBitmap[i] = !Cells[i].IsOccupied();
    }
}

UMedComItemManager* USuspenseInventoryStorage::GetItemManager() const
{
    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GameInstance = World->GetGameInstance())
        {
            return GameInstance->GetSubsystem<UMedComItemManager>();
        }
    }
    
    return nullptr;
}

bool USuspenseInventoryStorage::GetItemData(const FName& ItemID, FMedComUnifiedItemData& OutData) const
{
    UMedComItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        return false;
    }
    return ItemManager->GetUnifiedItemData(ItemID, OutData);
}

bool USuspenseInventoryStorage::PlaceInstanceInCells(const FInventoryItemInstance& ItemInstance, int32 AnchorIndex)
{
    // Базовая валидация
    if (!bInitialized || !ItemInstance.IsValid() || !IsValidIndex(AnchorIndex))
    {
        UE_LOG(LogInventory, Error, TEXT("PlaceInstanceInCells: Invalid parameters"));
        return false;
    }
    
    // Получаем размер предмета через ItemManager
    FVector2D ItemSize(1, 1); // Размер по умолчанию
    
    FMedComUnifiedItemData ItemData;
    if (GetItemData(ItemInstance.ItemID, ItemData))
    {
        // Получаем базовый размер из DataTable
        ItemSize = FVector2D(ItemData.GridSize.X, ItemData.GridSize.Y);
        
        UE_LOG(LogInventory, Log, TEXT("PlaceInstanceInCells: Got size from ItemManager for %s: %.0fx%.0f"), 
            *ItemInstance.ItemID.ToString(), ItemSize.X, ItemSize.Y);
    }
    else
    {
        UE_LOG(LogInventory, Warning, TEXT("PlaceInstanceInCells: Failed to get item data for %s, using default size"), 
            *ItemInstance.ItemID.ToString());
    }
    
    // Применяем ротацию к размеру если предмет повернут
    if (ItemInstance.bIsRotated)
    {
        ItemSize = FVector2D(ItemSize.Y, ItemSize.X);
    }
    
    // Конвертируем в целые размеры для работы с сеткой
    int32 ItemWidth = FMath::CeilToInt(ItemSize.X);
    int32 ItemHeight = FMath::CeilToInt(ItemSize.Y);
    
    // Детальное логирование для отладки
    UE_LOG(LogInventory, Log, TEXT("PlaceInstanceInCells: Placing %s at index %d, final size %dx%d (rotated: %s)"), 
        *ItemInstance.ItemID.ToString(), 
        AnchorIndex, 
        ItemWidth, 
        ItemHeight,
        ItemInstance.bIsRotated ? TEXT("Yes") : TEXT("No"));
    
    // Получаем координаты якорной ячейки в сетке
    int32 AnchorX, AnchorY;
    if (!GetGridCoordinates(AnchorIndex, AnchorX, AnchorY))
    {
        UE_LOG(LogInventory, Error, TEXT("PlaceInstanceInCells: Invalid anchor index %d"), AnchorIndex);
        return false;
    }
    
    // Проверяем, что предмет не выйдет за границы сетки
    if (AnchorX + ItemWidth > GridWidth || AnchorY + ItemHeight > GridHeight)
    {
        UE_LOG(LogInventory, Warning, TEXT("PlaceInstanceInCells: Item would exceed grid bounds - Position: (%d,%d), Size: %dx%d, Grid: %dx%d"), 
            AnchorX, AnchorY, ItemWidth, ItemHeight, GridWidth, GridHeight);
        return false;
    }
    
    // Сначала собираем все ячейки, которые нужно занять, и проверяем их доступность
    TArray<int32> CellsToOccupy;
    CellsToOccupy.Reserve(ItemWidth * ItemHeight);
    
    for (int32 Y = 0; Y < ItemHeight; ++Y)
    {
        for (int32 X = 0; X < ItemWidth; ++X)
        {
            int32 CellIndex = (AnchorY + Y) * GridWidth + (AnchorX + X);
            
            if (!IsValidIndex(CellIndex))
            {
                UE_LOG(LogInventory, Error, TEXT("PlaceInstanceInCells: Invalid cell index %d"), CellIndex);
                return false;
            }
            
            // Проверяем через битовую карту для быстрой проверки
            if (!FreeCellsBitmap[CellIndex])
            {
                UE_LOG(LogInventory, Warning, TEXT("PlaceInstanceInCells: Cell %d already occupied"), CellIndex);
                return false;
            }
            
            // Дополнительная проверка через сами ячейки для надежности
            if (Cells[CellIndex].IsOccupied())
            {
                UE_LOG(LogInventory, Error, TEXT("PlaceInstanceInCells: Cell %d occupied but bitmap says free - integrity error!"), CellIndex);
                return false;
            }
            
            CellsToOccupy.Add(CellIndex);
        }
    }
    
    // Все необходимые ячейки свободны, теперь занимаем их атомарно
    for (int32 CellIndex : CellsToOccupy)
    {
        // Обновляем структуру ячейки
        Cells[CellIndex].bIsOccupied = true;
        Cells[CellIndex].OccupyingInstanceID = ItemInstance.InstanceID;
        
        // Синхронизируем битовую карту
        FreeCellsBitmap[CellIndex] = false;
    }
    
    UE_LOG(LogInventory, Log, TEXT("PlaceInstanceInCells: Successfully occupied %d cells for item %s (ID: %s)"), 
        CellsToOccupy.Num(), 
        *ItemInstance.ItemID.ToString(),
        *ItemInstance.InstanceID.ToString());
    
    return true;
}

bool USuspenseInventoryStorage::RemoveInstanceFromCells(const FGuid& InstanceID)
{
    bool bRemovedAny = false;
    
    // Ищем и очищаем все ячейки с данным ID
    for (int32 i = 0; i < Cells.Num(); ++i)
    {
        if (Cells[i].IsOccupied() && Cells[i].OccupyingInstanceID == InstanceID)
        {
            Cells[i].Clear();
            FreeCellsBitmap[i] = true;
            bRemovedAny = true;
        }
    }
    
    return bRemovedAny;
}

FInventoryItemInstance* USuspenseInventoryStorage::FindStoredInstance(const FGuid& InstanceID)
{
    for (FInventoryItemInstance& Instance : StoredInstances)
    {
        if (Instance.InstanceID == InstanceID)
        {
            return &Instance;
        }
    }
    return nullptr;
}

const FInventoryItemInstance* USuspenseInventoryStorage::FindStoredInstance(const FGuid& InstanceID) const
{
    for (const FInventoryItemInstance& Instance : StoredInstances)
    {
        if (Instance.InstanceID == InstanceID)
        {
            return &Instance;
        }
    }
    return nullptr;
}

void USuspenseInventoryStorage::CreateTransactionSnapshot()
{
    ActiveTransaction.CellsSnapshot = Cells;
    ActiveTransaction.InstancesSnapshot = StoredInstances;
}

void USuspenseInventoryStorage::RestoreFromTransactionSnapshot()
{
    Cells = ActiveTransaction.CellsSnapshot;
    StoredInstances = ActiveTransaction.InstancesSnapshot;
    
    // Пересчитываем битовую карту
    UpdateFreeCellsBitmap();
}

int32 USuspenseInventoryStorage::FindOptimalPlacement(const FVector2D& ItemSize, bool bOptimizeFragmentation) const
{
    if (!bInitialized || ItemSize.X <= 0 || ItemSize.Y <= 0)
    {
        return INDEX_NONE;
    }
    
    int32 ItemWidth = FMath::CeilToInt(ItemSize.X);
    int32 ItemHeight = FMath::CeilToInt(ItemSize.Y);
    
    // Проверяем что предмет помещается в сетку
    if (ItemWidth > GridWidth || ItemHeight > GridHeight)
    {
        return INDEX_NONE;
    }
    
    int32 BestIndex = INDEX_NONE;
    int32 BestScore = INT_MAX;
    
    // Ищем все возможные позиции
    for (int32 Y = 0; Y <= GridHeight - ItemHeight; ++Y)
    {
        for (int32 X = 0; X <= GridWidth - ItemWidth; ++X)
        {
            int32 StartIndex = Y * GridWidth + X;
            
            // Проверяем что все ячейки свободны
            bool bAllFree = true;
            for (int32 CheckY = 0; CheckY < ItemHeight && bAllFree; ++CheckY)
            {
                for (int32 CheckX = 0; CheckX < ItemWidth && bAllFree; ++CheckX)
                {
                    int32 CellIndex = (Y + CheckY) * GridWidth + (X + CheckX);
                    if (!FreeCellsBitmap[CellIndex])
                    {
                        bAllFree = false;
                    }
                }
            }
            
            if (bAllFree)
            {
                if (!bOptimizeFragmentation)
                {
                    return StartIndex; // Возвращаем первое найденное место
                }
                
                // Вычисляем score для минимизации фрагментации
                int32 Score = X + Y; // Предпочитаем левый верхний угол
                
                if (Score < BestScore)
                {
                    BestScore = Score;
                    BestIndex = StartIndex;
                }
            }
        }
    }
    
    return BestIndex;
}

bool USuspenseInventoryStorage::GetGridCoordinates(int32 Index, int32& OutX, int32& OutY) const
{
    if (!IsValidIndex(Index))
    {
        OutX = -1;
        OutY = -1;
        return false;
    }
    
    OutX = Index % GridWidth;
    OutY = Index / GridWidth;
    
    return true;
}

bool USuspenseInventoryStorage::GetLinearIndex(int32 X, int32 Y, int32& OutIndex) const
{
    if (!bInitialized || X < 0 || Y < 0 || X >= GridWidth || Y >= GridHeight)
    {
        OutIndex = INDEX_NONE;
        return false;
    }
    
    OutIndex = Y * GridWidth + X;
    return true;
}