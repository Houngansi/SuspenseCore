// SuspenseCoreInventoryConstraints.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreItemTypes.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryOperationTypes.h"
#include "SuspenseCoreInventoryConstraints.generated.h"

// Forward declarations
class USuspenseCoreInventoryComponent;
class USuspenseCoreDataManager;

/**
 * ESuspenseCoreConstraintResult
 *
 * Result of constraint validation.
 */
UENUM(BlueprintType)
enum class ESuspenseCoreConstraintResult : uint8
{
	Allowed,
	Denied,
	Conditional
};

/**
 * FSuspenseCoreConstraintContext
 *
 * Context for constraint evaluation.
 */
USTRUCT(BlueprintType)
struct INVENTORYSYSTEM_API FSuspenseCoreConstraintContext
{
	GENERATED_BODY()

	/** Source inventory */
	UPROPERTY(BlueprintReadWrite, Category = "Constraint")
	TWeakObjectPtr<USuspenseCoreInventoryComponent> SourceInventory;

	/** Target inventory (for transfers) */
	UPROPERTY(BlueprintReadWrite, Category = "Constraint")
	TWeakObjectPtr<USuspenseCoreInventoryComponent> TargetInventory;

	/** Item being validated */
	UPROPERTY(BlueprintReadWrite, Category = "Constraint")
	FSuspenseCoreItemInstance Item;

	/** Target slot index */
	UPROPERTY(BlueprintReadWrite, Category = "Constraint")
	int32 TargetSlot = -1;

	/** Operation type */
	UPROPERTY(BlueprintReadWrite, Category = "Constraint")
	ESuspenseCoreOperationType OperationType = ESuspenseCoreOperationType::None;

	/** Additional context tags */
	UPROPERTY(BlueprintReadWrite, Category = "Constraint")
	FGameplayTagContainer ContextTags;

	/** Instigator actor */
	UPROPERTY(BlueprintReadWrite, Category = "Constraint")
	TWeakObjectPtr<AActor> Instigator;

	FSuspenseCoreConstraintContext() = default;
};

/**
 * FSuspenseCoreConstraintViolation
 *
 * Details about a constraint violation.
 */
USTRUCT(BlueprintType)
struct INVENTORYSYSTEM_API FSuspenseCoreConstraintViolation
{
	GENERATED_BODY()

	/** Constraint that was violated */
	UPROPERTY(BlueprintReadOnly, Category = "Constraint")
	FName ConstraintID;

	/** Human-readable message */
	UPROPERTY(BlueprintReadOnly, Category = "Constraint")
	FText Message;

	/** Severity (for UI feedback) */
	UPROPERTY(BlueprintReadOnly, Category = "Constraint")
	uint8 Severity = 0;

	/** Tag for categorization */
	UPROPERTY(BlueprintReadOnly, Category = "Constraint")
	FGameplayTag ViolationTag;

	FSuspenseCoreConstraintViolation() = default;

	FSuspenseCoreConstraintViolation(FName InID, const FText& InMessage, uint8 InSeverity = 1)
		: ConstraintID(InID)
		, Message(InMessage)
		, Severity(InSeverity)
	{
	}
};

/**
 * FSuspenseCoreValidationResult
 *
 * Complete validation result.
 */
USTRUCT(BlueprintType)
struct INVENTORYSYSTEM_API FSuspenseCoreValidationResult
{
	GENERATED_BODY()

	/** Overall result */
	UPROPERTY(BlueprintReadOnly, Category = "Constraint")
	ESuspenseCoreConstraintResult Result = ESuspenseCoreConstraintResult::Allowed;

	/** All violations found */
	UPROPERTY(BlueprintReadOnly, Category = "Constraint")
	TArray<FSuspenseCoreConstraintViolation> Violations;

	/** Whether operation can proceed */
	UPROPERTY(BlueprintReadOnly, Category = "Constraint")
	bool bCanProceed = true;

	/** Suggested alternative (e.g., different slot) */
	UPROPERTY(BlueprintReadOnly, Category = "Constraint")
	int32 SuggestedSlot = -1;

	/** Maximum quantity allowed (for partial operations) */
	UPROPERTY(BlueprintReadOnly, Category = "Constraint")
	int32 MaxAllowedQuantity = 0;

	bool IsAllowed() const { return Result == ESuspenseCoreConstraintResult::Allowed && bCanProceed; }
	bool HasViolations() const { return Violations.Num() > 0; }

	void AddViolation(const FSuspenseCoreConstraintViolation& Violation)
	{
		Violations.Add(Violation);
		if (Violation.Severity > 0)
		{
			Result = ESuspenseCoreConstraintResult::Denied;
			bCanProceed = false;
		}
	}

	void AddViolation(FName ID, const FText& Message, uint8 Severity = 1)
	{
		AddViolation(FSuspenseCoreConstraintViolation(ID, Message, Severity));
	}

	FString GetViolationSummary() const
	{
		if (Violations.Num() == 0)
		{
			return TEXT("No violations");
		}

		FString Summary;
		for (const FSuspenseCoreConstraintViolation& V : Violations)
		{
			Summary += FString::Printf(TEXT("[%s] %s\n"), *V.ConstraintID.ToString(), *V.Message.ToString());
		}
		return Summary;
	}
};

/**
 * FSuspenseCoreSlotConstraint
 *
 * Constraint for a specific slot.
 */
USTRUCT(BlueprintType)
struct INVENTORYSYSTEM_API FSuspenseCoreSlotConstraint
{
	GENERATED_BODY()

	/** Slot index this constraint applies to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint")
	int32 SlotIndex = -1;

	/** Allowed item types (empty = all allowed) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint")
	FGameplayTagContainer AllowedTypes;

	/** Blocked item types */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint")
	FGameplayTagContainer BlockedTypes;

	/** Maximum item size for this slot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint")
	FIntPoint MaxItemSize = FIntPoint(1, 1);

	/** Whether slot is locked */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint")
	bool bIsLocked = false;

	/** Custom constraint ID for special handling */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint")
	FName CustomConstraintID;

	bool IsValid() const { return SlotIndex >= 0; }

	bool AllowsItemType(const FGameplayTagContainer& ItemTags) const
	{
		// Check blocked first
		if (BlockedTypes.HasAny(ItemTags))
		{
			return false;
		}

		// If no allowed types specified, allow all
		if (AllowedTypes.IsEmpty())
		{
			return true;
		}

		// Check if item has any allowed type
		return AllowedTypes.HasAny(ItemTags);
	}
};

/**
 * FSuspenseCoreInventoryRules
 *
 * Rules configuration for inventory.
 */
USTRUCT(BlueprintType)
struct INVENTORYSYSTEM_API FSuspenseCoreInventoryRules
{
	GENERATED_BODY()

	/** Allowed item types for this inventory */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules")
	FGameplayTagContainer AllowedItemTypes;

	/** Blocked item types */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules")
	FGameplayTagContainer BlockedItemTypes;

	/** Per-slot constraints */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules")
	TArray<FSuspenseCoreSlotConstraint> SlotConstraints;

	/** Whether stacking is allowed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules")
	bool bAllowStacking = true;

	/** Whether rotation is allowed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules")
	bool bAllowRotation = true;

	/** Whether items can be dropped from this inventory */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules")
	bool bAllowDrop = true;

	/** Maximum unique item types */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules")
	int32 MaxUniqueItems = 0; // 0 = no limit

	/** Maximum total quantity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules")
	int32 MaxTotalQuantity = 0; // 0 = no limit

	/** Required tags for adding items */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules")
	FGameplayTagContainer RequiredTagsForAdd;

	bool HasSlotConstraint(int32 SlotIndex) const
	{
		return SlotConstraints.ContainsByPredicate([SlotIndex](const FSuspenseCoreSlotConstraint& C)
		{
			return C.SlotIndex == SlotIndex;
		});
	}

	const FSuspenseCoreSlotConstraint* GetSlotConstraint(int32 SlotIndex) const
	{
		return SlotConstraints.FindByPredicate([SlotIndex](const FSuspenseCoreSlotConstraint& C)
		{
			return C.SlotIndex == SlotIndex;
		});
	}
};

/**
 * USuspenseCoreInventoryConstraints
 *
 * Constraint validator for inventory operations.
 * Validates operations before execution to prevent invalid states.
 *
 * FEATURES:
 * - Type-based restrictions
 * - Slot-specific constraints
 * - Weight/quantity limits
 * - Custom validation rules
 * - Blueprint extensibility
 */
UCLASS(BlueprintType, Blueprintable)
class INVENTORYSYSTEM_API USuspenseCoreInventoryConstraints : public UObject
{
	GENERATED_BODY()

public:
	USuspenseCoreInventoryConstraints();

	//==================================================================
	// Core Validation
	//==================================================================

	/**
	 * Validate an operation.
	 * @param Context Operation context
	 * @return Validation result
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Constraints")
	FSuspenseCoreValidationResult ValidateOperation(const FSuspenseCoreConstraintContext& Context);

	/**
	 * Validate adding item to inventory.
	 * @param Inventory Target inventory
	 * @param Item Item to add
	 * @param TargetSlot Optional target slot
	 * @return Validation result
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Constraints")
	FSuspenseCoreValidationResult ValidateAddItem(
		USuspenseCoreInventoryComponent* Inventory,
		const FSuspenseCoreItemInstance& Item,
		int32 TargetSlot = -1
	);

	/**
	 * Validate removing item from inventory.
	 * @param Inventory Source inventory
	 * @param Item Item to remove
	 * @param Quantity Quantity to remove
	 * @return Validation result
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Constraints")
	FSuspenseCoreValidationResult ValidateRemoveItem(
		USuspenseCoreInventoryComponent* Inventory,
		const FSuspenseCoreItemInstance& Item,
		int32 Quantity = -1
	);

	/**
	 * Validate moving item within inventory.
	 * @param Inventory Inventory
	 * @param Item Item to move
	 * @param TargetSlot Target slot
	 * @return Validation result
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Constraints")
	FSuspenseCoreValidationResult ValidateMoveItem(
		USuspenseCoreInventoryComponent* Inventory,
		const FSuspenseCoreItemInstance& Item,
		int32 TargetSlot
	);

	/**
	 * Validate transferring item between inventories.
	 * @param SourceInventory Source
	 * @param TargetInventory Destination
	 * @param Item Item to transfer
	 * @param TargetSlot Optional target slot
	 * @return Validation result
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Constraints")
	FSuspenseCoreValidationResult ValidateTransfer(
		USuspenseCoreInventoryComponent* SourceInventory,
		USuspenseCoreInventoryComponent* TargetInventory,
		const FSuspenseCoreItemInstance& Item,
		int32 TargetSlot = -1
	);

	//==================================================================
	// Quick Checks
	//==================================================================

	/**
	 * Quick check if item type is allowed in inventory.
	 * @param Inventory Inventory to check
	 * @param ItemID Item ID
	 * @return true if allowed
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Constraints")
	bool CanAcceptItemType(
		USuspenseCoreInventoryComponent* Inventory,
		FName ItemID
	);

	/**
	 * Quick check if slot can accept item.
	 * @param Inventory Inventory
	 * @param SlotIndex Slot to check
	 * @param Item Item to place
	 * @return true if slot can accept item
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Constraints")
	bool CanSlotAcceptItem(
		USuspenseCoreInventoryComponent* Inventory,
		int32 SlotIndex,
		const FSuspenseCoreItemInstance& Item
	);

	/**
	 * Find best slot for item.
	 * @param Inventory Inventory
	 * @param Item Item to place
	 * @return Best slot index or -1 if none
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Constraints")
	int32 FindBestSlot(
		USuspenseCoreInventoryComponent* Inventory,
		const FSuspenseCoreItemInstance& Item
	);

	/**
	 * Get maximum quantity that can be added.
	 * @param Inventory Inventory
	 * @param ItemID Item ID
	 * @return Maximum addable quantity
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Constraints")
	int32 GetMaxAddableQuantity(
		USuspenseCoreInventoryComponent* Inventory,
		FName ItemID
	);

	//==================================================================
	// Rules Management
	//==================================================================

	/**
	 * Set rules for inventory.
	 * @param Inventory Inventory
	 * @param Rules Rules to apply
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Constraints")
	void SetInventoryRules(
		USuspenseCoreInventoryComponent* Inventory,
		const FSuspenseCoreInventoryRules& Rules
	);

	/**
	 * Get rules for inventory.
	 * @param Inventory Inventory
	 * @param OutRules Output rules
	 * @return true if rules exist
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Constraints")
	bool GetInventoryRules(
		USuspenseCoreInventoryComponent* Inventory,
		FSuspenseCoreInventoryRules& OutRules
	);

	/**
	 * Clear rules for inventory.
	 * @param Inventory Inventory
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Constraints")
	void ClearInventoryRules(USuspenseCoreInventoryComponent* Inventory);

	/**
	 * Lock slot.
	 * @param Inventory Inventory
	 * @param SlotIndex Slot to lock
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Constraints")
	void LockSlot(USuspenseCoreInventoryComponent* Inventory, int32 SlotIndex);

	/**
	 * Unlock slot.
	 * @param Inventory Inventory
	 * @param SlotIndex Slot to unlock
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Constraints")
	void UnlockSlot(USuspenseCoreInventoryComponent* Inventory, int32 SlotIndex);

	/**
	 * Check if slot is locked.
	 * @param Inventory Inventory
	 * @param SlotIndex Slot to check
	 * @return true if locked
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Constraints")
	bool IsSlotLocked(USuspenseCoreInventoryComponent* Inventory, int32 SlotIndex);

	//==================================================================
	// Blueprint Extensibility
	//==================================================================

protected:
	/**
	 * Blueprint-implementable custom validation.
	 * @param Context Validation context
	 * @param OutViolation Output violation if failed
	 * @return true if validation passed
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|Inventory|Constraints")
	bool CustomValidation(
		const FSuspenseCoreConstraintContext& Context,
		FSuspenseCoreConstraintViolation& OutViolation
	);

	virtual bool CustomValidation_Implementation(
		const FSuspenseCoreConstraintContext& Context,
		FSuspenseCoreConstraintViolation& OutViolation
	);

	//==================================================================
	// Internal Validation
	//==================================================================

private:
	/** Validate item type against rules */
	bool ValidateItemType(
		const FSuspenseCoreInventoryRules& Rules,
		const FSuspenseCoreItemData& ItemData,
		FSuspenseCoreValidationResult& Result
	);

	/** Validate slot constraints */
	bool ValidateSlotConstraints(
		const FSuspenseCoreInventoryRules& Rules,
		int32 SlotIndex,
		const FSuspenseCoreItemInstance& Item,
		const FSuspenseCoreItemData& ItemData,
		FSuspenseCoreValidationResult& Result
	);

	/** Validate weight limit */
	bool ValidateWeight(
		USuspenseCoreInventoryComponent* Inventory,
		const FSuspenseCoreItemInstance& Item,
		const FSuspenseCoreItemData& ItemData,
		FSuspenseCoreValidationResult& Result
	);

	/** Validate quantity limits */
	bool ValidateQuantityLimits(
		USuspenseCoreInventoryComponent* Inventory,
		const FSuspenseCoreInventoryRules& Rules,
		const FSuspenseCoreItemInstance& Item,
		FSuspenseCoreValidationResult& Result
	);

	/** Get item data from DataManager */
	bool GetItemDataForValidation(FName ItemID, FSuspenseCoreItemData& OutData);

	//==================================================================
	// Data
	//==================================================================

	/** Cached rules per inventory */
	UPROPERTY()
	TMap<TWeakObjectPtr<USuspenseCoreInventoryComponent>, FSuspenseCoreInventoryRules> InventoryRulesMap;

	/** Locked slots per inventory */
	TMap<TWeakObjectPtr<USuspenseCoreInventoryComponent>, TSet<int32>> LockedSlotsMap;

	/** Reference to data manager */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreDataManager> DataManagerRef;
};

/**
 * USuspenseCoreConstraintPresets
 *
 * Common constraint presets.
 */
UCLASS(BlueprintType)
class INVENTORYSYSTEM_API USuspenseCoreConstraintPresets : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Create rules for weapon-only inventory.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Constraints")
	static FSuspenseCoreInventoryRules MakeWeaponOnlyRules();

	/**
	 * Create rules for armor-only inventory.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Constraints")
	static FSuspenseCoreInventoryRules MakeArmorOnlyRules();

	/**
	 * Create rules for consumables-only inventory.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Constraints")
	static FSuspenseCoreInventoryRules MakeConsumablesOnlyRules();

	/**
	 * Create rules for junk/vendor inventory.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Constraints")
	static FSuspenseCoreInventoryRules MakeJunkOnlyRules();

	/**
	 * Create rules for storage container.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Constraints")
	static FSuspenseCoreInventoryRules MakeStorageRules(bool bAllowAll = true);

	/**
	 * Create rules for quest items inventory.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Constraints")
	static FSuspenseCoreInventoryRules MakeQuestItemsRules();
};
