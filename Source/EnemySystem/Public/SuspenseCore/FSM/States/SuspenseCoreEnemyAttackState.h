#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/FSM/SuspenseCoreEnemyStateBase.h"
#include "SuspenseCoreEnemyAttackState.generated.h"

UCLASS()
class ENEMYSYSTEM_API USuspenseCoreEnemyAttackState : public USuspenseCoreEnemyStateBase
{
    GENERATED_BODY()

public:
    USuspenseCoreEnemyAttackState();

    virtual void OnEnterState(ASuspenseCoreEnemy* Enemy) override;
    virtual void OnExitState(ASuspenseCoreEnemy* Enemy) override;
    virtual void OnTickState(ASuspenseCoreEnemy* Enemy, float DeltaTime) override;
    virtual void OnFSMEvent(ASuspenseCoreEnemy* Enemy, const FGameplayTag& EventTag, AActor* Instigator) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Attack")
    float AttackRange;

    UPROPERTY(EditDefaultsOnly, Category = "Attack")
    float AttackCooldown;

    UPROPERTY(EditDefaultsOnly, Category = "Attack")
    float RotationSpeed;

    UPROPERTY(EditDefaultsOnly, Category = "Attack")
    float LoseTargetTime;

    float TimeSinceLastAttack;
    float TimeSinceTargetSeen;

    void PerformAttack(ASuspenseCoreEnemy* Enemy);
    void RotateTowardsTarget(ASuspenseCoreEnemy* Enemy, float DeltaTime);
    bool IsTargetInAttackRange(ASuspenseCoreEnemy* Enemy) const;
};
