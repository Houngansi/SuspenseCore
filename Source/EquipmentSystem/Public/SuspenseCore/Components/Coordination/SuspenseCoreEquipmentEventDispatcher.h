// Copyright Suspense Team. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/Utils/SuspenseEquipmentEventBus.h"
#include "GameplayTagContainer.h"
#include "Interfaces/Equipment/ISuspenseEventDispatcher.h"
#include "SuspenseCoreEquipmentEventDispatcher.generated.h"

struct FEventSubscriptionHandle;
class FSuspenseCoreEquipmentEventBus;

/** Внутренняя запись локальной подписки диспетчера */
struct FDispatcherLocalSubscription
{
	FDelegateHandle Handle;
	FEquipmentEventDelegate Delegate;
	TWeakObjectPtr<UObject> Subscriber;
	int32 Priority=0;
	bool bActive=true;
	int32 DispatchCount=0;
	float SubscribedAt=0.f;
};

/** Простейшая метрика диспетчера */
USTRUCT(BlueprintType)
struct FSuspenseCoreEventDispatcherStats
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere)int32 TotalEventsDispatched=0;
	UPROPERTY(VisibleAnywhere)int32 TotalEventsQueued=0;
	UPROPERTY(VisibleAnywhere)int32 ActiveLocalSubscriptions=0;
	UPROPERTY(VisibleAnywhere)int32 RegisteredEventTypes=0;
	UPROPERTY(VisibleAnywhere)int32 CurrentQueueSize=0;
	UPROPERTY(VisibleAnywhere)float AverageDispatchMs=0.f;
	UPROPERTY(VisibleAnywhere)float PeakQueueSize=0.f;
};

UCLASS(ClassGroup=(MedCom),meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentEventDispatcher:public UActorComponent,public ISuspenseEventDispatcher
{
	GENERATED_BODY()
public:
	USuspenseCoreEquipmentEventDispatcher();

	// lifecycle
	virtual void BeginPlay()override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason)override;
	virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)override;

	// ISuspenseEventDispatcher
	virtual FDelegateHandle Subscribe(const FGameplayTag& EventType,const FEquipmentEventDelegate& Delegate)override;
	virtual bool Unsubscribe(const FGameplayTag& EventType,const FDelegateHandle& Handle)override;
	virtual void BroadcastEvent(const FSuspenseEquipmentEventData& Event)override;
	virtual void QueueEvent(const FSuspenseEquipmentEventData& Event)override;
	virtual int32 ProcessEventQueue(int32 MaxEvents=-1)override;
	virtual void ClearEventQueue(const FGameplayTag& EventType=FGameplayTag())override;
	virtual int32 GetQueuedEventCount(const FGameplayTag& EventType=FGameplayTag())const override;
	virtual void SetEventFilter(const FGameplayTag& EventType,bool bAllow)override;
	virtual FString GetEventStatistics()const override;
	virtual bool RegisterEventType(const FGameplayTag& EventType,const FText& Description)override;

	// расширения (локальные)
	void SetBatchModeEnabled(bool bEnabled,float FlushIntervalSec=0.02f,int32 MaxPerTick=256);
	void FlushBatched();
	FSuspenseCoreEventDispatcherStats GetStats()const;
	int32 UnsubscribeAll(UObject* Subscriber);
	void SetDetailedLogging(bool bEnable);

private:
	// шина
	TSharedPtr<FSuspenseCoreEquipmentEventBus> EventBus;
	FEventSubscriptionHandle BusDelta;
	FEventSubscriptionHandle BusBatchDelta;
	FEventSubscriptionHandle BusOpCompleted;

	// локальные подписки диспетчера
	TMap<FGameplayTag,TArray<FDispatcherLocalSubscription>> LocalSubscriptions;
	TMap<FDelegateHandle,FGameplayTag> HandleToTag;

	// конфиг обработки очереди (локальной)
	bool bBatchMode=true;
	float FlushInterval=0.02f;
	int32 MaxPerTick=256;
	float Accumulator=0.f;

	// очередь локальная (коалесинг для UI/инструментов)
	TArray<FDispatcherEquipmentEventData> LocalQueue;
	mutable FCriticalSection QueueCs;

	// статистика
	FSuspenseCoreEventDispatcherStats Stats;
	double EMA_AvgMs=0.0;

	// фильтры типов (просто прослойка к EventBus)
	TMap<FGameplayTag,bool> LocalTypeEnabled;

	// логирование
	bool bVerbose=false;

	// теги, которые слушаем из EventBus
	UPROPERTY(EditDefaultsOnly,Category="Tags")FGameplayTag TagDelta;
	UPROPERTY(EditDefaultsOnly,Category="Tags")FGameplayTag TagBatchDelta;
	UPROPERTY(EditDefaultsOnly,Category="Tags")FGameplayTag TagOperationCompleted;

private:
	// wiring с EventBus
	void WireBus();
	void UnwireBus();
	void OnBusEvent_Delta(const FSuspenseEquipmentEventData& E);
	void OnBusEvent_BatchDelta(const FSuspenseEquipmentEventData& E);
	void OnBusEvent_OperationCompleted(const FSuspenseEquipmentEventData& E);

	// доставка локальным подписчикам
	void Enqueue(const FDispatcherEquipmentEventData& E);
	void Dispatch(const FDispatcherEquipmentEventData& E);
	void DispatchToLocal(const FGameplayTag& Type,const FDispatcherEquipmentEventData& E);
	static void SortByPriority(TArray<FDispatcherLocalSubscription>& Arr);

	// преобразование payload
	static FDispatcherEquipmentEventData ToDispatcherPayload(const FSuspenseEquipmentEventData& In);

	// служебное
	int32 CleanupInvalid();
};
