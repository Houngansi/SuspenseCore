// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreWeaponStateManager.generated.h"

// Forward declarations
class USuspenseCoreEventBus;
class USuspenseCoreServiceLocator;
struct FSuspenseCoreEventData;

/**
 * FSuspenseCoreWeaponStateTransition
 *
 * Defines a valid state transition with conditions
 */
USTRUCT(BlueprintType)
struct FSuspenseCoreWeaponStateTransition
{
	GENERATED_BODY()

	/** Source state */
	UPROPERTY(BlueprintReadWrite, Category = "State")
	FGameplayTag FromState;

	/** Target state */
	UPROPERTY(BlueprintReadWrite, Category = "State")
	FGameplayTag ToState;

	/** Required tags for transition to be valid */
	UPROPERTY(BlueprintReadWrite, Category = "State")
	FGameplayTagContainer RequiredTags;

	/** Blocked tags that prevent transition */
	UPROPERTY(BlueprintReadWrite, Category = "State")
	FGameplayTagContainer BlockedTags;

	/** Transition priority (lower = higher priority) */
	UPROPERTY(BlueprintReadWrite, Category = "State")
	int32 Priority = 0;

	/** Can transition be interrupted */
	UPROPERTY(BlueprintReadWrite, Category = "State")
	bool bInterruptible = true;
};

/**
 * FSuspenseCoreWeaponStateData
 *
 * Runtime data for current weapon state
 */
USTRUCT(BlueprintType)
struct FSuspenseCoreWeaponStateData
{
	GENERATED_BODY()

	/** Current state tag */
	UPROPERTY(BlueprintReadOnly, Category = "State")
	FGameplayTag CurrentState;

	/** Previous state tag */
	UPROPERTY(BlueprintReadOnly, Category = "State")
	FGameplayTag PreviousState;

	/** Time when entered current state */
	UPROPERTY(BlueprintReadOnly, Category = "State")
	float StateEntryTime = 0.0f;

	/** Duration in current state */
	UPROPERTY(BlueprintReadOnly, Category = "State")
	float StateDuration = 0.0f;

	/** Is state transition in progress */
	UPROPERTY(BlueprintReadOnly, Category = "State")
	bool bIsTransitioning = false;

	/** Active state tags */
	UPROPERTY(BlueprintReadOnly, Category = "State")
	FGameplayTagContainer ActiveStateTags;

	/** Get time in current state */
	float GetTimeInState(float CurrentTime) const
	{
		return CurrentTime - StateEntryTime;
	}

	/** Is in specific state */
	bool IsInState(FGameplayTag StateTag) const
	{
		return CurrentState.MatchesTagExact(StateTag);
	}
};

/**
 * USuspenseCoreWeaponStateManager
 *
 * Manages weapon state machine with event-driven transitions.
 *
 * Architecture:
 * - EventBus: Publishes state change events
 * - GameplayTags: State identification and transition rules
 * - State Machine: Deterministic state transitions with validation
 *
 * Responsibilities:
 * - Manage weapon state lifecycle (Idle, Firing, Reloading, etc.)
 * - Validate and execute state transitions
 * - Track state history and timing
 * - Publish state change events through EventBus
 * - Support state queries and predictions
 */
UCLASS(BlueprintType)
class EQUIPMENTSYSTEM_API USuspenseCoreWeaponStateManager : public UObject
{
	GENERATED_BODY()

public:
	USuspenseCoreWeaponStateManager();

	//================================================
	// Initialization
	//================================================

	/**
	 * Initialize state manager with dependencies
	 * @param InServiceLocator ServiceLocator for dependency injection
	 * @param InOwner Owning weapon actor/component
	 * @return True if initialized successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|WeaponState")
	bool Initialize(USuspenseCoreServiceLocator* InServiceLocator, UObject* InOwner);

	/**
	 * Setup initial state and transitions
	 * @param InitialState Starting state tag
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|WeaponState")
	void SetupStateMachine(FGameplayTag InitialState);

	/**
	 * Check if state manager is initialized
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|WeaponState")
	bool IsInitialized() const { return bIsInitialized; }

	//================================================
	// State Transitions
	//================================================

	/**
	 * Request state transition
	 * @param NewState Target state
	 * @param bForceTransition Force transition even if not normally allowed
	 * @return True if transition succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|WeaponState")
	bool RequestStateTransition(FGameplayTag NewState, bool bForceTransition = false);

	/**
	 * Can transition to target state
	 * @param TargetState State to check
	 * @param OutReason Reason if transition not allowed
	 * @return True if transition is valid
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|WeaponState")
	bool CanTransitionTo(FGameplayTag TargetState, FText& OutReason) const;

	/**
	 * Force immediate state change (skips validation)
	 * Use with caution - for initialization or emergency resets
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|WeaponState")
	void ForceSetState(FGameplayTag NewState);

	/**
	 * Return to previous state
	 * @return True if successfully returned to previous state
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|WeaponState")
	bool ReturnToPreviousState();

	//================================================
	// State Queries
	//================================================

	/**
	 * Get current state data
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|WeaponState")
	FSuspenseCoreWeaponStateData GetCurrentStateData() const { return CurrentStateData; }

	/**
	 * Get current state tag
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|WeaponState")
	FGameplayTag GetCurrentState() const { return CurrentStateData.CurrentState; }

	/**
	 * Get previous state tag
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|WeaponState")
	FGameplayTag GetPreviousState() const { return CurrentStateData.PreviousState; }

	/**
	 * Is in specific state
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|WeaponState")
	bool IsInState(FGameplayTag StateTag) const;

	/**
	 * Is in any of the specified states
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|WeaponState")
	bool IsInAnyState(const FGameplayTagContainer& StateTags) const;

	/**
	 * Get time in current state
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|WeaponState")
	float GetTimeInCurrentState() const;

	/**
	 * Is currently transitioning
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|WeaponState")
	bool IsTransitioning() const { return CurrentStateData.bIsTransitioning; }

	//================================================
	// Transition Configuration
	//================================================

	/**
	 * Register valid state transition
	 * @param Transition Transition definition
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|WeaponState|Config")
	void RegisterTransition(const FSuspenseCoreWeaponStateTransition& Transition);

	/**
	 * Remove registered transition
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|WeaponState|Config")
	void UnregisterTransition(FGameplayTag FromState, FGameplayTag ToState);

	/**
	 * Get all valid transitions from current state
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|WeaponState|Config")
	TArray<FGameplayTag> GetValidTransitionsFromCurrentState() const;

	//================================================
	// State Tags
	//================================================

	/**
	 * Add active state tag
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|WeaponState|Tags")
	void AddStateTag(FGameplayTag Tag);

	/**
	 * Remove active state tag
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|WeaponState|Tags")
	void RemoveStateTag(FGameplayTag Tag);

	/**
	 * Has specific state tag
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|WeaponState|Tags")
	bool HasStateTag(FGameplayTag Tag) const;

	/**
	 * Clear all state tags
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|WeaponState|Tags")
	void ClearStateTags();

	//================================================
	// Statistics
	//================================================

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|WeaponState|Stats")
	int32 GetTotalTransitions() const { return TotalTransitions; }

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|WeaponState|Stats")
	int32 GetFailedTransitions() const { return FailedTransitions; }

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|WeaponState|Stats")
	void ResetStatistics();

protected:
	//================================================
	// Event Publishing
	//================================================

	/** Publish state changed event */
	void PublishStateChanged(FGameplayTag OldState, FGameplayTag NewState, bool bInterrupted);

	/** Publish transition failed event */
	void PublishTransitionFailed(FGameplayTag FromState, FGameplayTag ToState, const FText& Reason);

	//================================================
	// Internal Methods
	//================================================

	/** Execute state transition */
	bool ExecuteTransition(FGameplayTag NewState, bool bForce);

	/** Find valid transition */
	const FSuspenseCoreWeaponStateTransition* FindTransition(FGameplayTag FromState, FGameplayTag ToState) const;

	/** Validate transition against current conditions */
	bool ValidateTransition(const FSuspenseCoreWeaponStateTransition& Transition, FText& OutReason) const;

	/** Setup default weapon state transitions */
	void SetupDefaultTransitions();

private:
	//================================================
	// Dependencies (Injected)
	//================================================

	/** ServiceLocator for dependency injection */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreServiceLocator> ServiceLocator;

	/** EventBus for event publishing */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> EventBus;

	/** Owning weapon */
	UPROPERTY()
	TWeakObjectPtr<UObject> Owner;

	//================================================
	// State Data
	//================================================

	/** Current state information */
	UPROPERTY()
	FSuspenseCoreWeaponStateData CurrentStateData;

	/** Registered state transitions */
	UPROPERTY()
	TArray<FSuspenseCoreWeaponStateTransition> RegisteredTransitions;

	//================================================
	// State
	//================================================

	/** Initialization flag */
	UPROPERTY()
	bool bIsInitialized;

	//================================================
	// Statistics
	//================================================

	/** Total state transitions */
	UPROPERTY()
	int32 TotalTransitions;

	/** Failed transitions */
	UPROPERTY()
	int32 FailedTransitions;

	//================================================
	// Thread Safety
	//================================================

	/** Critical section for thread-safe state access */
	mutable FCriticalSection StateCriticalSection;
};
