// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Components/Core/SuspenseCoreWeaponStateManager.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentDataProvider.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEventDispatcher.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "Engine/World.h"
#include "TimerManager.h"

USuspenseCoreWeaponStateManager::USuspenseCoreWeaponStateManager()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.016f; // 60 FPS for smooth transitions
    SetIsReplicatedByDefault(false);

    // Initialize default states
    DefaultIdleState = FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Ready"));
    DefaultHolsteredState = FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Holstered"));

    // Setup default transitions
    FSuspenseCoreStateTransitionDef DrawTransition;
    DrawTransition.FromState = FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Holstered"));
    DrawTransition.ToState = FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Drawing"));
    DrawTransition.Duration = 0.5f;
    DrawTransition.bInterruptible = false;
    TransitionDefinitions.Add(DrawTransition);

    FSuspenseCoreStateTransitionDef DrawCompleteTransition;
    DrawCompleteTransition.FromState = FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Drawing"));
    DrawCompleteTransition.ToState = FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Ready"));
    DrawCompleteTransition.Duration = 0.1f;
    DrawCompleteTransition.bInterruptible = false;
    TransitionDefinitions.Add(DrawCompleteTransition);

    FSuspenseCoreStateTransitionDef HolsterTransition;
    HolsterTransition.FromState = FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Ready"));
    HolsterTransition.ToState = FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Holstering"));
    HolsterTransition.Duration = 0.4f;
    HolsterTransition.bInterruptible = true;
    TransitionDefinitions.Add(HolsterTransition);

    FSuspenseCoreStateTransitionDef HolsterCompleteTransition;
    HolsterCompleteTransition.FromState = FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Holstering"));
    HolsterCompleteTransition.ToState = FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Holstered"));
    HolsterCompleteTransition.Duration = 0.1f;
    HolsterCompleteTransition.bInterruptible = false;
    TransitionDefinitions.Add(HolsterCompleteTransition);

    FSuspenseCoreStateTransitionDef FireTransition;
    FireTransition.FromState = FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Ready"));
    FireTransition.ToState = FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Firing"));
    FireTransition.Duration = 0.0f; // Instant
    FireTransition.bInterruptible = true;
    TransitionDefinitions.Add(FireTransition);

    FSuspenseCoreStateTransitionDef FireEndTransition;
    FireEndTransition.FromState = FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Firing"));
    FireEndTransition.ToState = FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Ready"));
    FireEndTransition.Duration = 0.1f;
    FireEndTransition.bInterruptible = true;
    TransitionDefinitions.Add(FireEndTransition);

    FSuspenseCoreStateTransitionDef ReloadTransition;
    ReloadTransition.FromState = FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Ready"));
    ReloadTransition.ToState = FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Reloading"));
    ReloadTransition.Duration = 2.0f;
    ReloadTransition.bInterruptible = false;
    TransitionDefinitions.Add(ReloadTransition);

    FSuspenseCoreStateTransitionDef ReloadCompleteTransition;
    ReloadCompleteTransition.FromState = FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Reloading"));
    ReloadCompleteTransition.ToState = FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Ready"));
    ReloadCompleteTransition.Duration = 0.2f;
    ReloadCompleteTransition.bInterruptible = false;
    TransitionDefinitions.Add(ReloadCompleteTransition);
}

void USuspenseCoreWeaponStateManager::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Log, TEXT("SuspenseCoreWeaponStateManager: BeginPlay - Weapon state manager started"));
}

void USuspenseCoreWeaponStateManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Clear all state machines
    FScopeLock Lock(&StateMachineLock);
    StateMachines.Empty();
    StateHistory.Empty();

    UE_LOG(LogTemp, Log, TEXT("SuspenseCoreWeaponStateManager: EndPlay - Weapon state manager stopped"));

    Super::EndPlay(EndPlayReason);
}

void USuspenseCoreWeaponStateManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Update all active transitions
    UpdateTransitions(DeltaTime);
}

bool USuspenseCoreWeaponStateManager::Initialize(
    TScriptInterface<ISuspenseCoreEquipmentDataProvider> InDataProvider,
    TScriptInterface<ISuspenseCoreEventDispatcher> InEventDispatcher)
{
    DataProvider   = InDataProvider;
    EventDispatcher = InEventDispatcher;

    if (!DataProvider.GetInterface())
    {
        UE_LOG(LogTemp, Error, TEXT("SuspenseCoreWeaponStateManager: Initialize failed - DataProvider is null"));
        return false;
    }

    // Инициализация стейт-машин только для реальных оружейных слотов из вашего ESuspenseCoreEquipmentSlotType
    // (Sidearm/Melee в вашем enum нет — используем Holster и Scabbard)
    const int32 SlotCount = DataProvider->GetSlotCount();
    for (int32 i = 0; i < SlotCount; ++i)
    {
        const FEquipmentSlotConfig SlotConfig = DataProvider->GetSlotConfiguration(i);

        const bool bIsWeaponSlot =
            (SlotConfig.SlotType == ESuspenseCoreEquipmentSlotType::PrimaryWeapon)  ||
            (SlotConfig.SlotType == ESuspenseCoreEquipmentSlotType::SecondaryWeapon)||
            (SlotConfig.SlotType == ESuspenseCoreEquipmentSlotType::Holster)        ||
            (SlotConfig.SlotType == ESuspenseCoreEquipmentSlotType::Scabbard);

        if (bIsWeaponSlot)
        {
            FSuspenseCoreWeaponStateMachine& StateMachine = GetOrCreateStateMachine(i);
            StateMachine.CurrentState  = DefaultHolsteredState;
            StateMachine.PreviousState = DefaultHolsteredState;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("SuspenseCoreWeaponStateManager: Initialized with %d weapon slots"), StateMachines.Num());
    return true;
}

FGameplayTag USuspenseCoreWeaponStateManager::GetWeaponState(int32 SlotIndex) const
{
    FScopeLock Lock(&StateMachineLock);

    if (SlotIndex == -1)
    {
        SlotIndex = ActiveWeaponSlot;
    }

    if (SlotIndex == INDEX_NONE)
    {
        return FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.None"));
    }

    const FSuspenseCoreWeaponStateMachine* StateMachine = StateMachines.FindByPredicate(
        [SlotIndex](const FSuspenseCoreWeaponStateMachine& SM) { return SM.SlotIndex == SlotIndex; });

    if (StateMachine)
    {
        return StateMachine->CurrentState;
    }

    return FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.None"));
}

FSuspenseCoreWeaponStateTransitionResult USuspenseCoreWeaponStateManager::RequestStateTransition(const FSuspenseCoreWeaponStateTransitionRequest& Request)
{
    FScopeLock Lock(&StateMachineLock);

    FSuspenseCoreWeaponStateTransitionResult Result;
    Result.bSuccess = false;

    int32 SlotIndex = Request.WeaponSlotIndex;
    if (SlotIndex == -1)
    {
        SlotIndex = ActiveWeaponSlot;
    }

    if (SlotIndex == INDEX_NONE)
    {
        Result.FailureReason = FText::FromString(TEXT("No active weapon slot"));
        return Result;
    }

    FSuspenseCoreWeaponStateMachine& StateMachine = GetOrCreateStateMachine(SlotIndex);

    // Check if already transitioning
    if (StateMachine.bIsTransitioning && !Request.bForceTransition)
    {
        const FSuspenseCoreStateTransitionDef* CurrentTransition = FindTransitionDef(
            StateMachine.ActiveTransition.FromState,
            StateMachine.ActiveTransition.ToState);

        if (CurrentTransition && !CurrentTransition->bInterruptible)
        {
            Result.FailureReason = FText::FromString(TEXT("Current transition cannot be interrupted"));
            return Result;
        }
    }

    // Validate transition
    if (!CanTransitionTo(Request.FromState.IsValid() ? Request.FromState : StateMachine.CurrentState, Request.ToState))
    {
        Result.FailureReason = FText::FromString(TEXT("Invalid state transition"));
        return Result;
    }

    // Find transition definition
    const FSuspenseCoreStateTransitionDef* TransitionDef = FindTransitionDef(
        Request.FromState.IsValid() ? Request.FromState : StateMachine.CurrentState,
        Request.ToState);

    float Duration = Request.TransitionDuration > 0 ? Request.TransitionDuration :
                     (TransitionDef ? TransitionDef->Duration : GetTransitionDuration(StateMachine.CurrentState, Request.ToState));

    // Start transition
    StateMachine.PreviousState = StateMachine.CurrentState;
    StateMachine.bIsTransitioning = true;
    StateMachine.TransitionStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    StateMachine.TransitionDuration = Duration;
    StateMachine.ActiveTransition = Request;

    // If instant transition, complete immediately
    if (Duration <= 0.0f)
    {
        CompleteTransition(SlotIndex);
    }

    Result.bSuccess = true;
    Result.ResultingState = Request.ToState;
    Result.ActualDuration = Duration;

    // Record in history
    RecordStateChange(SlotIndex, Request.ToState);

    // Broadcast event
    BroadcastStateChange(SlotIndex, StateMachine.PreviousState, Request.ToState);

    return Result;
}

bool USuspenseCoreWeaponStateManager::CanTransitionTo(const FGameplayTag& FromState, const FGameplayTag& ToState) const
{
    // Check if transition is defined
    const FSuspenseCoreStateTransitionDef* TransitionDef = FindTransitionDef(FromState, ToState);
    return TransitionDef != nullptr;
}

TArray<FGameplayTag> USuspenseCoreWeaponStateManager::GetValidTransitions(const FGameplayTag& CurrentState) const
{
    TArray<FGameplayTag> ValidTransitions;

    for (const FSuspenseCoreStateTransitionDef& Transition : TransitionDefinitions)
    {
        if (Transition.FromState == CurrentState)
        {
            ValidTransitions.Add(Transition.ToState);
        }
    }

    return ValidTransitions;
}

bool USuspenseCoreWeaponStateManager::ForceState(const FGameplayTag& NewState, int32 SlotIndex)
{
    FScopeLock Lock(&StateMachineLock);

    if (SlotIndex == INDEX_NONE)
    {
        SlotIndex = ActiveWeaponSlot;
    }

    if (SlotIndex == INDEX_NONE)
    {
        return false;
    }

    FSuspenseCoreWeaponStateMachine& StateMachine = GetOrCreateStateMachine(SlotIndex);

    // Force state without validation
    StateMachine.PreviousState = StateMachine.CurrentState;
    StateMachine.CurrentState = NewState;
    StateMachine.bIsTransitioning = false;
    StateMachine.TransitionStartTime = 0.0f;
    StateMachine.TransitionDuration = 0.0f;

    // Track statistics
    TotalForceStates++;

    // Record and broadcast
    RecordStateChange(SlotIndex, NewState);
    BroadcastStateChange(SlotIndex, StateMachine.PreviousState, NewState);

    UE_LOG(LogTemp, Log, TEXT("SuspenseCoreWeaponStateManager: Forced slot %d to state %s"),
        SlotIndex, *NewState.ToString());

    return true;
}

float USuspenseCoreWeaponStateManager::GetTransitionDuration(const FGameplayTag& FromState, const FGameplayTag& ToState) const
{
    const FSuspenseCoreStateTransitionDef* TransitionDef = FindTransitionDef(FromState, ToState);
    if (TransitionDef)
    {
        return TransitionDef->Duration;
    }

    // Default durations for common transitions
    if (FromState == FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Holstered")))
    {
        if (ToState == FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Drawing")))
        {
            return 0.5f;
        }
    }
    else if (FromState == FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Ready")))
    {
        if (ToState == FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Holstering")))
        {
            return 0.4f;
        }
        else if (ToState == FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Reloading")))
        {
            return 2.0f;
        }
        else if (ToState == FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Firing")))
        {
            return 0.0f; // Instant
        }
    }

    return 0.2f; // Default transition time
}

bool USuspenseCoreWeaponStateManager::IsTransitioning(int32 SlotIndex) const
{
    FScopeLock Lock(&StateMachineLock);

    if (SlotIndex == -1)
    {
        // Check if any weapon is transitioning
        for (const FSuspenseCoreWeaponStateMachine& StateMachine : StateMachines)
        {
            if (StateMachine.bIsTransitioning)
            {
                return true;
            }
        }
        return false;
    }

    const FSuspenseCoreWeaponStateMachine* StateMachine = StateMachines.FindByPredicate(
        [SlotIndex](const FSuspenseCoreWeaponStateMachine& SM) { return SM.SlotIndex == SlotIndex; });

    return StateMachine ? StateMachine->bIsTransitioning : false;
}

float USuspenseCoreWeaponStateManager::GetTransitionProgress(int32 SlotIndex) const
{
    FScopeLock Lock(&StateMachineLock);

    if (SlotIndex == -1)
    {
        SlotIndex = ActiveWeaponSlot;
    }

    const FSuspenseCoreWeaponStateMachine* StateMachine = StateMachines.FindByPredicate(
        [SlotIndex](const FSuspenseCoreWeaponStateMachine& SM) { return SM.SlotIndex == SlotIndex; });

    if (!StateMachine || !StateMachine->bIsTransitioning || !GetWorld())
    {
        return 0.0f;
    }

    float ElapsedTime = GetWorld()->GetTimeSeconds() - StateMachine->TransitionStartTime;
    return FMath::Clamp(ElapsedTime / StateMachine->TransitionDuration, 0.0f, 1.0f);
}

bool USuspenseCoreWeaponStateManager::AbortTransition(int32 SlotIndex)
{
    FScopeLock Lock(&StateMachineLock);

    if (SlotIndex == -1)
    {
        SlotIndex = ActiveWeaponSlot;
    }

    FSuspenseCoreWeaponStateMachine* StateMachine = StateMachines.FindByPredicate(
        [SlotIndex](FSuspenseCoreWeaponStateMachine& SM) { return SM.SlotIndex == SlotIndex; });

    if (!StateMachine || !StateMachine->bIsTransitioning)
    {
        return false;
    }

    // Check if transition is interruptible
    const FSuspenseCoreStateTransitionDef* TransitionDef = FindTransitionDef(
        StateMachine->ActiveTransition.FromState,
        StateMachine->ActiveTransition.ToState);

    if (TransitionDef && !TransitionDef->bInterruptible)
    {
        return false;
    }

    // Abort transition - return to previous state
    StateMachine->CurrentState = StateMachine->PreviousState;
    StateMachine->bIsTransitioning = false;
    StateMachine->TransitionStartTime = 0.0f;
    StateMachine->TransitionDuration = 0.0f;

    // Track statistics
    TotalTransitionsAborted++;

    UE_LOG(LogTemp, Log, TEXT("SuspenseCoreWeaponStateManager: Aborted transition for slot %d"), SlotIndex);

    return true;
}

TArray<FSuspenseCoreWSPHistoryEntry> USuspenseCoreWeaponStateManager::GetStateHistory(int32 MaxCount) const
{
    FScopeLock Lock(&StateMachineLock);

    TArray<FSuspenseCoreWSPHistoryEntry> History;
    if (MaxCount <= 0 || StateHistory.Num() == 0)
    {
        return History;
    }

    const int32 StartIndex = FMath::Max(0, StateHistory.Num() - MaxCount);
    for (int32 i = StartIndex; i < StateHistory.Num(); ++i)
    {
        FSuspenseCoreWSPHistoryEntry Entry;
        Entry.State = StateHistory[i].State;
        Entry.Timestamp = StateHistory[i].TimeSeconds;
        Entry.SlotIndex = INDEX_NONE; // Global history doesn't track per-slot
        History.Add(Entry);
    }

    return History;
}

TArray<FSuspenseCoreWSPHistoryEntry> USuspenseCoreWeaponStateManager::GetSlotStateHistory(int32 SlotIndex, int32 MaxCount) const
{
    FScopeLock Lock(&StateMachineLock);

    TArray<FSuspenseCoreWSPHistoryEntry> History;
    if (MaxCount <= 0 || StateHistory.Num() == 0)
    {
        return History;
    }

    // Filter history by slot if tracking per-slot is implemented
    // For now, return global history (same as GetStateHistory)
    return GetStateHistory(MaxCount);
}

void USuspenseCoreWeaponStateManager::ClearStateHistory()
{
    FScopeLock Lock(&StateMachineLock);
    StateHistory.Empty();
    UE_LOG(LogTemp, Log, TEXT("SuspenseCoreWeaponStateManager: State history cleared"));
}

void USuspenseCoreWeaponStateManager::RegisterTransition(const FSuspenseCoreStateTransitionDef& Transition)
{
    FScopeLock Lock(&StateMachineLock);

    // Remove existing transition if present
    TransitionDefinitions.RemoveAll([&Transition](const FSuspenseCoreStateTransitionDef& Def)
    {
        return Def.FromState == Transition.FromState && Def.ToState == Transition.ToState;
    });

    // Add new transition
    TransitionDefinitions.Add(Transition);

    UE_LOG(LogTemp, Log, TEXT("SuspenseCoreWeaponStateManager: Registered transition %s -> %s"),
        *Transition.FromState.ToString(), *Transition.ToState.ToString());
}

void USuspenseCoreWeaponStateManager::UpdateTransitions(float DeltaTime)
{
    FScopeLock Lock(&StateMachineLock);

    if (!GetWorld())
    {
        return;
    }

    float CurrentTime = GetWorld()->GetTimeSeconds();

    for (FSuspenseCoreWeaponStateMachine& StateMachine : StateMachines)
    {
        if (!StateMachine.bIsTransitioning)
        {
            continue;
        }

        float ElapsedTime = CurrentTime - StateMachine.TransitionStartTime;

        if (ElapsedTime >= StateMachine.TransitionDuration)
        {
            CompleteTransition(StateMachine.SlotIndex);
        }
    }
}

void USuspenseCoreWeaponStateManager::CompleteTransition(int32 SlotIndex)
{
    FSuspenseCoreWeaponStateMachine* StateMachine = StateMachines.FindByPredicate(
        [SlotIndex](FSuspenseCoreWeaponStateMachine& SM) { return SM.SlotIndex == SlotIndex; });

    if (!StateMachine || !StateMachine->bIsTransitioning)
    {
        return;
    }

    // Complete the transition
    StateMachine->CurrentState = StateMachine->ActiveTransition.ToState;
    StateMachine->bIsTransitioning = false;
    StateMachine->TransitionStartTime = 0.0f;
    StateMachine->TransitionDuration = 0.0f;

    // Track statistics
    TotalTransitionsCompleted++;

    UE_LOG(LogTemp, Verbose, TEXT("SuspenseCoreWeaponStateManager: Completed transition for slot %d to state %s"),
        SlotIndex, *StateMachine->CurrentState.ToString());

    // Check for chained transitions
    TArray<FGameplayTag> ValidNextStates = GetValidTransitions(StateMachine->CurrentState);

    // Auto-transition for intermediate states
    if (StateMachine->CurrentState == FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Drawing")))
    {
        FSuspenseCoreWeaponStateTransitionRequest AutoTransition;
        AutoTransition.ToState = FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Ready"));
        AutoTransition.WeaponSlotIndex = SlotIndex;
        RequestStateTransition(AutoTransition);
    }
    else if (StateMachine->CurrentState == FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Holstering")))
    {
        FSuspenseCoreWeaponStateTransitionRequest AutoTransition;
        AutoTransition.ToState = FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Holstered"));
        AutoTransition.WeaponSlotIndex = SlotIndex;
        RequestStateTransition(AutoTransition);
    }
}

FSuspenseCoreWeaponStateMachine& USuspenseCoreWeaponStateManager::GetOrCreateStateMachine(int32 SlotIndex)
{
    FSuspenseCoreWeaponStateMachine* ExistingMachine = StateMachines.FindByPredicate(
        [SlotIndex](const FSuspenseCoreWeaponStateMachine& SM) { return SM.SlotIndex == SlotIndex; });

    if (ExistingMachine)
    {
        return *ExistingMachine;
    }

    // Create new state machine
    FSuspenseCoreWeaponStateMachine NewMachine;
    NewMachine.SlotIndex = SlotIndex;
    NewMachine.CurrentState = DefaultHolsteredState;
    NewMachine.PreviousState = DefaultHolsteredState;

    int32 Index = StateMachines.Add(NewMachine);
    return StateMachines[Index];
}

const FSuspenseCoreStateTransitionDef* USuspenseCoreWeaponStateManager::FindTransitionDef(const FGameplayTag& FromState, const FGameplayTag& ToState) const
{
    return TransitionDefinitions.FindByPredicate([&FromState, &ToState](const FSuspenseCoreStateTransitionDef& Def)
    {
        return Def.FromState == FromState && Def.ToState == ToState;
    });
}

void USuspenseCoreWeaponStateManager::RecordStateChange(int32 SlotIndex, const FGameplayTag& NewState)
{
    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    FSuspenseCoreWeaponStateHistoryEntry Entry;
    Entry.State       = NewState;
    Entry.TimeSeconds = CurrentTime;

    StateHistory.Add(Entry);

    // Ограничиваем буфер истории
    if (StateHistory.Num() > MaxHistorySize)
    {
        const int32 Excess = StateHistory.Num() - MaxHistorySize;
        StateHistory.RemoveAt(0, Excess);
    }
}

void USuspenseCoreWeaponStateManager::BroadcastStateChange(int32 SlotIndex, const FGameplayTag& OldState, const FGameplayTag& NewState)
{
    if (!EventDispatcher.GetInterface())
    {
        return;
    }

    // SuspenseCore architecture: FSuspenseCoreEventData with Publish()
    FSuspenseCoreEventData Ev = FSuspenseCoreEventData::Create(const_cast<USuspenseCoreWeaponStateManager*>(this));
    Ev.SetObject(FName("Target"), GetOwner());
    Ev.SetInt(FName("Slot"), SlotIndex);
    Ev.SetString(FName("From"), OldState.ToString());
    Ev.SetString(FName("To"), NewState.ToString());

    EventDispatcher->Publish(FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Weapon.StateChanged")), Ev);
}

//==================================================================
// Additional ISuspenseCoreWeaponStateProvider Interface Methods
//==================================================================

FSuspenseCoreWeaponStateSnapshot USuspenseCoreWeaponStateManager::GetStateSnapshot(int32 SlotIndex) const
{
    FScopeLock Lock(&StateMachineLock);

    FSuspenseCoreWeaponStateSnapshot Snapshot;

    if (SlotIndex == INDEX_NONE)
    {
        SlotIndex = ActiveWeaponSlot;
    }

    if (SlotIndex == INDEX_NONE)
    {
        return Snapshot;
    }

    const FSuspenseCoreWeaponStateMachine* StateMachine = StateMachines.FindByPredicate(
        [SlotIndex](const FSuspenseCoreWeaponStateMachine& SM) { return SM.SlotIndex == SlotIndex; });

    if (StateMachine)
    {
        Snapshot.SlotIndex = StateMachine->SlotIndex;
        Snapshot.CurrentState = StateMachine->CurrentState;
        Snapshot.PreviousState = StateMachine->PreviousState;
        Snapshot.bIsTransitioning = StateMachine->bIsTransitioning;
        Snapshot.TransitionProgress = GetTransitionProgress(SlotIndex);
        Snapshot.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    }

    return Snapshot;
}

TArray<FSuspenseCoreWeaponStateSnapshot> USuspenseCoreWeaponStateManager::GetAllStateSnapshots() const
{
    FScopeLock Lock(&StateMachineLock);

    TArray<FSuspenseCoreWeaponStateSnapshot> Snapshots;
    Snapshots.Reserve(StateMachines.Num());

    for (const FSuspenseCoreWeaponStateMachine& StateMachine : StateMachines)
    {
        FSuspenseCoreWeaponStateSnapshot Snapshot;
        Snapshot.SlotIndex = StateMachine.SlotIndex;
        Snapshot.CurrentState = StateMachine.CurrentState;
        Snapshot.PreviousState = StateMachine.PreviousState;
        Snapshot.bIsTransitioning = StateMachine.bIsTransitioning;
        Snapshot.TransitionProgress = GetTransitionProgress(StateMachine.SlotIndex);
        Snapshot.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        Snapshots.Add(Snapshot);
    }

    return Snapshots;
}

FSuspenseCoreWeaponStateTransitionResult USuspenseCoreWeaponStateManager::TransitionTo(const FGameplayTag& NewState, int32 SlotIndex)
{
    FSuspenseCoreWeaponStateTransitionRequest Request;
    Request.ToState = NewState;
    Request.WeaponSlotIndex = SlotIndex;

    TotalTransitionsRequested++;
    return RequestStateTransition(Request);
}

bool USuspenseCoreWeaponStateManager::ResetToDefaultState(int32 SlotIndex)
{
    return ForceState(DefaultIdleState, SlotIndex);
}

bool USuspenseCoreWeaponStateManager::RegisterTransitionRule(const FSuspenseCoreStateTransitionRule& Rule)
{
    FScopeLock Lock(&StateMachineLock);

    // Convert Rule to TransitionDef
    FSuspenseCoreStateTransitionDef Def;
    Def.FromState = Rule.FromState;
    Def.ToState = Rule.ToState;
    Def.Duration = Rule.Duration;
    Def.bInterruptible = Rule.bInterruptible;

    // Remove existing transition if present
    TransitionDefinitions.RemoveAll([&Rule](const FSuspenseCoreStateTransitionDef& Existing)
    {
        return Existing.FromState == Rule.FromState && Existing.ToState == Rule.ToState;
    });

    TransitionDefinitions.Add(Def);

    UE_LOG(LogTemp, Log, TEXT("SuspenseCoreWeaponStateManager: Registered transition rule %s -> %s"),
        *Rule.FromState.ToString(), *Rule.ToState.ToString());

    return true;
}

bool USuspenseCoreWeaponStateManager::UnregisterTransitionRule(const FGameplayTag& FromState, const FGameplayTag& ToState)
{
    FScopeLock Lock(&StateMachineLock);

    int32 RemovedCount = TransitionDefinitions.RemoveAll([&FromState, &ToState](const FSuspenseCoreStateTransitionDef& Def)
    {
        return Def.FromState == FromState && Def.ToState == ToState;
    });

    if (RemovedCount > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("SuspenseCoreWeaponStateManager: Unregistered transition rule %s -> %s"),
            *FromState.ToString(), *ToState.ToString());
    }

    return RemovedCount > 0;
}

bool USuspenseCoreWeaponStateManager::GetTransitionRule(const FGameplayTag& FromState, const FGameplayTag& ToState, FSuspenseCoreStateTransitionRule& OutRule) const
{
    const FSuspenseCoreStateTransitionDef* Def = FindTransitionDef(FromState, ToState);
    if (!Def)
    {
        return false;
    }

    OutRule.FromState = Def->FromState;
    OutRule.ToState = Def->ToState;
    OutRule.Duration = Def->Duration;
    OutRule.bInterruptible = Def->bInterruptible;

    return true;
}

TArray<FSuspenseCoreStateTransitionRule> USuspenseCoreWeaponStateManager::GetAllTransitionRules() const
{
    FScopeLock Lock(&StateMachineLock);

    TArray<FSuspenseCoreStateTransitionRule> Rules;
    Rules.Reserve(TransitionDefinitions.Num());

    for (const FSuspenseCoreStateTransitionDef& Def : TransitionDefinitions)
    {
        FSuspenseCoreStateTransitionRule Rule;
        Rule.FromState = Def.FromState;
        Rule.ToState = Def.ToState;
        Rule.Duration = Def.Duration;
        Rule.bInterruptible = Def.bInterruptible;
        Rules.Add(Rule);
    }

    return Rules;
}

int32 USuspenseCoreWeaponStateManager::GetActiveWeaponSlot() const
{
    return ActiveWeaponSlot;
}

bool USuspenseCoreWeaponStateManager::SetActiveWeaponSlot(int32 SlotIndex)
{
    FScopeLock Lock(&StateMachineLock);

    if (SlotIndex != INDEX_NONE)
    {
        // Validate slot exists
        const FSuspenseCoreWeaponStateMachine* StateMachine = StateMachines.FindByPredicate(
            [SlotIndex](const FSuspenseCoreWeaponStateMachine& SM) { return SM.SlotIndex == SlotIndex; });

        if (!StateMachine)
        {
            UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreWeaponStateManager: SetActiveWeaponSlot failed - slot %d not found"), SlotIndex);
            return false;
        }
    }

    int32 PreviousSlot = ActiveWeaponSlot;
    ActiveWeaponSlot = SlotIndex;

    UE_LOG(LogTemp, Log, TEXT("SuspenseCoreWeaponStateManager: Active weapon slot changed from %d to %d"),
        PreviousSlot, ActiveWeaponSlot);

    return true;
}

USuspenseCoreEventBus* USuspenseCoreWeaponStateManager::GetEventBus() const
{
    return EventBus;
}

void USuspenseCoreWeaponStateManager::SetEventBus(USuspenseCoreEventBus* InEventBus)
{
    EventBus = InEventBus;
    UE_LOG(LogTemp, Log, TEXT("SuspenseCoreWeaponStateManager: EventBus set: %s"),
        EventBus ? *EventBus->GetName() : TEXT("None"));
}

FString USuspenseCoreWeaponStateManager::GetDebugInfo() const
{
    FScopeLock Lock(&StateMachineLock);

    FString DebugInfo;
    DebugInfo += TEXT("=== Weapon State Manager Debug ===\n");
    DebugInfo += FString::Printf(TEXT("Component: %s\n"), *GetName());
    DebugInfo += FString::Printf(TEXT("Owner: %s\n"), GetOwner() ? *GetOwner()->GetName() : TEXT("None"));
    DebugInfo += FString::Printf(TEXT("Active Weapon Slot: %d\n"), ActiveWeaponSlot);
    DebugInfo += FString::Printf(TEXT("State Machines: %d\n"), StateMachines.Num());
    DebugInfo += FString::Printf(TEXT("Transition Definitions: %d\n"), TransitionDefinitions.Num());
    DebugInfo += FString::Printf(TEXT("History Entries: %d\n"), StateHistory.Num());

    DebugInfo += TEXT("\n--- State Machines ---\n");
    for (const FSuspenseCoreWeaponStateMachine& SM : StateMachines)
    {
        DebugInfo += FString::Printf(TEXT("  Slot %d: %s%s\n"),
            SM.SlotIndex,
            *SM.CurrentState.ToString(),
            SM.bIsTransitioning ? TEXT(" (Transitioning)") : TEXT(""));
    }

    return DebugInfo;
}

FString USuspenseCoreWeaponStateManager::GetStatistics() const
{
    FString Stats;
    Stats += TEXT("=== Weapon State Manager Statistics ===\n");
    Stats += FString::Printf(TEXT("Transitions Requested: %d\n"), TotalTransitionsRequested);
    Stats += FString::Printf(TEXT("Transitions Completed: %d\n"), TotalTransitionsCompleted);
    Stats += FString::Printf(TEXT("Transitions Aborted: %d\n"), TotalTransitionsAborted);
    Stats += FString::Printf(TEXT("Force States: %d\n"), TotalForceStates);

    if (TotalTransitionsRequested > 0)
    {
        float SuccessRate = (float)TotalTransitionsCompleted / TotalTransitionsRequested * 100.0f;
        Stats += FString::Printf(TEXT("Success Rate: %.1f%%\n"), SuccessRate);
    }

    return Stats;
}

void USuspenseCoreWeaponStateManager::ResetStatistics()
{
    TotalTransitionsRequested = 0;
    TotalTransitionsCompleted = 0;
    TotalTransitionsAborted = 0;
    TotalForceStates = 0;

    UE_LOG(LogTemp, Log, TEXT("SuspenseCoreWeaponStateManager: Statistics reset"));
}
