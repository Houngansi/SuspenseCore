// MedComBasePickupItem.cpp
// Copyright MedCom Team. All Rights Reserved.

#include "Pickup/MedComBasePickupItem.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraComponent.h"
#include "Components/AudioComponent.h"
#include "Interfaces/Inventory/IMedComInventoryInterface.h"
#include "ItemSystem/MedComItemManager.h"
#include "Utils/MedComStaticHelpers.h"
#include "Utils/MedComInteractionSettings.h"
#include "Delegates/EventDelegateManager.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/DataTable.h"
#include "NiagaraFunctionLibrary.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

AMedComBasePickupItem::AMedComBasePickupItem()
{
    bReplicates = true;
    SetReplicateMovement(true);
    
    // Get default settings
    const UMedComInteractionSettings* Settings = GetDefault<UMedComInteractionSettings>();
    TEnumAsByte<ECollisionChannel> TraceChannel = 
        Settings ? Settings->DefaultTraceChannel : TEnumAsByte<ECollisionChannel>(ECC_Visibility);
    
    // Create root collision component
    SphereCollision = CreateDefaultSubobject<USphereComponent>(TEXT("SphereCollision"));
    RootComponent = SphereCollision;
    SphereCollision->InitSphereRadius(100.0f);
    SphereCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    SphereCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
    SphereCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    SphereCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    SphereCollision->SetCollisionResponseToChannel(TraceChannel, ECR_Block);
    
    // Create mesh component
    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    MeshComponent->SetupAttachment(RootComponent);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    MeshComponent->SetCollisionResponseToChannel(TraceChannel, ECR_Block);
    
    // Create VFX component (inactive by default)
    SpawnVFXComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SpawnVFX"));
    SpawnVFXComponent->SetupAttachment(RootComponent);
    SpawnVFXComponent->bAutoActivate = false;
    
    // Create audio component (inactive by default)
    AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComponent"));
    AudioComponent->SetupAttachment(RootComponent);
    AudioComponent->bAutoActivate = false;
    
    // Default values
    Amount = 1;
    bHasSavedAmmoState = false;
    SavedCurrentAmmo = 0.0f;
    SavedRemainingAmmo = 0.0f;
    DestroyDelay = 0.1f;
    InteractionPriority = 0;
    InteractionDistanceOverride = 0.0f;
    bDataCached = false;
    bUseRuntimeInstance = false;
}

void AMedComBasePickupItem::BeginPlay()
{
    Super::BeginPlay();
    
    UE_LOG(LogMedComInteraction, Log, TEXT("Pickup BeginPlay: %s with ItemID: %s"), 
        *GetName(), *ItemID.ToString());
    
    // Load item data from DataTable
    if (!ItemID.IsNone())
    {
        if (LoadItemData())
        {
            // Apply visuals and effects from loaded data
            ApplyItemVisuals();
            ApplyItemAudio();
            ApplyItemVFX();
        }
        else
        {
            UE_LOG(LogMedComInteraction, Error, TEXT("Pickup %s failed to load item data for: %s"), 
                *GetName(), *ItemID.ToString());
        }
    }
    else
    {
        UE_LOG(LogMedComInteraction, Warning, TEXT("Pickup %s has no ItemID set!"), *GetName());
    }
    
    // Broadcast spawn event
    BroadcastPickupSpawned();
}

void AMedComBasePickupItem::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CachedDelegateManager.Reset();
    Super::EndPlay(EndPlayReason);
}

void AMedComBasePickupItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    DOREPLIFETIME(AMedComBasePickupItem, ItemID);
    DOREPLIFETIME(AMedComBasePickupItem, Amount);
    DOREPLIFETIME(AMedComBasePickupItem, bHasSavedAmmoState);
    DOREPLIFETIME(AMedComBasePickupItem, SavedCurrentAmmo);
    DOREPLIFETIME(AMedComBasePickupItem, SavedRemainingAmmo);
    DOREPLIFETIME(AMedComBasePickupItem, bUseRuntimeInstance);
    DOREPLIFETIME(AMedComBasePickupItem, PresetRuntimeProperties);
}

void AMedComBasePickupItem::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    
    // In editor, try to load and apply visuals
    if (GetWorld() && GetWorld()->IsEditorWorld() && !ItemID.IsNone())
    {
        if (LoadItemData())
        {
            ApplyItemVisuals();
        }
    }
}

//==================================================================
// New Initialization Methods
//==================================================================

void AMedComBasePickupItem::InitializeFromInstance(const FInventoryItemInstance& Instance)
{
    if (!Instance.IsValid())
    {
        UE_LOG(LogMedComInteraction, Warning, TEXT("InitializeFromInstance: Invalid instance provided"));
        return;
    }
    
    // Set runtime instance and enable its usage
    RuntimeInstance = Instance;
    bUseRuntimeInstance = true;
    
    // Extract basic properties from instance
    ItemID = Instance.ItemID;
    Amount = Instance.Quantity;
    
    // Handle weapon ammo state
    if (Instance.HasRuntimeProperty(TEXT("Ammo")))
    {
        bHasSavedAmmoState = true;
        SavedCurrentAmmo = Instance.GetRuntimeProperty(TEXT("Ammo"), 0.0f);
        SavedRemainingAmmo = Instance.GetRuntimeProperty(TEXT("RemainingAmmo"), 0.0f);
    }
    
    // Trigger data loading and visual update
    if (LoadItemData())
    {
        ApplyItemVisuals();
        ApplyItemAudio();
        ApplyItemVFX();
    }
    
    UE_LOG(LogMedComInteraction, Log, TEXT("InitializeFromInstance: Initialized pickup for %s with full runtime state"), 
        *ItemID.ToString());
}

void AMedComBasePickupItem::InitializeFromSpawnData(const FPickupSpawnData& SpawnData)
{
    if (!SpawnData.IsValid())
    {
        UE_LOG(LogMedComInteraction, Warning, 
            TEXT("InitializeFromSpawnData: Invalid spawn data provided"));
        return;
    }
    
    // Устанавливаем базовые свойства
    ItemID = SpawnData.ItemID;
    Amount = SpawnData.Quantity;
    
    // Конвертируем TMap в TArray для репликации
    SetPresetPropertiesFromMap(SpawnData.PresetRuntimeProperties);
    
    // Не используем полный runtime instance для spawn data
    bUseRuntimeInstance = false;
    
    // Проверяем наличие патронов в предустановленных свойствах
    float AmmoValue = GetPresetProperty(TEXT("Ammo"), -1.0f);
    float RemainingAmmoValue = GetPresetProperty(TEXT("RemainingAmmo"), -1.0f);
    
    if (AmmoValue >= 0.0f && RemainingAmmoValue >= 0.0f)
    {
        bHasSavedAmmoState = true;
        SavedCurrentAmmo = AmmoValue;
        SavedRemainingAmmo = RemainingAmmoValue;
    }
    
    // Загружаем данные и применяем визуальные эффекты
    if (LoadItemData())
    {
        ApplyItemVisuals();
        ApplyItemAudio();
        ApplyItemVFX();
    }
    
    UE_LOG(LogMedComInteraction, Log, 
        TEXT("InitializeFromSpawnData: Initialized pickup for %s from spawn data"), 
        *ItemID.ToString());
}

//==================================================================
// IMedComInteractInterface Implementation
//==================================================================

bool AMedComBasePickupItem::CanInteract_Implementation(APlayerController* InstigatingController) const
{
    UE_LOG(LogMedComInteraction, Warning, TEXT("CanInteract: Checking for %s"), *GetName());
    
    if (!InstigatingController || !InstigatingController->GetPawn())
    {
        UE_LOG(LogMedComInteraction, Warning, TEXT("CanInteract: No controller or pawn"));
        return false;
    }
    
    // Must have valid item ID and cached data
    if (ItemID.IsNone() || !bDataCached)
    {
        UE_LOG(LogMedComInteraction, Warning, 
            TEXT("CanInteract: Failed - ItemID=%s, DataCached=%d"), 
            *ItemID.ToString(), bDataCached);
        return false;
    }
    
    // On client, always allow (server will validate)
    if (!HasAuthority())
    {
        UE_LOG(LogMedComInteraction, Log, TEXT("CanInteract: Client - allowing interaction"));
        return true;
    }
    
    // Check if can be picked up
    bool bCanPickup = CanBePickedUpBy_Implementation(InstigatingController->GetPawn());
    UE_LOG(LogMedComInteraction, Warning, 
        TEXT("CanInteract: CanBePickedUpBy returned %s"), 
        bCanPickup ? TEXT("true") : TEXT("false"));
    
    return bCanPickup;
}

bool AMedComBasePickupItem::Interact_Implementation(APlayerController* InstigatingController)
{
    if (!HasAuthority())
    {
        UE_LOG(LogMedComInteraction, Warning, TEXT("Interact called on client for %s"), *GetName());
        return false;
    }
    
    if (!InstigatingController || !InstigatingController->GetPawn())
    {
        return false;
    }
    
    AActor* Pawn = InstigatingController->GetPawn();
    
    // Broadcast interaction started
    IMedComInteractInterface::BroadcastInteractionStarted(this, InstigatingController, GetInteractionType_Implementation());
    
    bool bSuccess = HandlePickedUp_Implementation(Pawn);
    
    // Broadcast interaction completed
    IMedComInteractInterface::BroadcastInteractionCompleted(this, InstigatingController, bSuccess);
    
    return bSuccess;
}

FGameplayTag AMedComBasePickupItem::GetInteractionType_Implementation() const
{
    // Load data if not cached
    if (!bDataCached)
    {
        LoadItemData();
    }
    
    // Return specific interaction type based on item
    if (bDataCached && CachedItemData.bIsWeapon)
    {
        return FGameplayTag::RequestGameplayTag(TEXT("Interaction.Type.Weapon"));
    }
    
    if (bDataCached && CachedItemData.bIsAmmo)
    {
        return FGameplayTag::RequestGameplayTag(TEXT("Interaction.Type.Ammo"));
    }
    
    return FGameplayTag::RequestGameplayTag(TEXT("Interaction.Type.Pickup"));
}

FText AMedComBasePickupItem::GetInteractionText_Implementation() const
{
    // Load data if not cached
    if (!bDataCached)
    {
        LoadItemData();
    }
    
    if (bDataCached)
    {
        return FText::Format(FText::FromString(TEXT("Pick up {0}")), CachedItemData.DisplayName);
    }
    
    return FText::FromString(TEXT("Pick up"));
}

int32 AMedComBasePickupItem::GetInteractionPriority_Implementation() const
{
    return InteractionPriority;
}

float AMedComBasePickupItem::GetInteractionDistance_Implementation() const
{
    if (InteractionDistanceOverride > 0.0f)
    {
        return InteractionDistanceOverride;
    }
    
    const UMedComInteractionSettings* Settings = GetDefault<UMedComInteractionSettings>();
    return Settings ? Settings->DefaultTraceDistance : 300.0f;
}

void AMedComBasePickupItem::OnInteractionFocusGained_Implementation(APlayerController* InstigatingController)
{
    IMedComInteractInterface::BroadcastInteractionFocusChanged(this, InstigatingController, true);
    HandleInteractionFeedback(true);
}

void AMedComBasePickupItem::OnInteractionFocusLost_Implementation(APlayerController* InstigatingController)
{
    IMedComInteractInterface::BroadcastInteractionFocusChanged(this, InstigatingController, false);
    HandleInteractionFeedback(false);
}

UEventDelegateManager* AMedComBasePickupItem::GetDelegateManager() const
{
    if (CachedDelegateManager.IsValid())
    {
        return CachedDelegateManager.Get();
    }
    
    UEventDelegateManager* Manager = IMedComInteractInterface::GetDelegateManagerStatic(this);
    if (Manager)
    {
        CachedDelegateManager = Manager;
    }
    
    return Manager;
}

//==================================================================
// IMedComPickupInterface Implementation
//==================================================================

FName AMedComBasePickupItem::GetItemID_Implementation() const
{
    return ItemID;
}

void AMedComBasePickupItem::SetItemID_Implementation(FName NewItemID)
{
    if (ItemID != NewItemID)
    {
        ItemID = NewItemID;
        bDataCached = false; // Invalidate cache
        
        // Reload data if needed
        if (HasAuthority() || (GetWorld() && GetWorld()->IsEditorWorld()))
        {
            LoadItemData();
        }
    }
}

bool AMedComBasePickupItem::GetUnifiedItemData_Implementation(FMedComUnifiedItemData& OutItemData) const
{
    // Load data if not cached
    if (!bDataCached)
    {
        LoadItemData();
    }
    
    if (bDataCached)
    {
        OutItemData = CachedItemData;
        return true;
    }
    
    return false;
}

int32 AMedComBasePickupItem::GetItemAmount_Implementation() const
{
    return Amount;
}

void AMedComBasePickupItem::SetAmount_Implementation(int32 NewAmount)
{
    Amount = FMath::Max(1, NewAmount);
}

bool AMedComBasePickupItem::HasSavedAmmoState_Implementation() const
{
    return bHasSavedAmmoState;
}

bool AMedComBasePickupItem::GetSavedAmmoState_Implementation(float& OutCurrentAmmo, float& OutRemainingAmmo) const
{
    if (bHasSavedAmmoState)
    {
        OutCurrentAmmo = SavedCurrentAmmo;
        OutRemainingAmmo = SavedRemainingAmmo;
        return true;
    }
    
    return false;
}

void AMedComBasePickupItem::SetSavedAmmoState_Implementation(float CurrentAmmo, float RemainingAmmo)
{
    bHasSavedAmmoState = true;
    SavedCurrentAmmo = CurrentAmmo;
    SavedRemainingAmmo = RemainingAmmo;
}

bool AMedComBasePickupItem::CreateItemInstance_Implementation(FInventoryItemInstance& OutInstance) const
{
    // Если у нас есть полный runtime instance - используем его
    if (bUseRuntimeInstance && RuntimeInstance.IsValid())
    {
        OutInstance = RuntimeInstance;
        UE_LOG(LogMedComInteraction, Log, 
            TEXT("CreateItemInstance: Using full runtime instance for %s"), 
            *ItemID.ToString());
        return true;
    }
    
    // Иначе создаем новый экземпляр
    if (!bDataCached)
    {
        LoadItemData();
    }
    
    if (!bDataCached)
    {
        return false;
    }
    
    // Получаем менеджер предметов для создания экземпляра
    UMedComItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        return false;
    }
    
    // Создаем экземпляр через менеджер предметов
    if (!ItemManager->CreateItemInstance(ItemID, Amount, OutInstance))
    {
        return false;
    }
    
    // Применяем предустановленные свойства к создаваемому экземпляру
    for (const FPresetPropertyPair& PropertyPair : PresetRuntimeProperties)
    {
        OutInstance.SetRuntimeProperty(PropertyPair.PropertyName, PropertyPair.PropertyValue);
    }
    
    // Применяем состояние патронов для оружия
    if (CachedItemData.bIsWeapon && bHasSavedAmmoState)
    {
        OutInstance.SetRuntimeProperty(TEXT("Ammo"), SavedCurrentAmmo);
        OutInstance.SetRuntimeProperty(TEXT("RemainingAmmo"), SavedRemainingAmmo);
    }
    
    return true;
}

FGameplayTag AMedComBasePickupItem::GetItemRarity_Implementation() const
{
    if (!bDataCached)
    {
        LoadItemData();
    }
    
    if (bDataCached)
    {
        return CachedItemData.Rarity;
    }
    
    return FGameplayTag();
}

FText AMedComBasePickupItem::GetDisplayName_Implementation() const
{
    if (!bDataCached)
    {
        LoadItemData();
    }
    
    if (bDataCached)
    {
        return CachedItemData.DisplayName;
    }
    
    return FText::FromString(ItemID.ToString());
}

bool AMedComBasePickupItem::IsStackable_Implementation() const
{
    if (!bDataCached)
    {
        LoadItemData();
    }
    
    if (bDataCached)
    {
        return CachedItemData.MaxStackSize > 1;
    }
    
    return false;
}

float AMedComBasePickupItem::GetItemWeight_Implementation() const
{
    if (!bDataCached)
    {
        LoadItemData();
    }
    
    if (bDataCached)
    {
        return CachedItemData.Weight;
    }
    
    return 1.0f;
}

bool AMedComBasePickupItem::HandlePickedUp_Implementation(AActor* InstigatorActor)
{
    if (!HasAuthority() || !InstigatorActor)
    {
        return false;
    }
    
    if (!CanBePickedUpBy_Implementation(InstigatorActor))
    {
        return false;
    }
    
    // Try to add to inventory
    if (TryAddToInventory(InstigatorActor))
    {
        OnPickedUp(InstigatorActor);
        return true;
    }
    
    return false;
}

bool AMedComBasePickupItem::CanBePickedUpBy_Implementation(AActor* InstigatorActor) const
{
    UE_LOG(LogMedComInteraction, Log, 
        TEXT("CanBePickedUpBy: Checking pickup %s for actor %s"), 
        *GetName(), *GetNameSafe(InstigatorActor));
    
    if (!InstigatorActor)
    {
        UE_LOG(LogMedComInteraction, Warning, 
            TEXT("CanBePickedUpBy: No instigator actor"));
        return false;
    }
    
    // Убеждаемся что данные загружены
    if (!bDataCached)
    {
        LoadItemData();
    }
    
    if (!bDataCached)
    {
        UE_LOG(LogMedComInteraction, Warning, 
            TEXT("CanBePickedUpBy: Failed to load item data for %s"), 
            *GetName());
        return false;
    }
    
    // Детальное логирование данных предмета
    UE_LOG(LogMedComInteraction, Log, 
        TEXT("CanBePickedUpBy: Item details - ID:%s, Type:%s, DisplayName:%s, Quantity:%d"), 
        *ItemID.ToString(), 
        *CachedItemData.ItemType.ToString(),
        *CachedItemData.DisplayName.ToString(),
        Amount);
    
    // Проверяем базовую валидность типа предмета
    static const FGameplayTag BaseItemTag = FGameplayTag::RequestGameplayTag(TEXT("Item"));
    if (!CachedItemData.ItemType.MatchesTag(BaseItemTag))
    {
        UE_LOG(LogMedComInteraction, Error, 
            TEXT("CanBePickedUpBy: Item type %s is not in Item.* hierarchy! Cannot pickup."), 
            *CachedItemData.ItemType.ToString());
        return false;
    }
    
    // Проверяем через статический хелпер с расширенной диагностикой
    bool bCanPickup = UMedComStaticHelpers::CanActorPickupItem(InstigatorActor, ItemID, Amount);
    
    UE_LOG(LogMedComInteraction, Log, 
        TEXT("CanBePickedUpBy: Final result for %s = %s"), 
        *ItemID.ToString(),
        bCanPickup ? TEXT("CAN PICKUP") : TEXT("CANNOT PICKUP"));
    
    return bCanPickup;
}
FGameplayTag AMedComBasePickupItem::GetItemType_Implementation() const
{
    // Load data if not cached
    if (!bDataCached)
    {
        LoadItemData();
    }
    
    if (bDataCached)
    {
        return CachedItemData.GetEffectiveItemType();
    }
    
    return FGameplayTag::RequestGameplayTag(TEXT("Item.Generic"));
}

//==================================================================
// Event Handlers
//==================================================================

bool AMedComBasePickupItem::OnPickedUp_Implementation(AActor* InstigatorActor)
{
    UE_LOG(LogMedComInteraction, Log, TEXT("Item %s picked up by %s"), 
        *ItemID.ToString(), *InstigatorActor->GetName());
    
    // Broadcast event
    BroadcastPickupCollected(InstigatorActor);
    
    // Schedule destruction
    SetLifeSpan(DestroyDelay);
    
    // Disable collision
    SphereCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    
    // Hide visuals
    if (MeshComponent)
    {
        MeshComponent->SetVisibility(false);
    }
    
    // Play collect VFX
    if (bDataCached && !CachedItemData.PickupCollectVFX.IsNull())
    {
        UNiagaraSystem* CollectVFX = CachedItemData.PickupCollectVFX.LoadSynchronous();
        if (CollectVFX)
        {
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                GetWorld(),
                CollectVFX,
                GetActorLocation(),
                GetActorRotation()
            );
        }
    }
    
    // Play pickup sound
    if (bDataCached && !CachedItemData.PickupSound.IsNull())
    {
        USoundBase* Sound = CachedItemData.PickupSound.LoadSynchronous();
        if (Sound)
        {
            UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
        }
    }
    
    return true;
}

//==================================================================
// Data Management
//==================================================================

bool AMedComBasePickupItem::LoadItemData() const
{
    if (ItemID.IsNone())
    {
        UE_LOG(LogMedComInteraction, Warning, TEXT("LoadItemData: ItemID is None"));
        return false;
    }
    
    // Get item manager
    UMedComItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        UE_LOG(LogMedComInteraction, Warning, TEXT("LoadItemData: ItemManager not found"));
        return false;
    }
    
    // Get unified data from DataTable
    if (ItemManager->GetUnifiedItemData(ItemID, CachedItemData))
    {
        bDataCached = true;
        
        UE_LOG(LogMedComInteraction, Log, TEXT("LoadItemData: Loaded data for %s"), *ItemID.ToString());
        
        // Call blueprint events for type-specific setup
        if (CachedItemData.bIsWeapon)
        {
            const_cast<AMedComBasePickupItem*>(this)->OnWeaponPickupSetup();
        }
        else if (CachedItemData.bIsArmor)
        {
            const_cast<AMedComBasePickupItem*>(this)->OnArmorPickupSetup();
        }
        
        return true;
    }
    
    UE_LOG(LogMedComInteraction, Warning, TEXT("LoadItemData: Failed to load data for %s"), *ItemID.ToString());
    return false;
}

void AMedComBasePickupItem::ApplyItemVisuals()
{
    if (!bDataCached || !MeshComponent)
    {
        return;
    }
    
    // Apply world mesh from DataTable
    if (!CachedItemData.WorldMesh.IsNull())
    {
        UStaticMesh* Mesh = CachedItemData.WorldMesh.LoadSynchronous();
        if (Mesh)
        {
            MeshComponent->SetStaticMesh(Mesh);
            UE_LOG(LogMedComInteraction, Log, TEXT("Applied mesh for %s"), *ItemID.ToString());
        }
    }
    
    // Notify blueprint
    OnVisualsApplied();
}

void AMedComBasePickupItem::ApplyItemAudio()
{
    // Audio is played on pickup, not ambient
    // Could be extended for ambient sounds if needed
}

void AMedComBasePickupItem::ApplyItemVFX()
{
    if (!bDataCached || !SpawnVFXComponent)
    {
        return;
    }
    
    // Apply spawn VFX from DataTable
    if (!CachedItemData.PickupSpawnVFX.IsNull())
    {
        UNiagaraSystem* SpawnVFX = CachedItemData.PickupSpawnVFX.LoadSynchronous();
        if (SpawnVFX)
        {
            SpawnVFXComponent->SetAsset(SpawnVFX);
            SpawnVFXComponent->Activate();
        }
    }
}

//==================================================================
// Utility Methods
//==================================================================

void AMedComBasePickupItem::SetAmmoState(bool bHasState, float CurrentAmmo, float RemainingAmmo)
{
    bHasSavedAmmoState = bHasState;
    SavedCurrentAmmo = CurrentAmmo;
    SavedRemainingAmmo = RemainingAmmo;
}

bool AMedComBasePickupItem::TryAddToInventory(AActor* InstigatorActor)
{
    if (!HasAuthority() || !InstigatorActor || !bDataCached)
    {
        UE_LOG(LogMedComInteraction, Warning, 
            TEXT("TryAddToInventory: Basic validation failed - HasAuth:%d, Actor:%s, DataCached:%d"), 
            HasAuthority(), *GetNameSafe(InstigatorActor), bDataCached);
        return false;
    }
    
    // Детальная диагностика типа предмета
    UE_LOG(LogMedComInteraction, Log, 
        TEXT("TryAddToInventory: Processing item - ID:%s, Type:%s, Quantity:%d"), 
        *ItemID.ToString(), 
        *CachedItemData.ItemType.ToString(), 
        Amount);
    
    // Проверяем, что тип предмета в правильной иерархии
    static const FGameplayTag BaseItemTag = FGameplayTag::RequestGameplayTag(TEXT("Item"));
    if (!CachedItemData.ItemType.MatchesTag(BaseItemTag))
    {
        UE_LOG(LogMedComInteraction, Error, 
            TEXT("TryAddToInventory: Item type %s is not in Item.* hierarchy!"), 
            *CachedItemData.ItemType.ToString());
        return false;
    }
    
    // Create item instance
    FInventoryItemInstance ItemInstance;
    if (!CreateItemInstance_Implementation(ItemInstance))
    {
        UE_LOG(LogMedComInteraction, Warning, TEXT("TryAddToInventory: Failed to create item instance"));
        return false;
    }
    
    // Find inventory component
    UObject* InventoryComponent = UMedComStaticHelpers::FindInventoryComponent(InstigatorActor);
    if (!InventoryComponent)
    {
        UE_LOG(LogMedComInteraction, Warning, TEXT("TryAddToInventory: No inventory component found"));
        return false;
    }
    
    // Check interface implementation
    if (!InventoryComponent->GetClass()->ImplementsInterface(UMedComInventoryInterface::StaticClass()))
    {
        UE_LOG(LogMedComInteraction, Warning, TEXT("TryAddToInventory: Inventory doesn't implement interface"));
        return false;
    }
    
    // Используем ТОЛЬКО интерфейсные методы для проверок
    
    // 1. Проверяем, может ли инвентарь принять этот предмет через интерфейс
    bool bCanReceive = IMedComInventoryInterface::Execute_CanReceiveItem(
        InventoryComponent, 
        CachedItemData, 
        Amount
    );
    
    if (!bCanReceive)
    {
        UE_LOG(LogMedComInteraction, Warning, 
            TEXT("TryAddToInventory: Inventory cannot receive item (CanReceiveItem returned false)"));
        
        // Дополнительная диагностика через интерфейсные методы
        
        // Проверяем разрешенные типы
        FGameplayTagContainer AllowedTypes = IMedComInventoryInterface::Execute_GetAllowedItemTypes(InventoryComponent);
        if (!AllowedTypes.IsEmpty())
        {
            UE_LOG(LogMedComInteraction, Warning, 
                TEXT("  - Inventory has type restrictions (%d allowed types)"), 
                AllowedTypes.Num());
            
            bool bTypeAllowed = AllowedTypes.HasTag(CachedItemData.ItemType);
            UE_LOG(LogMedComInteraction, Warning, 
                TEXT("  - Item type %s allowed: %s"), 
                *CachedItemData.ItemType.ToString(),
                bTypeAllowed ? TEXT("YES") : TEXT("NO"));
        }
        
        // Проверяем вес через интерфейс
        float CurrentWeight = IMedComInventoryInterface::Execute_GetCurrentWeight(InventoryComponent);
        float MaxWeight = IMedComInventoryInterface::Execute_GetMaxWeight(InventoryComponent);
        float RequiredWeight = CachedItemData.Weight * Amount;
        
        UE_LOG(LogMedComInteraction, Warning, 
            TEXT("  - Weight: Current=%.2f, Max=%.2f, Required=%.2f"), 
            CurrentWeight, MaxWeight, RequiredWeight);
        
        if (CurrentWeight + RequiredWeight > MaxWeight)
        {
            UE_LOG(LogMedComInteraction, Warning, TEXT("  - Would exceed weight limit"));
            
            IMedComInventoryInterface::BroadcastInventoryError(
                InventoryComponent, 
                EInventoryErrorCode::WeightLimit,
                TEXT("Weight limit exceeded")
            );
        }
        else
        {
            // Если не вес, то возможно нет места или тип не разрешен
            IMedComInventoryInterface::BroadcastInventoryError(
                InventoryComponent, 
                EInventoryErrorCode::NoSpace,
                TEXT("Cannot add item to inventory")
            );
        }
        
        return false;
    }
    
    // Try to add using the interface method
    UE_LOG(LogMedComInteraction, Log, TEXT("TryAddToInventory: Adding item through interface..."));
    
    bool bAdded = IMedComInventoryInterface::Execute_AddItemByID(InventoryComponent, ItemID, Amount);
    
    if (bAdded)
    {
        UE_LOG(LogMedComInteraction, Log, TEXT("Successfully added %s to inventory"), *ItemID.ToString());
        
        // Broadcast success event
        IMedComInventoryInterface::BroadcastItemAdded(
            InventoryComponent, 
            ItemInstance,
            INDEX_NONE
        );
    }
    else
    {
        UE_LOG(LogMedComInteraction, Warning, TEXT("Failed to add %s to inventory"), *ItemID.ToString());
        
        // Broadcast generic error since we don't have specific reason
        IMedComInventoryInterface::BroadcastInventoryError(
            InventoryComponent, 
            EInventoryErrorCode::NoSpace,
            TEXT("Pickup failed")
        );
    }
    
    return bAdded;
}

UMedComItemManager* AMedComBasePickupItem::GetItemManager() const
{
    UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(GetWorld());
    if (GameInstance)
    {
        return GameInstance->GetSubsystem<UMedComItemManager>();
    }
    
    return nullptr;
}

void AMedComBasePickupItem::BroadcastPickupSpawned()
{
    UEventDelegateManager* Manager = GetDelegateManager();
    if (Manager)
    {
        FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TEXT("Pickup.Event.Spawned"));
        FString EventData = FString::Printf(TEXT("ItemID:%s,Amount:%d,Location:%s"), 
            *ItemID.ToString(),
            Amount,
            *GetActorLocation().ToString());
            
        Manager->NotifyEquipmentEvent(this, EventTag, EventData);
    }
}

void AMedComBasePickupItem::BroadcastPickupCollected(AActor* Collector)
{
    UEventDelegateManager* Manager = GetDelegateManager();
    if (Manager && Collector)
    {
        FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TEXT("Pickup.Event.Collected"));
        FString EventData = FString::Printf(TEXT("ItemID:%s,Amount:%d,Collector:%s"), 
            *ItemID.ToString(),
            Amount,
            *Collector->GetName());
            
        Manager->NotifyEquipmentEvent(this, EventTag, EventData);
    }
}

void AMedComBasePickupItem::HandleInteractionFeedback(bool bGainedFocus)
{
    // Visual feedback can be implemented here
    // For example: outline effect, glow, etc.
}

float AMedComBasePickupItem::GetPresetProperty(FName PropertyName, float DefaultValue) const
{
    const FPresetPropertyPair* Found = FindPresetProperty(PropertyName);
    return Found ? Found->PropertyValue : DefaultValue;
}

void AMedComBasePickupItem::SetPresetProperty(FName PropertyName, float Value)
{
    if (!HasAuthority())
    {
        UE_LOG(LogMedComInteraction, Warning, 
            TEXT("SetPresetProperty called on client for %s"), *GetName());
        return;
    }

    FPresetPropertyPair* Found = FindPresetProperty(PropertyName);
    if (Found)
    {
        // Обновляем существующее значение
        Found->PropertyValue = Value;
    }
    else
    {
        // Добавляем новое свойство
        PresetRuntimeProperties.Add(FPresetPropertyPair(PropertyName, Value));
    }
}

bool AMedComBasePickupItem::HasPresetProperty(FName PropertyName) const
{
    return FindPresetProperty(PropertyName) != nullptr;
}

bool AMedComBasePickupItem::RemovePresetProperty(FName PropertyName)
{
    if (!HasAuthority())
    {
        return false;
    }

    // Ищем и удаляем свойство
    const int32 RemovedCount = PresetRuntimeProperties.RemoveAll(
        [PropertyName](const FPresetPropertyPair& Pair)
        {
            return Pair.PropertyName == PropertyName;
        });

    return RemovedCount > 0;
}

TMap<FName, float> AMedComBasePickupItem::GetPresetPropertiesAsMap() const
{
    TMap<FName, float> ResultMap;
    
    // Конвертируем массив в map для удобства использования
    for (const FPresetPropertyPair& Pair : PresetRuntimeProperties)
    {
        ResultMap.Add(Pair.PropertyName, Pair.PropertyValue);
    }
    
    return ResultMap;
}

void AMedComBasePickupItem::SetPresetPropertiesFromMap(const TMap<FName, float>& NewProperties)
{
    if (!HasAuthority())
    {
        return;
    }

    // Очищаем существующие свойства
    PresetRuntimeProperties.Empty(NewProperties.Num());
    
    // Добавляем новые из map
    for (const auto& PropertyPair : NewProperties)
    {
        PresetRuntimeProperties.Add(
            FPresetPropertyPair(PropertyPair.Key, PropertyPair.Value)
        );
    }
}

// Приватные вспомогательные методы
FPresetPropertyPair* AMedComBasePickupItem::FindPresetProperty(FName PropertyName)
{
    return PresetRuntimeProperties.FindByPredicate(
        [PropertyName](const FPresetPropertyPair& Pair)
        {
            return Pair.PropertyName == PropertyName;
        });
}

const FPresetPropertyPair* AMedComBasePickupItem::FindPresetProperty(FName PropertyName) const
{
    return PresetRuntimeProperties.FindByPredicate(
        [PropertyName](const FPresetPropertyPair& Pair)
        {
            return Pair.PropertyName == PropertyName;
        });
}