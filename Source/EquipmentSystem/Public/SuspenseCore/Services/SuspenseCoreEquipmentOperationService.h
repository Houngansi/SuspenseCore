// SuspenseEquipmentOperationService.h
// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Transaction/SuspenseEquipmentTransactionProcessor.h"
#include "Components/Core/SuspenseEquipmentDataStore.h"
#include "Interfaces/Equipment/ISuspenseCoreEquipmentService.h"
#include "Interfaces/Equipment/ISuspenseCoreEquipmentOperations.h"
#include "Interfaces/Equipment/ISuspenseCoreEquipmentDataProvider.h"
#include "Interfaces/Equipment/ISuspenseCoreTransactionManager.h"
#include "Interfaces/Equipment/ISuspenseCoreEquipmentRules.h"
#include "Interfaces/Equipment/ISuspensePredictionManager.h"
#include "Types/Equipment/SuspenseEquipmentTypes.h"
#include "Core/Utils/SuspenseEquipmentCacheManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceMacros.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/PlayerState.h"
#include "HAL/CriticalSection.h"
#include "Containers/Queue.h"
#include "Containers/Ticker.h"
#include <atomic>
#include "SuspenseCoreEquipmentOperationService.generated.h"

class USuspenseCoreEquipmentValidationService;

DECLARE_LOG_CATEGORY_EXTERN(LogSuspenseCoreEquipmentOperations, Log, All);

// Forward declarations for plan types from Executor
struct FSuspenseCoreTransactionPlanStep;
struct FSuspenseCoreTransactionPlan;

/**
 * Operation queue entry with priority support
 */
USTRUCT()
struct FSuspenseCoreQueuedOperation
{
    GENERATED_BODY()

    UPROPERTY()
    FEquipmentOperationRequest Request;

    UPROPERTY()
    float QueueTime = 0.0f;

    UPROPERTY()
    int32 Priority = 0;

    UPROPERTY()
    FGuid TransactionId;

    // Diagnostic flag
    bool bIsFromPool = false;

    bool operator<(const FSuspenseCoreQueuedOperation& Other) const
    {
        return Priority < Other.Priority;
    }

    void Reset()
    {
        Request = FEquipmentOperationRequest();
        QueueTime = 0.0f;
        Priority = 0;
        TransactionId = FGuid();
    }
};

/**
 * Operation history entry for undo/redo support
 */
USTRUCT()
struct FSuspenseCoreOperationHistoryEntry
{
    GENERATED_BODY()

    UPROPERTY()
    FEquipmentOperationRequest Request;

    UPROPERTY()
    FEquipmentOperationResult Result;

    UPROPERTY()
    FEquipmentStateSnapshot StateBefore;

    UPROPERTY()
    FEquipmentStateSnapshot StateAfter;

    UPROPERTY()
    FDateTime ExecutionTime;

    UPROPERTY()
    bool bCanUndo = false;
};

/**
 * Equipment Operation Service Implementation
 *
 * ARCHITECTURE PHILOSOPHY:
 * Thin coordinator orchestrating operations between subsystems:
 * - Operations Executor (plan building and validation)
 * - Transaction Manager (ACID transactions)
 * - Data Provider (state management)
 * - Rules Engine (business rules validation)
 * - Network Service (client-server communication)
 * - Prediction Manager (client-side prediction)
 *
 * KEY FEATURES:
 * - Transaction-based execution with plans
 * - Batch validation support
 * - Server authority with client prediction
 * - Queue management with priorities and coalescing
 * - Result and validation caching
 * - Event publishing after transaction commit
 * - History tracking for undo/redo
 * - Object pooling for GC optimization
 * - Comprehensive metrics and telemetry
 *
 * THREAD SAFETY:
 * Lock ordering: QueueLock → ExecutorLock → HistoryLock → StatsLock → PoolLocks
 * All public methods are thread-safe with granular locking
 */
UCLASS(Config=Game)
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentOperationService : public UObject,
    public IEquipmentOperationService
{
    GENERATED_BODY()

public:
    USuspenseCoreEquipmentOperationService();
    virtual ~USuspenseCoreEquipmentOperationService();

    //========================================
    // IEquipmentService Interface
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

    virtual ISuspenseEquipmentOperations* GetOperationsExecutor() override;
    virtual bool QueueOperation(const FEquipmentOperationRequest& Request) override;
    virtual void ProcessOperationQueue() override;

    //========================================
    // Ownership and Authority
    //========================================

    UFUNCTION(BlueprintCallable, Category = "Equipment|Operations")
    void InitializeWithOwner(APlayerState* InOwnerPS, bool bInServerAuthority);

    UFUNCTION(BlueprintCallable, Category = "Equipment|Operations", BlueprintPure)
    bool IsServerAuthority() const { return bServerAuthority; }

    UFUNCTION(BlueprintCallable, Category = "Equipment|Operations", BlueprintPure)
    FGuid GetOwnerPlayerGuid() const { return OwnerPlayerGuid; }

    //========================================
    // Operation Execution
    //========================================

    UFUNCTION(BlueprintCallable, Category = "Equipment|Operations")
    FEquipmentOperationResult ExecuteImmediate(const FEquipmentOperationRequest& Request);

    UFUNCTION(BlueprintCallable, Category = "Equipment|Operations")
    int32 QueueOperationWithPriority(const FEquipmentOperationRequest& Request, int32 Priority);

    UFUNCTION(BlueprintCallable, Category = "Equipment|Operations")
    FGuid BatchOperations(const TArray<FEquipmentOperationRequest>& Requests, bool bAtomic = true);

    UFUNCTION(BlueprintCallable, Category = "Equipment|Operations")
    FGuid BatchOperationsEx(const TArray<FEquipmentOperationRequest>& Requests, bool bAtomic,
                            TArray<FEquipmentOperationResult>& OutResults);

    //========================================
    // Queue Management
    //========================================

    UFUNCTION(BlueprintCallable, Category = "Equipment|Operations")
    bool CancelQueuedOperation(const FGuid& OperationId);

    UFUNCTION(BlueprintCallable, Category = "Equipment|Operations", BlueprintPure)
    int32 GetQueueSize() const;

    UFUNCTION(BlueprintCallable, Category = "Equipment|Operations")
    void ClearQueue(bool bForce = false);

    UFUNCTION(BlueprintCallable, Category = "Equipment|Operations")
    void SetQueueProcessingEnabled(bool bEnabled);

    //========================================
    // History and Undo/Redo
    //========================================

    UFUNCTION(BlueprintCallable, Category = "Equipment|History")
    FEquipmentOperationResult UndoLastOperation();

    UFUNCTION(BlueprintCallable, Category = "Equipment|History")
    FEquipmentOperationResult RedoLastOperation();

    //UFUNCTION(BlueprintCallable, Category = "Equipment|History")
    TArray<FSuspenseCoreOperationHistoryEntry> GetOperationHistory(int32 MaxCount = 10) const;

    UFUNCTION(BlueprintCallable, Category = "Equipment|History")
    void ClearHistory();

    UFUNCTION(BlueprintCallable, Category = "Equipment|History", BlueprintPure)
    bool CanUndo() const;

    UFUNCTION(BlueprintCallable, Category = "Equipment|History", BlueprintPure)
    bool CanRedo() const;

    //========================================
    // Metrics and Telemetry
    //========================================

    UFUNCTION(BlueprintCallable, Category = "Equipment|Metrics")
    bool ExportMetricsToCSV(const FString& FilePath) const;

    UFUNCTION(BlueprintCallable, Category = "Equipment|Metrics")
    void ResetMetrics();

    //========================================
    // Events
    //========================================

    DECLARE_MULTICAST_DELEGATE_OneParam(FOnOperationQueued, const FGuid& /*OperationId*/);
    FOnOperationQueued OnOperationQueued;

    DECLARE_MULTICAST_DELEGATE_OneParam(FOnOperationStarted, const FEquipmentOperationRequest&);
    FOnOperationStarted OnOperationStarted;

    DECLARE_MULTICAST_DELEGATE_OneParam(FOnOperationCompleted, const FEquipmentOperationResult&);
    FOnOperationCompleted OnOperationCompleted;

    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBatchCompleted, const FGuid& /*BatchId*/, bool /*Success*/);
    FOnBatchCompleted OnBatchCompleted;

    UFUNCTION(BlueprintCallable, Category="SuspenseCore|Equipment|Operations")
    void SetOperationsExecutor(TScriptInterface<ISuspenseEquipmentOperations> InExecutor);

protected:
    // Initialization
    bool InitializeDependencies();
    void SetupEventSubscriptions();
    void StartQueueProcessing();
    void StopQueueProcessing();
    void InitializeObjectPools();
    void CleanupObjectPools();

    // Network delegation
    bool ShouldDelegateToServer(const FEquipmentOperationRequest& Request) const;
    FEquipmentOperationResult DelegateOperationToServer(const FEquipmentOperationRequest& Request);

    // Prediction support
    void StartPrediction(const FEquipmentOperationRequest& Request);
    void ConfirmPrediction(const FGuid& OperationId, const FEquipmentOperationResult& ServerResult);

    // Object Pool Management
    FSuspenseCoreQueuedOperation* AcquireOperation();
    void ReleaseOperation(FSuspenseCoreQueuedOperation* Operation);
    FEquipmentOperationResult* AcquireResult();
    void ReleaseResult(FEquipmentOperationResult* Result);

    // Operation Processing - Core methods

    // Preflight validation hook (S8)
    bool PreflightRequests(const TArray<FSuspenseCoreQueuedOperation*>& BatchOps, TArray<FEquipmentOperationResult>* OutResults);

    FEquipmentOperationResult ProcessSingleOperation(FSuspenseCoreQueuedOperation* QueuedOp,
                                                     const FGuid& OuterTransactionId = FGuid());
    bool ProcessBatch(const TArray<FSuspenseCoreQueuedOperation*>& BatchOps, bool bAtomic,
                     TArray<FEquipmentOperationResult>* OutResults = nullptr);
    void ProcessQueueAsync();
    bool TickQueueFallback(float DeltaTime);

    // Queue optimization
    int32 TryCoalesceOperation(FSuspenseCoreQueuedOperation* NewOp);
    void OptimizeQueue();

    // Validation with enhanced caching
    FSlotValidationResult ValidateOperationCached(const FEquipmentOperationRequest& Request) const;
    uint32 GenerateValidationCacheKey(const FEquipmentOperationRequest& Request) const;
    void InvalidateValidationCache();

    // Transaction Management
    FGuid BeginOperationTransaction(const FEquipmentOperationRequest& Request,
                                   const FGuid& OuterTransactionId = FGuid());
    void CompleteTransaction(const FGuid& TransactionId, bool bSuccess, bool bIsOuter = false);

    // History Management
    void RecordOperation(const FEquipmentOperationRequest& Request,
                        const FEquipmentOperationResult& Result,
                        const FEquipmentStateSnapshot& StateBefore);
    void PruneHistory();

    // Event Handling
    void PublishOperationEvent(const FEquipmentOperationResult& Result);
    void OnValidationRulesChanged(const FSuspenseEquipmentEventData& EventData);
    void OnDataStateChanged(const FSuspenseEquipmentEventData& EventData);
    void OnNetworkOperationResult(const FSuspenseEquipmentEventData& EventData);

    // Statistics and Logging
    void UpdateStatistics(const FEquipmentOperationResult& Result);
    void LogOperation(const FEquipmentOperationRequest& Request, const FEquipmentOperationResult& Result) const;
    FString GetPoolStatistics() const;
    float GetPoolEfficiency() const;

    // Configuration validation
    void EnsureValidConfig();

    // Memory management
    void TrimPools(int32 KeepPerPool);

private:
    // NEW: Transaction Plan Support Methods
    FTransactionOperation MakeTxnOpFromStep(const FSuspenseCoreTransactionPlanStep& Step) const;
    bool BatchValidatePlan(const FSuspenseCoreTransactionPlan& Plan, FText& OutError) const;
    bool ExecutePlanTransactional(const FSuspenseCoreTransactionPlan& Plan, const FGuid& OuterTxnId,
                                  TArray<FEquipmentDelta>& OutDeltas);

    /**
    * Helper method to commit transaction with explicit deltas using new API
    * Falls back to old commit if new API is not available
    */
    bool CommitTransactionWithDeltas(const FGuid& TxnId, const TArray<FEquipmentDelta>& Deltas);

    // Legacy compatibility helpers
    bool BuildSingleStepPlanFromRequest(const FEquipmentOperationRequest& Request, FSuspenseCoreTransactionPlan& OutPlan) const;
    FSuspenseCoreTransactionPlanStep MakePlanStepFromRequest(const FEquipmentOperationRequest& Request) const;

    // Batch processing with unified plan path
    bool ProcessBatchUsingPlans(const TArray<FSuspenseCoreQueuedOperation*>& BatchOps,
                                bool bAtomic,
                                TArray<FEquipmentOperationResult>* OutResults);
    FGameplayTag MapOperationTypeToTag(EEquipmentOperationType OpType) const;

    // Service State
    UPROPERTY()
    EServiceLifecycleState ServiceState = EServiceLifecycleState::Uninitialized;

    UPROPERTY()
    FDateTime InitializationTime;

    // Ownership and Authority
    UPROPERTY()
    TWeakObjectPtr<APlayerState> OwnerPlayerState;

    UPROPERTY()
    FGuid OwnerPlayerGuid;

    UPROPERTY(EditAnywhere, Category = "Configuration")
    bool bServerAuthority = false;

    // Core Dependencies
    UPROPERTY()
    TScriptInterface<ISuspenseEquipmentOperations> OperationsExecutor;

    UPROPERTY()
    TScriptInterface<ISuspenseEquipmentDataProvider> DataProvider;

    UPROPERTY()
    TScriptInterface<ISuspenseTransactionManager> TransactionManager;

    UPROPERTY()
    TScriptInterface<ISuspenseEquipmentRules> RulesEngine;

    // Optional Dependencies
    TWeakObjectPtr<UObject> NetworkServiceObject;
    TScriptInterface<ISuspensePredictionManager> PredictionManager;

    // Prediction tracking
    TMap<FGuid, FGuid> OperationToPredictionMap;

    // Queue Management
    TArray<FSuspenseCoreQueuedOperation*> OperationQueue;
    TMap<FGuid, TArray<FSuspenseCoreQueuedOperation*>> ActiveBatches;
    bool bIsProcessingQueue = false;
    bool bQueueProcessingEnabled = true;
    bool bClearQueueAfterProcessing = false;
    FTimerHandle QueueProcessTimer;
    FTSTicker::FDelegateHandle TickerHandle;

    // Object Pools
    TQueue<FSuspenseCoreQueuedOperation*> OperationPool;
    TQueue<FEquipmentOperationResult*> ResultPool;
    static constexpr int32 MaxPoolSize = 100;
    static constexpr int32 InitialPoolSize = 50;

    // Pool Tracking
    std::atomic<int32> OperationPoolSize{0};
    std::atomic<int32> ResultPoolSize{0};
    mutable std::atomic<int32> OperationPoolHits{0};
    mutable std::atomic<int32> OperationPoolMisses{0};
    mutable std::atomic<int32> ResultPoolHits{0};
    mutable std::atomic<int32> ResultPoolMisses{0};
    mutable std::atomic<int32> PoolOverflows{0};

    // History Management
    TArray<FSuspenseCoreOperationHistoryEntry> OperationHistory;
    TArray<FSuspenseCoreOperationHistoryEntry> RedoStack;
    int32 MaxHistorySize = 50;

    // Caching
    TSharedPtr<FSuspenseEquipmentCacheManager<uint32, FSlotValidationResult>> ValidationCache;
    TSharedPtr<FSuspenseEquipmentCacheManager<FGuid, FEquipmentOperationResult>> ResultCache;
    float ValidationCacheTTL = 5.0f;
    float ResultCacheTTL = 2.0f;

    // Thread Safety
    mutable FRWLock QueueLock;
    mutable FRWLock ExecutorLock;
    mutable FRWLock HistoryLock;
    mutable FRWLock StatsLock;
    mutable FCriticalSection OperationPoolLock;
    mutable FCriticalSection ResultPoolLock;

    // Event Management
    FEventSubscriptionScope EventScope;
    TArray<FEventSubscriptionHandle> EventHandles;

    // Configuration
    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    int32 MaxQueueSize = 100;

    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    int32 BatchSize = 10;

    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    float QueueProcessInterval = 0.1f;

    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    bool bEnableBatching = true;

    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    bool bEnableDetailedLogging = false;

    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    bool bEnableObjectPooling = true;

    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    bool bEnableQueueCoalescing = true;

    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    int32 CoalescingLookback = 10;

    // NEW: Transaction Plan Support Flag
    UPROPERTY(EditDefaultsOnly, Config, Category = "Configuration")
    bool bUseTransactionPlans = true;

    // Legacy Statistics
    std::atomic<int32> TotalOperationsQueued{0};
    std::atomic<int32> TotalOperationsExecuted{0};
    std::atomic<int32> SuccessfulOperations{0};
    std::atomic<int32> FailedOperations{0};
    std::atomic<int32> CancelledOperations{0};
    std::atomic<int32> TotalBatchesProcessed{0};
    float CacheHitRate = 0.0f;
    float AverageQueueTime = 0.0f;
    float AverageExecutionTime = 0.0f;
    int32 PeakQueueSize = 0;

    // Unified Service Metrics
    mutable FSuspenseCoreServiceMetrics ServiceMetrics;

    // Validation service (for preflight batch checks)
    UPROPERTY(Transient)
    TWeakObjectPtr<USuspenseCoreEquipmentValidationService> ValidationServiceObject;

    // Service Locator reference (CRITICAL: stored from InitParams)
    UPROPERTY(Transient)
    TWeakObjectPtr<USuspenseEquipmentServiceLocator> CachedServiceLocator;

    // Helper to safely get service locator
    USuspenseEquipmentServiceLocator* GetServiceLocator() const;
};
