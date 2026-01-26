#include "SuspenseCore/FSM/SuspenseCoreEnemyFSMComponent.h"
#include "SuspenseCore/FSM/SuspenseCoreEnemyStateBase.h"
#include "SuspenseCore/Characters/SuspenseCoreEnemy.h"
#include "SuspenseCore/Data/SuspenseCoreEnemyBehaviorData.h"
#include "SuspenseCore/Tags/SuspenseCoreEnemyTags.h"
#include "TimerManager.h"
#include "EnemySystem.h"

USuspenseCoreEnemyFSMComponent::USuspenseCoreEnemyFSMComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;

    bIsInitialized = false;
    bIsProcessingEvent = false;
}

void USuspenseCoreEnemyFSMComponent::BeginPlay()
{
    Super::BeginPlay();

    OwnerEnemy = Cast<ASuspenseCoreEnemy>(GetOwner());
    if (!OwnerEnemy)
    {
        UE_LOG(LogEnemySystem, Error, TEXT("FSMComponent owner is not ASuspenseCoreEnemy"));
    }
}

void USuspenseCoreEnemyFSMComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopAllTimers();
    Super::EndPlay(EndPlayReason);
}

void USuspenseCoreEnemyFSMComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    ProcessEventQueue();

    if (CurrentState && OwnerEnemy)
    {
        CurrentState->OnTickState(OwnerEnemy, DeltaTime);
    }
}

void USuspenseCoreEnemyFSMComponent::Initialize(USuspenseCoreEnemyBehaviorData* BehaviorData)
{
    if (!BehaviorData)
    {
        UE_LOG(LogEnemySystem, Error, TEXT("Cannot initialize FSM: BehaviorData is null"));
        return;
    }

    StateMap.Empty();
    Transitions.Empty();

    for (const FSuspenseCoreEnemyStateConfig& StateConfig : BehaviorData->States)
    {
        if (StateConfig.StateClass)
        {
            USuspenseCoreEnemyStateBase* StateInstance = CreateStateInstance(StateConfig.StateClass);
            if (StateInstance)
            {
                StateMap.Add(StateConfig.StateTag, StateInstance);
            }
        }
    }

    for (const FSuspenseCoreEnemyTransitionConfig& TransitionConfig : BehaviorData->Transitions)
    {
        FSuspenseCoreEnemyFSMTransition Transition;
        Transition.FromState = TransitionConfig.FromState;
        Transition.EventTag = TransitionConfig.OnEvent;
        Transition.ToState = TransitionConfig.ToState;
        Transitions.Add(Transition);
    }

    InitialStateTag = BehaviorData->InitialState;
    bIsInitialized = true;

    SetComponentTickEnabled(true);

    if (InitialStateTag.IsValid())
    {
        PerformStateChange(InitialStateTag);
    }

    UE_LOG(LogEnemySystem, Log, TEXT("FSM initialized with %d states and %d transitions"),
        StateMap.Num(), Transitions.Num());
}

void USuspenseCoreEnemyFSMComponent::RequestStateChange(const FGameplayTag& NewStateTag)
{
    if (!bIsInitialized)
    {
        return;
    }

    if (!NewStateTag.IsValid())
    {
        return;
    }

    if (CurrentState && CurrentState->GetStateTag() == NewStateTag)
    {
        return;
    }

    PerformStateChange(NewStateTag);
}

void USuspenseCoreEnemyFSMComponent::SendFSMEvent(const FGameplayTag& EventTag, AActor* Instigator)
{
    if (!bIsInitialized)
    {
        return;
    }

    EventQueue.Enqueue(TPair<FGameplayTag, TWeakObjectPtr<AActor>>(EventTag, Instigator));
}

FGameplayTag USuspenseCoreEnemyFSMComponent::GetCurrentStateTag() const
{
    if (CurrentState)
    {
        return CurrentState->GetStateTag();
    }
    return FGameplayTag();
}

bool USuspenseCoreEnemyFSMComponent::IsInState(const FGameplayTag& StateTag) const
{
    return CurrentState && CurrentState->GetStateTag() == StateTag;
}

void USuspenseCoreEnemyFSMComponent::PerformStateChange(const FGameplayTag& NewStateTag)
{
    TObjectPtr<USuspenseCoreEnemyStateBase>* NewStatePtr = StateMap.Find(NewStateTag);
    if (!NewStatePtr || !(*NewStatePtr))
    {
        UE_LOG(LogEnemySystem, Warning, TEXT("State not found in StateMap: %s"), *NewStateTag.ToString());
        return;
    }

    FGameplayTag OldStateTag;

    if (CurrentState)
    {
        OldStateTag = CurrentState->GetStateTag();
        CurrentState->OnExitState(OwnerEnemy);
    }

    StopAllTimers();

    CurrentState = *NewStatePtr;
    CurrentState->OnEnterState(OwnerEnemy);

    OnStateChanged.Broadcast(OldStateTag, NewStateTag);

    UE_LOG(LogEnemySystem, Log, TEXT("[%s] State changed: %s -> %s"),
        OwnerEnemy ? *OwnerEnemy->GetName() : TEXT("None"),
        OldStateTag.IsValid() ? *OldStateTag.ToString() : TEXT("None"),
        *NewStateTag.ToString());
}

void USuspenseCoreEnemyFSMComponent::ProcessEventQueue()
{
    if (bIsProcessingEvent)
    {
        return;
    }

    bIsProcessingEvent = true;

    TPair<FGameplayTag, TWeakObjectPtr<AActor>> EventPair;
    while (EventQueue.Dequeue(EventPair))
    {
        FGameplayTag EventTag = EventPair.Key;
        AActor* Instigator = EventPair.Value.Get();

        if (CurrentState)
        {
            CurrentState->OnFSMEvent(OwnerEnemy, EventTag, Instigator);
        }

        FGameplayTag CurrentStateTag = GetCurrentStateTag();
        FGameplayTag TargetState = FindTransitionTarget(CurrentStateTag, EventTag);

        if (TargetState.IsValid())
        {
            PerformStateChange(TargetState);
        }
    }

    bIsProcessingEvent = false;
}

FGameplayTag USuspenseCoreEnemyFSMComponent::FindTransitionTarget(const FGameplayTag& FromState, const FGameplayTag& EventTag) const
{
    for (const FSuspenseCoreEnemyFSMTransition& Transition : Transitions)
    {
        if (Transition.FromState == FromState && Transition.EventTag == EventTag)
        {
            return Transition.ToState;
        }
    }
    return FGameplayTag();
}

void USuspenseCoreEnemyFSMComponent::StartStateTimer(FName TimerName, float Duration, bool bLoop)
{
    if (!GetWorld())
    {
        return;
    }

    StopStateTimer(TimerName);

    FSuspenseCoreEnemyStateTimer NewTimer;
    NewTimer.TimerName = TimerName;
    NewTimer.bLoop = bLoop;

    FTimerDelegate TimerDelegate;
    TimerDelegate.BindUObject(this, &USuspenseCoreEnemyFSMComponent::HandleTimerFired, TimerName);

    GetWorld()->GetTimerManager().SetTimer(
        NewTimer.TimerHandle,
        TimerDelegate,
        Duration,
        bLoop
    );

    ActiveTimers.Add(TimerName, NewTimer);
}

void USuspenseCoreEnemyFSMComponent::StopStateTimer(FName TimerName)
{
    FSuspenseCoreEnemyStateTimer* Timer = ActiveTimers.Find(TimerName);
    if (Timer && GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(Timer->TimerHandle);
        ActiveTimers.Remove(TimerName);
    }
}

void USuspenseCoreEnemyFSMComponent::StopAllTimers()
{
    if (!GetWorld())
    {
        return;
    }

    for (auto& TimerPair : ActiveTimers)
    {
        GetWorld()->GetTimerManager().ClearTimer(TimerPair.Value.TimerHandle);
    }
    ActiveTimers.Empty();
}

void USuspenseCoreEnemyFSMComponent::HandleTimerFired(FName TimerName)
{
    UE_LOG(LogEnemySystem, Verbose, TEXT("Timer fired: %s"), *TimerName.ToString());

    if (TimerName == FName(TEXT("IdleTimeout")))
    {
        SendFSMEvent(SuspenseCoreEnemyTags::Event::IdleTimeout, nullptr);
    }
    else if (TimerName == FName(TEXT("PatrolWait")))
    {
        SendFSMEvent(SuspenseCoreEnemyTags::Event::PatrolComplete, nullptr);
    }
}

USuspenseCoreEnemyStateBase* USuspenseCoreEnemyFSMComponent::CreateStateInstance(TSubclassOf<USuspenseCoreEnemyStateBase> StateClass)
{
    if (!StateClass)
    {
        return nullptr;
    }

    USuspenseCoreEnemyStateBase* StateInstance = NewObject<USuspenseCoreEnemyStateBase>(this, StateClass);
    if (StateInstance)
    {
        StateInstance->SetFSMComponent(this);
    }
    return StateInstance;
}
