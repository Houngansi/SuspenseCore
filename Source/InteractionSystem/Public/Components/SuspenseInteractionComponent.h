// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "AbilitySystemComponent.h"
#include "Utils/SuspenseHelpers.h"
#include "SuspenseInteractionComponent.generated.h"

// Delegates for interaction events
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInteractionSucceededDelegate, AActor*, InteractedActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInteractionFailedDelegate, AActor*, AttemptedActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInteractionTypeChangedDelegate, AActor*, InteractableActor, FGameplayTag, InteractionType);

// Forward declarations
class USuspenseEventManager;

/**
 * Component for interacting with objects in the world through GAS
 * Uses tracing to find objects and activates appropriate abilities
 *
 * ARCHITECTURAL IMPROVEMENTS:
 * - Integrated with centralized delegate system
 * - Broadcasts interaction events through delegate manager
 * - Supports focus tracking for enhanced UI feedback
 */
UCLASS(ClassGroup = (Suspense), meta = (BlueprintSpawnableComponent))
class INTERACTIONSYSTEM_API USuspenseInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USuspenseInteractionComponent();

    // Base UActorComponent methods
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Main method to start interaction
    UFUNCTION(BlueprintCallable, Category = "Interaction")
    void StartInteraction();

    // Method to check if interaction is possible
    UFUNCTION(BlueprintCallable, Category = "Interaction")
    bool CanInteractNow() const;

    // Method for UI to display highlights and prompts
    UFUNCTION(BlueprintCallable, Category = "Interaction|UI")
    AActor* PerformUIInteractionTrace() const;

    // Trace settings
    UFUNCTION(BlueprintCallable, Category = "Interaction|Settings")
    void SetTraceDistance(float NewDistance) { TraceDistance = FMath::Max(0.0f, NewDistance); }

    // Get current trace distance
    UFUNCTION(BlueprintCallable, Category = "Interaction|Settings")
    float GetTraceDistance() const { return TraceDistance; }

    // Get current focused interactable
    UFUNCTION(BlueprintCallable, Category = "Interaction|State")
    AActor* GetFocusedInteractable() const { return LastInteractableActor.Get(); }

    // Event delegates
    UPROPERTY(BlueprintAssignable, Category = "Interaction|Events")
    FInteractionSucceededDelegate OnInteractionSucceeded;

    UPROPERTY(BlueprintAssignable, Category = "Interaction|Events")
    FInteractionFailedDelegate OnInteractionFailed;

    UPROPERTY(BlueprintAssignable, Category = "Interaction|Events")
    FInteractionTypeChangedDelegate OnInteractionTypeChanged;

    // Settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Settings")
    float TraceDistance = 300.0f;

    // Use TEnumAsByte<ECollisionChannel> for proper collision handling
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Settings")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Settings")
    bool bEnableDebugTrace = false;

    // Tags for GAS interaction
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Gameplay Tags")
    FGameplayTag InteractAbilityTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Gameplay Tags")
    FGameplayTag InteractSuccessTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Gameplay Tags")
    FGameplayTag InteractFailedTag;

    // Tags that block interaction
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Gameplay Tags")
    FGameplayTagContainer BlockingTags;

protected:
    // Cached AbilitySystemComponent for performance
    UPROPERTY(Transient)
    TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

    // Last detected interactable object (for UI optimization)
    UPROPERTY(Transient)
    TWeakObjectPtr<AActor> LastInteractableActor;

    // Cached delegate manager for performance
    UPROPERTY(Transient)
    TWeakObjectPtr<USuspenseEventManager> CachedDelegateManager;

    // Timer handle for interaction cooldown
    FTimerHandle CooldownTimerHandle;

    // Delay between interaction attempts to prevent spam
    UPROPERTY(EditAnywhere, Category = "Interaction|Settings")
    float InteractionCooldown = 0.5f;

    // Flag indicating interaction is on cooldown
    bool bInteractionOnCooldown = false;

    // Event handlers from interaction abilities
    void HandleInteractionSuccess(const FGameplayEventData& Payload);
    void HandleInteractionFailure(const FGameplayEventData& Payload);

    // Delegate methods with correct signature
    void HandleInteractionSuccessDelegate(const FGameplayEventData* Payload);
    void HandleInteractionFailureDelegate(const FGameplayEventData* Payload);

    // Helper methods
    bool HasBlockingTags() const;
    void LogInteraction(const FString& Message, bool bError = false) const;
    UAbilitySystemComponent* GetOwnerASC() const;
    USuspenseEventManager* GetDelegateManager() const;

    // Set interaction cooldown
    void SetInteractionCooldown();

    // Reset interaction cooldown
    UFUNCTION()
    void ResetInteractionCooldown();

    // Handle focus changes
    void UpdateInteractionFocus(AActor* NewFocusActor);

    // Broadcast interaction events through delegate manager
    void BroadcastInteractionAttempt(AActor* TargetActor);
    void BroadcastInteractionResult(AActor* TargetActor, bool bSuccess);
};
