// Copyright Suspense Team. All Rights Reserved.
#ifndef SUSPENSECORE_CORE_UTILS_SUSPENSECOREEQUIPMENTEVENTBUS_H
#define SUSPENSECORE_CORE_UTILS_SUSPENSECOREEQUIPMENTEVENTBUS_H
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Templates/SharedPointer.h"
#include "HAL/CriticalSection.h"
#include "Async/TaskGraphInterfaces.h"
#include "Engine/World.h"
#include "TimerManager.h"

/**
 * Event priority levels
 */
enum class EEventPriority:uint8
{
    Lowest=0,
    Low=50,
    Normal=100,
    High=150,
    Highest=200,
    System=255
};

/**
 * Event execution context
 */
enum class EEventExecutionContext:uint8
{
    Immediate,
    GameThread,
    AsyncTask,
    NextFrame
};

/**
 * Base event data structure
 * Enhanced with metadata support for structured data passing
 */
struct BRIDGESYSTEM_API FSuspenseEquipmentEventData
{
    FGameplayTag EventType;
    TWeakObjectPtr<UObject> Source;
    TWeakObjectPtr<UObject> Target;
    FString Payload;
    TMap<FString,FString> Metadata;
    float Timestamp;
    EEventPriority Priority;
    float NumericData;
    uint32 Flags;

    FSuspenseEquipmentEventData()
        : Timestamp(0.0f)
        , Priority(EEventPriority::Normal)
        , NumericData(0.0f)
        , Flags(0)
    {}

    virtual ~FSuspenseEquipmentEventData()=default;

    template<typename T>
    T* GetSourceAs() const { return Cast<T>(Source.Get()); }

    template<typename T>
    T* GetTargetAs() const { return Cast<T>(Target.Get()); }

    void AddMetadata(const FString& Key,const FString& Value)
    {
        Metadata.Add(Key,Value);
    }

    FString GetMetadata(const FString& Key,const FString& DefaultValue=TEXT("")) const
    {
        const FString* Value=Metadata.Find(Key);
        return Value?*Value:DefaultValue;
    }

    bool HasMetadata(const FString& Key) const
    {
        return Metadata.Contains(Key);
    }

    void SetFlag(uint8 FlagBit)
    {
        if(FlagBit<32){ Flags|=(1<<FlagBit); }
    }

    void ClearFlag(uint8 FlagBit)
    {
        if(FlagBit<32){ Flags&=~(1<<FlagBit); }
    }

    bool HasFlag(uint8 FlagBit) const
    {
        if(FlagBit<32){ return (Flags&(1<<FlagBit))!=0; }
        return false;
    }

    FSuspenseEquipmentEventData Clone() const
    {
        FSuspenseEquipmentEventData Copy;
        Copy.EventType=EventType;
        Copy.Source=Source;
        Copy.Target=Target;
        Copy.Payload=Payload;
        Copy.Metadata=Metadata;
        Copy.Timestamp=Timestamp;
        Copy.Priority=Priority;
        Copy.NumericData=NumericData;
        Copy.Flags=Flags;
        return Copy;
    }

    FString ToString() const
    {
        const UObject* SrcObj=Source.Get();
        const UObject* TgtObj=Target.Get();
        const FString SrcName=SrcObj?SrcObj->GetName():TEXT("None");
        const FString TgtName=TgtObj?TgtObj->GetName():TEXT("None");
        return FString::Printf(
            TEXT("Event[%s]: Source=%s, Target=%s, Payload=%s, Metadata=%d entries, Time=%.2f"),
            *EventType.ToString(),
            *SrcName,
            *TgtName,
            *Payload,
            Metadata.Num(),
            Timestamp
        );
    }
};

/**
 * Event handler delegate
 */
DECLARE_DELEGATE_OneParam(FEventHandlerDelegate,const FSuspenseEquipmentEventData&);

struct FEventSubscriptionHandle
{
    FGuid SubscriptionId;
    FEventSubscriptionHandle(){ SubscriptionId=FGuid::NewGuid(); }
    bool IsValid() const { return SubscriptionId.IsValid(); }
    void Invalidate(){ SubscriptionId.Invalidate(); }
    bool operator==(const FEventSubscriptionHandle& Other) const { return SubscriptionId==Other.SubscriptionId; }
    friend uint32 GetTypeHash(const FEventSubscriptionHandle& Handle){ return GetTypeHash(Handle.SubscriptionId); }
};

struct FEventSubscription
{
    FEventSubscriptionHandle Handle;
    FEventHandlerDelegate Handler;
    EEventPriority Priority;
    EEventExecutionContext ExecutionContext;
    TWeakObjectPtr<UObject> Owner;
    FGameplayTagContainer EventFilter;
    bool bIsActive;
    float SubscriptionTime;
    int32 ExecutionCount;

    FEventSubscription()
        : Priority(EEventPriority::Normal)
        , ExecutionContext(EEventExecutionContext::Immediate)
        , bIsActive(true)
        , SubscriptionTime(0.0f)
        , ExecutionCount(0)
    {}
};

/**
 * Centralized event bus for equipment system with automatic cleanup
 */
class BRIDGESYSTEM_API FSuspenseEquipmentEventBus:public TSharedFromThis<FSuspenseEquipmentEventBus>
{
public:
    FSuspenseEquipmentEventBus();
    ~FSuspenseEquipmentEventBus();

    static TSharedPtr<FSuspenseEquipmentEventBus> Get()
    {
        static TSharedPtr<FSuspenseEquipmentEventBus> Instance;
        if(!Instance.IsValid())
        {
            Instance=MakeShareable(new FSuspenseEquipmentEventBus());
            Instance->InitializeAutomaticCleanup();
        }
        return Instance;
    }

    FEventSubscriptionHandle Subscribe(const FGameplayTag& EventType,const FEventHandlerDelegate& Handler,EEventPriority Priority=EEventPriority::Normal,EEventExecutionContext Context=EEventExecutionContext::Immediate,UObject* Owner=nullptr);
    TArray<FEventSubscriptionHandle> SubscribeMultiple(const FGameplayTagContainer& EventTypes,const FEventHandlerDelegate& Handler,EEventPriority Priority=EEventPriority::Normal,EEventExecutionContext Context=EEventExecutionContext::Immediate,UObject* Owner=nullptr);
    bool Unsubscribe(const FEventSubscriptionHandle& Handle);
    int32 UnsubscribeAll(UObject* Owner);
    void Broadcast(const FSuspenseEquipmentEventData& EventData);
    void BroadcastDelayed(const FSuspenseEquipmentEventData& EventData,float Delay);
    void QueueEvent(const FSuspenseEquipmentEventData& EventData);
    int32 ProcessEventQueue(int32 MaxEvents=-1);
    void ClearEventQueue(const FGameplayTag& EventType=FGameplayTag());
    void SetEventFilter(const FGameplayTag& EventType,bool bAllow);
    bool SetSubscriptionEnabled(const FEventSubscriptionHandle& Handle,bool bEnabled);

    struct FEventBusStats
    {
        int32 TotalSubscriptions=0;
        int32 ActiveSubscriptions=0;
        int32 QueuedEvents=0;
        int32 TotalEventsDispatched=0;
        int32 TotalEventsFailed=0;
        float AverageDispatchTime=0.f;
        int32 TotalCleanedSubscriptions=0;
        int32 RejectedSubscriptions=0;
        TMap<FGameplayTag,int32> EventTypeCounts;
        TMap<UObject*,int32> SubscriptionsPerOwner;

        BRIDGESYSTEM_API FString ToString() const;
    };
    
    FEventBusStats GetStatistics() const;
    void ResetStatistics();
    bool ValidateBusIntegrity() const;
    int32 CleanupInvalidSubscriptions();
    void SetMaxSubscriptionsPerOwner(int32 MaxCount){ MaxSubscriptionsPerOwner=MaxCount; }
    void SetCleanupInterval(float Interval){ CleanupInterval=Interval; }

private:
    void InitializeAutomaticCleanup();
    void PerformAutomaticCleanup();
    void DispatchEvent(const FSuspenseEquipmentEventData& EventData,const TArray<FEventSubscription>& Subscriptions);
    void ExecuteHandler(const FEventSubscription& Subscription,const FSuspenseEquipmentEventData& EventData);
    void ProcessDelayedEvents();
    void SortByPriority(TArray<FEventSubscription>& Subscriptions) const;
    bool PassesFilter(const FGameplayTag& EventType) const;
    bool IsOwnerAtSubscriptionLimit(UObject* Owner) const;
    void UpdateOwnerSubscriptionCount(UObject* Owner,int32 Delta);

    TMap<FGameplayTag,TArray<FEventSubscription>> SubscriptionMap;
    TMap<FEventSubscriptionHandle,FGameplayTag> HandleToEventMap;

    TArray<FSuspenseEquipmentEventData> EventQueueArray;
    FCriticalSection EventQueueLock;
    int32 EventQueueHead=0;
    int32 EventQueueTail=0;
    int32 EventQueueCount=0;

    TMap<float,TArray<FSuspenseEquipmentEventData>> DelayedEvents;
    TMap<FGameplayTag,bool> EventFilters;

    mutable FEventBusStats Statistics;
    mutable FCriticalSection BusLock;

    FTimerHandle DelayedEventTimerHandle;
    FTimerHandle CleanupTimerHandle;

    bool bProcessingQueue=false;
    int32 MaxQueueSize=1024;
    float DelayedEventCheckInterval=0.05f;

    float CleanupInterval=10.0f;
    float LastCleanupTime=0.0f;

    TMap<TWeakObjectPtr<UObject>,int32> SubscriptionCountPerOwner;
    int32 MaxSubscriptionsPerOwner=128;

    int32 TotalCleanedSubscriptions=0;
    int32 RejectedSubscriptions=0;
    FDateTime LastCleanupDateTime;
};

/**
 * RAII event subscription manager
 */
class BRIDGESYSTEM_API FEventSubscriptionScope
{
public:
    FEventSubscriptionScope(TSharedPtr<FSuspenseEquipmentEventBus> InBus=nullptr)
        : EventBus(InBus?InBus:FSuspenseEquipmentEventBus::Get())
    {}

    ~FEventSubscriptionScope(){ UnsubscribeAll(); }

    FEventSubscriptionHandle Subscribe(const FGameplayTag& EventType,const FEventHandlerDelegate& Handler,EEventPriority Priority=EEventPriority::Normal)
    {
        if(EventBus.IsValid())
        {
            FEventSubscriptionHandle Handle=EventBus->Subscribe(EventType,Handler,Priority);
            Subscriptions.Add(Handle);
            return Handle;
        }
        return FEventSubscriptionHandle();
    }

    void UnsubscribeAll()
    {
        if(EventBus.IsValid())
        {
            for(const FEventSubscriptionHandle& Handle:Subscriptions)
            {
                EventBus->Unsubscribe(Handle);
            }
        }
        Subscriptions.Empty();
    }

private:
    TSharedPtr<FSuspenseEquipmentEventBus> EventBus;
    TArray<FEventSubscriptionHandle> Subscriptions;
};

#define BROADCAST_EQUIPMENT_EVENT(EventTag,SourceObj,TargetObj,PayloadStr) \
    { \
        FSuspenseEquipmentEventData _EventData; \
        _EventData.EventType=EventTag; \
        _EventData.Source=SourceObj; \
        _EventData.Target=TargetObj; \
        _EventData.Payload=PayloadStr; \
        _EventData.Timestamp=FPlatformTime::Seconds(); \
        FSuspenseEquipmentEventBus::Get()->Broadcast(_EventData); \
    }

#define BROADCAST_EQUIPMENT_EVENT_WITH_META(EventTag,SourceObj,TargetObj,PayloadStr,MetaKey,MetaValue) \
    { \
        FSuspenseEquipmentEventData _EventData; \
        _EventData.EventType=EventTag; \
        _EventData.Source=SourceObj; \
        _EventData.Target=TargetObj; \
        _EventData.Payload=PayloadStr; \
        _EventData.AddMetadata(MetaKey,MetaValue); \
        _EventData.Timestamp=FPlatformTime::Seconds(); \
        FSuspenseEquipmentEventBus::Get()->Broadcast(_EventData); \
    }

#define QUEUE_EQUIPMENT_EVENT(EventTag,SourceObj,PayloadStr) \
    { \
        FSuspenseEquipmentEventData _EventData; \
        _EventData.EventType=EventTag; \
        _EventData.Source=SourceObj; \
        _EventData.Payload=PayloadStr; \
        _EventData.Timestamp=FPlatformTime::Seconds(); \
        FSuspenseEquipmentEventBus::Get()->QueueEvent(_EventData); \
    }

#define QUEUE_EQUIPMENT_EVENT_WITH_META(EventTag,SourceObj,PayloadStr,MetaKey,MetaValue) \
    { \
        FSuspenseEquipmentEventData _EventData; \
        _EventData.EventType=EventTag; \
        _EventData.Source=SourceObj; \
        _EventData.Payload=PayloadStr; \
        _EventData.AddMetadata(MetaKey,MetaValue); \
        _EventData.Timestamp=FPlatformTime::Seconds(); \
        FSuspenseEquipmentEventBus::Get()->QueueEvent(_EventData); \
    }


#endif // SUSPENSECORE_CORE_UTILS_SUSPENSECOREEQUIPMENTEVENTBUS_H