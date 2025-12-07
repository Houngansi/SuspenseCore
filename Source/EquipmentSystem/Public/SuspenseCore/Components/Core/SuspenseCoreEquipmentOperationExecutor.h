// SuspenseCoreEquipmentOperationExecutor.h
// Copyright Medcom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/Equipment/ISuspenseCoreEquipmentOperations.h"
#include "Interfaces/Equipment/ISuspenseCoreEquipmentDataProvider.h"
#include "Interfaces/Equipment/ISuspenseSlotValidator.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Types/Equipment/SuspenseCoreEquipmentTypes.h"
#include "Types/Loadout/SuspenseLoadoutSettings.h" // For updated ESuspenseCoreEquipmentSlotType (Primary/Secondary/Holster/Scabbard)
#include "GameplayTagContainer.h"
#include <atomic>
#include "SuspenseCoreEquipmentOperationExecutor.generated.h"

/**
 * One atomic step inside a transaction plan.
 * Pure data structure, no side effects.
 */
USTRUCT(BlueprintType)
struct FSuspenseCoreTransactionPlanStep
{
	GENERATED_BODY()

	/** Concrete request to execute as a step */
	UPROPERTY(BlueprintReadWrite, Category="Plan")
	FEquipmentOperationRequest Request;

	/** Human-readable description for logs/metrics */
	UPROPERTY(BlueprintReadWrite, Category="Plan")
	FString Description;

	/** Expected validation result (can be pre-cached) */
	UPROPERTY(BlueprintReadWrite, Category="Plan")
	FSlotValidationResult ExpectedValidation;

	/** Step priority within the plan */
	UPROPERTY(BlueprintReadWrite, Category="Plan")
	int32 StepPriority = 0;

	/** Is this step reversible */
	UPROPERTY(BlueprintReadWrite, Category="Plan")
	bool bReversible = true;

	FSuspenseCoreTransactionPlanStep() = default;

	explicit FSuspenseCoreTransactionPlanStep(const FEquipmentOperationRequest& InReq, const FString& InDesc = FString())
		: Request(InReq)
		, Description(InDesc)
		, StepPriority(0)
		, bReversible(true)
	{
		// Ensure step has valid ID
		if (!Request.OperationId.IsValid())
		{
			Request.OperationId = FGuid::NewGuid();
		}
	}
};

/**
 * A deterministic sequence of equipment requests to be executed inside a single transaction.
 * Pure planning artifact with NO side-effects.
 *
 * Design Philosophy:
 * Plans are immutable once created. They represent the "intent" of an operation
 * without any actual state changes. This allows validation, caching, and safe
 * concurrent planning without locks on data.
 */
USTRUCT(BlueprintType)
struct FSuspenseCoreTransactionPlan
{
	GENERATED_BODY()

	/** Stable identifier for cross-system correlation */
	UPROPERTY(BlueprintReadOnly, Category="Plan")
	FGuid PlanId;

	/** If true, all steps must succeed; otherwise rollback entire plan */
	UPROPERTY(BlueprintReadWrite, Category="Plan")
	bool bAtomic = true;

	/** If true, service is allowed to create a savepoint before execution */
	UPROPERTY(BlueprintReadWrite, Category="Plan")
	bool bReversible = true;

	/** If true, plan can be safely retried on network failures */
	UPROPERTY(BlueprintReadWrite, Category="Plan")
	bool bIdempotent = false;

	/** Ordered steps to execute */
	UPROPERTY(BlueprintReadWrite, Category="Plan")
	TArray<FSuspenseCoreTransactionPlanStep> Steps;

	/** Short label for logs/metrics */
	UPROPERTY(BlueprintReadWrite, Category="Plan")
	FString DebugLabel;

	/** Estimated execution time in milliseconds */
	UPROPERTY(BlueprintReadWrite, Category="Plan")
	float EstimatedExecutionTimeMs = 0.0f;

	/** Metadata for extended context */
	UPROPERTY(BlueprintReadWrite, Category="Plan")
	TMap<FString, FString> Metadata;

	/** Static factory method to create plan with generated ID */
	static FSuspenseCoreTransactionPlan Create()
	{
		FSuspenseCoreTransactionPlan Plan;
		Plan.PlanId = FGuid::NewGuid();
		return Plan;
	}

	int32 Num() const { return Steps.Num(); }
	bool IsValid() const { return PlanId.IsValid() && Steps.Num() > 0; }
	void Add(const FSuspenseCoreTransactionPlanStep& Step) { Steps.Add(Step); }
	void Clear() { Steps.Empty(); Metadata.Empty(); }

	/** Get total priority score for queue sorting */
	int32 GetTotalPriority() const
	{
		int32 Total = 0;
		for (const FSuspenseCoreTransactionPlanStep& Step : Steps)
		{
			Total += Step.StepPriority;
		}
		return Total;
	}
};

/**
 * Equipment Operation Executor Component
 *
 * Philosophy: Pure deterministic plan builder for equipment operations.
 * No side effects, no state mutations, no data modifications.
 *
 * Key Design Principles:
 * - Stateless plan generation (read-only data access)
 * - Complete validation without execution
 * - Deterministic planning for all operations
 * - Thread-safe planning operations
 * - NO history tracking (moved to service layer)
 * - NO direct data modifications
 *
 * SRP Compliance:
 * This component is ONLY responsible for:
 * 1) Building deterministic plans from high-level requests
 * 2) Validating plans against business rules
 * 3) Providing operation metadata and estimates
 */
UCLASS(ClassGroup=(Equipment), meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentOperationExecutor
	: public UActorComponent
	, public ISuspenseCoreEquipmentOperations
{
	GENERATED_BODY()

public:
	USuspenseCoreEquipmentOperationExecutor();
	virtual ~USuspenseCoreEquipmentOperationExecutor() = default;

	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

	// ================================
	// Planning API (Core Functionality)
	// ================================

	/**
	 * Build a deterministic transaction plan for a given high-level request.
	 * No side-effects. Pure function.
	 *
	 * @param Request The operation request to plan
	 * @param OutPlan The generated plan (empty on failure)
	 * @param OutError Error message if planning fails
	 * @return True if plan was successfully built
	 */
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Operations|Planning")
	bool BuildPlan(const FEquipmentOperationRequest& Request, FSuspenseCoreTransactionPlan& OutPlan, FText& OutError) const;

	/**
	 * Validate a plan step-by-step via slot validator.
	 * No side-effects. Pure function.
	 *
	 * @param Plan The plan to validate
	 * @param OutError Error message if validation fails
	 * @return True if all steps are valid
	 */
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Operations|Planning")
	bool ValidatePlan(const FSuspenseCoreTransactionPlan& Plan, FText& OutError) const;

	/**
	 * Estimate execution time for a plan (for queue prioritization).
	 *
	 * @param Plan The plan to estimate
	 * @return Estimated time in milliseconds
	 */
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Operations|Planning", BlueprintPure)
	float EstimatePlanExecutionTime(const FSuspenseCoreTransactionPlan& Plan) const;

	/**
	 * Check if a plan is idempotent (safe to retry).
	 *
	 * @param Plan The plan to check
	 * @return True if plan can be safely retried
	 */
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Operations|Planning", BlueprintPure)
	bool IsPlanIdempotent(const FSuspenseCoreTransactionPlan& Plan) const;

	// =====================================================
	// ISuspenseCoreEquipmentOperations Implementation (Compatibility)
	// =====================================================

	virtual FEquipmentOperationResult ExecuteOperation(const FEquipmentOperationRequest& Request) override;
	virtual FSlotValidationResult ValidateOperation(const FEquipmentOperationRequest& Request) const override;
	virtual FEquipmentOperationResult EquipItem(const FSuspenseInventoryItemInstance& ItemInstance, int32 SlotIndex) override;
	virtual FEquipmentOperationResult UnequipItem(int32 SlotIndex) override;
	virtual FEquipmentOperationResult SwapItems(int32 SlotIndexA, int32 SlotIndexB) override;
	virtual FEquipmentOperationResult MoveItem(int32 SourceSlot, int32 TargetSlot) override;
	virtual FEquipmentOperationResult DropItem(int32 SlotIndex) override;
	virtual FEquipmentOperationResult QuickSwitchWeapon() override;

	// History methods - deprecated, return empty
	virtual TArray<FEquipmentOperationResult> GetOperationHistory(int32 MaxCount = 10) const override;
	virtual bool CanUndoLastOperation() const override;
	virtual FEquipmentOperationResult UndoLastOperation() override;

	// ==============
	// Configuration
	// ==============

	/**
	 * Initialize executor with dependencies.
	 * @param InDataProvider Data provider interface (required)
	 * @param InValidator Slot validator interface (optional)
	 * @return True if initialized successfully
	 */
	UFUNCTION(BlueprintCallable)
	bool Initialize(TScriptInterface<ISuspenseCoreEquipmentDataProvider> InDataProvider,
					TScriptInterface<ISuspenseSlotValidator> InValidator);

	/** Check if executor is properly initialized */
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Operations", BlueprintPure)
	bool IsInitialized() const;

	/** Toggle validation requirement for plans */
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Operations")
	void SetValidationRequired(bool bRequired) { bRequireValidation = bRequired; }

	/** Get whether validation is required */
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Operations", BlueprintPure)
	bool IsValidationRequired() const { return bRequireValidation; }

	// =========================
	// Statistics (Read-Only API)
	// =========================

	/** Get total plans built */
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Operations|Stats", BlueprintPure)
	int32 GetTotalPlansBuilt() const { return TotalPlansBuilt; }

	/** Get successful validations count */
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Operations|Stats", BlueprintPure)
	int32 GetSuccessfulValidations() const { return SuccessfulValidations; }

	/** Get failed validations count */
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Operations|Stats", BlueprintPure)
	int32 GetFailedValidations() const { return FailedValidations; }

	/** Get average plan size */
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Operations|Stats", BlueprintPure)
	float GetAveragePlanSize() const { return AveragePlanSize; }

	/** Reset statistics */
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Operations|Stats")
	void ResetStatistics();

	/** Get statistics as formatted string */
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Operations|Stats", BlueprintPure)
	FString GetStatistics() const;

	// =====================================================
	// Pre-Validation API (для Bridge и внешних систем)
	// =====================================================

	/**
	 * Check if item can be equipped to specific slot WITHOUT executing operation.
	 * Used by Bridge for pre-validation before removing item from inventory.
	 *
	 * This is a pure validation method with NO side effects - safe to call
	 * multiple times and from any context.
	 *
	 * @param ItemInstance The item to validate
	 * @param TargetSlotIndex The slot to validate against
	 * @return Validation result with detailed error info if incompatible
	 */
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Operations|Validation")
	FSlotValidationResult CanEquipItemToSlot(
		const FSuspenseInventoryItemInstance& ItemInstance,
		int32 TargetSlotIndex) const;

protected:
	// ==========================================
	// Plan Building Methods (Pure, no side-effects)
	// ==========================================

	/** Expand high-level operations into atomic steps */
	void Expand_Equip(const FEquipmentOperationRequest& In, FSuspenseCoreTransactionPlan& Out) const;
	void Expand_Unequip(const FEquipmentOperationRequest& In, FSuspenseCoreTransactionPlan& Out) const;
	void Expand_Move(const FEquipmentOperationRequest& In, FSuspenseCoreTransactionPlan& Out) const;
	void Expand_Drop(const FEquipmentOperationRequest& In, FSuspenseCoreTransactionPlan& Out) const;
	void Expand_Swap(const FEquipmentOperationRequest& In, FSuspenseCoreTransactionPlan& Out) const;
	void Expand_QuickSwitch(const FEquipmentOperationRequest& In, FSuspenseCoreTransactionPlan& Out) const;
	void Expand_Transfer(const FEquipmentOperationRequest& In, FSuspenseCoreTransactionPlan& Out) const;
	void Expand_Reload(const FEquipmentOperationRequest& In, FSuspenseCoreTransactionPlan& Out) const;
	void Expand_Repair(const FEquipmentOperationRequest& In, FSuspenseCoreTransactionPlan& Out) const;
	void Expand_Upgrade(const FEquipmentOperationRequest& In, FSuspenseCoreTransactionPlan& Out) const;
	void Expand_Modify(const FEquipmentOperationRequest& In, FSuspenseCoreTransactionPlan& Out) const;
	void Expand_Combine(const FEquipmentOperationRequest& In, FSuspenseCoreTransactionPlan& Out) const;
	void Expand_Split(const FEquipmentOperationRequest& In, FSuspenseCoreTransactionPlan& Out) const;

	/** Validate a single plan step */
	bool ValidateStep(const FSuspenseCoreTransactionPlanStep& Step, FText& OutError) const;

	// ==============================
	// Validation Methods (Pure APIs)
	// ==============================

	FSlotValidationResult ValidateEquip(const FEquipmentOperationRequest& Request) const;
	FSlotValidationResult ValidateUnequip(const FEquipmentOperationRequest& Request) const;
	FSlotValidationResult ValidateSwap(const FEquipmentOperationRequest& Request) const;
	FSlotValidationResult ValidateMove(const FEquipmentOperationRequest& Request) const;
	FSlotValidationResult ValidateDrop(const FEquipmentOperationRequest& Request) const;
	FSlotValidationResult ValidateQuickSwitch(const FEquipmentOperationRequest& Request) const;

	// ==============
	// Helper Methods
	// ==============

	/** Generate unique operation ID */
	FGuid GenerateOperationId() const;

	/** Find best slot for item (read-only query) */
	int32 FindBestSlotForItem(const FSuspenseInventoryItemInstance& ItemInstance) const;

	/** Check if slot is weapon slot (read-only query) */
	bool IsWeaponSlot(int32 SlotIndex) const;

	/** Check if slot is for firearms (not melee) */
	bool IsFirearmSlot(int32 SlotIndex) const;

	/** Check if slot is for melee weapons */
	bool IsMeleeWeaponSlot(int32 SlotIndex) const;

	/** Get weapon slot type for a given slot index */
	EEquipmentSlotType GetWeaponSlotType(int32 SlotIndex) const;

	/** Get priority for weapon slot type (lower = higher priority) */
	int32 GetWeaponSlotPriority(EEquipmentSlotType SlotType) const;

	/** Get currently active weapon slot (read-only query) */
	int32 GetCurrentActiveWeaponSlot() const;

	/** Find next available weapon slot (read-only query) */
	int32 FindNextWeaponSlot(int32 CurrentSlot) const;

private:
	// =============
	// Dependencies
	// =============

	/** Data provider interface (read-only access) */
	UPROPERTY()
	TScriptInterface<ISuspenseCoreEquipmentDataProvider> DataProvider;

	/** Slot validator interface (optional) */
	UPROPERTY()
	TScriptInterface<ISuspenseSlotValidator> SlotValidator;

	// ============
	// Statistics
	// ============

	/** Total plans built */
	mutable std::atomic<int32> TotalPlansBuilt{0};

	/** Successful validations */
	mutable std::atomic<int32> SuccessfulValidations{0};

	/** Failed validations */
	mutable std::atomic<int32> FailedValidations{0};

	/** Average plan size */
	mutable std::atomic<float> AveragePlanSize{0.0f};

	// =============
	// Configuration
	// =============

	/** Require validation before execution */
	UPROPERTY(EditDefaultsOnly, Category="Configuration")
	bool bRequireValidation = true;

	/** Enable detailed logging */
	UPROPERTY(EditDefaultsOnly, Category="Configuration")
	bool bEnableDetailedLogging = false;

	/** Maximum plan complexity (steps) */
	UPROPERTY(EditDefaultsOnly, Category="Configuration")
	int32 MaxPlanComplexity = 20;

	/** Default operation timeout in ms */
	UPROPERTY(EditDefaultsOnly, Category="Configuration")
	float DefaultOperationTimeoutMs = 100.0f;

	// =============
	// Thread Safety
	// =============

	/** Critical section for thread-safe planning */
	mutable FCriticalSection PlanningCriticalSection;
};
