// Copyright MedCom Team. All Rights Reserved.

#include "Utils/MedComItemFactory.h"
#include "ItemSystem/MedComItemManager.h"
#include "Pickup/MedComBasePickupItem.h"
#include "Interfaces/Interaction/IMedComPickupInterface.h"
#include "Delegates/EventDelegateManager.h"
#include "Engine/World.h"

void UMedComItemFactory::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    // Cache references
    CachedDelegateManager = GetDelegateManager();
    CachedItemManager = GetItemManager();
    
    // Set default pickup class if not set
    if (!DefaultPickupClass)
    {
        DefaultPickupClass = AMedComBasePickupItem::StaticClass();
    }
    
    UE_LOG(LogTemp, Log, TEXT("UMedComItemFactory: Initialized with default class %s"), 
        *DefaultPickupClass->GetName());
}

void UMedComItemFactory::Deinitialize()
{
    // Clear cached references
    CachedDelegateManager.Reset();
    CachedItemManager.Reset();
    
    Super::Deinitialize();
}

AActor* UMedComItemFactory::CreatePickupFromItemID_Implementation(
    FName ItemID,
    UWorld* World,
    const FTransform& Transform,
    int32 Quantity)
{
    if (ItemID.IsNone() || !World)
    {
        UE_LOG(LogTemp, Warning, TEXT("CreatePickupFromItemID: Invalid parameters"));
        return nullptr;
    }
    
    // Get item data
    UMedComItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        UE_LOG(LogTemp, Error, TEXT("CreatePickupFromItemID: ItemManager not found"));
        return nullptr;
    }
    
    FMedComUnifiedItemData ItemData;
    if (!ItemManager->GetUnifiedItemData(ItemID, ItemData))
    {
        UE_LOG(LogTemp, Warning, TEXT("CreatePickupFromItemID: Item '%s' not found in DataTable"), 
            *ItemID.ToString());
        return nullptr;
    }
    
    // Determine pickup class
    TSubclassOf<AActor> PickupClass = DefaultPickupClass;
    
    // Could override class based on item type here if needed
    // For example: if (ItemData.bIsWeapon) PickupClass = WeaponPickupClass;
    
    if (!PickupClass)
    {
        UE_LOG(LogTemp, Error, TEXT("CreatePickupFromItemID: No pickup class set"));
        return nullptr;
    }
    
    // Spawn actor
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    
    AActor* PickupActor = World->SpawnActor<AActor>(PickupClass, Transform, SpawnParams);
    if (!PickupActor)
    {
        UE_LOG(LogTemp, Error, TEXT("CreatePickupFromItemID: Failed to spawn pickup actor"));
        return nullptr;
    }
    
    // Configure pickup
    ConfigurePickup(PickupActor, ItemData, Quantity);
    
    // Broadcast creation event
    BroadcastItemCreated(PickupActor, ItemID, Quantity);
    
    UE_LOG(LogTemp, Log, TEXT("CreatePickupFromItemID: Created pickup for %s x%d"), 
        *ItemID.ToString(), Quantity);
    
    return PickupActor;
}

AActor* UMedComItemFactory::CreatePickupWithAmmo(
    FName ItemID,
    UWorld* World,
    const FTransform& Transform,
    int32 Quantity,
    bool bWithAmmoState,
    float CurrentAmmo,
    float RemainingAmmo)
{
    // Create basic pickup
    AActor* PickupActor = CreatePickupFromItemID_Implementation(ItemID, World, Transform, Quantity);
    if (!PickupActor)
    {
        return nullptr;
    }
    
    // Get item data to check if weapon
    UMedComItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        return PickupActor;
    }
    
    FMedComUnifiedItemData ItemData;
    if (!ItemManager->GetUnifiedItemData(ItemID, ItemData))
    {
        return PickupActor;
    }
    
    // Configure weapon-specific properties
    if (ItemData.bIsWeapon)
    {
        ConfigureWeaponPickup(PickupActor, ItemData, bWithAmmoState, CurrentAmmo, RemainingAmmo);
    }
    
    return PickupActor;
}

TSubclassOf<AActor> UMedComItemFactory::GetDefaultPickupClass_Implementation() const
{
    return DefaultPickupClass;
}

void UMedComItemFactory::SetDefaultPickupClass(TSubclassOf<AActor> NewDefaultClass)
{
    DefaultPickupClass = NewDefaultClass;
}

UEventDelegateManager* UMedComItemFactory::GetDelegateManager() const
{
    if (CachedDelegateManager.IsValid())
    {
        return CachedDelegateManager.Get();
    }
    
    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance)
    {
        return nullptr;
    }
    
    return GameInstance->GetSubsystem<UEventDelegateManager>();
}

UMedComItemManager* UMedComItemFactory::GetItemManager() const
{
    if (CachedItemManager.IsValid())
    {
        return CachedItemManager.Get();
    }
    
    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance)
    {
        return nullptr;
    }
    
    return GameInstance->GetSubsystem<UMedComItemManager>();
}

void UMedComItemFactory::ConfigurePickup(AActor* PickupActor, const FMedComUnifiedItemData& ItemData, int32 Quantity)
{
    if (!PickupActor)
    {
        return;
    }
    
    // Configure through pickup interface
    if (PickupActor->GetClass()->ImplementsInterface(UMedComPickupInterface::StaticClass()))
    {
        // Set item ID and quantity
        IMedComPickupInterface::Execute_SetItemID(PickupActor, ItemData.ItemID);
        IMedComPickupInterface::Execute_SetAmount(PickupActor, Quantity);
        
        // If it's MedComBasePickupItem, we can set more properties
        AMedComBasePickupItem* BasePickup = Cast<AMedComBasePickupItem>(PickupActor);
        if (BasePickup)
        {
            // The pickup will load its data from ItemManager using ItemID
            // Additional configuration can be done here if needed
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("ConfigurePickup: Actor doesn't implement pickup interface"));
    }
}

void UMedComItemFactory::ConfigureWeaponPickup(AActor* PickupActor, const FMedComUnifiedItemData& ItemData, 
    bool bWithAmmoState, float CurrentAmmo, float RemainingAmmo)
{
    if (!PickupActor || !ItemData.bIsWeapon)
    {
        return;
    }
    
    // Configure ammo state through interface
    if (PickupActor->GetClass()->ImplementsInterface(UMedComPickupInterface::StaticClass()))
    {
        if (bWithAmmoState)
        {
            IMedComPickupInterface::Execute_SetSavedAmmoState(PickupActor, CurrentAmmo, RemainingAmmo);
        }
    }
    
    // Additional weapon configuration
    AMedComBasePickupItem* BasePickup = Cast<AMedComBasePickupItem>(PickupActor);
    if (BasePickup)
    {
        // Direct access for weapon setup
        if (bWithAmmoState)
        {
            BasePickup->SetAmmoState(true, CurrentAmmo, RemainingAmmo);
        }
    }
}

void UMedComItemFactory::BroadcastItemCreated(AActor* CreatedActor, FName ItemID, int32 Quantity)
{
    UEventDelegateManager* Manager = GetDelegateManager();
    if (Manager && CreatedActor)
    {
        FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TEXT("Factory.Event.ItemCreated"));
        FString EventData = FString::Printf(TEXT("ItemID:%s,Quantity:%d,Location:%s"), 
            *ItemID.ToString(),
            Quantity,
            *CreatedActor->GetActorLocation().ToString());
            
        Manager->NotifyEquipmentEvent(this, EventTag, EventData);
    }
}