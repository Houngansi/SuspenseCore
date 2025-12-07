// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Components/Core/SuspenseCoreWeaponStateManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Services/SuspenseCoreServiceLocator.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreWeaponState, Log, All);

#define WEAPONSTATE_LOG(Verbosity, Format, ...) \
	UE_LOG(LogSuspenseCoreWeaponState, Verbosity, TEXT("%s: " Format), *GetNameSafe(this), ##__VA_ARGS__)

USuspenseCoreWeaponStateManager::USuspenseCoreWeaponStateManager()
{
	bIsInitialized = false;
	TotalTransitions = 0;
	FailedTransitions = 0;
}

bool USuspenseCoreWeaponStateManager::Initialize(
	USuspenseCoreServiceLocator* InServiceLocator,
	UObject* InOwner)
{
	if (!InServiceLocator || !InOwner)
	{
		WEAPONSTATE_LOG(Error, TEXT("Initialize: Invalid parameters"));
		return false;
	}

	ServiceLocator = InServiceLocator;
	Owner = InOwner;

	// Get EventBus from ServiceLocator
	EventBus = ServiceLocator->GetService<USuspenseCoreEventBus>();
	if (!EventBus.IsValid())
	{
		WEAPONSTATE_LOG(Warning, TEXT("Initialize: EventBus not found in ServiceLocator"));
	}

	// Setup default transitions
	SetupDefaultTransitions();

	bIsInitialized = true;
	WEAPONSTATE_LOG(Log, TEXT("Initialize: Success"));
	return true;
}

void USuspenseCoreWeaponStateManager::SetupStateMachine(FGameplayTag InitialState)
{
	if (!InitialState.IsValid())
	{
		WEAPONSTATE_LOG(Error, TEXT("SetupStateMachine: Invalid initial state"));
		return;
	}

	FScopeLock Lock(&StateCriticalSection);

	CurrentStateData.CurrentState = InitialState;
	CurrentStateData.PreviousState = FGameplayTag();
	CurrentStateData.StateEntryTime = 0.0f;
	CurrentStateData.bIsTransitioning = false;
	CurrentStateData.ActiveStateTags.Reset();

	WEAPONSTATE_LOG(Log, TEXT("SetupStateMachine: Initial state set to %s"), *InitialState.ToString());
}

bool USuspenseCoreWeaponStateManager::RequestStateTransition(FGameplayTag NewState, bool bForceTransition)
{
	if (!bIsInitialized)
	{
		WEAPONSTATE_LOG(Error, TEXT("RequestStateTransition: State manager not initialized"));
		return false;
	}

	if (!NewState.IsValid())
	{
		WEAPONSTATE_LOG(Error, TEXT("RequestStateTransition: Invalid target state"));
		return false;
	}

	FScopeLock Lock(&StateCriticalSection);

	// Check if already in target state
	if (CurrentStateData.CurrentState.MatchesTagExact(NewState))
	{
		WEAPONSTATE_LOG(Verbose, TEXT("RequestStateTransition: Already in state %s"), *NewState.ToString());
		return true;
	}

	// Validate transition
	FText ValidationReason;
	if (!bForceTransition && !CanTransitionTo(NewState, ValidationReason))
	{
		WEAPONSTATE_LOG(Warning, TEXT("RequestStateTransition: Transition denied - %s"),
			*ValidationReason.ToString());
		PublishTransitionFailed(CurrentStateData.CurrentState, NewState, ValidationReason);
		FailedTransitions++;
		return false;
	}

	// Execute transition
	return ExecuteTransition(NewState, bForceTransition);
}

bool USuspenseCoreWeaponStateManager::CanTransitionTo(FGameplayTag TargetState, FText& OutReason) const
{
	FScopeLock Lock(&StateCriticalSection);

	// Find transition
	const FSuspenseCoreWeaponStateTransition* Transition = FindTransition(
		CurrentStateData.CurrentState,
		TargetState);

	if (!Transition)
	{
		OutReason = FText::FromString(FString::Printf(
			TEXT("No transition defined from %s to %s"),
			*CurrentStateData.CurrentState.ToString(),
			*TargetState.ToString()));
		return false;
	}

	// Validate transition conditions
	return ValidateTransition(*Transition, OutReason);
}

void USuspenseCoreWeaponStateManager::ForceSetState(FGameplayTag NewState)
{
	if (!NewState.IsValid())
	{
		return;
	}

	FScopeLock Lock(&StateCriticalSection);

	const FGameplayTag OldState = CurrentStateData.CurrentState;
	CurrentStateData.PreviousState = OldState;
	CurrentStateData.CurrentState = NewState;
	CurrentStateData.StateEntryTime = 0.0f;
	CurrentStateData.bIsTransitioning = false;

	WEAPONSTATE_LOG(Warning, TEXT("ForceSetState: %s -> %s (forced)"),
		*OldState.ToString(),
		*NewState.ToString());

	PublishStateChanged(OldState, NewState, true);
}

bool USuspenseCoreWeaponStateManager::ReturnToPreviousState()
{
	FScopeLock Lock(&StateCriticalSection);

	if (!CurrentStateData.PreviousState.IsValid())
	{
		WEAPONSTATE_LOG(Warning, TEXT("ReturnToPreviousState: No previous state"));
		return false;
	}

	return RequestStateTransition(CurrentStateData.PreviousState, false);
}

bool USuspenseCoreWeaponStateManager::IsInState(FGameplayTag StateTag) const
{
	FScopeLock Lock(&StateCriticalSection);
	return CurrentStateData.CurrentState.MatchesTagExact(StateTag);
}

bool USuspenseCoreWeaponStateManager::IsInAnyState(const FGameplayTagContainer& StateTags) const
{
	FScopeLock Lock(&StateCriticalSection);
	return StateTags.HasTag(CurrentStateData.CurrentState);
}

float USuspenseCoreWeaponStateManager::GetTimeInCurrentState() const
{
	FScopeLock Lock(&StateCriticalSection);
	const float CurrentTime = 0.0f; // TODO: Get actual game time
	return CurrentStateData.GetTimeInState(CurrentTime);
}

void USuspenseCoreWeaponStateManager::RegisterTransition(
	const FSuspenseCoreWeaponStateTransition& Transition)
{
	FScopeLock Lock(&StateCriticalSection);

	RegisteredTransitions.Add(Transition);

	WEAPONSTATE_LOG(Verbose, TEXT("RegisterTransition: %s -> %s"),
		*Transition.FromState.ToString(),
		*Transition.ToState.ToString());
}

void USuspenseCoreWeaponStateManager::UnregisterTransition(FGameplayTag FromState, FGameplayTag ToState)
{
	FScopeLock Lock(&StateCriticalSection);

	RegisteredTransitions.RemoveAll([&](const FSuspenseCoreWeaponStateTransition& Trans)
	{
		return Trans.FromState.MatchesTagExact(FromState) && Trans.ToState.MatchesTagExact(ToState);
	});
}

TArray<FGameplayTag> USuspenseCoreWeaponStateManager::GetValidTransitionsFromCurrentState() const
{
	FScopeLock Lock(&StateCriticalSection);

	TArray<FGameplayTag> ValidTransitions;

	for (const FSuspenseCoreWeaponStateTransition& Trans : RegisteredTransitions)
	{
		if (Trans.FromState.MatchesTagExact(CurrentStateData.CurrentState))
		{
			FText ValidationReason;
			if (ValidateTransition(Trans, ValidationReason))
			{
				ValidTransitions.Add(Trans.ToState);
			}
		}
	}

	return ValidTransitions;
}

void USuspenseCoreWeaponStateManager::AddStateTag(FGameplayTag Tag)
{
	FScopeLock Lock(&StateCriticalSection);
	CurrentStateData.ActiveStateTags.AddTag(Tag);
}

void USuspenseCoreWeaponStateManager::RemoveStateTag(FGameplayTag Tag)
{
	FScopeLock Lock(&StateCriticalSection);
	CurrentStateData.ActiveStateTags.RemoveTag(Tag);
}

bool USuspenseCoreWeaponStateManager::HasStateTag(FGameplayTag Tag) const
{
	FScopeLock Lock(&StateCriticalSection);
	return CurrentStateData.ActiveStateTags.HasTag(Tag);
}

void USuspenseCoreWeaponStateManager::ClearStateTags()
{
	FScopeLock Lock(&StateCriticalSection);
	CurrentStateData.ActiveStateTags.Reset();
}

void USuspenseCoreWeaponStateManager::ResetStatistics()
{
	TotalTransitions = 0;
	FailedTransitions = 0;
	WEAPONSTATE_LOG(Log, TEXT("ResetStatistics"));
}

void USuspenseCoreWeaponStateManager::PublishStateChanged(
	FGameplayTag OldState,
	FGameplayTag NewState,
	bool bInterrupted)
{
	if (!EventBus.IsValid())
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(Owner.Get());
	EventData.SetString(TEXT("OldState"), OldState.ToString());
	EventData.SetString(TEXT("NewState"), NewState.ToString());
	EventData.SetBool(TEXT("Interrupted"), bInterrupted);

	EventBus->Publish(
		FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.Weapon.State.Changed"))),
		EventData);

	WEAPONSTATE_LOG(Verbose, TEXT("PublishStateChanged: %s -> %s"),
		*OldState.ToString(),
		*NewState.ToString());
}

void USuspenseCoreWeaponStateManager::PublishTransitionFailed(
	FGameplayTag FromState,
	FGameplayTag ToState,
	const FText& Reason)
{
	if (!EventBus.IsValid())
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(Owner.Get());
	EventData.SetString(TEXT("FromState"), FromState.ToString());
	EventData.SetString(TEXT("ToState"), ToState.ToString());
	EventData.SetString(TEXT("Reason"), Reason.ToString());

	EventBus->Publish(
		FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.Weapon.State.TransitionFailed"))),
		EventData);

	WEAPONSTATE_LOG(Warning, TEXT("PublishTransitionFailed: %s -> %s - %s"),
		*FromState.ToString(),
		*ToState.ToString(),
		*Reason.ToString());
}

bool USuspenseCoreWeaponStateManager::ExecuteTransition(FGameplayTag NewState, bool bForce)
{
	const FGameplayTag OldState = CurrentStateData.CurrentState;

	// Update state
	CurrentStateData.PreviousState = OldState;
	CurrentStateData.CurrentState = NewState;
	CurrentStateData.StateEntryTime = 0.0f; // TODO: Get actual game time
	CurrentStateData.bIsTransitioning = false;

	TotalTransitions++;

	WEAPONSTATE_LOG(Log, TEXT("ExecuteTransition: %s -> %s"),
		*OldState.ToString(),
		*NewState.ToString());

	// Publish state changed event
	PublishStateChanged(OldState, NewState, bForce);

	return true;
}

const FSuspenseCoreWeaponStateTransition* USuspenseCoreWeaponStateManager::FindTransition(
	FGameplayTag FromState,
	FGameplayTag ToState) const
{
	for (const FSuspenseCoreWeaponStateTransition& Trans : RegisteredTransitions)
	{
		if (Trans.FromState.MatchesTagExact(FromState) && Trans.ToState.MatchesTagExact(ToState))
		{
			return &Trans;
		}
	}

	return nullptr;
}

bool USuspenseCoreWeaponStateManager::ValidateTransition(
	const FSuspenseCoreWeaponStateTransition& Transition,
	FText& OutReason) const
{
	// Check required tags
	if (Transition.RequiredTags.Num() > 0)
	{
		if (!CurrentStateData.ActiveStateTags.HasAll(Transition.RequiredTags))
		{
			OutReason = FText::FromString(TEXT("Missing required tags for transition"));
			return false;
		}
	}

	// Check blocked tags
	if (Transition.BlockedTags.Num() > 0)
	{
		if (CurrentStateData.ActiveStateTags.HasAny(Transition.BlockedTags))
		{
			OutReason = FText::FromString(TEXT("Blocked tags present for transition"));
			return false;
		}
	}

	return true;
}

void USuspenseCoreWeaponStateManager::SetupDefaultTransitions()
{
	// TODO: Setup common weapon state transitions
	// Example: Idle -> Firing, Firing -> Reloading, etc.
	WEAPONSTATE_LOG(Log, TEXT("SetupDefaultTransitions: Complete"));
}
