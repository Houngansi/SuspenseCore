// SuspenseCoreAutoFireAbility.h
// SuspenseCore - Automatic Fire Ability
// Copyright Suspense Team. All Rights Reserved.
//
// Continuous fire while trigger is held.
// Used for fully automatic weapons.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Abilities/Base/SuspenseCoreBaseFireAbility.h"
#include "SuspenseCoreAutoFireAbility.generated.h"

/**
 * USuspenseCoreAutoFireAbility
 *
 * Automatic fire mode. Fires continuously while input is held.
 * Uses timer-based shot scheduling at weapon fire rate.
 *
 * Behavior:
 * 1. ActivateAbility() starts auto fire timer
 * 2. Timer fires shots at FireRate interval
 * 3. InputReleased() stops timer and ends ability
 * 4. Out of ammo stops auto fire
 */
UCLASS()
class GAS_API USuspenseCoreAutoFireAbility : public USuspenseCoreBaseFireAbility
{
	GENERATED_BODY()

public:
	USuspenseCoreAutoFireAbility();

	/** Default fire rate (shots per second) if no weapon attributes */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|AutoFire")
	float DefaultFireRate;

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
	// Auto Fire Control
	//========================================================================

	/** Start the auto fire timer */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|AutoFire")
	void StartAutoFire();

	/** Stop the auto fire timer */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|AutoFire")
	void StopAutoFire();

	/** Execute one shot in auto fire sequence */
	void ExecuteAutoShot();

	/** Get fire interval from weapon or defaults */
	float GetFireInterval() const;

private:
	/** Timer handle for auto fire */
	FTimerHandle AutoFireTimerHandle;

	/** Is auto fire currently active */
	bool bIsAutoFireActive;

	/** Time when auto fire started */
	float AutoFireStartTime;
};
