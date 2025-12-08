// ISuspenseCoreWeaponStateProvider.h
// SuspenseCore Weapon State Provider Interface
// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "ISuspenseCoreWeaponStateProvider.generated.h"

// Forward declarations
class USuspenseCoreEventBus;

/**
 * Weapon state transition request
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreWeaponStateTransitionRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Transition")
	FGameplayTag FromState;

	UPROPERTY(BlueprintReadWrite, Category = "Transition")
	FGameplayTag ToState;

	UPROPERTY(BlueprintReadWrite, Category = "Transition")
	int32 WeaponSlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "Transition")
	float TransitionDuration = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Transition")
	bool bForceTransition = false;

	UPROPERTY(BlueprintReadWrite, Category = "Transition")
	FGameplayTag ContextTag;

	UPROPERTY(BlueprintReadOnly, Category = "Transition")
	FGuid RequestId;

	FSuspenseCoreWeaponStateTransitionRequest()
	{
		RequestId = FGuid::NewGuid();
	}

	static FSuspenseCoreWeaponStateTransitionRequest Create(
		const FGameplayTag& From,
		const FGameplayTag& To,
		int32 SlotIndex = INDEX_NONE)
	{
		FSuspenseCoreWeaponStateTransitionRequest Request;
		Request.FromState = From;
		Request.ToState = To;
		Request.WeaponSlotIndex = SlotIndex;
		return Request;
	}
};

/**
 * Weapon state transition result
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreWeaponStateTransitionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FText FailureReason;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FGameplayTag ResultingState;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FGameplayTag PreviousState;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	float ActualDuration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 AffectedSlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FGuid RequestId;

	static FSuspenseCoreWeaponStateTransitionResult Success(
		const FGameplayTag& NewState,
		const FGameplayTag& OldState,
		float Duration,
		const FGuid& InRequestId)
	{
		FSuspenseCoreWeaponStateTransitionResult Result;
		Result.bSuccess = true;
		Result.ResultingState = NewState;
		Result.PreviousState = OldState;
		Result.ActualDuration = Duration;
		Result.RequestId = InRequestId;
		return Result;
	}

	static FSuspenseCoreWeaponStateTransitionResult Failure(
		const FText& Reason,
		const FGameplayTag& CurrentState,
		const FGuid& InRequestId)
	{
		FSuspenseCoreWeaponStateTransitionResult Result;
		Result.bSuccess = false;
		Result.FailureReason = Reason;
		Result.ResultingState = CurrentState;
		Result.RequestId = InRequestId;
		return Result;
	}
};

/**
 * Weapon state snapshot for a slot
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreWeaponStateSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "State")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	FGameplayTag CurrentState;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	FGameplayTag TargetState;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	bool bIsTransitioning = false;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	float TransitionProgress = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	float TransitionDuration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	float StateEntryTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	FName WeaponId = NAME_None;

	bool IsValid() const { return CurrentState.IsValid(); }
};

/**
 * State transition rule
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreStateTransitionRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	FGameplayTag FromState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	FGameplayTag ToState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	float DefaultDuration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	bool bCanInterrupt = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	FGameplayTagContainer RequiredTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	FGameplayTagContainer BlockedByTags;

	bool Matches(const FGameplayTag& From, const FGameplayTag& To) const
	{
		return FromState == From && ToState == To;
	}
};

/**
 * Weapon state history entry
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreWSPHistoryEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "History")
	FGameplayTag State;

	UPROPERTY(BlueprintReadOnly, Category = "History")
	float EntryTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "History")
	float ExitTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "History")
	FGameplayTag TransitionReason;

	UPROPERTY(BlueprintReadOnly, Category = "History")
	int32 SlotIndex = INDEX_NONE;

	float GetDuration() const { return ExitTime > 0.0f ? ExitTime - EntryTime : 0.0f; }
};

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreWeaponStateProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISuspenseCoreWeaponStateProvider
 *
 * Interface for weapon state management using FSM (Finite State Machine).
 * Manages weapon states (Idle, Equipping, Equipped, Firing, Reloading, etc.)
 *
 * Architecture:
 * - FSM-based state management
 * - Validates state transitions
 * - Supports interruptible transitions
 * - Integrates with EventBus for notifications
 *
 * EventBus Events Published:
 * - SuspenseCore.Event.Weapon.StateChanged
 * - SuspenseCore.Event.Weapon.TransitionStarted
 * - SuspenseCore.Event.Weapon.TransitionCompleted
 * - SuspenseCore.Event.Weapon.TransitionAborted
 *
 * Standard States:
 * - Weapon.State.Idle
 * - Weapon.State.Equipping
 * - Weapon.State.Equipped
 * - Weapon.State.Holstering
 * - Weapon.State.Holstered
 * - Weapon.State.Firing
 * - Weapon.State.Reloading
 * - Weapon.State.Inspecting
 */
class BRIDGESYSTEM_API ISuspenseCoreWeaponStateProvider
{
	GENERATED_BODY()

public:
	//========================================
	// State Query
	//========================================

	/**
	 * Get current weapon state
	 * @param SlotIndex Weapon slot (-1 for active weapon)
	 * @return Current state tag
	 */
	virtual FGameplayTag GetWeaponState(int32 SlotIndex = INDEX_NONE) const = 0;

	/**
	 * Get complete state snapshot for slot
	 * @param SlotIndex Weapon slot
	 * @return State snapshot
	 */
	virtual FSuspenseCoreWeaponStateSnapshot GetStateSnapshot(
		int32 SlotIndex = INDEX_NONE) const = 0;

	/**
	 * Get all weapon state snapshots
	 * @return Array of snapshots for all weapon slots
	 */
	virtual TArray<FSuspenseCoreWeaponStateSnapshot> GetAllStateSnapshots() const = 0;

	//========================================
	// State Transitions
	//========================================

	/**
	 * Request state transition
	 * @param Request Transition request
	 * @return Transition result
	 */
	virtual FSuspenseCoreWeaponStateTransitionResult RequestStateTransition(
		const FSuspenseCoreWeaponStateTransitionRequest& Request) = 0;

	/**
	 * Simple state transition
	 * @param NewState State to transition to
	 * @param SlotIndex Weapon slot
	 * @return Transition result
	 */
	virtual FSuspenseCoreWeaponStateTransitionResult TransitionTo(
		const FGameplayTag& NewState,
		int32 SlotIndex = INDEX_NONE) = 0;

	/**
	 * Check if transition is valid
	 * @param FromState Starting state
	 * @param ToState Target state
	 * @return True if transition is allowed
	 */
	virtual bool CanTransitionTo(
		const FGameplayTag& FromState,
		const FGameplayTag& ToState) const = 0;

	/**
	 * Get valid transitions from current state
	 * @param CurrentState State to query
	 * @return Array of valid target states
	 */
	virtual TArray<FGameplayTag> GetValidTransitions(
		const FGameplayTag& CurrentState) const = 0;

	//========================================
	// Force State Operations
	//========================================

	/**
	 * Force state without transition animation
	 * @param NewState State to force
	 * @param SlotIndex Weapon slot
	 * @return True if successful
	 */
	virtual bool ForceState(
		const FGameplayTag& NewState,
		int32 SlotIndex = INDEX_NONE) = 0;

	/**
	 * Reset to default state
	 * @param SlotIndex Weapon slot
	 * @return True if reset
	 */
	virtual bool ResetToDefaultState(int32 SlotIndex = INDEX_NONE) = 0;

	//========================================
	// Transition Management
	//========================================

	/**
	 * Get transition duration between states
	 * @param FromState Starting state
	 * @param ToState Target state
	 * @return Duration in seconds
	 */
	virtual float GetTransitionDuration(
		const FGameplayTag& FromState,
		const FGameplayTag& ToState) const = 0;

	/**
	 * Check if weapon is currently transitioning
	 * @param SlotIndex Weapon slot
	 * @return True if in transition
	 */
	virtual bool IsTransitioning(int32 SlotIndex = INDEX_NONE) const = 0;

	/**
	 * Get transition progress
	 * @param SlotIndex Weapon slot
	 * @return Progress from 0 to 1
	 */
	virtual float GetTransitionProgress(int32 SlotIndex = INDEX_NONE) const = 0;

	/**
	 * Abort current transition
	 * @param SlotIndex Weapon slot
	 * @return True if aborted
	 */
	virtual bool AbortTransition(int32 SlotIndex = INDEX_NONE) = 0;

	//========================================
	// Transition Rules
	//========================================

	/**
	 * Register state transition rule
	 * @param Rule Transition rule to register
	 * @return True if registered
	 */
	virtual bool RegisterTransitionRule(
		const FSuspenseCoreStateTransitionRule& Rule) = 0;

	/**
	 * Unregister transition rule
	 * @param FromState Starting state
	 * @param ToState Target state
	 * @return True if removed
	 */
	virtual bool UnregisterTransitionRule(
		const FGameplayTag& FromState,
		const FGameplayTag& ToState) = 0;

	/**
	 * Get transition rule
	 * @param FromState Starting state
	 * @param ToState Target state
	 * @param OutRule Found rule
	 * @return True if found
	 */
	virtual bool GetTransitionRule(
		const FGameplayTag& FromState,
		const FGameplayTag& ToState,
		FSuspenseCoreStateTransitionRule& OutRule) const = 0;

	/**
	 * Get all registered rules
	 * @return Array of transition rules
	 */
	virtual TArray<FSuspenseCoreStateTransitionRule> GetAllTransitionRules() const = 0;

	//========================================
	// Active Weapon
	//========================================

	/**
	 * Get currently active weapon slot
	 * @return Active slot index
	 */
	virtual int32 GetActiveWeaponSlot() const = 0;

	/**
	 * Set active weapon slot
	 * @param SlotIndex Slot to activate
	 * @return True if activated
	 */
	virtual bool SetActiveWeaponSlot(int32 SlotIndex) = 0;

	//========================================
	// State History
	//========================================

	/**
	 * Get state history
	 * @param MaxCount Maximum entries to return
	 * @return Array of history entries
	 */
	virtual TArray<FSuspenseCoreWSPHistoryEntry> GetStateHistory(
		int32 MaxCount = 10) const = 0;

	/**
	 * Get state history for specific slot
	 * @param SlotIndex Slot to query
	 * @param MaxCount Maximum entries
	 * @return Array of history entries
	 */
	virtual TArray<FSuspenseCoreWSPHistoryEntry> GetSlotStateHistory(
		int32 SlotIndex,
		int32 MaxCount = 10) const = 0;

	/**
	 * Clear state history
	 */
	virtual void ClearStateHistory() = 0;

	//========================================
	// EventBus Integration
	//========================================

	/**
	 * Get EventBus used by this provider
	 * @return EventBus instance
	 */
	virtual USuspenseCoreEventBus* GetEventBus() const = 0;

	/**
	 * Set EventBus for this provider
	 * @param InEventBus EventBus to use
	 */
	virtual void SetEventBus(USuspenseCoreEventBus* InEventBus) = 0;

	//========================================
	// Diagnostics
	//========================================

	/**
	 * Get state machine debug info
	 * @return Formatted debug string
	 */
	virtual FString GetDebugInfo() const = 0;

	/**
	 * Get statistics
	 * @return Formatted statistics string
	 */
	virtual FString GetStatistics() const = 0;

	/**
	 * Reset statistics
	 */
	virtual void ResetStatistics() = 0;
};

//========================================
// Backward Compatibility Aliases
//========================================

/** Legacy type aliases for existing implementation files */
using FWeaponStateTransitionRequest = FSuspenseCoreWeaponStateTransitionRequest;
using FWeaponStateTransitionResult = FSuspenseCoreWeaponStateTransitionResult;
using FWeaponStateSnapshot = FSuspenseCoreWeaponStateSnapshot;
using FStateTransitionRule = FSuspenseCoreStateTransitionRule;

/** Legacy interface alias */
using ISuspenseWeaponStateProvider = ISuspenseCoreWeaponStateProvider;
using USuspenseWeaponStateProvider = USuspenseCoreWeaponStateProvider;
