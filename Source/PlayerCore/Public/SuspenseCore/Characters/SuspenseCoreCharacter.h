// SuspenseCoreCharacter.h
// Copyright Suspense Team. All Rights Reserved.
// Clean Architecture Character with EventBus integration and Cinematic Camera

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "CineCameraComponent.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCoreCharacter.generated.h"

class USpringArmComponent;
class UAbilitySystemComponent;
class USuspenseCoreEventBus;
class ASuspenseCorePlayerState;
class USuspenseCoreCharacterClassData;

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
 * Clean Architecture Character with EventBus integration and Cinematic Camera
 *
 * DESIGN PRINCIPLES:
 * - ASC lives on PlayerState, not Character
 * - Movement through standard CharacterMovementComponent
 * - EventBus for state notifications
 * - Cinematic camera for first person view (UCineCameraComponent)
 *
 * CAMERA SETUP:
 * - UCineCameraComponent attached to Mesh1P "head" bone
 * - Supports DOF, focal length, aperture, filmback settings
 * - Optional camera lag via SpringArm
 *
 * RESPONSIBILITIES:
 * - Visual representation (mesh, camera)
 * - Movement execution (receives commands from Controller)
 * - Animation state for procedural animations
 * - Weapon attachment points
 * - Character class application
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

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - MOVEMENT
	// ═══════════════════════════════════════════════════════════════════════════════

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Movement")
	void Move(const FVector2D& MovementInput);

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Movement")
	void Look(const FVector2D& LookInput);

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Movement")
	void StartSprinting();

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Movement")
	void StopSprinting();

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Movement")
	void ToggleCrouch();

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - STATE
	// ═══════════════════════════════════════════════════════════════════════════════

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|State")
	ESuspenseCoreMovementState GetMovementState() const { return CurrentMovementState; }

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|State")
	bool IsSprinting() const { return bIsSprinting; }

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|State")
	bool HasMovementInput() const;

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|State")
	ASuspenseCorePlayerState* GetSuspenseCorePlayerState() const;

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - ANIMATION
	// ═══════════════════════════════════════════════════════════════════════════════

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Animation")
	float GetMoveForwardValue() const { return MoveForwardValue; }

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Animation")
	float GetMoveRightValue() const { return MoveRightValue; }

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Animation")
	float GetAnimationForwardValue() const;

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Animation")
	float GetAnimationRightValue() const;

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - WEAPON
	// ═══════════════════════════════════════════════════════════════════════════════

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Weapon")
	bool HasWeapon() const { return bHasWeapon; }

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Weapon")
	void SetHasWeapon(bool bNewHasWeapon);

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Weapon")
	AActor* GetCurrentWeaponActor() const { return CurrentWeaponActor.Get(); }

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Weapon")
	void SetCurrentWeaponActor(AActor* WeaponActor);

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - COMPONENTS
	// ═══════════════════════════════════════════════════════════════════════════════

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Components")
	USkeletalMeshComponent* GetFirstPersonMesh() const { return Mesh1P; }

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Components")
	UCineCameraComponent* GetCineCameraComponent() const { return Camera; }

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Components")
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - CINEMATIC CAMERA CONTROL
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Set camera field of view */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Camera")
	void SetCameraFOV(float NewFOV);

	/** Set camera focal length */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Camera")
	void SetCameraFocalLength(float NewFocalLength);

	/** Set camera aperture (f-stop) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Camera")
	void SetCameraAperture(float NewAperture);

	/** Enable or disable depth of field */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Camera")
	void SetDepthOfFieldEnabled(bool bEnabled);

	/** Set manual focus distance */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Camera")
	void SetCameraFocusDistance(float Distance);

	/** Apply cinematic preset (for cutscenes or special moments) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Camera")
	void ApplyCinematicPreset(bool bEnableDOF, float Aperture, float FocusDistance);

	/** Reset camera to default gameplay settings */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Camera")
	void ResetCameraToDefaults();

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - CHARACTER CLASS
	// ═══════════════════════════════════════════════════════════════════════════════

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CharacterClass")
	void ApplyCharacterClass(USuspenseCoreCharacterClassData* ClassData);

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CharacterClass")
	USuspenseCoreCharacterClassData* GetAppliedCharacterClass() const { return AppliedClassData; }

protected:
	// ═══════════════════════════════════════════════════════════════════════════════
	// COMPONENTS
	// ═══════════════════════════════════════════════════════════════════════════════

	/** First person mesh (arms) - attached to main mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SuspenseCore|Components")
	USkeletalMeshComponent* Mesh1P;

	/** Camera boom for optional camera lag/smoothing */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SuspenseCore|Components")
	USpringArmComponent* CameraBoom;

	/** First person cinematic camera - attached to Mesh1P "head" bone */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SuspenseCore|Components")
	UCineCameraComponent* Camera;

	// ═══════════════════════════════════════════════════════════════════════════════
	// CONFIGURATION - MOVEMENT
	// ═══════════════════════════════════════════════════════════════════════════════

	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Movement")
	float BaseWalkSpeed = 400.0f;

	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Movement")
	float SprintSpeedMultiplier = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Movement")
	float CrouchSpeedMultiplier = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Animation")
	float AnimationInterpSpeed = 10.0f;

	// ═══════════════════════════════════════════════════════════════════════════════
	// CONFIGURATION - CAMERA LAG
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Enable camera position lag */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SuspenseCore|Camera|Lag")
	bool bEnableCameraLag = true;

	/** Enable camera rotation lag */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SuspenseCore|Camera|Lag", meta = (EditCondition = "bEnableCameraLag"))
	bool bEnableCameraRotationLag = true;

	/** Camera position lag speed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SuspenseCore|Camera|Lag", meta = (EditCondition = "bEnableCameraLag"))
	float CameraLagSpeed = 15.0f;

	/** Camera rotation lag speed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SuspenseCore|Camera|Lag", meta = (EditCondition = "bEnableCameraRotationLag"))
	float CameraRotationLagSpeed = 10.0f;

	/** Maximum camera lag distance */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SuspenseCore|Camera|Lag", meta = (EditCondition = "bEnableCameraLag"))
	float CameraLagMaxDistance = 20.0f;

	// ═══════════════════════════════════════════════════════════════════════════════
	// CONFIGURATION - CINEMATIC CAMERA LENS
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Field of view in degrees */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SuspenseCore|Camera|Lens", meta = (ClampMin = "5.0", ClampMax = "170.0"))
	float CinematicFieldOfView = 90.0f;

	/** Current focal length of the camera */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SuspenseCore|Camera|Lens", meta = (ClampMin = "4.0", ClampMax = "1000.0"))
	float CurrentFocalLength = 35.0f;

	/** Current aperture (f-stop), affects DOF */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SuspenseCore|Camera|DOF", meta = (ClampMin = "0.7", ClampMax = "32.0"))
	float CurrentAperture = 2.8f;

	/** Number of diaphragm blades (affects bokeh shape) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SuspenseCore|Camera|Lens", meta = (ClampMin = "4", ClampMax = "16"))
	int32 DiaphragmBladeCount = 7;

	// ═══════════════════════════════════════════════════════════════════════════════
	// CONFIGURATION - DEPTH OF FIELD
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Enable depth of field effect */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SuspenseCore|Camera|DOF")
	bool bEnableDepthOfField = false;

	/** Manual focus distance */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SuspenseCore|Camera|DOF", meta = (EditCondition = "bEnableDepthOfField"))
	float ManualFocusDistance = 1000.0f;

	/** Enable smooth focus transitions */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SuspenseCore|Camera|DOF")
	bool bSmoothFocusChanges = true;

	/** Speed of focus transitions */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SuspenseCore|Camera|DOF", meta = (EditCondition = "bSmoothFocusChanges"))
	float FocusSmoothingSpeed = 8.0f;

	// ═══════════════════════════════════════════════════════════════════════════════
	// CONFIGURATION - FILMBACK/SENSOR
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Sensor width in mm (affects FOV and DOF calculations) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SuspenseCore|Camera|Sensor")
	float SensorWidth = 24.89f;

	/** Sensor height in mm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SuspenseCore|Camera|Sensor")
	float SensorHeight = 18.67f;

	// ═══════════════════════════════════════════════════════════════════════════════
	// STATE
	// ═══════════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|State")
	ESuspenseCoreMovementState CurrentMovementState = ESuspenseCoreMovementState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|State")
	bool bIsSprinting = false;

	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|State")
	bool bHasWeapon = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CurrentWeaponActor;

	UPROPERTY(Transient)
	USuspenseCoreCharacterClassData* AppliedClassData;

	// ═══════════════════════════════════════════════════════════════════════════════
	// ANIMATION VALUES
	// ═══════════════════════════════════════════════════════════════════════════════

	float MoveForwardValue = 0.0f;
	float MoveRightValue = 0.0f;
	float TargetMoveForward = 0.0f;
	float TargetMoveRight = 0.0f;

	// ═══════════════════════════════════════════════════════════════════════════════
	// INTERNAL METHODS
	// ═══════════════════════════════════════════════════════════════════════════════

	void UpdateMovementState();
	void UpdateAnimationValues(float DeltaTime);
	void UpdateMovementSpeed();

	void SetupCameraSettings();
	void ApplyCameraLagSettings();
	void ApplyCinematicCameraSettings();

	void PublishCharacterEvent(const FGameplayTag& EventTag, const FString& Payload = TEXT(""));
	void PublishCameraEvent(const FGameplayTag& EventTag, float Value = 0.0f);
	USuspenseCoreEventBus* GetEventBus() const;

	void LoadCharacterClassFromSubsystem();

private:
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	UPROPERTY()
	TWeakObjectPtr<ASuspenseCorePlayerState> CachedPlayerState;

	ESuspenseCoreMovementState PreviousMovementState = ESuspenseCoreMovementState::Idle;
};
