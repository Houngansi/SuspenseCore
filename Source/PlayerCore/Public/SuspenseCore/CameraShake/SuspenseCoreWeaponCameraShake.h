// SuspenseCoreWeaponCameraShake.h
// SuspenseCore - Weapon Recoil Camera Shake
// Copyright Suspense Team. All Rights Reserved.
//
// Pattern-based camera shake for weapon recoil effects.
// Uses oscillator system for pitch, yaw, and location shake.
// Supports both Sin-wave and Perlin Noise modes for organic feel.
//
// Usage:
//   PlayerController->ClientStartCameraShake(USuspenseCoreWeaponCameraShake::StaticClass(), Scale);

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "SuspenseCoreWeaponCameraShake.generated.h"

/**
 * Oscillator mode for camera shake
 */
UENUM(BlueprintType)
enum class ESuspenseCoreOscillatorMode : uint8
{
	/** Classic sin-wave oscillation */
	SinWave,
	/** Perlin noise for organic feel (AAA standard) */
	PerlinNoise,
	/** Combined: Sin + Perlin for complex movement */
	Combined
};

/**
 * Advanced oscillator for camera shake calculations.
 * Supports Sin-wave, Perlin Noise, and Combined modes.
 * Perlin Noise provides more organic, less "mechanical" feel (Battlefield/CoD style).
 */
USTRUCT(BlueprintType)
struct PLAYERCORE_API FSuspenseCoreOscillator
{
	GENERATED_BODY()

	/** Oscillation amplitude (max displacement) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Oscillator")
	float Amplitude;

	/** Oscillation frequency (cycles per second) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Oscillator")
	float Frequency;

	/** Oscillator mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Oscillator")
	ESuspenseCoreOscillatorMode Mode;

	/** Perlin noise scale (higher = more detail) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Oscillator|Perlin", meta = (EditCondition = "Mode != ESuspenseCoreOscillatorMode::SinWave"))
	float NoiseScale;

	/** Random seed for unique noise pattern per instance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Oscillator|Perlin", meta = (EditCondition = "Mode != ESuspenseCoreOscillatorMode::SinWave"))
	float NoiseSeed;

	/** Current time accumulator */
	float Time;

	FSuspenseCoreOscillator()
		: Amplitude(0.0f)
		, Frequency(0.0f)
		, Mode(ESuspenseCoreOscillatorMode::SinWave)
		, NoiseScale(1.0f)
		, NoiseSeed(0.0f)
		, Time(0.0f)
	{
	}

	FSuspenseCoreOscillator(float InAmplitude, float InFrequency)
		: Amplitude(InAmplitude)
		, Frequency(InFrequency)
		, Mode(ESuspenseCoreOscillatorMode::SinWave)
		, NoiseScale(1.0f)
		, NoiseSeed(FMath::FRand() * 1000.0f)
		, Time(0.0f)
	{
	}

	FSuspenseCoreOscillator(float InAmplitude, float InFrequency, ESuspenseCoreOscillatorMode InMode)
		: Amplitude(InAmplitude)
		, Frequency(InFrequency)
		, Mode(InMode)
		, NoiseScale(1.0f)
		, NoiseSeed(FMath::FRand() * 1000.0f)
		, Time(0.0f)
	{
	}

	/**
	 * Update oscillator and return current value.
	 * @param DeltaTime Time since last update
	 * @param Scale Overall scale multiplier
	 * @return Current oscillation value
	 */
	float Update(float DeltaTime, float Scale = 1.0f)
	{
		Time += DeltaTime;

		float Result = 0.0f;

		switch (Mode)
		{
		case ESuspenseCoreOscillatorMode::SinWave:
			Result = CalculateSinWave();
			break;

		case ESuspenseCoreOscillatorMode::PerlinNoise:
			Result = CalculatePerlinNoise();
			break;

		case ESuspenseCoreOscillatorMode::Combined:
			// Mix 60% Perlin + 40% Sin for organic but rhythmic feel
			Result = CalculatePerlinNoise() * 0.6f + CalculateSinWave() * 0.4f;
			break;
		}

		return Result * Amplitude * Scale;
	}

	/** Reset oscillator state */
	void Reset()
	{
		Time = 0.0f;
	}

	/** Randomize seed for unique pattern */
	void RandomizeSeed()
	{
		NoiseSeed = FMath::FRand() * 1000.0f;
	}

private:
	/** Classic sin-wave calculation */
	float CalculateSinWave() const
	{
		return FMath::Sin(Time * Frequency * 2.0f * PI);
	}

	/** Perlin noise calculation - more organic feel */
	float CalculatePerlinNoise() const
	{
		// 2D Perlin noise using time and seed
		// This creates smooth, organic movement unlike mechanical sin waves
		const float NoiseX = Time * Frequency * NoiseScale;
		const float NoiseY = NoiseSeed;

		// FMath::PerlinNoise2D returns -1 to 1
		return FMath::PerlinNoise2D(FVector2D(NoiseX, NoiseY));
	}
};

/**
 * Camera shake configuration for weapon recoil.
 * Defines all oscillation parameters for a complete recoil feel.
 */
USTRUCT(BlueprintType)
struct PLAYERCORE_API FSuspenseCoreWeaponShakeParams
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

	/** Pitch (up/down) rotation oscillation - primary recoil feel */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
	FSuspenseCoreOscillator Pitch;

	/** Yaw (left/right) rotation oscillation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
	FSuspenseCoreOscillator Yaw;

	/** Roll rotation oscillation (usually minimal) */
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

	/** FOV oscillation (zoom effect) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FOV")
	FSuspenseCoreOscillator FOV;

	/** Default constructor with weapon-appropriate values */
	FSuspenseCoreWeaponShakeParams()
		: Duration(0.18f)
		, BlendInTime(0.0f)
		, BlendOutTime(0.12f)
		, Pitch(1.8f, 15.0f)       // Strong upward kick
		, Yaw(0.1f, 12.0f)         // Minimal side-to-side
		, Roll(0.0f, 0.0f)         // No roll
		, LocationX(0.2f, 18.0f)   // Minimal lateral
		, LocationY(-4.0f, 4.0f)   // Push camera back (recoil)
		, LocationZ(0.5f, 15.0f)   // Slight upward
		, FOV(0.2f, 20.0f)         // Subtle zoom
	{
	}

	/** Get rifle preset (stronger recoil) */
	static FSuspenseCoreWeaponShakeParams GetRiflePreset()
	{
		FSuspenseCoreWeaponShakeParams Params;
		Params.Duration = 0.2f;
		Params.Pitch = FSuspenseCoreOscillator(2.2f, 14.0f);
		Params.Yaw = FSuspenseCoreOscillator(0.15f, 11.0f);
		Params.LocationY = FSuspenseCoreOscillator(-5.0f, 4.0f);
		return Params;
	}

	/** Get pistol preset (snappy recoil) */
	static FSuspenseCoreWeaponShakeParams GetPistolPreset()
	{
		FSuspenseCoreWeaponShakeParams Params;
		Params.Duration = 0.15f;
		Params.Pitch = FSuspenseCoreOscillator(1.5f, 18.0f);
		Params.Yaw = FSuspenseCoreOscillator(0.08f, 14.0f);
		Params.LocationY = FSuspenseCoreOscillator(-3.0f, 5.0f);
		return Params;
	}

	/** Get SMG preset (fast, light recoil) */
	static FSuspenseCoreWeaponShakeParams GetSMGPreset()
	{
		FSuspenseCoreWeaponShakeParams Params;
		Params.Duration = 0.12f;
		Params.Pitch = FSuspenseCoreOscillator(1.2f, 20.0f);
		Params.Yaw = FSuspenseCoreOscillator(0.1f, 16.0f);
		Params.LocationY = FSuspenseCoreOscillator(-2.5f, 6.0f);
		return Params;
	}

	/** Get shotgun preset (heavy recoil) */
	static FSuspenseCoreWeaponShakeParams GetShotgunPreset()
	{
		FSuspenseCoreWeaponShakeParams Params;
		Params.Duration = 0.3f;
		Params.BlendOutTime = 0.2f;
		Params.Pitch = FSuspenseCoreOscillator(3.5f, 10.0f);
		Params.Yaw = FSuspenseCoreOscillator(0.3f, 8.0f);
		Params.LocationY = FSuspenseCoreOscillator(-8.0f, 3.0f);
		Params.LocationZ = FSuspenseCoreOscillator(1.0f, 12.0f);
		return Params;
	}

	/** Get sniper preset (heavy, slow recoil) */
	static FSuspenseCoreWeaponShakeParams GetSniperPreset()
	{
		FSuspenseCoreWeaponShakeParams Params;
		Params.Duration = 0.35f;
		Params.BlendOutTime = 0.25f;
		Params.Pitch = FSuspenseCoreOscillator(4.0f, 8.0f);
		Params.Yaw = FSuspenseCoreOscillator(0.2f, 6.0f);
		Params.LocationY = FSuspenseCoreOscillator(-10.0f, 2.5f);
		Params.LocationZ = FSuspenseCoreOscillator(1.5f, 10.0f);
		Params.FOV = FSuspenseCoreOscillator(0.5f, 15.0f);
		return Params;
	}
};

/**
 * USuspenseCoreWeaponCameraShakePattern
 *
 * Custom camera shake pattern for weapon recoil.
 * Implements oscillator-based shake with configurable parameters.
 */
UCLASS()
class PLAYERCORE_API USuspenseCoreWeaponCameraShakePattern : public UCameraShakePattern
{
	GENERATED_BODY()

public:
	/** Shake parameters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern")
	FSuspenseCoreWeaponShakeParams ShakeParams;

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

	/** Current scale (for blending) */
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
 * USuspenseCoreWeaponCameraShake
 *
 * Main camera shake class for weapon recoil.
 * Uses USuspenseCoreWeaponCameraShakePattern for shake behavior.
 *
 * Usage:
 *   // Start shake with default settings
 *   PlayerController->ClientStartCameraShake(USuspenseCoreWeaponCameraShake::StaticClass());
 *
 *   // Start shake with scale
 *   PlayerController->ClientStartCameraShake(USuspenseCoreWeaponCameraShake::StaticClass(), 1.5f);
 */
UCLASS(Blueprintable, BlueprintType)
class PLAYERCORE_API USuspenseCoreWeaponCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	/**
	 * Set shake parameters before starting.
	 * Call this before ClientStartCameraShake if custom params needed.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CameraShake")
	void SetShakeParams(const FSuspenseCoreWeaponShakeParams& NewParams);

	/**
	 * Apply weapon type preset.
	 * @param WeaponType "Rifle", "Pistol", "SMG", "Shotgun", "Sniper"
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CameraShake")
	void ApplyWeaponPreset(const FString& WeaponType);
};
