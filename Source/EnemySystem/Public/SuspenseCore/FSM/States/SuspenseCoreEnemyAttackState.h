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

    virtual void OnEnterState(ASuspenseCoreEnemyCharacter* Enemy) override;
    virtual void OnExitState(ASuspenseCoreEnemyCharacter* Enemy) override;
    virtual void OnTickState(ASuspenseCoreEnemyCharacter* Enemy, float DeltaTime) override;
    virtual void OnFSMEvent(ASuspenseCoreEnemyCharacter* Enemy, const FGameplayTag& EventTag, AActor* Instigator) override;

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

    void PerformAttack(ASuspenseCoreEnemyCharacter* Enemy);
    void RotateTowardsTarget(ASuspenseCoreEnemyCharacter* Enemy, float DeltaTime);
    bool IsTargetInAttackRange(ASuspenseCoreEnemyCharacter* Enemy) const;
};
