// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Components/Presentation/SuspenseCoreEquipmentActorFactory.h"
#include "SuspenseCore/ItemSystem/SuspenseCoreItemManager.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceMacros.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Base/SuspenseCoreWeaponActor.h"
#include "SuspenseCore/Base/SuspenseCoreEquipmentActor.h"
#include "SuspenseCore/Components/SuspenseCoreEquipmentAttributeComponent.h"
#include "SuspenseCore/Components/SuspenseCoreEquipmentAttachmentComponent.h"
#include "SuspenseCore/Components/SuspenseCoreWeaponAmmoComponent.h"
#include "SuspenseCore/Components/SuspenseCoreWeaponFireModeComponent.h"
#include "SuspenseCore/Components/SuspenseCoreEquipmentMeshComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceLocator.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipment.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeapon.h"
#include "SuspenseCore/Types/Weapon/SuspenseCoreInventoryAmmoState.h"
#include "UObject/SoftObjectPath.h"

// ===== helper (поместите в .cpp файла фабрики, над методами) =====
static USuspenseCoreItemManager* GetItemManagerSafe(const UWorld* World)
{
    if (!World) return nullptr;
    if (const UGameInstance* GI = World->GetGameInstance())
    {
        return GI->GetSubsystem<USuspenseCoreItemManager>();
    }
    return nullptr;
}
//========================================
// Constructor & Lifecycle
//========================================

USuspenseCoreEquipmentActorFactory::USuspenseCoreEquipmentActorFactory()
    : ActorClassCache(100) // правильная инициализация кеша (без operator=)
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f;
}

void USuspenseCoreEquipmentActorFactory::BeginPlay()
{
    Super::BeginPlay();

    // Initialize EventBus integration
    SetupEventBus();

    USuspenseCoreEquipmentServiceLocator* Locator = USuspenseCoreEquipmentServiceLocator::Get(this);
    if (Locator)
    {
        const FGameplayTag FactoryTag = FGameplayTag::RequestGameplayTag(TEXT("Service.ActorFactory"));

        if (!Locator->IsServiceRegistered(FactoryTag))
        {
            Locator->RegisterServiceInstance(FactoryTag, this);

            UE_LOG(LogSuspenseCoreEquipmentOperation, Log,
                TEXT("✓ ActorFactory registered as service: Service.ActorFactory"));
        }
        else
        {
            UE_LOG(LogSuspenseCoreEquipmentOperation, Warning,
                TEXT("ActorFactory already registered in ServiceLocator"));
        }
    }
    else
    {
        UE_LOG(LogSuspenseCoreEquipmentOperation, Error,
            TEXT("❌ Failed to get ServiceLocator - ActorFactory NOT registered!"));
        UE_LOG(LogSuspenseCoreEquipmentOperation, Error,
            TEXT("   VisualizationService will use fallback spawn and actors won't be pooled"));
    }

    // Остальной код BeginPlay...
    if (FactoryConfig.PoolCleanupInterval > 0.0f)
    {
        GetWorld()->GetTimerManager().SetTimer(
            PoolCleanupTimerHandle,
            this,
            &USuspenseCoreEquipmentActorFactory::CleanupPool,
            FactoryConfig.PoolCleanupInterval,
            true
        );
    }

    if (FactoryConfig.bEnableAsyncLoading && FactoryConfig.PriorityPreloadItems.Num() > 0)
    {
        PreloadItemClasses(FactoryConfig.PriorityPreloadItems);
    }

    LogFactoryOperation(TEXT("BeginPlay"), TEXT("Factory initialized"));
}

void USuspenseCoreEquipmentActorFactory::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    USuspenseCoreEquipmentServiceLocator* Locator = USuspenseCoreEquipmentServiceLocator::Get(this);
    if (Locator)
    {
        const FGameplayTag FactoryTag = FGameplayTag::RequestGameplayTag(TEXT("Service.ActorFactory"));

        if (Locator->IsServiceRegistered(FactoryTag))
        {
            Locator->UnregisterService(FactoryTag, /*bForceShutdown=*/false);

            UE_LOG(LogSuspenseCoreEquipmentOperation, Log,
                TEXT("ActorFactory unregistered from ServiceLocator"));
        }
    }

    // Остальной код EndPlay...
    ClearAllActors(true);

    if (PoolCleanupTimerHandle.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(PoolCleanupTimerHandle);
    }

    for (auto& LoadHandle : LoadingHandles)
    {
        if (LoadHandle.Value.IsValid())
        {
            LoadHandle.Value->CancelHandle();
        }
    }
    LoadingHandles.Empty();

    Super::EndPlay(EndPlayReason);
}
void USuspenseCoreEquipmentActorFactory::TickComponent(
    float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Periodic maintenance placeholder:
    // FEquipmentCacheManager не имеет явного API для «очистки просроченных».
    // Записи с TTL удаляются лениво при Get(), а мы кладем классы без TTL,
    // так что здесь ничего чистить не нужно.
    static float CacheMaintenanceAccumulator = 0.0f;
    CacheMaintenanceAccumulator += DeltaTime;
    if (CacheMaintenanceAccumulator > 10.0f)
    {
        // Ничего не делаем: оставлено на случай будущих хуков обслуживания кэша
        CacheMaintenanceAccumulator = 0.0f;
    }
}

//========================================
/* ISuspenseCoreActorFactory Implementation */
//========================================

// SuspenseCoreEquipmentActorFactory - SpawnEquipmentActor Implementation

FEquipmentActorSpawnResult USuspenseCoreEquipmentActorFactory::SpawnEquipmentActor(const FEquipmentActorSpawnParams& Params)
{
    EQUIPMENT_SCOPE_LOCK(PoolLock);

    FEquipmentActorSpawnResult Result;

    // ============================================================================
    // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ #1: Загружаем полные данные предмета из DataTable
    // ============================================================================
    USuspenseCoreItemManager* ItemManager = GetItemManagerSafe(GetWorld());
    if (!ItemManager)
    {
        Result.bSuccess = false;
        Result.ErrorMessage = FText::FromString(TEXT("ItemManager subsystem not available"));
        UE_LOG(LogSuspenseCoreEquipmentOperation, Error,
            TEXT("[SpawnEquipmentActor] ItemManager not found - cannot load item data"));
        return Result;
    }

    // Load full item data from DataTable - single source of truth
    FSuspenseCoreUnifiedItemData ItemData;
    if (!ItemManager->GetUnifiedItemData(Params.ItemInstance.ItemID, ItemData))
    {
        Result.bSuccess = false;
        Result.ErrorMessage = FText::FromString(
            FString::Printf(TEXT("Item data not found for ItemID: %s"),
            *Params.ItemInstance.ItemID.ToString()));
        UE_LOG(LogSuspenseCoreEquipmentOperation, Error,
            TEXT("[SpawnEquipmentActor] Failed to load item data for ItemID: %s"),
            *Params.ItemInstance.ItemID.ToString());
        return Result;
    }

    UE_LOG(LogSuspenseCoreEquipmentOperation, Log,
        TEXT("[SpawnEquipmentActor] Loaded ItemData for %s: Type=%s, IsWeapon=%d, IsEquippable=%d"),
        *Params.ItemInstance.ItemID.ToString(),
        *ItemData.ItemType.ToString(),
        ItemData.bIsWeapon,
        ItemData.bIsEquippable);

    // ============================================================================
    // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ #2: Инициализируем RuntimeProperties для оружия
    // ============================================================================
    // Create enriched instance with initialized runtime properties
    FSuspenseCoreInventoryItemInstance EnrichedInstance = Params.ItemInstance;

    // Ensure quantity is valid
    if (EnrichedInstance.Quantity <= 0)
    {
        EnrichedInstance.Quantity = 1;
    }

    // For weapons: initialize ammo state if not already set
    if (ItemData.bIsWeapon)
    {
        // Only initialize if properties are not already set (preserve existing state)
        if (!EnrichedInstance.RuntimeProperties.Contains(TEXT("CurrentAmmo")))
        {
            // Default starting ammo - will be overridden by AttributeSet initialization
            // These keys match ConfigureEquipmentActor expectations
            EnrichedInstance.SetRuntimeProperty(TEXT("CurrentAmmo"), 30.0f);

            UE_LOG(LogSuspenseCoreEquipmentOperation, Verbose,
                TEXT("[SpawnEquipmentActor] Initialized CurrentAmmo=30 for weapon %s"),
                *ItemData.ItemID.ToString());
        }

        if (!EnrichedInstance.RuntimeProperties.Contains(TEXT("RemainingAmmo")))
        {
            EnrichedInstance.SetRuntimeProperty(TEXT("RemainingAmmo"), 90.0f);

            UE_LOG(LogSuspenseCoreEquipmentOperation, Verbose,
                TEXT("[SpawnEquipmentActor] Initialized RemainingAmmo=90 for weapon %s"),
                *ItemData.ItemID.ToString());
        }
    }

    // ============================================================================
    // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ #3: Используем ActorClass из DataTable
    // ============================================================================
    // Resolve actor class from ItemData (not from cache directly)
    TSubclassOf<AActor> ActorClass = nullptr;

    if (!ItemData.EquipmentActorClass.IsNull())
    {
        // Prefer already loaded class
        if (ItemData.EquipmentActorClass.IsValid())
        {
            ActorClass = ItemData.EquipmentActorClass.Get();

            UE_LOG(LogSuspenseCoreEquipmentOperation, Verbose,
                TEXT("[SpawnEquipmentActor] Using already loaded ActorClass: %s"),
                *ActorClass->GetName());
        }
        else
        {
            // Synchronous load if needed
            ActorClass = ItemData.EquipmentActorClass.LoadSynchronous();

            if (ActorClass)
            {
                UE_LOG(LogSuspenseCoreEquipmentOperation, Log,
                    TEXT("[SpawnEquipmentActor] Loaded ActorClass synchronously: %s"),
                    *ActorClass->GetName());
            }
        }
    }

    if (!ActorClass)
    {
        // Fallback to cache-based resolution (last resort)
        UE_LOG(LogSuspenseCoreEquipmentOperation, Warning,
            TEXT("[SpawnEquipmentActor] EquipmentActorClass is null in DataTable, trying cache fallback"));

        ActorClass = GetActorClassForItem(Params.ItemInstance.ItemID);
    }

    if (!ActorClass)
    {
        Result.bSuccess = false;
        Result.ErrorMessage = FText::FromString(TEXT("Actor class not found"));
        UE_LOG(LogSuspenseCoreEquipmentOperation, Error,
            TEXT("[SpawnEquipmentActor] No valid ActorClass for ItemID: %s - DataTable EquipmentActorClass is null or invalid"),
            *Params.ItemInstance.ItemID.ToString());
        return Result;
    }

    UE_LOG(LogSuspenseCoreEquipmentOperation, Log,
        TEXT("[SpawnEquipmentActor] Resolved ActorClass: %s for ItemID: %s"),
        *ActorClass->GetName(),
        *Params.ItemInstance.ItemID.ToString());

    // ============================================================================
    // Actor spawning - check pool first, then spawn new
    // ============================================================================
    AActor* SpawnedActor = GetPooledActor(ActorClass);
    if (!SpawnedActor)
    {
        SpawnedActor = SpawnActorInternal(ActorClass, Params.SpawnTransform, Params.Owner.Get());
        if (!SpawnedActor)
        {
            Result.bSuccess = false;
            Result.ErrorMessage = FText::FromString(TEXT("Failed to spawn actor"));
            UE_LOG(LogSuspenseCoreEquipmentOperation, Error,
                TEXT("[SpawnEquipmentActor] SpawnActorInternal failed for class: %s"),
                *ActorClass->GetName());
            return Result;
        }

        UE_LOG(LogSuspenseCoreEquipmentOperation, Log,
            TEXT("[SpawnEquipmentActor] ✓ Spawned new actor: %s"),
            *SpawnedActor->GetName());
    }
    else
    {
        // Reset pooled actor state
        SpawnedActor->SetActorTransform(Params.SpawnTransform);
        SpawnedActor->SetActorHiddenInGame(false);
        SpawnedActor->SetActorEnableCollision(true);
        SpawnedActor->SetActorTickEnabled(true);

        UE_LOG(LogSuspenseCoreEquipmentOperation, Log,
            TEXT("[SpawnEquipmentActor] ✓ Reused pooled actor: %s"),
            *SpawnedActor->GetName());
    }

    // ============================================================================
    // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ #4: Передаём обогащённый ItemInstance
    // ============================================================================
    // Configure actor with enriched item data (contains initialized RuntimeProperties)
    // Actor will internally query ItemManager for full DataTable data via ItemID
    if (!ConfigureEquipmentActor(SpawnedActor, EnrichedInstance))
    {
        UE_LOG(LogSuspenseCoreEquipmentOperation, Error,
            TEXT("[SpawnEquipmentActor] ConfigureEquipmentActor failed for actor: %s"),
            *SpawnedActor->GetName());
        DestroyEquipmentActor(SpawnedActor, true);
        Result.bSuccess = false;
        Result.ErrorMessage = FText::FromString(TEXT("Failed to configure actor"));
        return Result;
    }

    UE_LOG(LogSuspenseCoreEquipmentOperation, Log,
        TEXT("[SpawnEquipmentActor] ✓ Successfully configured actor: %s"),
        *SpawnedActor->GetName());

    // ============================================================================
    // Register spawned actor in slot registry
    // ============================================================================
    int32 SlotIndex = Params.SlotIndex;

    if (SlotIndex != INDEX_NONE)
    {
        RegisterSpawnedActor(SpawnedActor, SlotIndex);

        UE_LOG(LogSuspenseCoreEquipmentOperation, Verbose,
            TEXT("[SpawnEquipmentActor] ✓ Registered actor in slot: %d"), SlotIndex);
    }

    // ============================================================================
    // Success - return result
    // ============================================================================
    Result.bSuccess = true;
    Result.SpawnedActor = SpawnedActor;

    // Broadcast event via EventBus for inter-component communication
    BroadcastActorSpawned(SpawnedActor, EnrichedInstance.ItemID, SlotIndex);

    UE_LOG(LogSuspenseCoreEquipmentOperation, Log,
        TEXT("[SpawnEquipmentActor] ✓✓✓ SUCCESS ✓✓✓"));
    UE_LOG(LogSuspenseCoreEquipmentOperation, Log,
        TEXT("  Spawned: %s"), *SpawnedActor->GetName());
    UE_LOG(LogSuspenseCoreEquipmentOperation, Log,
        TEXT("  ItemID: %s"), *EnrichedInstance.ItemID.ToString());
    UE_LOG(LogSuspenseCoreEquipmentOperation, Log,
        TEXT("  InstanceID: %s"), *EnrichedInstance.InstanceID.ToString());
    UE_LOG(LogSuspenseCoreEquipmentOperation, Log,
        TEXT("  RuntimeProperties: %d"), EnrichedInstance.RuntimeProperties.Num());

    return Result;
}
bool USuspenseCoreEquipmentActorFactory::DestroyEquipmentActor(AActor* Actor, bool bImmediate)
{
    if (!IsActorValid(Actor))
    {
        return false;
    }

    // Get ItemId from pool entry before unregistering (for EventBus broadcast)
    FName ItemId = NAME_None;
    if (const FSuspenseCoreActorPoolEntry* Entry = FindPoolEntry(Actor))
    {
        ItemId = Entry->ItemId;
    }

    // Broadcast event via EventBus before destroying
    BroadcastActorDestroyed(Actor, ItemId);

    // Unregister from registry
    UnregisterActor(Actor);

    // Try to recycle to pool
    if (!bImmediate && RecycleActor(Actor))
    {
        return true;
    }

    // Destroy actor
    DestroyActorInternal(Actor, bImmediate);

    LogFactoryOperation(TEXT("DestroyEquipmentActor"),
        FString::Printf(TEXT("Destroyed %s"), *Actor->GetName()));

    return true;
}

bool USuspenseCoreEquipmentActorFactory::ConfigureEquipmentActor(AActor* Actor, const FSuspenseCoreInventoryItemInstance& ItemInstance)
{
    if (!IsActorValid(Actor) || !ItemInstance.IsValid())
    {
        return false;
    }

    // 1) Всегда сначала пробуем интерфейс экипировки
    if (Actor->GetClass()->ImplementsInterface(USuspenseCoreEquipment::StaticClass()))
    {
        ISuspenseCoreEquipment::Execute_OnItemInstanceEquipped(Actor, ItemInstance);
    }
    else if (ASuspenseCoreEquipmentActor* EquipmentActor = Cast<ASuspenseCoreEquipmentActor>(Actor))
    {
        // Fallback для редких случаев (но по нашей архитектуре базовый актор реализует интерфейс)
        EquipmentActor->OnItemInstanceEquipped_Implementation(ItemInstance);
    }

    // 2) Если актор поддерживает оружейный интерфейс — пробрасываем стартовый AmmoState ЧЕРЕЗ ИНТЕРФЕЙС
    if (Actor->GetClass()->ImplementsInterface(USuspenseCoreWeapon::StaticClass()))
    {
        FSuspenseCoreInventoryAmmoState AmmoState{};
        bool bHasAnyAmmo = false;

        // Ключи должны совпадать с тем, как оружие сохраняет состояние (см. WeaponActor::SaveWeaponState)
        // CurrentAmmo
        if (ItemInstance.RuntimeProperties.Contains(TEXT("CurrentAmmo")))
        {
            AmmoState.CurrentAmmo = ItemInstance.GetRuntimeProperty(TEXT("CurrentAmmo"));
            bHasAnyAmmo = true;
        }
        // RemainingAmmo
        if (ItemInstance.RuntimeProperties.Contains(TEXT("RemainingAmmo")))
        {
            AmmoState.RemainingAmmo = ItemInstance.GetRuntimeProperty(TEXT("RemainingAmmo"));
            bHasAnyAmmo = true;
        }

        if (bHasAnyAmmo)
        {
            ISuspenseCoreWeapon::Execute_SetAmmoState(Actor, AmmoState);
        }
    }

    return true;
}

bool USuspenseCoreEquipmentActorFactory::RecycleActor(AActor* Actor)
{
    if (!IsActorValid(Actor))
    {
        return false;
    }

    EQUIPMENT_SCOPE_LOCK(PoolLock);

    // Find existing pool entry
    FSuspenseCoreActorPoolEntry* Entry = FindPoolEntry(Actor);
    if (Entry)
    {
        // Already in pool → просто обновляем таймстамп/флаги
        Entry->bInUse = false;
        Entry->LastUsedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

        // Hide and disable
        Actor->SetActorHiddenInGame(true);
        Actor->SetActorEnableCollision(false);
        Actor->SetActorTickEnabled(false);

        return true;
    }

    // Check pool size limit per class
    int32 ClassCount = 0;
    for (const FSuspenseCoreActorPoolEntry& PoolEntry : ActorPool)
    {
        if (PoolEntry.ActorClass == Actor->GetClass())
        {
            ClassCount++;
        }
    }

    if (ClassCount >= FactoryConfig.MaxPoolSizePerClass)
    {
        return false;
    }

    // Add to pool
    FSuspenseCoreActorPoolEntry NewEntry;
    NewEntry.Actor = Actor;
    NewEntry.ActorClass = Actor->GetClass();
    NewEntry.bInUse = false;
    NewEntry.LastUsedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    ActorPool.Add(NewEntry);

    // Hide and disable
    Actor->SetActorHiddenInGame(true);
    Actor->SetActorEnableCollision(false);
    Actor->SetActorTickEnabled(false);

    return true;
}

AActor* USuspenseCoreEquipmentActorFactory::GetPooledActor(TSubclassOf<AActor> ActorClass)
{
    if (!ActorClass)
    {
        return nullptr;
    }

    EQUIPMENT_SCOPE_LOCK(PoolLock);

    FSuspenseCoreActorPoolEntry* Entry = FindAvailablePoolEntry(ActorClass);
    if (Entry)
    {
        Entry->bInUse = true;
        Entry->LastUsedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

        // Reset actor state
        if (IsActorValid(Entry->Actor))
        {
            Entry->Actor->SetActorHiddenInGame(false);
            Entry->Actor->SetActorEnableCollision(true);
            Entry->Actor->SetActorTickEnabled(true);
        }

        return Entry->Actor;
    }

    return nullptr;
}

bool USuspenseCoreEquipmentActorFactory::PreloadActorClass(const FName& ItemId)
{
    if (!FactoryConfig.bEnableAsyncLoading)
    {
        // синхронный путь: просто прогреем класс (менеджер загрузит и положит в кеш)
        return GetActorClassForItem(ItemId) != nullptr;
    }

    // cache hit?
    {
        TSubclassOf<AActor> CachedClass = nullptr;
        if (ActorClassCache.Get(ItemId, CachedClass) && CachedClass)
        {
            return true;
        }
    }

    // already loading?
    if (LoadingHandles.Contains(ItemId))
    {
        return true;
    }

    // берем менеджер как сабсистему GI (без статического GetItemManager)
    if (USuspenseCoreItemManager* ItemManager = GetItemManagerSafe(GetWorld()))
    {
        FSuspenseCoreUnifiedItemData ItemData;
        if (ItemManager->GetUnifiedItemData(ItemId, ItemData) && !ItemData.EquipmentActorClass.IsNull())
        {
            const FSoftObjectPath Path = ItemData.EquipmentActorClass.ToSoftObjectPath();

            // уже загружен
            if (ItemData.EquipmentActorClass.IsValid())
            {
                TSubclassOf<AActor> ActorClass = ItemData.EquipmentActorClass.Get();
                if (ActorClass)
                {
                    ActorClassCache.Set(ItemId, ActorClass);
                    return true;
                }
                return false;
            }

            // запускаем асинхронную загрузку
            TSharedPtr<FStreamableHandle> Handle = StreamableManager.RequestAsyncLoad(
                Path,
                FStreamableDelegate::CreateUObject(this, &USuspenseCoreEquipmentActorFactory::OnAsyncLoadComplete, ItemId)
            );

            if (Handle.IsValid())
            {
                LoadingHandles.Add(ItemId, Handle);
                return true;
            }
        }
    }

    return false;
}

FTransform USuspenseCoreEquipmentActorFactory::GetSpawnTransformForSlot(int32 SlotIndex, AActor* Owner) const
{
    // Default spawn at owner location
    if (IsValid(Owner))
    {
        return Owner->GetActorTransform();
    }

    return FTransform::Identity;
}

bool USuspenseCoreEquipmentActorFactory::RegisterSpawnedActor(AActor* Actor, int32 SlotIndex)
{
    if (!IsActorValid(Actor) || SlotIndex == INDEX_NONE)
    {
        return false;
    }

    // чтобы не зациклиться на повторном захвате RegistryLock внутри Destroy/Unregister —
    // переназначаем под локом и уничтожаем прежний актор уже после выхода из лока.
    AActor* OldActor = nullptr;
    {
        EQUIPMENT_SCOPE_LOCK(RegistryLock);

        if (AActor** ExistingActor = SpawnedActorRegistry.Find(SlotIndex))
        {
            OldActor = *ExistingActor;
        }
        SpawnedActorRegistry.Add(SlotIndex, Actor);
    }

    if (IsActorValid(OldActor) && OldActor != Actor)
    {
        // мягко в пул
        if (!RecycleActor(OldActor))
        {
            DestroyActorInternal(OldActor, false);
        }
    }

    return true;
}

bool USuspenseCoreEquipmentActorFactory::UnregisterActor(AActor* Actor)
{
    if (!Actor)
    {
        return false;
    }

    EQUIPMENT_SCOPE_LOCK(RegistryLock);

    // Find and remove from registry
    for (auto It = SpawnedActorRegistry.CreateIterator(); It; ++It)
    {
        if (It.Value() == Actor)
        {
            It.RemoveCurrent();
            return true;
        }
    }

    return false;
}

TMap<int32, AActor*> USuspenseCoreEquipmentActorFactory::GetAllSpawnedActors() const
{
    EQUIPMENT_SCOPE_LOCK(RegistryLock);
    return SpawnedActorRegistry;
}

void USuspenseCoreEquipmentActorFactory::ClearAllActors(bool bDestroy)
{
    // Clear registry
    {
        TArray<AActor*> ToDestroy;
        {
            EQUIPMENT_SCOPE_LOCK(RegistryLock);
            if (bDestroy)
            {
                for (auto& Pair : SpawnedActorRegistry)
                {
                    if (IsActorValid(Pair.Value))
                    {
                        ToDestroy.Add(Pair.Value);
                    }
                }
            }
            SpawnedActorRegistry.Empty();
        }

        // уничтожаем вне лока
        if (bDestroy)
        {
            for (AActor* A : ToDestroy)
            {
                DestroyActorInternal(A, true);
            }
        }
    }

    // Clear pool
    {
        TArray<AActor*> PoolToDestroy;
        {
            EQUIPMENT_SCOPE_LOCK(PoolLock);

            if (bDestroy)
            {
                for (FSuspenseCoreActorPoolEntry& Entry : ActorPool)
                {
                    if (IsActorValid(Entry.Actor))
                    {
                        PoolToDestroy.Add(Entry.Actor);
                    }
                }
            }

            ActorPool.Empty();
        }

        for (AActor* A : PoolToDestroy)
        {
            DestroyActorInternal(A, true);
        }
    }

    LogFactoryOperation(TEXT("ClearAllActors"),
        FString::Printf(TEXT("Cleared all actors, destroy=%s"), bDestroy ? TEXT("true") : TEXT("false")));
}

//========================================
// Public Methods
//========================================

void USuspenseCoreEquipmentActorFactory::SetFactoryConfiguration(const FSuspenseCoreActorFactoryConfig& NewConfig)
{
    FactoryConfig = NewConfig;

    // Restart cleanup timer with new interval
    if (FactoryConfig.PoolCleanupInterval > 0.0f && GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(PoolCleanupTimerHandle);
        GetWorld()->GetTimerManager().SetTimer(
            PoolCleanupTimerHandle,
            this,
            &USuspenseCoreEquipmentActorFactory::CleanupPool,
            FactoryConfig.PoolCleanupInterval,
            true
        );
    }
}

void USuspenseCoreEquipmentActorFactory::GetPoolStatistics(int32& TotalActors, int32& ActiveActors, int32& PooledActors) const
{
    EQUIPMENT_SCOPE_LOCK(PoolLock);

    TotalActors = ActorPool.Num();
    ActiveActors = 0;
    PooledActors = 0;

    for (const FSuspenseCoreActorPoolEntry& Entry : ActorPool)
    {
        if (Entry.bInUse)
        {
            ActiveActors++;
        }
        else
        {
            PooledActors++;
        }
    }
}

void USuspenseCoreEquipmentActorFactory::PreloadItemClasses(const TArray<FName>& ItemIds)
{
    for (const FName& ItemId : ItemIds)
    {
        PreloadActorClass(ItemId);
    }
}

//========================================
// Private Methods
//========================================

AActor* USuspenseCoreEquipmentActorFactory::SpawnActorInternal(TSubclassOf<AActor> ActorClass, const FTransform& SpawnTransform, AActor* Owner)
{
    if (!ActorClass || !GetWorld())
    {
        return nullptr;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = Owner;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    return GetWorld()->SpawnActor<AActor>(ActorClass, SpawnTransform, SpawnParams);
}

void USuspenseCoreEquipmentActorFactory::DestroyActorInternal(AActor* Actor, bool bImmediate)
{
    if (!IsActorValid(Actor))
    {
        return;
    }

    if (bImmediate)
    {
        Actor->Destroy();
    }
    else
    {
        Actor->SetLifeSpan(0.1f);
    }
}

TSubclassOf<AActor> USuspenseCoreEquipmentActorFactory::GetActorClassForItem(const FName& ItemId)
{
    // Сначала кеш
    {
        TSubclassOf<AActor> Cached = nullptr;
        if (ActorClassCache.Get(ItemId, Cached) && Cached)
        {
            return Cached;
        }
    }

    // Сабсистема ItemManager из GameInstance
    USuspenseCoreItemManager* ItemManager = nullptr;
    if (GetWorld())
    {
        if (UGameInstance* GI = GetWorld()->GetGameInstance())
        {
            ItemManager = GI->GetSubsystem<USuspenseCoreItemManager>();
        }
    }

    if (ItemManager)
    {
        FSuspenseCoreUnifiedItemData ItemData;
        if (ItemManager->GetUnifiedItemData(ItemId, ItemData))
        {
            if (!ItemData.EquipmentActorClass.IsNull())
            {
                // Пробуем уже загруженный класс
                if (ItemData.EquipmentActorClass.IsValid())
                {
                    TSubclassOf<AActor> ActorClass = ItemData.EquipmentActorClass.Get();
                    if (ActorClass)
                    {
                        ActorClassCache.Set(ItemId, ActorClass);
                        return ActorClass;
                    }
                }

                // Синхронная загрузка при необходимости
                TSubclassOf<AActor> ActorClass = ItemData.EquipmentActorClass.LoadSynchronous();
                if (ActorClass)
                {
                    ActorClassCache.Set(ItemId, ActorClass);
                    return ActorClass;
                }
            }
        }
    }

    return nullptr;
}

FSuspenseCoreActorPoolEntry* USuspenseCoreEquipmentActorFactory::FindPoolEntry(AActor* Actor)
{
    for (FSuspenseCoreActorPoolEntry& Entry : ActorPool)
    {
        if (Entry.Actor == Actor)
        {
            return &Entry;
        }
    }
    return nullptr;
}

FSuspenseCoreActorPoolEntry* USuspenseCoreEquipmentActorFactory::FindAvailablePoolEntry(TSubclassOf<AActor> ActorClass)
{
    for (FSuspenseCoreActorPoolEntry& Entry : ActorPool)
    {
        if (Entry.ActorClass == ActorClass && !Entry.bInUse && IsActorValid(Entry.Actor))
        {
            return &Entry;
        }
    }
    return nullptr;
}

FSuspenseCoreActorPoolEntry* USuspenseCoreEquipmentActorFactory::CreatePoolEntry(TSubclassOf<AActor> ActorClass)
{
    FSuspenseCoreActorPoolEntry NewEntry;
    NewEntry.ActorClass = ActorClass;
    NewEntry.bInUse = false;
    NewEntry.LastUsedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    return &ActorPool.Add_GetRef(NewEntry);
}

void USuspenseCoreEquipmentActorFactory::CleanupPool()
{
    EQUIPMENT_SCOPE_LOCK(PoolLock);

    if (!GetWorld())
    {
        return;
    }

    const float CurrentTime = GetWorld()->GetTimeSeconds();

    for (int32 i = ActorPool.Num() - 1; i >= 0; i--)
    {
        FSuspenseCoreActorPoolEntry& Entry = ActorPool[i];

        // Remove expired unused entries
        if (!Entry.bInUse && (CurrentTime - Entry.LastUsedTime) > FactoryConfig.ActorIdleTimeout)
        {
            if (IsActorValid(Entry.Actor))
            {
                DestroyActorInternal(Entry.Actor, true);
            }
            ActorPool.RemoveAt(i);
        }
        // Remove invalid actors
        else if (!IsActorValid(Entry.Actor))
        {
            ActorPool.RemoveAt(i);
        }
    }
}

void USuspenseCoreEquipmentActorFactory::OnAsyncLoadComplete(FName ItemId)
{
    // Удаляем handle загрузки
    LoadingHandles.Remove(ItemId);

    // Кладём в кеш, если теперь доступно
    TSubclassOf<AActor> ActorClass = GetActorClassForItem(ItemId);
    if (ActorClass)
    {
        ActorClassCache.Set(ItemId, ActorClass);
    }

    LogFactoryOperation(TEXT("AsyncLoadComplete"),
        FString::Printf(TEXT("Loaded class for %s"), *ItemId.ToString()));
}

bool USuspenseCoreEquipmentActorFactory::IsActorValid(AActor* Actor) const
{
    return Actor && IsValid(Actor) && !Actor->IsPendingKillPending();
}

void USuspenseCoreEquipmentActorFactory::LogFactoryOperation(const FString& Operation, const FString& Details) const
{
    // Категория логов актуализирована под EquipmentServiceMacros.h
    UE_LOG(LogSuspenseCoreEquipmentOperation, Verbose, TEXT("[EquipmentActorFactory] %s: %s"), *Operation, *Details);
}

// ============================================================================
// EventBus Integration
// ============================================================================

void USuspenseCoreEquipmentActorFactory::SetupEventBus()
{
    using namespace SuspenseCoreEquipmentTags;

    // Get EventBus via EventManager (Clean Architecture)
    if (USuspenseCoreEventManager* EventMgr = USuspenseCoreEventManager::Get(this))
    {
        EventBus = EventMgr->GetEventBus();
    }

    if (!EventBus)
    {
        UE_LOG(LogSuspenseCoreEquipmentOperation, Warning,
            TEXT("[ActorFactory] EventBus not available via EventManager"));
        return;
    }

    // Initialize event tags using native compile-time tags
    Tag_Visual_Spawned = Event::TAG_Equipment_Event_Visual_Spawned;
    Tag_Visual_Destroyed = Event::TAG_Equipment_Event_Visual_Detached;

    UE_LOG(LogSuspenseCoreEquipmentOperation, Log,
        TEXT("[ActorFactory] EventBus integration initialized"));
}

void USuspenseCoreEquipmentActorFactory::BroadcastActorSpawned(AActor* Actor, const FName& ItemId, int32 SlotIndex)
{
    if (!EventBus || !Tag_Visual_Spawned.IsValid())
    {
        return;
    }

    FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
    EventData.SetObject(TEXT("Target"), Actor);
    EventData.SetString(TEXT("ItemId"), ItemId.ToString());
    EventData.SetInt(TEXT("SlotIndex"), SlotIndex);
    EventData.SetString(TEXT("ActorClass"), Actor ? Actor->GetClass()->GetName() : TEXT("None"));

    EventBus->Publish(Tag_Visual_Spawned, EventData);

    UE_LOG(LogSuspenseCoreEquipmentOperation, Verbose,
        TEXT("[ActorFactory] Broadcast Visual.Spawned: Item=%s, Slot=%d"),
        *ItemId.ToString(), SlotIndex);
}

void USuspenseCoreEquipmentActorFactory::BroadcastActorDestroyed(AActor* Actor, const FName& ItemId)
{
    if (!EventBus || !Tag_Visual_Destroyed.IsValid())
    {
        return;
    }

    FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
    EventData.SetObject(TEXT("Target"), Actor);
    EventData.SetString(TEXT("ItemId"), ItemId.ToString());

    EventBus->Publish(Tag_Visual_Destroyed, EventData);

    UE_LOG(LogSuspenseCoreEquipmentOperation, Verbose,
        TEXT("[ActorFactory] Broadcast Visual.Destroyed: Item=%s"),
        *ItemId.ToString());
}
