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
#include "SuspenseCore/SuspenseCoreInterfaces.h"
#include "SuspenseCore/Interfaces/Core/ISuspenseCoreCharacter.h"
#include "SuspenseCoreCharacter.generated.h"

class USpringArmComponent;
class UAbilitySystemComponent;
class USuspenseCoreEventBus;
class ASuspenseCorePlayerState;
class USuspenseCoreCharacterClassData;
class USuspenseCoreWeaponStanceComponent;
class USuspenseCoreQuickSlotComponent;

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
// CAMERA ATTACHMENT MODE
// ═══════════════════════════════════════════════════════════════════════════════

UENUM(BlueprintType)
enum class ESuspenseCoreCameraAttachMode : uint8
{
	/** Attach to CameraBoom - stable FPS, no head bob */
	CameraBoom		UMETA(DisplayName = "Camera Boom (Stable)"),

	/** Attach to MetaHuman Face component by name */
	MetaHumanFace	UMETA(DisplayName = "MetaHuman Face"),

	/** Attach to MetaHuman Body SkeletalMesh by name */
	MetaHumanBody	UMETA(DisplayName = "MetaHuman Body"),

	/** Attach to component found by name */
	ComponentByName	UMETA(DisplayName = "Component By Name"),

	/** Attach to component found by tag */
	ComponentByTag	UMETA(DisplayName = "Component By Tag"),

	/** Attach to Mesh1P (legacy first person arms) */
	Mesh1P			UMETA(DisplayName = "Mesh1P (Legacy)")
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
 * - Event publishing via ISuspenseCoreEventEmitter
 */
UCLASS()
class PLAYERCORE_API ASuspenseCoreCharacter
	: public ACharacter
	, public IAbilitySystemInterface
	, public ISuspenseCoreEventEmitter
	, public ISuspenseCoreMovementInterface
	, public ISuspenseCoreADSCamera
	, public ISuspenseCoreCharacterInterface
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
	// ISUSPENSECOREEVENTEMITTER INTERFACE
	// ═══════════════════════════════════════════════════════════════════════════════

	virtual void EmitEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& Data) override;
	virtual USuspenseCoreEventBus* GetEventBus() const override;

	// ═══════════════════════════════════════════════════════════════════════════════
	// ISUSPENSECOREMOVEMENTINTERFACE
	// ═══════════════════════════════════════════════════════════════════════════════

	virtual bool MovementCanSprint_Implementation() const override;
	virtual bool MovementIsSprinting_Implementation() const override { return bIsSprinting; }
	virtual void MovementStartSprint_Implementation() override;
	virtual void MovementStopSprint_Implementation() override;

	virtual bool MovementCanCrouch_Implementation() const override;
	virtual bool MovementIsCrouching_Implementation() const override { return bIsCrouched; }
	virtual void MovementStartCrouch_Implementation() override;
	virtual void MovementStopCrouch_Implementation() override;

	virtual bool MovementCanJump_Implementation() const override;
	virtual bool MovementIsInAir_Implementation() const override;

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
	// ISUSPENSECORECHARACTERINTERFACE IMPLEMENTATION
	// Provides weapon management API via interface (BlueprintCallable in interface)
	// ═══════════════════════════════════════════════════════════════════════════════

	virtual void SetHasWeapon_Implementation(bool bNewHasWeapon) override;
	virtual void SetCurrentWeaponActor_Implementation(AActor* WeaponActor) override;
	virtual AActor* GetCurrentWeaponActor_Implementation() const override { return CurrentWeaponActor.Get(); }
	virtual bool HasWeapon_Implementation() const override { return bHasWeapon; }
	virtual UAbilitySystemComponent* GetASC_Implementation() const override { return GetAbilitySystemComponent(); }
	virtual float GetCharacterLevel_Implementation() const override { return 1.0f; }
	virtual bool IsAlive_Implementation() const override { return !IsPendingKillPending(); }
	virtual int32 GetTeamId_Implementation() const override { return 0; }
	virtual USuspenseCoreEventManager* GetDelegateManager() const override { return nullptr; }

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - COMPONENTS
	// ═══════════════════════════════════════════════════════════════════════════════

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Components")
	USkeletalMeshComponent* GetFirstPersonMesh() const { return Mesh1P; }

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Components")
	UCineCameraComponent* GetCineCameraComponent() const { return Camera; }

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Components")
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Get interaction component (nullptr if WITH_INTERACTION_SYSTEM=0) */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Components")
	UActorComponent* GetInteractionComponent() const { return InteractionComponent; }

	/** Get QuickSlot component for Tarkov-style magazine access (nullptr if WITH_EQUIPMENT_SYSTEM=0) */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Components")
	USuspenseCoreQuickSlotComponent* GetQuickSlotComponent() const { return QuickSlotComponent; }

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - EQUIPMENT ATTACHMENT (MetaHuman Support)
	// ═══════════════════════════════════════════════════════════════════════════════

	/**
	 * Get the skeletal mesh component used for equipment attachment.
	 * For MetaHuman characters, this returns the "Body" component which has the sockets.
	 * For legacy characters, this returns GetMesh().
	 *
	 * Override in Blueprint to customize equipment attachment target.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment")
	USkeletalMeshComponent* GetEquipmentAttachMesh() const;

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
	// ISUSPENSECOREADSСAMERA INTERFACE
	// ═══════════════════════════════════════════════════════════════════════════════

	virtual void ADSSwitchCamera_Implementation(bool bToScopeCam) override;
	virtual bool ADSIsInScopeView_Implementation() const override { return bIsInScopeView; }
	virtual float ADSGetCurrentFOV_Implementation() const override;

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - ADS CAMERA SWITCHING (Legacy compatibility)
	// ═══════════════════════════════════════════════════════════════════════════════

	/**
	 * Switch between first person camera and weapon's scope camera for ADS.
	 * Uses SetViewTargetWithBlend for smooth cubic transition.
	 * Note: Prefer using ISuspenseCoreADSCamera interface from GAS abilities.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Camera|ADS")
	void SwitchToScopeCamera(bool bToScopeCam, float TransitionDuration = 0.2f, float TargetFOV = 60.0f);

	/**
	 * Check if currently viewing through scope camera.
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Camera|ADS")
	bool IsInScopeView() const { return bIsInScopeView; }

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

	/** Interaction component for interacting with world objects (nullptr if WITH_INTERACTION_SYSTEM=0) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SuspenseCore|Components")
	UActorComponent* InteractionComponent = nullptr;

	/** Weapon stance component for animation state (nullptr if WITH_EQUIPMENT_SYSTEM=0) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SuspenseCore|Components")
	USuspenseCoreWeaponStanceComponent* WeaponStanceComponent = nullptr;

	/** QuickSlot component for Tarkov-style magazine access (nullptr if WITH_EQUIPMENT_SYSTEM=0) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SuspenseCore|Components")
	USuspenseCoreQuickSlotComponent* QuickSlotComponent = nullptr;

	// ═══════════════════════════════════════════════════════════════════════════════
	// CONFIGURATION - MOVEMENT (Tarkov-style)
	// Default = slow tactical walk, Sprint = fast run, Crouch = very slow
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Walk speed in cm/s (slow tactical walk, default movement) */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Movement")
	float WalkSpeed = 200.0f;

	/** Sprint speed in cm/s (fast run when holding sprint) */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Movement")
	float SprintSpeed = 600.0f;

	/** Crouch speed in cm/s (very slow, sneaking) */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Movement")
	float CrouchSpeed = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Animation")
	float AnimationInterpSpeed = 10.0f;

	// ═══════════════════════════════════════════════════════════════════════════════
	// CONFIGURATION - CAMERA ATTACHMENT (MetaHuman Support)
	// ═══════════════════════════════════════════════════════════════════════════════

	/**
	 * Camera attachment mode:
	 * - CameraBoom: Stable FPS, no head bob (default)
	 * - MetaHumanFace: Auto-find MetaHuman Face component and attach to "head" bone
	 * - MetaHumanBody: Auto-find MetaHuman Body SkeletalMesh and attach to "head" bone
	 * - ComponentByName: Search component by name (CameraAttachComponentName)
	 * - ComponentByTag: Search component by tag (CameraAttachComponentTag)
	 * - Mesh1P: Legacy attachment to first person arms mesh
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Camera|Attachment")
	ESuspenseCoreCameraAttachMode CameraAttachMode = ESuspenseCoreCameraAttachMode::CameraBoom;

	/**
	 * Component name to search for when CameraAttachMode is ComponentByName.
	 * For MetaHuman, typical names: "Face", "Body", "SkeletalMesh"
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Camera|Attachment",
		meta = (EditCondition = "CameraAttachMode == ESuspenseCoreCameraAttachMode::ComponentByName"))
	FName CameraAttachComponentName = FName("Face");

	/**
	 * Component tag to search for when CameraAttachMode is ComponentByTag.
	 * Add this tag to your component in Blueprint.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Camera|Attachment",
		meta = (EditCondition = "CameraAttachMode == ESuspenseCoreCameraAttachMode::ComponentByTag"))
	FName CameraAttachComponentTag = FName("CameraAttach");

	/**
	 * Socket/bone name to attach camera to on the target component.
	 * For MetaHuman, use "head" bone.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Camera|Attachment",
		meta = (EditCondition = "CameraAttachMode != ESuspenseCoreCameraAttachMode::CameraBoom"))
	FName CameraAttachSocketName = FName("head");

	/**
	 * Camera offset from attachment point.
	 * Adjust for proper eye-level positioning.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Camera|Attachment")
	FVector CameraAttachOffset = FVector::ZeroVector;

	/**
	 * Camera rotation offset from attachment point.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Camera|Attachment")
	FRotator CameraAttachRotation = FRotator::ZeroRotator;

	// ═══════════════════════════════════════════════════════════════════════════════
	// CONFIGURATION - CAMERA BOOM (Spring Arm)
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Spring arm length (distance from attachment point). 0 = first person. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Camera|Boom")
	float CameraBoomArmLength = 0.0f;

	/** Spring arm offset from attachment point */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Camera|Boom")
	FVector CameraBoomOffset = FVector::ZeroVector;

	/** Spring arm rotation offset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Camera|Boom")
	FRotator CameraBoomRotation = FRotator::ZeroRotator;

	/** Enable collision test on spring arm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Camera|Boom")
	bool bCameraBoomCollisionTest = false;

	// ═══════════════════════════════════════════════════════════════════════════════
	// CONFIGURATION - CAMERA LAG
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Enable camera position lag */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Camera|Lag")
	bool bEnableCameraLag = false;

	/** Enable camera rotation lag */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Camera|Lag", meta = (EditCondition = "bEnableCameraLag"))
	bool bEnableCameraRotationLag = false;

	/** Camera position lag speed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Camera|Lag", meta = (EditCondition = "bEnableCameraLag"))
	float CameraLagSpeed = 15.0f;

	/** Camera rotation lag speed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Camera|Lag", meta = (EditCondition = "bEnableCameraRotationLag"))
	float CameraRotationLagSpeed = 10.0f;

	/** Maximum camera lag distance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Camera|Lag", meta = (EditCondition = "bEnableCameraLag"))
	float CameraLagMaxDistance = 20.0f;

	// ═══════════════════════════════════════════════════════════════════════════════
	// CONFIGURATION - CINEMATIC CAMERA LENS
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Field of view in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Camera|Lens", meta = (ClampMin = "5.0", ClampMax = "170.0"))
	float CinematicFieldOfView = 90.0f;

	/** Current focal length of the camera */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Camera|Lens", meta = (ClampMin = "4.0", ClampMax = "1000.0"))
	float CurrentFocalLength = 35.0f;

	/** Current aperture (f-stop), affects DOF */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Camera|DOF", meta = (ClampMin = "0.7", ClampMax = "32.0"))
	float CurrentAperture = 2.8f;

	/** Number of diaphragm blades (affects bokeh shape) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Camera|Lens", meta = (ClampMin = "4", ClampMax = "16"))
	int32 DiaphragmBladeCount = 7;

	// ═══════════════════════════════════════════════════════════════════════════════
	// CONFIGURATION - DEPTH OF FIELD
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Enable depth of field effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Camera|DOF")
	bool bEnableDepthOfField = false;

	/** Manual focus distance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Camera|DOF", meta = (EditCondition = "bEnableDepthOfField"))
	float ManualFocusDistance = 1000.0f;

	/** Enable smooth focus transitions */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Camera|DOF")
	bool bSmoothFocusChanges = true;

	/** Speed of focus transitions */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Camera|DOF", meta = (EditCondition = "bSmoothFocusChanges"))
	float FocusSmoothingSpeed = 8.0f;

	// ═══════════════════════════════════════════════════════════════════════════════
	// CONFIGURATION - FILMBACK/SENSOR
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Sensor width in mm (affects FOV and DOF calculations) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Camera|Sensor")
	float SensorWidth = 24.89f;

	/** Sensor height in mm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Camera|Sensor")
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

	/** Whether currently viewing through weapon's scope camera (ADS) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|State")
	bool bIsInScopeView = false;

	// ═══════════════════════════════════════════════════════════════════════════════
	// SMOOTH CROUCH
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Speed of crouch transition interpolation (higher = faster) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Movement|Crouch", meta = (ClampMin = "1.0", ClampMax = "30.0"))
	float CrouchInterpSpeed = 12.0f;

	/** Current interpolated crouch alpha (0 = standing, 1 = fully crouched) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement|Crouch")
	float CurrentCrouchAlpha = 0.0f;

	/** Target crouch alpha we're interpolating towards */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement|Crouch")
	float TargetCrouchAlpha = 0.0f;

	/** Cached standing capsule half-height */
	UPROPERTY(Transient)
	float StandingCapsuleHalfHeight = 0.0f;

	/** Cached crouched capsule half-height */
	UPROPERTY(Transient)
	float CrouchedCapsuleHalfHeight = 0.0f;

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

	/**
	 * Camera Shake offset - процедурный шейк камеры.
	 * Устанавливается из Blueprint при определённых состояниях (прыжок, приземление и т.д.)
	 * ABP интерполирует это значение для плавного эффекта.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "SuspenseCore|Animation|CameraShake")
	FVector CameraShake = FVector::ZeroVector;

	// ═══════════════════════════════════════════════════════════════════════════════
	// INTERNAL METHODS
	// ═══════════════════════════════════════════════════════════════════════════════

	void UpdateMovementState();
	void UpdateAnimationValues(float DeltaTime);
	void UpdateMovementSpeed();
	void UpdateSmoothCrouch(float DeltaTime);
	void InitializeCrouchHeights();

	void SetupCameraSettings();
	void SetupCameraAttachment();
	void ApplyCameraLagSettings();
	void ApplyCinematicCameraSettings();

	/** Find MetaHuman Face component (searches by name "Face") */
	USceneComponent* FindMetaHumanFaceComponent() const;

	/** Find MetaHuman Body SkeletalMesh component */
	USceneComponent* FindMetaHumanBodyComponent() const;

	/** Hide MetaHuman Face from owner but keep shadow (bCastHiddenShadow) */
	void HideMetaHumanFaceFromOwner();

	/**
	 * Toggle Face and head components visibility for ADS mode.
	 * When entering ADS, hides Face/Hair/Beard etc so scope camera doesn't see them.
	 * When exiting ADS, restores visibility.
	 * @param bVisible - true to show face components, false to hide them
	 */
	void SetFaceVisibilityForADS(bool bVisible);

	/** Find component by name for camera attachment */
	USceneComponent* FindComponentByName(FName ComponentName) const;

	/** Find component by tag for camera attachment */
	USceneComponent* FindComponentByTag(FName Tag) const;

	void PublishCharacterEvent(const FGameplayTag& EventTag, const FString& Payload = TEXT(""));
	void PublishCameraEvent(const FGameplayTag& EventTag, float Value = 0.0f);

	void LoadCharacterClassFromSubsystem();

private:
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	UPROPERTY()
	TWeakObjectPtr<ASuspenseCorePlayerState> CachedPlayerState;

	ESuspenseCoreMovementState PreviousMovementState = ESuspenseCoreMovementState::Idle;
};
