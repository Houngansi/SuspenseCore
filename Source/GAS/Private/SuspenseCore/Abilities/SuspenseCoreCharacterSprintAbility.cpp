// SuspenseCoreCharacterSprintAbility.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Abilities/SuspenseCoreCharacterSprintAbility.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

USuspenseCoreCharacterSprintAbility::USuspenseCoreCharacterSprintAbility()
{
	SprintSpeedMultiplier = 1.5f;
	StaminaCostPerSecond = 15.0f;
	MinimumStaminaToSprint = 10.0f;
	StaminaExhaustionThreshold = 1.0f;
	CurrentActorInfo = nullptr;

	// Configure ability
	AbilityInputID = ESuspenseAbilityInputID::Sprint;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bRetriggerInstancedAbility = false;

	// Ability tags
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Movement.Sprint")));

	// Applied tag while sprinting
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Sprinting")));

	// Block tags
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Dead")));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Stunned")));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Crouching")));
}

//==================================================================
// GameplayAbility Interface
//==================================================================

bool USuspenseCoreCharacterSprintAbility::CanActivateAbility(
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

	// Check if on ground (can't sprint in air)
	UCharacterMovementComponent* CMC = Character->GetCharacterMovement();
	if (!CMC || CMC->IsFalling())
	{
		return false;
	}

	// Check stamina
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (ASC && MinimumStaminaToSprint > 0.0f)
	{
		static FGameplayAttribute StaminaAttribute =
			UAbilitySystemGlobals::Get().GetGameplayAttributeFromName(TEXT("Stamina"));

		if (StaminaAttribute.IsValid())
		{
			float CurrentStamina = ASC->GetNumericAttribute(StaminaAttribute);
			if (CurrentStamina < MinimumStaminaToSprint)
			{
				return false;
			}
		}
	}

	return true;
}

void USuspenseCoreCharacterSprintAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Save parameters for callbacks
	CurrentSpecHandle = Handle;
	CurrentActorInfo = ActorInfo;
	CurrentActivationInfo = ActivationInfo;

	// Apply sprint effects
	if (!ApplySprintEffects(ActorInfo))
	{
		LogAbilityDebug(TEXT("Failed to apply sprint effects"), true);
		K2_EndAbility();
		return;
	}

	// Call super to broadcast activation event
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Start stamina check timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			StaminaCheckTimer,
			this,
			&USuspenseCoreCharacterSprintAbility::CheckStaminaDepletion,
			0.1f,
			true,
			0.1f
		);
	}

	// Wait for input release using ability task
	UAbilityTask_WaitInputRelease* WaitInputTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	if (WaitInputTask)
	{
		WaitInputTask->OnRelease.AddDynamic(this, &USuspenseCoreCharacterSprintAbility::OnSprintInputReleased);
		WaitInputTask->ReadyForActivation();
	}

	LogAbilityDebug(TEXT("Sprint started"));
}

void USuspenseCoreCharacterSprintAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// Clear timers
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StaminaCheckTimer);
	}

	// Remove sprint effects
	RemoveSprintEffects(ActorInfo);

	// Clear saved state
	CurrentActorInfo = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	LogAbilityDebug(TEXT("Sprint ended"));
}

void USuspenseCoreCharacterSprintAbility::InputReleased(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	LogAbilityDebug(TEXT("Sprint input released"));
	K2_EndAbility();
}

//==================================================================
// Internal Methods
//==================================================================

bool USuspenseCoreCharacterSprintAbility::ApplySprintEffects(
	const FGameplayAbilityActorInfo* ActorInfo)
{
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!ASC)
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	// Apply speed buff
	if (SprintBuffEffectClass)
	{
		FGameplayEffectSpecHandle BuffSpec = ASC->MakeOutgoingSpec(
			SprintBuffEffectClass,
			1.0f,
			EffectContext
		);

		if (BuffSpec.IsValid())
		{
			SprintBuffEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*BuffSpec.Data.Get());
		}
	}

	// Apply stamina cost
	if (SprintCostEffectClass)
	{
		FGameplayEffectSpecHandle CostSpec = ASC->MakeOutgoingSpec(
			SprintCostEffectClass,
			1.0f,
			EffectContext
		);

		if (CostSpec.IsValid())
		{
			SprintCostEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*CostSpec.Data.Get());
		}
	}

	return true;
}

void USuspenseCoreCharacterSprintAbility::RemoveSprintEffects(
	const FGameplayAbilityActorInfo* ActorInfo)
{
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC)
	{
		return;
	}

	// Remove speed buff
	if (SprintBuffEffectHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(SprintBuffEffectHandle);
		SprintBuffEffectHandle.Invalidate();
	}

	// Remove stamina cost
	if (SprintCostEffectHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(SprintCostEffectHandle);
		SprintCostEffectHandle.Invalidate();
	}
}

void USuspenseCoreCharacterSprintAbility::CheckStaminaDepletion()
{
	if (!CurrentActorInfo)
	{
		return;
	}

	UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	if (!ASC)
	{
		return;
	}

	// Check stamina
	static FGameplayAttribute StaminaAttribute =
		UAbilitySystemGlobals::Get().GetGameplayAttributeFromName(TEXT("Stamina"));

	if (StaminaAttribute.IsValid())
	{
		float CurrentStamina = ASC->GetNumericAttribute(StaminaAttribute);
		if (CurrentStamina <= StaminaExhaustionThreshold)
		{
			LogAbilityDebug(TEXT("Stamina depleted, ending sprint"));
			K2_EndAbility();
		}
	}
}

void USuspenseCoreCharacterSprintAbility::OnSprintInputReleased(float TimeHeld)
{
	LogAbilityDebug(FString::Printf(TEXT("Sprint released after %.2f seconds"), TimeHeld));
	K2_EndAbility();
}

void USuspenseCoreCharacterSprintAbility::OnStaminaBelowThreshold(bool bMatched, float CurrentValue)
{
	if (bMatched)
	{
		LogAbilityDebug(FString::Printf(TEXT("Stamina below threshold: %.1f"), CurrentValue));
		K2_EndAbility();
	}
}
