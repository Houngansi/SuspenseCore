// SuspenseCoreRepNode_PlayerStateFrequency.h
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ReplicationGraph.h"
#include "SuspenseCoreRepNode_PlayerStateFrequency.generated.h"

/**
 * USuspenseCoreRepNode_PlayerStateFrequency
 *
 * Node for PlayerStates with distance-based frequency adjustment.
 * Reduces bandwidth by replicating distant players less frequently.
 *
 * FREQUENCY BUCKETS:
 * ═══════════════════════════════════════════════════════════════════════════
 * - Near (< NearThreshold):      Full frequency (every frame)
 * - Mid (NearThreshold-MidThreshold):  Every 2nd frame
 * - Far (MidThreshold-FarThreshold):   Every 3rd frame
 * - Very Far (> FarThreshold):    Every 5th frame
 *
 * BANDWIDTH IMPACT:
 * ═══════════════════════════════════════════════════════════════════════════
 * With 100 players, naive approach = 9900 PlayerState replications/frame.
 * With frequency buckets (assuming even distribution):
 * - 25 near: 25 * 1 = 25 per frame
 * - 25 mid:  25 * 0.5 = 12.5 per frame
 * - 25 far:  25 * 0.33 = 8.3 per frame
 * - 25 very far: 25 * 0.2 = 5 per frame
 * Total: ~50 per frame (80% reduction!)
 */
UCLASS()
class BRIDGESYSTEM_API USuspenseCoreRepNode_PlayerStateFrequency : public UReplicationGraphNode
{
	GENERATED_BODY()

public:
	USuspenseCoreRepNode_PlayerStateFrequency();

	// ═══════════════════════════════════════════════════════════════════════════
	// UReplicationGraphNode Interface
	// ═══════════════════════════════════════════════════════════════════════════

	/** Add PlayerState to tracking */
	virtual void NotifyAddNetworkActor(const FNewReplicatedActorInfo& ActorInfo) override;

	/** Remove PlayerState from tracking */
	virtual bool NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& ActorInfo, bool bWarnIfNotFound = true) override;

	/** Gather PlayerStates based on distance and frequency */
	virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) override;

	/** Prepare for replication - update frame counter */
	virtual void PrepareForReplication() override;

	// ═══════════════════════════════════════════════════════════════════════════
	// Configuration
	// ═══════════════════════════════════════════════════════════════════════════

	/** Set distance thresholds */
	void SetDistanceThresholds(float Near, float Mid, float Far);

	/** Set replication periods for each bucket */
	void SetReplicationPeriods(int32 Near, int32 Mid, int32 Far, int32 VeryFar);

	// ═══════════════════════════════════════════════════════════════════════════
	// Accessors
	// ═══════════════════════════════════════════════════════════════════════════

	/** Get tracked PlayerState count */
	int32 GetPlayerStateCount() const { return AllPlayerStates.Num(); }

protected:
	/** All tracked PlayerStates */
	UPROPERTY()
	TArray<TObjectPtr<APlayerState>> AllPlayerStates;

	/** Replication actor list for gathering */
	FActorRepListType ReplicationActorList;

	/** Distance thresholds squared (for fast comparison) */
	float NearDistanceSq;
	float MidDistanceSq;
	float FarDistanceSq;

	/** Replication periods for each bucket */
	int32 NearPeriod;
	int32 MidPeriod;
	int32 FarPeriod;
	int32 VeryFarPeriod;

	/** Frame counter for frequency buckets */
	uint32 FrameCounter;

	/** Check if should replicate this frame for bucket */
	bool ShouldReplicateNear() const { return (FrameCounter % NearPeriod) == 0; }
	bool ShouldReplicateMid() const { return (FrameCounter % MidPeriod) == 0; }
	bool ShouldReplicateFar() const { return (FrameCounter % FarPeriod) == 0; }
	bool ShouldReplicateVeryFar() const { return (FrameCounter % VeryFarPeriod) == 0; }

	/** Get viewer location for connection */
	FVector GetViewerLocation(const FConnectionGatherActorListParameters& Params) const;

	/** Get PlayerState location (uses pawn location if available) */
	FVector GetPlayerStateLocation(APlayerState* PlayerState) const;

	/** Get owning PlayerState for this connection */
	APlayerState* GetConnectionPlayerState(const FConnectionGatherActorListParameters& Params) const;
};
