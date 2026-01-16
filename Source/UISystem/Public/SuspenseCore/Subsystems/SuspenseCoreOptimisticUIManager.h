// SuspenseCoreOptimisticUIManager.h
// SuspenseCore - Optimistic UI Manager (AAA-Level Client Prediction)
// Copyright Suspense Team. All Rights Reserved.
//
// Implements Optimistic UI pattern (a.k.a. Client Prediction) for AAA-level
// responsiveness. Follows the proven pattern from MagazineComponent.
//
// KEY PRINCIPLE:
// UI updates IMMEDIATELY on user action, before server confirmation.
// If server rejects, we rollback to snapshot.
//
// FLOW:
// 1. User initiates action (drag-drop, equip, etc.)
// 2. CreatePrediction() - save snapshot of affected slots
// 3. Apply visual changes immediately (optimistic update)
// 4. Send request to server
// 5. Server responds:
//    - Success: ConfirmPrediction() - remove prediction (already correct)
//    - Failure: RollbackPrediction() - restore from snapshot
//
// BENEFITS:
// - Zero perceived latency for user actions
// - Smooth, responsive UI even with network lag
// - Automatic recovery from failed operations

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SuspenseCore/Types/UI/SuspenseCoreOptimisticUITypes.h"
#include "SuspenseCoreOptimisticUIManager.generated.h"

// Forward declarations
class USuspenseCoreEventBus;
class ISuspenseCoreUIContainer;

/**
 * USuspenseCoreOptimisticUIManager
 *
 * Centralized manager for Optimistic UI predictions.
 * Provides prediction creation, confirmation, and rollback.
 *
 * AAA-LEVEL FEATURES:
 * - Automatic timeout handling for stale predictions
 * - EventBus integration for cross-widget notifications
 * - Support for nested/chained predictions
 * - Comprehensive logging for debugging
 *
 * @see FSuspenseCoreUIPrediction
 * @see USuspenseCoreMagazineComponent - Reference implementation
 */
UCLASS()
class UISYSTEM_API USuspenseCoreOptimisticUIManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//==================================================================
	// Static Access
	//==================================================================

	/**
	 * Get OptimisticUIManager from world context
	 * @param WorldContext Object with world context
	 * @return Manager or nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|OptimisticUI", meta = (WorldContext = "WorldContext"))
	static USuspenseCoreOptimisticUIManager* Get(const UObject* WorldContext);

	//==================================================================
	// Lifecycle
	//==================================================================

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

	//==================================================================
	// Prediction Management
	//==================================================================

	/**
	 * Generate next unique prediction key
	 * @return Unique prediction key (monotonically increasing)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|OptimisticUI")
	int32 GeneratePredictionKey();

	/**
	 * Create a new prediction
	 * Stores the prediction for later confirmation/rollback.
	 *
	 * @param Prediction Prediction data with snapshots
	 * @return true if prediction was stored
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|OptimisticUI")
	bool CreatePrediction(const FSuspenseCoreUIPrediction& Prediction);

	/**
	 * Create move item prediction with automatic snapshot
	 * Convenience method that creates prediction and captures slot snapshots.
	 *
	 * @param Container Container widget
	 * @param SourceSlot Source slot index
	 * @param TargetSlot Target slot index
	 * @param bIsRotated Is item being rotated
	 * @return Prediction key, or INDEX_NONE if failed
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|OptimisticUI")
	int32 CreateMoveItemPrediction(
		TScriptInterface<ISuspenseCoreUIContainer> Container,
		int32 SourceSlot,
		int32 TargetSlot,
		bool bIsRotated);

	/**
	 * Confirm a prediction (server accepted)
	 * Removes prediction from pending list - visual state is already correct.
	 *
	 * @param PredictionKey Key of prediction to confirm
	 * @return true if prediction was found and confirmed
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|OptimisticUI")
	bool ConfirmPrediction(int32 PredictionKey);

	/**
	 * Rollback a prediction (server rejected)
	 * Restores all affected slots to their snapshot state.
	 *
	 * @param PredictionKey Key of prediction to rollback
	 * @param ErrorMessage Optional error message for feedback
	 * @return true if prediction was found and rolled back
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|OptimisticUI")
	bool RollbackPrediction(int32 PredictionKey, const FText& ErrorMessage = FText::GetEmpty());

	/**
	 * Process prediction result from server
	 * Routes to confirm or rollback based on result.
	 *
	 * @param Result Prediction result from server
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|OptimisticUI")
	void ProcessPredictionResult(const FSuspenseCoreUIPredictionResult& Result);

	//==================================================================
	// State Queries
	//==================================================================

	/**
	 * Check if there's an active prediction for a slot
	 * @param ContainerID Container ID
	 * @param SlotIndex Slot index
	 * @return true if slot has pending prediction
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|OptimisticUI")
	bool HasPendingPredictionForSlot(const FGuid& ContainerID, int32 SlotIndex) const;

	/**
	 * Get pending prediction count
	 * @return Number of pending predictions
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|OptimisticUI")
	int32 GetPendingPredictionCount() const { return PendingPredictions.Num(); }

	/**
	 * Get prediction by key
	 * @param PredictionKey Key to look up
	 * @return Prediction pointer or nullptr
	 */
	const FSuspenseCoreUIPrediction* GetPrediction(int32 PredictionKey) const;

	/**
	 * Check if prediction key exists
	 * @param PredictionKey Key to check
	 * @return true if prediction exists
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|OptimisticUI")
	bool HasPrediction(int32 PredictionKey) const;

	//==================================================================
	// Events
	//==================================================================

	/** Fired when prediction state changes */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|OptimisticUI")
	FSuspenseCoreOnPredictionStateChanged OnPredictionStateChanged;

	/** Fired when prediction result is received */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|OptimisticUI")
	FSuspenseCoreOnPredictionResult OnPredictionResult;

protected:
	//==================================================================
	// Internal Methods
	//==================================================================

	/**
	 * Apply rollback for a prediction
	 * Restores all affected slots from snapshots.
	 * @param Prediction Prediction to rollback
	 */
	void ApplyRollback(const FSuspenseCoreUIPrediction& Prediction);

	/**
	 * Check and expire stale predictions
	 * Called periodically to clean up.
	 */
	void CheckPredictionTimeouts();

	/**
	 * Broadcast prediction state change
	 * @param PredictionKey Key of prediction
	 * @param NewState New state
	 */
	void BroadcastStateChange(int32 PredictionKey, ESuspenseCoreUIPredictionState NewState);

	/**
	 * Publish feedback event to EventBus
	 * @param bSuccess Was operation successful
	 * @param Message Feedback message
	 */
	void PublishFeedbackEvent(bool bSuccess, const FText& Message);

	/**
	 * Get EventBus
	 * @return EventBus or nullptr
	 */
	USuspenseCoreEventBus* GetEventBus() const;

private:
	//==================================================================
	// State
	//==================================================================

	/** Next prediction key to use */
	int32 NextPredictionKey = 1;

	/** Map of pending predictions by key */
	UPROPERTY(Transient)
	TMap<int32, FSuspenseCoreUIPrediction> PendingPredictions;

	/** Cached EventBus reference */
	mutable TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	/** Timer handle for timeout checks */
	FTimerHandle TimeoutCheckHandle;

	//==================================================================
	// Configuration
	//==================================================================

	/** Interval for checking prediction timeouts (seconds) */
	static constexpr float TIMEOUT_CHECK_INTERVAL = 1.0f;

	/** Maximum pending predictions allowed */
	static constexpr int32 MAX_PENDING_PREDICTIONS = 32;
};
