// SuspenseCoreEventBus.cpp
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCoreEventBus.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreEventBus, Log, All);

USuspenseCoreEventBus::USuspenseCoreEventBus()
{
}

// ═══════════════════════════════════════════════════════════════════════════════
// ПУБЛИКАЦИЯ
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

	// Прямые подписчики
	{
		FScopeLock Lock(&SubscriptionLock);

		if (TArray<FSuspenseCoreSubscription>* Subs = Subscriptions.Find(EventTag))
		{
			// Копируем чтобы избежать проблем при модификации во время итерации
			TArray<FSuspenseCoreSubscription> SubsCopy = *Subs;
			Lock.Unlock();

			NotifySubscribers(SubsCopy, EventTag, EventData);
		}
	}

	// Подписчики на родительские теги
	{
		FScopeLock Lock(&SubscriptionLock);

		for (auto& Pair : ChildSubscriptions)
		{
			if (EventTag.MatchesTag(Pair.Key))
			{
				TArray<FSuspenseCoreSubscription> SubsCopy = Pair.Value;
				Lock.Unlock();

				NotifySubscribers(SubsCopy, EventTag, EventData);

				Lock.Lock();
			}
		}
	}
}

void USuspenseCoreEventBus::NotifySubscribers(
	const TArray<FSuspenseCoreSubscription>& Subs,
	FGameplayTag EventTag,
	const FSuspenseCoreEventData& EventData)
{
	for (const FSuspenseCoreSubscription& Sub : Subs)
	{
		if (!Sub.IsValid())
		{
			continue;
		}

		// Проверка фильтра по источнику
		if (Sub.SourceFilter.IsValid() && Sub.SourceFilter.Get() != EventData.Source.Get())
		{
			continue;
		}

		// Вызов callback
		if (Sub.bUseNativeCallback)
		{
			if (Sub.NativeCallback.IsBound())
			{
				Sub.NativeCallback.Execute(EventTag, EventData);
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
}

// ═══════════════════════════════════════════════════════════════════════════════
// ПОДПИСКА
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

	// Получаем объект из callback
	UObject* Subscriber = Callback.GetUObject();

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

	UObject* Subscriber = Callback.GetUObject();

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

	UObject* Subscriber = Callback.GetUObject();

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

	// Сортируем по приоритету
	SortSubscriptionsByPriority(Subs);

	UE_LOG(LogSuspenseCoreEventBus, Verbose, TEXT("Subscribed to %s (ID: %llu, Children: %d)"),
		*EventTag.ToString(), NewSub.Id, bSubscribeToChildren);

	return FSuspenseCoreSubscriptionHandle(NewSub.Id);
}

// ═══════════════════════════════════════════════════════════════════════════════
// ОТПИСКА
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreEventBus::Unsubscribe(FSuspenseCoreSubscriptionHandle Handle)
{
	if (!Handle.IsValid())
	{
		return;
	}

	FScopeLock Lock(&SubscriptionLock);

	uint64 TargetId = Handle.GetId();

	// Ищем в обычных подписках
	for (auto& Pair : Subscriptions)
	{
		Pair.Value.RemoveAll([TargetId](const FSuspenseCoreSubscription& Sub)
		{
			return Sub.Id == TargetId;
		});
	}

	// Ищем в child подписках
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
// УТИЛИТЫ
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

	// Удаляем пустые записи
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
	Subs.Sort([](const FSuspenseCoreSubscription& A, const FSuspenseCoreSubscription& B)
	{
		return static_cast<uint8>(A.Priority) < static_cast<uint8>(B.Priority);
	});
}
