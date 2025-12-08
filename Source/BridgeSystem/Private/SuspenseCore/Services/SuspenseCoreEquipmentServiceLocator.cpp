// SuspenseCoreEquipmentServiceLocator.cpp
// SuspenseCore - EventBus Architecture
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceLocator.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogSuspenseCoreServiceLocator);

// ═══════════════════════════════════════════════════════════════════════════════
// SERVICE EVENT TAGS
// ═══════════════════════════════════════════════════════════════════════════════

namespace SuspenseCoreServiceTags
{
	static const FGameplayTag Registered = FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Service.Registered")), false);
	static const FGameplayTag Initialized = FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Service.Initialized")), false);
	static const FGameplayTag Ready = FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Service.Ready")), false);
	static const FGameplayTag ShuttingDown = FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Service.ShuttingDown")), false);
	static const FGameplayTag Shutdown = FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Service.Shutdown")), false);
	static const FGameplayTag Failed = FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Service.Failed")), false);
}

// ═══════════════════════════════════════════════════════════════════════════════
// STATIC ACCESSOR
// ═══════════════════════════════════════════════════════════════════════════════

USuspenseCoreEquipmentServiceLocator* USuspenseCoreEquipmentServiceLocator::Get(const UObject* WorldContext)
{
	if (!WorldContext) return nullptr;
	const UWorld* World = WorldContext->GetWorld();
	if (!World) return nullptr;
	UGameInstance* GI = World->GetGameInstance();
	return GI ? GI->GetSubsystem<USuspenseCoreEquipmentServiceLocator>() : nullptr;
}

// ═══════════════════════════════════════════════════════════════════════════════
// SUBSYSTEM LIFECYCLE
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreEquipmentServiceLocator::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Periodic cleanup
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			CleanupTimer,
			this,
			&USuspenseCoreEquipmentServiceLocator::PerformAutomaticCleanup,
			CleanupInterval,
			true);
	}

	UE_LOG(LogSuspenseCoreServiceLocator, Log, TEXT("SuspenseCoreEquipmentServiceLocator initialized (EventBus architecture)"));
}

void USuspenseCoreEquipmentServiceLocator::Deinitialize()
{
	UE_LOG(LogSuspenseCoreServiceLocator, Log, TEXT("SuspenseCoreEquipmentServiceLocator deinitializing..."));

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CleanupTimer);
	}

	ShutdownAllServices(false);

	{
		FScopeLock Lock(&RegistryLock);
		Registry.Empty();
		Initializing.Empty();
		ReadySet.Empty();
	}

	CachedEventManager.Reset();

	Super::Deinitialize();
	UE_LOG(LogSuspenseCoreServiceLocator, Log, TEXT("SuspenseCoreEquipmentServiceLocator deinitialized"));
}

// ═══════════════════════════════════════════════════════════════════════════════
// REGISTRATION API
// ═══════════════════════════════════════════════════════════════════════════════

bool USuspenseCoreEquipmentServiceLocator::RegisterServiceClass(
	const FGameplayTag& ServiceTag,
	TSubclassOf<UObject> ServiceClass,
	const FSuspenseCoreServiceInitParams& InitParams)
{
	return RegisterServiceClassWithInjection(ServiceTag, ServiceClass, InitParams, FSuspenseCoreServiceInjectionDelegate());
}

bool USuspenseCoreEquipmentServiceLocator::RegisterServiceClassWithInjection(
	const FGameplayTag& ServiceTag,
	TSubclassOf<UObject> ServiceClass,
	const FSuspenseCoreServiceInitParams& InitParams,
	const FSuspenseCoreServiceInjectionDelegate& InjectionCallback)
{
	if (!ServiceTag.IsValid() || !ServiceClass)
	{
		UE_LOG(LogSuspenseCoreServiceLocator, Error, TEXT("RegisterServiceClass: invalid params"));
		return false;
	}

	// Class must implement ISuspenseCoreEquipmentService interface
	if (!ServiceClass->ImplementsInterface(USuspenseCoreEquipmentService::StaticClass()))
	{
		UE_LOG(LogSuspenseCoreServiceLocator, Error,
			TEXT("RegisterServiceClass: %s does not implement USuspenseCoreEquipmentService"),
			*ServiceClass->GetName());
		return false;
	}

	FScopeLock Lock(&RegistryLock);

	if (Registry.Contains(ServiceTag))
	{
		UE_LOG(LogSuspenseCoreServiceLocator, Verbose,
			TEXT("RegisterServiceClass: %s already registered"), *ServiceTag.ToString());
		return false;
	}

	FSuspenseCoreServiceRegistration Reg;
	Reg.ServiceTag = ServiceTag;
	Reg.ServiceClass = ServiceClass;
	Reg.ServiceInstance = nullptr;
	Reg.InitParams = InitParams;
	Reg.InjectionCallback = InjectionCallback;
	Reg.State = ESuspenseCoreServiceLifecycleState::Uninitialized;
	Reg.ReferenceCount = 0;
	Reg.RegistrationTime = FPlatformTime::Seconds();

	Registry.Add(ServiceTag, MoveTemp(Reg));

	UE_LOG(LogSuspenseCoreServiceLocator, Log,
		TEXT("Registered service class: %s (%s)"),
		*ServiceTag.ToString(), *ServiceClass->GetName());

	// Broadcast registration event via EventBus
	BroadcastServiceEvent(SuspenseCoreServiceTags::Registered, ServiceTag, ESuspenseCoreServiceLifecycleState::Uninitialized);

	return true;
}

bool USuspenseCoreEquipmentServiceLocator::RegisterServiceInstance(
	const FGameplayTag& ServiceTag,
	UObject* ServiceInstance,
	const FSuspenseCoreServiceInitParams& InitParams)
{
	if (!ServiceTag.IsValid() || !ServiceInstance)
	{
		UE_LOG(LogSuspenseCoreServiceLocator, Error, TEXT("RegisterServiceInstance: invalid params"));
		return false;
	}

	if (!ServiceInstance->GetClass()->ImplementsInterface(USuspenseCoreEquipmentService::StaticClass()))
	{
		UE_LOG(LogSuspenseCoreServiceLocator, Error,
			TEXT("RegisterServiceInstance: %s does not implement USuspenseCoreEquipmentService"),
			*ServiceInstance->GetName());
		return false;
	}

	FScopeLock Lock(&RegistryLock);

	if (Registry.Contains(ServiceTag))
	{
		UE_LOG(LogSuspenseCoreServiceLocator, Verbose,
			TEXT("RegisterServiceInstance: %s already registered"), *ServiceTag.ToString());
		return false;
	}

	FSuspenseCoreServiceRegistration Reg;
	Reg.ServiceTag = ServiceTag;
	Reg.ServiceClass = ServiceInstance->GetClass();
	Reg.ServiceInstance = ServiceInstance;
	Reg.InitParams = InitParams;
	Reg.State = ESuspenseCoreServiceLifecycleState::Ready;
	Reg.RegistrationTime = FPlatformTime::Seconds();

	Registry.Add(ServiceTag, MoveTemp(Reg));
	ReadySet.Add(ServiceTag);

	UE_LOG(LogSuspenseCoreServiceLocator, Log,
		TEXT("Registered service instance: %s (%s)"),
		*ServiceTag.ToString(), *ServiceInstance->GetName());

	// Broadcast ready event via EventBus
	BroadcastServiceEvent(SuspenseCoreServiceTags::Ready, ServiceTag, ESuspenseCoreServiceLifecycleState::Ready);

	return true;
}

bool USuspenseCoreEquipmentServiceLocator::RegisterServiceFactory(
	const FGameplayTag& ServiceTag,
	TFunction<UObject*(UObject*)> Factory,
	const FSuspenseCoreServiceInitParams& InitParams)
{
	if (!ServiceTag.IsValid() || !Factory)
	{
		UE_LOG(LogSuspenseCoreServiceLocator, Error, TEXT("RegisterServiceFactory: invalid params"));
		return false;
	}

	FScopeLock Lock(&RegistryLock);

	if (Registry.Contains(ServiceTag))
	{
		UE_LOG(LogSuspenseCoreServiceLocator, Verbose,
			TEXT("RegisterServiceFactory: %s already registered"), *ServiceTag.ToString());
		return false;
	}

	FSuspenseCoreServiceRegistration Reg;
	Reg.ServiceTag = ServiceTag;
	Reg.Factory = MoveTemp(Factory);
	Reg.InitParams = InitParams;
	Reg.RegistrationTime = FPlatformTime::Seconds();

	Registry.Add(ServiceTag, MoveTemp(Reg));

	UE_LOG(LogSuspenseCoreServiceLocator, Log,
		TEXT("Registered service factory: %s"), *ServiceTag.ToString());

	// Broadcast registration event
	BroadcastServiceEvent(SuspenseCoreServiceTags::Registered, ServiceTag, ESuspenseCoreServiceLifecycleState::Uninitialized);

	return true;
}

bool USuspenseCoreEquipmentServiceLocator::UnregisterService(const FGameplayTag& ServiceTag, bool bForceShutdown)
{
	FScopeLock Lock(&RegistryLock);

	FSuspenseCoreServiceRegistration* Reg = Registry.Find(ServiceTag);
	if (!Reg) return false;

	if (Reg->State == ESuspenseCoreServiceLifecycleState::Ready ||
		Reg->State == ESuspenseCoreServiceLifecycleState::Initializing)
	{
		ShutdownService(*Reg, bForceShutdown);
	}

	Registry.Remove(ServiceTag);
	ReadySet.Remove(ServiceTag);
	Initializing.Remove(ServiceTag);

	UE_LOG(LogSuspenseCoreServiceLocator, Log,
		TEXT("Unregistered service: %s"), *ServiceTag.ToString());

	return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ACCESS API
// ═══════════════════════════════════════════════════════════════════════════════

UObject* USuspenseCoreEquipmentServiceLocator::GetService(const FGameplayTag& ServiceTag)
{
	if (!ServiceTag.IsValid()) return nullptr;

	FScopeLock Lock(&RegistryLock);

	FSuspenseCoreServiceRegistration* Reg = Registry.Find(ServiceTag);
	if (!Reg)
	{
		UE_LOG(LogSuspenseCoreServiceLocator, Warning,
			TEXT("GetService: %s is not registered"), *ServiceTag.ToString());
		return nullptr;
	}

	if (Reg->State == ESuspenseCoreServiceLifecycleState::Ready && Reg->ServiceInstance)
	{
		Reg->ReferenceCount++;
		return Reg->ServiceInstance;
	}

	if (Reg->State == ESuspenseCoreServiceLifecycleState::Uninitialized)
	{
		if (!InitializeService(*Reg))
		{
			UE_LOG(LogSuspenseCoreServiceLocator, Error,
				TEXT("GetService: failed to initialize %s"), *ServiceTag.ToString());
			return nullptr;
		}
	}

	Reg->ReferenceCount++;
	return Reg->ServiceInstance;
}

UObject* USuspenseCoreEquipmentServiceLocator::TryGetService(const FGameplayTag& ServiceTag) const
{
	FScopeLock Lock(&RegistryLock);

	const FSuspenseCoreServiceRegistration* Reg = Registry.Find(ServiceTag);
	if (!Reg || Reg->State != ESuspenseCoreServiceLifecycleState::Ready) return nullptr;
	return Reg->ServiceInstance;
}

bool USuspenseCoreEquipmentServiceLocator::IsServiceRegistered(const FGameplayTag& ServiceTag) const
{
	FScopeLock Lock(&RegistryLock);
	return Registry.Contains(ServiceTag);
}

bool USuspenseCoreEquipmentServiceLocator::IsServiceReady(const FGameplayTag& ServiceTag) const
{
	FScopeLock Lock(&RegistryLock);
	const FSuspenseCoreServiceRegistration* Reg = Registry.Find(ServiceTag);
	return Reg && Reg->State == ESuspenseCoreServiceLifecycleState::Ready;
}

// ═══════════════════════════════════════════════════════════════════════════════
// LIFECYCLE MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════════════

int32 USuspenseCoreEquipmentServiceLocator::InitializeAllServices()
{
	FScopeLock Lock(&RegistryLock);

	TArray<FGameplayTag> Pending;
	for (const auto& Pair : Registry)
	{
		if (Pair.Value.State == ESuspenseCoreServiceLifecycleState::Uninitialized)
			Pending.Add(Pair.Key);
	}

	Pending = TopoSort(Pending);

	int32 Count = 0;
	for (const FGameplayTag& Tag : Pending)
	{
		FSuspenseCoreServiceRegistration* Reg = Registry.Find(Tag);
		if (Reg && InitializeService(*Reg))
		{
			++Count;
		}
	}
	return Count;
}

int32 USuspenseCoreEquipmentServiceLocator::ShutdownAllServices(bool bForce)
{
	FScopeLock Lock(&RegistryLock);

	TArray<FGameplayTag> Active;
	for (const auto& Pair : Registry)
	{
		if (Pair.Value.State == ESuspenseCoreServiceLifecycleState::Ready)
			Active.Add(Pair.Key);
	}

	Active = TopoSort(Active);
	Algo::Reverse(Active);

	int32 Count = 0;
	for (const FGameplayTag& Tag : Active)
	{
		FSuspenseCoreServiceRegistration* Reg = Registry.Find(Tag);
		if (Reg && ShutdownService(*Reg, bForce))
		{
			++Count;
		}
	}
	return Count;
}

void USuspenseCoreEquipmentServiceLocator::ResetAllServices()
{
	FScopeLock Lock(&RegistryLock);

	for (auto& Pair : Registry)
	{
		FSuspenseCoreServiceRegistration& Reg = Pair.Value;
		Reg.State = ESuspenseCoreServiceLifecycleState::Uninitialized;
		Reg.ReferenceCount = 0;
		Reg.ServiceInstance = nullptr;
	}
	Initializing.Empty();
	ReadySet.Empty();

	UE_LOG(LogSuspenseCoreServiceLocator, Log,
		TEXT("ResetAllServices: all services reset to Uninitialized"));
}

ESuspenseCoreServiceLifecycleState USuspenseCoreEquipmentServiceLocator::GetServiceState(const FGameplayTag& ServiceTag) const
{
	FScopeLock Lock(&RegistryLock);
	const FSuspenseCoreServiceRegistration* Reg = Registry.Find(ServiceTag);
	return Reg ? Reg->State : ESuspenseCoreServiceLifecycleState::Uninitialized;
}

// ═══════════════════════════════════════════════════════════════════════════════
// DEBUG & VALIDATION
// ═══════════════════════════════════════════════════════════════════════════════

FString USuspenseCoreEquipmentServiceLocator::BuildDependencyGraph() const
{
	FScopeLock Lock(&RegistryLock);

	FString Out = TEXT("Service Dependency Graph (SuspenseCore)\n");
	for (const auto& Pair : Registry)
	{
		const FSuspenseCoreServiceRegistration& Reg = Pair.Value;
		Out += FString::Printf(TEXT("- %s [%s]\n"),
			*Reg.ServiceTag.ToString(),
			*UEnum::GetValueAsString(Reg.State));

		const FGameplayTagContainer Deps = GetRequiredDeps_NoLock(Reg);
		for (const FGameplayTag& D : Deps)
		{
			Out += FString::Printf(TEXT("    -> %s\n"), *D.ToString());
		}
	}
	return Out;
}

bool USuspenseCoreEquipmentServiceLocator::ValidateAllServices(TArray<FText>& OutErrors)
{
	OutErrors.Empty();

	FScopeLock Lock(&RegistryLock);

	bool bAllOk = true;

	for (const auto& Pair : Registry)
	{
		const FSuspenseCoreServiceRegistration& Reg = Pair.Value;

		if (Reg.State == ESuspenseCoreServiceLifecycleState::Failed)
		{
			OutErrors.Add(FText::FromString(
				FString::Printf(TEXT("Service %s failed."), *Reg.ServiceTag.ToString())));
			bAllOk = false;
		}

		TSet<FGameplayTag> Visited;
		if (HasCircular(Reg.ServiceTag, Visited))
		{
			OutErrors.Add(FText::FromString(
				FString::Printf(TEXT("Circular deps for %s."), *Reg.ServiceTag.ToString())));
			bAllOk = false;
		}

		if (Reg.ServiceInstance && !ValidateServiceInstance(Reg.ServiceInstance))
		{
			OutErrors.Add(FText::FromString(
				FString::Printf(TEXT("Invalid instance for %s."), *Reg.ServiceTag.ToString())));
			bAllOk = false;
		}
	}

	return bAllOk;
}

TArray<FGameplayTag> USuspenseCoreEquipmentServiceLocator::GetRegisteredServices() const
{
	FScopeLock Lock(&RegistryLock);
	TArray<FGameplayTag> Keys;
	Registry.GetKeys(Keys);
	return Keys;
}

TArray<FGameplayTag> USuspenseCoreEquipmentServiceLocator::GetAllRegisteredServiceTags() const
{
	FScopeLock Lock(&RegistryLock);
	TArray<FGameplayTag> Tags;
	Tags.Reserve(Registry.Num());
	Registry.GetKeys(Tags);
	return Tags;
}

// ═══════════════════════════════════════════════════════════════════════════════
// EVENTBUS INTEGRATION
// ═══════════════════════════════════════════════════════════════════════════════

USuspenseCoreEventBus* USuspenseCoreEquipmentServiceLocator::GetEventBus() const
{
	if (USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(this))
	{
		return Manager->GetEventBus();
	}
	return nullptr;
}

FSuspenseCoreSubscriptionHandle USuspenseCoreEquipmentServiceLocator::SubscribeToServiceEvents(
	const FGameplayTag& ServiceTag,
	const FSuspenseCoreNativeEventCallback& Callback)
{
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		// Subscribe to SuspenseCore.Service.* events filtered by ServiceTag
		return EventBus->SubscribeNative(
			FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Service"))),
			const_cast<USuspenseCoreEquipmentServiceLocator*>(this),
			Callback,
			ESuspenseCoreEventPriority::Normal);
	}
	return FSuspenseCoreSubscriptionHandle();
}

void USuspenseCoreEquipmentServiceLocator::BroadcastServiceEvent(
	const FGameplayTag& EventTag,
	const FGameplayTag& ServiceTag,
	ESuspenseCoreServiceLifecycleState State)
{
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
		EventData.SetString(FName(TEXT("ServiceTag")), ServiceTag.ToString());
		EventData.SetInt(FName(TEXT("State")), static_cast<int32>(State));
		EventData.AddTag(ServiceTag);

		EventBus->Publish(EventTag, EventData);

		if (bDetailedLogging)
		{
			UE_LOG(LogSuspenseCoreServiceLocator, Log,
				TEXT("EventBus: %s -> %s (State: %s)"),
				*EventTag.ToString(),
				*ServiceTag.ToString(),
				*UEnum::GetValueAsString(State));
		}
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

bool USuspenseCoreEquipmentServiceLocator::InitializeService(FSuspenseCoreServiceRegistration& Reg)
{
	// Called only under RegistryLock
	if (ReadySet.Contains(Reg.ServiceTag))
	{
		return true; // Already ready
	}

	if (Initializing.Contains(Reg.ServiceTag))
	{
		UE_LOG(LogSuspenseCoreServiceLocator, Error,
			TEXT("InitializeService: circular init for %s"), *Reg.ServiceTag.ToString());
		Reg.State = ESuspenseCoreServiceLifecycleState::Failed;
		BroadcastServiceEvent(SuspenseCoreServiceTags::Failed, Reg.ServiceTag, Reg.State);
		++TotalFailed;
		return false;
	}

	// 1) Initialize dependencies first (recursively)
	Initializing.Add(Reg.ServiceTag);

	const FGameplayTagContainer Deps = GetRequiredDeps_NoLock(Reg);
	for (const FGameplayTag& DepTag : Deps)
	{
		if (!DepTag.IsValid()) continue;

		FSuspenseCoreServiceRegistration* DepReg = Registry.Find(DepTag);
		if (!DepReg)
		{
			UE_LOG(LogSuspenseCoreServiceLocator, Error,
				TEXT("InitializeService: missing dependency %s for %s"),
				*DepTag.ToString(), *Reg.ServiceTag.ToString());
			Reg.State = ESuspenseCoreServiceLifecycleState::Failed;
			Initializing.Remove(Reg.ServiceTag);
			BroadcastServiceEvent(SuspenseCoreServiceTags::Failed, Reg.ServiceTag, Reg.State);
			++TotalFailed;
			return false;
		}

		if (!InitializeService(*DepReg))
		{
			UE_LOG(LogSuspenseCoreServiceLocator, Error,
				TEXT("InitializeService: dependency %s failed for %s"),
				*DepTag.ToString(), *Reg.ServiceTag.ToString());
			Reg.State = ESuspenseCoreServiceLifecycleState::Failed;
			Initializing.Remove(Reg.ServiceTag);
			BroadcastServiceEvent(SuspenseCoreServiceTags::Failed, Reg.ServiceTag, Reg.State);
			++TotalFailed;
			return false;
		}
	}

	// 2) Create service instance if not yet created
	const double T0 = FPlatformTime::Seconds();

	if (!Reg.ServiceInstance)
	{
		Reg.ServiceInstance = CreateServiceInstance(Reg);
		if (!ValidateServiceInstance(Reg.ServiceInstance))
		{
			UE_LOG(LogSuspenseCoreServiceLocator, Error,
				TEXT("InitializeService: invalid instance for %s"), *Reg.ServiceTag.ToString());
			Reg.State = ESuspenseCoreServiceLifecycleState::Failed;
			Initializing.Remove(Reg.ServiceTag);
			BroadcastServiceEvent(SuspenseCoreServiceTags::Failed, Reg.ServiceTag, Reg.State);
			++TotalFailed;
			return false;
		}
		++TotalCreated;
	}

	Reg.InitParams.ServiceLocator = this;

	// 3) Dependency Injection via callback
	if (!InjectServiceDependencies(Reg))
	{
		UE_LOG(LogSuspenseCoreServiceLocator, Error,
			TEXT("InitializeService: dependency injection failed for %s"), *Reg.ServiceTag.ToString());
		Reg.State = ESuspenseCoreServiceLifecycleState::Failed;
		Initializing.Remove(Reg.ServiceTag);
		BroadcastServiceEvent(SuspenseCoreServiceTags::Failed, Reg.ServiceTag, Reg.State);
		++TotalFailed;
		return false;
	}

	// 4) Call service initialization via ISuspenseCoreEquipmentService
	Reg.State = ESuspenseCoreServiceLifecycleState::Initializing;
	BroadcastServiceEvent(SuspenseCoreServiceTags::Initialized, Reg.ServiceTag, Reg.State);

	ISuspenseCoreEquipmentService* CoreSvc = static_cast<ISuspenseCoreEquipmentService*>(
		Reg.ServiceInstance->GetInterfaceAddress(USuspenseCoreEquipmentService::StaticClass()));

	if (!CoreSvc)
	{
		UE_LOG(LogSuspenseCoreServiceLocator, Error,
			TEXT("Service %s does not implement ISuspenseCoreEquipmentService"), *Reg.ServiceTag.ToString());
		Reg.State = ESuspenseCoreServiceLifecycleState::Failed;
		Initializing.Remove(Reg.ServiceTag);
		BroadcastServiceEvent(SuspenseCoreServiceTags::Failed, Reg.ServiceTag, Reg.State);
		++TotalFailed;
		return false;
	}

	if (!CoreSvc->InitializeService(Reg.InitParams))
	{
		UE_LOG(LogSuspenseCoreServiceLocator, Error,
			TEXT("InitializeService() returned false for %s"), *Reg.ServiceTag.ToString());
		Reg.State = ESuspenseCoreServiceLifecycleState::Failed;
		Initializing.Remove(Reg.ServiceTag);
		BroadcastServiceEvent(SuspenseCoreServiceTags::Failed, Reg.ServiceTag, Reg.State);
		++TotalFailed;
		return false;
	}

	// 5) Mark as ready
	Reg.State = ESuspenseCoreServiceLifecycleState::Ready;
	Initializing.Remove(Reg.ServiceTag);
	ReadySet.Add(Reg.ServiceTag);

	const double Dt = FPlatformTime::Seconds() - T0;
	SumInitTime += static_cast<float>(Dt);
	++TotalInited;

	UE_LOG(LogSuspenseCoreServiceLocator, Log,
		TEXT("Service %s initialized in %.3f s"), *Reg.ServiceTag.ToString(), Dt);

	// Broadcast ready event
	BroadcastServiceEvent(SuspenseCoreServiceTags::Ready, Reg.ServiceTag, Reg.State);

	return true;
}

bool USuspenseCoreEquipmentServiceLocator::InjectServiceDependencies(FSuspenseCoreServiceRegistration& Reg)
{
	if (!Reg.InjectionCallback.IsBound())
	{
		return true;
	}

	UE_LOG(LogSuspenseCoreServiceLocator, Verbose,
		TEXT("InjectServiceDependencies: %s"), *Reg.ServiceTag.ToString());

	Reg.InjectionCallback.Execute(Reg.ServiceInstance, this);

	return true;
}

bool USuspenseCoreEquipmentServiceLocator::ShutdownService(FSuspenseCoreServiceRegistration& Reg, bool bForce)
{
	if (Reg.State == ESuspenseCoreServiceLifecycleState::Shutdown ||
		Reg.State == ESuspenseCoreServiceLifecycleState::Uninitialized)
	{
		return true;
	}

	if (!bForce && Reg.ReferenceCount > 0)
	{
		UE_LOG(LogSuspenseCoreServiceLocator, Warning,
			TEXT("ShutdownService: %s has %d references"),
			*Reg.ServiceTag.ToString(), Reg.ReferenceCount);
		return false;
	}

	Reg.State = ESuspenseCoreServiceLifecycleState::Shutting;
	BroadcastServiceEvent(SuspenseCoreServiceTags::ShuttingDown, Reg.ServiceTag, Reg.State);

	if (Reg.ServiceInstance)
	{
		if (ISuspenseCoreEquipmentService* CoreSvc = static_cast<ISuspenseCoreEquipmentService*>(
			Reg.ServiceInstance->GetInterfaceAddress(USuspenseCoreEquipmentService::StaticClass())))
		{
			CoreSvc->ShutdownService(bForce);
		}
	}

	Reg.ServiceInstance = nullptr;
	ReadySet.Remove(Reg.ServiceTag);
	Reg.State = ESuspenseCoreServiceLifecycleState::Shutdown;

	UE_LOG(LogSuspenseCoreServiceLocator, Log,
		TEXT("Service %s shutdown"), *Reg.ServiceTag.ToString());

	BroadcastServiceEvent(SuspenseCoreServiceTags::Shutdown, Reg.ServiceTag, Reg.State);

	return true;
}

UObject* USuspenseCoreEquipmentServiceLocator::CreateServiceInstance(const FSuspenseCoreServiceRegistration& Reg)
{
	if (Reg.Factory)
	{
		return Reg.Factory(this);
	}

	if (Reg.ServiceClass)
	{
		return NewObject<UObject>(
			GetTransientPackage(),
			Reg.ServiceClass,
			*FString::Printf(TEXT("SuspenseCoreSvc_%s_%08X"),
				*Reg.ServiceTag.ToString(),
				FMath::Rand()),
			RF_NoFlags);
	}

	return nullptr;
}

FSuspenseCoreServiceRegistration* USuspenseCoreEquipmentServiceLocator::FindReg(const FGameplayTag& Tag)
{
	return Registry.Find(Tag);
}

const FSuspenseCoreServiceRegistration* USuspenseCoreEquipmentServiceLocator::FindReg(const FGameplayTag& Tag) const
{
	return Registry.Find(Tag);
}

FGameplayTagContainer USuspenseCoreEquipmentServiceLocator::GetRequiredDeps_NoLock(const FSuspenseCoreServiceRegistration& Reg) const
{
	FGameplayTagContainer Deps = Reg.InitParams.RequiredServices;

	if (Reg.ServiceClass && Reg.ServiceClass->ImplementsInterface(USuspenseCoreEquipmentService::StaticClass()))
	{
		UObject* CDO = Reg.ServiceClass->GetDefaultObject();
		if (CDO)
		{
			ISuspenseCoreEquipmentService* Iface = static_cast<ISuspenseCoreEquipmentService*>(
				CDO->GetInterfaceAddress(USuspenseCoreEquipmentService::StaticClass()));
			if (Iface)
			{
				const FGameplayTagContainer Declared = Iface->GetRequiredDependencies();
				for (const FGameplayTag& Tag : Declared)
				{
					Deps.AddTag(Tag);
				}
			}
		}
	}

	return Deps;
}

TArray<FGameplayTag> USuspenseCoreEquipmentServiceLocator::TopoSort(const TArray<FGameplayTag>& Services) const
{
	TArray<FGameplayTag> Sorted;
	TSet<FGameplayTag> Visited;
	TSet<FGameplayTag> Stack;

	TFunction<void(const FGameplayTag&)> Visit = [&](const FGameplayTag& T)
	{
		if (Visited.Contains(T)) return;
		if (Stack.Contains(T))
		{
			UE_LOG(LogSuspenseCoreServiceLocator, Warning,
				TEXT("TopoSort: cycle at %s"), *T.ToString());
			return;
		}

		Stack.Add(T);

		const FSuspenseCoreServiceRegistration* Reg = Registry.Find(T);
		if (Reg)
		{
			const FGameplayTagContainer Deps = GetRequiredDeps_NoLock(*Reg);
			for (const FGameplayTag& D : Deps)
			{
				if (Services.Contains(D)) Visit(D);
			}
		}

		Stack.Remove(T);
		Visited.Add(T);
		Sorted.Add(T);
	};

	for (const FGameplayTag& T : Services) Visit(T);
	return Sorted;
}

bool USuspenseCoreEquipmentServiceLocator::HasCircular(const FGameplayTag& Tag, TSet<FGameplayTag>& Visited) const
{
	if (Visited.Contains(Tag)) return true;
	Visited.Add(Tag);

	const FSuspenseCoreServiceRegistration* Reg = Registry.Find(Tag);
	if (!Reg) { Visited.Remove(Tag); return false; }

	const FGameplayTagContainer Deps = GetRequiredDeps_NoLock(*Reg);
	for (const FGameplayTag& D : Deps)
	{
		if (HasCircular(D, Visited)) return true;
	}

	Visited.Remove(Tag);
	return false;
}

void USuspenseCoreEquipmentServiceLocator::PerformAutomaticCleanup()
{
	int32 Invalid = 0;

	{
		FScopeLock Lock(&RegistryLock);

		for (auto& Pair : Registry)
		{
			FSuspenseCoreServiceRegistration& Reg = Pair.Value;
			if (Reg.ServiceInstance && !IsValid(Reg.ServiceInstance))
			{
				Reg.ServiceInstance = nullptr;
				Reg.State = ESuspenseCoreServiceLifecycleState::Failed;
				++Invalid;
			}
		}

		CleanupUnusedServices();
	}

	if (Invalid > 0)
	{
		UE_LOG(LogSuspenseCoreServiceLocator, Warning,
			TEXT("Cleanup: %d invalid instances cleared"), Invalid);
	}
}

int32 USuspenseCoreEquipmentServiceLocator::CleanupUnusedServices()
{
	int32 Removed = 0;

	for (auto It = Registry.CreateIterator(); It; ++It)
	{
		FSuspenseCoreServiceRegistration& Reg = It->Value;
		if (Reg.ReferenceCount == 0 &&
			Reg.State == ESuspenseCoreServiceLifecycleState::Ready &&
			!Reg.InitParams.bAutoStart)
		{
			ShutdownService(Reg, false);
			It.RemoveCurrent();
			++Removed;
		}
	}

	if (Removed > 0)
	{
		UE_LOG(LogSuspenseCoreServiceLocator, Log,
			TEXT("CleanupUnusedServices: %d removed"), Removed);
	}

	return Removed;
}

bool USuspenseCoreEquipmentServiceLocator::ValidateServiceInstance(const UObject* ServiceInstance) const
{
	if (!IsValid(ServiceInstance))
	{
		return false;
	}

	const UClass* Cls = ServiceInstance->GetClass();
	if (!Cls)
	{
		return false;
	}

	// Must implement ISuspenseCoreEquipmentService
	if (!Cls->ImplementsInterface(USuspenseCoreEquipmentService::StaticClass()))
	{
		return false;
	}

	return true;
}
