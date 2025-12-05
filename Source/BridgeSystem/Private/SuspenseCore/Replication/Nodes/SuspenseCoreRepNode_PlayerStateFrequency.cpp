// SuspenseCoreRepNode_PlayerStateFrequency.cpp
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Replication/Nodes/SuspenseCoreRepNode_PlayerStateFrequency.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

// ═══════════════════════════════════════════════════════════════════════════════
// CONSTRUCTION
// ═══════════════════════════════════════════════════════════════════════════════

USuspenseCoreRepNode_PlayerStateFrequency::USuspenseCoreRepNode_PlayerStateFrequency()
{
	// Default distance thresholds (squared for fast comparison)
	NearDistanceSq = FMath::Square(2000.0f);   // 20m
	MidDistanceSq = FMath::Square(5000.0f);    // 50m
	FarDistanceSq = FMath::Square(10000.0f);   // 100m

	// Default replication periods
	NearPeriod = 1;      // Every frame
	MidPeriod = 2;       // Every 2nd frame
	FarPeriod = 3;       // Every 3rd frame
	VeryFarPeriod = 5;   // Every 5th frame

	// Frame counter
	FrameCounter = 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// UReplicationGraphNode Interface
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreRepNode_PlayerStateFrequency::NotifyAddNetworkActor(const FNewReplicatedActorInfo& ActorInfo)
{
	APlayerState* PlayerState = Cast<APlayerState>(ActorInfo.Actor);
	if (PlayerState && !AllPlayerStates.Contains(PlayerState))
	{
		AllPlayerStates.Add(PlayerState);
	}
}

bool USuspenseCoreRepNode_PlayerStateFrequency::NotifyRemoveNetworkActor(const FNewReplicatedActorInfo& ActorInfo, bool bWarnIfNotFound)
{
	APlayerState* PlayerState = Cast<APlayerState>(ActorInfo.Actor);
	if (PlayerState)
	{
		const int32 NumRemoved = AllPlayerStates.Remove(PlayerState);
		return NumRemoved > 0;
	}
	return false;
}

void USuspenseCoreRepNode_PlayerStateFrequency::GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params)
{
	// Get viewer location for distance calculation
	FVector ViewerLocation = GetViewerLocation(Params);

	// Get the connection's own PlayerState - always replicate at full frequency
	APlayerState* ConnectionPlayerState = nullptr;
	if (Params.ConnectionManager.ConnectionOrderNum < Params.ReplicationGraph->Connections.Num())
	{
		if (UNetReplicationGraphConnection* GraphConnection = Params.ReplicationGraph->Connections[Params.ConnectionManager.ConnectionOrderNum])
		{
			if (APlayerController* PC = Cast<APlayerController>(GraphConnection->NetConnection->OwningActor))
			{
				ConnectionPlayerState = PC->PlayerState;
			}
		}
	}

	// Iterate through all tracked PlayerStates
	for (TObjectPtr<APlayerState>& PlayerState : AllPlayerStates)
	{
		if (!PlayerState || !IsValid(PlayerState))
		{
			continue;
		}

		// Own PlayerState - always replicate
		if (PlayerState == ConnectionPlayerState)
		{
			Params.OutGatheredReplicationLists.AddActor(PlayerState, Params.ConnectionManager);
			continue;
		}

		// Get PlayerState location (use pawn location if available)
		FVector PlayerStateLocation = PlayerState->GetActorLocation();
		if (APawn* Pawn = PlayerState->GetPawn())
		{
			PlayerStateLocation = Pawn->GetActorLocation();
		}

		// Calculate distance squared
		float DistSq = FVector::DistSquared(ViewerLocation, PlayerStateLocation);

		// Determine bucket and check if should replicate this frame
		bool bShouldReplicate = false;

		if (DistSq <= NearDistanceSq)
		{
			// Near bucket - full frequency
			bShouldReplicate = ShouldReplicateNear();
		}
		else if (DistSq <= MidDistanceSq)
		{
			// Mid bucket - reduced frequency
			bShouldReplicate = ShouldReplicateMid();
		}
		else if (DistSq <= FarDistanceSq)
		{
			// Far bucket - minimal frequency
			bShouldReplicate = ShouldReplicateFar();
		}
		else
		{
			// Very far bucket - sparse frequency
			bShouldReplicate = ShouldReplicateVeryFar();
		}

		if (bShouldReplicate)
		{
			Params.OutGatheredReplicationLists.AddActor(PlayerState, Params.ConnectionManager);
		}
	}
}

void USuspenseCoreRepNode_PlayerStateFrequency::PrepareForReplication()
{
	Super::PrepareForReplication();

	// Increment frame counter (with wrap-around)
	FrameCounter++;

	// Clean up invalid references periodically
	if ((FrameCounter % 60) == 0) // Every ~1 second at 60Hz
	{
		AllPlayerStates.RemoveAll([](const TObjectPtr<APlayerState>& PS)
		{
			return !PS || !IsValid(PS);
		});
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreRepNode_PlayerStateFrequency::SetDistanceThresholds(float Near, float Mid, float Far)
{
	NearDistanceSq = FMath::Square(Near);
	MidDistanceSq = FMath::Square(Mid);
	FarDistanceSq = FMath::Square(Far);
}

void USuspenseCoreRepNode_PlayerStateFrequency::SetReplicationPeriods(int32 Near, int32 Mid, int32 Far, int32 VeryFar)
{
	NearPeriod = FMath::Max(1, Near);
	MidPeriod = FMath::Max(1, Mid);
	FarPeriod = FMath::Max(1, Far);
	VeryFarPeriod = FMath::Max(1, VeryFar);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════════════════════════════════

FVector USuspenseCoreRepNode_PlayerStateFrequency::GetViewerLocation(const FConnectionGatherActorListParameters& Params) const
{
	// Try to get viewer location from connection
	if (Params.ConnectionManager.ConnectionOrderNum < Params.ReplicationGraph->Connections.Num())
	{
		if (UNetReplicationGraphConnection* GraphConnection = Params.ReplicationGraph->Connections[Params.ConnectionManager.ConnectionOrderNum])
		{
			if (APlayerController* PC = Cast<APlayerController>(GraphConnection->NetConnection->OwningActor))
			{
				if (APawn* ViewerPawn = PC->GetPawn())
				{
					return ViewerPawn->GetActorLocation();
				}

				// Fallback to PlayerController location
				return PC->GetActorLocation();
			}
		}
	}

	// Last fallback - world origin
	return FVector::ZeroVector;
}
