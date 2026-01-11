// SuspenseCoreAutoFireAbility.cpp
// SuspenseCore - Automatic Fire Ability Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Abilities/Weapon/SuspenseCoreAutoFireAbility.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "SuspenseCore/Attributes/SuspenseCoreWeaponAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "TimerManager.h"

USuspenseCoreAutoFireAbility::USuspenseCoreAutoFireAbility()
	: DefaultFireRate(10.0f)  // 10 shots per second = 600 RPM
	, bIsAutoFireActive(false)
	, AutoFireStartTime(0.0f)
{
	// Require auto fire mode
	ActivationRequiredTags.AddTag(SuspenseCoreTags::Weapon::FireMode::Auto);

	// Block if in single or burst mode
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::BurstActive);

	// Add auto fire active tag when activated
	ActivationOwnedTags.AddTag(SuspenseCoreTags::State::AutoFireActive);
}

void USuspenseCoreAutoFireAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Call base to set firing state
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Note: Base class calls FireNextShot() which starts auto fire
}

void USuspenseCoreAutoFireAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// Stop auto fire
	StopAutoFire();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void USuspenseCoreAutoFireAbility::FireNextShot_Implementation()
{
	// Start auto fire loop
	StartAutoFire();
}

void USuspenseCoreAutoFireAbility::InputReleased(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	// Stop firing on release
	StopAutoFire();
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void USuspenseCoreAutoFireAbility::StartAutoFire()
{
	if (bIsAutoFireActive)
	{
		return;
	}

	bIsAutoFireActive = true;
	AutoFireStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	// Fire first shot immediately
	ExecuteAutoShot();

	// Start timer for subsequent shots
	if (UWorld* World = GetWorld())
	{
		const float FireInterval = GetFireInterval();
		World->GetTimerManager().SetTimer(
			AutoFireTimerHandle,
			this,
			&USuspenseCoreAutoFireAbility::ExecuteAutoShot,
			FireInterval,
			true  // Looping
		);
	}
}

void USuspenseCoreAutoFireAbility::StopAutoFire()
{
	if (!bIsAutoFireActive)
	{
		return;
	}

	bIsAutoFireActive = false;

	// Clear timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoFireTimerHandle);
	}
}

void USuspenseCoreAutoFireAbility::ExecuteAutoShot()
{
	// Check if still active
	if (!bIsAutoFireActive || !IsActive())
	{
		StopAutoFire();
		return;
	}

	// Check ammo
	if (!HasAmmo())
	{
		StopAutoFire();
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// Execute shot using base class
	ExecuteSingleShot();
}

float USuspenseCoreAutoFireAbility::GetFireInterval() const
{
	// Try to get from weapon attributes
	if (const USuspenseCoreWeaponAttributeSet* Attrs = GetWeaponAttributes())
	{
		const float RateOfFire = Attrs->GetRateOfFire();
		if (RateOfFire > 0.0f)
		{
			// RateOfFire is rounds per minute, convert to interval
			return 60.0f / RateOfFire;
		}
	}

	// Use default
	if (DefaultFireRate > 0.0f)
	{
		return 1.0f / DefaultFireRate;
	}

	return 0.1f;  // 10 RPS fallback
}
