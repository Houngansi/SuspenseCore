#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "SuspenseCoreEnemyAIController.generated.h"

class UAIPerceptionComponent;
class ASuspenseCoreEnemy;

UCLASS()
class ENEMYSYSTEM_API ASuspenseCoreEnemyAIController : public AAIController
{
    GENERATED_BODY()

public:
    ASuspenseCoreEnemyAIController();

    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    TObjectPtr<UAIPerceptionComponent> AIPerceptionComponent;

    UPROPERTY(EditDefaultsOnly, Category = "AI")
    TSubclassOf<class ASuspenseCoreEnemyState> EnemyStateClass;

    UFUNCTION()
    void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

    TWeakObjectPtr<ASuspenseCoreEnemy> ControlledEnemy;
};
