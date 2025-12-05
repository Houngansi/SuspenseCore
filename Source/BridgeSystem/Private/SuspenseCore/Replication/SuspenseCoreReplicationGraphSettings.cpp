// SuspenseCoreReplicationGraphSettings.cpp
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Replication/SuspenseCoreReplicationGraphSettings.h"

USuspenseCoreReplicationGraphSettings::USuspenseCoreReplicationGraphSettings()
{
	// ═══════════════════════════════════════════════════════════════════════════
	// Spatial Grid Defaults
	// ═══════════════════════════════════════════════════════════════════════════
	SpatialGridCellSize = 10000.0f;      // 100m cells
	SpatialGridExtent = 500000.0f;       // 5km world radius
	bUseSpatialGridForCharacters = true;
	bUseSpatialGridForPickups = true;
	bUseSpatialGridForProjectiles = true;

	// ═══════════════════════════════════════════════════════════════════════════
	// Cull Distance Defaults (for MMO scale)
	// ═══════════════════════════════════════════════════════════════════════════
	CharacterCullDistance = 15000.0f;    // 150m - other players visible
	PickupCullDistance = 5000.0f;        // 50m - pickups nearby
	ProjectileCullDistance = 20000.0f;   // 200m - projectiles need far visibility

	// ═══════════════════════════════════════════════════════════════════════════
	// Frequency Defaults (optimized for 60Hz server)
	// ═══════════════════════════════════════════════════════════════════════════
	NearDistanceThreshold = 2000.0f;     // 20m
	MidDistanceThreshold = 5000.0f;      // 50m
	FarDistanceThreshold = 10000.0f;     // 100m

	NearReplicationPeriod = 1;           // Every frame (60Hz)
	MidReplicationPeriod = 2;            // Every 2nd frame (30Hz)
	FarReplicationPeriod = 3;            // Every 3rd frame (20Hz)
	VeryFarReplicationPeriod = 5;        // Every 5th frame (12Hz)

	// ═══════════════════════════════════════════════════════════════════════════
	// Dormancy Defaults
	// ═══════════════════════════════════════════════════════════════════════════
	bEnableEquipmentDormancy = true;
	EquipmentDormancyTimeout = 5.0f;     // 5 seconds of inactivity
	bEnableInventoryDormancy = true;
	InventoryDormancyTimeout = 10.0f;    // 10 seconds of inactivity

	// ═══════════════════════════════════════════════════════════════════════════
	// Always Relevant Defaults
	// ═══════════════════════════════════════════════════════════════════════════
	bAlwaysReplicateGameState = true;
	bAlwaysReplicateGameMode = true;

	// ═══════════════════════════════════════════════════════════════════════════
	// Owner Only Defaults (for bandwidth savings)
	// ═══════════════════════════════════════════════════════════════════════════
	bInventoryOwnerOnly = true;          // Only owner sees detailed inventory
	bEquipmentOwnerOnly = false;         // Others see equipped items

	// ═══════════════════════════════════════════════════════════════════════════
	// Debug Defaults (off for production)
	// ═══════════════════════════════════════════════════════════════════════════
	bEnableDebugVisualization = false;
	bDrawSpatialGrid = false;
	bDrawReplicationLines = false;
	bLogReplicationDecisions = false;
	bLogDormancyChanges = false;
}
