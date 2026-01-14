// SuspenseCoreExplosionCameraShake.h
// SuspenseCore - Explosion Camera Shake
// Copyright Suspense Team. All Rights Reserved.
//
// Camera shake for explosions: Nearby, Medium, Distant.
// Intensity scales with distance from explosion center.
//
// Usage:
//   EventBus->Publish(SuspenseCoreTags::Event::Camera::ShakeExplosion, EventData);
//   EventData.SetString("Type", "Grenade|Rocket|Artillery");
//   EventData.SetFloat("Distance", DistanceToExplosion);

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "SuspenseCore/CameraShake/SuspenseCoreWeaponCameraShake.h"
#include "SuspenseCoreExplosionCameraShake.generated.h"

/**
 * Camera shake parameters for explosion effects.
 * Provides presets for various explosion types and distances.
 */
USTRUCT(BlueprintType)
struct PLAYERCORE_API FSuspenseCoreExplosionShakeParams
{
	GENERATED_BODY()

	/** Total shake duration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0.0"))
	float Duration;

	/** Time to blend in the shake */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0.0"))
	float BlendInTime;

	/** Time to blend out the shake */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0.0"))
	float BlendOutTime;

	/** Pitch (up/down) rotation oscillation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
	FSuspenseCoreOscillator Pitch;

	/** Yaw (left/right) rotation oscillation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
	FSuspenseCoreOscillator Yaw;

	/** Roll rotation oscillation - disorienting effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
	FSuspenseCoreOscillator Roll;

	/** X (forward/back) location oscillation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location")
	FSuspenseCoreOscillator LocationX;

	/** Y (left/right) location oscillation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location")
	FSuspenseCoreOscillator LocationY;

	/** Z (up/down) location oscillation - shockwave effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location")
	FSuspenseCoreOscillator LocationZ;

	/** FOV punch effect - compression from shockwave */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FOV")
	FSuspenseCoreOscillator FOV;

	/** Default constructor */
	FSuspenseCoreExplosionShakeParams()
		: Duration(0.6f)
		, BlendInTime(0.0f)  // Immediate
		, BlendOutTime(0.4f)
		, Pitch(2.0f, 8.0f)
		, Yaw(1.5f, 7.0f)
		, Roll(1.0f, 6.0f)
		, LocationX(3.0f, 10.0f)
		, LocationY(3.0f, 10.0f)
		, LocationZ(4.0f, 8.0f)
		, FOV(1.5f, 12.0f)
	{
	}

	/** Nearby explosion (0-5m) - massive shake */
	static FSuspenseCoreExplosionShakeParams GetNearbyPreset()
	{
		FSuspenseCoreExplosionShakeParams Params;
		Params.Duration = 0.8f;
		Params.BlendOutTime = 0.6f;
		Params.Pitch = FSuspenseCoreOscillator(5.0f, 6.0f);
		Params.Yaw = FSuspenseCoreOscillator(4.0f, 5.0f);
		Params.Roll = FSuspenseCoreOscillator(3.0f, 4.0f);
		Params.LocationX = FSuspenseCoreOscillator(8.0f, 8.0f);
		Params.LocationY = FSuspenseCoreOscillator(8.0f, 8.0f);
		Params.LocationZ = FSuspenseCoreOscillator(10.0f, 6.0f);
		Params.FOV = FSuspenseCoreOscillator(3.0f, 10.0f);
		return Params;
	}

	/** Medium distance explosion (5-15m) */
	static FSuspenseCoreExplosionShakeParams GetMediumPreset()
	{
		FSuspenseCoreExplosionShakeParams Params;
		Params.Duration = 0.5f;
		Params.BlendOutTime = 0.35f;
		Params.Pitch = FSuspenseCoreOscillator(2.0f, 8.0f);
		Params.Yaw = FSuspenseCoreOscillator(1.5f, 7.0f);
		Params.Roll = FSuspenseCoreOscillator(1.0f, 6.0f);
		Params.LocationX = FSuspenseCoreOscillator(4.0f, 10.0f);
		Params.LocationY = FSuspenseCoreOscillator(4.0f, 10.0f);
		Params.LocationZ = FSuspenseCoreOscillator(5.0f, 8.0f);
		Params.FOV = FSuspenseCoreOscillator(1.5f, 12.0f);
		return Params;
	}

	/** Distant explosion (15-30m) - rumble effect */
	static FSuspenseCoreExplosionShakeParams GetDistantPreset()
	{
		FSuspenseCoreExplosionShakeParams Params;
		Params.Duration = 0.4f;
		Params.BlendOutTime = 0.3f;
		Params.Pitch = FSuspenseCoreOscillator(0.8f, 10.0f);
		Params.Yaw = FSuspenseCoreOscillator(0.5f, 9.0f);
		Params.Roll = FSuspenseCoreOscillator(0.3f, 8.0f);
		Params.LocationX = FSuspenseCoreOscillator(1.5f, 12.0f);
		Params.LocationY = FSuspenseCoreOscillator(1.5f, 12.0f);
		Params.LocationZ = FSuspenseCoreOscillator(2.0f, 10.0f);
		Params.FOV = FSuspenseCoreOscillator(0.5f, 15.0f);
		return Params;
	}

	/** Grenade preset - quick, sharp shake */
	static FSuspenseCoreExplosionShakeParams GetGrenadePreset()
	{
		FSuspenseCoreExplosionShakeParams Params;
		Params.Duration = 0.4f;
		Params.BlendOutTime = 0.3f;
		Params.Pitch = FSuspenseCoreOscillator(2.5f, 12.0f);
		Params.Yaw = FSuspenseCoreOscillator(2.0f, 10.0f);
		Params.Roll = FSuspenseCoreOscillator(1.5f, 8.0f);
		Params.LocationX = FSuspenseCoreOscillator(3.0f, 14.0f);
		Params.LocationY = FSuspenseCoreOscillator(3.0f, 14.0f);
		Params.LocationZ = FSuspenseCoreOscillator(4.0f, 12.0f);
		Params.FOV = FSuspenseCoreOscillator(1.0f, 16.0f);
		return Params;
	}

	/** Artillery/Mortar preset - heavy, prolonged shake */
	static FSuspenseCoreExplosionShakeParams GetArtilleryPreset()
	{
		FSuspenseCoreExplosionShakeParams Params;
		Params.Duration = 1.0f;
		Params.BlendOutTime = 0.7f;
		Params.Pitch = FSuspenseCoreOscillator(4.0f, 5.0f);
		Params.Yaw = FSuspenseCoreOscillator(3.5f, 4.0f);
		Params.Roll = FSuspenseCoreOscillator(2.5f, 3.0f);
		Params.LocationX = FSuspenseCoreOscillator(6.0f, 6.0f);
		Params.LocationY = FSuspenseCoreOscillator(6.0f, 6.0f);
		Params.LocationZ = FSuspenseCoreOscillator(8.0f, 5.0f);
		Params.FOV = FSuspenseCoreOscillator(2.0f, 8.0f);
		return Params;
	}

	/** Vehicle explosion preset - massive, sustained shake */
	static FSuspenseCoreExplosionShakeParams GetVehiclePreset()
	{
		FSuspenseCoreExplosionShakeParams Params;
		Params.Duration = 1.2f;
		Params.BlendOutTime = 0.8f;
		Params.Pitch = FSuspenseCoreOscillator(5.0f, 4.0f);
		Params.Yaw = FSuspenseCoreOscillator(4.0f, 3.5f);
		Params.Roll = FSuspenseCoreOscillator(3.0f, 3.0f);
		Params.LocationX = FSuspenseCoreOscillator(8.0f, 5.0f);
		Params.LocationY = FSuspenseCoreOscillator(8.0f, 5.0f);
		Params.LocationZ = FSuspenseCoreOscillator(10.0f, 4.0f);
		Params.FOV = FSuspenseCoreOscillator(2.5f, 7.0f);
		return Params;
	}
};

/**
 * USuspenseCoreExplosionCameraShakePattern
 *
 * Custom camera shake pattern for explosion effects.
 */
UCLASS()
class PLAYERCORE_API USuspenseCoreExplosionCameraShakePattern : public UCameraShakePattern
{
	GENERATED_BODY()

public:
	/** Shake parameters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern")
	FSuspenseCoreExplosionShakeParams ShakeParams;

protected:
	//~ Begin UCameraShakePattern Interface
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
	virtual void StopShakePatternImpl(const FCameraShakePatternStopParams& Params) override;
	virtual bool IsFinishedImpl() const override;
	//~ End UCameraShakePattern Interface

private:
	/** Current elapsed time */
	float ElapsedTime = 0.0f;

	/** Current scale */
	float CurrentScale = 1.0f;

	/** Is shake finished */
	bool bIsFinished = false;

	/** Working copies of oscillators */
	FSuspenseCoreOscillator PitchOsc;
	FSuspenseCoreOscillator YawOsc;
	FSuspenseCoreOscillator RollOsc;
	FSuspenseCoreOscillator LocXOsc;
	FSuspenseCoreOscillator LocYOsc;
	FSuspenseCoreOscillator LocZOsc;
	FSuspenseCoreOscillator FOVOsc;

	/** Calculate blend amount based on elapsed time */
	float CalculateBlendAmount() const;
};

/**
 * USuspenseCoreExplosionCameraShake
 *
 * Main camera shake class for explosion effects.
 * Uses USuspenseCoreExplosionCameraShakePattern for shake behavior.
 */
UCLASS(Blueprintable, BlueprintType)
class PLAYERCORE_API USuspenseCoreExplosionCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	/**
	 * Set shake parameters before starting.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CameraShake")
	void SetShakeParams(const FSuspenseCoreExplosionShakeParams& NewParams);

	/**
	 * Apply explosion type preset.
	 * @param ExplosionType "Nearby", "Medium", "Distant", "Grenade", "Artillery", "Vehicle"
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CameraShake")
	void ApplyExplosionPreset(const FString& ExplosionType);

	/**
	 * Calculate appropriate preset based on distance.
	 * @param Distance Distance from explosion in cm
	 * @return Scaled shake parameters
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CameraShake")
	static FSuspenseCoreExplosionShakeParams GetPresetForDistance(float Distance);
};

