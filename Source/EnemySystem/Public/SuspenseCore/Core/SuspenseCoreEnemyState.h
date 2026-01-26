#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreEnemyState.generated.h"

class UAbilitySystemComponent;
class USuspenseCoreEnemyAttributeSet;
class UGameplayAbility;
class UGameplayEffect;

UCLASS()
class ENEMYSYSTEM_API ASuspenseCoreEnemyState : public APlayerState, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ASuspenseCoreEnemyState();

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    USuspenseCoreEnemyAttributeSet* GetAttributeSet() const;

    void InitializeAbilities(const TArray<TSubclassOf<UGameplayAbility>>& AbilitiesToGrant);

    void ApplyStartupEffects(const TArray<TSubclassOf<UGameplayEffect>>& EffectsToApply);

    void SetEnemyLevel(int32 NewLevel);

    int32 GetEnemyLevel() const;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    TObjectPtr<USuspenseCoreEnemyAttributeSet> AttributeSet;

    UPROPERTY(Replicated)
    int32 EnemyLevel;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
