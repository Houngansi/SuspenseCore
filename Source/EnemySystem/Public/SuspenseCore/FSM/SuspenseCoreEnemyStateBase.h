#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreEnemyStateBase.generated.h"

class ASuspenseCoreEnemyCharacter;
class USuspenseCoreEnemyFSMComponent;

UCLASS(Abstract, Blueprintable)
class ENEMYSYSTEM_API USuspenseCoreEnemyStateBase : public UObject
{
    GENERATED_BODY()

public:
    USuspenseCoreEnemyStateBase();

    virtual void OnEnterState(ASuspenseCoreEnemyCharacter* Enemy);
    virtual void OnExitState(ASuspenseCoreEnemyCharacter* Enemy);
    virtual void OnTickState(ASuspenseCoreEnemyCharacter* Enemy, float DeltaTime);
    virtual void OnFSMEvent(ASuspenseCoreEnemyCharacter* Enemy, const FGameplayTag& EventTag, AActor* Instigator);

    FGameplayTag GetStateTag() const;
    void SetFSMComponent(USuspenseCoreEnemyFSMComponent* InFSMComponent);

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State")
    FGameplayTag StateTag;

    UPROPERTY()
    TObjectPtr<USuspenseCoreEnemyFSMComponent> FSMComponent;

    void RequestStateChange(const FGameplayTag& NewStateTag);
    void StartTimer(ASuspenseCoreEnemyCharacter* Enemy, FName TimerName, float Duration, bool bLoop);
    void StopTimer(ASuspenseCoreEnemyCharacter* Enemy, FName TimerName);

    bool CanSeeTarget(ASuspenseCoreEnemyCharacter* Enemy, AActor* Target) const;
    float GetDistanceToTarget(ASuspenseCoreEnemyCharacter* Enemy, AActor* Target) const;
    AActor* GetCurrentTarget(ASuspenseCoreEnemyCharacter* Enemy) const;
    void SetCurrentTarget(ASuspenseCoreEnemyCharacter* Enemy, AActor* NewTarget);
};
