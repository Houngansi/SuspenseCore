// Copyright Suspense Team. All Rights Reserved.

#include "Core/Utils/FSuspenseEquipmentEventBus.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Async/Async.h"
#include "TimerManager.h"

FSuspenseEquipmentEventBus::FSuspenseEquipmentEventBus()
    : bProcessingQueue(false)
    , MaxQueueSize(1000)
    , DelayedEventCheckInterval(0.1f)
    , CleanupInterval(30.0f)
    , LastCleanupTime(0.0f)
    , MaxSubscriptionsPerOwner(100)
    , TotalCleanedSubscriptions(0)
    , RejectedSubscriptions(0)
    , EventQueueHead(0)
    , EventQueueTail(0)
    , EventQueueCount(0)
{
    // Initialize statistics
    Statistics = FEventBusStats();
    LastCleanupDateTime = FDateTime::Now();
    
    // Pre-allocate event queue
    EventQueueArray.Reserve(MaxQueueSize);
}

FSuspenseEquipmentEventBus::~FSuspenseEquipmentEventBus()
{
    // Cancel timers if they exist
    if (UWorld* World = GWorld)
    {
        World->GetTimerManager().ClearTimer(DelayedEventTimerHandle);
        World->GetTimerManager().ClearTimer(CleanupTimerHandle);
    }
    
    // Clear all subscriptions
    {
        FScopeLock Lock(&BusLock);
        SubscriptionMap.Empty();
        HandleToEventMap.Empty();
        SubscriptionCountPerOwner.Empty();
    }
    
    // Clear event queue
    {
        FScopeLock Lock(&EventQueueLock);
        EventQueueArray.Empty();
        EventQueueHead = 0;
        EventQueueTail = 0;
        EventQueueCount = 0;
        DelayedEvents.Empty();
    }
}

void FSuspenseEquipmentEventBus::InitializeAutomaticCleanup()
{
    if (UWorld* World = GWorld)
    {
        // Schedule automatic cleanup
        World->GetTimerManager().SetTimer(
            CleanupTimerHandle,
            [WeakThis = TWeakPtr<FSuspenseEquipmentEventBus>(AsShared())]()
            {
                if (auto This = WeakThis.Pin())
                {
                    This->PerformAutomaticCleanup();
                }
            },
            CleanupInterval,
            true // Loop
        );
        
        // Schedule delayed event processing
        World->GetTimerManager().SetTimer(
            DelayedEventTimerHandle,
            [WeakThis = TWeakPtr<FSuspenseEquipmentEventBus>(AsShared())]()
            {
                if (auto This = WeakThis.Pin())
                {
                    This->ProcessDelayedEvents();
                }
            },
            DelayedEventCheckInterval,
            true // Loop
        );
        
        UE_LOG(LogTemp, Log, TEXT("EventBus: Initialized with automatic cleanup every %.1f seconds"), 
            CleanupInterval);
    }
}

FEventSubscriptionHandle FSuspenseEquipmentEventBus::Subscribe(
    const FGameplayTag& EventType,
    const FEventHandlerDelegate& Handler,
    EEventPriority Priority,
    EEventExecutionContext Context,
    UObject* Owner)
{
    if (!EventType.IsValid() || !Handler.IsBound())
    {
        UE_LOG(LogTemp, Warning, TEXT("EventBus: Invalid subscription parameters"));
        return FEventSubscriptionHandle();
    }
    
    FScopeLock Lock(&BusLock);
    
    // Check subscription limit for owner
    if (Owner && IsOwnerAtSubscriptionLimit(Owner))
    {
        RejectedSubscriptions++;
        Statistics.RejectedSubscriptions++;
        
        UE_LOG(LogTemp, Error, TEXT("EventBus: Owner %s exceeded max subscriptions limit (%d)"), 
            *GetNameSafe(Owner), MaxSubscriptionsPerOwner);
        
        return FEventSubscriptionHandle(); // Return invalid handle
    }
    
    // Create subscription
    FEventSubscription Subscription;
    Subscription.Handler = Handler;
    Subscription.Priority = Priority;
    Subscription.ExecutionContext = Context;
    Subscription.Owner = Owner;
    Subscription.EventFilter.AddTag(EventType);
    Subscription.SubscriptionTime = FPlatformTime::Seconds();
    
    // Add to subscription map
    TArray<FEventSubscription>& Subscriptions = SubscriptionMap.FindOrAdd(EventType);
    Subscriptions.Add(Subscription);
    
    // Sort by priority
    SortByPriority(Subscriptions);
    
    // Track handle to event mapping
    HandleToEventMap.Add(Subscription.Handle, EventType);
    
    // Update owner subscription count
    if (Owner)
    {
        UpdateOwnerSubscriptionCount(Owner, 1);
    }
    
    // Update statistics
    Statistics.TotalSubscriptions++;
    Statistics.ActiveSubscriptions++;
    
    UE_LOG(LogTemp, Verbose, TEXT("EventBus: Subscribed to %s (Priority: %d, Owner: %s)"),
        *EventType.ToString(),
        (int32)Priority,
        Owner ? *GetNameSafe(Owner) : TEXT("None"));
    
    return Subscription.Handle;
}

TArray<FEventSubscriptionHandle> FSuspenseEquipmentEventBus::SubscribeMultiple(
    const FGameplayTagContainer& EventTypes,
    const FEventHandlerDelegate& Handler,
    EEventPriority Priority,
    EEventExecutionContext Context,
    UObject* Owner)
{
    TArray<FEventSubscriptionHandle> Handles;
    
    // Check if owner would exceed limit with all subscriptions
    if (Owner)
    {
        FScopeLock Lock(&BusLock);
        
        int32 CurrentCount = SubscriptionCountPerOwner.Contains(Owner) ? 
            SubscriptionCountPerOwner[Owner] : 0;
        
        if (CurrentCount + EventTypes.Num() > MaxSubscriptionsPerOwner)
        {
            RejectedSubscriptions += EventTypes.Num();
            Statistics.RejectedSubscriptions += EventTypes.Num();
            
            UE_LOG(LogTemp, Error, TEXT("EventBus: Owner %s would exceed limit with %d new subscriptions"),
                *GetNameSafe(Owner), EventTypes.Num());
            
            return Handles; // Return empty array
        }
    }
    
    for (const FGameplayTag& EventType : EventTypes)
    {
        FEventSubscriptionHandle Handle = Subscribe(EventType, Handler, Priority, Context, Owner);
        if (Handle.IsValid())
        {
            Handles.Add(Handle);
        }
    }
    
    return Handles;
}

bool FSuspenseEquipmentEventBus::Unsubscribe(const FEventSubscriptionHandle& Handle)
{
    if (!Handle.IsValid())
    {
        return false;
    }
    
    FScopeLock Lock(&BusLock);
    
    // Find event type for this handle
    FGameplayTag* EventType = HandleToEventMap.Find(Handle);
    if (!EventType)
    {
        return false;
    }
    
    // Find and remove subscription
    TArray<FEventSubscription>* Subscriptions = SubscriptionMap.Find(*EventType);
    if (Subscriptions)
    {
        for (int32 i = 0; i < Subscriptions->Num(); i++)
        {
            if ((*Subscriptions)[i].Handle == Handle)
            {
                // Update owner count
                if ((*Subscriptions)[i].Owner.IsValid())
                {
                    UpdateOwnerSubscriptionCount((*Subscriptions)[i].Owner.Get(), -1);
                }
                
                Subscriptions->RemoveAt(i);
                HandleToEventMap.Remove(Handle);
                
                Statistics.ActiveSubscriptions--;
                
                UE_LOG(LogTemp, Verbose, TEXT("EventBus: Unsubscribed handle from %s"),
                    *EventType->ToString());
                
                return true;
            }
        }
    }
    
    return false;
}

int32 FSuspenseEquipmentEventBus::UnsubscribeAll(UObject* Owner)
{
    if (!Owner)
    {
        return 0;
    }
    
    FScopeLock Lock(&BusLock);
    
    int32 RemovedCount = 0;
    TArray<FEventSubscriptionHandle> HandlesToRemove;
    
    // Find all subscriptions for this owner
    for (auto& Pair : SubscriptionMap)
    {
        TArray<FEventSubscription>& Subscriptions = Pair.Value;
        
        for (int32 i = Subscriptions.Num() - 1; i >= 0; i--)
        {
            if (Subscriptions[i].Owner.Get() == Owner)
            {
                HandlesToRemove.Add(Subscriptions[i].Handle);
                Subscriptions.RemoveAt(i);
                RemovedCount++;
            }
        }
    }
    
    // Remove handle mappings
    for (const FEventSubscriptionHandle& Handle : HandlesToRemove)
    {
        HandleToEventMap.Remove(Handle);
    }
    
    // Clear owner subscription count
    SubscriptionCountPerOwner.Remove(Owner);
    
    Statistics.ActiveSubscriptions -= RemovedCount;
    
    if (RemovedCount > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("EventBus: Unsubscribed %d handlers for %s"),
            RemovedCount, *GetNameSafe(Owner));
    }
    
    return RemovedCount;
}

void FSuspenseEquipmentEventBus::Broadcast(const FSuspenseEquipmentEventData& EventData)
{
    if (!EventData.EventType.IsValid())
    {
        return;
    }
    
    // Check filter
    if (!PassesFilter(EventData.EventType))
    {
        return;
    }
    
    FScopeLock Lock(&BusLock);
    
    // Find subscriptions for this event
    TArray<FEventSubscription>* Subscriptions = SubscriptionMap.Find(EventData.EventType);
    if (!Subscriptions || Subscriptions->Num() == 0)
    {
        return;
    }
    
    // Create a copy to avoid issues with modifications during dispatch
    TArray<FEventSubscription> SubscriptionsCopy = *Subscriptions;
    
    // Dispatch event
    DispatchEvent(EventData, SubscriptionsCopy);
    
    // Update statistics
    Statistics.TotalEventsDispatched++;
    Statistics.EventTypeCounts.FindOrAdd(EventData.EventType)++;
}

void FSuspenseEquipmentEventBus::BroadcastDelayed(const FSuspenseEquipmentEventData& EventData, float Delay)
{
    if (Delay <= 0.0f)
    {
        Broadcast(EventData);
        return;
    }
    
    FScopeLock Lock(&BusLock);
    
    float ExecutionTime = FPlatformTime::Seconds() + Delay;
    TArray<FSuspenseEquipmentEventData>& EventsAtTime = DelayedEvents.FindOrAdd(ExecutionTime);
    EventsAtTime.Add(EventData);
    
    UE_LOG(LogTemp, Verbose, TEXT("EventBus: Scheduled delayed event %s for %.2f seconds"),
        *EventData.EventType.ToString(), Delay);
}

void FSuspenseEquipmentEventBus::QueueEvent(const FSuspenseEquipmentEventData& EventData)
{
    FScopeLock Lock(&EventQueueLock);
    
    // Проверяем, не переполнена ли очередь
    if (EventQueueCount >= MaxQueueSize)
    {
        UE_LOG(LogTemp, Warning, TEXT("EventBus: Event queue is full (%d/%d), dropping event %s"),
            EventQueueCount, MaxQueueSize, *EventData.EventType.ToString());
        Statistics.TotalEventsFailed++;
        return;
    }
    
    // Расширяем массив если нужно
    if (EventQueueArray.Num() <= EventQueueTail)
    {
        EventQueueArray.SetNum(FMath::Max(EventQueueTail + 1, MaxQueueSize));
    }
    
    // Добавляем событие в очередь
    EventQueueArray[EventQueueTail] = EventData;
    EventQueueTail = (EventQueueTail + 1) % MaxQueueSize;
    EventQueueCount++;
    
    Statistics.QueuedEvents++;
}

int32 FSuspenseEquipmentEventBus::ProcessEventQueue(int32 MaxEvents)
{
    if (bProcessingQueue)
    {
        return 0; // Already processing
    }
    
    bProcessingQueue = true;
    int32 ProcessedCount = 0;
    
    while ((MaxEvents < 0 || ProcessedCount < MaxEvents) && EventQueueCount > 0)
    {
        FSuspenseEquipmentEventData EventData;
        
        {
            FScopeLock Lock(&EventQueueLock);
            
            if (EventQueueCount == 0)
            {
                break;
            }
            
            // Извлекаем событие из очереди
            EventData = EventQueueArray[EventQueueHead];
            EventQueueHead = (EventQueueHead + 1) % MaxQueueSize;
            EventQueueCount--;
            Statistics.QueuedEvents--;
        }
        
        Broadcast(EventData);
        ProcessedCount++;
    }
    
    bProcessingQueue = false;
    
    return ProcessedCount;
}

void FSuspenseEquipmentEventBus::ClearEventQueue(const FGameplayTag& EventType)
{
    FScopeLock Lock(&EventQueueLock);
    
    if (!EventType.IsValid())
    {
        // Clear all events
        EventQueueHead = 0;
        EventQueueTail = 0;
        EventQueueCount = 0;
        EventQueueArray.Empty();
        Statistics.QueuedEvents = 0;
        
        UE_LOG(LogTemp, Log, TEXT("EventBus: Cleared all queued events"));
    }
    else
    {
        // Clear specific event type
        TArray<FSuspenseEquipmentEventData> RemainingEvents;
        
        // Извлекаем все события из очереди
        while (EventQueueCount > 0)
        {
            FSuspenseEquipmentEventData EventData = EventQueueArray[EventQueueHead];
            EventQueueHead = (EventQueueHead + 1) % MaxQueueSize;
            EventQueueCount--;
            
            if (EventData.EventType != EventType)
            {
                RemainingEvents.Add(EventData);
            }
            else
            {
                Statistics.QueuedEvents--;
            }
        }
        
        // Сбрасываем очередь
        EventQueueHead = 0;
        EventQueueTail = 0;
        EventQueueCount = 0;
        
        // Добавляем обратно оставшиеся события
        for (const FSuspenseEquipmentEventData& Event : RemainingEvents)
        {
            if (EventQueueArray.Num() <= EventQueueTail)
            {
                EventQueueArray.SetNum(EventQueueTail + 1);
            }
            EventQueueArray[EventQueueTail] = Event;
            EventQueueTail = (EventQueueTail + 1) % MaxQueueSize;
            EventQueueCount++;
        }
        
        UE_LOG(LogTemp, Log, TEXT("EventBus: Cleared queued events of type %s"),
            *EventType.ToString());
    }
}

void FSuspenseEquipmentEventBus::SetEventFilter(const FGameplayTag& EventType, bool bAllow)
{
    FScopeLock Lock(&BusLock);
    
    EventFilters.Add(EventType, bAllow);
    
    UE_LOG(LogTemp, Log, TEXT("EventBus: Set filter for %s to %s"),
        *EventType.ToString(),
        bAllow ? TEXT("Allow") : TEXT("Block"));
}

bool FSuspenseEquipmentEventBus::SetSubscriptionEnabled(const FEventSubscriptionHandle& Handle, bool bEnabled)
{
    FScopeLock Lock(&BusLock);
    
    // Find event type for this handle
    FGameplayTag* EventType = HandleToEventMap.Find(Handle);
    if (!EventType)
    {
        return false;
    }
    
    // Find subscription
    TArray<FEventSubscription>* Subscriptions = SubscriptionMap.Find(*EventType);
    if (Subscriptions)
    {
        for (FEventSubscription& Subscription : *Subscriptions)
        {
            if (Subscription.Handle == Handle)
            {
                if (Subscription.bIsActive != bEnabled)
                {
                    Subscription.bIsActive = bEnabled;
                    
                    UE_LOG(LogTemp, Verbose, TEXT("EventBus: Subscription %s"),
                        bEnabled ? TEXT("enabled") : TEXT("disabled"));
                    
                    return true;
                }
                break;
            }
        }
    }
    
    return false;
}

FSuspenseEquipmentEventBus::FEventBusStats FSuspenseEquipmentEventBus::GetStatistics() const
{
    FScopeLock Lock(&BusLock);
    
    // Update subscription counts per owner
    Statistics.SubscriptionsPerOwner.Empty();
    for (const auto& Pair : SubscriptionCountPerOwner)
    {
        // Проверяем валидность TWeakObjectPtr
        if (Pair.Key.IsValid())
        {
            UObject* OwnerObj = Pair.Key.Get();
            if (OwnerObj)
            {
                Statistics.SubscriptionsPerOwner.Add(OwnerObj, Pair.Value);
            }
        }
    }
    
    Statistics.TotalCleanedSubscriptions = TotalCleanedSubscriptions;
    Statistics.RejectedSubscriptions = RejectedSubscriptions;
    Statistics.QueuedEvents = EventQueueCount;
    
    return Statistics;
}

void FSuspenseEquipmentEventBus::ResetStatistics()
{
    FScopeLock Lock(&BusLock);
    
    Statistics.TotalEventsDispatched = 0;
    Statistics.TotalEventsFailed = 0;
    Statistics.AverageDispatchTime = 0.0f;
    Statistics.EventTypeCounts.Empty();
    Statistics.TotalCleanedSubscriptions = 0;
    Statistics.RejectedSubscriptions = 0;
    
    TotalCleanedSubscriptions = 0;
    RejectedSubscriptions = 0;
    
    UE_LOG(LogTemp, Log, TEXT("EventBus: Statistics reset"));
}

bool FSuspenseEquipmentEventBus::ValidateBusIntegrity() const
{
    FScopeLock Lock(&BusLock);
    
    // Check handle mappings
    for (const auto& HandlePair : HandleToEventMap)
    {
        const FGameplayTag& EventType = HandlePair.Value;
        const TArray<FEventSubscription>* Subscriptions = SubscriptionMap.Find(EventType);
        
        if (!Subscriptions)
        {
            UE_LOG(LogTemp, Error, TEXT("EventBus: Orphaned handle mapping for event %s"),
                *EventType.ToString());
            return false;
        }
        
        bool bFoundHandle = false;
        for (const FEventSubscription& Sub : *Subscriptions)
        {
            if (Sub.Handle == HandlePair.Key)
            {
                bFoundHandle = true;
                break;
            }
        }
        
        if (!bFoundHandle)
        {
            UE_LOG(LogTemp, Error, TEXT("EventBus: Handle not found in subscriptions for event %s"),
                *EventType.ToString());
            return false;
        }
    }
    
    // Check subscription owner counts
    TMap<TWeakObjectPtr<UObject>, int32> CalculatedCounts;
    for (const auto& SubPair : SubscriptionMap)
    {
        for (const FEventSubscription& Sub : SubPair.Value)
        {
            if (Sub.Owner.IsValid())
            {
                TWeakObjectPtr<UObject> WeakOwner = Sub.Owner;
                CalculatedCounts.FindOrAdd(WeakOwner)++;
            }
        }
    }
    
    // Проверяем соответствие подсчетов
    for (const auto& CountPair : SubscriptionCountPerOwner)
    {
        if (CountPair.Key.IsValid())
        {
            int32* CalculatedCount = CalculatedCounts.Find(CountPair.Key);
            if (!CalculatedCount || *CalculatedCount != CountPair.Value)
            {
                UObject* OwnerObj = CountPair.Key.Get();
                UE_LOG(LogTemp, Error, TEXT("EventBus: Owner subscription count mismatch for %s"),
                    OwnerObj ? *GetNameSafe(OwnerObj) : TEXT("Invalid"));
                return false;
            }
        }
    }
    
    return true;
}

int32 FSuspenseEquipmentEventBus::CleanupInvalidSubscriptions()
{
    FScopeLock Lock(&BusLock);
    
    int32 CleanedCount = 0;
    TArray<FEventSubscriptionHandle> HandlesToRemove;
    
    // Clean up subscriptions with invalid owners
    for (auto& Pair : SubscriptionMap)
    {
        TArray<FEventSubscription>& Subscriptions = Pair.Value;
        
        for (int32 i = Subscriptions.Num() - 1; i >= 0; i--)
        {
            if (Subscriptions[i].Owner.IsValid() == false && Subscriptions[i].Owner.IsExplicitlyNull() == false)
            {
                HandlesToRemove.Add(Subscriptions[i].Handle);
                Subscriptions.RemoveAt(i);
                CleanedCount++;
            }
        }
    }
    
    // Remove handle mappings
    for (const FEventSubscriptionHandle& Handle : HandlesToRemove)
    {
        HandleToEventMap.Remove(Handle);
    }
    
    // Clean up invalid owner counts
    TArray<TWeakObjectPtr<UObject>> InvalidOwners;
    for (const auto& CountPair : SubscriptionCountPerOwner)
    {
        // Проверяем валидность TWeakObjectPtr
        if (!CountPair.Key.IsValid())
        {
            InvalidOwners.Add(CountPair.Key);
        }
    }
    
    // Удаляем невалидные записи
    for (const TWeakObjectPtr<UObject>& InvalidOwner : InvalidOwners)
    {
        SubscriptionCountPerOwner.Remove(InvalidOwner);
    }
    
    Statistics.ActiveSubscriptions -= CleanedCount;
    TotalCleanedSubscriptions += CleanedCount;
    
    if (CleanedCount > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("EventBus: Cleaned up %d invalid subscriptions"),
            CleanedCount);
    }
    
    return CleanedCount;
}

void FSuspenseEquipmentEventBus::PerformAutomaticCleanup()
{
    float CurrentTime = FPlatformTime::Seconds();
    
    // Perform cleanup
    int32 CleanedCount = CleanupInvalidSubscriptions();
    
    // Update last cleanup time
    LastCleanupTime = CurrentTime;
    LastCleanupDateTime = FDateTime::Now();
    
    if (CleanedCount > 0)
    {
        UE_LOG(LogTemp, Verbose, TEXT("EventBus: Automatic cleanup removed %d invalid subscriptions"),
            CleanedCount);
    }
    
    // Also process delayed events while we're at it
    ProcessDelayedEvents();
}

void FSuspenseEquipmentEventBus::DispatchEvent(const FSuspenseEquipmentEventData& EventData, const TArray<FEventSubscription>& Subscriptions)
{
    float StartTime = FPlatformTime::Cycles();
    
    for (const FEventSubscription& Subscription : Subscriptions)
    {
        if (!Subscription.bIsActive)
        {
            continue;
        }
        
        // Check if owner is still valid
        if (Subscription.Owner.IsValid() == false && Subscription.Owner.IsExplicitlyNull() == false)
        {
            continue; // Skip invalid owners
        }
        
        // Execute handler
        ExecuteHandler(Subscription, EventData);
        
        // Update execution count
        const_cast<FEventSubscription&>(Subscription).ExecutionCount++;
    }
    
    float ElapsedMs = FPlatformTime::ToMilliseconds(FPlatformTime::Cycles() - StartTime);
    
    // Update average dispatch time
    Statistics.AverageDispatchTime = (Statistics.AverageDispatchTime * 0.9f) + (ElapsedMs * 0.1f);
}

void FSuspenseEquipmentEventBus::ExecuteHandler(const FEventSubscription& Subscription, const FSuspenseEquipmentEventData& EventData)
{
    switch (Subscription.ExecutionContext)
    {
        case EEventExecutionContext::Immediate:
            Subscription.Handler.Execute(EventData);
            break;
            
        case EEventExecutionContext::GameThread:
            if (IsInGameThread())
            {
                Subscription.Handler.Execute(EventData);
            }
            else
            {
                Async(EAsyncExecution::TaskGraphMainThread, [Subscription, EventData]()
                {
                    Subscription.Handler.Execute(EventData);
                });
            }
            break;
            
        case EEventExecutionContext::AsyncTask:
            Async(EAsyncExecution::TaskGraph, [Subscription, EventData]()
            {
                Subscription.Handler.Execute(EventData);
            });
            break;
            
        case EEventExecutionContext::NextFrame:
            // Queue for next frame processing
            QueueEvent(EventData);
            break;
    }
}

void FSuspenseEquipmentEventBus::ProcessDelayedEvents()
{
    FScopeLock Lock(&BusLock);
    
    float CurrentTime = FPlatformTime::Seconds();
    TArray<float> ProcessedTimes;
    
    for (auto& Pair : DelayedEvents)
    {
        if (Pair.Key <= CurrentTime)
        {
            for (const FSuspenseEquipmentEventData& EventData : Pair.Value)
            {
                Broadcast(EventData);
            }
            ProcessedTimes.Add(Pair.Key);
        }
    }
    
    // Remove processed events
    for (float Time : ProcessedTimes)
    {
        DelayedEvents.Remove(Time);
    }
}

void FSuspenseEquipmentEventBus::SortByPriority(TArray<FEventSubscription>& Subscriptions) const
{
    Subscriptions.Sort([](const FEventSubscription& A, const FEventSubscription& B)
    {
        return (uint8)A.Priority > (uint8)B.Priority;
    });
}

bool FSuspenseEquipmentEventBus::PassesFilter(const FGameplayTag& EventType) const
{
    const bool* FilterValue = EventFilters.Find(EventType);
    return !FilterValue || *FilterValue; // Default to allow if not filtered
}

bool FSuspenseEquipmentEventBus::IsOwnerAtSubscriptionLimit(UObject* Owner) const
{
    if (!Owner)
    {
        return false;
    }
    
    // Создаем TWeakObjectPtr для поиска в мапе
    TWeakObjectPtr<UObject> WeakOwner(Owner);
    const int32* Count = SubscriptionCountPerOwner.Find(WeakOwner);
    return Count && *Count >= MaxSubscriptionsPerOwner;
}

void FSuspenseEquipmentEventBus::UpdateOwnerSubscriptionCount(UObject* Owner, int32 Delta)
{
    if (!Owner)
    {
        return;
    }
    
    TWeakObjectPtr<UObject> WeakOwner(Owner);
    int32& Count = SubscriptionCountPerOwner.FindOrAdd(WeakOwner);
    Count += Delta;
    
    if (Count <= 0)
    {
        SubscriptionCountPerOwner.Remove(WeakOwner);
    }
}

FString FSuspenseEquipmentEventBus::FEventBusStats::ToString() const
{
    FString Stats = TEXT("=== Event Bus Statistics ===\n");
    Stats += FString::Printf(TEXT("Total Subscriptions: %d\n"), TotalSubscriptions);
    Stats += FString::Printf(TEXT("Active Subscriptions: %d\n"), ActiveSubscriptions);
    Stats += FString::Printf(TEXT("Queued Events: %d\n"), QueuedEvents);
    Stats += FString::Printf(TEXT("Total Events Dispatched: %d\n"), TotalEventsDispatched);
    Stats += FString::Printf(TEXT("Total Events Failed: %d\n"), TotalEventsFailed);
    Stats += FString::Printf(TEXT("Average Dispatch Time: %.3f ms\n"), AverageDispatchTime);
    Stats += FString::Printf(TEXT("Total Cleaned Subscriptions: %d\n"), TotalCleanedSubscriptions);
    Stats += FString::Printf(TEXT("Rejected Subscriptions: %d\n"), RejectedSubscriptions);
    
    if (EventTypeCounts.Num() > 0)
    {
        Stats += TEXT("\n--- Event Type Counts ---\n");
        for (const auto& Pair : EventTypeCounts)
        {
            Stats += FString::Printf(TEXT("%s: %d\n"),
                *Pair.Key.ToString(), Pair.Value);
        }
    }
    
    if (SubscriptionsPerOwner.Num() > 0)
    {
        Stats += TEXT("\n--- Subscriptions Per Owner ---\n");
        for (const auto& Pair : SubscriptionsPerOwner)
        {
            Stats += FString::Printf(TEXT("%s: %d\n"),
                Pair.Key ? *GetNameSafe(Pair.Key) : TEXT("Invalid"), Pair.Value);
        }
    }
    
    return Stats;
}
