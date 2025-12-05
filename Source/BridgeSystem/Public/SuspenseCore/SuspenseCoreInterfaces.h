// SuspenseCoreInterfaces.h
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Data/SuspenseCorePlayerData.h"
#include "SuspenseCoreInterfaces.generated.h"

// Forward declarations
class USuspenseCoreEventBus;

// ═══════════════════════════════════════════════════════════════════════════════
// ISuspenseCoreEventSubscriber
// ═══════════════════════════════════════════════════════════════════════════════

UINTERFACE(MinimalAPI, Blueprintable, meta = (CannotImplementInterfaceInBlueprint))
class USuspenseCoreEventSubscriber : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISuspenseCoreEventSubscriber
 *
 * Интерфейс для объектов, подписывающихся на события EventBus.
 * Обеспечивает стандартный lifecycle для подписок.
 */
class BRIDGESYSTEM_API ISuspenseCoreEventSubscriber
{
	GENERATED_BODY()

public:
	/**
	 * Настройка подписок на события.
	 * Вызывается при инициализации объекта.
	 */
	virtual void SetupEventSubscriptions(USuspenseCoreEventBus* EventBus) = 0;

	/**
	 * Отписка от событий.
	 * Вызывается при уничтожении объекта.
	 */
	virtual void TeardownEventSubscriptions(USuspenseCoreEventBus* EventBus) = 0;

	/**
	 * Получить все активные handles подписок.
	 */
	virtual TArray<FSuspenseCoreSubscriptionHandle> GetSubscriptionHandles() const = 0;
};

// ═══════════════════════════════════════════════════════════════════════════════
// ISuspenseCoreEventEmitter
// ═══════════════════════════════════════════════════════════════════════════════

UINTERFACE(MinimalAPI, Blueprintable, meta = (CannotImplementInterfaceInBlueprint))
class USuspenseCoreEventEmitter : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISuspenseCoreEventEmitter
 *
 * Интерфейс для объектов, публикующих события.
 */
class BRIDGESYSTEM_API ISuspenseCoreEventEmitter
{
	GENERATED_BODY()

public:
	/**
	 * Публикует событие.
	 */
	virtual void EmitEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& Data) = 0;

	/**
	 * Получить EventBus для публикации.
	 */
	virtual USuspenseCoreEventBus* GetEventBus() const = 0;
};

// ═══════════════════════════════════════════════════════════════════════════════
// ISuspenseCoreService
// ═══════════════════════════════════════════════════════════════════════════════

UINTERFACE(MinimalAPI, Blueprintable, meta = (CannotImplementInterfaceInBlueprint))
class USuspenseCoreService : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISuspenseCoreService
 *
 * Базовый интерфейс для сервисов в ServiceLocator.
 */
class BRIDGESYSTEM_API ISuspenseCoreService
{
	GENERATED_BODY()

public:
	/**
	 * Инициализация сервиса.
	 */
	virtual void InitializeService() = 0;

	/**
	 * Остановка сервиса.
	 */
	virtual void ShutdownService() = 0;

	/**
	 * Имя сервиса для отладки.
	 */
	virtual FName GetServiceName() const = 0;

	/**
	 * Готов ли сервис к использованию.
	 */
	virtual bool IsServiceReady() const { return true; }
};

// ═══════════════════════════════════════════════════════════════════════════════
// ISuspenseCorePlayerRepository
// ═══════════════════════════════════════════════════════════════════════════════

UINTERFACE(MinimalAPI, Blueprintable, meta = (CannotImplementInterfaceInBlueprint))
class USuspenseCorePlayerRepository : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISuspenseCorePlayerRepository
 *
 * Интерфейс репозитория для данных игрока.
 * Позволяет легко мигрировать между разными БД:
 * - FileRepository (JSON/Binary) — для dedicated server
 * - SQLRepository (PostgreSQL/SQLite) — для production
 * - CloudRepository (PlayFab, etc.) — для облака
 */
class BRIDGESYSTEM_API ISuspenseCorePlayerRepository
{
	GENERATED_BODY()

public:
	// ═══════════════════════════════════════════════════════════════════════════
	// CRUD Operations
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Загрузить данные игрока по ID.
	 * @param PlayerId - уникальный ID игрока
	 * @param OutPlayerData - данные игрока (out)
	 * @return true если загрузка успешна
	 */
	virtual bool LoadPlayer(const FString& PlayerId, FSuspenseCorePlayerData& OutPlayerData) = 0;

	/**
	 * Сохранить данные игрока.
	 * @param PlayerData - данные для сохранения
	 * @return true если сохранение успешно
	 */
	virtual bool SavePlayer(const FSuspenseCorePlayerData& PlayerData) = 0;

	/**
	 * Удалить данные игрока.
	 * @param PlayerId - ID игрока для удаления
	 * @return true если удаление успешно
	 */
	virtual bool DeletePlayer(const FString& PlayerId) = 0;

	/**
	 * Проверить существует ли игрок.
	 */
	virtual bool PlayerExists(const FString& PlayerId) = 0;

	// ═══════════════════════════════════════════════════════════════════════════
	// Queries
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Создать нового игрока.
	 * @param DisplayName - отображаемое имя
	 * @param OutPlayerData - созданные данные (out)
	 * @return true если создание успешно
	 */
	virtual bool CreatePlayer(const FString& DisplayName, FSuspenseCorePlayerData& OutPlayerData) = 0;

	/**
	 * Получить список всех игроков.
	 * @param OutPlayerIds - массив ID (out)
	 */
	virtual void GetAllPlayerIds(TArray<FString>& OutPlayerIds) = 0;

	/**
	 * Получить лидерборд.
	 * @param Category - категория (Kills, Score, etc.)
	 * @param Count - количество записей
	 * @param OutLeaderboard - результат (out)
	 */
	virtual void GetLeaderboard(const FString& Category, int32 Count, TArray<FSuspenseCorePlayerData>& OutLeaderboard) = 0;

	// ═══════════════════════════════════════════════════════════════════════════
	// Partial Updates
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Обновить только статистику.
	 */
	virtual bool UpdateStats(const FString& PlayerId, int32 Kills, int32 Deaths, int32 Assists) = 0;

	/**
	 * Обновить валюту.
	 */
	virtual bool UpdateCurrency(const FString& PlayerId, int64 SoftCurrency, int64 HardCurrency) = 0;

	/**
	 * Обновить XP и уровень.
	 */
	virtual bool UpdateProgress(const FString& PlayerId, int64 XP, int32 Level) = 0;

	// ═══════════════════════════════════════════════════════════════════════════
	// Async Operations (опционально)
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Асинхронная загрузка (для сетевых БД)
	 */
	virtual void LoadPlayerAsync(const FString& PlayerId, TFunction<void(bool, const FSuspenseCorePlayerData&)> Callback)
	{
		// Default: синхронная реализация
		FSuspenseCorePlayerData Data;
		bool bSuccess = LoadPlayer(PlayerId, Data);
		Callback(bSuccess, Data);
	}

	/**
	 * Асинхронное сохранение
	 */
	virtual void SavePlayerAsync(const FSuspenseCorePlayerData& PlayerData, TFunction<void(bool)> Callback)
	{
		// Default: синхронная реализация
		bool bSuccess = SavePlayer(PlayerData);
		Callback(bSuccess);
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// Repository Info
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Получить тип репозитория.
	 */
	virtual FString GetRepositoryType() const = 0;

	/**
	 * Проверить соединение (для сетевых БД).
	 */
	virtual bool IsConnected() const { return true; }
};
