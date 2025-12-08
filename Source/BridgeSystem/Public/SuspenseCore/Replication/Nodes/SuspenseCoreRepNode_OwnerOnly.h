// SuspenseCoreRepNode_OwnerOnly.h
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ReplicationGraph.h"
#include "SuspenseCoreRepNode_OwnerOnly.generated.h"

/**
 * USuspenseCoreRepNode_OwnerOnly
 *
 * Per-connection node that only replicates actors to their owner.
 * Used for sensitive data like detailed inventory contents.
 *
 * BANDWIDTH IMPACT:
 * ═══════════════════════════════════════════════════════════════════════════
 * With 100 players, each having 50-slot inventory:
 * - Naive: 100 * 99 * 50 = 495,000 property replications/frame
 * - Owner-only: 100 * 50 = 5,000 property replications/frame
 * Result: 99% bandwidth reduction for inventory data!
 *
 * USE CASES:
 * ═══════════════════════════════════════════════════════════════════════════
 * - Inventory contents (item stacks, quantities, stats)
 * - Currency/wallet data
 * - Quest progress
 * - Personal settings/preferences
 * - Private chat messages
 */
UCLASS()
class BRIDGESYSTEM_API USuspenseCoreRepNode_OwnerOnly : public UReplicationGraphNode
{
	GENERATED_BODY()

public:
	USuspenseCoreRepNode_OwnerOnly();

	// ═══════════════════════════════════════════════════════════════════════════
	// UReplicationGraphNode Interface
	// ═══════════════════════════════════════════════════════════════════════════

	/** Add actor to owner-only tracking */
	virtual void NotifyAddNetworkActor(const FNewReplicatedActorInfo& ActorInfo) override;

	/** Remove actor from tracking */
	virtual bool NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& ActorInfo, bool bWarnIfNotFound = true) override;

	/** Gather actors - only add to owner's connection */
	virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) override;

	// ═══════════════════════════════════════════════════════════════════════════
	// Per-Connection Setup
	// ═══════════════════════════════════════════════════════════════════════════

	/** Set the owning connection for this node */
	void SetOwningConnection(UNetReplicationGraphConnection* InConnection);

	/** Get tracked actor count */
	int32 GetOwnerActorCount() const { return OwnerActors.Num(); }

protected:
	/** Actors that should only replicate to owner */
	UPROPERTY()
	TArray<TObjectPtr<AActor>> OwnerActors;

	/** Replication actor list for gathering */
	FActorRepListRefView ReplicationActorList;

	/** The owning connection */
	UPROPERTY()
	TWeakObjectPtr<UNetReplicationGraphConnection> OwningConnection;

	/** Check if actor belongs to this connection's owner */
	bool IsActorOwnedByConnection(AActor* Actor) const;

	/** Check if this is our owning connection */
	bool IsOwningConnection(const FConnectionGatherActorListParameters& Params) const;
};

