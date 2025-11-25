// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GASAbility.h"
#include "GameplayTagContainer.h"
#include "InteractAbility.generated.h"

class UEventDelegateManager;

/**
 * Enhanced interact ability with delegate integration
 * Provides safe client-server interaction with objects
 */
UCLASS(meta=(ReplicationPolicy=ReplicateYes))
class GAS_API UInteractAbility : public UGASAbility
{
    GENERATED_BODY()

public:
    UInteractAbility();

    /** Interaction distance (scalable by level) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interact")
    FScalableFloat InteractDistance;

    /** Cooldown duration (scalable by level) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cooldown")
    FScalableFloat CooldownDuration;

    /** Tags that block interaction */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Block")
    FGameplayTagContainer BlockTags;

    /** Cooldown tags */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cooldown")
    FGameplayTagContainer CooldownTags;

    /** Trace channel for interaction */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interact")
    TEnumAsByte<ECollisionChannel> TraceChannel;

    /** Multiple trace channels for broader compatibility */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interact")
    TArray<TEnumAsByte<ECollisionChannel>> AdditionalTraceChannels;

    /** Show debug visualization */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debug")
    bool bShowDebugTrace = false;

    /** Debug trace duration */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debug")
    float DebugTraceDuration = 2.0f;

protected:
    virtual bool CanActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags,
        const FGameplayTagContainer* TargetTags,
        FGameplayTagContainer* OptionalRelevantTags) const override;

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility,
        bool bWasCancelled) override;

    /** Server RPC for interaction validation */
    UFUNCTION(Server, Reliable)
    void ServerPerformInteraction(AActor* TargetActor);
    
    /** Client RPC for interaction feedback */
    UFUNCTION(Client, Reliable)
    void ClientInteractionResult(bool bSuccess, AActor* TargetActor);

    /** Perform interaction trace */
    AActor* PerformInteractionTrace(const FGameplayAbilityActorInfo* ActorInfo) const;

    /** Apply cooldown */
    void ApplyCooldown(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo) const;
        
    /** Send interaction events */
    void SendInteractionEvent(
        const FGameplayAbilityActorInfo* ActorInfo, 
        bool bSuccess, 
        AActor* TargetActor) const;

    /** Notify delegate manager about interaction */
    void NotifyInteraction(bool bSuccess, AActor* TargetActor) const;

    /** Get delegate manager */
    UEventDelegateManager* GetDelegateManager() const;

    /** Debug logging helper */
    void LogInteractionDebugInfo(const FString& Message, bool bError = false) const;

    /** Draw debug visualization */
    void DrawDebugInteraction(
        const FVector& Start, 
        const FVector& End, 
        bool bHit, 
        const TArray<FHitResult>& Hits) const;

private:
    /** Interaction tags */
    FGameplayTag InteractInputTag;
    FGameplayTag InteractSuccessTag;
    FGameplayTag InteractFailedTag;
    FGameplayTag InteractCooldownTag;
    FGameplayTag InteractingTag;

    /** Track prediction for proper networking */
    mutable FPredictionKey CurrentPredictionKey;
};