// SuspenseCoreInventoryOperationTypes.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreInventoryTypes.h"
#include "SuspenseCoreInventoryOperationTypes.generated.h"

// Forward declarations
struct FSuspenseCoreItemInstance;

/**
 * ESuspenseCoreOperationType
 *
 * Types of inventory operations for tracking and undo.
 */
UENUM(BlueprintType)
enum class ESuspenseCoreOperationType : uint8
{
	None = 0				UMETA(DisplayName = "None"),
	Add						UMETA(DisplayName = "Add"),
	Remove					UMETA(DisplayName = "Remove"),
	Move					UMETA(DisplayName = "Move"),
	Swap					UMETA(DisplayName = "Swap"),
	Rotate					UMETA(DisplayName = "Rotate"),
	SplitStack				UMETA(DisplayName = "Split Stack"),
	MergeStack				UMETA(DisplayName = "Merge Stack"),
	UpdateQuantity			UMETA(DisplayName = "Update Quantity"),
	Transfer				UMETA(DisplayName = "Transfer"),
	Batch					UMETA(DisplayName = "Batch")
};

/**
 * FSuspenseCoreOperationContext
 *
 * Context for operation execution.
 * Provides metadata and configuration for operations.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreOperationContext
{
	GENERATED_BODY()

	/** Source inventory component (raw pointer, not UPROPERTY due to cross-module) */
	TWeakObjectPtr<UObject> SourceInventory;

	/** Target inventory component (for transfers) */
	TWeakObjectPtr<UObject> TargetInventory;

	/** Actor who initiated the operation */
	UPROPERTY(BlueprintReadWrite, Category = "Context")
	TWeakObjectPtr<AActor> Instigator;

	/** Unique operation ID for tracking */
	UPROPERTY(BlueprintReadOnly, Category = "Context")
	FGuid OperationID;

	/** Timestamp when operation was created */
	UPROPERTY(BlueprintReadOnly, Category = "Context")
	float Timestamp;

	/** Skip validation (use with caution) */
	UPROPERTY(BlueprintReadWrite, Category = "Context")
	bool bSkipValidation;

	/** Skip network replication */
	UPROPERTY(BlueprintReadWrite, Category = "Context")
	bool bSkipReplication;

	/** Skip event broadcasting */
	UPROPERTY(BlueprintReadWrite, Category = "Context")
	bool bSkipEvents;

	/** Is part of a batch operation */
	UPROPERTY(BlueprintReadOnly, Category = "Context")
	bool bIsBatchOperation;

	/** Custom tags for filtering/tracking */
	UPROPERTY(BlueprintReadWrite, Category = "Context")
	FGameplayTagContainer ContextTags;

	FSuspenseCoreOperationContext()
		: OperationID(FGuid::NewGuid())
		, Timestamp(0.0f)
		, bSkipValidation(false)
		, bSkipReplication(false)
		, bSkipEvents(false)
		, bIsBatchOperation(false)
	{
	}

	static FSuspenseCoreOperationContext Default()
	{
		FSuspenseCoreOperationContext Context;
		if (GWorld)
		{
			Context.Timestamp = GWorld->GetTimeSeconds();
		}
		return Context;
	}
};

/**
 * FSuspenseCoreMoveOperation
 *
 * Parameters for move operation.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreMoveOperation
{
	GENERATED_BODY()

	/** Instance being moved */
	UPROPERTY(BlueprintReadWrite, Category = "Move")
	FGuid InstanceID;

	/** Source slot */
	UPROPERTY(BlueprintReadWrite, Category = "Move")
	int32 FromSlot;

	/** Target slot */
	UPROPERTY(BlueprintReadWrite, Category = "Move")
	int32 ToSlot;

	/** Source grid position */
	UPROPERTY(BlueprintReadWrite, Category = "Move")
	FIntPoint FromGridPosition;

	/** Target grid position */
	UPROPERTY(BlueprintReadWrite, Category = "Move")
	FIntPoint ToGridPosition;

	/** Allow auto-rotation to fit */
	UPROPERTY(BlueprintReadWrite, Category = "Move")
	bool bAllowRotation;

	FSuspenseCoreMoveOperation()
		: FromSlot(-1)
		, ToSlot(-1)
		, FromGridPosition(FIntPoint::NoneValue)
		, ToGridPosition(FIntPoint::NoneValue)
		, bAllowRotation(true)
	{
	}
};

/**
 * FSuspenseCoreSwapOperation
 *
 * Parameters for swap operation.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreSwapOperation
{
	GENERATED_BODY()

	/** First item instance */
	UPROPERTY(BlueprintReadWrite, Category = "Swap")
	FGuid InstanceID1;

	/** Second item instance */
	UPROPERTY(BlueprintReadWrite, Category = "Swap")
	FGuid InstanceID2;

	/** First slot */
	UPROPERTY(BlueprintReadWrite, Category = "Swap")
	int32 Slot1;

	/** Second slot */
	UPROPERTY(BlueprintReadWrite, Category = "Swap")
	int32 Slot2;

	FSuspenseCoreSwapOperation()
		: Slot1(-1)
		, Slot2(-1)
	{
	}
};

/**
 * FSuspenseCoreRotateOperation
 *
 * Parameters for rotate operation.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreRotateOperation
{
	GENERATED_BODY()

	/** Instance being rotated */
	UPROPERTY(BlueprintReadWrite, Category = "Rotate")
	FGuid InstanceID;

	/** Slot containing item */
	UPROPERTY(BlueprintReadWrite, Category = "Rotate")
	int32 SlotIndex;

	/** Previous rotation (degrees) */
	UPROPERTY(BlueprintReadWrite, Category = "Rotate")
	int32 PreviousRotation;

	/** New rotation (degrees: 0, 90, 180, 270) */
	UPROPERTY(BlueprintReadWrite, Category = "Rotate")
	int32 NewRotation;

	FSuspenseCoreRotateOperation()
		: SlotIndex(-1)
		, PreviousRotation(0)
		, NewRotation(90)
	{
	}

	/** Get rotation delta */
	int32 GetRotationDelta() const
	{
		return (NewRotation - PreviousRotation + 360) % 360;
	}
};

/**
 * FSuspenseCoreStackOperation
 *
 * Parameters for stack split/merge operations.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreStackOperation
{
	GENERATED_BODY()

	/** Source instance ID */
	UPROPERTY(BlueprintReadWrite, Category = "Stack")
	FGuid SourceInstanceID;

	/** Target instance ID (for merge) */
	UPROPERTY(BlueprintReadWrite, Category = "Stack")
	FGuid TargetInstanceID;

	/** Source slot */
	UPROPERTY(BlueprintReadWrite, Category = "Stack")
	int32 SourceSlot;

	/** Target slot */
	UPROPERTY(BlueprintReadWrite, Category = "Stack")
	int32 TargetSlot;

	/** Quantity to transfer */
	UPROPERTY(BlueprintReadWrite, Category = "Stack")
	int32 Quantity;

	/** Is split operation (vs merge) */
	UPROPERTY(BlueprintReadWrite, Category = "Stack")
	bool bIsSplit;

	FSuspenseCoreStackOperation()
		: SourceSlot(-1)
		, TargetSlot(-1)
		, Quantity(1)
		, bIsSplit(true)
	{
	}
};

/**
 * FSuspenseCoreTransferOperation
 *
 * Parameters for transfer between inventories.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreTransferOperation
{
	GENERATED_BODY()

	/** Instance being transferred */
	UPROPERTY(BlueprintReadWrite, Category = "Transfer")
	FGuid InstanceID;

	/** Item ID (for quantity-based transfers) */
	UPROPERTY(BlueprintReadWrite, Category = "Transfer")
	FName ItemID;

	/** Source slot */
	UPROPERTY(BlueprintReadWrite, Category = "Transfer")
	int32 SourceSlot;

	/** Target slot (-1 for auto) */
	UPROPERTY(BlueprintReadWrite, Category = "Transfer")
	int32 TargetSlot;

	/** Quantity to transfer */
	UPROPERTY(BlueprintReadWrite, Category = "Transfer")
	int32 Quantity;

	/** Allow partial transfer if full quantity unavailable */
	UPROPERTY(BlueprintReadWrite, Category = "Transfer")
	bool bAllowPartial;

	FSuspenseCoreTransferOperation()
		: ItemID(NAME_None)
		, SourceSlot(-1)
		, TargetSlot(-1)
		, Quantity(1)
		, bAllowPartial(false)
	{
	}
};

/**
 * FSuspenseCoreOperationRecord
 *
 * Record of a completed operation for history/undo.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreOperationRecord
{
	GENERATED_BODY()

	/** Unique operation ID */
	UPROPERTY(BlueprintReadOnly, Category = "Record")
	FGuid OperationID;

	/** Operation type */
	UPROPERTY(BlueprintReadOnly, Category = "Record")
	ESuspenseCoreOperationType OperationType;

	/** Timestamp when operation was executed */
	UPROPERTY(BlueprintReadOnly, Category = "Record")
	float ExecutionTime;

	/** Affected item ID */
	UPROPERTY(BlueprintReadOnly, Category = "Record")
	FName ItemID;

	/** Affected instance ID */
	UPROPERTY(BlueprintReadOnly, Category = "Record")
	FGuid InstanceID;

	/** Secondary instance ID (for swaps) */
	UPROPERTY(BlueprintReadOnly, Category = "Record")
	FGuid SecondaryInstanceID;

	/** Previous slot */
	UPROPERTY(BlueprintReadOnly, Category = "Record")
	int32 PreviousSlot;

	/** New slot */
	UPROPERTY(BlueprintReadOnly, Category = "Record")
	int32 NewSlot;

	/** Quantity affected */
	UPROPERTY(BlueprintReadOnly, Category = "Record")
	int32 Quantity;

	/** Previous rotation */
	UPROPERTY(BlueprintReadOnly, Category = "Record")
	int32 PreviousRotation;

	/** New rotation */
	UPROPERTY(BlueprintReadOnly, Category = "Record")
	int32 NewRotation;

	/** Operation was successful */
	UPROPERTY(BlueprintReadOnly, Category = "Record")
	bool bSuccess;

	/** Result code */
	UPROPERTY(BlueprintReadOnly, Category = "Record")
	ESuspenseCoreInventoryResult ResultCode;

	/** Can be undone */
	UPROPERTY(BlueprintReadOnly, Category = "Record")
	bool bCanUndo;

	FSuspenseCoreOperationRecord()
		: OperationID(FGuid::NewGuid())
		, OperationType(ESuspenseCoreOperationType::None)
		, ExecutionTime(0.0f)
		, ItemID(NAME_None)
		, PreviousSlot(-1)
		, NewSlot(-1)
		, Quantity(0)
		, PreviousRotation(0)
		, NewRotation(0)
		, bSuccess(false)
		, ResultCode(ESuspenseCoreInventoryResult::Unknown)
		, bCanUndo(true)
	{
	}

	static FSuspenseCoreOperationRecord CreateAddRecord(FName InItemID, FGuid InInstanceID, int32 InSlot, int32 InQuantity)
	{
		FSuspenseCoreOperationRecord Record;
		Record.OperationType = ESuspenseCoreOperationType::Add;
		Record.ItemID = InItemID;
		Record.InstanceID = InInstanceID;
		Record.NewSlot = InSlot;
		Record.Quantity = InQuantity;
		Record.bSuccess = true;
		Record.ResultCode = ESuspenseCoreInventoryResult::Success;
		return Record;
	}

	static FSuspenseCoreOperationRecord CreateRemoveRecord(FName InItemID, FGuid InInstanceID, int32 InSlot, int32 InQuantity)
	{
		FSuspenseCoreOperationRecord Record;
		Record.OperationType = ESuspenseCoreOperationType::Remove;
		Record.ItemID = InItemID;
		Record.InstanceID = InInstanceID;
		Record.PreviousSlot = InSlot;
		Record.Quantity = InQuantity;
		Record.bSuccess = true;
		Record.ResultCode = ESuspenseCoreInventoryResult::Success;
		return Record;
	}

	static FSuspenseCoreOperationRecord CreateMoveRecord(FGuid InInstanceID, int32 InFromSlot, int32 InToSlot)
	{
		FSuspenseCoreOperationRecord Record;
		Record.OperationType = ESuspenseCoreOperationType::Move;
		Record.InstanceID = InInstanceID;
		Record.PreviousSlot = InFromSlot;
		Record.NewSlot = InToSlot;
		Record.bSuccess = true;
		Record.ResultCode = ESuspenseCoreInventoryResult::Success;
		return Record;
	}
};

/**
 * FSuspenseCoreBatchOperation
 *
 * Container for batch operations.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreBatchOperation
{
	GENERATED_BODY()

	/** Batch ID */
	UPROPERTY(BlueprintReadOnly, Category = "Batch")
	FGuid BatchID;

	/** Description of batch */
	UPROPERTY(BlueprintReadWrite, Category = "Batch")
	FString Description;

	/** Move operations */
	UPROPERTY(BlueprintReadWrite, Category = "Batch")
	TArray<FSuspenseCoreMoveOperation> MoveOperations;

	/** Swap operations */
	UPROPERTY(BlueprintReadWrite, Category = "Batch")
	TArray<FSuspenseCoreSwapOperation> SwapOperations;

	/** Rotate operations */
	UPROPERTY(BlueprintReadWrite, Category = "Batch")
	TArray<FSuspenseCoreRotateOperation> RotateOperations;

	/** Stack operations */
	UPROPERTY(BlueprintReadWrite, Category = "Batch")
	TArray<FSuspenseCoreStackOperation> StackOperations;

	/** Results for each sub-operation */
	UPROPERTY(BlueprintReadOnly, Category = "Batch")
	TArray<FSuspenseCoreOperationRecord> Results;

	/** Execute atomically (all or nothing) */
	UPROPERTY(BlueprintReadWrite, Category = "Batch")
	bool bAtomic;

	FSuspenseCoreBatchOperation()
		: BatchID(FGuid::NewGuid())
		, bAtomic(true)
	{
	}

	int32 GetOperationCount() const
	{
		return MoveOperations.Num() + SwapOperations.Num() +
			   RotateOperations.Num() + StackOperations.Num();
	}

	bool IsEmpty() const
	{
		return GetOperationCount() == 0;
	}
};
