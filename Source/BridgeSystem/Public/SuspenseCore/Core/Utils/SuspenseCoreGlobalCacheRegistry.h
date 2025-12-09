// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "Templates/Function.h"

// forward-declare шаблон, чтобы не тянуть весь FSuspenseCoreEquipmentCacheManager.h сюда
template<typename K, typename V>
class FSuspenseCoreEquipmentCacheManager;

/**
 * Глобальный реестр кэшей: регистрация, сводная статистика, инвалидция, аудит.
 * Символы реализованы в .cpp и экспортируются из BridgeSystem.
 */
class BRIDGESYSTEM_API FSuspenseCoreGlobalCacheRegistry
{
public:
	// Singleton (реализация в .cpp)
	static FSuspenseCoreGlobalCacheRegistry& Get();

	// Регистрация кэша произвольным getter'ом статистики
	void RegisterCache(const FString& Name, TFunction<FString(void)> Getter);

	// Удобная перегрузка: регистрируем указатель на кэш — берём DumpStats()
	template<typename K, typename V>
	void RegisterCache(const FString& Name, FSuspenseCoreEquipmentCacheManager<K, V>* Cache)
	{
		check(Cache != nullptr);
		RegisterCache(Name, [Cache]() { return Cache->DumpStats(); });
	}

	// Отписка
	void UnregisterCache(const FString& Name);

	// Сводный дамп по всем кэшам
	FString DumpAllStats() const;

	// Глобальная инвалидция (подписчики сами очищают свои кэши)
	void InvalidateAllCaches();

	// Простой аудит (hook для расширений)
	void SecurityAudit();

	// Делегат для глобальной инвалидции
	DECLARE_MULTICAST_DELEGATE(FOnGlobalInvalidate);
	FOnGlobalInvalidate OnGlobalInvalidate;

private:
	FSuspenseCoreGlobalCacheRegistry() = default;

private:
	mutable FCriticalSection RegistryLock;
	TMap<FString, TFunction<FString(void)>> CacheStatsGetters;
};