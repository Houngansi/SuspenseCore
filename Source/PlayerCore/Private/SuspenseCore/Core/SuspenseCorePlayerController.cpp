// SuspenseCorePlayerController.cpp
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Core/SuspenseCorePlayerController.h"
#include "SuspenseCore/Core/SuspenseCorePlayerState.h"
#include "SuspenseCore/Characters/SuspenseCoreCharacter.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Save/SuspenseCoreSaveManager.h"
#include "AbilitySystemComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"

// ═══════════════════════════════════════════════════════════════════════════════
// OPTIONAL MODULE INCLUDES
// ═══════════════════════════════════════════════════════════════════════════════

#if WITH_UI_SYSTEM
#include "SuspenseCore/Subsystems/SuspenseCoreUIManager.h"
#include "SuspenseCore/Widgets/SuspenseCorePauseMenuWidget.h"
#include "SuspenseCore/Widgets/Layout/SuspenseCoreContainerScreenWidget.h"
#include "SuspenseCore/Widgets/Tooltip/SuspenseCoreTooltipWidget.h"
#endif

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

	UE_LOG(LogTemp, Warning, TEXT("=== SuspenseCorePlayerController::BeginPlay ==="));
	UE_LOG(LogTemp, Warning, TEXT("  Controller: %s"), *GetName());
	UE_LOG(LogTemp, Warning, TEXT("  IsLocalController: %s"), IsLocalController() ? TEXT("true") : TEXT("false"));

	// CRITICAL: Set Input Mode to Game Only!
	// When transitioning from MainMenu (which uses UIOnly mode), the input mode
	// may still be in UI mode. We MUST explicitly set it to Game mode.
	if (IsLocalController())
	{
		UE_LOG(LogTemp, Warning, TEXT("=== Setting Input Mode to Game Only ==="));
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		bShowMouseCursor = false;
		UE_LOG(LogTemp, Warning, TEXT("  Input Mode set to GameOnly, cursor hidden"));
	}

	SetupEnhancedInput();

	// Create UI widgets for local player (conditional on WITH_UI_SYSTEM)
	if (IsLocalController())
	{
		CreatePauseMenu();
		CreateHUDWidget();

#if WITH_UI_SYSTEM
		// Configure UIManager with widget classes
		if (USuspenseCoreUIManager* UIManager = USuspenseCoreUIManager::Get(this))
		{
			if (ContainerScreenWidgetClass)
			{
				UIManager->ContainerScreenClass = ContainerScreenWidgetClass;
				UE_LOG(LogTemp, Warning, TEXT("  UIManager: ContainerScreenClass set to %s"), *ContainerScreenWidgetClass->GetName());
			}
			if (TooltipWidgetClass)
			{
				UIManager->TooltipWidgetClass = TooltipWidgetClass;
				UE_LOG(LogTemp, Warning, TEXT("  UIManager: TooltipWidgetClass set to %s"), *TooltipWidgetClass->GetName());
			}
		}
#endif

		// Verify input mode
		UE_LOG(LogTemp, Warning, TEXT("=== Input Mode Check ==="));
		UE_LOG(LogTemp, Warning, TEXT("  bShowMouseCursor: %s"), bShowMouseCursor ? TEXT("true") : TEXT("false"));
	}

	// Publish controller ready event
	PublishEvent(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.ControllerReady")),
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
		UE_LOG(LogTemp, Warning, TEXT("  IA_Interact: %s"), IA_Interact ? *IA_Interact->GetName() : TEXT("NULL!"));
		if (IA_Interact)
		{
			EnhancedInput->BindAction(IA_Interact, ETriggerEvent::Started, this, &ASuspenseCorePlayerController::HandleInteract);
			UE_LOG(LogTemp, Warning, TEXT("  BOUND: IA_Interact -> HandleInteract (ETriggerEvent::Started)"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("  ERROR: IA_Interact is NULL! Interact input will NOT work!"));
		}

		// UI Inputs
		UE_LOG(LogTemp, Warning, TEXT("=== SetupInputComponent: Binding UI Inputs ==="));
		UE_LOG(LogTemp, Warning, TEXT("  IA_PauseGame: %s"), IA_PauseGame ? *IA_PauseGame->GetName() : TEXT("NULL!"));
		UE_LOG(LogTemp, Warning, TEXT("  IA_QuickSave: %s"), IA_QuickSave ? *IA_QuickSave->GetName() : TEXT("NULL!"));
		UE_LOG(LogTemp, Warning, TEXT("  IA_QuickLoad: %s"), IA_QuickLoad ? *IA_QuickLoad->GetName() : TEXT("NULL!"));

		if (IA_PauseGame)
		{
			// Bind to multiple trigger events for debugging
			EnhancedInput->BindAction(IA_PauseGame, ETriggerEvent::Started, this, &ASuspenseCorePlayerController::HandlePauseGame);
			EnhancedInput->BindAction(IA_PauseGame, ETriggerEvent::Triggered, this, &ASuspenseCorePlayerController::HandlePauseGameTriggered);
			UE_LOG(LogTemp, Warning, TEXT("  BOUND: IA_PauseGame -> HandlePauseGame (Started + Triggered)"));
		}

		if (IA_QuickSave)
		{
			EnhancedInput->BindAction(IA_QuickSave, ETriggerEvent::Started, this, &ASuspenseCorePlayerController::HandleQuickSave);
			UE_LOG(LogTemp, Warning, TEXT("  BOUND: IA_QuickSave -> HandleQuickSave"));
		}

		if (IA_QuickLoad)
		{
			EnhancedInput->BindAction(IA_QuickLoad, ETriggerEvent::Started, this, &ASuspenseCorePlayerController::HandleQuickLoad);
			UE_LOG(LogTemp, Warning, TEXT("  BOUND: IA_QuickLoad -> HandleQuickLoad"));
		}

		// Inventory toggle
		UE_LOG(LogTemp, Warning, TEXT("  IA_ToggleInventory: %s"), IA_ToggleInventory ? *IA_ToggleInventory->GetName() : TEXT("NULL!"));
		if (IA_ToggleInventory)
		{
			EnhancedInput->BindAction(IA_ToggleInventory, ETriggerEvent::Started, this, &ASuspenseCorePlayerController::HandleToggleInventory);
			UE_LOG(LogTemp, Warning, TEXT("  BOUND: IA_ToggleInventory -> HandleToggleInventory"));
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
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.ControllerPossessed")),
		FString::Printf(TEXT("{\"pawnClass\":\"%s\"}"), *InPawn->GetClass()->GetName())
	);
}

void ASuspenseCorePlayerController::OnUnPossess()
{
	PublishEvent(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.ControllerUnPossessed")),
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
		FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(const_cast<ASuspenseCorePlayerController*>(this));
		if (!Payload.IsEmpty())
		{
			EventData.SetString(FName("Payload"), Payload);
		}
		EventBus->Publish(EventTag, EventData);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// INPUT HANDLERS
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCorePlayerController::HandleMove(const FInputActionValue& Value)
{
	CurrentMovementInput = Value.Get<FVector2D>();

	if (ASuspenseCoreCharacter* SuspenseCharacter = GetSuspenseCoreCharacter())
	{
		SuspenseCharacter->Move(CurrentMovementInput);
	}
}

void ASuspenseCorePlayerController::HandleLook(const FInputActionValue& Value)
{
	const FVector2D LookInput = Value.Get<FVector2D>();

	if (ASuspenseCoreCharacter* SuspenseCharacter = GetSuspenseCoreCharacter())
	{
		SuspenseCharacter->Look(LookInput);
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
	UE_LOG(LogTemp, Warning, TEXT("=== HandleInteract CALLED ==="));
	ActivateAbilityByTag(FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Ability.Interact")), true);
}

// ═══════════════════════════════════════════════════════════════════════════════
// ABILITY ACTIVATION
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCorePlayerController::ActivateAbilityByTag(const FGameplayTag& AbilityTag, bool bPressed)
{
	UE_LOG(LogTemp, Warning, TEXT("=== ActivateAbilityByTag ==="));
	UE_LOG(LogTemp, Warning, TEXT("  AbilityTag: %s"), *AbilityTag.ToString());
	UE_LOG(LogTemp, Warning, TEXT("  bPressed: %s"), bPressed ? TEXT("true") : TEXT("false"));

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		UE_LOG(LogTemp, Error, TEXT("  ERROR: ASC is NULL! Cannot activate ability."));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("  ASC: %s (Owner: %s)"), *ASC->GetName(), *GetNameSafe(ASC->GetOwner()));

	if (bPressed)
	{
		// Try to activate ability with tag
		FGameplayTagContainer TagContainer;
		TagContainer.AddTag(AbilityTag);

		// Log available abilities and their tags
		UE_LOG(LogTemp, Warning, TEXT("  === Checking activatable abilities with tag %s ==="), *AbilityTag.ToString());

		TArray<FGameplayAbilitySpec*> MatchingAbilities;
		ASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(TagContainer, MatchingAbilities);

		UE_LOG(LogTemp, Warning, TEXT("  Found %d matching abilities"), MatchingAbilities.Num());

		for (FGameplayAbilitySpec* Spec : MatchingAbilities)
		{
			if (Spec && Spec->Ability)
			{
				UE_LOG(LogTemp, Warning, TEXT("    - Ability: %s"), *Spec->Ability->GetName());
				UE_LOG(LogTemp, Warning, TEXT("      AbilityTags: %s"), *Spec->Ability->AbilityTags.ToString());
				UE_LOG(LogTemp, Warning, TEXT("      IsActive: %s"), Spec->IsActive() ? TEXT("Yes") : TEXT("No"));
			}
		}

		// Try to activate
		bool bActivated = ASC->TryActivateAbilitiesByTag(TagContainer);
		UE_LOG(LogTemp, Warning, TEXT("  TryActivateAbilitiesByTag result: %s"), bActivated ? TEXT("SUCCESS") : TEXT("FAILED"));

		if (!bActivated)
		{
			// Log all granted abilities for debugging
			UE_LOG(LogTemp, Warning, TEXT("  === All granted abilities ==="));
			for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
			{
				if (Spec.Ability)
				{
					UE_LOG(LogTemp, Warning, TEXT("    - %s (Tags: %s)"),
						*Spec.Ability->GetName(),
						*Spec.Ability->AbilityTags.ToString());
				}
			}
		}
	}
	else
	{
		// Cancel ability with tag if it's active
		FGameplayTagContainer TagContainer;
		TagContainer.AddTag(AbilityTag);
		ASC->CancelAbilities(&TagContainer);
		UE_LOG(LogTemp, Warning, TEXT("  CancelAbilities called"));
	}

	// Also publish input event for UI/other systems
	PublishEvent(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Input.AbilityActivated")),
		FString::Printf(TEXT("{\"ability\":\"%s\",\"pressed\":%s}"),
			*AbilityTag.ToString(),
			bPressed ? TEXT("true") : TEXT("false"))
	);
}

void ASuspenseCorePlayerController::SendAbilityInput(const FGameplayTag& InputTag, bool bPressed)
{
	// Forward to ASC for ability input handling
	// Note: In GAS, ability input is typically handled via InputID (int32)
	// This method is a placeholder for custom input binding systems
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		// For now, use input ID 0 as default
		// Custom implementations should map InputTag to proper InputID
		if (bPressed)
		{
			ASC->AbilityLocalInputPressed(0);
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
	UE_LOG(LogTemp, Warning, TEXT("=== SetupEnhancedInput ==="));
	UE_LOG(LogTemp, Warning, TEXT("  DefaultMappingContext: %s"), DefaultMappingContext ? *DefaultMappingContext->GetName() : TEXT("NOT SET!"));
	UE_LOG(LogTemp, Warning, TEXT("  LocalPlayer: %s"), GetLocalPlayer() ? TEXT("EXISTS") : TEXT("NULL!"));

	// Log all mappings in the context to debug
	if (DefaultMappingContext)
	{
		UE_LOG(LogTemp, Warning, TEXT("  === MAPPINGS IN %s ==="), *DefaultMappingContext->GetName());
		const TArray<FEnhancedActionKeyMapping>& Mappings = DefaultMappingContext->GetMappings();
		for (const FEnhancedActionKeyMapping& Mapping : Mappings)
		{
			if (Mapping.Action)
			{
				UE_LOG(LogTemp, Warning, TEXT("    Action: %s -> Key: %s"),
					*Mapping.Action->GetName(),
					*Mapping.Key.ToString());
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("  === END MAPPINGS ==="));
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("  FAILED: LocalPlayer is NULL! Cannot setup Enhanced Input."));
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);
	if (Subsystem)
	{
		// Clear any existing mapping contexts first to ensure clean state
		if (DefaultMappingContext)
		{
			// Remove if already exists (in case of re-entry)
			Subsystem->RemoveMappingContext(DefaultMappingContext);

			// Add fresh
			Subsystem->AddMappingContext(DefaultMappingContext, MappingContextPriority);
			UE_LOG(LogTemp, Warning, TEXT("  SUCCESS: MappingContext added to subsystem with priority %d"), MappingContextPriority);

			// Verify it was added
			if (Subsystem->HasMappingContext(DefaultMappingContext))
			{
				UE_LOG(LogTemp, Warning, TEXT("  VERIFIED: MappingContext is active in subsystem"));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("  ERROR: MappingContext was NOT added to subsystem!"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("  FAILED: DefaultMappingContext is NOT SET in BP_SuspenseCorePlayerController!"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("  FAILED: Could not get EnhancedInputLocalPlayerSubsystem!"));
	}
}

void ASuspenseCorePlayerController::BindAbilityInputs()
{
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		return;
	}

	for (int32 i = 0; i < AbilityInputBindings.Num(); ++i)
	{
		const FSuspenseCoreInputBinding& Binding = AbilityInputBindings[i];

		if (!Binding.InputAction || !Binding.AbilityTag.IsValid())
		{
			continue;
		}

		// Use index-based approach to avoid lambda capture issues
		if (Binding.bActivateOnRelease)
		{
			EnhancedInput->BindAction(Binding.InputAction, ETriggerEvent::Completed, this,
				&ASuspenseCorePlayerController::HandleAbilityInputByIndex, i);
		}
		else
		{
			EnhancedInput->BindAction(Binding.InputAction, ETriggerEvent::Started, this,
				&ASuspenseCorePlayerController::HandleAbilityInputByIndex, i);
			EnhancedInput->BindAction(Binding.InputAction, ETriggerEvent::Completed, this,
				&ASuspenseCorePlayerController::HandleAbilityInputReleasedByIndex, i);
		}
	}
}

void ASuspenseCorePlayerController::HandleAbilityInputByIndex(const FInputActionValue& Value, int32 BindingIndex)
{
	if (AbilityInputBindings.IsValidIndex(BindingIndex))
	{
		ActivateAbilityByTag(AbilityInputBindings[BindingIndex].AbilityTag, true);
	}
}

void ASuspenseCorePlayerController::HandleAbilityInputReleasedByIndex(const FInputActionValue& Value, int32 BindingIndex)
{
	if (AbilityInputBindings.IsValidIndex(BindingIndex))
	{
		ActivateAbilityByTag(AbilityInputBindings[BindingIndex].AbilityTag, false);
	}
}

USuspenseCoreEventBus* ASuspenseCorePlayerController::GetEventBus() const
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	if (USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(this))
	{
		USuspenseCoreEventBus* EventBus = Manager->GetEventBus();
		if (EventBus)
		{
			const_cast<ASuspenseCorePlayerController*>(this)->CachedEventBus = EventBus;
		}
		return EventBus;
	}

	return nullptr;
}

// ═══════════════════════════════════════════════════════════════════════════════
// PAUSE MENU
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCorePlayerController::CreatePauseMenu()
{
#if WITH_UI_SYSTEM
	UE_LOG(LogTemp, Warning, TEXT("=== SuspenseCorePlayerController::CreatePauseMenu ==="));
	UE_LOG(LogTemp, Warning, TEXT("  PauseMenuWidgetClass: %s"), PauseMenuWidgetClass ? *PauseMenuWidgetClass->GetName() : TEXT("NOT SET!"));
	UE_LOG(LogTemp, Warning, TEXT("  IA_PauseGame: %s"), IA_PauseGame ? *IA_PauseGame->GetName() : TEXT("NOT SET!"));
	UE_LOG(LogTemp, Warning, TEXT("  IA_QuickSave: %s"), IA_QuickSave ? *IA_QuickSave->GetName() : TEXT("NOT SET!"));
	UE_LOG(LogTemp, Warning, TEXT("  IA_QuickLoad: %s"), IA_QuickLoad ? *IA_QuickLoad->GetName() : TEXT("NOT SET!"));

	if (!PauseMenuWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("  FAILED: PauseMenuWidgetClass is not set in BP_SuspenseCorePlayerController!"));
		return;
	}

	PauseMenuWidget = CreateWidget<USuspenseCorePauseMenuWidget>(this, PauseMenuWidgetClass);
	if (PauseMenuWidget)
	{
		PauseMenuWidget->AddToViewport(100); // High Z-order for pause menu
		PauseMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
		UE_LOG(LogTemp, Warning, TEXT("  SUCCESS: PauseMenuWidget created and added to viewport"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("  FAILED: Could not create PauseMenuWidget!"));
	}
#endif // WITH_UI_SYSTEM
}

void ASuspenseCorePlayerController::TogglePauseMenu()
{
#if WITH_UI_SYSTEM
	UE_LOG(LogTemp, Warning, TEXT("=== TogglePauseMenu called ==="));
	UE_LOG(LogTemp, Warning, TEXT("  PauseMenuWidget: %s"), PauseMenuWidget ? TEXT("EXISTS") : TEXT("NULL!"));

	if (USuspenseCorePauseMenuWidget* PauseMenu = Cast<USuspenseCorePauseMenuWidget>(PauseMenuWidget))
	{
		UE_LOG(LogTemp, Warning, TEXT("  Calling PauseMenuWidget->TogglePauseMenu()"));
		PauseMenu->TogglePauseMenu();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("  FAILED: PauseMenuWidget is NULL! Cannot toggle pause menu."));
	}
#endif // WITH_UI_SYSTEM
}

void ASuspenseCorePlayerController::ShowPauseMenu()
{
#if WITH_UI_SYSTEM
	if (USuspenseCorePauseMenuWidget* PauseMenu = Cast<USuspenseCorePauseMenuWidget>(PauseMenuWidget))
	{
		PauseMenu->ShowPauseMenu();
	}
#endif // WITH_UI_SYSTEM
}

void ASuspenseCorePlayerController::HidePauseMenu()
{
#if WITH_UI_SYSTEM
	if (USuspenseCorePauseMenuWidget* PauseMenu = Cast<USuspenseCorePauseMenuWidget>(PauseMenuWidget))
	{
		PauseMenu->HidePauseMenu();
	}
#endif // WITH_UI_SYSTEM
}

bool ASuspenseCorePlayerController::IsPauseMenuVisible() const
{
#if WITH_UI_SYSTEM
	if (USuspenseCorePauseMenuWidget* PauseMenu = Cast<USuspenseCorePauseMenuWidget>(PauseMenuWidget))
	{
		return PauseMenu->IsMenuVisible();
	}
#endif // WITH_UI_SYSTEM
	return false;
}

// ═══════════════════════════════════════════════════════════════════════════════
// HUD WIDGET
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCorePlayerController::CreateHUDWidget()
{
#if WITH_UI_SYSTEM
	if (!HUDWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCorePlayerController: HUDWidgetClass is not set"));
		return;
	}

	HUDWidget = CreateWidget<UUserWidget>(this, HUDWidgetClass);
	if (HUDWidget)
	{
		HUDWidget->AddToViewport(0); // Low Z-order for HUD (under pause menu)
		HUDWidget->SetVisibility(ESlateVisibility::Visible);
		UE_LOG(LogTemp, Log, TEXT("SuspenseCorePlayerController: HUD widget created and added to viewport"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("SuspenseCorePlayerController: Failed to create HUD widget"));
	}
#endif // WITH_UI_SYSTEM
}

void ASuspenseCorePlayerController::ShowHUD()
{
	if (HUDWidget)
	{
		HUDWidget->SetVisibility(ESlateVisibility::Visible);
	}
}

void ASuspenseCorePlayerController::HideHUD()
{
	if (HUDWidget)
	{
		HUDWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool ASuspenseCorePlayerController::IsHUDVisible() const
{
	return HUDWidget && HUDWidget->GetVisibility() == ESlateVisibility::Visible;
}

void ASuspenseCorePlayerController::QuickSave()
{
	if (USuspenseCoreSaveManager* SaveManager = USuspenseCoreSaveManager::Get(this))
	{
		SaveManager->QuickSave();
	}
}

void ASuspenseCorePlayerController::QuickLoad()
{
	if (USuspenseCoreSaveManager* SaveManager = USuspenseCoreSaveManager::Get(this))
	{
		SaveManager->QuickLoad();
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// UI INPUT HANDLERS
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCorePlayerController::HandlePauseGame(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Warning, TEXT("=== HandlePauseGame called! (ETriggerEvent::Started) ==="));
	UE_LOG(LogTemp, Warning, TEXT("  PauseMenuWidget: %s"), PauseMenuWidget ? TEXT("EXISTS") : TEXT("NULL!"));
	UE_LOG(LogTemp, Warning, TEXT("  bShowMouseCursor: %s"), bShowMouseCursor ? TEXT("true") : TEXT("false"));
	TogglePauseMenu();
}

void ASuspenseCorePlayerController::HandlePauseGameTriggered(const FInputActionValue& Value)
{
	// Debug: This helps us understand if the input is being received at all
	UE_LOG(LogTemp, Warning, TEXT("=== HandlePauseGameTriggered called! (ETriggerEvent::Triggered) ==="));
}

void ASuspenseCorePlayerController::HandleQuickSave(const FInputActionValue& Value)
{
	QuickSave();

	// Publish event for UI feedback
	PublishEvent(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Save.QuickSave")),
		TEXT("{}")
	);
}

void ASuspenseCorePlayerController::HandleQuickLoad(const FInputActionValue& Value)
{
	QuickLoad();

	// Publish event for UI feedback
	PublishEvent(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Save.QuickLoad")),
		TEXT("{}")
	);
}

// ═══════════════════════════════════════════════════════════════════════════════
// ISuspenseCoreUIController Interface
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCorePlayerController::PushUIMode_Implementation(const FString& Reason)
{
	UE_LOG(LogTemp, Log, TEXT("SuspenseCorePC: PushUIMode('%s') - Stack size: %d -> %d"),
		*Reason, UIModeStack.Num(), UIModeStack.Num() + 1);

	UIModeStack.Add(Reason);
	ApplyCurrentUIMode();
}

void ASuspenseCorePlayerController::PopUIMode_Implementation(const FString& Reason)
{
	int32 Index = UIModeStack.Find(Reason);
	if (Index != INDEX_NONE)
	{
		UIModeStack.RemoveAt(Index);
		UE_LOG(LogTemp, Log, TEXT("SuspenseCorePC: PopUIMode('%s') - Stack size: %d"),
			*Reason, UIModeStack.Num());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCorePC: PopUIMode('%s') - Reason not found in stack!"), *Reason);
	}

	ApplyCurrentUIMode();
}

void ASuspenseCorePlayerController::SetCursorVisible_Implementation(bool bShowCursor)
{
	bShowMouseCursor = bShowCursor;

	if (bShowCursor)
	{
		// GameAndUI allows both gameplay input and UI interaction
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
	}
	else
	{
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
	}
}

void ASuspenseCorePlayerController::ApplyCurrentUIMode()
{
	if (UIModeStack.Num() > 0)
	{
		// UI is active - show cursor, use GameAndUI mode for responsiveness
		bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);

		UE_LOG(LogTemp, Log, TEXT("SuspenseCorePC: UI Mode ACTIVE - Cursor ON, Mode: GameAndUI"));
	}
	else
	{
		// No UI - hide cursor, game only mode
		bShowMouseCursor = false;
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);

		UE_LOG(LogTemp, Log, TEXT("SuspenseCorePC: UI Mode INACTIVE - Cursor OFF, Mode: GameOnly"));
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// INVENTORY / CONTAINER SCREEN
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCorePlayerController::HandleToggleInventory(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Warning, TEXT("=== HandleToggleInventory called! ==="));
	ToggleInventory();
}

void ASuspenseCorePlayerController::ToggleInventory()
{
#if WITH_UI_SYSTEM
	UE_LOG(LogTemp, Warning, TEXT("=== ToggleInventory ==="));

	USuspenseCoreUIManager* UIManager = USuspenseCoreUIManager::Get(this);
	if (!UIManager)
	{
		UE_LOG(LogTemp, Error, TEXT("  FAILED: UIManager is NULL!"));
		return;
	}

	if (UIManager->IsContainerScreenVisible())
	{
		HideInventory();
	}
	else
	{
		ShowInventory();
	}
#endif // WITH_UI_SYSTEM
}

void ASuspenseCorePlayerController::ShowInventory()
{
#if WITH_UI_SYSTEM
	UE_LOG(LogTemp, Warning, TEXT("=== ShowInventory ==="));

	USuspenseCoreUIManager* UIManager = USuspenseCoreUIManager::Get(this);
	if (!UIManager)
	{
		UE_LOG(LogTemp, Error, TEXT("  FAILED: UIManager is NULL!"));
		return;
	}

	// Default to inventory panel
	static const FGameplayTag InventoryPanelTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UIPanel.Inventory"));

	if (UIManager->ShowContainerScreen(this, InventoryPanelTag))
	{
		// Push UI mode for cursor management
		ISuspenseCoreUIController::Execute_PushUIMode(this, TEXT("Inventory"));
		UE_LOG(LogTemp, Warning, TEXT("  SUCCESS: Container screen shown"));
	}
#endif // WITH_UI_SYSTEM
}

void ASuspenseCorePlayerController::HideInventory()
{
#if WITH_UI_SYSTEM
	UE_LOG(LogTemp, Warning, TEXT("=== HideInventory ==="));

	USuspenseCoreUIManager* UIManager = USuspenseCoreUIManager::Get(this);
	if (!UIManager)
	{
		UE_LOG(LogTemp, Error, TEXT("  FAILED: UIManager is NULL!"));
		return;
	}

	UIManager->HideContainerScreen();

	// Pop UI mode
	ISuspenseCoreUIController::Execute_PopUIMode(this, TEXT("Inventory"));
	UE_LOG(LogTemp, Warning, TEXT("  SUCCESS: Container screen hidden"));
#endif // WITH_UI_SYSTEM
}

bool ASuspenseCorePlayerController::IsInventoryVisible() const
{
#if WITH_UI_SYSTEM
	USuspenseCoreUIManager* UIManager = USuspenseCoreUIManager::Get(this);
	return UIManager && UIManager->IsContainerScreenVisible();
#else
	return false;
#endif // WITH_UI_SYSTEM
}
