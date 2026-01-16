// SuspenseCoreOptimisticUITypes.h
// SuspenseCore - Optimistic UI (Client Prediction) Types
// Copyright Suspense Team. All Rights Reserved.
//
// AAA-Level Optimistic UI Pattern:
// - Immediate visual feedback before server confirmation
// - Automatic rollback on server rejection
// - Snapshot-based state management
// - Follows pattern from MagazineComponent (Prediction system)

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreUITypes.h"
#include "SuspenseCoreOptimisticUITypes.generated.h"

/**
 * ESuspenseCoreUIPredictionState
 * State of a UI prediction operation
 */
UENUM(BlueprintType)
enum class ESuspenseCoreUIPredictionState : uint8
{
	/** Prediction not started */
	None = 0				UMETA(DisplayName = "None"),

	/** Prediction applied locally, awaiting server */
	Pending					UMETA(DisplayName = "Pending"),

	/** Server confirmed - prediction becomes permanent */
	Confirmed				UMETA(DisplayName = "Confirmed"),

	/** Server rejected - rollback applied */
	RolledBack				UMETA(DisplayName = "Rolled Back"),

	/** Prediction expired (timeout) */
	Expired					UMETA(DisplayName = "Expired")
};

/**
 * ESuspenseCoreUIPredictionType
 * Types of operations that can be predicted
 */
UENUM(BlueprintType)
enum class ESuspenseCoreUIPredictionType : uint8
{
	None = 0				UMETA(DisplayName = "None"),

	/** Move item within container */
	MoveItem				UMETA(DisplayName = "Move Item"),

	/** Rotate item */
	RotateItem				UMETA(DisplayName = "Rotate Item"),

	/** Transfer item between containers */
	TransferItem			UMETA(DisplayName = "Transfer Item"),

	/** Equip item to slot */
	EquipItem				UMETA(DisplayName = "Equip Item"),

	/** Unequip item from slot */
	UnequipItem				UMETA(DisplayName = "Unequip Item"),

	/** Stack items together */
	StackItems				UMETA(DisplayName = "Stack Items"),

	/** Split item stack */
	SplitStack				UMETA(DisplayName = "Split Stack"),

	/** Drop item to ground */
	DropItem				UMETA(DisplayName = "Drop Item"),

	/** Use/consume item */
	UseItem					UMETA(DisplayName = "Use Item")
};

/**
 * FSuspenseCoreSlotSnapshot
 * Snapshot of a single slot's state for rollback
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreSlotSnapshot
{
	GENERATED_BODY()

	/** Slot index */
	UPROPERTY(BlueprintReadOnly, Category = "Snapshot")
	int32 SlotIndex = INDEX_NONE;

	/** Slot state data */
	UPROPERTY(BlueprintReadOnly, Category = "Snapshot")
	FSuspenseCoreSlotUIData SlotData;

	/** Item data (if occupied) */
	UPROPERTY(BlueprintReadOnly, Category = "Snapshot")
	FSuspenseCoreItemUIData ItemData;

	/** Was slot occupied */
	UPROPERTY(BlueprintReadOnly, Category = "Snapshot")
	bool bWasOccupied = false;

	/** Multi-cell item size */
	UPROPERTY(BlueprintReadOnly, Category = "Snapshot")
	FIntPoint ItemGridSize = FIntPoint(1, 1);

	/** Was item rotated */
	UPROPERTY(BlueprintReadOnly, Category = "Snapshot")
	bool bWasRotated = false;

	/** Create snapshot from current slot state */
	static FSuspenseCoreSlotSnapshot Create(
		int32 InSlotIndex,
		const FSuspenseCoreSlotUIData& InSlotData,
		const FSuspenseCoreItemUIData& InItemData)
	{
		FSuspenseCoreSlotSnapshot Snapshot;
		Snapshot.SlotIndex = InSlotIndex;
		Snapshot.SlotData = InSlotData;
		Snapshot.ItemData = InItemData;
		Snapshot.bWasOccupied = InSlotData.IsOccupied();
		Snapshot.ItemGridSize = InItemData.GridSize;
		Snapshot.bWasRotated = InItemData.bIsRotated;
		return Snapshot;
	}
};

/**
 * FSuspenseCoreUIPrediction
 * Complete prediction data for optimistic UI operations.
 *
 * USAGE:
 * 1. Create prediction with unique key
 * 2. Store snapshots of affected slots
 * 3. Apply optimistic visual update
 * 4. On server confirm: remove prediction (state is correct)
 * 5. On server reject: rollback to snapshots
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreUIPrediction
{
	GENERATED_BODY()

	//==================================================================
	// Identity
	//==================================================================

	/** Unique prediction key (monotonically increasing) */
	UPROPERTY(BlueprintReadOnly, Category = "Prediction")
	int32 PredictionKey = INDEX_NONE;

	/** Type of operation being predicted */
	UPROPERTY(BlueprintReadOnly, Category = "Prediction")
	ESuspenseCoreUIPredictionType OperationType = ESuspenseCoreUIPredictionType::None;

	/** Current state of this prediction */
	UPROPERTY(BlueprintReadOnly, Category = "Prediction")
	ESuspenseCoreUIPredictionState State = ESuspenseCoreUIPredictionState::None;

	//==================================================================
	// Operation Data
	//==================================================================

	/** Source container ID */
	UPROPERTY(BlueprintReadOnly, Category = "Operation")
	FGuid SourceContainerID;

	/** Target container ID (for transfers) */
	UPROPERTY(BlueprintReadOnly, Category = "Operation")
	FGuid TargetContainerID;

	/** Source slot index */
	UPROPERTY(BlueprintReadOnly, Category = "Operation")
	int32 SourceSlot = INDEX_NONE;

	/** Target slot index */
	UPROPERTY(BlueprintReadOnly, Category = "Operation")
	int32 TargetSlot = INDEX_NONE;

	/** Item being operated on */
	UPROPERTY(BlueprintReadOnly, Category = "Operation")
	FGuid ItemInstanceID;

	/** Quantity being moved (for splits) */
	UPROPERTY(BlueprintReadOnly, Category = "Operation")
	int32 Quantity = 0;

	/** Rotation applied */
	UPROPERTY(BlueprintReadOnly, Category = "Operation")
	bool bIsRotated = false;

	//==================================================================
	// Snapshots for Rollback
	//==================================================================

	/** Snapshots of all affected slots (for rollback) */
	UPROPERTY(BlueprintReadOnly, Category = "Rollback")
	TArray<FSuspenseCoreSlotSnapshot> AffectedSlotSnapshots;

	//==================================================================
	// Timing
	//==================================================================

	/** When prediction was created (for timeout) */
	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	double CreationTime = 0.0;

	/** Timeout in seconds (default 5.0) */
	UPROPERTY(BlueprintReadOnly, Category = "Timing")
	float TimeoutSeconds = 5.0f;

	//==================================================================
	// Helpers
	//==================================================================

	/** Is this prediction valid */
	bool IsValid() const
	{
		return PredictionKey != INDEX_NONE &&
			   OperationType != ESuspenseCoreUIPredictionType::None &&
			   State != ESuspenseCoreUIPredictionState::None;
	}

	/** Is prediction still pending (awaiting server) */
	bool IsPending() const
	{
		return State == ESuspenseCoreUIPredictionState::Pending;
	}

	/** Has prediction expired */
	bool IsExpired() const
	{
		if (State != ESuspenseCoreUIPredictionState::Pending)
		{
			return false;
		}
		double CurrentTime = FPlatformTime::Seconds();
		return (CurrentTime - CreationTime) > TimeoutSeconds;
	}

	/** Add slot snapshot for rollback */
	void AddSlotSnapshot(const FSuspenseCoreSlotSnapshot& Snapshot)
	{
		AffectedSlotSnapshots.Add(Snapshot);
	}

	/** Find snapshot for slot */
	const FSuspenseCoreSlotSnapshot* FindSlotSnapshot(int32 SlotIndex) const
	{
		for (const FSuspenseCoreSlotSnapshot& Snapshot : AffectedSlotSnapshots)
		{
			if (Snapshot.SlotIndex == SlotIndex)
			{
				return &Snapshot;
			}
		}
		return nullptr;
	}

	/** Create a move item prediction */
	static FSuspenseCoreUIPrediction CreateMoveItem(
		int32 InPredictionKey,
		const FGuid& InContainerID,
		int32 InSourceSlot,
		int32 InTargetSlot,
		const FGuid& InItemID,
		bool bInIsRotated)
	{
		FSuspenseCoreUIPrediction Prediction;
		Prediction.PredictionKey = InPredictionKey;
		Prediction.OperationType = ESuspenseCoreUIPredictionType::MoveItem;
		Prediction.State = ESuspenseCoreUIPredictionState::Pending;
		Prediction.SourceContainerID = InContainerID;
		Prediction.TargetContainerID = InContainerID;
		Prediction.SourceSlot = InSourceSlot;
		Prediction.TargetSlot = InTargetSlot;
		Prediction.ItemInstanceID = InItemID;
		Prediction.bIsRotated = bInIsRotated;
		Prediction.CreationTime = FPlatformTime::Seconds();
		return Prediction;
	}

	/** Create a transfer item prediction */
	static FSuspenseCoreUIPrediction CreateTransferItem(
		int32 InPredictionKey,
		const FGuid& InSourceContainerID,
		const FGuid& InTargetContainerID,
		int32 InSourceSlot,
		int32 InTargetSlot,
		const FGuid& InItemID,
		int32 InQuantity)
	{
		FSuspenseCoreUIPrediction Prediction;
		Prediction.PredictionKey = InPredictionKey;
		Prediction.OperationType = ESuspenseCoreUIPredictionType::TransferItem;
		Prediction.State = ESuspenseCoreUIPredictionState::Pending;
		Prediction.SourceContainerID = InSourceContainerID;
		Prediction.TargetContainerID = InTargetContainerID;
		Prediction.SourceSlot = InSourceSlot;
		Prediction.TargetSlot = InTargetSlot;
		Prediction.ItemInstanceID = InItemID;
		Prediction.Quantity = InQuantity;
		Prediction.CreationTime = FPlatformTime::Seconds();
		return Prediction;
	}

	/** Create an equip item prediction */
	static FSuspenseCoreUIPrediction CreateEquipItem(
		int32 InPredictionKey,
		const FGuid& InInventoryID,
		const FGuid& InEquipmentID,
		int32 InSourceSlot,
		int32 InEquipmentSlot,
		const FGuid& InItemID)
	{
		FSuspenseCoreUIPrediction Prediction;
		Prediction.PredictionKey = InPredictionKey;
		Prediction.OperationType = ESuspenseCoreUIPredictionType::EquipItem;
		Prediction.State = ESuspenseCoreUIPredictionState::Pending;
		Prediction.SourceContainerID = InInventoryID;
		Prediction.TargetContainerID = InEquipmentID;
		Prediction.SourceSlot = InSourceSlot;
		Prediction.TargetSlot = InEquipmentSlot;
		Prediction.ItemInstanceID = InItemID;
		Prediction.CreationTime = FPlatformTime::Seconds();
		return Prediction;
	}
};

/**
 * FSuspenseCoreUIPredictionResult
 * Result of a prediction confirmation/rejection from server
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreUIPredictionResult
{
	GENERATED_BODY()

	/** Prediction key that was processed */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 PredictionKey = INDEX_NONE;

	/** Was prediction successful */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bSuccess = false;

	/** Error message if failed */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FText ErrorMessage;

	/** Error tag for categorization */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FGameplayTag ErrorTag;

	/** Create success result */
	static FSuspenseCoreUIPredictionResult Success(int32 InKey)
	{
		FSuspenseCoreUIPredictionResult Result;
		Result.PredictionKey = InKey;
		Result.bSuccess = true;
		return Result;
	}

	/** Create failure result */
	static FSuspenseCoreUIPredictionResult Failure(int32 InKey, const FText& InError, const FGameplayTag& InErrorTag = FGameplayTag())
	{
		FSuspenseCoreUIPredictionResult Result;
		Result.PredictionKey = InKey;
		Result.bSuccess = false;
		Result.ErrorMessage = InError;
		Result.ErrorTag = InErrorTag;
		return Result;
	}
};

/**
 * Delegate for prediction state changes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FSuspenseCoreOnPredictionStateChanged,
	int32, PredictionKey,
	ESuspenseCoreUIPredictionState, NewState
);

/**
 * Delegate for prediction result (confirm/reject)
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FSuspenseCoreOnPredictionResult,
	const FSuspenseCoreUIPredictionResult&, Result
);
