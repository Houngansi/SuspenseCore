// SuspenseCoreInventoryLibrary.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryOperationTypes.h"
#include "SuspenseCore/Types/Items/SuspenseCoreItemTypes.h"
#include "SuspenseCoreInventoryLibrary.generated.h"

// Forward declarations
class USuspenseCoreInventoryComponent;
class USuspenseCoreInventoryManager;
class AActor;

/**
 * USuspenseCoreInventoryLibrary
 *
 * Blueprint function library for SuspenseCore inventory operations.
 * Provides static helper functions for common inventory tasks.
 *
 * USAGE:
 * Call from Blueprint or C++ for utility operations.
 */
UCLASS()
class INVENTORYSYSTEM_API USuspenseCoreInventoryLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//==================================================================
	// Manager Access
	//==================================================================

	/**
	 * Get inventory manager from world context.
	 * @param WorldContextObject World context
	 * @return Inventory manager instance
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory",
		meta = (WorldContext = "WorldContextObject"))
	static USuspenseCoreInventoryManager* GetInventoryManager(UObject* WorldContextObject);

	/**
	 * Get inventory component from actor.
	 * @param Actor Actor to search
	 * @return First inventory component found
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory")
	static USuspenseCoreInventoryComponent* GetInventoryComponent(AActor* Actor);

	/**
	 * Get all inventory components from actor.
	 * @param Actor Actor to search
	 * @return All inventory components
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory")
	static TArray<USuspenseCoreInventoryComponent*> GetAllInventoryComponents(AActor* Actor);

	//==================================================================
	// Item Utilities
	//==================================================================

	/**
	 * Check if item instance is valid.
	 * @param Instance Instance to check
	 * @return true if valid
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Items")
	static bool IsItemInstanceValid(const FSuspenseCoreItemInstance& Instance);

	/**
	 * Check if two instances can stack.
	 * @param Instance1 First instance
	 * @param Instance2 Second instance
	 * @return true if stackable
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Items")
	static bool CanItemsStack(const FSuspenseCoreItemInstance& Instance1, const FSuspenseCoreItemInstance& Instance2);

	/**
	 * Get item display name.
	 * @param WorldContextObject World context
	 * @param ItemID Item ID
	 * @return Display name or ItemID if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Items",
		meta = (WorldContext = "WorldContextObject"))
	static FText GetItemDisplayName(UObject* WorldContextObject, FName ItemID);

	/**
	 * Get item description.
	 * @param WorldContextObject World context
	 * @param ItemID Item ID
	 * @return Item description
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Items",
		meta = (WorldContext = "WorldContextObject"))
	static FText GetItemDescription(UObject* WorldContextObject, FName ItemID);

	/**
	 * Get item icon.
	 * @param WorldContextObject World context
	 * @param ItemID Item ID
	 * @return Icon texture (may need async load)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Items",
		meta = (WorldContext = "WorldContextObject"))
	static UTexture2D* GetItemIcon(UObject* WorldContextObject, FName ItemID);

	/**
	 * Get item rarity.
	 * @param WorldContextObject World context
	 * @param ItemID Item ID
	 * @return Rarity tag
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Items",
		meta = (WorldContext = "WorldContextObject"))
	static FGameplayTag GetItemRarity(UObject* WorldContextObject, FName ItemID);

	/**
	 * Get item type.
	 * @param WorldContextObject World context
	 * @param ItemID Item ID
	 * @return Type tag
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Items",
		meta = (WorldContext = "WorldContextObject"))
	static FGameplayTag GetItemType(UObject* WorldContextObject, FName ItemID);

	//==================================================================
	// Grid Utilities
	//==================================================================

	/**
	 * Convert slot index to grid position.
	 * @param SlotIndex Linear slot index
	 * @param GridWidth Grid width
	 * @return Grid position (X, Y)
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Grid")
	static FIntPoint SlotToGridPosition(int32 SlotIndex, int32 GridWidth);

	/**
	 * Convert grid position to slot index.
	 * @param GridPosition Grid position
	 * @param GridWidth Grid width
	 * @return Linear slot index
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Grid")
	static int32 GridPositionToSlot(FIntPoint GridPosition, int32 GridWidth);

	/**
	 * Check if grid position is valid.
	 * @param Position Position to check
	 * @param GridSize Grid dimensions
	 * @return true if valid
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Grid")
	static bool IsValidGridPosition(FIntPoint Position, FIntPoint GridSize);

	/**
	 * Get rotated item size.
	 * @param OriginalSize Original size
	 * @param Rotation Rotation (0, 90, 180, 270)
	 * @return Rotated size
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Grid")
	static FIntPoint GetRotatedSize(FIntPoint OriginalSize, int32 Rotation);

	/**
	 * Get slots occupied by item.
	 * @param AnchorSlot Top-left slot
	 * @param ItemSize Item size
	 * @param GridWidth Grid width
	 * @return Array of occupied slot indices
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Grid")
	static TArray<int32> GetOccupiedSlots(int32 AnchorSlot, FIntPoint ItemSize, int32 GridWidth);

	//==================================================================
	// Weight Utilities
	//==================================================================

	/**
	 * Calculate total weight of items.
	 * @param WorldContextObject World context
	 * @param Items Items to calculate
	 * @return Total weight
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Weight",
		meta = (WorldContext = "WorldContextObject"))
	static float CalculateTotalWeight(UObject* WorldContextObject, const TArray<FSuspenseCoreItemInstance>& Items);

	/**
	 * Get weight of single item.
	 * @param WorldContextObject World context
	 * @param ItemID Item ID
	 * @return Weight per unit
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Weight",
		meta = (WorldContext = "WorldContextObject"))
	static float GetItemWeight(UObject* WorldContextObject, FName ItemID);

	/**
	 * Format weight for display.
	 * @param Weight Weight value
	 * @return Formatted string (e.g., "1.5 kg")
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Weight")
	static FString FormatWeight(float Weight);

	//==================================================================
	// Result Utilities
	//==================================================================

	/**
	 * Check if operation result is success.
	 * @param Result Operation result
	 * @return true if successful
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Results")
	static bool IsOperationSuccess(const FSuspenseInventoryOperationResult& Result);

	/**
	 * Get result message.
	 * @param Result Operation result
	 * @return Human-readable message
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Results")
	static FString GetResultMessage(const FSuspenseInventoryOperationResult& Result);

	/**
	 * Get localized result code name.
	 * @param ResultCode Result code
	 * @return Localized name
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Results")
	static FText GetResultCodeDisplayName(ESuspenseCoreInventoryResult ResultCode);

	//==================================================================
	// Comparison Utilities
	//==================================================================

	/**
	 * Compare items by name.
	 * @param A First instance
	 * @param B Second instance
	 * @return <0 if A<B, 0 if equal, >0 if A>B
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Compare")
	static int32 CompareItemsByName(const FSuspenseCoreItemInstance& A, const FSuspenseCoreItemInstance& B);

	/**
	 * Compare items by quantity.
	 * @param A First instance
	 * @param B Second instance
	 * @return <0 if A<B, 0 if equal, >0 if A>B
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Compare")
	static int32 CompareItemsByQuantity(const FSuspenseCoreItemInstance& A, const FSuspenseCoreItemInstance& B);

	//==================================================================
	// Debug Utilities
	//==================================================================

	/**
	 * Get item instance debug string.
	 * @param Instance Instance to describe
	 * @return Debug string
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Debug")
	static FString GetItemInstanceDebugString(const FSuspenseCoreItemInstance& Instance);

	/**
	 * Get operation record debug string.
	 * @param Record Record to describe
	 * @return Debug string
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Debug")
	static FString GetOperationRecordDebugString(const FSuspenseCoreOperationRecord& Record);
};
