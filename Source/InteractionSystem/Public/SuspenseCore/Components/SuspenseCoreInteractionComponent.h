// SuspenseCoreInteractionComponent.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "AbilitySystemComponent.h"
#include "SuspenseCore/SuspenseCoreInterfaces.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCoreInteractionComponent.generated.h"

// Forward declarations
class USuspenseCoreEventBus;
class USuspenseCoreInteractionSettings;

// Delegates for interaction events (Blueprint compatible)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSuspenseCoreInteractionSucceededDelegate, AActor*, InteractedActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSuspenseCoreInteractionFailedDelegate, AActor*, AttemptedActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSuspenseCoreInteractionTypeChangedDelegate, AActor*, InteractableActor, FGameplayTag, InteractionType);

/**
 * USuspenseCoreInteractionComponent
 *
 * Component for interacting with objects in the world through GAS.
 * Uses tracing to find objects and activates appropriate abilities.
 *
 * EVENTBUS INTEGRATION:
 * - Implements ISuspenseCoreEventSubscriber for event lifecycle
 * - Implements ISuspenseCoreEventEmitter for publishing events
 * - Broadcasts interaction events: Started, Completed, Cancelled
 * - Broadcasts focus events: FocusGained, FocusLost
 * - Subscribes to settings change events for hot-reload
 *
 * ARCHITECTURE:
 * - Lives on Character (traces from camera)
 * - Gets ASC via Character→PlayerState delegation
 * - Uses centralized EventBus instead of direct delegate calls
 *
 * @see USuspenseInteractionComponent (Legacy reference)
 */
UCLASS(ClassGroup = (SuspenseCore), meta = (BlueprintSpawnableComponent, DisplayName = "Suspense Core Interaction"))
class INTERACTIONSYSTEM_API USuspenseCoreInteractionComponent
	: public UActorComponent
	, public ISuspenseCoreEventSubscriber
	, public ISuspenseCoreEventEmitter
{
	GENERATED_BODY()

public:
	USuspenseCoreInteractionComponent();

	//==================================================================
	// UActorComponent Interface
	//==================================================================

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	//==================================================================
	// ISuspenseCoreEventSubscriber Interface
	//==================================================================

	virtual void SetupEventSubscriptions(USuspenseCoreEventBus* EventBus) override;
	virtual void TeardownEventSubscriptions(USuspenseCoreEventBus* EventBus) override;
	virtual TArray<FSuspenseCoreSubscriptionHandle> GetSubscriptionHandles() const override;

	//==================================================================
	// ISuspenseCoreEventEmitter Interface
	//==================================================================

	virtual void EmitEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& Data) override;
	virtual USuspenseCoreEventBus* GetEventBus() const override;

	//==================================================================
	// Main Interaction API
	//==================================================================

	/**
	 * Start interaction with the currently focused object.
	 * Activates interaction ability through GAS.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Interaction")
	void StartInteraction();

	/**
	 * Check if interaction is currently possible.
	 * Validates cooldown, blocking tags, and target availability.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Interaction")
	bool CanInteractNow() const;

	/**
	 * Perform trace for UI purposes (highlight, prompt).
	 * @return Actor implementing ISuspenseInteract or nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Interaction|UI")
	AActor* PerformUIInteractionTrace() const;

	//==================================================================
	// Settings API
	//==================================================================

	/** Set trace distance */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Interaction|Settings")
	void SetTraceDistance(float NewDistance) { TraceDistance = FMath::Max(0.0f, NewDistance); }

	/** Get current trace distance */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Interaction|Settings")
	float GetTraceDistance() const { return TraceDistance; }

	/** Set trace sphere radius */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Interaction|Settings")
	void SetTraceSphereRadius(float NewRadius) { TraceSphereRadius = FMath::Max(0.0f, NewRadius); }

	/** Get trace sphere radius */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Interaction|Settings")
	float GetTraceSphereRadius() const { return TraceSphereRadius; }

	//==================================================================
	// State API
	//==================================================================

	/** Get current focused interactable */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Interaction|State")
	AActor* GetFocusedInteractable() const { return LastInteractableActor.Get(); }

	/** Check if currently on cooldown */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Interaction|State")
	bool IsOnCooldown() const { return bInteractionOnCooldown; }

	//==================================================================
	// Blueprint Events
	//==================================================================

	/** Successful interaction */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Interaction|Events")
	FSuspenseCoreInteractionSucceededDelegate OnInteractionSucceeded;

	/** Failed interaction */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Interaction|Events")
	FSuspenseCoreInteractionFailedDelegate OnInteractionFailed;

	/** Interaction type changed (for UI) */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Interaction|Events")
	FSuspenseCoreInteractionTypeChangedDelegate OnInteractionTypeChanged;

	//==================================================================
	// Settings
	//==================================================================

	/** Trace distance for interaction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Interaction|Settings")
	float TraceDistance;

	/** Trace sphere radius (0 = line trace) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Interaction|Settings")
	float TraceSphereRadius;

	/** Collision channel for trace */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Interaction|Settings")
	TEnumAsByte<ECollisionChannel> TraceChannel;

	/** Enable debug visualization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Interaction|Debug")
	bool bEnableDebugTrace;

	/** Cooldown between interaction attempts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Interaction|Settings")
	float InteractionCooldown;

	//==================================================================
	// GameplayTags
	//==================================================================

	/** Tag for interaction ability activation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Interaction|GameplayTags")
	FGameplayTag InteractAbilityTag;

	/** Tag for successful interaction event */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Interaction|GameplayTags")
	FGameplayTag InteractSuccessTag;

	/** Tag for failed interaction event */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Interaction|GameplayTags")
	FGameplayTag InteractFailedTag;

	/** Tags that block interaction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Interaction|GameplayTags")
	FGameplayTagContainer BlockingTags;

protected:
	//==================================================================
	// Cached References
	//==================================================================

	/** Cached AbilitySystemComponent */
	UPROPERTY(Transient)
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

	/** Last detected interactable object */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> LastInteractableActor;

	/** Cached EventBus reference */
	UPROPERTY(Transient)
	mutable TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	/** Active subscription handles */
	TArray<FSuspenseCoreSubscriptionHandle> SubscriptionHandles;

	//==================================================================
	// State
	//==================================================================

	/** Timer handle for cooldown */
	FTimerHandle CooldownTimerHandle;

	/** Interaction cooldown flag */
	bool bInteractionOnCooldown;

	/** Time accumulator for focus update rate limiting */
	float FocusUpdateAccumulator;

	//==================================================================
	// Event Handlers
	//==================================================================

	/** Handle GAS success event */
	void HandleInteractionSuccess(const FGameplayEventData& Payload);

	/** Handle GAS failure event */
	void HandleInteractionFailure(const FGameplayEventData& Payload);

	/** Delegate wrappers for GAS callbacks */
	void HandleInteractionSuccessDelegate(const FGameplayEventData* Payload);
	void HandleInteractionFailureDelegate(const FGameplayEventData* Payload);

	/** Handle settings change event from EventBus */
	void HandleSettingsChanged(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	//==================================================================
	// Helper Methods
	//==================================================================

	/** Check for blocking tags on owner's ASC */
	bool HasBlockingTags() const;

	/** Log with component context */
	void LogInteraction(const FString& Message, bool bError = false) const;

	/** Get owner's AbilitySystemComponent */
	UAbilitySystemComponent* GetOwnerASC() const;

	/** Set interaction cooldown */
	void SetInteractionCooldown();

	/** Reset interaction cooldown */
	UFUNCTION()
	void ResetInteractionCooldown();

	/** Update focus and notify listeners */
	void UpdateInteractionFocus(AActor* NewFocusActor);

	/** Apply settings from DeveloperSettings */
	void ApplySettings(const USuspenseCoreInteractionSettings* Settings);

	//==================================================================
	// EventBus Broadcasting
	//==================================================================

	/** Broadcast interaction attempt event */
	void BroadcastInteractionAttempt(AActor* TargetActor);

	/** Broadcast interaction result event */
	void BroadcastInteractionResult(AActor* TargetActor, bool bSuccess);

	/** Broadcast focus change event */
	void BroadcastFocusChanged(AActor* FocusedActor, bool bGained);
};
