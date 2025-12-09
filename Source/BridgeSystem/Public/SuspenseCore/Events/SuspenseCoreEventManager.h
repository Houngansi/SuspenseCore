// SuspenseCoreEventManager.h
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCoreEventManager.generated.h"

class USuspenseCoreEventBus;
class USuspenseCoreServiceLocator;

/**
 * USuspenseCoreEventManager
 *
 * Главный менеджер — точка входа в систему SuspenseCore.
 * GameInstanceSubsystem — живёт всё время работы игры.
 *
 * Владеет:
 * - EventBus (шина событий)
 * - ServiceLocator (DI контейнер)
 *
 * Использование:
 *   auto* Manager = USuspenseCoreEventManager::Get(WorldContext);
 *   Manager->GetEventBus()->Publish(...);
 *   Manager->GetServiceLocator()->RegisterService(...);
 *
 * Это НОВЫЙ класс, написанный С НУЛЯ.
 */
UCLASS()
class BRIDGESYSTEM_API USuspenseCoreEventManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ═══════════════════════════════════════════════════════════════════════════
	// USubsystem Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// ═══════════════════════════════════════════════════════════════════════════
	// СТАТИЧЕСКИЙ ДОСТУП
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Получить EventManager из любого места.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore", meta = (WorldContext = "WorldContextObject"))
	static USuspenseCoreEventManager* Get(const UObject* WorldContextObject);

	// ═══════════════════════════════════════════════════════════════════════════
	// ДОСТУП К ПОДСИСТЕМАМ
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * EventBus для публикации/подписки.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
	USuspenseCoreEventBus* GetEventBus() const { return EventBus; }

	/**
	 * ServiceLocator для DI.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Services")
	USuspenseCoreServiceLocator* GetServiceLocator() const { return ServiceLocator; }

	// ═══════════════════════════════════════════════════════════════════════════
	// БЫСТРЫЕ ХЕЛПЕРЫ
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Быстрая публикация события.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
	void PublishEvent(FGameplayTag EventTag, UObject* Source);

	/**
	 * Публикация с данными.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
	void PublishEventWithData(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/**
	 * Быстрая подписка.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
	FSuspenseCoreSubscriptionHandle SubscribeToEvent(
		FGameplayTag EventTag,
		const FSuspenseCoreEventCallback& Callback
	);

	/**
	 * Быстрая отписка.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Events")
	void UnsubscribeFromEvent(FSuspenseCoreSubscriptionHandle Handle);

	// ═══════════════════════════════════════════════════════════════════════════
	// DEBUG
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Получить статистику EventBus.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Debug")
	FSuspenseCoreEventBusStats GetEventBusStats() const;

	/**
	 * Включить/выключить логирование событий.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Debug")
	void SetEventLogging(bool bEnabled);

protected:
	/** EventBus instance */
	UPROPERTY()
	TObjectPtr<USuspenseCoreEventBus> EventBus;

	/** ServiceLocator instance */
	UPROPERTY()
	TObjectPtr<USuspenseCoreServiceLocator> ServiceLocator;

	/** Логирование событий */
	bool bLogEvents = false;

	/** Handle для Tick */
	FTSTicker::FDelegateHandle TickDelegateHandle;

	/** Timer for periodic cleanup of stale subscriptions (thread-safe as class member) */
	float CleanupTimer = 0.0f;

	/** Cleanup interval in seconds */
	static constexpr float CleanupIntervalSeconds = 10.0f;

private:
	/**
	 * Создание подсистем.
	 */
	void CreateSubsystems();

	/**
	 * Tick callback для обработки deferred событий.
	 */
	bool Tick(float DeltaTime);

	/**
	 * Публикация системного события инициализации.
	 */
	void PublishSystemInitialized();
};
