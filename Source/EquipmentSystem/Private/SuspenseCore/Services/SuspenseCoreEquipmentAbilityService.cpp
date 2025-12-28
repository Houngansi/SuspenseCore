// EquipmentAbilityServiceImpl.cpp
// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Services/SuspenseCoreEquipmentAbilityService.h"
#include "SuspenseCore/Components/Integration/SuspenseCoreEquipmentAbilityConnector.h"
#include "SuspenseCore/Components/Coordination/SuspenseCoreEquipmentEventDispatcher.h"
#include "SuspenseCore/Services/SuspenseCoreServiceProvider.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

USuspenseCoreEquipmentAbilityService::USuspenseCoreEquipmentAbilityService()
{
    // Initialize cache with reasonable size
    MappingCache = MakeShareable(new FSuspenseCoreEquipmentCacheManager<FName, FSuspenseCoreEquipmentAbilityMapping>(100));
}

USuspenseCoreEquipmentAbilityService::~USuspenseCoreEquipmentAbilityService()
{
    ShutdownService(true);
}

void USuspenseCoreEquipmentAbilityService::BeginDestroy()
{
    ShutdownService(true);
    Super::BeginDestroy();
}

//========================================
// IEquipmentService Implementation
//========================================

bool USuspenseCoreEquipmentAbilityService::InitializeService(const FSuspenseCoreServiceInitParams& Params)
{
    UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] ═══════════════════════════════════════════════════════"));
    UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] AbilityService::InitializeService CALLED"));

    SCOPED_SERVICE_TIMER("InitializeService");
    EQUIPMENT_WRITE_LOCK(ConnectorLock);

    if (ServiceState != ESuspenseCoreServiceLifecycleState::Uninitialized)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG]   Service already initialized, returning"));
        UE_LOG(LogSuspenseCoreEquipmentAbility, Warning, TEXT("Service already initialized"));
        ServiceMetrics.RecordError();
        return false;
    }

    ServiceState = ESuspenseCoreServiceLifecycleState::Initializing;

    // Ensure valid configuration
    EnsureValidConfig();

    // Load default mappings
    InitializeDefaultMappings();

    // Initialize S7 event tags using SuspenseCore.Event.* format (per BestPractices.md)
    Tag_OnEquipped        = FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Equipment.Equipped"), /*ErrorIfNotFound*/false);
    Tag_OnUnequipped      = FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Equipment.Unequipped"), /*ErrorIfNotFound*/false);
    Tag_OnAbilitiesRefresh= FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Equipment.Ability.Refresh"), /*ErrorIfNotFound*/false);
    Tag_OnCommit          = FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Equipment.Commit"), /*ErrorIfNotFound*/false);

    // Setup event handlers
    SetupEventHandlers();

    // Register cache for monitoring
    FSuspenseCoreGlobalCacheRegistry::Get().RegisterCache(
        TEXT("EquipmentAbilityService.Mappings"),
        [this]() {
            float HitRate = (CacheHits + CacheMisses) > 0
                ? (float)CacheHits / (CacheHits + CacheMisses) * 100.0f
                : 0.0f;
            return FString::Printf(TEXT("Cache: Hits=%d, Misses=%d, HitRate=%.1f%%\n%s"),
                CacheHits.load(), CacheMisses.load(), HitRate,
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
                &USuspenseCoreEquipmentAbilityService::OnCleanupTimer,
                CleanupInterval,
                true // Loop
            );

            UE_LOG(LogSuspenseCoreEquipmentAbility, Log, TEXT("Periodic cleanup enabled every %.1f seconds"),
                CleanupInterval);
        }
    }

    ServiceState = ESuspenseCoreServiceLifecycleState::Ready;
    ServiceMetrics.RecordSuccess();

    UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] ═══════════════════════════════════════════════════════"));
    UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] AbilityService::InitializeService COMPLETE"));
    UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG]   State: Ready, Mappings: %d, EventBus: %s"),
        AbilityMappings.Num(),
        EventBus ? TEXT("VALID") : TEXT("NULL"));
    UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] ═══════════════════════════════════════════════════════"));

    UE_LOG(LogSuspenseCoreEquipmentAbility, Log, TEXT("EquipmentAbilityService initialized with %d mappings"),
        AbilityMappings.Num());

    return true;
}

bool USuspenseCoreEquipmentAbilityService::ShutdownService(bool bForce)
{
    SCOPED_SERVICE_TIMER("ShutdownService");
    EQUIPMENT_WRITE_LOCK(ConnectorLock);

    if (ServiceState == ESuspenseCoreServiceLifecycleState::Shutdown)
    {
        return true;
    }

    ServiceState = ESuspenseCoreServiceLifecycleState::Shutting;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(CleanupTimerHandle);
    }

    for (auto& Pair : EquipmentConnectors)
    {
        if (USuspenseCoreEquipmentAbilityConnector* Connector = Pair.Value)
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

    // Безопасная отписка от EventBus (SuspenseCore architecture)
    if (EventBus)
    {
        for (const FSuspenseCoreSubscriptionHandle& Handle : EventSubscriptions)
        {
            EventBus->Unsubscribe(Handle);
        }
    }
    EventSubscriptions.Empty();
    EventBus = nullptr;

    // Безопасная очистка кеша
    if (MappingCache.IsValid())
    {
        MappingCache->Clear();
    }

    // ВАЖНО: во время выхода движка НЕ трогаем глобальные реестры/синглтоны
    if (bCacheRegistered && !IsEngineExitRequested())
    {
        FSuspenseCoreGlobalCacheRegistry::Get().UnregisterCache(TEXT("EquipmentAbilityService.Mappings"));
        bCacheRegistered = false;
    }

    ServiceState = ESuspenseCoreServiceLifecycleState::Shutdown;
    ServiceMetrics.RecordSuccess();
    UE_LOG(LogSuspenseCoreEquipmentAbility, Log, TEXT("EquipmentAbilityService shutdown complete"));
    return true;
}

FGameplayTag USuspenseCoreEquipmentAbilityService::GetServiceTag() const
{
    return FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Service.Equipment.Ability"));
}

FGameplayTagContainer USuspenseCoreEquipmentAbilityService::GetRequiredDependencies() const
{
    FGameplayTagContainer Dependencies;
    // We don't strictly require other services - we're self-contained
    return Dependencies;
}

bool USuspenseCoreEquipmentAbilityService::ValidateService(TArray<FText>& OutErrors) const
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

void USuspenseCoreEquipmentAbilityService::ResetService()
{
    SCOPED_SERVICE_TIMER("ResetService");
    EQUIPMENT_WRITE_LOCK(ConnectorLock);

    // Clear all connectors
    for (auto& ConnectorPair : EquipmentConnectors)
    {
        if (USuspenseCoreEquipmentAbilityConnector* Connector = ConnectorPair.Value)
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

    UE_LOG(LogSuspenseCoreEquipmentAbility, Log, TEXT("EquipmentAbilityService reset"));
}

FString USuspenseCoreEquipmentAbilityService::GetServiceStats() const
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
        CacheHits.load(), CacheMisses.load(), HitRate);

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

                if (USuspenseCoreEquipmentAbilityConnector* Connector = ConnectorPair.Value)
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

int32 USuspenseCoreEquipmentAbilityService::LoadAbilityMappings(UDataTable* MappingTable)
{
    SCOPED_SERVICE_TIMER("LoadAbilityMappings");

    if (!ensure(IsInGameThread()))
    {
        UE_LOG(LogSuspenseCoreEquipmentAbility, Error, TEXT("LoadAbilityMappings must be called on GameThread"));
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
        const FSuspenseCoreEquipmentAbilityMapping* Mapping =
            MappingTable->FindRow<FSuspenseCoreEquipmentAbilityMapping>(RowName, TEXT("LoadAbilityMappings"));

        if (!Mapping)
        {
            UE_LOG(LogSuspenseCoreEquipmentAbility, Warning,
                TEXT("Failed to cast row %s to FSuspenseCoreEquipmentAbilityMapping"),
                *RowName.ToString());
            InvalidCount++;
            continue;
        }

        // Validate mapping
        if (!Mapping->IsValid())
        {
            UE_LOG(LogSuspenseCoreEquipmentAbility, Warning,
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
                UE_LOG(LogSuspenseCoreEquipmentAbility, Warning,
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
                UE_LOG(LogSuspenseCoreEquipmentAbility, Warning,
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
            UE_LOG(LogSuspenseCoreEquipmentAbility, Verbose,
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
        UE_LOG(LogSuspenseCoreEquipmentAbility, Warning,
            TEXT("Loaded %d ability mappings from DataTable, skipped %d invalid entries"),
            LoadedCount, InvalidCount);
    }
    else
    {
        UE_LOG(LogSuspenseCoreEquipmentAbility, Log,
            TEXT("Loaded %d ability mappings from DataTable"),
            LoadedCount);
    }

    ServiceMetrics.RecordSuccess();
    return LoadedCount;
}

USuspenseCoreEquipmentAbilityConnector* USuspenseCoreEquipmentAbilityService::GetOrCreateConnectorForEquipment(
    AActor* EquipmentActor,
    AActor* OwnerActor)
{
    SCOPED_SERVICE_TIMER("GetOrCreateConnectorForEquipment");

    if (!ensure(IsInGameThread()))
    {
        UE_LOG(LogSuspenseCoreEquipmentAbility, Error, TEXT("GetOrCreateConnectorForEquipment must be called on GameThread"));
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
        UE_LOG(LogSuspenseCoreEquipmentAbility, Warning, TEXT("Equipment %s or Owner %s is not valid"),
            *GetNameSafe(EquipmentActor), *GetNameSafe(OwnerActor));
        ServiceMetrics.RecordError();
        return nullptr;
    }

    EQUIPMENT_WRITE_LOCK(ConnectorLock);

    // Check if connector already exists for this equipment
    USuspenseCoreEquipmentAbilityConnector** ExistingConnector = EquipmentConnectors.Find(EquipmentActor);
    if (ExistingConnector && IsValid(*ExistingConnector))
    {
        ServiceMetrics.Inc(FName("Ability.Connectors.Reused"));
        return *ExistingConnector;
    }

    // Create new connector for equipment
    USuspenseCoreEquipmentAbilityConnector* NewConnector = CreateConnectorForEquipment(EquipmentActor, OwnerActor);
    if (NewConnector)
    {
        EquipmentConnectors.Add(EquipmentActor, NewConnector);
        EquipmentToOwnerMap.Add(EquipmentActor, OwnerActor);
        ServiceMetrics.Inc(FName("Ability.Connectors.Created"));

        // Subscribe to equipment destruction
        EquipmentActor->OnDestroyed.AddDynamic(this, &USuspenseCoreEquipmentAbilityService::OnEquipmentActorDestroyed);

        UE_LOG(LogSuspenseCoreEquipmentAbility, Log,
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

bool USuspenseCoreEquipmentAbilityService::RemoveConnectorForEquipment(AActor* EquipmentActor)
{
    SCOPED_SERVICE_TIMER("RemoveConnectorForEquipment");

    if (!ensure(IsInGameThread()))
    {
        UE_LOG(LogSuspenseCoreEquipmentAbility, Error, TEXT("RemoveConnectorForEquipment must be called on GameThread"));
        ServiceMetrics.RecordError();
        return false;
    }

    if (!EquipmentActor)
    {
        ServiceMetrics.RecordError();
        return false;
    }

    EQUIPMENT_WRITE_LOCK(ConnectorLock);

    USuspenseCoreEquipmentAbilityConnector* Connector = nullptr;
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
        EquipmentActor->OnDestroyed.RemoveDynamic(this, &USuspenseCoreEquipmentAbilityService::OnEquipmentActorDestroyed);

        UE_LOG(LogSuspenseCoreEquipmentAbility, Log,
            TEXT("Removed ability connector for equipment %s"),
            *GetNameSafe(EquipmentActor));

        ServiceMetrics.RecordSuccess();
        return true;
    }

    // Not an error if connector doesn't exist (idempotent)
    return false;
}

bool USuspenseCoreEquipmentAbilityService::HasAbilityMapping(FName ItemID) const
{
    EQUIPMENT_READ_LOCK(MappingLock);

    // Check cache first
    FSuspenseCoreEquipmentAbilityMapping CachedMapping;
    if (MappingCache->Get(ItemID, CachedMapping))
    {
        CacheHits++;
        return true;
    }

    CacheMisses++;
    return AbilityMappings.Contains(ItemID);
}

bool USuspenseCoreEquipmentAbilityService::GetAbilityMapping(
    FName ItemID,
    FSuspenseCoreEquipmentAbilityMapping& OutMapping) const
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
    const FSuspenseCoreEquipmentAbilityMapping* Mapping = AbilityMappings.Find(ItemID);
    if (Mapping)
    {
        OutMapping = *Mapping;

        // Update cache
        const_cast<USuspenseCoreEquipmentAbilityService*>(this)->MappingCache->Set(
            ItemID, *Mapping, MappingCacheTTL);

        CacheMisses++;
        ServiceMetrics.Inc(FName("Ability.Cache.Miss"));
        return true;
    }

    CacheMisses++;
    ServiceMetrics.Inc(FName("Ability.Cache.Miss"));
    return false;
}

bool USuspenseCoreEquipmentAbilityService::ExportMetricsToCSV(const FString& FilePath) const
{
    SCOPED_SERVICE_TIMER_CONST("ExportMetricsToCSV");

    bool bResult = ServiceMetrics.ExportToCSV(FilePath, TEXT("EquipmentAbilityService"));

    if (bResult)
    {
        UE_LOG(LogSuspenseCoreEquipmentAbility, Log, TEXT("Exported metrics to %s"), *FilePath);
    }
    else
    {
        UE_LOG(LogSuspenseCoreEquipmentAbility, Error, TEXT("Failed to export metrics to %s"), *FilePath);
    }

    return bResult;
}

//========================================
// Public API - Operations
//========================================

void USuspenseCoreEquipmentAbilityService::ProcessEquipmentSpawn(
    AActor* EquipmentActor,
    AActor* OwnerActor,
    const FSuspenseCoreInventoryItemInstance& ItemInstance)
{
    SCOPED_SERVICE_TIMER("ProcessEquipmentSpawn");

    if (!ensure(IsInGameThread()))
    {
        UE_LOG(LogSuspenseCoreEquipmentAbility, Error, TEXT("ProcessEquipmentSpawn must be called on GameThread"));
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
        UE_LOG(LogSuspenseCoreEquipmentAbility, Warning,
            TEXT("Invalid item instance for equipment %s"),
            *GetNameSafe(EquipmentActor));
        ServiceMetrics.RecordError();
        return;
    }

    // Get or create connector for this equipment
    USuspenseCoreEquipmentAbilityConnector* Connector = GetOrCreateConnectorForEquipment(EquipmentActor, OwnerActor);
    if (!Connector)
    {
        UE_LOG(LogSuspenseCoreEquipmentAbility, Error,
            TEXT("Failed to get connector for equipment %s owned by %s"),
            *GetNameSafe(EquipmentActor),
            *GetNameSafe(OwnerActor));
        ServiceMetrics.RecordError();
        return;
    }

    // Check if we have a mapping for this item (optional - used for tag filtering)
    FSuspenseCoreEquipmentAbilityMapping Mapping;
    bool bHasMapping = GetAbilityMapping(ItemInstance.ItemID, Mapping);

    // If mapping exists, check tag requirements
    if (bHasMapping && (Mapping.RequiredTags.Num() > 0 || Mapping.BlockedTags.Num() > 0))
    {
        FGameplayTagContainer EquipmentTags = GetEquipmentTags(EquipmentActor);

        // Check required tags
        if (Mapping.RequiredTags.Num() > 0 &&
            !EquipmentTags.HasAll(Mapping.RequiredTags))
        {
            UE_LOG(LogSuspenseCoreEquipmentAbility, Warning,
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
            UE_LOG(LogSuspenseCoreEquipmentAbility, Warning,
                TEXT("Equipment %s has blocked tags for item %s. Blocked: %s, Has: %s"),
                *GetNameSafe(EquipmentActor),
                *ItemInstance.ItemID.ToString(),
                *Mapping.BlockedTags.ToString(),
                *EquipmentTags.ToString());
            ServiceMetrics.Inc(FName("Ability.Spawn.BlockedByTags"));
            return;
        }
    }

    // Grant abilities through connector (from DataTable's GrantedAbilities)
    // Mapping is optional - abilities are always granted from DataTable
    int32 SlotIndex = ItemInstance.AnchorIndex != INDEX_NONE ? ItemInstance.AnchorIndex : 0;
    Connector->GrantAbilitiesForSlot(SlotIndex, ItemInstance);
    Connector->ApplyEffectsForSlot(SlotIndex, ItemInstance);

    UE_LOG(LogSuspenseCoreEquipmentAbility, Log,
        TEXT("[ADS DEBUG] Granted abilities for equipment %s (item: %s, slot: %d, hasMapping: %s)"),
        *GetNameSafe(EquipmentActor),
        *ItemInstance.ItemID.ToString(),
        SlotIndex,
        bHasMapping ? TEXT("YES") : TEXT("NO"));

    ServiceMetrics.Inc(FName("Ability.Spawn.Processed"));

    ServiceMetrics.RecordSuccess();
}

void USuspenseCoreEquipmentAbilityService::ProcessEquipmentDestroy(AActor* EquipmentActor)
{
    SCOPED_SERVICE_TIMER("ProcessEquipmentDestroy");

    if (!ensure(IsInGameThread()))
    {
        UE_LOG(LogSuspenseCoreEquipmentAbility, Error, TEXT("ProcessEquipmentDestroy must be called on GameThread"));
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
            UE_LOG(LogSuspenseCoreEquipmentAbility, Verbose,
                TEXT("No connector found for equipment %s"),
                *GetNameSafe(EquipmentActor));
        }
    }
}

void USuspenseCoreEquipmentAbilityService::UpdateEquipmentAbilities(
    AActor* EquipmentActor,
    const FSuspenseCoreInventoryItemInstance& UpdatedItemInstance)
{
    SCOPED_SERVICE_TIMER("UpdateEquipmentAbilities");

    if (!ensure(IsInGameThread()))
    {
        UE_LOG(LogSuspenseCoreEquipmentAbility, Error, TEXT("UpdateEquipmentAbilities must be called on GameThread"));
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
    USuspenseCoreEquipmentAbilityConnector** ConnectorPtr = EquipmentConnectors.Find(EquipmentActor);
    if (ConnectorPtr && *ConnectorPtr)
    {
        USuspenseCoreEquipmentAbilityConnector* Connector = *ConnectorPtr;

        // Clear current abilities/effects for the slot
        int32 SlotIndex = UpdatedItemInstance.AnchorIndex != INDEX_NONE ? UpdatedItemInstance.AnchorIndex : 0;
        Connector->RemoveAbilitiesForSlot(SlotIndex);
        Connector->RemoveEffectsForSlot(SlotIndex);

        // Re-grant with updated item data
        FSuspenseCoreEquipmentAbilityMapping Mapping;
        if (GetAbilityMapping(UpdatedItemInstance.ItemID, Mapping))
        {
            Connector->GrantAbilitiesForSlot(SlotIndex, UpdatedItemInstance);
            Connector->ApplyEffectsForSlot(SlotIndex, UpdatedItemInstance);

            UE_LOG(LogSuspenseCoreEquipmentAbility, Log,
                TEXT("Updated abilities for equipment %s with item %s"),
                *GetNameSafe(EquipmentActor),
                *UpdatedItemInstance.ItemID.ToString());

            ServiceMetrics.Inc(FName("Ability.Updates.Processed"));
        }

        ServiceMetrics.RecordSuccess();
    }
    else
    {
        UE_LOG(LogSuspenseCoreEquipmentAbility, Warning,
            TEXT("No connector found for equipment %s"),
            *GetNameSafe(EquipmentActor));
        ServiceMetrics.RecordError();
    }
}

int32 USuspenseCoreEquipmentAbilityService::CleanupInvalidConnectors()
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
        USuspenseCoreEquipmentAbilityConnector** ConnectorPtr = EquipmentConnectors.Find(Equipment);
        if (ConnectorPtr)
        {
            USuspenseCoreEquipmentAbilityConnector* Connector = *ConnectorPtr;
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
        UE_LOG(LogSuspenseCoreEquipmentAbility, Log,
            TEXT("Cleaned up %d invalid equipment connectors"),
            CleanedCount);
        ServiceMetrics.RecordValue(FName("Ability.Connectors.Cleaned"), CleanedCount);
    }

    return CleanedCount;
}

//========================================
// Protected Methods
//========================================

void USuspenseCoreEquipmentAbilityService::InitializeDefaultMappings()
{
    // Try to load default mapping table
    if (DefaultMappingTable.IsNull())
    {
        UE_LOG(LogSuspenseCoreEquipmentAbility, Log, TEXT("No default mapping table configured"));
        return;
    }

#if !(UE_BUILD_SHIPPING)
    // Synchronous load for development builds
    if (UDataTable* DefaultTable = DefaultMappingTable.LoadSynchronous())
    {
        int32 Loaded = LoadAbilityMappings(DefaultTable);
        UE_LOG(LogSuspenseCoreEquipmentAbility, Log,
            TEXT("Loaded %d default ability mappings"),
            Loaded);
    }
    else
    {
        UE_LOG(LogSuspenseCoreEquipmentAbility, Warning,
            TEXT("Failed to load default mapping table from %s"),
            *DefaultMappingTable.ToString());
    }
#else
    // Async load for shipping builds
    TWeakObjectPtr<USuspenseCoreEquipmentAbilityService> WeakThis(this);
    StreamableManager.RequestAsyncLoad(
        DefaultMappingTable.ToSoftObjectPath(),
        [WeakThis]()
        {
            if (USuspenseCoreEquipmentAbilityService* StrongThis = WeakThis.Get())
            {
                if (UDataTable* DefaultTable = StrongThis->DefaultMappingTable.Get())
                {
                    int32 Loaded = StrongThis->LoadAbilityMappings(DefaultTable);
                    UE_LOG(LogSuspenseCoreEquipmentAbility, Log,
                        TEXT("Async loaded %d default ability mappings"),
                        Loaded);
                }
            }
        }
    );
#endif
}

void USuspenseCoreEquipmentAbilityService::SetupEventHandlers()
{
    using namespace SuspenseCoreEquipmentTags;

    UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] ═══════════════════════════════════════════════════════"));
    UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] AbilityService::SetupEventHandlers CALLED"));

    // Get EventBus from ServiceProvider (SuspenseCore architecture)
    if (USuspenseCoreServiceProvider* Provider = USuspenseCoreServiceProvider::Get(this))
    {
        EventBus = Provider->GetEventBus();
        UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG]   ServiceProvider found, EventBus=%s"),
            EventBus ? TEXT("VALID") : TEXT("NULL"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[ADS DEBUG]   ServiceProvider NOT FOUND!"));
    }

    if (!EventBus)
    {
        UE_LOG(LogTemp, Error, TEXT("[ADS DEBUG]   EventBus is NULL - event handling DISABLED!"));
        UE_LOG(LogSuspenseCoreEquipmentAbility, Warning,
            TEXT("EventBus not available from ServiceProvider, event handling disabled"));
        return;
    }

    // Spawned - using SuspenseCore.Event.Equipment.Visual.Spawned tag
    const FGameplayTag SpawnedTag = Event::TAG_Equipment_Event_Visual_Spawned;
    UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG]   Subscribing to Visual.Spawned: %s"),
        *SpawnedTag.ToString());

    EventSubscriptions.Add(EventBus->SubscribeNative(
        Event::TAG_Equipment_Event_Visual_Spawned,
        this,
        FSuspenseCoreNativeEventCallback::CreateUObject(
            this, &USuspenseCoreEquipmentAbilityService::OnEquipmentSpawned),
        ESuspenseCoreEventPriority::High
    ));

    // Destroyed - using SuspenseCore.Event.Equipment.Visual.Detached tag
    EventSubscriptions.Add(EventBus->SubscribeNative(
        Event::TAG_Equipment_Event_Visual_Detached,
        this,
        FSuspenseCoreNativeEventCallback::CreateUObject(
            this, &USuspenseCoreEquipmentAbilityService::OnEquipmentDestroyed),
        ESuspenseCoreEventPriority::High
    ));

    // Equipped event (SuspenseCore.Event.Equipment.Equipped)
    if (Tag_OnEquipped.IsValid())
    {
        EventSubscriptions.Add(EventBus->SubscribeNative(
            Tag_OnEquipped,
            this,
            FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreEquipmentAbilityService::OnEquipped),
            ESuspenseCoreEventPriority::High));
    }

    // Unequipped event (SuspenseCore.Event.Equipment.Unequipped)
    if (Tag_OnUnequipped.IsValid())
    {
        EventSubscriptions.Add(EventBus->SubscribeNative(
            Tag_OnUnequipped,
            this,
            FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreEquipmentAbilityService::OnUnequipped),
            ESuspenseCoreEventPriority::High));
    }

    // Abilities refresh event (SuspenseCore.Event.Equipment.Ability.Refresh)
    if (Tag_OnAbilitiesRefresh.IsValid())
    {
        EventSubscriptions.Add(EventBus->SubscribeNative(
            Tag_OnAbilitiesRefresh,
            this,
            FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreEquipmentAbilityService::OnAbilitiesRefresh),
            ESuspenseCoreEventPriority::Normal));
    }

    // Commit event (SuspenseCore.Event.Equipment.Commit)
    if (Tag_OnCommit.IsValid())
    {
        EventSubscriptions.Add(EventBus->SubscribeNative(
            Tag_OnCommit,
            this,
            FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreEquipmentAbilityService::OnCommit),
            ESuspenseCoreEventPriority::Normal));
    }

    UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG]   ✓ Total subscriptions registered: %d"), EventSubscriptions.Num());
    UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] ═══════════════════════════════════════════════════════"));

    UE_LOG(LogSuspenseCoreEquipmentAbility, Log,
        TEXT("Event handlers registered: %d subscriptions"), EventSubscriptions.Num());
}

void USuspenseCoreEquipmentAbilityService::EnsureValidConfig()
{
    // Sanitize cache TTL
    MappingCacheTTL = FMath::Clamp(MappingCacheTTL, 60.0f, 3600.0f);

    // Sanitize cleanup interval
    CleanupInterval = FMath::Clamp(CleanupInterval, 10.0f, 300.0f);

    UE_LOG(LogSuspenseCoreEquipmentAbility, Log,
        TEXT("Configuration sanitized: CacheTTL=%.1fs, CleanupInterval=%.1fs"),
        MappingCacheTTL, CleanupInterval);
}

void USuspenseCoreEquipmentAbilityService::OnEquipmentSpawned(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
    UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] ═══════════════════════════════════════════════════════"));
    UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] AbilityService::OnEquipmentSpawned RECEIVED! Tag=%s"), *EventTag.ToString());

    // Parse structured event data from SuspenseCore event
    FSuspenseCoreInventoryItemInstance ItemInstance;
    AActor* EquipmentActor = nullptr;
    AActor* OwnerActor = nullptr;

    if (!ParseSuspenseCoreEventData(EventData, ItemInstance, EquipmentActor, OwnerActor))
    {
        UE_LOG(LogTemp, Error, TEXT("[ADS DEBUG] AbilityService: ParseSuspenseCoreEventData FAILED!"));
        UE_LOG(LogSuspenseCoreEquipmentAbility, Warning,
            TEXT("Failed to parse equipment spawned event [%s]"), *EventTag.ToString());
        ServiceMetrics.Inc(FName("Ability.Events.ParseFailed"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] AbilityService: Parsed OK! Equipment=%s, Owner=%s, ItemID=%s"),
        EquipmentActor ? *EquipmentActor->GetName() : TEXT("NULL"),
        OwnerActor ? *OwnerActor->GetName() : TEXT("NULL"),
        *ItemInstance.ItemID.ToString());

    ProcessEquipmentSpawn(EquipmentActor, OwnerActor, ItemInstance);
    ServiceMetrics.Inc(FName("Ability.Events.Spawned"));
}

void USuspenseCoreEquipmentAbilityService::OnEquipmentDestroyed(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
    AActor* EquipmentActor = Cast<AActor>(EventData.GetObject<UObject>(FName("Source")));
    if (!EquipmentActor)
    {
        ServiceMetrics.Inc(FName("Ability.Events.InvalidSource"));
        return;
    }

    ProcessEquipmentDestroy(EquipmentActor);
    ServiceMetrics.Inc(FName("Ability.Events.Destroyed"));
}

// === S7 Handlers (SuspenseCore Event format) ===

void USuspenseCoreEquipmentAbilityService::OnEquipped(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
    FSuspenseCoreInventoryItemInstance ItemInstance;
    AActor* EquipmentActor = nullptr;
    AActor* OwnerActor = nullptr;
    if (!ParseSuspenseCoreEventData(EventData, ItemInstance, EquipmentActor, OwnerActor))
    {
        UE_LOG(LogSuspenseCoreEquipmentAbility, Warning, TEXT("OnEquipped [%s]: parse failed"), *EventTag.ToString());
        ServiceMetrics.Inc(FName("Ability.Events.ParseFailed"));
        return;
    }
    ProcessEquipmentSpawn(EquipmentActor, OwnerActor, ItemInstance);
    ServiceMetrics.Inc(FName("Ability.Events.Equipped"));
}

void USuspenseCoreEquipmentAbilityService::OnUnequipped(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
    AActor* EquipmentActor = Cast<AActor>(EventData.GetObject<UObject>(FName("Source")));
    if (!EquipmentActor)
    {
        // Try full parse (in case source differs)
        FSuspenseCoreInventoryItemInstance Item;
        AActor* Owner = nullptr;
        if (!ParseSuspenseCoreEventData(EventData, Item, EquipmentActor, Owner) || !EquipmentActor)
        {
            UE_LOG(LogSuspenseCoreEquipmentAbility, Warning, TEXT("OnUnequipped [%s]: invalid source"), *EventTag.ToString());
            ServiceMetrics.Inc(FName("Ability.Events.InvalidSource"));
            return;
        }
    }
    ProcessEquipmentDestroy(EquipmentActor);
    ServiceMetrics.Inc(FName("Ability.Events.Unequipped"));
}

void USuspenseCoreEquipmentAbilityService::OnAbilitiesRefresh(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
    FSuspenseCoreInventoryItemInstance ItemInstance;
    AActor* EquipmentActor = nullptr;
    AActor* OwnerActor = nullptr;
    if (!ParseSuspenseCoreEventData(EventData, ItemInstance, EquipmentActor, OwnerActor))
    {
        UE_LOG(LogSuspenseCoreEquipmentAbility, Warning, TEXT("OnAbilitiesRefresh [%s]: parse failed"), *EventTag.ToString());
        ServiceMetrics.Inc(FName("Ability.Events.ParseFailed"));
        return;
    }
    UpdateEquipmentAbilities(EquipmentActor, ItemInstance);
    ServiceMetrics.Inc(FName("Ability.Events.Refresh"));
}

void USuspenseCoreEquipmentAbilityService::OnCommit(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
    FSuspenseCoreInventoryItemInstance ItemInstance;
    AActor* EquipmentActor = nullptr;
    AActor* OwnerActor = nullptr;
    if (!ParseSuspenseCoreEventData(EventData, ItemInstance, EquipmentActor, OwnerActor))
    {
        UE_LOG(LogSuspenseCoreEquipmentAbility, Warning, TEXT("OnCommit [%s]: parse failed"), *EventTag.ToString());
        ServiceMetrics.Inc(FName("Ability.Events.ParseFailed"));
        return;
    }
    UpdateEquipmentAbilities(EquipmentActor, ItemInstance);
    ServiceMetrics.Inc(FName("Ability.Events.Commit"));
}

void USuspenseCoreEquipmentAbilityService::OnEquipmentActorDestroyed(AActor* DestroyedActor)
{
    // Idempotent - RemoveConnectorForEquipment handles if connector doesn't exist
    RemoveConnectorForEquipment(DestroyedActor);
}

void USuspenseCoreEquipmentAbilityService::OnCleanupTimer()
{
    int32 Cleaned = CleanupInvalidConnectors();
    if (Cleaned > 0 && bEnableDetailedLogging)
    {
        UE_LOG(LogSuspenseCoreEquipmentAbility, Verbose,
            TEXT("Periodic cleanup removed %d invalid connectors"),
            Cleaned);
    }
}

USuspenseCoreEquipmentAbilityConnector* USuspenseCoreEquipmentAbilityService::CreateConnectorForEquipment(
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
        UE_LOG(LogSuspenseCoreEquipmentAbility, Warning,
            TEXT("No AbilitySystemComponent found on owner %s (checked: component, interface, controller, playerstate)"),
            *GetNameSafe(OwnerActor));
        return nullptr;
    }

    // Create connector component on the EQUIPMENT actor
    USuspenseCoreEquipmentAbilityConnector* Connector = NewObject<USuspenseCoreEquipmentAbilityConnector>(
        EquipmentActor,
        USuspenseCoreEquipmentAbilityConnector::StaticClass(),
        TEXT("EquipmentAbilityConnector"),
        RF_Transient
    );

    if (!Connector)
    {
        UE_LOG(LogSuspenseCoreEquipmentAbility, Error,
            TEXT("Failed to create ability connector"));
        return nullptr;
    }

    // Register component with equipment actor
    Connector->RegisterComponent();

    // Initialize connector with owner's ASC
    TScriptInterface<ISuspenseCoreEquipmentDataProvider> DataProvider;
    if (!Connector->Initialize(ASC, DataProvider))
    {
        UE_LOG(LogSuspenseCoreEquipmentAbility, Error,
            TEXT("Failed to initialize ability connector"));
        Connector->DestroyComponent();
        return nullptr;
    }

    return Connector;
}

UAbilitySystemComponent* USuspenseCoreEquipmentAbilityService::FindOwnerAbilitySystemComponent(AActor* OwnerActor) const
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

FGameplayTagContainer USuspenseCoreEquipmentAbilityService::GetEquipmentTags(AActor* EquipmentActor) const
{
    FGameplayTagContainer Tags;

    if (!EquipmentActor || !IsValid(EquipmentActor))
    {
        UE_LOG(LogSuspenseCoreEquipmentAbility, Warning, TEXT("GetEquipmentTags called with invalid equipment"));
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

bool USuspenseCoreEquipmentAbilityService::ParseSuspenseCoreEventData(
    const FSuspenseCoreEventData& EventData,
    FSuspenseCoreInventoryItemInstance& OutItem,
    AActor*& OutEquipmentActor,
    AActor*& OutOwnerActor) const
{
    // Equipment actor should be in Source (using FSuspenseCoreEventData API)
    OutEquipmentActor = Cast<AActor>(EventData.GetObject<UObject>(FName("Source")));
    if (!OutEquipmentActor)
    {
        return false;
    }

    // Owner should be in Target
    OutOwnerActor = Cast<AActor>(EventData.GetObject<UObject>(FName("Target")));
    if (!OutOwnerActor)
    {
        return false;
    }

    // Parse item data from FSuspenseCoreEventData (uses TMap<FName, FString> for strings)
    FString ItemIDStr = EventData.GetString(FName("ItemID"));
    if (!ItemIDStr.IsEmpty())
    {
        OutItem.ItemID = *ItemIDStr;

        FString InstanceIDStr = EventData.GetString(FName("InstanceID"));
        if (!InstanceIDStr.IsEmpty())
        {
            FGuid::Parse(InstanceIDStr, OutItem.InstanceID);
        }

        // Quantity can be stored as int or string
        int32 Quantity = EventData.GetInt(FName("Quantity"));
        if (Quantity > 0)
        {
            OutItem.Quantity = Quantity;
        }
        else
        {
            FString QuantityStr = EventData.GetString(FName("Quantity"));
            if (!QuantityStr.IsEmpty())
            {
                OutItem.Quantity = FCString::Atoi(*QuantityStr);
            }
        }

        // Try to get anchor index
        int32 AnchorIndex = EventData.GetInt(FName("AnchorIndex"));
        if (AnchorIndex != 0) // 0 is default, check if explicitly set
        {
            OutItem.AnchorIndex = AnchorIndex;
        }

        return OutItem.IsValid();
    }

    // Fallback: Try JSON parsing from Payload string field
    FString PayloadStr = EventData.GetString(FName("Payload"));
    if (!PayloadStr.IsEmpty())
    {
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PayloadStr);

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
