// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Components/Core/SuspenseCoreEquipmentOperationExecutor.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Services/SuspenseCoreServiceLocator.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "Interfaces/Equipment/ISuspenseEquipmentDataProvider.h"
#include "Interfaces/Equipment/ISuspenseSlotValidator.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreOperationExecutor, Log, All);

#define OPERATION_LOG(Verbosity, Format, ...) \
	UE_LOG(LogSuspenseCoreOperationExecutor, Verbosity, TEXT("%s: " Format), *GetNameSafe(this), ##__VA_ARGS__)

USuspenseCoreEquipmentOperationExecutor::USuspenseCoreEquipmentOperationExecutor()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedComponent(false);

	bIsInitialized = false;
	TotalOperationsExecuted = 0;
	SuccessfulOperations = 0;
	FailedOperations = 0;
}

void USuspenseCoreEquipmentOperationExecutor::BeginPlay()
{
	Super::BeginPlay();
	OPERATION_LOG(Log, TEXT("BeginPlay"));
}

void USuspenseCoreEquipmentOperationExecutor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OPERATION_LOG(Log, TEXT("EndPlay"));

	// Cleanup
	ServiceLocator.Reset();
	EventBus.Reset();
	bIsInitialized = false;

	Super::EndPlay(EndPlayReason);
}

bool USuspenseCoreEquipmentOperationExecutor::Initialize(USuspenseCoreServiceLocator* InServiceLocator)
{
	if (!InServiceLocator)
	{
		OPERATION_LOG(Error, TEXT("Initialize: Invalid ServiceLocator"));
		return false;
	}

	ServiceLocator = InServiceLocator;

	// Get EventBus from ServiceLocator
	EventBus = ServiceLocator->GetService<USuspenseCoreEventBus>();
	if (!EventBus.IsValid())
	{
		OPERATION_LOG(Warning, TEXT("Initialize: EventBus not found in ServiceLocator"));
	}

	bIsInitialized = true;
	OPERATION_LOG(Log, TEXT("Initialize: Success"));
	return true;
}

bool USuspenseCoreEquipmentOperationExecutor::BuildPlan(
	const FEquipmentOperationRequest& Request,
	FSuspenseCoreTransactionPlan& OutPlan,
	FText& OutError) const
{
	FScopeLock Lock(&OperationCriticalSection);

	if (!bIsInitialized)
	{
		OutError = FText::FromString(TEXT("OperationExecutor not initialized"));
		return false;
	}

	// Create new plan
	OutPlan = FSuspenseCoreTransactionPlan::Create();
	OutPlan.DebugLabel = FString::Printf(TEXT("Operation_%s"), *Request.OperationType.ToString());

	// Build plan based on operation type
	switch (Request.OperationType)
	{
	case EEquipmentOperationType::Equip:
		BuildEquipPlan(Request, OutPlan);
		break;
	case EEquipmentOperationType::Unequip:
		BuildUnequipPlan(Request, OutPlan);
		break;
	case EEquipmentOperationType::Swap:
		BuildSwapPlan(Request, OutPlan);
		break;
	case EEquipmentOperationType::Move:
		BuildMovePlan(Request, OutPlan);
		break;
	default:
		OutError = FText::FromString(TEXT("Unsupported operation type"));
		return false;
	}

	OPERATION_LOG(Verbose, TEXT("BuildPlan: Created plan with %d steps"), OutPlan.Num());
	return OutPlan.IsValid();
}

bool USuspenseCoreEquipmentOperationExecutor::ValidatePlan(
	const FSuspenseCoreTransactionPlan& Plan,
	FText& OutError) const
{
	FScopeLock Lock(&OperationCriticalSection);

	if (!Plan.IsValid())
	{
		OutError = FText::FromString(TEXT("Invalid plan"));
		return false;
	}

	// Validate each step
	for (const FSuspenseCoreTransactionPlanStep& Step : Plan.Steps)
	{
		FText StepError;
		FSlotValidationResult ValidationResult = ValidateOperation(Step.Request);
		if (!ValidationResult.bIsValid)
		{
			OutError = FText::FromString(FString::Printf(
				TEXT("Step validation failed: %s"),
				*ValidationResult.ErrorMessage.ToString()));
			return false;
		}
	}

	return true;
}

bool USuspenseCoreEquipmentOperationExecutor::ExecutePlan(
	const FSuspenseCoreTransactionPlan& Plan,
	FEquipmentOperationResult& OutResult)
{
	FScopeLock Lock(&OperationCriticalSection);

	if (!bIsInitialized)
	{
		OutResult.bSuccess = false;
		OutResult.ErrorMessage = FText::FromString(TEXT("OperationExecutor not initialized"));
		return false;
	}

	// Validate plan first
	FText ValidationError;
	if (!ValidatePlan(Plan, ValidationError))
	{
		OutResult.bSuccess = false;
		OutResult.ErrorMessage = ValidationError;
		FailedOperations++;
		return false;
	}

	// Execute plan steps
	OPERATION_LOG(Log, TEXT("ExecutePlan: Executing %d steps"), Plan.Num());

	// Publish operation started event
	if (Plan.Steps.Num() > 0)
	{
		PublishOperationStarted(Plan.Steps[0].Request);
	}

	// Execute each step
	for (const FSuspenseCoreTransactionPlanStep& Step : Plan.Steps)
	{
		// TODO: Execute step through data service
		OPERATION_LOG(Verbose, TEXT("ExecutePlan: Executing step - %s"), *Step.Description);
	}

	// Success
	OutResult.bSuccess = true;
	OutResult.OperationId = Plan.PlanId;
	TotalOperationsExecuted++;
	SuccessfulOperations++;

	// Publish operation completed event
	PublishOperationCompleted(OutResult);

	OPERATION_LOG(Log, TEXT("ExecutePlan: Success"));
	return true;
}

FEquipmentOperationResult USuspenseCoreEquipmentOperationExecutor::ExecuteOperation(
	const FEquipmentOperationRequest& Request)
{
	FEquipmentOperationResult Result;

	// Build plan
	FSuspenseCoreTransactionPlan Plan;
	FText PlanError;
	if (!BuildPlan(Request, Plan, PlanError))
	{
		Result.bSuccess = false;
		Result.ErrorMessage = PlanError;
		FailedOperations++;
		PublishOperationFailed(Request, PlanError);
		return Result;
	}

	// Execute plan
	if (!ExecutePlan(Plan, Result))
	{
		PublishOperationFailed(Request, Result.ErrorMessage);
	}

	return Result;
}

FSlotValidationResult USuspenseCoreEquipmentOperationExecutor::ValidateOperation(
	const FEquipmentOperationRequest& Request) const
{
	FSlotValidationResult Result;

	switch (Request.OperationType)
	{
	case EEquipmentOperationType::Equip:
		return ValidateEquipRequest(Request);
	case EEquipmentOperationType::Unequip:
		return ValidateUnequipRequest(Request);
	case EEquipmentOperationType::Swap:
		return ValidateSwapRequest(Request);
	default:
		Result.bIsValid = false;
		Result.ErrorMessage = FText::FromString(TEXT("Unsupported operation type"));
		return Result;
	}
}

void USuspenseCoreEquipmentOperationExecutor::ResetStatistics()
{
	TotalOperationsExecuted = 0;
	SuccessfulOperations = 0;
	FailedOperations = 0;
	OPERATION_LOG(Log, TEXT("ResetStatistics"));
}

void USuspenseCoreEquipmentOperationExecutor::PublishOperationStarted(
	const FEquipmentOperationRequest& Request)
{
	if (!EventBus.IsValid())
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
	EventData.SetString(TEXT("OperationType"), Request.OperationType.ToString());
	EventData.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.Equipment.Operation.Started"))));

	EventBus->Publish(
		FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.Equipment.Operation.Started"))),
		EventData);

	OPERATION_LOG(Verbose, TEXT("PublishOperationStarted: %s"), *Request.OperationType.ToString());
}

void USuspenseCoreEquipmentOperationExecutor::PublishOperationCompleted(
	const FEquipmentOperationResult& Result)
{
	if (!EventBus.IsValid())
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
	EventData.SetBool(TEXT("Success"), Result.bSuccess);

	EventBus->Publish(
		FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.Equipment.Operation.Completed"))),
		EventData);

	OPERATION_LOG(Verbose, TEXT("PublishOperationCompleted: Success=%d"), Result.bSuccess);
}

void USuspenseCoreEquipmentOperationExecutor::PublishOperationFailed(
	const FEquipmentOperationRequest& Request,
	const FText& Reason)
{
	if (!EventBus.IsValid())
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
	EventData.SetString(TEXT("OperationType"), Request.OperationType.ToString());
	EventData.SetString(TEXT("Reason"), Reason.ToString());

	EventBus->Publish(
		FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.Equipment.Operation.Failed"))),
		EventData);

	OPERATION_LOG(Warning, TEXT("PublishOperationFailed: %s - %s"),
		*Request.OperationType.ToString(),
		*Reason.ToString());
}

void USuspenseCoreEquipmentOperationExecutor::BuildEquipPlan(
	const FEquipmentOperationRequest& Request,
	FSuspenseCoreTransactionPlan& OutPlan) const
{
	// Create equip step
	FSuspenseCoreTransactionPlanStep Step(Request, TEXT("Equip item to slot"));
	Step.StepPriority = 100;
	OutPlan.Add(Step);
}

void USuspenseCoreEquipmentOperationExecutor::BuildUnequipPlan(
	const FEquipmentOperationRequest& Request,
	FSuspenseCoreTransactionPlan& OutPlan) const
{
	// Create unequip step
	FSuspenseCoreTransactionPlanStep Step(Request, TEXT("Unequip item from slot"));
	Step.StepPriority = 100;
	OutPlan.Add(Step);
}

void USuspenseCoreEquipmentOperationExecutor::BuildSwapPlan(
	const FEquipmentOperationRequest& Request,
	FSuspenseCoreTransactionPlan& OutPlan) const
{
	// Create swap step
	FSuspenseCoreTransactionPlanStep Step(Request, TEXT("Swap items between slots"));
	Step.StepPriority = 100;
	OutPlan.Add(Step);
}

void USuspenseCoreEquipmentOperationExecutor::BuildMovePlan(
	const FEquipmentOperationRequest& Request,
	FSuspenseCoreTransactionPlan& OutPlan) const
{
	// Create move step
	FSuspenseCoreTransactionPlanStep Step(Request, TEXT("Move item to new slot"));
	Step.StepPriority = 100;
	OutPlan.Add(Step);
}

FSlotValidationResult USuspenseCoreEquipmentOperationExecutor::ValidateEquipRequest(
	const FEquipmentOperationRequest& Request) const
{
	FSlotValidationResult Result;
	Result.bIsValid = true;

	// Get slot validator if available
	ISuspenseSlotValidator* Validator = GetSlotValidator();
	if (Validator)
	{
		// TODO: Use validator to check slot compatibility
	}

	return Result;
}

FSlotValidationResult USuspenseCoreEquipmentOperationExecutor::ValidateUnequipRequest(
	const FEquipmentOperationRequest& Request) const
{
	FSlotValidationResult Result;
	Result.bIsValid = true;
	return Result;
}

FSlotValidationResult USuspenseCoreEquipmentOperationExecutor::ValidateSwapRequest(
	const FEquipmentOperationRequest& Request) const
{
	FSlotValidationResult Result;
	Result.bIsValid = true;
	return Result;
}

ISuspenseEquipmentDataProvider* USuspenseCoreEquipmentOperationExecutor::GetDataProvider() const
{
	if (!ServiceLocator.IsValid())
	{
		return nullptr;
	}

	// TODO: Get data provider from ServiceLocator
	return nullptr;
}

ISuspenseSlotValidator* USuspenseCoreEquipmentOperationExecutor::GetSlotValidator() const
{
	if (!ServiceLocator.IsValid())
	{
		return nullptr;
	}

	// TODO: Get slot validator from ServiceLocator
	return nullptr;
}
