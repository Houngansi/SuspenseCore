// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "Core/Utils/SuspenseEquipmentEventBus.h"
#include "ISuspenseEventDispatcher.generated.h"

/**
 * Equipment event data
 */
USTRUCT(BlueprintType)
struct FDispatcherEquipmentEventData
{
    GENERATED_BODY()

    UPROPERTY()
    FGameplayTag EventType;

    UPROPERTY()
    UObject* Source = nullptr;

    UPROPERTY()
    FString EventPayload;

    UPROPERTY()
    float Timestamp = 0.0f;

    UPROPERTY()
    int32 Priority = 0;

    UPROPERTY()
    TMap<FString, FString> Metadata;
};

DECLARE_DELEGATE_OneParam(FEquipmentEventDelegate, const FDispatcherEquipmentEventData&);

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseEventDispatcher : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for event dispatching
 *
 * Philosophy: Central event bus for loose coupling.
 * Components communicate through events without direct dependencies.
 */
class BRIDGESYSTEM_API ISuspenseEventDispatcher
{
    GENERATED_BODY()

public:
    /**
     * Subscribe to event
     * @param EventType Type of event
     * @param Delegate Callback delegate
     * @return Subscription handle
     */
    virtual FDelegateHandle Subscribe(const FGameplayTag& EventType, const FEquipmentEventDelegate& Delegate) = 0;

    /**
     * Unsubscribe from event
     * @param EventType Type of event
     * @param Handle Subscription handle
     * @return True if unsubscribed
     */
    virtual bool Unsubscribe(const FGameplayTag& EventType, const FDelegateHandle& Handle) = 0;

    /**
     * Broadcast event immediately
     * @param Event Event to broadcast
     */
    virtual void BroadcastEvent(const FSuspenseEquipmentEventData& Event) = 0;

    /**
     * Queue event for later processing
     * @param Event Event to queue
     */
    virtual void QueueEvent(const FSuspenseEquipmentEventData& Event) = 0;

    /**
     * Process queued events
     * @param MaxEvents Maximum events to process
     * @return Number processed
     */
    virtual int32 ProcessEventQueue(int32 MaxEvents = -1) = 0;

    /**
     * Clear event queue
     * @param EventType Specific type or all
     */
    virtual void ClearEventQueue(const FGameplayTag& EventType = FGameplayTag()) = 0;

    /**
     * Get queued event count
     * @param EventType Specific type or all
     * @return Number of queued events
     */
    virtual int32 GetQueuedEventCount(const FGameplayTag& EventType = FGameplayTag()) const = 0;

    /**
     * Set event filter
     * @param EventType Type to filter
     * @param bAllow Allow or block
     */
    virtual void SetEventFilter(const FGameplayTag& EventType, bool bAllow) = 0;

    /**
     * Get event statistics
     * @return Statistics string
     */
    virtual FString GetEventStatistics() const = 0;

    /**
     * Register event type
     * @param EventType Type to register
     * @param Description Event description
     * @return True if registered
     */
    virtual bool RegisterEventType(const FGameplayTag& EventType, const FText& Description) = 0;
};
