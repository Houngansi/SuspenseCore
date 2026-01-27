#include "SuspenseCore/Animation/SuspenseCoreEnemyAnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

USuspenseCoreEnemyAnimInstance::USuspenseCoreEnemyAnimInstance()
{
    MovingThreshold = 10.0f;
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

    UpdateLocomotionData(DeltaSeconds);
}

void USuspenseCoreEnemyAnimInstance::UpdateLocomotionData(float DeltaSeconds)
{
    APawn* OwnerPawn = TryGetPawnOwner();
    if (!OwnerPawn)
    {
        return;
    }

    // Get velocity from pawn (works for any pawn type)
    const FVector Velocity = OwnerPawn->GetVelocity();

    // Calculate speeds
    Speed = Velocity.Size();
    GroundSpeed = FVector(Velocity.X, Velocity.Y, 0.0f).Size();
    VerticalVelocity = Velocity.Z;

    // Calculate normalized speed
    float MaxSpeed = 450.0f; // Default
    if (CachedMovementComponent.IsValid())
    {
        MaxSpeed = CachedMovementComponent->MaxWalkSpeed;
        if (MaxSpeed <= 0.0f)
        {
            MaxSpeed = 450.0f;
        }
    }
    NormalizedSpeed = FMath::Clamp(GroundSpeed / MaxSpeed, 0.0f, 2.0f);

    // Is moving check
    bIsMoving = GroundSpeed > MovingThreshold;

    // Calculate movement direction relative to actor facing
    if (bIsMoving)
    {
        const FRotator ActorRotation = OwnerPawn->GetActorRotation();
        const FRotator VelocityRotation = Velocity.ToOrientationRotator();
        const FRotator DeltaRotation = (VelocityRotation - ActorRotation).GetNormalized();
        MovementDirection = DeltaRotation.Yaw;

        // Calculate forward/right components
        const FVector LocalVelocity = ActorRotation.UnrotateVector(Velocity);
        const float VelocityMagnitude = LocalVelocity.Size2D();
        if (VelocityMagnitude > KINDA_SMALL_NUMBER)
        {
            MoveForward = LocalVelocity.X / VelocityMagnitude;
            MoveRight = LocalVelocity.Y / VelocityMagnitude;
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

    // Air state
    if (CachedMovementComponent.IsValid())
    {
        bIsInAir = CachedMovementComponent->IsFalling();
        bIsFalling = bIsInAir && VerticalVelocity < 0.0f;
    }
    else
    {
        bIsInAir = false;
        bIsFalling = false;
    }
}
