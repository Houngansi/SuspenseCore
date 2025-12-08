// SuspenseCoreAbility.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreAbility.generated.h"

class USuspenseCoreEventBus;
struct FSuspenseCoreEventData;

/**
 * USuspenseCoreAbility
 *
 * Base class for all SuspenseCore abilities with EventBus integration.
 * Extends UGameplayAbility with event publishing capabilities.
 *
 * ARCHITECTURE:
 * - All ability events are published through EventBus
 * - Uses GameplayTags for event identification
 * - Integrated with SuspenseCoreEventManager
 *
 * EVENT TAGS:
 * - SuspenseCore.Event.Ability.Activated
 * - SuspenseCore.Event.Ability.Ended
 * - SuspenseCore.Event.Ability.Cancelled
 * - SuspenseCore.Event.Ability.Failed
 *
 * @see UGameplayAbility (base class)
 * @see USuspenseCoreEventBus
 */
UCLASS(Abstract, Blueprintable)
class GAS_API USuspenseCoreAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	USuspenseCoreAbility();

	//==================================================================
	// EventBus Configuration
	//==================================================================

	/** Whether to publish ability lifecycle events through EventBus */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Events")
	bool bPublishAbilityEvents;

	/** Custom event tag for this ability (optional, defaults to class name based) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Events")
	FGameplayTag AbilityEventTag;

protected:
	//==================================================================
	// GameplayAbility Overrides
	//==================================================================

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

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
	// EventBus Helpers
	//==================================================================

	/**
	 * Get cached EventBus reference.
	 * @return EventBus or nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
	USuspenseCoreEventBus* GetEventBus() const;

	/**
	 * Publish event through EventBus.
	 * @param EventTag Event identifier
	 * @param EventData Event payload
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
	void PublishEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/**
	 * Publish simple event with just the source.
	 * @param EventTag Event identifier
	 */
	void PublishSimpleEvent(FGameplayTag EventTag);

	/**
	 * Broadcast ability activated event.
	 */
	virtual void BroadcastAbilityActivated();

	/**
	 * Broadcast ability ended event.
	 * @param bWasCancelled Whether ability was cancelled
	 */
	virtual void BroadcastAbilityEnded(bool bWasCancelled);

	//==================================================================
	// Utility Helpers
	//==================================================================

	/**
	 * Get owning character for movement operations.
	 * @return Character or nullptr
	 */
	ACharacter* GetOwningCharacter() const;

	/**
	 * Get ability specific tag (combining base with ability identifier).
	 * @param Suffix Tag suffix (e.g., "Activated", "Ended")
	 * @return Combined gameplay tag
	 */
	FGameplayTag GetAbilitySpecificTag(const FString& Suffix) const;

	/**
	 * Log ability debug message.
	 * @param Message Log message
	 * @param bError Whether this is an error
	 */
	void LogAbilityDebug(const FString& Message, bool bError = false) const;

private:
	/** Cached EventBus reference */
	mutable TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;
};
