// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Interfaces/Equipment/ISuspenseEquipmentService.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreEquipmentServiceMacros.h"
#include "SuspenseCoreEquipmentDataService.generated.h"

// Forward declarations
class USuspenseEquipmentServiceLocator;
class USuspenseCoreEventBus;
class USuspenseEquipmentDataStore;
class USuspenseEquipmentTransactionProcessor;
class ISuspenseEquipmentDataProvider;
class ISuspenseTransactionManager;
struct FEquipmentStateSnapshot;

/**
 * SuspenseCoreEquipmentDataService
 *
 * Philosophy:
 * Single Source of Truth (SSOT) for all equipment data.
 * Manages equipment state through DataStore and TransactionProcessor components.
 *
 * Key Responsibilities:
 * - Equipment data storage and retrieval
 * - Transaction management for ACID operations
 * - Data state snapshots for rollback
 * - Change notification through events
 * - Thread-safe data access
 * - Cache management for performance
 *
 * Architecture Patterns:
 * - Event Bus: Publishes data change events
 * - Dependency Injection: Receives components via InjectComponents()
 * - GameplayTags: Service identification
 * - SSOT: Authoritative source for equipment data
 * - ACID: Transaction semantics for data modifications
 *
 * Initialization Order (CRITICAL):
 * 1. InjectComponents(DataStore, TransactionProcessor) - inject core components
 * 2. SetValidator(Validator) - optional validation component
 * 3. InitializeService(Params) - complete initialization
 */
UCLASS(BlueprintType)
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentDataService : public UObject, public IEquipmentDataService
{
	GENERATED_BODY()

public:
	USuspenseCoreEquipmentDataService();
	virtual ~USuspenseCoreEquipmentDataService();

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
	// IEquipmentDataService Interface
	//========================================

	virtual void InjectComponents(UObject* InDataStore, UObject* InTransactionProcessor) override;
	virtual void SetValidator(UObject* InValidator) override;
	virtual ISuspenseEquipmentDataProvider* GetDataProvider() override;
	virtual ISuspenseTransactionManager* GetTransactionManager() override;

	//========================================
	// Data Access (Thread-Safe)
	//========================================

	/**
	 * Get equipment data for slot
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Data")
	struct FSuspenseInventoryItemInstance GetSlotData(int32 SlotIndex) const;

	/**
	 * Set equipment data for slot
	 * Publishes data changed event
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Data")
	bool SetSlotData(int32 SlotIndex, const struct FSuspenseInventoryItemInstance& ItemData);

	/**
	 * Swap items between two slots
	 * Atomic operation with transaction support
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Data")
	bool SwapSlots(int32 SlotA, int32 SlotB);

	/**
	 * Clear slot data
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Data")
	bool ClearSlot(int32 SlotIndex);

	/**
	 * Get all slot data
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Data")
	TArray<FSuspenseInventoryItemInstance> GetAllSlotData() const;

	//========================================
	// Transaction Management
	//========================================

	/**
	 * Begin transaction for multiple operations
	 * Returns transaction ID for commit/rollback
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Transaction")
	FGuid BeginTransaction(const FString& Description);

	/**
	 * Commit transaction
	 * Publishes batch data changed events
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Transaction")
	bool CommitTransaction(const FGuid& TransactionId);

	/**
	 * Rollback transaction
	 * Reverts all changes made in transaction
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Transaction")
	bool RollbackTransaction(const FGuid& TransactionId);

	//========================================
	// State Management
	//========================================

	/**
	 * Create state snapshot for rollback
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|State")
	FGuid CreateSnapshot(const FString& SnapshotName);

	/**
	 * Restore from snapshot
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|State")
	bool RestoreSnapshot(const FGuid& SnapshotId);

	/**
	 * Get current equipment state
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|State")
	FEquipmentStateSnapshot GetCurrentState() const;

	/**
	 * Validate data integrity
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|State")
	bool ValidateDataIntegrity(TArray<FText>& OutErrors) const;

	//========================================
	// Cache Management
	//========================================

	/**
	 * Invalidate cache for slot
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Cache")
	void InvalidateSlotCache(int32 SlotIndex);

	/**
	 * Clear all caches
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Cache")
	void ClearAllCaches();

	/**
	 * Get cache statistics
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Cache")
	FString GetCacheStats() const;

	//========================================
	// Event Publishing
	//========================================

	/**
	 * Publish data changed event
	 */
	void PublishDataChanged(int32 SlotIndex);

	/**
	 * Publish batch data changed event
	 */
	void PublishBatchDataChanged(const TArray<int32>& SlotIndices);

protected:
	//========================================
	// Service Lifecycle
	//========================================

	/** Initialize data storage components */
	bool InitializeDataStorage();

	/** Setup event subscriptions */
	void SetupEventSubscriptions();

	/** Cleanup resources */
	void CleanupResources();

	//========================================
	// Data Operations
	//========================================

	/** Internal data access with locking */
	struct FSuspenseInventoryItemInstance GetSlotDataInternal(int32 SlotIndex) const;

	/** Internal data modification with locking */
	bool SetSlotDataInternal(int32 SlotIndex, const struct FSuspenseInventoryItemInstance& ItemData);

	/** Validate slot index */
	bool IsValidSlotIndex(int32 SlotIndex) const;

	//========================================
	// Event Handlers
	//========================================

	/** Handle transaction completed event */
	void OnTransactionCompleted(const FGuid& TransactionId, bool bSuccess);

	/** Handle cache invalidation request */
	void OnCacheInvalidationRequested(const struct FSuspenseEquipmentEventData& EventData);

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

	/** Components injected flag */
	bool bComponentsInjected;

	//========================================
	// Core Components (Injected)
	//========================================

	/** DataStore component - manages raw data storage */
	UPROPERTY()
	TObjectPtr<USuspenseEquipmentDataStore> DataStore;

	/** TransactionProcessor - manages ACID transactions */
	UPROPERTY()
	TObjectPtr<USuspenseEquipmentTransactionProcessor> TransactionProcessor;

	/** Validator - optional slot validation */
	UPROPERTY()
	TObjectPtr<UObject> SlotValidator;

	//========================================
	// Dependencies (via ServiceLocator)
	//========================================

	/** ServiceLocator for dependency injection */
	UPROPERTY()
	TWeakObjectPtr<USuspenseEquipmentServiceLocator> ServiceLocator;

	/** EventBus for event publishing */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> EventBus;

	//========================================
	// Thread Safety
	//========================================

	/** Read/Write lock for data access */
	mutable FRWLock DataLock;

	/** Lock for cache operations */
	mutable FRWLock CacheLock;

	//========================================
	// Configuration
	//========================================

	/** Maximum number of slots */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	int32 MaxSlotCount;

	/** Enable caching */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	bool bEnableCaching;

	/** Cache TTL in seconds */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	float CacheTTL;

	/** Enable detailed logging */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	bool bEnableDetailedLogging;

	//========================================
	// Statistics
	//========================================

	/** Total read operations */
	UPROPERTY()
	int32 TotalReads;

	/** Total write operations */
	UPROPERTY()
	int32 TotalWrites;

	/** Total transactions */
	UPROPERTY()
	int32 TotalTransactions;

	/** Cache hit count */
	UPROPERTY()
	int32 CacheHits;

	/** Cache miss count */
	UPROPERTY()
	int32 CacheMisses;
};
