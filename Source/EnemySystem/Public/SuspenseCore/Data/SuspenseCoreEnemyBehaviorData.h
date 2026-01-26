#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreEnemyBehaviorData.generated.h"

class USuspenseCoreEnemyStateBase;
class UGameplayAbility;
class UGameplayEffect;

USTRUCT(BlueprintType)
struct FSuspenseCoreEnemyStateConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTag StateTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSubclassOf<USuspenseCoreEnemyStateBase> StateClass;
};

USTRUCT(BlueprintType)
struct FSuspenseCoreEnemyTransitionConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTag FromState;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTag OnEvent;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTag ToState;
};

UCLASS(BlueprintType)
class ENEMYSYSTEM_API USuspenseCoreEnemyBehaviorData : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FName BehaviorID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FGameplayTag EnemyTypeTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FSM")
    FGameplayTag InitialState;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FSM")
    TArray<FSuspenseCoreEnemyStateConfig> States;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FSM")
    TArray<FSuspenseCoreEnemyTransitionConfig> Transitions;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
    float MaxHealth;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
    float AttackPower;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
    float Armor;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
    float WalkSpeed;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
    float RunSpeed;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perception")
    float SightRange;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perception")
    float HearingRange;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS")
    TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS")
    TArray<TSubclassOf<UGameplayEffect>> StartupEffects;

    USuspenseCoreEnemyBehaviorData();
};
