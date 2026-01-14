// SuspenseCoreRecoilConvergenceComponent.h
// SuspenseCore - Recoil Convergence Component
// Copyright Suspense Team. All Rights Reserved.
//
// Component responsible for camera convergence (return to aim point) after recoil.
// Lives on Character, independent of ability lifecycle.
//
// ARCHITECTURE (AAA Pattern):
// - Fire ability PUBLISHES Event::Weapon::RecoilImpulse via EventBus
// - Component SUBSCRIBES to event and handles convergence
// - Decoupled: No direct call between GAS and PlayerCore modules
// - Uses TickComponent (proper UE way) instead of timers
//
// Usage:
//   1. Add component to Character via CreateDefaultSubobject
//   2. Component auto-subscribes to EventBus in BeginPlay
//   3. Fire ability publishes recoil event
//   4. Component handles convergence automatically

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCoreRecoilConvergenceComponent.generated.h"

// Forward declarations
class USuspenseCoreEventBus;

/**
 * USuspenseCoreRecoilConvergenceComponent
 *
 * Handles camera convergence (return to aim point) after weapon recoil.
 * Decoupled from ability lifecycle - continues working after ability ends.
 *
 * EVENT-DRIVEN ARCHITECTURE:
 * - Subscribes to SuspenseCoreTags::Event::Weapon::RecoilImpulse
 * - Receives: PitchImpulse, YawImpulse, ConvergenceDelay, ConvergenceSpeed, Ergonomics
 * - No direct dependency on GAS module
 *
 * FLOW:
 * 1. Fire ability publishes RecoilImpulse event via EventBus
 * 2. Component receives event and stores accumulated offset
 * 3. TickComponent smoothly returns camera to center
 * 4. Auto-disables tick when no offset (performance)
 */
UCLASS(ClassGroup = (SuspenseCore), meta = (BlueprintSpawnableComponent))
class PLAYERCORE_API USuspenseCoreRecoilConvergenceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USuspenseCoreRecoilConvergenceComponent();

	//========================================================================
	// Public API
	//========================================================================

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	//========================================================================
	// EventBus Subscription
	//========================================================================

	/** Subscribe to recoil events from EventBus */
	void SubscribeToEvents();

	/** Unsubscribe from EventBus */
	void UnsubscribeFromEvents();

	/** Handle recoil impulse event from fire ability (native callback) */
	void OnRecoilImpulseEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Cached EventBus reference */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	/** Subscription handle for cleanup */
	FSuspenseCoreSubscriptionHandle RecoilEventHandle;

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

	/** Get EventBus from world */
	USuspenseCoreEventBus* GetEventBus() const;
};
