// ISuspenseCoreSlotValidator.h
// SuspenseCore Slot Validator Interface
// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Types/Equipment/SuspenseEquipmentTypes.h"
#include "ISuspenseCoreSlotValidator.generated.h"

/**
 * Validation failure types for detailed error reporting
 */
UENUM(BlueprintType)
enum class ESuspenseCoreValidationFailure : uint8
{
	None = 0					UMETA(DisplayName = "No Failure"),
	SlotTypeIncompatible		UMETA(DisplayName = "Slot Type Incompatible"),
	ItemTypeIncompatible		UMETA(DisplayName = "Item Type Incompatible"),
	LevelRequirementNotMet		UMETA(DisplayName = "Level Requirement Not Met"),
	ClassRequirementNotMet		UMETA(DisplayName = "Class Requirement Not Met"),
	WeightLimitExceeded			UMETA(DisplayName = "Weight Limit Exceeded"),
	SizeLimitExceeded			UMETA(DisplayName = "Size Limit Exceeded"),
	SlotLocked					UMETA(DisplayName = "Slot Locked"),
	SlotDisabled				UMETA(DisplayName = "Slot Disabled"),
	ItemConflict				UMETA(DisplayName = "Item Conflict"),
	UniqueConstraintViolation	UMETA(DisplayName = "Unique Constraint Violation"),
	RequiredTagMissing			UMETA(DisplayName = "Required Tag Missing"),
	ExcludedTagPresent			UMETA(DisplayName = "Excluded Tag Present"),
	CustomRuleFailed			UMETA(DisplayName = "Custom Rule Failed"),
	InvalidItem					UMETA(DisplayName = "Invalid Item"),
	InvalidSlot					UMETA(DisplayName = "Invalid Slot")
};

/**
 * SuspenseCore Slot Validation Result
 *
 * Detailed validation result with error information.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreSlotValidationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	bool bIsValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	ESuspenseCoreValidationFailure FailureType = ESuspenseCoreValidationFailure::None;

	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	FText ErrorMessage;

	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	FGameplayTag ErrorTag;

	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	TArray<FString> ValidationDetails;

	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	float ConfidenceScore = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	int32 ResultCode = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	float ValidationTimeMs = 0.0f;

	static FSuspenseCoreSlotValidationResult Success()
	{
		FSuspenseCoreSlotValidationResult Result;
		Result.bIsValid = true;
		Result.ConfidenceScore = 1.0f;
		return Result;
	}

	static FSuspenseCoreSlotValidationResult Failure(
		ESuspenseCoreValidationFailure Type,
		const FText& Message,
		const FGameplayTag& Tag = FGameplayTag())
	{
		FSuspenseCoreSlotValidationResult Result;
		Result.bIsValid = false;
		Result.FailureType = Type;
		Result.ErrorMessage = Message;
		Result.ErrorTag = Tag;
		Result.ConfidenceScore = 0.0f;
		return Result;
	}

	void AddDetail(const FString& Detail) { ValidationDetails.Add(Detail); }
	bool HasDetails() const { return ValidationDetails.Num() > 0; }

	/** Convert to legacy FSlotValidationResult for compatibility */
	FSlotValidationResult ToLegacy() const
	{
		FSlotValidationResult Legacy;
		Legacy.bIsValid = bIsValid;
		Legacy.ErrorMessage = ErrorMessage;
		Legacy.ErrorTag = ErrorTag;
		return Legacy;
	}

	/** Create from legacy FSlotValidationResult */
	static FSuspenseCoreSlotValidationResult FromLegacy(const FSlotValidationResult& Legacy)
	{
		FSuspenseCoreSlotValidationResult Result;
		Result.bIsValid = Legacy.bIsValid;
		Result.ErrorMessage = Legacy.ErrorMessage;
		Result.ErrorTag = Legacy.ErrorTag;
		return Result;
	}
};

/**
 * Batch validation request for multiple operations
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreBatchValidationRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Validation")
	TArray<FSuspenseInventoryItemInstance> Items;

	UPROPERTY(BlueprintReadWrite, Category = "Validation")
	TArray<FEquipmentSlotConfig> SlotConfigs;

	UPROPERTY(BlueprintReadWrite, Category = "Validation")
	FGuid RequestId;

	UPROPERTY(BlueprintReadWrite, Category = "Validation")
	bool bStopOnFirstFailure = false;

	FSuspenseCoreBatchValidationRequest()
	{
		RequestId = FGuid::NewGuid();
	}
};

/**
 * Batch validation result
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreBatchValidationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	bool bAllValid = true;

	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	TArray<FSuspenseCoreSlotValidationResult> Results;

	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	TArray<int32> FailedIndices;

	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	float TotalValidationTimeMs = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	FGuid RequestId;

	int32 GetPassedCount() const { return Results.Num() - FailedIndices.Num(); }
	int32 GetFailedCount() const { return FailedIndices.Num(); }
};

/**
 * Slot restriction configuration
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreSlotRestrictions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Restrictions")
	float MaxWeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Restrictions")
	FIntVector MaxSize = FIntVector::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Restrictions")
	int32 MinLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Restrictions")
	FGameplayTagContainer RequiredTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Restrictions")
	FGameplayTagContainer ExcludedTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Restrictions")
	FGameplayTag UniqueGroupTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Restrictions")
	bool bIsLocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Restrictions")
	bool bIsDisabled = false;

	bool HasRestrictions() const
	{
		return MaxWeight > 0.0f || MaxSize != FIntVector::ZeroValue ||
			   MinLevel > 0 || RequiredTags.Num() > 0 || ExcludedTags.Num() > 0 ||
			   UniqueGroupTag.IsValid() || bIsLocked || bIsDisabled;
	}
};

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreSlotValidator : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISuspenseCoreSlotValidator
 *
 * Interface for slot validation logic.
 * Provides centralized validation rules for equipment slot operations.
 *
 * Architecture:
 * - Pure validation (no state changes)
 * - Thread-safe for concurrent validation
 * - Supports caching for performance
 * - Extensible rule system
 *
 * Contract:
 * - All validation methods are const (read-only)
 * - Thread-safe after initialization
 * - No side effects during validation
 */
class BRIDGESYSTEM_API ISuspenseCoreSlotValidator
{
	GENERATED_BODY()

public:
	//========================================
	// Primary Validation
	//========================================

	/**
	 * Validate if item can be placed in slot
	 * @param SlotConfig Slot configuration
	 * @param ItemInstance Item to validate
	 * @return Validation result with details
	 */
	virtual FSuspenseCoreSlotValidationResult CanPlaceItemInSlot(
		const FEquipmentSlotConfig& SlotConfig,
		const FSuspenseInventoryItemInstance& ItemInstance) const = 0;

	/**
	 * Validate if items can be swapped between slots
	 * @param SlotConfigA First slot
	 * @param ItemA Item in first slot
	 * @param SlotConfigB Second slot
	 * @param ItemB Item in second slot
	 * @return Validation result
	 */
	virtual FSuspenseCoreSlotValidationResult CanSwapItems(
		const FEquipmentSlotConfig& SlotConfigA,
		const FSuspenseInventoryItemInstance& ItemA,
		const FEquipmentSlotConfig& SlotConfigB,
		const FSuspenseInventoryItemInstance& ItemB) const = 0;

	/**
	 * Validate slot configuration integrity
	 * @param SlotConfig Configuration to validate
	 * @return Validation result
	 */
	virtual FSuspenseCoreSlotValidationResult ValidateSlotConfiguration(
		const FEquipmentSlotConfig& SlotConfig) const = 0;

	//========================================
	// Batch Validation
	//========================================

	/**
	 * Validate multiple items/slots in batch
	 * @param Request Batch validation request
	 * @return Batch validation result
	 */
	virtual FSuspenseCoreBatchValidationResult ValidateBatch(
		const FSuspenseCoreBatchValidationRequest& Request) const = 0;

	/**
	 * Quick validation without detailed results
	 * @param SlotConfig Slot to check
	 * @param ItemInstance Item to check
	 * @return True if valid
	 */
	virtual bool QuickValidate(
		const FEquipmentSlotConfig& SlotConfig,
		const FSuspenseInventoryItemInstance& ItemInstance) const = 0;

	//========================================
	// Specialized Checks
	//========================================

	/**
	 * Check slot requirements against tags
	 * @param SlotConfig Slot configuration
	 * @param Requirements Required tags
	 * @return Validation result
	 */
	virtual FSuspenseCoreSlotValidationResult CheckSlotRequirements(
		const FEquipmentSlotConfig& SlotConfig,
		const FGameplayTagContainer& Requirements) const = 0;

	/**
	 * Check item type compatibility with slot type
	 * @param ItemType Item type tag
	 * @param SlotType Equipment slot type
	 * @return True if compatible
	 */
	virtual bool IsItemTypeCompatibleWithSlot(
		const FGameplayTag& ItemType,
		EEquipmentSlotType SlotType) const = 0;

	/**
	 * Check weight limit
	 * @param ItemWeight Item weight
	 * @param SlotMaxWeight Slot weight limit
	 * @return Validation result
	 */
	virtual FSuspenseCoreSlotValidationResult CheckWeightLimit(
		float ItemWeight,
		float SlotMaxWeight) const = 0;

	/**
	 * Check level requirement
	 * @param RequiredLevel Required level
	 * @param ActualLevel Actual level
	 * @return Validation result
	 */
	virtual FSuspenseCoreSlotValidationResult CheckLevelRequirement(
		int32 RequiredLevel,
		int32 ActualLevel) const = 0;

	//========================================
	// Slot Query
	//========================================

	/**
	 * Find compatible slots for item type
	 * @param ItemType Item type to find slots for
	 * @return Array of compatible slot types
	 */
	virtual TArray<EEquipmentSlotType> GetCompatibleSlotTypes(
		const FGameplayTag& ItemType) const = 0;

	/**
	 * Get compatible item types for slot
	 * @param SlotType Slot type
	 * @return Array of compatible item type tags
	 */
	virtual TArray<FGameplayTag> GetCompatibleItemTypes(
		EEquipmentSlotType SlotType) const = 0;

	//========================================
	// Restrictions Management
	//========================================

	/**
	 * Set slot restrictions
	 * @param SlotTag Slot identifier
	 * @param Restrictions Restrictions to set
	 */
	virtual void SetSlotRestrictions(
		const FGameplayTag& SlotTag,
		const FSuspenseCoreSlotRestrictions& Restrictions) = 0;

	/**
	 * Get slot restrictions
	 * @param SlotTag Slot identifier
	 * @return Current restrictions
	 */
	virtual FSuspenseCoreSlotRestrictions GetSlotRestrictions(
		const FGameplayTag& SlotTag) const = 0;

	/**
	 * Clear slot restrictions
	 * @param SlotTag Slot identifier
	 */
	virtual void ClearSlotRestrictions(const FGameplayTag& SlotTag) = 0;

	//========================================
	// Cache Management
	//========================================

	/**
	 * Clear validation cache
	 */
	virtual void ClearValidationCache() = 0;

	/**
	 * Get cache statistics
	 * @return Formatted statistics string
	 */
	virtual FString GetCacheStatistics() const = 0;

	//========================================
	// Custom Rules
	//========================================

	/**
	 * Register custom validation rule
	 * @param RuleTag Rule identifier
	 * @param Priority Rule priority (higher = earlier)
	 * @param ErrorMessage Error message on failure
	 * @return True if registered
	 */
	virtual bool RegisterValidationRule(
		const FGameplayTag& RuleTag,
		int32 Priority,
		const FText& ErrorMessage) = 0;

	/**
	 * Unregister custom validation rule
	 * @param RuleTag Rule to remove
	 * @return True if removed
	 */
	virtual bool UnregisterValidationRule(const FGameplayTag& RuleTag) = 0;

	/**
	 * Enable or disable a rule
	 * @param RuleTag Rule to toggle
	 * @param bEnabled New state
	 */
	virtual void SetRuleEnabled(const FGameplayTag& RuleTag, bool bEnabled) = 0;

	/**
	 * Get all registered rules
	 * @return Array of rule tags
	 */
	virtual TArray<FGameplayTag> GetRegisteredRules() const = 0;

	//========================================
	// Diagnostics
	//========================================

	/**
	 * Get validation statistics
	 * @return Formatted statistics
	 */
	virtual FString GetValidationStatistics() const = 0;

	/**
	 * Reset statistics
	 */
	virtual void ResetStatistics() = 0;
};

