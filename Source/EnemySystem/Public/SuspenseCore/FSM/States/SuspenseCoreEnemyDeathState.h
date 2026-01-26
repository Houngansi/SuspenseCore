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

    virtual void OnEnterState(ASuspenseCoreEnemyCharacter* Enemy) override;
    virtual void OnExitState(ASuspenseCoreEnemyCharacter* Enemy) override;
    virtual void OnTickState(ASuspenseCoreEnemyCharacter* Enemy, float DeltaTime) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Death")
    float DespawnDelay;

    UPROPERTY(EditDefaultsOnly, Category = "Death")
    bool bEnableRagdoll;

    void EnableRagdoll(ASuspenseCoreEnemyCharacter* Enemy);
    void ScheduleDespawn(ASuspenseCoreEnemyCharacter* Enemy);
};
