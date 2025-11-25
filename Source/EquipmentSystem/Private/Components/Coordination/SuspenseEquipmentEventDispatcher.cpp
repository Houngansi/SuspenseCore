// Copyright Suspense Team. All Rights Reserved.
#include "Components/Coordination/SuspenseEquipmentEventDispatcher.h"
#include "Core/Utils/FEquipmentEventBus.h"
#include "Engine/World.h"
#include "Algo/Sort.h"
#include "Async/Async.h"

USuspenseEquipmentEventDispatcher::USuspenseEquipmentEventDispatcher()
{
	PrimaryComponentTick.bCanEverTick=true;
	TagDelta=FGameplayTag::RequestGameplayTag(TEXT("Equipment.Delta"));
	TagBatchDelta=FGameplayTag::RequestGameplayTag(TEXT("Equipment.Delta.Batch"));
	TagOperationCompleted=FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Completed"));
}

void USuspenseEquipmentEventDispatcher::BeginPlay()
{
	Super::BeginPlay();
	EventBus=FEquipmentEventBus::Get();
	WireBus();
}

void USuspenseEquipmentEventDispatcher::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnwireBus();
	{
		FScopeLock L(&QueueCs);
		LocalQueue.Empty();
	}
	{
		int32 Removed=UnsubscribeAll(nullptr);
		if(bVerbose){UE_LOG(LogTemp,Verbose,TEXT("Dispatcher EndPlay: removed %d local subs"),Removed);}
	}
	Super::EndPlay(EndPlayReason);
}

void USuspenseEquipmentEventDispatcher::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime,TickType,ThisTickFunction);
	if(!bBatchMode)return;
	Accumulator+=DeltaTime;
	if(Accumulator<FlushInterval)return;
	Accumulator=0.f;

	int32 Dispatched=0;
	while(Dispatched<MaxPerTick)
	{
		FDispatcherEquipmentEventData E;
		{
			FScopeLock L(&QueueCs);
			if(LocalQueue.Num()==0)break;
			E=MoveTemp(LocalQueue[0]);
			LocalQueue.RemoveAt(0,1,false);
			Stats.CurrentQueueSize=LocalQueue.Num();
		}
		Dispatch(E);
		++Dispatched;
	}
}

FDelegateHandle USuspenseEquipmentEventDispatcher::Subscribe(const FGameplayTag& EventType,const FEquipmentEventDelegate& Delegate)
{
	if(!EventType.IsValid()||!Delegate.IsBound())return FDelegateHandle();
	FDispatcherLocalSubscription S;
	S.Handle = FDelegateHandle(FDelegateHandle::GenerateNewHandle);
	S.Delegate=Delegate;
	S.Priority=0;
	S.Subscriber=Delegate.GetUObject();
	S.bActive=true;
	S.SubscribedAt=GetWorld()?GetWorld()->GetTimeSeconds():0.f;

	TArray<FDispatcherLocalSubscription>& Arr=LocalSubscriptions.FindOrAdd(EventType);
	Arr.Add(S);
	SortByPriority(Arr);
	HandleToTag.Add(S.Handle,EventType);
	Stats.ActiveLocalSubscriptions++;
	return S.Handle;
}

bool USuspenseEquipmentEventDispatcher::Unsubscribe(const FGameplayTag& EventType,const FDelegateHandle& Handle)
{
	if(!Handle.IsValid())return false;
	TArray<FDispatcherLocalSubscription>* Arr=LocalSubscriptions.Find(EventType);
	if(!Arr)return false;
	const int32 Before=Arr->Num();
	Arr->RemoveAll([&](const FDispatcherLocalSubscription& S){return S.Handle==Handle;});
	const int32 Removed=Before-Arr->Num();
	if(Removed>0)
	{
		HandleToTag.Remove(Handle);
		Stats.ActiveLocalSubscriptions=FMath::Max(0,Stats.ActiveLocalSubscriptions-Removed);
		return true;
	}
	return false;
}

int32 USuspenseEquipmentEventDispatcher::UnsubscribeAll(UObject* Subscriber)
{
	if (Subscriber == nullptr)
	{
		int32 Removed = 0;
		for (auto& P : LocalSubscriptions)
		{
			Removed += P.Value.Num();
		}
		LocalSubscriptions.Empty();
		HandleToTag.Empty();
		Stats.ActiveLocalSubscriptions = 0;
		return Removed;
	}

	int32 Removed = 0;
	for (auto& P : LocalSubscriptions)
	{
		Removed += P.Value.RemoveAll([Subscriber](const FDispatcherLocalSubscription& S)
		{
			return S.Subscriber.Get() == Subscriber;
		});
	}
	Stats.ActiveLocalSubscriptions = FMath::Max(0, Stats.ActiveLocalSubscriptions - Removed);

	// Перестраиваем обратную карту хендлов по актуальным подпискам
	HandleToTag.Reset();
	for (const auto& P : LocalSubscriptions)
	{
		for (const FDispatcherLocalSubscription& S : P.Value)
		{
			HandleToTag.Add(S.Handle, P.Key);
		}
	}
	return Removed;
}

void USuspenseEquipmentEventDispatcher::BroadcastEvent(const FEquipmentEventData& Event)
{
	if(!EventBus.IsValid())return;
	EventBus->Broadcast(Event);
}

void USuspenseEquipmentEventDispatcher::QueueEvent(const FEquipmentEventData& Event)
{
	if(!EventBus.IsValid())return;
	EventBus->QueueEvent(Event);
}

int32 USuspenseEquipmentEventDispatcher::ProcessEventQueue(int32 MaxEvents)
{
	if(!EventBus.IsValid())return 0;
	return EventBus->ProcessEventQueue(MaxEvents);
}

void USuspenseEquipmentEventDispatcher::ClearEventQueue(const FGameplayTag& EventType)
{
	if(!EventBus.IsValid())return;
	EventBus->ClearEventQueue(EventType);
	{
		FScopeLock L(&QueueCs);
		if(EventType.IsValid())
		{
			const int32 Before=LocalQueue.Num();
			LocalQueue.RemoveAll([&](const FDispatcherEquipmentEventData& E){return E.EventType==EventType;});
			const int32 Removed=Before-LocalQueue.Num();
			if(Removed>0){Stats.CurrentQueueSize=LocalQueue.Num();}
		}
		else
		{
			LocalQueue.Reset();
			Stats.CurrentQueueSize=0;
		}
	}
}

int32 USuspenseEquipmentEventDispatcher::GetQueuedEventCount(const FGameplayTag& EventType)const
{
	FScopeLock L(&QueueCs);
	if(!EventType.IsValid())return LocalQueue.Num();
	int32 C=0;
	for(const auto& E:LocalQueue){if(E.EventType==EventType)++C;}
	return C;
}

void USuspenseEquipmentEventDispatcher::SetEventFilter(const FGameplayTag& EventType,bool bAllow)
{
	LocalTypeEnabled.Add(EventType,bAllow);
	if(EventBus.IsValid()){EventBus->SetEventFilter(EventType,bAllow);}
}

FString USuspenseEquipmentEventDispatcher::GetEventStatistics()const
{
	FString S;
	S+=FString::Printf(TEXT("LocalSubs:%d Queue:%d Peak:%.0f Dispatched:%d AvgMs:%.2f\n"),
		Stats.ActiveLocalSubscriptions,Stats.CurrentQueueSize,Stats.PeakQueueSize,Stats.TotalEventsDispatched,Stats.AverageDispatchMs);
	if(EventBus.IsValid())
	{
		const auto BusStats=EventBus->GetStatistics().ToString();
		S+=TEXT("Bus:\n");
		S+=BusStats;
	}
	return S;
}

bool USuspenseEquipmentEventDispatcher::RegisterEventType(const FGameplayTag& EventType,const FText& Description)
{
	if(!EventType.IsValid())return false;
	if(!LocalTypeEnabled.Contains(EventType))
	{
		LocalTypeEnabled.Add(EventType,true);
		Stats.RegisteredEventTypes++;
	}
	return true;
}

void USuspenseEquipmentEventDispatcher::SetBatchModeEnabled(bool bEnabled,float FlushIntervalSec,int32 MaxPerTickIn)
{
	bBatchMode=bEnabled;
	FlushInterval=FMath::Max(0.f,FlushIntervalSec);
	MaxPerTick=FMath::Max(1,MaxPerTickIn);
}

void USuspenseEquipmentEventDispatcher::FlushBatched()
{
	if(!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread,[this](){FlushBatched();});
		return;
	}
	TArray<FDispatcherEquipmentEventData> Local;
	{
		FScopeLock L(&QueueCs);
		Local=MoveTemp(LocalQueue);
		Stats.CurrentQueueSize=0;
	}
	for(const auto& E:Local){Dispatch(E);}
}

FEventDispatcherStats USuspenseEquipmentEventDispatcher::GetStats()const
{
	return Stats;
}

void USuspenseEquipmentEventDispatcher::SetDetailedLogging(bool bEnable)
{
	bVerbose=bEnable;
}

void USuspenseEquipmentEventDispatcher::WireBus()
{
	if(!EventBus.IsValid())return;
	BusDelta=EventBus->Subscribe(TagDelta,FEventHandlerDelegate::CreateUObject(this,&USuspenseEquipmentEventDispatcher::OnBusEvent_Delta));
	BusBatchDelta=EventBus->Subscribe(TagBatchDelta,FEventHandlerDelegate::CreateUObject(this,&USuspenseEquipmentEventDispatcher::OnBusEvent_BatchDelta));
	BusOpCompleted=EventBus->Subscribe(TagOperationCompleted,FEventHandlerDelegate::CreateUObject(this,&USuspenseEquipmentEventDispatcher::OnBusEvent_OperationCompleted));
}

void USuspenseEquipmentEventDispatcher::UnwireBus()
{
	if(!EventBus.IsValid())return;
	if(BusDelta.IsValid())EventBus->Unsubscribe(BusDelta);
	if(BusBatchDelta.IsValid())EventBus->Unsubscribe(BusBatchDelta);
	if(BusOpCompleted.IsValid())EventBus->Unsubscribe(BusOpCompleted);
	BusDelta.Invalidate();
	BusBatchDelta.Invalidate();
	BusOpCompleted.Invalidate();
}

void USuspenseEquipmentEventDispatcher::OnBusEvent_Delta(const FEquipmentEventData& E)
{
	const FDispatcherEquipmentEventData D=ToDispatcherPayload(E);
	if(bBatchMode){Enqueue(D);}else{Dispatch(D);}
}

void USuspenseEquipmentEventDispatcher::OnBusEvent_BatchDelta(const FEquipmentEventData& E)
{
	const FDispatcherEquipmentEventData D=ToDispatcherPayload(E);
	Enqueue(D);
}

void USuspenseEquipmentEventDispatcher::OnBusEvent_OperationCompleted(const FEquipmentEventData& E)
{
	const FDispatcherEquipmentEventData D=ToDispatcherPayload(E);
	if(bBatchMode){Enqueue(D);}else{Dispatch(D);}
}

void USuspenseEquipmentEventDispatcher::Enqueue(const FDispatcherEquipmentEventData& E)
{
	FScopeLock L(&QueueCs);
	LocalQueue.Add(E);
	Stats.TotalEventsQueued++;
	Stats.CurrentQueueSize=LocalQueue.Num();
	Stats.PeakQueueSize=FMath::Max(Stats.PeakQueueSize,(float)LocalQueue.Num());
}

void USuspenseEquipmentEventDispatcher::Dispatch(const FDispatcherEquipmentEventData& E)
{
	if(!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread,[this,E](){Dispatch(E);});
		return;
	}
	const double T0=FPlatformTime::Seconds();
	DispatchToLocal(E.EventType,E);
	const double Ms=(FPlatformTime::Seconds()-T0)*1000.0;
	Stats.TotalEventsDispatched++;
	const float Alpha=0.01f;
	Stats.AverageDispatchMs=(1.f-Alpha)*Stats.AverageDispatchMs+Alpha*(float)Ms;
	if(bVerbose){UE_LOG(LogTemp,Verbose,TEXT("Dispatch %s in %.2f ms"),*E.EventType.ToString(),(float)Ms);}
}

void USuspenseEquipmentEventDispatcher::DispatchToLocal(const FGameplayTag& Type,const FDispatcherEquipmentEventData& E)
{
	TArray<FDispatcherLocalSubscription> Copy;
	{
		TArray<FDispatcherLocalSubscription>* Arr=LocalSubscriptions.Find(Type);
		if(!Arr)return;
		Copy=*Arr;
	}
	for(FDispatcherLocalSubscription& S:Copy)
	{
		if(!S.bActive)continue;
		if(S.Subscriber.IsValid()&&!IsValid(S.Subscriber.Get()))continue;
		S.Delegate.Execute(E);
	}
}

void USuspenseEquipmentEventDispatcher::SortByPriority(TArray<FDispatcherLocalSubscription>& Arr)
{
	Arr.Sort([](const FDispatcherLocalSubscription& A,const FDispatcherLocalSubscription& B){return A.Priority>B.Priority;});
}

FDispatcherEquipmentEventData USuspenseEquipmentEventDispatcher::ToDispatcherPayload(const FEquipmentEventData& In)
{
	FDispatcherEquipmentEventData Out;
	Out.EventType=In.EventType;
	Out.Source=In.Source.Get();
	Out.EventPayload=In.Payload;
	Out.Timestamp=In.Timestamp;
	Out.Priority=(int32)In.Priority;
	Out.Metadata=In.Metadata;
	return Out;
}

int32 USuspenseEquipmentEventDispatcher::CleanupInvalid()
{
	int32 Removed=0;
	for(auto& P:LocalSubscriptions)
	{
		Removed+=P.Value.RemoveAll([](const FDispatcherLocalSubscription& S)
		{
			return !S.Delegate.IsBound()||(S.Subscriber.IsValid()&&!IsValid(S.Subscriber.Get()));
		});
	}
	Stats.ActiveLocalSubscriptions=FMath::Max(0,Stats.ActiveLocalSubscriptions-Removed);
	return Removed;
}
