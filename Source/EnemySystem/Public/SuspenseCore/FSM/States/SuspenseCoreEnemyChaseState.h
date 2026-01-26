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

    virtual void OnEnterState(ASuspenseCoreEnemyCharacter* Enemy) override;
    virtual void OnExitState(ASuspenseCoreEnemyCharacter* Enemy) override;
    virtual void OnTickState(ASuspenseCoreEnemyCharacter* Enemy, float DeltaTime) override;
    virtual void OnFSMEvent(ASuspenseCoreEnemyCharacter* Enemy, const FGameplayTag& EventTag, AActor* Instigator) override;

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

    void UpdateChase(ASuspenseCoreEnemyCharacter* Enemy, float DeltaTime);
    void MoveToTarget(ASuspenseCoreEnemyCharacter* Enemy, const FVector& TargetLocation);
};
