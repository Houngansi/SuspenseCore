// SuspenseCoreCharacterJumpAbility.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Abilities/SuspenseCoreAbility.h"
#include "GameplayEffectTypes.h"
#include "SuspenseCoreCharacterJumpAbility.generated.h"

class UGameplayEffect;

/**
 * USuspenseCoreCharacterJumpAbility
 *
 * Reliable jump ability with EventBus integration.
 * Handles stamina cost and landing detection with safety timers.
 *
 * FEATURES:
 * - Variable jump height (hold to jump higher)
 * - Stamina cost on jump initiation
 * - Automatic landing detection
 * - Safety timeout to prevent stuck states
 * - EventBus events for jump lifecycle
 *
 * EVENT TAGS:
 * - SuspenseCore.Event.Ability.CharacterJump.Activated
 * - SuspenseCore.Event.Ability.CharacterJump.Ended
 * - SuspenseCore.Event.Ability.CharacterJump.Landed
 *
 * @see USuspenseCoreAbility
 */
UCLASS()
class GAS_API USuspenseCoreCharacterJumpAbility : public USuspenseCoreAbility
{
	GENERATED_BODY()

public:
	USuspenseCoreCharacterJumpAbility();

	//==================================================================
	// Jump Configuration
	//==================================================================

	/** Jump power multiplier (applied to character's JumpZVelocity) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Jump|Mechanics",
		meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float JumpPowerMultiplier;

	/** Stamina cost per jump */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Jump|Stamina",
		meta = (ClampMin = "0.0", ClampMax = "50.0"))
	float StaminaCostPerJump;

	/** Minimum stamina required to jump */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Jump|Stamina",
		meta = (ClampMin = "0.0"))
	float MinimumStaminaToJump;

	/** GameplayEffect class for one-time stamina cost */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Jump|Effects")
	TSubclassOf<UGameplayEffect> JumpStaminaCostEffectClass;

	/** Maximum jump duration before forced end (safety timeout) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Jump|Safety",
		meta = (ClampMin = "0.5", ClampMax = "10.0"))
	float MaxJumpDuration;

	/** Interval for ground check during jump */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Jump|Safety",
		meta = (ClampMin = "0.05", ClampMax = "0.5"))
	float GroundCheckInterval;

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

	/** Check if character is on ground */
	bool IsCharacterGrounded(const FGameplayAbilityActorInfo* ActorInfo) const;

	/** Apply stamina cost effect */
	bool ApplyStaminaCost(const FGameplayAbilityActorInfo* ActorInfo);

	/** Execute jump through character movement */
	void PerformJump(const FGameplayAbilityActorInfo* ActorInfo);

	/** Periodic check for landing */
	void CheckForLanding();

	/** Force end ability on timeout */
	void ForceEndAbility();

	/** Broadcast jump landed event */
	void BroadcastJumpLanded();

	//==================================================================
	// Internal State
	//==================================================================

	/** Timer for landing detection */
	FTimerHandle LandingCheckTimer;

	/** Safety timer for forced end */
	FTimerHandle SafetyTimer;

	/** Flag to prevent double ending */
	bool bIsEnding;
};
