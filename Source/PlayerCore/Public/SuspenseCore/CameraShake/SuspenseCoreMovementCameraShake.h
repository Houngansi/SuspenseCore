// SuspenseCoreMovementCameraShake.h
// SuspenseCore - Movement Camera Shake
// Copyright Suspense Team. All Rights Reserved.
//
// Camera shake for movement actions: Jump, Landing, Sprint.
// Uses oscillator system for realistic movement feedback.
//
// Usage:
//   EventBus->Publish(SuspenseCoreTags::Event::Camera::ShakeMovement, EventData);
//   EventData.SetString("Type", "Jump|Landing|Sprint");
//   EventData.SetFloat("Scale", 1.0f);

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "SuspenseCore/CameraShake/SuspenseCoreWeaponCameraShake.h"
#include "SuspenseCoreMovementCameraShake.generated.h"

/**
 * Camera shake parameters for movement actions.
 * Provides presets for Jump, Landing, Sprint, and Crouch.
 */
USTRUCT(BlueprintType)
struct PLAYERCORE_API FSuspenseCoreMovementShakeParams
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

	/** Roll rotation oscillation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
	FSuspenseCoreOscillator Roll;

	/** Z (up/down) location oscillation - primary for movement */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location")
	FSuspenseCoreOscillator LocationZ;

	/** Default constructor */
	FSuspenseCoreMovementShakeParams()
		: Duration(0.2f)
		, BlendInTime(0.02f)
		, BlendOutTime(0.15f)
		, Pitch(0.5f, 12.0f)
		, Yaw(0.0f, 0.0f)
		, Roll(0.2f, 8.0f)
		, LocationZ(1.5f, 10.0f)
	{
	}

	/** Jump start preset - upward motion */
	static FSuspenseCoreMovementShakeParams GetJumpPreset()
	{
		FSuspenseCoreMovementShakeParams Params;
		Params.Duration = 0.15f;
		Params.BlendOutTime = 0.1f;
		Params.Pitch = FSuspenseCoreOscillator(-0.3f, 15.0f);  // Slight look down on jump
		Params.Roll = FSuspenseCoreOscillator(0.1f, 10.0f);
		Params.LocationZ = FSuspenseCoreOscillator(2.0f, 12.0f);  // Upward push
		return Params;
	}

	/** Landing preset - smooth impact feel */
	static FSuspenseCoreMovementShakeParams GetLandingPreset()
	{
		FSuspenseCoreMovementShakeParams Params;
		Params.Duration = 0.3f;
		Params.BlendInTime = 0.03f;   // Soft start
		Params.BlendOutTime = 0.25f;  // Gradual decay
		Params.Pitch = FSuspenseCoreOscillator(0.6f, 8.0f);    // Gentle head bob
		Params.Roll = FSuspenseCoreOscillator(0.15f, 6.0f);    // Subtle roll
		Params.LocationZ = FSuspenseCoreOscillator(-2.5f, 6.0f); // Soft downward
		return Params;
	}

	/** Hard landing preset - heavy impact from high fall */
	static FSuspenseCoreMovementShakeParams GetHardLandingPreset()
	{
		FSuspenseCoreMovementShakeParams Params;
		Params.Duration = 0.45f;
		Params.BlendInTime = 0.02f;
		Params.BlendOutTime = 0.35f;
		Params.Pitch = FSuspenseCoreOscillator(1.5f, 10.0f);
		Params.Roll = FSuspenseCoreOscillator(0.4f, 8.0f);
		Params.LocationZ = FSuspenseCoreOscillator(-5.0f, 5.0f);
		return Params;
	}

	/** Sprint preset - subtle rhythmic shake */
	static FSuspenseCoreMovementShakeParams GetSprintPreset()
	{
		FSuspenseCoreMovementShakeParams Params;
		Params.Duration = 0.1f;  // Short, repeating
		Params.BlendInTime = 0.02f;
		Params.BlendOutTime = 0.05f;
		Params.Pitch = FSuspenseCoreOscillator(0.15f, 8.0f);
		Params.Roll = FSuspenseCoreOscillator(0.1f, 6.0f);
		Params.LocationZ = FSuspenseCoreOscillator(0.8f, 10.0f);
		return Params;
	}

	/** Crouch preset - subtle downward motion */
	static FSuspenseCoreMovementShakeParams GetCrouchPreset()
	{
		FSuspenseCoreMovementShakeParams Params;
		Params.Duration = 0.2f;
		Params.BlendInTime = 0.05f;
		Params.BlendOutTime = 0.15f;
		Params.Pitch = FSuspenseCoreOscillator(0.2f, 10.0f);
		Params.LocationZ = FSuspenseCoreOscillator(-1.5f, 8.0f);
		return Params;
	}
};

/**
 * USuspenseCoreMovementCameraShakePattern
 *
 * Custom camera shake pattern for movement actions.
 */
UCLASS()
class PLAYERCORE_API USuspenseCoreMovementCameraShakePattern : public UCameraShakePattern
{
	GENERATED_BODY()

public:
	/** Shake parameters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern")
	FSuspenseCoreMovementShakeParams ShakeParams;

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
	FSuspenseCoreOscillator LocZOsc;

	/** Calculate blend amount based on elapsed time */
	float CalculateBlendAmount() const;
};

/**
 * USuspenseCoreMovementCameraShake
 *
 * Main camera shake class for movement actions.
 * Uses USuspenseCoreMovementCameraShakePattern for shake behavior.
 */
UCLASS(Blueprintable, BlueprintType)
class PLAYERCORE_API USuspenseCoreMovementCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	USuspenseCoreMovementCameraShake(const FObjectInitializer& ObjectInitializer);

	/**
	 * Set shake parameters before starting.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CameraShake")
	void SetShakeParams(const FSuspenseCoreMovementShakeParams& NewParams);

	/**
	 * Apply movement type preset.
	 * @param MovementType "Jump", "Landing", "HardLanding", "Sprint", "Crouch"
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CameraShake")
	void ApplyMovementPreset(const FString& MovementType);
};

