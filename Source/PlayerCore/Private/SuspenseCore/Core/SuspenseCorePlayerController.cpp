// SuspenseCorePlayerController.cpp
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Core/SuspenseCorePlayerController.h"
#include "SuspenseCore/Core/SuspenseCorePlayerState.h"
#include "SuspenseCore/Characters/SuspenseCoreCharacter.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Services/SuspenseCoreServiceLocator.h"
#include "AbilitySystemComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"

// ═══════════════════════════════════════════════════════════════════════════════
// CONSTRUCTOR
// ═══════════════════════════════════════════════════════════════════════════════

ASuspenseCorePlayerController::ASuspenseCorePlayerController()
{
	// Enable input replication for multiplayer
	bReplicates = true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// CONTROLLER LIFECYCLE
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCorePlayerController::BeginPlay()
{
	Super::BeginPlay();

	SetupEnhancedInput();

	// Publish controller ready event
	PublishEvent(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Controller.Ready")),
		FString::Printf(TEXT("{\"isLocal\":%s}"), IsLocalController() ? TEXT("true") : TEXT("false"))
	);
}

void ASuspenseCorePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clear cached references
	CachedEventBus.Reset();
	CachedPlayerState.Reset();

	Super::EndPlay(EndPlayReason);
}

void ASuspenseCorePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// Core movement
		if (IA_Move)
		{
			EnhancedInput->BindAction(IA_Move, ETriggerEvent::Triggered, this, &ASuspenseCorePlayerController::HandleMove);
			EnhancedInput->BindAction(IA_Move, ETriggerEvent::Completed, this, &ASuspenseCorePlayerController::HandleMove);
		}

		if (IA_Look)
		{
			EnhancedInput->BindAction(IA_Look, ETriggerEvent::Triggered, this, &ASuspenseCorePlayerController::HandleLook);
		}

		// Jump
		if (IA_Jump)
		{
			EnhancedInput->BindAction(IA_Jump, ETriggerEvent::Started, this, &ASuspenseCorePlayerController::HandleJumpPressed);
			EnhancedInput->BindAction(IA_Jump, ETriggerEvent::Completed, this, &ASuspenseCorePlayerController::HandleJumpReleased);
		}

		// Sprint
		if (IA_Sprint)
		{
			EnhancedInput->BindAction(IA_Sprint, ETriggerEvent::Started, this, &ASuspenseCorePlayerController::HandleSprintPressed);
			EnhancedInput->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &ASuspenseCorePlayerController::HandleSprintReleased);
		}

		// Crouch
		if (IA_Crouch)
		{
			EnhancedInput->BindAction(IA_Crouch, ETriggerEvent::Started, this, &ASuspenseCorePlayerController::HandleCrouchPressed);
			EnhancedInput->BindAction(IA_Crouch, ETriggerEvent::Completed, this, &ASuspenseCorePlayerController::HandleCrouchReleased);
		}

		// Interact
		if (IA_Interact)
		{
			EnhancedInput->BindAction(IA_Interact, ETriggerEvent::Started, this, &ASuspenseCorePlayerController::HandleInteract);
		}

		// Bind additional ability inputs
		BindAbilityInputs();
	}
}

void ASuspenseCorePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// Initialize ASC with new pawn
	if (ASuspenseCorePlayerState* PS = GetSuspenseCorePlayerState())
	{
		if (UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent())
		{
			ASC->InitAbilityActorInfo(PS, InPawn);
		}
	}

	PublishEvent(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Controller.Possessed")),
		FString::Printf(TEXT("{\"pawnClass\":\"%s\"}"), *InPawn->GetClass()->GetName())
	);
}

void ASuspenseCorePlayerController::OnUnPossess()
{
	PublishEvent(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Controller.UnPossessed")),
		TEXT("{}")
	);

	Super::OnUnPossess();
}

void ASuspenseCorePlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// Cache new player state
	CachedPlayerState = Cast<ASuspenseCorePlayerState>(PlayerState);
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API - STATE ACCESS
// ═══════════════════════════════════════════════════════════════════════════════

ASuspenseCorePlayerState* ASuspenseCorePlayerController::GetSuspenseCorePlayerState() const
{
	if (CachedPlayerState.IsValid())
	{
		return CachedPlayerState.Get();
	}

	ASuspenseCorePlayerState* PS = Cast<ASuspenseCorePlayerState>(PlayerState);
	if (PS)
	{
		const_cast<ASuspenseCorePlayerController*>(this)->CachedPlayerState = PS;
	}

	return PS;
}

ASuspenseCoreCharacter* ASuspenseCorePlayerController::GetSuspenseCoreCharacter() const
{
	return Cast<ASuspenseCoreCharacter>(GetPawn());
}

UAbilitySystemComponent* ASuspenseCorePlayerController::GetAbilitySystemComponent() const
{
	if (ASuspenseCorePlayerState* PS = GetSuspenseCorePlayerState())
	{
		return PS->GetAbilitySystemComponent();
	}
	return nullptr;
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API - INPUT
// ═══════════════════════════════════════════════════════════════════════════════

bool ASuspenseCorePlayerController::HasMovementInput() const
{
	return !CurrentMovementInput.IsNearlyZero();
}

FVector2D ASuspenseCorePlayerController::GetMovementInput() const
{
	return CurrentMovementInput;
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API - EVENTS
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCorePlayerController::PublishEvent(const FGameplayTag& EventTag, const FString& Payload)
{
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		EventBus->Publish(this, EventTag, Payload);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// INPUT HANDLERS
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCorePlayerController::HandleMove(const FInputActionValue& Value)
{
	CurrentMovementInput = Value.Get<FVector2D>();

	if (ASuspenseCoreCharacter* Character = GetSuspenseCoreCharacter())
	{
		Character->Move(CurrentMovementInput);
	}
}

void ASuspenseCorePlayerController::HandleLook(const FInputActionValue& Value)
{
	const FVector2D LookInput = Value.Get<FVector2D>();

	if (ASuspenseCoreCharacter* Character = GetSuspenseCoreCharacter())
	{
		Character->Look(LookInput);
	}
}

void ASuspenseCorePlayerController::HandleJumpPressed(const FInputActionValue& Value)
{
	ActivateAbilityByTag(FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Ability.Jump")), true);
}

void ASuspenseCorePlayerController::HandleJumpReleased(const FInputActionValue& Value)
{
	ActivateAbilityByTag(FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Ability.Jump")), false);
}

void ASuspenseCorePlayerController::HandleSprintPressed(const FInputActionValue& Value)
{
	ActivateAbilityByTag(FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Ability.Sprint")), true);
}

void ASuspenseCorePlayerController::HandleSprintReleased(const FInputActionValue& Value)
{
	ActivateAbilityByTag(FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Ability.Sprint")), false);
}

void ASuspenseCorePlayerController::HandleCrouchPressed(const FInputActionValue& Value)
{
	ActivateAbilityByTag(FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Ability.Crouch")), true);
}

void ASuspenseCorePlayerController::HandleCrouchReleased(const FInputActionValue& Value)
{
	ActivateAbilityByTag(FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Ability.Crouch")), false);
}

void ASuspenseCorePlayerController::HandleInteract(const FInputActionValue& Value)
{
	ActivateAbilityByTag(FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Ability.Interact")), true);
}

// ═══════════════════════════════════════════════════════════════════════════════
// ABILITY ACTIVATION
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCorePlayerController::ActivateAbilityByTag(const FGameplayTag& AbilityTag, bool bPressed)
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		if (bPressed)
		{
			// Try to activate ability with tag
			FGameplayTagContainer TagContainer;
			TagContainer.AddTag(AbilityTag);
			ASC->TryActivateAbilitiesByTag(TagContainer);
		}
		else
		{
			// Cancel ability with tag if it's active
			FGameplayTagContainer TagContainer;
			TagContainer.AddTag(AbilityTag);
			ASC->CancelAbilities(&TagContainer);
		}
	}

	// Also publish input event for UI/other systems
	PublishEvent(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Input.Ability")),
		FString::Printf(TEXT("{\"ability\":\"%s\",\"pressed\":%s}"),
			*AbilityTag.ToString(),
			bPressed ? TEXT("true") : TEXT("false"))
	);
}

void ASuspenseCorePlayerController::SendAbilityInput(const FGameplayTag& InputTag, bool bPressed)
{
	// Forward to ASC for ability input handling
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		if (bPressed)
		{
			ASC->AbilityLocalInputPressed(InputTag.IsValid() ? InputTag.GetTagName().GetComparisonIndex().IsValidIndex() ? 0 : 0 : 0);
		}
		else
		{
			ASC->AbilityLocalInputReleased(0);
		}
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCorePlayerController::SetupEnhancedInput()
{
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (DefaultMappingContext)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, MappingContextPriority);
		}
	}
}

void ASuspenseCorePlayerController::BindAbilityInputs()
{
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		for (const FSuspenseCoreInputBinding& Binding : AbilityInputBindings)
		{
			if (Binding.InputAction && Binding.AbilityTag.IsValid())
			{
				// Capture binding for lambda
				const FGameplayTag CapturedTag = Binding.AbilityTag;
				const bool bOnRelease = Binding.bActivateOnRelease;

				if (bOnRelease)
				{
					EnhancedInput->BindAction(Binding.InputAction, ETriggerEvent::Completed, this,
						[this, CapturedTag](const FInputActionValue& Value)
						{
							ActivateAbilityByTag(CapturedTag, true);
						});
				}
				else
				{
					EnhancedInput->BindAction(Binding.InputAction, ETriggerEvent::Started, this,
						[this, CapturedTag](const FInputActionValue& Value)
						{
							ActivateAbilityByTag(CapturedTag, true);
						});

					EnhancedInput->BindAction(Binding.InputAction, ETriggerEvent::Completed, this,
						[this, CapturedTag](const FInputActionValue& Value)
						{
							ActivateAbilityByTag(CapturedTag, false);
						});
				}
			}
		}
	}
}

USuspenseCoreEventBus* ASuspenseCorePlayerController::GetEventBus() const
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	USuspenseCoreEventBus* EventBus = USuspenseCoreServiceLocator::GetEventBus(GetWorld());
	if (EventBus)
	{
		const_cast<ASuspenseCorePlayerController*>(this)->CachedEventBus = EventBus;
	}

	return EventBus;
}
