// Copyright Suspense Team. All Rights Reserved.
#include "SuspenseCore/Core/Utils/SuspenseCoreGlobalCacheRegistry.h"

FSuspenseCoreGlobalCacheRegistry& FSuspenseCoreGlobalCacheRegistry::Get()
{
	static FSuspenseCoreGlobalCacheRegistry Instance;
	return Instance;
}

void FSuspenseCoreGlobalCacheRegistry::RegisterCache(const FString& Name, TFunction<FString(void)> Getter)
{
	FScopeLock Lock(&RegistryLock);
	CacheStatsGetters.Add(Name, MoveTemp(Getter));
}

void FSuspenseCoreGlobalCacheRegistry::UnregisterCache(const FString& Name)
{
	FScopeLock Lock(&RegistryLock);
	CacheStatsGetters.Remove(Name);
}

FString FSuspenseCoreGlobalCacheRegistry::DumpAllStats() const
{
	FScopeLock Lock(&RegistryLock);
	FString Out;
	for (const auto& Pair : CacheStatsGetters)
	{
		Out += FString::Printf(TEXT("[%s]\n%s\n\n"), *Pair.Key, *Pair.Value());
	}
	return Out;
}

void FSuspenseCoreGlobalCacheRegistry::InvalidateAllCaches()
{
	OnGlobalInvalidate.Broadcast();
}

void FSuspenseCoreGlobalCacheRegistry::SecurityAudit()
{
	UE_LOG(LogTemp, Log, TEXT("FSuspenseCoreGlobalCacheRegistry: Security audit requested"));
	// При необходимости — расширяем аудит проекта.
}
