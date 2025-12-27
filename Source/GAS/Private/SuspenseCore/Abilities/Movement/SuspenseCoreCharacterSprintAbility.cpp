// SuspenseCoreCharacterSprintAbility.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Abilities/Movement/SuspenseCoreCharacterSprintAbility.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "SuspenseCore/SuspenseCoreInterfaces.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "SuspenseCore/Input/SuspenseCoreAbilityInputID.h"

USuspenseCoreCharacterSprintAbility::USuspenseCoreCharacterSprintAbility()
{
	SprintSpeedMultiplier = 1.5f;
	StaminaCostPerSecond = 15.0f;
	MinimumStaminaToSprint = 10.0f;
	StaminaExhaustionThreshold = 1.0f;
	CurrentActorInfo = nullptr;

	// Configure ability
	AbilityInputID = ESuspenseCoreAbilityInputID::Sprint;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bRetriggerInstancedAbility = false;

	// AbilityTags (AssetTags) - used by TryActivateAbilitiesByTag to find matching abilities
	// Using Native Tags for compile-time safety
	FGameplayTagContainer AbilityTagContainer;
	AbilityTagContainer.AddTag(SuspenseCoreTags::Ability::Sprint);
	AbilityTagContainer.AddTag(SuspenseCoreTags::Ability::Movement::Sprint);
	SetAssetTags(AbilityTagContainer);

	// Applied tag while sprinting
	ActivationOwnedTags.AddTag(SuspenseCoreTags::State::Sprinting);

	// Block tags - can't sprint in these states
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Dead);
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Stunned);
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Crouching);
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

	// Check stamina via SuspenseCore AttributeSet
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (ASC && MinimumStaminaToSprint > 0.0f)
	{
		const USuspenseCoreAttributeSet* Attributes = ASC->GetSet<USuspenseCoreAttributeSet>();
		if (Attributes)
		{
			float CurrentStamina = Attributes->GetStamina();
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

	// Set sprint state on character via interface (updates speed and animation state)
	AActor* Avatar = ActorInfo->AvatarActor.Get();
	if (Avatar && Avatar->GetClass()->ImplementsInterface(USuspenseCoreMovementInterface::StaticClass()))
	{
		ISuspenseCoreMovementInterface::Execute_MovementStartSprint(Avatar);
	}

	// Apply sprint effects (stamina drain, etc.)
	ApplySprintEffects(ActorInfo);

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
	// bTestAlreadyReleased = false - don't check immediately, only fire on actual release event
	UAbilityTask_WaitInputRelease* WaitInputTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, false);
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

	// Stop sprint state on character via interface (updates speed and animation state)
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (Avatar && Avatar->GetClass()->ImplementsInterface(USuspenseCoreMovementInterface::StaticClass()))
	{
		ISuspenseCoreMovementInterface::Execute_MovementStopSprint(Avatar);
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

	// Check stamina via SuspenseCore AttributeSet
	const USuspenseCoreAttributeSet* Attributes = ASC->GetSet<USuspenseCoreAttributeSet>();
	if (Attributes)
	{
		float CurrentStamina = Attributes->GetStamina();
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
