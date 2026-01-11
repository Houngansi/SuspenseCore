// SuspenseCoreWeaponCameraShake.cpp
// SuspenseCore - Weapon Recoil Camera Shake Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/CameraShake/SuspenseCoreWeaponCameraShake.h"

//========================================================================
// USuspenseCoreWeaponCameraShakePattern
//========================================================================

USuspenseCoreWeaponCameraShakePattern::USuspenseCoreWeaponCameraShakePattern()
	: ElapsedTime(0.0f)
	, CurrentScale(1.0f)
	, bIsFinished(false)
{
}

void USuspenseCoreWeaponCameraShakePattern::StartShakePatternImpl(const FCameraShakeStartParams& Params)
{
	// Reset state
	ElapsedTime = 0.0f;
	CurrentScale = Params.Scale;
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

void USuspenseCoreWeaponCameraShakePattern::UpdateShakePatternImpl(
	const FCameraShakeUpdateParams& Params,
	FCameraShakeUpdateResult& OutResult)
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

void USuspenseCoreWeaponCameraShakePattern::StopShakePatternImpl(const FCameraShakeStopParams& Params)
{
	if (Params.bImmediately)
	{
		bIsFinished = true;
	}
	// If not immediate, let it blend out naturally
}

bool USuspenseCoreWeaponCameraShakePattern::IsFinishedImpl() const
{
	return bIsFinished;
}

float USuspenseCoreWeaponCameraShakePattern::CalculateBlendAmount() const
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
// USuspenseCoreWeaponCameraShake
//========================================================================

USuspenseCoreWeaponCameraShake::USuspenseCoreWeaponCameraShake()
{
	// Set default root shake pattern
	RootShakePattern = CreateDefaultSubobject<USuspenseCoreWeaponCameraShakePattern>(TEXT("DefaultShakePattern"));

	// Configure default settings
	bSingleInstance = false; // Allow multiple overlapping shakes
}

void USuspenseCoreWeaponCameraShake::SetShakeParams(const FSuspenseCoreWeaponShakeParams& NewParams)
{
	if (USuspenseCoreWeaponCameraShakePattern* Pattern = Cast<USuspenseCoreWeaponCameraShakePattern>(RootShakePattern))
	{
		Pattern->ShakeParams = NewParams;
	}
}

void USuspenseCoreWeaponCameraShake::ApplyWeaponPreset(const FString& WeaponType)
{
	FSuspenseCoreWeaponShakeParams Params;

	if (WeaponType.Equals(TEXT("Rifle"), ESearchCase::IgnoreCase))
	{
		Params = FSuspenseCoreWeaponShakeParams::GetRiflePreset();
	}
	else if (WeaponType.Equals(TEXT("Pistol"), ESearchCase::IgnoreCase))
	{
		Params = FSuspenseCoreWeaponShakeParams::GetPistolPreset();
	}
	else if (WeaponType.Equals(TEXT("SMG"), ESearchCase::IgnoreCase))
	{
		Params = FSuspenseCoreWeaponShakeParams::GetSMGPreset();
	}
	else if (WeaponType.Equals(TEXT("Shotgun"), ESearchCase::IgnoreCase))
	{
		Params = FSuspenseCoreWeaponShakeParams::GetShotgunPreset();
	}
	else if (WeaponType.Equals(TEXT("Sniper"), ESearchCase::IgnoreCase))
	{
		Params = FSuspenseCoreWeaponShakeParams::GetSniperPreset();
	}
	// Default to rifle if unknown
	else
	{
		Params = FSuspenseCoreWeaponShakeParams::GetRiflePreset();
	}

	SetShakeParams(Params);
}
