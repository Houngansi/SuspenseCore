// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Interfaces/Equipment/IEquipmentService.h"
#include "Interfaces/Equipment/ISuspenseTransactionManager.h"
#include "Interfaces/Equipment/ISuspenseEquipmentRules.h"
#include "Interfaces/Equipment/ISuspenseEquipmentDataProvider.h"
#include "Core/Utils/SuspenseEquipmentCacheManager.h"
#include "Core/Utils/SuspenseEquipmentThreadGuard.h"
#include "Core/Utils/SuspenseEquipmentEventBus.h"
#include "Services/SuspenseEquipmentServiceMacros.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Types/Rules/SuspenseRulesTypes.h"
#include "Types/Equipment/SuspenseEquipmentTypes.h"
#include "HAL/CriticalSection.h"
#include <atomic>
#include "SuspenseEquipmentValidationService.generated.h"

/**
 * Buffered validation event for thread-safe dispatching
 */
struct FBufferedValidationEvent
{
    enum EEventType
    {
        Started,
        Completed,
        Failed,
        Custom
    };

    EEventType Type;
    FEquipmentOperationRequest Request;
    FSlotValidationResult Result;
    FGameplayTag CustomEventTag;

    FBufferedValidationEvent() : Type(Started) {}
};

/**
 * Shadow snapshot for batch validation
 * Represents temporary equipment state during validation
 */
struct FShadowEquipmentSnapshot
{
    TMap<int32, FSuspenseInventoryItemInstance> SlotItems;
    TMap<FName, int32> ItemQuantities;
    float TotalWeight = 0.0f;

    // Apply operation to shadow state without side effects
    bool ApplyOperation(const FEquipmentOperationRequest& Operation);

    // Check if slot is occupied
    bool IsSlotOccupied(int32 SlotIndex) const;

    // Get item at slot
    FSuspenseInventoryItemInstance GetItemAtSlot(int32 SlotIndex) const;
};

/**
 * Batch validation report
 */
USTRUCT(BlueprintType)
struct FBatchValidationReport
{
    GENERATED_BODY()

    UPROPERTY()
    bool bAllPassed = true;

    UPROPERTY()
    int32 TotalOperations = 0;

    UPROPERTY()
    int32 PassedOperations = 0;

    UPROPERTY()
    int32 FailedOperations = 0;

    UPROPERTY()
    TArray<FSlotValidationResult> Results;

    UPROPERTY()
    TArray<FText> Warnings;

    UPROPERTY()
    TArray<FText> HardErrors;
};

// Alias to keep external API consistent
using FSlotValidationBatchResult = FBatchValidationReport;


/**
 * Equipment Validation Service - Pure Coordination Layer
 */
UCLASS()
class EQUIPMENTSYSTEM_API USuspenseEquipmentValidationService : public UObject,
    public IEquipmentValidationService
{
    GENERATED_BODY()

public:
    USuspenseEquipmentValidationService();
    virtual ~USuspenseEquipmentValidationService();

    //========================================
    // IEquipmentService Implementation
    //========================================

    virtual bool InitializeService(const FServiceInitParams& Params) override;
    virtual bool ShutdownService(bool bForce = false) override;
    virtual EServiceLifecycleState GetServiceState() const override { return ServiceState; }
    virtual bool IsServiceReady() const override { return ServiceState == EServiceLifecycleState::Ready; }
    virtual FGameplayTag GetServiceTag() const override;
    virtual FGameplayTagContainer GetRequiredDependencies() const override;
    virtual bool ValidateService(TArray<FText>& OutErrors) const override;
    virtual void ResetService() override;
    virtual FString GetServiceStats() const override;

    //========================================
    // IEquipmentValidationService Implementation
    //========================================

    virtual ISuspenseEquipmentRules* GetRulesEngine() override;
    virtual bool RegisterValidator(const FGameplayTag& ValidatorTag, TFunction<bool(const void*)> Validator) override;
    virtual void ClearValidationCache() override;

    //========================================
    // Coordination API
    //========================================

    /**
     * S8: Preflight helper that mirrors batch validation with sane defaults.
     * Uses sequential order with shadow snapshot and stops on first failure.
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Validation")
    FORCEINLINE FBatchValidationReport Preflight(const TArray<FEquipmentOperationRequest>& Requests)
    {
        return BatchValidateEx(Requests, /*bFastPath=*/true, /*bServerAuthoritative=*/false, /*bStopOnFailure=*/true);
    }

    /**
     * Validate equipment operation - delegates to Rules
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Validation")
    FSlotValidationResult ValidateEquipmentOperation(const FEquipmentOperationRequest& Request);

    /**
     * Validate within transaction context (read-only in validation layer)
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Validation")
    FSlotValidationResult ValidateInTransaction(
        const FEquipmentOperationRequest& Request,
        const FGuid& TransactionId);

    /**
     * Detailed batch validation (C++ only). Renamed from BatchValidate(...) to avoid UHT overload ambiguity.
     */
	FSlotValidationBatchResult BatchValidateEx(
	    const TArray<FEquipmentOperationRequest>& Operations,
	    bool bFastPath = true,
	    bool bServerAuthoritative = false,
	    bool bStopOnFailure = false);

    /**
     * Batch validation for multiple operations
     * Uses shadow snapshots for sequential validation
     * Uses parallel processing for large non-atomic batches
     * (Blueprint-exposed, simple signature)
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Validation")
    TArray<FSlotValidationResult> BatchValidate(
        const TArray<FEquipmentOperationRequest>& Requests,
        bool bAtomic = false);

    /**
     * Batch validation with detailed report
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Validation")
    FBatchValidationReport BatchValidateWithReport(
        const TArray<FEquipmentOperationRequest>& Requests,
        bool bAtomic = false);

    /**
     * Async validation with callback (C++ only)
     */
    void ValidateAsync(
        const FEquipmentOperationRequest& Request,
        TFunction<void(const FSlotValidationResult&)> Callback);

    /**
     * Export metrics to CSV file
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Validation|Metrics")
    bool ExportMetricsToCSV(const FString& AbsoluteFilePath) const;

    //========================================
    // Events
    //========================================

    DECLARE_MULTICAST_DELEGATE_OneParam(FOnValidationStarted, const FEquipmentOperationRequest&);
    FOnValidationStarted OnValidationStarted;

    DECLARE_MULTICAST_DELEGATE_OneParam(FOnValidationCompleted, const FSlotValidationResult&);
    FOnValidationCompleted OnValidationCompleted;

    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnValidationFailed, const FEquipmentOperationRequest&, const FText&);
    FOnValidationFailed OnValidationFailed;

protected:
    /** Ensure configuration values are within safe bounds */
    void EnsureValidConfig();

    /** Initialize service dependencies */
    bool InitializeDependencies();

    /** Setup event subscriptions */
    void SetupEventSubscriptions();

    /** Generate stable cache key for request */
    uint32 GenerateCacheKey(const FEquipmentOperationRequest& Request) const;

    /** Get stable identifier for actor (for cache key) */
    uint32 GetStableActorIdentifier(AActor* Actor) const;

    /** Publish validation event (safe for game thread only) */
    void PublishValidationEvent(
        const FGameplayTag& EventType,
        const FEquipmentOperationRequest& Request,
        const FSlotValidationResult& Result);

    /** Determine failure type from rule evaluation result */
    EEquipmentValidationFailure DetermineFailureType(const FRuleEvaluationResult& RuleResult) const;

    /** Validate single request (thread-safe, with optional event broadcasting) */
    FSlotValidationResult ValidateSingleRequest(
        const FEquipmentOperationRequest& Request,
        bool bBroadcastEvents = true);

    /** Validate single request for parallel execution (no events) */
    FSlotValidationResult ValidateSingleRequestParallel(
        const FEquipmentOperationRequest& Request);

    /** Process validation batch in parallel */
    TArray<FSlotValidationResult> ProcessParallelBatch(
        const TArray<FEquipmentOperationRequest>& Requests);

    /** Process validation batch sequentially */
    TArray<FSlotValidationResult> ProcessSequentialBatch(
        const TArray<FEquipmentOperationRequest>& Requests,
        bool bStopOnFailure = false);

    /** Process batch with shadow snapshot (no side effects) */
    FBatchValidationReport ProcessBatchWithShadowSnapshot(
        const TArray<FEquipmentOperationRequest>& Requests);

    /** Validate operation against shadow snapshot */
    FSlotValidationResult ValidateAgainstShadowSnapshot(
        const FEquipmentOperationRequest& Request,
        const FShadowEquipmentSnapshot& Snapshot);

    /** Dispatch buffered events on game thread */
    void DispatchBufferedEvents(const TArray<FBufferedValidationEvent>& Events);

    /** Handle rules or configuration change */
    void OnRulesOrConfigChanged();

private:
    //========================================
    // Service State
    //========================================

    UPROPERTY()
    EServiceLifecycleState ServiceState = EServiceLifecycleState::Uninitialized;

    UPROPERTY()
    FDateTime InitializationTime;

    UPROPERTY(Transient)
    TWeakObjectPtr<USuspenseEquipmentServiceLocator> ServiceLocatorRef;
    //========================================
    // Core Dependencies
    //========================================

    /** Rules interface (coordinator) that performs actual validation */
    UPROPERTY()
    TScriptInterface<ISuspenseEquipmentRules> Rules;

    /** Data provider interface */
    UPROPERTY()
    TScriptInterface<ISuspenseEquipmentDataProvider> DataProvider;

    /** Transaction manager interface (optional) */
    UPROPERTY()
    TScriptInterface<ISuspenseTransactionManager> TransactionManager;

    /** Custom validators registered by external systems */
    TMap<FGameplayTag, TFunction<bool(const void*)>> CustomValidators;

    //========================================
    // Configuration
    //========================================

    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    float CacheTTL = 5.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    bool bEnableDetailedLogging = false;

    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    bool bEnableParallelValidation = true;

    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    int32 ParallelBatchThreshold = 10;

    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    int32 MaxParallelThreads = 4;

    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    bool bUseShadowSnapshot = true;

    //========================================
    // Thread Safety
    //========================================

    /** Lock for cache access */
    mutable FEquipmentRWLock CacheLock;

    /** Lock for statistics */
    mutable FEquipmentRWLock StatsLock;

    /** Lock for custom validators map */
    mutable FCriticalSection ValidatorsLock;

    //========================================
    // Versioning and Cache Management
    //========================================

    /** Rules version for cache invalidation */
    std::atomic<uint32> RulesEpoch{1};

    /** Result cache with versioning support */
    TSharedPtr<FEquipmentCacheManager<uint32, FSlotValidationResult>> ResultCache;

    //========================================
    // Event Management
    //========================================

    FEventSubscriptionScope EventScope;

    //========================================
    // Statistics (using atomics for lock-free updates)
    //========================================

    std::atomic<int32> TotalValidations{0};
    std::atomic<int32> CacheHits{0};
    std::atomic<int32> ValidationsPassed{0};
    std::atomic<int32> ValidationsFailed{0};
    std::atomic<int32> ParallelBatches{0};
    std::atomic<int32> SequentialBatches{0};
    std::atomic<int32> ShadowSnapshotBatches{0};

    // Timing statistics (still need lock for float operations)
    float AverageValidationTime = 0.0f;
    float PeakValidationTime = 0.0f;
    float AverageParallelBatchTime = 0.0f;
    float PeakParallelBatchTime = 0.0f;
    float LastParallelBatchTimeMs = 0.0f;
    float AverageShadowBatchTime = 0.0f;

    // Progress logging throttle (thread-safe)
    std::atomic<double> LastProgressLogTime{0.0};
    static constexpr double ProgressLogInterval = 0.25;

    //========================================
    // Service Metrics (mutable for const methods)
    //========================================

    mutable FServiceMetrics ServiceMetrics;
};
