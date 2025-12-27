// SuspenseCoreAimDownSightAbility.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Abilities/Base/SuspenseCoreAbility.h"
#include "SuspenseCoreAimDownSightAbility.generated.h"

class ISuspenseCoreWeaponCombatState;
class UCameraComponent;

/**
 * USuspenseCoreAimDownSightAbility
 *
 * Aim Down Sight (ADS) ability for weapon combat.
 *
 * BEHAVIOR:
 * - Hold-to-aim model (RMB pressed = aiming, released = stop aiming)
 * - Sets aiming state on WeaponStanceComponent (SSOT)
 * - Applies speed reduction GameplayEffect
 * - Publishes camera FOV change events for camera system
 *
 * INTEGRATION:
 * - ISuspenseCoreWeaponCombatState::SetAiming() handles:
 *   - bIsAiming state (replicated)
 *   - TargetAimPoseAlpha interpolation
 *   - EventBus publishing (AimStarted/AimEnded)
 *
 * - AnimInstance automatically receives:
 *   - bIsAiming from StanceSnapshot
 *   - AimPoseAlpha (interpolated) from StanceSnapshot
 *   - Blends LHGripTransform[0] ↔ [1] based on AimingAlpha
 *
 * BLOCKING TAGS:
 * - State.Sprinting (can't aim while sprinting)
 * - State.Reloading (can't aim while reloading)
 * - State.Dead, State.Stunned, State.Disabled
 *
 * CANCELLATION:
 * - Cancels SuspenseCore.Ability.Sprint when activated
 * - Cancelled by State.Sprinting, State.Reloading
 *
 * @see ISuspenseCoreWeaponCombatState
 * @see USuspenseCoreCharacterAnimInstance
 */
UCLASS()
class GAS_API USuspenseCoreAimDownSightAbility : public USuspenseCoreAbility
{
	GENERATED_BODY()

public:
	USuspenseCoreAimDownSightAbility();

	//==================================================================
	// Configuration
	//==================================================================

	/** GameplayEffect for movement speed reduction while aiming */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|ADS|Effects")
	TSubclassOf<UGameplayEffect> AimSpeedDebuffClass;

	/** Target FOV when aiming (published via EventBus for camera system) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|ADS|Camera")
	float AimFOV;

	/** Default FOV to restore when exiting ADS */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|ADS|Camera")
	float DefaultFOV;

	/** Speed of camera FOV transition */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|ADS|Camera")
	float FOVTransitionSpeed;

	/** Whether to publish camera FOV events (disable if camera handles itself) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|ADS|Camera")
	bool bPublishCameraEvents;

protected:
	//==================================================================
	// GameplayAbility Interface
	//==================================================================

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags
	) const override;

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

	virtual void InputReleased(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo
	) override;

private:
	//==================================================================
	// Internal Methods
	//==================================================================

	/** Get WeaponCombatState interface from owner character */
	ISuspenseCoreWeaponCombatState* GetWeaponCombatState() const;

	/** Apply speed debuff effect */
	void ApplyAimEffects(const FGameplayAbilityActorInfo* ActorInfo);

	/** Remove speed debuff effect */
	void RemoveAimEffects(const FGameplayAbilityActorInfo* ActorInfo);

	/** Publish camera FOV change event through EventBus */
	void NotifyCameraFOVChange(bool bAiming);

	/** Callback for WaitInputRelease task */
	UFUNCTION()
	void OnAimInputReleased(float TimeHeld);

	//==================================================================
	// Runtime State
	//==================================================================

	/** Handle for applied speed debuff effect */
	FActiveGameplayEffectHandle AimSpeedEffectHandle;

	/** Cached ActorInfo for EndAbility access */
	const FGameplayAbilityActorInfo* CachedActorInfo;

	/** Cached ability handle */
	FGameplayAbilitySpecHandle CachedSpecHandle;

	/** Cached activation info */
	FGameplayAbilityActivationInfo CachedActivationInfo;
};
