// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Components/Presentation/SuspenseEquipmentActorFactory.h"
#include "ItemSystem/SuspenseItemManager.h"
#include "Services/SuspenseEquipmentServiceMacros.h"
#include "SuspenseCore/Base/SuspenseWeaponActor.h"
#include "SuspenseCore/Base/SuspenseEquipmentActor.h"
#include "SuspenseCore/Components/SuspenseEquipmentAttributeComponent.h"
#include "SuspenseCore/Components/SuspenseEquipmentAttachmentComponent.h"
#include "SuspenseCore/Components/SuspenseWeaponAmmoComponent.h"
#include "SuspenseCore/Components/SuspenseWeaponFireModeComponent.h"
#include "SuspenseCore/Components/SuspenseEquipmentMeshComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "Core/Services/SuspenseEquipmentServiceLocator.h"
#include "UObject/SoftObjectPath.h"

// ===== helper (поместите в .cpp файла фабрики, над методами) =====
static USuspenseItemManager* GetItemManagerSafe(const UWorld* World)
{
    if (!World) return nullptr;
    if (const UGameInstance* GI = World->GetGameInstance())
    {
        return GI->GetSubsystem<USuspenseItemManager>();
    }
    return nullptr;
}
//========================================
// Constructor & Lifecycle
//========================================

USuspenseEquipmentActorFactory::USuspenseEquipmentActorFactory()
    : ActorClassCache(100) // правильная инициализация кеша (без operator=)
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f;
}

void USuspenseEquipmentActorFactory::BeginPlay()
{
    Super::BeginPlay();

    USuspenseEquipmentServiceLocator* Locator = USuspenseEquipmentServiceLocator::Get(this);
    if (Locator)
    {
        const FGameplayTag FactoryTag = FGameplayTag::RequestGameplayTag(TEXT("Service.ActorFactory"));

        if (!Locator->IsServiceRegistered(FactoryTag))
        {
            Locator->RegisterServiceInstance(FactoryTag, this);

            UE_LOG(LogEquipmentOperation, Log,
                TEXT("✓ ActorFactory registered as service: Service.ActorFactory"));
        }
        else
        {
            UE_LOG(LogEquipmentOperation, Warning,
                TEXT("ActorFactory already registered in ServiceLocator"));
        }
    }
    else
    {
        UE_LOG(LogEquipmentOperation, Error,
            TEXT("❌ Failed to get ServiceLocator - ActorFactory NOT registered!"));
        UE_LOG(LogEquipmentOperation, Error,
            TEXT("   VisualizationService will use fallback spawn and actors won't be pooled"));
    }

    // Остальной код BeginPlay...
    if (FactoryConfig.PoolCleanupInterval > 0.0f)
    {
        GetWorld()->GetTimerManager().SetTimer(
            PoolCleanupTimerHandle,
            this,
            &USuspenseEquipmentActorFactory::CleanupPool,
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

void USuspenseEquipmentActorFactory::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    USuspenseEquipmentServiceLocator* Locator = USuspenseEquipmentServiceLocator::Get(this);
    if (Locator)
    {
        const FGameplayTag FactoryTag = FGameplayTag::RequestGameplayTag(TEXT("Service.ActorFactory"));

        if (Locator->IsServiceRegistered(FactoryTag))
        {
            Locator->UnregisterService(FactoryTag, /*bForceShutdown=*/false);

            UE_LOG(LogEquipmentOperation, Log,
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
void USuspenseEquipmentActorFactory::TickComponent(
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
/* ISuspenseActorFactory Implementation */
//========================================

// MedComEquipmentActorFactory.cpp

FEquipmentActorSpawnResult USuspenseEquipmentActorFactory::SpawnEquipmentActor(const FEquipmentActorSpawnParams& Params)
{
    EQUIPMENT_SCOPE_LOCK(PoolLock);

    FEquipmentActorSpawnResult Result;

    // ============================================================================
    // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ #1: Загружаем полные данные предмета из DataTable
    // ============================================================================
    USuspenseItemManager* ItemManager = GetItemManagerSafe(GetWorld());
    if (!ItemManager)
    {
        Result.bSuccess = false;
        Result.FailureReason = FText::FromString(TEXT("ItemManager subsystem not available"));
        UE_LOG(LogEquipmentOperation, Error,
            TEXT("[SpawnEquipmentActor] ItemManager not found - cannot load item data"));
        return Result;
    }

    // Load full item data from DataTable - single source of truth
    FSuspenseUnifiedItemData ItemData;
    if (!ItemManager->GetUnifiedItemData(Params.ItemInstance.ItemID, ItemData))
    {
        Result.bSuccess = false;
        Result.FailureReason = FText::FromString(
            FString::Printf(TEXT("Item data not found for ItemID: %s"),
            *Params.ItemInstance.ItemID.ToString()));
        UE_LOG(LogEquipmentOperation, Error,
            TEXT("[SpawnEquipmentActor] Failed to load item data for ItemID: %s"),
            *Params.ItemInstance.ItemID.ToString());
        return Result;
    }

    UE_LOG(LogEquipmentOperation, Log,
        TEXT("[SpawnEquipmentActor] Loaded ItemData for %s: Type=%s, IsWeapon=%d, IsEquippable=%d"),
        *Params.ItemInstance.ItemID.ToString(),
        *ItemData.ItemType.ToString(),
        ItemData.bIsWeapon,
        ItemData.bIsEquippable);

    // ============================================================================
    // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ #2: Инициализируем RuntimeProperties для оружия
    // ============================================================================
    // Create enriched instance with initialized runtime properties
    FSuspenseInventoryItemInstance EnrichedInstance = Params.ItemInstance;

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

            UE_LOG(LogEquipmentOperation, Verbose,
                TEXT("[SpawnEquipmentActor] Initialized CurrentAmmo=30 for weapon %s"),
                *ItemData.ItemID.ToString());
        }

        if (!EnrichedInstance.RuntimeProperties.Contains(TEXT("RemainingAmmo")))
        {
            EnrichedInstance.SetRuntimeProperty(TEXT("RemainingAmmo"), 90.0f);

            UE_LOG(LogEquipmentOperation, Verbose,
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

            UE_LOG(LogEquipmentOperation, Verbose,
                TEXT("[SpawnEquipmentActor] Using already loaded ActorClass: %s"),
                *ActorClass->GetName());
        }
        else
        {
            // Synchronous load if needed
            ActorClass = ItemData.EquipmentActorClass.LoadSynchronous();

            if (ActorClass)
            {
                UE_LOG(LogEquipmentOperation, Log,
                    TEXT("[SpawnEquipmentActor] Loaded ActorClass synchronously: %s"),
                    *ActorClass->GetName());
            }
        }
    }

    if (!ActorClass)
    {
        // Fallback to cache-based resolution (last resort)
        UE_LOG(LogEquipmentOperation, Warning,
            TEXT("[SpawnEquipmentActor] EquipmentActorClass is null in DataTable, trying cache fallback"));

        ActorClass = GetActorClassForItem(Params.ItemInstance.ItemID);
    }

    if (!ActorClass)
    {
        Result.bSuccess = false;
        Result.FailureReason = FText::FromString(TEXT("Actor class not found"));
        UE_LOG(LogEquipmentOperation, Error,
            TEXT("[SpawnEquipmentActor] No valid ActorClass for ItemID: %s - DataTable EquipmentActorClass is null or invalid"),
            *Params.ItemInstance.ItemID.ToString());
        return Result;
    }

    UE_LOG(LogEquipmentOperation, Log,
        TEXT("[SpawnEquipmentActor] Resolved ActorClass: %s for ItemID: %s"),
        *ActorClass->GetName(),
        *Params.ItemInstance.ItemID.ToString());

    // ============================================================================
    // Actor spawning - check pool first, then spawn new
    // ============================================================================
    AActor* SpawnedActor = GetPooledActor(ActorClass);
    if (!SpawnedActor)
    {
        SpawnedActor = SpawnActorInternal(ActorClass, Params.SpawnTransform, Params.Owner);
        if (!SpawnedActor)
        {
            Result.bSuccess = false;
            Result.FailureReason = FText::FromString(TEXT("Failed to spawn actor"));
            UE_LOG(LogEquipmentOperation, Error,
                TEXT("[SpawnEquipmentActor] SpawnActorInternal failed for class: %s"),
                *ActorClass->GetName());
            return Result;
        }

        UE_LOG(LogEquipmentOperation, Log,
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

        UE_LOG(LogEquipmentOperation, Log,
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
        UE_LOG(LogEquipmentOperation, Error,
            TEXT("[SpawnEquipmentActor] ConfigureEquipmentActor failed for actor: %s"),
            *SpawnedActor->GetName());
        DestroyEquipmentActor(SpawnedActor, true);
        Result.bSuccess = false;
        Result.FailureReason = FText::FromString(TEXT("Failed to configure actor"));
        return Result;
    }

    UE_LOG(LogEquipmentOperation, Log,
        TEXT("[SpawnEquipmentActor] ✓ Successfully configured actor: %s"),
        *SpawnedActor->GetName());

    // ============================================================================
    // Register spawned actor in slot registry
    // ============================================================================
    // Extract SlotIndex from custom parameters if available
    int32 SlotIndex = INDEX_NONE;
    if (const FString* SlotStr = Params.CustomParameters.Find(TEXT("SlotIndex")))
    {
        SlotIndex = FCString::Atoi(**SlotStr);
    }

    if (SlotIndex != INDEX_NONE)
    {
        RegisterSpawnedActor(SpawnedActor, SlotIndex);

        UE_LOG(LogEquipmentOperation, Verbose,
            TEXT("[SpawnEquipmentActor] ✓ Registered actor in slot: %d"), SlotIndex);
    }

    // ============================================================================
    // Success - return result
    // ============================================================================
    Result.bSuccess = true;
    Result.SpawnedActor = SpawnedActor;

    UE_LOG(LogEquipmentOperation, Log,
        TEXT("[SpawnEquipmentActor] ✓✓✓ SUCCESS ✓✓✓"));
    UE_LOG(LogEquipmentOperation, Log,
        TEXT("  Spawned: %s"), *SpawnedActor->GetName());
    UE_LOG(LogEquipmentOperation, Log,
        TEXT("  ItemID: %s"), *EnrichedInstance.ItemID.ToString());
    UE_LOG(LogEquipmentOperation, Log,
        TEXT("  InstanceID: %s"), *EnrichedInstance.InstanceID.ToString());
    UE_LOG(LogEquipmentOperation, Log,
        TEXT("  RuntimeProperties: %d"), EnrichedInstance.RuntimeProperties.Num());

    return Result;
}
bool USuspenseEquipmentActorFactory::DestroyEquipmentActor(AActor* Actor, bool bImmediate)
{
    if (!IsActorValid(Actor))
    {
        return false;
    }

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

bool USuspenseEquipmentActorFactory::ConfigureEquipmentActor(AActor* Actor, const FSuspenseInventoryItemInstance& ItemInstance)
{
    if (!IsActorValid(Actor) || !ItemInstance.IsValid())
    {
        return false;
    }

    // 1) Всегда сначала пробуем интерфейс экипировки
    if (Actor->GetClass()->ImplementsInterface(USuspenseEquipment::StaticClass()))
    {
        ISuspenseEquipment::Execute_OnItemInstanceEquipped(Actor, ItemInstance);
    }
    else if (ASuspenseEquipmentActor* EquipmentActor = Cast<ASuspenseEquipmentActor>(Actor))
    {
        // Fallback для редких случаев (но по нашей архитектуре базовый актор реализует интерфейс)
        EquipmentActor->OnItemInstanceEquipped_Implementation(ItemInstance);
    }

    // 2) Если актор поддерживает оружейный интерфейс — пробрасываем стартовый AmmoState ЧЕРЕЗ ИНТЕРФЕЙС
    if (Actor->GetClass()->ImplementsInterface(USuspenseWeapon::StaticClass()))
    {
        FSuspenseInventoryAmmoState AmmoState{};
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
            ISuspenseWeapon::Execute_SetAmmoState(Actor, AmmoState);
        }
    }

    return true;
}

bool USuspenseEquipmentActorFactory::RecycleActor(AActor* Actor)
{
    if (!IsActorValid(Actor))
    {
        return false;
    }

    EQUIPMENT_SCOPE_LOCK(PoolLock);

    // Find existing pool entry
    FActorPoolEntry* Entry = FindPoolEntry(Actor);
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
    for (const FActorPoolEntry& PoolEntry : ActorPool)
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
    FActorPoolEntry NewEntry;
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

AActor* USuspenseEquipmentActorFactory::GetPooledActor(TSubclassOf<AActor> ActorClass)
{
    if (!ActorClass)
    {
        return nullptr;
    }

    EQUIPMENT_SCOPE_LOCK(PoolLock);

    FActorPoolEntry* Entry = FindAvailablePoolEntry(ActorClass);
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

bool USuspenseEquipmentActorFactory::PreloadActorClass(const FName& ItemId)
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
    if (USuspenseItemManager* ItemManager = GetItemManagerSafe(GetWorld()))
    {
        FSuspenseUnifiedItemData ItemData;
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
                FStreamableDelegate::CreateUObject(this, &USuspenseEquipmentActorFactory::OnAsyncLoadComplete, ItemId)
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

FTransform USuspenseEquipmentActorFactory::GetSpawnTransformForSlot(int32 SlotIndex, AActor* Owner) const
{
    // Default spawn at owner location
    if (IsValid(Owner))
    {
        return Owner->GetActorTransform();
    }

    return FTransform::Identity;
}

bool USuspenseEquipmentActorFactory::RegisterSpawnedActor(AActor* Actor, int32 SlotIndex)
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

bool USuspenseEquipmentActorFactory::UnregisterActor(AActor* Actor)
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

TMap<int32, AActor*> USuspenseEquipmentActorFactory::GetAllSpawnedActors() const
{
    EQUIPMENT_SCOPE_LOCK(RegistryLock);
    return SpawnedActorRegistry;
}

void USuspenseEquipmentActorFactory::ClearAllActors(bool bDestroy)
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
                for (FActorPoolEntry& Entry : ActorPool)
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

void USuspenseEquipmentActorFactory::SetFactoryConfiguration(const FActorFactoryConfig& NewConfig)
{
    FactoryConfig = NewConfig;

    // Restart cleanup timer with new interval
    if (FactoryConfig.PoolCleanupInterval > 0.0f && GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(PoolCleanupTimerHandle);
        GetWorld()->GetTimerManager().SetTimer(
            PoolCleanupTimerHandle,
            this,
            &USuspenseEquipmentActorFactory::CleanupPool,
            FactoryConfig.PoolCleanupInterval,
            true
        );
    }
}

void USuspenseEquipmentActorFactory::GetPoolStatistics(int32& TotalActors, int32& ActiveActors, int32& PooledActors) const
{
    EQUIPMENT_SCOPE_LOCK(PoolLock);

    TotalActors = ActorPool.Num();
    ActiveActors = 0;
    PooledActors = 0;

    for (const FActorPoolEntry& Entry : ActorPool)
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

void USuspenseEquipmentActorFactory::PreloadItemClasses(const TArray<FName>& ItemIds)
{
    for (const FName& ItemId : ItemIds)
    {
        PreloadActorClass(ItemId);
    }
}

//========================================
// Private Methods
//========================================

AActor* USuspenseEquipmentActorFactory::SpawnActorInternal(TSubclassOf<AActor> ActorClass, const FTransform& SpawnTransform, AActor* Owner)
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

void USuspenseEquipmentActorFactory::DestroyActorInternal(AActor* Actor, bool bImmediate)
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

TSubclassOf<AActor> USuspenseEquipmentActorFactory::GetActorClassForItem(const FName& ItemId)
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
    USuspenseItemManager* ItemManager = nullptr;
    if (GetWorld())
    {
        if (UGameInstance* GI = GetWorld()->GetGameInstance())
        {
            ItemManager = GI->GetSubsystem<USuspenseItemManager>();
        }
    }

    if (ItemManager)
    {
        FSuspenseUnifiedItemData ItemData;
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

FActorPoolEntry* USuspenseEquipmentActorFactory::FindPoolEntry(AActor* Actor)
{
    for (FActorPoolEntry& Entry : ActorPool)
    {
        if (Entry.Actor == Actor)
        {
            return &Entry;
        }
    }
    return nullptr;
}

FActorPoolEntry* USuspenseEquipmentActorFactory::FindAvailablePoolEntry(TSubclassOf<AActor> ActorClass)
{
    for (FActorPoolEntry& Entry : ActorPool)
    {
        if (Entry.ActorClass == ActorClass && !Entry.bInUse && IsActorValid(Entry.Actor))
        {
            return &Entry;
        }
    }
    return nullptr;
}

FActorPoolEntry* USuspenseEquipmentActorFactory::CreatePoolEntry(TSubclassOf<AActor> ActorClass)
{
    FActorPoolEntry NewEntry;
    NewEntry.ActorClass = ActorClass;
    NewEntry.bInUse = false;
    NewEntry.LastUsedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    return &ActorPool.Add_GetRef(NewEntry);
}

void USuspenseEquipmentActorFactory::CleanupPool()
{
    EQUIPMENT_SCOPE_LOCK(PoolLock);

    if (!GetWorld())
    {
        return;
    }

    const float CurrentTime = GetWorld()->GetTimeSeconds();

    for (int32 i = ActorPool.Num() - 1; i >= 0; i--)
    {
        FActorPoolEntry& Entry = ActorPool[i];

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

void USuspenseEquipmentActorFactory::OnAsyncLoadComplete(FName ItemId)
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

bool USuspenseEquipmentActorFactory::IsActorValid(AActor* Actor) const
{
    return Actor && IsValid(Actor) && !Actor->IsPendingKillPending();
}

void USuspenseEquipmentActorFactory::LogFactoryOperation(const FString& Operation, const FString& Details) const
{
    // Категория логов актуализирована под EquipmentServiceMacros.h
    UE_LOG(LogEquipmentOperation, Verbose, TEXT("[EquipmentActorFactory] %s: %s"), *Operation, *Details);
}
