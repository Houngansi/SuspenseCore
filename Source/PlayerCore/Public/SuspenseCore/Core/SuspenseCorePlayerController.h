// SuspenseCorePlayerController.h
// Copyright Suspense Team. All Rights Reserved.
// Clean Architecture PlayerController with EventBus integration

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "SuspenseCore/Interfaces/SuspenseCoreUIController.h"
#include "SuspenseCorePlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class USuspenseCoreEventBus;
class UAbilitySystemComponent;
class ASuspenseCorePlayerState;
class ASuspenseCoreCharacter;
class UUserWidget;

// ═══════════════════════════════════════════════════════════════════════════════
// INPUT CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════════

USTRUCT(BlueprintType)
struct FSuspenseCoreInputBinding
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UInputAction* InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag AbilityTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bActivateOnRelease = false;
};

// ═══════════════════════════════════════════════════════════════════════════════
// PLAYER CONTROLLER
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Clean Architecture PlayerController with EventBus integration
 *
 * DESIGN PRINCIPLES:
 * - Input handling routed through GAS abilities
 * - EventBus for UI and system communication
 * - Minimal state - delegates to PlayerState and Character
 *
 * RESPONSIBILITIES:
 * - Enhanced Input setup and handling
 * - Route ability input to ASC
 * - Camera control
 * - UI management coordination
 */
UCLASS()
class PLAYERCORE_API ASuspenseCorePlayerController : public APlayerController, public ISuspenseCoreUIController
{
	GENERATED_BODY()

public:
	ASuspenseCorePlayerController();

	// ═══════════════════════════════════════════════════════════════════════════════
	// CONTROLLER LIFECYCLE
	// ═══════════════════════════════════════════════════════════════════════════════

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void OnRep_PlayerState() override;

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - STATE ACCESS
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Get SuspenseCore PlayerState (typed) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|State")
	ASuspenseCorePlayerState* GetSuspenseCorePlayerState() const;

	/** Get SuspenseCore Character (typed) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|State")
	ASuspenseCoreCharacter* GetSuspenseCoreCharacter() const;

	/** Get ASC from PlayerState */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|GAS")
	UAbilitySystemComponent* GetAbilitySystemComponent() const;

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - INPUT
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Check if any movement input is active */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Input")
	bool HasMovementInput() const;

	/** Get current movement input as 2D vector */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Input")
	FVector2D GetMovementInput() const;

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - EVENTS
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Publish an event to the EventBus */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
	void PublishEvent(const FGameplayTag& EventTag, const FString& Payload = TEXT(""));

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - PAUSE MENU
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Toggle pause menu visibility */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	void TogglePauseMenu();

	/** Show pause menu */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	void ShowPauseMenu();

	/** Hide pause menu */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	void HidePauseMenu();

	/** Quick save game */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Save")
	void QuickSave();

	/** Quick load game */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Save")
	void QuickLoad();

	/** Check if pause menu is visible */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	bool IsPauseMenuVisible() const;

	/** Get pause menu widget (nullptr if WITH_UI_SYSTEM=0) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	UUserWidget* GetPauseMenuWidget() const { return PauseMenuWidget; }

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - HUD WIDGET (nullptr if WITH_UI_SYSTEM=0)
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Get HUD widget */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	UUserWidget* GetHUDWidget() const { return HUDWidget; }

	/** Show HUD widget */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	void ShowHUD();

	/** Hide HUD widget */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	void HideHUD();

	/** Check if HUD is visible */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	bool IsHUDVisible() const;

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - INVENTORY/CONTAINER SCREEN
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Toggle inventory screen visibility */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	void ToggleInventory();

	/** Show inventory screen */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	void ShowInventory();

	/** Hide inventory screen */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	void HideInventory();

	/** Check if inventory screen is visible */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	bool IsInventoryVisible() const;

	// ═══════════════════════════════════════════════════════════════════════════════
	// ISuspenseCoreUIController Interface
	// ═══════════════════════════════════════════════════════════════════════════════

	virtual void PushUIMode_Implementation(const FString& Reason) override;
	virtual void PopUIMode_Implementation(const FString& Reason) override;
	virtual void SetCursorVisible_Implementation(bool bShowCursor) override;
	virtual bool IsUIActive_Implementation() const override { return UIModeStack.Num() > 0; }

	/**
	 * Get count of active UI layers.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	int32 GetUIStackCount() const { return UIModeStack.Num(); }

protected:
	// ═══════════════════════════════════════════════════════════════════════════════
	// INPUT CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Default input mapping context */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input")
	UInputMappingContext* DefaultMappingContext;

	/** Input priority for mapping context */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input")
	int32 MappingContextPriority = 0;

	// ═══════════════════════════════════════════════════════════════════════════════
	// CORE INPUT ACTIONS
	// ═══════════════════════════════════════════════════════════════════════════════

	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input|Movement")
	UInputAction* IA_Move;

	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input|Movement")
	UInputAction* IA_Look;

	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input|Actions")
	UInputAction* IA_Jump;

	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input|Actions")
	UInputAction* IA_Sprint;

	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input|Actions")
	UInputAction* IA_Crouch;

	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input|Actions")
	UInputAction* IA_Interact;

	// ═══════════════════════════════════════════════════════════════════════════════
	// UI INPUT ACTIONS
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Pause game / toggle pause menu */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input|UI")
	UInputAction* IA_PauseGame;

	/** Quick save (F5) */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input|UI")
	UInputAction* IA_QuickSave;

	/** Quick load (F9) */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input|UI")
	UInputAction* IA_QuickLoad;

	/** Toggle inventory (Tab or I) */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input|UI")
	UInputAction* IA_ToggleInventory;

	// ═══════════════════════════════════════════════════════════════════════════════
	// WEAPON INPUT ACTIONS
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Aim down sights (RMB) - hold to aim */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input|Weapon")
	UInputAction* IA_Aim;

	/** Fire weapon (LMB) */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input|Weapon")
	UInputAction* IA_Fire;

	/** Reload weapon (R) */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input|Weapon")
	UInputAction* IA_Reload;

	/** Switch fire mode (Middle Mouse Button) - activates on release */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input|Weapon")
	UInputAction* IA_SwitchFireMode;

	// ═══════════════════════════════════════════════════════════════════════════════
	// WEAPON SLOT INPUT ACTIONS (Direct weapon slot switching via keys 1-3, V)
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Primary Weapon (Key 1 → Slot 0) */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input|WeaponSlot")
	UInputAction* IA_WeaponSlot1;

	/** Secondary Weapon (Key 2 → Slot 1) */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input|WeaponSlot")
	UInputAction* IA_WeaponSlot2;

	/** Sidearm/Holster (Key 3 → Slot 2) */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input|WeaponSlot")
	UInputAction* IA_WeaponSlot3;

	/** Melee/Knife (Key V → Slot 3) */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input|WeaponSlot")
	UInputAction* IA_MeleeWeapon;

	// ═══════════════════════════════════════════════════════════════════════════════
	// QUICKSLOT INPUT ACTIONS (Tarkov-style magazine/item access)
	// ═══════════════════════════════════════════════════════════════════════════════

	/** QuickSlot 1 (Key 4) */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input|QuickSlot")
	UInputAction* IA_QuickSlot1;

	/** QuickSlot 2 (Key 5) */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input|QuickSlot")
	UInputAction* IA_QuickSlot2;

	/** QuickSlot 3 (Key 6) */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input|QuickSlot")
	UInputAction* IA_QuickSlot3;

	/** QuickSlot 4 (Key 7) */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input|QuickSlot")
	UInputAction* IA_QuickSlot4;

	// ═══════════════════════════════════════════════════════════════════════════════
	// ABILITY INPUT BINDINGS
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Additional ability bindings (beyond core movement) */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input|Abilities")
	TArray<FSuspenseCoreInputBinding> AbilityInputBindings;

	// ═══════════════════════════════════════════════════════════════════════════════
	// UI CONFIGURATION (nullptr if WITH_UI_SYSTEM=0)
	// Widget classes using base UUserWidget* to satisfy UHT when module is disabled
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Pause menu widget class to spawn */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|UI")
	TSubclassOf<UUserWidget> PauseMenuWidgetClass;

	/** Spawned pause menu widget */
	UPROPERTY()
	UUserWidget* PauseMenuWidget = nullptr;

	/** HUD widget class to spawn (vitals display: HP/Shield/Stamina)
	 * Blueprint MUST inherit from USuspenseCoreGameHUDWidget! */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|UI")
	TSubclassOf<UUserWidget> HUDWidgetClass;

	/** Spawned HUD widget */
	UPROPERTY()
	UUserWidget* HUDWidget = nullptr;

	/** Container screen widget class (inventory, equipment, stash) */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|UI")
	TSubclassOf<UUserWidget> ContainerScreenWidgetClass;

	/** Tooltip widget class for items */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|UI")
	TSubclassOf<UUserWidget> TooltipWidgetClass;

	// ═══════════════════════════════════════════════════════════════════════════════
	// INPUT HANDLERS
	// ═══════════════════════════════════════════════════════════════════════════════

	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
	void HandleJumpPressed(const FInputActionValue& Value);
	void HandleJumpReleased(const FInputActionValue& Value);
	void HandleSprintPressed(const FInputActionValue& Value);
	void HandleSprintReleased(const FInputActionValue& Value);
	void HandleCrouchPressed(const FInputActionValue& Value);
	void HandleCrouchReleased(const FInputActionValue& Value);
	void HandleInteract(const FInputActionValue& Value);

	// UI Input Handlers
	void HandlePauseGame(const FInputActionValue& Value);
	void HandlePauseGameTriggered(const FInputActionValue& Value);  // Debug handler
	void HandleQuickSave(const FInputActionValue& Value);
	void HandleQuickLoad(const FInputActionValue& Value);
	void HandleToggleInventory(const FInputActionValue& Value);

	// Weapon Input Handlers
	void HandleAimPressed(const FInputActionValue& Value);
	void HandleAimReleased(const FInputActionValue& Value);
	void HandleFirePressed(const FInputActionValue& Value);
	void HandleFireReleased(const FInputActionValue& Value);
	void HandleReload(const FInputActionValue& Value);
	void HandleSwitchFireMode(const FInputActionValue& Value);

	// WeaponSlot Input Handlers (Direct weapon slot switching)
	void HandleWeaponSlot1(const FInputActionValue& Value);
	void HandleWeaponSlot2(const FInputActionValue& Value);
	void HandleWeaponSlot3(const FInputActionValue& Value);
	void HandleMeleeWeapon(const FInputActionValue& Value);

	// QuickSlot Input Handlers (Tarkov-style magazine/item access)
	void HandleQuickSlot1(const FInputActionValue& Value);
	void HandleQuickSlot2(const FInputActionValue& Value);
	void HandleQuickSlot3(const FInputActionValue& Value);
	void HandleQuickSlot4(const FInputActionValue& Value);

	// ═══════════════════════════════════════════════════════════════════════════════
	// ABILITY ACTIVATION
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Activate ability via tag */
	void ActivateAbilityByTag(const FGameplayTag& AbilityTag, bool bPressed);

	/** Send input event to ASC */
	void SendAbilityInput(const FGameplayTag& InputTag, bool bPressed);

	// ═══════════════════════════════════════════════════════════════════════════════
	// INTERNAL
	// ═══════════════════════════════════════════════════════════════════════════════

	void SetupEnhancedInput();
	void BindAbilityInputs();
	void CreatePauseMenu();
	void CreateHUDWidget();

	/** Handle ability input by binding index */
	void HandleAbilityInputByIndex(const FInputActionValue& Value, int32 BindingIndex);
	void HandleAbilityInputReleasedByIndex(const FInputActionValue& Value, int32 BindingIndex);

	USuspenseCoreEventBus* GetEventBus() const;

private:
	/** Current movement input */
	FVector2D CurrentMovementInput = FVector2D::ZeroVector;

	/** Cached references */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	UPROPERTY()
	TWeakObjectPtr<ASuspenseCorePlayerState> CachedPlayerState;

	/** Stack of active UI modes for cursor management */
	TArray<FString> UIModeStack;

	/** Apply UI mode based on stack state */
	void ApplyCurrentUIMode();
};
