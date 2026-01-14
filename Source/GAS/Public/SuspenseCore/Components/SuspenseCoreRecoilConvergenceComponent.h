// SuspenseCoreRecoilConvergenceComponent.h
// SuspenseCore - Recoil Convergence Component
// Copyright Suspense Team. All Rights Reserved.
//
// Component responsible for camera convergence (return to aim point) after recoil.
// Lives on Character, independent of ability lifecycle.
//
// ARCHITECTURE:
// - Fire ability applies recoil kick via ApplyRecoilImpulse()
// - Component ticks every frame and smoothly returns camera to center
// - Uses TickComponent (proper UE way) instead of timers
//
// Usage:
//   1. Add component to Character
//   2. Call ApplyRecoilImpulse() from fire ability
//   3. Component handles convergence automatically

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SuspenseCoreRecoilConvergenceComponent.generated.h"

/**
 * USuspenseCoreRecoilConvergenceComponent
 *
 * Handles camera convergence (return to aim point) after weapon recoil.
 * Decoupled from ability lifecycle - continues working after ability ends.
 *
 * FLOW:
 * 1. Fire ability calls ApplyRecoilImpulse() with kick values
 * 2. Component stores accumulated offset
 * 3. TickComponent smoothly returns camera to center
 * 4. Auto-disables tick when no offset (performance)
 */
UCLASS(ClassGroup = (SuspenseCore), meta = (BlueprintSpawnableComponent))
class GAS_API USuspenseCoreRecoilConvergenceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USuspenseCoreRecoilConvergenceComponent();

	//========================================================================
	// Public API - Called by Fire Abilities
	//========================================================================

	/**
	 * Apply recoil impulse to the component.
	 * Called by fire ability after applying camera kick.
	 *
	 * @param PitchImpulse Vertical recoil in degrees (positive = kicked up)
	 * @param YawImpulse Horizontal recoil in degrees (positive = kicked right)
	 * @param ConvergenceDelay Delay before convergence starts (seconds)
	 * @param ConvergenceSpeed Speed of convergence (degrees/second)
	 * @param Ergonomics Weapon ergonomics (affects convergence speed)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Recoil")
	void ApplyRecoilImpulse(
		float PitchImpulse,
		float YawImpulse,
		float ConvergenceDelay = 0.1f,
		float ConvergenceSpeed = 5.0f,
		float Ergonomics = 42.0f
	);

	/**
	 * Force reset convergence state.
	 * Call on weapon switch, death, etc.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Recoil")
	void ResetConvergence();

	/**
	 * Check if convergence is currently active.
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Recoil")
	bool IsConverging() const { return bIsConverging; }

	/**
	 * Get current accumulated offset.
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Recoil")
	FVector2D GetCurrentOffset() const { return FVector2D(AccumulatedPitch, AccumulatedYaw); }

protected:
	//========================================================================
	// UActorComponent Interface
	//========================================================================

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	//========================================================================
	// Convergence State
	//========================================================================

	/** Accumulated pitch offset (positive = camera kicked up) */
	UPROPERTY(VisibleAnywhere, Category = "SuspenseCore|Debug")
	float AccumulatedPitch;

	/** Accumulated yaw offset (positive = camera kicked right) */
	UPROPERTY(VisibleAnywhere, Category = "SuspenseCore|Debug")
	float AccumulatedYaw;

	/** Time since last recoil impulse */
	float TimeSinceLastImpulse;

	/** Current convergence delay (from weapon) */
	float CurrentConvergenceDelay;

	/** Current convergence speed (from weapon) */
	float CurrentConvergenceSpeed;

	/** Current ergonomics (affects convergence speed) */
	float CurrentErgonomics;

	/** Whether we're waiting for delay before convergence */
	bool bWaitingForDelay;

	/** Whether convergence is actively happening */
	bool bIsConverging;

	//========================================================================
	// Internal Methods
	//========================================================================

	/** Check if there's any offset to converge */
	bool HasOffset() const;

	/** Get effective convergence speed with ergonomics */
	float GetEffectiveConvergenceSpeed() const;

	/** Apply convergence recovery to camera */
	void ApplyConvergenceRecovery(float DeltaTime);

	/** Get owning player controller */
	APlayerController* GetOwnerPlayerController() const;
};
