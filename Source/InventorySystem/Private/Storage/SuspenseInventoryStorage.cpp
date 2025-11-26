// SuspenseInventory/Storage/SuspenseInventoryStorage.cpp
// Copyright SuspenseCore Team. All Rights Reserved.

#include "Storage/SuspenseInventoryStorage.h"
#include "ItemSystem/SuspenseItemManager.h"
#include "Types/Loadout/SuspenseItemDataTable.h"
#include "Interfaces/Inventory/ISuspenseInventoryItemInterface.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Base/InventoryLogs.h"

//==================================================================
// Constructor and Lifecycle Implementation
//==================================================================

USuspenseInventoryStorage::USuspenseInventoryStorage()
{
    // Base component property setup
    PrimaryComponentTick.bCanEverTick = false;
    bWantsInitializeComponent = true;

    // State initialization
    GridWidth = 0;
    GridHeight = 0;
    MaxWeight = 0.0f;
    bInitialized = false;

    UE_LOG(LogSuspenseInventory, VeryVerbose, TEXT("USuspenseInventoryStorage: Constructor called"));
}

void USuspenseInventoryStorage::BeginPlay()
{
    Super::BeginPlay();

    // Validate state if already initialized
    if (bInitialized)
    {
        TArray<FString> ValidationErrors;
        if (!ValidateStorageIntegrity(ValidationErrors))
        {
            UE_LOG(LogSuspenseInventory, Warning, TEXT("USuspenseInventoryStorage: Storage integrity validation failed"));
            for (const FString& Error : ValidationErrors)
            {
                UE_LOG(LogSuspenseInventory, Warning, TEXT("  - %s"), *Error);
            }
        }
    }
}

void USuspenseInventoryStorage::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Rollback active transaction if any
    if (IsTransactionActive())
    {
        UE_LOG(LogSuspenseInventory, Warning, TEXT("USuspenseInventoryStorage: Rolling back active transaction during EndPlay"));
        RollbackTransaction();
    }

    UE_LOG(LogSuspenseInventory, VeryVerbose, TEXT("USuspenseInventoryStorage: EndPlay cleanup completed"));

    Super::EndPlay(EndPlayReason);
}

//==================================================================
// Core Storage Management Implementation
//==================================================================

bool USuspenseInventoryStorage::InitializeGrid(int32 Width, int32 Height, float InMaxWeight)
{
    // Validate parameters
    if (Width <= 0 || Height <= 0)
    {
        UE_LOG(LogSuspenseInventory, Error, TEXT("InitializeGrid: Invalid grid dimensions: %dx%d"), Width, Height);
        return false;
    }

    // Check for int32 overflow
    int64 TotalCells64 = static_cast<int64>(Width) * static_cast<int64>(Height);
    if (TotalCells64 > INT32_MAX)
    {
        UE_LOG(LogSuspenseInventory, Error, TEXT("InitializeGrid: Grid size would overflow int32"));
        return false;
    }

    // Check reasonable limits
    constexpr int32 MAX_GRID_SIZE = 100;
    if (Width > MAX_GRID_SIZE || Height > MAX_GRID_SIZE)
    {
        UE_LOG(LogSuspenseInventory, Error, TEXT("InitializeGrid: Grid dimensions too large: %dx%d (max: %dx%d)"),
            Width, Height, MAX_GRID_SIZE, MAX_GRID_SIZE);
        return false;
    }

    UE_LOG(LogSuspenseInventory, Log, TEXT("InitializeGrid: Initializing storage with dimensions %dx%d, max weight %.1f"),
        Width, Height, InMaxWeight);

    // If already initialized, clear first
    if (bInitialized)
    {
        UE_LOG(LogSuspenseInventory, Warning, TEXT("InitializeGrid: Storage already initialized, clearing existing data"));
        ClearAllItems();
    }

    // Set grid dimensions
    GridWidth = Width;
    GridHeight = Height;
    MaxWeight = InMaxWeight;

    // Initialize cells array
    const int32 TotalCells = Width * Height;
    Cells.SetNum(TotalCells);

    // Initialize each cell with proper index
    for (int32 i = 0; i < TotalCells; ++i)
    {
        Cells[i] = FInventoryCell(i);
    }

    // Initialize free cells bitmap
    FreeCellsBitmap.Init(true, TotalCells);

    // Clear runtime data
    StoredInstances.Empty();

    // Reset transaction if active
    ActiveTransaction = FSuspenseStorageTransaction();

    // Mark as initialized
    bInitialized = true;

    UE_LOG(LogSuspenseInventory, Log, TEXT("InitializeGrid: Successfully initialized %dx%d grid (%d total cells)"),
        Width, Height, TotalCells);

    return true;
}

int32 USuspenseInventoryStorage::GetFreeCellCount() const
{
    if (!bInitialized)
    {
        return 0;
    }

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
    if (!bInitialized)
    {
        UE_LOG(LogSuspenseInventory, Error, TEXT("AddItemInstance: Storage not initialized"));
        return false;
    }

    if (!ItemInstance.IsValid())
    {
        UE_LOG(LogSuspenseInventory, Error, TEXT("AddItemInstance: Invalid item instance"));
        return false;
    }

    UE_LOG(LogSuspenseInventory, VeryVerbose, TEXT("AddItemInstance: Adding %s"), *ItemInstance.GetShortDebugString());

    // Check weight restrictions
    if (MaxWeight > 0.0f)
    {
        float ItemWeight = 0.0f;
        FSuspenseUnifiedItemData ItemData;
        if (GetItemData(ItemInstance.ItemID, ItemData))
        {
            ItemWeight = ItemData.Weight * ItemInstance.Quantity;
        }

        if (GetCurrentWeight() + ItemWeight > MaxWeight)
        {
            UE_LOG(LogSuspenseInventory, Warning, TEXT("AddItemInstance: Weight limit exceeded - Current: %.1f, Adding: %.1f, Max: %.1f"),
                GetCurrentWeight(), ItemWeight, MaxWeight);
            return false;
        }
    }

    // Find suitable space
    int32 PlacementIndex = FindFreeSpace(ItemInstance.ItemID, bAllowRotation);
    if (PlacementIndex == INDEX_NONE)
    {
        UE_LOG(LogSuspenseInventory, Warning, TEXT("AddItemInstance: No free space found for %s"),
            *ItemInstance.ItemID.ToString());
        return false;
    }

    // Place item
    if (PlaceItemInstance(ItemInstance, PlacementIndex))
    {
        UE_LOG(LogSuspenseInventory, Log, TEXT("AddItemInstance: Successfully added %s at index %d"),
            *ItemInstance.GetShortDebugString(), PlacementIndex);
        return true;
    }

    return false;
}

bool USuspenseInventoryStorage::RemoveItemInstance(const FGuid& InstanceID)
{
    if (!bInitialized)
    {
        UE_LOG(LogSuspenseInventory, Error, TEXT("RemoveItemInstance: Storage not initialized"));
        return false;
    }

    if (!InstanceID.IsValid())
    {
        UE_LOG(LogSuspenseInventory, Error, TEXT("RemoveItemInstance: Invalid instance ID"));
        return false;
    }

    UE_LOG(LogSuspenseInventory, VeryVerbose, TEXT("RemoveItemInstance: Removing instance %s"), *InstanceID.ToString());

    // Find instance
    FInventoryItemInstance* FoundInstance = FindStoredInstance(InstanceID);
    if (!FoundInstance)
    {
        UE_LOG(LogSuspenseInventory, Warning, TEXT("RemoveItemInstance: Instance not found: %s"), *InstanceID.ToString());
        return false;
    }

    // Remove from grid cells
    if (!RemoveInstanceFromCells(InstanceID))
    {
        UE_LOG(LogSuspenseInventory, Error, TEXT("RemoveItemInstance: Failed to remove from grid cells"));
        return false;
    }

    // Remove from runtime instances array
    StoredInstances.RemoveAll([&InstanceID](const FInventoryItemInstance& Instance)
    {
        return Instance.InstanceID == InstanceID;
    });

    UE_LOG(LogSuspenseInventory, Log, TEXT("RemoveItemInstance: Successfully removed instance %s"),
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

    // Find existing instance
    FInventoryItemInstance* ExistingInstance = FindStoredInstance(UpdatedInstance.InstanceID);
    if (!ExistingInstance)
    {
        UE_LOG(LogSuspenseInventory, Warning, TEXT("UpdateItemInstance: Instance not found: %s"),
            *UpdatedInstance.InstanceID.ToString());
        return false;
    }

    // Check that critical fields haven't changed
    if (ExistingInstance->ItemID != UpdatedInstance.ItemID)
    {
        UE_LOG(LogSuspenseInventory, Error, TEXT("UpdateItemInstance: Cannot change ItemID of existing instance"));
        return false;
    }

    // Update instance
    *ExistingInstance = UpdatedInstance;

    UE_LOG(LogSuspenseInventory, VeryVerbose, TEXT("UpdateItemInstance: Updated %s"),
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

    // Get item data through ItemManager
    FVector2D BaseSize(1, 1); // Default size

    FSuspenseUnifiedItemData ItemData;
    if (GetItemData(ItemID, ItemData))
    {
        BaseSize = FVector2D(ItemData.GridSize.X, ItemData.GridSize.Y);
        UE_LOG(LogSuspenseInventory, VeryVerbose, TEXT("FindFreeSpace: Looking for space for %s (size: %.0fx%.0f)"),
            *ItemID.ToString(), BaseSize.X, BaseSize.Y);
    }
    else
    {
        UE_LOG(LogSuspenseInventory, Warning, TEXT("FindFreeSpace: Item data not found for %s, using default size"),
            *ItemID.ToString());
    }

    // Try normal orientation first
    int32 PlacementIndex = FindOptimalPlacement(BaseSize, bOptimizeFragmentation);

    // If rotation allowed and normal orientation didn't fit, try rotated
    if (PlacementIndex == INDEX_NONE && bAllowRotation && BaseSize.X != BaseSize.Y)
    {
        FVector2D RotatedSize(BaseSize.Y, BaseSize.X);
        PlacementIndex = FindOptimalPlacement(RotatedSize, bOptimizeFragmentation);

        if (PlacementIndex != INDEX_NONE)
        {
            UE_LOG(LogSuspenseInventory, VeryVerbose, TEXT("FindFreeSpace: Found space for %s with rotation at index %d"),
                *ItemID.ToString(), PlacementIndex);
        }
    }

    if (PlacementIndex == INDEX_NONE)
    {
        UE_LOG(LogSuspenseInventory, VeryVerbose, TEXT("FindFreeSpace: No space found for %s"), *ItemID.ToString());
    }

    return PlacementIndex;
}

bool USuspenseInventoryStorage::AreCellsFreeForItem(int32 StartIndex, const FName& ItemID, bool bIsRotated) const
{
    if (!bInitialized || !IsValidIndex(StartIndex) || ItemID.IsNone())
    {
        return false;
    }

    // Get size through ItemManager
    FVector2D ItemSize(1, 1); // Default size

    FSuspenseUnifiedItemData ItemData;
    if (GetItemData(ItemID, ItemData))
    {
        ItemSize = FVector2D(ItemData.GridSize.X, ItemData.GridSize.Y);
    }
    else
    {
        UE_LOG(LogSuspenseInventory, Warning, TEXT("AreCellsFreeForItem: Failed to get item data for %s, using default size"),
            *ItemID.ToString());
    }

    // Apply rotation if needed
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

    int32 ItemWidth = FMath::CeilToInt(Size.X);
    int32 ItemHeight = FMath::CeilToInt(Size.Y);

    int32 StartX, StartY;
    if (!GetGridCoordinates(StartIndex, StartX, StartY))
    {
        return false;
    }

    if (StartX + ItemWidth > GridWidth || StartY + ItemHeight > GridHeight)
    {
        return false;
    }

    for (int32 Y = 0; Y < ItemHeight; ++Y)
    {
        for (int32 X = 0; X < ItemWidth; ++X)
        {
            int32 CellIndex = (StartY + Y) * GridWidth + (StartX + X);

            if (!IsValidIndex(CellIndex))
            {
                return false;
            }

            if (!FreeCellsBitmap[CellIndex])
            {
                return false;
            }
        }
    }

    return true;
}

bool USuspenseInventoryStorage::PlaceItemInstance(const FInventoryItemInstance& ItemInstance, int32 AnchorIndex)
{
    if (!bInitialized || !ItemInstance.IsValid() || !IsValidIndex(AnchorIndex))
    {
        return false;
    }

    // Check that space is free
    if (!AreCellsFreeForItem(AnchorIndex, ItemInstance.ItemID, ItemInstance.bIsRotated))
    {
        UE_LOG(LogSuspenseInventory, Warning, TEXT("PlaceItemInstance: Cells not free at index %d"), AnchorIndex);
        return false;
    }

    // Create copy with proper placement
    FInventoryItemInstance PlacedInstance = ItemInstance;
    PlacedInstance.AnchorIndex = AnchorIndex;

    // Add to runtime instances array
    StoredInstances.Add(PlacedInstance);

    // Place in grid cells
    if (!PlaceInstanceInCells(PlacedInstance, AnchorIndex))
    {
        // Rollback on failure
        StoredInstances.RemoveAll([&PlacedInstance](const FInventoryItemInstance& Instance)
        {
            return Instance.InstanceID == PlacedInstance.InstanceID;
        });
        return false;
    }

    UE_LOG(LogSuspenseInventory, VeryVerbose, TEXT("PlaceItemInstance: Placed %s at index %d"),
        *ItemInstance.GetShortDebugString(), AnchorIndex);

    return true;
}

bool USuspenseInventoryStorage::MoveItem(const FGuid& InstanceID, int32 NewAnchorIndex, bool bAllowRotation)
{
    if (!bInitialized || !InstanceID.IsValid() || !IsValidIndex(NewAnchorIndex))
    {
        UE_LOG(LogSuspenseInventory, Warning, TEXT("MoveItem: Invalid parameters - Init:%d, ID:%s, Index:%d"),
            bInitialized, *InstanceID.ToString(), NewAnchorIndex);
        return false;
    }

    FInventoryItemInstance* ExistingInstance = FindStoredInstance(InstanceID);
    if (!ExistingInstance)
    {
        UE_LOG(LogSuspenseInventory, Warning, TEXT("MoveItem: Instance not found: %s"), *InstanceID.ToString());
        return false;
    }

    int32 OldAnchorIndex = ExistingInstance->AnchorIndex;
    bool bOldRotation = ExistingInstance->bIsRotated;

    if (OldAnchorIndex == NewAnchorIndex && !bAllowRotation)
    {
        UE_LOG(LogSuspenseInventory, VeryVerbose, TEXT("MoveItem: Already at target position"));
        return true;
    }

    // Create complete backup
    FInventoryItemInstance BackupInstance = *ExistingInstance;
    TArray<FInventoryCell> BackupCells;

    TArray<int32> OccupiedCellsList = GetOccupiedCells(InstanceID);
    for (int32 CellIdx : OccupiedCellsList)
    {
        if (IsValidIndex(CellIdx))
        {
            BackupCells.Add(Cells[CellIdx]);
        }
    }

    FInventoryItemInstance TempInstance = *ExistingInstance;

    BeginTransaction();

    if (!RemoveInstanceFromCells(InstanceID))
    {
        UE_LOG(LogSuspenseInventory, Error, TEXT("MoveItem: Failed to remove from current position"));
        RollbackTransaction();
        return false;
    }

    TempInstance.AnchorIndex = NewAnchorIndex;

    bool bCanPlace = AreCellsFreeForItem(NewAnchorIndex, TempInstance.ItemID, TempInstance.bIsRotated);

    if (!bCanPlace && bAllowRotation)
    {
        TempInstance.bIsRotated = !TempInstance.bIsRotated;
        bCanPlace = AreCellsFreeForItem(NewAnchorIndex, TempInstance.ItemID, TempInstance.bIsRotated);
    }

    if (bCanPlace)
    {
        if (PlaceInstanceInCells(TempInstance, NewAnchorIndex))
        {
            *ExistingInstance = TempInstance;
            UpdateFreeCellsBitmap();
            CommitTransaction();

            UE_LOG(LogSuspenseInventory, Log, TEXT("MoveItem: Successfully moved %s from %d to %d (rotated: %s)"),
                *TempInstance.ItemID.ToString(),
                OldAnchorIndex,
                NewAnchorIndex,
                TempInstance.bIsRotated ? TEXT("Yes") : TEXT("No"));

            return true;
        }
    }

    UE_LOG(LogSuspenseInventory, Warning, TEXT("MoveItem: Failed to place at new position, restoring to original"));

    RollbackTransaction();

    FInventoryItemInstance* RestoredInstance = FindStoredInstance(InstanceID);
    if (!RestoredInstance)
    {
        UE_LOG(LogSuspenseInventory, Error, TEXT("MoveItem: CRITICAL - Item lost during move, recreating from backup"));

        StoredInstances.Add(BackupInstance);

        for (const FInventoryCell& BackupCell : BackupCells)
        {
            if (IsValidIndex(BackupCell.CellIndex))
            {
                Cells[BackupCell.CellIndex] = BackupCell;
            }
        }

        UpdateFreeCellsBitmap();
    }
    else
    {
        if (RestoredInstance->AnchorIndex != OldAnchorIndex)
        {
            UE_LOG(LogSuspenseInventory, Warning, TEXT("MoveItem: Item restored but at wrong position, fixing"));
            RestoredInstance->AnchorIndex = OldAnchorIndex;
            RestoredInstance->bIsRotated = bOldRotation;

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

    const FInventoryCell& Cell = Cells[Index];
    if (!Cell.IsOccupied() || !Cell.OccupyingInstanceID.IsValid())
    {
        return false;
    }

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

    USuspenseItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        return FoundItems;
    }

    for (const FInventoryItemInstance& Instance : StoredInstances)
    {
        FSuspenseUnifiedItemData ItemData;
        if (ItemManager->GetUnifiedItemData(Instance.ItemID, ItemData))
        {
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
    USuspenseItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        return 0.0f;
    }

    for (const FInventoryItemInstance& Instance : StoredInstances)
    {
        FSuspenseUnifiedItemData ItemData;
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

    UE_LOG(LogSuspenseInventory, Log, TEXT("SetMaxWeight: Updated max weight to %.1f"), MaxWeight);
}

bool USuspenseInventoryStorage::HasWeightCapacity(const FName& ItemID, int32 Quantity) const
{
    if (MaxWeight <= 0.0f)
    {
        return true;
    }

    FSuspenseUnifiedItemData ItemData;
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
        UE_LOG(LogSuspenseInventory, Error, TEXT("BeginTransaction: Storage not initialized"));
        return;
    }

    if (ActiveTransaction.bIsActive)
    {
        UE_LOG(LogSuspenseInventory, Warning, TEXT("BeginTransaction: Transaction already active, committing previous"));
        CommitTransaction();
    }

    CreateTransactionSnapshot();

    ActiveTransaction.bIsActive = true;
    ActiveTransaction.StartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    UE_LOG(LogSuspenseInventory, VeryVerbose, TEXT("BeginTransaction: Started new transaction"));
}

void USuspenseInventoryStorage::CommitTransaction()
{
    if (!ActiveTransaction.bIsActive)
    {
        UE_LOG(LogSuspenseInventory, Warning, TEXT("CommitTransaction: No active transaction"));
        return;
    }

    ActiveTransaction = FSuspenseStorageTransaction();

    UE_LOG(LogSuspenseInventory, VeryVerbose, TEXT("CommitTransaction: Transaction committed"));
}

void USuspenseInventoryStorage::RollbackTransaction()
{
    if (!ActiveTransaction.bIsActive)
    {
        UE_LOG(LogSuspenseInventory, Warning, TEXT("RollbackTransaction: No active transaction"));
        return;
    }

    RestoreFromTransactionSnapshot();

    ActiveTransaction = FSuspenseStorageTransaction();

    UE_LOG(LogSuspenseInventory, Log, TEXT("RollbackTransaction: Transaction rolled back"));
}

bool USuspenseInventoryStorage::IsTransactionActive() const
{
    if (!ActiveTransaction.bIsActive)
    {
        return false;
    }

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

    UE_LOG(LogSuspenseInventory, Log, TEXT("ClearAllItems: Clearing %d items from storage"), StoredInstances.Num());

    for (FInventoryCell& Cell : Cells)
    {
        Cell.Clear();
    }

    FreeCellsBitmap.Init(true, GridWidth * GridHeight);

    StoredInstances.Empty();

    ActiveTransaction = FSuspenseStorageTransaction();

    UE_LOG(LogSuspenseInventory, Log, TEXT("ClearAllItems: Storage cleared"));
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

    TSet<FGuid> CellInstanceIDs;
    TSet<FGuid> StoredInstanceIDs;

    for (const FInventoryCell& Cell : Cells)
    {
        if (Cell.IsOccupied() && Cell.OccupyingInstanceID.IsValid())
        {
            CellInstanceIDs.Add(Cell.OccupyingInstanceID);
        }
    }

    for (const FInventoryItemInstance& Instance : StoredInstances)
    {
        StoredInstanceIDs.Add(Instance.InstanceID);

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

    for (const FGuid& CellInstanceID : CellInstanceIDs)
    {
        if (!StoredInstanceIDs.Contains(CellInstanceID))
        {
            OutErrors.Add(FString::Printf(TEXT("Orphaned cell references instance %s"),
                *CellInstanceID.ToString()));
            bIsValid = false;

            if (bAutoFix)
            {
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

    if (bAutoFix && !bIsValid)
    {
        UpdateFreeCellsBitmap();
        UE_LOG(LogSuspenseInventory, Log, TEXT("ValidateStorageIntegrity: Auto-fixed bitmap inconsistencies"));
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

    UE_LOG(LogSuspenseInventory, Log, TEXT("DefragmentStorage: Starting defragmentation"));

    BeginTransaction();

    TArray<FInventoryItemInstance> AllInstances = StoredInstances;

    ClearAllItems();

    AllInstances.Sort([this](const FInventoryItemInstance& A, const FInventoryItemInstance& B)
    {
        FSuspenseUnifiedItemData DataA, DataB;
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

    int32 MovedCount = 0;
    for (const FInventoryItemInstance& Instance : AllInstances)
    {
        if (AddItemInstance(Instance, true))
        {
            MovedCount++;
        }
        else
        {
            UE_LOG(LogSuspenseInventory, Warning, TEXT("DefragmentStorage: Failed to place item %s during defragmentation"),
                *Instance.ItemID.ToString());
        }
    }

    CommitTransaction();

    UE_LOG(LogSuspenseInventory, Log, TEXT("DefragmentStorage: Successfully moved %d items"), MovedCount);
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

    for (int32 i = 0; i < Cells.Num() && i < FreeCellsBitmap.Num(); ++i)
    {
        FreeCellsBitmap[i] = !Cells[i].IsOccupied();
    }
}

USuspenseItemManager* USuspenseInventoryStorage::GetItemManager() const
{
    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GameInstance = World->GetGameInstance())
        {
            return GameInstance->GetSubsystem<USuspenseItemManager>();
        }
    }

    return nullptr;
}

bool USuspenseInventoryStorage::GetItemData(const FName& ItemID, FSuspenseUnifiedItemData& OutData) const
{
    USuspenseItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        return false;
    }
    return ItemManager->GetUnifiedItemData(ItemID, OutData);
}

bool USuspenseInventoryStorage::PlaceInstanceInCells(const FInventoryItemInstance& ItemInstance, int32 AnchorIndex)
{
    if (!bInitialized || !ItemInstance.IsValid() || !IsValidIndex(AnchorIndex))
    {
        UE_LOG(LogSuspenseInventory, Error, TEXT("PlaceInstanceInCells: Invalid parameters"));
        return false;
    }

    FVector2D ItemSize(1, 1);

    FSuspenseUnifiedItemData ItemData;
    if (GetItemData(ItemInstance.ItemID, ItemData))
    {
        ItemSize = FVector2D(ItemData.GridSize.X, ItemData.GridSize.Y);

        UE_LOG(LogSuspenseInventory, Log, TEXT("PlaceInstanceInCells: Got size from ItemManager for %s: %.0fx%.0f"),
            *ItemInstance.ItemID.ToString(), ItemSize.X, ItemSize.Y);
    }
    else
    {
        UE_LOG(LogSuspenseInventory, Warning, TEXT("PlaceInstanceInCells: Failed to get item data for %s, using default size"),
            *ItemInstance.ItemID.ToString());
    }

    if (ItemInstance.bIsRotated)
    {
        ItemSize = FVector2D(ItemSize.Y, ItemSize.X);
    }

    int32 ItemWidth = FMath::CeilToInt(ItemSize.X);
    int32 ItemHeight = FMath::CeilToInt(ItemSize.Y);

    UE_LOG(LogSuspenseInventory, Log, TEXT("PlaceInstanceInCells: Placing %s at index %d, final size %dx%d (rotated: %s)"),
        *ItemInstance.ItemID.ToString(),
        AnchorIndex,
        ItemWidth,
        ItemHeight,
        ItemInstance.bIsRotated ? TEXT("Yes") : TEXT("No"));

    int32 AnchorX, AnchorY;
    if (!GetGridCoordinates(AnchorIndex, AnchorX, AnchorY))
    {
        UE_LOG(LogSuspenseInventory, Error, TEXT("PlaceInstanceInCells: Invalid anchor index %d"), AnchorIndex);
        return false;
    }

    if (AnchorX + ItemWidth > GridWidth || AnchorY + ItemHeight > GridHeight)
    {
        UE_LOG(LogSuspenseInventory, Warning, TEXT("PlaceInstanceInCells: Item would exceed grid bounds - Position: (%d,%d), Size: %dx%d, Grid: %dx%d"),
            AnchorX, AnchorY, ItemWidth, ItemHeight, GridWidth, GridHeight);
        return false;
    }

    TArray<int32> CellsToOccupy;
    CellsToOccupy.Reserve(ItemWidth * ItemHeight);

    for (int32 Y = 0; Y < ItemHeight; ++Y)
    {
        for (int32 X = 0; X < ItemWidth; ++X)
        {
            int32 CellIndex = (AnchorY + Y) * GridWidth + (AnchorX + X);

            if (!IsValidIndex(CellIndex))
            {
                UE_LOG(LogSuspenseInventory, Error, TEXT("PlaceInstanceInCells: Invalid cell index %d"), CellIndex);
                return false;
            }

            if (!FreeCellsBitmap[CellIndex])
            {
                UE_LOG(LogSuspenseInventory, Warning, TEXT("PlaceInstanceInCells: Cell %d already occupied"), CellIndex);
                return false;
            }

            if (Cells[CellIndex].IsOccupied())
            {
                UE_LOG(LogSuspenseInventory, Error, TEXT("PlaceInstanceInCells: Cell %d occupied but bitmap says free - integrity error!"), CellIndex);
                return false;
            }

            CellsToOccupy.Add(CellIndex);
        }
    }

    for (int32 CellIndex : CellsToOccupy)
    {
        Cells[CellIndex].bIsOccupied = true;
        Cells[CellIndex].OccupyingInstanceID = ItemInstance.InstanceID;

        FreeCellsBitmap[CellIndex] = false;
    }

    UE_LOG(LogSuspenseInventory, Log, TEXT("PlaceInstanceInCells: Successfully occupied %d cells for item %s (ID: %s)"),
        CellsToOccupy.Num(),
        *ItemInstance.ItemID.ToString(),
        *ItemInstance.InstanceID.ToString());

    return true;
}

bool USuspenseInventoryStorage::RemoveInstanceFromCells(const FGuid& InstanceID)
{
    bool bRemovedAny = false;

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

    if (ItemWidth > GridWidth || ItemHeight > GridHeight)
    {
        return INDEX_NONE;
    }

    int32 BestIndex = INDEX_NONE;
    int32 BestScore = INT_MAX;

    for (int32 Y = 0; Y <= GridHeight - ItemHeight; ++Y)
    {
        for (int32 X = 0; X <= GridWidth - ItemWidth; ++X)
        {
            int32 StartIndex = Y * GridWidth + X;

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
                    return StartIndex;
                }

                int32 Score = X + Y;

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
