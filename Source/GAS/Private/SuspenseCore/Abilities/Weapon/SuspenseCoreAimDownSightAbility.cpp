// SuspenseCoreAimDownSightAbility.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Abilities/Weapon/SuspenseCoreAimDownSightAbility.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeaponCombatState.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Characters/SuspenseCoreCharacter.h"
#include "SuspenseCore/Base/SuspenseCoreWeaponActor.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "GameFramework/Character.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreADS, Log, All);

USuspenseCoreAimDownSightAbility::USuspenseCoreAimDownSightAbility()
{
	// ===================================================================
	// Input Binding
	// ===================================================================
	AbilityInputID = ESuspenseCoreAbilityInputID::Aim;

	// ===================================================================
	// Ability Tags Configuration
	// ===================================================================

	// This ability's identification tag
	// Note: AbilityTags is deprecated for direct modification in UE 5.5+
	// Using pragma to suppress warning until migration to Blueprint configuration
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags.AddTag(SuspenseCoreTags::Ability::Weapon::AimDownSight);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Tags added to owner when ability is active
	ActivationOwnedTags.AddTag(SuspenseCoreTags::State::Aiming);

	// Tags that block this ability from activating
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Sprinting);
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Reloading);
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Dead);
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Stunned);
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Disabled);

	// Tags that cancel this ability when applied
	CancelAbilitiesWithTag.AddTag(SuspenseCoreTags::Ability::Sprint);

	// ===================================================================
	// Network Configuration
	// ===================================================================
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bRetriggerInstancedAbility = false;

	// ===================================================================
	// Default Values
	// ===================================================================
	bPublishAbilityEvents = true;

	// Cached state
	CachedActorInfo = nullptr;
}

bool USuspenseCoreAimDownSightAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] CanActivateAbility called"));

	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] CanActivateAbility: Super returned FALSE"));
		return false;
	}

	// Require weapon combat state interface (indicates weapon is equipped)
	ISuspenseCoreWeaponCombatState* CombatState = GetWeaponCombatState();
	if (!CombatState)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] CanActivateAbility: No WeaponCombatState interface - FAILED"));
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] CanActivateAbility: CombatState found, IsWeaponDrawn=%s, IsReloading=%s"),
		CombatState->IsWeaponDrawn() ? TEXT("TRUE") : TEXT("FALSE"),
		CombatState->IsReloading() ? TEXT("TRUE") : TEXT("FALSE"));

	// Require weapon to be drawn
	if (!CombatState->IsWeaponDrawn())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] CanActivateAbility: Weapon not drawn - FAILED"));
		return false;
	}

	// Cannot aim while already reloading
	if (CombatState->IsReloading())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] CanActivateAbility: Currently reloading - FAILED"));
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] CanActivateAbility: All checks passed - SUCCESS"));
	return true;
}

void USuspenseCoreAimDownSightAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] ═══════════════════════════════════════════════════════"));
	UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] ActivateAbility CALLED - ADS ability activating!"));
	UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] AvatarActor: %s"),
		ActorInfo && ActorInfo->AvatarActor.IsValid() ? *ActorInfo->AvatarActor->GetName() : TEXT("NULL"));

	// Cache for EndAbility access
	CachedActorInfo = ActorInfo;
	CachedSpecHandle = Handle;
	CachedActivationInfo = ActivationInfo;

	// Call base implementation (broadcasts ability activated event)
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// ===================================================================
	// Set Aiming State on SSOT (via ISuspenseCoreWeaponCombatState)
	// This automatically:
	// - Sets bIsAiming = true (replicated)
	// - Sets TargetAimPoseAlpha = 1.0f (for AimPoseAlpha interpolation)
	// - Publishes EventBus: TAG_Equipment_Event_Weapon_Stance_AimStarted
	// - Forces network update
	// ===================================================================
	ISuspenseCoreWeaponCombatState* CombatState = GetWeaponCombatState();
	if (CombatState)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] CombatState found, calling SetAiming(true)"));
		CombatState->SetAiming(true);
		UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] SetAiming(true) completed. IsAiming now: %s"),
			CombatState->IsAiming() ? TEXT("TRUE") : TEXT("FALSE"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[ADS DEBUG] ERROR: CombatState is NULL! Cannot set aiming state!"));
	}

	// ===================================================================
	// Apply Speed Debuff GameplayEffect
	// ===================================================================
	ApplyAimEffects(ActorInfo);

	// ===================================================================
	// Switch to Scope Camera (Legacy Camera Blend Integration)
	// ===================================================================
	if (ASuspenseCoreCharacter* SuspenseCharacter = Cast<ASuspenseCoreCharacter>(ActorInfo->AvatarActor.Get()))
	{
		// Get weapon's camera config
		float TransitionDuration = 0.2f;
		float AimFOV = 60.0f;

		if (ASuspenseCoreWeaponActor* WeaponActor = Cast<ASuspenseCoreWeaponActor>(SuspenseCharacter->GetCurrentWeaponActor()))
		{
			TransitionDuration = WeaponActor->GetCameraTransitionDuration();
			AimFOV = WeaponActor->GetAimFOV();
		}

		SuspenseCharacter->SwitchToScopeCamera(true, TransitionDuration, AimFOV);
		UE_LOG(LogTemp, Log, TEXT("[ADS DEBUG] Camera switched to scope (FOV: %.1f, Duration: %.2fs)"), AimFOV, TransitionDuration);
	}

	// ===================================================================
	// Hold-to-Aim Model
	// ===================================================================
	// NOTE: WaitInputRelease doesn't work with TryActivateAbilitiesByTag
	// because input state is not passed through that activation path.
	// Instead, we rely on:
	// 1. InputReleased() callback from ASC when input is released
	// 2. External cancel via CancelAbilitiesByTag when bPressed=false
	// The ability stays active until one of these is triggered.
	// ===================================================================

	UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] ADS ability now ACTIVE - waiting for release/cancel"));
}

void USuspenseCoreAimDownSightAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] ═══════════════════════════════════════════════════════"));
	UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] EndAbility CALLED - ADS ability ending! (Cancelled: %s)"),
		bWasCancelled ? TEXT("TRUE") : TEXT("FALSE"));

	// ===================================================================
	// Clear Aiming State on SSOT
	// This automatically:
	// - Sets bIsAiming = false (replicated)
	// - Sets TargetAimPoseAlpha = 0.0f (for AimPoseAlpha interpolation)
	// - Publishes EventBus: TAG_Equipment_Event_Weapon_Stance_AimEnded
	// - Forces network update
	// ===================================================================
	ISuspenseCoreWeaponCombatState* CombatState = GetWeaponCombatState();
	if (CombatState)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] CombatState found, calling SetAiming(false)"));
		CombatState->SetAiming(false);
		UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] SetAiming(false) completed. IsAiming now: %s"),
			CombatState->IsAiming() ? TEXT("TRUE") : TEXT("FALSE"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[ADS DEBUG] ERROR: CombatState is NULL in EndAbility!"));
	}

	// ===================================================================
	// Remove Speed Debuff GameplayEffect
	// ===================================================================
	RemoveAimEffects(ActorInfo);

	// ===================================================================
	// Switch back to First Person Camera
	// ===================================================================
	if (ASuspenseCoreCharacter* SuspenseCharacter = Cast<ASuspenseCoreCharacter>(ActorInfo->AvatarActor.Get()))
	{
		// Get weapon's camera config for transition duration
		float TransitionDuration = 0.2f;

		if (ASuspenseCoreWeaponActor* WeaponActor = Cast<ASuspenseCoreWeaponActor>(SuspenseCharacter->GetCurrentWeaponActor()))
		{
			TransitionDuration = WeaponActor->GetCameraTransitionDuration();
		}

		SuspenseCharacter->SwitchToScopeCamera(false, TransitionDuration);
		UE_LOG(LogTemp, Log, TEXT("[ADS DEBUG] Camera switched back to first person (Duration: %.2fs)"), TransitionDuration);
	}

	// Clear cached state
	CachedActorInfo = nullptr;

	// Call base implementation (broadcasts ability ended event)
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void USuspenseCoreAimDownSightAbility::InputReleased(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	// For hold-to-aim, ending the ability when input is released
	// This is a fallback if WaitInputRelease task doesn't catch it
	if (IsActive())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

// =========================================================================
// Internal Methods
// =========================================================================

ISuspenseCoreWeaponCombatState* USuspenseCoreAimDownSightAbility::GetWeaponCombatState() const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		return nullptr;
	}

	// Find component that implements ISuspenseCoreWeaponCombatState
	AActor* AvatarActor = ActorInfo->AvatarActor.Get();

	// Check all components for interface implementation
	TArray<UActorComponent*> Components;
	AvatarActor->GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (ISuspenseCoreWeaponCombatState* CombatState = Cast<ISuspenseCoreWeaponCombatState>(Component))
		{
			return CombatState;
		}
	}

	return nullptr;
}

void USuspenseCoreAimDownSightAbility::ApplyAimEffects(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!AimSpeedDebuffClass)
	{
		return;
	}

	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		return;
	}

	// Create and apply the gameplay effect
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(AimSpeedDebuffClass, GetAbilityLevel());
	if (SpecHandle.IsValid())
	{
		// UE 5.5+ requires 4 arguments: Handle, ActorInfo, ActivationInfo, SpecHandle
		AimSpeedEffectHandle = ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle);

		if (AimSpeedEffectHandle.IsValid())
		{
			UE_LOG(LogSuspenseCoreADS, Verbose, TEXT("Applied AimSpeedDebuff effect"));
		}
	}
}

void USuspenseCoreAimDownSightAbility::RemoveAimEffects(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!AimSpeedEffectHandle.IsValid())
	{
		return;
	}

	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		return;
	}

	// Remove the applied gameplay effect
	ActorInfo->AbilitySystemComponent->RemoveActiveGameplayEffect(AimSpeedEffectHandle);
	AimSpeedEffectHandle.Invalidate();

	UE_LOG(LogSuspenseCoreADS, Verbose, TEXT("Removed AimSpeedDebuff effect"));
}

void USuspenseCoreAimDownSightAbility::OnAimInputReleased(float TimeHeld)
{
	// End the ability when aim input is released
	if (IsActive())
	{
		UE_LOG(LogSuspenseCoreADS, Log, TEXT("Aim input released after %.2f seconds"), TimeHeld);
		EndAbility(CachedSpecHandle, CachedActorInfo, CachedActivationInfo, true, false);
	}
}
