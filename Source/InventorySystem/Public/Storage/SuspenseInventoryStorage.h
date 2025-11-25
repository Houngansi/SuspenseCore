// SuspenseInventory/Storage/SuspenseInventoryStorage.h
// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/Inventory/InventoryTypes.h"
#include "Base/InventoryLogs.h"
#include "GameplayTagContainer.h"
#include "SuspenseInventoryStorage.generated.h"

// Forward declarations for clean architecture
class USuspenseItemManager;
struct FSuspenseUnifiedItemData;
struct FInventoryItemInstance;
class UEventDelegateManager;

/**
 * Structure describing inventory change transaction
 * Provides atomic operations and rollback capability on errors
 */
USTRUCT()
struct FSuspenseInventoryTransaction
{
    GENERATED_BODY()

    /** Snapshot of cells state before change */
    UPROPERTY()
    TArray<FInventoryCell> CellsSnapshot;

    /** Snapshot of runtime instances before change */
    UPROPERTY()
    TArray<FInventoryItemInstance> InstancesSnapshot;

    /** Is transaction active */
    UPROPERTY()
    bool bIsActive = false;

    /** Transaction start time for timeout protection */
    UPROPERTY()
    float StartTime = 0.0f;

    FSuspenseInventoryTransaction()
    {
        bIsActive = false;
        StartTime = 0.0f;
    }

    /** Check transaction validity */
    bool IsValid(float CurrentTime, float TimeoutSeconds = 30.0f) const
    {
        return bIsActive && (CurrentTime - StartTime) < TimeoutSeconds;
    }
};

/**
 * Component responsible for storing inventory items in a grid-based structure
 *
 * NEW ARCHITECTURE:
 * - Fully integrated with ItemManager and DataTable
 * - Works exclusively with FGuid and FSuspenseUnifiedItemData
 * - Supports FInventoryItemInstance for runtime data
 * - Provides atomic operations through transaction system
 * - Integrated with event system for UI updates
 */
UCLASS(ClassGroup=(Suspense), meta=(BlueprintSpawnableComponent))
class SUSPENSEINVENTORY_API USuspenseInventoryStorage : public UActorComponent
{
    GENERATED_BODY()

public:
    //==================================================================
    // Constructor and Lifecycle
    //==================================================================

    USuspenseInventoryStorage();

    /** Component initialization */
    virtual void BeginPlay() override;

    /** Component cleanup */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    //==================================================================
    // Core Storage Management
    //==================================================================

    /**
     * Initializes storage grid with specified dimensions
     * Clears all existing data and creates clean grid
     *
     * @param Width Grid width in cells
     * @param Height Grid height in cells
     * @param MaxWeight Maximum weight for storage (0 = no limit)
     * @return true if initialization was successful
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Storage")
    bool InitializeGrid(int32 Width, int32 Height, float MaxWeight = 0.0f);

    /**
     * Checks if storage is initialized
     * @return true if storage is ready for use
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Storage")
    bool IsInitialized() const { return bInitialized; }

    /**
     * Gets inventory grid dimensions
     * @return 2D vector containing width and height
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Storage")
    FVector2D GetGridSize() const { return FVector2D(GridWidth, GridHeight); }

    /**
     * Gets total number of cells in grid
     * @return Total cell count
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Storage")
    int32 GetTotalCells() const { return GridWidth * GridHeight; }

    /**
     * Gets number of free cells
     * @return Number of unoccupied cells
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Storage")
    int32 GetFreeCellCount() const;

    //==================================================================
    // Item Instance Management
    //==================================================================

    /**
     * Adds runtime item instance to storage
     * Automatically finds suitable location and places item
     *
     * @param ItemInstance Runtime instance to place
     * @param bAllowRotation Allow rotation for optimal placement
     * @return true if item was successfully placed
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Items")
    bool AddItemInstance(const FInventoryItemInstance& ItemInstance, bool bAllowRotation = true);

    /**
     * Removes runtime item instance from storage
     *
     * @param InstanceID Unique ID of instance to remove
     * @return true if item was found and removed
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Items")
    bool RemoveItemInstance(const FGuid& InstanceID);

    /**
     * Gets runtime item instance by its ID
     *
     * @param InstanceID Unique instance ID
     * @param OutInstance Output runtime instance
     * @return true if instance was found
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Items")
    bool GetItemInstance(const FGuid& InstanceID, FInventoryItemInstance& OutInstance) const;

    /**
     * Gets all runtime instances in storage
     * @return Array of all runtime instances
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Items")
    TArray<FInventoryItemInstance> GetAllItemInstances() const;

    /**
     * Updates runtime item instance
     * Useful for changing quantity, runtime properties, etc.
     *
     * @param UpdatedInstance Updated runtime instance
     * @return true if update was successful
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Items")
    bool UpdateItemInstance(const FInventoryItemInstance& UpdatedInstance);

    //==================================================================
    // Space Management and Placement
    //==================================================================

    /**
     * Finds free space for item of specified size
     * Uses intelligent algorithms for optimal placement
     *
     * @param ItemID Item ID from DataTable (for getting size)
     * @param bAllowRotation Allow rotation for better placement
     * @param bOptimizeFragmentation Minimize fragmentation
     * @return Anchor cell index or INDEX_NONE if no space
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Placement")
    int32 FindFreeSpace(const FName& ItemID, bool bAllowRotation = true, bool bOptimizeFragmentation = true) const;

    /**
     * Checks if cells are free for item placement
     *
     * @param StartIndex Starting anchor cell index
     * @param ItemID Item ID for getting size from DataTable
     * @param bIsRotated Is item rotated
     * @return true if all required cells are free
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Placement")
    bool AreCellsFreeForItem(int32 StartIndex, const FName& ItemID, bool bIsRotated = false) const;

    /**
     * Places runtime instance at specified position
     *
     * @param ItemInstance Runtime instance to place
     * @param AnchorIndex Anchor cell index
     * @return true if placement was successful
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Placement")
    bool PlaceItemInstance(const FInventoryItemInstance& ItemInstance, int32 AnchorIndex);

    /**
     * Moves item from one position to another
     * Supports atomic operation with rollback on failure
     *
     * @param InstanceID ID of instance to move
     * @param NewAnchorIndex New anchor cell position
     * @param bAllowRotation Allow rotation change
     * @return true if move was successful
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Placement")
    bool MoveItem(const FGuid& InstanceID, int32 NewAnchorIndex, bool bAllowRotation = false);

    //==================================================================
    // Item Queries and Access
    //==================================================================

    /**
     * Gets runtime instance at specified cell
     * @param Index Cell index
     * @param OutInstance Output runtime instance
     * @return true if cell contains an item
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Query")
    bool GetItemInstanceAt(int32 Index, FInventoryItemInstance& OutInstance) const;

    /**
     * Counts total items by ID
     * @param ItemID Item ID to count
     * @return Total quantity of items of this type
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Query")
    int32 GetItemCountByID(const FName& ItemID) const;

    /**
     * Finds items by type from DataTable
     * @param ItemType Item type to search for
     * @return Array of found runtime instances
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Query")
    TArray<FInventoryItemInstance> FindItemsByType(const FGameplayTag& ItemType) const;

    //==================================================================
    // Grid Coordinate Utilities
    //==================================================================

    /**
     * Converts linear index to grid coordinates
     * @param Index Linear index
     * @param OutX Output X coordinate
     * @param OutY Output Y coordinate
     * @return true if conversion was successful
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Utilities")
    bool GetGridCoordinates(int32 Index, int32& OutX, int32& OutY) const;

    /**
     * Converts grid coordinates to linear index
     * @param X X coordinate
     * @param Y Y coordinate
     * @param OutIndex Output linear index
     * @return true if coordinates are valid
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Utilities")
    bool GetLinearIndex(int32 X, int32 Y, int32& OutIndex) const;

    /**
     * Gets all occupied cells for an item
     * @param InstanceID Item instance ID
     * @return Array of occupied cell indices
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Utilities")
    TArray<int32> GetOccupiedCells(const FGuid& InstanceID) const;

    //==================================================================
    // Weight Management
    //==================================================================

    /**
     * Gets current total weight in storage
     * Calculated based on items and their DataTable data
     * @return Current weight in game units
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Weight")
    float GetCurrentWeight() const;

    /**
     * Gets maximum allowed weight
     * @return Maximum weight or 0 if no limit
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Weight")
    float GetMaxWeight() const { return MaxWeight; }

    /**
     * Sets maximum weight
     * @param NewMaxWeight New weight limit
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Weight")
    void SetMaxWeight(float NewMaxWeight);

    /**
     * Checks if there's enough weight capacity for an item
     * @param ItemID Item ID to check
     * @param Quantity Item quantity
     * @return true if there's enough weight capacity
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Weight")
    bool HasWeightCapacity(const FName& ItemID, int32 Quantity = 1) const;

    //==================================================================
    // Transaction Support
    //==================================================================

    /**
     * Begins atomic transaction for multiple operations
     * Creates snapshot of current state for possible rollback
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Transactions")
    void BeginTransaction();

    /**
     * Commits all changes in current transaction
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Transactions")
    void CommitTransaction();

    /**
     * Rolls back all changes in current transaction
     * Restores state from transaction start
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Transactions")
    void RollbackTransaction();

    /**
     * Checks if transaction is active
     * @return true if transaction is active
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Transactions")
    bool IsTransactionActive() const;

    //==================================================================
    // Maintenance and Utilities
    //==================================================================

    /**
     * Clears all items from storage
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Maintenance")
    void ClearAllItems();

    /**
     * Validates storage integrity
     * Checks consistency between cells and runtime instances
     * @param OutErrors Array of found errors
     * @param bAutoFix Automatically fix found issues
     * @return true if storage is in valid state
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    bool ValidateStorageIntegrity(TArray<FString>& OutErrors, bool bAutoFix = false);

    /**
     * Gets detailed debug information about storage
     * @return Detailed string with storage state
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug")
    FString GetStorageDebugInfo() const;

    /**
     * Defragments storage for optimal space usage
     * Moves items to reduce fragmentation
     * @return Number of moved items
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Optimization")
    int32 DefragmentStorage();

protected:
    //==================================================================
    // Storage State
    //==================================================================

    /** Grid width in cells */
    UPROPERTY(VisibleAnywhere, Category = "Inventory|State")
    int32 GridWidth = 0;

    /** Grid height in cells */
    UPROPERTY(VisibleAnywhere, Category = "Inventory|State")
    int32 GridHeight = 0;

    /** Maximum weight for storage (0 = no limit) */
    UPROPERTY(EditAnywhere, Category = "Inventory|Configuration")
    float MaxWeight = 0.0f;

    /** Initialization state */
    UPROPERTY(VisibleAnywhere, Category = "Inventory|State")
    bool bInitialized = false;

    /** Grid cells with placement information */
    UPROPERTY()
    TArray<FInventoryCell> Cells;

    /** Runtime item instances in storage */
    UPROPERTY()
    TArray<FInventoryItemInstance> StoredInstances;

    /** Bitmap for fast free cell lookup */
    TBitArray<> FreeCellsBitmap;

    /** Current transaction for atomic operations */
    UPROPERTY()
    FSuspenseInventoryTransaction ActiveTransaction;

    //==================================================================
    // Internal Helper Methods
    //==================================================================

    /**
     * Validates index against grid bounds
     * @param Index Index to check
     * @return true if index is valid
     */
    bool IsValidIndex(int32 Index) const;

    /**
     * Recalculates free cells bitmap
     */
    void UpdateFreeCellsBitmap();

    /**
     * Gets ItemManager for DataTable access
     * @return ItemManager or nullptr if unavailable
     */
    USuspenseItemManager* GetItemManager() const;

    /**
     * Gets unified item data from DataTable
     * @param ItemID Item ID
     * @param OutData Output data
     * @return true if data was retrieved
     */
    bool GetItemData(const FName& ItemID, FSuspenseUnifiedItemData& OutData) const;

    /**
     * Places runtime instance in grid cells
     * @param ItemInstance Instance to place
     * @param AnchorIndex Anchor cell
     * @return true if placement was successful
     */
    bool PlaceInstanceInCells(const FInventoryItemInstance& ItemInstance, int32 AnchorIndex);

    /**
     * Removes runtime instance from grid cells
     * @param InstanceID ID of instance to remove
     * @return true if removal was successful
     */
    bool RemoveInstanceFromCells(const FGuid& InstanceID);

    /**
     * Finds runtime instance by ID
     * @param InstanceID ID to search for
     * @return Pointer to instance or nullptr
     */
    FInventoryItemInstance* FindStoredInstance(const FGuid& InstanceID);

    /**
     * Finds runtime instance by ID (const version)
     */
    const FInventoryItemInstance* FindStoredInstance(const FGuid& InstanceID) const;

    /**
     * Creates snapshot of current state for transaction
     */
    void CreateTransactionSnapshot();

    /**
     * Restores state from transaction snapshot
     */
    void RestoreFromTransactionSnapshot();

    /**
     * Calculates optimal placement coordinates for item
     * @param ItemSize Item size
     * @param bOptimizeFragmentation Minimize fragmentation
     * @return Optimal position index or INDEX_NONE
     */
    int32 FindOptimalPlacement(const FVector2D& ItemSize, bool bOptimizeFragmentation = true) const;

    /**
     * Checks if cells are free for specified size
     * @param StartIndex Starting index
     * @param Size Size of area to check
     * @return true if all cells in area are free
     */
    bool AreCellsFree(int32 StartIndex, const FVector2D& Size) const;
};
