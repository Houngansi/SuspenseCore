// SuspenseCoreCharacterSprintAbility.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Abilities/Base/SuspenseCoreAbility.h"
#include "GameplayEffectTypes.h"
#include "SuspenseCoreCharacterSprintAbility.generated.h"

class UGameplayEffect;
class UAbilityTask_WaitInputRelease;

/**
 * USuspenseCoreCharacterSprintAbility
 *
 * Sprint ability with hold-to-sprint behavior and EventBus integration.
 * Applies speed buff and drains stamina while active.
 *
 * FEATURES:
 * - Hold-to-sprint activation model
 * - Speed increase via GameplayEffect
 * - Continuous stamina drain
 * - Automatic end on stamina depletion
 * - EventBus events for sprint lifecycle
 *
 * EVENT TAGS:
 * - SuspenseCore.Event.Ability.CharacterSprint.Activated
 * - SuspenseCore.Event.Ability.CharacterSprint.Ended
 *
 * @see USuspenseCoreAbility
 */
UCLASS()
class GAS_API USuspenseCoreCharacterSprintAbility : public USuspenseCoreAbility
{
	GENERATED_BODY()

public:
	USuspenseCoreCharacterSprintAbility();

	//==================================================================
	// Sprint Configuration
	//==================================================================

	/** Effect for speed boost during sprint */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Sprint|Effects")
	TSubclassOf<UGameplayEffect> SprintBuffEffectClass;

	/** Effect for stamina cost during sprint (periodic drain) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Sprint|Effects")
	TSubclassOf<UGameplayEffect> SprintCostEffectClass;

	/** Speed multiplier while sprinting (for UI display, actual value in effect) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Sprint|Speed",
		meta = (ClampMin = "1.1", ClampMax = "3.0"))
	float SprintSpeedMultiplier;

	/** Stamina cost per second (for validation, actual drain in effect) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Sprint|Stamina",
		meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float StaminaCostPerSecond;

	/** Minimum stamina required to start sprinting */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Sprint|Stamina",
		meta = (ClampMin = "0.0"))
	float MinimumStaminaToSprint;

	/** Stamina threshold at which sprint automatically ends */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Sprint|Stamina",
		meta = (ClampMin = "0.0"))
	float StaminaExhaustionThreshold;

protected:
	//==================================================================
	// GameplayAbility Interface
	//==================================================================

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr
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

	/** Apply sprint effects to character */
	bool ApplySprintEffects(const FGameplayAbilityActorInfo* ActorInfo);

	/** Remove sprint effects from character */
	void RemoveSprintEffects(const FGameplayAbilityActorInfo* ActorInfo);

	/** Check stamina and end if depleted */
	void CheckStaminaDepletion();

	/** Called when sprint input is released */
	UFUNCTION()
	void OnSprintInputReleased(float TimeHeld);

	/** Called when stamina drops below threshold */
	UFUNCTION()
	void OnStaminaBelowThreshold(bool bMatched, float CurrentValue);

	//==================================================================
	// Internal State
	//==================================================================

	/** Handle for active speed buff effect */
	FActiveGameplayEffectHandle SprintBuffEffectHandle;

	/** Handle for active stamina cost effect */
	FActiveGameplayEffectHandle SprintCostEffectHandle;

	/** Saved activation parameters for callbacks */
	FGameplayAbilitySpecHandle CurrentSpecHandle;
	const FGameplayAbilityActorInfo* CurrentActorInfo;
	FGameplayAbilityActivationInfo CurrentActivationInfo;

	/** Timer for stamina check */
	FTimerHandle StaminaCheckTimer;
};
