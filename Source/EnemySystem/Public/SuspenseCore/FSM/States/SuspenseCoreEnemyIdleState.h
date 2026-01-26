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

    virtual void OnEnterState(ASuspenseCoreEnemyCharacter* Enemy) override;
    virtual void OnExitState(ASuspenseCoreEnemyCharacter* Enemy) override;
    virtual void OnTickState(ASuspenseCoreEnemyCharacter* Enemy, float DeltaTime) override;
    virtual void OnFSMEvent(ASuspenseCoreEnemyCharacter* Enemy, const FGameplayTag& EventTag, AActor* Instigator) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Idle")
    float IdleTimeout;

    UPROPERTY(EditDefaultsOnly, Category = "Idle")
    float LookAroundInterval;

    float TimeSinceLastLook;
    FRotator OriginalRotation;
};
