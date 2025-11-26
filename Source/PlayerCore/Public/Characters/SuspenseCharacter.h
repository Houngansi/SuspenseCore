// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interfaces/Core/ISuspenseCharacter.h"
#include "Interfaces/Core/ISuspenseMovement.h"
#include "CineCameraComponent.h"
#include "Characters/SuspenseCharacterMovementComponent.h"
#include "SuspenseCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UAbilitySystemComponent;
class UEventDelegateManager;
class UCharacterMovementComponent;

UCLASS()
class SUSPENSECORE_API ASuspenseCharacter : public ACharacter, public ISuspenseCharacter, public ISuspenseMovement
{
    GENERATED_BODY()
public:
    ASuspenseCharacter(const FObjectInitializer&);

    // ========================================
    // ISuspenseCharacter Implementation
    // ========================================
    virtual UAbilitySystemComponent* GetASC_Implementation() const override;
    virtual void SetHasWeapon_Implementation(bool bHasWeapon) override;
    virtual void SetCurrentWeaponActor_Implementation(AActor* WeaponActor) override;
    virtual AActor* GetCurrentWeaponActor_Implementation() const override;
    virtual bool HasWeapon_Implementation() const override;
    virtual float GetCharacterLevel_Implementation() const override;
    virtual bool IsAlive_Implementation() const override;
    virtual int32 GetTeamId_Implementation() const override;

    // Pure virtual method from ISuspenseCharacter
    virtual USuspenseEventManager* GetDelegateManager() const override;

    // ========================================
    // ISuspenseMovement Implementation
    // ========================================
    
    // Speed Management
    virtual float GetCurrentMovementSpeed_Implementation() const override;
    virtual void SetMovementSpeed_Implementation(float NewSpeed) override;
    virtual float GetDefaultMovementSpeed_Implementation() const override;
    virtual float GetMaxWalkSpeed_Implementation() const override;
    
    // Sprint Management
    virtual bool CanSprint_Implementation() const override;
    virtual bool IsSprinting_Implementation() const override;
    virtual void StartSprinting_Implementation() override;
    virtual void StopSprinting_Implementation() override;
    
    // Jump Management
    virtual void Jump_Implementation() override;
    virtual void StopJumping_Implementation() override;
    virtual bool CanJump_Implementation() const override;
    virtual bool IsGrounded_Implementation() const override;
    virtual bool IsFalling_Implementation() const override;
    virtual float GetJumpZVelocity_Implementation() const override;
    virtual void SetJumpZVelocity_Implementation(float NewJumpZVelocity) override;
    
    // Crouch Management
    virtual void Crouch_Implementation() override;
    virtual void UnCrouch_Implementation() override;
    virtual bool CanCrouch_Implementation() const override;
    virtual bool IsCrouching_Implementation() const override;
    virtual float GetCrouchedHalfHeight_Implementation() const override;
    
    // State Management
    virtual FGameplayTag GetMovementState_Implementation() const override;
    virtual void SetMovementState_Implementation(FGameplayTag NewState) override;
    virtual FGameplayTagContainer GetActiveMovementTags_Implementation() const override;
    virtual bool HasMovementTag_Implementation(FGameplayTag Tag) const override;
    
    // Physics & Environment
    virtual bool IsSwimming_Implementation() const override;
    virtual bool IsFlying_Implementation() const override;
    virtual FVector GetVelocity_Implementation() const override;
    virtual FVector GetGroundNormal_Implementation() const override;
    
    // ========================================
    // Movement Methods
    // ========================================
    void Move(const FVector2D& Value);
    void Look(const FVector2D& Value);
    
    /** Get custom movement component */
    UFUNCTION(BlueprintCallable, Category = "Movement")
    class USuspenseCharacterMovementComponent* GetSuspenseMovementComponent() const;

    // ========================================
    // Movement Input Values
    // ========================================

    /** Get forward movement input value (-1 to 1) */
    UFUNCTION(BlueprintCallable, Category = "Movement|Input")
    float GetMoveForwardValue() const { return MoveForwardValue; }

    /** Get right movement input value (-1 to 1) */
    UFUNCTION(BlueprintCallable, Category = "Movement|Input")
    float GetMoveRightValue() const { return MoveRightValue; }

    // ========================================
    // Animation Input Values - ДЛЯ АНИМАЦИЙ
    // ========================================
    
    /** 
     * Get forward value for animations (modified by sprint state)
     * Returns: -2 to 2 when sprinting, -1 to 1 when walking
     */
    UFUNCTION(BlueprintCallable, Category = "Animation|Input", meta = (DisplayName = "Get Animation Forward Value"))
    float GetAnimationForwardValue() const;
    
    /** 
     * Get right value for animations (modified by sprint state)
     * Returns: -2 to 2 when sprinting, -1 to 1 when walking
     */
    UFUNCTION(BlueprintCallable, Category = "Animation|Input", meta = (DisplayName = "Get Animation Right Value"))
    float GetAnimationRightValue() const;
    
    /**
     * Check if character is currently sprinting (source of truth)
     */
    UFUNCTION(BlueprintCallable, Category = "Movement|Sprint")
    bool IsCharacterSprinting() const;
    
    /** Get combined movement speed (0 to ~1.414 for diagonal) */
    UFUNCTION(BlueprintCallable, Category = "Movement|Input")
    float GetMovementInputSpeed() const;

    /** Check if character has any movement input */
    UFUNCTION(BlueprintCallable, Category = "Movement|Input")
    bool HasMovementInput() const;

    /** Get movement input vector in world space */
    UFUNCTION(BlueprintCallable, Category = "Movement|Input")
    FVector GetMovementInputVector() const;

    UFUNCTION(BlueprintCallable, Category = "Animation|Movement")
    bool IsMoving() const;
    
    // ========================================
    // Animation State Getters
    // ========================================

    /** Get if character is jumping for animation */
    UFUNCTION(BlueprintCallable, Category = "Animation|States")
    bool GetIsJumping() const;

    /** Get if character is in air for animation */
    UFUNCTION(BlueprintCallable, Category = "Animation|States")
    bool GetIsInAir() const;

    /** Get if character is crouching for animation */
    UFUNCTION(BlueprintCallable, Category = "Animation|States")
    bool GetIsCrouching() const;

    /** Get if character is sliding for animation */
    UFUNCTION(BlueprintCallable, Category = "Animation|States")
    bool GetIsSliding() const;

    /** Get if character is sprinting for animation */
    UFUNCTION(BlueprintCallable, Category = "Animation|States")
    bool GetIsSprinting() const;

    /** Get current movement mode enum for animation */
    UFUNCTION(BlueprintCallable, Category = "Animation|States")
    ESuspenseMovementMode GetMovementMode() const;

    // ========================================
    // Movement Actions
    // ========================================

    /** Start sliding action */
    UFUNCTION(BlueprintCallable, Category = "Movement")
    void StartSliding();

    /** Stop sliding action */
    UFUNCTION(BlueprintCallable, Category = "Movement")
    void StopSliding();

    // ========================================
    // Procedural Animation
    // ========================================
    
    /** Updates procedural animation values based on movement and state */
    UFUNCTION(BlueprintCallable, Category = "Animation|Procedural")
    void UpdateProceduralAnimationValues(float DeltaTime);

    /** Get current lean sides amount */
    UFUNCTION(BlueprintCallable, Category = "Animation|Procedural")
    float GetLeanSidesAmount() const { return LeanSidesAmount; }
    
    /** Set lean sides amount */
    UFUNCTION(BlueprintCallable, Category = "Animation|Procedural")
    void SetLeanSidesAmount(float NewValue) { LeanSidesAmount = NewValue; }
    
    /** Get current look up/down amount */
    UFUNCTION(BlueprintCallable, Category = "Animation|Procedural")
    float GetLookUpAmount() const { return LookUpAmount; }
    
    /** Set look up/down amount */
    UFUNCTION(BlueprintCallable, Category = "Animation|Procedural")
    void SetLookUpAmount(float NewValue) { LookUpAmount = NewValue; }
    
    /** Get current arm animation weight multiplier */
    UFUNCTION(BlueprintCallable, Category = "Animation|Procedural")
    float GetArmGroupAnimationWeightMultiplier() const { return ArmGroupAnimationWeightMultiplier; }
    
    /** Set arm animation weight multiplier */
    UFUNCTION(BlueprintCallable, Category = "Animation|Procedural")
    void SetArmGroupAnimationWeightMultiplier(float NewValue) { ArmGroupAnimationWeightMultiplier = NewValue; }

    /** Get vertical recoil amount */
    UFUNCTION(BlueprintCallable, Category = "Animation|Procedural")
    float GetVerticalRecoilAmount() const { return VerticalRecoilAmount; }
    
    /** Set vertical recoil amount */
    UFUNCTION(BlueprintCallable, Category = "Animation|Procedural")
    void SetVerticalRecoilAmount(float NewValue) { VerticalRecoilAmount = NewValue; }

    /** Get horizontal recoil amount */
    UFUNCTION(BlueprintCallable, Category = "Animation|Procedural")
    float GetHorizontalRecoilAmount() const { return HorizontalRecoilAmount; }
    
    /** Set horizontal recoil amount */
    UFUNCTION(BlueprintCallable, Category = "Animation|Procedural")
    void SetHorizontalRecoilAmount(float NewValue) { HorizontalRecoilAmount = NewValue; }
    
    // ========================================
    // Cinematic Camera Control
    // ========================================

    /** Set camera field of view */
    UFUNCTION(BlueprintCallable, Category = "Camera|Cinematic")
    void SetCameraFOV(float NewFOV);

    /** Set camera focal length */
    UFUNCTION(BlueprintCallable, Category = "Camera|Cinematic")
    void SetCameraFocalLength(float NewFocalLength);

    /** Set camera aperture (f-stop) */
    UFUNCTION(BlueprintCallable, Category = "Camera|Cinematic")
    void SetCameraAperture(float NewAperture);

    /** Enable or disable depth of field */
    UFUNCTION(BlueprintCallable, Category = "Camera|Cinematic")
    void SetDepthOfFieldEnabled(bool bEnabled);

    /** Set manual focus distance */
    UFUNCTION(BlueprintCallable, Category = "Camera|Cinematic")
    void SetCameraFocusDistance(float Distance);

    /** Apply cinematic preset (for cutscenes or special moments) */
    UFUNCTION(BlueprintCallable, Category = "Camera|Cinematic")
    void ApplyCinematicPreset(bool bEnableDOF, float Aperture, float FocusDistance);
    
    // ========================================
    // Public Mesh Access
    // ========================================
    
    /** Get first person mesh component */
    UFUNCTION(BlueprintPure, Category = "Mesh")
    USkeletalMeshComponent* GetFirstPersonMesh() const { return Mesh1P; }

protected:
    virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    
    // Override base Jump and StopJumping to route through interface
    virtual void Jump() override;
    virtual void StopJumping() override;

    /** First person mesh (arms) - visible only to owner */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh") 
    USkeletalMeshComponent* Mesh1P;
    
    /** Camera boom for smooth camera movement */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera") 
    USpringArmComponent* CameraBoom;
    
    /** First person cinematic camera */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera") 
    UCineCameraComponent* Camera;

    // ========================================
    // Camera Settings
    // ========================================
    
    /** Camera lag settings for smooth movement */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera|Lag")
    bool bEnableCameraLag = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera|Lag", meta = (EditCondition = "bEnableCameraLag"))
    bool bEnableCameraRotationLag = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera|Lag", meta = (EditCondition = "bEnableCameraLag"))
    float CameraLagSpeed = 15.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera|Lag", meta = (EditCondition = "bEnableCameraRotationLag"))
    float CameraRotationLagSpeed = 10.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera|Lag", meta = (EditCondition = "bEnableCameraLag"))
    float CameraLagMaxDistance = 20.f;
    
    // ========================================
    // Cinematic Camera Settings
    // ========================================

    /** Camera lens settings */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera|Cinematic|Lens")
    FName DefaultLensPresetName = TEXT("35mm Anamorphic (1.33x Squeeze)");

    /** Field of view in degrees */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera|Cinematic|Lens", meta = (ClampMin = "5.0", ClampMax = "170.0"))
    float CinematicFieldOfView = 90.0f;

    /** Current focal length of the camera */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera|Cinematic|Lens", meta = (ClampMin = "4.0", ClampMax = "1000.0"))
    float CurrentFocalLength = 35.0f;

    /** Current aperture, affects depth of field */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera|Cinematic|DOF", meta = (ClampMin = "0.7", ClampMax = "32.0"))
    float CurrentAperture = 2.8f;

    /** Controls depth of field blur amount */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera|Cinematic|DOF")
    bool bEnableDepthOfField = false;

    /** Focus distance for depth of field effect */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera|Cinematic|DOF", meta = (EditCondition = "bEnableDepthOfField"))
    float ManualFocusDistance = 1000.0f;

    /** Number of diaphragm blades (affects bokeh shape) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera|Cinematic|Lens", meta = (ClampMin = "4", ClampMax = "16"))
    int32 DiaphragmBladeCount = 7;

    /** Enable smooth focus transitions */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera|Cinematic|Focus")
    bool bSmoothFocusChanges = true;

    /** Speed of focus transitions */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera|Cinematic|Focus", meta = (EditCondition = "bSmoothFocusChanges"))
    float FocusSmoothingSpeed = 8.0f;

    /** Sensor width in mm (affects field of view and depth of field calculations) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera|Cinematic|Sensor")
    float SensorWidth = 24.89f;

    /** Sensor height in mm */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera|Cinematic|Sensor")
    float SensorHeight = 18.67f;
    
    // ========================================
    // Procedural Animation Variables
    // ========================================
    
    /** Lean sides amount for procedural animation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation|Procedural")
    float LeanSidesAmount = 0.f;
    
    /** Look up/down amount for procedural animation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation|Procedural")
    float LookUpAmount = 0.f;
    
    /** Arm animation weight multiplier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation|Procedural")
    float ArmGroupAnimationWeightMultiplier = 1.f;

    /** Lean strength multiplier */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Animation|Procedural")
    float LeanStrengthMultiplier = 1.0f;
    
    /** Look up/down strength multiplier */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Animation|Procedural")
    float LookUpStrengthMultiplier = 1.0f;
    
    /** Maximum lean angle (degrees) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Animation|Procedural")
    float MaxLeanAngle = 10.0f;
    
    /** Maximum look up/down angle (degrees) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Animation|Procedural")
    float MaxLookUpAngle = 15.0f;
    
    /** Procedural animation interpolation speed */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Animation|Procedural")
    float ProceduralAnimInterpolationSpeed = 5.0f;

    /** Vertical recoil strength multiplier */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Animation|Procedural")
    float VerticalRecoilStrengthMultiplier = 0.8f;
    
    /** Maximum vertical recoil angle */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Animation|Procedural")
    float MaxVerticalRecoilAngle = 5.0f;

    /** Horizontal recoil strength multiplier */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Animation|Procedural")
    float HorizontalRecoilStrengthMultiplier = 0.8f;
    
    /** Maximum horizontal recoil angle */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Animation|Procedural")
    float MaxHorizontalRecoilAngle = 5.0f;

    /** Vertical recoil amount for animation */
    UPROPERTY(BlueprintReadOnly, Category = "Animation|Procedural")
    float VerticalRecoilAmount = 0.f;
    
    /** Horizontal recoil amount for animation */
    UPROPERTY(BlueprintReadOnly, Category = "Animation|Procedural")
    float HorizontalRecoilAmount = 0.f;

    // ========================================
    // Weapon State
    // ========================================
    
    /** Does this character currently have a weapon equipped */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Weapon")
    bool bHasWeapon = false;
    
    /** Currently equipped weapon actor */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Weapon")
    AActor* CurrentWeaponActor = nullptr;

    /** Cached reference to custom movement component */
    UPROPERTY()
    class USuspenseCharacterMovementComponent* SuspenseMovementComponent;

    /** Current movement state tag */
    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    FGameplayTag CurrentMovementState;
    
    /** Active movement tags container */
    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    FGameplayTagContainer ActiveMovementTags;
    
private:
    // Target values for smooth interpolation
    float TargetLeanSidesAmount = 0.f;
    float TargetLookUpAmount = 0.f;

    // Variables for camera rotation tracking
    float LastCameraYawValue = 0.f;
    float LastCameraPitchValue = 0.f;
    float TargetVerticalRecoilAmount = 0.f;
    float TargetHorizontalRecoilAmount = 0.f;

    /** Track if we're currently sprinting for proper notifications */
    bool bIsCurrentlySprinting = false;
    
    /** Saved jump Z velocity for restoration after modifications */
    float OriginalJumpZVelocity = 0.0f;

    /** Current forward/backward input value (smoothed for actual movement) */
    float MoveForwardValue = 0.0f;

    /** Current right/left input value (smoothed for actual movement) */
    float MoveRightValue = 0.0f;

    /** Raw input values from current frame (before smoothing) */
    float RawMoveForwardInput = 0.0f;
    float RawMoveRightInput = 0.0f;
    
    /** Track if we received movement input this frame */
    bool bHasActiveMovementInput = false;
    
    /** Time since last movement input was received */
    float TimeSinceLastMovementInput = 0.0f;

    /** Last non-zero movement input for direction memory */
    FVector2D LastMovementInput = FVector2D::ZeroVector;
	
};