// SuspenseCoreReplicationGraphSettings.h
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SuspenseCoreReplicationGraphSettings.generated.h"

/**
 * USuspenseCoreReplicationGraphSettings
 *
 * Configuration for SuspenseCore Replication Graph.
 * Accessible via Project Settings → Game → SuspenseCore Replication.
 *
 * SPATIAL GRID:
 * ═══════════════════════════════════════════════════════════════════════════
 * The spatial grid divides the world into cells for efficient actor culling.
 * Actors only replicate to connections viewing nearby cells.
 *
 * FREQUENCY BUCKETS:
 * ═══════════════════════════════════════════════════════════════════════════
 * Distance-based replication frequency for PlayerStates:
 * - Near: Full frequency (every frame)
 * - Mid: Reduced frequency (every 2nd frame)
 * - Far: Minimal frequency (every 3rd frame)
 * - Very Far: Sparse (every 5th frame)
 *
 * DORMANCY:
 * ═══════════════════════════════════════════════════════════════════════════
 * Equipment actors go dormant when not actively used, reducing bandwidth.
 * They wake up when state changes.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "SuspenseCore Replication"))
class BRIDGESYSTEM_API USuspenseCoreReplicationGraphSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	USuspenseCoreReplicationGraphSettings();

	// ═══════════════════════════════════════════════════════════════════════════
	// SPATIAL GRID
	// ═══════════════════════════════════════════════════════════════════════════

	/** Grid cell size in Unreal units (default: 10000 = 100m) */
	UPROPERTY(Config, EditAnywhere, Category = "Spatial Grid",
		meta = (ClampMin = "1000", ClampMax = "50000", Units = "cm"))
	float SpatialGridCellSize;

	/** Grid extent - half size of world bounds (default: 500000 = 5km) */
	UPROPERTY(Config, EditAnywhere, Category = "Spatial Grid",
		meta = (ClampMin = "100000", Units = "cm"))
	float SpatialGridExtent;

	/** Enable spatial grid for Characters */
	UPROPERTY(Config, EditAnywhere, Category = "Spatial Grid")
	bool bUseSpatialGridForCharacters;

	/** Enable spatial grid for Pickup items */
	UPROPERTY(Config, EditAnywhere, Category = "Spatial Grid")
	bool bUseSpatialGridForPickups;

	/** Enable spatial grid for Projectiles */
	UPROPERTY(Config, EditAnywhere, Category = "Spatial Grid")
	bool bUseSpatialGridForProjectiles;

	// ═══════════════════════════════════════════════════════════════════════════
	// CULL DISTANCES
	// ═══════════════════════════════════════════════════════════════════════════

	/** Cull distance for Characters (beyond this, character is not replicated) */
	UPROPERTY(Config, EditAnywhere, Category = "Cull Distances",
		meta = (ClampMin = "5000", Units = "cm"))
	float CharacterCullDistance;

	/** Cull distance for Pickup items */
	UPROPERTY(Config, EditAnywhere, Category = "Cull Distances",
		meta = (ClampMin = "1000", Units = "cm"))
	float PickupCullDistance;

	/** Cull distance for Projectiles */
	UPROPERTY(Config, EditAnywhere, Category = "Cull Distances",
		meta = (ClampMin = "5000", Units = "cm"))
	float ProjectileCullDistance;

	// ═══════════════════════════════════════════════════════════════════════════
	// FREQUENCY BUCKETS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Near distance threshold - full replication frequency */
	UPROPERTY(Config, EditAnywhere, Category = "Frequency",
		meta = (ClampMin = "500", Units = "cm"))
	float NearDistanceThreshold;

	/** Mid distance threshold - reduced frequency */
	UPROPERTY(Config, EditAnywhere, Category = "Frequency",
		meta = (ClampMin = "1000", Units = "cm"))
	float MidDistanceThreshold;

	/** Far distance threshold - minimal frequency */
	UPROPERTY(Config, EditAnywhere, Category = "Frequency",
		meta = (ClampMin = "2000", Units = "cm"))
	float FarDistanceThreshold;

	/** Replication period for near actors (1 = every frame) */
	UPROPERTY(Config, EditAnywhere, Category = "Frequency",
		meta = (ClampMin = "1", ClampMax = "10"))
	int32 NearReplicationPeriod;

	/** Replication period for mid-range actors */
	UPROPERTY(Config, EditAnywhere, Category = "Frequency",
		meta = (ClampMin = "1", ClampMax = "10"))
	int32 MidReplicationPeriod;

	/** Replication period for far actors */
	UPROPERTY(Config, EditAnywhere, Category = "Frequency",
		meta = (ClampMin = "1", ClampMax = "20"))
	int32 FarReplicationPeriod;

	/** Replication period for very far actors (beyond FarDistanceThreshold) */
	UPROPERTY(Config, EditAnywhere, Category = "Frequency",
		meta = (ClampMin = "1", ClampMax = "30"))
	int32 VeryFarReplicationPeriod;

	// ═══════════════════════════════════════════════════════════════════════════
	// DORMANCY
	// ═══════════════════════════════════════════════════════════════════════════

	/** Enable dormancy for Equipment actors */
	UPROPERTY(Config, EditAnywhere, Category = "Dormancy")
	bool bEnableEquipmentDormancy;

	/** Dormancy timeout for inactive equipment (seconds) */
	UPROPERTY(Config, EditAnywhere, Category = "Dormancy",
		meta = (ClampMin = "1.0", ClampMax = "60.0", Units = "s", EditCondition = "bEnableEquipmentDormancy"))
	float EquipmentDormancyTimeout;

	/** Enable dormancy for Inventory components */
	UPROPERTY(Config, EditAnywhere, Category = "Dormancy")
	bool bEnableInventoryDormancy;

	/** Dormancy timeout for inactive inventory (seconds) */
	UPROPERTY(Config, EditAnywhere, Category = "Dormancy",
		meta = (ClampMin = "1.0", ClampMax = "60.0", Units = "s", EditCondition = "bEnableInventoryDormancy"))
	float InventoryDormancyTimeout;

	// ═══════════════════════════════════════════════════════════════════════════
	// ALWAYS RELEVANT
	// ═══════════════════════════════════════════════════════════════════════════

	/** Always replicate GameState to all connections */
	UPROPERTY(Config, EditAnywhere, Category = "Always Relevant")
	bool bAlwaysReplicateGameState;

	/** Always replicate GameMode proxy to all connections */
	UPROPERTY(Config, EditAnywhere, Category = "Always Relevant")
	bool bAlwaysReplicateGameMode;

	// ═══════════════════════════════════════════════════════════════════════════
	// OWNER ONLY
	// ═══════════════════════════════════════════════════════════════════════════

	/** Replicate detailed inventory only to owner */
	UPROPERTY(Config, EditAnywhere, Category = "Owner Only")
	bool bInventoryOwnerOnly;

	/** Replicate equipment details only to owner */
	UPROPERTY(Config, EditAnywhere, Category = "Owner Only")
	bool bEquipmentOwnerOnly;

	// ═══════════════════════════════════════════════════════════════════════════
	// DEBUGGING
	// ═══════════════════════════════════════════════════════════════════════════

	/** Enable replication graph debug drawing */
	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bEnableDebugVisualization;

	/** Draw spatial grid cells */
	UPROPERTY(Config, EditAnywhere, Category = "Debug",
		meta = (EditCondition = "bEnableDebugVisualization"))
	bool bDrawSpatialGrid;

	/** Draw replication connections */
	UPROPERTY(Config, EditAnywhere, Category = "Debug",
		meta = (EditCondition = "bEnableDebugVisualization"))
	bool bDrawReplicationLines;

	/** Log replication decisions to output log */
	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bLogReplicationDecisions;

	/** Log dormancy state changes */
	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bLogDormancyChanges;

	// ═══════════════════════════════════════════════════════════════════════════
	// STATIC ACCESS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Get settings singleton */
	static const USuspenseCoreReplicationGraphSettings* Get()
	{
		return GetDefault<USuspenseCoreReplicationGraphSettings>();
	}

	/** Get mutable settings (for editor) */
	static USuspenseCoreReplicationGraphSettings* GetMutable()
	{
		return GetMutableDefault<USuspenseCoreReplicationGraphSettings>();
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// UDeveloperSettings
	// ═══════════════════════════════════════════════════════════════════════════

	virtual FName GetCategoryName() const override { return FName("Game"); }

#if WITH_EDITOR
	virtual FText GetSectionText() const override
	{
		return NSLOCTEXT("SuspenseCore", "SuspenseCoreReplicationSettingsSection", "SuspenseCore Replication");
	}

	virtual FText GetSectionDescription() const override
	{
		return NSLOCTEXT("SuspenseCore", "SuspenseCoreReplicationSettingsDescription",
			"Configure replication graph settings for MMO scalability");
	}
};

