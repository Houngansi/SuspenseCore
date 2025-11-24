// Copyright MedCom Team. All Rights Reserved.

#include "Characters/MedComBaseCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interfaces/Core/IMedComCharacterInterface.h"
#include "Delegates/EventDelegateManager.h"
#include "Attributes/MedComBaseAttributeSet.h"
#include "Characters/MedComCharacterMovementComponent.h"
#include "Engine/World.h"
#include "AbilitySystemGlobals.h"

// Console variable for debug visualization - MUST BE DECLARED BEFORE USE
static TAutoConsoleVariable<int32> CVarShowProceduralAnimDebug(
    TEXT("MedCom.ShowProceduralAnimDebug"),
    0,
    TEXT("Show procedural animation debug info.\n")
    TEXT("0: Disabled\n")
    TEXT("1: Enabled"),
    ECVF_Cheat
);

AMedComBaseCharacter::AMedComBaseCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UMedComCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
    // Setup capsule size
    GetCapsuleComponent()->InitCapsuleSize(34.0f, 96.0f);

    // Configure third person mesh (seen by other players)
    GetMesh()->SetOwnerNoSee(true);  // Owner doesn't see this mesh
    GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
    GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
    GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::None;

    GetMesh()->SetCastShadow(true);// Включаем отбрасывание теней
    GetMesh()->bCastDynamicShadow = true;// Динамические тени
    GetMesh()->bCastStaticShadow = false;// Отключаем статические (персонаж движется)
    GetMesh()->bCastHiddenShadow = true;// Отбрасывать тень даже когда меш скрыт для владельца
    
    // Create first person mesh (arms) - directly attached to the main mesh
    Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh1P"));
    Mesh1P->SetupAttachment(GetMesh());  // Directly attached to main mesh
    Mesh1P->SetOnlyOwnerSee(true);  // Only owner sees this mesh
    Mesh1P->bCastDynamicShadow = false;
    Mesh1P->CastShadow = false;
    Mesh1P->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
    Mesh1P->SetCollisionProfileName(FName("NoCollision"));
    // Set proper relative transform for first person arms
    Mesh1P->SetRelativeLocation(FVector(0.f, 0.f, 160.f));  // Adjust to proper eye level
    Mesh1P->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));  // Face forward

    // Create cinematic camera attached to first person mesh
    Camera = CreateDefaultSubobject<UCineCameraComponent>(TEXT("FirstPersonCamera"));
    Camera->SetupAttachment(Mesh1P, FName("head"));
    Camera->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
    Camera->bUsePawnControlRotation = true;

    // Configure cinematic camera settings
    Camera->SetFieldOfView(CinematicFieldOfView);
    Camera->SetCurrentFocalLength(CurrentFocalLength);
    Camera->SetCurrentAperture(CurrentAperture);

    // Setup lens settings with correct field names
    Camera->LensSettings.MaxFocalLength = 1000.0f;
    Camera->LensSettings.MinFocalLength = 4.0f;
    Camera->LensSettings.MaxFStop = 32.0f;
    Camera->LensSettings.MinFStop = 0.7f;
    Camera->LensSettings.DiaphragmBladeCount = 7;

    // Configure depth of field
    Camera->FocusSettings.FocusMethod = ECameraFocusMethod::Manual;
    Camera->FocusSettings.ManualFocusDistance = ManualFocusDistance;
    Camera->FocusSettings.bDrawDebugFocusPlane = false;
    Camera->FocusSettings.bSmoothFocusChanges = true;
    Camera->FocusSettings.FocusSmoothingInterpSpeed = 8.0f;

    // Filmback settings for sensor size (affects FOV and DOF)
    Camera->Filmback.SensorWidth = 24.89f;  // Super 35mm sensor
    Camera->Filmback.SensorHeight = 18.67f;
    Camera->Filmback.SensorAspectRatio = Camera->Filmback.SensorWidth / Camera->Filmback.SensorHeight;

    // Post process settings specific to FPS games
    Camera->PostProcessSettings.bOverride_MotionBlurAmount = true;
    Camera->PostProcessSettings.MotionBlurAmount = 0.1f;  // Reduced motion blur for competitive play
    Camera->PostProcessSettings.bOverride_SceneFringeIntensity = true;
    Camera->PostProcessSettings.SceneFringeIntensity = 0.0f;  // No chromatic aberration by default

    // Additional cinematic settings
    Camera->CurrentFocusDistance = ManualFocusDistance;

    // Create camera boom for optional camera smoothing
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(GetCapsuleComponent());
    CameraBoom->TargetArmLength = 0.f;
    CameraBoom->bUsePawnControlRotation = true;
    CameraBoom->bDoCollisionTest = false;
    CameraBoom->bEnableCameraLag = bEnableCameraLag;
    CameraBoom->CameraLagSpeed = CameraLagSpeed;
    CameraBoom->CameraLagMaxDistance = CameraLagMaxDistance;
    CameraBoom->bEnableCameraRotationLag = bEnableCameraRotationLag;
    CameraBoom->CameraRotationLagSpeed = CameraRotationLagSpeed;

    // Configure movement
    GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
    GetCharacterMovement()->AirControl = 0.5f;

    // CRITICAL: Enable crouch support!
    GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
    GetCharacterMovement()->bCanWalkOffLedgesWhenCrouching = true;
    GetCharacterMovement()->SetCrouchedHalfHeight(40.0f); // Use setter method instead of direct access
    
    // Additional crouch settings
    GetCharacterMovement()->MaxWalkSpeedCrouched = 150.0f; // Base crouch speed (will be multiplied by modifier)
    GetCharacterMovement()->bUseSeparateBrakingFriction = true;

    // Initialize weapon state
    bHasWeapon = false;
    CurrentWeaponActor = nullptr;

    // Initialize procedural animation values
    LeanSidesAmount = 0.f;
    LookUpAmount = 0.f;
    ArmGroupAnimationWeightMultiplier = 1.f;
    TargetLeanSidesAmount = 0.f;
    TargetLookUpAmount = 0.f;
    VerticalRecoilAmount = 0.f;
    HorizontalRecoilAmount = 0.f;
    TargetVerticalRecoilAmount = 0.f;
    TargetHorizontalRecoilAmount = 0.f;

    // Cache movement component
    MedComMovementComponent = Cast<UMedComCharacterMovementComponent>(GetCharacterMovement());
    
    // Initialize default movement state
    CurrentMovementState = FGameplayTag::RequestGameplayTag(TEXT("Movement.Walking"));
    
    // Save original jump velocity
    if (GetCharacterMovement())
    {
        OriginalJumpZVelocity = GetCharacterMovement()->JumpZVelocity;
    }
    
    // Initialize movement input tracking
    MoveForwardValue = 0.0f;
    MoveRightValue = 0.0f;
    RawMoveForwardInput = 0.0f;
    RawMoveRightInput = 0.0f;
    bHasActiveMovementInput = false;
    TimeSinceLastMovementInput = 0.0f;
}

void AMedComBaseCharacter::BeginPlay()
{
    Super::BeginPlay();
    
    // Apply camera boom settings
    if (CameraBoom)
    {
        CameraBoom->bEnableCameraLag = bEnableCameraLag;
        CameraBoom->CameraLagSpeed = CameraLagSpeed;
        CameraBoom->CameraLagMaxDistance = CameraLagMaxDistance;
        CameraBoom->bEnableCameraRotationLag = bEnableCameraRotationLag;
        CameraBoom->CameraRotationLagSpeed = CameraRotationLagSpeed;
    }
    
    // Apply cinematic camera settings
    if (Camera)
    {
        Camera->PostProcessSettings.bOverride_DepthOfFieldFstop = bEnableDepthOfField;
        Camera->PostProcessSettings.bOverride_DepthOfFieldFocalDistance = bEnableDepthOfField;
        Camera->PostProcessSettings.bOverride_DepthOfFieldDepthBlurAmount = bEnableDepthOfField;
        Camera->PostProcessSettings.bOverride_DepthOfFieldDepthBlurRadius = bEnableDepthOfField;
        
        if (bEnableDepthOfField)
        {
            Camera->PostProcessSettings.DepthOfFieldFstop = CurrentAperture;
            Camera->PostProcessSettings.DepthOfFieldFocalDistance = ManualFocusDistance;
        }
    }
    
    // Initialize active movement tags
    ActiveMovementTags.AddTag(CurrentMovementState);
    
    // CRITICAL: MovementComponent syncs with AttributeSet itself
    UE_LOG(LogTemp, Log, TEXT("[BaseCharacter] BeginPlay completed. Movement speed will be set by AttributeSet."));
    
    if (MedComMovementComponent)
    {
        UE_LOG(LogTemp, Log, TEXT("[BaseCharacter] MedComMovementComponent found. Current MaxWalkSpeed: %.1f"), 
            MedComMovementComponent->MaxWalkSpeed);
    }
    
    // Initialize camera rotation tracking
    if (Controller)
    {
        FRotator ControlRot = Controller->GetControlRotation();
        LastCameraYawValue = ControlRot.Yaw;
        LastCameraPitchValue = ControlRot.Pitch;
    }
}

void AMedComBaseCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // Update time since last movement input
    if (!bHasActiveMovementInput)
    {
        TimeSinceLastMovementInput += DeltaTime;
    }
    else
    {
        TimeSinceLastMovementInput = 0.0f;
    }
    
    // Handle movement value decay when no input
    if (!bHasActiveMovementInput)
    {
        // Determine decay rate based on time since last input
        float DecayRate = 10.0f;
        
        // Faster decay after brief period without input
        if (TimeSinceLastMovementInput > 0.1f)
        {
            DecayRate = 20.0f;
        }
        
        // Instant stop after longer period to prevent drift
        if (TimeSinceLastMovementInput > 0.3f)
        {
            MoveForwardValue = 0.0f;
            MoveRightValue = 0.0f;
        }
        else
        {
            // Smooth decay towards zero
            MoveForwardValue = FMath::FInterpTo(MoveForwardValue, 0.0f, DeltaTime, DecayRate);
            MoveRightValue = FMath::FInterpTo(MoveRightValue, 0.0f, DeltaTime, DecayRate);
            
            // Snap to zero when very close to prevent tiny values
            const float SnapThreshold = 0.01f;
            if (FMath::Abs(MoveForwardValue) < SnapThreshold)
            {
                MoveForwardValue = 0.0f;
            }
            if (FMath::Abs(MoveRightValue) < SnapThreshold)
            {
                MoveRightValue = 0.0f;
            }
        }
    }
    else
    {
        // Apply input smoothing when we have active input
        const float InputSmoothingRate = 15.0f;
        MoveForwardValue = FMath::FInterpTo(MoveForwardValue, RawMoveForwardInput, DeltaTime, InputSmoothingRate);
        MoveRightValue = FMath::FInterpTo(MoveRightValue, RawMoveRightInput, DeltaTime, InputSmoothingRate);
    }
    
    // Update procedural animation values
    UpdateProceduralAnimationValues(DeltaTime);
    
    // Reset input tracking for next frame
    bHasActiveMovementInput = false;
    RawMoveForwardInput = 0.0f;
    RawMoveRightInput = 0.0f;
}

// ========================================
// IMedComCharacterInterface Implementation
// ========================================

UAbilitySystemComponent* AMedComBaseCharacter::GetASC_Implementation() const
{
    if (const APlayerState* PS = GetPlayerState())
    {
        if (PS->GetClass()->ImplementsInterface(UMedComCharacterInterface::StaticClass()))
        {
            return IMedComCharacterInterface::Execute_GetASC(PS);
        }
    }
    return nullptr;
}

void AMedComBaseCharacter::SetHasWeapon_Implementation(bool bInHasWeapon)
{
    bHasWeapon = bInHasWeapon;
    
    if (UEventDelegateManager* Manager = GetDelegateManager())
    {
        IMedComCharacterInterface::BroadcastWeaponChanged(this, CurrentWeaponActor, bHasWeapon);
    }
}

void AMedComBaseCharacter::SetCurrentWeaponActor_Implementation(AActor* WeaponActor)
{
    AActor* OldWeapon = CurrentWeaponActor;
    CurrentWeaponActor = WeaponActor;
    
    if (OldWeapon != CurrentWeaponActor)
    {
        if (UEventDelegateManager* Manager = GetDelegateManager())
        {
            IMedComCharacterInterface::BroadcastWeaponChanged(this, CurrentWeaponActor, bHasWeapon);
        }
    }
}

AActor* AMedComBaseCharacter::GetCurrentWeaponActor_Implementation() const
{
    return CurrentWeaponActor;
}

bool AMedComBaseCharacter::HasWeapon_Implementation() const
{
    return bHasWeapon && CurrentWeaponActor != nullptr;
}

float AMedComBaseCharacter::GetCharacterLevel_Implementation() const
{
    if (const APlayerState* PS = GetPlayerState())
    {
        if (const IMedComCharacterInterface* CharInterface = Cast<IMedComCharacterInterface>(PS))
        {
            return CharInterface->Execute_GetCharacterLevel(PS);
        }
    }
    return 1.0f;
}

bool AMedComBaseCharacter::IsAlive_Implementation() const
{
    if (const APlayerState* PS = GetPlayerState())
    {
        if (const IMedComCharacterInterface* CharInterface = Cast<IMedComCharacterInterface>(PS))
        {
            return CharInterface->Execute_IsAlive(PS);
        }
    }
    return true;
}

int32 AMedComBaseCharacter::GetTeamId_Implementation() const
{
    if (const APlayerState* PS = GetPlayerState())
    {
        if (const IMedComCharacterInterface* CharInterface = Cast<IMedComCharacterInterface>(PS))
        {
            return CharInterface->Execute_GetTeamId(PS);
        }
    }
    return 0;
}

UEventDelegateManager* AMedComBaseCharacter::GetDelegateManager() const
{
    return IMedComCharacterInterface::GetDelegateManagerStatic(this);
}

// ========================================
// IMedComMovementInterface - Speed Management
// ========================================

float AMedComBaseCharacter::GetCurrentMovementSpeed_Implementation() const
{
    if (MedComMovementComponent)
    {
        return MedComMovementComponent->MaxWalkSpeed;
    }
    
    if (GetCharacterMovement())
    {
        return GetCharacterMovement()->MaxWalkSpeed;
    }
    
    return 300.0f; // Default fallback
}

void AMedComBaseCharacter::SetMovementSpeed_Implementation(float NewSpeed)
{
    // CRITICAL: This method should NOT be used!
    // Speed is modified ONLY through GameplayEffects
    UE_LOG(LogTemp, Error, TEXT("[BaseCharacter] SetMovementSpeed called - this should NOT be used! Use GameplayEffects to modify speed!"));
}

float AMedComBaseCharacter::GetDefaultMovementSpeed_Implementation() const
{
    // Get base speed from AttributeSet
    if (APlayerState* PS = GetPlayerState())
    {
        if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS))
        {
            if (const UMedComBaseAttributeSet* AttributeSet = ASC->GetSet<UMedComBaseAttributeSet>())
            {
                // Return base value without modifiers
                return AttributeSet->GetMovementSpeed();
            }
        }
    }
    return 300.0f; // Fallback value
}

float AMedComBaseCharacter::GetMaxWalkSpeed_Implementation() const
{
    if (GetCharacterMovement())
    {
        return GetCharacterMovement()->MaxWalkSpeed;
    }
    return 300.0f;
}

// ========================================
// IMedComMovementInterface - Sprint Management
// ========================================

bool AMedComBaseCharacter::CanSprint_Implementation() const
{
    if (!GetCharacterMovement())
    {
        return false;
    }
    
    bool bCanMove = GetCharacterMovement()->IsMovingOnGround() && 
                    !GetCharacterMovement()->IsFalling() &&
                    !GetCharacterMovement()->IsSwimming() &&
                    !GetCharacterMovement()->IsFlying();
    
    if (bCanMove)
    {
        if (UAbilitySystemComponent* ASC = GetASC_Implementation())
        {
            FGameplayTagContainer BlockingTags;
            BlockingTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Dead")));
            BlockingTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Stunned")));
            BlockingTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Disabled.Movement")));
            
            return !ASC->HasAnyMatchingGameplayTags(BlockingTags);
        }
    }
    
    return bCanMove;
}

bool AMedComBaseCharacter::IsSprinting_Implementation() const
{
    if (MedComMovementComponent)
    {
        return MedComMovementComponent->IsSprinting();
    }
    return false;
}

void AMedComBaseCharacter::StartSprinting_Implementation()
{
    // NOT used - sprint is controlled through GAS
    UE_LOG(LogTemp, Warning, TEXT("[BaseCharacter] StartSprinting called - sprint should be controlled through GAS abilities!"));
}

void AMedComBaseCharacter::StopSprinting_Implementation()
{
    // NOT used - sprint is controlled through GAS
    UE_LOG(LogTemp, Warning, TEXT("[BaseCharacter] StopSprinting called - sprint should be controlled through GAS abilities!"));
}

// ========================================
// IMedComMovementInterface - Jump Management
// ========================================

void AMedComBaseCharacter::Jump()
{
    Jump_Implementation();
}

void AMedComBaseCharacter::StopJumping()
{
    StopJumping_Implementation();
}

void AMedComBaseCharacter::Jump_Implementation()
{
    if (CanJump_Implementation())
    {
        Super::Jump();
        SetMovementState_Implementation(FGameplayTag::RequestGameplayTag(TEXT("Movement.Jumping")));
        UE_LOG(LogTemp, Log, TEXT("[BaseCharacter] Jump performed"));
    }
}

void AMedComBaseCharacter::StopJumping_Implementation()
{
    Super::StopJumping();
    UE_LOG(LogTemp, Log, TEXT("[BaseCharacter] Stop jumping"));
}

bool AMedComBaseCharacter::CanJump_Implementation() const
{
    return Super::CanJump();
}

bool AMedComBaseCharacter::IsGrounded_Implementation() const
{
    if (GetCharacterMovement())
    {
        return GetCharacterMovement()->IsMovingOnGround();
    }
    return false;
}

bool AMedComBaseCharacter::IsFalling_Implementation() const
{
    if (GetCharacterMovement())
    {
        return GetCharacterMovement()->IsFalling();
    }
    return false;
}

float AMedComBaseCharacter::GetJumpZVelocity_Implementation() const
{
    if (GetCharacterMovement())
    {
        return GetCharacterMovement()->JumpZVelocity;
    }
    return 0.0f;
}

void AMedComBaseCharacter::SetJumpZVelocity_Implementation(float NewJumpZVelocity)
{
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->JumpZVelocity = NewJumpZVelocity;
        UE_LOG(LogTemp, Log, TEXT("[BaseCharacter] Jump Z velocity set to: %.1f"), NewJumpZVelocity);
    }
}

// ========================================
// IMedComMovementInterface - Crouch Management
// ========================================

void AMedComBaseCharacter::Crouch_Implementation()
{
    if (CanCrouch_Implementation())
    {
        Super::Crouch();
        SetMovementState_Implementation(FGameplayTag::RequestGameplayTag(TEXT("Movement.Crouching")));
        UE_LOG(LogTemp, Log, TEXT("[BaseCharacter] Crouch performed"));
    }
}

void AMedComBaseCharacter::UnCrouch_Implementation()
{
    Super::UnCrouch();
    
    if (IsGrounded_Implementation())
    {
        SetMovementState_Implementation(FGameplayTag::RequestGameplayTag(TEXT("Movement.Walking")));
    }
    
    UE_LOG(LogTemp, Log, TEXT("[BaseCharacter] UnCrouch performed"));
}

bool AMedComBaseCharacter::CanCrouch_Implementation() const
{
    // First check base conditions
    if (!GetCharacterMovement())
    {
        UE_LOG(LogTemp, Error, TEXT("[Character] CanCrouch: No movement component"));
        return false;
    }
    
    // Check movement settings
    bool bNavAgentCanCrouch = GetCharacterMovement()->NavAgentProps.bCanCrouch;
    if (!bNavAgentCanCrouch)
    {
        UE_LOG(LogTemp, Error, TEXT("[Character] CanCrouch: NavAgentProps.bCanCrouch is FALSE! Set it in constructor!"));
        return false;
    }
    
    // Check that character is on ground
    bool bIsOnGround = GetCharacterMovement()->IsMovingOnGround();
    if (!bIsOnGround)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Character] CanCrouch: Not on ground"));
        return false;
    }
    
    // Check crouch height - use getter method
    float CrouchedHeight = GetCharacterMovement()->GetCrouchedHalfHeight();
    if (CrouchedHeight <= 0.0f)
    {
        UE_LOG(LogTemp, Error, TEXT("[Character] CanCrouch: CrouchedHalfHeight not set! Current value: %.1f"), CrouchedHeight);
        return false;
    }
    
    // Check parent implementation
    bool bParentCanCrouch = Super::CanCrouch();
    if (!bParentCanCrouch)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Character] CanCrouch: Parent CanCrouch returned false"));
        return false;
    }
    
    // All checks passed
    UE_LOG(LogTemp, Verbose, TEXT("[Character] CanCrouch: All checks passed, returning true"));
    return true;
}

bool AMedComBaseCharacter::IsCrouching_Implementation() const
{
    return bIsCrouched;
}

float AMedComBaseCharacter::GetCrouchedHalfHeight_Implementation() const
{
    if (GetCharacterMovement())
    {
        return GetCharacterMovement()->GetCrouchedHalfHeight();
    }
    return 40.0f;
}

// ========================================
// IMedComMovementInterface - State Management
// ========================================

FGameplayTag AMedComBaseCharacter::GetMovementState_Implementation() const
{
    if (!CurrentMovementState.IsValid())
    {
        return FGameplayTag::RequestGameplayTag(TEXT("Movement.Walking"));
    }
    
    return CurrentMovementState;
}

void AMedComBaseCharacter::SetMovementState_Implementation(FGameplayTag NewState)
{
    if (CurrentMovementState != NewState)
    {
        FGameplayTag OldState = CurrentMovementState;
        CurrentMovementState = NewState;
        
        ActiveMovementTags.RemoveTag(OldState);
        ActiveMovementTags.AddTag(NewState);
        
        IMedComMovementInterface::NotifyMovementStateChanged(this, NewState, false);
        
        UE_LOG(LogTemp, Log, TEXT("[BaseCharacter] Movement state changed: %s -> %s"), 
            *OldState.ToString(), *NewState.ToString());
    }
}

FGameplayTagContainer AMedComBaseCharacter::GetActiveMovementTags_Implementation() const
{
    return ActiveMovementTags;
}

bool AMedComBaseCharacter::HasMovementTag_Implementation(FGameplayTag Tag) const
{
    return ActiveMovementTags.HasTag(Tag);
}

// ========================================
// IMedComMovementInterface - Physics & Environment
// ========================================

bool AMedComBaseCharacter::IsSwimming_Implementation() const
{
    if (GetCharacterMovement())
    {
        return GetCharacterMovement()->IsSwimming();
    }
    return false;
}

bool AMedComBaseCharacter::IsFlying_Implementation() const
{
    if (GetCharacterMovement())
    {
        return GetCharacterMovement()->IsFlying();
    }
    return false;
}

FVector AMedComBaseCharacter::GetVelocity_Implementation() const
{
    return Super::GetVelocity();
}

FVector AMedComBaseCharacter::GetGroundNormal_Implementation() const
{
    if (GetCharacterMovement())
    {
        return GetCharacterMovement()->CurrentFloor.HitResult.Normal;
    }
    return FVector::UpVector;
}

// ========================================
// Animation State Getters Implementation
// ========================================

bool AMedComBaseCharacter::GetIsJumping() const
{
    if (MedComMovementComponent)
    {
        return MedComMovementComponent->IsJumping();
    }
    return false;
}

bool AMedComBaseCharacter::GetIsInAir() const
{
    if (MedComMovementComponent)
    {
        return MedComMovementComponent->IsInAir();
    }
    return false;
}

bool AMedComBaseCharacter::GetIsCrouching() const
{
    if (MedComMovementComponent)
    {
        return MedComMovementComponent->IsCrouching();
    }
    return bIsCrouched;
}

bool AMedComBaseCharacter::GetIsSliding() const
{
    if (MedComMovementComponent)
    {
        return MedComMovementComponent->IsSliding();
    }
    return false;
}

bool AMedComBaseCharacter::GetIsSprinting() const
{
    if (MedComMovementComponent)
    {
        return MedComMovementComponent->IsSprinting();
    }
    return false;
}

EMedComMovementMode AMedComBaseCharacter::GetMovementMode() const
{
    if (MedComMovementComponent)
    {
        return MedComMovementComponent->GetCurrentMovementMode();
    }
    return EMedComMovementMode::None;
}

// ========================================
// Movement Actions Implementation
// ========================================

void AMedComBaseCharacter::StartSliding()
{
    if (MedComMovementComponent)
    {
        MedComMovementComponent->StartSliding();
        SetMovementState_Implementation(FGameplayTag::RequestGameplayTag(TEXT("Movement.Sliding")));
        UE_LOG(LogTemp, Log, TEXT("[BaseCharacter] Slide initiated"));
    }
}

void AMedComBaseCharacter::StopSliding()
{
    if (MedComMovementComponent)
    {
        MedComMovementComponent->StopSliding();
        
        if (IsGrounded_Implementation())
        {
            if (IsSprinting_Implementation())
            {
                SetMovementState_Implementation(FGameplayTag::RequestGameplayTag(TEXT("Movement.Sprinting")));
            }
            else if (bIsCrouched)
            {
                SetMovementState_Implementation(FGameplayTag::RequestGameplayTag(TEXT("Movement.Crouching")));
            }
            else
            {
                SetMovementState_Implementation(FGameplayTag::RequestGameplayTag(TEXT("Movement.Walking")));
            }
        }
        
        UE_LOG(LogTemp, Log, TEXT("[BaseCharacter] Slide stopped"));
    }
}

// ========================================
// Movement and Procedural Animation
// ========================================

void AMedComBaseCharacter::Move(const FVector2D& In)
{
    // Store raw input values for this frame
    RawMoveForwardInput = In.Y;
    RawMoveRightInput = In.X;
    
    // Check if we have meaningful input (considering deadzone)
    const float InputDeadZone = 0.1f;
    bHasActiveMovementInput = (FMath::Abs(In.Y) > InputDeadZone || FMath::Abs(In.X) > InputDeadZone);
    
    // Store last meaningful input for direction memory
    if (bHasActiveMovementInput)
    {
        LastMovementInput = In;
    }
    
    // Apply movement using smoothed values
    if (Controller && (MoveForwardValue != 0.0f || MoveRightValue != 0.0f))
    {
        const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
        
        if (MoveForwardValue != 0.0f)
        {
            AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), MoveForwardValue);
        }
        
        if (MoveRightValue != 0.0f)
        {
            AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), MoveRightValue);
        }
    }
}

void AMedComBaseCharacter::Look(const FVector2D& In)
{
    // Apply rotation input
    AddControllerYawInput(In.X);
    AddControllerPitchInput(In.Y);
}

float AMedComBaseCharacter::GetMovementInputSpeed() const
{
    // Return the magnitude of smoothed movement values
    // This is what AnimBP should use for Movement float
    return FMath::Sqrt(MoveForwardValue * MoveForwardValue + MoveRightValue * MoveRightValue);
}

bool AMedComBaseCharacter::HasMovementInput() const
{
    // Return whether we have active input THIS FRAME
    // This prevents circular dependency issues
    return bHasActiveMovementInput;
}

FVector AMedComBaseCharacter::GetMovementInputVector() const
{
    if (!Controller)
    {
        return FVector::ZeroVector;
    }
    
    // Use smoothed values for actual movement vector
    const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
    const FVector ForwardDirection = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X) * MoveForwardValue;
    const FVector RightDirection = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y) * MoveRightValue;
    
    FVector ResultVector = ForwardDirection + RightDirection;
    
    // Normalize only if we have meaningful movement
    if (ResultVector.SizeSquared() > 0.01f)
    {
        return ResultVector.GetSafeNormal();
    }
    
    return FVector::ZeroVector;
}

bool AMedComBaseCharacter::IsMoving() const
{
    if (!GetCharacterMovement())
    {
        return false;
    }
    
    // Get horizontal velocity
    FVector Velocity = GetCharacterMovement()->Velocity;
    Velocity.Z = 0.0f;
    float Speed = Velocity.Size();
    
    // Movement thresholds
    const float MinimumSpeed = 10.0f;
    const float MinimumInputMagnitude = 0.1f;
    
    // Check actual velocity
    bool bHasVelocity = Speed > MinimumSpeed;
    
    // Check input magnitude (use smoothed values for consistency)
    bool bHasInput = GetMovementInputSpeed() > MinimumInputMagnitude;
    
    // Character is moving if they have velocity OR meaningful input
    // This prevents animation pops when starting/stopping
    return bHasVelocity || bHasInput;
}

float AMedComBaseCharacter::GetAnimationForwardValue() const
{
    // Получаем базовое значение из внутренней переменной
    float BaseValue = MoveForwardValue;
    
    // Проверяем состояние спринта
    if (IsCharacterSprinting())
    {
        // При спринте умножаем на 2
        // Это позволит вашему макросу определить спринт когда значение > 1
        BaseValue *= 2.0f;
        
        UE_LOG(LogTemp, VeryVerbose, TEXT("[Animation] Forward value for animation: %.2f (Sprint Active)"), BaseValue);
    }
    
    return BaseValue;
}

float AMedComBaseCharacter::GetAnimationRightValue() const
{
    // Получаем базовое значение из внутренней переменной
    float BaseValue = MoveRightValue;
    
    // Проверяем состояние спринта
    if (IsCharacterSprinting())
    {
        // При спринте умножаем на 2
        BaseValue *= 2.0f;
        
        UE_LOG(LogTemp, VeryVerbose, TEXT("[Animation] Right value for animation: %.2f (Sprint Active)"), BaseValue);
    }
    
    return BaseValue;
}

bool AMedComBaseCharacter::IsCharacterSprinting() const
{
    // Единый источник истины для состояния спринта
    // Берём из MovementComponent, который синхронизирован с GAS
    if (MedComMovementComponent)
    {
        return MedComMovementComponent->IsSprinting();
    }
    
    return false;
}

UMedComCharacterMovementComponent* AMedComBaseCharacter::GetMedComMovementComponent() const
{
    return MedComMovementComponent;
}

void AMedComBaseCharacter::UpdateProceduralAnimationValues(float DeltaTime)
{
    if (!IsLocallyControlled() || !Controller)
    {
        return;
    }
    
    // Calculate camera rotation deltas
    FRotator CurrentControlRotation = Controller->GetControlRotation();
    
    // Calculate raw deltas
    float YawDelta = CurrentControlRotation.Yaw - LastCameraYawValue;
    float PitchDelta = CurrentControlRotation.Pitch - LastCameraPitchValue;
    
    // Normalize deltas to handle wraparound
    if (YawDelta > 180.0f)
        YawDelta -= 360.0f;
    else if (YawDelta < -180.0f)
        YawDelta += 360.0f;
        
    if (PitchDelta > 180.0f)
        PitchDelta -= 360.0f;
    else if (PitchDelta < -180.0f)
        PitchDelta += 360.0f;
    
    // Convert deltas to rotation rates (degrees per second)
    float YawRate = YawDelta / DeltaTime;
    float PitchRate = PitchDelta / DeltaTime;
    
    // Clamp rates to reasonable values
    const float MaxRotationRate = 1000.0f; // Max 1000 degrees per second
    YawRate = FMath::Clamp(YawRate, -MaxRotationRate, MaxRotationRate);
    PitchRate = FMath::Clamp(PitchRate, -MaxRotationRate, MaxRotationRate);
    
    // Normalize rates to -1 to 1 range
    float NormalizedYawRate = YawRate / MaxRotationRate;
    float NormalizedPitchRate = PitchRate / MaxRotationRate;
    
    // Calculate target lean based on camera rotation rate
    // Negative yaw rate (turning left) = positive lean (lean right to compensate)
    TargetLeanSidesAmount = -NormalizedYawRate * MaxLeanAngle * LeanStrengthMultiplier;
    
    // Calculate target look up/down based on pitch rate
    // Negative pitch rate (looking up) = positive look up
    TargetLookUpAmount = -NormalizedPitchRate * MaxLookUpAngle * LookUpStrengthMultiplier;
    
    // Apply additional weight based on whether we're moving
    float MovementMultiplier = 1.0f;
    if (IsMoving())
    {
        // Less lean when moving (more stable)
        MovementMultiplier = 0.7f;
    }
    
    TargetLeanSidesAmount *= MovementMultiplier;
    TargetLookUpAmount *= MovementMultiplier;
    
    // Calculate recoil amounts (stronger reaction to fast movements)
    const float RecoilThreshold = 0.3f; // Only apply recoil for fast movements
    
    if (FMath::Abs(NormalizedYawRate) > RecoilThreshold)
    {
        TargetHorizontalRecoilAmount = -NormalizedYawRate * MaxHorizontalRecoilAngle * HorizontalRecoilStrengthMultiplier;
    }
    else
    {
        TargetHorizontalRecoilAmount = 0.0f;
    }
    
    if (FMath::Abs(NormalizedPitchRate) > RecoilThreshold)
    {
        TargetVerticalRecoilAmount = -NormalizedPitchRate * MaxVerticalRecoilAngle * VerticalRecoilStrengthMultiplier;
    }
    else
    {
        TargetVerticalRecoilAmount = 0.0f;
    }
    
    // Interpolate to target values
    float InterpSpeed = ProceduralAnimInterpolationSpeed;
    
    // Faster interpolation when returning to center
    if (FMath::Abs(TargetLeanSidesAmount) < 0.01f && FMath::Abs(LeanSidesAmount) > 0.01f)
    {
        InterpSpeed *= 2.0f;
    }
    if (FMath::Abs(TargetLookUpAmount) < 0.01f && FMath::Abs(LookUpAmount) > 0.01f)
    {
        InterpSpeed *= 2.0f;
    }
    
    // Smooth interpolation for lean and look
    LeanSidesAmount = FMath::FInterpTo(LeanSidesAmount, TargetLeanSidesAmount, DeltaTime, InterpSpeed);
    LookUpAmount = FMath::FInterpTo(LookUpAmount, TargetLookUpAmount, DeltaTime, InterpSpeed);
    
    // Faster interpolation for recoil (more responsive)
    float RecoilInterpSpeed = ProceduralAnimInterpolationSpeed * 2.0f;
    
    VerticalRecoilAmount = FMath::FInterpTo(
        VerticalRecoilAmount, 
        TargetVerticalRecoilAmount, 
        DeltaTime, 
        RecoilInterpSpeed
    );
    
    HorizontalRecoilAmount = FMath::FInterpTo(
        HorizontalRecoilAmount, 
        TargetHorizontalRecoilAmount, 
        DeltaTime, 
        RecoilInterpSpeed
    );
    
    // Update arm animation weight based on overall activity
    float OverallActivity = FMath::Max(
        FMath::Abs(NormalizedYawRate),
        FMath::Abs(NormalizedPitchRate)
    );
    
    if (OverallActivity > 0.1f || IsMoving())
    {
        ArmGroupAnimationWeightMultiplier = 1.0f;
    }
    else
    {
        // Reduce arm animation weight when idle
        ArmGroupAnimationWeightMultiplier = FMath::FInterpTo(
            ArmGroupAnimationWeightMultiplier, 
            0.7f, 
            DeltaTime, 
            2.0f
        );
    }
    
    // Store current values for next frame
    LastCameraYawValue = CurrentControlRotation.Yaw;
    LastCameraPitchValue = CurrentControlRotation.Pitch;
    
    // Debug visualization (only in editor)
#if WITH_EDITOR
    if (GEngine && CVarShowProceduralAnimDebug.GetValueOnGameThread())
    {
        FString DebugText = FString::Printf(
            TEXT("Camera Rate - Yaw: %.1f deg/s | Pitch: %.1f deg/s"),
            YawRate, PitchRate
        );
        GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Yellow, DebugText);
        
        FString ProceduralDebugText = FString::Printf(
            TEXT("Procedural - Lean: %.2f | LookUp: %.2f | VRecoil: %.2f | HRecoil: %.2f"),
            LeanSidesAmount, LookUpAmount, VerticalRecoilAmount, HorizontalRecoilAmount
        );
        GEngine->AddOnScreenDebugMessage(2, 0.0f, FColor::Cyan, ProceduralDebugText);
        
        FString WeightDebugText = FString::Printf(
            TEXT("Arm Weight: %.2f | Activity: %.2f"),
            ArmGroupAnimationWeightMultiplier, OverallActivity
        );
        GEngine->AddOnScreenDebugMessage(3, 0.0f, FColor::Green, WeightDebugText);
    }
#endif
}

void AMedComBaseCharacter::SetupPlayerInputComponent(UInputComponent* IC)
{
    Super::SetupPlayerInputComponent(IC);
}

// ========================================
// Cinematic Camera Control Implementation
// ========================================

void AMedComBaseCharacter::SetCameraFOV(float NewFOV)
{
    if (Camera)
    {
        Camera->SetFieldOfView(FMath::Clamp(NewFOV, 5.0f, 170.0f));
    }
}

void AMedComBaseCharacter::SetCameraFocalLength(float NewFocalLength)
{
    if (Camera)
    {
        float ClampedFocalLength = FMath::Clamp(NewFocalLength,
            Camera->LensSettings.MinFocalLength,
            Camera->LensSettings.MaxFocalLength);
        
        Camera->SetCurrentFocalLength(ClampedFocalLength);
        CurrentFocalLength = ClampedFocalLength;
    }
}

void AMedComBaseCharacter::SetCameraAperture(float NewAperture)
{
    if (Camera)
    {
        float ClampedAperture = FMath::Clamp(NewAperture, 
            Camera->LensSettings.MinFStop, 
            Camera->LensSettings.MaxFStop);
        
        Camera->SetCurrentAperture(ClampedAperture);
        
        if (bEnableDepthOfField)
        {
            Camera->PostProcessSettings.DepthOfFieldFstop = ClampedAperture;
        }
        
        CurrentAperture = ClampedAperture;
    }
}

void AMedComBaseCharacter::SetDepthOfFieldEnabled(bool bEnabled)
{
    if (Camera)
    {
        bEnableDepthOfField = bEnabled;
        
        Camera->PostProcessSettings.bOverride_DepthOfFieldFstop = bEnabled;
        Camera->PostProcessSettings.bOverride_DepthOfFieldFocalDistance = bEnabled;
        Camera->PostProcessSettings.bOverride_DepthOfFieldDepthBlurAmount = bEnabled;
        Camera->PostProcessSettings.bOverride_DepthOfFieldDepthBlurRadius = bEnabled;
    }
}

void AMedComBaseCharacter::SetCameraFocusDistance(float Distance)
{
    if (Camera)
    {
        Camera->FocusSettings.ManualFocusDistance = Distance;
        Camera->PostProcessSettings.DepthOfFieldFocalDistance = Distance;
    }
}

void AMedComBaseCharacter::ApplyCinematicPreset(bool bEnableDOF, float Aperture, float FocusDistance)
{
    SetDepthOfFieldEnabled(bEnableDOF);
    SetCameraAperture(Aperture);
    SetCameraFocusDistance(FocusDistance);
    
    if (Camera && bEnableDOF)
    {
        Camera->PostProcessSettings.bOverride_MotionBlurAmount = true;
        Camera->PostProcessSettings.MotionBlurAmount = 0.5f;
        
        Camera->PostProcessSettings.bOverride_VignetteIntensity = true;
        Camera->PostProcessSettings.VignetteIntensity = 0.4f;
    }
}