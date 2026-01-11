// SuspenseCoreAbilityTask_PlayMontageAndWaitForEvent.h
// SuspenseCore - Montage Ability Task with Event Support
// Copyright Suspense Team. All Rights Reserved.
//
// Plays an animation montage and waits for completion or events.
// Provides delegates for various montage states (completed, interrupted, cancelled).
//
// Usage:
//   auto Task = USuspenseCoreAbilityTask_PlayMontageAndWaitForEvent::CreateTask(this, Montage, 1.0f);
//   Task->OnCompleted.AddDynamic(this, &UMyAbility::OnMontageCompleted);
//   Task->ReadyForActivation();

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreAbilityTask_PlayMontageAndWaitForEvent.generated.h"

class UAnimInstance;
struct FAbilityEndedData;

/**
 * Delegate for montage events.
 * @param EventTag Tag associated with the event (if any)
 * @param EventData Additional event data
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FSuspenseCoreMontageEventDelegate,
	FGameplayTag, EventTag,
	FGameplayEventData, EventData
);

/**
 * Delegate for simple montage completion (no event data).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSuspenseCoreMontageSimpleDelegate);

/**
 * USuspenseCoreAbilityTask_PlayMontageAndWaitForEvent
 *
 * Ability task that plays an animation montage and broadcasts events
 * for various montage states. Supports waiting for gameplay events
 * during montage playback.
 *
 * Features:
 * - Plays montage with configurable rate and start section
 * - Broadcasts on completion, blend out, interruption, cancellation
 * - Optionally waits for gameplay events with specific tags
 * - Automatic montage stop on ability end (configurable)
 * - Network-aware (local prediction support)
 *
 * @see UAbilityTask
 */
UCLASS()
class GAS_API USuspenseCoreAbilityTask_PlayMontageAndWaitForEvent : public UAbilityTask
{
	GENERATED_BODY()

public:
	USuspenseCoreAbilityTask_PlayMontageAndWaitForEvent();

	//========================================================================
	// Task Creation
	//========================================================================

	/**
	 * Create and setup the montage task.
	 *
	 * @param OwningAbility Ability that owns this task
	 * @param TaskInstanceName Name for this task instance
	 * @param MontageToPlay Animation montage to play
	 * @param Rate Playback rate (1.0 = normal speed)
	 * @param StartSection Section name to start from (empty = beginning)
	 * @param bStopWhenAbilityEnds Whether to stop montage when ability ends
	 * @param AnimRootMotionTranslationScale Root motion scale
	 * @param StartTimeSeconds Start time offset in seconds
	 * @return Created task instance
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Tasks",
		meta = (DisplayName = "Play Montage And Wait For Event",
			HidePin = "OwningAbility",
			DefaultToSelf = "OwningAbility",
			BlueprintInternalUseOnly = "TRUE"))
	static USuspenseCoreAbilityTask_PlayMontageAndWaitForEvent* CreateTask(
		UGameplayAbility* OwningAbility,
		FName TaskInstanceName,
		UAnimMontage* MontageToPlay,
		float Rate = 1.0f,
		FName StartSection = NAME_None,
		bool bStopWhenAbilityEnds = true,
		float AnimRootMotionTranslationScale = 1.0f,
		float StartTimeSeconds = 0.0f
	);

	/**
	 * Simplified task creation with defaults.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Tasks",
		meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility"))
	static USuspenseCoreAbilityTask_PlayMontageAndWaitForEvent* PlayMontageAndWait(
		UGameplayAbility* OwningAbility,
		UAnimMontage* MontageToPlay,
		float Rate = 1.0f
	);

	//========================================================================
	// Event Configuration
	//========================================================================

	/**
	 * Set event tags to listen for during montage playback.
	 * Events with these tags will trigger EventReceived delegate.
	 *
	 * @param EventTags Tags to listen for
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Tasks")
	void SetEventTagsToListenFor(const FGameplayTagContainer& EventTags);

	//========================================================================
	// Delegates
	//========================================================================

	/** Called when montage plays to completion (reaches end) */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Tasks")
	FSuspenseCoreMontageSimpleDelegate OnCompleted;

	/** Called when montage blend out starts */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Tasks")
	FSuspenseCoreMontageSimpleDelegate OnBlendOut;

	/** Called when montage is interrupted by another montage */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Tasks")
	FSuspenseCoreMontageSimpleDelegate OnInterrupted;

	/** Called when montage is cancelled (ability cancelled) */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Tasks")
	FSuspenseCoreMontageSimpleDelegate OnCancelled;

	/** Called when a gameplay event with matching tag is received */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Tasks")
	FSuspenseCoreMontageEventDelegate EventReceived;

	//========================================================================
	// Task Interface
	//========================================================================

	virtual void Activate() override;
	virtual void ExternalCancel() override;
	virtual FString GetDebugString() const override;

protected:
	virtual void OnDestroy(bool bInOwnerFinished) override;

	//========================================================================
	// Montage Callbacks
	//========================================================================

	/** Called when montage blend out begins */
	UFUNCTION()
	void OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

	/** Called when montage ends */
	UFUNCTION()
	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** Called when ability is cancelled */
	void OnAbilityCancelled(const FAbilityEndedData& AbilityEndedData);

	/** Called when gameplay event is received */
	void OnGameplayEvent(const FGameplayEventData* Payload, FGameplayTag EventTag);

	//========================================================================
	// Internal State
	//========================================================================

	/** Stop the montage if still playing */
	bool StopPlayingMontage();

	/** Get animation instance from avatar */
	UAnimInstance* GetAnimInstanceFromAvatar() const;

private:
	/** Montage to play */
	UPROPERTY()
	TObjectPtr<UAnimMontage> MontageToPlay;

	/** Playback rate */
	UPROPERTY()
	float Rate;

	/** Section to start from */
	UPROPERTY()
	FName StartSection;

	/** Root motion translation scale */
	UPROPERTY()
	float AnimRootMotionTranslationScale;

	/** Start time offset */
	UPROPERTY()
	float StartTimeSeconds;

	/** Whether to stop montage when ability ends */
	UPROPERTY()
	bool bStopWhenAbilityEnds;

	/** Tags to listen for during playback */
	UPROPERTY()
	FGameplayTagContainer EventTagsToListenFor;

	/** Is montage currently playing */
	bool bIsPlayingMontage;

	/** Blend out delegate handle */
	FOnMontageBlendingOutStarted BlendingOutDelegate;

	/** Montage ended delegate handle */
	FOnMontageEnded MontageEndedDelegate;

	/** Ability cancelled delegate handle */
	FDelegateHandle CancelledHandle;

	/** Event handles for gameplay events */
	TArray<FDelegateHandle> EventHandles;
};
