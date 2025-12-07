// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Services/SuspenseCoreEquipmentOperationService.h"
#include "SuspenseCore/Services/SuspenseCoreServiceLocator.h"
#include "Interfaces/Equipment/ISuspenseEquipmentService.h"

DEFINE_LOG_CATEGORY(LogSuspenseCoreEquipmentService);

USuspenseCoreEquipmentOperationService::USuspenseCoreEquipmentOperationService()
	: ServiceState(EServiceLifecycleState::Uninitialized)
	, bQueueProcessingEnabled(true)
	, MaxQueueSize(100)
	, bEnableDetailedLogging(false)
	, bEnableBatching(true)
	, TotalOperationsExecuted(0)
	, TotalOperationsFailed(0)
	, TotalOperationsQueued(0)
{
}

USuspenseCoreEquipmentOperationService::~USuspenseCoreEquipmentOperationService()
{
}

//========================================
// ISuspenseEquipmentService Interface
//========================================

bool USuspenseCoreEquipmentOperationService::InitializeService(const FServiceInitParams& Params)
{
	TRACK_SERVICE_INIT();

	if (ServiceState != EServiceLifecycleState::Uninitialized)
	{
		LOG_SERVICE_WARNING(TEXT("Service already initialized"));
		return false;
	}

	ServiceState = EServiceLifecycleState::Initializing;

	// Store ServiceLocator reference
	ServiceLocator = Params.ServiceLocator;

	// Initialize dependencies
	if (!InitializeDependencies())
	{
		LOG_SERVICE_ERROR(TEXT("Failed to initialize dependencies"));
		ServiceState = EServiceLifecycleState::Failed;
		return false;
	}

	// Setup event subscriptions
	SetupEventSubscriptions();

	InitializationTime = FDateTime::UtcNow();
	ServiceState = EServiceLifecycleState::Ready;

	LOG_SERVICE_INFO(TEXT("Service initialized successfully"));
	return true;
}

bool USuspenseCoreEquipmentOperationService::ShutdownService(bool bForce)
{
	TRACK_SERVICE_SHUTDOWN();

	if (ServiceState == EServiceLifecycleState::Shutdown)
	{
		return true;
	}

	ServiceState = EServiceLifecycleState::Shutting;

	// Clear operation queue
	if (!bForce && OperationQueue.Num() > 0)
	{
		LOG_SERVICE_WARNING(TEXT("Shutting down with %d pending operations"), OperationQueue.Num());
	}

	CleanupResources();

	ServiceState = EServiceLifecycleState::Shutdown;
	LOG_SERVICE_INFO(TEXT("Service shut down"));
	return true;
}

EServiceLifecycleState USuspenseCoreEquipmentOperationService::GetServiceState() const
{
	return ServiceState;
}

bool USuspenseCoreEquipmentOperationService::IsServiceReady() const
{
	return ServiceState == EServiceLifecycleState::Ready;
}

FGameplayTag USuspenseCoreEquipmentOperationService::GetServiceTag() const
{
	static FGameplayTag ServiceTag = FGameplayTag::RequestGameplayTag(FName("Equipment.Service.Operation"));
	return ServiceTag;
}

FGameplayTagContainer USuspenseCoreEquipmentOperationService::GetRequiredDependencies() const
{
	FGameplayTagContainer Dependencies;
	Dependencies.AddTag(FGameplayTag::RequestGameplayTag(FName("Equipment.Service.Data")));
	Dependencies.AddTag(FGameplayTag::RequestGameplayTag(FName("Equipment.Service.Validation")));
	return Dependencies;
}

bool USuspenseCoreEquipmentOperationService::ValidateService(TArray<FText>& OutErrors) const
{
	bool bValid = true;

	if (!ServiceLocator.IsValid())
	{
		OutErrors.Add(FText::FromString(TEXT("ServiceLocator is null")));
		bValid = false;
	}

	if (ServiceState != EServiceLifecycleState::Ready)
	{
		OutErrors.Add(FText::FromString(TEXT("Service is not ready")));
		bValid = false;
	}

	return bValid;
}

void USuspenseCoreEquipmentOperationService::ResetService()
{
	ClearOperationQueue();
	TotalOperationsExecuted = 0;
	TotalOperationsFailed = 0;
	TotalOperationsQueued = 0;
	LOG_SERVICE_INFO(TEXT("Service reset"));
}

FString USuspenseCoreEquipmentOperationService::GetServiceStats() const
{
	return FString::Printf(TEXT("Operations - Executed: %d, Failed: %d, Queued: %d, Queue Size: %d"),
		TotalOperationsExecuted, TotalOperationsFailed, TotalOperationsQueued, OperationQueue.Num());
}

//========================================
// IEquipmentOperationService Interface
//========================================

ISuspenseEquipmentOperations* USuspenseCoreEquipmentOperationService::GetOperationsExecutor()
{
	// Return executor interface if available
	return nullptr;
}

bool USuspenseCoreEquipmentOperationService::QueueOperation(const FEquipmentOperationRequest& Request)
{
	return QueueOperationDeferred(Request, 0).IsValid();
}

void USuspenseCoreEquipmentOperationService::ProcessOperationQueue()
{
	CHECK_SERVICE_READY();

	if (!bQueueProcessingEnabled || OperationQueue.Num() == 0)
	{
		return;
	}

	ProcessNextQueuedOperation();
}

FEquipmentOperationResult USuspenseCoreEquipmentOperationService::ExecuteImmediate(const FEquipmentOperationRequest& Request)
{
	return ExecuteOperation(Request);
}

//========================================
// Operation Execution
//========================================

FEquipmentOperationResult USuspenseCoreEquipmentOperationService::ExecuteOperation(const FEquipmentOperationRequest& Request)
{
	CHECK_SERVICE_READY();
	RECORD_SERVICE_OPERATION(ExecuteOperation);

	FEquipmentOperationResult Result;
	Result.bSuccess = false;

	// Validate operation
	FText ValidationError;
	if (!ValidateOperation(Request, ValidationError))
	{
		Result.ErrorMessage = ValidationError;
		PublishOperationFailed(Request, ValidationError);
		TotalOperationsFailed++;
		return Result;
	}

	// Publish operation started
	PublishOperationStarted(Request);

	// Execute operation
	Result = ExecuteOperationInternal(Request);

	// Replicate if successful
	if (Result.bSuccess)
	{
		ReplicateOperation(Request, Result);
		PublishOperationCompleted(Result);
		TotalOperationsExecuted++;
	}
	else
	{
		PublishOperationFailed(Request, Result.ErrorMessage);
		TotalOperationsFailed++;
	}

	return Result;
}

FGuid USuspenseCoreEquipmentOperationService::QueueOperationDeferred(const FEquipmentOperationRequest& Request, int32 Priority)
{
	CHECK_SERVICE_READY();

	if (OperationQueue.Num() >= MaxQueueSize)
	{
		LOG_SERVICE_WARNING(TEXT("Operation queue full, cannot queue operation"));
		return FGuid();
	}

	FGuid OperationId = FGuid::NewGuid();
	OperationQueue.Add(Request);
	OperationIdMap.Add(OperationId, OperationQueue.Num() - 1);
	TotalOperationsQueued++;

	LOG_SERVICE_VERBOSE(TEXT("Operation queued: %s"), *OperationId.ToString());
	return OperationId;
}

TArray<FEquipmentOperationResult> USuspenseCoreEquipmentOperationService::ExecuteBatchAtomic(const TArray<FEquipmentOperationRequest>& Requests)
{
	CHECK_SERVICE_READY();

	TArray<FEquipmentOperationResult> Results;
	Results.Reserve(Requests.Num());

	// Execute all operations
	for (const FEquipmentOperationRequest& Request : Requests)
	{
		Results.Add(ExecuteOperation(Request));
	}

	return Results;
}

bool USuspenseCoreEquipmentOperationService::CancelOperation(const FGuid& OperationId)
{
	if (int32* IndexPtr = OperationIdMap.Find(OperationId))
	{
		if (OperationQueue.IsValidIndex(*IndexPtr))
		{
			OperationQueue.RemoveAt(*IndexPtr);
			OperationIdMap.Remove(OperationId);
			LOG_SERVICE_INFO(TEXT("Operation cancelled: %s"), *OperationId.ToString());
			return true;
		}
	}

	return false;
}

int32 USuspenseCoreEquipmentOperationService::GetQueueSize() const
{
	return OperationQueue.Num();
}

void USuspenseCoreEquipmentOperationService::ClearOperationQueue()
{
	OperationQueue.Empty();
	OperationIdMap.Empty();
	LOG_SERVICE_INFO(TEXT("Operation queue cleared"));
}

//========================================
// Event Publishing
//========================================

void USuspenseCoreEquipmentOperationService::PublishOperationStarted(const FEquipmentOperationRequest& Request)
{
	PUBLISH_SERVICE_EVENT(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Equipment.Operation.Started")),
		FSuspenseEquipmentEventData()
	);
}

void USuspenseCoreEquipmentOperationService::PublishOperationCompleted(const FEquipmentOperationResult& Result)
{
	PUBLISH_SERVICE_EVENT(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Equipment.Operation.Completed")),
		FSuspenseEquipmentEventData()
	);
}

void USuspenseCoreEquipmentOperationService::PublishOperationFailed(const FEquipmentOperationRequest& Request, const FText& Reason)
{
	LOG_SERVICE_WARNING(TEXT("Operation failed: %s"), *Reason.ToString());

	PUBLISH_SERVICE_EVENT(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Equipment.Operation.Failed")),
		FSuspenseEquipmentEventData()
	);
}

//========================================
// Service Lifecycle
//========================================

bool USuspenseCoreEquipmentOperationService::InitializeDependencies()
{
	if (!ServiceLocator.IsValid())
	{
		LOG_SERVICE_ERROR(TEXT("ServiceLocator is null"));
		return false;
	}

	// Get EventBus
	EventBus = ServiceLocator->GetService<USuspenseCoreEventBus>();

	// Dependencies will be resolved through ServiceLocator as needed
	return true;
}

void USuspenseCoreEquipmentOperationService::SetupEventSubscriptions()
{
	// Subscribe to relevant events
	SUBSCRIBE_SERVICE_EVENT(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Equipment.Data.Changed")),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreEquipmentOperationService::OnDataChanged)
	);
}

void USuspenseCoreEquipmentOperationService::CleanupResources()
{
	ClearOperationQueue();
	EventBus.Reset();
	ValidationService.Reset();
	DataService.Reset();
	NetworkService.Reset();
}

//========================================
// Operation Processing
//========================================

bool USuspenseCoreEquipmentOperationService::ValidateOperation(const FEquipmentOperationRequest& Request, FText& OutError) const
{
	// Basic validation - actual validation done by ValidationService
	return true;
}

FEquipmentOperationResult USuspenseCoreEquipmentOperationService::ExecuteOperationInternal(const FEquipmentOperationRequest& Request)
{
	FEquipmentOperationResult Result;
	Result.bSuccess = true;

	// Actual execution would be done through DataService
	// This is a stub implementation

	return Result;
}

void USuspenseCoreEquipmentOperationService::ReplicateOperation(const FEquipmentOperationRequest& Request, const FEquipmentOperationResult& Result)
{
	// Replication would be done through NetworkService
	// This is a stub implementation
}

void USuspenseCoreEquipmentOperationService::ProcessNextQueuedOperation()
{
	if (OperationQueue.Num() == 0)
	{
		return;
	}

	FEquipmentOperationRequest Request = OperationQueue[0];
	OperationQueue.RemoveAt(0);

	ExecuteOperation(Request);
}

//========================================
// Event Handlers
//========================================

void USuspenseCoreEquipmentOperationService::OnDataChanged(const FSuspenseEquipmentEventData& EventData)
{
	// Handle data changed event
}

void USuspenseCoreEquipmentOperationService::OnValidationRulesChanged(const FSuspenseEquipmentEventData& EventData)
{
	// Handle validation rules changed event
}
