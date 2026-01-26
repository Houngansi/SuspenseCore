#pragma once

#include "CoreMinimal.h"
#include "Core/Enemy/FSM/EnemyBehaviorDataAsset.h"
#include "MedComDefaultEnemyBehavior.generated.h"

class UMedComIdleState;
class UMedComPatrolState;
class UGameplayEffect;
class UGameplayAbility;

/**
 * Готовый Data Asset с предустановленными состояниями для врагов.
 * Поддерживает как классический FSM подход, так и интеграцию с GAS.
 */
UCLASS(BlueprintType)
class MEDCOMCORE_API UMedComDefaultEnemyBehavior : public UEnemyBehaviorDataAsset
{
    GENERATED_BODY()

public:
    UMedComDefaultEnemyBehavior();

    //----------------------------------------------
    // Настройки FSM состояний
    //----------------------------------------------
    
    /** Режим интеграции с GAS (если включен, будет использовать GameplayAbilities) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Integration")
    bool bUseGASForMovement = true;
    
    /** Классы GameplayAbility для интеграции с FSM */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Integration", meta = (EditCondition = "bUseGASForMovement"))
    TSubclassOf<UGameplayAbility> PatrolAbilityClass;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Integration", meta = (EditCondition = "bUseGASForMovement"))
    TSubclassOf<UGameplayAbility> MoveAbilityClass;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Integration", meta = (EditCondition = "bUseGASForMovement"))
    TSubclassOf<UGameplayAbility> ReturnAbilityClass;

    /** Инициализация Data Asset с правильными настройками */
    void InitializeDefaultStates();

#if WITH_EDITOR
    /** Обновляет состояния при изменении свойств в редакторе */
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
    /** Подготовка состояния Idle с базовыми параметрами */
    void SetupIdleState(FStateDescription& StateDesc);

    /** Подготовка состояния Patrol с базовыми параметрами */
    void SetupPatrolState(FStateDescription& StateDesc);
    
    /** Подготовка состояния Chase */
    void SetupChaseState(FStateDescription& StateDesc);
    
    /** Подготовка состояния Attack */
    void SetupAttackState(FStateDescription& StateDesc);
    
    /** Подготовка состояния Return */
    void SetupReturnState(FStateDescription& StateDesc);
    
    /** Подготовка состояния Death */
    void SetupDeathState(FStateDescription& StateDesc);

    /** Настройка базовых переходов между состояниями */
    void SetupBasicTransitions();
    
    /** Настройка специальных переходов для GAS интеграции */
    void SetupGASIntegration();
};