// SuspenseCoreInventoryReplicator.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Network/SuspenseCoreInventoryReplicator.h"
#include "SuspenseCore/Components/SuspenseCoreInventoryComponent.h"
#include "SuspenseCore/Base/SuspenseCoreInventoryLogs.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

USuspenseCoreInventoryReplicator::USuspenseCoreInventoryReplicator()
	: ReplicationMode(ESuspenseCoreReplicationMode::ServerAuthoritative)
{
}

void USuspenseCoreInventoryReplicator::Initialize(USuspenseCoreInventoryComponent* Component, ESuspenseCoreReplicationMode Mode)
{
	TargetComponent = Component;
	ReplicationMode = Mode;
	DirtyItems.Empty();
	PendingPredictions.Empty();
	Stats.Reset();

	UE_LOG(LogSuspenseCoreInventoryNet, Log,
		TEXT("Replicator initialized for %s with mode %d"),
		Component ? (Component->GetOwner() ? *Component->GetOwner()->GetName() : TEXT("Unknown")) : TEXT("None"),
		static_cast<int32>(Mode));
}

void USuspenseCoreInventoryReplicator::SetReplicationMode(ESuspenseCoreReplicationMode NewMode)
{
	if (ReplicationMode != NewMode)
	{
		ReplicationMode = NewMode;
		// Force full sync on mode change
		RequestFullSync();
	}
}

void USuspenseCoreInventoryReplicator::RequestFullSync()
{
	if (!TargetComponent.IsValid())
	{
		return;
	}

	MarkAllDirty();
	FlushReplication();
	Stats.FullSyncCount++;

	UE_LOG(LogSuspenseCoreInventoryNet, Verbose, TEXT("Full sync requested"));
}

void USuspenseCoreInventoryReplicator::MarkItemDirty(const FGuid& InstanceID)
{
	if (!DirtyItems.Contains(InstanceID))
	{
		DirtyItems.Add(InstanceID);
	}
}

void USuspenseCoreInventoryReplicator::MarkAllDirty()
{
	if (!TargetComponent.IsValid())
	{
		return;
	}

	DirtyItems.Empty();
	TArray<FSuspenseCoreItemInstance> Items = TargetComponent->GetAllItemInstances();
	for (const FSuspenseCoreItemInstance& Item : Items)
	{
		DirtyItems.Add(Item.UniqueInstanceID);
	}
}

void USuspenseCoreInventoryReplicator::FlushReplication()
{
	if (!TargetComponent.IsValid() || DirtyItems.Num() == 0)
	{
		return;
	}

	// In actual implementation, this would trigger network replication
	// The FSuspenseCoreReplicatedInventory::MarkItemDirty handles FastArray serialization

	Stats.DeltaUpdateCount++;
	Stats.LastSyncTime = TargetComponent->GetWorld() ?
		TargetComponent->GetWorld()->GetTimeSeconds() : 0.0f;

	// Estimate bytes (rough calculation)
	Stats.BytesSent += DirtyItems.Num() * sizeof(FSuspenseCoreReplicatedItem);

	FSuspenseCoreInventoryLogHelper::LogReplication(TEXT("Flush"), DirtyItems.Num());

	DirtyItems.Empty();
}

void USuspenseCoreInventoryReplicator::BeginPrediction(const FGuid& PredictionID)
{
	if (!TargetComponent.IsValid())
	{
		return;
	}

	// Capture current state for potential rollback
	FSuspenseCoreInventorySnapshot Snapshot;
	Snapshot.Items = TargetComponent->GetAllItemInstances();
	Snapshot.CurrentWeight = TargetComponent->Execute_GetCurrentWeight(TargetComponent.Get());

	if (TargetComponent->GetWorld())
	{
		Snapshot.SnapshotTime = TargetComponent->GetWorld()->GetTimeSeconds();
	}

	PendingPredictions.Add(PredictionID, Snapshot);

	UE_LOG(LogSuspenseCoreInventoryNet, Verbose,
		TEXT("Prediction started: %s"), *PredictionID.ToString().Left(8));
}

void USuspenseCoreInventoryReplicator::EndPrediction(const FGuid& PredictionID, bool bWasCorrect)
{
	if (!PendingPredictions.Contains(PredictionID))
	{
		return;
	}

	if (!bWasCorrect && TargetComponent.IsValid())
	{
		// Rollback to saved state
		const FSuspenseCoreInventorySnapshot& Snapshot = PendingPredictions[PredictionID];

		TargetComponent->Clear();
		for (const FSuspenseCoreItemInstance& Item : Snapshot.Items)
		{
			TargetComponent->AddItemInstanceToSlot(Item, Item.SlotIndex);
		}

		UE_LOG(LogSuspenseCoreInventoryNet, Warning,
			TEXT("Prediction mismatch, rolled back: %s"), *PredictionID.ToString().Left(8));
	}
	else
	{
		UE_LOG(LogSuspenseCoreInventoryNet, Verbose,
			TEXT("Prediction confirmed: %s"), *PredictionID.ToString().Left(8));
	}

	PendingPredictions.Remove(PredictionID);
}

bool USuspenseCoreInventoryReplicator::IsPredictionPending(const FGuid& PredictionID) const
{
	return PendingPredictions.Contains(PredictionID);
}

void USuspenseCoreInventoryReplicator::Server_AddItem(FName ItemID, int32 Quantity, const FGuid& PredictionID)
{
	if (!TargetComponent.IsValid() || !IsServer())
	{
		return;
	}

	bool bSuccess = TargetComponent->Execute_AddItemByID(TargetComponent.Get(), ItemID, Quantity);

	// In full implementation, would send RPC to client with result
	// Client_PredictionResult(PredictionID, bSuccess, GetServerState());
}

void USuspenseCoreInventoryReplicator::Server_RemoveItem(const FGuid& InstanceID, int32 Quantity, const FGuid& PredictionID)
{
	if (!TargetComponent.IsValid() || !IsServer())
	{
		return;
	}

	bool bSuccess = TargetComponent->RemoveItemInstance(InstanceID);

	// Send result to client
}

void USuspenseCoreInventoryReplicator::Server_MoveItem(const FGuid& InstanceID, int32 ToSlot, const FGuid& PredictionID)
{
	if (!TargetComponent.IsValid() || !IsServer())
	{
		return;
	}

	FSuspenseCoreItemInstance Instance;
	if (TargetComponent->FindItemInstance(InstanceID, Instance))
	{
		TargetComponent->Execute_MoveItem(TargetComponent.Get(), Instance.SlotIndex, ToSlot);
	}
}

void USuspenseCoreInventoryReplicator::Client_PredictionResult(
	const FGuid& PredictionID, bool bSuccess,
	const TArray<FSuspenseCoreReplicatedItem>& ServerState)
{
	EndPrediction(PredictionID, bSuccess);

	if (!bSuccess && TargetComponent.IsValid())
	{
		// Apply authoritative server state
		TargetComponent->Clear();
		for (const FSuspenseCoreReplicatedItem& RepItem : ServerState)
		{
			FSuspenseCoreItemInstance Instance = RepItem.ToItemInstance();
			TargetComponent->AddItemInstanceToSlot(Instance, Instance.SlotIndex);
		}
	}
}

void USuspenseCoreInventoryReplicator::Client_FullStateSync(const FSuspenseCoreReplicatedInventory& ReplicatedState)
{
	if (!TargetComponent.IsValid())
	{
		return;
	}

	// Clear and apply full state
	TargetComponent->Clear();

	for (const FSuspenseCoreReplicatedItem& RepItem : ReplicatedState.Items)
	{
		FSuspenseCoreItemInstance Instance = RepItem.ToItemInstance();
		TargetComponent->AddItemInstanceToSlot(Instance, Instance.SlotIndex);
	}

	Stats.FullSyncCount++;
	Stats.BytesReceived += ReplicatedState.Items.Num() * sizeof(FSuspenseCoreReplicatedItem);

	UE_LOG(LogSuspenseCoreInventoryNet, Log,
		TEXT("Full state sync received: %d items"), ReplicatedState.Items.Num());
}

void USuspenseCoreInventoryReplicator::OnReplicatedItemAdded(const FSuspenseCoreReplicatedItem& Item)
{
	if (!TargetComponent.IsValid())
	{
		return;
	}

	FSuspenseCoreItemInstance Instance = Item.ToItemInstance();

	// Check if this is a new item or update
	FSuspenseCoreItemInstance Existing;
	if (!TargetComponent->FindItemInstance(Instance.UniqueInstanceID, Existing))
	{
		TargetComponent->AddItemInstanceToSlot(Instance, Instance.SlotIndex);
	}

	Stats.DeltaUpdateCount++;
}

void USuspenseCoreInventoryReplicator::OnReplicatedItemRemoved(const FSuspenseCoreReplicatedItem& Item)
{
	if (!TargetComponent.IsValid())
	{
		return;
	}

	TargetComponent->RemoveItemInstance(Item.InstanceID);
	Stats.DeltaUpdateCount++;
}

void USuspenseCoreInventoryReplicator::OnReplicatedItemChanged(const FSuspenseCoreReplicatedItem& Item)
{
	if (!TargetComponent.IsValid())
	{
		return;
	}

	// Update existing item with new data
	// Implementation would update the local item instance
	Stats.DeltaUpdateCount++;
}

FString USuspenseCoreInventoryReplicator::GetDebugString() const
{
	FString ModeStr;
	switch (ReplicationMode)
	{
	case ESuspenseCoreReplicationMode::FullSync: ModeStr = TEXT("FullSync"); break;
	case ESuspenseCoreReplicationMode::DeltaSync: ModeStr = TEXT("DeltaSync"); break;
	case ESuspenseCoreReplicationMode::OwnerAuthoritative: ModeStr = TEXT("OwnerAuth"); break;
	case ESuspenseCoreReplicationMode::ServerAuthoritative: ModeStr = TEXT("ServerAuth"); break;
	}

	return FString::Printf(
		TEXT("Replicator[%s] Mode=%s, DirtyItems=%d, PendingPredictions=%d, FullSyncs=%d, Deltas=%d"),
		TargetComponent.IsValid() ? TEXT("Valid") : TEXT("Invalid"),
		*ModeStr,
		DirtyItems.Num(),
		PendingPredictions.Num(),
		Stats.FullSyncCount,
		Stats.DeltaUpdateCount);
}

bool USuspenseCoreInventoryReplicator::IsServer() const
{
	if (!TargetComponent.IsValid())
	{
		return false;
	}

	UWorld* World = TargetComponent->GetWorld();
	if (!World)
	{
		return false;
	}

	return World->GetNetMode() != NM_Client;
}

APlayerController* USuspenseCoreInventoryReplicator::GetOwningController() const
{
	if (!TargetComponent.IsValid())
	{
		return nullptr;
	}

	AActor* Owner = TargetComponent->GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	if (APawn* Pawn = Cast<APawn>(Owner))
	{
		return Cast<APlayerController>(Pawn->GetController());
	}

	return nullptr;
}
