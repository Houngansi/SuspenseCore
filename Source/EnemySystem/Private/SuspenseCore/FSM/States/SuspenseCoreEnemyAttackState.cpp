#include "SuspenseCore/FSM/States/SuspenseCoreEnemyAttackState.h"
#include "SuspenseCore/Characters/SuspenseCoreEnemy.h"
#include "SuspenseCore/Tags/SuspenseCoreEnemyTags.h"
#include "SuspenseCore/FSM/SuspenseCoreEnemyFSMComponent.h"
#include "EnemySystem.h"

USuspenseCoreEnemyAttackState::USuspenseCoreEnemyAttackState()
{
    StateTag = SuspenseCoreEnemyTags::State::Attack;
    AttackRange = 200.0f;
    AttackCooldown = 1.5f;
    RotationSpeed = 360.0f;
    LoseTargetTime = 3.0f;
    TimeSinceLastAttack = 0.0f;
    TimeSinceTargetSeen = 0.0f;
}

void USuspenseCoreEnemyAttackState::OnEnterState(ASuspenseCoreEnemy* Enemy)
{
    Super::OnEnterState(Enemy);

    if (Enemy)
    {
        Enemy->StopMovement();
    }

    TimeSinceLastAttack = AttackCooldown;
    TimeSinceTargetSeen = 0.0f;
}

void USuspenseCoreEnemyAttackState::OnExitState(ASuspenseCoreEnemy* Enemy)
{
    Super::OnExitState(Enemy);
}

void USuspenseCoreEnemyAttackState::OnTickState(ASuspenseCoreEnemy* Enemy, float DeltaTime)
{
    Super::OnTickState(Enemy, DeltaTime);

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

    if (CanSeeTarget(Enemy, Target))
    {
        TimeSinceTargetSeen = 0.0f;
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

    if (!IsTargetInAttackRange(Enemy))
    {
        if (FSMComponent)
        {
            FSMComponent->SendFSMEvent(SuspenseCoreEnemyTags::Event::TargetOutOfRange, Target);
        }
        return;
    }

    RotateTowardsTarget(Enemy, DeltaTime);

    TimeSinceLastAttack += DeltaTime;
    if (TimeSinceLastAttack >= AttackCooldown)
    {
        PerformAttack(Enemy);
        TimeSinceLastAttack = 0.0f;
    }
}

void USuspenseCoreEnemyAttackState::OnFSMEvent(ASuspenseCoreEnemy* Enemy, const FGameplayTag& EventTag, AActor* Instigator)
{
    Super::OnFSMEvent(Enemy, EventTag, Instigator);
}

void USuspenseCoreEnemyAttackState::PerformAttack(ASuspenseCoreEnemy* Enemy)
{
    if (!Enemy)
    {
        return;
    }

    Enemy->ExecuteAttack();

    UE_LOG(LogEnemySystem, Verbose, TEXT("[%s] Performing attack"), *Enemy->GetName());
}

void USuspenseCoreEnemyAttackState::RotateTowardsTarget(ASuspenseCoreEnemy* Enemy, float DeltaTime)
{
    if (!Enemy)
    {
        return;
    }

    AActor* Target = GetCurrentTarget(Enemy);
    if (!Target)
    {
        return;
    }

    FVector Direction = Target->GetActorLocation() - Enemy->GetActorLocation();
    Direction.Z = 0.0f;
    Direction.Normalize();

    FRotator TargetRotation = Direction.Rotation();
    FRotator CurrentRotation = Enemy->GetActorRotation();

    FRotator NewRotation = FMath::RInterpConstantTo(
        CurrentRotation,
        TargetRotation,
        DeltaTime,
        RotationSpeed
    );

    Enemy->SetActorRotation(NewRotation);
}

bool USuspenseCoreEnemyAttackState::IsTargetInAttackRange(ASuspenseCoreEnemy* Enemy) const
{
    AActor* Target = GetCurrentTarget(Enemy);
    if (!Target)
    {
        return false;
    }

    return GetDistanceToTarget(Enemy, Target) <= AttackRange;
}
