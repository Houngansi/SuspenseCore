// SuspenseCoreSwitchFireModeAbility.h
// SuspenseCore - Fire Mode Switch Ability
// Copyright Suspense Team. All Rights Reserved.
//
// Cycles through available fire modes on current weapon.
// NOT a fire ability - just switches modes.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Abilities/Base/SuspenseCoreAbility.h"
#include "SuspenseCoreSwitchFireModeAbility.generated.h"

class ISuspenseCoreWeapon;
class ISuspenseCoreFireModeProvider;

/**
 * USuspenseCoreSwitchFireModeAbility
 *
 * Switches between available fire modes on the current weapon.
 * Does NOT extend BaseFireAbility - this is a utility ability.
 *
 * Behavior:
 * 1. Activates on input (default: B key)
 * 2. Calls weapon's CycleFireMode()
 * 3. Publishes FireModeChanged event
 * 4. Ends immediately
 *
 * Network: Server-only execution to prevent double-switching.
 */
UCLASS()
class GAS_API USuspenseCoreSwitchFireModeAbility : public USuspenseCoreAbility
{
	GENERATED_BODY()

public:
	USuspenseCoreSwitchFireModeAbility();

	/** Sound to play when switching modes */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Effects")
	TObjectPtr<USoundBase> SwitchSound;

	/** Montage to play when switching modes (optional) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Animation")
	TObjectPtr<UAnimMontage> SwitchMontage;

protected:
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

	//========================================================================
	// Fire Mode Switching
	//========================================================================

	/**
	 * Execute fire mode switch.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|FireMode")
	void SwitchFireMode();

	/**
	 * Get weapon interface.
	 */
	ISuspenseCoreWeapon* GetWeaponInterface() const;

	/**
	 * Get fire mode provider interface.
	 */
	ISuspenseCoreFireModeProvider* GetFireModeProvider() const;

	/**
	 * Publish fire mode changed event.
	 */
	void PublishFireModeChangedEvent(const FGameplayTag& NewFireMode);

	/**
	 * Play switch effects (sound, animation).
	 */
	void PlaySwitchEffects();
};
