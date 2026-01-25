// SuspenseCorePlayerController.cpp
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Core/SuspenseCorePlayerController.h"
#include "SuspenseCore/Core/SuspenseCorePlayerState.h"
#include "SuspenseCore/Characters/SuspenseCoreCharacter.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "SuspenseCore/Tags/SuspenseCoreMedicalNativeTags.h"
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
	bReplicates = true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// CONTROLLER LIFECYCLE
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCorePlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Set Input Mode to Game Only when transitioning from MainMenu
	if (IsLocalController())
	{
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		bShowMouseCursor = false;
	}

	SetupEnhancedInput();

	// Create UI widgets for local player
	if (IsLocalController())
	{
		CreatePauseMenu();
		CreateHUDWidget();

#if WITH_UI_SYSTEM
		if (USuspenseCoreUIManager* UIManager = USuspenseCoreUIManager::Get(this))
		{
			if (ContainerScreenWidgetClass)
			{
				UIManager->ContainerScreenClass = ContainerScreenWidgetClass;
			}
			if (TooltipWidgetClass)
			{
				UIManager->TooltipWidgetClass = TooltipWidgetClass;
			}
		}
#endif
	}

	// Publish controller ready event
	PublishEvent(
		SuspenseCoreTags::Event::Player::ControllerReady,
		FString::Printf(TEXT("{\"isLocal\":%s}"), IsLocalController() ? TEXT("true") : TEXT("false"))
	);
}

void ASuspenseCorePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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

		// UI Inputs
		if (IA_PauseGame)
		{
			EnhancedInput->BindAction(IA_PauseGame, ETriggerEvent::Started, this, &ASuspenseCorePlayerController::HandlePauseGame);
		}

		if (IA_QuickSave)
		{
			EnhancedInput->BindAction(IA_QuickSave, ETriggerEvent::Started, this, &ASuspenseCorePlayerController::HandleQuickSave);
		}

		if (IA_QuickLoad)
		{
			EnhancedInput->BindAction(IA_QuickLoad, ETriggerEvent::Started, this, &ASuspenseCorePlayerController::HandleQuickLoad);
		}

		// Inventory toggle
		if (IA_ToggleInventory)
		{
			EnhancedInput->BindAction(IA_ToggleInventory, ETriggerEvent::Started, this, &ASuspenseCorePlayerController::HandleToggleInventory);
		}

		// Weapon Inputs
		if (IA_Aim)
		{
			EnhancedInput->BindAction(IA_Aim, ETriggerEvent::Started, this, &ASuspenseCorePlayerController::HandleAimPressed);
			EnhancedInput->BindAction(IA_Aim, ETriggerEvent::Completed, this, &ASuspenseCorePlayerController::HandleAimReleased);
		}

		if (IA_Fire)
		{
			EnhancedInput->BindAction(IA_Fire, ETriggerEvent::Started, this, &ASuspenseCorePlayerController::HandleFirePressed);
			EnhancedInput->BindAction(IA_Fire, ETriggerEvent::Completed, this, &ASuspenseCorePlayerController::HandleFireReleased);
		}

		if (IA_Reload)
		{
			EnhancedInput->BindAction(IA_Reload, ETriggerEvent::Started, this, &ASuspenseCorePlayerController::HandleReload);
		}

		if (IA_SwitchFireMode)
		{
			// Activate on release (Completed) to prevent accidental switches during scroll
			EnhancedInput->BindAction(IA_SwitchFireMode, ETriggerEvent::Completed, this, &ASuspenseCorePlayerController::HandleSwitchFireMode);
		}

		// WeaponSlot Inputs (Direct weapon slot switching - keys 1-3, V)
		if (IA_WeaponSlot1)
		{
			EnhancedInput->BindAction(IA_WeaponSlot1, ETriggerEvent::Started, this, &ASuspenseCorePlayerController::HandleWeaponSlot1);
		}
		if (IA_WeaponSlot2)
		{
			EnhancedInput->BindAction(IA_WeaponSlot2, ETriggerEvent::Started, this, &ASuspenseCorePlayerController::HandleWeaponSlot2);
		}
		if (IA_WeaponSlot3)
		{
			EnhancedInput->BindAction(IA_WeaponSlot3, ETriggerEvent::Started, this, &ASuspenseCorePlayerController::HandleWeaponSlot3);
		}
		if (IA_MeleeWeapon)
		{
			EnhancedInput->BindAction(IA_MeleeWeapon, ETriggerEvent::Started, this, &ASuspenseCorePlayerController::HandleMeleeWeapon);
		}

		// QuickSlot Inputs (Tarkov-style magazine/item access - keys 4-7)
		if (IA_QuickSlot1)
		{
			EnhancedInput->BindAction(IA_QuickSlot1, ETriggerEvent::Started, this, &ASuspenseCorePlayerController::HandleQuickSlot1);
		}
		if (IA_QuickSlot2)
		{
			EnhancedInput->BindAction(IA_QuickSlot2, ETriggerEvent::Started, this, &ASuspenseCorePlayerController::HandleQuickSlot2);
		}
		if (IA_QuickSlot3)
		{
			EnhancedInput->BindAction(IA_QuickSlot3, ETriggerEvent::Started, this, &ASuspenseCorePlayerController::HandleQuickSlot3);
		}
		if (IA_QuickSlot4)
		{
			EnhancedInput->BindAction(IA_QuickSlot4, ETriggerEvent::Started, this, &ASuspenseCorePlayerController::HandleQuickSlot4);
		}

		// Bind additional ability inputs
		BindAbilityInputs();
	}
}

void ASuspenseCorePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (ASuspenseCorePlayerState* PS = GetSuspenseCorePlayerState())
	{
		if (UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent())
		{
			ASC->InitAbilityActorInfo(PS, InPawn);
		}
	}

	PublishEvent(
		SuspenseCoreTags::Event::Player::ControllerPossessed,
		FString::Printf(TEXT("{\"pawnClass\":\"%s\"}"), *InPawn->GetClass()->GetName())
	);
}

void ASuspenseCorePlayerController::OnUnPossess()
{
	PublishEvent(
		SuspenseCoreTags::Event::Player::ControllerUnPossessed,
		TEXT("{}")
	);

	Super::OnUnPossess();
}

void ASuspenseCorePlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
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
	ActivateAbilityByTag(SuspenseCoreTags::Ability::Jump, true);
}

void ASuspenseCorePlayerController::HandleJumpReleased(const FInputActionValue& Value)
{
	ActivateAbilityByTag(SuspenseCoreTags::Ability::Jump, false);
}

void ASuspenseCorePlayerController::HandleSprintPressed(const FInputActionValue& Value)
{
	ActivateAbilityByTag(SuspenseCoreTags::Ability::Sprint, true);
}

void ASuspenseCorePlayerController::HandleSprintReleased(const FInputActionValue& Value)
{
	ActivateAbilityByTag(SuspenseCoreTags::Ability::Sprint, false);
}

void ASuspenseCorePlayerController::HandleCrouchPressed(const FInputActionValue& Value)
{
	ActivateAbilityByTag(SuspenseCoreTags::Ability::Crouch, true);
}

void ASuspenseCorePlayerController::HandleCrouchReleased(const FInputActionValue& Value)
{
	ActivateAbilityByTag(SuspenseCoreTags::Ability::Crouch, false);
}

void ASuspenseCorePlayerController::HandleInteract(const FInputActionValue& Value)
{
	ActivateAbilityByTag(SuspenseCoreTags::Ability::Interact, true);
}

// ═══════════════════════════════════════════════════════════════════════════════
// WEAPON INPUT HANDLERS
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCorePlayerController::HandleAimPressed(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] ═══════════════════════════════════════════════════════"));
	UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] HandleAimPressed CALLED - RMB Pressed"));
	UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] Calling ActivateAbilityByTag(AimDownSight, true)"));
	ActivateAbilityByTag(SuspenseCoreTags::Ability::Weapon::AimDownSight, true);
}

void ASuspenseCorePlayerController::HandleAimReleased(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] HandleAimReleased CALLED - RMB Released"));
	UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] Calling ActivateAbilityByTag(AimDownSight, false) to CANCEL"));
	ActivateAbilityByTag(SuspenseCoreTags::Ability::Weapon::AimDownSight, false);
}

void ASuspenseCorePlayerController::HandleFirePressed(const FInputActionValue& Value)
{
	// Check equipment state to route Fire input to the appropriate ability:
	// 1. Medical equipped → GA_MedicalUse (use medical item)
	// 2. Grenade equipped → GA_GrenadeThrow (throw grenade)
	// 3. Otherwise → Normal weapon fire
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();

	UE_LOG(LogTemp, Warning, TEXT("[FIRE DEBUG] HandleFirePressed called"));
	UE_LOG(LogTemp, Warning, TEXT("[FIRE DEBUG] ASC: %s"), ASC ? TEXT("VALID") : TEXT("NULL"));

	if (ASC)
	{
		// Check for medical item equipped first (Tarkov-style medical flow)
		bool bHasMedicalTag = ASC->HasMatchingGameplayTag(SuspenseCoreMedicalTags::State::TAG_State_Medical_Equipped);
		bool bHasGrenadeTag = ASC->HasMatchingGameplayTag(SuspenseCoreTags::State::GrenadeEquipped);

		UE_LOG(LogTemp, Warning, TEXT("[FIRE DEBUG] HasMatchingGameplayTag(State.Medical.Equipped): %s"), bHasMedicalTag ? TEXT("YES") : TEXT("NO"));
		UE_LOG(LogTemp, Warning, TEXT("[FIRE DEBUG] HasMatchingGameplayTag(State.GrenadeEquipped): %s"), bHasGrenadeTag ? TEXT("YES") : TEXT("NO"));

		if (bHasMedicalTag)
		{
			// Medical item is equipped - activate use ability
			UE_LOG(LogTemp, Warning, TEXT("[FIRE DEBUG] >>> Activating MEDICAL USE ability <<<"));
			ActivateAbilityByTag(SuspenseCoreMedicalTags::Ability::TAG_Ability_Medical_Use, true);
		}
		else if (bHasGrenadeTag)
		{
			UE_LOG(LogTemp, Warning, TEXT("[FIRE DEBUG] >>> Activating GRENADE THROW ability <<<"));
			ActivateAbilityByTag(SuspenseCoreTags::Ability::Throwable::Grenade, true);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[FIRE DEBUG] Activating normal weapon fire"));
			ActivateAbilityByTag(SuspenseCoreTags::Ability::Weapon::Fire, true);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[FIRE DEBUG] No ASC - cannot activate ability"));
	}
}

void ASuspenseCorePlayerController::HandleFireReleased(const FInputActionValue& Value)
{
	// Check equipment state to route Fire release to the appropriate ability
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	// Check for medical item equipped (Tarkov-style medical flow)
	if (ASC->HasMatchingGameplayTag(SuspenseCoreMedicalTags::State::TAG_State_Medical_Equipped))
	{
		// Medical item is equipped - signal InputReleased to active medical use ability
		// Note: Medical use typically continues even after release (unlike grenades)
		// But we still signal the release in case the ability needs it
		FGameplayTagContainer TagContainer;
		TagContainer.AddTag(SuspenseCoreMedicalTags::Ability::TAG_Ability_Medical_Use);

		TArray<FGameplayAbilitySpec*> MatchingSpecs;
		ASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(TagContainer, MatchingSpecs);

		for (FGameplayAbilitySpec* Spec : MatchingSpecs)
		{
			if (Spec && Spec->IsActive())
			{
				ASC->AbilitySpecInputReleased(*Spec);
			}
		}
	}
	else if (ASC->HasMatchingGameplayTag(SuspenseCoreTags::State::GrenadeEquipped))
	{
		// Grenade is equipped - find and signal InputReleased to active grenade throw ability
		// This is better than CancelAbilities because it lets the ability handle release gracefully
		FGameplayTagContainer TagContainer;
		TagContainer.AddTag(SuspenseCoreTags::Ability::Throwable::Grenade);

		TArray<FGameplayAbilitySpec*> MatchingSpecs;
		ASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(TagContainer, MatchingSpecs);

		for (FGameplayAbilitySpec* Spec : MatchingSpecs)
		{
			if (Spec && Spec->IsActive())
			{
				// Signal input release to the active ability instance
				ASC->AbilitySpecInputReleased(*Spec);
			}
		}
	}
	else
	{
		// Normal weapon fire release
		ActivateAbilityByTag(SuspenseCoreTags::Ability::Weapon::Fire, false);
	}
}

void ASuspenseCorePlayerController::HandleReload(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Warning, TEXT("[RELOAD DEBUG] ═══════════════════════════════════════════════════════"));
	UE_LOG(LogTemp, Warning, TEXT("[RELOAD DEBUG] HandleReload CALLED - R key pressed"));
	UE_LOG(LogTemp, Warning, TEXT("[RELOAD DEBUG] Calling ActivateAbilityByTag(Reload, true)"));

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (ASC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RELOAD DEBUG] ASC found: %s"), *ASC->GetName());

		// Check if ability is granted
		FGameplayTagContainer TagContainer;
		TagContainer.AddTag(SuspenseCoreTags::Ability::Weapon::Reload);
		TArray<FGameplayAbilitySpec*> MatchingSpecs;
		ASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(TagContainer, MatchingSpecs);
		UE_LOG(LogTemp, Warning, TEXT("[RELOAD DEBUG] Found %d abilities matching Reload tag"), MatchingSpecs.Num());

		for (FGameplayAbilitySpec* Spec : MatchingSpecs)
		{
			if (Spec && Spec->Ability)
			{
				UE_LOG(LogTemp, Warning, TEXT("[RELOAD DEBUG]   Ability: %s (Active: %s)"),
					*Spec->Ability->GetName(),
					Spec->IsActive() ? TEXT("YES") : TEXT("NO"));
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[RELOAD DEBUG] ❌ ASC NOT FOUND!"));
	}

	ActivateAbilityByTag(SuspenseCoreTags::Ability::Weapon::Reload, true);
}

void ASuspenseCorePlayerController::HandleSwitchFireMode(const FInputActionValue& Value)
{
	ActivateAbilityByTag(SuspenseCoreTags::Ability::Weapon::FireModeSwitch, true);
}

// ═══════════════════════════════════════════════════════════════════════════════
// WEAPONSLOT INPUT HANDLERS
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCorePlayerController::HandleWeaponSlot1(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Log, TEXT("[WeaponSlot] HandleWeaponSlot1 triggered (Key 1 → Primary)"));
	ActivateAbilityByTag(SuspenseCoreTags::Ability::WeaponSlot::Primary, true);
}

void ASuspenseCorePlayerController::HandleWeaponSlot2(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Log, TEXT("[WeaponSlot] HandleWeaponSlot2 triggered (Key 2 → Secondary)"));
	ActivateAbilityByTag(SuspenseCoreTags::Ability::WeaponSlot::Secondary, true);
}

void ASuspenseCorePlayerController::HandleWeaponSlot3(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Log, TEXT("[WeaponSlot] HandleWeaponSlot3 triggered (Key 3 → Sidearm)"));
	ActivateAbilityByTag(SuspenseCoreTags::Ability::WeaponSlot::Sidearm, true);
}

void ASuspenseCorePlayerController::HandleMeleeWeapon(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Log, TEXT("[WeaponSlot] HandleMeleeWeapon triggered (Key V → Melee)"));
	ActivateAbilityByTag(SuspenseCoreTags::Ability::WeaponSlot::Melee, true);
}

// ═══════════════════════════════════════════════════════════════════════════════
// QUICKSLOT INPUT HANDLERS
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCorePlayerController::HandleQuickSlot1(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Log, TEXT("[QuickSlot] HandleQuickSlot1 triggered"));
	ActivateAbilityByTag(SuspenseCoreTags::Ability::QuickSlot::Slot1, true);
}

void ASuspenseCorePlayerController::HandleQuickSlot2(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Log, TEXT("[QuickSlot] HandleQuickSlot2 triggered"));
	ActivateAbilityByTag(SuspenseCoreTags::Ability::QuickSlot::Slot2, true);
}

void ASuspenseCorePlayerController::HandleQuickSlot3(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Log, TEXT("[QuickSlot] HandleQuickSlot3 triggered"));
	ActivateAbilityByTag(SuspenseCoreTags::Ability::QuickSlot::Slot3, true);
}

void ASuspenseCorePlayerController::HandleQuickSlot4(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Log, TEXT("[QuickSlot] HandleQuickSlot4 triggered"));
	ActivateAbilityByTag(SuspenseCoreTags::Ability::QuickSlot::Slot4, true);
}

// ═══════════════════════════════════════════════════════════════════════════════
// ABILITY ACTIVATION
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCorePlayerController::ActivateAbilityByTag(const FGameplayTag& AbilityTag, bool bPressed)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ActivateAbilityByTag] No ASC!"));
		return;
	}

	if (bPressed)
	{
		// Try to activate ability with matching tag
		FGameplayTagContainer TagContainer;
		TagContainer.AddTag(AbilityTag);

		// Debug: Check if any abilities match this tag
		TArray<FGameplayAbilitySpec*> MatchingSpecs;
		ASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(TagContainer, MatchingSpecs);
		UE_LOG(LogTemp, Warning, TEXT("[ActivateAbilityByTag] Tag: %s, Found %d matching abilities"), *AbilityTag.ToString(), MatchingSpecs.Num());

		for (FGameplayAbilitySpec* Spec : MatchingSpecs)
		{
			if (Spec && Spec->Ability)
			{
				UE_LOG(LogTemp, Warning, TEXT("[ActivateAbilityByTag]   - %s (Active: %s)"),
					*Spec->Ability->GetName(),
					Spec->IsActive() ? TEXT("YES") : TEXT("NO"));
			}
		}

		bool bSuccess = ASC->TryActivateAbilitiesByTag(TagContainer);
		UE_LOG(LogTemp, Warning, TEXT("[ActivateAbilityByTag] TryActivateAbilitiesByTag result: %s"), bSuccess ? TEXT("SUCCESS") : TEXT("FAILED"));
	}
	else
	{
		// Cancel ability on release (for abilities that need release handling)
		FGameplayTagContainer TagContainer;
		TagContainer.AddTag(AbilityTag);
		ASC->CancelAbilities(&TagContainer);
	}

	// Publish input event for UI/other systems
	PublishEvent(
		SuspenseCoreTags::Event::Input::AbilityActivated,
		FString::Printf(TEXT("{\"ability\":\"%s\",\"pressed\":%s}"),
			*AbilityTag.ToString(),
			bPressed ? TEXT("true") : TEXT("false"))
	);
}

void ASuspenseCorePlayerController::SendAbilityInput(const FGameplayTag& InputTag, bool bPressed)
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
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
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);
	if (Subsystem && DefaultMappingContext)
	{
		Subsystem->RemoveMappingContext(DefaultMappingContext);
		Subsystem->AddMappingContext(DefaultMappingContext, MappingContextPriority);
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
	if (!PauseMenuWidgetClass)
	{
		return;
	}

	PauseMenuWidget = CreateWidget<USuspenseCorePauseMenuWidget>(this, PauseMenuWidgetClass);
	if (PauseMenuWidget)
	{
		PauseMenuWidget->AddToViewport(100);
		PauseMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
#endif
}

void ASuspenseCorePlayerController::TogglePauseMenu()
{
#if WITH_UI_SYSTEM
	if (USuspenseCorePauseMenuWidget* PauseMenu = Cast<USuspenseCorePauseMenuWidget>(PauseMenuWidget))
	{
		PauseMenu->TogglePauseMenu();
	}
#endif
}

void ASuspenseCorePlayerController::ShowPauseMenu()
{
#if WITH_UI_SYSTEM
	if (USuspenseCorePauseMenuWidget* PauseMenu = Cast<USuspenseCorePauseMenuWidget>(PauseMenuWidget))
	{
		PauseMenu->ShowPauseMenu();
	}
#endif
}

void ASuspenseCorePlayerController::HidePauseMenu()
{
#if WITH_UI_SYSTEM
	if (USuspenseCorePauseMenuWidget* PauseMenu = Cast<USuspenseCorePauseMenuWidget>(PauseMenuWidget))
	{
		PauseMenu->HidePauseMenu();
	}
#endif
}

bool ASuspenseCorePlayerController::IsPauseMenuVisible() const
{
#if WITH_UI_SYSTEM
	if (USuspenseCorePauseMenuWidget* PauseMenu = Cast<USuspenseCorePauseMenuWidget>(PauseMenuWidget))
	{
		return PauseMenu->IsMenuVisible();
	}
#endif
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
		return;
	}

	HUDWidget = CreateWidget<UUserWidget>(this, HUDWidgetClass);
	if (HUDWidget)
	{
		HUDWidget->AddToViewport(10);
		HUDWidget->SetVisibility(ESlateVisibility::Visible);
	}
#endif
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
	TogglePauseMenu();
}

void ASuspenseCorePlayerController::HandlePauseGameTriggered(const FInputActionValue& Value)
{
	// Kept for potential debug use
}

void ASuspenseCorePlayerController::HandleQuickSave(const FInputActionValue& Value)
{
	QuickSave();

	PublishEvent(
		SuspenseCoreTags::Event::Save::QuickSave,
		TEXT("{}")
	);
}

void ASuspenseCorePlayerController::HandleQuickLoad(const FInputActionValue& Value)
{
	QuickLoad();

	PublishEvent(
		SuspenseCoreTags::Event::Save::QuickLoad,
		TEXT("{}")
	);
}

// ═══════════════════════════════════════════════════════════════════════════════
// ISuspenseCoreUIController Interface
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCorePlayerController::PushUIMode_Implementation(const FString& Reason)
{
	UIModeStack.Add(Reason);
	ApplyCurrentUIMode();
}

void ASuspenseCorePlayerController::PopUIMode_Implementation(const FString& Reason)
{
	int32 Index = UIModeStack.Find(Reason);
	if (Index != INDEX_NONE)
	{
		UIModeStack.RemoveAt(Index);
	}

	ApplyCurrentUIMode();
}

void ASuspenseCorePlayerController::SetCursorVisible_Implementation(bool bShowCursor)
{
	bShowMouseCursor = bShowCursor;

	if (bShowCursor)
	{
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
		bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
	}
	else
	{
		bShowMouseCursor = false;
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// INVENTORY / CONTAINER SCREEN
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCorePlayerController::HandleToggleInventory(const FInputActionValue& Value)
{
	ToggleInventory();
}

void ASuspenseCorePlayerController::ToggleInventory()
{
#if WITH_UI_SYSTEM
	USuspenseCoreUIManager* UIManager = USuspenseCoreUIManager::Get(this);
	if (!UIManager)
	{
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
#endif
}

void ASuspenseCorePlayerController::ShowInventory()
{
#if WITH_UI_SYSTEM
	USuspenseCoreUIManager* UIManager = USuspenseCoreUIManager::Get(this);
	if (!UIManager)
	{
		return;
	}

	static const FGameplayTag EquipmentPanelTag = SuspenseCoreTags::UI::Panel::Equipment;

	if (UIManager->ShowContainerScreen(this, EquipmentPanelTag))
	{
		ISuspenseCoreUIController::Execute_PushUIMode(this, TEXT("Inventory"));
	}
#endif
}

void ASuspenseCorePlayerController::HideInventory()
{
#if WITH_UI_SYSTEM
	USuspenseCoreUIManager* UIManager = USuspenseCoreUIManager::Get(this);
	if (!UIManager)
	{
		return;
	}

	UIManager->HideContainerScreen();
	ISuspenseCoreUIController::Execute_PopUIMode(this, TEXT("Inventory"));
#endif
}

bool ASuspenseCorePlayerController::IsInventoryVisible() const
{
#if WITH_UI_SYSTEM
	USuspenseCoreUIManager* UIManager = USuspenseCoreUIManager::Get(this);
	return UIManager && UIManager->IsContainerScreenVisible();
#else
	return false;
#endif
}
