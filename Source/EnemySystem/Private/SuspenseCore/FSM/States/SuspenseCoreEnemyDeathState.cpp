#include "SuspenseCore/FSM/States/SuspenseCoreEnemyDeathState.h"
#include "SuspenseCore/Characters/SuspenseCoreEnemyCharacter.h"
#include "SuspenseCore/Tags/SuspenseCoreEnemyTags.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnemySystem.h"

USuspenseCoreEnemyDeathState::USuspenseCoreEnemyDeathState()
{
    StateTag = SuspenseCoreEnemyTags::State::Death;
    DespawnDelay = 10.0f;
    bEnableRagdoll = true;
}

void USuspenseCoreEnemyDeathState::OnEnterState(ASuspenseCoreEnemyCharacter* Enemy)
{
    Super::OnEnterState(Enemy);

    if (!Enemy)
    {
        return;
    }

    Enemy->StopMovement();

    if (UCharacterMovementComponent* MovementComp = Enemy->GetCharacterMovement())
    {
        MovementComp->DisableMovement();
        MovementComp->StopMovementImmediately();
    }

    if (UCapsuleComponent* CapsuleComp = Enemy->GetCapsuleComponent())
    {
        CapsuleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    if (bEnableRagdoll)
    {
        EnableRagdoll(Enemy);
    }

    if (AController* Controller = Enemy->GetController())
    {
        Enemy->DetachFromControllerPendingDestroy();
    }

    ScheduleDespawn(Enemy);

    UE_LOG(LogEnemySystem, Log, TEXT("[%s] Entered death state"), *Enemy->GetName());
}

void USuspenseCoreEnemyDeathState::OnExitState(ASuspenseCoreEnemyCharacter* Enemy)
{
    Super::OnExitState(Enemy);
}

void USuspenseCoreEnemyDeathState::OnTickState(ASuspenseCoreEnemyCharacter* Enemy, float DeltaTime)
{
    Super::OnTickState(Enemy, DeltaTime);
}

void USuspenseCoreEnemyDeathState::EnableRagdoll(ASuspenseCoreEnemyCharacter* Enemy)
{
    if (!Enemy)
    {
        return;
    }

    USkeletalMeshComponent* MeshComp = Enemy->GetMesh();
    if (MeshComp)
    {
        MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        MeshComp->SetSimulatePhysics(true);
    }
}

void USuspenseCoreEnemyDeathState::ScheduleDespawn(ASuspenseCoreEnemyCharacter* Enemy)
{
    if (!Enemy || DespawnDelay <= 0.0f)
    {
        return;
    }

    FTimerHandle DespawnTimerHandle;
    Enemy->GetWorldTimerManager().SetTimer(
        DespawnTimerHandle,
        [Enemy]()
        {
            if (Enemy && IsValid(Enemy))
            {
                Enemy->Destroy();
            }
        },
        DespawnDelay,
        false
    );
}
