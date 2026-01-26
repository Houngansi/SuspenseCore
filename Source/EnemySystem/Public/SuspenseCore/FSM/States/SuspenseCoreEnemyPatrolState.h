#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/FSM/SuspenseCoreEnemyStateBase.h"
#include "Navigation/PathFollowingComponent.h"
#include "SuspenseCoreEnemyPatrolState.generated.h"

UCLASS()
class ENEMYSYSTEM_API USuspenseCoreEnemyPatrolState : public USuspenseCoreEnemyStateBase
{
    GENERATED_BODY()

public:
    USuspenseCoreEnemyPatrolState();

    virtual void OnEnterState(ASuspenseCoreEnemyCharacter* Enemy) override;
    virtual void OnExitState(ASuspenseCoreEnemyCharacter* Enemy) override;
    virtual void OnTickState(ASuspenseCoreEnemyCharacter* Enemy, float DeltaTime) override;
    virtual void OnFSMEvent(ASuspenseCoreEnemyCharacter* Enemy, const FGameplayTag& EventTag, AActor* Instigator) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Patrol")
    float PatrolSpeed;

    UPROPERTY(EditDefaultsOnly, Category = "Patrol")
    float PatrolRadius;

    UPROPERTY(EditDefaultsOnly, Category = "Patrol")
    float WaitTimeAtPoint;

    UPROPERTY(EditDefaultsOnly, Category = "Patrol")
    float AcceptanceRadius;

    TArray<FVector> PatrolPoints;
    int32 CurrentPointIndex;
    FVector SpawnLocation;
    bool bIsWaiting;
    bool bIsMoving;

    void GeneratePatrolPoints(ASuspenseCoreEnemyCharacter* Enemy);
    void MoveToNextPoint(ASuspenseCoreEnemyCharacter* Enemy);
    void OnReachedPatrolPoint(ASuspenseCoreEnemyCharacter* Enemy);

    UFUNCTION()
    void OnMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result);

    TWeakObjectPtr<class AAIController> CachedController;
};
