// SuspenseCoreTraceUtils.h
// SuspenseCore - Weapon Trace Utilities
// Copyright Suspense Team. All Rights Reserved.
//
// Static utility library for weapon line tracing operations.
// Provides debug visualization, aim point calculation, and trace helpers.
//
// Usage:
//   TArray<FHitResult> Hits;
//   bool bBlocking = USuspenseCoreTraceUtils::PerformLineTrace(World, Start, End, "Weapon", {}, false, 0.0f, Hits);

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SuspenseCoreTraceUtils.generated.h"

class APlayerController;

/**
 * USuspenseCoreTraceUtils
 *
 * Static utility library for weapon tracing operations.
 * Provides line trace, debug visualization, and aim point calculation.
 *
 * Features:
 * - Multi-hit line traces with physical material support
 * - Debug visualization with color-coded hits
 * - Screen-center aim point calculation
 * - Collision profile-based tracing
 *
 * @see SuspenseCoreWeaponAsyncTask_PerformTrace (uses these utilities)
 */
UCLASS()
class BRIDGESYSTEM_API USuspenseCoreTraceUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//========================================================================
	// Line Tracing
	//========================================================================

	/**
	 * Perform a multi-hit line trace using collision profile.
	 * Returns all hits along the trace path, including penetrations.
	 *
	 * @param World World context
	 * @param Start Trace start point
	 * @param End Trace end point
	 * @param TraceProfile Collision profile name (e.g., "Weapon", "Projectile")
	 * @param ActorsToIgnore Actors to exclude from trace
	 * @param bDebug Enable debug visualization
	 * @param DebugDrawTime Duration for debug lines (0 = single frame)
	 * @param OutHits Output array of hit results
	 * @return True if any blocking hit occurred
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Trace", meta = (WorldContext = "WorldContextObject"))
	static bool PerformLineTrace(
		const UObject* WorldContextObject,
		const FVector& Start,
		const FVector& End,
		FName TraceProfile,
		const TArray<AActor*>& ActorsToIgnore,
		bool bDebug,
		float DebugDrawTime,
		TArray<FHitResult>& OutHits
	);

	/**
	 * Perform a single-hit line trace (first blocking hit only).
	 *
	 * @param World World context
	 * @param Start Trace start point
	 * @param End Trace end point
	 * @param TraceProfile Collision profile name
	 * @param ActorsToIgnore Actors to exclude
	 * @param OutHit Output hit result
	 * @return True if hit occurred
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Trace", meta = (WorldContext = "WorldContextObject"))
	static bool PerformLineTraceSingle(
		const UObject* WorldContextObject,
		const FVector& Start,
		const FVector& End,
		FName TraceProfile,
		const TArray<AActor*>& ActorsToIgnore,
		FHitResult& OutHit
	);

	//========================================================================
	// Aim Point Calculation
	//========================================================================

	/**
	 * Get where the player is aiming (screen center raycast).
	 * Traces from camera forward to find aim target point.
	 *
	 * @param PlayerController Player controller for viewpoint
	 * @param MaxRange Maximum trace distance
	 * @param OutCameraLocation Output camera world location
	 * @param OutAimPoint Output aim target point (hit location or max range endpoint)
	 * @return True if valid aim point was calculated
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Trace")
	static bool GetAimPoint(
		APlayerController* PlayerController,
		float MaxRange,
		FVector& OutCameraLocation,
		FVector& OutAimPoint
	);

	/**
	 * Get aim direction from camera forward vector.
	 *
	 * @param PlayerController Player controller
	 * @param OutDirection Output normalized aim direction
	 * @return True if valid direction was obtained
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Trace")
	static bool GetAimDirection(
		APlayerController* PlayerController,
		FVector& OutDirection
	);

	//========================================================================
	// Debug Visualization
	//========================================================================

	/**
	 * Draw debug visualization for trace results.
	 * Color scheme:
	 * - Green: No blocking hit (clean shot)
	 * - Red: Blocking hit (impact)
	 * - Orange: Non-blocking hit (penetration)
	 * - Blue: Surface normal
	 *
	 * @param World World context
	 * @param Start Trace start point
	 * @param HitResults Array of hit results to visualize
	 * @param DrawTime Duration for debug visualization
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Trace|Debug", meta = (WorldContext = "WorldContextObject"))
	static void DrawDebugTrace(
		const UObject* WorldContextObject,
		const FVector& Start,
		const TArray<FHitResult>& HitResults,
		float DrawTime = 2.0f
	);

	/**
	 * Draw debug visualization for a single hit.
	 *
	 * @param World World context
	 * @param Hit Hit result to visualize
	 * @param bIsBlockingHit Whether this is a blocking hit
	 * @param DrawTime Duration for debug visualization
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Trace|Debug", meta = (WorldContext = "WorldContextObject"))
	static void DrawDebugHit(
		const UObject* WorldContextObject,
		const FHitResult& Hit,
		bool bIsBlockingHit,
		float DrawTime = 2.0f
	);

	//========================================================================
	// Utility Functions
	//========================================================================

	/**
	 * Apply spread to a direction vector.
	 * Creates a random direction within a cone defined by spread angle.
	 *
	 * @param Direction Base direction (should be normalized)
	 * @param SpreadAngle Spread angle in degrees (half-cone angle)
	 * @param RandomSeed Seed for deterministic spread (0 = random)
	 * @return Direction with spread applied
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Trace")
	static FVector ApplySpreadToDirection(
		const FVector& Direction,
		float SpreadAngle,
		int32 RandomSeed = 0
	);

	/**
	 * Calculate end point from start, direction, and range.
	 *
	 * @param Start Start point
	 * @param Direction Normalized direction
	 * @param Range Trace distance
	 * @return End point
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Trace")
	static FVector CalculateTraceEndPoint(
		const FVector& Start,
		const FVector& Direction,
		float Range
	);

	/**
	 * Check if a bone name indicates a headshot.
	 *
	 * @param BoneName Bone name from hit result
	 * @return True if this is a head bone
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Trace")
	static bool IsHeadshot(FName BoneName);

	/**
	 * Get damage multiplier for a specific bone/hitzone.
	 *
	 * @param BoneName Bone name from hit result
	 * @return Damage multiplier (1.0 = normal, 2.0 = headshot, etc.)
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Trace")
	static float GetHitZoneDamageMultiplier(FName BoneName);

	//========================================================================
	// Constants
	//========================================================================

	/** Default maximum trace range */
	static constexpr float DefaultMaxRange = 10000.0f;

	/** Default collision profile for weapon traces */
	static const FName DefaultWeaponTraceProfile;

	/** Headshot damage multiplier */
	static constexpr float HeadshotDamageMultiplier = 2.0f;

	/** Limb damage multiplier */
	static constexpr float LimbDamageMultiplier = 0.75f;

	/** Debug sphere radius for hit visualization */
	static constexpr float DebugSphereRadius = 10.0f;

	/** Debug normal line length */
	static constexpr float DebugNormalLength = 50.0f;
};
