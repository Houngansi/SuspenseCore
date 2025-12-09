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
#include "GameFramework/Pawn.h"
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
	// NOTE: SpatialGridNode is NOT added to GlobalGraphNodes because
	// GridSpatialization2D requires AddActor_Dormancy(), not NotifyAddNetworkActor()
	SpatialGridNode = CreateNewNode<UReplicationGraphNode_GridSpatialization2D>();
	SpatialGridNode->CellSize = Settings->SpatialGridCellSize;
	SpatialGridNode->SpatialBias = FVector2D(Settings->SpatialGridExtent, Settings->SpatialGridExtent);
	// Do NOT call AddGlobalGraphNode - spatial grid is managed via AddActor_Dormancy in routing
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
	// Base class adds all GlobalGraphNodes to the connection automatically
	Super::InitConnectionGraphNodes(RepGraphConnection);

	UE_LOG(LogSuspenseCoreReplicationGraph, Log,
		TEXT("InitConnectionGraphNodes for %s"),
		*GetNameSafe(RepGraphConnection->NetConnection->OwningActor));

	// Note: Global nodes (AlwaysRelevantNode, PlayerStateNode, EquipmentDormancyNode)
	// are added automatically by Super::InitConnectionGraphNodes() since they were added via AddGlobalGraphNode()
	// SpatialGridNode is NOT a global node - it's managed separately via PrepareForReplication.

	// ═══════════════════════════════════════════════════════════════════════════
	// Create Per-Connection Owner-Only Node
	// ═══════════════════════════════════════════════════════════════════════════
	// Owner-only nodes are stored per-connection for actor routing.
	// They filter actors in GatherActorListsForConnection based on ownership.
	const USuspenseCoreReplicationGraphSettings* Settings = GetSuspenseCoreSettings();
	if (Settings && Settings->bInventoryOwnerOnly)
	{
		USuspenseCoreRepNode_OwnerOnly* OwnerOnlyNode = CreateNewNode<USuspenseCoreRepNode_OwnerOnly>();
		OwnerOnlyNode->SetOwningConnection(RepGraphConnection);

		// Add to global nodes so it gets called for all connections
		// The node itself filters to only return actors for its owning connection
		AddGlobalGraphNode(OwnerOnlyNode);
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

	// Default: route to spatial grid for Characters and Pawns
	if (Actor->IsA(ACharacter::StaticClass()) || Actor->IsA(APawn::StaticClass()))
	{
		SpatialGridNode->AddActor_Dormancy(ActorInfo, GlobalInfo);

		if (CachedSettings && CachedSettings->bLogReplicationDecisions)
		{
			UE_LOG(LogSuspenseCoreReplicationGraph, Verbose,
				TEXT("Routed Character/Pawn %s to SpatialGridNode (default)"), *Actor->GetName());
		}
		return;
	}

	// Fallback: route all other replicated actors to spatial grid
	// Do NOT call Super::RouteAddNetworkActorToNodes - it would call NotifyAddNetworkActor on all global nodes
	// which is not allowed for GridSpatialization2D
	if (SpatialGridNode)
	{
		SpatialGridNode->AddActor_Dormancy(ActorInfo, GlobalInfo);

		if (CachedSettings && CachedSettings->bLogReplicationDecisions)
		{
			UE_LOG(LogSuspenseCoreReplicationGraph, Verbose,
				TEXT("Routed %s to SpatialGridNode (fallback)"), *Actor->GetName());
		}
	}
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

	// Default: try to remove from spatial grid
	// This handles SpatializedClasses, Characters, Pawns, and fallback
	if (SpatialGridNode)
	{
		SpatialGridNode->RemoveActor_Dormancy(ActorInfo);
	}
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
	const USuspenseCoreReplicationGraphSettings* Settings = GetSuspenseCoreSettings();

	// ═══════════════════════════════════════════════════════════════════════════
	// SuspenseCore PlayerState - Frequency-based replication
	// ═══════════════════════════════════════════════════════════════════════════
	// ASuspenseCorePlayerState inherits from APlayerState, already handled by base
	// But we configure specific replication info

	FClassReplicationInfo SuspensePlayerStateInfo;
	SuspensePlayerStateInfo.ReplicationPeriodFrame = 1; // Start with full frequency

	// Try to find ASuspenseCorePlayerState class
	if (UClass* PlayerStateClass = FindObject<UClass>(nullptr, TEXT("/Script/PlayerCore.SuspenseCorePlayerState")))
	{
		GlobalActorReplicationInfoMap.SetClassInfo(PlayerStateClass, SuspensePlayerStateInfo);
		PlayerStateClasses.Add(PlayerStateClass);
		UE_LOG(LogSuspenseCoreReplicationGraph, Log, TEXT("  Registered ASuspenseCorePlayerState"));
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// SuspenseCore Character - Spatial Grid with cull distance
	// ═══════════════════════════════════════════════════════════════════════════
	if (Settings && Settings->bUseSpatialGridForCharacters)
	{
		FClassReplicationInfo SuspenseCharacterInfo;
		SuspenseCharacterInfo.SetCullDistanceSquared(FMath::Square(Settings->CharacterCullDistance));

		if (UClass* CharacterClass = FindObject<UClass>(nullptr, TEXT("/Script/PlayerCore.SuspenseCoreCharacter")))
		{
			GlobalActorReplicationInfoMap.SetClassInfo(CharacterClass, SuspenseCharacterInfo);
			SpatializedClasses.Add(CharacterClass);
			UE_LOG(LogSuspenseCoreReplicationGraph, Log, TEXT("  Registered ASuspenseCoreCharacter (CullDist=%.0f)"),
				Settings->CharacterCullDistance);
		}
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// SuspenseCore Pickup Items - Spatial Grid with pickup cull distance
	// ═══════════════════════════════════════════════════════════════════════════
	if (Settings)
	{
		FClassReplicationInfo PickupInfo;
		PickupInfo.SetCullDistanceSquared(FMath::Square(Settings->PickupCullDistance));

		// SuspenseCorePickupItem
		if (UClass* PickupClass = FindObject<UClass>(nullptr, TEXT("/Script/InteractionSystem.SuspenseCorePickupItem")))
		{
			GlobalActorReplicationInfoMap.SetClassInfo(PickupClass, PickupInfo);
			SpatializedClasses.Add(PickupClass);
			UE_LOG(LogSuspenseCoreReplicationGraph, Log, TEXT("  Registered ASuspenseCorePickupItem (CullDist=%.0f)"),
				Settings->PickupCullDistance);
		}

		// Legacy SuspensePickupItem
		if (UClass* LegacyPickupClass = FindObject<UClass>(nullptr, TEXT("/Script/InteractionSystem.SuspensePickupItem")))
		{
			GlobalActorReplicationInfoMap.SetClassInfo(LegacyPickupClass, PickupInfo);
			SpatializedClasses.Add(LegacyPickupClass);
			UE_LOG(LogSuspenseCoreReplicationGraph, Log, TEXT("  Registered ASuspenseCorePickupItem (legacy)"));
		}
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// Equipment Actors - Dormancy-based replication
	// ═══════════════════════════════════════════════════════════════════════════
	if (Settings && Settings->bEnableEquipmentDormancy)
	{
		FClassReplicationInfo EquipmentInfo;
		EquipmentInfo.SetCullDistanceSquared(FMath::Square(Settings->CharacterCullDistance)); // Same as character

		// SuspenseEquipmentActor
		if (UClass* EquipmentClass = FindObject<UClass>(nullptr, TEXT("/Script/EquipmentSystem.SuspenseEquipmentActor")))
		{
			GlobalActorReplicationInfoMap.SetClassInfo(EquipmentClass, EquipmentInfo);
			DormancyClasses.Add(EquipmentClass);
			UE_LOG(LogSuspenseCoreReplicationGraph, Log, TEXT("  Registered ASuspenseCoreEquipmentActor (Dormancy)"));
		}

		// SuspenseWeaponActor
		if (UClass* WeaponClass = FindObject<UClass>(nullptr, TEXT("/Script/EquipmentSystem.SuspenseWeaponActor")))
		{
			GlobalActorReplicationInfoMap.SetClassInfo(WeaponClass, EquipmentInfo);
			DormancyClasses.Add(WeaponClass);
			UE_LOG(LogSuspenseCoreReplicationGraph, Log, TEXT("  Registered ASuspenseCoreWeaponActor (Dormancy)"));
		}
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// Inventory Items - Owner-only replication
	// ═══════════════════════════════════════════════════════════════════════════
	if (Settings && Settings->bInventoryOwnerOnly)
	{
		// SuspenseInventoryItem
		if (UClass* InventoryClass = FindObject<UClass>(nullptr, TEXT("/Script/InventorySystem.SuspenseInventoryItem")))
		{
			OwnerOnlyClasses.Add(InventoryClass);
			UE_LOG(LogSuspenseCoreReplicationGraph, Log, TEXT("  Registered ASuspenseCoreInventoryItem (OwnerOnly)"));
		}
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// GameModes - Always Relevant
	// ═══════════════════════════════════════════════════════════════════════════
	if (UClass* GameModeClass = FindObject<UClass>(nullptr, TEXT("/Script/PlayerCore.SuspenseCoreGameGameMode")))
	{
		AlwaysRelevantClasses.Add(GameModeClass);
		UE_LOG(LogSuspenseCoreReplicationGraph, Log, TEXT("  Registered ASuspenseCoreGameGameMode (AlwaysRelevant)"));
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// Summary
	// ═══════════════════════════════════════════════════════════════════════════
	UE_LOG(LogSuspenseCoreReplicationGraph, Log,
		TEXT("SetupSuspenseCoreClassRouting complete: Spatialized=%d, Dormancy=%d, OwnerOnly=%d, AlwaysRelevant=%d, PlayerState=%d"),
		SpatializedClasses.Num(), DormancyClasses.Num(), OwnerOnlyClasses.Num(),
		AlwaysRelevantClasses.Num(), PlayerStateClasses.Num());
}

float USuspenseCoreReplicationGraph::GetCullDistanceSquaredForClass(UClass* ActorClass) const
{
	const USuspenseCoreReplicationGraphSettings* Settings = GetSuspenseCoreSettings();
	if (!Settings)
	{
		return FMath::Square(15000.0f); // Default 150m
	}

	// Check SuspenseCore classes first (by name since we may not have direct references)
	FString ClassName = ActorClass->GetName();

	// Characters
	if (ActorClass->IsChildOf(ACharacter::StaticClass()) ||
		ClassName.Contains(TEXT("Character")))
	{
		return FMath::Square(Settings->CharacterCullDistance);
	}

	// Pickups
	if (ClassName.Contains(TEXT("Pickup")) ||
		ClassName.Contains(TEXT("PickupItem")))
	{
		return FMath::Square(Settings->PickupCullDistance);
	}

	// Projectiles
	if (ClassName.Contains(TEXT("Projectile")))
	{
		return FMath::Square(Settings->ProjectileCullDistance);
	}

	// Equipment/Weapons - same as characters (attached to them)
	if (ClassName.Contains(TEXT("Equipment")) ||
		ClassName.Contains(TEXT("Weapon")))
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
