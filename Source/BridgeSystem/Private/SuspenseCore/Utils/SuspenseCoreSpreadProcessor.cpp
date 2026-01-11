// SuspenseCoreSpreadProcessor.cpp
// SuspenseCore - Weapon Spread Calculator Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Utils/SuspenseCoreSpreadProcessor.h"

// Note: CalculateSpreadFromAttributes implementation moved to GAS module
// to avoid circular dependency. Use CalculateCurrentSpread with extracted values.

//========================================================================
// Main Calculation Functions
//========================================================================

float USuspenseCoreSpreadProcessor::CalculateSpread(
	const FSuspenseCoreSpreadInput& Input,
	const FSuspenseCoreSpreadModifiers& Modifiers)
{
	float CurrentSpread = Input.BaseSpread;

	// Apply aiming modifier (typically reduces spread)
	if (Input.bIsAiming)
	{
		CurrentSpread *= Modifiers.AimingModifier;
	}

	// Apply crouching modifier (typically reduces spread)
	if (Input.bIsCrouching && !Input.bIsSprinting)
	{
		CurrentSpread *= Modifiers.CrouchingModifier;
	}

	// Apply sprinting modifier (increases spread significantly)
	if (Input.bIsSprinting)
	{
		CurrentSpread *= Modifiers.SprintingModifier;
	}

	// Apply jumping/in-air modifier (increases spread significantly)
	if (Input.bIsInAir)
	{
		CurrentSpread *= Modifiers.JumpingModifier;
	}

	// Apply movement modifier (speed-based increase)
	const float MovementMod = GetMovementModifier(
		Input.MovementSpeed,
		Input.StationaryThreshold,
		Modifiers.MovementSpeedFactor
	);
	CurrentSpread *= MovementMod;

	// Apply fire mode modifier
	const float FireModeMod = GetFireModeModifier(
		Input.bIsAutoFire,
		Input.bIsBurstFire,
		Modifiers.AutoFireModifier,
		Modifiers.BurstFireModifier
	);
	CurrentSpread *= FireModeMod;

	// Apply recoil modifier (increases with continuous fire)
	CurrentSpread *= Input.RecoilModifier;

	// Clamp to valid range
	return FMath::Clamp(CurrentSpread, MinSpread, MaxSpread);
}

float USuspenseCoreSpreadProcessor::CalculateCurrentSpread(
	float BaseSpread,
	bool bIsAiming,
	float MovementSpeed,
	float RecoilModifier)
{
	// Build simplified input
	FSuspenseCoreSpreadInput Input;
	Input.BaseSpread = BaseSpread;
	Input.bIsAiming = bIsAiming;
	Input.MovementSpeed = MovementSpeed;
	Input.RecoilModifier = RecoilModifier;

	// Use default modifiers
	const FSuspenseCoreSpreadModifiers Modifiers;

	// Calculate with simplified logic
	float CurrentSpread = BaseSpread;

	// Aiming reduces spread
	if (bIsAiming)
	{
		CurrentSpread *= Modifiers.AimingModifier;
	}

	// Movement increases spread
	const float MovementMod = GetMovementModifier(MovementSpeed);
	CurrentSpread *= MovementMod;

	// Apply recoil
	CurrentSpread *= RecoilModifier;

	return FMath::Clamp(CurrentSpread, MinSpread, MaxSpread);
}

// NOTE: CalculateSpreadFromAttributes has been moved to GAS module
// See USuspenseCoreSpreadCalculator::CalculateSpreadWithAttributes

//========================================================================
// Individual Modifier Calculations
//========================================================================

float USuspenseCoreSpreadProcessor::GetMovementModifier(
	float MovementSpeed,
	float StationaryThreshold,
	float SpeedFactor)
{
	// If below threshold, no movement penalty
	if (MovementSpeed <= StationaryThreshold)
	{
		return 1.0f;
	}

	// Calculate movement-based increase
	const float EffectiveSpeed = MovementSpeed - StationaryThreshold;
	const float MovementIncrease = EffectiveSpeed * SpeedFactor;

	// Return modifier, capped at maximum
	return FMath::Min(1.0f + MovementIncrease, MaxMovementModifier);
}

float USuspenseCoreSpreadProcessor::GetFireModeModifier(
	bool bIsAutoFire,
	bool bIsBurstFire,
	float AutoMod,
	float BurstMod)
{
	if (bIsAutoFire)
	{
		return AutoMod;
	}

	if (bIsBurstFire)
	{
		return BurstMod;
	}

	// Semi-auto / single shot has no penalty
	return 1.0f;
}

//========================================================================
// Default Modifiers
//========================================================================

FSuspenseCoreSpreadModifiers USuspenseCoreSpreadProcessor::GetDefaultModifiers()
{
	return FSuspenseCoreSpreadModifiers();
}
