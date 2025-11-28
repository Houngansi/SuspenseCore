// EquipmentAbilityServiceImpl.cpp
// Copyright Suspense Team. All Rights Reserved.

#include "Services/SuspenseEquipmentAbilityService.h"
#include "Components/Integration/SuspenseEquipmentAbilityConnector.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Components/Coordination/SuspenseEquipmentEventDispatcher.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

USuspenseEquipmentAbilityService::USuspenseEquipmentAbilityService()
{
    // Initialize cache with reasonable size
    MappingCache = MakeShareable(new FSuspenseEquipmentCacheManager<FName, FEquipmentAbilityMapping>(100));
}

USuspenseEquipmentAbilityService::~USuspenseEquipmentAbilityService()
{
    ShutdownService(true);
}

void USuspenseEquipmentAbilityService::BeginDestroy()
{
    ShutdownService(true);
    Super::BeginDestroy();
}

//========================================
// IEquipmentService Implementation
//========================================

bool USuspenseEquipmentAbilityService::InitializeService(const FServiceInitParams& Params)
{
    SCOPED_SERVICE_TIMER("InitializeService");
    EQUIPMENT_WRITE_LOCK(ConnectorLock);

    if (ServiceState != EServiceLifecycleState::Uninitialized)
    {
        UE_LOG(LogSuspenseEquipmentAbility, Warning, TEXT("Service already initialized"));
        ServiceMetrics.RecordError();
        return false;
    }

    ServiceState = EServiceLifecycleState::Initializing;

    // Ensure valid configuration
    EnsureValidConfig();

    // Load default mappings
    InitializeDefaultMappings();

    // Initialize S7 event tags (non-fatal if tags not found)
    Tag_OnEquipped        = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.Equipped"), /*ErrorIfNotFound*/false);
    Tag_OnUnequipped      = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.Unequipped"), /*ErrorIfNotFound*/false);
    Tag_OnAbilitiesRefresh= FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.Ability.Refresh"), /*ErrorIfNotFound*/false);
    Tag_OnCommit          = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.Commit"), /*ErrorIfNotFound*/false);

    // Setup event handlers
    SetupEventHandlers();

    // Register cache for monitoring
    FSuspenseGlobalCacheRegistry::Get().RegisterCache(
        TEXT("EquipmentAbilityService.Mappings"),
        [this]() {
            float HitRate = (CacheHits + CacheMisses) > 0
                ? (float)CacheHits / (CacheHits + CacheMisses) * 100.0f
                : 0.0f;
            return FString::Printf(TEXT("Cache: Hits=%d, Misses=%d, HitRate=%.1f%%\n%s"),
                CacheHits, CacheMisses, HitRate,
                *MappingCache->GetStatistics().ToString());
        }
    );
    bCacheRegistered = true;
    // Setup periodic cleanup if enabled
    if (bEnablePeriodicCleanup)
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(
                CleanupTimerHandle,
                this,
                &USuspenseEquipmentAbilityService::OnCleanupTimer,
                CleanupInterval,
                true // Loop
            );

            UE_LOG(LogSuspenseEquipmentAbility, Log, TEXT("Periodic cleanup enabled every %.1f seconds"),
                CleanupInterval);
        }
    }

    ServiceState = EServiceLifecycleState::Ready;
    ServiceMetrics.RecordSuccess();

    UE_LOG(LogSuspenseEquipmentAbility, Log, TEXT("EquipmentAbilityService initialized with %d mappings"),
        AbilityMappings.Num());

    return true;
}

bool USuspenseEquipmentAbilityService::ShutdownService(bool bForce)
{
    SCOPED_SERVICE_TIMER("ShutdownService");
    EQUIPMENT_WRITE_LOCK(ConnectorLock);

    if (ServiceState == EServiceLifecycleState::Shutdown)
    {
        return true;
    }

    ServiceState = EServiceLifecycleState::Shutting;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(CleanupTimerHandle);
    }

    for (auto& Pair : EquipmentConnectors)
    {
        if (USuspenseEquipmentAbilityConnector* Connector = Pair.Value)
        {
            if (IsValid(Connector))
            {
                Connector->ClearAll();
                Connector->DestroyComponent();
            }
        }
    }
    EquipmentConnectors.Empty();
    EquipmentToOwnerMap.Empty();

    // Безопасная отписка от EventBus
    if (auto EventBus = FSuspenseEquipmentEventBus::Get())
    {
        for (const FEventSubscriptionHandle& Handle : EventSubscriptions)
        {
            EventBus->Unsubscribe(Handle);
        }
    }
    EventSubscriptions.Empty();

    // Безопасная очистка кеша
    if (MappingCache.IsValid())
    {
        MappingCache->Clear();
    }

    // ВАЖНО: во время выхода движка НЕ трогаем глобальные реестры/синглтоны
    if (bCacheRegistered && !IsEngineExitRequested())
    {
        FSuspenseGlobalCacheRegistry::Get().UnregisterCache(TEXT("EquipmentAbilityService.Mappings"));
        bCacheRegistered = false;
    }

    ServiceState = EServiceLifecycleState::Shutdown;
    ServiceMetrics.RecordSuccess();
    UE_LOG(LogSuspenseEquipmentAbility, Log, TEXT("EquipmentAbilityService shutdown complete"));
    return true;
}

FGameplayTag USuspenseEquipmentAbilityService::GetServiceTag() const
{
    return FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Ability"));
}

FGameplayTagContainer USuspenseEquipmentAbilityService::GetRequiredDependencies() const
{
    FGameplayTagContainer Dependencies;
    // We don't strictly require other services - we're self-contained
    return Dependencies;
}

bool USuspenseEquipmentAbilityService::ValidateService(TArray<FText>& OutErrors) const
{
    SCOPED_SERVICE_TIMER_CONST("ValidateService");
    EQUIPMENT_READ_LOCK(ConnectorLock);

    OutErrors.Empty();
    bool bIsValid = true;

    // Check for invalid connectors
    int32 InvalidConnectors = 0;
    for (const auto& ConnectorPair : EquipmentConnectors)
    {
        if (!ConnectorPair.Key.IsValid() || !IsValid(ConnectorPair.Value))
        {
            InvalidConnectors++;
        }
    }

    if (InvalidConnectors > 0)
    {
        OutErrors.Add(FText::FromString(FString::Printf(
            TEXT("%d invalid equipment connectors detected"), InvalidConnectors)));
        bIsValid = false;
    }

    // Check if we have any mappings
    if (AbilityMappings.Num() == 0)
    {
        OutErrors.Add(FText::FromString(TEXT("No ability mappings loaded")));
        // This is a warning, not an error
    }

    if (bIsValid)
    {
        ServiceMetrics.RecordSuccess();
    }
    else
    {
        ServiceMetrics.RecordError();
    }

    return bIsValid;
}

void USuspenseEquipmentAbilityService::ResetService()
{
    SCOPED_SERVICE_TIMER("ResetService");
    EQUIPMENT_WRITE_LOCK(ConnectorLock);

    // Clear all connectors
    for (auto& ConnectorPair : EquipmentConnectors)
    {
        if (USuspenseEquipmentAbilityConnector* Connector = ConnectorPair.Value)
        {
            if (IsValid(Connector))
            {
                Connector->ClearAll();
            }
        }
    }

    // Clear cache
    MappingCache->Clear();

    // Reset statistics
    ServiceMetrics.Reset();
    CacheHits = 0;
    CacheMisses = 0;

    UE_LOG(LogSuspenseEquipmentAbility, Log, TEXT("EquipmentAbilityService reset"));
}

FString USuspenseEquipmentAbilityService::GetServiceStats() const
{
    EQUIPMENT_READ_LOCK(ConnectorLock);

    FString Stats = TEXT("=== Equipment Ability Service Statistics ===\n");
    Stats += FString::Printf(TEXT("Service State: %s\n"),
        *UEnum::GetValueAsString(ServiceState));
    Stats += FString::Printf(TEXT("Active Equipment Connectors: %d\n"),
        EquipmentConnectors.Num());
    Stats += FString::Printf(TEXT("Loaded Mappings: %d\n"),
        AbilityMappings.Num());

    // Cache statistics
    float HitRate = (CacheHits + CacheMisses) > 0
        ? (float)CacheHits / (CacheHits + CacheMisses) * 100.0f
        : 0.0f;
    Stats += FString::Printf(TEXT("Cache: Hits=%d, Misses=%d, HitRate=%.1f%%\n"),
        CacheHits, CacheMisses, HitRate);

    // List active equipment connectors
    if (EquipmentConnectors.Num() > 0)
    {
        Stats += TEXT("\n--- Active Equipment Connectors ---\n");
        for (const auto& ConnectorPair : EquipmentConnectors)
        {
            if (AActor* Equipment = ConnectorPair.Key.Get())
            {
                AActor* Owner = nullptr;
                if (auto* OwnerPtr = EquipmentToOwnerMap.Find(Equipment))
                {
                    Owner = OwnerPtr->Get();
                }

                if (USuspenseEquipmentAbilityConnector* Connector = ConnectorPair.Value)
                {
                    Stats += FString::Printf(TEXT("  Equipment: %s | Owner: %s | Valid: %s\n"),
                        *GetNameSafe(Equipment),
                        *GetNameSafe(Owner),
                        IsValid(Connector) ? TEXT("Yes") : TEXT("No")
                    );
                }
            }
        }
    }

    // Add service metrics
    Stats += ServiceMetrics.ToString(TEXT("EquipmentAbilityService"));

    return Stats;
}

//========================================
// Public API - Configuration
//========================================

int32 USuspenseEquipmentAbilityService::LoadAbilityMappings(UDataTable* MappingTable)
{
    SCOPED_SERVICE_TIMER("LoadAbilityMappings");

    if (!ensure(IsInGameThread()))
    {
        UE_LOG(LogSuspenseEquipmentAbility, Error, TEXT("LoadAbilityMappings must be called on GameThread"));
        ServiceMetrics.RecordError();
        return 0;
    }

    if (!MappingTable)
    {
        ServiceMetrics.RecordError();
        return 0;
    }

    EQUIPMENT_WRITE_LOCK(MappingLock);

    int32 LoadedCount = 0;
    int32 InvalidCount = 0;

    // Type-safe iteration through rows
    const TArray<FName> RowNames = MappingTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        const FEquipmentAbilityMapping* Mapping =
            MappingTable->FindRow<FEquipmentAbilityMapping>(RowName, TEXT("LoadAbilityMappings"));

        if (!Mapping)
        {
            UE_LOG(LogSuspenseEquipmentAbility, Warning,
                TEXT("Failed to cast row %s to FEquipmentAbilityMapping"),
                *RowName.ToString());
            InvalidCount++;
            continue;
        }

        // Validate mapping
        if (!Mapping->IsValid())
        {
            UE_LOG(LogSuspenseEquipmentAbility, Warning,
                TEXT("Invalid mapping: ItemID is None for row %s"),
                *RowName.ToString());
            InvalidCount++;
            continue;
        }

        // Validate ability classes
        bool bHasInvalidAbility = false;
        for (const auto& AbilityClass : Mapping->GrantedAbilities)
        {
            if (!AbilityClass)
            {
                UE_LOG(LogSuspenseEquipmentAbility, Warning,
                    TEXT("Null ability class in mapping for item %s"),
                    *Mapping->ItemID.ToString());
                bHasInvalidAbility = true;
            }
        }

        // Validate effect classes
        bool bHasInvalidEffect = false;
        for (const auto& EffectClass : Mapping->PassiveEffects)
        {
            if (!EffectClass)
            {
                UE_LOG(LogSuspenseEquipmentAbility, Warning,
                    TEXT("Null effect class in mapping for item %s"),
                    *Mapping->ItemID.ToString());
                bHasInvalidEffect = true;
            }
        }

        if (bHasInvalidAbility || bHasInvalidEffect)
        {
            InvalidCount++;
            continue;
        }

        // Add or update mapping
        AbilityMappings.Add(Mapping->ItemID, *Mapping);

        // Update cache
        MappingCache->Set(Mapping->ItemID, *Mapping, MappingCacheTTL);

        LoadedCount++;

        if (bEnableDetailedLogging)
        {
            UE_LOG(LogSuspenseEquipmentAbility, Verbose,
                TEXT("Loaded ability mapping for item %s: %d abilities, %d effects"),
                *Mapping->ItemID.ToString(),
                Mapping->GrantedAbilities.Num(),
                Mapping->PassiveEffects.Num());
        }
    }

    ServiceMetrics.RecordValue(FName("Ability.Mappings.Loaded"), LoadedCount);
    ServiceMetrics.RecordValue(FName("Ability.Mappings.Invalid"), InvalidCount);

    if (InvalidCount > 0)
    {
        UE_LOG(LogSuspenseEquipmentAbility, Warning,
            TEXT("Loaded %d ability mappings from DataTable, skipped %d invalid entries"),
            LoadedCount, InvalidCount);
    }
    else
    {
        UE_LOG(LogSuspenseEquipmentAbility, Log,
            TEXT("Loaded %d ability mappings from DataTable"),
            LoadedCount);
    }

    ServiceMetrics.RecordSuccess();
    return LoadedCount;
}

USuspenseEquipmentAbilityConnector* USuspenseEquipmentAbilityService::GetOrCreateConnectorForEquipment(
    AActor* EquipmentActor,
    AActor* OwnerActor)
{
    SCOPED_SERVICE_TIMER("GetOrCreateConnectorForEquipment");

    if (!ensure(IsInGameThread()))
    {
        UE_LOG(LogSuspenseEquipmentAbility, Error, TEXT("GetOrCreateConnectorForEquipment must be called on GameThread"));
        ServiceMetrics.RecordError();
        return nullptr;
    }

    if (!EquipmentActor || !OwnerActor)
    {
        ServiceMetrics.RecordError();
        return nullptr;
    }

    // Check validity of actors
    if (!IsValid(EquipmentActor) || !IsValid(OwnerActor))
    {
        UE_LOG(LogSuspenseEquipmentAbility, Warning, TEXT("Equipment %s or Owner %s is not valid"),
            *GetNameSafe(EquipmentActor), *GetNameSafe(OwnerActor));
        ServiceMetrics.RecordError();
        return nullptr;
    }

    EQUIPMENT_WRITE_LOCK(ConnectorLock);

    // Check if connector already exists for this equipment
    USuspenseEquipmentAbilityConnector** ExistingConnector = EquipmentConnectors.Find(EquipmentActor);
    if (ExistingConnector && IsValid(*ExistingConnector))
    {
        ServiceMetrics.Inc(FName("Ability.Connectors.Reused"));
        return *ExistingConnector;
    }

    // Create new connector for equipment
    USuspenseEquipmentAbilityConnector* NewConnector = CreateConnectorForEquipment(EquipmentActor, OwnerActor);
    if (NewConnector)
    {
        EquipmentConnectors.Add(EquipmentActor, NewConnector);
        EquipmentToOwnerMap.Add(EquipmentActor, OwnerActor);
        ServiceMetrics.Inc(FName("Ability.Connectors.Created"));

        // Subscribe to equipment destruction
        EquipmentActor->OnDestroyed.AddDynamic(this, &USuspenseEquipmentAbilityService::OnEquipmentActorDestroyed);

        UE_LOG(LogSuspenseEquipmentAbility, Log,
            TEXT("Created ability connector for equipment %s owned by %s"),
            *GetNameSafe(EquipmentActor),
            *GetNameSafe(OwnerActor));

        ServiceMetrics.RecordSuccess();
    }
    else
    {
        ServiceMetrics.RecordError();
    }

    return NewConnector;
}

bool USuspenseEquipmentAbilityService::RemoveConnectorForEquipment(AActor* EquipmentActor)
{
    SCOPED_SERVICE_TIMER("RemoveConnectorForEquipment");

    if (!ensure(IsInGameThread()))
    {
        UE_LOG(LogSuspenseEquipmentAbility, Error, TEXT("RemoveConnectorForEquipment must be called on GameThread"));
        ServiceMetrics.RecordError();
        return false;
    }

    if (!EquipmentActor)
    {
        ServiceMetrics.RecordError();
        return false;
    }

    EQUIPMENT_WRITE_LOCK(ConnectorLock);

    USuspenseEquipmentAbilityConnector* Connector = nullptr;
    if (EquipmentConnectors.RemoveAndCopyValue(EquipmentActor, Connector))
    {
        if (IsValid(Connector))
        {
            Connector->ClearAll();
            Connector->DestroyComponent();
        }

        // Remove from owner map
        EquipmentToOwnerMap.Remove(EquipmentActor);

        ServiceMetrics.Inc(FName("Ability.Connectors.Destroyed"));

        // Unsubscribe from destruction (safe to call even if not subscribed)
        EquipmentActor->OnDestroyed.RemoveDynamic(this, &USuspenseEquipmentAbilityService::OnEquipmentActorDestroyed);

        UE_LOG(LogSuspenseEquipmentAbility, Log,
            TEXT("Removed ability connector for equipment %s"),
            *GetNameSafe(EquipmentActor));

        ServiceMetrics.RecordSuccess();
        return true;
    }

    // Not an error if connector doesn't exist (idempotent)
    return false;
}

bool USuspenseEquipmentAbilityService::HasAbilityMapping(FName ItemID) const
{
    EQUIPMENT_READ_LOCK(MappingLock);

    // Check cache first
    FEquipmentAbilityMapping CachedMapping;
    if (MappingCache->Get(ItemID, CachedMapping))
    {
        CacheHits++;
        return true;
    }

    CacheMisses++;
    return AbilityMappings.Contains(ItemID);
}

bool USuspenseEquipmentAbilityService::GetAbilityMapping(
    FName ItemID,
    FEquipmentAbilityMapping& OutMapping) const
{
    EQUIPMENT_READ_LOCK(MappingLock);

    // Check cache first
    if (MappingCache->Get(ItemID, OutMapping))
    {
        CacheHits++;
        ServiceMetrics.Inc(FName("Ability.Cache.Hit"));
        return true;
    }

    // Check main storage
    const FEquipmentAbilityMapping* Mapping = AbilityMappings.Find(ItemID);
    if (Mapping)
    {
        OutMapping = *Mapping;

        // Update cache
        const_cast<USuspenseEquipmentAbilityService*>(this)->MappingCache->Set(
            ItemID, *Mapping, MappingCacheTTL);

        CacheMisses++;
        ServiceMetrics.Inc(FName("Ability.Cache.Miss"));
        return true;
    }

    CacheMisses++;
    ServiceMetrics.Inc(FName("Ability.Cache.Miss"));
    return false;
}

bool USuspenseEquipmentAbilityService::ExportMetricsToCSV(const FString& FilePath) const
{
    SCOPED_SERVICE_TIMER_CONST("ExportMetricsToCSV");

    bool bResult = ServiceMetrics.ExportToCSV(FilePath, TEXT("EquipmentAbilityService"));

    if (bResult)
    {
        UE_LOG(LogSuspenseEquipmentAbility, Log, TEXT("Exported metrics to %s"), *FilePath);
    }
    else
    {
        UE_LOG(LogSuspenseEquipmentAbility, Error, TEXT("Failed to export metrics to %s"), *FilePath);
    }

    return bResult;
}

//========================================
// Public API - Operations
//========================================

void USuspenseEquipmentAbilityService::ProcessEquipmentSpawn(
    AActor* EquipmentActor,
    AActor* OwnerActor,
    const FSuspenseInventoryItemInstance& ItemInstance)
{
    SCOPED_SERVICE_TIMER("ProcessEquipmentSpawn");

    if (!ensure(IsInGameThread()))
    {
        UE_LOG(LogSuspenseEquipmentAbility, Error, TEXT("ProcessEquipmentSpawn must be called on GameThread"));
        ServiceMetrics.RecordError();
        return;
    }

    if (!EquipmentActor || !OwnerActor)
    {
        ServiceMetrics.RecordError();
        return;
    }

    if (!ItemInstance.IsValid())
    {
        UE_LOG(LogSuspenseEquipmentAbility, Warning,
            TEXT("Invalid item instance for equipment %s"),
            *GetNameSafe(EquipmentActor));
        ServiceMetrics.RecordError();
        return;
    }

    // Get or create connector for this equipment
    USuspenseEquipmentAbilityConnector* Connector = GetOrCreateConnectorForEquipment(EquipmentActor, OwnerActor);
    if (!Connector)
    {
        UE_LOG(LogSuspenseEquipmentAbility, Error,
            TEXT("Failed to get connector for equipment %s owned by %s"),
            *GetNameSafe(EquipmentActor),
            *GetNameSafe(OwnerActor));
        ServiceMetrics.RecordError();
        return;
    }

    // Check if we have a mapping for this item
    FEquipmentAbilityMapping Mapping;
    if (GetAbilityMapping(ItemInstance.ItemID, Mapping))
    {
        // Check requirements based on EQUIPMENT tags, not character tags
        if (Mapping.RequiredTags.Num() > 0 || Mapping.BlockedTags.Num() > 0)
        {
            FGameplayTagContainer EquipmentTags = GetEquipmentTags(EquipmentActor);

            // Check required tags
            if (Mapping.RequiredTags.Num() > 0 &&
                !EquipmentTags.HasAll(Mapping.RequiredTags))
            {
                UE_LOG(LogSuspenseEquipmentAbility, Warning,
                    TEXT("Equipment %s missing required tags for item %s. Required: %s, Has: %s"),
                    *GetNameSafe(EquipmentActor),
                    *ItemInstance.ItemID.ToString(),
                    *Mapping.RequiredTags.ToString(),
                    *EquipmentTags.ToString());
                ServiceMetrics.Inc(FName("Ability.Spawn.BlockedByTags"));
                return;
            }

            // Check blocked tags
            if (Mapping.BlockedTags.Num() > 0 &&
                EquipmentTags.HasAny(Mapping.BlockedTags))
            {
                UE_LOG(LogSuspenseEquipmentAbility, Warning,
                    TEXT("Equipment %s has blocked tags for item %s. Blocked: %s, Has: %s"),
                    *GetNameSafe(EquipmentActor),
                    *ItemInstance.ItemID.ToString(),
                    *Mapping.BlockedTags.ToString(),
                    *EquipmentTags.ToString());
                ServiceMetrics.Inc(FName("Ability.Spawn.BlockedByTags"));
                return;
            }
        }

        // Grant abilities through connector
        // The connector will use the ItemInstance's slot index if it has one
        int32 SlotIndex = ItemInstance.AnchorIndex != INDEX_NONE ? ItemInstance.AnchorIndex : 0;
        Connector->GrantAbilitiesForSlot(SlotIndex, ItemInstance);
        Connector->ApplyEffectsForSlot(SlotIndex, ItemInstance);

        if (bEnableDetailedLogging)
        {
            UE_LOG(LogSuspenseEquipmentAbility, Verbose,
                TEXT("Granted abilities for equipment %s (item: %s)"),
                *GetNameSafe(EquipmentActor),
                *ItemInstance.ItemID.ToString());
        }

        ServiceMetrics.Inc(FName("Ability.Spawn.Processed"));
    }
    else
    {
        if (bEnableDetailedLogging)
        {
            UE_LOG(LogSuspenseEquipmentAbility, Verbose,
                TEXT("No ability mapping for item %s on equipment %s"),
                *ItemInstance.ItemID.ToString(),
                *GetNameSafe(EquipmentActor));
        }
        ServiceMetrics.Inc(FName("Ability.Spawn.NoMapping"));
    }

    ServiceMetrics.RecordSuccess();
}

void USuspenseEquipmentAbilityService::ProcessEquipmentDestroy(AActor* EquipmentActor)
{
    SCOPED_SERVICE_TIMER("ProcessEquipmentDestroy");

    if (!ensure(IsInGameThread()))
    {
        UE_LOG(LogSuspenseEquipmentAbility, Error, TEXT("ProcessEquipmentDestroy must be called on GameThread"));
        ServiceMetrics.RecordError();
        return;
    }

    if (!EquipmentActor)
    {
        ServiceMetrics.RecordError();
        return;
    }

    // Remove connector and clean up abilities
    if (RemoveConnectorForEquipment(EquipmentActor))
    {
        ServiceMetrics.Inc(FName("Ability.Destroy.Processed"));
        ServiceMetrics.RecordSuccess();
    }
    else
    {
        // Not necessarily an error - equipment might not have had abilities
        if (bEnableDetailedLogging)
        {
            UE_LOG(LogSuspenseEquipmentAbility, Verbose,
                TEXT("No connector found for equipment %s"),
                *GetNameSafe(EquipmentActor));
        }
    }
}

void USuspenseEquipmentAbilityService::UpdateEquipmentAbilities(
    AActor* EquipmentActor,
    const FSuspenseInventoryItemInstance& UpdatedItemInstance)
{
    SCOPED_SERVICE_TIMER("UpdateEquipmentAbilities");

    if (!ensure(IsInGameThread()))
    {
        UE_LOG(LogSuspenseEquipmentAbility, Error, TEXT("UpdateEquipmentAbilities must be called on GameThread"));
        ServiceMetrics.RecordError();
        return;
    }

    if (!EquipmentActor || !UpdatedItemInstance.IsValid())
    {
        ServiceMetrics.RecordError();
        return;
    }

    EQUIPMENT_READ_LOCK(ConnectorLock);

    // Find existing connector
    USuspenseEquipmentAbilityConnector** ConnectorPtr = EquipmentConnectors.Find(EquipmentActor);
    if (ConnectorPtr && *ConnectorPtr)
    {
        USuspenseEquipmentAbilityConnector* Connector = *ConnectorPtr;

        // Clear current abilities/effects for the slot
        int32 SlotIndex = UpdatedItemInstance.AnchorIndex != INDEX_NONE ? UpdatedItemInstance.AnchorIndex : 0;
        Connector->RemoveAbilitiesForSlot(SlotIndex);
        Connector->RemoveEffectsForSlot(SlotIndex);

        // Re-grant with updated item data
        FEquipmentAbilityMapping Mapping;
        if (GetAbilityMapping(UpdatedItemInstance.ItemID, Mapping))
        {
            Connector->GrantAbilitiesForSlot(SlotIndex, UpdatedItemInstance);
            Connector->ApplyEffectsForSlot(SlotIndex, UpdatedItemInstance);

            UE_LOG(LogSuspenseEquipmentAbility, Log,
                TEXT("Updated abilities for equipment %s with item %s"),
                *GetNameSafe(EquipmentActor),
                *UpdatedItemInstance.ItemID.ToString());

            ServiceMetrics.Inc(FName("Ability.Updates.Processed"));
        }

        ServiceMetrics.RecordSuccess();
    }
    else
    {
        UE_LOG(LogSuspenseEquipmentAbility, Warning,
            TEXT("No connector found for equipment %s"),
            *GetNameSafe(EquipmentActor));
        ServiceMetrics.RecordError();
    }
}

int32 USuspenseEquipmentAbilityService::CleanupInvalidConnectors()
{
    SCOPED_SERVICE_TIMER("CleanupInvalidConnectors");
    EQUIPMENT_WRITE_LOCK(ConnectorLock);

    int32 CleanedCount = 0;

    // Find invalid connectors
    TArray<TWeakObjectPtr<AActor>> ToRemove;

    for (const auto& ConnectorPair : EquipmentConnectors)
    {
        if (!ConnectorPair.Key.IsValid() || !IsValid(ConnectorPair.Value))
        {
            ToRemove.Add(ConnectorPair.Key);
        }
    }

    // Remove invalid connectors
    for (const auto& Equipment : ToRemove)
    {
        USuspenseEquipmentAbilityConnector** ConnectorPtr = EquipmentConnectors.Find(Equipment);
        if (ConnectorPtr)
        {
            USuspenseEquipmentAbilityConnector* Connector = *ConnectorPtr;
            if (IsValid(Connector))
            {
                Connector->ClearAll();
                Connector->DestroyComponent();
            }
        }

        EquipmentConnectors.Remove(Equipment);
        EquipmentToOwnerMap.Remove(Equipment);
        CleanedCount++;
    }

    if (CleanedCount > 0)
    {
        UE_LOG(LogSuspenseEquipmentAbility, Log,
            TEXT("Cleaned up %d invalid equipment connectors"),
            CleanedCount);
        ServiceMetrics.RecordValue(FName("Ability.Connectors.Cleaned"), CleanedCount);
    }

    return CleanedCount;
}

//========================================
// Protected Methods
//========================================

void USuspenseEquipmentAbilityService::InitializeDefaultMappings()
{
    // Try to load default mapping table
    if (DefaultMappingTable.IsNull())
    {
        UE_LOG(LogSuspenseEquipmentAbility, Log, TEXT("No default mapping table configured"));
        return;
    }

#if !(UE_BUILD_SHIPPING)
    // Synchronous load for development builds
    if (UDataTable* DefaultTable = DefaultMappingTable.LoadSynchronous())
    {
        int32 Loaded = LoadAbilityMappings(DefaultTable);
        UE_LOG(LogSuspenseEquipmentAbility, Log,
            TEXT("Loaded %d default ability mappings"),
            Loaded);
    }
    else
    {
        UE_LOG(LogSuspenseEquipmentAbility, Warning,
            TEXT("Failed to load default mapping table from %s"),
            *DefaultMappingTable.ToString());
    }
#else
    // Async load for shipping builds
    TWeakObjectPtr<USuspenseEquipmentAbilityService> WeakThis(this);
    StreamableManager.RequestAsyncLoad(
        DefaultMappingTable.ToSoftObjectPath(),
        [WeakThis]()
        {
            if (USuspenseEquipmentAbilityService* StrongThis = WeakThis.Get())
            {
                if (UDataTable* DefaultTable = StrongThis->DefaultMappingTable.Get())
                {
                    int32 Loaded = StrongThis->LoadAbilityMappings(DefaultTable);
                    UE_LOG(LogSuspenseEquipmentAbility, Log,
                        TEXT("Async loaded %d default ability mappings"),
                        Loaded);
                }
            }
        }
    );
#endif
}

void USuspenseEquipmentAbilityService::SetupEventHandlers()
{
    auto EventBus = FSuspenseEquipmentEventBus::Get();
    if (!EventBus.IsValid())
    {
        UE_LOG(LogSuspenseEquipmentAbility, Warning,
            TEXT("EventBus not available, event handling disabled"));
        return;
    }

    // Spawned
    FEventSubscriptionHandle SpawnedHandle = EventBus->Subscribe(
        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Spawned")),
        FEventHandlerDelegate::CreateUObject(
            this, &USuspenseEquipmentAbilityService::OnEquipmentSpawned),
        EEventPriority::High
    );
    EventSubscriptions.Add(SpawnedHandle);

    // Destroyed
    FEventSubscriptionHandle DestroyedHandle = EventBus->Subscribe(
        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Destroyed")),
        FEventHandlerDelegate::CreateUObject(
            this, &USuspenseEquipmentAbilityService::OnEquipmentDestroyed),
        EEventPriority::High
    );
    EventSubscriptions.Add(DestroyedHandle);

    // === S7: Equipped / Unequipped / Refresh / Commit ===
    if (Tag_OnEquipped.IsValid())
    {
        EventSubscriptions.Add(EventBus->Subscribe(
            Tag_OnEquipped,
            FEventHandlerDelegate::CreateUObject(this, &USuspenseEquipmentAbilityService::OnEquipped),
            EEventPriority::High));
    }
    if (Tag_OnUnequipped.IsValid())
    {
        EventSubscriptions.Add(EventBus->Subscribe(
            Tag_OnUnequipped,
            FEventHandlerDelegate::CreateUObject(this, &USuspenseEquipmentAbilityService::OnUnequipped),
            EEventPriority::High));
    }
    if (Tag_OnAbilitiesRefresh.IsValid())
    {
        EventSubscriptions.Add(EventBus->Subscribe(
            Tag_OnAbilitiesRefresh,
            FEventHandlerDelegate::CreateUObject(this, &USuspenseEquipmentAbilityService::OnAbilitiesRefresh),
            EEventPriority::Normal));
    }
    if (Tag_OnCommit.IsValid())
    {
        EventSubscriptions.Add(EventBus->Subscribe(
            Tag_OnCommit,
            FEventHandlerDelegate::CreateUObject(this, &USuspenseEquipmentAbilityService::OnCommit),
            EEventPriority::Normal));
    }

    UE_LOG(LogSuspenseEquipmentAbility, Log, TEXT("Event handlers registered"));
}

void USuspenseEquipmentAbilityService::EnsureValidConfig()
{
    // Sanitize cache TTL
    MappingCacheTTL = FMath::Clamp(MappingCacheTTL, 60.0f, 3600.0f);

    // Sanitize cleanup interval
    CleanupInterval = FMath::Clamp(CleanupInterval, 10.0f, 300.0f);

    UE_LOG(LogSuspenseEquipmentAbility, Log,
        TEXT("Configuration sanitized: CacheTTL=%.1fs, CleanupInterval=%.1fs"),
        MappingCacheTTL, CleanupInterval);
}

void USuspenseEquipmentAbilityService::OnEquipmentSpawned(const FSuspenseEquipmentEventData& EventData)
{
    // Parse structured event data
    FSuspenseInventoryItemInstance ItemInstance;
    AActor* EquipmentActor = nullptr;
    AActor* OwnerActor = nullptr;

    if (!ParseEquipmentEventData(EventData, ItemInstance, EquipmentActor, OwnerActor))
    {
        UE_LOG(LogSuspenseEquipmentAbility, Warning,
            TEXT("Failed to parse equipment spawned event"));
        ServiceMetrics.Inc(FName("Ability.Events.ParseFailed"));
        return;
    }

    ProcessEquipmentSpawn(EquipmentActor, OwnerActor, ItemInstance);
    ServiceMetrics.Inc(FName("Ability.Events.Spawned"));
}

void USuspenseEquipmentAbilityService::OnEquipmentDestroyed(const FSuspenseEquipmentEventData& EventData)
{
    AActor* EquipmentActor = Cast<AActor>(EventData.Source.Get());
    if (!EquipmentActor)
    {
        ServiceMetrics.Inc(FName("Ability.Events.InvalidSource"));
        return;
    }

    ProcessEquipmentDestroy(EquipmentActor);
    ServiceMetrics.Inc(FName("Ability.Events.Destroyed"));
}

// === S7 Handlers ===

void USuspenseEquipmentAbilityService::OnEquipped(const FSuspenseEquipmentEventData& EventData)
{
    FSuspenseInventoryItemInstance ItemInstance;
    AActor* EquipmentActor = nullptr;
    AActor* OwnerActor = nullptr;
    if (!ParseEquipmentEventData(EventData, ItemInstance, EquipmentActor, OwnerActor))
    {
        UE_LOG(LogSuspenseEquipmentAbility, Warning, TEXT("OnEquipped: parse failed"));
        ServiceMetrics.Inc(FName("Ability.Events.ParseFailed"));
        return;
    }
    ProcessEquipmentSpawn(EquipmentActor, OwnerActor, ItemInstance);
    ServiceMetrics.Inc(FName("Ability.Events.Equipped"));
}

void USuspenseEquipmentAbilityService::OnUnequipped(const FSuspenseEquipmentEventData& EventData)
{
    AActor* EquipmentActor = Cast<AActor>(EventData.Source.Get());
    if (!EquipmentActor)
    {
        // Try full parse (in case source differs)
        FSuspenseInventoryItemInstance Item;
        AActor* Owner = nullptr;
        if (!ParseEquipmentEventData(EventData, Item, EquipmentActor, Owner) || !EquipmentActor)
        {
            UE_LOG(LogSuspenseEquipmentAbility, Warning, TEXT("OnUnequipped: invalid source"));
            ServiceMetrics.Inc(FName("Ability.Events.InvalidSource"));
            return;
        }
    }
    ProcessEquipmentDestroy(EquipmentActor);
    ServiceMetrics.Inc(FName("Ability.Events.Unequipped"));
}

void USuspenseEquipmentAbilityService::OnAbilitiesRefresh(const FSuspenseEquipmentEventData& EventData)
{
    FSuspenseInventoryItemInstance ItemInstance;
    AActor* EquipmentActor = nullptr;
    AActor* OwnerActor = nullptr;
    if (!ParseEquipmentEventData(EventData, ItemInstance, EquipmentActor, OwnerActor))
    {
        UE_LOG(LogSuspenseEquipmentAbility, Warning, TEXT("OnAbilitiesRefresh: parse failed"));
        ServiceMetrics.Inc(FName("Ability.Events.ParseFailed"));
        return;
    }
    UpdateEquipmentAbilities(EquipmentActor, ItemInstance);
    ServiceMetrics.Inc(FName("Ability.Events.Refresh"));
}

void USuspenseEquipmentAbilityService::OnCommit(const FSuspenseEquipmentEventData& EventData)
{
    FSuspenseInventoryItemInstance ItemInstance;
    AActor* EquipmentActor = nullptr;
    AActor* OwnerActor = nullptr;
    if (!ParseEquipmentEventData(EventData, ItemInstance, EquipmentActor, OwnerActor))
    {
        UE_LOG(LogSuspenseEquipmentAbility, Warning, TEXT("OnCommit: parse failed"));
        ServiceMetrics.Inc(FName("Ability.Events.ParseFailed"));
        return;
    }
    UpdateEquipmentAbilities(EquipmentActor, ItemInstance);
    ServiceMetrics.Inc(FName("Ability.Events.Commit"));
}

void USuspenseEquipmentAbilityService::OnEquipmentActorDestroyed(AActor* DestroyedActor)
{
    // Idempotent - RemoveConnectorForEquipment handles if connector doesn't exist
    RemoveConnectorForEquipment(DestroyedActor);
}

void USuspenseEquipmentAbilityService::OnCleanupTimer()
{
    int32 Cleaned = CleanupInvalidConnectors();
    if (Cleaned > 0 && bEnableDetailedLogging)
    {
        UE_LOG(LogSuspenseEquipmentAbility, Verbose,
            TEXT("Periodic cleanup removed %d invalid connectors"),
            Cleaned);
    }
}

USuspenseEquipmentAbilityConnector* USuspenseEquipmentAbilityService::CreateConnectorForEquipment(
    AActor* EquipmentActor,
    AActor* OwnerActor)
{
    if (!EquipmentActor || !OwnerActor)
    {
        return nullptr;
    }

    // Find ASC on the OWNER (character/pawn), not the equipment
    UAbilitySystemComponent* ASC = FindOwnerAbilitySystemComponent(OwnerActor);
    if (!ASC)
    {
        UE_LOG(LogSuspenseEquipmentAbility, Warning,
            TEXT("No AbilitySystemComponent found on owner %s (checked: component, interface, controller, playerstate)"),
            *GetNameSafe(OwnerActor));
        return nullptr;
    }

    // Create connector component on the EQUIPMENT actor
    USuspenseEquipmentAbilityConnector* Connector = NewObject<USuspenseEquipmentAbilityConnector>(
        EquipmentActor,
        USuspenseEquipmentAbilityConnector::StaticClass(),
        TEXT("EquipmentAbilityConnector"),
        RF_Transient
    );

    if (!Connector)
    {
        UE_LOG(LogSuspenseEquipmentAbility, Error,
            TEXT("Failed to create ability connector"));
        return nullptr;
    }

    // Register component with equipment actor
    Connector->RegisterComponent();

    // Initialize connector with owner's ASC
    TScriptInterface<ISuspenseEquipmentDataProvider> DataProvider;
    if (!Connector->Initialize(ASC, DataProvider))
    {
        UE_LOG(LogSuspenseEquipmentAbility, Error,
            TEXT("Failed to initialize ability connector"));
        Connector->DestroyComponent();
        return nullptr;
    }

    return Connector;
}

UAbilitySystemComponent* USuspenseEquipmentAbilityService::FindOwnerAbilitySystemComponent(AActor* OwnerActor) const
{
    if (!OwnerActor)
    {
        return nullptr;
    }

    // 1. Try to find ASC as component on owner
    if (UAbilitySystemComponent* ASC = OwnerActor->FindComponentByClass<UAbilitySystemComponent>())
    {
        return ASC;
    }

    // 2. Try through interface on owner
    if (OwnerActor->GetClass()->ImplementsInterface(UAbilitySystemInterface::StaticClass()))
    {
        if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(OwnerActor))
        {
            if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
            {
                return ASC;
            }
        }
    }

    // 3. Try on controller (if owner is a pawn)
    if (APawn* Pawn = Cast<APawn>(OwnerActor))
    {
        if (AController* Controller = Pawn->GetController())
        {
            if (Controller->GetClass()->ImplementsInterface(UAbilitySystemInterface::StaticClass()))
            {
                if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Controller))
                {
                    if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
                    {
                        return ASC;
                    }
                }
            }
        }
    }

    // 4. Try on player state (most common in your architecture)
    if (APawn* Pawn = Cast<APawn>(OwnerActor))
    {
        if (APlayerState* PS = Pawn->GetPlayerState())
        {
            // Direct component check
            if (UAbilitySystemComponent* ASC = PS->FindComponentByClass<UAbilitySystemComponent>())
            {
                return ASC;
            }

            // Interface check
            if (PS->GetClass()->ImplementsInterface(UAbilitySystemInterface::StaticClass()))
            {
                if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(PS))
                {
                    if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
                    {
                        return ASC;
                    }
                }
            }
        }
    }

    return nullptr;
}

FGameplayTagContainer USuspenseEquipmentAbilityService::GetEquipmentTags(AActor* EquipmentActor) const
{
    FGameplayTagContainer Tags;

    if (!EquipmentActor || !IsValid(EquipmentActor))
    {
        UE_LOG(LogSuspenseEquipmentAbility, Warning, TEXT("GetEquipmentTags called with invalid equipment"));
        return Tags;
    }

    // Option 1: If any component implements IGameplayTagAssetInterface
    if (UActorComponent* TagComponent = EquipmentActor->GetComponentByClass(UGameplayTagAssetInterface::StaticClass()))
    {
        if (IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(TagComponent))
        {
            TagInterface->GetOwnedGameplayTags(Tags);
        }
    }

    // Option 2: Actor implements IGameplayTagAssetInterface
    if (EquipmentActor->GetClass()->ImplementsInterface(UGameplayTagAssetInterface::StaticClass()))
    {
        if (IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(EquipmentActor))
        {
            FGameplayTagContainer ActorTags;
            TagInterface->GetOwnedGameplayTags(ActorTags);
            Tags.AppendTags(ActorTags);
        }
    }

    return Tags;
}

bool USuspenseEquipmentAbilityService::ParseEquipmentEventData(
    const FSuspenseEquipmentEventData& EventData,
    FSuspenseInventoryItemInstance& OutItem,
    AActor*& OutEquipmentActor,
    AActor*& OutOwnerActor) const
{
    // Equipment actor should be in Source
    OutEquipmentActor = Cast<AActor>(EventData.Source.Get());
    if (!OutEquipmentActor)
    {
        return false;
    }

    // Owner should be in Target
    OutOwnerActor = Cast<AActor>(EventData.Target.Get());
    if (!OutOwnerActor)
    {
        return false;
    }

    // Try to parse item data from metadata
    FString ItemIDStr = EventData.GetMetadata(TEXT("ItemID"));
    if (!ItemIDStr.IsEmpty())
    {
        OutItem.ItemID = *ItemIDStr;

        FString InstanceIDStr = EventData.GetMetadata(TEXT("InstanceID"));
        if (!InstanceIDStr.IsEmpty())
        {
            FGuid::Parse(InstanceIDStr, OutItem.InstanceID);
        }

        FString QuantityStr = EventData.GetMetadata(TEXT("Quantity"));
        if (!QuantityStr.IsEmpty())
        {
            OutItem.Quantity = FCString::Atoi(*QuantityStr);
        }

        return OutItem.IsValid();
    }

    // Fallback: Try JSON parsing from payload
    if (!EventData.Payload.IsEmpty())
    {
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(EventData.Payload);

        if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
        {
            FString ItemID;
            if (JsonObject->TryGetStringField(TEXT("ItemID"), ItemID))
            {
                OutItem.ItemID = *ItemID;
            }

            FString InstanceID;
            if (JsonObject->TryGetStringField(TEXT("InstanceID"), InstanceID))
            {
                FGuid::Parse(InstanceID, OutItem.InstanceID);
            }

            int32 Quantity;
            if (JsonObject->TryGetNumberField(TEXT("Quantity"), Quantity))
            {
                OutItem.Quantity = Quantity;
            }

            return OutItem.IsValid();
        }
    }

    return false;
}
