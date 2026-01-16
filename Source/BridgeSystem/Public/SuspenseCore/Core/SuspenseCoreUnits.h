// SuspenseCoreUnits.h
// SuspenseCore - Unit Conversion System (SSOT)
// Copyright Suspense Team. All Rights Reserved.
//
// SINGLE SOURCE OF TRUTH for all unit conversions in SuspenseCore.
// All modules should use these constants for consistent unit handling.
//
// ARCHITECTURE:
// =============================================================================
// DataTables/JSON store values in REAL-WORLD UNITS (meters, kg, m/s)
// Unreal Engine uses CENTIMETERS internally (1 UE unit = 1 cm)
// This header provides conversion constants to bridge the gap.
//
// IMPORTANT: Always convert at the USAGE POINT, not at data loading!
// This keeps DataTable values human-readable and consistent with Tarkov.
//
// @see Documentation/GAS/UnitConversionSystem.md
// @see SuspenseCoreWeaponAttributes.json - Range values in meters
// @see SuspenseCoreAmmoAttributes.json - Velocity in m/s
//
// Usage:
//   float TraceRangeUE = WeaponAttrs->GetMaxRange() * SuspenseCoreUnits::MetersToUnits;
//   float VelocityUE = AmmoAttrs->GetMuzzleVelocity() * SuspenseCoreUnits::MetersToUnits;

#pragma once

#include "CoreMinimal.h"

/**
 * SuspenseCoreUnits
 *
 * Centralized unit conversion constants for SuspenseCore.
 * All gameplay systems should reference these when converting
 * between real-world units (used in data) and UE units.
 *
 * CONVERSION PHILOSOPHY:
 * - DataTables store REAL-WORLD values (meters, seconds, kg)
 *   → Human-readable, matches Tarkov wiki, easy to balance
 * - Gameplay code converts TO UE units at point of use
 *   → Clear, explicit, traceable
 * - Never store converted values back to DataTables
 *   → Prevents confusion, maintains SSOT integrity
 *
 * EXAMPLES:
 * =========
 * Weapon MaxRange 600m (from JSON) → 60000 UE units for trace
 * Ammo MuzzleVelocity 890 m/s → 89000 UE units/s for projectile
 * Character height 1.8m → 180 UE units for capsule
 *
 * @see FSuspenseCoreWeaponAttributeRow::MaxRange - Stored in meters
 * @see FSuspenseCoreAmmoAttributeRow::MuzzleVelocity - Stored in m/s
 */
namespace SuspenseCoreUnits
{
	//==========================================================================
	// DISTANCE CONVERSIONS
	// UE uses centimeters: 1 UE unit = 1 cm = 0.01 m
	//==========================================================================

	/** Convert meters to Unreal units (centimeters)
	 *  Usage: float TraceRange = MaxRange_Meters * MetersToUnits;
	 *  Example: 600m * 100 = 60000 UE units
	 */
	constexpr float MetersToUnits = 100.0f;

	/** Convert Unreal units to meters
	 *  Usage: float Distance_M = TraceHit.Distance * UnitsToMeters;
	 *  Example: 35000 UE units * 0.01 = 350m
	 */
	constexpr float UnitsToMeters = 0.01f;

	/** Convert kilometers to Unreal units
	 *  Usage: float LongRange = Kilometers * KilometersToUnits;
	 *  Example: 1.2km * 100000 = 120000 UE units
	 */
	constexpr float KilometersToUnits = 100000.0f;

	/** Convert Unreal units to kilometers */
	constexpr float UnitsToKilometers = 0.00001f;

	/** Convert centimeters to Unreal units (1:1, for clarity) */
	constexpr float CentimetersToUnits = 1.0f;

	/** Convert millimeters to Unreal units */
	constexpr float MillimetersToUnits = 0.1f;

	//==========================================================================
	// VELOCITY CONVERSIONS
	// Game uses m/s in data, UE uses units/s (cm/s)
	//==========================================================================

	/** Convert meters per second to UE units per second
	 *  Usage: float ProjectileSpeed = MuzzleVelocity * MetersPerSecToUnitsPerSec;
	 *  Example: 890 m/s * 100 = 89000 UE units/s
	 */
	constexpr float MetersPerSecToUnitsPerSec = MetersToUnits;

	/** Convert UE units per second to meters per second */
	constexpr float UnitsPerSecToMetersPerSec = UnitsToMeters;

	//==========================================================================
	// MASS CONVERSIONS
	// Tarkov uses kilograms, UE physics uses kg (same units)
	//==========================================================================

	/** Convert grams to kilograms (for bullet mass)
	 *  Usage: float BulletMassKg = BulletMass_Grams * GramsToKilograms;
	 *  Example: 3.4g * 0.001 = 0.0034 kg
	 */
	constexpr float GramsToKilograms = 0.001f;

	/** Convert kilograms to grams */
	constexpr float KilogramsToGrams = 1000.0f;

	//==========================================================================
	// ANGLE CONVERSIONS
	// Game uses various angle units
	//==========================================================================

	/** Convert MOA (Minute of Angle) to degrees
	 *  1 MOA = 1/60 degree
	 *  Usage: float SpreadDegrees = MOA * MOAToDegrees;
	 */
	constexpr float MOAToDegrees = 1.0f / 60.0f;

	/** Convert degrees to MOA */
	constexpr float DegreesToMOA = 60.0f;

	/** Convert degrees to radians */
	constexpr float DegreesToRadians = UE_PI / 180.0f;

	/** Convert radians to degrees */
	constexpr float RadiansToDegrees = 180.0f / UE_PI;

	//==========================================================================
	// TIME CONVERSIONS
	// Game typically uses seconds
	//==========================================================================

	/** Milliseconds to seconds */
	constexpr float MillisecondsToSeconds = 0.001f;

	/** Seconds to milliseconds */
	constexpr float SecondsToMilliseconds = 1000.0f;

	//==========================================================================
	// TARKOV-SPECIFIC CONSTANTS
	// Values matching Escape from Tarkov for authentic feel
	//==========================================================================

	/** Maximum engagement distance for any weapon in the game (meters)
	 *  SVD/Mosin can reach ~1200m, this is ceiling for all traces */
	constexpr float MaxGameRangeMeters = 1500.0f;

	/** Maximum engagement distance in UE units */
	constexpr float MaxGameRangeUnits = MaxGameRangeMeters * MetersToUnits; // 150000 UE units

	/** Effective pistol range (meters) - Typical 50-100m */
	constexpr float PistolEffectiveRangeMeters = 50.0f;

	/** Effective SMG range (meters) - Typical 100-200m */
	constexpr float SMGEffectiveRangeMeters = 150.0f;

	/** Effective assault rifle range (meters) - Typical 300-500m */
	constexpr float AREffectiveRangeMeters = 400.0f;

	/** Effective DMR/sniper range (meters) - Typical 600-1200m */
	constexpr float DMREffectiveRangeMeters = 800.0f;

	//==========================================================================
	// DEFAULT FALLBACK VALUES (in UE units)
	// Used when weapon attributes are not available
	//==========================================================================

	/** Default trace range when no weapon attributes (10km in UE units)
	 *  This is a generous fallback to prevent issues during development */
	constexpr float DefaultTraceRangeUnits = 1000000.0f; // 10km

	/** Default effective range fallback (400m in UE units) */
	constexpr float DefaultEffectiveRangeUnits = 40000.0f; // 400m

	/** Minimum trace range to prevent zero-length traces */
	constexpr float MinTraceRangeUnits = 100.0f; // 1m minimum

	//==========================================================================
	// INLINE HELPER FUNCTIONS
	// For convenient conversions with validation
	//==========================================================================

	/**
	 * Convert weapon range from meters (DataTable) to UE units (trace).
	 * Includes validation and clamping for safety.
	 *
	 * @param RangeMeters Range value from WeaponAttributeSet (in meters)
	 * @return Range in UE units, clamped to valid range
	 */
	FORCEINLINE float ConvertRangeToUnits(float RangeMeters)
	{
		if (RangeMeters <= 0.0f)
		{
			return DefaultTraceRangeUnits;
		}

		float RangeUnits = RangeMeters * MetersToUnits;

		// Clamp to game maximum (prevent absurd trace distances)
		return FMath::Clamp(RangeUnits, MinTraceRangeUnits, MaxGameRangeUnits);
	}

	/**
	 * Convert velocity from m/s (DataTable) to UE units/s (physics).
	 *
	 * @param VelocityMS Velocity in meters per second
	 * @return Velocity in UE units per second
	 */
	FORCEINLINE float ConvertVelocityToUnits(float VelocityMS)
	{
		return VelocityMS * MetersPerSecToUnitsPerSec;
	}

	/**
	 * Convert distance from UE units (trace result) to meters (display).
	 *
	 * @param DistanceUnits Distance in UE units (e.g., HitResult.Distance)
	 * @return Distance in meters for UI display
	 */
	FORCEINLINE float ConvertDistanceToMeters(float DistanceUnits)
	{
		return DistanceUnits * UnitsToMeters;
	}

	/**
	 * Get readable distance string for UI.
	 *
	 * @param DistanceUnits Distance in UE units
	 * @return Formatted string like "350m" or "1.2km"
	 */
	FORCEINLINE FString GetDistanceString(float DistanceUnits)
	{
		float Meters = ConvertDistanceToMeters(DistanceUnits);

		if (Meters >= 1000.0f)
		{
			return FString::Printf(TEXT("%.1fkm"), Meters / 1000.0f);
		}
		return FString::Printf(TEXT("%.0fm"), Meters);
	}
}
