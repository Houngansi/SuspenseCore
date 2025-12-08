// SuspenseCoreRepNode_AlwaysRelevant.h
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ReplicationGraph.h"
#include "SuspenseCoreRepNode_AlwaysRelevant.generated.h"

/**
 * USuspenseCoreRepNode_AlwaysRelevant
 *
 * Node for actors that are ALWAYS relevant to ALL connections.
 * These actors replicate to every connected client every frame.
 *
 * USAGE:
 * ═══════════════════════════════════════════════════════════════════════════
 * - GameState (match state, scores)
 * - GameMode proxies
 * - World settings
 * - Global managers
 *
 * PERFORMANCE:
 * ═══════════════════════════════════════════════════════════════════════════
 * Keep this list minimal! Every actor here replicates to N connections.
 * For 100 players, 10 always-relevant actors = 1000 replications/frame.
 */
UCLASS()
class BRIDGESYSTEM_API USuspenseCoreRepNode_AlwaysRelevant : public UReplicationGraphNode
{
	GENERATED_BODY()

public:
	USuspenseCoreRepNode_AlwaysRelevant();

	// ═══════════════════════════════════════════════════════════════════════════
	// UReplicationGraphNode Interface
	// ═══════════════════════════════════════════════════════════════════════════

	/** Add actor to always-relevant list */
	virtual void NotifyAddNetworkActor(const FNewReplicatedActorInfo& ActorInfo) override;

	/** Remove actor from always-relevant list */
	virtual bool NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& ActorInfo, bool bWarnIfNotFound = true) override;

	/** Gather all always-relevant actors for connection */
	virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) override;

	// ═══════════════════════════════════════════════════════════════════════════
	// Accessors
	// ═══════════════════════════════════════════════════════════════════════════

	/** Get count of always-relevant actors */
	int32 GetActorCount() const { return ReplicationActorList.Num(); }

protected:
	/** List of always-relevant actors */
	FActorRepListRefView ReplicationActorList;
};

