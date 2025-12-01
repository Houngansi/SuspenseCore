// SuspenseCoreCharacter.h
// Copyright Suspense Team. All Rights Reserved.
// Clean Architecture Character with EventBus integration

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UAbilitySystemComponent;
class USuspenseCoreEventBus;
class ASuspenseCorePlayerState;

// ═══════════════════════════════════════════════════════════════════════════════
// MOVEMENT STATE ENUM
// ═══════════════════════════════════════════════════════════════════════════════

UENUM(BlueprintType)
enum class ESuspenseCoreMovementState : uint8
{
	Idle		UMETA(DisplayName = "Idle"),
	Walking		UMETA(DisplayName = "Walking"),
	Sprinting	UMETA(DisplayName = "Sprinting"),
	Crouching	UMETA(DisplayName = "Crouching"),
	Jumping		UMETA(DisplayName = "Jumping"),
	Falling		UMETA(DisplayName = "Falling")
};

// ═══════════════════════════════════════════════════════════════════════════════
// CHARACTER
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Clean Architecture Character with EventBus integration
 *
 * DESIGN PRINCIPLES:
 * - ASC lives on PlayerState, not Character
 * - Movement through standard CharacterMovementComponent
 * - EventBus for state notifications
 * - Minimal state - visual representation only
 *
 * RESPONSIBILITIES:
 * - Visual representation (mesh, camera)
 * - Movement execution (receives commands from Controller)
 * - Animation state for procedural animations
 * - Weapon attachment points
 */
UCLASS()
class PLAYERCORE_API ASuspenseCoreCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ASuspenseCoreCharacter(const FObjectInitializer& ObjectInitializer);

	// ═══════════════════════════════════════════════════════════════════════════════
	// CHARACTER LIFECYCLE
	// ═══════════════════════════════════════════════════════════════════════════════

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_PlayerState() override;

	// ═══════════════════════════════════════════════════════════════════════════════
	// IABILITYSYSTEMINTERFACE
	// ═══════════════════════════════════════════════════════════════════════════════

	/** ASC is on PlayerState, not Character */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - MOVEMENT
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Process movement input */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Movement")
	void Move(const FVector2D& MovementInput);

	/** Process look input */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Movement")
	void Look(const FVector2D& LookInput);

	/** Start sprinting */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Movement")
	void StartSprinting();

	/** Stop sprinting */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Movement")
	void StopSprinting();

	/** Toggle crouch */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Movement")
	void ToggleCrouch();

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - STATE
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Get current movement state */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|State")
	ESuspenseCoreMovementState GetMovementState() const { return CurrentMovementState; }

	/** Check if character is sprinting */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|State")
	bool IsSprinting() const { return bIsSprinting; }

	/** Check if character has movement input */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|State")
	bool HasMovementInput() const;

	/** Get SuspenseCore PlayerState */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|State")
	ASuspenseCorePlayerState* GetSuspenseCorePlayerState() const;

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - ANIMATION
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Get forward movement value for animation (-1 to 1) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Animation")
	float GetMoveForwardValue() const { return MoveForwardValue; }

	/** Get right movement value for animation (-1 to 1) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Animation")
	float GetMoveRightValue() const { return MoveRightValue; }

	/** Get animation forward value (scaled for sprint) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Animation")
	float GetAnimationForwardValue() const;

	/** Get animation right value (scaled for sprint) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Animation")
	float GetAnimationRightValue() const;

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - WEAPON
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Check if character has a weapon */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Weapon")
	bool HasWeapon() const { return bHasWeapon; }

	/** Set weapon state */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Weapon")
	void SetHasWeapon(bool bNewHasWeapon);

	/** Get current weapon actor */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Weapon")
	AActor* GetCurrentWeaponActor() const { return CurrentWeaponActor.Get(); }

	/** Set current weapon actor */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Weapon")
	void SetCurrentWeaponActor(AActor* WeaponActor);

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - COMPONENTS
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Get first person mesh */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Components")
	USkeletalMeshComponent* GetFirstPersonMesh() const { return Mesh1P; }

	/** Get camera component */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Components")
	UCameraComponent* GetCameraComponent() const { return CameraComponent; }

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - RENDER TARGET (Character Preview for UI)
	// ═══════════════════════════════════════════════════════════════════════════════

	/**
	 * Get render target for character preview display in UI.
	 * Use this to display character in main menu, inventory, etc.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|RenderTarget")
	UTextureRenderTarget2D* GetCharacterRenderTarget() const { return CharacterRenderTarget; }

	/**
	 * Get dynamic material instance created from render target.
	 * Assign this to UImage widget's brush.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|RenderTarget")
	UMaterialInstanceDynamic* GetRenderTargetMaterial() const { return RenderTargetMaterialInstance; }

	/**
	 * Set capture camera location relative to character.
	 * @param RelativeLocation - Location offset from character mesh
	 * @param RelativeRotation - Rotation to look at character
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|RenderTarget")
	void SetCaptureLocation(const FVector& RelativeLocation, const FRotator& RelativeRotation);

	/**
	 * Refresh the render target capture (force update).
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|RenderTarget")
	void RefreshCharacterCapture();

	/**
	 * Enable/disable character capture.
	 * Disable when not in menu to save performance.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|RenderTarget")
	void SetCaptureEnabled(bool bEnabled);

	/** Check if capture is currently enabled */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|RenderTarget")
	bool IsCaptureEnabled() const;

protected:
	// ═══════════════════════════════════════════════════════════════════════════════
	// COMPONENTS
	// ═══════════════════════════════════════════════════════════════════════════════

	/** First person mesh (arms) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SuspenseCore|Components")
	USkeletalMeshComponent* Mesh1P;

	/** Camera boom */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SuspenseCore|Components")
	USpringArmComponent* CameraBoom;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SuspenseCore|Components")
	UCameraComponent* CameraComponent;

	// ═══════════════════════════════════════════════════════════════════════════════
	// RENDER TARGET COMPONENTS (Character Preview)
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Scene capture component for character preview */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SuspenseCore|RenderTarget")
	USceneCaptureComponent2D* CharacterCaptureComponent;

	/** Spring arm for positioning capture camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SuspenseCore|RenderTarget")
	USpringArmComponent* CaptureCameraBoom;

	/** Render target texture for character preview */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "SuspenseCore|RenderTarget")
	UTextureRenderTarget2D* CharacterRenderTarget;

	/** Dynamic material instance for UI display */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "SuspenseCore|RenderTarget")
	UMaterialInstanceDynamic* RenderTargetMaterialInstance;

	// ═══════════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Base walk speed */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Movement")
	float BaseWalkSpeed = 400.0f;

	/** Sprint speed multiplier */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Movement")
	float SprintSpeedMultiplier = 1.5f;

	/** Crouch speed multiplier */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Movement")
	float CrouchSpeedMultiplier = 0.5f;

	/** Animation value interpolation speed */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Animation")
	float AnimationInterpSpeed = 10.0f;

	// ═══════════════════════════════════════════════════════════════════════════════
	// RENDER TARGET CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Render target resolution width */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|RenderTarget")
	int32 RenderTargetWidth = 512;

	/** Render target resolution height */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|RenderTarget")
	int32 RenderTargetHeight = 512;

	/** Distance from character for capture camera */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|RenderTarget")
	float CaptureDistance = 200.0f;

	/** Height offset for capture camera relative to mesh root */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|RenderTarget")
	float CaptureHeightOffset = 80.0f;

	/** Capture camera field of view */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|RenderTarget", meta = (ClampMin = "5.0", ClampMax = "170.0"))
	float CaptureFOV = 30.0f;

	/** Material to use for render target display (must have TextureParameter named 'RenderTargetTexture') */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|RenderTarget")
	UMaterialInterface* RenderTargetBaseMaterial;

	/** Whether to continuously capture or only on-demand */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|RenderTarget")
	bool bContinuousCapture = false;

	// ═══════════════════════════════════════════════════════════════════════════════
	// STATE
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Current movement state */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|State")
	ESuspenseCoreMovementState CurrentMovementState = ESuspenseCoreMovementState::Idle;

	/** Is currently sprinting */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|State")
	bool bIsSprinting = false;

	/** Has weapon equipped */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|State")
	bool bHasWeapon = false;

	/** Current weapon actor (weak reference - weapon can be destroyed) */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CurrentWeaponActor;

	// ═══════════════════════════════════════════════════════════════════════════════
	// ANIMATION VALUES
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Current forward movement value */
	float MoveForwardValue = 0.0f;

	/** Current right movement value */
	float MoveRightValue = 0.0f;

	/** Target forward movement value */
	float TargetMoveForward = 0.0f;

	/** Target right movement value */
	float TargetMoveRight = 0.0f;

	// ═══════════════════════════════════════════════════════════════════════════════
	// INTERNAL
	// ═══════════════════════════════════════════════════════════════════════════════

	void UpdateMovementState();
	void UpdateAnimationValues(float DeltaTime);
	void UpdateMovementSpeed();

	void PublishCharacterEvent(const FGameplayTag& EventTag, const FString& Payload = TEXT(""));
	USuspenseCoreEventBus* GetEventBus() const;

	// Render target initialization
	void InitializeRenderTarget();
	void SetupCaptureComponent();
	void CreateRenderTargetMaterial();

private:
	/** Cached references */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	UPROPERTY()
	TWeakObjectPtr<ASuspenseCorePlayerState> CachedPlayerState;

	/** Movement state tracking */
	ESuspenseCoreMovementState PreviousMovementState = ESuspenseCoreMovementState::Idle;

	/** Is capture currently enabled */
	bool bCaptureEnabled = false;

	/** Has render target been initialized */
	bool bRenderTargetInitialized = false;
};
