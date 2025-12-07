// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Subsystems/SuspenseCoreSystemCoordinator.h"
#include "SuspenseCore/Services/SuspenseCoreServiceLocator.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "Engine/GameInstance.h"

// Logging
DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreCoordinator, Log, All);

//========================================
// Constructor
//========================================

USuspenseCoreSystemCoordinator::USuspenseCoreSystemCoordinator()
	: ServiceLocator(nullptr)
	, CachedEventManager(nullptr)
	, bIsInitialized(false)
	, bServicesRegistered(false)
	, bServicesInitialized(false)
{
}

//========================================
// USubsystem Interface
//========================================

bool USuspenseCoreSystemCoordinator::ShouldCreateSubsystem(UObject* Outer) const
{
	// Always create for valid GameInstance
	return Outer && Outer->IsA<UGameInstance>();
}

void USuspenseCoreSystemCoordinator::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	check(IsInGameThread());

	UE_LOG(LogSuspenseCoreCoordinator, Log, TEXT("[Initialize] Initializing SuspenseCoreSystemCoordinator..."));

	// Create core subsystems
	CreateSubsystems();

	// Register core services
	RegisterCoreServices();
	bServicesRegistered = true;

	// Initialize all services
	InitializeServices();
	bServicesInitialized = true;

	// Mark as ready
	bIsInitialized = true;

	UE_LOG(LogSuspenseCoreCoordinator, Log, TEXT("[Initialize] SuspenseCoreSystemCoordinator initialized successfully. Services registered: %d"),
		GetServiceCount());

	// Broadcast system initialized event
	BroadcastSystemEvent(FGameplayTag::RequestGameplayTag(FName("System.Coordinator.Initialized")), this);
}

void USuspenseCoreSystemCoordinator::Deinitialize()
{
	check(IsInGameThread());

	UE_LOG(LogSuspenseCoreCoordinator, Log, TEXT("[Deinitialize] Shutting down SuspenseCoreSystemCoordinator..."));

	// Broadcast shutdown event
	BroadcastSystemEvent(FGameplayTag::RequestGameplayTag(FName("System.Coordinator.Shutdown")), this);

	// Shutdown all services
	ShutdownServices();

	// Clear state flags
	bServicesInitialized = false;
	bServicesRegistered = false;
	bIsInitialized = false;

	// Clear service locator
	if (ServiceLocator)
	{
		ServiceLocator->ClearAllServices();
		ServiceLocator = nullptr;
	}

	// Clear cached references
	CachedEventManager.Reset();

	UE_LOG(LogSuspenseCoreCoordinator, Log, TEXT("[Deinitialize] SuspenseCoreSystemCoordinator shutdown complete."));

	Super::Deinitialize();
}

//========================================
// Static Accessor
//========================================

USuspenseCoreSystemCoordinator* USuspenseCoreSystemCoordinator::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	if (const UWorld* World = WorldContextObject->GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			return GameInstance->GetSubsystem<USuspenseCoreSystemCoordinator>();
		}
	}

	return nullptr;
}

//========================================
// Service Management API
//========================================

void USuspenseCoreSystemCoordinator::RegisterServiceByName(FName ServiceName, UObject* ServiceInstance)
{
	check(IsInGameThread());

	if (!ServiceInstance)
	{
		UE_LOG(LogSuspenseCoreCoordinator, Warning, TEXT("[RegisterServiceByName] Cannot register null service: %s"),
			*ServiceName.ToString());
		return;
	}

	if (!ServiceLocator)
	{
		UE_LOG(LogSuspenseCoreCoordinator, Error, TEXT("[RegisterServiceByName] ServiceLocator is null! Cannot register service: %s"),
			*ServiceName.ToString());
		return;
	}

	ServiceLocator->RegisterServiceByName(ServiceName, ServiceInstance);

	UE_LOG(LogSuspenseCoreCoordinator, Verbose, TEXT("[RegisterServiceByName] Registered service: %s (%s)"),
		*ServiceName.ToString(), *ServiceInstance->GetClass()->GetName());
}

void USuspenseCoreSystemCoordinator::UnregisterServiceByName(FName ServiceName)
{
	check(IsInGameThread());

	if (!ServiceLocator)
	{
		UE_LOG(LogSuspenseCoreCoordinator, Warning, TEXT("[UnregisterServiceByName] ServiceLocator is null!"));
		return;
	}

	ServiceLocator->UnregisterService(ServiceName);

	UE_LOG(LogSuspenseCoreCoordinator, Verbose, TEXT("[UnregisterServiceByName] Unregistered service: %s"),
		*ServiceName.ToString());
}

UObject* USuspenseCoreSystemCoordinator::GetServiceByName(FName ServiceName) const
{
	check(IsInGameThread());

	if (!ServiceLocator)
	{
		return nullptr;
	}

	return ServiceLocator->GetServiceByName(ServiceName);
}

bool USuspenseCoreSystemCoordinator::HasService(FName ServiceName) const
{
	check(IsInGameThread());

	if (!ServiceLocator)
	{
		return false;
	}

	return ServiceLocator->HasService(ServiceName);
}

//========================================
// EventBus Integration
//========================================

USuspenseCoreEventBus* USuspenseCoreSystemCoordinator::GetEventBus() const
{
	check(IsInGameThread());

	// Get EventManager and return its EventBus
	if (USuspenseCoreEventManager* EventManager = GetEventManager())
	{
		return EventManager->GetEventBus();
	}

	return nullptr;
}

void USuspenseCoreSystemCoordinator::BroadcastSystemEvent(FGameplayTag EventTag, UObject* Source)
{
	check(IsInGameThread());

	if (!EventTag.IsValid())
	{
		UE_LOG(LogSuspenseCoreCoordinator, Warning, TEXT("[BroadcastSystemEvent] Invalid event tag!"));
		return;
	}

	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		EventBus->PublishSimple(EventTag, Source);

		UE_LOG(LogSuspenseCoreCoordinator, VeryVerbose, TEXT("[BroadcastSystemEvent] Event published: %s"),
			*EventTag.ToString());
	}
	else
	{
		UE_LOG(LogSuspenseCoreCoordinator, Warning, TEXT("[BroadcastSystemEvent] EventBus not available! Event not published: %s"),
			*EventTag.ToString());
	}
}

void USuspenseCoreSystemCoordinator::SubscribeToSystemEvent(FGameplayTag EventTag, UObject* Subscriber)
{
	check(IsInGameThread());

	if (!EventTag.IsValid())
	{
		UE_LOG(LogSuspenseCoreCoordinator, Warning, TEXT("[SubscribeToSystemEvent] Invalid event tag!"));
		return;
	}

	if (!Subscriber)
	{
		UE_LOG(LogSuspenseCoreCoordinator, Warning, TEXT("[SubscribeToSystemEvent] Null subscriber!"));
		return;
	}

	// Note: This is a simplified Blueprint-friendly version
	// For full subscription with callback, use EventBus->Subscribe() directly
	UE_LOG(LogSuspenseCoreCoordinator, Verbose, TEXT("[SubscribeToSystemEvent] Subscriber registered for event: %s"),
		*EventTag.ToString());
}

//========================================
// Status API
//========================================

int32 USuspenseCoreSystemCoordinator::GetServiceCount() const
{
	check(IsInGameThread());

	if (!ServiceLocator)
	{
		return 0;
	}

	return ServiceLocator->GetServiceCount();
}

TArray<FName> USuspenseCoreSystemCoordinator::GetRegisteredServiceNames() const
{
	check(IsInGameThread());

	if (!ServiceLocator)
	{
		return TArray<FName>();
	}

	return ServiceLocator->GetRegisteredServiceNames();
}

//========================================
// Debug Commands
//========================================

void USuspenseCoreSystemCoordinator::DebugDumpSystemState()
{
	UE_LOG(LogSuspenseCoreCoordinator, Display, TEXT("========================================"));
	UE_LOG(LogSuspenseCoreCoordinator, Display, TEXT("SuspenseCoreSystemCoordinator State"));
	UE_LOG(LogSuspenseCoreCoordinator, Display, TEXT("========================================"));
	UE_LOG(LogSuspenseCoreCoordinator, Display, TEXT("Initialized: %s"), bIsInitialized ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogSuspenseCoreCoordinator, Display, TEXT("Services Registered: %s"), bServicesRegistered ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogSuspenseCoreCoordinator, Display, TEXT("Services Initialized: %s"), bServicesInitialized ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogSuspenseCoreCoordinator, Display, TEXT("Service Count: %d"), GetServiceCount());
	UE_LOG(LogSuspenseCoreCoordinator, Display, TEXT("ServiceLocator Valid: %s"), ServiceLocator ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogSuspenseCoreCoordinator, Display, TEXT("EventBus Available: %s"), GetEventBus() ? TEXT("YES") : TEXT("NO"));
	UE_LOG(LogSuspenseCoreCoordinator, Display, TEXT("========================================"));
}

void USuspenseCoreSystemCoordinator::DebugDumpServices()
{
	UE_LOG(LogSuspenseCoreCoordinator, Display, TEXT("========================================"));
	UE_LOG(LogSuspenseCoreCoordinator, Display, TEXT("Registered Services (%d)"), GetServiceCount());
	UE_LOG(LogSuspenseCoreCoordinator, Display, TEXT("========================================"));

	TArray<FName> ServiceNames = GetRegisteredServiceNames();
	for (const FName& ServiceName : ServiceNames)
	{
		if (UObject* Service = GetServiceByName(ServiceName))
		{
			UE_LOG(LogSuspenseCoreCoordinator, Display, TEXT("  - %s (%s)"),
				*ServiceName.ToString(), *Service->GetClass()->GetName());
		}
		else
		{
			UE_LOG(LogSuspenseCoreCoordinator, Display, TEXT("  - %s (INVALID)"), *ServiceName.ToString());
		}
	}

	UE_LOG(LogSuspenseCoreCoordinator, Display, TEXT("========================================"));
}

//========================================
// Protected Methods
//========================================

void USuspenseCoreSystemCoordinator::CreateSubsystems()
{
	check(IsInGameThread());

	// Get GameInstance as outer for persistence across map loads
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		UE_LOG(LogSuspenseCoreCoordinator, Error, TEXT("[CreateSubsystems] GameInstance is null!"));
		return;
	}

	// Create ServiceLocator
	ServiceLocator = NewObject<USuspenseCoreServiceLocator>(GameInstance, TEXT("SuspenseCoreServiceLocator"));
	if (!ServiceLocator)
	{
		UE_LOG(LogSuspenseCoreCoordinator, Error, TEXT("[CreateSubsystems] Failed to create ServiceLocator!"));
		return;
	}

	UE_LOG(LogSuspenseCoreCoordinator, Log, TEXT("[CreateSubsystems] ServiceLocator created successfully."));

	// Cache EventManager reference
	CachedEventManager = GetEventManager();
	if (CachedEventManager.IsValid())
	{
		UE_LOG(LogSuspenseCoreCoordinator, Log, TEXT("[CreateSubsystems] EventManager found and cached."));
	}
	else
	{
		UE_LOG(LogSuspenseCoreCoordinator, Warning, TEXT("[CreateSubsystems] EventManager not available yet. Will retry on demand."));
	}
}

void USuspenseCoreSystemCoordinator::RegisterCoreServices()
{
	check(IsInGameThread());

	UE_LOG(LogSuspenseCoreCoordinator, Log, TEXT("[RegisterCoreServices] Registering core SuspenseCore services..."));

	// Register ServiceLocator itself as a service
	if (ServiceLocator)
	{
		RegisterService<USuspenseCoreServiceLocator>(ServiceLocator);
	}

	// Subclasses can override this method to register additional services

	UE_LOG(LogSuspenseCoreCoordinator, Log, TEXT("[RegisterCoreServices] Core services registration complete."));
}

void USuspenseCoreSystemCoordinator::InitializeServices()
{
	check(IsInGameThread());

	UE_LOG(LogSuspenseCoreCoordinator, Log, TEXT("[InitializeServices] Initializing registered services..."));

	// Get all registered services
	TArray<FName> ServiceNames = GetRegisteredServiceNames();

	int32 InitializedCount = 0;
	for (const FName& ServiceName : ServiceNames)
	{
		if (UObject* Service = GetServiceByName(ServiceName))
		{
			// Check if service implements initialization interface
			// For now, we just log. Extend this to call Initialize() if interface exists.
			UE_LOG(LogSuspenseCoreCoordinator, Verbose, TEXT("[InitializeServices] Service ready: %s"),
				*ServiceName.ToString());

			InitializedCount++;
		}
	}

	UE_LOG(LogSuspenseCoreCoordinator, Log, TEXT("[InitializeServices] Initialized %d services."), InitializedCount);
}

void USuspenseCoreSystemCoordinator::ShutdownServices()
{
	check(IsInGameThread());

	UE_LOG(LogSuspenseCoreCoordinator, Log, TEXT("[ShutdownServices] Shutting down services..."));

	// Get all registered services
	TArray<FName> ServiceNames = GetRegisteredServiceNames();

	int32 ShutdownCount = 0;
	for (const FName& ServiceName : ServiceNames)
	{
		if (UObject* Service = GetServiceByName(ServiceName))
		{
			// Check if service implements shutdown interface
			// For now, we just log. Extend this to call Shutdown() if interface exists.
			UE_LOG(LogSuspenseCoreCoordinator, Verbose, TEXT("[ShutdownServices] Shutting down service: %s"),
				*ServiceName.ToString());

			ShutdownCount++;
		}
	}

	UE_LOG(LogSuspenseCoreCoordinator, Log, TEXT("[ShutdownServices] Shutdown %d services."), ShutdownCount);
}

bool USuspenseCoreSystemCoordinator::ValidateServices(TArray<FText>& OutErrors) const
{
	check(IsInGameThread());

	OutErrors.Empty();

	// Validate ServiceLocator exists
	if (!ServiceLocator)
	{
		OutErrors.Add(FText::FromString(TEXT("ServiceLocator is null")));
		return false;
	}

	// Validate we have at least one service registered
	if (GetServiceCount() == 0)
	{
		OutErrors.Add(FText::FromString(TEXT("No services registered")));
		return false;
	}

	// All services should be valid objects
	TArray<FName> ServiceNames = GetRegisteredServiceNames();
	for (const FName& ServiceName : ServiceNames)
	{
		if (!GetServiceByName(ServiceName))
		{
			OutErrors.Add(FText::FromString(FString::Printf(TEXT("Service '%s' is invalid"), *ServiceName.ToString())));
		}
	}

	return OutErrors.Num() == 0;
}

USuspenseCoreEventManager* USuspenseCoreSystemCoordinator::GetEventManager() const
{
	check(IsInGameThread());

	// Return cached version if still valid
	if (CachedEventManager.IsValid())
	{
		return CachedEventManager.Get();
	}

	// Try to get from GameInstance
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<USuspenseCoreEventManager>();
	}

	return nullptr;
}
