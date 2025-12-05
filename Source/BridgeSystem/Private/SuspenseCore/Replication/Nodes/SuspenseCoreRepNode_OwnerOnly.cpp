// SuspenseCoreRepNode_OwnerOnly.cpp
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Replication/Nodes/SuspenseCoreRepNode_OwnerOnly.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

// ═══════════════════════════════════════════════════════════════════════════════
// CONSTRUCTION
// ═══════════════════════════════════════════════════════════════════════════════

USuspenseCoreRepNode_OwnerOnly::USuspenseCoreRepNode_OwnerOnly()
{
	bRequiresPrepareForReplicationCall = false;
}

// ═══════════════════════════════════════════════════════════════════════════════
// UReplicationGraphNode Interface
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreRepNode_OwnerOnly::NotifyAddNetworkActor(const FNewReplicatedActorInfo& ActorInfo)
{
	AActor* Actor = ActorInfo.Actor;
	if (Actor && !OwnerActors.Contains(Actor))
	{
		// Only add if this actor belongs to our connection
		if (IsActorOwnedByConnection(Actor))
		{
			OwnerActors.Add(Actor);
		}
	}
}

bool USuspenseCoreRepNode_OwnerOnly::NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& ActorInfo, bool bWarnIfNotFound)
{
	AActor* Actor = ActorInfo.Actor;
	if (Actor)
	{
		const int32 NumRemoved = OwnerActors.Remove(Actor);
		return NumRemoved > 0;
	}
	return false;
}

void USuspenseCoreRepNode_OwnerOnly::GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params)
{
	// Only add actors if this is the owning connection
	if (!OwningConnection.IsValid())
	{
		return;
	}

	// Check if current connection matches our owner
	bool bIsOwner = false;
	if (Params.ConnectionManager.ConnectionOrderNum < Params.ReplicationGraph->Connections.Num())
	{
		UNetReplicationGraphConnection* CurrentConnection = Params.ReplicationGraph->Connections[Params.ConnectionManager.ConnectionOrderNum];
		bIsOwner = (CurrentConnection == OwningConnection.Get());
	}

	if (!bIsOwner)
	{
		return;
	}

	// Add all owner actors to the replication list
	for (TObjectPtr<AActor>& Actor : OwnerActors)
	{
		if (Actor && IsValid(Actor))
		{
			Params.OutGatheredReplicationLists.AddActor(Actor, Params.ConnectionManager);
		}
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// Per-Connection Setup
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreRepNode_OwnerOnly::SetOwningConnection(UNetReplicationGraphConnection* InConnection)
{
	OwningConnection = InConnection;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════════════════════════════════

bool USuspenseCoreRepNode_OwnerOnly::IsActorOwnedByConnection(AActor* Actor) const
{
	if (!Actor || !OwningConnection.IsValid())
	{
		return false;
	}

	UNetConnection* NetConnection = OwningConnection->NetConnection;
	if (!NetConnection)
	{
		return false;
	}

	// Get the owning actor of the connection (usually PlayerController)
	APlayerController* PC = Cast<APlayerController>(NetConnection->OwningActor);
	if (!PC)
	{
		return false;
	}

	// Check if actor's owner chain includes this PlayerController
	AActor* ActorOwner = Actor->GetOwner();
	while (ActorOwner)
	{
		if (ActorOwner == PC)
		{
			return true;
		}

		// Check if owner is the PC's pawn
		if (ActorOwner == PC->GetPawn())
		{
			return true;
		}

		ActorOwner = ActorOwner->GetOwner();
	}

	// Also check direct actor owner
	if (Actor->GetOwner() == PC || Actor->GetOwner() == PC->GetPawn())
	{
		return true;
	}

	return false;
}
