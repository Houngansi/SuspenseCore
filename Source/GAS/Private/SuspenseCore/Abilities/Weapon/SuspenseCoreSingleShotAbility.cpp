// SuspenseCoreSingleShotAbility.cpp
// SuspenseCore - Single Shot Fire Ability Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Abilities/Weapon/SuspenseCoreSingleShotAbility.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"

USuspenseCoreSingleShotAbility::USuspenseCoreSingleShotAbility()
{
	// Require single/semi fire mode tag on ASC
	// Fire mode tags are added by:
	// 1. SuspenseCoreSwitchFireModeAbility::UpdateFireModeTagsOnASC() on mode switch
	// 2. Weapon initialization when equipped
	ActivationRequiredTags.AddTag(SuspenseCoreTags::Weapon::FireMode::Single);

	// Block if in burst or auto mode
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::BurstActive);
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::AutoFireActive);
}

void USuspenseCoreSingleShotAbility::FireNextShot_Implementation()
{
	// Execute single shot
	ExecuteSingleShot();

	// Apply cooldown based on fire rate (from weapon attributes)
	CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, nullptr);

	// Single shot ends immediately after firing
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void USuspenseCoreSingleShotAbility::InputReleased(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	// Single shot ends on shot, not on release
	// This override prevents premature ending
}
