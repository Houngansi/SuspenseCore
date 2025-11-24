// Copyright MedCom Team. All Rights Reserved.
#include "Core/Utils/FGlobalCacheRegistry.h"

FGlobalCacheRegistry& FGlobalCacheRegistry::Get()
{
	static FGlobalCacheRegistry Instance;
	return Instance;
}

void FGlobalCacheRegistry::RegisterCache(const FString& Name, TFunction<FString(void)> Getter)
{
	FScopeLock Lock(&RegistryLock);
	CacheStatsGetters.Add(Name, MoveTemp(Getter));
}

void FGlobalCacheRegistry::UnregisterCache(const FString& Name)
{
	FScopeLock Lock(&RegistryLock);
	CacheStatsGetters.Remove(Name);
}

FString FGlobalCacheRegistry::DumpAllStats() const
{
	FScopeLock Lock(&RegistryLock);
	FString Out;
	for (const auto& Pair : CacheStatsGetters)
	{
		Out += FString::Printf(TEXT("[%s]\n%s\n\n"), *Pair.Key, *Pair.Value());
	}
	return Out;
}

void FGlobalCacheRegistry::InvalidateAllCaches()
{
	OnGlobalInvalidate.Broadcast();
}

void FGlobalCacheRegistry::SecurityAudit()
{
	UE_LOG(LogTemp, Log, TEXT("FGlobalCacheRegistry: Security audit requested"));
	// При необходимости — расширяем аудит проекта.
}
