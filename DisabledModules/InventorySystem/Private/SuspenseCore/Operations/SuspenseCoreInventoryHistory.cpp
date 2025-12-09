// SuspenseCoreInventoryHistory.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Operations/SuspenseCoreInventoryHistory.h"
#include "SuspenseCore/Components/SuspenseCoreInventoryComponent.h"
#include "SuspenseCore/Base/SuspenseCoreInventoryLogs.h"

USuspenseCoreInventoryHistory::USuspenseCoreInventoryHistory()
	: MaxSize(50)
{
}

void USuspenseCoreInventoryHistory::Initialize(USuspenseCoreInventoryComponent* Inventory, int32 MaxHistorySize)
{
	TargetInventory = Inventory;
	MaxSize = FMath::Max(1, MaxHistorySize);
	ClearHistory();
}

void USuspenseCoreInventoryHistory::SetMaxHistorySize(int32 NewSize)
{
	MaxSize = FMath::Max(1, NewSize);
	EnforceMaxSize();
}

void USuspenseCoreInventoryHistory::RecordOperation(const FSuspenseCoreOperationRecord& Record)
{
	// Clear redo stack when new operation is recorded
	RedoStack.Empty();

	// Add to undo stack
	UndoStack.Add(Record);

	// Enforce size limit
	EnforceMaxSize();

	UE_LOG(LogSuspenseCoreInventoryOps, Verbose,
		TEXT("History recorded: %s for %s"),
		*Record.OperationID.ToString().Left(8),
		*Record.ItemID.ToString());
}

void USuspenseCoreInventoryHistory::RecordAdd(FName ItemID, const FGuid& InstanceID, int32 Slot, int32 Quantity)
{
	FSuspenseCoreOperationRecord Record = FSuspenseCoreOperationRecord::CreateAddRecord(ItemID, InstanceID, Slot, Quantity);
	RecordOperation(Record);
}

void USuspenseCoreInventoryHistory::RecordRemove(FName ItemID, const FGuid& InstanceID, int32 Slot, int32 Quantity)
{
	FSuspenseCoreOperationRecord Record = FSuspenseCoreOperationRecord::CreateRemoveRecord(ItemID, InstanceID, Slot, Quantity);
	RecordOperation(Record);
}

void USuspenseCoreInventoryHistory::RecordMove(const FGuid& InstanceID, int32 FromSlot, int32 ToSlot)
{
	FSuspenseCoreOperationRecord Record = FSuspenseCoreOperationRecord::CreateMoveRecord(InstanceID, FromSlot, ToSlot);
	RecordOperation(Record);
}

void USuspenseCoreInventoryHistory::RecordSwap(const FGuid& InstanceID1, const FGuid& InstanceID2, int32 Slot1, int32 Slot2)
{
	FSuspenseCoreOperationRecord Record;
	Record.OperationType = ESuspenseCoreOperationType::Swap;
	Record.InstanceID = InstanceID1;
	Record.SecondaryInstanceID = InstanceID2;
	Record.PreviousSlot = Slot1;
	Record.NewSlot = Slot2;
	Record.bSuccess = true;
	Record.ResultCode = ESuspenseCoreInventoryResult::Success;
	RecordOperation(Record);
}

void USuspenseCoreInventoryHistory::RecordRotate(const FGuid& InstanceID, int32 Slot, int32 OldRotation, int32 NewRotation)
{
	FSuspenseCoreOperationRecord Record;
	Record.OperationType = ESuspenseCoreOperationType::Rotate;
	Record.InstanceID = InstanceID;
	Record.NewSlot = Slot;
	Record.PreviousRotation = OldRotation;
	Record.NewRotation = NewRotation;
	Record.bSuccess = true;
	Record.ResultCode = ESuspenseCoreInventoryResult::Success;
	RecordOperation(Record);
}

bool USuspenseCoreInventoryHistory::CanUndo() const
{
	return UndoStack.Num() > 0;
}

bool USuspenseCoreInventoryHistory::CanRedo() const
{
	return RedoStack.Num() > 0;
}

bool USuspenseCoreInventoryHistory::Undo()
{
	if (!CanUndo())
	{
		return false;
	}

	FSuspenseCoreOperationRecord Record = UndoStack.Pop();

	if (!ExecuteUndo(Record))
	{
		// Put it back if undo failed
		UndoStack.Add(Record);
		return false;
	}

	// Move to redo stack
	RedoStack.Add(Record);

	UE_LOG(LogSuspenseCoreInventoryOps, Log,
		TEXT("Undone operation: %s"),
		*Record.OperationID.ToString().Left(8));

	return true;
}

bool USuspenseCoreInventoryHistory::Redo()
{
	if (!CanRedo())
	{
		return false;
	}

	FSuspenseCoreOperationRecord Record = RedoStack.Pop();

	if (!ExecuteRedo(Record))
	{
		// Put it back if redo failed
		RedoStack.Add(Record);
		return false;
	}

	// Move back to undo stack
	UndoStack.Add(Record);

	UE_LOG(LogSuspenseCoreInventoryOps, Log,
		TEXT("Redone operation: %s"),
		*Record.OperationID.ToString().Left(8));

	return true;
}

int32 USuspenseCoreInventoryHistory::UndoMultiple(int32 Count)
{
	int32 Undone = 0;
	for (int32 i = 0; i < Count && CanUndo(); ++i)
	{
		if (Undo())
		{
			++Undone;
		}
		else
		{
			break;
		}
	}
	return Undone;
}

int32 USuspenseCoreInventoryHistory::RedoMultiple(int32 Count)
{
	int32 Redone = 0;
	for (int32 i = 0; i < Count && CanRedo(); ++i)
	{
		if (Redo())
		{
			++Redone;
		}
		else
		{
			break;
		}
	}
	return Redone;
}

bool USuspenseCoreInventoryHistory::GetLastOperation(FSuspenseCoreOperationRecord& OutRecord) const
{
	if (UndoStack.Num() == 0)
	{
		return false;
	}

	OutRecord = UndoStack.Last();
	return true;
}

TArray<FSuspenseCoreOperationRecord> USuspenseCoreInventoryHistory::GetUndoHistory() const
{
	TArray<FSuspenseCoreOperationRecord> Result = UndoStack;
	Algo::Reverse(Result);
	return Result;
}

TArray<FSuspenseCoreOperationRecord> USuspenseCoreInventoryHistory::GetOperationsByType(ESuspenseCoreOperationType Type) const
{
	TArray<FSuspenseCoreOperationRecord> Result;
	for (const FSuspenseCoreOperationRecord& Record : UndoStack)
	{
		if (Record.OperationType == Type)
		{
			Result.Add(Record);
		}
	}
	return Result;
}

TArray<FSuspenseCoreOperationRecord> USuspenseCoreInventoryHistory::GetOperationsForItem(const FGuid& InstanceID) const
{
	TArray<FSuspenseCoreOperationRecord> Result;
	for (const FSuspenseCoreOperationRecord& Record : UndoStack)
	{
		if (Record.InstanceID == InstanceID || Record.SecondaryInstanceID == InstanceID)
		{
			Result.Add(Record);
		}
	}
	return Result;
}

void USuspenseCoreInventoryHistory::ClearHistory()
{
	UndoStack.Empty();
	RedoStack.Empty();
	SavePointID = FGuid();
}

void USuspenseCoreInventoryHistory::ClearRedoStack()
{
	RedoStack.Empty();
}

void USuspenseCoreInventoryHistory::MarkSavePoint()
{
	if (UndoStack.Num() > 0)
	{
		SavePointID = UndoStack.Last().OperationID;
	}
	else
	{
		SavePointID = FGuid();
	}
}

bool USuspenseCoreInventoryHistory::IsAtSavePoint() const
{
	if (!SavePointID.IsValid())
	{
		return UndoStack.Num() == 0;
	}

	if (UndoStack.Num() == 0)
	{
		return false;
	}

	return UndoStack.Last().OperationID == SavePointID;
}

FString USuspenseCoreInventoryHistory::GetDebugString() const
{
	return FString::Printf(TEXT("History: Undo=%d, Redo=%d, MaxSize=%d, AtSavePoint=%s"),
		UndoStack.Num(),
		RedoStack.Num(),
		MaxSize,
		IsAtSavePoint() ? TEXT("Yes") : TEXT("No"));
}

bool USuspenseCoreInventoryHistory::ExecuteUndo(const FSuspenseCoreOperationRecord& Record)
{
	if (!TargetInventory.IsValid())
	{
		return false;
	}

	USuspenseCoreInventoryComponent* Inv = TargetInventory.Get();

	switch (Record.OperationType)
	{
	case ESuspenseCoreOperationType::Add:
		// Undo add = remove
		return Inv->RemoveItemInstance(Record.InstanceID);

	case ESuspenseCoreOperationType::Remove:
	{
		// Undo remove = add back
		FSuspenseCoreItemInstance Instance(Record.ItemID, Record.Quantity);
		Instance.UniqueInstanceID = Record.InstanceID;
		Instance.SlotIndex = Record.PreviousSlot;
		return Inv->AddItemInstanceToSlot(Instance, Record.PreviousSlot);
	}

	case ESuspenseCoreOperationType::Move:
		// Undo move = move back
		return Inv->Execute_MoveItem(Inv, Record.NewSlot, Record.PreviousSlot);

	case ESuspenseCoreOperationType::Swap:
		// Undo swap = swap back (same operation)
		return Inv->SwapItems(Record.PreviousSlot, Record.NewSlot);

	case ESuspenseCoreOperationType::Rotate:
	{
		// Undo rotate = rotate back
		FSuspenseCoreItemInstance Instance;
		if (Inv->FindItemInstance(Record.InstanceID, Instance))
		{
			Instance.Rotation = Record.PreviousRotation;
			// Would need SetItemRotation method
		}
		return true;
	}

	default:
		return false;
	}
}

bool USuspenseCoreInventoryHistory::ExecuteRedo(const FSuspenseCoreOperationRecord& Record)
{
	if (!TargetInventory.IsValid())
	{
		return false;
	}

	USuspenseCoreInventoryComponent* Inv = TargetInventory.Get();

	switch (Record.OperationType)
	{
	case ESuspenseCoreOperationType::Add:
	{
		// Redo add = add
		FSuspenseCoreItemInstance Instance(Record.ItemID, Record.Quantity);
		Instance.UniqueInstanceID = Record.InstanceID;
		return Inv->AddItemInstanceToSlot(Instance, Record.NewSlot);
	}

	case ESuspenseCoreOperationType::Remove:
		// Redo remove = remove
		return Inv->RemoveItemInstance(Record.InstanceID);

	case ESuspenseCoreOperationType::Move:
		// Redo move = move forward
		return Inv->Execute_MoveItem(Inv, Record.PreviousSlot, Record.NewSlot);

	case ESuspenseCoreOperationType::Swap:
		// Redo swap = swap (same operation)
		return Inv->SwapItems(Record.PreviousSlot, Record.NewSlot);

	case ESuspenseCoreOperationType::Rotate:
	{
		// Redo rotate = rotate to new value
		FSuspenseCoreItemInstance Instance;
		if (Inv->FindItemInstance(Record.InstanceID, Instance))
		{
			Instance.Rotation = Record.NewRotation;
			// Would need SetItemRotation method
		}
		return true;
	}

	default:
		return false;
	}
}

void USuspenseCoreInventoryHistory::EnforceMaxSize()
{
	while (UndoStack.Num() > MaxSize)
	{
		UndoStack.RemoveAt(0);
	}
}
