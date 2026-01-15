// SuspenseCoreCameraShakeDataAsset.h
// SuspenseCore - Camera Shake Configuration Data Asset
// Copyright Suspense Team. All Rights Reserved.
//
// Data-driven configuration for camera shakes.
// Allows designers to tune shake parameters in Editor without code changes.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SuspenseCoreCameraShakeTypes.h"
#include "SuspenseCoreWeaponCameraShake.h"
#include "SuspenseCoreMovementCameraShake.h"
#include "SuspenseCoreDamageCameraShake.h"
#include "SuspenseCoreExplosionCameraShake.h"
#include "SuspenseCoreCameraShakeDataAsset.generated.h"

/**
 * Weapon shake preset configuration
 */
USTRUCT(BlueprintType)
struct PLAYERCORE_API FSuspenseCoreWeaponShakePreset
{
	GENERATED_BODY()

	/** Preset name (e.g., "Rifle", "Pistol", "SMG") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset")
	FName PresetName;

	/** Shake parameters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset")
	FSuspenseCoreWeaponShakeParams ShakeParams;

	/** Priority in layered system */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset")
	ESuspenseCoreShakePriority Priority = ESuspenseCoreShakePriority::Weapon;

	/** Base scale multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float BaseScale = 1.0f;

	FSuspenseCoreWeaponShakePreset()
		: Priority(ESuspenseCoreShakePriority::Weapon)
		, BaseScale(1.0f)
	{
	}
};

/**
 * Movement shake preset configuration
 */
USTRUCT(BlueprintType)
struct PLAYERCORE_API FSuspenseCoreMovementShakePreset
{
	GENERATED_BODY()

	/** Preset name (e.g., "Landing", "HardLanding", "Sprint") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset")
	FName PresetName;

	/** Shake parameters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset")
	FSuspenseCoreMovementShakeParams ShakeParams;

	/** Priority in layered system */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset")
	ESuspenseCoreShakePriority Priority = ESuspenseCoreShakePriority::Movement;

	/** Base scale multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float BaseScale = 1.0f;

	FSuspenseCoreMovementShakePreset()
		: Priority(ESuspenseCoreShakePriority::Movement)
		, BaseScale(1.0f)
	{
	}
};

/**
 * Damage shake preset configuration
 */
USTRUCT(BlueprintType)
struct PLAYERCORE_API FSuspenseCoreDamageShakePreset
{
	GENERATED_BODY()

	/** Preset name (e.g., "Light", "Heavy", "Critical") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset")
	FName PresetName;

	/** Shake parameters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset")
	FSuspenseCoreDamageShakeParams ShakeParams;

	/** Priority in layered system */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset")
	ESuspenseCoreShakePriority Priority = ESuspenseCoreShakePriority::Combat;

	/** Base scale multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float BaseScale = 1.0f;

	FSuspenseCoreDamageShakePreset()
		: Priority(ESuspenseCoreShakePriority::Combat)
		, BaseScale(1.0f)
	{
	}
};

/**
 * Explosion shake preset configuration
 */
USTRUCT(BlueprintType)
struct PLAYERCORE_API FSuspenseCoreExplosionShakePreset
{
	GENERATED_BODY()

	/** Preset name (e.g., "Grenade", "Artillery", "Vehicle") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset")
	FName PresetName;

	/** Shake parameters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset")
	FSuspenseCoreExplosionShakeParams ShakeParams;

	/** Priority in layered system */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset")
	ESuspenseCoreShakePriority Priority = ESuspenseCoreShakePriority::Environmental;

	/** Base scale multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float BaseScale = 1.0f;

	/** Maximum effective distance (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset", meta = (ClampMin = "0.0"))
	float MaxDistance = 3000.0f;

	/** Distance falloff curve (0 = linear, 1 = exponential) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DistanceFalloff = 0.5f;

	FSuspenseCoreExplosionShakePreset()
		: Priority(ESuspenseCoreShakePriority::Environmental)
		, BaseScale(1.0f)
		, MaxDistance(3000.0f)
		, DistanceFalloff(0.5f)
	{
	}
};

/**
 * USuspenseCoreCameraShakeDataAsset
 *
 * Data-driven camera shake configuration.
 * Create in Content Browser: Right-click > Miscellaneous > Data Asset > SuspenseCoreCameraShakeDataAsset
 *
 * Features:
 * - Weapon shake presets (by weapon type)
 * - Movement shake presets (landing, sprint, etc.)
 * - Damage shake presets (by damage type)
 * - Explosion shake presets (by explosion type)
 * - Global configuration
 *
 * Usage:
 *   - Create DataAsset in Content Browser
 *   - Assign to CameraShakeComponent
 *   - Configure presets for your game
 */
UCLASS(BlueprintType)
class PLAYERCORE_API USuspenseCoreCameraShakeDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	USuspenseCoreCameraShakeDataAsset();

	// =========================================================================
	// Global Configuration
	// =========================================================================

	/** Global master scale for all shakes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Global", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float MasterScale = 1.0f;

	/** Use Perlin Noise by default for more organic feel */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Global")
	bool bUsePerlinNoiseByDefault = true;

	/** Default oscillator mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Global")
	ESuspenseCoreOscillatorMode DefaultOscillatorMode = ESuspenseCoreOscillatorMode::Combined;

	/** Enable priority-based blending */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Global")
	bool bEnablePriorityBlending = true;

	// =========================================================================
	// Weapon Shake Presets
	// =========================================================================

	/** Weapon shake presets by type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Shakes")
	TArray<FSuspenseCoreWeaponShakePreset> WeaponPresets;

	// =========================================================================
	// Movement Shake Presets
	// =========================================================================

	/** Movement shake presets by type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Shakes")
	TArray<FSuspenseCoreMovementShakePreset> MovementPresets;

	// =========================================================================
	// Damage Shake Presets
	// =========================================================================

	/** Damage shake presets by type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Shakes")
	TArray<FSuspenseCoreDamageShakePreset> DamagePresets;

	// =========================================================================
	// Explosion Shake Presets
	// =========================================================================

	/** Explosion shake presets by type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Explosion Shakes")
	TArray<FSuspenseCoreExplosionShakePreset> ExplosionPresets;

	// =========================================================================
	// Lookup Functions
	// =========================================================================

	/** Find weapon preset by name */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|CameraShake")
	bool FindWeaponPreset(FName PresetName, FSuspenseCoreWeaponShakePreset& OutPreset) const;

	/** Find movement preset by name */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|CameraShake")
	bool FindMovementPreset(FName PresetName, FSuspenseCoreMovementShakePreset& OutPreset) const;

	/** Find damage preset by name */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|CameraShake")
	bool FindDamagePreset(FName PresetName, FSuspenseCoreDamageShakePreset& OutPreset) const;

	/** Find explosion preset by name */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|CameraShake")
	bool FindExplosionPreset(FName PresetName, FSuspenseCoreExplosionShakePreset& OutPreset) const;

	// =========================================================================
	// UPrimaryDataAsset Interface
	// =========================================================================

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

protected:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
