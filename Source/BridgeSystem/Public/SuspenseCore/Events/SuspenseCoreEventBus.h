// SuspenseCoreEventBus.h
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/SuspenseCoreTypes.h"
#include "SuspenseCoreEventBus.generated.h"

/**
 * FSuspenseCoreSubscription
 *
 * Внутренняя структура подписки.
 */
USTRUCT()
struct FSuspenseCoreSubscription
{
	GENERATED_BODY()

	uint64 Id = 0;
	TWeakObjectPtr<UObject> Subscriber;
	ESuspenseCoreEventPriority Priority = ESuspenseCoreEventPriority::Normal;
	TWeakObjectPtr<UObject> SourceFilter;
	FSuspenseCoreNativeEventCallback NativeCallback;
	FSuspenseCoreEventCallback DynamicCallback;
	bool bUseNativeCallback = false;

	bool IsValid() const
	{
		return Id != 0 && Subscriber.IsValid();
	}
};

/**
 * USuspenseCoreEventBus
 *
 * Центральная шина событий. Все модули общаются ТОЛЬКО через неё.
 *
 * Ключевые особенности:
 * - События идентифицируются через GameplayTags
 * - Поддержка приоритетов обработки
 * - Фильтрация по источнику события
 * - Thread-safe операции
 * - Отложенные события (deferred)
 *
 * Это НОВЫЙ класс, написанный С НУЛЯ.
 * Legacy: SuspenseEquipmentEventBus (не трогаем)
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
