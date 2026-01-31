#include "SuspenseCore/Animation/SuspenseCoreEnemyAnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

USuspenseCoreEnemyAnimInstance::USuspenseCoreEnemyAnimInstance()
{
}

void USuspenseCoreEnemyAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

    // Cache references
    APawn* OwnerPawn = TryGetPawnOwner();
    if (OwnerPawn)
    {
        CachedCharacter = Cast<ACharacter>(OwnerPawn);
        if (CachedCharacter.IsValid())
        {
            CachedMovementComponent = CachedCharacter->GetCharacterMovement();
        }
    }
}

void USuspenseCoreEnemyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    // Re-cache if needed
    if (!CachedCharacter.IsValid())
    {
        APawn* OwnerPawn = TryGetPawnOwner();
        if (OwnerPawn)
        {
            CachedCharacter = Cast<ACharacter>(OwnerPawn);
            if (CachedCharacter.IsValid())
            {
                CachedMovementComponent = CachedCharacter->GetCharacterMovement();
            }
        }
    }

    // CRITICAL: UpdateVelocityData MUST be called FIRST!
    // It calculates GroundSpeed which is used by UpdateMovementData
    UpdateVelocityData(DeltaSeconds);
    UpdateMovementData(DeltaSeconds);
    UpdateAimData(DeltaSeconds);
}

void USuspenseCoreEnemyAnimInstance::UpdateMovementData(float DeltaSeconds)
{
    if (!CachedMovementComponent.IsValid())
    {
        return;
    }

    UCharacterMovementComponent* MovementComp = CachedMovementComponent.Get();

    // Movement states
    bIsInAir = MovementComp->IsFalling();
    bIsFalling = bIsInAir && VerticalVelocity < 0.0f;
    bIsJumping = bIsInAir && VerticalVelocity > 0.0f;
    bIsOnGround = MovementComp->IsMovingOnGround();
    bIsCrouching = MovementComp->IsCrouching();

    // For AI, sprinting is when speed > walk speed
    bIsSprinting = GroundSpeed > MovementComp->MaxWalkSpeed * 0.9f;

    // Movement input - true when actually moving
    bHasMovementInput = GroundSpeed > 10.0f;

    // Sliding not used for basic AI
    bIsSliding = false;

    // Determine movement state enum
    if (bIsInAir)
    {
        MovementState = bIsFalling ? ESuspenseCoreMovementState::Falling : ESuspenseCoreMovementState::Jumping;
    }
    else if (bIsCrouching)
    {
        MovementState = ESuspenseCoreMovementState::Crouching;
    }
    else if (bIsSprinting)
    {
        MovementState = ESuspenseCoreMovementState::Sprinting;
    }
    else if (bHasMovementInput)
    {
        MovementState = ESuspenseCoreMovementState::Walking;
    }
    else
    {
        MovementState = ESuspenseCoreMovementState::Idle;
    }

    // Update max speeds from movement component
    MaxWalkSpeed = MovementComp->MaxWalkSpeed;
    MaxCrouchSpeed = MovementComp->MaxWalkSpeedCrouched;
    MaxSprintSpeed = MaxWalkSpeed * 1.5f;
}

void USuspenseCoreEnemyAnimInstance::UpdateVelocityData(float DeltaSeconds)
{
    APawn* OwnerPawn = TryGetPawnOwner();
    if (!OwnerPawn)
    {
        return;
    }

    // Get velocity from pawn
    const FVector Velocity = OwnerPawn->GetVelocity();

    // Calculate speeds
    Speed = Velocity.Size();
    GroundSpeed = FVector(Velocity.X, Velocity.Y, 0.0f).Size();
    VerticalVelocity = Velocity.Z;

    // Calculate normalized speed (0-2 range)
    if (MaxWalkSpeed > 0.0f)
    {
        NormalizedSpeed = FMath::Clamp(GroundSpeed / MaxWalkSpeed, 0.0f, 2.0f);
    }
    else
    {
        NormalizedSpeed = 0.0f;
    }

    // Movement magnitude (0-2 for state machine)
    Movement = NormalizedSpeed;

    // Calculate movement direction relative to actor facing
    if (GroundSpeed > 10.0f)
    {
        const FRotator ActorRotation = OwnerPawn->GetActorRotation();
        const FRotator VelocityRotation = Velocity.ToOrientationRotator();
        const FRotator DeltaRotation = (VelocityRotation - ActorRotation).GetNormalized();
        MovementDirection = DeltaRotation.Yaw;

        // Calculate forward/right components (-1 to 1)
        const FVector LocalVelocity = ActorRotation.UnrotateVector(Velocity);
        const float VelocityMagnitude = LocalVelocity.Size2D();
        if (VelocityMagnitude > KINDA_SMALL_NUMBER)
        {
            MoveForward = FMath::Clamp(LocalVelocity.X / VelocityMagnitude, -1.0f, 1.0f);
            MoveRight = FMath::Clamp(LocalVelocity.Y / VelocityMagnitude, -1.0f, 1.0f);
        }
        else
        {
            MoveForward = 0.0f;
            MoveRight = 0.0f;
        }
    }
    else
    {
        MovementDirection = 0.0f;
        MoveForward = 0.0f;
        MoveRight = 0.0f;
    }
}

void USuspenseCoreEnemyAnimInstance::UpdateAimData(float DeltaSeconds)
{
    APawn* OwnerPawn = TryGetPawnOwner();
    if (!OwnerPawn)
    {
        return;
    }

    // For AI, aim direction is based on control rotation or actor rotation
    const FRotator ControlRotation = OwnerPawn->GetControlRotation();
    const FRotator ActorRotation = OwnerPawn->GetActorRotation();

    // Calculate aim offset relative to actor facing
    const FRotator AimDelta = (ControlRotation - ActorRotation).GetNormalized();
    AimYaw = FMath::Clamp(AimDelta.Yaw, -90.0f, 90.0f);
    AimPitch = FMath::Clamp(AimDelta.Pitch, -90.0f, 90.0f);

    // Calculate lean based on movement direction change
    // Simple implementation - lean into turns
    const float YawDelta = ActorRotation.Yaw - LastYawForLean;
    LastYawForLean = ActorRotation.Yaw;

    const float TargetLean = FMath::Clamp(YawDelta * 0.5f, -15.0f, 15.0f);
    Lean = FMath::FInterpTo(Lean, TargetLean, DeltaSeconds, 5.0f);

    // Body pitch for slopes
    BodyPitch = 0.0f; // Could be extended for slope detection

    // Yaw offset (for turn in place)
    YawOffset = FMath::FInterpTo(YawOffset, 0.0f, DeltaSeconds, 4.0f);
}
