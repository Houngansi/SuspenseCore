// SuspenseCoreBurstFireAbility.cpp
// SuspenseCore - Burst Fire Ability Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Abilities/Weapon/SuspenseCoreBurstFireAbility.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "TimerManager.h"

USuspenseCoreBurstFireAbility::USuspenseCoreBurstFireAbility()
	: BurstCount(3)
	, BurstDelay(0.15f)
	, CurrentBurstShotCount(0)
	, bIsBurstActive(false)
{
	// Require burst fire mode
	ActivationRequiredTags.AddTag(SuspenseCoreTags::Weapon::FireMode::Burst);

	// Block if in single or auto mode
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::AutoFireActive);

	// Add burst active tag when activated
	ActivationOwnedTags.AddTag(SuspenseCoreTags::State::BurstActive);
}

void USuspenseCoreBurstFireAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Reset burst state
	CurrentBurstShotCount = 0;
	bIsBurstActive = false;

	// Call base to set firing state and fire first shot
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void USuspenseCoreBurstFireAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// Clear burst timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BurstTimerHandle);
	}

	bIsBurstActive = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void USuspenseCoreBurstFireAbility::FireNextShot_Implementation()
{
	// Start burst sequence
	bIsBurstActive = true;
	CurrentBurstShotCount = 0;

	// Fire first shot immediately
	ExecuteBurstShot();
}

void USuspenseCoreBurstFireAbility::InputReleased(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	// Burst cannot be interrupted - do nothing
	// Ability will end when burst completes
}

void USuspenseCoreBurstFireAbility::ExecuteBurstShot()
{
	// Check if still active
	if (!bIsBurstActive || !IsActive())
	{
		CompleteBurst();
		return;
	}

	// Check ammo
	if (!HasAmmo())
	{
		CompleteBurst();
		return;
	}

	// Execute shot
	ExecuteSingleShot();
	CurrentBurstShotCount++;

	// Check if burst complete
	if (CurrentBurstShotCount >= BurstCount)
	{
		CompleteBurst();
		return;
	}

	// Schedule next shot in burst
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			BurstTimerHandle,
			this,
			&USuspenseCoreBurstFireAbility::ExecuteBurstShot,
			BurstDelay,
			false  // Not looping
		);
	}
}

void USuspenseCoreBurstFireAbility::CompleteBurst()
{
	bIsBurstActive = false;

	// Clear timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BurstTimerHandle);
	}

	// Apply cooldown
	CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, nullptr);

	// End ability
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
