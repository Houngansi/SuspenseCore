#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/FSM/SuspenseCoreEnemyStateBase.h"
#include "SuspenseCoreEnemyIdleState.generated.h"

UCLASS()
class ENEMYSYSTEM_API USuspenseCoreEnemyIdleState : public USuspenseCoreEnemyStateBase
{
    GENERATED_BODY()

public:
    USuspenseCoreEnemyIdleState();

    virtual void OnEnterState(ASuspenseCoreEnemy* Enemy) override;
    virtual void OnExitState(ASuspenseCoreEnemy* Enemy) override;
    virtual void OnTickState(ASuspenseCoreEnemy* Enemy, float DeltaTime) override;
    virtual void OnFSMEvent(ASuspenseCoreEnemy* Enemy, const FGameplayTag& EventTag, AActor* Instigator) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Idle")
    float IdleTimeout;

    UPROPERTY(EditDefaultsOnly, Category = "Idle")
    float LookAroundInterval;

    float TimeSinceLastLook;
    FRotator OriginalRotation;
};
