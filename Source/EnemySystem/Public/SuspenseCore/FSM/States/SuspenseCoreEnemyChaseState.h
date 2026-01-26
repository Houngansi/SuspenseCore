#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/FSM/SuspenseCoreEnemyStateBase.h"
#include "SuspenseCoreEnemyChaseState.generated.h"

UCLASS()
class ENEMYSYSTEM_API USuspenseCoreEnemyChaseState : public USuspenseCoreEnemyStateBase
{
    GENERATED_BODY()

public:
    USuspenseCoreEnemyChaseState();

    virtual void OnEnterState(ASuspenseCoreEnemy* Enemy) override;
    virtual void OnExitState(ASuspenseCoreEnemy* Enemy) override;
    virtual void OnTickState(ASuspenseCoreEnemy* Enemy, float DeltaTime) override;
    virtual void OnFSMEvent(ASuspenseCoreEnemy* Enemy, const FGameplayTag& EventTag, AActor* Instigator) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Chase")
    float ChaseSpeed;

    UPROPERTY(EditDefaultsOnly, Category = "Chase")
    float AttackRange;

    UPROPERTY(EditDefaultsOnly, Category = "Chase")
    float LoseTargetDistance;

    UPROPERTY(EditDefaultsOnly, Category = "Chase")
    float LoseTargetTime;

    UPROPERTY(EditDefaultsOnly, Category = "Chase")
    float PathUpdateInterval;

    float TimeSinceTargetSeen;
    float TimeSinceLastPathUpdate;
    FVector LastKnownTargetLocation;

    TWeakObjectPtr<class AAIController> CachedController;

    void UpdateChase(ASuspenseCoreEnemy* Enemy, float DeltaTime);
    void MoveToTarget(ASuspenseCoreEnemy* Enemy, const FVector& TargetLocation);
};
