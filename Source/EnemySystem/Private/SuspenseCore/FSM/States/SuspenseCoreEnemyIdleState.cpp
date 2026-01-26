#include "SuspenseCore/FSM/States/SuspenseCoreEnemyIdleState.h"
#include "SuspenseCore/Characters/SuspenseCoreEnemy.h"
#include "SuspenseCore/Tags/SuspenseCoreEnemyTags.h"
#include "EnemySystem.h"

USuspenseCoreEnemyIdleState::USuspenseCoreEnemyIdleState()
{
    StateTag = SuspenseCoreEnemyTags::State::Idle;
    IdleTimeout = 5.0f;
    LookAroundInterval = 3.0f;
    TimeSinceLastLook = 0.0f;
}

void USuspenseCoreEnemyIdleState::OnEnterState(ASuspenseCoreEnemy* Enemy)
{
    Super::OnEnterState(Enemy);

    if (Enemy)
    {
        OriginalRotation = Enemy->GetActorRotation();
        Enemy->StopMovement();
    }

    TimeSinceLastLook = 0.0f;

    if (IdleTimeout > 0.0f)
    {
        StartTimer(Enemy, FName(TEXT("IdleTimeout")), IdleTimeout, false);
    }
}

void USuspenseCoreEnemyIdleState::OnExitState(ASuspenseCoreEnemy* Enemy)
{
    StopTimer(Enemy, FName(TEXT("IdleTimeout")));
    Super::OnExitState(Enemy);
}

void USuspenseCoreEnemyIdleState::OnTickState(ASuspenseCoreEnemy* Enemy, float DeltaTime)
{
    Super::OnTickState(Enemy, DeltaTime);

    if (!Enemy)
    {
        return;
    }

    TimeSinceLastLook += DeltaTime;

    if (TimeSinceLastLook >= LookAroundInterval)
    {
        TimeSinceLastLook = 0.0f;

        float RandomYaw = FMath::RandRange(-60.0f, 60.0f);
        FRotator NewRotation = OriginalRotation;
        NewRotation.Yaw += RandomYaw;

        Enemy->SetActorRotation(FMath::RInterpTo(
            Enemy->GetActorRotation(),
            NewRotation,
            DeltaTime,
            2.0f
        ));
    }
}

void USuspenseCoreEnemyIdleState::OnFSMEvent(ASuspenseCoreEnemy* Enemy, const FGameplayTag& EventTag, AActor* Instigator)
{
    Super::OnFSMEvent(Enemy, EventTag, Instigator);

    if (EventTag == SuspenseCoreEnemyTags::Event::PlayerDetected)
    {
        SetCurrentTarget(Enemy, Instigator);
    }
}
