// SuspenseCoreBurstFireAbility.h
// SuspenseCore - Burst Fire Ability
// Copyright Suspense Team. All Rights Reserved.
//
// Fixed burst of shots (e.g., 3-round burst).
// Burst cannot be interrupted once started.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Abilities/Base/SuspenseCoreBaseFireAbility.h"
#include "SuspenseCoreBurstFireAbility.generated.h"

/**
 * USuspenseCoreBurstFireAbility
 *
 * Burst fire mode. Fires N shots with delay between each, then ends.
 * Burst cannot be interrupted once started.
 *
 * Behavior:
 * 1. ActivateAbility() starts burst sequence
 * 2. Fires BurstCount shots with BurstDelay between each
 * 3. Ends automatically after all shots fired
 * 4. Cannot be cancelled during burst
 */
UCLASS()
class GAS_API USuspenseCoreBurstFireAbility : public USuspenseCoreBaseFireAbility
{
	GENERATED_BODY()

public:
	USuspenseCoreBurstFireAbility();

	/** Number of shots per burst */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|BurstFire", meta = (ClampMin = "2", ClampMax = "10"))
	int32 BurstCount;

	/** Delay between burst shots (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|BurstFire", meta = (ClampMin = "0.01"))
	float BurstDelay;

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled
	) override;

	virtual void FireNextShot_Implementation() override;

	virtual void InputReleased(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo
	) override;

	//========================================================================
	// Burst Control
	//========================================================================

	/** Execute one shot in burst sequence */
	void ExecuteBurstShot();

	/** Complete the burst sequence */
	void CompleteBurst();

private:
	/** Current shot count in burst */
	int32 CurrentBurstShotCount;

	/** Timer for burst shots */
	FTimerHandle BurstTimerHandle;

	/** Is burst currently active */
	bool bIsBurstActive;
};
