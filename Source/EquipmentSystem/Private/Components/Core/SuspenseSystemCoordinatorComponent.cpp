// MedComSystemCoordinator.cpp
// Copyright Suspense Team. All Rights Reserved.

#include "Components/Core/SuspenseSystemCoordinatorComponent.h"
#include "Core/Services/EquipmentServiceLocator.h"
#include "Interfaces/Equipment/IEquipmentService.h"

// Concrete service implementations
#include "Services/EquipmentDataServiceImpl.h"
#include "Services/EquipmentValidationServiceImpl.h"
#include "Services/EquipmentOperationServiceImpl.h"
#include "Services/EquipmentVisualizationServiceImpl.h"
#include "Services/EquipmentAbilityServiceImpl.h"

// Presentation layer components (registered as services)
#include "Components/.*/SuspenseEquipmentActorFactory.h"
#include "Components/.*/SuspenseEquipmentAttachmentSystem.h"
#include "Components/.*/SuspenseEquipmentVisualController.h"

// Additional interfaces
#include "Interfaces/Equipment/ISuspenseEquipmentDataProvider.h"
#include "ItemSystem/SuspenseItemManager.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogMedComCoordinator, Log, All);

USuspenseSystemCoordinatorComponent::USuspenseSystemCoordinatorComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(false);
}

void USuspenseSystemCoordinatorComponent::BeginPlay()
{
    Super::BeginPlay();

    // Cache service tags
    DataServiceTag          = FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Data"));
    ValidationServiceTag    = FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Validation"));
    OperationServiceTag     = FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Operations"));
    VisualizationServiceTag = FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Visualization"));
    AbilityServiceTag       = FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Ability"));

    UE_LOG(LogMedComCoordinator, Log, TEXT("Coordinator BeginPlay: Service tags cached"));
    
    bBootstrapped = false;
}

void USuspenseSystemCoordinatorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
}

void USuspenseSystemCoordinatorComponent::Shutdown()
{
    UE_LOG(LogMedComCoordinator, Log, TEXT("=== Coordinator Shutdown START ==="));
    
    bBootstrapped = false;
    
    USuspenseEquipmentServiceLocator* Locator = GetLocator();
    if (Locator)
    {
        UE_LOG(LogMedComCoordinator, Log, TEXT("Locator services notified of shutdown"));
    }
    
    DataServiceTag = FGameplayTag();
    ValidationServiceTag = FGameplayTag();
    OperationServiceTag = FGameplayTag();
    VisualizationServiceTag = FGameplayTag();
    AbilityServiceTag = FGameplayTag();
    
    SlotValidator = nullptr;
    
    UE_LOG(LogMedComCoordinator, Log, TEXT("=== Coordinator Shutdown COMPLETE ==="));
}

USuspenseEquipmentServiceLocator* USuspenseSystemCoordinatorComponent::GetLocator() const
{
    if (const UWorld* World = GetWorld())
    {
        if (USuspenseEquipmentServiceLocator* L = USuspenseEquipmentServiceLocator::Get(World))
        {
            return L;
        }
    }

    if (const UGameInstance* GI = GetTypedOuter<UGameInstance>())
    {
        return GI->GetSubsystem<USuspenseEquipmentServiceLocator>();
    }
    if (const UGameInstanceSubsystem* GISub = GetTypedOuter<UGameInstanceSubsystem>())
    {
        if (UGameInstance* GI2 = GISub->GetGameInstance())
        {
            return GI2->GetSubsystem<USuspenseEquipmentServiceLocator>();
        }
    }

    return nullptr;
}

FGameplayTag USuspenseSystemCoordinatorComponent::GetServiceTagFromClass(UClass* ServiceClass) const
{
    if (!ServiceClass)
    {
        UE_LOG(LogMedComCoordinator, Error, TEXT("GetServiceTagFromClass: ServiceClass is null"));
        return FGameplayTag();
    }

    if (!ServiceClass->ImplementsInterface(UEquipmentService::StaticClass()))
    {
        UE_LOG(LogMedComCoordinator, Error, TEXT("GetServiceTagFromClass: %s does not implement UEquipmentService"),
            *ServiceClass->GetName());
        return FGameplayTag();
    }

    UObject* CDO = ServiceClass->GetDefaultObject();
    if (!CDO)
    {
        UE_LOG(LogMedComCoordinator, Error, TEXT("GetServiceTagFromClass: CDO is null for %s"),
            *ServiceClass->GetName());
        return FGameplayTag();
    }

    IEquipmentService* Iface = Cast<IEquipmentService>(CDO);
    if (!Iface)
    {
        UE_LOG(LogMedComCoordinator, Error, TEXT("GetServiceTagFromClass: Interface cast failed on CDO: %s"),
            *ServiceClass->GetName());
        return FGameplayTag();
    }

    const FGameplayTag Tag = Iface->GetServiceTag();
    if (!Tag.IsValid())
    {
        UE_LOG(LogMedComCoordinator, Error, TEXT("GetServiceTagFromClass: Invalid tag from CDO: %s"),
            *ServiceClass->GetName());
    }

    return Tag;
}

bool USuspenseSystemCoordinatorComponent::BootstrapServices()
{
    USuspenseEquipmentServiceLocator* Locator = GetLocator();
    if (!Locator)
    {
        UE_LOG(LogMedComCoordinator, Error, TEXT("BootstrapServices: Locator not available"));
        return false;
    }

    UE_LOG(LogMedComCoordinator, Log, TEXT("BootstrapServices: starting"));

    RegisterCoreServices();
    RegisterPresentationServices();
    WarmUpServices();

    TArray<FText> Errors;
    const bool bValid = ValidateServices(Errors);
    
    if (!bValid)
    {
        for (const FText& E : Errors)
        {
            UE_LOG(LogMedComCoordinator, Error, TEXT("Service validation error: %s"), *E.ToString());
        }
        UE_LOG(LogMedComCoordinator, Warning, TEXT("BootstrapServices: completed with %d validation errors"), Errors.Num());
    }
    else
    {
        UE_LOG(LogMedComCoordinator, Log, TEXT("BootstrapServices: completed successfully"));
    }

    bBootstrapped = true;
    return true;
}

void USuspenseSystemCoordinatorComponent::RegisterCoreServices()
{
    USuspenseEquipmentServiceLocator* Locator = GetLocator();
    if (!Locator) 
    {
        UE_LOG(LogMedComCoordinator, Error, TEXT("RegisterCoreServices: Locator is null"));
        return;
    }

    UE_LOG(LogMedComCoordinator, Log, TEXT("RegisterCoreServices: starting"));

    const FGameplayTag TagData          = FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Data"));
    const FGameplayTag TagValidation    = FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Validation"));
    const FGameplayTag TagOperations    = FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Operations"));
    const FGameplayTag TagVisualization = FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Visualization"));
    const FGameplayTag TagAbility       = FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Ability"));

    // Data Service
    if (!Locator->IsServiceRegistered(TagData))
    {
        FServiceInitParams DataParams;
        DataParams.bAutoStart = true;
        
        FServiceInjectionDelegate DataInjection;
        DataInjection.BindLambda([](UObject* ServiceInstance, USuspenseEquipmentServiceLocator* ServiceLocator)
        {
            if (!ServiceInstance)
            {
                UE_LOG(LogMedComCoordinator, Error, TEXT("DataService injection: ServiceInstance is null"));
                return;
            }
            
            UGameInstance* GI = ServiceLocator->GetGameInstance();
            if (!GI)
            {
                UE_LOG(LogMedComCoordinator, Error, TEXT("DataService injection: GameInstance not available"));
                return;
            }
            
            USuspenseItemManager* ItemManager = GI->GetSubsystem<USuspenseItemManager>();
            if (!ItemManager)
            {
                UE_LOG(LogMedComCoordinator, Error, TEXT("DataService injection: ItemManager subsystem not found"));
                return;
            }
            
            if (ItemManager->GetCachedItemCount() == 0)
            {
                UE_LOG(LogMedComCoordinator, Warning, TEXT("DataService injection: ItemManager has no cached items yet"));
            }
            
            if (UEquipmentDataServiceImpl* DataService = Cast<UEquipmentDataServiceImpl>(ServiceInstance))
            {
                DataService->InjectComponents(nullptr, ItemManager);
                
                UE_LOG(LogMedComCoordinator, Log, 
                    TEXT("DataService: ItemManager injected successfully (stateless mode)"));
            }
            else
            {
                UE_LOG(LogMedComCoordinator, Error,
                    TEXT("DataService injection: Failed to cast ServiceInstance to UEquipmentDataServiceImpl"));
            }
        });
        
        Locator->RegisterServiceClassWithInjection(
            TagData, 
            UEquipmentDataServiceImpl::StaticClass(), 
            DataParams, 
            DataInjection);
        
        UE_LOG(LogMedComCoordinator, Log, TEXT("Registered Data Service with ItemManager injection"));
    }

    // Validation Service
    if (!Locator->IsServiceRegistered(TagValidation))
    {
        FServiceInitParams ValidationParams;
        ValidationParams.bAutoStart = true;
        ValidationParams.RequiredServices.AddTag(TagData);
        
        Locator->RegisterServiceClass(
            TagValidation, 
            UEquipmentValidationServiceImpl::StaticClass(), 
            ValidationParams);
        
        UE_LOG(LogMedComCoordinator, Log, TEXT("Registered Validation Service"));
    }

    // Operation Service
    if (!Locator->IsServiceRegistered(TagOperations))
    {
        FServiceInitParams OperationParams;
        OperationParams.bAutoStart = true;
        OperationParams.RequiredServices.AddTag(TagData);
        OperationParams.RequiredServices.AddTag(TagValidation);
        
        Locator->RegisterServiceClass(
            TagOperations, 
            UEquipmentOperationServiceImpl::StaticClass(), 
            OperationParams);
        
        UE_LOG(LogMedComCoordinator, Log, TEXT("Registered Operation Service"));
    }

    // Visualization Service
    if (!Locator->IsServiceRegistered(TagVisualization))
    {
        FServiceInitParams VisualizationParams;
        VisualizationParams.bAutoStart = true;
        VisualizationParams.RequiredServices.AddTag(TagData);
        
        Locator->RegisterServiceClass(
            TagVisualization, 
            UEquipmentVisualizationServiceImpl::StaticClass(), 
            VisualizationParams);
        
        UE_LOG(LogMedComCoordinator, Log, TEXT("Registered Visualization Service"));
    }

    // Ability Service
    if (!Locator->IsServiceRegistered(TagAbility))
    {
        FServiceInitParams AbilityParams;
        AbilityParams.bAutoStart = true;
        
        Locator->RegisterServiceClass(
            TagAbility, 
            UEquipmentAbilityServiceImpl::StaticClass(), 
            AbilityParams);
        
        UE_LOG(LogMedComCoordinator, Log, TEXT("Registered Ability Service"));
    }

    UE_LOG(LogMedComCoordinator, Log, TEXT("RegisterCoreServices: completed (5 services registered)"));
}

void USuspenseSystemCoordinatorComponent::RegisterPresentationServices()
{
    USuspenseEquipmentServiceLocator* Locator = GetLocator();
    if (!Locator)
    {
        UE_LOG(LogMedComCoordinator, Error, TEXT("RegisterPresentationServices: Locator is null"));
        return;
    }

    UE_LOG(LogMedComCoordinator, Warning, TEXT("=== RegisterPresentationServices START ==="));

    // ✅ ИСПРАВЛЕНИЕ: Получаем Owner через GetTypedOuter<AActor>()
    // Компонент принадлежит PlayerState (AActor)
    AActor* Owner = GetTypedOuter<AActor>();
    if (!Owner)
    {
        UE_LOG(LogMedComCoordinator, Error, 
            TEXT("RegisterPresentationServices: Owner actor is null"));
        UE_LOG(LogMedComCoordinator, Error,
            TEXT("  This component should be attached to PlayerState (AActor)"));
        return;
    }

    UE_LOG(LogMedComCoordinator, Log, 
        TEXT("RegisterPresentationServices: Owner = %s (Class: %s)"),
        *Owner->GetName(), *Owner->GetClass()->GetName());

    // ========================================
    // 1. ActorFactory - per-player компонент
    // ========================================
    {
        const FGameplayTag FactoryTag = FGameplayTag::RequestGameplayTag(TEXT("Service.ActorFactory"));
        
        if (!Locator->IsServiceRegistered(FactoryTag))
        {
            // Ищем ActorFactory компонент на PlayerState
            USuspenseEquipmentActorFactory* Factory = Owner->FindComponentByClass<USuspenseEquipmentActorFactory>();
            
            if (!Factory)
            {
                UE_LOG(LogMedComCoordinator, Warning, 
                    TEXT("ActorFactory not found on %s - this is expected at initialization"),
                    *Owner->GetName());
                UE_LOG(LogMedComCoordinator, Warning,
                    TEXT("  ActorFactory should be created in Blueprint and will register itself on BeginPlay"));
            }
            else
            {
                // Если уже есть - регистрируем напрямую
                UE_LOG(LogMedComCoordinator, Log, TEXT("Found existing ActorFactory, registering..."));
                Locator->RegisterServiceInstance(FactoryTag, Factory);
                UE_LOG(LogMedComCoordinator, Log, TEXT("✓ Registered ActorFactory service"));
            }
        }
        else
        {
            UE_LOG(LogMedComCoordinator, Verbose,
                TEXT("ActorFactory already registered in ServiceLocator"));
        }
    }

    // ========================================
    // 2. AttachmentSystem - per-player компонент
    // ========================================
    {
        const FGameplayTag AttachmentTag = FGameplayTag::RequestGameplayTag(TEXT("Service.AttachmentSystem"));
        
        if (!Locator->IsServiceRegistered(AttachmentTag))
        {
            USuspenseEquipmentAttachmentSystem* AttachmentSystem = 
                Owner->FindComponentByClass<USuspenseEquipmentAttachmentSystem>();
            
            if (!AttachmentSystem)
            {
                UE_LOG(LogMedComCoordinator, Warning,
                    TEXT("AttachmentSystem not found on %s - will register when created"),
                    *Owner->GetName());
            }
            else
            {
                UE_LOG(LogMedComCoordinator, Log, TEXT("Found existing AttachmentSystem, registering..."));
                Locator->RegisterServiceInstance(AttachmentTag, AttachmentSystem);
                UE_LOG(LogMedComCoordinator, Log, TEXT("✓ Registered AttachmentSystem service"));
            }
        }
        else
        {
            UE_LOG(LogMedComCoordinator, Verbose,
                TEXT("AttachmentSystem already registered in ServiceLocator"));
        }
    }

    // ========================================
    // 3. VisualController - per-player компонент
    // ========================================
    {
        const FGameplayTag VisualControllerTag = FGameplayTag::RequestGameplayTag(TEXT("Service.VisualController"));
        
        if (!Locator->IsServiceRegistered(VisualControllerTag))
        {
            USuspenseEquipmentVisualController* VisualController = 
                Owner->FindComponentByClass<USuspenseEquipmentVisualController>();
            
            if (!VisualController)
            {
                UE_LOG(LogMedComCoordinator, Warning,
                    TEXT("VisualController not found on %s - will register when created"),
                    *Owner->GetName());
            }
            else
            {
                UE_LOG(LogMedComCoordinator, Log, TEXT("Found existing VisualController, registering..."));
                Locator->RegisterServiceInstance(VisualControllerTag, VisualController);
                UE_LOG(LogMedComCoordinator, Log, TEXT("✓ Registered VisualController service"));
            }
        }
        else
        {
            UE_LOG(LogMedComCoordinator, Verbose,
                TEXT("VisualController already registered in ServiceLocator"));
        }
    }

    UE_LOG(LogMedComCoordinator, Warning, TEXT("=== RegisterPresentationServices END ==="));
    UE_LOG(LogMedComCoordinator, Warning, 
        TEXT("  Presentation services should be created as components in Blueprint"));
    UE_LOG(LogMedComCoordinator, Warning,
        TEXT("  They will auto-register on their BeginPlay if not already registered"));
}

void USuspenseSystemCoordinatorComponent::WarmUpServices()
{
    USuspenseEquipmentServiceLocator* Locator = GetLocator();
    if (!Locator) return;

    UE_LOG(LogMedComCoordinator, Log, TEXT("WarmUpServices: starting"));
    const int32 Inited = Locator->InitializeAllServices();
    UE_LOG(LogMedComCoordinator, Log, TEXT("WarmUpServices: completed (%d initialized)"), Inited);
}

bool USuspenseSystemCoordinatorComponent::ValidateServices(TArray<FText>& OutErrors) const
{
    OutErrors.Reset();

    USuspenseEquipmentServiceLocator* Locator = const_cast<USuspenseSystemCoordinatorComponent*>(this)->GetLocator();
    if (!Locator)
    {
        OutErrors.Add(FText::FromString(TEXT("Locator is null")));
        return false;
    }

    bool bOk = Locator->ValidateAllServices(OutErrors);

    const FGameplayTag TagData       = FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Data"));
    const FGameplayTag TagValidation = FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Validation"));
    const FGameplayTag TagOperations = FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Operations"));

    if (!Locator->IsServiceReady(TagData))       { OutErrors.Add(FText::FromString(TEXT("Service Data not ready")));       bOk = false; }
    if (!Locator->IsServiceReady(TagValidation)) { OutErrors.Add(FText::FromString(TEXT("Service Validation not ready"))); bOk = false; }
    if (!Locator->IsServiceReady(TagOperations)) { OutErrors.Add(FText::FromString(TEXT("Service Operations not ready"))); bOk = false; }

    // Presentation services are optional - don't fail validation if missing
    const FGameplayTag TagFactory = FGameplayTag::RequestGameplayTag(TEXT("Service.ActorFactory"), false);
    if (TagFactory.IsValid() && !Locator->IsServiceReady(TagFactory))
    {
        UE_LOG(LogMedComCoordinator, Warning, TEXT("ActorFactory service not ready (this is OK if not created yet)"));
    }

    return bOk && OutErrors.Num() == 0;
}