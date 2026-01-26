#include "SuspenseCore/FSM/States/SuspenseCoreEnemyPatrolState.h"
#include "SuspenseCore/Characters/SuspenseCoreEnemyCharacter.h"
#include "SuspenseCore/Tags/SuspenseCoreEnemyTags.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "EnemySystem.h"

USuspenseCoreEnemyPatrolState::USuspenseCoreEnemyPatrolState()
{
    StateTag = SuspenseCoreEnemyTags::State::Patrol;
    PatrolSpeed = 200.0f;
    PatrolRadius = 800.0f;
    WaitTimeAtPoint = 2.0f;
    AcceptanceRadius = 50.0f;
    CurrentPointIndex = 0;
    bIsWaiting = false;
    bIsMoving = false;
}

void USuspenseCoreEnemyPatrolState::OnEnterState(ASuspenseCoreEnemyCharacter* Enemy)
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
        AIController->ReceiveMoveCompleted.AddDynamic(this, &USuspenseCoreEnemyPatrolState::OnMoveCompleted);
    }

    if (PatrolPoints.Num() == 0)
    {
        SpawnLocation = Enemy->GetActorLocation();
        GeneratePatrolPoints(Enemy);
    }

    CurrentPointIndex = 0;
    bIsWaiting = false;
    bIsMoving = false;

    MoveToNextPoint(Enemy);
}

void USuspenseCoreEnemyPatrolState::OnExitState(ASuspenseCoreEnemyCharacter* Enemy)
{
    if (CachedController.IsValid())
    {
        CachedController->ReceiveMoveCompleted.RemoveDynamic(this, &USuspenseCoreEnemyPatrolState::OnMoveCompleted);
        CachedController->StopMovement();
    }

    StopTimer(Enemy, FName(TEXT("PatrolWait")));

    Super::OnExitState(Enemy);
}

void USuspenseCoreEnemyPatrolState::OnTickState(ASuspenseCoreEnemyCharacter* Enemy, float DeltaTime)
{
    Super::OnTickState(Enemy, DeltaTime);
}

void USuspenseCoreEnemyPatrolState::OnFSMEvent(ASuspenseCoreEnemyCharacter* Enemy, const FGameplayTag& EventTag, AActor* Instigator)
{
    Super::OnFSMEvent(Enemy, EventTag, Instigator);

    if (EventTag == SuspenseCoreEnemyTags::Event::PlayerDetected)
    {
        SetCurrentTarget(Enemy, Instigator);
    }
    else if (EventTag == SuspenseCoreEnemyTags::Event::PatrolComplete && bIsWaiting)
    {
        bIsWaiting = false;
        MoveToNextPoint(Enemy);
    }
}

void USuspenseCoreEnemyPatrolState::GeneratePatrolPoints(ASuspenseCoreEnemyCharacter* Enemy)
{
    PatrolPoints.Empty();

    if (!Enemy)
    {
        return;
    }

    UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(Enemy->GetWorld());
    if (!NavSystem)
    {
        return;
    }

    int32 NumPoints = 4;
    float AngleStep = 360.0f / static_cast<float>(NumPoints);

    for (int32 Index = 0; Index < NumPoints; ++Index)
    {
        float Angle = FMath::DegreesToRadians(AngleStep * static_cast<float>(Index));
        FVector Offset(
            FMath::Cos(Angle) * PatrolRadius,
            FMath::Sin(Angle) * PatrolRadius,
            0.0f
        );

        FVector TestLocation = SpawnLocation + Offset;
        FNavLocation NavLocation;

        if (NavSystem->ProjectPointToNavigation(TestLocation, NavLocation))
        {
            PatrolPoints.Add(NavLocation.Location);
        }
    }

    if (PatrolPoints.Num() == 0)
    {
        PatrolPoints.Add(SpawnLocation);
    }
}

void USuspenseCoreEnemyPatrolState::MoveToNextPoint(ASuspenseCoreEnemyCharacter* Enemy)
{
    if (!Enemy || PatrolPoints.Num() == 0)
    {
        return;
    }

    AAIController* AIController = CachedController.Get();
    if (!AIController)
    {
        return;
    }

    FVector TargetLocation = PatrolPoints[CurrentPointIndex];

    bIsMoving = true;
    AIController->MoveToLocation(TargetLocation, AcceptanceRadius, true, true, false, true);
}

void USuspenseCoreEnemyPatrolState::OnReachedPatrolPoint(ASuspenseCoreEnemyCharacter* Enemy)
{
    bIsMoving = false;
    bIsWaiting = true;

    CurrentPointIndex = (CurrentPointIndex + 1) % PatrolPoints.Num();

    StartTimer(Enemy, FName(TEXT("PatrolWait")), WaitTimeAtPoint, false);
}

void USuspenseCoreEnemyPatrolState::OnMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result)
{
    if (Result == EPathFollowingResult::Success)
    {
        ASuspenseCoreEnemyCharacter* Enemy = Cast<ASuspenseCoreEnemyCharacter>(CachedController.IsValid() ? CachedController->GetPawn() : nullptr);
        OnReachedPatrolPoint(Enemy);
    }
}
