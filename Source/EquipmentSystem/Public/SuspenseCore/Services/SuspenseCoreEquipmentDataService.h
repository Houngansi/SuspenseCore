// SuspenseEquipmentDataService.h
// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentService.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentDataProvider.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreTransactionManager.h"
#include "SuspenseCore/Core/Utils/SuspenseCoreEquipmentCacheManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreLoadoutSettings.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceMacros.h"
#include "Delegates/Delegate.h"
#include "SuspenseCore/ItemSystem/SuspenseCoreItemManager.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCoreEquipmentDataService.generated.h"

// Forward declarations
class USuspenseCoreEquipmentDataStore;
class USuspenseCoreEquipmentTransactionProcessor;
class USuspenseCoreEquipmentSlotValidator;
struct FSuspenseCoreEquipmentDelta;
class USuspenseCoreItemManager;
/** Delegate for equipment delta notifications from service */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnServiceEquipmentDelta, const FEquipmentDelta&);

/** Delegate for batch delta notifications */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnServiceBatchDeltas, const TArray<FEquipmentDelta>&);

/**
 * Equipment Data Service - Foundation Layer for Equipment System
 *
 * Design Philosophy:
 * This service acts as the single source of truth for all equipment data in the game.
 * It coordinates between the DataStore (raw storage) and TransactionProcessor (ACID operations).
 *
 * Key Responsibilities:
 * 1. Lifecycle management of data components
 * 2. Transaction coordination for atomic operations
 * 3. Multi-level caching for performance
 * 4. Thread-safe data access patterns
 * 5. Event propagation for data changes
 * 6. DIFF-based change tracking and propagation
 *
 * Performance Considerations for MMO:
 * - Read operations are cached with TTL to minimize database hits
 * - Write operations are batched through transactions
 * - Snapshots are used for rollback and state synchronization
 * - Lock-free reads where possible, write locks are minimized
 * - Fine-grained DIFF events for efficient network replication
 *
 * The service does NOT:
 * - Implement business logic (that's for operation services)
 * - Handle networking (that's for network service)
 * - Manage UI state (that's for UI systems)
 *
 * Locking Policy:
 * - DataStore access is protected by DataLock (FRWLock).
 * - Cache access is protected by CacheLock (FRWLock).
 * - Never hold both locks simultaneously to avoid deadlocks.
 * - FRWLock is NOT recursive - avoid nested locks on same mutex.
 *
 * Non-Functional Guarantees:
 * - No business rules implemented here (coordination only).
 * - ACID at the coordinator level: commit/rollback enforced, no partial side effects.
 * - Threading contract: no recursive locking of the same FRWLock; DataLock and CacheLock never held together.
 * - Event dispatch happens on the Game Thread.
 * - DIFF events are generated for all state changes.
 * - Observability only (metrics/logs) – does not affect decisions.
 *
 * INITIALIZATION ORDER (CRITICAL):
 * 1. InjectComponents(DataStore, TransactionProcessor) - provides components
 * 2. SetValidator(SlotValidator) - optional but recommended for validation
 * 3. InitializeService(Params) - initializes the service with proper phases
 *
 * Breaking this order will cause initialization failures and undefined behavior.
 */

UCLASS()
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentDataService : public UObject,
    public ISuspenseCoreEquipmentDataServiceInterface
{
    GENERATED_BODY()

public:
    USuspenseCoreEquipmentDataService();
    virtual ~USuspenseCoreEquipmentDataService();

    //========================================
    // ISuspenseCoreEquipmentService Implementation
    //========================================

    virtual bool InitializeService(const FSuspenseCoreServiceInitParams& Params) override;
    virtual bool ShutdownService(bool bForce = false) override;
    virtual ESuspenseCoreServiceLifecycleState GetServiceState() const override { return ServiceState; }
    virtual bool IsServiceReady() const override { return ServiceState == ESuspenseCoreServiceLifecycleState::Ready; }
    virtual FGameplayTag GetServiceTag() const override;
    virtual FGameplayTagContainer GetRequiredDependencies() const override;
    virtual bool ValidateService(TArray<FText>& OutErrors) const override;
    virtual void ResetService() override;
    virtual FString GetServiceStats() const override;

    //========================================
    // ISuspenseEquipmentDataServiceInterface Implementation
    //========================================

    virtual ISuspenseCoreEquipmentDataProvider* GetDataProvider() override;
    virtual ISuspenseCoreTransactionManager* GetTransactionManager() override;

 //========================================
 // Component Injection and Configuration
 //========================================

 /**
  * Inject ready components into the service
  *
  * IMPLEMENTATION NOTE: Receives type-erased UObject* from interface,
  * casts to actual types here in SuspenseCoreEquipment module.
  *
  * This method is critical for proper architecture of the system.
  * The service does NOT create components, but receives them from outside already configured.
  * This guarantees component uniqueness and proper initialization order.
  *
  * @param InDataStore Ready data storage component (UObject* will be cast to USuspenseCoreEquipmentDataStore*)
  * @param InTransactionProcessor Ready transaction processing component (UObject* will be cast to USuspenseCoreEquipmentTransactionProcessor*)
  */
 virtual void InjectComponents(UObject* InDataStore, UObject* InTransactionProcessor) override;

 /**
  * Set validator for slot operations
  * Must be called before InitializeService for proper validation setup
  *
  * @param InValidator Validator component (UObject* will be cast to USuspenseCoreEquipmentSlotValidator*)
  */
 virtual void SetValidator(UObject* InValidator) override;

    //========================================
    // Data Operations (High-Level API)
    //========================================

    /**
     * Get item from slot with caching and thread-safe double-checked locking
     * @param SlotIndex Slot to query
     * @return Item instance (may be invalid if empty)
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Data")
    FSuspenseCoreInventoryItemInstance GetSlotItemCached(int32 SlotIndex) const;

    /**
     * Batch get multiple slot items with single lock acquisition
     * Optimized for mass reads in MMO scenarios
     * @param SlotIndices Array of slot indices to retrieve
     * @return Map of slot index to item instance
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Data")
    TMap<int32, FSuspenseCoreInventoryItemInstance> BatchGetSlotItems(const TArray<int32>& SlotIndices) const;

    /**
     * Perform atomic swap of items between slots
     * @param SlotA First slot
     * @param SlotB Second slot
     * @return Transaction ID for tracking
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Data")
    FGuid SwapSlotItems(int32 SlotA, int32 SlotB);

    /**
     * Batch update multiple slots in single transaction
     * @param Updates Map of slot index to new item
     * @return Transaction ID for tracking
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Data")
    FGuid BatchUpdateSlots(const TMap<int32, FSuspenseCoreInventoryItemInstance>& Updates);

    /**
     * Create checkpoint for rollback capability
     * @param CheckpointName Descriptive name
     * @return Checkpoint ID
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Data")
    FGuid CreateDataCheckpoint(const FString& CheckpointName);

    /**
     * Rollback to checkpoint
     * @param CheckpointId Checkpoint to restore
     * @return True if successful
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Data")
    bool RollbackToCheckpoint(const FGuid& CheckpointId);

    /**
     * Validate data integrity (public version that acquires locks)
     * @param bDeep Perform deep validation
     * @return Validation result
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Data")
    bool ValidateDataIntegrity(bool bDeep = false) const;

    /**
     * Force cache refresh for specific slot
     * @param SlotIndex Slot to refresh (-1 for all)
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Data")
    void InvalidateCache(int32 SlotIndex = -1);

    /**
     * Get cache statistics
     * @return Cache stats structure
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Data")
    FString GetCacheStatistics() const;

    /**
     * Export service metrics to CSV file
     * @param FilePath Absolute path to output file
     * @return True if export successful
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Data|Metrics")
    bool ExportMetricsToCSV(const FString& FilePath) const;

    //========================================
    // Delta Event Access
    //========================================

    /** Get delta event delegate */
    FOnServiceEquipmentDelta& OnEquipmentDelta() { return OnEquipmentDeltaDelegate; }

    /** Get batch delta event delegate */
    FOnServiceBatchDeltas& OnBatchDeltas() { return OnBatchDeltasDelegate; }

    /**
     * Get recent deltas
     * @param MaxCount Maximum number to return
     * @return Recent deltas in reverse chronological order
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Delta")
    TArray<FSuspenseCoreEquipmentDelta> GetRecentDeltas(int32 MaxCount = 10) const;

    /**
     * Get delta statistics
     * @return String with delta metrics
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Delta")
    FString GetDeltaStatistics() const;

protected:
    /**
     * Initialize data storage with default configuration
     */
    void InitializeDataStorage();

    /**
     * Setup event subscriptions for cache invalidation
     */
    void SetupEventSubscriptions();

    /**
     * Handle cache invalidation event (SuspenseCore EventBus)
     */
    void OnCacheInvalidation(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

    /**
     * Handle transaction completion
     */
    void OnTransactionCompleted(const FGuid& TransactionId, bool bSuccess);

    /**
     * Handle equipment delta from DataStore
     */
    void OnDataStoreDelta(const FEquipmentDelta& Delta);

    /**
     * Handle batch deltas from TransactionProcessor
     */
    void OnTransactionDeltas(const TArray<FEquipmentDelta>& Deltas);

    /**
     * Broadcast delta through event system
     */
    void BroadcastDelta(const FEquipmentDelta& Delta);

    /**
     * Broadcast batch deltas through event system
     */
    void BroadcastBatchDeltas(const TArray<FEquipmentDelta>& Deltas);

    /**
     * Handle external request to resend current state as deltas (SuspenseCore EventBus)
     */
    void OnResendRequested(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);


    /**
     * Perform deep validation of data consistency (public version that acquires locks)
     */
    bool PerformDeepValidation() const;

    /**
     * Internal deep validation (assumes DataLock already held by caller)
     */
    bool PerformDeepValidation_Internal() const;

    /**
     * Internal data integrity validation (assumes DataLock already held by caller)
     */
    bool ValidateDataIntegrity_Internal(bool bDeep) const;

    /**
     * Create default slot configuration for initialization
     */
    TArray<FEquipmentSlotConfig> CreateDefaultSlotConfiguration() const;

    /**
     * Update cache entry with TTL using proper locking
     */
    void UpdateCacheEntry(int32 SlotIndex, const FSuspenseCoreInventoryItemInstance& Item) const;

    /**
     * Batch update cache entries with single lock
     */
    void BatchUpdateCache(const TMap<int32, FSuspenseCoreInventoryItemInstance>& Items) const;

    /**
     * Log data operation for debugging
     */
    void LogDataOperation(const FString& Operation, int32 SlotIndex = -1) const;

    /**
     * Reset data store to initial state
     */
    void ResetDataStore();

    /**
     * Check if slot index is valid
     */
    bool IsValidSlotIndex(int32 SlotIndex) const;

    /** Safe cache warm-up without holding DataLock and CacheLock simultaneously */
    void WarmupCachesSafe();

    /** Flag showing that components were injected */
    bool bComponentsInjected = false;

private:
    //========================================
    // Service State
    //========================================

    /** Current service lifecycle state */
    UPROPERTY()
    ESuspenseCoreServiceLifecycleState ServiceState = ESuspenseCoreServiceLifecycleState::Uninitialized;

    /** Service initialization timestamp */
    UPROPERTY()
    FDateTime InitializationTime;

    //========================================
    // Core Components
    //========================================

    /** Data store component for raw storage */
    UPROPERTY()
    USuspenseCoreEquipmentDataStore* DataStore = nullptr;

    /** Transaction processor for ACID operations */
    UPROPERTY()
    USuspenseCoreEquipmentTransactionProcessor* TransactionProcessor = nullptr;

    /** Slot validator for business logic */
    UPROPERTY()
    USuspenseCoreEquipmentSlotValidator* SlotValidator = nullptr;

    /** Interface wrapper for data provider - for universal access */
    TScriptInterface<ISuspenseCoreEquipmentDataProvider> DataProviderInterface;

    /** Interface wrapper for transaction manager - for universal access */
    TScriptInterface<ISuspenseCoreTransactionManager> TransactionManagerInterface;

 UPROPERTY(Transient)
    TObjectPtr<USuspenseCoreItemManager> ItemManager = nullptr;
    //========================================
    // Thread Safety
    //========================================

    /** Read/write lock for data access */
    mutable FRWLock DataLock;

    /** Separate lock for cache operations */
    mutable FRWLock CacheLock;

    /** Lock for delta history */
    mutable FCriticalSection DeltaLock;

    //========================================
    // Caching Layer
    //========================================

    /** Cache for state snapshots */
    mutable TSharedPtr<FSuspenseCoreEquipmentCacheManager<FGuid, FEquipmentStateSnapshot>> SnapshotCache;

    /** Cache for individual item instances */
    mutable TSharedPtr<FSuspenseCoreEquipmentCacheManager<int32, FSuspenseCoreInventoryItemInstance>> ItemCache;

    /** Cache for slot configurations */
    mutable TSharedPtr<FSuspenseCoreEquipmentCacheManager<int32, FEquipmentSlotConfig>> ConfigCache;

    /** Default cache TTL in seconds */
    float DefaultCacheTTL = 60.0f;

    //========================================
    // Event Management (SuspenseCore architecture)
    //========================================

    /** Cached EventBus reference */
    UPROPERTY(Transient)
    TObjectPtr<USuspenseCoreEventBus> EventBus = nullptr;

    /** Handles for specific event subscriptions */
    TArray<FSuspenseCoreSubscriptionHandle> EventHandles;

    /** Delegate handle for global cache invalidation */
    FDelegateHandle GlobalCacheInvalidateHandle;

    /** Delegate for delta events */
    FOnServiceEquipmentDelta OnEquipmentDeltaDelegate;

    /** Delegate for batch delta events */
    FOnServiceBatchDeltas OnBatchDeltasDelegate;

    //========================================
    // Delta Management
    //========================================

    /** Recent delta history for debugging/replay */
    UPROPERTY()
    TArray<FSuspenseCoreEquipmentDelta> RecentDeltaHistory;

    /** Maximum deltas to keep in history */
    static constexpr int32 MaxDeltaHistory = 100;

    /** Total deltas processed */
    mutable std::atomic<int32> TotalDeltasProcessed{0};

    /** Deltas by type counter */
    mutable TMap<FGameplayTag, int32> DeltasByType;

    //========================================
    // Configuration
    //========================================

    /** Maximum number of slots supported */
    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    int32 MaxSlotCount = 20;

    /** Enable automatic cache warming */
    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    bool bEnableCacheWarming = true;

    /** Cache warming interval in seconds */
    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    float CacheWarmingInterval = 30.0f;

    /** Enable detailed logging for debugging */
    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    bool bEnableDetailedLogging = false;

    /** Enable delta tracking */
    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    bool bEnableDeltaTracking = true;

    //========================================
    // Statistics and Monitoring
    //========================================

    /** Total read operations performed */
    mutable std::atomic<int32> TotalReads{0};

    /** Total write operations performed */
    mutable std::atomic<int32> TotalWrites{0};

    /** Cache hit count */
    mutable std::atomic<int32> CacheHits{0};

    /** Cache miss count */
    mutable std::atomic<int32> CacheMisses{0};

    /** Cache contention count for monitoring lock conflicts */
    mutable std::atomic<int32> CacheContention{0};

    /** Total transactions started */
    mutable std::atomic<int32> TotalTransactions{0};

    /** Successful transactions */
    mutable std::atomic<int32> SuccessfulTransactions{0};

    /** Failed transactions */
    mutable std::atomic<int32> FailedTransactions{0};

    /** Last cache warming time */
    float LastCacheWarmingTime = 0.0f;

    /** Service metrics collection */
    mutable FSuspenseCoreServiceMetrics ServiceMetrics;

    /** Delta metrics collection */
    mutable FSuspenseCoreDeltaMetrics DeltaMetrics;
};
