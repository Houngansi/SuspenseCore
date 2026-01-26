#pragma once

#include "CoreMinimal.h"
#include "Core/Enemy/FSM/MedComEnemyState.h"
#include "Navigation/PathFollowingComponent.h"
#include "MedComChaseState.generated.h"

class AAIController;
class AMedComEnemyCharacter;

/**
 * State.Chase – преследование ближайшего player-pawn’а
 *   – Full-LOD: обычный MoveTo + CharacterMovement
 *   – Reduced/Minimal: перемещение через CrowdManager
 *   – Sleep: не активируется
 */
UCLASS()
class MEDCOMCORE_API UMedComChaseState : public UMedComEnemyState
{
    GENERATED_BODY()

public:
    UMedComChaseState();

    // MedComEnemyState
    virtual void OnEnter (AMedComEnemyCharacter* Owner) override;
    virtual void OnExit  (AMedComEnemyCharacter* Owner) override;
    virtual void ProcessTick(AMedComEnemyCharacter* Owner,float DeltaTime) override;
    virtual void OnEvent (AMedComEnemyCharacter* Owner,EEnemyEvent Event,AActor* Instigator) override;

private:
    /* — helpers — */
    void              StartMoveToTarget(AMedComEnemyCharacter* Owner);
    bool              NeedRepath(float Now) const;
    void              ConfigureMovement(AMedComEnemyCharacter* Owner) const;
    bool              CanSeePawn(const AMedComEnemyCharacter* Owner,const APawn* Target) const;

    /* — cached — */
    TWeakObjectPtr<AAIController> Controller;
    TWeakObjectPtr<APawn>         Target;

    /* — tunables (DataAsset override) — */
    float ChaseSpeed        = 600.f;
    float AcceptanceRadius  = 120.f;
    float RepathInterval    = 0.4f;
    float RepathDistance    = 250.f;   // uu
    float LoseSightTime     = 4.f;
    float AttackRange       = 250.f;

    /* — runtime — */
    float LastRepathTime    = 0.f;
    float TimeSinceLost     = 0.f;
    FVector LastPathGoal    = FVector::ZeroVector;
};
