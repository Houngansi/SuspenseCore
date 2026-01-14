// SuspenseCoreDamageCameraShake.cpp
// SuspenseCore - Damage Camera Shake Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/CameraShake/SuspenseCoreDamageCameraShake.h"

//========================================================================
// USuspenseCoreDamageCameraShakePattern
//========================================================================

void USuspenseCoreDamageCameraShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
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

void USuspenseCoreDamageCameraShakePattern::UpdateShakePatternImpl(
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

void USuspenseCoreDamageCameraShakePattern::StopShakePatternImpl(const FCameraShakePatternStopParams& Params)
{
	if (Params.bImmediately)
	{
		bIsFinished = true;
	}
}

bool USuspenseCoreDamageCameraShakePattern::IsFinishedImpl() const
{
	return bIsFinished;
}

float USuspenseCoreDamageCameraShakePattern::CalculateBlendAmount() const
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
// USuspenseCoreDamageCameraShake
//========================================================================

USuspenseCoreDamageCameraShake::USuspenseCoreDamageCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<USuspenseCoreDamageCameraShakePattern>(TEXT("RootShakePattern")))
{
}

void USuspenseCoreDamageCameraShake::SetShakeParams(const FSuspenseCoreDamageShakeParams& NewParams)
{
	if (UCameraShakePattern* Pattern = GetRootShakePattern())
	{
		if (USuspenseCoreDamageCameraShakePattern* DamagePattern = Cast<USuspenseCoreDamageCameraShakePattern>(Pattern))
		{
			DamagePattern->ShakeParams = NewParams;
		}
	}
}

void USuspenseCoreDamageCameraShake::ApplyDamagePreset(const FString& DamageType)
{
	FSuspenseCoreDamageShakeParams Params;

	if (DamageType.Equals(TEXT("Light"), ESearchCase::IgnoreCase))
	{
		Params = FSuspenseCoreDamageShakeParams::GetLightPreset();
	}
	else if (DamageType.Equals(TEXT("Heavy"), ESearchCase::IgnoreCase))
	{
		Params = FSuspenseCoreDamageShakeParams::GetHeavyPreset();
	}
	else if (DamageType.Equals(TEXT("Critical"), ESearchCase::IgnoreCase))
	{
		Params = FSuspenseCoreDamageShakeParams::GetCriticalPreset();
	}
	else if (DamageType.Equals(TEXT("Headshot"), ESearchCase::IgnoreCase))
	{
		Params = FSuspenseCoreDamageShakeParams::GetHeadshotPreset();
	}
	else
	{
		// Default to light
		Params = FSuspenseCoreDamageShakeParams::GetLightPreset();
	}

	SetShakeParams(Params);
}

