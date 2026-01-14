// SuspenseCoreDamageCameraShake.h
// SuspenseCore - Damage Camera Shake
// Copyright Suspense Team. All Rights Reserved.
//
// Camera shake for damage received: Light, Heavy, Critical hits.
// Directional shake based on damage source location.
//
// Usage:
//   EventBus->Publish(SuspenseCoreTags::Event::Camera::ShakeDamage, EventData);
//   EventData.SetString("Type", "Light|Heavy|Critical");
//   EventData.SetFloat("Scale", DamageAmount / 100.0f);

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "SuspenseCore/CameraShake/SuspenseCoreWeaponCameraShake.h"
#include "SuspenseCoreDamageCameraShake.generated.h"

/**
 * Camera shake parameters for damage feedback.
 * Provides presets for Light, Heavy, and Critical damage.
 */
USTRUCT(BlueprintType)
struct PLAYERCORE_API FSuspenseCoreDamageShakeParams
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

	/** Roll rotation oscillation - head tilt from impact */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
	FSuspenseCoreOscillator Roll;

	/** X (forward/back) location oscillation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location")
	FSuspenseCoreOscillator LocationX;

	/** Y (left/right) location oscillation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location")
	FSuspenseCoreOscillator LocationY;

	/** Z (up/down) location oscillation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location")
	FSuspenseCoreOscillator LocationZ;

	/** FOV punch effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FOV")
	FSuspenseCoreOscillator FOV;

	/** Default constructor */
	FSuspenseCoreDamageShakeParams()
		: Duration(0.2f)
		, BlendInTime(0.0f)  // Immediate impact
		, BlendOutTime(0.15f)
		, Pitch(1.0f, 15.0f)
		, Yaw(0.8f, 12.0f)
		, Roll(0.5f, 10.0f)
		, LocationX(1.0f, 18.0f)
		, LocationY(1.0f, 18.0f)
		, LocationZ(0.5f, 15.0f)
		, FOV(0.3f, 20.0f)
	{
	}

	/** Light damage preset - small hits, scratches */
	static FSuspenseCoreDamageShakeParams GetLightPreset()
	{
		FSuspenseCoreDamageShakeParams Params;
		Params.Duration = 0.12f;
		Params.BlendOutTime = 0.1f;
		Params.Pitch = FSuspenseCoreOscillator(0.5f, 20.0f);
		Params.Yaw = FSuspenseCoreOscillator(0.3f, 18.0f);
		Params.Roll = FSuspenseCoreOscillator(0.2f, 15.0f);
		Params.LocationX = FSuspenseCoreOscillator(0.5f, 22.0f);
		Params.LocationY = FSuspenseCoreOscillator(0.5f, 22.0f);
		Params.LocationZ = FSuspenseCoreOscillator(0.3f, 20.0f);
		Params.FOV = FSuspenseCoreOscillator(0.1f, 25.0f);
		return Params;
	}

	/** Heavy damage preset - significant hits */
	static FSuspenseCoreDamageShakeParams GetHeavyPreset()
	{
		FSuspenseCoreDamageShakeParams Params;
		Params.Duration = 0.25f;
		Params.BlendOutTime = 0.2f;
		Params.Pitch = FSuspenseCoreOscillator(1.5f, 12.0f);
		Params.Yaw = FSuspenseCoreOscillator(1.2f, 10.0f);
		Params.Roll = FSuspenseCoreOscillator(0.8f, 8.0f);
		Params.LocationX = FSuspenseCoreOscillator(2.0f, 15.0f);
		Params.LocationY = FSuspenseCoreOscillator(2.0f, 15.0f);
		Params.LocationZ = FSuspenseCoreOscillator(1.0f, 12.0f);
		Params.FOV = FSuspenseCoreOscillator(0.5f, 18.0f);
		return Params;
	}

	/** Critical damage preset - near-death hits, headshots */
	static FSuspenseCoreDamageShakeParams GetCriticalPreset()
	{
		FSuspenseCoreDamageShakeParams Params;
		Params.Duration = 0.4f;
		Params.BlendOutTime = 0.3f;
		Params.Pitch = FSuspenseCoreOscillator(3.0f, 8.0f);
		Params.Yaw = FSuspenseCoreOscillator(2.5f, 7.0f);
		Params.Roll = FSuspenseCoreOscillator(1.5f, 6.0f);
		Params.LocationX = FSuspenseCoreOscillator(4.0f, 10.0f);
		Params.LocationY = FSuspenseCoreOscillator(4.0f, 10.0f);
		Params.LocationZ = FSuspenseCoreOscillator(2.0f, 8.0f);
		Params.FOV = FSuspenseCoreOscillator(1.0f, 12.0f);
		return Params;
	}

	/** Headshot preset - disorienting head impact */
	static FSuspenseCoreDamageShakeParams GetHeadshotPreset()
	{
		FSuspenseCoreDamageShakeParams Params;
		Params.Duration = 0.5f;
		Params.BlendOutTime = 0.4f;
		Params.Pitch = FSuspenseCoreOscillator(4.0f, 6.0f);
		Params.Yaw = FSuspenseCoreOscillator(3.0f, 5.0f);
		Params.Roll = FSuspenseCoreOscillator(2.0f, 5.0f);
		Params.LocationX = FSuspenseCoreOscillator(5.0f, 8.0f);
		Params.LocationY = FSuspenseCoreOscillator(5.0f, 8.0f);
		Params.LocationZ = FSuspenseCoreOscillator(3.0f, 6.0f);
		Params.FOV = FSuspenseCoreOscillator(1.5f, 10.0f);
		return Params;
	}
};

/**
 * USuspenseCoreDamageCameraShakePattern
 *
 * Custom camera shake pattern for damage feedback.
 */
UCLASS()
class PLAYERCORE_API USuspenseCoreDamageCameraShakePattern : public UCameraShakePattern
{
	GENERATED_BODY()

public:
	/** Shake parameters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern")
	FSuspenseCoreDamageShakeParams ShakeParams;

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
 * USuspenseCoreDamageCameraShake
 *
 * Main camera shake class for damage feedback.
 * Uses USuspenseCoreDamageCameraShakePattern for shake behavior.
 */
UCLASS(Blueprintable, BlueprintType)
class PLAYERCORE_API USuspenseCoreDamageCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	/**
	 * Set shake parameters before starting.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CameraShake")
	void SetShakeParams(const FSuspenseCoreDamageShakeParams& NewParams);

	/**
	 * Apply damage type preset.
	 * @param DamageType "Light", "Heavy", "Critical", "Headshot"
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CameraShake")
	void ApplyDamagePreset(const FString& DamageType);
};

