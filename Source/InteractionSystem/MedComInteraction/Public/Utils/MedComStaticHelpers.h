// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameFramework/PlayerState.h"
#include "Types/Inventory/InventoryTypes.h"
#include "MedComStaticHelpers.generated.h"

// Forward declarations
class UMedComItemManager;
class UEventDelegateManager;
struct FMedComUnifiedItemData;
struct FInventoryItemInstance;

// Log categories definition
DECLARE_LOG_CATEGORY_EXTERN(LogMedComInteraction, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogInventoryStatistics, Log, All);

/**
 * Static helper library for MedCom interaction and inventory systems
 * Provides utility functions for item management in new DataTable architecture
 * 
 * Key principles:
 * - Works exclusively with ItemID references to DataTable
 * - No data duplication or legacy structures
 * - Thread-safe operations for multiplayer
 * - Centralized access to subsystems
 */
UCLASS()
class MEDCOMINTERACTION_API UMedComStaticHelpers : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
    
public:
    //==================================================================
    // Component Discovery
    //==================================================================
    
    /**
     * Find inventory component on specified actor
     * Searches in PlayerState first, then Actor, then Controller
     * @param Actor Actor to search
     * @return Inventory component or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "MedCom|Interaction")
    static UObject* FindInventoryComponent(AActor* Actor);
    
    /**
     * Find PlayerState for specified actor
     * Handles Pawn, Controller, and direct PlayerState cases
     * @param Actor Actor to search
     * @return PlayerState or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "MedCom|Interaction")
    static APlayerState* FindPlayerState(AActor* Actor);
    
    /**
     * Check if object implements inventory interface
     * @param Object Object to check
     * @return true if object implements IMedComInventoryInterface
     */
    UFUNCTION(BlueprintPure, Category = "MedCom|Interaction")
    static bool ImplementsInventoryInterface(UObject* Object);
    
    //==================================================================
    // Item Operations (DataTable-based)
    //==================================================================
    
    /**
     * Add item to inventory by ItemID
     * Primary method for adding items in new architecture
     * @param InventoryComponent Target inventory component
     * @param ItemID Item identifier from DataTable
     * @param Quantity Amount to add
     * @return true if item successfully added
     */
    UFUNCTION(BlueprintCallable, Category = "MedCom|Interaction")
    static bool AddItemToInventoryByID(UObject* InventoryComponent, FName ItemID, int32 Quantity);
    
    /**
     * Add runtime item instance to inventory
     * Used for transferring items with preserved state
     * @param InventoryComponent Target inventory component
     * @param ItemInstance Complete runtime instance
     * @return true if item successfully added
     */
    UFUNCTION(BlueprintCallable, Category = "MedCom|Interaction")
    static bool AddItemInstanceToInventory(UObject* InventoryComponent, const FInventoryItemInstance& ItemInstance);
    
    /**
     * Check if actor can pickup item
     * Validates weight, type restrictions, and space
     * @param Actor Actor attempting pickup
     * @param ItemID Item identifier from DataTable
     * @param Quantity Amount to pickup
     * @return true if pickup is possible
     */
    UFUNCTION(BlueprintCallable, Category = "MedCom|Interaction")
    static bool CanActorPickupItem(AActor* Actor, FName ItemID, int32 Quantity = 1);
    
    /**
     * Create item instance from ItemID
     * Convenience method for creating properly initialized instances
     * @param ItemID Item identifier from DataTable
     * @param Quantity Amount for the instance
     * @param OutInstance Output item instance
     * @return true if instance created successfully
     */
    UFUNCTION(BlueprintCallable, Category = "MedCom|Interaction")
    static bool CreateItemInstance(FName ItemID, int32 Quantity, FInventoryItemInstance& OutInstance);
    
    //==================================================================
    // Item Information
    //==================================================================
    
    /**
     * Get unified item data from DataTable
     * @param ItemID Item identifier
     * @param OutItemData Output unified data structure
     * @return true if data found
     */
    UFUNCTION(BlueprintCallable, Category = "MedCom|Items")
    static bool GetUnifiedItemData(FName ItemID, FMedComUnifiedItemData& OutItemData);
    
    /**
     * Get item display name
     * @param ItemID Item identifier
     * @return Localized display name or empty text
     */
    UFUNCTION(BlueprintCallable, Category = "MedCom|Items")
    static FText GetItemDisplayName(FName ItemID);
    
    /**
     * Get item weight
     * @param ItemID Item identifier
     * @return Weight per unit or 0.0f if not found
     */
    UFUNCTION(BlueprintCallable, Category = "MedCom|Items")
    static float GetItemWeight(FName ItemID);
    
    /**
     * Check if item is stackable
     * @param ItemID Item identifier
     * @return true if MaxStackSize > 1
     */
    UFUNCTION(BlueprintCallable, Category = "MedCom|Items")
    static bool IsItemStackable(FName ItemID);
    
    //==================================================================
    // Subsystem Access
    //==================================================================
    
    /**
     * Get ItemManager subsystem
     * @param WorldContextObject Any object with world context
     * @return ItemManager or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "MedCom|Subsystems", meta = (WorldContext = "WorldContextObject"))
    static UMedComItemManager* GetItemManager(const UObject* WorldContextObject);
    
    /**
     * Get EventDelegateManager subsystem
     * @param WorldContextObject Any object with world context
     * @return EventDelegateManager or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "MedCom|Subsystems", meta = (WorldContext = "WorldContextObject"))
    static UEventDelegateManager* GetEventDelegateManager(const UObject* WorldContextObject);
    
    //==================================================================
    // Inventory Validation
    //==================================================================
    
    /**
     * Validate inventory space for item
     * Checks if inventory has room for specified item
     * @param InventoryComponent Target inventory
     * @param ItemID Item to check
     * @param Quantity Amount to check
     * @param OutErrorMessage Optional error message
     * @return true if space available
     */
    UFUNCTION(BlueprintCallable, Category = "MedCom|Validation")
    static bool ValidateInventorySpace(UObject* InventoryComponent, FName ItemID, int32 Quantity, FString& OutErrorMessage);
    
    /**
     * Validate weight capacity for item
     * @param InventoryComponent Target inventory
     * @param ItemID Item to check
     * @param Quantity Amount to check
     * @param OutRemainingCapacity Remaining weight capacity
     * @return true if weight capacity sufficient
     */
    UFUNCTION(BlueprintCallable, Category = "MedCom|Validation")
    static bool ValidateWeightCapacity(UObject* InventoryComponent, FName ItemID, int32 Quantity, float& OutRemainingCapacity);
    
    //==================================================================
    // Utility Functions
    //==================================================================
    
    /**
     * Get inventory statistics
     * @param InventoryComponent Target inventory
     * @param OutTotalItems Total item count
     * @param OutTotalWeight Current weight
     * @param OutUsedSlots Used slot count
     */
    UFUNCTION(BlueprintCallable, Category = "MedCom|Statistics")
    static void GetInventoryStatistics(UObject* InventoryComponent, int32& OutTotalItems, float& OutTotalWeight, int32& OutUsedSlots);
    
    /**
     * Log inventory contents for debugging
     * @param InventoryComponent Target inventory
     * @param LogCategory Optional log category name
     */
    UFUNCTION(BlueprintCallable, Category = "MedCom|Debug")
    static void LogInventoryContents(UObject* InventoryComponent, const FString& LogCategory = TEXT("Inventory"));
};