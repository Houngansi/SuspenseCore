#include "SuspenseCore/FSM/SuspenseCoreEnemyStateBase.h"
#include "SuspenseCore/FSM/SuspenseCoreEnemyFSMComponent.h"
#include "SuspenseCore/Characters/SuspenseCoreEnemyCharacter.h"
#include "AIController.h"
#include "EnemySystem.h"

USuspenseCoreEnemyStateBase::USuspenseCoreEnemyStateBase()
{
    FSMComponent = nullptr;
}

void USuspenseCoreEnemyStateBase::OnEnterState(ASuspenseCoreEnemyCharacter* Enemy)
{
    UE_LOG(LogEnemySystem, Verbose, TEXT("[%s] Entering state: %s"),
        Enemy ? *Enemy->GetName() : TEXT("None"),
        *StateTag.ToString());
}

void USuspenseCoreEnemyStateBase::OnExitState(ASuspenseCoreEnemyCharacter* Enemy)
{
    UE_LOG(LogEnemySystem, Verbose, TEXT("[%s] Exiting state: %s"),
        Enemy ? *Enemy->GetName() : TEXT("None"),
        *StateTag.ToString());
}

void USuspenseCoreEnemyStateBase::OnTickState(ASuspenseCoreEnemyCharacter* Enemy, float DeltaTime)
{
}

void USuspenseCoreEnemyStateBase::OnFSMEvent(ASuspenseCoreEnemyCharacter* Enemy, const FGameplayTag& EventTag, AActor* Instigator)
{
}

FGameplayTag USuspenseCoreEnemyStateBase::GetStateTag() const
{
    return StateTag;
}

void USuspenseCoreEnemyStateBase::SetFSMComponent(USuspenseCoreEnemyFSMComponent* InFSMComponent)
{
    FSMComponent = InFSMComponent;
}

void USuspenseCoreEnemyStateBase::RequestStateChange(const FGameplayTag& NewStateTag)
{
    if (FSMComponent)
    {
        FSMComponent->RequestStateChange(NewStateTag);
    }
}

void USuspenseCoreEnemyStateBase::StartTimer(ASuspenseCoreEnemyCharacter* Enemy, FName TimerName, float Duration, bool bLoop)
{
    if (FSMComponent)
    {
        FSMComponent->StartStateTimer(TimerName, Duration, bLoop);
    }
}

void USuspenseCoreEnemyStateBase::StopTimer(ASuspenseCoreEnemyCharacter* Enemy, FName TimerName)
{
    if (FSMComponent)
    {
        FSMComponent->StopStateTimer(TimerName);
    }
}

bool USuspenseCoreEnemyStateBase::CanSeeTarget(ASuspenseCoreEnemyCharacter* Enemy, AActor* Target) const
{
    if (!Enemy || !Target)
    {
        return false;
    }

    FVector StartLocation = Enemy->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
    FVector EndLocation = Target->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(Enemy);

    bool bHit = Enemy->GetWorld()->LineTraceSingleByChannel(
        HitResult,
        StartLocation,
        EndLocation,
        ECollisionChannel::ECC_Visibility,
        QueryParams
    );

    if (!bHit)
    {
        return true;
    }

    return HitResult.GetActor() == Target;
}

float USuspenseCoreEnemyStateBase::GetDistanceToTarget(ASuspenseCoreEnemyCharacter* Enemy, AActor* Target) const
{
    if (!Enemy || !Target)
    {
        return MAX_FLT;
    }

    return FVector::Dist(Enemy->GetActorLocation(), Target->GetActorLocation());
}

AActor* USuspenseCoreEnemyStateBase::GetCurrentTarget(ASuspenseCoreEnemyCharacter* Enemy) const
{
    if (!Enemy)
    {
        return nullptr;
    }
    return Enemy->GetCurrentTarget();
}

void USuspenseCoreEnemyStateBase::SetCurrentTarget(ASuspenseCoreEnemyCharacter* Enemy, AActor* NewTarget)
{
    if (Enemy)
    {
        Enemy->SetCurrentTarget(NewTarget);
    }
}
