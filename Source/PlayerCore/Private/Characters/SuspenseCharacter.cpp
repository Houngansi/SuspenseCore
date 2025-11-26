// Copyright Suspense Team. All Rights Reserved.

#include "Characters/SuspenseCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interfaces/Core/ISuspenseCharacter.h"
#include "Delegates/SuspenseEventManager.h"
#include "Attributes/SuspenseBaseAttributeSet.h"
#include "Characters/SuspenseCharacterMovementComponent.h"
#include "Engine/World.h"
#include "AbilitySystemGlobals.h"

// Console variable for debug visualization - MUST BE DECLARED BEFORE USE
static TAutoConsoleVariable<int32> CVarShowProceduralAnimDebug(
    TEXT("Suspense.ShowProceduralAnimDebug"),
    0,
    TEXT("Show procedural animation debug info.\n")
    TEXT("0: Disabled\n")
    TEXT("1: Enabled"),
    ECVF_Cheat
);

ASuspenseCharacter::ASuspenseCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<USuspenseCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
    // Setup capsule size
    GetCapsuleComponent()->InitCapsuleSize(34.0f, 96.0f);

    // Configure third person mesh (seen by other players)
    GetMesh()->SetOwnerNoSee(true);  // Owner doesn't see this mesh
    GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
    GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
    GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::None;

    GetMesh()->SetCastShadow(true);
    GetMesh()->bCastDynamicShadow = true;
    GetMesh()->bCastStaticShadow = false;
    GetMesh()->bCastHiddenShadow = true;

    // Create first person mesh (arms) - directly attached to the main mesh
    Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh1P"));
    Mesh1P->SetupAttachment(GetMesh());
    Mesh1P->SetOnlyOwnerSee(true);
    Mesh1P->bCastDynamicShadow = false;
    Mesh1P->CastShadow = false;
    Mesh1P->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
    Mesh1P->SetCollisionProfileName(FName("NoCollision"));
    Mesh1P->SetRelativeLocation(FVector(0.f, 0.f, 160.f));
    Mesh1P->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));

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
    Camera->Filmback.SensorWidth = 24.89f;
    Camera->Filmback.SensorHeight = 18.67f;
    Camera->Filmback.SensorAspectRatio = Camera->Filmback.SensorWidth / Camera->Filmback.SensorHeight;

    // Post process settings specific to FPS games
    Camera->PostProcessSettings.bOverride_MotionBlurAmount = true;
    Camera->PostProcessSettings.MotionBlurAmount = 0.1f;
    Camera->PostProcessSettings.bOverride_SceneFringeIntensity = true;
    Camera->PostProcessSettings.SceneFringeIntensity = 0.0f;

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
    GetCharacterMovement()->SetCrouchedHalfHeight(40.0f);

    // Additional crouch settings
    GetCharacterMovement()->MaxWalkSpeedCrouched = 150.0f;
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
    SuspenseMovementComponent = Cast<USuspenseCharacterMovementComponent>(GetCharacterMovement());

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

void ASuspenseCharacter::BeginPlay()
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
    UE_LOG(LogTemp, Log, TEXT("[SuspenseCharacter] BeginPlay completed. Movement speed will be set by AttributeSet."));

    if (SuspenseMovementComponent)
    {
        UE_LOG(LogTemp, Log, TEXT("[SuspenseCharacter] SuspenseMovementComponent found. Current MaxWalkSpeed: %.1f"),
            SuspenseMovementComponent->MaxWalkSpeed);
    }

    // Initialize camera rotation tracking
    if (Controller)
    {
        FRotator ControlRot = Controller->GetControlRotation();
        LastCameraYawValue = ControlRot.Yaw;
        LastCameraPitchValue = ControlRot.Pitch;
    }
}

void ASuspenseCharacter::Tick(float DeltaTime)
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
        float DecayRate = 10.0f;

        if (TimeSinceLastMovementInput > 0.1f)
        {
            DecayRate = 20.0f;
        }

        if (TimeSinceLastMovementInput > 0.3f)
        {
            MoveForwardValue = 0.0f;
            MoveRightValue = 0.0f;
        }
        else
        {
            MoveForwardValue = FMath::FInterpTo(MoveForwardValue, 0.0f, DeltaTime, DecayRate);
            MoveRightValue = FMath::FInterpTo(MoveRightValue, 0.0f, DeltaTime, DecayRate);

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
// ISuspenseCharacterInterface Implementation
// ========================================

UAbilitySystemComponent* ASuspenseCharacter::GetASC_Implementation() const
{
    if (const APlayerState* PS = GetPlayerState())
    {
        if (PS->GetClass()->ImplementsInterface(USuspenseCharacterInterface::StaticClass()))
        {
            return ISuspenseCharacterInterface::Execute_GetASC(PS);
        }
    }
    return nullptr;
}

void ASuspenseCharacter::SetHasWeapon_Implementation(bool bInHasWeapon)
{
    bHasWeapon = bInHasWeapon;

    if (USuspenseEventManager* Manager = GetDelegateManager())
    {
        ISuspenseCharacterInterface::BroadcastWeaponChanged(this, CurrentWeaponActor, bHasWeapon);
    }
}

void ASuspenseCharacter::SetCurrentWeaponActor_Implementation(AActor* WeaponActor)
{
    AActor* OldWeapon = CurrentWeaponActor;
    CurrentWeaponActor = WeaponActor;

    if (OldWeapon != CurrentWeaponActor)
    {
        if (USuspenseEventManager* Manager = GetDelegateManager())
        {
            ISuspenseCharacterInterface::BroadcastWeaponChanged(this, CurrentWeaponActor, bHasWeapon);
        }
    }
}

AActor* ASuspenseCharacter::GetCurrentWeaponActor_Implementation() const
{
    return CurrentWeaponActor;
}

bool ASuspenseCharacter::HasWeapon_Implementation() const
{
    return bHasWeapon && CurrentWeaponActor != nullptr;
}

float ASuspenseCharacter::GetCharacterLevel_Implementation() const
{
    if (const APlayerState* PS = GetPlayerState())
    {
        if (const ISuspenseCharacterInterface* CharInterface = Cast<ISuspenseCharacterInterface>(PS))
        {
            return CharInterface->Execute_GetCharacterLevel(PS);
        }
    }
    return 1.0f;
}

bool ASuspenseCharacter::IsAlive_Implementation() const
{
    if (const APlayerState* PS = GetPlayerState())
    {
        if (const ISuspenseCharacterInterface* CharInterface = Cast<ISuspenseCharacterInterface>(PS))
        {
            return CharInterface->Execute_IsAlive(PS);
        }
    }
    return true;
}

int32 ASuspenseCharacter::GetTeamId_Implementation() const
{
    if (const APlayerState* PS = GetPlayerState())
    {
        if (const ISuspenseCharacterInterface* CharInterface = Cast<ISuspenseCharacterInterface>(PS))
        {
            return CharInterface->Execute_GetTeamId(PS);
        }
    }
    return 0;
}

USuspenseEventManager* ASuspenseCharacter::GetDelegateManager() const
{
    return ISuspenseCharacterInterface::GetDelegateManagerStatic(this);
}

// ========================================
// ISuspenseMovement - Speed Management
// ========================================

float ASuspenseCharacter::GetCurrentMovementSpeed_Implementation() const
{
    if (SuspenseMovementComponent)
    {
        return SuspenseMovementComponent->MaxWalkSpeed;
    }

    if (GetCharacterMovement())
    {
        return GetCharacterMovement()->MaxWalkSpeed;
    }

    return 300.0f;
}

void ASuspenseCharacter::SetMovementSpeed_Implementation(float NewSpeed)
{
    // CRITICAL: This method should NOT be used!
    // Speed is modified ONLY through GameplayEffects
    UE_LOG(LogTemp, Error, TEXT("[SuspenseCharacter] SetMovementSpeed called - this should NOT be used! Use GameplayEffects to modify speed!"));
}

float ASuspenseCharacter::GetDefaultMovementSpeed_Implementation() const
{
    if (APlayerState* PS = GetPlayerState())
    {
        if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS))
        {
            if (const USuspenseBaseAttributeSet* AttributeSet = ASC->GetSet<USuspenseBaseAttributeSet>())
            {
                return AttributeSet->GetMovementSpeed();
            }
        }
    }
    return 300.0f;
}

float ASuspenseCharacter::GetMaxWalkSpeed_Implementation() const
{
    if (GetCharacterMovement())
    {
        return GetCharacterMovement()->MaxWalkSpeed;
    }
    return 300.0f;
}

// ========================================
// ISuspenseMovement - Sprint Management
// ========================================

bool ASuspenseCharacter::CanSprint_Implementation() const
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

bool ASuspenseCharacter::IsSprinting_Implementation() const
{
    if (SuspenseMovementComponent)
    {
        return SuspenseMovementComponent->IsSprinting();
    }
    return false;
}

void ASuspenseCharacter::StartSprinting_Implementation()
{
    UE_LOG(LogTemp, Warning, TEXT("[SuspenseCharacter] StartSprinting called - sprint should be controlled through GAS abilities!"));
}

void ASuspenseCharacter::StopSprinting_Implementation()
{
    UE_LOG(LogTemp, Warning, TEXT("[SuspenseCharacter] StopSprinting called - sprint should be controlled through GAS abilities!"));
}

// ========================================
// ISuspenseMovement - Jump Management
// ========================================

void ASuspenseCharacter::Jump()
{
    Jump_Implementation();
}

void ASuspenseCharacter::StopJumping()
{
    StopJumping_Implementation();
}

void ASuspenseCharacter::Jump_Implementation()
{
    if (CanJump_Implementation())
    {
        Super::Jump();
        SetMovementState_Implementation(FGameplayTag::RequestGameplayTag(TEXT("Movement.Jumping")));
        UE_LOG(LogTemp, Log, TEXT("[SuspenseCharacter] Jump performed"));
    }
}

void ASuspenseCharacter::StopJumping_Implementation()
{
    Super::StopJumping();
    UE_LOG(LogTemp, Log, TEXT("[SuspenseCharacter] Stop jumping"));
}

bool ASuspenseCharacter::CanJump_Implementation() const
{
    return Super::CanJump();
}

bool ASuspenseCharacter::IsGrounded_Implementation() const
{
    if (GetCharacterMovement())
    {
        return GetCharacterMovement()->IsMovingOnGround();
    }
    return false;
}

bool ASuspenseCharacter::IsFalling_Implementation() const
{
    if (GetCharacterMovement())
    {
        return GetCharacterMovement()->IsFalling();
    }
    return false;
}

float ASuspenseCharacter::GetJumpZVelocity_Implementation() const
{
    if (GetCharacterMovement())
    {
        return GetCharacterMovement()->JumpZVelocity;
    }
    return 0.0f;
}

void ASuspenseCharacter::SetJumpZVelocity_Implementation(float NewJumpZVelocity)
{
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->JumpZVelocity = NewJumpZVelocity;
        UE_LOG(LogTemp, Log, TEXT("[SuspenseCharacter] Jump Z velocity set to: %.1f"), NewJumpZVelocity);
    }
}

// ========================================
// ISuspenseMovement - Crouch Management
// ========================================

void ASuspenseCharacter::Crouch_Implementation()
{
    if (CanCrouch_Implementation())
    {
        Super::Crouch();
        SetMovementState_Implementation(FGameplayTag::RequestGameplayTag(TEXT("Movement.Crouching")));
        UE_LOG(LogTemp, Log, TEXT("[SuspenseCharacter] Crouch performed"));
    }
}

void ASuspenseCharacter::UnCrouch_Implementation()
{
    Super::UnCrouch();

    if (IsGrounded_Implementation())
    {
        SetMovementState_Implementation(FGameplayTag::RequestGameplayTag(TEXT("Movement.Walking")));
    }

    UE_LOG(LogTemp, Log, TEXT("[SuspenseCharacter] UnCrouch performed"));
}

bool ASuspenseCharacter::CanCrouch_Implementation() const
{
    if (!GetCharacterMovement())
    {
        UE_LOG(LogTemp, Error, TEXT("[Character] CanCrouch: No movement component"));
        return false;
    }

    bool bNavAgentCanCrouch = GetCharacterMovement()->NavAgentProps.bCanCrouch;
    if (!bNavAgentCanCrouch)
    {
        UE_LOG(LogTemp, Error, TEXT("[Character] CanCrouch: NavAgentProps.bCanCrouch is FALSE! Set it in constructor!"));
        return false;
    }

    bool bIsOnGround = GetCharacterMovement()->IsMovingOnGround();
    if (!bIsOnGround)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Character] CanCrouch: Not on ground"));
        return false;
    }

    float CrouchedHeight = GetCharacterMovement()->GetCrouchedHalfHeight();
    if (CrouchedHeight <= 0.0f)
    {
        UE_LOG(LogTemp, Error, TEXT("[Character] CanCrouch: CrouchedHalfHeight not set! Current value: %.1f"), CrouchedHeight);
        return false;
    }

    bool bParentCanCrouch = Super::CanCrouch();
    if (!bParentCanCrouch)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Character] CanCrouch: Parent CanCrouch returned false"));
        return false;
    }

    UE_LOG(LogTemp, Verbose, TEXT("[Character] CanCrouch: All checks passed, returning true"));
    return true;
}

bool ASuspenseCharacter::IsCrouching_Implementation() const
{
    return bIsCrouched;
}

float ASuspenseCharacter::GetCrouchedHalfHeight_Implementation() const
{
    if (GetCharacterMovement())
    {
        return GetCharacterMovement()->GetCrouchedHalfHeight();
    }
    return 40.0f;
}

// ========================================
// ISuspenseMovement - State Management
// ========================================

FGameplayTag ASuspenseCharacter::GetMovementState_Implementation() const
{
    if (!CurrentMovementState.IsValid())
    {
        return FGameplayTag::RequestGameplayTag(TEXT("Movement.Walking"));
    }

    return CurrentMovementState;
}

void ASuspenseCharacter::SetMovementState_Implementation(FGameplayTag NewState)
{
    if (CurrentMovementState != NewState)
    {
        FGameplayTag OldState = CurrentMovementState;
        CurrentMovementState = NewState;

        ActiveMovementTags.RemoveTag(OldState);
        ActiveMovementTags.AddTag(NewState);

        ISuspenseMovement::NotifyMovementStateChanged(this, NewState, false);

        UE_LOG(LogTemp, Log, TEXT("[SuspenseCharacter] Movement state changed: %s -> %s"),
            *OldState.ToString(), *NewState.ToString());
    }
}

FGameplayTagContainer ASuspenseCharacter::GetActiveMovementTags_Implementation() const
{
    return ActiveMovementTags;
}

bool ASuspenseCharacter::HasMovementTag_Implementation(FGameplayTag Tag) const
{
    return ActiveMovementTags.HasTag(Tag);
}

// ========================================
// ISuspenseMovement - Physics & Environment
// ========================================

bool ASuspenseCharacter::IsSwimming_Implementation() const
{
    if (GetCharacterMovement())
    {
        return GetCharacterMovement()->IsSwimming();
    }
    return false;
}

bool ASuspenseCharacter::IsFlying_Implementation() const
{
    if (GetCharacterMovement())
    {
        return GetCharacterMovement()->IsFlying();
    }
    return false;
}

FVector ASuspenseCharacter::GetVelocity_Implementation() const
{
    return Super::GetVelocity();
}

FVector ASuspenseCharacter::GetGroundNormal_Implementation() const
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

bool ASuspenseCharacter::GetIsJumping() const
{
    if (SuspenseMovementComponent)
    {
        return SuspenseMovementComponent->IsJumping();
    }
    return false;
}

bool ASuspenseCharacter::GetIsInAir() const
{
    if (SuspenseMovementComponent)
    {
        return SuspenseMovementComponent->IsInAir();
    }
    return false;
}

bool ASuspenseCharacter::GetIsCrouching() const
{
    if (SuspenseMovementComponent)
    {
        return SuspenseMovementComponent->IsCrouching();
    }
    return bIsCrouched;
}

bool ASuspenseCharacter::GetIsSliding() const
{
    if (SuspenseMovementComponent)
    {
        return SuspenseMovementComponent->IsSliding();
    }
    return false;
}

bool ASuspenseCharacter::GetIsSprinting() const
{
    if (SuspenseMovementComponent)
    {
        return SuspenseMovementComponent->IsSprinting();
    }
    return false;
}

ESuspenseMovementMode ASuspenseCharacter::GetMovementMode() const
{
    if (SuspenseMovementComponent)
    {
        return SuspenseMovementComponent->GetCurrentMovementMode();
    }
    return ESuspenseMovementMode::None;
}

// ========================================
// Movement Actions Implementation
// ========================================

void ASuspenseCharacter::StartSliding()
{
    if (SuspenseMovementComponent)
    {
        SuspenseMovementComponent->StartSliding();
        SetMovementState_Implementation(FGameplayTag::RequestGameplayTag(TEXT("Movement.Sliding")));
        UE_LOG(LogTemp, Log, TEXT("[SuspenseCharacter] Slide initiated"));
    }
}

void ASuspenseCharacter::StopSliding()
{
    if (SuspenseMovementComponent)
    {
        SuspenseMovementComponent->StopSliding();

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

        UE_LOG(LogTemp, Log, TEXT("[SuspenseCharacter] Slide stopped"));
    }
}

// ========================================
// Movement and Procedural Animation
// ========================================

void ASuspenseCharacter::Move(const FVector2D& In)
{
    RawMoveForwardInput = In.Y;
    RawMoveRightInput = In.X;

    const float InputDeadZone = 0.1f;
    bHasActiveMovementInput = (FMath::Abs(In.Y) > InputDeadZone || FMath::Abs(In.X) > InputDeadZone);

    if (bHasActiveMovementInput)
    {
        LastMovementInput = In;
    }

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

void ASuspenseCharacter::Look(const FVector2D& In)
{
    AddControllerYawInput(In.X);
    AddControllerPitchInput(In.Y);
}

float ASuspenseCharacter::GetMovementInputSpeed() const
{
    return FMath::Sqrt(MoveForwardValue * MoveForwardValue + MoveRightValue * MoveRightValue);
}

bool ASuspenseCharacter::HasMovementInput() const
{
    return bHasActiveMovementInput;
}

FVector ASuspenseCharacter::GetMovementInputVector() const
{
    if (!Controller)
    {
        return FVector::ZeroVector;
    }

    const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
    const FVector ForwardDirection = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X) * MoveForwardValue;
    const FVector RightDirection = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y) * MoveRightValue;

    FVector ResultVector = ForwardDirection + RightDirection;

    if (ResultVector.SizeSquared() > 0.01f)
    {
        return ResultVector.GetSafeNormal();
    }

    return FVector::ZeroVector;
}

bool ASuspenseCharacter::IsMoving() const
{
    if (!GetCharacterMovement())
    {
        return false;
    }

    FVector Velocity = GetCharacterMovement()->Velocity;
    Velocity.Z = 0.0f;
    float Speed = Velocity.Size();

    const float MinimumSpeed = 10.0f;
    const float MinimumInputMagnitude = 0.1f;

    bool bHasVelocity = Speed > MinimumSpeed;
    bool bHasInput = GetMovementInputSpeed() > MinimumInputMagnitude;

    return bHasVelocity || bHasInput;
}

float ASuspenseCharacter::GetAnimationForwardValue() const
{
    float BaseValue = MoveForwardValue;

    if (IsCharacterSprinting())
    {
        BaseValue *= 2.0f;
        UE_LOG(LogTemp, VeryVerbose, TEXT("[Animation] Forward value for animation: %.2f (Sprint Active)"), BaseValue);
    }

    return BaseValue;
}

float ASuspenseCharacter::GetAnimationRightValue() const
{
    float BaseValue = MoveRightValue;

    if (IsCharacterSprinting())
    {
        BaseValue *= 2.0f;
        UE_LOG(LogTemp, VeryVerbose, TEXT("[Animation] Right value for animation: %.2f (Sprint Active)"), BaseValue);
    }

    return BaseValue;
}

bool ASuspenseCharacter::IsCharacterSprinting() const
{
    if (SuspenseMovementComponent)
    {
        return SuspenseMovementComponent->IsSprinting();
    }

    return false;
}

USuspenseCharacterMovementComponent* ASuspenseCharacter::GetSuspenseMovementComponent() const
{
    return SuspenseMovementComponent;
}

void ASuspenseCharacter::UpdateProceduralAnimationValues(float DeltaTime)
{
    if (!IsLocallyControlled() || !Controller)
    {
        return;
    }

    FRotator CurrentControlRotation = Controller->GetControlRotation();

    float YawDelta = CurrentControlRotation.Yaw - LastCameraYawValue;
    float PitchDelta = CurrentControlRotation.Pitch - LastCameraPitchValue;

    if (YawDelta > 180.0f)
        YawDelta -= 360.0f;
    else if (YawDelta < -180.0f)
        YawDelta += 360.0f;

    if (PitchDelta > 180.0f)
        PitchDelta -= 360.0f;
    else if (PitchDelta < -180.0f)
        PitchDelta += 360.0f;

    float YawRate = YawDelta / DeltaTime;
    float PitchRate = PitchDelta / DeltaTime;

    const float MaxRotationRate = 1000.0f;
    YawRate = FMath::Clamp(YawRate, -MaxRotationRate, MaxRotationRate);
    PitchRate = FMath::Clamp(PitchRate, -MaxRotationRate, MaxRotationRate);

    float NormalizedYawRate = YawRate / MaxRotationRate;
    float NormalizedPitchRate = PitchRate / MaxRotationRate;

    TargetLeanSidesAmount = -NormalizedYawRate * MaxLeanAngle * LeanStrengthMultiplier;
    TargetLookUpAmount = -NormalizedPitchRate * MaxLookUpAngle * LookUpStrengthMultiplier;

    float MovementMultiplier = 1.0f;
    if (IsMoving())
    {
        MovementMultiplier = 0.7f;
    }

    TargetLeanSidesAmount *= MovementMultiplier;
    TargetLookUpAmount *= MovementMultiplier;

    const float RecoilThreshold = 0.3f;

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

    float InterpSpeed = ProceduralAnimInterpolationSpeed;

    if (FMath::Abs(TargetLeanSidesAmount) < 0.01f && FMath::Abs(LeanSidesAmount) > 0.01f)
    {
        InterpSpeed *= 2.0f;
    }
    if (FMath::Abs(TargetLookUpAmount) < 0.01f && FMath::Abs(LookUpAmount) > 0.01f)
    {
        InterpSpeed *= 2.0f;
    }

    LeanSidesAmount = FMath::FInterpTo(LeanSidesAmount, TargetLeanSidesAmount, DeltaTime, InterpSpeed);
    LookUpAmount = FMath::FInterpTo(LookUpAmount, TargetLookUpAmount, DeltaTime, InterpSpeed);

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
        ArmGroupAnimationWeightMultiplier = FMath::FInterpTo(
            ArmGroupAnimationWeightMultiplier,
            0.7f,
            DeltaTime,
            2.0f
        );
    }

    LastCameraYawValue = CurrentControlRotation.Yaw;
    LastCameraPitchValue = CurrentControlRotation.Pitch;

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

void ASuspenseCharacter::SetupPlayerInputComponent(UInputComponent* IC)
{
    Super::SetupPlayerInputComponent(IC);
}

// ========================================
// Cinematic Camera Control Implementation
// ========================================

void ASuspenseCharacter::SetCameraFOV(float NewFOV)
{
    if (Camera)
    {
        Camera->SetFieldOfView(FMath::Clamp(NewFOV, 5.0f, 170.0f));
    }
}

void ASuspenseCharacter::SetCameraFocalLength(float NewFocalLength)
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

void ASuspenseCharacter::SetCameraAperture(float NewAperture)
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

void ASuspenseCharacter::SetDepthOfFieldEnabled(bool bEnabled)
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

void ASuspenseCharacter::SetCameraFocusDistance(float Distance)
{
    if (Camera)
    {
        Camera->FocusSettings.ManualFocusDistance = Distance;
        Camera->PostProcessSettings.DepthOfFieldFocalDistance = Distance;
    }
}

void ASuspenseCharacter::ApplyCinematicPreset(bool bEnableDOF, float Aperture, float FocusDistance)
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
