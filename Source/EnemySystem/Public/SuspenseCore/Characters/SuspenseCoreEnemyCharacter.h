#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreEnemyCharacter.generated.h"

class ASuspenseCoreEnemyState;
class USuspenseCoreEnemyFSMComponent;
class USuspenseCoreEnemyBehaviorData;
class UAbilitySystemComponent;

UCLASS()
class ENEMYSYSTEM_API ASuspenseCoreEnemyCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ASuspenseCoreEnemyCharacter();

    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void UnPossessed() override;

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    UFUNCTION(BlueprintCallable, Category = "Enemy")
    void InitializeEnemy(USuspenseCoreEnemyBehaviorData* BehaviorData);

    UFUNCTION(BlueprintPure, Category = "Enemy")
    bool IsAlive() const;

    UFUNCTION(BlueprintPure, Category = "Enemy")
    FGameplayTag GetCurrentStateTag() const;

    UFUNCTION(BlueprintCallable, Category = "Enemy")
    void StopMovement();

    UFUNCTION(BlueprintCallable, Category = "Enemy")
    void ExecuteAttack();

    AActor* GetCurrentTarget() const;
    void SetCurrentTarget(AActor* NewTarget);

    UFUNCTION(BlueprintCallable, Category = "Enemy")
    void OnPerceptionUpdated(AActor* Actor, bool bIsSensed);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USuspenseCoreEnemyFSMComponent> FSMComponent;

    UPROPERTY()
    TObjectPtr<ASuspenseCoreEnemyState> EnemyState;

    UPROPERTY()
    TWeakObjectPtr<AActor> CurrentTarget;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy")
    TObjectPtr<USuspenseCoreEnemyBehaviorData> DefaultBehaviorData;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy")
    FGameplayTag EnemyTypeTag;

    void SetupAbilitySystem();
};
