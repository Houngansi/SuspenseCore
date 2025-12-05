// SuspenseCoreRepNode_Dormancy.cpp
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Replication/Nodes/SuspenseCoreRepNode_Dormancy.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

// ═══════════════════════════════════════════════════════════════════════════════
// CONSTRUCTION
// ═══════════════════════════════════════════════════════════════════════════════

USuspenseCoreRepNode_Dormancy::USuspenseCoreRepNode_Dormancy()
{
	// Default configuration
	DormancyTimeout = 5.0f;              // 5 seconds
	DormantReplicationPeriod = 300;      // Every 300 frames (~5 seconds at 60Hz)
	DormantCullDistanceSq = FMath::Square(10000.0f);  // 100m

	FrameCounter = 0;
	CurrentWorldTime = 0.0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// UReplicationGraphNode Interface
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreRepNode_Dormancy::NotifyAddNetworkActor(const FNewReplicatedActorInfo& ActorInfo)
{
	AActor* Actor = ActorInfo.Actor;
	if (!Actor || TrackedActors.Contains(Actor))
	{
		return;
	}

	FSuspenseCoreDormancyInfo Info;
	Info.LastActivityTime = CurrentWorldTime;
	Info.bIsDormant = false;
	Info.FramesSinceReplication = 0;

	TrackedActors.Add(Actor, Info);
}

bool USuspenseCoreRepNode_Dormancy::NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& ActorInfo, bool bWarnIfNotFound)
{
	AActor* Actor = ActorInfo.Actor;
	if (Actor)
	{
		const int32 NumRemoved = TrackedActors.Remove(Actor);
		return NumRemoved > 0;
	}
	return false;
}

void USuspenseCoreRepNode_Dormancy::GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params)
{
	// Get viewer location for distance checks
	FVector ViewerLocation = GetViewerLocation(Params);

	// Build replication list
	ReplicationActorList.Reset();

	// Iterate tracked actors
	for (auto& Pair : TrackedActors)
	{
		AActor* Actor = Pair.Key;
		FSuspenseCoreDormancyInfo& Info = Pair.Value;

		if (!Actor || !IsValid(Actor))
		{
			continue;
		}

		// Distance check for dormant actors
		if (Info.bIsDormant)
		{
			float DistSq = FVector::DistSquared(ViewerLocation, Actor->GetActorLocation());
			if (DistSq > DormantCullDistanceSq)
			{
				// Too far for dormant actor
				continue;
			}

			// Check if dormant actor should replicate this frame
			if (!ShouldReplicateDormantActor(Info))
			{
				Info.FramesSinceReplication++;
				continue;
			}
		}

		// Add to replication list
		ReplicationActorList.Add(Actor);
		Info.FramesSinceReplication = 0;
	}

	// Add to gathered lists if we have actors
	if (ReplicationActorList.Num() > 0)
	{
		Params.OutGatheredReplicationLists.AddReplicationActorList(ReplicationActorList);
	}
}

void USuspenseCoreRepNode_Dormancy::PrepareForReplication()
{
	Super::PrepareForReplication();

	// Update frame counter
	FrameCounter++;

	// Cache world time
	UWorld* World = GetWorld();
	if (World)
	{
		CurrentWorldTime = World->GetTimeSeconds();
	}

	// Update dormancy states
	UpdateDormancyStates();

	// Clean up invalid references periodically
	if ((FrameCounter % 60) == 0)
	{
		TArray<TObjectPtr<AActor>> ToRemove;
		for (auto& Pair : TrackedActors)
		{
			if (!Pair.Key || !IsValid(Pair.Key))
			{
				ToRemove.Add(Pair.Key);
			}
		}

		for (TObjectPtr<AActor>& Actor : ToRemove)
		{
			TrackedActors.Remove(Actor);
		}
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreRepNode_Dormancy::SetDormancyTimeout(float TimeoutSeconds)
{
	DormancyTimeout = FMath::Max(0.5f, TimeoutSeconds);
}

void USuspenseCoreRepNode_Dormancy::SetDormantReplicationPeriod(int32 Frames)
{
	DormantReplicationPeriod = FMath::Max(1, Frames);
}

void USuspenseCoreRepNode_Dormancy::SetDormantCullDistance(float Distance)
{
	DormantCullDistanceSq = FMath::Square(FMath::Max(1000.0f, Distance));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Activity Notification
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreRepNode_Dormancy::NotifyActorActivity(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	if (FSuspenseCoreDormancyInfo* Info = TrackedActors.Find(Actor))
	{
		Info->LastActivityTime = CurrentWorldTime;

		// Wake from dormancy if needed
		if (Info->bIsDormant)
		{
			Info->bIsDormant = false;
			Info->FramesSinceReplication = 0;
		}
	}
}

void USuspenseCoreRepNode_Dormancy::WakeActor(AActor* Actor)
{
	NotifyActorActivity(Actor);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Accessors
// ═══════════════════════════════════════════════════════════════════════════════

int32 USuspenseCoreRepNode_Dormancy::GetDormantActorCount() const
{
	int32 Count = 0;
	for (const auto& Pair : TrackedActors)
	{
		if (Pair.Value.bIsDormant)
		{
			Count++;
		}
	}
	return Count;
}

bool USuspenseCoreRepNode_Dormancy::IsActorDormant(AActor* Actor) const
{
	if (const FSuspenseCoreDormancyInfo* Info = TrackedActors.Find(Actor))
	{
		return Info->bIsDormant;
	}
	return false;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Internal
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreRepNode_Dormancy::UpdateDormancyStates()
{
	for (auto& Pair : TrackedActors)
	{
		FSuspenseCoreDormancyInfo& Info = Pair.Value;

		if (!Info.bIsDormant)
		{
			// Check if should go dormant
			double TimeSinceActivity = CurrentWorldTime - Info.LastActivityTime;
			if (TimeSinceActivity >= DormancyTimeout)
			{
				Info.bIsDormant = true;
			}
		}
	}
}

bool USuspenseCoreRepNode_Dormancy::ShouldReplicateDormantActor(const FSuspenseCoreDormancyInfo& Info) const
{
	// Use frame counter to determine if this is a heartbeat frame for dormant actors
	// Offset by FramesSinceReplication to avoid all dormant actors replicating same frame
	return Info.FramesSinceReplication >= DormantReplicationPeriod;
}

FVector USuspenseCoreRepNode_Dormancy::GetViewerLocation(const FConnectionGatherActorListParameters& Params) const
{
	// Use Viewers array to get view location
	if (Params.Viewers.Num() > 0)
	{
		return Params.Viewers[0].ViewLocation;
	}

	return FVector::ZeroVector;
}
