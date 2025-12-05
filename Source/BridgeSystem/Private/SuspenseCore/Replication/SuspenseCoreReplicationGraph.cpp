// SuspenseCoreReplicationGraph.cpp
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Replication/SuspenseCoreReplicationGraph.h"
#include "SuspenseCore/Replication/SuspenseCoreReplicationGraphSettings.h"
#include "SuspenseCore/Replication/Nodes/SuspenseCoreRepNode_AlwaysRelevant.h"
#include "SuspenseCore/Replication/Nodes/SuspenseCoreRepNode_PlayerStateFrequency.h"
#include "SuspenseCore/Replication/Nodes/SuspenseCoreRepNode_OwnerOnly.h"
#include "SuspenseCore/Replication/Nodes/SuspenseCoreRepNode_Dormancy.h"
#include "SuspenseCore/Services/SuspenseCoreServiceProvider.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY(LogSuspenseCoreReplicationGraph);

// ═══════════════════════════════════════════════════════════════════════════════
// CONSTRUCTION
// ═══════════════════════════════════════════════════════════════════════════════

USuspenseCoreReplicationGraph::USuspenseCoreReplicationGraph()
{
	CachedSettings = USuspenseCoreReplicationGraphSettings::Get();
}

// ═══════════════════════════════════════════════════════════════════════════════
// INITIALIZATION
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreReplicationGraph::InitGlobalActorClassSettings()
{
	Super::InitGlobalActorClassSettings();

	UE_LOG(LogSuspenseCoreReplicationGraph, Log, TEXT("InitGlobalActorClassSettings"));

	// Configure class policies based on settings
	ConfigureClassPolicies();

	// Publish initialization event
	PublishReplicationEvent(SUSPENSE_CORE_TAG(Event.Replication.Initialized));
}

void USuspenseCoreReplicationGraph::InitGlobalGraphNodes()
{
	Super::InitGlobalGraphNodes();

	UE_LOG(LogSuspenseCoreReplicationGraph, Log, TEXT("InitGlobalGraphNodes"));

	const USuspenseCoreReplicationGraphSettings* Settings = GetSuspenseCoreSettings();

	// ═══════════════════════════════════════════════════════════════════════════
	// Create Always Relevant Node
	// ═══════════════════════════════════════════════════════════════════════════
	AlwaysRelevantNode = CreateNewNode<USuspenseCoreRepNode_AlwaysRelevant>();
	AddGlobalGraphNode(AlwaysRelevantNode);
	UE_LOG(LogSuspenseCoreReplicationGraph, Log, TEXT("  Created AlwaysRelevantNode"));

	// ═══════════════════════════════════════════════════════════════════════════
	// Create PlayerState Frequency Node
	// ═══════════════════════════════════════════════════════════════════════════
	PlayerStateNode = CreateNewNode<USuspenseCoreRepNode_PlayerStateFrequency>();
	PlayerStateNode->SetDistanceThresholds(
		Settings->NearDistanceThreshold,
		Settings->MidDistanceThreshold,
		Settings->FarDistanceThreshold
	);
	PlayerStateNode->SetReplicationPeriods(
		Settings->NearReplicationPeriod,
		Settings->MidReplicationPeriod,
		Settings->FarReplicationPeriod,
		Settings->VeryFarReplicationPeriod
	);
	AddGlobalGraphNode(PlayerStateNode);
	UE_LOG(LogSuspenseCoreReplicationGraph, Log, TEXT("  Created PlayerStateNode"));

	// ═══════════════════════════════════════════════════════════════════════════
	// Create Spatial Grid Node
	// ═══════════════════════════════════════════════════════════════════════════
	SpatialGridNode = CreateNewNode<UReplicationGraphNode_GridSpatialization2D>();
	SpatialGridNode->CellSize = Settings->SpatialGridCellSize;
	SpatialGridNode->SpatialBias = FVector2D(Settings->SpatialGridExtent, Settings->SpatialGridExtent);
	AddGlobalGraphNode(SpatialGridNode);
	UE_LOG(LogSuspenseCoreReplicationGraph, Log,
		TEXT("  Created SpatialGridNode (CellSize=%.0f, Extent=%.0f)"),
		Settings->SpatialGridCellSize, Settings->SpatialGridExtent);

	// ═══════════════════════════════════════════════════════════════════════════
	// Create Equipment Dormancy Node
	// ═══════════════════════════════════════════════════════════════════════════
	if (Settings->bEnableEquipmentDormancy)
	{
		EquipmentDormancyNode = CreateNewNode<USuspenseCoreRepNode_Dormancy>();
		EquipmentDormancyNode->SetDormancyTimeout(Settings->EquipmentDormancyTimeout);
		EquipmentDormancyNode->SetDormantReplicationPeriod(300); // ~5 sec heartbeat at 60Hz
		AddGlobalGraphNode(EquipmentDormancyNode);
		UE_LOG(LogSuspenseCoreReplicationGraph, Log,
			TEXT("  Created EquipmentDormancyNode (Timeout=%.1fs)"),
			Settings->EquipmentDormancyTimeout);
	}

	// Setup routing
	SetupBaseClassRouting();
	SetupSuspenseCoreClassRouting();
}

void USuspenseCoreReplicationGraph::InitConnectionGraphNodes(UNetReplicationGraphConnection* RepGraphConnection)
{
	Super::InitConnectionGraphNodes(RepGraphConnection);

	UE_LOG(LogSuspenseCoreReplicationGraph, Log,
		TEXT("InitConnectionGraphNodes for %s"),
		*GetNameSafe(RepGraphConnection->NetConnection->OwningActor));

	// Add always relevant node for all connections
	RepGraphConnection->AddConnectionGraphNode(AlwaysRelevantNode);

	// Add PlayerState frequency node
	RepGraphConnection->AddConnectionGraphNode(PlayerStateNode);

	// Add spatial grid node
	RepGraphConnection->AddConnectionGraphNode(SpatialGridNode);

	// Add equipment dormancy node if enabled
	if (EquipmentDormancyNode)
	{
		RepGraphConnection->AddConnectionGraphNode(EquipmentDormancyNode);
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// Create Per-Connection Owner-Only Node
	// ═══════════════════════════════════════════════════════════════════════════
	const USuspenseCoreReplicationGraphSettings* Settings = GetSuspenseCoreSettings();
	if (Settings && Settings->bInventoryOwnerOnly)
	{
		USuspenseCoreRepNode_OwnerOnly* OwnerOnlyNode = CreateNewNode<USuspenseCoreRepNode_OwnerOnly>();
		OwnerOnlyNode->SetOwningConnection(RepGraphConnection);
		RepGraphConnection->AddConnectionGraphNode(OwnerOnlyNode);
		ConnectionOwnerOnlyNodes.Add(RepGraphConnection, OwnerOnlyNode);

		UE_LOG(LogSuspenseCoreReplicationGraph, Log,
			TEXT("  Created OwnerOnlyNode for connection"));
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// ROUTING
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreReplicationGraph::RouteAddNetworkActorToNodes(
	const FNewReplicatedActorInfo& ActorInfo,
	FGlobalActorReplicationInfo& GlobalInfo)
{
	AActor* Actor = ActorInfo.Actor;
	UClass* ActorClass = Actor->GetClass();

	// Check Always Relevant
	if (AlwaysRelevantClasses.Contains(ActorClass))
	{
		AlwaysRelevantNode->NotifyAddNetworkActor(ActorInfo);

		if (CachedSettings && CachedSettings->bLogReplicationDecisions)
		{
			UE_LOG(LogSuspenseCoreReplicationGraph, Verbose,
				TEXT("Routed %s to AlwaysRelevantNode"), *Actor->GetName());
		}
		return;
	}

	// Check PlayerState
	if (PlayerStateClasses.Contains(ActorClass) || Actor->IsA(APlayerState::StaticClass()))
	{
		PlayerStateNode->NotifyAddNetworkActor(ActorInfo);

		if (CachedSettings && CachedSettings->bLogReplicationDecisions)
		{
			UE_LOG(LogSuspenseCoreReplicationGraph, Verbose,
				TEXT("Routed %s to PlayerStateNode"), *Actor->GetName());
		}
		return;
	}

	// Check Dormancy (Equipment)
	if (DormancyClasses.Contains(ActorClass) && EquipmentDormancyNode)
	{
		EquipmentDormancyNode->NotifyAddNetworkActor(ActorInfo);

		if (CachedSettings && CachedSettings->bLogReplicationDecisions)
		{
			UE_LOG(LogSuspenseCoreReplicationGraph, Verbose,
				TEXT("Routed %s to EquipmentDormancyNode"), *Actor->GetName());
		}
		return;
	}

	// Check Owner-Only (Inventory)
	if (OwnerOnlyClasses.Contains(ActorClass))
	{
		// Route to all per-connection owner-only nodes
		// Each node will filter to only accept if it's the owner
		for (auto& Pair : ConnectionOwnerOnlyNodes)
		{
			if (Pair.Value)
			{
				Pair.Value->NotifyAddNetworkActor(ActorInfo);
			}
		}

		if (CachedSettings && CachedSettings->bLogReplicationDecisions)
		{
			UE_LOG(LogSuspenseCoreReplicationGraph, Verbose,
				TEXT("Routed %s to OwnerOnlyNodes"), *Actor->GetName());
		}
		return;
	}

	// Check Spatial Grid
	if (SpatializedClasses.Contains(ActorClass))
	{
		SpatialGridNode->AddActor_Dormancy(ActorInfo, GlobalInfo);

		if (CachedSettings && CachedSettings->bLogReplicationDecisions)
		{
			UE_LOG(LogSuspenseCoreReplicationGraph, Verbose,
				TEXT("Routed %s to SpatialGridNode"), *Actor->GetName());
		}
		return;
	}

	// Default: route to spatial grid for Characters
	if (Actor->IsA(ACharacter::StaticClass()))
	{
		SpatialGridNode->AddActor_Dormancy(ActorInfo, GlobalInfo);

		if (CachedSettings && CachedSettings->bLogReplicationDecisions)
		{
			UE_LOG(LogSuspenseCoreReplicationGraph, Verbose,
				TEXT("Routed Character %s to SpatialGridNode (default)"), *Actor->GetName());
		}
		return;
	}

	// Fallback: let parent handle it
	Super::RouteAddNetworkActorToNodes(ActorInfo, GlobalInfo);
}

void USuspenseCoreReplicationGraph::RouteRemoveNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo)
{
	AActor* Actor = ActorInfo.Actor;
	UClass* ActorClass = Actor->GetClass();

	// Remove from all potential nodes
	if (AlwaysRelevantClasses.Contains(ActorClass))
	{
		AlwaysRelevantNode->NotifyRemoveNetworkActor(ActorInfo);
		return;
	}

	if (PlayerStateClasses.Contains(ActorClass) || Actor->IsA(APlayerState::StaticClass()))
	{
		PlayerStateNode->NotifyRemoveNetworkActor(ActorInfo);
		return;
	}

	if (DormancyClasses.Contains(ActorClass) && EquipmentDormancyNode)
	{
		EquipmentDormancyNode->NotifyRemoveNetworkActor(ActorInfo);
		return;
	}

	if (OwnerOnlyClasses.Contains(ActorClass))
	{
		for (auto& Pair : ConnectionOwnerOnlyNodes)
		{
			if (Pair.Value)
			{
				Pair.Value->NotifyRemoveNetworkActor(ActorInfo);
			}
		}
		return;
	}

	if (SpatializedClasses.Contains(ActorClass) || Actor->IsA(ACharacter::StaticClass()))
	{
		SpatialGridNode->RemoveActor_Dormancy(ActorInfo);
		return;
	}

	Super::RouteRemoveNetworkActorToNodes(ActorInfo);
}

// ═══════════════════════════════════════════════════════════════════════════════
// CLASS POLICIES
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreReplicationGraph::ConfigureClassPolicies()
{
	const USuspenseCoreReplicationGraphSettings* Settings = GetSuspenseCoreSettings();
	if (!Settings)
	{
		UE_LOG(LogSuspenseCoreReplicationGraph, Warning, TEXT("ConfigureClassPolicies: Settings not found"));
		return;
	}

	// Configure default replication info for characters
	FClassReplicationInfo CharacterRepInfo;
	CharacterRepInfo.SetCullDistanceSquared(FMath::Square(Settings->CharacterCullDistance));
	GlobalActorReplicationInfoMap.SetClassInfo(ACharacter::StaticClass(), CharacterRepInfo);

	// Configure PlayerState
	FClassReplicationInfo PlayerStateRepInfo;
	PlayerStateRepInfo.ReplicationPeriodFrame = Settings->NearReplicationPeriod;
	GlobalActorReplicationInfoMap.SetClassInfo(APlayerState::StaticClass(), PlayerStateRepInfo);

	UE_LOG(LogSuspenseCoreReplicationGraph, Log, TEXT("ConfigureClassPolicies: Configured %d base class policies"), 2);
}

void USuspenseCoreReplicationGraph::SetupBaseClassRouting()
{
	const USuspenseCoreReplicationGraphSettings* Settings = GetSuspenseCoreSettings();
	if (!Settings)
	{
		return;
	}

	// Always Relevant
	if (Settings->bAlwaysReplicateGameState)
	{
		AlwaysRelevantClasses.Add(AGameStateBase::StaticClass());
	}

	if (Settings->bAlwaysReplicateGameMode)
	{
		AlwaysRelevantClasses.Add(AGameModeBase::StaticClass());
	}

	// PlayerStates
	PlayerStateClasses.Add(APlayerState::StaticClass());

	// Spatialized
	if (Settings->bUseSpatialGridForCharacters)
	{
		SpatializedClasses.Add(ACharacter::StaticClass());
	}

	UE_LOG(LogSuspenseCoreReplicationGraph, Log,
		TEXT("SetupBaseClassRouting: AlwaysRelevant=%d, PlayerState=%d, Spatialized=%d"),
		AlwaysRelevantClasses.Num(), PlayerStateClasses.Num(), SpatializedClasses.Num());
}

void USuspenseCoreReplicationGraph::SetupSuspenseCoreClassRouting()
{
	// SuspenseCore classes will be added dynamically when they register
	// This allows modules to register their classes at runtime

	UE_LOG(LogSuspenseCoreReplicationGraph, Log, TEXT("SetupSuspenseCoreClassRouting: Ready for dynamic registration"));
}

float USuspenseCoreReplicationGraph::GetCullDistanceSquaredForClass(UClass* ActorClass) const
{
	const USuspenseCoreReplicationGraphSettings* Settings = GetSuspenseCoreSettings();
	if (!Settings)
	{
		return FMath::Square(15000.0f); // Default 150m
	}

	if (ActorClass->IsChildOf(ACharacter::StaticClass()))
	{
		return FMath::Square(Settings->CharacterCullDistance);
	}

	// Default
	return FMath::Square(Settings->CharacterCullDistance);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SETTINGS ACCESS
// ═══════════════════════════════════════════════════════════════════════════════

const USuspenseCoreReplicationGraphSettings* USuspenseCoreReplicationGraph::GetSuspenseCoreSettings() const
{
	if (!CachedSettings)
	{
		const_cast<USuspenseCoreReplicationGraph*>(this)->CachedSettings =
			USuspenseCoreReplicationGraphSettings::Get();
	}
	return CachedSettings;
}

bool USuspenseCoreReplicationGraph::IsDebugVisualizationEnabled() const
{
	const USuspenseCoreReplicationGraphSettings* Settings = GetSuspenseCoreSettings();
	return Settings ? Settings->bEnableDebugVisualization : false;
}

// ═══════════════════════════════════════════════════════════════════════════════
// EVENTBUS INTEGRATION
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreReplicationGraph::PublishReplicationEvent(FGameplayTag EventTag, AActor* Actor)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(const_cast<USuspenseCoreReplicationGraph*>(this));

	if (Actor)
	{
		EventData.SetObject(FName("Actor"), Actor);
		EventData.SetString(FName("ActorClass"), Actor->GetClass()->GetName());
	}

	EventBus->Publish(EventTag, EventData);
}

USuspenseCoreEventBus* USuspenseCoreReplicationGraph::GetEventBus() const
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	// Try to get from ServiceProvider
	UWorld* World = GetWorld();
	if (World)
	{
		if (USuspenseCoreServiceProvider* Provider = USuspenseCoreServiceProvider::Get(World))
		{
			CachedEventBus = Provider->GetEventBus();
			return CachedEventBus.Get();
		}
	}

	return nullptr;
}
