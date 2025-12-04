// SuspenseCoreInventoryStorage.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Containers/BitArray.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "SuspenseCoreInventoryStorage.generated.h"

/**
 * USuspenseCoreInventoryStorage
 *
 * Grid-based inventory storage manager.
 * Handles spatial placement of items in a 2D grid.
 *
 * ARCHITECTURE:
 * - Manages grid slots with multi-cell item support
 * - Uses anchor cells with offsets for large items
 * - Supports item rotation (90 degree increments)
 * - Free slot bitmap for fast space queries
 * - Fragmentation detection and defragmentation
 *
 * GRID LAYOUT:
 * - Linear indexing: index = y * GridWidth + x
 * - Anchor cell is top-left of multi-cell items
 * - Other cells reference anchor via offset
 *
 * THREAD SAFETY:
 * - GameThread only (checked in operations)
 */
UCLASS()
class INVENTORYSYSTEM_API USuspenseCoreInventoryStorage : public UObject
{
	GENERATED_BODY()

public:
	USuspenseCoreInventoryStorage();

	//==================================================================
	// Initialization
	//==================================================================

	/**
	 * Initialize storage grid.
	 * @param InGridWidth Grid width (columns)
	 * @param InGridHeight Grid height (rows)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Storage")
	void Initialize(int32 InGridWidth, int32 InGridHeight);

	/**
	 * Check if initialized.
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Storage")
	bool IsInitialized() const { return bIsInitialized; }

	/**
	 * Clear all slots.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Storage")
	void Clear();

	//==================================================================
	// Grid Properties
	//==================================================================

	/** Get grid width */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Storage")
	int32 GetGridWidth() const { return GridWidth; }

	/** Get grid height */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Storage")
	int32 GetGridHeight() const { return GridHeight; }

	/** Get grid size as FIntPoint */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Storage")
	FIntPoint GetGridSize() const { return FIntPoint(GridWidth, GridHeight); }

	/** Get total slot count */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Storage")
	int32 GetTotalSlots() const { return GridWidth * GridHeight; }

	//==================================================================
	// Slot Operations
	//==================================================================

	/**
	 * Check if slot is occupied.
	 * @param SlotIndex Slot to check
	 * @return true if slot has item
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Storage")
	bool IsSlotOccupied(int32 SlotIndex) const;

	/**
	 * Check if slot is valid.
	 * @param SlotIndex Slot to check
	 * @return true if within grid bounds
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Storage")
	bool IsValidSlot(int32 SlotIndex) const;

	/**
	 * Get slot data.
	 * @param SlotIndex Slot to query
	 * @return Slot data (empty struct if invalid)
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Storage")
	FSuspenseCoreInventorySlot GetSlot(int32 SlotIndex) const;

	/**
	 * Get instance ID at slot.
	 * @param SlotIndex Slot to query
	 * @return Instance ID or invalid GUID if empty
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Storage")
	FGuid GetInstanceIDAtSlot(int32 SlotIndex) const;

	/**
	 * Get anchor slot for item at given slot.
	 * @param SlotIndex Any slot occupied by item
	 * @return Anchor slot index or INDEX_NONE
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Storage")
	int32 GetAnchorSlot(int32 SlotIndex) const;

	//==================================================================
	// Placement Operations
	//==================================================================

	/**
	 * Check if item can be placed at slot.
	 * @param ItemSize Item grid size
	 * @param SlotIndex Target anchor slot
	 * @param bRotated Is item rotated 90 degrees
	 * @param IgnoreInstanceID Instance to ignore (for moves)
	 * @return true if placement is valid
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Storage")
	bool CanPlaceItem(FIntPoint ItemSize, int32 SlotIndex, bool bRotated = false, FGuid IgnoreInstanceID = FGuid()) const;

	/**
	 * Place item in grid.
	 * @param InstanceID Item instance ID
	 * @param ItemSize Item grid size
	 * @param SlotIndex Anchor slot
	 * @param bRotated Is item rotated
	 * @return true if placed successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Storage")
	bool PlaceItem(FGuid InstanceID, FIntPoint ItemSize, int32 SlotIndex, bool bRotated = false);

	/**
	 * Remove item from grid.
	 * @param InstanceID Item to remove
	 * @return true if removed
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Storage")
	bool RemoveItem(FGuid InstanceID);

	/**
	 * Move item to new slot.
	 * @param InstanceID Item to move
	 * @param ItemSize Item grid size
	 * @param NewSlotIndex Target slot
	 * @param bRotated New rotation state
	 * @return true if moved
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Storage")
	bool MoveItem(FGuid InstanceID, FIntPoint ItemSize, int32 NewSlotIndex, bool bRotated = false);

	//==================================================================
	// Free Space Queries
	//==================================================================

	/**
	 * Find free slot for item.
	 * @param ItemSize Item grid size
	 * @param bAllowRotation Try rotation if needed
	 * @param OutRotated Was rotation needed
	 * @return Slot index or INDEX_NONE
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Storage")
	int32 FindFreeSlot(FIntPoint ItemSize, bool bAllowRotation, bool& OutRotated) const;

	/**
	 * Get free slot count.
	 * @return Number of unoccupied slots
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Storage")
	int32 GetFreeSlotCount() const;

	/**
	 * Get fragmentation ratio.
	 * Higher = more fragmented.
	 * @return Fragmentation ratio (0-1)
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Storage")
	float GetFragmentationRatio() const;

	//==================================================================
	// Coordinate Conversion
	//==================================================================

	/** Convert slot index to grid coordinates */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Storage")
	FIntPoint SlotToCoords(int32 SlotIndex) const;

	/** Convert grid coordinates to slot index */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Storage")
	int32 CoordsToSlot(FIntPoint Coords) const;

	/** Check if coordinates are valid */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Storage")
	bool IsValidCoords(FIntPoint Coords) const;

	//==================================================================
	// Occupied Slots Query
	//==================================================================

	/**
	 * Get all slots occupied by an item.
	 * @param InstanceID Item to query
	 * @return Array of occupied slot indices
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Storage")
	TArray<int32> GetOccupiedSlots(FGuid InstanceID) const;

	/**
	 * Get all anchor slots (one per item).
	 * @return Array of anchor slot indices
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Storage")
	TArray<int32> GetAllAnchorSlots() const;

	//==================================================================
	// Debug
	//==================================================================

	/** Get debug visualization string */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Storage|Debug")
	FString GetDebugGridString() const;

	/** Log grid state */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Storage|Debug")
	void LogGridState() const;

private:
	/** Grid width */
	UPROPERTY()
	int32 GridWidth;

	/** Grid height */
	UPROPERTY()
	int32 GridHeight;

	/** Grid slots */
	UPROPERTY()
	TArray<FSuspenseCoreInventorySlot> Slots;

	/** Free slot bitmap for fast queries (not UPROPERTY - TBitArray not supported) */
	TBitArray<> FreeSlotBitmap;

	/** Is initialized */
	UPROPERTY()
	bool bIsInitialized;

	/** Get effective size considering rotation */
	FIntPoint GetEffectiveSize(FIntPoint ItemSize, bool bRotated) const;

	/** Get all slots that would be occupied by item */
	TArray<int32> CalculateOccupiedSlots(int32 AnchorSlot, FIntPoint ItemSize, bool bRotated) const;

	/** Update free slot bitmap */
	void UpdateFreeBitmap();
};
