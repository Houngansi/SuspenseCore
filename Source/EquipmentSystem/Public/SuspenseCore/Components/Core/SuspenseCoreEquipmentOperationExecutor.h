// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Interfaces/Equipment/ISuspenseEquipmentOperations.h"
#include "Types/Equipment/SuspenseEquipmentTypes.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "SuspenseCoreEquipmentOperationExecutor.generated.h"

// Forward declarations
class USuspenseCoreEventBus;
class USuspenseCoreServiceLocator;
class ISuspenseEquipmentDataProvider;
class ISuspenseSlotValidator;
struct FEquipmentOperationRequest;
struct FEquipmentOperationResult;
struct FSuspenseCoreEventData;

/**
 * FSuspenseCoreTransactionPlanStep
 *
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
	{}
};

/**
 * FSuspenseCoreTransactionPlan
 *
 * A deterministic sequence of equipment requests to be executed inside a single transaction.
 * Pure planning artifact with NO side-effects.
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

	/** Ordered steps to execute */
	UPROPERTY(BlueprintReadWrite, Category="Plan")
	TArray<FSuspenseCoreTransactionPlanStep> Steps;

	/** Short label for logs/metrics */
	UPROPERTY(BlueprintReadWrite, Category="Plan")
	FString DebugLabel;

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
};

/**
 * USuspenseCoreEquipmentOperationExecutor
 *
 * Executes equipment operations with validation and event-driven architecture.
 *
 * Architecture:
 * - EventBus: Publishes operation lifecycle events
 * - ServiceLocator: Dependency injection for validation/data services
 * - GameplayTags: Operation categorization and state identification
 * - Pure Planning: Builds deterministic plans without side effects
 *
 * Responsibilities:
 * - Build transaction plans from operation requests
 * - Validate operations against business rules
 * - Execute atomic operations with rollback support
 * - Publish operation events through EventBus
 */
UCLASS(ClassGroup=(SuspenseCore), meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentOperationExecutor : public UActorComponent
{
	GENERATED_BODY()

public:
	USuspenseCoreEquipmentOperationExecutor();
	virtual ~USuspenseCoreEquipmentOperationExecutor() = default;

	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

	//================================================
	// Initialization
	//================================================

	/**
	 * Initialize executor with dependencies via ServiceLocator
	 * @param InServiceLocator ServiceLocator for dependency injection
	 * @return True if initialized successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Operations")
	bool Initialize(USuspenseCoreServiceLocator* InServiceLocator);

	/**
	 * Check if executor is properly initialized
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Equipment|Operations")
	bool IsInitialized() const { return bIsInitialized; }

	//================================================
	// Planning API (Core Functionality)
	//================================================

	/**
	 * Build a deterministic transaction plan for a given operation request.
	 * No side-effects. Pure function.
	 *
	 * @param Request The operation request to plan
	 * @param OutPlan The generated plan (empty on failure)
	 * @param OutError Error message if planning fails
	 * @return True if plan was successfully built
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Operations")
	bool BuildPlan(const FEquipmentOperationRequest& Request, FSuspenseCoreTransactionPlan& OutPlan, FText& OutError) const;

	/**
	 * Validate a plan step-by-step.
	 * No side-effects. Pure function.
	 *
	 * @param Plan The plan to validate
	 * @param OutError Error message if validation fails
	 * @return True if all steps are valid
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Operations")
	bool ValidatePlan(const FSuspenseCoreTransactionPlan& Plan, FText& OutError) const;

	/**
	 * Execute a validated transaction plan
	 * Publishes events through EventBus
	 *
	 * @param Plan The plan to execute
	 * @param OutResult The execution result
	 * @return True if execution succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Operations")
	bool ExecutePlan(const FSuspenseCoreTransactionPlan& Plan, FEquipmentOperationResult& OutResult);

	//================================================
	// High-Level Operations
	//================================================

	/**
	 * Execute equipment operation (plan + validate + execute)
	 * @param Request The operation request
	 * @return Operation result
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Operations")
	FEquipmentOperationResult ExecuteOperation(const FEquipmentOperationRequest& Request);

	/**
	 * Validate operation without executing
	 * @param Request The operation request
	 * @return Validation result
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Operations")
	FSlotValidationResult ValidateOperation(const FEquipmentOperationRequest& Request) const;

	//================================================
	// Statistics
	//================================================

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Equipment|Operations|Stats")
	int32 GetTotalOperationsExecuted() const { return TotalOperationsExecuted; }

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Equipment|Operations|Stats")
	int32 GetSuccessfulOperations() const { return SuccessfulOperations; }

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Equipment|Operations|Stats")
	int32 GetFailedOperations() const { return FailedOperations; }

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Operations|Stats")
	void ResetStatistics();

protected:
	//================================================
	// Event Publishing
	//================================================

	/** Publish operation started event */
	void PublishOperationStarted(const FEquipmentOperationRequest& Request);

	/** Publish operation completed event */
	void PublishOperationCompleted(const FEquipmentOperationResult& Result);

	/** Publish operation failed event */
	void PublishOperationFailed(const FEquipmentOperationRequest& Request, const FText& Reason);

	//================================================
	// Plan Building (Pure Methods)
	//================================================

	void BuildEquipPlan(const FEquipmentOperationRequest& Request, FSuspenseCoreTransactionPlan& OutPlan) const;
	void BuildUnequipPlan(const FEquipmentOperationRequest& Request, FSuspenseCoreTransactionPlan& OutPlan) const;
	void BuildSwapPlan(const FEquipmentOperationRequest& Request, FSuspenseCoreTransactionPlan& OutPlan) const;
	void BuildMovePlan(const FEquipmentOperationRequest& Request, FSuspenseCoreTransactionPlan& OutPlan) const;

	//================================================
	// Validation (Pure Methods)
	//================================================

	FSlotValidationResult ValidateEquipRequest(const FEquipmentOperationRequest& Request) const;
	FSlotValidationResult ValidateUnequipRequest(const FEquipmentOperationRequest& Request) const;
	FSlotValidationResult ValidateSwapRequest(const FEquipmentOperationRequest& Request) const;

	//================================================
	// Helper Methods
	//================================================

	/** Get data provider from ServiceLocator */
	ISuspenseEquipmentDataProvider* GetDataProvider() const;

	/** Get slot validator from ServiceLocator */
	ISuspenseSlotValidator* GetSlotValidator() const;

private:
	//================================================
	// Dependencies (Injected)
	//================================================

	/** ServiceLocator for dependency injection */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreServiceLocator> ServiceLocator;

	/** EventBus for event publishing */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> EventBus;

	//================================================
	// State
	//================================================

	/** Initialization flag */
	UPROPERTY()
	bool bIsInitialized;

	//================================================
	// Statistics
	//================================================

	/** Total operations executed */
	UPROPERTY()
	int32 TotalOperationsExecuted;

	/** Successful operations */
	UPROPERTY()
	int32 SuccessfulOperations;

	/** Failed operations */
	UPROPERTY()
	int32 FailedOperations;

	//================================================
	// Thread Safety
	//================================================

	/** Critical section for thread-safe operations */
	mutable FCriticalSection OperationCriticalSection;
};
