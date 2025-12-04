// SuspenseCoreHelpers.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameFramework/PlayerState.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCoreHelpers.generated.h"

// Forward declarations
class USuspenseItemManager;
class USuspenseCoreEventBus;
struct FSuspenseUnifiedItemData;
struct FSuspenseInventoryItemInstance;

// Log categories
INTERACTIONSYSTEM_API DECLARE_LOG_CATEGORY_EXTERN(LogSuspenseCoreInteraction, Log, All);

/**
 * USuspenseCoreHelpers
 *
 * Static helper library for SuspenseCore interaction and inventory systems.
 * Provides utility functions with EventBus integration.
 *
 * EVENTBUS INTEGRATION:
 * - Broadcasts validation error events for UI feedback
 * - Uses FSuspenseCoreEventData for typed payloads
 * - Provides centralized access to EventBus
 *
 * KEY PRINCIPLES:
 * - Works exclusively with ItemID references to DataTable
 * - No data duplication or legacy structures
 * - Thread-safe operations for multiplayer
 * - Centralized access to subsystems
 *
 * @see USuspenseHelpers (Legacy reference)
 */
UCLASS()
class INTERACTIONSYSTEM_API USuspenseCoreHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//==================================================================
	// EventBus Access
	//==================================================================

	/**
	 * Get EventBus subsystem.
	 * @param WorldContextObject Any object with world context
	 * @return EventBus or nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|EventBus",
		meta = (WorldContext = "WorldContextObject"))
	static USuspenseCoreEventBus* GetEventBus(const UObject* WorldContextObject);

	/**
	 * Broadcast an event through EventBus.
	 * Convenience method for simple event broadcasting.
	 * @param WorldContextObject Any object with world context
	 * @param EventTag Event tag to broadcast
	 * @param Source Event source object
	 * @return true if event was broadcast successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|EventBus",
		meta = (WorldContext = "WorldContextObject"))
	static bool BroadcastSimpleEvent(
		const UObject* WorldContextObject,
		FGameplayTag EventTag,
		UObject* Source
	);

	//==================================================================
	// Component Discovery
	//==================================================================

	/**
	 * Find inventory component on specified actor.
	 * Searches in PlayerState first, then Actor, then Controller.
	 * @param Actor Actor to search
	 * @return Inventory component or nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Interaction")
	static UObject* FindInventoryComponent(AActor* Actor);

	/**
	 * Find PlayerState for specified actor.
	 * Handles Pawn, Controller, and direct PlayerState cases.
	 * @param Actor Actor to search
	 * @return PlayerState or nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Interaction")
	static APlayerState* FindPlayerState(AActor* Actor);

	/**
	 * Check if object implements inventory interface.
	 * @param Object Object to check
	 * @return true if object implements ISuspenseInventory
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Interaction")
	static bool ImplementsInventoryInterface(UObject* Object);

	//==================================================================
	// Item Operations (DataTable-based)
	//==================================================================

	/**
	 * Add item to inventory by ItemID.
	 * Primary method for adding items in new architecture.
	 * @param InventoryComponent Target inventory component
	 * @param ItemID Item identifier from DataTable
	 * @param Quantity Amount to add
	 * @return true if item successfully added
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Interaction")
	static bool AddItemToInventoryByID(UObject* InventoryComponent, FName ItemID, int32 Quantity);

	/**
	 * Add runtime item instance to inventory.
	 * Used for transferring items with preserved state.
	 * @param InventoryComponent Target inventory component
	 * @param ItemInstance Complete runtime instance
	 * @return true if item successfully added
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Interaction")
	static bool AddItemInstanceToInventory(
		UObject* InventoryComponent,
		const FSuspenseInventoryItemInstance& ItemInstance
	);

	/**
	 * Check if actor can pickup item.
	 * Validates weight, type restrictions, and space.
	 * Broadcasts validation failure events for UI feedback.
	 * @param Actor Actor attempting pickup
	 * @param ItemID Item identifier from DataTable
	 * @param Quantity Amount to pickup
	 * @return true if pickup is possible
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Interaction")
	static bool CanActorPickupItem(AActor* Actor, FName ItemID, int32 Quantity = 1);

	/**
	 * Create item instance from ItemID.
	 * Convenience method for creating properly initialized instances.
	 * @param WorldContextObject Any object with world context
	 * @param ItemID Item identifier from DataTable
	 * @param Quantity Amount for the instance
	 * @param OutInstance Output item instance
	 * @return true if instance created successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Interaction",
		meta = (WorldContext = "WorldContextObject"))
	static bool CreateItemInstance(
		const UObject* WorldContextObject,
		FName ItemID,
		int32 Quantity,
		FSuspenseInventoryItemInstance& OutInstance
	);

	//==================================================================
	// Item Information
	//==================================================================

	/**
	 * Get unified item data from DataTable.
	 * @param WorldContextObject Any object with world context
	 * @param ItemID Item identifier
	 * @param OutItemData Output unified data structure
	 * @return true if data found
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Items",
		meta = (WorldContext = "WorldContextObject"))
	static bool GetUnifiedItemData(
		const UObject* WorldContextObject,
		FName ItemID,
		FSuspenseUnifiedItemData& OutItemData
	);

	/**
	 * Get item display name.
	 * @param WorldContextObject Any object with world context
	 * @param ItemID Item identifier
	 * @return Localized display name or empty text
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Items",
		meta = (WorldContext = "WorldContextObject"))
	static FText GetItemDisplayName(const UObject* WorldContextObject, FName ItemID);

	/**
	 * Get item weight.
	 * @param WorldContextObject Any object with world context
	 * @param ItemID Item identifier
	 * @return Weight per unit or 0.0f if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Items",
		meta = (WorldContext = "WorldContextObject"))
	static float GetItemWeight(const UObject* WorldContextObject, FName ItemID);

	/**
	 * Check if item is stackable.
	 * @param WorldContextObject Any object with world context
	 * @param ItemID Item identifier
	 * @return true if MaxStackSize > 1
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Items",
		meta = (WorldContext = "WorldContextObject"))
	static bool IsItemStackable(const UObject* WorldContextObject, FName ItemID);

	//==================================================================
	// Subsystem Access
	//==================================================================

	/**
	 * Get ItemManager subsystem.
	 * @param WorldContextObject Any object with world context
	 * @return ItemManager or nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Subsystems",
		meta = (WorldContext = "WorldContextObject"))
	static USuspenseItemManager* GetItemManager(const UObject* WorldContextObject);

	//==================================================================
	// Inventory Validation
	//==================================================================

	/**
	 * Validate inventory space for item.
	 * Checks if inventory has room for specified item.
	 * @param InventoryComponent Target inventory
	 * @param ItemID Item to check
	 * @param Quantity Amount to check
	 * @param OutErrorMessage Optional error message
	 * @return true if space available
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Validation")
	static bool ValidateInventorySpace(
		UObject* InventoryComponent,
		FName ItemID,
		int32 Quantity,
		FString& OutErrorMessage
	);

	/**
	 * Validate weight capacity for item.
	 * @param InventoryComponent Target inventory
	 * @param ItemID Item to check
	 * @param Quantity Amount to check
	 * @param OutRemainingCapacity Remaining weight capacity
	 * @return true if weight capacity sufficient
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Validation")
	static bool ValidateWeightCapacity(
		UObject* InventoryComponent,
		FName ItemID,
		int32 Quantity,
		float& OutRemainingCapacity
	);

	//==================================================================
	// Utility Functions
	//==================================================================

	/**
	 * Get inventory statistics.
	 * @param InventoryComponent Target inventory
	 * @param OutTotalItems Total item count
	 * @param OutTotalWeight Current weight
	 * @param OutUsedSlots Used slot count
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Statistics")
	static void GetInventoryStatistics(
		UObject* InventoryComponent,
		int32& OutTotalItems,
		float& OutTotalWeight,
		int32& OutUsedSlots
	);

	/**
	 * Log inventory contents for debugging.
	 * @param InventoryComponent Target inventory
	 * @param LogCategory Optional log category name
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Debug")
	static void LogInventoryContents(
		UObject* InventoryComponent,
		const FString& LogCategory = TEXT("Inventory")
	);

protected:
	//==================================================================
	// EventBus Event Broadcasting
	//==================================================================

	/**
	 * Broadcast validation failure event.
	 * Used to notify UI of pickup failures.
	 * @param WorldContextObject World context
	 * @param Actor Actor that attempted pickup
	 * @param ItemID Item that failed validation
	 * @param Reason Failure reason
	 */
	static void BroadcastValidationFailed(
		const UObject* WorldContextObject,
		AActor* Actor,
		FName ItemID,
		const FString& Reason
	);
};
