// SuspenseCoreEventManager.cpp
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Services/SuspenseCoreServiceLocator.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreEventManager, Log, All);

// ═══════════════════════════════════════════════════════════════════════════════
// USubsystem Interface
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreEventManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogSuspenseCoreEventManager, Log, TEXT("SuspenseCoreEventManager initializing..."));

	CreateSubsystems();

	// Register Tick for processing deferred events
	TickDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &USuspenseCoreEventManager::Tick),
		0.0f // Every frame
	);

	PublishSystemInitialized();

	UE_LOG(LogSuspenseCoreEventManager, Log, TEXT("SuspenseCoreEventManager initialized successfully"));
}

void USuspenseCoreEventManager::Deinitialize()
{
	UE_LOG(LogSuspenseCoreEventManager, Log, TEXT("SuspenseCoreEventManager deinitializing..."));

	// Remove Tick
	if (TickDelegateHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
	}

	// Publish shutdown event
	if (EventBus)
	{
		FSuspenseCoreEventData Data = FSuspenseCoreEventData::Create(this);
		EventBus->Publish(SUSPENSE_CORE_TAG(Event.System.Shutdown), Data);
	}

	// Clear services
	if (ServiceLocator)
	{
		ServiceLocator->ClearAllServices();
	}

	EventBus = nullptr;
	ServiceLocator = nullptr;

	Super::Deinitialize();

	UE_LOG(LogSuspenseCoreEventManager, Log, TEXT("SuspenseCoreEventManager deinitialized"));
}

bool USuspenseCoreEventManager::ShouldCreateSubsystem(UObject* Outer) const
{
	// Always create
	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// STATIC ACCESS
// ═══════════════════════════════════════════════════════════════════════════════

USuspenseCoreEventManager* USuspenseCoreEventManager::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		return nullptr;
	}

	return GameInstance->GetSubsystem<USuspenseCoreEventManager>();
}

// ═══════════════════════════════════════════════════════════════════════════════
// HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreEventManager::PublishEvent(FGameplayTag EventTag, UObject* Source)
{
	if (EventBus)
	{
		EventBus->PublishSimple(EventTag, Source);
	}
}

void USuspenseCoreEventManager::PublishEventWithData(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	if (EventBus)
	{
		EventBus->Publish(EventTag, EventData);
	}
}

FSuspenseCoreSubscriptionHandle USuspenseCoreEventManager::SubscribeToEvent(
	FGameplayTag EventTag,
	const FSuspenseCoreEventCallback& Callback)
{
	if (EventBus)
	{
		return EventBus->Subscribe(EventTag, Callback);
	}
	return FSuspenseCoreSubscriptionHandle();
}

void USuspenseCoreEventManager::UnsubscribeFromEvent(FSuspenseCoreSubscriptionHandle Handle)
{
	if (EventBus)
	{
		EventBus->Unsubscribe(Handle);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// DEBUG
// ═══════════════════════════════════════════════════════════════════════════════

FSuspenseCoreEventBusStats USuspenseCoreEventManager::GetEventBusStats() const
{
	if (EventBus)
	{
		return EventBus->GetStats();
	}
	return FSuspenseCoreEventBusStats();
}

void USuspenseCoreEventManager::SetEventLogging(bool bEnabled)
{
	bLogEvents = bEnabled;
	UE_LOG(LogSuspenseCoreEventManager, Log, TEXT("Event logging %s"), bEnabled ? TEXT("enabled") : TEXT("disabled"));
}

// ═══════════════════════════════════════════════════════════════════════════════
// PRIVATE
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreEventManager::CreateSubsystems()
{
	// Create EventBus
	EventBus = NewObject<USuspenseCoreEventBus>(this, TEXT("SuspenseCoreEventBus"));
	UE_LOG(LogSuspenseCoreEventManager, Log, TEXT("Created EventBus"));

	// Create ServiceLocator
	ServiceLocator = NewObject<USuspenseCoreServiceLocator>(this, TEXT("SuspenseCoreServiceLocator"));
	UE_LOG(LogSuspenseCoreEventManager, Log, TEXT("Created ServiceLocator"));

	// Register EventBus and ServiceLocator as services
	ServiceLocator->RegisterService<USuspenseCoreEventBus>(EventBus);
	ServiceLocator->RegisterService<USuspenseCoreServiceLocator>(ServiceLocator);
}

bool USuspenseCoreEventManager::Tick(float DeltaTime)
{
	// Process deferred events
	if (EventBus)
	{
		EventBus->ProcessDeferredEvents();
	}

	// Periodically cleanup stale subscriptions (approximately every 10 seconds)
	static float CleanupTimer = 0.0f;
	CleanupTimer += DeltaTime;
	if (CleanupTimer > 10.0f)
	{
		CleanupTimer = 0.0f;
		if (EventBus)
		{
			EventBus->CleanupStaleSubscriptions();
		}
	}

	return true; // Continue ticking
}

void USuspenseCoreEventManager::PublishSystemInitialized()
{
	if (EventBus)
	{
		FSuspenseCoreEventData Data = FSuspenseCoreEventData::Create(this);
		Data.SetString(TEXT("Version"), TEXT("1.0.0"));
		Data.SetString(TEXT("Module"), TEXT("BridgeSystem"));

		EventBus->Publish(SUSPENSE_CORE_TAG(Event.System.Initialized), Data);
	}
}
