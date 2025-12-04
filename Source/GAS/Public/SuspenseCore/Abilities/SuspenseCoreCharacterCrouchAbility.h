// SuspenseCoreCharacterCrouchAbility.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Abilities/SuspenseCoreAbility.h"
#include "GameplayEffectTypes.h"
#include "SuspenseCoreCharacterCrouchAbility.generated.h"

class UGameplayEffect;
class USoundBase;

/**
 * USuspenseCoreCharacterCrouchAbility
 *
 * Crouch ability with hold-to-crouch behavior and EventBus integration.
 * Applies movement debuff while crouching.
 *
 * FEATURES:
 * - Hold-to-crouch activation model
 * - Speed reduction via GameplayEffect
 * - Audio feedback for crouch state changes
 * - EventBus events for crouch lifecycle
 *
 * EVENT TAGS:
 * - SuspenseCore.Event.Ability.CharacterCrouch.Activated
 * - SuspenseCore.Event.Ability.CharacterCrouch.Ended
 *
 * @see USuspenseCoreAbility
 */
UCLASS()
class GAS_API USuspenseCoreCharacterCrouchAbility : public USuspenseCoreAbility
{
	GENERATED_BODY()

public:
	USuspenseCoreCharacterCrouchAbility();

	//==================================================================
	// Crouch Configuration
	//==================================================================

	/** Effect for crouch debuff (speed reduction + state tag) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Crouch|Effects")
	TSubclassOf<UGameplayEffect> CrouchDebuffEffectClass;

	/** Speed multiplier while crouching (for UI display, actual value in effect) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Crouch|Speed",
		meta = (ClampMin = "0.1", ClampMax = "0.9"))
	float CrouchSpeedMultiplier;

	/** Sound when starting crouch */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Crouch|Audio")
	USoundBase* CrouchStartSound;

	/** Sound when ending crouch */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Crouch|Audio")
	USoundBase* CrouchEndSound;

	/** Toggle mode - press once to crouch, press again to stand */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Crouch|Behavior")
	bool bToggleMode;

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

	virtual void InputPressed(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo
	) override;

private:
	//==================================================================
	// Internal Methods
	//==================================================================

	/** Apply crouch effects to character */
	bool ApplyCrouchEffects(const FGameplayAbilityActorInfo* ActorInfo);

	/** Remove crouch effects from character */
	void RemoveCrouchEffects(const FGameplayAbilityActorInfo* ActorInfo);

	/** Set character crouch state */
	void SetCharacterCrouchState(const FGameplayAbilityActorInfo* ActorInfo, bool bCrouch);

	/** Play crouch audio feedback */
	void PlayCrouchSound(bool bCrouchStart);

	/** Called when crouch input is released */
	UFUNCTION()
	void OnCrouchInputReleased(float TimeHeld);

	//==================================================================
	// Internal State
	//==================================================================

	/** Handle for active crouch debuff effect */
	FActiveGameplayEffectHandle CrouchDebuffEffectHandle;

	/** Saved activation parameters for callbacks */
	FGameplayAbilitySpecHandle CurrentSpecHandle;
	const FGameplayAbilityActorInfo* CurrentActorInfo;
	FGameplayAbilityActivationInfo CurrentActivationInfo;
};
