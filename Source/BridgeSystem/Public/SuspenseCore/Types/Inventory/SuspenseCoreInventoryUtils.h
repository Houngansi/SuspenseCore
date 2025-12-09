// SuspenseCoreInventoryUtils.h
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#pragma once


#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "BridgeSystem.h"

// Forward declarations to avoid circular dependencies
struct FSuspenseCoreInventoryItemInstance;
struct FSuspenseCoreUnifiedItemData;

/**
 * Utility namespace for inventory operations and DataTable integration
 *
 * ARCHITECTURAL PRINCIPLES FOR MMO FPS:
 * - All functions accept WorldContextObject as first parameter (explicit dependency)
 * - Static data comes ONLY from DataTable through ItemManager (single source of truth)
 * - Functions fail gracefully with detailed logging if ItemManager unavailable
 * - No duplicate data storage - always query ItemManager for authoritative data
 *
 * CRITICAL FOR MMO:
 * Server and all clients load identical DataTable at startup. ItemManager on each
 * side provides identical data. This ensures server-side validation matches
 * client-side prediction, preventing desyncs and exploits.
 *
 * IMPLEMENTATION NOTES:
 * - Implementation is located in InventoryTypes.cpp
 * - All functions are exported with BRIDGESYSTEM_API for cross-module usage
 * - Runtime properties are initialized based on item type from DataTable
 *
 * PERFORMANCE:
 * For hot paths called every frame, consider caching ItemManager at component level.
 * Most functions here are cheap (pointer lookups in TMap) but can be optimized further.
 */
namespace InventoryUtils
{
    /**
     * Get unified item data from DataTable through ItemManager
     * This is the primary function for accessing static item data
     *
     * CHANGED: Now requires WorldContextObject for ItemManager access
     *
     * @param WorldContextObject - Valid UObject with World context (Actor, Component, etc)
     * @param ItemID - ID of the item to look up in DataTable
     * @param OutData - Output structure with item data
     * @return true if data was found and loaded successfully
     */
    BRIDGESYSTEM_API bool GetUnifiedItemData(
        const UObject* WorldContextObject,
        const FName& ItemID,
        FSuspenseCoreUnifiedItemData& OutData);

    /**
     * Create properly initialized item instance
     * This is the main way to create new item instances with all runtime properties set
     *
     * The function automatically initializes:
     * - Durability for equippable items
     * - Ammo for weapons
     * - Charges for consumables
     * - Other runtime properties based on item type
     *
     * CHANGED: Now requires WorldContextObject for ItemManager access
     *
     * @param WorldContextObject - Valid UObject with World context
     * @param ItemID - ID of the item from DataTable
     * @param Quantity - Number of items in the stack (default: 1)
     * @return Fully initialized item instance with runtime properties
     */
    BRIDGESYSTEM_API FSuspenseCoreInventoryItemInstance CreateItemInstance(
        const UObject* WorldContextObject,
        const FName& ItemID,
        int32 Quantity = 1);

    /**
     * Get item grid size with rotation applied
     * Reads size from DataTable and applies rotation if needed
     *
     * Rotation swaps width and height dimensions in the inventory grid
     *
     * CHANGED: Now requires WorldContextObject for ItemManager access
     *
     * @param WorldContextObject - Valid UObject with World context
     * @param ItemID - ID of the item
     * @param bIsRotated - Is the item rotated 90 degrees (default: false)
     * @return Item size in grid cells (width, height)
     */
    BRIDGESYSTEM_API FVector2D GetItemGridSize(
        const UObject* WorldContextObject,
        const FName& ItemID,
        bool bIsRotated = false);

    /**
     * Check if item can be placed at specific position
     * Takes grid dimensions as parameters for flexibility
     *
     * Validates:
     * - Anchor index is within grid bounds
     * - Item doesn't extend beyond grid edges
     * - Position calculations are correct
     *
     * CHANGED: Now requires WorldContextObject for item size lookup
     *
     * @param WorldContextObject - Valid UObject with World context
     * @param Item - Item instance to place
     * @param AnchorIndex - Anchor cell index (top-left corner)
     * @param GridWidth - Width of the inventory grid
     * @param GridHeight - Height of the inventory grid
     * @return true if item can be placed at the position without extending beyond bounds
     */
    BRIDGESYSTEM_API bool CanPlaceItemAt(
        const UObject* WorldContextObject,
        const FSuspenseCoreInventoryItemInstance& Item,
        int32 AnchorIndex,
        int32 GridWidth,
        int32 GridHeight);

    /**
     * Get all cell indices occupied by item
     * Used for collision checking and grid state updates
     *
     * Returns empty array if item is not placed (AnchorIndex == INDEX_NONE)
     *
     * CHANGED: Now requires WorldContextObject for item size lookup
     *
     * @param WorldContextObject - Valid UObject with World context
     * @param Item - Item instance (must have valid AnchorIndex)
     * @param GridWidth - Width of the inventory grid
     * @return Array of all occupied cell indices in linear format
     */
    BRIDGESYSTEM_API TArray<int32> GetOccupiedCellIndices(
        const UObject* WorldContextObject,
        const FSuspenseCoreInventoryItemInstance& Item,
        int32 GridWidth);

    /**
     * Check if items can be stacked together
     * Items must have same ItemID and be stackable in DataTable
     *
     * Future improvements may check:
     * - Same durability for equipment
     * - Same modifications for weapons
     * - Expiration dates for consumables
     *
     * CHANGED: Now requires WorldContextObject for DataTable access
     *
     * @param WorldContextObject - Valid UObject with World context
     * @param Item1 - First item instance
     * @param Item2 - Second item instance
     * @return true if items can be combined into one stack
     */
    BRIDGESYSTEM_API bool CanStackItems(
        const UObject* WorldContextObject,
        const FSuspenseCoreInventoryItemInstance& Item1,
        const FSuspenseCoreInventoryItemInstance& Item2);

    /**
     * Get maximum stack size for item from DataTable
     *
     * CHANGED: Now requires WorldContextObject for ItemManager access
     *
     * @param WorldContextObject - Valid UObject with World context
     * @param ItemID - ID of the item
     * @return Maximum stack size (1 = not stackable)
     */
    BRIDGESYSTEM_API int32 GetMaxStackSize(
        const UObject* WorldContextObject,
        const FName& ItemID);

    /**
     * Get weight of single item from DataTable
     *
     * CHANGED: Now requires WorldContextObject for ItemManager access
     *
     * @param WorldContextObject - Valid UObject with World context
     * @param ItemID - ID of the item
     * @return Weight of one item instance in game units
     */
    BRIDGESYSTEM_API float GetItemWeight(
        const UObject* WorldContextObject,
        const FName& ItemID);

    /**
     * Calculate total weight of item instance including stack size
     *
     * CHANGED: Now requires WorldContextObject for item weight lookup
     *
     * @param WorldContextObject - Valid UObject with World context
     * @param Instance - Item instance
     * @return Total weight of the entire stack (weight * quantity)
     */
    BRIDGESYSTEM_API float CalculateInstanceWeight(
        const UObject* WorldContextObject,
        const FSuspenseCoreInventoryItemInstance& Instance);

    /**
     * Check if item is allowed in inventory based on type filters
     *
     * Blacklist (DisallowedTypes) has priority over whitelist (AllowedTypes)
     * Empty whitelist means all types are allowed (except blacklisted)
     *
     * CHANGED: Now requires WorldContextObject for item type lookup
     *
     * @param WorldContextObject - Valid UObject with World context
     * @param ItemID - ID of the item to check
     * @param AllowedTypes - Allowed item types (empty = all allowed)
     * @param DisallowedTypes - Disallowed item types (priority over allowed)
     * @return true if item can be placed in this inventory
     */
    BRIDGESYSTEM_API bool IsItemAllowedInInventory(
        const UObject* WorldContextObject,
        const FName& ItemID,
        const FGameplayTagContainer& AllowedTypes,
        const FGameplayTagContainer& DisallowedTypes);
}


