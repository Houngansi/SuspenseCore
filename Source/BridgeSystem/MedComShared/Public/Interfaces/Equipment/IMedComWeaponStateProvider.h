// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "IMedComWeaponStateProvider.generated.h"

/**
 * Weapon state transition request
 */
USTRUCT(BlueprintType)
struct FWeaponStateTransitionRequest
{
    GENERATED_BODY()
    
    UPROPERTY()
    FGameplayTag FromState;
    
    UPROPERTY()
    FGameplayTag ToState;
    
    UPROPERTY()
    int32 WeaponSlotIndex = INDEX_NONE;
    
    UPROPERTY()
    float TransitionDuration = 0.0f;
    
    UPROPERTY()
    bool bForceTransition = false;
};

/**
 * Weapon state transition result
 */
USTRUCT(BlueprintType)
struct FWeaponStateTransitionResult
{
    GENERATED_BODY()
    
    UPROPERTY()
    bool bSuccess = false;
    
    UPROPERTY()
    FText FailureReason;
    
    UPROPERTY()
    FGameplayTag ResultingState;
    
    UPROPERTY()
    float ActualDuration = 0.0f;
};

UINTERFACE(MinimalAPI, Blueprintable)
class UMedComWeaponStateProvider : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for weapon state management
 * 
 * Philosophy: Finite State Machine for weapon states.
 * Manages transitions and validates state changes.
 */
class MEDCOMSHARED_API IMedComWeaponStateProvider
{
    GENERATED_BODY()

public:
    /**
     * Get current weapon state
     * @param SlotIndex Weapon slot (-1 for active)
     * @return Current state tag
     */
    virtual FGameplayTag GetWeaponState(int32 SlotIndex = -1) const = 0;
    
    /**
     * Request state transition
     * @param Request Transition request
     * @return Transition result
     */
    virtual FWeaponStateTransitionResult RequestStateTransition(const FWeaponStateTransitionRequest& Request) = 0;
    
    /**
     * Check if transition is valid
     * @param FromState Starting state
     * @param ToState Target state
     * @return True if transition is allowed
     */
    virtual bool CanTransitionTo(const FGameplayTag& FromState, const FGameplayTag& ToState) const = 0;
    
    /**
     * Get valid transitions from current state
     * @param CurrentState Current state
     * @return Array of valid target states
     */
    virtual TArray<FGameplayTag> GetValidTransitions(const FGameplayTag& CurrentState) const = 0;
    
    /**
     * Force state without transition
     * @param NewState State to force
     * @param SlotIndex Weapon slot
     * @return True if successful
     */
    virtual bool ForceState(const FGameplayTag& NewState, int32 SlotIndex = -1) = 0;
    
    /**
     * Get transition duration
     * @param FromState Starting state
     * @param ToState Target state
     * @return Duration in seconds
     */
    virtual float GetTransitionDuration(const FGameplayTag& FromState, const FGameplayTag& ToState) const = 0;
    
    /**
     * Is weapon currently transitioning
     * @param SlotIndex Weapon slot
     * @return True if in transition
     */
    virtual bool IsTransitioning(int32 SlotIndex = -1) const = 0;
    
    /**
     * Get transition progress
     * @param SlotIndex Weapon slot
     * @return Progress from 0 to 1
     */
    virtual float GetTransitionProgress(int32 SlotIndex = -1) const = 0;
    
    /**
     * Abort current transition
     * @param SlotIndex Weapon slot
     * @return True if aborted
     */
    virtual bool AbortTransition(int32 SlotIndex = -1) = 0;
    
    /**
     * Get state history
     * @param MaxCount Maximum entries
     * @return Array of state tags
     */
    virtual TArray<FGameplayTag> GetStateHistory(int32 MaxCount = 10) const = 0;
};