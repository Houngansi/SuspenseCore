// ISuspenseCoreEventDispatcher.h
// SuspenseCore Event Dispatcher Interface
// Copyright SuspenseCore Team. All Rights Reserved.
//
// This interface defines the contract for event dispatching using
// the SuspenseCore EventBus architecture (NOT legacy FSuspenseEquipmentEventBus).
//
// Migration from legacy:
// - FSuspenseEquipmentEventBus -> USuspenseCoreEventBus
// - FSuspenseEquipmentEventData -> FSuspenseCoreEventData
// - FEventSubscriptionHandle -> FSuspenseCoreSubscriptionHandle

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "ISuspenseCoreEventDispatcher.generated.h"

// Forward declarations
class USuspenseCoreEventBus;

/**
 * Delegate for SuspenseCore event handling
 */
DECLARE_DELEGATE_TwoParams(FSuspenseCoreEventDelegate, FGameplayTag /*EventTag*/, const FSuspenseCoreEventData& /*EventData*/);

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreEventDispatcher : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISuspenseCoreEventDispatcher
 *
 * Interface for components that dispatch events through USuspenseCoreEventBus.
 * This is the NEW architecture - replaces legacy ISuspenseEventDispatcher.
 *
 * Key differences from legacy:
 * - Uses USuspenseCoreEventBus (UObject-based) instead of FSuspenseEquipmentEventBus (TSharedPtr singleton)
 * - Uses FSuspenseCoreEventData with flexible TMap payload instead of FSuspenseEquipmentEventData
 * - Uses FSuspenseCoreSubscriptionHandle instead of FEventSubscriptionHandle
 * - Event tags follow SuspenseCore.Event.* format (per BestPractices.md)
 *
 * Usage:
 *   // Get EventBus from ServiceProvider
 *   USuspenseCoreEventBus* EventBus = USuspenseCoreServiceProvider::Get(this)->GetEventBus();
 *
 *   // Subscribe to events
 *   FSuspenseCoreSubscriptionHandle Handle = EventBus->SubscribeNative(
 *       FGameplayTag::RequestGameplayTag("SuspenseCore.Event.Equipment.Equipped"),
 *       this,
 *       FSuspenseCoreNativeEventCallback::CreateUObject(this, &ThisClass::OnEquipped)
 *   );
 *
 *   // Publish events
 *   FSuspenseCoreEventData Data = FSuspenseCoreEventData::Create(this);
 *   Data.SetInt(FName("SlotIndex"), SlotIndex);
 *   EventBus->Publish(SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Equipped, Data);
 */
class BRIDGESYSTEM_API ISuspenseCoreEventDispatcher
{
	GENERATED_BODY()

public:
	/**
	 * Get the EventBus instance used by this dispatcher
	 * @return The EventBus instance (never null for valid dispatchers)
	 */
	virtual USuspenseCoreEventBus* GetEventBus() const = 0;

	/**
	 * Subscribe to an event
	 * @param EventTag Event to subscribe to (must follow SuspenseCore.Event.* format)
	 * @param Subscriber Object subscribing (used for automatic cleanup)
	 * @param Callback Native callback function
	 * @param Priority Event processing priority
	 * @return Subscription handle for unsubscribing
	 */
	virtual FSuspenseCoreSubscriptionHandle Subscribe(
		const FGameplayTag& EventTag,
		UObject* Subscriber,
		FSuspenseCoreNativeEventCallback Callback,
		ESuspenseCoreEventPriority Priority = ESuspenseCoreEventPriority::Normal
	) = 0;

	/**
	 * Unsubscribe from an event
	 * @param Handle Subscription handle
	 * @return True if successfully unsubscribed
	 */
	virtual bool Unsubscribe(const FSuspenseCoreSubscriptionHandle& Handle) = 0;

	/**
	 * Unsubscribe all subscriptions for an object
	 * @param Subscriber Object to unsubscribe
	 * @return Number of subscriptions removed
	 */
	virtual int32 UnsubscribeAll(UObject* Subscriber) = 0;

	/**
	 * Publish an event immediately
	 * @param EventTag Event tag (must follow SuspenseCore.Event.* format)
	 * @param EventData Event data
	 */
	virtual void Publish(const FGameplayTag& EventTag, const FSuspenseCoreEventData& EventData) = 0;

	/**
	 * Publish an event deferred (end of frame)
	 * @param EventTag Event tag
	 * @param EventData Event data
	 */
	virtual void PublishDeferred(const FGameplayTag& EventTag, const FSuspenseCoreEventData& EventData) = 0;

	/**
	 * Check if there are any subscribers for an event
	 * @param EventTag Event to check
	 * @return True if there are subscribers
	 */
	virtual bool HasSubscribers(const FGameplayTag& EventTag) const = 0;

	/**
	 * Get dispatcher statistics
	 * @return Statistics as string
	 */
	virtual FString GetStatistics() const = 0;
};

//========================================
// HELPER MACROS for SuspenseCore EventBus
//========================================

/**
 * Publish a simple event with source only
 */
#define SUSPENSECORE_PUBLISH_EVENT(EventBus, EventTag, Source) \
	if (EventBus) { \
		FSuspenseCoreEventData _Data = FSuspenseCoreEventData::Create(Source); \
		EventBus->Publish(EventTag, _Data); \
	}

/**
 * Publish event with int payload
 */
#define SUSPENSECORE_PUBLISH_EVENT_INT(EventBus, EventTag, Source, Key, Value) \
	if (EventBus) { \
		FSuspenseCoreEventData _Data = FSuspenseCoreEventData::Create(Source); \
		_Data.SetInt(FName(Key), Value); \
		EventBus->Publish(EventTag, _Data); \
	}

/**
 * Publish event with string payload
 */
#define SUSPENSECORE_PUBLISH_EVENT_STRING(EventBus, EventTag, Source, Key, Value) \
	if (EventBus) { \
		FSuspenseCoreEventData _Data = FSuspenseCoreEventData::Create(Source); \
		_Data.SetString(FName(Key), Value); \
		EventBus->Publish(EventTag, _Data); \
	}

