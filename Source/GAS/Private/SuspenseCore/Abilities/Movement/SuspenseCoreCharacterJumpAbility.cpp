// SuspenseCoreCharacterJumpAbility.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Abilities/Movement/SuspenseCoreCharacterJumpAbility.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "SuspenseCore/Input/SuspenseCoreAbilityInputID.h"

USuspenseCoreCharacterJumpAbility::USuspenseCoreCharacterJumpAbility()
{
	JumpPowerMultiplier = 1.0f;
	StaminaCostPerJump = 10.0f;
	MinimumStaminaToJump = 5.0f;
	MaxJumpDuration = 3.0f;
	GroundCheckInterval = 0.1f;
	bIsEnding = false;

	// Configure ability
	AbilityInputID = ESuspenseCoreAbilityInputID::Jump;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// Allow activation while airborne for multi-jump
	bRetriggerInstancedAbility = false;

	// AbilityTags (AssetTags) - used by TryActivateAbilitiesByTag to find matching abilities
	// Using Native Tags for compile-time safety
	FGameplayTagContainer AbilityTagContainer;
	AbilityTagContainer.AddTag(SuspenseCoreTags::Ability::Jump);
	AbilityTagContainer.AddTag(SuspenseCoreTags::Ability::Movement::Jump);
	SetAssetTags(AbilityTagContainer);

	// Applied tag while jumping
	ActivationOwnedTags.AddTag(SuspenseCoreTags::State::Jumping);

	// Block tags - can't jump in these states
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Dead);
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Stunned);
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Disabled);
}

//==================================================================
// GameplayAbility Interface
//==================================================================

bool USuspenseCoreCharacterJumpAbility::CanActivateAbility(
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

	// Check if grounded (for initial jump)
	if (!IsCharacterGrounded(ActorInfo))
	{
		// Check if character can double jump via movement component
		UCharacterMovementComponent* CMC = Character->GetCharacterMovement();
		if (CMC && CMC->IsFalling())
		{
			// Could add double-jump logic here
			return false;
		}
	}

	// Check stamina via SuspenseCore AttributeSet
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (ASC && MinimumStaminaToJump > 0.0f)
	{
		const USuspenseCoreAttributeSet* Attributes = ASC->GetSet<USuspenseCoreAttributeSet>();
		if (Attributes)
		{
			float CurrentStamina = Attributes->GetStamina();
			if (CurrentStamina < MinimumStaminaToJump)
			{
				return false;
			}
		}
	}

	return true;
}

void USuspenseCoreCharacterJumpAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	bIsEnding = false;

	// Apply stamina cost
	if (!ApplyStaminaCost(ActorInfo))
	{
		LogAbilityDebug(TEXT("Failed to apply stamina cost, ending ability"), true);
		K2_EndAbility();
		return;
	}

	// Perform the jump
	PerformJump(ActorInfo);

	// Call super to broadcast activation event
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Start landing check timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			LandingCheckTimer,
			this,
			&USuspenseCoreCharacterJumpAbility::CheckForLanding,
			GroundCheckInterval,
			true,
			GroundCheckInterval
		);

		// Safety timer
		World->GetTimerManager().SetTimer(
			SafetyTimer,
			this,
			&USuspenseCoreCharacterJumpAbility::ForceEndAbility,
			MaxJumpDuration,
			false
		);
	}

	LogAbilityDebug(TEXT("Jump initiated"));
}

void USuspenseCoreCharacterJumpAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (bIsEnding)
	{
		return;
	}
	bIsEnding = true;

	// Clear timers
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LandingCheckTimer);
		World->GetTimerManager().ClearTimer(SafetyTimer);
	}

	// Stop jump if still in air
	if (ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get()))
	{
		Character->StopJumping();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void USuspenseCoreCharacterJumpAbility::InputReleased(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	// Stop jump to allow variable height
	if (ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get()))
	{
		Character->StopJumping();
	}

	LogAbilityDebug(TEXT("Jump input released"));
}

//==================================================================
// Internal Methods
//==================================================================

bool USuspenseCoreCharacterJumpAbility::IsCharacterGrounded(
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		return false;
	}

	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (!Character)
	{
		return false;
	}

	UCharacterMovementComponent* CMC = Character->GetCharacterMovement();
	return CMC && !CMC->IsFalling();
}

bool USuspenseCoreCharacterJumpAbility::ApplyStaminaCost(
	const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!JumpStaminaCostEffectClass)
	{
		// No cost effect configured, allow jump
		return true;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!ASC)
	{
		return true;
	}

	// Create effect context
	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	// Create effect spec
	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(
		JumpStaminaCostEffectClass,
		1.0f,
		EffectContext
	);

	if (!SpecHandle.IsValid())
	{
		LogAbilityDebug(TEXT("Failed to create stamina cost effect spec"), true);
		return false;
	}

	// Apply effect
	FActiveGameplayEffectHandle EffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

	return EffectHandle.IsValid() || !JumpStaminaCostEffectClass;
}

void USuspenseCoreCharacterJumpAbility::PerformJump(
	const FGameplayAbilityActorInfo* ActorInfo)
{
	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (!Character)
	{
		return;
	}

	// Apply jump power multiplier if needed
	if (FMath::Abs(JumpPowerMultiplier - 1.0f) > KINDA_SMALL_NUMBER)
	{
		UCharacterMovementComponent* CMC = Character->GetCharacterMovement();
		if (CMC)
		{
			// Store original value and modify temporarily
			// Note: In production, use a gameplay effect for this
			float OriginalJumpZ = CMC->JumpZVelocity;
			CMC->JumpZVelocity *= JumpPowerMultiplier;
			Character->Jump();
			CMC->JumpZVelocity = OriginalJumpZ;
		}
	}
	else
	{
		Character->Jump();
	}
}

void USuspenseCoreCharacterJumpAbility::CheckForLanding()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActorInfo)
	{
		ForceEndAbility();
		return;
	}

	if (IsCharacterGrounded(ActorInfo))
	{
		LogAbilityDebug(TEXT("Character landed"));
		BroadcastJumpLanded();
		K2_EndAbility();
	}
}

void USuspenseCoreCharacterJumpAbility::ForceEndAbility()
{
	LogAbilityDebug(TEXT("Force ending jump ability (safety timeout)"), true);
	K2_EndAbility();
}

void USuspenseCoreCharacterJumpAbility::BroadcastJumpLanded()
{
	if (!bPublishAbilityEvents)
	{
		return;
	}

	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(
		ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr,
		ESuspenseCoreEventPriority::Normal
	);

	// Using Native Tag for compile-time safety
	EventBus->Publish(SuspenseCoreTags::Event::Ability::CharacterJump::Landed, EventData);
}
