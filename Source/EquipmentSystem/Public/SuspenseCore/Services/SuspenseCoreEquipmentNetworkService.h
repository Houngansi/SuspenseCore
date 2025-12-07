// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Interfaces/Equipment/ISuspenseEquipmentService.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreEquipmentServiceMacros.h"
#include "SuspenseCoreEquipmentNetworkService.generated.h"

// Forward declarations
class USuspenseEquipmentServiceLocator;
class USuspenseCoreEventBus;
class APlayerController;
class ISuspenseNetworkDispatcher;
class ISuspensePredictionManager;
class ISuspenseReplicationProvider;
struct FEquipmentOperationRequest;
struct FEquipmentOperationResult;

/**
 * SuspenseCoreEquipmentNetworkService
 *
 * Philosophy:
 * Manages network replication and client-server communication for equipment.
 * Ensures server authority while providing responsive client prediction.
 *
 * Key Responsibilities:
 * - Server RPC handling
 * - Client prediction management
 * - Replication of equipment state
 * - Network event dispatching
 * - Rollback and reconciliation
 * - Bandwidth optimization
 * - Security validation
 *
 * Architecture Patterns:
 * - Event Bus: Publishes network events
 * - Dependency Injection: Uses ServiceLocator
 * - GameplayTags: Event categorization
 * - Server Authority: All operations validated on server
 * - Client Prediction: Optimistic client updates
 * - Rollback: Prediction correction
 *
 * Network Flow:
 * 1. Client requests operation → Predict locally
 * 2. Send RPC to server
 * 3. Server validates and executes
 * 4. Server replicates result
 * 5. Client receives → Reconcile prediction
 * 6. Rollback if mismatch
 *
 * Best Practices (from BestPractices.md):
 * - Server authority for all data changes
 * - WithValidation for all RPCs
 * - Rate limiting to prevent spam
 * - COND_OwnerOnly for private data
 * - ReplicationGraph support for MMO scale
 */
UCLASS(BlueprintType)
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentNetworkService : public UObject, public IEquipmentNetworkService
{
	GENERATED_BODY()

public:
	USuspenseCoreEquipmentNetworkService();
	virtual ~USuspenseCoreEquipmentNetworkService();

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
	// IEquipmentNetworkService Interface
	//========================================

	virtual ISuspenseNetworkDispatcher* GetNetworkDispatcher() override;
	virtual ISuspensePredictionManager* GetPredictionManager() override;
	virtual ISuspenseReplicationProvider* GetReplicationProvider() override;

	//========================================
	// Server Authority
	//========================================

	/**
	 * Execute operation on server (authority)
	 * Called from client RPC
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Network")
	FEquipmentOperationResult ServerExecuteOperation(const FEquipmentOperationRequest& Request);

	/**
	 * Validate server operation request
	 * Checks authority, rate limits, security
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Network")
	bool ValidateServerRequest(const FEquipmentOperationRequest& Request, FText& OutError) const;

	/**
	 * Check if local player has authority
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Equipment|Network")
	bool HasAuthority() const;

	/**
	 * Get owning player controller
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Equipment|Network")
	APlayerController* GetOwningController() const;

	//========================================
	// Client Prediction
	//========================================

	/**
	 * Start client prediction for operation
	 * Returns prediction ID for tracking
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Network")
	FGuid StartPrediction(const FEquipmentOperationRequest& Request);

	/**
	 * Confirm prediction when server result received
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Network")
	void ConfirmPrediction(const FGuid& PredictionId, const FEquipmentOperationResult& ServerResult);

	/**
	 * Rollback failed prediction
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Network")
	void RollbackPrediction(const FGuid& PredictionId);

	/**
	 * Get pending predictions
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Equipment|Network")
	TArray<FGuid> GetPendingPredictions() const;

	/**
	 * Clear all predictions
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Network")
	void ClearPredictions();

	//========================================
	// Replication Management
	//========================================

	/**
	 * Replicate equipment state to clients
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Network")
	void ReplicateEquipmentState(int32 SlotIndex);

	/**
	 * Replicate batch state changes
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Network")
	void ReplicateBatchChanges(const TArray<int32>& SlotIndices);

	/**
	 * Force full state refresh for client
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Network")
	void ForceStateRefresh();

	/**
	 * Set replication condition for slot
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Network")
	void SetReplicationCondition(int32 SlotIndex, ELifetimeCondition Condition);

	//========================================
	// Security and Rate Limiting
	//========================================

	/**
	 * Check rate limit for operation
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Network")
	bool CheckRateLimit(const FEquipmentOperationRequest& Request) const;

	/**
	 * Record operation for rate limiting
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Network")
	void RecordOperation(const FEquipmentOperationRequest& Request);

	/**
	 * Check if client is flooding
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Equipment|Network")
	bool IsClientFlooding() const;

	/**
	 * Reset rate limit counters
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Network")
	void ResetRateLimits();

	//========================================
	// Network Diagnostics
	//========================================

	/**
	 * Get network statistics
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Network")
	FString GetNetworkStats() const;

	/**
	 * Get prediction accuracy
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Equipment|Network")
	float GetPredictionAccuracy() const;

	/**
	 * Get average round-trip time
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Equipment|Network")
	float GetAverageRTT() const;

	//========================================
	// Event Publishing
	//========================================

	/**
	 * Publish server operation event
	 */
	void PublishServerOperation(const FEquipmentOperationRequest& Request, const FEquipmentOperationResult& Result);

	/**
	 * Publish prediction started event
	 */
	void PublishPredictionStarted(const FGuid& PredictionId);

	/**
	 * Publish prediction confirmed event
	 */
	void PublishPredictionConfirmed(const FGuid& PredictionId, bool bMatched);

	/**
	 * Publish rate limit exceeded event
	 */
	void PublishRateLimitExceeded(const FEquipmentOperationRequest& Request);

protected:
	//========================================
	// Service Lifecycle
	//========================================

	/** Initialize network components */
	bool InitializeNetworkComponents();

	/** Setup event subscriptions */
	void SetupEventSubscriptions();

	/** Cleanup network resources */
	void CleanupNetworkResources();

	//========================================
	// Prediction Management
	//========================================

	/** Create prediction snapshot */
	struct FEquipmentStateSnapshot CreatePredictionSnapshot() const;

	/** Apply prediction to local state */
	void ApplyPrediction(const FGuid& PredictionId, const FEquipmentOperationRequest& Request);

	/** Reconcile prediction with server result */
	void ReconcilePrediction(const FGuid& PredictionId, const FEquipmentOperationResult& ServerResult);

	/** Cleanup expired predictions */
	void CleanupExpiredPredictions();

	//========================================
	// Rate Limiting
	//========================================

	/** Update rate limit window */
	void UpdateRateLimitWindow();

	/** Get operations in current window */
	int32 GetOperationsInWindow() const;

	/** Calculate rate limit threshold */
	int32 CalculateRateLimit() const;

	//========================================
	// Event Handlers
	//========================================

	/** Handle server operation completed */
	void OnServerOperationCompleted(const struct FSuspenseEquipmentEventData& EventData);

	/** Handle replication update */
	void OnReplicationUpdate(const struct FSuspenseEquipmentEventData& EventData);

	/** Handle network error */
	void OnNetworkError(const struct FSuspenseEquipmentEventData& EventData);

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

	/** Is this service on server */
	UPROPERTY()
	bool bIsServer;

	/** Is this service on client */
	UPROPERTY()
	bool bIsClient;

	//========================================
	// Dependencies (via ServiceLocator)
	//========================================

	/** ServiceLocator for dependency injection */
	UPROPERTY()
	TWeakObjectPtr<USuspenseEquipmentServiceLocator> ServiceLocator;

	/** EventBus for event subscription/publishing */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> EventBus;

	/** Data service for state access */
	UPROPERTY()
	TWeakObjectPtr<UObject> DataService;

	/** Operation service for execution */
	UPROPERTY()
	TWeakObjectPtr<UObject> OperationService;

	//========================================
	// Network Components
	//========================================

	/** Owning player controller */
	UPROPERTY()
	TWeakObjectPtr<APlayerController> OwningController;

	//========================================
	// Client Prediction
	//========================================

	/** Pending predictions map */
	UPROPERTY()
	TMap<FGuid, struct FEquipmentStateSnapshot> PendingPredictions;

	/** Prediction timestamps */
	UPROPERTY()
	TMap<FGuid, FDateTime> PredictionTimestamps;

	/** Prediction timeout in seconds */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	float PredictionTimeout;

	//========================================
	// Rate Limiting
	//========================================

	/** Operation timestamps for rate limiting */
	UPROPERTY()
	TArray<FDateTime> RecentOperationTimes;

	/** Rate limit window in seconds */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	float RateLimitWindow;

	/** Max operations per window */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	int32 MaxOperationsPerWindow;

	/** Last rate limit check time */
	UPROPERTY()
	FDateTime LastRateLimitCheck;

	//========================================
	// Configuration
	//========================================

	/** Enable client prediction */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	bool bEnablePrediction;

	/** Enable rate limiting */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	bool bEnableRateLimiting;

	/** Enable detailed logging */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	bool bEnableDetailedLogging;

	/** Replication frequency (Hz) */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	float ReplicationFrequency;

	//========================================
	// Statistics
	//========================================

	/** Total server operations */
	UPROPERTY()
	int32 TotalServerOperations;

	/** Total client predictions */
	UPROPERTY()
	int32 TotalPredictions;

	/** Successful predictions */
	UPROPERTY()
	int32 SuccessfulPredictions;

	/** Failed predictions */
	UPROPERTY()
	int32 FailedPredictions;

	/** Rate limit violations */
	UPROPERTY()
	int32 RateLimitViolations;

	/** Total bytes sent */
	UPROPERTY()
	int32 TotalBytesSent;

	/** Total bytes received */
	UPROPERTY()
	int32 TotalBytesReceived;
};
