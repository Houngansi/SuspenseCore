// SuspenseCoreServiceProvider.cpp
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Services/SuspenseCoreServiceProvider.h"
#include "SuspenseCore/Services/SuspenseCoreServiceLocator.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY(LogSuspenseCoreServiceProvider);

// ═══════════════════════════════════════════════════════════════════════════════
// STATIC ACCESS
// ═══════════════════════════════════════════════════════════════════════════════

USuspenseCoreServiceProvider* USuspenseCoreServiceProvider::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		return nullptr;
	}

	return GameInstance->GetSubsystem<USuspenseCoreServiceProvider>();
}

// ═══════════════════════════════════════════════════════════════════════════════
// USUBSYSTEM INTERFACE
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreServiceProvider::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogSuspenseCoreServiceProvider, Log, TEXT("SuspenseCoreServiceProvider initializing..."));

	// Ensure dependent subsystems are initialized first
	Collection.InitializeDependency<USuspenseCoreEventManager>();
	Collection.InitializeDependency<USuspenseCoreDataManager>();

	// Cache references to subsystems
	CacheSubsystemReferences(Collection);

	// Register core services in ServiceLocator
	RegisterCoreServices();

	bIsInitialized = true;

	// Broadcast initialization event
	BroadcastInitialized();

	UE_LOG(LogSuspenseCoreServiceProvider, Log,
		TEXT("SuspenseCoreServiceProvider initialized with %d services"),
		GetServiceCount());
}

void USuspenseCoreServiceProvider::Deinitialize()
{
	UE_LOG(LogSuspenseCoreServiceProvider, Log, TEXT("SuspenseCoreServiceProvider deinitializing..."));

	bIsInitialized = false;

	// Clear cached references
	CachedEventManager.Reset();
	CachedDataManager.Reset();
	CachedEventBus.Reset();
	CachedServiceLocator.Reset();

	Super::Deinitialize();

	UE_LOG(LogSuspenseCoreServiceProvider, Log, TEXT("SuspenseCoreServiceProvider deinitialized"));
}

bool USuspenseCoreServiceProvider::ShouldCreateSubsystem(UObject* Outer) const
{
	// Always create for GameInstance
	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// CORE SERVICE ACCESSORS
// ═══════════════════════════════════════════════════════════════════════════════

USuspenseCoreEventBus* USuspenseCoreServiceProvider::GetEventBus() const
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	// Fallback: get from EventManager
	if (CachedEventManager.IsValid())
	{
		return CachedEventManager->GetEventBus();
	}

	return nullptr;
}

USuspenseCoreDataManager* USuspenseCoreServiceProvider::GetDataManager() const
{
	if (CachedDataManager.IsValid())
	{
		return CachedDataManager.Get();
	}

	return nullptr;
}

USuspenseCoreEventManager* USuspenseCoreServiceProvider::GetEventManager() const
{
	if (CachedEventManager.IsValid())
	{
		return CachedEventManager.Get();
	}

	return nullptr;
}

USuspenseCoreServiceLocator* USuspenseCoreServiceProvider::GetServiceLocator() const
{
	if (CachedServiceLocator.IsValid())
	{
		return CachedServiceLocator.Get();
	}

	// Fallback: get from EventManager
	if (CachedEventManager.IsValid())
	{
		return CachedEventManager->GetServiceLocator();
	}

	return nullptr;
}

// ═══════════════════════════════════════════════════════════════════════════════
// SERVICE REGISTRATION
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreServiceProvider::RegisterServiceByName(FName ServiceName, UObject* ServiceInstance)
{
	if (CachedServiceLocator.IsValid())
	{
		CachedServiceLocator->RegisterServiceByName(ServiceName, ServiceInstance);
		BroadcastServiceRegistered(ServiceName);
	}
	else
	{
		UE_LOG(LogSuspenseCoreServiceProvider, Warning,
			TEXT("Cannot register service '%s': ServiceLocator not available"),
			*ServiceName.ToString());
	}
}

void USuspenseCoreServiceProvider::UnregisterService(FName ServiceName)
{
	if (CachedServiceLocator.IsValid())
	{
		CachedServiceLocator->UnregisterService(ServiceName);
		BroadcastServiceUnregistered(ServiceName);
	}
}

bool USuspenseCoreServiceProvider::HasService(FName ServiceName) const
{
	if (CachedServiceLocator.IsValid())
	{
		return CachedServiceLocator->HasService(ServiceName);
	}
	return false;
}

TArray<FName> USuspenseCoreServiceProvider::GetRegisteredServiceNames() const
{
	if (CachedServiceLocator.IsValid())
	{
		return CachedServiceLocator->GetRegisteredServiceNames();
	}
	return TArray<FName>();
}

int32 USuspenseCoreServiceProvider::GetServiceCount() const
{
	if (CachedServiceLocator.IsValid())
	{
		return CachedServiceLocator->GetServiceCount();
	}
	return 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// INITIALIZATION HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreServiceProvider::CacheSubsystemReferences(FSubsystemCollectionBase& Collection)
{
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		UE_LOG(LogSuspenseCoreServiceProvider, Error, TEXT("GameInstance not available"));
		return;
	}

	// Cache EventManager
	CachedEventManager = GameInstance->GetSubsystem<USuspenseCoreEventManager>();
	if (CachedEventManager.IsValid())
	{
		// Cache EventBus and ServiceLocator from EventManager
		CachedEventBus = CachedEventManager->GetEventBus();
		CachedServiceLocator = CachedEventManager->GetServiceLocator();

		UE_LOG(LogSuspenseCoreServiceProvider, Log, TEXT("Cached EventManager, EventBus, ServiceLocator"));
	}
	else
	{
		UE_LOG(LogSuspenseCoreServiceProvider, Warning, TEXT("EventManager not available"));
	}

	// Cache DataManager
	CachedDataManager = GameInstance->GetSubsystem<USuspenseCoreDataManager>();
	if (CachedDataManager.IsValid())
	{
		UE_LOG(LogSuspenseCoreServiceProvider, Log, TEXT("Cached DataManager"));
	}
	else
	{
		UE_LOG(LogSuspenseCoreServiceProvider, Warning, TEXT("DataManager not available"));
	}
}

void USuspenseCoreServiceProvider::RegisterCoreServices()
{
	if (!CachedServiceLocator.IsValid())
	{
		UE_LOG(LogSuspenseCoreServiceProvider, Error, TEXT("Cannot register core services: ServiceLocator not available"));
		return;
	}

	// Register this provider itself
	CachedServiceLocator->RegisterService<USuspenseCoreServiceProvider>(this);

	// Register DataManager (EventBus and ServiceLocator are already registered by EventManager)
	if (CachedDataManager.IsValid())
	{
		CachedServiceLocator->RegisterService<USuspenseCoreDataManager>(CachedDataManager.Get());
	}

	UE_LOG(LogSuspenseCoreServiceProvider, Log, TEXT("Core services registered"));
}

void USuspenseCoreServiceProvider::BroadcastInitialized()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
	EventData.SetInt(FName("ServiceCount"), GetServiceCount());

	TArray<FName> ServiceNames = GetRegisteredServiceNames();
	FString ServicesStr;
	for (const FName& Name : ServiceNames)
	{
		if (!ServicesStr.IsEmpty())
		{
			ServicesStr += TEXT(", ");
		}
		ServicesStr += Name.ToString();
	}
	EventData.SetString(FName("Services"), ServicesStr);

	EventBus->Publish(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Services.Initialized")),
		EventData
	);
}

void USuspenseCoreServiceProvider::BroadcastServiceRegistered(FName ServiceName)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
	EventData.SetString(FName("ServiceName"), ServiceName.ToString());
	EventData.SetInt(FName("TotalServices"), GetServiceCount());

	EventBus->Publish(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Services.ServiceRegistered")),
		EventData
	);
}

void USuspenseCoreServiceProvider::BroadcastServiceUnregistered(FName ServiceName)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
	EventData.SetString(FName("ServiceName"), ServiceName.ToString());
	EventData.SetInt(FName("TotalServices"), GetServiceCount());

	EventBus->Publish(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Services.ServiceUnregistered")),
		EventData
	);
}
