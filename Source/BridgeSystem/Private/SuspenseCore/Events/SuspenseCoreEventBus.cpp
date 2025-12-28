// SuspenseCoreEventBus.cpp
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Events/SuspenseCoreEventBus.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreEventBus, Log, All);

USuspenseCoreEventBus::USuspenseCoreEventBus()
{
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLISHING
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreEventBus::Publish(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	if (!EventTag.IsValid())
	{
		UE_LOG(LogSuspenseCoreEventBus, Warning, TEXT("Publish: Invalid EventTag"));
		return;
	}

	PublishInternal(EventTag, EventData);
}

void USuspenseCoreEventBus::PublishDeferred(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	if (!EventTag.IsValid())
	{
		UE_LOG(LogSuspenseCoreEventBus, Warning, TEXT("PublishDeferred: Invalid EventTag"));
		return;
	}

	FScopeLock Lock(&SubscriptionLock);

	FSuspenseCoreQueuedEvent QueuedEvent;
	QueuedEvent.EventTag = EventTag;
	QueuedEvent.EventData = EventData;
	QueuedEvent.QueuedTime = FPlatformTime::Seconds();

	DeferredEvents.Add(QueuedEvent);
}

void USuspenseCoreEventBus::PublishSimple(FGameplayTag EventTag, UObject* Source)
{
	FSuspenseCoreEventData Data = FSuspenseCoreEventData::Create(Source);
	Publish(EventTag, Data);
}

void USuspenseCoreEventBus::PublishInternal(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	TotalEventsPublished++;

	// DEBUG: Log Visual.Spawned events for troubleshooting
	const bool bIsVisualSpawned = EventTag.ToString().Contains(TEXT("Visual.Spawned"));
	if (bIsVisualSpawned)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EventBus DEBUG] ═══════════════════════════════════════════════════════"));
		UE_LOG(LogTemp, Warning, TEXT("[EventBus DEBUG] PublishInternal: %s"), *EventTag.ToString());
		UE_LOG(LogTemp, Warning, TEXT("[EventBus DEBUG]   EventBus address: %p"), this);
		UE_LOG(LogTemp, Warning, TEXT("[EventBus DEBUG]   Total subscriptions maps: %d"), Subscriptions.Num());
	}

	// Direct subscribers - copy under lock, call without lock
	TArray<FSuspenseCoreSubscription> DirectSubs;
	{
		FScopeLock Lock(&SubscriptionLock);
		if (TArray<FSuspenseCoreSubscription>* Subs = Subscriptions.Find(EventTag))
		{
			DirectSubs = *Subs;
			if (bIsVisualSpawned)
			{
				UE_LOG(LogTemp, Warning, TEXT("[EventBus DEBUG]   Found %d direct subscribers for tag"), DirectSubs.Num());
			}
		}
		else if (bIsVisualSpawned)
		{
			UE_LOG(LogTemp, Error, TEXT("[EventBus DEBUG]   ✗ NO subscribers found for exact tag!"));
			// List all registered tags for debugging
			UE_LOG(LogTemp, Warning, TEXT("[EventBus DEBUG]   Registered tags:"));
			for (const auto& Pair : Subscriptions)
			{
				UE_LOG(LogTemp, Warning, TEXT("[EventBus DEBUG]     - %s (%d subs)"),
					*Pair.Key.ToString(), Pair.Value.Num());
			}
		}
	}

	if (DirectSubs.Num() > 0)
	{
		if (bIsVisualSpawned)
		{
			UE_LOG(LogTemp, Warning, TEXT("[EventBus DEBUG]   Notifying %d direct subscribers..."), DirectSubs.Num());
		}
		NotifySubscribers(DirectSubs, EventTag, EventData);
	}

	// Child tag subscribers - collect under lock
	TArray<FSuspenseCoreSubscription> ChildSubs;
	{
		FScopeLock Lock(&SubscriptionLock);
		for (const auto& Pair : ChildSubscriptions)
		{
			if (EventTag.MatchesTag(Pair.Key))
			{
				ChildSubs.Append(Pair.Value);
			}
		}
	}

	if (ChildSubs.Num() > 0)
	{
		NotifySubscribers(ChildSubs, EventTag, EventData);
	}
}

void USuspenseCoreEventBus::NotifySubscribers(
	const TArray<FSuspenseCoreSubscription>& Subs,
	FGameplayTag EventTag,
	const FSuspenseCoreEventData& EventData)
{
	// DEBUG: Log Visual.Spawned events for troubleshooting
	const bool bIsVisualSpawned = EventTag.ToString().Contains(TEXT("Visual.Spawned"));

	for (const FSuspenseCoreSubscription& Sub : Subs)
	{
		if (!Sub.IsValid())
		{
			if (bIsVisualSpawned)
			{
				UE_LOG(LogTemp, Error, TEXT("[EventBus DEBUG]   ✗ Subscriber INVALID (ID=%llu, Subscriber=%s)"),
					Sub.Id, Sub.Subscriber.IsValid() ? *Sub.Subscriber->GetName() : TEXT("STALE"));
			}
			continue;
		}

		// Check source filter
		if (Sub.SourceFilter.IsValid() && Sub.SourceFilter.Get() != EventData.Source.Get())
		{
			if (bIsVisualSpawned)
			{
				UE_LOG(LogTemp, Warning, TEXT("[EventBus DEBUG]   Skipping due to SourceFilter mismatch"));
			}
			continue;
		}

		if (bIsVisualSpawned)
		{
			UE_LOG(LogTemp, Warning, TEXT("[EventBus DEBUG]   ✓ Calling subscriber: %s (ID=%llu, Native=%d)"),
				*GetNameSafe(Sub.Subscriber.Get()), Sub.Id, Sub.bUseNativeCallback);
		}

		// Call callback
		if (Sub.bUseNativeCallback)
		{
			if (Sub.NativeCallback.IsBound())
			{
				Sub.NativeCallback.Execute(EventTag, EventData);
				if (bIsVisualSpawned)
				{
					UE_LOG(LogTemp, Warning, TEXT("[EventBus DEBUG]   ✓ Native callback executed"));
				}
			}
			else if (bIsVisualSpawned)
			{
				UE_LOG(LogTemp, Error, TEXT("[EventBus DEBUG]   ✗ Native callback NOT bound!"));
			}
		}
		else
		{
			if (Sub.DynamicCallback.IsBound())
			{
				Sub.DynamicCallback.Execute(EventTag, EventData);
			}
		}
	}

	if (bIsVisualSpawned)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EventBus DEBUG] ═══════════════════════════════════════════════════════"));
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// SUBSCRIPTION
// ═══════════════════════════════════════════════════════════════════════════════

FSuspenseCoreSubscriptionHandle USuspenseCoreEventBus::Subscribe(
	FGameplayTag EventTag,
	const FSuspenseCoreEventCallback& Callback)
{
	if (!Callback.IsBound())
	{
		UE_LOG(LogSuspenseCoreEventBus, Warning, TEXT("Subscribe: Callback not bound"));
		return FSuspenseCoreSubscriptionHandle();
	}

	// Get object from callback (const_cast needed for TWeakObjectPtr)
	UObject* Subscriber = const_cast<UObject*>(Callback.GetUObject());

	return CreateSubscription(
		EventTag,
		Subscriber,
		FSuspenseCoreNativeEventCallback(),
		Callback,
		nullptr,
		ESuspenseCoreEventPriority::Normal,
		false
	);
}

FSuspenseCoreSubscriptionHandle USuspenseCoreEventBus::SubscribeToChildren(
	FGameplayTag ParentTag,
	const FSuspenseCoreEventCallback& Callback)
{
	if (!Callback.IsBound())
	{
		return FSuspenseCoreSubscriptionHandle();
	}

	// Get object from callback (const_cast needed for TWeakObjectPtr)
	UObject* Subscriber = const_cast<UObject*>(Callback.GetUObject());

	return CreateSubscription(
		ParentTag,
		Subscriber,
		FSuspenseCoreNativeEventCallback(),
		Callback,
		nullptr,
		ESuspenseCoreEventPriority::Normal,
		true  // Subscribe to children
	);
}

FSuspenseCoreSubscriptionHandle USuspenseCoreEventBus::SubscribeWithFilter(
	FGameplayTag EventTag,
	const FSuspenseCoreEventCallback& Callback,
	UObject* SourceFilter)
{
	if (!Callback.IsBound())
	{
		return FSuspenseCoreSubscriptionHandle();
	}

	// Get object from callback (const_cast needed for TWeakObjectPtr)
	UObject* Subscriber = const_cast<UObject*>(Callback.GetUObject());

	return CreateSubscription(
		EventTag,
		Subscriber,
		FSuspenseCoreNativeEventCallback(),
		Callback,
		SourceFilter,
		ESuspenseCoreEventPriority::Normal,
		false
	);
}

FSuspenseCoreSubscriptionHandle USuspenseCoreEventBus::SubscribeNative(
	FGameplayTag EventTag,
	UObject* Subscriber,
	FSuspenseCoreNativeEventCallback Callback,
	ESuspenseCoreEventPriority Priority)
{
	return CreateSubscription(
		EventTag,
		Subscriber,
		Callback,
		FSuspenseCoreEventCallback(),
		nullptr,
		Priority,
		false
	);
}

FSuspenseCoreSubscriptionHandle USuspenseCoreEventBus::SubscribeNativeWithFilter(
	FGameplayTag EventTag,
	UObject* Subscriber,
	FSuspenseCoreNativeEventCallback Callback,
	UObject* SourceFilter,
	ESuspenseCoreEventPriority Priority)
{
	return CreateSubscription(
		EventTag,
		Subscriber,
		Callback,
		FSuspenseCoreEventCallback(),
		SourceFilter,
		Priority,
		false
	);
}

FSuspenseCoreSubscriptionHandle USuspenseCoreEventBus::CreateSubscription(
	FGameplayTag EventTag,
	UObject* Subscriber,
	FSuspenseCoreNativeEventCallback NativeCallback,
	FSuspenseCoreEventCallback DynamicCallback,
	UObject* SourceFilter,
	ESuspenseCoreEventPriority Priority,
	bool bSubscribeToChildren)
{
	if (!EventTag.IsValid())
	{
		UE_LOG(LogSuspenseCoreEventBus, Warning, TEXT("CreateSubscription: Invalid EventTag"));
		return FSuspenseCoreSubscriptionHandle();
	}

	// DEBUG: Log Visual.Spawned subscriptions
	const bool bIsVisualSpawned = EventTag.ToString().Contains(TEXT("Visual.Spawned"));
	if (bIsVisualSpawned)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EventBus DEBUG] ═══════════════════════════════════════════════════════"));
		UE_LOG(LogTemp, Warning, TEXT("[EventBus DEBUG] CreateSubscription: %s"), *EventTag.ToString());
		UE_LOG(LogTemp, Warning, TEXT("[EventBus DEBUG]   EventBus address: %p"), this);
		UE_LOG(LogTemp, Warning, TEXT("[EventBus DEBUG]   Subscriber: %s (Valid=%d)"),
			*GetNameSafe(Subscriber), IsValid(Subscriber));
		UE_LOG(LogTemp, Warning, TEXT("[EventBus DEBUG]   NativeCallback bound: %d"), NativeCallback.IsBound());
		UE_LOG(LogTemp, Warning, TEXT("[EventBus DEBUG]   SubscribeToChildren: %d"), bSubscribeToChildren);
	}

	FScopeLock Lock(&SubscriptionLock);

	FSuspenseCoreSubscription NewSub;
	NewSub.Id = NextSubscriptionId++;
	NewSub.Subscriber = Subscriber;
	NewSub.Priority = Priority;
	NewSub.SourceFilter = SourceFilter;
	NewSub.NativeCallback = NativeCallback;
	NewSub.DynamicCallback = DynamicCallback;
	NewSub.bUseNativeCallback = NativeCallback.IsBound();

	TMap<FGameplayTag, TArray<FSuspenseCoreSubscription>>& TargetMap =
		bSubscribeToChildren ? ChildSubscriptions : Subscriptions;

	TArray<FSuspenseCoreSubscription>& Subs = TargetMap.FindOrAdd(EventTag);
	Subs.Add(NewSub);

	// Sort by priority
	SortSubscriptionsByPriority(Subs);

	if (bIsVisualSpawned)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EventBus DEBUG]   ✓ Subscription created (ID=%llu), total for tag: %d"),
			NewSub.Id, Subs.Num());
		UE_LOG(LogTemp, Warning, TEXT("[EventBus DEBUG] ═══════════════════════════════════════════════════════"));
	}

	UE_LOG(LogSuspenseCoreEventBus, Verbose, TEXT("Subscribed to %s (ID: %llu, Children: %d)"),
		*EventTag.ToString(), NewSub.Id, bSubscribeToChildren);

	return FSuspenseCoreSubscriptionHandle(NewSub.Id);
}

// ═══════════════════════════════════════════════════════════════════════════════
// UNSUBSCRIPTION
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreEventBus::Unsubscribe(FSuspenseCoreSubscriptionHandle Handle)
{
	if (!Handle.IsValid())
	{
		return;
	}

	FScopeLock Lock(&SubscriptionLock);

	uint64 TargetId = Handle.GetId();

	// Search in regular subscriptions
	for (auto& Pair : Subscriptions)
	{
		Pair.Value.RemoveAll([TargetId](const FSuspenseCoreSubscription& Sub)
		{
			return Sub.Id == TargetId;
		});
	}

	// Search in child subscriptions
	for (auto& Pair : ChildSubscriptions)
	{
		Pair.Value.RemoveAll([TargetId](const FSuspenseCoreSubscription& Sub)
		{
			return Sub.Id == TargetId;
		});
	}

	UE_LOG(LogSuspenseCoreEventBus, Verbose, TEXT("Unsubscribed (ID: %llu)"), TargetId);
}

void USuspenseCoreEventBus::UnsubscribeAll(UObject* Subscriber)
{
	if (!Subscriber)
	{
		return;
	}

	FScopeLock Lock(&SubscriptionLock);

	for (auto& Pair : Subscriptions)
	{
		Pair.Value.RemoveAll([Subscriber](const FSuspenseCoreSubscription& Sub)
		{
			return Sub.Subscriber.Get() == Subscriber;
		});
	}

	for (auto& Pair : ChildSubscriptions)
	{
		Pair.Value.RemoveAll([Subscriber](const FSuspenseCoreSubscription& Sub)
		{
			return Sub.Subscriber.Get() == Subscriber;
		});
	}

	UE_LOG(LogSuspenseCoreEventBus, Verbose, TEXT("Unsubscribed all for %s"), *GetNameSafe(Subscriber));
}

// ═══════════════════════════════════════════════════════════════════════════════
// UTILITIES
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreEventBus::ProcessDeferredEvents()
{
	TArray<FSuspenseCoreQueuedEvent> EventsToProcess;

	{
		FScopeLock Lock(&SubscriptionLock);
		EventsToProcess = MoveTemp(DeferredEvents);
		DeferredEvents.Empty();
	}

	for (const FSuspenseCoreQueuedEvent& Event : EventsToProcess)
	{
		PublishInternal(Event.EventTag, Event.EventData);
	}
}

void USuspenseCoreEventBus::CleanupStaleSubscriptions()
{
	FScopeLock Lock(&SubscriptionLock);

	for (auto& Pair : Subscriptions)
	{
		Pair.Value.RemoveAll([](const FSuspenseCoreSubscription& Sub)
		{
			return !Sub.IsValid();
		});
	}

	for (auto& Pair : ChildSubscriptions)
	{
		Pair.Value.RemoveAll([](const FSuspenseCoreSubscription& Sub)
		{
			return !Sub.IsValid();
		});
	}

	// Remove empty entries
	for (auto It = Subscriptions.CreateIterator(); It; ++It)
	{
		if (It.Value().Num() == 0)
		{
			It.RemoveCurrent();
		}
	}

	for (auto It = ChildSubscriptions.CreateIterator(); It; ++It)
	{
		if (It.Value().Num() == 0)
		{
			It.RemoveCurrent();
		}
	}
}

FSuspenseCoreEventBusStats USuspenseCoreEventBus::GetStats() const
{
	FScopeLock Lock(&SubscriptionLock);

	FSuspenseCoreEventBusStats Stats;

	for (const auto& Pair : Subscriptions)
	{
		Stats.ActiveSubscriptions += Pair.Value.Num();
	}
	for (const auto& Pair : ChildSubscriptions)
	{
		Stats.ActiveSubscriptions += Pair.Value.Num();
	}

	Stats.UniqueEventTags = Subscriptions.Num() + ChildSubscriptions.Num();
	Stats.TotalEventsPublished = TotalEventsPublished;
	Stats.DeferredEventsQueued = DeferredEvents.Num();

	return Stats;
}

bool USuspenseCoreEventBus::HasSubscribers(FGameplayTag EventTag) const
{
	FScopeLock Lock(&SubscriptionLock);

	if (const TArray<FSuspenseCoreSubscription>* Subs = Subscriptions.Find(EventTag))
	{
		return Subs->Num() > 0;
	}

	return false;
}

void USuspenseCoreEventBus::SortSubscriptionsByPriority(TArray<FSuspenseCoreSubscription>& Subs)
{
	// Uses FSuspenseCoreSubscription::operator< for sorting
	Subs.Sort();
}
