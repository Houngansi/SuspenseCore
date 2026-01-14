// SuspenseCoreRecoilConvergenceComponent.cpp
// SuspenseCore - Recoil Convergence Component Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Components/SuspenseCoreRecoilConvergenceComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

USuspenseCoreRecoilConvergenceComponent::USuspenseCoreRecoilConvergenceComponent()
	: AccumulatedPitch(0.0f)
	, AccumulatedYaw(0.0f)
	, TimeSinceLastImpulse(0.0f)
	, CurrentConvergenceDelay(0.1f)
	, CurrentConvergenceSpeed(5.0f)
	, CurrentErgonomics(42.0f)
	, bWaitingForDelay(false)
	, bIsConverging(false)
{
	// Start with tick disabled - enable only when needed
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void USuspenseCoreRecoilConvergenceComponent::BeginPlay()
{
	Super::BeginPlay();

	// Ensure tick is disabled at start
	SetComponentTickEnabled(false);
}

void USuspenseCoreRecoilConvergenceComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Early out if no offset
	if (!HasOffset())
	{
		bIsConverging = false;
		bWaitingForDelay = false;
		SetComponentTickEnabled(false);
		return;
	}

	// Update time since last impulse
	TimeSinceLastImpulse += DeltaTime;

	// Wait for convergence delay
	if (bWaitingForDelay)
	{
		if (TimeSinceLastImpulse >= CurrentConvergenceDelay)
		{
			bWaitingForDelay = false;
			bIsConverging = true;
			UE_LOG(LogTemp, Log, TEXT("RecoilConvergence: Delay complete, starting recovery. Pitch=%.3f, Yaw=%.3f"),
				AccumulatedPitch, AccumulatedYaw);
		}
		return;
	}

	// Apply convergence
	if (bIsConverging)
	{
		ApplyConvergenceRecovery(DeltaTime);
	}
}

void USuspenseCoreRecoilConvergenceComponent::ApplyRecoilImpulse(
	float PitchImpulse,
	float YawImpulse,
	float ConvergenceDelay,
	float ConvergenceSpeed,
	float Ergonomics)
{
	// Accumulate offset
	AccumulatedPitch += PitchImpulse;
	AccumulatedYaw += YawImpulse;

	// Update convergence parameters from weapon
	CurrentConvergenceDelay = ConvergenceDelay;
	CurrentConvergenceSpeed = ConvergenceSpeed;
	CurrentErgonomics = Ergonomics;

	// Reset timing - start waiting for delay again
	TimeSinceLastImpulse = 0.0f;
	bWaitingForDelay = true;
	bIsConverging = false;

	// Enable tick
	SetComponentTickEnabled(true);

	UE_LOG(LogTemp, Verbose, TEXT("RecoilConvergence: Impulse applied. Pitch=%.3f, Yaw=%.3f, Total: Pitch=%.3f, Yaw=%.3f"),
		PitchImpulse, YawImpulse, AccumulatedPitch, AccumulatedYaw);
}

void USuspenseCoreRecoilConvergenceComponent::ResetConvergence()
{
	AccumulatedPitch = 0.0f;
	AccumulatedYaw = 0.0f;
	TimeSinceLastImpulse = 0.0f;
	bWaitingForDelay = false;
	bIsConverging = false;
	SetComponentTickEnabled(false);

	UE_LOG(LogTemp, Log, TEXT("RecoilConvergence: Reset"));
}

bool USuspenseCoreRecoilConvergenceComponent::HasOffset() const
{
	return !FMath::IsNearlyZero(AccumulatedPitch, 0.01f) ||
		   !FMath::IsNearlyZero(AccumulatedYaw, 0.01f);
}

float USuspenseCoreRecoilConvergenceComponent::GetEffectiveConvergenceSpeed() const
{
	// Ergonomics bonus: 42 ergo = 1.42x speed, 70 ergo = 1.70x speed
	return CurrentConvergenceSpeed * (1.0f + CurrentErgonomics / 100.0f);
}

void USuspenseCoreRecoilConvergenceComponent::ApplyConvergenceRecovery(float DeltaTime)
{
	APlayerController* PC = GetOwnerPlayerController();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("RecoilConvergence: No PlayerController"));
		return;
	}

	float EffectiveSpeed = GetEffectiveConvergenceSpeed();
	float ConvergenceRate = EffectiveSpeed * DeltaTime;

	float PitchRecovery = 0.0f;
	float YawRecovery = 0.0f;

	// Calculate pitch recovery
	if (FMath::Abs(AccumulatedPitch) > 0.01f)
	{
		PitchRecovery = -FMath::Sign(AccumulatedPitch) *
			FMath::Min(ConvergenceRate, FMath::Abs(AccumulatedPitch));
	}

	// Calculate yaw recovery
	if (FMath::Abs(AccumulatedYaw) > 0.01f)
	{
		YawRecovery = -FMath::Sign(AccumulatedYaw) *
			FMath::Min(ConvergenceRate, FMath::Abs(AccumulatedYaw));
	}

	// Apply recovery to camera
	// AccumulatedPitch is positive when camera kicked UP
	// To return DOWN, we need AddPitchInput with positive value
	// PitchRecovery is negative (to reduce AccumulatedPitch toward 0)
	// So we negate it: -PitchRecovery = positive = camera goes DOWN
	if (!FMath::IsNearlyZero(PitchRecovery) || !FMath::IsNearlyZero(YawRecovery))
	{
		PC->AddPitchInput(-PitchRecovery);
		PC->AddYawInput(YawRecovery);

		// Update accumulated offset
		AccumulatedPitch += PitchRecovery;
		AccumulatedYaw += YawRecovery;

		UE_LOG(LogTemp, Verbose, TEXT("RecoilConvergence: Recovery applied. PitchRecovery=%.4f, Remaining: Pitch=%.3f, Yaw=%.3f"),
			PitchRecovery, AccumulatedPitch, AccumulatedYaw);
	}

	// Snap to zero if very small
	if (FMath::IsNearlyZero(AccumulatedPitch, 0.01f))
	{
		AccumulatedPitch = 0.0f;
	}
	if (FMath::IsNearlyZero(AccumulatedYaw, 0.01f))
	{
		AccumulatedYaw = 0.0f;
	}

	// Check if convergence complete
	if (!HasOffset())
	{
		bIsConverging = false;
		SetComponentTickEnabled(false);
		UE_LOG(LogTemp, Log, TEXT("RecoilConvergence: Complete"));
	}
}

APlayerController* USuspenseCoreRecoilConvergenceComponent::GetOwnerPlayerController() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	// Try to get controller from Pawn
	if (APawn* Pawn = Cast<APawn>(Owner))
	{
		return Cast<APlayerController>(Pawn->GetController());
	}

	// Try to get controller from PlayerController directly (if attached to controller)
	if (APlayerController* PC = Cast<APlayerController>(Owner))
	{
		return PC;
	}

	return nullptr;
}
