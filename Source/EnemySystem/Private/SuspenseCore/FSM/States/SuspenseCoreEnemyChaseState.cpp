#include "SuspenseCore/FSM/States/SuspenseCoreEnemyChaseState.h"
#include "SuspenseCore/Characters/SuspenseCoreEnemyCharacter.h"
#include "SuspenseCore/Tags/SuspenseCoreEnemyTags.h"
#include "SuspenseCore/FSM/SuspenseCoreEnemyFSMComponent.h"
#include "AIController.h"
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
        return;
    }

    AIController->MoveToLocation(TargetLocation, AttackRange * 0.8f, true, true, false, true);
}
