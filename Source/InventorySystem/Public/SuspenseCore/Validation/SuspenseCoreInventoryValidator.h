// SuspenseCoreInventoryValidator.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/SuspenseCoreInventoryTypes.h"
#include "SuspenseCoreInventoryValidator.generated.h"

// Forward declarations
struct FSuspenseCoreItemData;
struct FSuspenseCoreItemInstance;
class USuspenseCoreInventoryComponent;
class USuspenseCoreDataManager;

/**
 * USuspenseCoreInventoryValidator
 *
 * Validation logic for inventory operations.
 * Centralized validation rules for consistency.
 *
 * ARCHITECTURE:
 * - Stateless validation (uses passed context)
 * - Detailed error reporting
 * - Extensible via subclassing
 *
 * VALIDATION CATEGORIES:
 * - Space: Grid space available
 * - Weight: Weight capacity
 * - Type: Item type restrictions
 * - Quantity: Stack limits
 * - Custom: Game-specific rules
 */
UCLASS(Blueprintable)
class INVENTORYSYSTEM_API USuspenseCoreInventoryValidator : public UObject
{
	GENERATED_BODY()

public:
	USuspenseCoreInventoryValidator();

	//==================================================================
	// Main Validation Entry Points
	//==================================================================

	/**
	 * Validate add item operation.
	 * Checks all constraints for adding item to inventory.
	 * @param Component Inventory component
	 * @param ItemID Item to add
	 * @param Quantity Amount to add
	 * @param OutResult Detailed result
	 * @return true if add is valid
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Validation")
	bool ValidateAddItem(
		USuspenseCoreInventoryComponent* Component,
		FName ItemID,
		int32 Quantity,
		FSuspenseCoreInventoryOperationResult& OutResult
	) const;

	/**
	 * Validate add item instance operation.
	 * @param Component Inventory component
	 * @param ItemInstance Instance to add
	 * @param TargetSlot Target slot (-1 for auto)
	 * @param OutResult Detailed result
	 * @return true if add is valid
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Validation")
	bool ValidateAddItemInstance(
		USuspenseCoreInventoryComponent* Component,
		const FSuspenseCoreItemInstance& ItemInstance,
		int32 TargetSlot,
		FSuspenseCoreInventoryOperationResult& OutResult
	) const;

	/**
	 * Validate remove item operation.
	 * @param Component Inventory component
	 * @param ItemID Item to remove
	 * @param Quantity Amount to remove
	 * @param OutResult Detailed result
	 * @return true if remove is valid
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Validation")
	bool ValidateRemoveItem(
		USuspenseCoreInventoryComponent* Component,
		FName ItemID,
		int32 Quantity,
		FSuspenseCoreInventoryOperationResult& OutResult
	) const;

	/**
	 * Validate move item operation.
	 * @param Component Inventory component
	 * @param FromSlot Source slot
	 * @param ToSlot Target slot
	 * @param OutResult Detailed result
	 * @return true if move is valid
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Validation")
	bool ValidateMoveItem(
		USuspenseCoreInventoryComponent* Component,
		int32 FromSlot,
		int32 ToSlot,
		FSuspenseCoreInventoryOperationResult& OutResult
	) const;

	/**
	 * Validate swap items operation.
	 * @param Component Inventory component
	 * @param Slot1 First slot
	 * @param Slot2 Second slot
	 * @param OutResult Detailed result
	 * @return true if swap is valid
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Validation")
	bool ValidateSwapItems(
		USuspenseCoreInventoryComponent* Component,
		int32 Slot1,
		int32 Slot2,
		FSuspenseCoreInventoryOperationResult& OutResult
	) const;

	/**
	 * Validate split stack operation.
	 * @param Component Inventory component
	 * @param SourceSlot Slot with stack
	 * @param SplitQuantity Amount to split
	 * @param TargetSlot Where to place split
	 * @param OutResult Detailed result
	 * @return true if split is valid
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Validation")
	bool ValidateSplitStack(
		USuspenseCoreInventoryComponent* Component,
		int32 SourceSlot,
		int32 SplitQuantity,
		int32 TargetSlot,
		FSuspenseCoreInventoryOperationResult& OutResult
	) const;

	//==================================================================
	// Individual Constraint Checks
	//==================================================================

	/**
	 * Check weight constraint.
	 * @param Component Inventory component
	 * @param AdditionalWeight Weight to add
	 * @param OutRemainingCapacity Remaining capacity after add
	 * @return true if has capacity
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Validation")
	bool CheckWeightConstraint(
		USuspenseCoreInventoryComponent* Component,
		float AdditionalWeight,
		float& OutRemainingCapacity
	) const;

	/**
	 * Check type constraint.
	 * @param Component Inventory component
	 * @param ItemType Item type tag
	 * @return true if type allowed
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Validation")
	bool CheckTypeConstraint(
		USuspenseCoreInventoryComponent* Component,
		FGameplayTag ItemType
	) const;

	/**
	 * Check space constraint for item.
	 * @param Component Inventory component
	 * @param ItemGridSize Item size in grid
	 * @param TargetSlot Specific slot or -1 for any
	 * @param bAllowRotation Allow rotation to fit
	 * @param OutAvailableSlot Slot where item can fit
	 * @return true if space available
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Validation")
	bool CheckSpaceConstraint(
		USuspenseCoreInventoryComponent* Component,
		FIntPoint ItemGridSize,
		int32 TargetSlot,
		bool bAllowRotation,
		int32& OutAvailableSlot
	) const;

	/**
	 * Check stack constraint.
	 * @param Component Inventory component
	 * @param ItemID Item type
	 * @param AdditionalQuantity Quantity to add
	 * @param OutCanFullyStack Can add full quantity
	 * @param OutRemainder Quantity that wouldn't fit
	 * @return true if at least partial stack possible
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Validation")
	bool CheckStackConstraint(
		USuspenseCoreInventoryComponent* Component,
		FName ItemID,
		int32 AdditionalQuantity,
		bool& OutCanFullyStack,
		int32& OutRemainder
	) const;

	//==================================================================
	// Integrity Validation
	//==================================================================

	/**
	 * Validate inventory integrity.
	 * Checks for inconsistencies in inventory state.
	 * @param Component Inventory component
	 * @param OutErrors Array of error messages
	 * @return true if no integrity issues
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Validation")
	bool ValidateIntegrity(
		USuspenseCoreInventoryComponent* Component,
		TArray<FString>& OutErrors
	) const;

	/**
	 * Attempt to repair integrity issues.
	 * @param Component Inventory component
	 * @param OutRepairLog Log of repairs made
	 * @return Number of issues repaired
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Validation")
	int32 RepairIntegrity(
		USuspenseCoreInventoryComponent* Component,
		TArray<FString>& OutRepairLog
	);

	//==================================================================
	// Utility
	//==================================================================

	/**
	 * Get item data from DataManager.
	 * @param Component Component for world context
	 * @param ItemID Item to lookup
	 * @param OutItemData Output data
	 * @return true if found
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Validation")
	bool GetItemData(
		USuspenseCoreInventoryComponent* Component,
		FName ItemID,
		FSuspenseCoreItemData& OutItemData
	) const;

protected:
	/**
	 * Get DataManager from component's world.
	 */
	USuspenseCoreDataManager* GetDataManager(USuspenseCoreInventoryComponent* Component) const;

	/**
	 * Virtual hook for custom validation.
	 * Override in subclass for game-specific rules.
	 * @param Component Inventory component
	 * @param ItemID Item being validated
	 * @param OutResult Result to modify
	 * @return true if custom validation passes
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|Inventory|Validation")
	bool ValidateCustomRules(
		USuspenseCoreInventoryComponent* Component,
		FName ItemID,
		FSuspenseCoreInventoryOperationResult& OutResult
	) const;
};
