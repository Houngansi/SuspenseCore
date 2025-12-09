// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreWeaponStateProvider.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentDataProvider.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEventDispatcher.h"
#include "SuspenseCoreWeaponStateManager.generated.h"

/**
 * Weapon state machine configuration
 */
USTRUCT()
struct FSuspenseCoreWeaponStateMachine
{
    GENERATED_BODY()

    UPROPERTY()
    int32 SlotIndex = INDEX_NONE;

    UPROPERTY()
    FGameplayTag CurrentState;

    UPROPERTY()
    FGameplayTag PreviousState;

    UPROPERTY()
    bool bIsTransitioning = false;

    UPROPERTY()
    float TransitionStartTime = 0.0f;

    UPROPERTY()
    float TransitionDuration = 0.0f;

    UPROPERTY()
    FSuspenseCoreWeaponStateTransitionRequest ActiveTransition;
};

/**
 * State transition definition
 */
USTRUCT()
struct FSuspenseCoreStateTransitionDef
{
    GENERATED_BODY()

    UPROPERTY()
    FGameplayTag FromState;

    UPROPERTY()
    FGameplayTag ToState;

    UPROPERTY()
    float Duration = 0.2f;

    UPROPERTY()
    bool bInterruptible = false;

    UPROPERTY()
    FGameplayTagContainer RequiredTags;
};

USTRUCT(BlueprintType)
struct FSuspenseCoreWeaponStateHistoryEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag State;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TimeSeconds = 0.f;
};

/**
 * Weapon State Manager Component
 *
 * Philosophy: Dedicated finite state machine for weapon states.
 * Manages state transitions, validates state changes, and tracks history.
 *
 * Key Principles:
 * - Single Responsibility: Only manages weapon states
 * - State Machine Pattern: Clear transition rules and validation
 * - Per-slot independence: Each weapon slot has its own state
 * - Event-driven: Broadcasts state changes for observers
 */
UCLASS(ClassGroup=(Equipment), meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseCoreWeaponStateManager : public UActorComponent, public ISuspenseCoreWeaponStateProvider
{
    GENERATED_BODY()

public:
    USuspenseCoreWeaponStateManager();

    //~ Begin UActorComponent Interface
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    //~ End UActorComponent Interface

    //~ Begin ISuspenseCoreWeaponStateProvider Interface
    virtual FGameplayTag GetWeaponState(int32 SlotIndex = -1) const override;
    virtual FSuspenseCoreWeaponStateTransitionResult RequestStateTransition(const FSuspenseCoreWeaponStateTransitionRequest& Request) override;
    virtual bool CanTransitionTo(const FGameplayTag& FromState, const FGameplayTag& ToState) const override;
    virtual TArray<FGameplayTag> GetValidTransitions(const FGameplayTag& CurrentState) const override;
    virtual bool ForceState(const FGameplayTag& NewState, int32 SlotIndex = -1) override;
    virtual float GetTransitionDuration(const FGameplayTag& FromState, const FGameplayTag& ToState) const override;
    virtual bool IsTransitioning(int32 SlotIndex = -1) const override;
    virtual float GetTransitionProgress(int32 SlotIndex = -1) const override;
    virtual bool AbortTransition(int32 SlotIndex = -1) override;
    virtual TArray<FGameplayTag> GetStateHistory(int32 MaxCount = 10) const override;
    //~ End ISuspenseCoreWeaponStateProvider Interface

    /**
     * Initialize with dependencies
     */
    UFUNCTION(BlueprintCallable, Category = "Weapon|State")
    bool Initialize(
        TScriptInterface<ISuspenseCoreEquipmentDataProvider> DataProvider,
        TScriptInterface<ISuspenseCoreEventDispatcher> EventDispatcher
    );

    /**
     * Register custom state transition
     */
    //UFUNCTION(BlueprintCallable, Category = "Weapon|State")
    void RegisterTransition(const FSuspenseCoreStateTransitionDef& Transition);

    /**
     * Get all state machines
     */
    //UFUNCTION(BlueprintCallable, Category = "Weapon|State")
    TArray<FSuspenseCoreWeaponStateMachine> GetAllStateMachines() const { return StateMachines; }

private:
    // Process ongoing transitions
    void UpdateTransitions(float DeltaTime);

    // Complete a transition
    void CompleteTransition(int32 SlotIndex);

    // Get or create state machine for slot
    FSuspenseCoreWeaponStateMachine& GetOrCreateStateMachine(int32 SlotIndex);

    // Find transition definition
    const FSuspenseCoreStateTransitionDef* FindTransitionDef(const FGameplayTag& FromState, const FGameplayTag& ToState) const;

    // Record state change in history
    void RecordStateChange(int32 SlotIndex, const FGameplayTag& NewState);

    // Broadcast state change event
    void BroadcastStateChange(int32 SlotIndex, const FGameplayTag& OldState, const FGameplayTag& NewState);

private:
    // Dependencies
    UPROPERTY()
    TScriptInterface<ISuspenseCoreEquipmentDataProvider> DataProvider;

    UPROPERTY()
    TScriptInterface<ISuspenseCoreEventDispatcher> EventDispatcher;

    // State machines per slot
    UPROPERTY()
    TArray<FSuspenseCoreWeaponStateMachine> StateMachines;

    // Transition definitions
    UPROPERTY()
    TArray<FSuspenseCoreStateTransitionDef> TransitionDefinitions;

    // State history (circular buffer)
	UPROPERTY()
	TArray<FSuspenseCoreWeaponStateHistoryEntry> StateHistory;

    UPROPERTY()
    int32 MaxHistorySize = 50;

    // Default states
    UPROPERTY()
    FGameplayTag DefaultIdleState;

    UPROPERTY()
    FGameplayTag DefaultHolsteredState;

    // Active weapon slot tracking
    UPROPERTY()
    int32 ActiveWeaponSlot = INDEX_NONE;

    // Thread safety
    mutable FCriticalSection StateMachineLock;
};
