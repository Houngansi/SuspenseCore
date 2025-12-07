// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Interfaces/Equipment/ISuspenseEquipmentService.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreEquipmentServiceMacros.h"
#include "SuspenseCoreEquipmentOperationService.generated.h"

// Forward declarations
class USuspenseEquipmentServiceLocator;
class USuspenseCoreEventBus;
struct FEquipmentOperationRequest;
struct FEquipmentOperationResult;

/**
 * SuspenseCoreEquipmentOperationService
 *
 * Philosophy:
 * Orchestrates equipment operations by coordinating between validation, data, and network services.
 * Implements event-driven architecture with ServiceLocator dependency injection.
 *
 * Key Responsibilities:
 * - Operation request orchestration
 * - Service coordination (validation -> data -> network)
 * - Event publishing for operation lifecycle
 * - Operation queueing and batching
 * - Server authority enforcement
 *
 * Architecture Patterns:
 * - Event Bus: Publishes operation events for decoupled communication
 * - Dependency Injection: Uses ServiceLocator to access other services
 * - GameplayTags: Service identification and event categorization
 * - ACID Operations: Ensures data consistency through transaction coordination
 */
UCLASS(BlueprintType)
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentOperationService : public UObject, public IEquipmentOperationService
{
	GENERATED_BODY()

public:
	USuspenseCoreEquipmentOperationService();
	virtual ~USuspenseCoreEquipmentOperationService();

	//========================================
	// ISuspenseEquipmentService Interface
	//========================================

	virtual bool InitializeService(const FServiceInitParams& Params) override;
	virtual bool ShutdownService(bool bForce = false) override;
	virtual EServiceLifecycleState GetServiceState() const override;
	virtual bool IsServiceReady() const override;
	virtual FGameplayTag GetServiceTag() const override;
	virtual FGameplayTagContainer GetRequiredDependencies() const override;
	virtual bool ValidateService(TArray<FText>& OutErrors) const override;
	virtual void ResetService() override;
	virtual FString GetServiceStats() const override;

	//========================================
	// IEquipmentOperationService Interface
	//========================================

	virtual class ISuspenseEquipmentOperations* GetOperationsExecutor() override;
	virtual bool QueueOperation(const FEquipmentOperationRequest& Request) override;
	virtual void ProcessOperationQueue() override;
	virtual FEquipmentOperationResult ExecuteImmediate(const FEquipmentOperationRequest& Request) override;

	//========================================
	// Operation Execution
	//========================================

	/**
	 * Execute equipment operation with full orchestration
	 * - Validates through ValidationService
	 * - Executes through DataService
	 * - Replicates through NetworkService
	 * - Publishes events through EventBus
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Operations")
	FEquipmentOperationResult ExecuteOperation(const FEquipmentOperationRequest& Request);

	/**
	 * Queue operation for deferred execution
	 * Returns operation ID for tracking
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Operations")
	FGuid QueueOperationDeferred(const FEquipmentOperationRequest& Request, int32 Priority = 0);

	/**
	 * Execute batch of operations atomically
	 * All operations succeed or all fail (transaction semantics)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Operations")
	TArray<FEquipmentOperationResult> ExecuteBatchAtomic(const TArray<FEquipmentOperationRequest>& Requests);

	/**
	 * Cancel queued operation
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Operations")
	bool CancelOperation(const FGuid& OperationId);

	/**
	 * Get operation queue size
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Equipment|Operations")
	int32 GetQueueSize() const;

	/**
	 * Clear all queued operations
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Operations")
	void ClearOperationQueue();

	//========================================
	// Event Publishing
	//========================================

	/**
	 * Publish operation started event
	 */
	void PublishOperationStarted(const FEquipmentOperationRequest& Request);

	/**
	 * Publish operation completed event
	 */
	void PublishOperationCompleted(const FEquipmentOperationResult& Result);

	/**
	 * Publish operation failed event
	 */
	void PublishOperationFailed(const FEquipmentOperationRequest& Request, const FText& Reason);

protected:
	//========================================
	// Service Lifecycle
	//========================================

	/** Initialize dependencies from ServiceLocator */
	bool InitializeDependencies();

	/** Setup event subscriptions */
	void SetupEventSubscriptions();

	/** Cleanup resources on shutdown */
	void CleanupResources();

	//========================================
	// Operation Processing
	//========================================

	/** Validate operation through ValidationService */
	bool ValidateOperation(const FEquipmentOperationRequest& Request, FText& OutError) const;

	/** Execute operation through DataService */
	FEquipmentOperationResult ExecuteOperationInternal(const FEquipmentOperationRequest& Request);

	/** Replicate operation through NetworkService */
	void ReplicateOperation(const FEquipmentOperationRequest& Request, const FEquipmentOperationResult& Result);

	/** Process next operation in queue */
	void ProcessNextQueuedOperation();

	//========================================
	// Event Handlers
	//========================================

	/** Handle data changed event */
	void OnDataChanged(const struct FSuspenseEquipmentEventData& EventData);

	/** Handle validation rules changed event */
	void OnValidationRulesChanged(const struct FSuspenseEquipmentEventData& EventData);

private:
	//========================================
	// Service State
	//========================================

	/** Current service lifecycle state */
	UPROPERTY()
	EServiceLifecycleState ServiceState;

	/** Service initialization timestamp */
	UPROPERTY()
	FDateTime InitializationTime;

	//========================================
	// Dependencies (Injected via ServiceLocator)
	//========================================

	/** ServiceLocator for dependency injection */
	UPROPERTY()
	TWeakObjectPtr<USuspenseEquipmentServiceLocator> ServiceLocator;

	/** EventBus for event-driven communication */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> EventBus;

	/** Validation service dependency */
	UPROPERTY()
	TWeakObjectPtr<UObject> ValidationService;

	/** Data service dependency */
	UPROPERTY()
	TWeakObjectPtr<UObject> DataService;

	/** Network service dependency */
	UPROPERTY()
	TWeakObjectPtr<UObject> NetworkService;

	//========================================
	// Operation Queue
	//========================================

	/** Queued operations awaiting execution */
	UPROPERTY()
	TArray<FEquipmentOperationRequest> OperationQueue;

	/** Map of operation IDs to queue indices */
	UPROPERTY()
	TMap<FGuid, int32> OperationIdMap;

	/** Queue processing enabled flag */
	UPROPERTY()
	bool bQueueProcessingEnabled;

	//========================================
	// Configuration
	//========================================

	/** Maximum queue size */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	int32 MaxQueueSize;

	/** Enable detailed logging */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	bool bEnableDetailedLogging;

	/** Enable operation batching */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	bool bEnableBatching;

	//========================================
	// Statistics
	//========================================

	/** Total operations executed */
	UPROPERTY()
	int32 TotalOperationsExecuted;

	/** Total operations failed */
	UPROPERTY()
	int32 TotalOperationsFailed;

	/** Total operations queued */
	UPROPERTY()
	int32 TotalOperationsQueued;
};
