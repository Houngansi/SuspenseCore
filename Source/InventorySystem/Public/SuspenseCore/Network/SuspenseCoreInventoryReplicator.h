// SuspenseCoreInventoryReplicator.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "SuspenseCoreInventoryReplicator.generated.h"

// Forward declarations
class USuspenseCoreInventoryComponent;
class APlayerController;

/**
 * ESuspenseCoreReplicationMode
 *
 * Replication mode options.
 */
UENUM(BlueprintType)
enum class ESuspenseCoreReplicationMode : uint8
{
	/** Full state sync on every change */
	FullSync = 0			UMETA(DisplayName = "Full Sync"),
	/** Delta updates only */
	DeltaSync				UMETA(DisplayName = "Delta Sync"),
	/** Owner authoritative, others receive state */
	OwnerAuthoritative		UMETA(DisplayName = "Owner Authoritative"),
	/** Server authoritative, clients receive state */
	ServerAuthoritative		UMETA(DisplayName = "Server Authoritative")
};

/**
 * FSuspenseCoreReplicationStats
 *
 * Replication performance statistics.
 */
USTRUCT(BlueprintType)
struct INVENTORYSYSTEM_API FSuspenseCoreReplicationStats
{
	GENERATED_BODY()

	/** Number of full syncs */
	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 FullSyncCount;

	/** Number of delta updates */
	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 DeltaUpdateCount;

	/** Bytes sent (estimated) */
	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int64 BytesSent;

	/** Bytes received (estimated) */
	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int64 BytesReceived;

	/** Last sync time */
	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	float LastSyncTime;

	/** Average sync latency */
	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	float AverageSyncLatency;

	FSuspenseCoreReplicationStats()
		: FullSyncCount(0)
		, DeltaUpdateCount(0)
		, BytesSent(0)
		, BytesReceived(0)
		, LastSyncTime(0.0f)
		, AverageSyncLatency(0.0f)
	{
	}

	void Reset()
	{
		*this = FSuspenseCoreReplicationStats();
	}
};

/**
 * USuspenseCoreInventoryReplicator
 *
 * Handles network replication for inventory components.
 * Works with FFastArraySerializer for efficient delta sync.
 *
 * ARCHITECTURE:
 * - Manages FSuspenseCoreReplicatedInventory lifecycle
 * - Handles client prediction and reconciliation
 * - Optimizes bandwidth with delta compression
 *
 * USAGE:
 * Component automatically creates replicator when replicated.
 */
UCLASS(BlueprintType)
class INVENTORYSYSTEM_API USuspenseCoreInventoryReplicator : public UObject
{
	GENERATED_BODY()

public:
	USuspenseCoreInventoryReplicator();

	//==================================================================
	// Initialization
	//==================================================================

	/**
	 * Initialize replicator.
	 * @param Component Target inventory component
	 * @param Mode Replication mode
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Network")
	void Initialize(USuspenseCoreInventoryComponent* Component, ESuspenseCoreReplicationMode Mode);

	/**
	 * Set replication mode.
	 * @param NewMode New mode
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Network")
	void SetReplicationMode(ESuspenseCoreReplicationMode NewMode);

	/**
	 * Get replication mode.
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Network")
	ESuspenseCoreReplicationMode GetReplicationMode() const { return ReplicationMode; }

	//==================================================================
	// Replication Control
	//==================================================================

	/**
	 * Request full state sync.
	 * Forces complete inventory state to be sent.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Network")
	void RequestFullSync();

	/**
	 * Mark item dirty for replication.
	 * @param InstanceID Item that changed
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Network")
	void MarkItemDirty(const FGuid& InstanceID);

	/**
	 * Mark all items dirty.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Network")
	void MarkAllDirty();

	/**
	 * Flush pending replication.
	 * Forces immediate sync of dirty items.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Network")
	void FlushReplication();

	//==================================================================
	// Client Prediction
	//==================================================================

	/**
	 * Begin client prediction.
	 * Client-side changes are applied immediately but can be reconciled.
	 * @param PredictionID Unique prediction ID
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Network")
	void BeginPrediction(const FGuid& PredictionID);

	/**
	 * End client prediction with result.
	 * @param PredictionID Prediction to resolve
	 * @param bWasCorrect True if server confirmed prediction
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Network")
	void EndPrediction(const FGuid& PredictionID, bool bWasCorrect);

	/**
	 * Check if prediction is pending.
	 * @param PredictionID Prediction to check
	 * @return true if awaiting server response
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Network")
	bool IsPredictionPending(const FGuid& PredictionID) const;

	/**
	 * Get pending prediction count.
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Network")
	int32 GetPendingPredictionCount() const { return PendingPredictions.Num(); }

	//==================================================================
	// Server RPCs
	//==================================================================

	/**
	 * Server: Process add item request.
	 * @param ItemID Item to add
	 * @param Quantity Amount
	 * @param PredictionID Client prediction ID
	 */
	void Server_AddItem(FName ItemID, int32 Quantity, const FGuid& PredictionID);

	/**
	 * Server: Process remove item request.
	 * @param InstanceID Instance to remove
	 * @param Quantity Amount
	 * @param PredictionID Client prediction ID
	 */
	void Server_RemoveItem(const FGuid& InstanceID, int32 Quantity, const FGuid& PredictionID);

	/**
	 * Server: Process move item request.
	 * @param InstanceID Instance to move
	 * @param ToSlot Target slot
	 * @param PredictionID Client prediction ID
	 */
	void Server_MoveItem(const FGuid& InstanceID, int32 ToSlot, const FGuid& PredictionID);

	//==================================================================
	// Client RPCs
	//==================================================================

	/**
	 * Client: Receive prediction result.
	 * @param PredictionID Prediction resolved
	 * @param bSuccess Was prediction correct
	 * @param ServerState Correct server state (if misprediction)
	 */
	void Client_PredictionResult(const FGuid& PredictionID, bool bSuccess,
		const TArray<FSuspenseCoreReplicatedItem>& ServerState);

	/**
	 * Client: Receive full state sync.
	 * @param ReplicatedState Complete server state
	 */
	void Client_FullStateSync(const FSuspenseCoreReplicatedInventory& ReplicatedState);

	//==================================================================
	// Event Handlers
	//==================================================================

	/**
	 * Handle replicated item added.
	 * @param Item Added item
	 */
	void OnReplicatedItemAdded(const FSuspenseCoreReplicatedItem& Item);

	/**
	 * Handle replicated item removed.
	 * @param Item Removed item
	 */
	void OnReplicatedItemRemoved(const FSuspenseCoreReplicatedItem& Item);

	/**
	 * Handle replicated item changed.
	 * @param Item Changed item
	 */
	void OnReplicatedItemChanged(const FSuspenseCoreReplicatedItem& Item);

	//==================================================================
	// Statistics
	//==================================================================

	/**
	 * Get replication statistics.
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Network")
	FSuspenseCoreReplicationStats GetStatistics() const { return Stats; }

	/**
	 * Reset statistics.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Network")
	void ResetStatistics() { Stats.Reset(); }

	//==================================================================
	// Debug
	//==================================================================

	/**
	 * Get debug string.
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Network")
	FString GetDebugString() const;

protected:
	/** Target inventory component */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreInventoryComponent> TargetComponent;

	/** Replication mode */
	UPROPERTY()
	ESuspenseCoreReplicationMode ReplicationMode;

	/** Pending client predictions */
	UPROPERTY()
	TMap<FGuid, FSuspenseCoreInventorySnapshot> PendingPredictions;

	/** Items marked dirty for replication */
	TArray<FGuid> DirtyItems;

	/** Replication statistics */
	FSuspenseCoreReplicationStats Stats;

	/** Is server authority */
	bool IsServer() const;

	/** Get owning player controller */
	APlayerController* GetOwningController() const;
};
