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
class USuspenseCorePauseMenuWidget;

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

	/** Get pause menu widget */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	USuspenseCorePauseMenuWidget* GetPauseMenuWidget() const { return PauseMenuWidget; }

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

	// ═══════════════════════════════════════════════════════════════════════════════
	// ABILITY INPUT BINDINGS
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Additional ability bindings (beyond core movement) */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input|Abilities")
	TArray<FSuspenseCoreInputBinding> AbilityInputBindings;

	// ═══════════════════════════════════════════════════════════════════════════════
	// PAUSE MENU CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Pause menu widget class to spawn */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|UI")
	TSubclassOf<USuspenseCorePauseMenuWidget> PauseMenuWidgetClass;

	/** Spawned pause menu widget */
	UPROPERTY()
	USuspenseCorePauseMenuWidget* PauseMenuWidget;

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
