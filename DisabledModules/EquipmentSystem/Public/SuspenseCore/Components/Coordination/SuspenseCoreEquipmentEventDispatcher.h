// SuspenseCoreEquipmentEventDispatcher.h
// Copyright SuspenseCore Team. All Rights Reserved.
//
// Equipment Event Dispatcher - Migrated to SuspenseCore EventBus architecture.
// Uses USuspenseCoreEventBus instead of legacy FSuspenseCoreEquipmentEventBus.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEventDispatcher.h"
#include "SuspenseCoreEquipmentEventDispatcher.generated.h"

// Forward declarations
class USuspenseCoreEventBus;
class USuspenseCoreServiceProvider;

/**
 * Internal local subscription record for dispatcher
 * Uses SuspenseCore types instead of legacy
 */
struct FSuspenseCoreDispatcherSubscription
{
	FSuspenseCoreSubscriptionHandle Handle;
	FSuspenseCoreNativeEventCallback Callback;
	TWeakObjectPtr<UObject> Subscriber;
	ESuspenseCoreEventPriority Priority = ESuspenseCoreEventPriority::Normal;
	bool bActive = true;
	int32 DispatchCount = 0;
	float SubscribedAt = 0.f;
};

/**
 * Dispatcher statistics - SuspenseCore version
 */
USTRUCT(BlueprintType)
struct FSuspenseCoreEventDispatcherStats
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	int32 TotalEventsDispatched = 0;

	UPROPERTY(VisibleAnywhere)
	int32 TotalEventsQueued = 0;

	UPROPERTY(VisibleAnywhere)
	int32 ActiveLocalSubscriptions = 0;

	UPROPERTY(VisibleAnywhere)
	int32 RegisteredEventTypes = 0;

	UPROPERTY(VisibleAnywhere)
	int32 CurrentQueueSize = 0;

	UPROPERTY(VisibleAnywhere)
	float AverageDispatchMs = 0.f;

	UPROPERTY(VisibleAnywhere)
	float PeakQueueSize = 0.f;
};

/**
 * USuspenseCoreEquipmentEventDispatcher
 *
 * Equipment event dispatcher component using SuspenseCore EventBus architecture.
 * This is the NEW implementation - uses USuspenseCoreEventBus instead of FSuspenseCoreEquipmentEventBus.
 *
 * Key features:
 * - Integrates with USuspenseCoreEventBus via ServiceProvider
 * - Uses FSuspenseCoreEventData for all event payloads
 * - Uses FSuspenseCoreSubscriptionHandle for subscription management
 * - Event tags follow SuspenseCore.Event.Equipment.* format (per BestPractices.md)
 * - Batch mode support for high-frequency events
 * - Local subscription layer for component-specific filtering
 *
 * Usage:
 *   // In your component
 *   USuspenseCoreEquipmentEventDispatcher* Dispatcher = GetOwner()->FindComponentByClass<USuspenseCoreEquipmentEventDispatcher>();
 *   if (Dispatcher)
 *   {
 *       // Subscribe to equipment events
 *       auto Handle = Dispatcher->Subscribe(
 *           SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Equipped,
 *           this,
 *           FSuspenseCoreNativeEventCallback::CreateUObject(this, &ThisClass::OnEquipped)
 *       );
 *
 *       // Publish events
 *       FSuspenseCoreEventData Data = FSuspenseCoreEventData::Create(this);
 *       Data.SetInt(FName("SlotIndex"), 0);
 *       Dispatcher->Publish(SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Equipped, Data);
 *   }
 */
UCLASS(ClassGroup=(SuspenseCore), meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentEventDispatcher : public UActorComponent,
	public ISuspenseCoreEventDispatcher
{
	GENERATED_BODY()

public:
	USuspenseCoreEquipmentEventDispatcher();

	//========================================
	// UActorComponent Interface
	//========================================
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	//========================================
	// ISuspenseCoreEventDispatcher Interface
	//========================================
	virtual USuspenseCoreEventBus* GetEventBus() const override;

	virtual FSuspenseCoreSubscriptionHandle Subscribe(
		const FGameplayTag& EventTag,
		UObject* Subscriber,
		FSuspenseCoreNativeEventCallback Callback,
		ESuspenseCoreEventPriority Priority = ESuspenseCoreEventPriority::Normal
	) override;

	virtual bool Unsubscribe(const FSuspenseCoreSubscriptionHandle& Handle) override;
	virtual int32 UnsubscribeAll(UObject* Subscriber) override;

	virtual void Publish(const FGameplayTag& EventTag, const FSuspenseCoreEventData& EventData) override;
	virtual void PublishDeferred(const FGameplayTag& EventTag, const FSuspenseCoreEventData& EventData) override;

	virtual bool HasSubscribers(const FGameplayTag& EventTag) const override;
	virtual FString GetStatistics() const override;

	//========================================
	// Extended API
	//========================================

	/**
	 * Enable/disable batch mode for high-frequency events
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
	void SetBatchModeEnabled(bool bEnabled, float FlushIntervalSec = 0.02f, int32 MaxPerTick = 256);

	/**
	 * Flush all batched events immediately
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
	void FlushBatched();

	/**
	 * Get dispatcher statistics
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
	FSuspenseCoreEventDispatcherStats GetStats() const;

	/**
	 * Enable detailed logging
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
	void SetDetailedLogging(bool bEnable);

private:
	//========================================
	// EventBus Connection
	//========================================

	/** Cached EventBus reference from ServiceProvider */
	UPROPERTY(Transient)
	TObjectPtr<USuspenseCoreEventBus> EventBus = nullptr;

	/** Subscription handles for EventBus events */
	TArray<FSuspenseCoreSubscriptionHandle> BusSubscriptions;

	//========================================
	// Local Subscription Management
	//========================================

	/** Local subscriptions by event tag */
	TMap<FGameplayTag, TArray<FSuspenseCoreDispatcherSubscription>> LocalSubscriptions;

	/** Handle to tag mapping for quick unsubscribe */
	TMap<uint64, FGameplayTag> HandleToTag;

	/** Next subscription ID */
	uint64 NextSubscriptionId = 1;

	//========================================
	// Batch Mode Configuration
	//========================================

	/** Enable batch mode for event processing */
	bool bBatchMode = true;

	/** Flush interval in seconds */
	float FlushInterval = 0.02f;

	/** Maximum events to process per tick */
	int32 MaxPerTick = 256;

	/** Time accumulator for flush interval */
	float Accumulator = 0.f;

	//========================================
	// Event Queue
	//========================================

	/** Queued events structure */
	struct FQueuedEvent
	{
		FGameplayTag EventTag;
		FSuspenseCoreEventData EventData;
	};

	/** Local event queue for batch processing */
	TArray<FQueuedEvent> LocalQueue;

	/** Critical section for thread-safe queue access */
	mutable FCriticalSection QueueCs;

	//========================================
	// Statistics
	//========================================

	/** Dispatcher statistics */
	FSuspenseCoreEventDispatcherStats Stats;

	/** Enable verbose logging */
	bool bVerbose = false;

	//========================================
	// Event Tags (using new SuspenseCore.Event.* format)
	//========================================

	/** Delta event tag */
	UPROPERTY(EditDefaultsOnly, Category = "Tags")
	FGameplayTag TagDelta;

	/** Batch delta event tag */
	UPROPERTY(EditDefaultsOnly, Category = "Tags")
	FGameplayTag TagBatchDelta;

	/** Operation completed event tag */
	UPROPERTY(EditDefaultsOnly, Category = "Tags")
	FGameplayTag TagOperationCompleted;

	//========================================
	// Internal Methods
	//========================================

	/** Connect to EventBus */
	void ConnectToEventBus();

	/** Disconnect from EventBus */
	void DisconnectFromEventBus();

	/** Handle event from EventBus */
	void OnBusEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Enqueue event for batch processing */
	void EnqueueEvent(const FGameplayTag& EventTag, const FSuspenseCoreEventData& EventData);

	/** Dispatch event to local subscribers */
	void DispatchEvent(const FGameplayTag& EventTag, const FSuspenseCoreEventData& EventData);

	/** Dispatch to local subscriptions */
	void DispatchToLocal(const FGameplayTag& EventTag, const FSuspenseCoreEventData& EventData);

	/** Sort subscriptions by priority */
	static void SortByPriority(TArray<FSuspenseCoreDispatcherSubscription>& Arr);

	/** Cleanup invalid subscriptions */
	int32 CleanupInvalid();

	/** Generate unique subscription handle */
	FSuspenseCoreSubscriptionHandle GenerateHandle();
};
