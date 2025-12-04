// SuspenseCoreInteractAbility.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Abilities/Base/SuspenseCoreAbility.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreInteractAbility.generated.h"

/**
 * USuspenseCoreInteractAbility
 *
 * Interaction ability with EventBus integration and ISuspenseCoreInteractable support.
 * Performs ray/sphere trace to find interactable objects and executes interaction.
 *
 * FEATURES:
 * - Line/sphere trace for interactable detection
 * - ISuspenseCoreInteractable interface integration
 * - Client-server RPC for network safety
 * - EventBus events for interaction lifecycle
 * - Cooldown support via GameplayTags
 *
 * EVENT TAGS:
 * - SuspenseCore.Event.Ability.Interact.Activated
 * - SuspenseCore.Event.Ability.Interact.Success
 * - SuspenseCore.Event.Ability.Interact.Failed
 *
 * @see USuspenseCoreAbility
 * @see ISuspenseCoreInteractable
 */
UCLASS(meta = (ReplicationPolicy = ReplicateYes))
class GAS_API USuspenseCoreInteractAbility : public USuspenseCoreAbility
{
	GENERATED_BODY()

public:
	USuspenseCoreInteractAbility();

	//==================================================================
	// Interaction Configuration
	//==================================================================

	/** Maximum interaction distance */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Interact")
	FScalableFloat InteractDistance;

	/** Sphere trace radius (0 for line trace) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Interact",
		meta = (ClampMin = "0.0"))
	float TraceSphereRadius;

	/** Trace channel for interaction detection */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Interact")
	TEnumAsByte<ECollisionChannel> TraceChannel;

	/** Additional trace channels for broader compatibility */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Interact")
	TArray<TEnumAsByte<ECollisionChannel>> AdditionalTraceChannels;

	/** Cooldown duration */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Interact|Cooldown")
	FScalableFloat CooldownDuration;

	/** Tags applied during cooldown */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Interact|Cooldown")
	FGameplayTagContainer CooldownTags;

	/** Show debug visualization */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Interact|Debug")
	bool bShowDebugTrace;

	/** Debug trace duration */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Interact|Debug",
		meta = (ClampMin = "0.0"))
	float DebugTraceDuration;

protected:
	//==================================================================
	// GameplayAbility Interface
	//==================================================================

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr
	) const override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled
	) override;

	//==================================================================
	// Network RPCs
	//==================================================================

	/** Server RPC for interaction validation */
	UFUNCTION(Server, Reliable)
	void ServerPerformInteraction(AActor* TargetActor);

	/** Client RPC for interaction feedback */
	UFUNCTION(Client, Reliable)
	void ClientInteractionResult(bool bSuccess, AActor* TargetActor);

	//==================================================================
	// Interaction Logic
	//==================================================================

	/**
	 * Perform interaction trace to find target.
	 * @param ActorInfo Actor performing the trace
	 * @return Target actor or nullptr
	 */
	AActor* PerformInteractionTrace(const FGameplayAbilityActorInfo* ActorInfo) const;

	/**
	 * Execute interaction with target.
	 * @param TargetActor Actor to interact with
	 * @return true if interaction succeeded
	 */
	bool ExecuteInteraction(AActor* TargetActor);

	/**
	 * Apply cooldown after interaction.
	 */
	void ApplyCooldown(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo
	) const;

	//==================================================================
	// EventBus Broadcasting
	//==================================================================

	/**
	 * Broadcast interaction success event.
	 * @param TargetActor Actor that was interacted with
	 */
	void BroadcastInteractionSuccess(AActor* TargetActor);

	/**
	 * Broadcast interaction failed event.
	 * @param TargetActor Actor interaction failed with (may be nullptr)
	 * @param Reason Failure reason
	 */
	void BroadcastInteractionFailed(AActor* TargetActor, const FString& Reason);

	//==================================================================
	// Debug Helpers
	//==================================================================

	/** Draw debug visualization */
	void DrawDebugInteraction(
		const FVector& Start,
		const FVector& End,
		bool bHit,
		const TArray<FHitResult>& Hits
	) const;

private:
	//==================================================================
	// Internal State
	//==================================================================

	/** Interaction tags */
	FGameplayTag InteractInputTag;
	FGameplayTag InteractSuccessTag;
	FGameplayTag InteractFailedTag;
	FGameplayTag InteractCooldownTag;
	FGameplayTag InteractingTag;

	/** Track prediction for networking */
	mutable FPredictionKey CurrentPredictionKey;
};
