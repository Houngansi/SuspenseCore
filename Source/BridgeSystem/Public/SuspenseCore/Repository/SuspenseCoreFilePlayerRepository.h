// SuspenseCoreFilePlayerRepository.h
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/SuspenseCoreInterfaces.h"
#include "SuspenseCore/SuspenseCorePlayerData.h"
#include "SuspenseCoreFilePlayerRepository.generated.h"

/**
 * USuspenseCoreFilePlayerRepository
 *
 * Файловая реализация репозитория игрока.
 * Сохраняет данные в JSON файлы.
 *
 * Идеально для:
 * - Dedicated server (нет внешних зависимостей)
 * - Разработка и тестирование
 * - Одиночная игра
 *
 * Путь сохранения: [SaveDir]/Players/[PlayerId].json
 *
 * Миграция на другую БД:
 * - Создайте новую реализацию ISuspenseCorePlayerRepository
 * - Зарегистрируйте в ServiceLocator вместо этой
 */
UCLASS(BlueprintType)
class BRIDGESYSTEM_API USuspenseCoreFilePlayerRepository : public UObject, public ISuspenseCorePlayerRepository
{
	GENERATED_BODY()

public:
	USuspenseCoreFilePlayerRepository();

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Инициализация репозитория.
	 * @param InBasePath - базовый путь для сохранения (опционально)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Repository")
	void Initialize(const FString& InBasePath = TEXT(""));

	/**
	 * Получить путь к директории.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Repository")
	FString GetBasePath() const { return BasePath; }

	// ═══════════════════════════════════════════════════════════════════════════
	// ISuspenseCorePlayerRepository Implementation
	// ═══════════════════════════════════════════════════════════════════════════

	virtual bool LoadPlayer(const FString& PlayerId, FSuspenseCorePlayerData& OutPlayerData) override;
	virtual bool SavePlayer(const FSuspenseCorePlayerData& PlayerData) override;
	virtual bool DeletePlayer(const FString& PlayerId) override;
	virtual bool PlayerExists(const FString& PlayerId) override;

	virtual bool CreatePlayer(const FString& DisplayName, FSuspenseCorePlayerData& OutPlayerData) override;
	virtual void GetAllPlayerIds(TArray<FString>& OutPlayerIds) override;
	virtual void GetLeaderboard(const FString& Category, int32 Count, TArray<FSuspenseCorePlayerData>& OutLeaderboard) override;

	virtual bool UpdateStats(const FString& PlayerId, int32 Kills, int32 Deaths, int32 Assists) override;
	virtual bool UpdateCurrency(const FString& PlayerId, int64 SoftCurrency, int64 HardCurrency) override;
	virtual bool UpdateProgress(const FString& PlayerId, int64 XP, int32 Level) override;

	virtual FString GetRepositoryType() const override { return TEXT("FileRepository"); }
	virtual bool IsConnected() const override { return true; } // Всегда доступен

protected:
	/** Базовый путь для сохранения */
	FString BasePath;

	/** Кэш загруженных игроков */
	TMap<FString, FSuspenseCorePlayerData> PlayerCache;

	/** Критическая секция */
	mutable FCriticalSection RepositoryLock;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Получить полный путь к файлу игрока.
	 */
	FString GetPlayerFilePath(const FString& PlayerId) const;

	/**
	 * Сериализовать данные в JSON.
	 */
	bool SerializeToJson(const FSuspenseCorePlayerData& Data, FString& OutJson) const;

	/**
	 * Десериализовать данные из JSON.
	 */
	bool DeserializeFromJson(const FString& Json, FSuspenseCorePlayerData& OutData) const;

	/**
	 * Загрузить из кэша или файла.
	 */
	bool LoadPlayerInternal(const FString& PlayerId, FSuspenseCorePlayerData& OutData);

	/**
	 * Сохранить и обновить кэш.
	 */
	bool SavePlayerInternal(const FSuspenseCorePlayerData& Data);
};
