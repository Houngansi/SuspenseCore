// SuspenseCoreReplicationGraph.h
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ReplicationGraph.h"
#include "SuspenseCoreReplicationGraph.generated.h"

class USuspenseCoreReplicationGraphSettings;
class USuspenseCoreEventBus;
class USuspenseCoreRepNode_AlwaysRelevant;
class USuspenseCoreRepNode_PlayerStateFrequency;
class USuspenseCoreRepNode_OwnerOnly;
class USuspenseCoreRepNode_Dormancy;
class UReplicationGraphNode_GridSpatialization2D;

DECLARE_LOG_CATEGORY_EXTERN(LogSuspenseCoreReplicationGraph, Log, All);

/**
 * USuspenseCoreReplicationGraph
 *
 * Custom Replication Graph for MMO shooter scalability.
 * Optimizes network replication for 100+ concurrent players.
 *
 * ARCHITECTURE:
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                    USuspenseCoreReplicationGraph                         │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │  Global Nodes (shared across all connections):                          │
 * │  ├── AlwaysRelevantNode     → GameState, GameMode                       │
 * │  ├── PlayerStateNode        → All PlayerStates with frequency buckets   │
 * │  └── SpatialGridNode        → Characters, Pickups, Projectiles          │
 * │                                                                          │
 * │  Per-Connection Nodes:                                                   │
 * │  ├── AlwaysRelevantForConnection → Player's own actors                  │
 * │  └── OwnerOnlyNode              → Inventory details                     │
 * └─────────────────────────────────────────────────────────────────────────┘
 *
 * EVENTBUS INTEGRATION:
 * ═══════════════════════════════════════════════════════════════════════════
 * - Publishes SuspenseCore.Event.Replication.* events
 * - Uses SuspenseCoreReplicationGraphSettings for configuration
 * - Integrates with ServiceProvider architecture
 *
 * CONFIGURATION:
 * ═══════════════════════════════════════════════════════════════════════════
 * Set in DefaultEngine.ini:
 *   [/Script/OnlineSubsystemUtils.IpNetDriver]
 *   ReplicationDriverClassName="/Script/BridgeSystem.SuspenseCoreReplicationGraph"
 *
 * @see USuspenseCoreReplicationGraphSettings
 * @see USuspenseCoreRepNode_AlwaysRelevant
 * @see USuspenseCoreRepNode_PlayerStateFrequency
 */
UCLASS(Transient, Config = Engine)
class BRIDGESYSTEM_API USuspenseCoreReplicationGraph : public UReplicationGraph
{
	GENERATED_BODY()

public:
	USuspenseCoreReplicationGraph();

	// ═══════════════════════════════════════════════════════════════════════════
	// UReplicationGraph Interface
	// ═══════════════════════════════════════════════════════════════════════════

	/** Initialize actor class settings */
	virtual void InitGlobalActorClassSettings() override;

	/** Create global replication nodes */
	virtual void InitGlobalGraphNodes() override;

	/** Create per-connection nodes */
	virtual void InitConnectionGraphNodes(UNetReplicationGraphConnection* RepGraphConnection) override;

	/** Route new actor to appropriate nodes */
	virtual void RouteAddNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& GlobalInfo) override;

	/** Remove actor from nodes */
	virtual void RouteRemoveNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo) override;

	// ═══════════════════════════════════════════════════════════════════════════
	// SuspenseCore Integration
	// ═══════════════════════════════════════════════════════════════════════════

	/** Get settings from Project Settings */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Replication")
	const USuspenseCoreReplicationGraphSettings* GetSuspenseCoreSettings() const;

	/** Check if debug visualization is enabled */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Replication|Debug")
	bool IsDebugVisualizationEnabled() const;

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// Global Nodes
	// ═══════════════════════════════════════════════════════════════════════════

	/** Always relevant actors (GameState, WorldSettings) */
	UPROPERTY()
	TObjectPtr<USuspenseCoreRepNode_AlwaysRelevant> AlwaysRelevantNode;

	/** PlayerStates with adaptive frequency */
	UPROPERTY()
	TObjectPtr<USuspenseCoreRepNode_PlayerStateFrequency> PlayerStateNode;

	/** Spatial grid for world actors */
	UPROPERTY()
	TObjectPtr<UReplicationGraphNode_GridSpatialization2D> SpatialGridNode;

	// ═══════════════════════════════════════════════════════════════════════════
	// Class Routing
	// ═══════════════════════════════════════════════════════════════════════════

	/** Configure class-specific replication policies */
	void ConfigureClassPolicies();

	/** Setup routing for SuspenseCore classes */
	void SetupSuspenseCoreClassRouting();

	/** Setup routing for Unreal base classes */
	void SetupBaseClassRouting();

	/** Get cull distance squared for actor class */
	float GetCullDistanceSquaredForClass(UClass* ActorClass) const;

	// ═══════════════════════════════════════════════════════════════════════════
	// EventBus Integration
	// ═══════════════════════════════════════════════════════════════════════════

	/** Publish replication event to EventBus */
	void PublishReplicationEvent(FGameplayTag EventTag, AActor* Actor = nullptr);

	/** Get EventBus */
	USuspenseCoreEventBus* GetEventBus() const;

private:
	/** Cached settings */
	UPROPERTY()
	TObjectPtr<const USuspenseCoreReplicationGraphSettings> CachedSettings;

	/** Cached EventBus */
	UPROPERTY()
	mutable TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	/** Actor classes routed to spatial grid */
	TSet<UClass*> SpatializedClasses;

	/** Actor classes that are always relevant */
	TSet<UClass*> AlwaysRelevantClasses;

	/** Actor classes routed to PlayerState node */
	TSet<UClass*> PlayerStateClasses;

	/** Actor classes routed to owner-only node */
	TSet<UClass*> OwnerOnlyClasses;

	/** Actor classes routed to dormancy node */
	TSet<UClass*> DormancyClasses;

	/** Equipment dormancy node (global) */
	UPROPERTY()
	TObjectPtr<USuspenseCoreRepNode_Dormancy> EquipmentDormancyNode;

	/** Per-connection owner-only nodes */
	TMap<UNetReplicationGraphConnection*, TObjectPtr<USuspenseCoreRepNode_OwnerOnly>> ConnectionOwnerOnlyNodes;
};
