// SuspenseCoreExplosionCameraShake.cpp
// SuspenseCore - Explosion Camera Shake Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/CameraShake/SuspenseCoreExplosionCameraShake.h"

//========================================================================
// USuspenseCoreExplosionCameraShakePattern
//========================================================================

void USuspenseCoreExplosionCameraShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
	// Reset state
	ElapsedTime = 0.0f;
	CurrentScale = 1.0f;
	bIsFinished = false;

	// Initialize working oscillators from params
	PitchOsc = ShakeParams.Pitch;
	YawOsc = ShakeParams.Yaw;
	RollOsc = ShakeParams.Roll;
	LocXOsc = ShakeParams.LocationX;
	LocYOsc = ShakeParams.LocationY;
	LocZOsc = ShakeParams.LocationZ;
	FOVOsc = ShakeParams.FOV;

	// Reset all oscillator times
	PitchOsc.Reset();
	YawOsc.Reset();
	RollOsc.Reset();
	LocXOsc.Reset();
	LocYOsc.Reset();
	LocZOsc.Reset();
	FOVOsc.Reset();
}

void USuspenseCoreExplosionCameraShakePattern::UpdateShakePatternImpl(
	const FCameraShakePatternUpdateParams& Params,
	FCameraShakePatternUpdateResult& OutResult)
{
	if (bIsFinished)
	{
		return;
	}

	const float DeltaTime = Params.DeltaTime;
	ElapsedTime += DeltaTime;

	// Check if finished
	if (ElapsedTime >= ShakeParams.Duration)
	{
		bIsFinished = true;
		return;
	}

	// Calculate blend amount for smooth start/end
	const float BlendAmount = CalculateBlendAmount();
	const float EffectiveScale = CurrentScale * BlendAmount * Params.ShakeScale * Params.DynamicScale;

	// Update oscillators and apply to result
	FRotator RotationDelta = FRotator::ZeroRotator;
	RotationDelta.Pitch = PitchOsc.Update(DeltaTime, EffectiveScale);
	RotationDelta.Yaw = YawOsc.Update(DeltaTime, EffectiveScale);
	RotationDelta.Roll = RollOsc.Update(DeltaTime, EffectiveScale);

	FVector LocationDelta = FVector::ZeroVector;
	LocationDelta.X = LocXOsc.Update(DeltaTime, EffectiveScale);
	LocationDelta.Y = LocYOsc.Update(DeltaTime, EffectiveScale);
	LocationDelta.Z = LocZOsc.Update(DeltaTime, EffectiveScale);

	const float FOVDelta = FOVOsc.Update(DeltaTime, EffectiveScale);

	// Apply to result
	OutResult.Location = LocationDelta;
	OutResult.Rotation = RotationDelta;
	OutResult.FOV = FOVDelta;
}

void USuspenseCoreExplosionCameraShakePattern::StopShakePatternImpl(const FCameraShakePatternStopParams& Params)
{
	if (Params.bImmediately)
	{
		bIsFinished = true;
	}
}

bool USuspenseCoreExplosionCameraShakePattern::IsFinishedImpl() const
{
	return bIsFinished;
}

float USuspenseCoreExplosionCameraShakePattern::CalculateBlendAmount() const
{
	float BlendAmount = 1.0f;

	// Blend in
	if (ShakeParams.BlendInTime > 0.0f && ElapsedTime < ShakeParams.BlendInTime)
	{
		BlendAmount = ElapsedTime / ShakeParams.BlendInTime;
	}
	// Blend out
	else if (ShakeParams.BlendOutTime > 0.0f)
	{
		const float TimeRemaining = ShakeParams.Duration - ElapsedTime;
		if (TimeRemaining < ShakeParams.BlendOutTime)
		{
			BlendAmount = TimeRemaining / ShakeParams.BlendOutTime;
		}
	}

	// Smooth the blend with ease-out curve
	return FMath::InterpEaseOut(0.0f, 1.0f, BlendAmount, 2.0f);
}

//========================================================================
// USuspenseCoreExplosionCameraShake
//========================================================================

void USuspenseCoreExplosionCameraShake::SetShakeParams(const FSuspenseCoreExplosionShakeParams& NewParams)
{
	if (UCameraShakePattern* Pattern = GetRootShakePattern())
	{
		if (USuspenseCoreExplosionCameraShakePattern* ExplosionPattern = Cast<USuspenseCoreExplosionCameraShakePattern>(Pattern))
		{
			ExplosionPattern->ShakeParams = NewParams;
		}
	}
}

void USuspenseCoreExplosionCameraShake::ApplyExplosionPreset(const FString& ExplosionType)
{
	FSuspenseCoreExplosionShakeParams Params;

	if (ExplosionType.Equals(TEXT("Nearby"), ESearchCase::IgnoreCase))
	{
		Params = FSuspenseCoreExplosionShakeParams::GetNearbyPreset();
	}
	else if (ExplosionType.Equals(TEXT("Medium"), ESearchCase::IgnoreCase))
	{
		Params = FSuspenseCoreExplosionShakeParams::GetMediumPreset();
	}
	else if (ExplosionType.Equals(TEXT("Distant"), ESearchCase::IgnoreCase))
	{
		Params = FSuspenseCoreExplosionShakeParams::GetDistantPreset();
	}
	else if (ExplosionType.Equals(TEXT("Grenade"), ESearchCase::IgnoreCase))
	{
		Params = FSuspenseCoreExplosionShakeParams::GetGrenadePreset();
	}
	else if (ExplosionType.Equals(TEXT("Artillery"), ESearchCase::IgnoreCase))
	{
		Params = FSuspenseCoreExplosionShakeParams::GetArtilleryPreset();
	}
	else if (ExplosionType.Equals(TEXT("Vehicle"), ESearchCase::IgnoreCase))
	{
		Params = FSuspenseCoreExplosionShakeParams::GetVehiclePreset();
	}
	else
	{
		// Default to medium
		Params = FSuspenseCoreExplosionShakeParams::GetMediumPreset();
	}

	SetShakeParams(Params);
}

FSuspenseCoreExplosionShakeParams USuspenseCoreExplosionCameraShake::GetPresetForDistance(float Distance)
{
	// Distance in cm (UE units)
	// Nearby: 0 - 500cm (0-5m)
	// Medium: 500 - 1500cm (5-15m)
	// Distant: 1500 - 3000cm (15-30m)
	// Beyond 3000cm: No shake or very minimal

	if (Distance < 500.0f)
	{
		// Nearby - full intensity
		FSuspenseCoreExplosionShakeParams Params = FSuspenseCoreExplosionShakeParams::GetNearbyPreset();
		// Scale slightly based on exact distance
		float Scale = FMath::GetMappedRangeValueClamped(FVector2D(0.0f, 500.0f), FVector2D(1.0f, 0.8f), Distance);
		Params.Pitch.Amplitude *= Scale;
		Params.Yaw.Amplitude *= Scale;
		Params.Roll.Amplitude *= Scale;
		Params.LocationX.Amplitude *= Scale;
		Params.LocationY.Amplitude *= Scale;
		Params.LocationZ.Amplitude *= Scale;
		Params.FOV.Amplitude *= Scale;
		return Params;
	}
	else if (Distance < 1500.0f)
	{
		// Medium distance
		FSuspenseCoreExplosionShakeParams Params = FSuspenseCoreExplosionShakeParams::GetMediumPreset();
		float Scale = FMath::GetMappedRangeValueClamped(FVector2D(500.0f, 1500.0f), FVector2D(1.0f, 0.6f), Distance);
		Params.Pitch.Amplitude *= Scale;
		Params.Yaw.Amplitude *= Scale;
		Params.Roll.Amplitude *= Scale;
		Params.LocationX.Amplitude *= Scale;
		Params.LocationY.Amplitude *= Scale;
		Params.LocationZ.Amplitude *= Scale;
		Params.FOV.Amplitude *= Scale;
		return Params;
	}
	else if (Distance < 3000.0f)
	{
		// Distant
		FSuspenseCoreExplosionShakeParams Params = FSuspenseCoreExplosionShakeParams::GetDistantPreset();
		float Scale = FMath::GetMappedRangeValueClamped(FVector2D(1500.0f, 3000.0f), FVector2D(1.0f, 0.3f), Distance);
		Params.Pitch.Amplitude *= Scale;
		Params.Yaw.Amplitude *= Scale;
		Params.Roll.Amplitude *= Scale;
		Params.LocationX.Amplitude *= Scale;
		Params.LocationY.Amplitude *= Scale;
		Params.LocationZ.Amplitude *= Scale;
		Params.FOV.Amplitude *= Scale;
		return Params;
	}
	else
	{
		// Very distant - minimal shake
		FSuspenseCoreExplosionShakeParams Params = FSuspenseCoreExplosionShakeParams::GetDistantPreset();
		float Scale = 0.1f;
		Params.Pitch.Amplitude *= Scale;
		Params.Yaw.Amplitude *= Scale;
		Params.Roll.Amplitude *= Scale;
		Params.LocationX.Amplitude *= Scale;
		Params.LocationY.Amplitude *= Scale;
		Params.LocationZ.Amplitude *= Scale;
		Params.FOV.Amplitude *= Scale;
		Params.Duration *= 0.5f;  // Shorter duration for distant
		return Params;
	}
}

