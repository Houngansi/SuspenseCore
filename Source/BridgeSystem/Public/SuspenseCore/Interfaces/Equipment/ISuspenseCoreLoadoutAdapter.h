// ISuspenseCoreLoadoutAdapter.h
// SuspenseCore Loadout Adapter Interface
// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/Equipment/SuspenseCoreEquipmentTypes.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreLoadoutSettings.h"
#include "ISuspenseCoreLoadoutAdapter.generated.h"

// Forward declarations
class USuspenseCoreEventBus;
struct FEquipmentStateSnapshot;
struct FEquipmentOperationRequest;

/**
 * Loadout application strategy
 */
UENUM(BlueprintType)
enum class ESuspenseCoreLoadoutStrategy : uint8
{
	/** Replace all equipment with loadout items */
	Replace = 0,

	/** Merge loadout with current equipment (fill empty slots only) */
	Merge,

	/** Apply only specific slots from loadout */
	Selective,

	/** Validate only, don't apply */
	ValidateOnly
};

/**
 * SuspenseCore Loadout Application Result
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreLoadoutApplicationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FName LoadoutId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 ItemsEquipped = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 ItemsFailed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	TArray<FText> Errors;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	TArray<FText> Warnings;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	float ApplicationTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	ESuspenseCoreLoadoutStrategy StrategyUsed = ESuspenseCoreLoadoutStrategy::Replace;

	static FSuspenseCoreLoadoutApplicationResult Success(const FName& InLoadoutId, int32 Equipped)
	{
		FSuspenseCoreLoadoutApplicationResult Result;
		Result.bSuccess = true;
		Result.LoadoutId = InLoadoutId;
		Result.ItemsEquipped = Equipped;
		return Result;
	}

	static FSuspenseCoreLoadoutApplicationResult Failure(const FName& InLoadoutId, const FText& Error)
	{
		FSuspenseCoreLoadoutApplicationResult Result;
		Result.bSuccess = false;
		Result.LoadoutId = InLoadoutId;
		Result.Errors.Add(Error);
		return Result;
	}

	void AddError(const FText& Error) { Errors.Add(Error); }
	void AddWarning(const FText& Warning) { Warnings.Add(Warning); }
	bool HasErrors() const { return Errors.Num() > 0; }
	bool HasWarnings() const { return Warnings.Num() > 0; }
};

/**
 * SuspenseCore Loadout Configuration
 *
 * Contains the loadout definition for application.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreLoadoutConfiguration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FName LoadoutId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FGameplayTag LoadoutType;

	/** Map of slot index to item ID */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	TMap<int32, FName> SlotToItem;

	/** Map of slot type to item ID (alternative) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	TMap<EEquipmentSlotType, FName> SlotTypeToItem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FGameplayTagContainer RequiredTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FGameplayTag CharacterClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	int32 MinLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FDateTime CreatedTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FDateTime ModifiedTime;

	bool IsValid() const { return !LoadoutId.IsNone(); }
	int32 GetItemCount() const { return SlotToItem.Num() + SlotTypeToItem.Num(); }
};

/**
 * Loadout Validation Options
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreLoadoutAdapterOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
	bool bCheckCharacterClass = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
	bool bCheckCharacterLevel = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
	bool bCheckInventorySpace = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
	bool bCheckItemAvailability = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
	bool bCheckSlotCompatibility = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
	bool bCheckWeightLimits = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
	bool bCheckConflictingItems = true;

	static FSuspenseCoreLoadoutAdapterOptions Default()
	{
		return FSuspenseCoreLoadoutAdapterOptions();
	}

	static FSuspenseCoreLoadoutAdapterOptions Minimal()
	{
		FSuspenseCoreLoadoutAdapterOptions Options;
		Options.bCheckCharacterClass = false;
		Options.bCheckCharacterLevel = false;
		Options.bCheckInventorySpace = false;
		Options.bCheckWeightLimits = false;
		return Options;
	}
};

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreLoadoutAdapter : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISuspenseCoreLoadoutAdapter
 *
 * Adapter interface for loadout system integration.
 * Translates loadout configurations to equipment operations.
 *
 * Architecture:
 * - Validates loadout compatibility before application
 * - Converts loadout data to equipment operations
 * - Supports multiple application strategies
 * - Integrates with EventBus for notifications
 *
 * EventBus Events Published:
 * - SuspenseCore.Event.Loadout.ApplicationStarted
 * - SuspenseCore.Event.Loadout.ApplicationCompleted
 * - SuspenseCore.Event.Loadout.ApplicationFailed
 * - SuspenseCore.Event.Loadout.Saved
 * - SuspenseCore.Event.Loadout.Validated
 */
class BRIDGESYSTEM_API ISuspenseCoreLoadoutAdapter
{
	GENERATED_BODY()

public:
	//========================================
	// Core Loadout Operations
	//========================================

	/**
	 * Apply loadout configuration to equipment
	 * @param LoadoutId Loadout to apply
	 * @param bForce Force application even if validation has warnings
	 * @return Application result with details
	 */
	virtual FSuspenseCoreLoadoutApplicationResult ApplyLoadout(
		const FName& LoadoutId,
		bool bForce = false) = 0;

	/**
	 * Apply loadout with specific strategy
	 * @param LoadoutId Loadout to apply
	 * @param Strategy Application strategy
	 * @return Application result
	 */
	virtual FSuspenseCoreLoadoutApplicationResult ApplyLoadoutWithStrategy(
		const FName& LoadoutId,
		ESuspenseCoreLoadoutStrategy Strategy) = 0;

	/**
	 * Apply loadout configuration directly (without lookup)
	 * @param Configuration Loadout configuration to apply
	 * @param Strategy Application strategy
	 * @return Application result
	 */
	virtual FSuspenseCoreLoadoutApplicationResult ApplyLoadoutConfiguration(
		const FSuspenseCoreLoadoutConfiguration& Configuration,
		ESuspenseCoreLoadoutStrategy Strategy = ESuspenseCoreLoadoutStrategy::Replace) = 0;

	/**
	 * Save current equipment state as loadout
	 * @param LoadoutId ID for the new loadout
	 * @return True if saved successfully
	 */
	virtual bool SaveAsLoadout(const FName& LoadoutId) = 0;

	/**
	 * Save with custom display name
	 * @param LoadoutId ID for the loadout
	 * @param DisplayName Human-readable name
	 * @return True if saved
	 */
	virtual bool SaveAsLoadoutWithName(const FName& LoadoutId, const FText& DisplayName) = 0;

	//========================================
	// Validation
	//========================================

	/**
	 * Validate loadout compatibility
	 * @param LoadoutId Loadout to validate
	 * @param OutErrors Validation errors found
	 * @return True if loadout can be applied
	 */
	virtual bool ValidateLoadout(
		const FName& LoadoutId,
		TArray<FText>& OutErrors) const = 0;

	/**
	 * Validate with specific options
	 * @param LoadoutId Loadout to validate
	 * @param Options Validation options
	 * @param OutErrors Errors found
	 * @param OutWarnings Warnings found
	 * @return True if valid (errors = 0)
	 */
	virtual bool ValidateLoadoutWithOptions(
		const FName& LoadoutId,
		const FSuspenseCoreLoadoutAdapterOptions& Options,
		TArray<FText>& OutErrors,
		TArray<FText>& OutWarnings) const = 0;

	//========================================
	// Loadout Query
	//========================================

	/**
	 * Get current loadout ID (if applied from loadout)
	 * @return Current loadout ID or NAME_None
	 */
	virtual FName GetCurrentLoadout() const = 0;

	/**
	 * Get loadout configuration by ID
	 * @param LoadoutId Loadout to get
	 * @param OutConfiguration Configuration output
	 * @return True if found
	 */
	virtual bool GetLoadoutConfiguration(
		const FName& LoadoutId,
		FSuspenseCoreLoadoutConfiguration& OutConfiguration) const = 0;

	/**
	 * Get all available loadouts for current character
	 * @return Array of loadout IDs
	 */
	virtual TArray<FName> GetAvailableLoadouts() const = 0;

	/**
	 * Get loadouts compatible with current state
	 * @return Array of compatible loadout IDs
	 */
	virtual TArray<FName> GetCompatibleLoadouts() const = 0;

	//========================================
	// Conversion
	//========================================

	/**
	 * Convert equipment state to loadout format
	 * @param State Equipment state snapshot
	 * @return Loadout configuration
	 */
	virtual FSuspenseCoreLoadoutConfiguration ConvertToLoadoutFormat(
		const FEquipmentStateSnapshot& State) const = 0;

	/**
	 * Convert loadout to equipment operations
	 * @param Configuration Loadout configuration
	 * @return Array of equipment operations to execute
	 */
	virtual TArray<FEquipmentOperationRequest> ConvertFromLoadoutFormat(
		const FSuspenseCoreLoadoutConfiguration& Configuration) const = 0;

	//========================================
	// Preview
	//========================================

	/**
	 * Get loadout preview description
	 * @param LoadoutId Loadout to preview
	 * @return Human-readable preview
	 */
	virtual FString GetLoadoutPreview(const FName& LoadoutId) const = 0;

	/**
	 * Estimate application time
	 * @param LoadoutId Loadout to estimate
	 * @return Estimated time in seconds
	 */
	virtual float EstimateApplicationTime(const FName& LoadoutId) const = 0;

	/**
	 * Get diff between current equipment and loadout
	 * @param LoadoutId Loadout to compare
	 * @param OutItemsToAdd Items that would be added
	 * @param OutItemsToRemove Items that would be removed
	 * @return True if diff computed
	 */
	virtual bool GetLoadoutDiff(
		const FName& LoadoutId,
		TArray<FName>& OutItemsToAdd,
		TArray<FName>& OutItemsToRemove) const = 0;

	//========================================
	// EventBus Integration
	//========================================

	/**
	 * Get EventBus used by this adapter
	 * @return EventBus instance
	 */
	virtual USuspenseCoreEventBus* GetEventBus() const = 0;

	/**
	 * Set EventBus for this adapter
	 * @param InEventBus EventBus to use
	 */
	virtual void SetEventBus(USuspenseCoreEventBus* InEventBus) = 0;

	//========================================
	// Status
	//========================================

	/**
	 * Check if loadout is currently being applied
	 * @return True if application in progress
	 */
	virtual bool IsApplyingLoadout() const = 0;

	/**
	 * Get last application result
	 * @return Last result (may be invalid if never applied)
	 */
	virtual FSuspenseCoreLoadoutApplicationResult GetLastApplicationResult() const = 0;

	/**
	 * Cancel ongoing loadout application
	 * @return True if cancelled
	 */
	virtual bool CancelApplication() = 0;
};

