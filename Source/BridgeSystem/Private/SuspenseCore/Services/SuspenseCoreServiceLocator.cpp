// SuspenseCoreServiceLocator.cpp
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/SuspenseCoreServiceLocator.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreServiceLocator, Log, All);

USuspenseCoreServiceLocator::USuspenseCoreServiceLocator()
{
}

// ═══════════════════════════════════════════════════════════════════════════════
// РЕГИСТРАЦИЯ
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreServiceLocator::RegisterServiceByName(FName ServiceName, UObject* ServiceInstance)
{
	if (ServiceName.IsNone())
	{
		UE_LOG(LogSuspenseCoreServiceLocator, Warning, TEXT("RegisterService: Invalid ServiceName"));
		return;
	}

	if (!ServiceInstance)
	{
		UE_LOG(LogSuspenseCoreServiceLocator, Warning, TEXT("RegisterService: Null ServiceInstance for %s"),
			*ServiceName.ToString());
		return;
	}

	FScopeLock Lock(&ServiceLock);

	if (Services.Contains(ServiceName))
	{
		UE_LOG(LogSuspenseCoreServiceLocator, Warning, TEXT("RegisterService: Overwriting existing service %s"),
			*ServiceName.ToString());
	}

	Services.Add(ServiceName, ServiceInstance);

	UE_LOG(LogSuspenseCoreServiceLocator, Log, TEXT("Registered service: %s (%s)"),
		*ServiceName.ToString(), *GetNameSafe(ServiceInstance));
}

// ═══════════════════════════════════════════════════════════════════════════════
// ПОЛУЧЕНИЕ
// ═══════════════════════════════════════════════════════════════════════════════

UObject* USuspenseCoreServiceLocator::GetServiceByName(FName ServiceName) const
{
	FScopeLock Lock(&ServiceLock);

	if (const TObjectPtr<UObject>* Found = Services.Find(ServiceName))
	{
		return Found->Get();
	}

	return nullptr;
}

bool USuspenseCoreServiceLocator::HasService(FName ServiceName) const
{
	FScopeLock Lock(&ServiceLock);
	return Services.Contains(ServiceName);
}

// ═══════════════════════════════════════════════════════════════════════════════
// УПРАВЛЕНИЕ
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreServiceLocator::UnregisterService(FName ServiceName)
{
	FScopeLock Lock(&ServiceLock);

	if (Services.Remove(ServiceName) > 0)
	{
		UE_LOG(LogSuspenseCoreServiceLocator, Log, TEXT("Unregistered service: %s"), *ServiceName.ToString());
	}
}

void USuspenseCoreServiceLocator::ClearAllServices()
{
	FScopeLock Lock(&ServiceLock);

	int32 Count = Services.Num();
	Services.Empty();

	UE_LOG(LogSuspenseCoreServiceLocator, Log, TEXT("Cleared all services (%d)"), Count);
}

TArray<FName> USuspenseCoreServiceLocator::GetRegisteredServiceNames() const
{
	FScopeLock Lock(&ServiceLock);

	TArray<FName> Names;
	Services.GenerateKeyArray(Names);
	return Names;
}

int32 USuspenseCoreServiceLocator::GetServiceCount() const
{
	FScopeLock Lock(&ServiceLock);
	return Services.Num();
}
