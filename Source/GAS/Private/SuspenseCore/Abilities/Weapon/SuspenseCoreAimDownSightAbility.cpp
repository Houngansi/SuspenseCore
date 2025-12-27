// SuspenseCoreAimDownSightAbility.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Abilities/Weapon/SuspenseCoreAimDownSightAbility.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeaponCombatState.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "GameFramework/Character.h"
#include "Camera/CameraComponent.h"

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
	AbilityTags.AddTag(SuspenseCoreTags::Ability::Weapon::AimDownSight);

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
	AimFOV = 65.0f;
	DefaultFOV = 90.0f;
	FOVTransitionSpeed = 15.0f;
	bPublishCameraEvents = true;
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
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// Require weapon combat state interface (indicates weapon is equipped)
	ISuspenseCoreWeaponCombatState* CombatState = GetWeaponCombatState();
	if (!CombatState)
	{
		UE_LOG(LogSuspenseCoreADS, Verbose, TEXT("CanActivateAbility: No WeaponCombatState interface"));
		return false;
	}

	// Require weapon to be drawn
	if (!CombatState->IsWeaponDrawn())
	{
		UE_LOG(LogSuspenseCoreADS, Verbose, TEXT("CanActivateAbility: Weapon not drawn"));
		return false;
	}

	// Cannot aim while already reloading
	if (CombatState->IsReloading())
	{
		UE_LOG(LogSuspenseCoreADS, Verbose, TEXT("CanActivateAbility: Currently reloading"));
		return false;
	}

	return true;
}

void USuspenseCoreAimDownSightAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
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
		CombatState->SetAiming(true);
		UE_LOG(LogSuspenseCoreADS, Log, TEXT("ADS Activated - SetAiming(true)"));
	}
	else
	{
		UE_LOG(LogSuspenseCoreADS, Warning, TEXT("ADS Activated but no CombatState interface found"));
	}

	// ===================================================================
	// Apply Speed Debuff GameplayEffect
	// ===================================================================
	ApplyAimEffects(ActorInfo);

	// ===================================================================
	// Publish Camera FOV Change Event
	// Camera system subscribes to EventBus and handles FOV interpolation
	// ===================================================================
	NotifyCameraFOVChange(true);

	// ===================================================================
	// Wait for Input Release (Hold-to-Aim Model)
	// When player releases aim input, OnAimInputReleased is called
	// ===================================================================
	if (IsLocallyControlled())
	{
		UAbilityTask_WaitInputRelease* WaitInputTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
		if (WaitInputTask)
		{
			WaitInputTask->OnRelease.AddDynamic(this, &USuspenseCoreAimDownSightAbility::OnAimInputReleased);
			WaitInputTask->ReadyForActivation();
		}
	}
}

void USuspenseCoreAimDownSightAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
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
		CombatState->SetAiming(false);
		UE_LOG(LogSuspenseCoreADS, Log, TEXT("ADS Ended - SetAiming(false) (Cancelled: %s)"),
			bWasCancelled ? TEXT("true") : TEXT("false"));
	}

	// ===================================================================
	// Remove Speed Debuff GameplayEffect
	// ===================================================================
	RemoveAimEffects(ActorInfo);

	// ===================================================================
	// Publish Camera FOV Reset Event
	// ===================================================================
	NotifyCameraFOVChange(false);

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
		AimSpeedEffectHandle = ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, SpecHandle);

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

void USuspenseCoreAimDownSightAbility::NotifyCameraFOVChange(bool bAiming)
{
	if (!bPublishCameraEvents)
	{
		return;
	}

	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UObject* Source = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;

	// Create event data
	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(Source);

	// Set camera parameters
	EventData.SetFloat(FName("TargetFOV"), bAiming ? AimFOV : DefaultFOV);
	EventData.SetFloat(FName("TransitionSpeed"), FOVTransitionSpeed);
	EventData.SetBool(FName("IsAiming"), bAiming);

	// Publish camera FOV change event
	EventBus->Publish(SuspenseCoreTags::Event::Camera::FOVChanged, EventData);

	UE_LOG(LogSuspenseCoreADS, Verbose, TEXT("Published Camera FOV event: TargetFOV=%.1f, Aiming=%s"),
		bAiming ? AimFOV : DefaultFOV,
		bAiming ? TEXT("true") : TEXT("false"));
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
