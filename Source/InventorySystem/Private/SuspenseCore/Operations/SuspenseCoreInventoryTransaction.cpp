// SuspenseCoreInventoryTransaction.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Operations/SuspenseCoreInventoryTransaction.h"
#include "SuspenseCore/Components/SuspenseCoreInventoryComponent.h"
#include "SuspenseCore/Base/SuspenseCoreInventoryLogs.h"
#include "Engine/World.h"

USuspenseCoreInventoryTransaction::USuspenseCoreInventoryTransaction()
	: State(ESuspenseCoreTransactionState::None)
	, StartTime(0.0f)
{
}

bool USuspenseCoreInventoryTransaction::Begin(USuspenseCoreInventoryComponent* Inventory)
{
	if (!Inventory)
	{
		UE_LOG(LogSuspenseCoreInventoryTxn, Warning,
			TEXT("Cannot begin transaction: Invalid inventory"));
		return false;
	}

	if (State == ESuspenseCoreTransactionState::Active)
	{
		UE_LOG(LogSuspenseCoreInventoryTxn, Warning,
			TEXT("Cannot begin transaction: Already active"));
		return false;
	}

	TargetInventory = Inventory;
	TransactionID = FGuid::NewGuid();
	State = ESuspenseCoreTransactionState::Active;
	OperationLog.Empty();

	// Capture initial state
	BeginSnapshot = CaptureSnapshot();

	if (UWorld* World = Inventory->GetWorld())
	{
		StartTime = World->GetTimeSeconds();
	}

	FSuspenseCoreInventoryLogHelper::LogTransactionStarted(TransactionID);

	return true;
}

bool USuspenseCoreInventoryTransaction::Commit()
{
	if (State != ESuspenseCoreTransactionState::Active)
	{
		UE_LOG(LogSuspenseCoreInventoryTxn, Warning,
			TEXT("Cannot commit: Transaction not active"));
		return false;
	}

	// Transaction is already applied to inventory
	// Just update state and broadcast events
	State = ESuspenseCoreTransactionState::Committed;

	FSuspenseCoreInventoryLogHelper::LogTransactionCommitted(TransactionID);

	// Broadcast inventory updated event
	if (TargetInventory.IsValid())
	{
		TargetInventory->BroadcastInventoryUpdated();
	}

	return true;
}

bool USuspenseCoreInventoryTransaction::Rollback()
{
	if (State != ESuspenseCoreTransactionState::Active)
	{
		UE_LOG(LogSuspenseCoreInventoryTxn, Warning,
			TEXT("Cannot rollback: Transaction not active"));
		return false;
	}

	// Apply the begin snapshot to revert changes
	if (!ApplySnapshot(BeginSnapshot))
	{
		State = ESuspenseCoreTransactionState::Failed;
		UE_LOG(LogSuspenseCoreInventoryTxn, Error,
			TEXT("Failed to apply rollback snapshot"));
		return false;
	}

	State = ESuspenseCoreTransactionState::RolledBack;

	FSuspenseCoreInventoryLogHelper::LogTransactionRolledBack(TransactionID);

	return true;
}

void USuspenseCoreInventoryTransaction::Cancel()
{
	if (State == ESuspenseCoreTransactionState::Active)
	{
		State = ESuspenseCoreTransactionState::None;
		OperationLog.Empty();

		UE_LOG(LogSuspenseCoreInventoryTxn, Verbose,
			TEXT("Transaction cancelled: %s"), *TransactionID.ToString());
	}
}

void USuspenseCoreInventoryTransaction::LogOperation(const FSuspenseCoreTransactionEntry& Entry)
{
	if (State != ESuspenseCoreTransactionState::Active)
	{
		return;
	}

	OperationLog.Add(Entry);
}

void USuspenseCoreInventoryTransaction::LogAdd(FName ItemID, const FSuspenseCoreItemInstance& Instance)
{
	FSuspenseCoreTransactionEntry Entry;
	Entry.OperationType = ESuspenseCoreOperationType::Add;
	Entry.ItemID = ItemID;
	Entry.InstanceID = Instance.UniqueInstanceID;
	Entry.AfterState = Instance;

	if (TargetInventory.IsValid() && TargetInventory->GetWorld())
	{
		Entry.Timestamp = TargetInventory->GetWorld()->GetTimeSeconds();
	}

	LogOperation(Entry);
}

void USuspenseCoreInventoryTransaction::LogRemove(FName ItemID, const FSuspenseCoreItemInstance& Instance)
{
	FSuspenseCoreTransactionEntry Entry;
	Entry.OperationType = ESuspenseCoreOperationType::Remove;
	Entry.ItemID = ItemID;
	Entry.InstanceID = Instance.UniqueInstanceID;
	Entry.BeforeState = Instance;

	if (TargetInventory.IsValid() && TargetInventory->GetWorld())
	{
		Entry.Timestamp = TargetInventory->GetWorld()->GetTimeSeconds();
	}

	LogOperation(Entry);
}

void USuspenseCoreInventoryTransaction::LogMove(const FSuspenseCoreItemInstance& Instance, int32 NewSlot)
{
	FSuspenseCoreTransactionEntry Entry;
	Entry.OperationType = ESuspenseCoreOperationType::Move;
	Entry.ItemID = Instance.ItemID;
	Entry.InstanceID = Instance.UniqueInstanceID;
	Entry.BeforeState = Instance;
	Entry.AfterState = Instance;
	Entry.AfterState.SlotIndex = NewSlot;

	if (TargetInventory.IsValid() && TargetInventory->GetWorld())
	{
		Entry.Timestamp = TargetInventory->GetWorld()->GetTimeSeconds();
	}

	LogOperation(Entry);
}

void USuspenseCoreInventoryTransaction::LogRotate(const FSuspenseCoreItemInstance& Instance, int32 NewRotation)
{
	FSuspenseCoreTransactionEntry Entry;
	Entry.OperationType = ESuspenseCoreOperationType::Rotate;
	Entry.ItemID = Instance.ItemID;
	Entry.InstanceID = Instance.UniqueInstanceID;
	Entry.BeforeState = Instance;
	Entry.AfterState = Instance;
	Entry.AfterState.Rotation = NewRotation;

	if (TargetInventory.IsValid() && TargetInventory->GetWorld())
	{
		Entry.Timestamp = TargetInventory->GetWorld()->GetTimeSeconds();
	}

	LogOperation(Entry);
}

float USuspenseCoreInventoryTransaction::GetElapsedTime() const
{
	if (State == ESuspenseCoreTransactionState::None || !TargetInventory.IsValid())
	{
		return 0.0f;
	}

	if (UWorld* World = TargetInventory->GetWorld())
	{
		return World->GetTimeSeconds() - StartTime;
	}

	return 0.0f;
}

FString USuspenseCoreInventoryTransaction::GetDebugString() const
{
	FString StateStr;
	switch (State)
	{
	case ESuspenseCoreTransactionState::None: StateStr = TEXT("None"); break;
	case ESuspenseCoreTransactionState::Active: StateStr = TEXT("Active"); break;
	case ESuspenseCoreTransactionState::Committed: StateStr = TEXT("Committed"); break;
	case ESuspenseCoreTransactionState::RolledBack: StateStr = TEXT("RolledBack"); break;
	case ESuspenseCoreTransactionState::Failed: StateStr = TEXT("Failed"); break;
	}

	return FString::Printf(TEXT("Transaction[%s] State: %s, Operations: %d, Elapsed: %.2fs"),
		*TransactionID.ToString().Left(8),
		*StateStr,
		OperationLog.Num(),
		GetElapsedTime());
}

bool USuspenseCoreInventoryTransaction::ApplySnapshot(const FSuspenseCoreInventorySnapshot& Snapshot)
{
	if (!TargetInventory.IsValid())
	{
		return false;
	}

	// Clear current inventory (skip events during rollback)
	TargetInventory->Clear();

	// Restore all items from snapshot
	for (const FSuspenseCoreItemInstance& Item : Snapshot.Items)
	{
		TargetInventory->AddItemInstanceToSlot(Item, Item.SlotIndex);
	}

	return true;
}

FSuspenseCoreInventorySnapshot USuspenseCoreInventoryTransaction::CaptureSnapshot() const
{
	FSuspenseCoreInventorySnapshot Snapshot;

	if (!TargetInventory.IsValid())
	{
		return Snapshot;
	}

	Snapshot.Items = TargetInventory->GetAllItemInstances();
	Snapshot.CurrentWeight = TargetInventory->Execute_GetCurrentWeight(TargetInventory.Get());

	if (TargetInventory->GetWorld())
	{
		Snapshot.SnapshotTime = TargetInventory->GetWorld()->GetTimeSeconds();
	}

	return Snapshot;
}

// FSuspenseCoreTransactionScope implementation
FSuspenseCoreTransactionScope::FSuspenseCoreTransactionScope(USuspenseCoreInventoryComponent* Inventory)
	: Transaction(nullptr)
	, bSuccess(false)
{
	if (Inventory)
	{
		Transaction = NewObject<USuspenseCoreInventoryTransaction>();
		Transaction->Begin(Inventory);
	}
}

FSuspenseCoreTransactionScope::~FSuspenseCoreTransactionScope()
{
	if (Transaction && Transaction->IsActive())
	{
		if (bSuccess)
		{
			Transaction->Commit();
		}
		else
		{
			Transaction->Rollback();
		}
	}
}
