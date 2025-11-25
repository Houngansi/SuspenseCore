// Copyright Suspense Team. All Rights Reserved.

#include "Core/Services/SuspenseEquipmentServiceLocator.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogServiceLocator);

USuspenseEquipmentServiceLocator* USuspenseEquipmentServiceLocator::Get(const UObject* WorldContext)
{
    if (!WorldContext) return nullptr;
    const UWorld* World = WorldContext->GetWorld();
    if (!World) return nullptr;
    UGameInstance* GI = World->GetGameInstance();
    return GI ? GI->GetSubsystem<USuspenseEquipmentServiceLocator>() : nullptr;
}

void USuspenseEquipmentServiceLocator::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Periodic cleanup (no cross-module work here)
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            CleanupTimer,
            this,
            &USuspenseEquipmentServiceLocator::PerformAutomaticCleanup,
            CleanupInterval,
            true);
    }

    UE_LOG(LogServiceLocator, Log, TEXT("ServiceLocator initialized (Shared)."));
}

void USuspenseEquipmentServiceLocator::Deinitialize()
{
    UE_LOG(LogServiceLocator, Log, TEXT("ServiceLocator deinitializing..."));

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(CleanupTimer);
    }

    ShutdownAllServices(false);

    {
        FScopeLock _( &RegistryLock );
        Registry.Empty();
        Initializing.Empty();
        ReadySet.Empty();
    }

    Super::Deinitialize();
    UE_LOG(LogServiceLocator, Log, TEXT("ServiceLocator deinitialized."));
}

bool USuspenseEquipmentServiceLocator::RegisterServiceClass(const FGameplayTag& ServiceTag,
                                                    TSubclassOf<UObject> ServiceClass,
                                                    const FServiceInitParams& InitParams)
{
    return RegisterServiceClassWithInjection(ServiceTag, ServiceClass, InitParams, FServiceInjectionDelegate());
}

bool USuspenseEquipmentServiceLocator::RegisterServiceClassWithInjection(const FGameplayTag& ServiceTag,
                                                                 TSubclassOf<UObject> ServiceClass,
                                                                 const FServiceInitParams& InitParams,
                                                                 const FServiceInjectionDelegate& InjectionCallback)
{
    if (!ServiceTag.IsValid() || !ServiceClass)
    {
        UE_LOG(LogServiceLocator, Error, TEXT("RegisterServiceClass: invalid params"));
        return false;
    }

    // Class must implement UEquipmentService interface
    if (!ServiceClass->ImplementsInterface(UEquipmentService::StaticClass()))
    {
        UE_LOG(LogServiceLocator, Error, TEXT("RegisterServiceClass: %s does not implement UEquipmentService"),
            *ServiceClass->GetName());
        return false;
    }

    FScopeLock _( &RegistryLock );

    if (Registry.Contains(ServiceTag))
    {
        UE_LOG(LogServiceLocator, Verbose, TEXT("RegisterServiceClass: %s already registered"), *ServiceTag.ToString());
        return false;
    }

    FServiceRegistration Reg;
    Reg.ServiceTag        = ServiceTag;
    Reg.ServiceClass      = ServiceClass;
    Reg.ServiceInstance   = nullptr;
    Reg.InitParams        = InitParams;
    Reg.InjectionCallback = InjectionCallback;
    Reg.State             = EServiceLifecycleState::Uninitialized;
    Reg.ReferenceCount    = 0;
    Reg.RegistrationTime  = FPlatformTime::Seconds();

    Registry.Add(ServiceTag, MoveTemp(Reg));

    UE_LOG(LogServiceLocator, Log, TEXT("Registered service class: %s (%s)"),
        *ServiceTag.ToString(), *ServiceClass->GetName());

    return true;
}

bool USuspenseEquipmentServiceLocator::RegisterServiceInstance(const FGameplayTag& ServiceTag,
                                                       UObject* ServiceInstance,
                                                       const FServiceInitParams& InitParams)
{
    if (!ServiceTag.IsValid() || !ServiceInstance)
    {
        UE_LOG(LogServiceLocator, Error, TEXT("RegisterServiceInstance: invalid params"));
        return false;
    }

    if (!ServiceInstance->GetClass()->ImplementsInterface(UEquipmentService::StaticClass()))
    {
        UE_LOG(LogServiceLocator, Error, TEXT("RegisterServiceInstance: %s does not implement UEquipmentService"),
            *ServiceInstance->GetName());
        return false;
    }

    FScopeLock _( &RegistryLock );

    if (Registry.Contains(ServiceTag))
    {
        UE_LOG(LogServiceLocator, Verbose, TEXT("RegisterServiceInstance: %s already registered"), *ServiceTag.ToString());
        return false;
    }

    FServiceRegistration Reg;
    Reg.ServiceTag     = ServiceTag;
    Reg.ServiceClass   = ServiceInstance->GetClass();
    Reg.ServiceInstance= ServiceInstance;
    Reg.InitParams     = InitParams;
    Reg.State          = EServiceLifecycleState::Ready;
    Reg.RegistrationTime = FPlatformTime::Seconds();

    Registry.Add(ServiceTag, MoveTemp(Reg));

    UE_LOG(LogServiceLocator, Log, TEXT("Registered service instance: %s (%s)"),
        *ServiceTag.ToString(), *ServiceInstance->GetName());

    return true;
}

bool USuspenseEquipmentServiceLocator::RegisterServiceFactory(const FGameplayTag& ServiceTag,
                                                      TFunction<UObject*(UObject*)> Factory,
                                                      const FServiceInitParams& InitParams)
{
    if (!ServiceTag.IsValid() || !Factory)
    {
        UE_LOG(LogServiceLocator, Error, TEXT("RegisterServiceFactory: invalid params"));
        return false;
    }

    FScopeLock _( &RegistryLock );

    if (Registry.Contains(ServiceTag))
    {
        UE_LOG(LogServiceLocator, Verbose, TEXT("RegisterServiceFactory: %s already registered"), *ServiceTag.ToString());
        return false;
    }

    FServiceRegistration Reg;
    Reg.ServiceTag    = ServiceTag;
    Reg.Factory       = MoveTemp(Factory);
    Reg.InitParams    = InitParams;
    Reg.RegistrationTime = FPlatformTime::Seconds();

    Registry.Add(ServiceTag, MoveTemp(Reg));

    UE_LOG(LogServiceLocator, Log, TEXT("Registered service factory: %s"), *ServiceTag.ToString());
    return true;
}

bool USuspenseEquipmentServiceLocator::UnregisterService(const FGameplayTag& ServiceTag, bool bForceShutdown)
{
    FScopeLock _( &RegistryLock );

    FServiceRegistration* Reg = Registry.Find(ServiceTag);
    if (!Reg) return false;

    if (Reg->State == EServiceLifecycleState::Ready || Reg->State == EServiceLifecycleState::Initializing)
    {
        ShutdownService(*Reg, bForceShutdown);
    }

    Registry.Remove(ServiceTag);
    ReadySet.Remove(ServiceTag);
    Initializing.Remove(ServiceTag);

    UE_LOG(LogServiceLocator, Log, TEXT("Unregistered service: %s"), *ServiceTag.ToString());
    return true;
}

UObject* USuspenseEquipmentServiceLocator::GetService(const FGameplayTag& ServiceTag)
{
    if (!ServiceTag.IsValid()) return nullptr;

    FScopeLock _( &RegistryLock );

    FServiceRegistration* Reg = Registry.Find(ServiceTag);
    if (!Reg)
    {
        UE_LOG(LogServiceLocator, Warning, TEXT("GetService: %s is not registered"), *ServiceTag.ToString());
        return nullptr;
    }

    if (Reg->State == EServiceLifecycleState::Ready && Reg->ServiceInstance)
    {
        Reg->ReferenceCount++;
        return Reg->ServiceInstance;
    }

    if (Reg->State == EServiceLifecycleState::Uninitialized)
    {
        if (!InitializeService(*Reg))
        {
            UE_LOG(LogServiceLocator, Error, TEXT("GetService: failed to initialize %s"), *ServiceTag.ToString());
            return nullptr;
        }
    }

    Reg->ReferenceCount++;
    return Reg->ServiceInstance;
}

UObject* USuspenseEquipmentServiceLocator::TryGetService(const FGameplayTag& ServiceTag) const
{
    FScopeLock _( &RegistryLock );

    const FServiceRegistration* Reg = Registry.Find(ServiceTag);
    if (!Reg || Reg->State != EServiceLifecycleState::Ready) return nullptr;
    return Reg->ServiceInstance;
}

bool USuspenseEquipmentServiceLocator::IsServiceRegistered(const FGameplayTag& ServiceTag) const
{
    FScopeLock _( &RegistryLock );
    return Registry.Contains(ServiceTag);
}

bool USuspenseEquipmentServiceLocator::IsServiceReady(const FGameplayTag& ServiceTag) const
{
    FScopeLock _( &RegistryLock );
    const FServiceRegistration* Reg = Registry.Find(ServiceTag);
    return Reg && Reg->State == EServiceLifecycleState::Ready;
}

int32 USuspenseEquipmentServiceLocator::InitializeAllServices()
{
    FScopeLock _( &RegistryLock );

    TArray<FGameplayTag> Pending;
    for (const auto& Pair : Registry)
    {
        if (Pair.Value.State == EServiceLifecycleState::Uninitialized)
            Pending.Add(Pair.Key);
    }

    Pending = TopoSort(Pending);

    int32 Count = 0;
    for (const FGameplayTag& Tag : Pending)
    {
        FServiceRegistration* Reg = Registry.Find(Tag);
        if (Reg && InitializeService(*Reg))
        {
            ++Count;
        }
    }
    return Count;
}

int32 USuspenseEquipmentServiceLocator::ShutdownAllServices(bool bForce)
{
    FScopeLock _( &RegistryLock );

    TArray<FGameplayTag> Active;
    for (const auto& Pair : Registry)
    {
        if (Pair.Value.State == EServiceLifecycleState::Ready)
            Active.Add(Pair.Key);
    }

    Active = TopoSort(Active);
    Algo::Reverse(Active);

    int32 Count = 0;
    for (const FGameplayTag& Tag : Active)
    {
        FServiceRegistration* Reg = Registry.Find(Tag);
        if (Reg && ShutdownService(*Reg, bForce))
        {
            ++Count;
        }
    }
    return Count;
}

void USuspenseEquipmentServiceLocator::ResetAllServices()
{
    FScopeLock _( &RegistryLock );

    for (auto& Pair : Registry)
    {
        FServiceRegistration& Reg = Pair.Value;
        Reg.State = EServiceLifecycleState::Uninitialized;
        Reg.ReferenceCount = 0;
        Reg.ServiceInstance = nullptr;
    }
    Initializing.Empty();
    ReadySet.Empty();

    UE_LOG(LogServiceLocator, Log, TEXT("ResetAllServices: all services reset to Uninitialized"));
}

EServiceLifecycleState USuspenseEquipmentServiceLocator::GetServiceState(const FGameplayTag& ServiceTag) const
{
    FScopeLock _( &RegistryLock );
    const FServiceRegistration* Reg = Registry.Find(ServiceTag);
    return Reg ? Reg->State : EServiceLifecycleState::Uninitialized;
}

FString USuspenseEquipmentServiceLocator::BuildDependencyGraph() const
{
    FScopeLock _( &RegistryLock );

    FString Out = TEXT("Service Dependency Graph\n");
    for (const auto& Pair : Registry)
    {
        const FServiceRegistration& Reg = Pair.Value;
        Out += FString::Printf(TEXT("- %s [%s]\n"),
            *Reg.ServiceTag.ToString(), *UEnum::GetValueAsString(Reg.State));

        const FGameplayTagContainer Deps = GetRequiredDeps_NoLock(Reg);
        for (const FGameplayTag& D : Deps)
        {
            Out += FString::Printf(TEXT("    -> %s\n"), *D.ToString());
        }
    }
    return Out;
}

bool USuspenseEquipmentServiceLocator::ValidateAllServices(TArray<FText>& OutErrors)
{
    OutErrors.Empty();

    FScopeLock _( &RegistryLock );

    bool bAllOk = true;

    for (const auto& Pair : Registry)
    {
        const FServiceRegistration& Reg = Pair.Value;

        if (Reg.State == EServiceLifecycleState::Failed)
        {
            OutErrors.Add(FText::FromString(FString::Printf(TEXT("Service %s failed."), *Reg.ServiceTag.ToString())));
            bAllOk = false;
        }

        TSet<FGameplayTag> Visited;
        if (HasCircular(Reg.ServiceTag, Visited))
        {
            OutErrors.Add(FText::FromString(FString::Printf(TEXT("Circular deps for %s."), *Reg.ServiceTag.ToString())));
            bAllOk = false;
        }

        if (Reg.ServiceInstance && !ValidateServiceInstance(Reg.ServiceInstance))
        {
            OutErrors.Add(FText::FromString(FString::Printf(TEXT("Invalid instance for %s."), *Reg.ServiceTag.ToString())));
            bAllOk = false;
        }
    }

    return bAllOk;
}

TArray<FGameplayTag> USuspenseEquipmentServiceLocator::GetRegisteredServices() const
{
    FScopeLock _( &RegistryLock );
    TArray<FGameplayTag> Keys;
    Registry.GetKeys(Keys);
    return Keys;
}

TArray<FGameplayTag> USuspenseEquipmentServiceLocator::GetAllRegisteredServiceTags() const
{
    // Same as GetRegisteredServices() but with explicit naming for clarity
    FScopeLock _( &RegistryLock );
    TArray<FGameplayTag> Tags;
    Tags.Reserve(Registry.Num());
    Registry.GetKeys(Tags);
    return Tags;
}

// -------- internals --------

bool USuspenseEquipmentServiceLocator::InitializeService(FServiceRegistration& Reg)
{
    // Called only under RegistryLock (see GetService/InitializeAllServices)
    if (ReadySet.Contains(Reg.ServiceTag))
    {
        return true; // Already ready
    }

    if (Initializing.Contains(Reg.ServiceTag))
    {
        UE_LOG(LogServiceLocator, Error, TEXT("InitializeService: circular init for %s"),
            *Reg.ServiceTag.ToString());
        Reg.State = EServiceLifecycleState::Failed;
        ++TotalFailed;
        return false;
    }

    // 1) Initialize dependencies first (recursively)
    Initializing.Add(Reg.ServiceTag);

    const FGameplayTagContainer Deps = GetRequiredDeps_NoLock(Reg);
    for (const FGameplayTag& DepTag : Deps)
    {
        if (!DepTag.IsValid()) continue;

        FServiceRegistration* DepReg = Registry.Find(DepTag);
        if (!DepReg)
        {
            UE_LOG(LogServiceLocator, Error, TEXT("InitializeService: missing dependency %s for %s"),
                *DepTag.ToString(), *Reg.ServiceTag.ToString());
            Reg.State = EServiceLifecycleState::Failed;
            Initializing.Remove(Reg.ServiceTag);
            ++TotalFailed;
            return false;
        }

        if (!InitializeService(*DepReg))
        {
            UE_LOG(LogServiceLocator, Error, TEXT("InitializeService: dependency %s failed for %s"),
                *DepTag.ToString(), *Reg.ServiceTag.ToString());
            Reg.State = EServiceLifecycleState::Failed;
            Initializing.Remove(Reg.ServiceTag);
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
            UE_LOG(LogServiceLocator, Error, TEXT("InitializeService: invalid instance for %s"),
                *Reg.ServiceTag.ToString());
            Reg.State = EServiceLifecycleState::Failed;
            Initializing.Remove(Reg.ServiceTag);
            ++TotalFailed;
            return false;
        }
        ++TotalCreated;
    }
    
    Reg.InitParams.ServiceLocator = this;
    
    // 3) Dependency Injection strictly via callback (no cross-module symbols here)
    if (!InjectServiceDependencies(Reg))
    {
        UE_LOG(LogServiceLocator, Error, TEXT("InitializeService: dependency injection failed for %s"),
            *Reg.ServiceTag.ToString());
        Reg.State = EServiceLifecycleState::Failed;
        Initializing.Remove(Reg.ServiceTag);
        ++TotalFailed;
        return false;
    }

    // 4) Call service initialization
    if (ISuspenseEquipmentService* Svc = static_cast<ISuspenseEquipmentService*>(
               Reg.ServiceInstance->GetInterfaceAddress(UEquipmentService::StaticClass())))
    {
        Reg.State = EServiceLifecycleState::Initializing;

        //Pass params with ServiceLocator reference
        if (!Svc->InitializeService(Reg.InitParams))
        {
            UE_LOG(LogServiceLocator, Error, TEXT("InitializeService() returned false for %s"),
                *Reg.ServiceTag.ToString());
            Reg.State = EServiceLifecycleState::Failed;
            Initializing.Remove(Reg.ServiceTag);
            ++TotalFailed;
            return false;
        }
    }
    else
    {
        UE_LOG(LogServiceLocator, Error, TEXT("Service %s does not implement UEquipmentService"),
            *Reg.ServiceTag.ToString());
        Reg.State = EServiceLifecycleState::Failed;
        Initializing.Remove(Reg.ServiceTag);
        ++TotalFailed;
        return false;
    }

    // 5) Mark as ready
    Reg.State = EServiceLifecycleState::Ready;
    Initializing.Remove(Reg.ServiceTag);
    ReadySet.Add(Reg.ServiceTag);

    const double Dt = FPlatformTime::Seconds() - T0;
    SumInitTime += (float)Dt;
    ++TotalInited;

    UE_LOG(LogServiceLocator, Log, TEXT("Service %s initialized in %.3f s"),
        *Reg.ServiceTag.ToString(), Dt);
    return true;
}

bool USuspenseEquipmentServiceLocator::InjectServiceDependencies(FServiceRegistration& Reg)
{
    if (!Reg.InjectionCallback.IsBound())
    {
        // No injection callback = assume service has no external dependencies
        return true;
    }

    UE_LOG(LogServiceLocator, Verbose, TEXT("InjectServiceDependencies: %s"), *Reg.ServiceTag.ToString());
    
    Reg.InjectionCallback.Execute(Reg.ServiceInstance, this);
    
    return true;
}

bool USuspenseEquipmentServiceLocator::ShutdownService(FServiceRegistration& Reg, bool bForce)
{
    if (Reg.State == EServiceLifecycleState::Shutdown ||
        Reg.State == EServiceLifecycleState::Uninitialized)
    {
        return true;
    }

    if (!bForce && Reg.ReferenceCount > 0)
    {
        UE_LOG(LogServiceLocator, Warning, TEXT("ShutdownService: %s has %d references"), *Reg.ServiceTag.ToString(), Reg.ReferenceCount);
        return false;
    }

    Reg.State = EServiceLifecycleState::Shutting;

    if (Reg.ServiceInstance &&
        Reg.ServiceInstance->GetClass()->ImplementsInterface(UEquipmentService::StaticClass()))
    {
        ISuspenseEquipmentService* Svc = static_cast<ISuspenseEquipmentService*>(Reg.ServiceInstance->GetInterfaceAddress(UEquipmentService::StaticClass()));
        Svc->ShutdownService(bForce);
    }

    Reg.ServiceInstance = nullptr;
    ReadySet.Remove(Reg.ServiceTag);
    Reg.State = EServiceLifecycleState::Shutdown;

    UE_LOG(LogServiceLocator, Log, TEXT("Service %s shutdown"), *Reg.ServiceTag.ToString());
    return true;
}

UObject* USuspenseEquipmentServiceLocator::CreateServiceInstance(const FServiceRegistration& Reg)
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
            *FString::Printf(TEXT("Svc_%s_%08X"),
                *Reg.ServiceTag.ToString(),
                FMath::Rand()),
            RF_NoFlags);
    }

    return nullptr;
}

FServiceRegistration* USuspenseEquipmentServiceLocator::FindReg(const FGameplayTag& Tag)
{
    return Registry.Find(Tag);
}
const FServiceRegistration* USuspenseEquipmentServiceLocator::FindReg(const FGameplayTag& Tag) const
{
    return Registry.Find(Tag);
}

FGameplayTagContainer USuspenseEquipmentServiceLocator::GetRequiredDeps_NoLock(const FServiceRegistration& Reg) const
{
    // Base: take deps from registration params
    FGameplayTagContainer Deps = Reg.InitParams.RequiredServices;

    if (Reg.ServiceClass && Reg.ServiceClass->ImplementsInterface(UEquipmentService::StaticClass()))
    {
        // IMPORTANT: GetDefaultObject() as UObject* (non-const) to call GetInterfaceAddress(...)
        UObject* CDO = Reg.ServiceClass->GetDefaultObject();
        if (CDO)
        {
            ISuspenseEquipmentService* Iface =
                static_cast<ISuspenseEquipmentService*>(CDO->GetInterfaceAddress(UEquipmentService::StaticClass()));
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

TArray<FGameplayTag> USuspenseEquipmentServiceLocator::TopoSort(const TArray<FGameplayTag>& Services) const
{
    TArray<FGameplayTag> Sorted;
    TSet<FGameplayTag> Visited;
    TSet<FGameplayTag> Stack;

    TFunction<void(const FGameplayTag&)> Visit = [&](const FGameplayTag& T)
    {
        if (Visited.Contains(T)) return;
        if (Stack.Contains(T))
        {
            UE_LOG(LogServiceLocator, Warning, TEXT("TopoSort: cycle at %s"), *T.ToString());
            return;
        }

        Stack.Add(T);

        const FServiceRegistration* Reg = Registry.Find(T);
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

bool USuspenseEquipmentServiceLocator::HasCircular(const FGameplayTag& Tag, TSet<FGameplayTag>& Visited) const
{
    if (Visited.Contains(Tag)) return true;
    Visited.Add(Tag);

    const FServiceRegistration* Reg = Registry.Find(Tag);
    if (!Reg) { Visited.Remove(Tag); return false; }

    const FGameplayTagContainer Deps = GetRequiredDeps_NoLock(*Reg);
    for (const FGameplayTag& D : Deps)
    {
        if (HasCircular(D, Visited)) return true;
    }

    Visited.Remove(Tag);
    return false;
}

void USuspenseEquipmentServiceLocator::PerformAutomaticCleanup()
{
    int32 Invalid = 0;

    {
        FScopeLock _( &RegistryLock );

        // Remove invalid instances
        for (auto& Pair : Registry)
        {
            FServiceRegistration& Reg = Pair.Value;
            if (Reg.ServiceInstance && !IsValid(Reg.ServiceInstance))
            {
                Reg.ServiceInstance = nullptr;
                Reg.State = EServiceLifecycleState::Failed;
                ++Invalid;
            }
        }

        CleanupUnusedServices();
    }

    if (Invalid > 0)
    {
        UE_LOG(LogServiceLocator, Warning, TEXT("Cleanup: %d invalid instances cleared"), Invalid);
    }
}

int32 USuspenseEquipmentServiceLocator::CleanupUnusedServices()
{
    int32 Removed = 0;

    for (auto It = Registry.CreateIterator(); It; ++It)
    {
        FServiceRegistration& Reg = It->Value;
        if (Reg.ReferenceCount == 0 &&
            Reg.State == EServiceLifecycleState::Ready &&
            !Reg.InitParams.bAutoStart)
        {
            ShutdownService(Reg, false);
            It.RemoveCurrent();
            ++Removed;
        }
    }
    if (Removed > 0)
    {
        UE_LOG(LogServiceLocator, Log, TEXT("CleanupUnusedServices: %d removed"), Removed);
    }
    return Removed;
}

bool USuspenseEquipmentServiceLocator::ValidateServiceInstance(const UObject* ServiceInstance) const
{
    // Reliable UObject liveness check in UE5
    if (!IsValid(ServiceInstance))
    {
        return false;
    }

    const UClass* Cls = ServiceInstance->GetClass();
    if (!Cls || !Cls->ImplementsInterface(UEquipmentService::StaticClass()))
    {
        return false;
    }

    return true;
}