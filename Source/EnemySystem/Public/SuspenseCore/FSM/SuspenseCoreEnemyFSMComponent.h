#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreEnemyFSMComponent.generated.h"

class ASuspenseCoreEnemyCharacter;
class USuspenseCoreEnemyStateBase;
class USuspenseCoreEnemyBehaviorData;

USTRUCT()
struct FSuspenseCoreEnemyFSMTransition
{
    GENERATED_BODY()

    UPROPERTY()
    FGameplayTag FromState;

    UPROPERTY()
    FGameplayTag EventTag;

    UPROPERTY()
    FGameplayTag ToState;
};

USTRUCT()
struct FSuspenseCoreEnemyStateTimer
{
    GENERATED_BODY()

    FTimerHandle TimerHandle;
    FName TimerName;
    bool bLoop;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemyStateChanged, FGameplayTag, OldState, FGameplayTag, NewState);

UCLASS(ClassGroup = "SuspenseCore", meta = (BlueprintSpawnableComponent))
class ENEMYSYSTEM_API USuspenseCoreEnemyFSMComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USuspenseCoreEnemyFSMComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    void Initialize(USuspenseCoreEnemyBehaviorData* BehaviorData);

    UFUNCTION(BlueprintCallable, Category = "EnemyAI|FSM")
    void RequestStateChange(const FGameplayTag& NewStateTag);

    UFUNCTION(BlueprintCallable, Category = "EnemyAI|FSM")
    void SendFSMEvent(const FGameplayTag& EventTag, AActor* Instigator);

    UFUNCTION(BlueprintPure, Category = "EnemyAI|FSM")
    FGameplayTag GetCurrentStateTag() const;

    UFUNCTION(BlueprintPure, Category = "EnemyAI|FSM")
    bool IsInState(const FGameplayTag& StateTag) const;

    void StartStateTimer(FName TimerName, float Duration, bool bLoop);
    void StopStateTimer(FName TimerName);
    void StopAllTimers();

    UPROPERTY(BlueprintAssignable, Category = "EnemyAI|FSM")
    FOnEnemyStateChanged OnStateChanged;

protected:
    UPROPERTY()
    TObjectPtr<ASuspenseCoreEnemyCharacter> OwnerEnemy;

    UPROPERTY()
    TObjectPtr<USuspenseCoreEnemyStateBase> CurrentState;

    UPROPERTY()
    TMap<FGameplayTag, TObjectPtr<USuspenseCoreEnemyStateBase>> StateMap;

    UPROPERTY()
    TArray<FSuspenseCoreEnemyFSMTransition> Transitions;

    UPROPERTY()
    TMap<FName, FSuspenseCoreEnemyStateTimer> ActiveTimers;

    UPROPERTY()
    FGameplayTag InitialStateTag;

    bool bIsInitialized;
    bool bIsProcessingEvent;

    TQueue<TPair<FGameplayTag, TWeakObjectPtr<AActor>>> EventQueue;

    void PerformStateChange(const FGameplayTag& NewStateTag);
    void ProcessEventQueue();
    FGameplayTag FindTransitionTarget(const FGameplayTag& FromState, const FGameplayTag& EventTag) const;
    void HandleTimerFired(FName TimerName);

    USuspenseCoreEnemyStateBase* CreateStateInstance(TSubclassOf<USuspenseCoreEnemyStateBase> StateClass);
};
