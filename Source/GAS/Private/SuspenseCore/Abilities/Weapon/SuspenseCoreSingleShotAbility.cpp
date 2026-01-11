// SuspenseCoreSingleShotAbility.cpp
// SuspenseCore - Single Shot Fire Ability Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Abilities/Weapon/SuspenseCoreSingleShotAbility.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"

USuspenseCoreSingleShotAbility::USuspenseCoreSingleShotAbility()
{
	// TODO: Re-enable once weapon properly adds fire mode tags to ASC on equip
	// For now, commented out to allow testing activation
	//
	// Require single/semi fire mode
	// ActivationRequiredTags.AddTag(SuspenseCoreTags::Weapon::FireMode::Single);

	// Block if in burst or auto mode
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::BurstActive);
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::AutoFireActive);
}

void USuspenseCoreSingleShotAbility::FireNextShot_Implementation()
{
	UE_LOG(LogTemp, Warning, TEXT("[SINGLESHOT] FireNextShot_Implementation called!"));

	// Execute single shot
	ExecuteSingleShot();
	UE_LOG(LogTemp, Warning, TEXT("[SINGLESHOT] ExecuteSingleShot completed"));

	// Apply cooldown based on fire rate
	CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, nullptr);
	UE_LOG(LogTemp, Warning, TEXT("[SINGLESHOT] Cooldown committed"));

	// End ability immediately
	UE_LOG(LogTemp, Warning, TEXT("[SINGLESHOT] Calling EndAbility..."));
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
