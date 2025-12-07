// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Services/SuspenseCoreEquipmentNetworkService.h"
#include "SuspenseCore/Services/SuspenseCoreServiceLocator.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

USuspenseCoreEquipmentNetworkService::USuspenseCoreEquipmentNetworkService()
	: ServiceState(EServiceLifecycleState::Uninitialized)
	, bIsServer(false)
	, bIsClient(false)
	, PredictionTimeout(5.0f)
	, RateLimitWindow(1.0f)
	, MaxOperationsPerWindow(10)
	, bEnablePrediction(true)
	, bEnableRateLimiting(true)
	, bEnableDetailedLogging(false)
	, ReplicationFrequency(10.0f)
	, TotalServerOperations(0)
	, TotalPredictions(0)
	, SuccessfulPredictions(0)
	, FailedPredictions(0)
	, RateLimitViolations(0)
	, TotalBytesSent(0)
	, TotalBytesReceived(0)
{
}

USuspenseCoreEquipmentNetworkService::~USuspenseCoreEquipmentNetworkService()
{
}

//========================================
// ISuspenseEquipmentService Interface
//========================================

bool USuspenseCoreEquipmentNetworkService::InitializeService(const FServiceInitParams& Params)
{
	TRACK_SERVICE_INIT();

	ServiceState = EServiceLifecycleState::Initializing;
	ServiceLocator = Params.ServiceLocator;

	// Determine network role
	UWorld* World = GetWorld();
	if (World)
	{
		bIsServer = World->IsServer();
		bIsClient = !bIsServer;
	}

	if (!InitializeNetworkComponents())
	{
		LOG_SERVICE_ERROR(TEXT("Failed to initialize network components"));
		ServiceState = EServiceLifecycleState::Failed;
		return false;
	}

	SetupEventSubscriptions();

	InitializationTime = FDateTime::UtcNow();
	ServiceState = EServiceLifecycleState::Ready;

	LOG_SERVICE_INFO(TEXT("Service initialized successfully (Server: %d, Client: %d)"), bIsServer, bIsClient);
	return true;
}

bool USuspenseCoreEquipmentNetworkService::ShutdownService(bool bForce)
{
	TRACK_SERVICE_SHUTDOWN();

	ServiceState = EServiceLifecycleState::Shutting;
	CleanupNetworkResources();
	ServiceState = EServiceLifecycleState::Shutdown;

	LOG_SERVICE_INFO(TEXT("Service shut down"));
	return true;
}

EServiceLifecycleState USuspenseCoreEquipmentNetworkService::GetServiceState() const
{
	return ServiceState;
}

bool USuspenseCoreEquipmentNetworkService::IsServiceReady() const
{
	return ServiceState == EServiceLifecycleState::Ready;
}

FGameplayTag USuspenseCoreEquipmentNetworkService::GetServiceTag() const
{
	static FGameplayTag ServiceTag = FGameplayTag::RequestGameplayTag(FName("Equipment.Service.Network"));
	return ServiceTag;
}

FGameplayTagContainer USuspenseCoreEquipmentNetworkService::GetRequiredDependencies() const
{
	FGameplayTagContainer Dependencies;
	Dependencies.AddTag(FGameplayTag::RequestGameplayTag(FName("Equipment.Service.Data")));
	Dependencies.AddTag(FGameplayTag::RequestGameplayTag(FName("Equipment.Service.Operation")));
	return Dependencies;
}

bool USuspenseCoreEquipmentNetworkService::ValidateService(TArray<FText>& OutErrors) const
{
	return true;
}

void USuspenseCoreEquipmentNetworkService::ResetService()
{
	ClearPredictions();
	ResetRateLimits();
	TotalServerOperations = 0;
	TotalPredictions = 0;
	SuccessfulPredictions = 0;
	FailedPredictions = 0;
	RateLimitViolations = 0;
	TotalBytesSent = 0;
	TotalBytesReceived = 0;
	LOG_SERVICE_INFO(TEXT("Service reset"));
}

FString USuspenseCoreEquipmentNetworkService::GetServiceStats() const
{
	float PredictionAccuracy = GetPredictionAccuracy();

	return FString::Printf(TEXT("Network - Server Ops: %d, Predictions: %d (Success: %d, Failed: %d, Accuracy: %.1f%%), Rate Limit Violations: %d"),
		TotalServerOperations, TotalPredictions, SuccessfulPredictions, FailedPredictions, PredictionAccuracy, RateLimitViolations);
}

//========================================
// IEquipmentNetworkService Interface
//========================================

ISuspenseNetworkDispatcher* USuspenseCoreEquipmentNetworkService::GetNetworkDispatcher()
{
	return nullptr;
}

ISuspensePredictionManager* USuspenseCoreEquipmentNetworkService::GetPredictionManager()
{
	return nullptr;
}

ISuspenseReplicationProvider* USuspenseCoreEquipmentNetworkService::GetReplicationProvider()
{
	return nullptr;
}

//========================================
// Server Authority
//========================================

FEquipmentOperationResult USuspenseCoreEquipmentNetworkService::ServerExecuteOperation(const FEquipmentOperationRequest& Request)
{
	CHECK_SERVICE_READY();

	FEquipmentOperationResult Result;
	Result.bSuccess = false;

	// Server authority check
	if (!HasAuthority())
	{
		LOG_SERVICE_ERROR(TEXT("ServerExecuteOperation called without authority"));
		Result.ErrorMessage = FText::FromString(TEXT("No authority"));
		return Result;
	}

	// Validate request
	FText ValidationError;
	if (!ValidateServerRequest(Request, ValidationError))
	{
		LOG_SERVICE_WARNING(TEXT("Server request validation failed: %s"), *ValidationError.ToString());
		Result.ErrorMessage = ValidationError;
		PublishRateLimitExceeded(Request);
		return Result;
	}

	// Record operation for rate limiting
	RecordOperation(Request);
	TotalServerOperations++;

	// Execute operation through Operation Service
	// Actual execution would be delegated to OperationService

	Result.bSuccess = true;
	PublishServerOperation(Request, Result);

	return Result;
}

bool USuspenseCoreEquipmentNetworkService::ValidateServerRequest(const FEquipmentOperationRequest& Request, FText& OutError) const
{
	CHECK_SERVICE_READY_BOOL();

	// Check rate limiting
	if (!CheckRateLimit(Request))
	{
		OutError = FText::FromString(TEXT("Rate limit exceeded"));
		return false;
	}

	// Additional security validation would go here
	return true;
}

bool USuspenseCoreEquipmentNetworkService::HasAuthority() const
{
	UWorld* World = GetWorld();
	return World && World->IsServer();
}

APlayerController* USuspenseCoreEquipmentNetworkService::GetOwningController() const
{
	return OwningController.Get();
}

//========================================
// Client Prediction
//========================================

FGuid USuspenseCoreEquipmentNetworkService::StartPrediction(const FEquipmentOperationRequest& Request)
{
	CHECK_SERVICE_READY();

	if (!bEnablePrediction)
	{
		return FGuid();
	}

	FGuid PredictionId = FGuid::NewGuid();

	// Create snapshot for rollback
	FEquipmentStateSnapshot Snapshot = CreatePredictionSnapshot();
	PendingPredictions.Add(PredictionId, Snapshot);
	PredictionTimestamps.Add(PredictionId, FDateTime::UtcNow());

	// Apply prediction locally
	ApplyPrediction(PredictionId, Request);

	TotalPredictions++;
	PublishPredictionStarted(PredictionId);

	LOG_SERVICE_VERBOSE(TEXT("Prediction started: %s"), *PredictionId.ToString());
	return PredictionId;
}

void USuspenseCoreEquipmentNetworkService::ConfirmPrediction(const FGuid& PredictionId, const FEquipmentOperationResult& ServerResult)
{
	CHECK_SERVICE_READY();

	if (!PendingPredictions.Contains(PredictionId))
	{
		LOG_SERVICE_WARNING(TEXT("Prediction not found: %s"), *PredictionId.ToString());
		return;
	}

	// Reconcile with server result
	ReconcilePrediction(PredictionId, ServerResult);

	// Clean up prediction
	PendingPredictions.Remove(PredictionId);
	PredictionTimestamps.Remove(PredictionId);

	bool bMatched = ServerResult.bSuccess;
	if (bMatched)
	{
		SuccessfulPredictions++;
	}
	else
	{
		FailedPredictions++;
	}

	PublishPredictionConfirmed(PredictionId, bMatched);
	LOG_SERVICE_VERBOSE(TEXT("Prediction confirmed: %s (Matched: %d)"), *PredictionId.ToString(), bMatched);
}

void USuspenseCoreEquipmentNetworkService::RollbackPrediction(const FGuid& PredictionId)
{
	CHECK_SERVICE_READY();

	FEquipmentStateSnapshot* Snapshot = PendingPredictions.Find(PredictionId);
	if (!Snapshot)
	{
		LOG_SERVICE_WARNING(TEXT("Prediction snapshot not found: %s"), *PredictionId.ToString());
		return;
	}

	// Restore snapshot
	// Actual restoration would be done through DataService

	PendingPredictions.Remove(PredictionId);
	PredictionTimestamps.Remove(PredictionId);
	FailedPredictions++;

	LOG_SERVICE_INFO(TEXT("Prediction rolled back: %s"), *PredictionId.ToString());
}

TArray<FGuid> USuspenseCoreEquipmentNetworkService::GetPendingPredictions() const
{
	TArray<FGuid> PredictionIds;
	PendingPredictions.GetKeys(PredictionIds);
	return PredictionIds;
}

void USuspenseCoreEquipmentNetworkService::ClearPredictions()
{
	PendingPredictions.Empty();
	PredictionTimestamps.Empty();
	LOG_SERVICE_INFO(TEXT("All predictions cleared"));
}

//========================================
// Replication Management
//========================================

void USuspenseCoreEquipmentNetworkService::ReplicateEquipmentState(int32 SlotIndex)
{
	CHECK_SERVICE_READY();

	if (!HasAuthority())
	{
		LOG_SERVICE_WARNING(TEXT("ReplicateEquipmentState called without authority"));
		return;
	}

	// Replicate state to clients
	LOG_SERVICE_VERBOSE(TEXT("Replicating state for slot %d"), SlotIndex);
}

void USuspenseCoreEquipmentNetworkService::ReplicateBatchChanges(const TArray<int32>& SlotIndices)
{
	CHECK_SERVICE_READY();

	if (!HasAuthority())
	{
		return;
	}

	for (int32 SlotIndex : SlotIndices)
	{
		ReplicateEquipmentState(SlotIndex);
	}
}

void USuspenseCoreEquipmentNetworkService::ForceStateRefresh()
{
	CHECK_SERVICE_READY();

	LOG_SERVICE_INFO(TEXT("Forcing state refresh"));
}

void USuspenseCoreEquipmentNetworkService::SetReplicationCondition(int32 SlotIndex, ELifetimeCondition Condition)
{
	CHECK_SERVICE_READY();

	// Set replication condition for slot
}

//========================================
// Security and Rate Limiting
//========================================

bool USuspenseCoreEquipmentNetworkService::CheckRateLimit(const FEquipmentOperationRequest& Request) const
{
	if (!bEnableRateLimiting)
	{
		return true;
	}

	int32 OperationsInWindow = GetOperationsInWindow();
	return OperationsInWindow < MaxOperationsPerWindow;
}

void USuspenseCoreEquipmentNetworkService::RecordOperation(const FEquipmentOperationRequest& Request)
{
	RecentOperationTimes.Add(FDateTime::UtcNow());
	UpdateRateLimitWindow();
}

bool USuspenseCoreEquipmentNetworkService::IsClientFlooding() const
{
	return GetOperationsInWindow() >= MaxOperationsPerWindow;
}

void USuspenseCoreEquipmentNetworkService::ResetRateLimits()
{
	RecentOperationTimes.Empty();
	LastRateLimitCheck = FDateTime::UtcNow();
	LOG_SERVICE_INFO(TEXT("Rate limits reset"));
}

//========================================
// Network Diagnostics
//========================================

FString USuspenseCoreEquipmentNetworkService::GetNetworkStats() const
{
	return FString::Printf(TEXT("Network Stats - RTT: %.2fms, Prediction Accuracy: %.1f%%, Bytes Sent: %d, Bytes Received: %d"),
		GetAverageRTT(), GetPredictionAccuracy(), TotalBytesSent, TotalBytesReceived);
}

float USuspenseCoreEquipmentNetworkService::GetPredictionAccuracy() const
{
	if (TotalPredictions == 0)
	{
		return 100.0f;
	}

	return (float)SuccessfulPredictions / (float)TotalPredictions * 100.0f;
}

float USuspenseCoreEquipmentNetworkService::GetAverageRTT() const
{
	// Average round-trip time calculation
	return 0.0f;
}

//========================================
// Event Publishing
//========================================

void USuspenseCoreEquipmentNetworkService::PublishServerOperation(const FEquipmentOperationRequest& Request, const FEquipmentOperationResult& Result)
{
	PUBLISH_SERVICE_EVENT(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Equipment.Network.ServerOperation")),
		FSuspenseEquipmentEventData()
	);
}

void USuspenseCoreEquipmentNetworkService::PublishPredictionStarted(const FGuid& PredictionId)
{
	PUBLISH_SERVICE_EVENT(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Equipment.Network.PredictionStarted")),
		FSuspenseEquipmentEventData()
	);
}

void USuspenseCoreEquipmentNetworkService::PublishPredictionConfirmed(const FGuid& PredictionId, bool bMatched)
{
	PUBLISH_SERVICE_EVENT(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Equipment.Network.PredictionConfirmed")),
		FSuspenseEquipmentEventData()
	);
}

void USuspenseCoreEquipmentNetworkService::PublishRateLimitExceeded(const FEquipmentOperationRequest& Request)
{
	RateLimitViolations++;

	PUBLISH_SERVICE_EVENT(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Equipment.Network.RateLimitExceeded")),
		FSuspenseEquipmentEventData()
	);

	LOG_SERVICE_WARNING(TEXT("Rate limit exceeded"));
}

//========================================
// Service Lifecycle
//========================================

bool USuspenseCoreEquipmentNetworkService::InitializeNetworkComponents()
{
	// Initialize network components
	return true;
}

void USuspenseCoreEquipmentNetworkService::SetupEventSubscriptions()
{
	// Subscribe to server operation events
	SUBSCRIBE_SERVICE_EVENT(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Equipment.Operation.Completed")),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreEquipmentNetworkService::OnServerOperationCompleted)
	);
}

void USuspenseCoreEquipmentNetworkService::CleanupNetworkResources()
{
	ClearPredictions();
	ResetRateLimits();
}

//========================================
// Prediction Management
//========================================

FEquipmentStateSnapshot USuspenseCoreEquipmentNetworkService::CreatePredictionSnapshot() const
{
	FEquipmentStateSnapshot Snapshot;
	// Create snapshot from current state via DataService
	return Snapshot;
}

void USuspenseCoreEquipmentNetworkService::ApplyPrediction(const FGuid& PredictionId, const FEquipmentOperationRequest& Request)
{
	// Apply prediction locally
}

void USuspenseCoreEquipmentNetworkService::ReconcilePrediction(const FGuid& PredictionId, const FEquipmentOperationResult& ServerResult)
{
	// Reconcile prediction with server result
}

void USuspenseCoreEquipmentNetworkService::CleanupExpiredPredictions()
{
	FDateTime Now = FDateTime::UtcNow();
	TArray<FGuid> ExpiredPredictions;

	for (const auto& Pair : PredictionTimestamps)
	{
		FTimespan Age = Now - Pair.Value;
		if (Age.GetTotalSeconds() > PredictionTimeout)
		{
			ExpiredPredictions.Add(Pair.Key);
		}
	}

	for (const FGuid& PredictionId : ExpiredPredictions)
	{
		RollbackPrediction(PredictionId);
	}
}

//========================================
// Rate Limiting
//========================================

void USuspenseCoreEquipmentNetworkService::UpdateRateLimitWindow()
{
	FDateTime Now = FDateTime::UtcNow();
	FDateTime WindowStart = Now - FTimespan::FromSeconds(RateLimitWindow);

	// Remove operations outside the window
	RecentOperationTimes.RemoveAll([WindowStart](const FDateTime& Time)
	{
		return Time < WindowStart;
	});

	LastRateLimitCheck = Now;
}

int32 USuspenseCoreEquipmentNetworkService::GetOperationsInWindow() const
{
	return RecentOperationTimes.Num();
}

int32 USuspenseCoreEquipmentNetworkService::CalculateRateLimit() const
{
	return MaxOperationsPerWindow;
}

//========================================
// Event Handlers
//========================================

void USuspenseCoreEquipmentNetworkService::OnServerOperationCompleted(const FSuspenseEquipmentEventData& EventData)
{
	// Handle server operation completed
}

void USuspenseCoreEquipmentNetworkService::OnReplicationUpdate(const FSuspenseEquipmentEventData& EventData)
{
	// Handle replication update
}

void USuspenseCoreEquipmentNetworkService::OnNetworkError(const FSuspenseEquipmentEventData& EventData)
{
	// Handle network error
	LOG_SERVICE_ERROR(TEXT("Network error occurred"));
}
