// SuspenseCoreEquipmentEventDispatcher.cpp
// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Components/Coordination/SuspenseCoreEquipmentEventDispatcher.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Services/SuspenseCoreServiceProvider.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "Engine/World.h"
#include "Async/Async.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreEventDispatcher, Log, All);

USuspenseCoreEquipmentEventDispatcher::USuspenseCoreEquipmentEventDispatcher()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Initialize tags with new SuspenseCore.Event.* format
	TagDelta = SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Data_Delta;
	TagBatchDelta = SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Data;
	TagOperationCompleted = SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Operation_Completed;
}

void USuspenseCoreEquipmentEventDispatcher::BeginPlay()
{
	Super::BeginPlay();
	ConnectToEventBus();
}

void USuspenseCoreEquipmentEventDispatcher::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DisconnectFromEventBus();

	// Clear local queue
	{
		FScopeLock Lock(&QueueCs);
		LocalQueue.Empty();
	}

	// Remove all subscriptions
	int32 Removed = UnsubscribeAll(nullptr);
	if (bVerbose)
	{
		UE_LOG(LogSuspenseCoreEventDispatcher, Verbose, TEXT("EndPlay: removed %d local subscriptions"), Removed);
	}

	Super::EndPlay(EndPlayReason);
}

void USuspenseCoreEquipmentEventDispatcher::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bBatchMode)
	{
		return;
	}

	Accumulator += DeltaTime;
	if (Accumulator < FlushInterval)
	{
		return;
	}
	Accumulator = 0.f;

	int32 Dispatched = 0;
	while (Dispatched < MaxPerTick)
	{
		FQueuedEvent QueuedEvent;
		{
			FScopeLock Lock(&QueueCs);
			if (LocalQueue.Num() == 0)
			{
				break;
			}
			QueuedEvent = MoveTemp(LocalQueue[0]);
			LocalQueue.RemoveAt(0, 1, EAllowShrinking::No);
			Stats.CurrentQueueSize = LocalQueue.Num();
		}
		DispatchEvent(QueuedEvent.EventTag, QueuedEvent.EventData);
		++Dispatched;
	}
}

//========================================
// ISuspenseCoreEventDispatcher Interface
//========================================

USuspenseCoreEventBus* USuspenseCoreEquipmentEventDispatcher::GetEventBus() const
{
	return EventBus;
}

FSuspenseCoreSubscriptionHandle USuspenseCoreEquipmentEventDispatcher::Subscribe(
	const FGameplayTag& EventTag,
	UObject* Subscriber,
	FSuspenseCoreNativeEventCallback Callback,
	ESuspenseCoreEventPriority Priority)
{
	if (!EventTag.IsValid() || !Callback.IsBound())
	{
		return FSuspenseCoreSubscriptionHandle();
	}

	FSuspenseCoreDispatcherSubscription Sub;
	Sub.Handle = GenerateHandle();
	Sub.Callback = Callback;
	Sub.Subscriber = Subscriber;
	Sub.Priority = Priority;
	Sub.bActive = true;
	Sub.SubscribedAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	TArray<FSuspenseCoreDispatcherSubscription>& Arr = LocalSubscriptions.FindOrAdd(EventTag);
	Arr.Add(Sub);
	SortByPriority(Arr);

	HandleToTag.Add(Sub.Handle.GetId(), EventTag);
	Stats.ActiveLocalSubscriptions++;

	if (bVerbose)
	{
		UE_LOG(LogSuspenseCoreEventDispatcher, Verbose, TEXT("Subscribe to %s, handle=%llu"),
			*EventTag.ToString(), Sub.Handle.GetId());
	}

	return Sub.Handle;
}

bool USuspenseCoreEquipmentEventDispatcher::Unsubscribe(const FSuspenseCoreSubscriptionHandle& Handle)
{
	if (!Handle.IsValid())
	{
		return false;
	}

	const FGameplayTag* TagPtr = HandleToTag.Find(Handle.GetId());
	if (!TagPtr)
	{
		return false;
	}

	TArray<FSuspenseCoreDispatcherSubscription>* Arr = LocalSubscriptions.Find(*TagPtr);
	if (!Arr)
	{
		return false;
	}

	const int32 Before = Arr->Num();
	Arr->RemoveAll([&](const FSuspenseCoreDispatcherSubscription& S)
	{
		return S.Handle == Handle;
	});

	const int32 Removed = Before - Arr->Num();
	if (Removed > 0)
	{
		HandleToTag.Remove(Handle.GetId());
		Stats.ActiveLocalSubscriptions = FMath::Max(0, Stats.ActiveLocalSubscriptions - Removed);

		if (bVerbose)
		{
			UE_LOG(LogSuspenseCoreEventDispatcher, Verbose, TEXT("Unsubscribe handle=%llu, removed=%d"),
				Handle.GetId(), Removed);
		}
		return true;
	}

	return false;
}

int32 USuspenseCoreEquipmentEventDispatcher::UnsubscribeAll(UObject* Subscriber)
{
	if (Subscriber == nullptr)
	{
		// Remove all subscriptions
		int32 Removed = 0;
		for (auto& Pair : LocalSubscriptions)
		{
			Removed += Pair.Value.Num();
		}
		LocalSubscriptions.Empty();
		HandleToTag.Empty();
		Stats.ActiveLocalSubscriptions = 0;
		return Removed;
	}

	int32 Removed = 0;
	for (auto& Pair : LocalSubscriptions)
	{
		Removed += Pair.Value.RemoveAll([Subscriber](const FSuspenseCoreDispatcherSubscription& S)
		{
			return S.Subscriber.Get() == Subscriber;
		});
	}

	Stats.ActiveLocalSubscriptions = FMath::Max(0, Stats.ActiveLocalSubscriptions - Removed);

	// Rebuild handle map from current subscriptions
	HandleToTag.Reset();
	for (const auto& Pair : LocalSubscriptions)
	{
		for (const FSuspenseCoreDispatcherSubscription& S : Pair.Value)
		{
			HandleToTag.Add(S.Handle.GetId(), Pair.Key);
		}
	}

	return Removed;
}

void USuspenseCoreEquipmentEventDispatcher::Publish(const FGameplayTag& EventTag, const FSuspenseCoreEventData& EventData)
{
	if (!EventBus)
	{
		// Fallback: dispatch locally only
		if (bBatchMode)
		{
			EnqueueEvent(EventTag, EventData);
		}
		else
		{
			DispatchEvent(EventTag, EventData);
		}
		return;
	}

	// Publish through EventBus
	EventBus->Publish(EventTag, EventData);
}

void USuspenseCoreEquipmentEventDispatcher::PublishDeferred(const FGameplayTag& EventTag, const FSuspenseCoreEventData& EventData)
{
	if (!EventBus)
	{
		// Always enqueue for deferred
		EnqueueEvent(EventTag, EventData);
		return;
	}

	EventBus->PublishDeferred(EventTag, EventData);
}

bool USuspenseCoreEquipmentEventDispatcher::HasSubscribers(const FGameplayTag& EventTag) const
{
	// Check local subscriptions
	const TArray<FSuspenseCoreDispatcherSubscription>* Arr = LocalSubscriptions.Find(EventTag);
	if (Arr && Arr->Num() > 0)
	{
		return true;
	}

	// Check EventBus
	if (EventBus)
	{
		return EventBus->HasSubscribers(EventTag);
	}

	return false;
}

FString USuspenseCoreEquipmentEventDispatcher::GetStatistics() const
{
	FString Result;
	Result += FString::Printf(
		TEXT("LocalSubs:%d Queue:%d Peak:%.0f Dispatched:%d AvgMs:%.2f\n"),
		Stats.ActiveLocalSubscriptions,
		Stats.CurrentQueueSize,
		Stats.PeakQueueSize,
		Stats.TotalEventsDispatched,
		Stats.AverageDispatchMs
	);

	if (EventBus)
	{
		FSuspenseCoreEventBusStats BusStats = EventBus->GetStats();
		Result += FString::Printf(
			TEXT("EventBus: Subscriptions=%d Published=%lld Queued=%d"),
			BusStats.ActiveSubscriptions,
			BusStats.TotalEventsPublished,
			BusStats.DeferredEventsQueued
		);
	}

	return Result;
}

//========================================
// Extended API
//========================================

void USuspenseCoreEquipmentEventDispatcher::SetBatchModeEnabled(bool bEnabled, float FlushIntervalSec, int32 InMaxPerTick)
{
	bBatchMode = bEnabled;
	FlushInterval = FMath::Max(0.f, FlushIntervalSec);
	MaxPerTick = FMath::Max(1, InMaxPerTick);
}

void USuspenseCoreEquipmentEventDispatcher::FlushBatched()
{
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, [this]() { FlushBatched(); });
		return;
	}

	TArray<FQueuedEvent> LocalCopy;
	{
		FScopeLock Lock(&QueueCs);
		LocalCopy = MoveTemp(LocalQueue);
		Stats.CurrentQueueSize = 0;
	}

	for (const FQueuedEvent& QE : LocalCopy)
	{
		DispatchEvent(QE.EventTag, QE.EventData);
	}
}

FSuspenseCoreEventDispatcherStats USuspenseCoreEquipmentEventDispatcher::GetStats() const
{
	return Stats;
}

void USuspenseCoreEquipmentEventDispatcher::SetDetailedLogging(bool bEnable)
{
	bVerbose = bEnable;
}

//========================================
// Internal Methods
//========================================

void USuspenseCoreEquipmentEventDispatcher::ConnectToEventBus()
{
	// Get EventBus from ServiceProvider
	USuspenseCoreServiceProvider* Provider = USuspenseCoreServiceProvider::Get(this);
	if (Provider)
	{
		EventBus = Provider->GetEventBus();
	}

	if (!EventBus)
	{
		UE_LOG(LogSuspenseCoreEventDispatcher, Warning,
			TEXT("Could not get EventBus from ServiceProvider. Local-only mode."));
		return;
	}

	// Subscribe to equipment events from EventBus
	auto DeltaHandle = EventBus->SubscribeNative(
		TagDelta,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreEquipmentEventDispatcher::OnBusEvent),
		ESuspenseCoreEventPriority::Normal
	);
	BusSubscriptions.Add(DeltaHandle);

	auto BatchHandle = EventBus->SubscribeNative(
		TagBatchDelta,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreEquipmentEventDispatcher::OnBusEvent),
		ESuspenseCoreEventPriority::Normal
	);
	BusSubscriptions.Add(BatchHandle);

	auto OpHandle = EventBus->SubscribeNative(
		TagOperationCompleted,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreEquipmentEventDispatcher::OnBusEvent),
		ESuspenseCoreEventPriority::Normal
	);
	BusSubscriptions.Add(OpHandle);

	UE_LOG(LogSuspenseCoreEventDispatcher, Log, TEXT("Connected to EventBus with %d subscriptions"),
		BusSubscriptions.Num());
}

void USuspenseCoreEquipmentEventDispatcher::DisconnectFromEventBus()
{
	if (!EventBus)
	{
		return;
	}

	for (const FSuspenseCoreSubscriptionHandle& Handle : BusSubscriptions)
	{
		EventBus->Unsubscribe(Handle);
	}
	BusSubscriptions.Empty();

	UE_LOG(LogSuspenseCoreEventDispatcher, Log, TEXT("Disconnected from EventBus"));
}

void USuspenseCoreEquipmentEventDispatcher::OnBusEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	if (bBatchMode)
	{
		EnqueueEvent(EventTag, EventData);
	}
	else
	{
		DispatchEvent(EventTag, EventData);
	}
}

void USuspenseCoreEquipmentEventDispatcher::EnqueueEvent(const FGameplayTag& EventTag, const FSuspenseCoreEventData& EventData)
{
	FScopeLock Lock(&QueueCs);

	FQueuedEvent QE;
	QE.EventTag = EventTag;
	QE.EventData = EventData;

	LocalQueue.Add(QE);
	Stats.TotalEventsQueued++;
	Stats.CurrentQueueSize = LocalQueue.Num();
	Stats.PeakQueueSize = FMath::Max(Stats.PeakQueueSize, (float)LocalQueue.Num());
}

void USuspenseCoreEquipmentEventDispatcher::DispatchEvent(const FGameplayTag& EventTag, const FSuspenseCoreEventData& EventData)
{
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, [this, EventTag, EventData]()
		{
			DispatchEvent(EventTag, EventData);
		});
		return;
	}

	const double StartTime = FPlatformTime::Seconds();

	DispatchToLocal(EventTag, EventData);

	const double ElapsedMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;
	Stats.TotalEventsDispatched++;

	// Exponential moving average
	const float Alpha = 0.01f;
	Stats.AverageDispatchMs = (1.f - Alpha) * Stats.AverageDispatchMs + Alpha * (float)ElapsedMs;

	if (bVerbose)
	{
		UE_LOG(LogSuspenseCoreEventDispatcher, Verbose, TEXT("Dispatch %s in %.2f ms"),
			*EventTag.ToString(), (float)ElapsedMs);
	}
}

void USuspenseCoreEquipmentEventDispatcher::DispatchToLocal(const FGameplayTag& EventTag, const FSuspenseCoreEventData& EventData)
{
	TArray<FSuspenseCoreDispatcherSubscription> CopyArr;
	{
		TArray<FSuspenseCoreDispatcherSubscription>* Arr = LocalSubscriptions.Find(EventTag);
		if (!Arr || Arr->Num() == 0)
		{
			return;
		}
		CopyArr = *Arr;
	}

	for (FSuspenseCoreDispatcherSubscription& Sub : CopyArr)
	{
		if (!Sub.bActive)
		{
			continue;
		}

		if (Sub.Subscriber.IsValid() && !IsValid(Sub.Subscriber.Get()))
		{
			continue;
		}

		Sub.Callback.Execute(EventTag, EventData);
		Sub.DispatchCount++;
	}
}

void USuspenseCoreEquipmentEventDispatcher::SortByPriority(TArray<FSuspenseCoreDispatcherSubscription>& Arr)
{
	// Lower priority value = higher priority (System=0 is highest)
	Arr.Sort([](const FSuspenseCoreDispatcherSubscription& A, const FSuspenseCoreDispatcherSubscription& B)
	{
		return static_cast<uint8>(A.Priority) < static_cast<uint8>(B.Priority);
	});
}

int32 USuspenseCoreEquipmentEventDispatcher::CleanupInvalid()
{
	int32 Removed = 0;
	for (auto& Pair : LocalSubscriptions)
	{
		Removed += Pair.Value.RemoveAll([](const FSuspenseCoreDispatcherSubscription& S)
		{
			return !S.Callback.IsBound() || (S.Subscriber.IsValid() && !IsValid(S.Subscriber.Get()));
		});
	}

	Stats.ActiveLocalSubscriptions = FMath::Max(0, Stats.ActiveLocalSubscriptions - Removed);
	return Removed;
}

FSuspenseCoreSubscriptionHandle USuspenseCoreEquipmentEventDispatcher::GenerateHandle()
{
	return FSuspenseCoreSubscriptionHandle(NextSubscriptionId++);
}
