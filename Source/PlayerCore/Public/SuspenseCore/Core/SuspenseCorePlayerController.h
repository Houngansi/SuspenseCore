// SuspenseCorePlayerController.h
// Copyright Suspense Team. All Rights Reserved.
// Clean Architecture PlayerController with EventBus integration

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "SuspenseCorePlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class USuspenseCoreEventBus;
class UAbilitySystemComponent;
class ASuspenseCorePlayerState;
class ASuspenseCoreCharacter;

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
class PLAYERCORE_API ASuspenseCorePlayerController : public APlayerController
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
	// ABILITY INPUT BINDINGS
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Additional ability bindings (beyond core movement) */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Input|Abilities")
	TArray<FSuspenseCoreInputBinding> AbilityInputBindings;

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
};
