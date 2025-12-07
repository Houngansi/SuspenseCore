// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GameplayTagContainer.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "SuspenseCoreEquipmentDataStore.generated.h"

// Forward declarations
class USuspenseCoreEventBus;
class USuspenseCoreServiceLocator;
struct FSuspenseCoreEventData;

/**
 * FSuspenseCoreEquipmentCacheEntry
 *
 * Cached equipment data entry with timestamp
 */
USTRUCT()
struct FSuspenseCoreEquipmentCacheEntry
{
	GENERATED_BODY()

	/** Cached item instance */
	UPROPERTY()
	FSuspenseInventoryItemInstance ItemInstance;

	/** Cache timestamp */
	UPROPERTY()
	float CacheTime = 0.0f;

	/** Cache version */
	UPROPERTY()
	int32 Version = 0;

	/** Is cache entry valid */
	UPROPERTY()
	bool bIsValid = false;

	/** Check if cache is expired */
	bool IsExpired(float CurrentTime, float MaxAge) const
	{
		return !bIsValid || (CurrentTime - CacheTime) > MaxAge;
	}

	/** Invalidate cache entry */
	void Invalidate()
	{
		bIsValid = false;
	}
};

/**
 * FSuspenseCoreEquipmentSnapshot
 *
 * Complete snapshot of equipment state for save/load
 */
USTRUCT(BlueprintType)
struct EQUIPMENTSYSTEM_API FSuspenseCoreEquipmentSnapshot
{
	GENERATED_BODY()

	/** Snapshot ID */
	UPROPERTY(BlueprintReadWrite, Category = "Snapshot")
	FGuid SnapshotId;

	/** Snapshot timestamp */
	UPROPERTY(BlueprintReadWrite, Category = "Snapshot")
	FDateTime Timestamp;

	/** All equipped items */
	UPROPERTY(BlueprintReadWrite, Category = "Snapshot")
	TMap<int32, FSuspenseInventoryItemInstance> EquippedItems;

	/** Active equipment tags */
	UPROPERTY(BlueprintReadWrite, Category = "Snapshot")
	FGameplayTagContainer ActiveTags;

	/** Custom metadata */
	UPROPERTY(BlueprintReadWrite, Category = "Snapshot")
	TMap<FName, FString> Metadata;

	/** Create new snapshot */
	static FSuspenseCoreEquipmentSnapshot Create()
	{
		FSuspenseCoreEquipmentSnapshot Snapshot;
		Snapshot.SnapshotId = FGuid::NewGuid();
		Snapshot.Timestamp = FDateTime::Now();
		return Snapshot;
	}
};

/**
 * USuspenseCoreEquipmentDataStore
 *
 * Local data storage with caching for equipment system.
 *
 * Architecture:
 * - EventBus: Publishes data change events
 * - ServiceLocator: Dependency injection for service access
 * - GameplayTags: Data categorization and event routing
 * - Caching: Multi-level cache with TTL and invalidation
 *
 * Responsibilities:
 * - Store and retrieve equipment item data
 * - Manage data cache with expiration
 * - Create and restore equipment snapshots
 * - Publish data change events through EventBus
 * - Support data queries with cache hits
 */
UCLASS(BlueprintType)
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentDataStore : public UObject
{
	GENERATED_BODY()

public:
	USuspenseCoreEquipmentDataStore();

	//================================================
	// Initialization
	//================================================

	/**
	 * Initialize data store with dependencies
	 * @param InServiceLocator ServiceLocator for dependency injection
	 * @return True if initialized successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DataStore")
	bool Initialize(USuspenseCoreServiceLocator* InServiceLocator);

	/**
	 * Shutdown data store and cleanup
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DataStore")
	void Shutdown();

	/**
	 * Check if data store is initialized
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|DataStore")
	bool IsInitialized() const { return bIsInitialized; }

	//================================================
	// Data Storage Operations
	//================================================

	/**
	 * Store equipment item data
	 * @param SlotIndex Equipment slot
	 * @param ItemInstance Item to store
	 * @return True if stored successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DataStore|Storage")
	bool StoreItemData(int32 SlotIndex, const FSuspenseInventoryItemInstance& ItemInstance);

	/**
	 * Retrieve equipment item data
	 * @param SlotIndex Equipment slot
	 * @param OutItem Output item instance
	 * @return True if item retrieved
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|DataStore|Storage")
	bool RetrieveItemData(int32 SlotIndex, FSuspenseInventoryItemInstance& OutItem) const;

	/**
	 * Remove item data from slot
	 * @param SlotIndex Equipment slot
	 * @return True if removed successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DataStore|Storage")
	bool RemoveItemData(int32 SlotIndex);

	/**
	 * Check if slot has data
	 * @param SlotIndex Equipment slot
	 * @return True if slot has stored data
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|DataStore|Storage")
	bool HasItemData(int32 SlotIndex) const;

	/**
	 * Get all stored item slots
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|DataStore|Storage")
	TArray<int32> GetStoredSlots() const;

	/**
	 * Clear all stored data
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DataStore|Storage")
	void ClearAllData();

	//================================================
	// Cache Management
	//================================================

	/**
	 * Update cache for slot
	 * @param SlotIndex Equipment slot
	 * @param ItemInstance Item to cache
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DataStore|Cache")
	void UpdateCache(int32 SlotIndex, const FSuspenseInventoryItemInstance& ItemInstance);

	/**
	 * Invalidate cache for slot
	 * @param SlotIndex Equipment slot
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DataStore|Cache")
	void InvalidateCache(int32 SlotIndex);

	/**
	 * Invalidate all caches
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DataStore|Cache")
	void InvalidateAllCaches();

	/**
	 * Get cache hit rate
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|DataStore|Cache")
	float GetCacheHitRate() const;

	/**
	 * Clean expired cache entries
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DataStore|Cache")
	void CleanExpiredCaches();

	//================================================
	// Snapshot Operations
	//================================================

	/**
	 * Create snapshot of current equipment state
	 * @return Equipment snapshot
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DataStore|Snapshot")
	FSuspenseCoreEquipmentSnapshot CreateSnapshot();

	/**
	 * Restore equipment state from snapshot
	 * @param Snapshot Snapshot to restore
	 * @return True if restored successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DataStore|Snapshot")
	bool RestoreSnapshot(const FSuspenseCoreEquipmentSnapshot& Snapshot);

	/**
	 * Save snapshot to persistent storage
	 * @param Snapshot Snapshot to save
	 * @param SaveName Name for saved snapshot
	 * @return True if saved successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DataStore|Snapshot")
	bool SaveSnapshot(const FSuspenseCoreEquipmentSnapshot& Snapshot, const FString& SaveName);

	/**
	 * Load snapshot from persistent storage
	 * @param SaveName Name of saved snapshot
	 * @param OutSnapshot Output snapshot
	 * @return True if loaded successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DataStore|Snapshot")
	bool LoadSnapshot(const FString& SaveName, FSuspenseCoreEquipmentSnapshot& OutSnapshot);

	//================================================
	// Query Operations
	//================================================

	/**
	 * Find items by tag
	 * @param Tag Tag to search for
	 * @return Array of matching items with their slots
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|DataStore|Query")
	TMap<int32, FSuspenseInventoryItemInstance> FindItemsByTag(FGameplayTag Tag) const;

	/**
	 * Find items by type
	 * @param ItemType Item type to search for
	 * @return Array of matching items with their slots
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|DataStore|Query")
	TMap<int32, FSuspenseInventoryItemInstance> FindItemsByType(FName ItemType) const;

	/**
	 * Get total stored items count
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|DataStore|Query")
	int32 GetStoredItemCount() const { return StoredItems.Num(); }

	//================================================
	// Statistics
	//================================================

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|DataStore|Stats")
	int32 GetTotalReads() const { return TotalReads; }

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|DataStore|Stats")
	int32 GetTotalWrites() const { return TotalWrites; }

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|DataStore|Stats")
	int32 GetCacheHits() const { return CacheHits; }

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|DataStore|Stats")
	int32 GetCacheMisses() const { return CacheMisses; }

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DataStore|Stats")
	void ResetStatistics();

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|DataStore|Stats")
	FString GetDataStoreStats() const;

protected:
	//================================================
	// Event Publishing
	//================================================

	/** Publish data changed event */
	void PublishDataChanged(int32 SlotIndex, bool bAdded);

	/** Publish cache invalidated event */
	void PublishCacheInvalidated(int32 SlotIndex);

	/** Publish snapshot created event */
	void PublishSnapshotCreated(const FSuspenseCoreEquipmentSnapshot& Snapshot);

	//================================================
	// Internal Methods
	//================================================

	/** Get or create cache entry */
	FSuspenseCoreEquipmentCacheEntry& GetOrCreateCacheEntry(int32 SlotIndex);

	/** Check if cache is valid */
	bool IsCacheValid(int32 SlotIndex) const;

	/** Update cache statistics */
	void UpdateCacheStats(bool bHit);

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
	// Data Storage
	//================================================

	/** Stored equipment items by slot */
	UPROPERTY()
	TMap<int32, FSuspenseInventoryItemInstance> StoredItems;

	/** Cache entries by slot */
	UPROPERTY()
	TMap<int32, FSuspenseCoreEquipmentCacheEntry> CacheEntries;

	//================================================
	// State
	//================================================

	/** Initialization flag */
	UPROPERTY()
	bool bIsInitialized;

	/** Data version for change tracking */
	UPROPERTY()
	int32 DataVersion;

	//================================================
	// Configuration
	//================================================

	/** Cache max age in seconds */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	float CacheMaxAge;

	/** Enable automatic cache cleanup */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	bool bEnableAutoCacheCleanup;

	/** Cache cleanup interval in seconds */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	float CacheCleanupInterval;

	//================================================
	// Statistics
	//================================================

	/** Total read operations */
	UPROPERTY()
	int32 TotalReads;

	/** Total write operations */
	UPROPERTY()
	int32 TotalWrites;

	/** Cache hits */
	UPROPERTY()
	int32 CacheHits;

	/** Cache misses */
	UPROPERTY()
	int32 CacheMisses;

	/** Last cache cleanup time */
	UPROPERTY()
	float LastCacheCleanupTime;

	//================================================
	// Thread Safety
	//================================================

	/** Critical section for thread-safe data access */
	mutable FCriticalSection DataCriticalSection;
};
