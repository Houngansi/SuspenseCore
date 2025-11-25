// Copyright Suspense Team. All Rights Reserved.
#include "Core/Utils/FSuspenseGlobalCacheRegistry.h"

FSuspenseGlobalCacheRegistry& FSuspenseGlobalCacheRegistry::Get()
{
	static FSuspenseGlobalCacheRegistry Instance;
	return Instance;
}

void FSuspenseGlobalCacheRegistry::RegisterCache(const FString& Name, TFunction<FString(void)> Getter)
{
	FScopeLock Lock(&RegistryLock);
	CacheStatsGetters.Add(Name, MoveTemp(Getter));
}

void FSuspenseGlobalCacheRegistry::UnregisterCache(const FString& Name)
{
	FScopeLock Lock(&RegistryLock);
	CacheStatsGetters.Remove(Name);
}

FString FSuspenseGlobalCacheRegistry::DumpAllStats() const
{
	FScopeLock Lock(&RegistryLock);
	FString Out;
	for (const auto& Pair : CacheStatsGetters)
	{
		Out += FString::Printf(TEXT("[%s]\n%s\n\n"), *Pair.Key, *Pair.Value());
	}
	return Out;
}

void FSuspenseGlobalCacheRegistry::InvalidateAllCaches()
{
	OnGlobalInvalidate.Broadcast();
}

void FSuspenseGlobalCacheRegistry::SecurityAudit()
{
	UE_LOG(LogTemp, Log, TEXT("FSuspenseGlobalCacheRegistry: Security audit requested"));
	// При необходимости — расширяем аудит проекта.
}
