// SuspenseCorePlayerComponent.h
// Copyright Suspense Team. All Rights Reserved.
// Clean Architecture Player Component for modular player functionality

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SuspenseCorePlayerComponent.generated.h"

class USuspenseCoreEventBus;
class UAbilitySystemComponent;

// ═══════════════════════════════════════════════════════════════════════════════
// EVENT SUBSCRIPTION HANDLE
// ═══════════════════════════════════════════════════════════════════════════════

USTRUCT()
struct FSuspenseCoreEventSubscription
{
	GENERATED_BODY()

	FGameplayTag EventTag;
	FDelegateHandle Handle;
};

// ═══════════════════════════════════════════════════════════════════════════════
// PLAYER COMPONENT
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Modular component for adding SuspenseCore player functionality to any actor
 *
 * DESIGN PRINCIPLES:
 * - Composable - can be added to any actor
 * - EventBus integration for communication
 * - Manages subscriptions lifecycle
 *
 * USE CASES:
 * - Add player behavior to non-standard actors
 * - Bridge between legacy systems and EventBus
 * - Component-based architecture support
 */
UCLASS(ClassGroup = (SuspenseCore), meta = (BlueprintSpawnableComponent))
class PLAYERCORE_API USuspenseCorePlayerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USuspenseCorePlayerComponent();

	// ═══════════════════════════════════════════════════════════════════════════════
	// COMPONENT LIFECYCLE
	// ═══════════════════════════════════════════════════════════════════════════════

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - EVENTS
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Publish an event to EventBus */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
	void PublishEvent(const FGameplayTag& EventTag, const FString& Payload = TEXT(""));

	/** Subscribe to an event */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
	bool SubscribeToEvent(const FGameplayTag& EventTag);

	/** Unsubscribe from an event */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
	void UnsubscribeFromEvent(const FGameplayTag& EventTag);

	/** Unsubscribe from all events */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
	void UnsubscribeFromAllEvents();

	// ═══════════════════════════════════════════════════════════════════════════════
	// PUBLIC API - STATE
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Get player identifier */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|State")
	FString GetPlayerIdentifier() const { return PlayerIdentifier; }

	/** Set player identifier */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|State")
	void SetPlayerIdentifier(const FString& NewIdentifier);

	/** Check if component is ready */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|State")
	bool IsReady() const { return bIsReady; }

	/** Get ASC if owner has one */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|GAS")
	UAbilitySystemComponent* GetOwnerASC() const;

	// ═══════════════════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Called when an event is received */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Events", meta = (DisplayName = "On Event Received"))
	void K2_OnEventReceived(const FGameplayTag& EventTag, const FString& Payload, UObject* Source);

	/** Called when component is ready */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Lifecycle", meta = (DisplayName = "On Ready"))
	void K2_OnReady();

protected:
	// ═══════════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Events to automatically subscribe to on BeginPlay */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	TArray<FGameplayTag> AutoSubscribeEvents;

	/** Whether to auto-generate player identifier */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	bool bAutoGenerateIdentifier = true;

	// ═══════════════════════════════════════════════════════════════════════════════
	// OVERRIDABLE HANDLERS
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Native handler for received events - override in C++ subclasses */
	virtual void OnEventReceived(const FGameplayTag& EventTag, const FString& Payload, UObject* Source);

	/** Native handler for ready state - override in C++ subclasses */
	virtual void OnReady();

private:
	// ═══════════════════════════════════════════════════════════════════════════════
	// STATE
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Player identifier for this component */
	UPROPERTY()
	FString PlayerIdentifier;

	/** Component ready flag */
	bool bIsReady = false;

	/** Active event subscriptions */
	TArray<FSuspenseCoreEventSubscription> ActiveSubscriptions;

	/** Cached EventBus reference */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	// ═══════════════════════════════════════════════════════════════════════════════
	// INTERNAL
	// ═══════════════════════════════════════════════════════════════════════════════

	void Initialize();
	void SetupAutoSubscriptions();
	void CleanupSubscriptions();

	USuspenseCoreEventBus* GetEventBus() const;

	void HandleEventReceived(const UObject* Source, const FGameplayTag& EventTag, const FString& Payload);
};
