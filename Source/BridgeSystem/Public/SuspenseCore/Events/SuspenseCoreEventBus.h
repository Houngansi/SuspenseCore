// SuspenseCoreEventBus.h
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCoreEventBus.generated.h"

/**
 * FSuspenseCoreSubscription
 *
 * Internal subscription structure holding callback info and filtering.
 * Sorted by Priority (System=0 first, Lowest=200 last).
 */
USTRUCT()
struct FSuspenseCoreSubscription
{
	GENERATED_BODY()

	/** Unique subscription ID for handle management */
	uint64 Id = 0;

	/** Subscriber object (weak ref for automatic cleanup) */
	TWeakObjectPtr<UObject> Subscriber;

	/** Execution priority (lower value = higher priority) */
	ESuspenseCoreEventPriority Priority = ESuspenseCoreEventPriority::Normal;

	/** Optional source filter - only receive events from this source */
	TWeakObjectPtr<UObject> SourceFilter;

	/** Native C++ callback (faster, no reflection) */
	FSuspenseCoreNativeEventCallback NativeCallback;

	/** Dynamic callback (Blueprint-compatible) */
	FSuspenseCoreEventCallback DynamicCallback;

	/** True if using NativeCallback, false for DynamicCallback */
	bool bUseNativeCallback = false;

	/** Check if subscription is still valid */
	bool IsValid() const
	{
		return Id != 0 && Subscriber.IsValid();
	}

	/** Comparison for priority sorting (lower priority value = first in execution order) */
	bool operator<(const FSuspenseCoreSubscription& Other) const
	{
		return static_cast<uint8>(Priority) < static_cast<uint8>(Other.Priority);
	}
};

/**
 * USuspenseCoreEventBus
 *
 * Central event bus for SuspenseCore. All modules communicate ONLY through this.
 * Part of Clean Architecture Foundation.
 *
 * Key Features:
 * - GameplayTag-based event identification (hierarchical, Blueprint-friendly)
 * - Priority-based handler execution (System > High > Normal > Low > Lowest)
 * - Source filtering (receive events only from specific objects)
 * - Thread-safe operations (copy-then-notify pattern to avoid deadlocks)
 * - Deferred events (processed at end of frame via EventManager)
 * - Child tag subscription (subscribe to parent, receive all children)
 *
 * Thread Safety:
 * - All public methods are thread-safe
 * - Uses copy-then-notify: subscribers are copied under lock, then notified without lock
 * - Subscriptions are sorted once on add, not on every publish
 *
 * Performance Optimizations:
 * - O(1) subscription lookup via TMap<FGameplayTag, TArray<...>>
 * - Subscriptions sorted by priority at registration time
 * - Native C++ callbacks avoid reflection overhead
 *
 * Usage:
 *   // Subscribe
 *   Handle = EventBus->SubscribeNative(MyTag, this,
 *       FSuspenseCoreNativeEventCallback::CreateUObject(this, &UMyClass::OnEvent));
 *
 *   // Publish
 *   FSuspenseCoreEventData Data = FSuspenseCoreEventData::Create(this);
 *   Data.SetFloat(TEXT("Damage"), 50.0f);
 *   EventBus->Publish(MyTag, Data);
 *
 *   // Unsubscribe
 *   EventBus->Unsubscribe(Handle);
 */
UCLASS(BlueprintType)
class BRIDGESYSTEM_API USuspenseCoreEventBus : public UObject
{
	GENERATED_BODY()

public:
	USuspenseCoreEventBus();

	// ═══════════════════════════════════════════════════════════════════════════
	// ПУБЛИКАЦИЯ СОБЫТИЙ
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Публикует событие немедленно.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
	void Publish(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/**
	 * Публикует событие отложенно (в конце кадра).
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
	void PublishDeferred(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/**
	 * C++ хелпер: быстрая публикация
	 */
	void PublishSimple(FGameplayTag EventTag, UObject* Source);

	/**
	 * Async publish - dispatches to game thread from any thread.
	 * Safe to call from background threads for non-critical events.
	 * Useful for: analytics, logging, state updates that don't need immediate response.
	 */
	void PublishAsync(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/**
	 * Batch publish multiple events asynchronously.
	 * More efficient than multiple individual PublishAsync calls.
	 */
	void PublishBatchAsync(const TArray<TPair<FGameplayTag, FSuspenseCoreEventData>>& Events);

	// ═══════════════════════════════════════════════════════════════════════════
	// ПОДПИСКА (Blueprint)
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Подписка на событие (Blueprint).
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
	FSuspenseCoreSubscriptionHandle Subscribe(
		FGameplayTag EventTag,
		const FSuspenseCoreEventCallback& Callback
	);

	/**
	 * Подписка на группу событий по родительскому тегу.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
	FSuspenseCoreSubscriptionHandle SubscribeToChildren(
		FGameplayTag ParentTag,
		const FSuspenseCoreEventCallback& Callback
	);

	/**
	 * Подписка с фильтром по источнику.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
	FSuspenseCoreSubscriptionHandle SubscribeWithFilter(
		FGameplayTag EventTag,
		const FSuspenseCoreEventCallback& Callback,
		UObject* SourceFilter
	);

	// ═══════════════════════════════════════════════════════════════════════════
	// ПОДПИСКА (C++ Native - более эффективная)
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Native подписка (C++).
	 */
	FSuspenseCoreSubscriptionHandle SubscribeNative(
		FGameplayTag EventTag,
		UObject* Subscriber,
		FSuspenseCoreNativeEventCallback Callback,
		ESuspenseCoreEventPriority Priority = ESuspenseCoreEventPriority::Normal
	);

	/**
	 * Native подписка с фильтром.
	 */
	FSuspenseCoreSubscriptionHandle SubscribeNativeWithFilter(
		FGameplayTag EventTag,
		UObject* Subscriber,
		FSuspenseCoreNativeEventCallback Callback,
		UObject* SourceFilter,
		ESuspenseCoreEventPriority Priority = ESuspenseCoreEventPriority::Normal
	);

	// ═══════════════════════════════════════════════════════════════════════════
	// ОТПИСКА
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Отписка по handle.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
	void Unsubscribe(FSuspenseCoreSubscriptionHandle Handle);

	/**
	 * Отписка всех подписок объекта.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
	void UnsubscribeAll(UObject* Subscriber);

	// ═══════════════════════════════════════════════════════════════════════════
	// УТИЛИТЫ
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Обработка отложенных событий.
	 * Вызывается EventManager каждый кадр.
	 */
	void ProcessDeferredEvents();

	/**
	 * Очистка невалидных подписок.
	 */
	void CleanupStaleSubscriptions();

	/**
	 * Получить статистику.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events|Debug")
	FSuspenseCoreEventBusStats GetStats() const;

	/**
	 * Есть ли подписчики на тег.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
	bool HasSubscribers(FGameplayTag EventTag) const;

protected:
	/** Карта подписок: Tag -> Array of Subscribers */
	TMap<FGameplayTag, TArray<FSuspenseCoreSubscription>> Subscriptions;

	/** Подписки на дочерние теги */
	TMap<FGameplayTag, TArray<FSuspenseCoreSubscription>> ChildSubscriptions;

	/** Очередь отложенных событий */
	TArray<FSuspenseCoreQueuedEvent> DeferredEvents;

	/** Счётчик для генерации уникальных handle */
	uint64 NextSubscriptionId = 1;

	/** Статистика */
	int64 TotalEventsPublished = 0;

	/** Критическая секция для thread-safety */
	mutable FCriticalSection SubscriptionLock;

private:
	/**
	 * Внутренний метод публикации.
	 */
	void PublishInternal(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/**
	 * Создать подписку.
	 */
	FSuspenseCoreSubscriptionHandle CreateSubscription(
		FGameplayTag EventTag,
		UObject* Subscriber,
		FSuspenseCoreNativeEventCallback NativeCallback,
		FSuspenseCoreEventCallback DynamicCallback,
		UObject* SourceFilter,
		ESuspenseCoreEventPriority Priority,
		bool bSubscribeToChildren
	);

	/**
	 * Уведомить подписчиков.
	 */
	void NotifySubscribers(
		const TArray<FSuspenseCoreSubscription>& Subs,
		FGameplayTag EventTag,
		const FSuspenseCoreEventData& EventData
	);

	/**
	 * Сортировка по приоритету.
	 */
	void SortSubscriptionsByPriority(TArray<FSuspenseCoreSubscription>& Subs);
};
