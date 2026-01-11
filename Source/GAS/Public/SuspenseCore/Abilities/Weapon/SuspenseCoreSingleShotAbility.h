// SuspenseCoreSingleShotAbility.h
// SuspenseCore - Single Shot Fire Ability
// Copyright Suspense Team. All Rights Reserved.
//
// Single shot per trigger pull. Ends immediately after firing.
// Used for semi-automatic weapons.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Abilities/Base/SuspenseCoreBaseFireAbility.h"
#include "SuspenseCoreSingleShotAbility.generated.h"

/**
 * USuspenseCoreSingleShotAbility
 *
 * Single shot fire mode. Fires once per activation and ends immediately.
 * Requires "Weapon.FireMode.Single" or "Weapon.FireMode.Semi" tag.
 *
 * Behavior:
 * 1. ActivateAbility() calls FireNextShot()
 * 2. FireNextShot() executes single shot and calls EndAbility()
 * 3. Next shot requires new input press
 */
UCLASS()
class GAS_API USuspenseCoreSingleShotAbility : public USuspenseCoreBaseFireAbility
{
	GENERATED_BODY()

public:
	USuspenseCoreSingleShotAbility();

protected:
	virtual void FireNextShot_Implementation() override;

	virtual void InputReleased(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo
	) override;
};
