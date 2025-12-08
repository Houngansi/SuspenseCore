// SuspenseCoreRepNode_Dormancy.h
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ReplicationGraph.h"
#include "SuspenseCoreRepNode_Dormancy.generated.h"

/**
 * Dormancy tracking info for an actor
 */
USTRUCT()
struct FSuspenseCoreDormancyInfo
{
	GENERATED_BODY()

	/** Time of last significant state change */
	UPROPERTY()
	double LastActivityTime = 0.0;

	/** Current dormancy state */
	UPROPERTY()
	bool bIsDormant = false;

	/** Number of frames since last replication */
	UPROPERTY()
	int32 FramesSinceReplication = 0;
};

/**
 * USuspenseCoreRepNode_Dormancy
 *
 * Node for actors that support dormancy-based replication.
 * Actors go dormant after timeout of inactivity, drastically reducing bandwidth.
 *
 * DORMANCY BEHAVIOR:
 * ═══════════════════════════════════════════════════════════════════════════
 * 1. Active: Replicate normally based on relevancy
 * 2. Going Dormant: After timeout, reduce replication to heartbeat interval
 * 3. Dormant: Minimal replication (once per N frames)
 * 4. Wake Up: On state change, immediately resume full replication
 *
 * BANDWIDTH IMPACT:
 * ═══════════════════════════════════════════════════════════════════════════
 * Equipment example with 100 players, 6 equipment slots each:
 * - Active (shooting): Full replication ~60 updates/sec
 * - Dormant (holstered): 1 update/5 sec
 *
 * Assuming 80% of equipment is dormant:
 * - Naive: 100 * 6 * 60 = 36,000 updates/sec
 * - With dormancy: 100 * 6 * (0.2 * 60 + 0.8 * 0.2) = 7,296 updates/sec
 * Result: ~80% bandwidth reduction for equipment!
 */
UCLASS()
class BRIDGESYSTEM_API USuspenseCoreRepNode_Dormancy : public UReplicationGraphNode
{
	GENERATED_BODY()

public:
	USuspenseCoreRepNode_Dormancy();

	// ═══════════════════════════════════════════════════════════════════════════
	// UReplicationGraphNode Interface
	// ═══════════════════════════════════════════════════════════════════════════

	/** Add actor to dormancy tracking */
	virtual void NotifyAddNetworkActor(const FNewReplicatedActorInfo& ActorInfo) override;

	/** Remove actor from tracking */
	virtual bool NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& ActorInfo, bool bWarnIfNotFound = true) override;

	/** Gather actors based on dormancy state */
	virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) override;

	/** Update dormancy states */
	virtual void PrepareForReplication() override;

	// ═══════════════════════════════════════════════════════════════════════════
	// Configuration
	// ═══════════════════════════════════════════════════════════════════════════

	/** Set dormancy timeout in seconds */
	void SetDormancyTimeout(float TimeoutSeconds);

	/** Set dormant replication period (frames between heartbeats) */
	void SetDormantReplicationPeriod(int32 Frames);

	/** Set cull distance for dormant actors */
	void SetDormantCullDistance(float Distance);

	// ═══════════════════════════════════════════════════════════════════════════
	// Activity Notification
	// ═══════════════════════════════════════════════════════════════════════════

	/** Notify that actor had activity - prevents/breaks dormancy */
	void NotifyActorActivity(AActor* Actor);

	/** Force actor to wake from dormancy */
	void WakeActor(AActor* Actor);

	// ═══════════════════════════════════════════════════════════════════════════
	// Accessors
	// ═══════════════════════════════════════════════════════════════════════════

	/** Get tracked actor count */
	int32 GetTrackedActorCount() const { return TrackedActors.Num(); }

	/** Get dormant actor count */
	int32 GetDormantActorCount() const;

	/** Check if actor is dormant */
	bool IsActorDormant(AActor* Actor) const;

protected:
	/** Map of tracked actors to their dormancy info */
	UPROPERTY()
	TMap<TObjectPtr<AActor>, FSuspenseCoreDormancyInfo> TrackedActors;

	/** Replication actor list for gathering */
	FActorRepListRefView ReplicationActorList;

	/** Dormancy timeout in seconds */
	float DormancyTimeout;

	/** Replication period for dormant actors (frames) */
	int32 DormantReplicationPeriod;

	/** Cull distance squared for dormant actors */
	float DormantCullDistanceSq;

	/** Frame counter for dormant replication scheduling */
	uint32 FrameCounter;

	/** Current world time (cached during PrepareForReplication) */
	double CurrentWorldTime;

	/** Update dormancy state for all actors */
	void UpdateDormancyStates();

	/** Check if dormant actor should replicate this frame */
	bool ShouldReplicateDormantActor(const FSuspenseCoreDormancyInfo& Info) const;

	/** Get viewer location from params */
	FVector GetViewerLocation(const FConnectionGatherActorListParameters& Params) const;
};

