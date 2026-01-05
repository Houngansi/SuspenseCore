// SuspenseCoreSystemCoordinator.cpp
// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Subsystems/SuspenseCoreSystemCoordinator.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineTypes.h"
#include "Misc/ScopeExit.h"
#include "HAL/IConsoleManager.h"

// Core interfaces
#include "SuspenseCore/Interfaces/Core/SuspenseCoreWorldBindable.h"

// Equipment infrastructure
#include "SuspenseCore/Subsystems/SuspenseCoreSystemCoordinator.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceLocator.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentService.h"

// Equipment Services (for subsystem-level registration)
#include "SuspenseCore/Services/SuspenseCoreEquipmentDataService.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentValidationService.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentOperationService.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentVisualizationService.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentAbilityService.h"
#include "SuspenseCore/Services/SuspenseCoreAmmoLoadingService.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"

// ItemManager for DataService injection
#include "SuspenseCore/ItemSystem/SuspenseCoreItemManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreCoordinatorSubsystem, Log, All);

//========================================
// Helper Functions
//========================================

namespace
{
    // Helper function to convert ENetMode to string without StaticEnum issues
    FString GetNetModeString(ENetMode NetMode)
    {
        switch (NetMode)
        {
            case NM_Standalone: return TEXT("Standalone");
            case NM_DedicatedServer: return TEXT("DedicatedServer");
            case NM_ListenServer: return TEXT("ListenServer");
            case NM_Client: return TEXT("Client");
            default: return TEXT("Unknown");
        }
    }
}
static const TCHAR* NetModeToString(const ENetMode InMode)
{
    switch (InMode)
    {
    case NM_Standalone: return TEXT("Standalone");
    case NM_DedicatedServer: return TEXT("DedicatedServer");
    case NM_ListenServer: return TEXT("ListenServer");
    case NM_Client: return TEXT("Client");
    default: return TEXT("Unknown");
    }
}
//========================================
// Construction
//========================================

USuspenseCoreSystemCoordinator::USuspenseCoreSystemCoordinator()
{
    // Subsystem created by UE automatically
}

//========================================
// USubsystem Interface
//========================================

bool USuspenseCoreSystemCoordinator::ShouldCreateSubsystem(UObject* Outer) const
{
    // Гарантируем порядок: сперва создаётся ServiceLocator (GI Subsystem), затем — координатор.
    if (UGameInstance* GI = Cast<UGameInstance>(Outer))
    {
        (void)GI->GetSubsystem<USuspenseCoreEquipmentServiceLocator>(); // намеренно ради side-effect, чтобы не плодить warning
    }
    return Super::ShouldCreateSubsystem(Outer);
}

void USuspenseCoreSystemCoordinator::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Log, TEXT("Initialize subsystem"));
    check(IsInGameThread());

    // 1) Получаем локатор (ServiceLocator is also a GI Subsystem)
    if (UGameInstance* GI = GetGameInstance())
    {
        ServiceLocator = GI->GetSubsystem<USuspenseCoreEquipmentServiceLocator>();
    }

    if (!ServiceLocator)
    {
        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Error,
            TEXT("ServiceLocator subsystem not found! Ensure USuspenseCoreEquipmentServiceLocator is properly configured."));
    }
    else
    {
        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Log, TEXT("ServiceLocator acquired from GameInstance"));
    }

    // 2) Регистрация/прогрев/валидация (this IS the coordinator)
    EnsureServicesRegistered(TryGetCurrentWorldSafe());
    ValidateAndLog();

    // 4) Подписки на жизненный цикл мира
    PostWorldInitHandle = FWorldDelegates::OnPostWorldInitialization.AddUObject(
        this, &USuspenseCoreSystemCoordinator::OnPostWorldInitialization);

    PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
        this, &USuspenseCoreSystemCoordinator::OnPostLoadMapWithWorld);

    UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Log, TEXT("Subsystem Initialize() complete. ServicesReady=%s"),
        bServicesReady ? TEXT("YES") : TEXT("NO"));
}

void USuspenseCoreSystemCoordinator::Deinitialize()
{
    UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Log, TEXT("Deinitialize subsystem"));
    check(IsInGameThread());

    if (PostWorldInitHandle.IsValid())
    {
        FWorldDelegates::OnPostWorldInitialization.Remove(PostWorldInitHandle);
        PostWorldInitHandle.Reset();
    }
    if (PostLoadMapHandle.IsValid())
    {
        FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
        PostLoadMapHandle.Reset();
    }

    // Shutdown services
    Shutdown();

    ServiceLocator = nullptr;
    bServicesRegistered = false;
    bServicesReady = false;

    Super::Deinitialize();
}


//========================================
// World Lifecycle Handlers
//========================================

void USuspenseCoreSystemCoordinator::OnPostWorldInitialization(UWorld* World, const UWorld::InitializationValues IVS)
{
    check(IsInGameThread());
    if (!World || World->IsPreviewWorld() || World->IsEditorWorld())
    {
        return;
    }

    UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Log, TEXT("OnPostWorldInitialization: %s (NetMode=%s, Ptr=%p)"),
        *World->GetName(), NetModeToString(World->GetNetMode()), World);

    EnsureServicesRegistered(World);
}

void USuspenseCoreSystemCoordinator::OnPostLoadMapWithWorld(UWorld* LoadedWorld)
{
    check(IsInGameThread());
    if (!LoadedWorld || LoadedWorld->IsPreviewWorld() || LoadedWorld->IsEditorWorld())
    {
        return;
    }

    RebindAllWorldBindableServices(LoadedWorld);
    ValidateAndLog();
}

//========================================
// Internal Operations
//========================================

void USuspenseCoreSystemCoordinator::EnsureServicesRegistered(UWorld* ForWorld)
{
    check(IsInGameThread());

    if (!ServiceLocator)
    {
        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Error, TEXT("EnsureServicesRegistered: ServiceLocator is null"));
        return;
    }

    if (!bServicesRegistered)
    {
        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Log, TEXT("=== RegisterCoreServices BEGIN ==="));
        RegisterCoreServices();   // Call directly on this subsystem
        WarmUpServices();         // Warm up caches/subscriptions
        bServicesRegistered = true;
    }

    // Инициализация ленивых сервисов (если требуется)
    const int32 Inited = ServiceLocator->InitializeAllServices();
    if (Inited > 0)
    {
        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Log, TEXT("Services warmed up (%d initialized)"), Inited);
    }

    // Первая привязка к миру
    RebindAllWorldBindableServices(ForWorld ? ForWorld : TryGetCurrentWorldSafe());
}

void USuspenseCoreSystemCoordinator::RebindAllWorldBindableServices(UWorld* ForWorld)
{
    check(IsInGameThread());

    if (!ForWorld)
    {
        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Warning, TEXT("RebindAllWorldBindableServices: ForWorld is nullptr"));
        return;
    }
    if (!ServiceLocator)
    {
        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Error, TEXT("RebindAllWorldBindableServices: ServiceLocator is nullptr"));
        return;
    }
    if (bRebindInProgress)
    {
        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Verbose, TEXT("RebindAllWorldBindableServices: skip (in progress)"));
        return;
    }

    bRebindInProgress = true;
    ON_SCOPE_EXIT { bRebindInProgress = false; };

    UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Log,
        TEXT("RebindAllWorldBindableServices: %s (NetMode=%s, Ptr=%p)"),
        *ForWorld->GetName(), NetModeToString(ForWorld->GetNetMode()), ForWorld);

    const TArray<FGameplayTag> AllTags = ServiceLocator->GetAllRegisteredServiceTags();

    int32 Rebound = 0;
    int32 Skipped = 0;

    for (const FGameplayTag& Tag : AllTags)
    {
        UObject* SvcObj = ServiceLocator->TryGetService(Tag);
        if (!SvcObj) { ++Skipped; continue; }

        if (SvcObj->GetClass()->ImplementsInterface(USuspenseCoreWorldBindable::StaticClass()))
        {
            // Pure C++ call (no UFUNCTION-Execute - we don't need BP reflection here)
            if (ISuspenseCoreWorldBindable* Iface = Cast<ISuspenseCoreWorldBindable>(SvcObj))
            {
                Iface->RebindWorld(ForWorld);
            }
            ++Rebound;
        }
        else
        {
            ++Skipped;
        }
    }

    ++RebindCount;
    LastBoundWorld = ForWorld;

    UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Log,
        TEXT("RebindAllWorldBindableServices: complete (Rebound=%d, Skipped=%d, Total=%d)"),
        Rebound, Skipped, AllTags.Num());
}

void USuspenseCoreSystemCoordinator::ValidateAndLog()
{
    check(IsInGameThread());
    bServicesReady = false;

    if (!ServiceLocator)
    {
        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Warning, TEXT("ValidateServices: ServiceLocator is null"));
        return;
    }

    TArray<FText> Errors;
    const bool bOk = ValidateServices(Errors);

    if (bOk && Errors.Num() == 0)
    {
        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Log, TEXT("ValidateServices: OK"));
        bServicesReady = true;
    }
    else
    {
        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Warning, TEXT("ValidateServices: %d issues detected"), Errors.Num());
        for (const FText& E : Errors)
        {
            UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Warning, TEXT("  - %s"), *E.ToString());
        }
    }
}


UWorld* USuspenseCoreSystemCoordinator::TryGetCurrentWorldSafe() const
{
    if (const UGameInstance* GI = GetGameInstance())
    {
        return GI->GetWorld();
    }
    return nullptr;
}


//========================================
// Public API
//========================================

void USuspenseCoreSystemCoordinator::ForceRebindWorld(UWorld* World)
{
    if (!World)
    {
        World = TryGetCurrentWorldSafe();
    }

    if (!World)
    {
        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Warning, TEXT("ForceRebindWorld: no valid world"));
        return;
    }

    UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Log, TEXT("ForceRebindWorld: manually triggered for %s"), *World->GetName());

    RebindAllWorldBindableServices(World);
}

//========================================
// Debug Commands
//========================================

void USuspenseCoreSystemCoordinator::DebugDumpServicesState()
{
    UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Display, TEXT(""));
    UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Display, TEXT("=== EQUIPMENT SERVICES STATE ==="));
    UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Display, TEXT(""));

    // Subsystem status
    UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Display, TEXT("Subsystem Status:"));
    UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Display, TEXT("  Services Registered: %s"), bServicesRegistered ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Display, TEXT("  Services Ready:      %s"), bServicesReady ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Display, TEXT("  Rebind In Progress:  %s"), bRebindInProgress ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Display, TEXT("  Total Rebinds:       %d"), RebindCount);

    // World status
    UWorld* CurrentWorld = TryGetCurrentWorldSafe();
    if (CurrentWorld)
    {
        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Display, TEXT(""));
        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Display, TEXT("Current World:"));
        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Display, TEXT("  Name:     %s"), *CurrentWorld->GetName());
        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Display, TEXT("  NetMode:  %s"), *GetNetModeString(CurrentWorld->GetNetMode()));
        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Display, TEXT("  Ptr:      %p"), CurrentWorld);
    }
    else
    {
        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Display, TEXT(""));
        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Display, TEXT("Current World: NONE"));
    }

    // Last bound world
    if (UWorld* LastWorld = LastBoundWorld.Get())
    {
        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Display, TEXT(""));
        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Display, TEXT("Last Bound World:"));
        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Display, TEXT("  Name: %s (Ptr=%p)"), *LastWorld->GetName(), LastWorld);
    }

    // Registered services
    if (ServiceLocator)
    {
        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Display, TEXT(""));

        const TArray<FGameplayTag> AllServiceTags = ServiceLocator->GetAllRegisteredServiceTags();
        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Display, TEXT("Registered Services: %d"), AllServiceTags.Num());

        for (const FGameplayTag& Tag : AllServiceTags)
        {
            UObject* ServiceObj = ServiceLocator->GetService(Tag);
            const bool bIsWorldBindable = ServiceObj && ServiceObj->GetClass()->ImplementsInterface(USuspenseCoreWorldBindable::StaticClass());
            const bool bIsReady = ServiceLocator->IsServiceReady(Tag);

            UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Display, TEXT("  - %s (Ready=%s, WorldBindable=%s)"),
                *Tag.ToString(),
                bIsReady ? TEXT("YES") : TEXT("NO"),
                bIsWorldBindable ? TEXT("YES") : TEXT("NO"));
        }
    }
    else
    {
        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Display, TEXT(""));
        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Display, TEXT("ServiceLocator: NONE"));
    }

    UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Display, TEXT(""));
    UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Display, TEXT("=== END ==="));
    UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Display, TEXT(""));
}

void USuspenseCoreSystemCoordinator::DebugForceRebind()
{
    UWorld* World = TryGetCurrentWorldSafe();
    if (!World)
    {
        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Warning, TEXT("DebugForceRebind: no current world"));
        return;
    }

    UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Display, TEXT("DebugForceRebind: forcing rebind to %s"), *World->GetName());
    ForceRebindWorld(World);
    UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Display, TEXT("DebugForceRebind: complete"));
}

//========================================
// Coordinator Lifecycle Methods
//========================================

void USuspenseCoreSystemCoordinator::Shutdown()
{
    UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Log, TEXT("Coordinator::Shutdown"));

    // Cleanup any active subscriptions or timers
    // Currently no additional cleanup needed - services handle their own cleanup
}

void USuspenseCoreSystemCoordinator::RegisterCoreServices()
{
    UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Log, TEXT("Coordinator::RegisterCoreServices"));

    if (!ServiceLocator)
    {
        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Error, TEXT("RegisterCoreServices: ServiceLocator is null"));
        return;
    }

    // Register core equipment services at subsystem level
    // This ensures services are available even if component is not added to actor

    const FGameplayTag TagData          = FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Data"), false);
    const FGameplayTag TagValidation    = FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Validation"), false);
    const FGameplayTag TagOperations    = FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Operations"), false);
    const FGameplayTag TagVisualization = FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Visualization"), false);
    const FGameplayTag TagAbility       = FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Ability"), false);

    int32 RegisteredCount = 0;

    // Data Service - requires ItemManager injection
    if (TagData.IsValid() && !ServiceLocator->IsServiceRegistered(TagData))
    {
        FSuspenseCoreServiceInitParams DataParams;
        DataParams.bAutoStart = true;

        // Create injection callback that provides ItemManager to DataService
        FSuspenseCoreServiceInjectionDelegate DataInjection;
        DataInjection.BindLambda([this](UObject* ServiceInstance, USuspenseCoreEquipmentServiceLocator* InServiceLocator)
        {
            if (!ServiceInstance || !InServiceLocator)
            {
                UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Error, TEXT("DataService injection: Invalid parameters"));
                return;
            }

            UGameInstance* GI = InServiceLocator->GetGameInstance();
            if (!GI)
            {
                UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Error, TEXT("DataService injection: GameInstance not available"));
                return;
            }

            USuspenseCoreItemManager* ItemManager = GI->GetSubsystem<USuspenseCoreItemManager>();
            if (!ItemManager)
            {
                UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Error, TEXT("DataService injection: ItemManager subsystem not found"));
                return;
            }

            if (ItemManager->GetCachedItemCount() == 0)
            {
                UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Warning, TEXT("DataService injection: ItemManager has no cached items yet"));
            }

            if (USuspenseCoreEquipmentDataService* DataService = Cast<USuspenseCoreEquipmentDataService>(ServiceInstance))
            {
                DataService->InjectComponents(nullptr, ItemManager);
                UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Log, TEXT("DataService: ItemManager injected successfully (stateless mode)"));
            }
            else
            {
                UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Error, TEXT("DataService injection: Failed to cast to DataService"));
            }
        });

        ServiceLocator->RegisterServiceClassWithInjection(
            TagData,
            USuspenseCoreEquipmentDataService::StaticClass(),
            DataParams,
            DataInjection);

        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Log, TEXT("  Registered: DataService (with ItemManager injection)"));
        RegisteredCount++;
    }

    // Validation Service
    if (TagValidation.IsValid() && !ServiceLocator->IsServiceRegistered(TagValidation))
    {
        FSuspenseCoreServiceInitParams ValidationParams;
        ValidationParams.bAutoStart = true;
        ValidationParams.RequiredServices.AddTag(TagData);

        ServiceLocator->RegisterServiceClass(
            TagValidation,
            USuspenseCoreEquipmentValidationService::StaticClass(),
            ValidationParams);

        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Log, TEXT("  Registered: ValidationService"));
        RegisteredCount++;
    }

    // Operation Service
    if (TagOperations.IsValid() && !ServiceLocator->IsServiceRegistered(TagOperations))
    {
        FSuspenseCoreServiceInitParams OperationParams;
        OperationParams.bAutoStart = true;
        OperationParams.RequiredServices.AddTag(TagData);
        OperationParams.RequiredServices.AddTag(TagValidation);

        ServiceLocator->RegisterServiceClass(
            TagOperations,
            USuspenseCoreEquipmentOperationService::StaticClass(),
            OperationParams);

        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Log, TEXT("  Registered: OperationService"));
        RegisteredCount++;
    }

    // Visualization Service - CRITICAL for equipment to spawn in world
    if (TagVisualization.IsValid() && !ServiceLocator->IsServiceRegistered(TagVisualization))
    {
        FSuspenseCoreServiceInitParams VisualizationParams;
        VisualizationParams.bAutoStart = true;
        VisualizationParams.RequiredServices.AddTag(TagData);

        ServiceLocator->RegisterServiceClass(
            TagVisualization,
            USuspenseCoreEquipmentVisualizationService::StaticClass(),
            VisualizationParams);

        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Log, TEXT("  Registered: VisualizationService"));
        RegisteredCount++;
    }

    // Ability Service
    if (TagAbility.IsValid() && !ServiceLocator->IsServiceRegistered(TagAbility))
    {
        FSuspenseCoreServiceInitParams AbilityParams;
        AbilityParams.bAutoStart = true;

        ServiceLocator->RegisterServiceClass(
            TagAbility,
            USuspenseCoreEquipmentAbilityService::StaticClass(),
            AbilityParams);

        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Log, TEXT("  Registered: AbilityService"));
        RegisteredCount++;
    }

    // Ammo Loading Service - use native tag
    {
        const FGameplayTag TagAmmoLoading = SuspenseCoreEquipmentTags::Service::TAG_Service_Equipment_AmmoLoading.Get();
        if (TagAmmoLoading.IsValid() && !ServiceLocator->IsServiceRegistered(TagAmmoLoading))
        {
            FSuspenseCoreServiceInitParams AmmoLoadingParams;
            AmmoLoadingParams.bAutoStart = true;

            ServiceLocator->RegisterServiceClass(
                TagAmmoLoading,
                USuspenseCoreAmmoLoadingService::StaticClass(),
                AmmoLoadingParams);

            UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Log, TEXT("  Registered: AmmoLoadingService"));
            RegisteredCount++;
        }
    }

    UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Log, TEXT("RegisterCoreServices: complete (%d services registered)"), RegisteredCount);
}

void USuspenseCoreSystemCoordinator::WarmUpServices()
{
    UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Log, TEXT("Coordinator::WarmUpServices"));

    if (!ServiceLocator)
    {
        UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Warning, TEXT("WarmUpServices: ServiceLocator is null"));
        return;
    }

    // Initialize all lazy services
    const int32 Initialized = ServiceLocator->InitializeAllServices();
    UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Log, TEXT("WarmUpServices: %d services initialized"), Initialized);
}

bool USuspenseCoreSystemCoordinator::ValidateServices(TArray<FText>& OutErrors) const
{
    UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Log, TEXT("Coordinator::ValidateServices"));

    OutErrors.Empty();

    if (!ServiceLocator)
    {
        OutErrors.Add(FText::FromString(TEXT("ServiceLocator is null")));
        return false;
    }

    // Validate all registered services
    const TArray<FGameplayTag> AllTags = ServiceLocator->GetAllRegisteredServiceTags();

    int32 ValidCount = 0;
    int32 InvalidCount = 0;

    for (const FGameplayTag& Tag : AllTags)
    {
        if (ServiceLocator->IsServiceReady(Tag))
        {
            ValidCount++;
        }
        else
        {
            InvalidCount++;
            OutErrors.Add(FText::Format(
                FText::FromString(TEXT("Service not ready: {0}")),
                FText::FromString(Tag.ToString())
            ));
        }
    }

    UE_LOG(LogSuspenseCoreCoordinatorSubsystem, Log,
        TEXT("ValidateServices: Valid=%d, Invalid=%d, Total=%d"),
        ValidCount, InvalidCount, AllTags.Num());

    return InvalidCount == 0;
}
