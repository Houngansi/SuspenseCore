#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/FSM/SuspenseCoreEnemyStateBase.h"
#include "SuspenseCoreEnemyDeathState.generated.h"

UCLASS()
class ENEMYSYSTEM_API USuspenseCoreEnemyDeathState : public USuspenseCoreEnemyStateBase
{
    GENERATED_BODY()

public:
    USuspenseCoreEnemyDeathState();

    virtual void OnEnterState(ASuspenseCoreEnemy* Enemy) override;
    virtual void OnExitState(ASuspenseCoreEnemy* Enemy) override;
    virtual void OnTickState(ASuspenseCoreEnemy* Enemy, float DeltaTime) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Death")
    float DespawnDelay;

    UPROPERTY(EditDefaultsOnly, Category = "Death")
    bool bEnableRagdoll;

    void EnableRagdoll(ASuspenseCoreEnemy* Enemy);
    void ScheduleDespawn(ASuspenseCoreEnemy* Enemy);
};
