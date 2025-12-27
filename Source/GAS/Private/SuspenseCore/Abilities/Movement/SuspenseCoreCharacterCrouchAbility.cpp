// SuspenseCoreCharacterCrouchAbility.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Abilities/Movement/SuspenseCoreCharacterCrouchAbility.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/SuspenseCoreInterfaces.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

USuspenseCoreCharacterCrouchAbility::USuspenseCoreCharacterCrouchAbility()
{
	CrouchSpeedMultiplier = 0.5f;
	CrouchStartSound = nullptr;
	CrouchEndSound = nullptr;
	bToggleMode = false;
	CurrentActorInfo = nullptr;

	// Configure ability
	AbilityInputID = ESuspenseCoreAbilityInputID::Crouch;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bRetriggerInstancedAbility = true; // Allow toggle behavior

	// AbilityTags (AssetTags) - used by TryActivateAbilitiesByTag to find matching abilities
	// Using Native Tags for compile-time safety
	FGameplayTagContainer AbilityTagContainer;
	AbilityTagContainer.AddTag(SuspenseCoreTags::Ability::Crouch);
	AbilityTagContainer.AddTag(SuspenseCoreTags::Ability::Movement::Crouch);
	SetAssetTags(AbilityTagContainer);

	// Applied tag while crouching
	ActivationOwnedTags.AddTag(SuspenseCoreTags::State::Crouching);

	// Block tags - can't crouch in these states
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Dead);
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Stunned);
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Sprinting);
}

//==================================================================
// GameplayAbility Interface
//==================================================================

bool USuspenseCoreCharacterCrouchAbility::CanActivateAbility(
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

	// Check character exists
	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (!Character)
	{
		return false;
	}

	// Check if movement component supports crouching
	UCharacterMovementComponent* CMC = Character->GetCharacterMovement();
	if (!CMC || !CMC->CanEverCrouch())
	{
		return false;
	}

	// Check if on ground
	if (CMC->IsFalling())
	{
		return false;
	}

	return true;
}

void USuspenseCoreCharacterCrouchAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Save parameters for callbacks
	CurrentSpecHandle = Handle;
	CurrentActorInfo = ActorInfo;
	CurrentActivationInfo = ActivationInfo;

	// Set crouch state
	SetCharacterCrouchState(ActorInfo, true);

	// Apply crouch effects
	ApplyCrouchEffects(ActorInfo);

	// Play sound
	PlayCrouchSound(true);

	// Call super to broadcast activation event
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Wait for input release if not in toggle mode
	if (!bToggleMode)
	{
		// bTestAlreadyReleased = false - don't check immediately, only fire on actual release event
		UAbilityTask_WaitInputRelease* WaitInputTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, false);
		if (WaitInputTask)
		{
			WaitInputTask->OnRelease.AddDynamic(this, &USuspenseCoreCharacterCrouchAbility::OnCrouchInputReleased);
			WaitInputTask->ReadyForActivation();
		}
	}

	LogAbilityDebug(TEXT("Crouch started"));
}

void USuspenseCoreCharacterCrouchAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// Remove crouch state
	SetCharacterCrouchState(ActorInfo, false);

	// Remove crouch effects
	RemoveCrouchEffects(ActorInfo);

	// Play sound
	PlayCrouchSound(false);

	// Clear saved state
	CurrentActorInfo = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	LogAbilityDebug(TEXT("Crouch ended"));
}

void USuspenseCoreCharacterCrouchAbility::InputReleased(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	// In hold mode, release ends crouch
	if (!bToggleMode)
	{
		LogAbilityDebug(TEXT("Crouch input released (hold mode)"));
		K2_EndAbility();
	}
}

void USuspenseCoreCharacterCrouchAbility::InputPressed(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	// In toggle mode, second press ends crouch
	if (bToggleMode && IsActive())
	{
		LogAbilityDebug(TEXT("Crouch toggled off"));
		K2_EndAbility();
	}
}

//==================================================================
// Internal Methods
//==================================================================

bool USuspenseCoreCharacterCrouchAbility::ApplyCrouchEffects(
	const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!CrouchDebuffEffectClass)
	{
		return true;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!ASC)
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	FGameplayEffectSpecHandle DebuffSpec = ASC->MakeOutgoingSpec(
		CrouchDebuffEffectClass,
		1.0f,
		EffectContext
	);

	if (DebuffSpec.IsValid())
	{
		// Set speed reduction via SetByCaller
		// CrouchSpeedMultiplier = 0.5 means 50% of normal speed
		// For MultiplyAdditive: value should be (multiplier - 1.0)
		// So 0.5 -> -0.5 (which gives -50% speed)
		const float SpeedReduction = CrouchSpeedMultiplier - 1.0f;
		DebuffSpec.Data->SetSetByCallerMagnitude(
			SuspenseCoreTags::Data::Cost::SpeedMultiplier,
			SpeedReduction
		);

		CrouchDebuffEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*DebuffSpec.Data.Get());
		return CrouchDebuffEffectHandle.IsValid();
	}

	return false;
}

void USuspenseCoreCharacterCrouchAbility::RemoveCrouchEffects(
	const FGameplayAbilityActorInfo* ActorInfo)
{
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC)
	{
		return;
	}

	if (CrouchDebuffEffectHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(CrouchDebuffEffectHandle);
		CrouchDebuffEffectHandle.Invalidate();
	}
}

void USuspenseCoreCharacterCrouchAbility::SetCharacterCrouchState(
	const FGameplayAbilityActorInfo* ActorInfo,
	bool bCrouch)
{
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!Avatar)
	{
		return;
	}

	// Use interface to communicate with Character without circular dependency
	if (Avatar->GetClass()->ImplementsInterface(USuspenseCoreMovementInterface::StaticClass()))
	{
		if (bCrouch)
		{
			ISuspenseCoreMovementInterface::Execute_MovementStartCrouch(Avatar);
		}
		else
		{
			ISuspenseCoreMovementInterface::Execute_MovementStopCrouch(Avatar);
		}
	}
}

void USuspenseCoreCharacterCrouchAbility::PlayCrouchSound(bool bCrouchStart)
{
	USoundBase* SoundToPlay = bCrouchStart ? CrouchStartSound : CrouchEndSound;
	if (!SoundToPlay)
	{
		return;
	}

	ACharacter* Character = GetOwningCharacter();
	if (Character)
	{
		UGameplayStatics::PlaySoundAtLocation(
			Character->GetWorld(),
			SoundToPlay,
			Character->GetActorLocation(),
			1.0f,
			1.0f,
			0.0f
		);
	}
}

void USuspenseCoreCharacterCrouchAbility::OnCrouchInputReleased(float TimeHeld)
{
	LogAbilityDebug(FString::Printf(TEXT("Crouch released after %.2f seconds"), TimeHeld));
	K2_EndAbility();
}
