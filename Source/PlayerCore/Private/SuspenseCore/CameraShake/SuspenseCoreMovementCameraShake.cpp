// SuspenseCoreMovementCameraShake.cpp
// SuspenseCore - Movement Camera Shake Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/CameraShake/SuspenseCoreMovementCameraShake.h"

//========================================================================
// USuspenseCoreMovementCameraShakePattern
//========================================================================

void USuspenseCoreMovementCameraShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
	// Reset state
	ElapsedTime = 0.0f;
	CurrentScale = 1.0f;
	bIsFinished = false;

	// Initialize working oscillators from params
	PitchOsc = ShakeParams.Pitch;
	YawOsc = ShakeParams.Yaw;
	RollOsc = ShakeParams.Roll;
	LocZOsc = ShakeParams.LocationZ;

	// Reset all oscillator times
	PitchOsc.Reset();
	YawOsc.Reset();
	RollOsc.Reset();
	LocZOsc.Reset();
}

void USuspenseCoreMovementCameraShakePattern::UpdateShakePatternImpl(
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
	LocationDelta.Z = LocZOsc.Update(DeltaTime, EffectiveScale);

	// Apply to result
	OutResult.Location = LocationDelta;
	OutResult.Rotation = RotationDelta;
	OutResult.FOV = 0.0f;
}

void USuspenseCoreMovementCameraShakePattern::StopShakePatternImpl(const FCameraShakePatternStopParams& Params)
{
	if (Params.bImmediately)
	{
		bIsFinished = true;
	}
}

bool USuspenseCoreMovementCameraShakePattern::IsFinishedImpl() const
{
	return bIsFinished;
}

float USuspenseCoreMovementCameraShakePattern::CalculateBlendAmount() const
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
// USuspenseCoreMovementCameraShake
//========================================================================

void USuspenseCoreMovementCameraShake::SetShakeParams(const FSuspenseCoreMovementShakeParams& NewParams)
{
	if (UCameraShakePattern* Pattern = GetRootShakePattern())
	{
		if (USuspenseCoreMovementCameraShakePattern* MovementPattern = Cast<USuspenseCoreMovementCameraShakePattern>(Pattern))
		{
			MovementPattern->ShakeParams = NewParams;
		}
	}
}

void USuspenseCoreMovementCameraShake::ApplyMovementPreset(const FString& MovementType)
{
	FSuspenseCoreMovementShakeParams Params;

	if (MovementType.Equals(TEXT("Jump"), ESearchCase::IgnoreCase))
	{
		Params = FSuspenseCoreMovementShakeParams::GetJumpPreset();
	}
	else if (MovementType.Equals(TEXT("Landing"), ESearchCase::IgnoreCase))
	{
		Params = FSuspenseCoreMovementShakeParams::GetLandingPreset();
	}
	else if (MovementType.Equals(TEXT("HardLanding"), ESearchCase::IgnoreCase))
	{
		Params = FSuspenseCoreMovementShakeParams::GetHardLandingPreset();
	}
	else if (MovementType.Equals(TEXT("Sprint"), ESearchCase::IgnoreCase))
	{
		Params = FSuspenseCoreMovementShakeParams::GetSprintPreset();
	}
	else if (MovementType.Equals(TEXT("Crouch"), ESearchCase::IgnoreCase))
	{
		Params = FSuspenseCoreMovementShakeParams::GetCrouchPreset();
	}
	else
	{
		// Default to jump
		Params = FSuspenseCoreMovementShakeParams::GetJumpPreset();
	}

	SetShakeParams(Params);
}

