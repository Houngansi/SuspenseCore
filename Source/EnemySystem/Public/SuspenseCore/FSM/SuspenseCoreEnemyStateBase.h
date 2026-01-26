#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreEnemyStateBase.generated.h"

class ASuspenseCoreEnemy;
class USuspenseCoreEnemyFSMComponent;

UCLASS(Abstract, Blueprintable)
class ENEMYSYSTEM_API USuspenseCoreEnemyStateBase : public UObject
{
    GENERATED_BODY()

public:
    USuspenseCoreEnemyStateBase();

    virtual void OnEnterState(ASuspenseCoreEnemy* Enemy);
    virtual void OnExitState(ASuspenseCoreEnemy* Enemy);
    virtual void OnTickState(ASuspenseCoreEnemy* Enemy, float DeltaTime);
    virtual void OnFSMEvent(ASuspenseCoreEnemy* Enemy, const FGameplayTag& EventTag, AActor* Instigator);

    FGameplayTag GetStateTag() const;
    void SetFSMComponent(USuspenseCoreEnemyFSMComponent* InFSMComponent);

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State")
    FGameplayTag StateTag;

    UPROPERTY()
    TObjectPtr<USuspenseCoreEnemyFSMComponent> FSMComponent;

    void RequestStateChange(const FGameplayTag& NewStateTag);
    void StartTimer(ASuspenseCoreEnemy* Enemy, FName TimerName, float Duration, bool bLoop);
    void StopTimer(ASuspenseCoreEnemy* Enemy, FName TimerName);

    bool CanSeeTarget(ASuspenseCoreEnemy* Enemy, AActor* Target) const;
    float GetDistanceToTarget(ASuspenseCoreEnemy* Enemy, AActor* Target) const;
    AActor* GetCurrentTarget(ASuspenseCoreEnemy* Enemy) const;
    void SetCurrentTarget(ASuspenseCoreEnemy* Enemy, AActor* NewTarget);
};
