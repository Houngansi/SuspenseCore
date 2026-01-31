#include "SuspenseCore/FSM/States/SuspenseCoreEnemyChaseState.h"
#include "SuspenseCore/Characters/SuspenseCoreEnemyCharacter.h"
#include "SuspenseCore/Tags/SuspenseCoreEnemyTags.h"
#include "SuspenseCore/FSM/SuspenseCoreEnemyFSMComponent.h"
#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "EnemySystem.h"

USuspenseCoreEnemyChaseState::USuspenseCoreEnemyChaseState()
{
    StateTag = SuspenseCoreEnemyTags::State::Chase;
    ChaseSpeed = 500.0f;
    AttackRange = 200.0f;
    LoseTargetDistance = 2000.0f;
    LoseTargetTime = 5.0f;
    PathUpdateInterval = 0.5f;
    TimeSinceTargetSeen = 0.0f;
    TimeSinceLastPathUpdate = 0.0f;
}

void USuspenseCoreEnemyChaseState::OnEnterState(ASuspenseCoreEnemyCharacter* Enemy)
{
    Super::OnEnterState(Enemy);

    if (!Enemy)
    {
        return;
    }

    AAIController* AIController = Cast<AAIController>(Enemy->GetController());
    if (AIController)
    {
        CachedController = AIController;
    }

    // Configure movement component for chase (CRITICAL for animation!)
    ConfigureMovement(Enemy);

    TimeSinceTargetSeen = 0.0f;
    TimeSinceLastPathUpdate = PathUpdateInterval;

    AActor* Target = GetCurrentTarget(Enemy);
    if (Target)
    {
        LastKnownTargetLocation = Target->GetActorLocation();
    }
}

void USuspenseCoreEnemyChaseState::OnExitState(ASuspenseCoreEnemyCharacter* Enemy)
{
    if (CachedController.IsValid())
    {
        CachedController->StopMovement();
    }

    Super::OnExitState(Enemy);
}

void USuspenseCoreEnemyChaseState::OnTickState(ASuspenseCoreEnemyCharacter* Enemy, float DeltaTime)
{
    Super::OnTickState(Enemy, DeltaTime);

    UpdateChase(Enemy, DeltaTime);
}

void USuspenseCoreEnemyChaseState::OnFSMEvent(ASuspenseCoreEnemyCharacter* Enemy, const FGameplayTag& EventTag, AActor* Instigator)
{
    Super::OnFSMEvent(Enemy, EventTag, Instigator);
}

void USuspenseCoreEnemyChaseState::UpdateChase(ASuspenseCoreEnemyCharacter* Enemy, float DeltaTime)
{
    if (!Enemy)
    {
        return;
    }

    AActor* Target = GetCurrentTarget(Enemy);

    if (!Target)
    {
        if (FSMComponent)
        {
            FSMComponent->SendFSMEvent(SuspenseCoreEnemyTags::Event::PlayerLost, nullptr);
        }
        return;
    }

    float DistanceToTarget = GetDistanceToTarget(Enemy, Target);

    if (DistanceToTarget <= AttackRange)
    {
        if (FSMComponent)
        {
            FSMComponent->SendFSMEvent(SuspenseCoreEnemyTags::Event::TargetInRange, Target);
        }
        return;
    }

    if (CanSeeTarget(Enemy, Target))
    {
        TimeSinceTargetSeen = 0.0f;
        LastKnownTargetLocation = Target->GetActorLocation();
    }
    else
    {
        TimeSinceTargetSeen += DeltaTime;

        if (TimeSinceTargetSeen >= LoseTargetTime)
        {
            if (FSMComponent)
            {
                FSMComponent->SendFSMEvent(SuspenseCoreEnemyTags::Event::PlayerLost, nullptr);
            }
            return;
        }
    }

    if (DistanceToTarget > LoseTargetDistance)
    {
        if (FSMComponent)
        {
            FSMComponent->SendFSMEvent(SuspenseCoreEnemyTags::Event::PlayerLost, nullptr);
        }
        return;
    }

    TimeSinceLastPathUpdate += DeltaTime;
    if (TimeSinceLastPathUpdate >= PathUpdateInterval)
    {
        TimeSinceLastPathUpdate = 0.0f;
        MoveToTarget(Enemy, LastKnownTargetLocation);
    }
}

void USuspenseCoreEnemyChaseState::MoveToTarget(ASuspenseCoreEnemyCharacter* Enemy, const FVector& TargetLocation)
{
    AAIController* AIController = CachedController.Get();
    if (!AIController)
    {
        UE_LOG(LogEnemySystem, Warning, TEXT("[%s] ChaseState: No AIController!"), *GetNameSafe(Enemy));
        return;
    }

    // Debug: Log movement attempt with velocity
    if (UCharacterMovementComponent* MovementComp = Enemy ? Enemy->GetCharacterMovement() : nullptr)
    {
        const FVector Velocity = Enemy->GetVelocity();
        UE_LOG(LogEnemySystem, Log, TEXT("[%s] ChaseState: MoveToTarget - MaxWalkSpeed=%.1f, CurrentVelocity=%.1f, Target=%s"),
            *GetNameSafe(Enemy),
            MovementComp->MaxWalkSpeed,
            Velocity.Size(),
            *TargetLocation.ToString());
    }

    // Use pathfinding with projection to NavMesh (5th param = true)
    EPathFollowingRequestResult::Type Result = AIController->MoveToLocation(
        TargetLocation,
        AttackRange * 0.8f,  // AcceptanceRadius
        true,                // bStopOnOverlap
        true,                // bUsePathfinding
        true,                // bProjectDestinationToNavigation - CRITICAL!
        true                 // bCanStrafe
    );

    if (Result == EPathFollowingRequestResult::Failed)
    {
        UE_LOG(LogEnemySystem, Warning, TEXT("[%s] ChaseState: MoveToLocation FAILED! Check NavMesh!"), *GetNameSafe(Enemy));
    }
    else if (Result == EPathFollowingRequestResult::AlreadyAtGoal)
    {
        UE_LOG(LogEnemySystem, Verbose, TEXT("[%s] ChaseState: Already at goal"), *GetNameSafe(Enemy));
    }
}

void USuspenseCoreEnemyChaseState::ConfigureMovement(ASuspenseCoreEnemyCharacter* Enemy)
{
    if (!Enemy)
    {
        return;
    }

    UCharacterMovementComponent* MovementComp = Enemy->GetCharacterMovement();
    if (!MovementComp)
    {
        UE_LOG(LogEnemySystem, Warning, TEXT("[%s] ChaseState: No CharacterMovementComponent!"), *GetNameSafe(Enemy));
        return;
    }

    // Enable movement component tick
    MovementComp->SetComponentTickEnabled(true);

    // Set walking mode (NavWalking for better pathfinding)
    MovementComp->SetMovementMode(MOVE_NavWalking);

    // Set chase speed
    MovementComp->MaxWalkSpeed = ChaseSpeed;

    // CRITICAL: Enable orientation to movement direction for walk/run animation
    MovementComp->bOrientRotationToMovement = true;
    MovementComp->RotationRate = FRotator(0.0f, 360.0f, 0.0f);

    // Disable controller rotation (let movement component handle it)
    Enemy->bUseControllerRotationYaw = false;

    // Movement physics for chase (faster acceleration)
    MovementComp->MaxAcceleration = 2048.0f;
    MovementComp->BrakingDecelerationWalking = 1024.0f;
    MovementComp->GroundFriction = 8.0f;
    MovementComp->bRequestedMoveUseAcceleration = true;

    UE_LOG(LogEnemySystem, Log, TEXT("[%s] ChaseState: Movement configured - Speed=%.1f, OrientToMovement=true"),
        *GetNameSafe(Enemy), ChaseSpeed);
}
