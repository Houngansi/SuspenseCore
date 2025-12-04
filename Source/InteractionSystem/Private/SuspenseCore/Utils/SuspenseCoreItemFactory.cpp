// SuspenseCoreItemFactory.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Utils/SuspenseCoreItemFactory.h"
#include "SuspenseCore/Utils/SuspenseCoreHelpers.h"
#include "SuspenseCore/Pickup/SuspenseCorePickupItem.h"
#include "SuspenseCore/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "ItemSystem/SuspenseItemManager.h"
#include "Interfaces/Interaction/ISuspensePickup.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreFactory, Log, All);

//==================================================================
// USubsystem Interface
//==================================================================

void USuspenseCoreItemFactory::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Cache references
	CachedEventBus = GetEventBus();
	CachedItemManager = GetItemManager();

	// Set default pickup class if not set
	if (!DefaultPickupClass)
	{
		DefaultPickupClass = ASuspenseCorePickupItem::StaticClass();
	}

	TotalPickupsCreated = 0;

	UE_LOG(LogSuspenseCoreFactory, Log, TEXT("USuspenseCoreItemFactory: Initialized with default class %s"),
		*GetNameSafe(DefaultPickupClass));
}

void USuspenseCoreItemFactory::Deinitialize()
{
	// Clear cached references
	CachedEventBus.Reset();
	CachedItemManager.Reset();

	UE_LOG(LogSuspenseCoreFactory, Log, TEXT("USuspenseCoreItemFactory: Deinitialized. Total pickups created: %d"),
		TotalPickupsCreated);

	Super::Deinitialize();
}

//==================================================================
// ISuspenseCoreEventEmitter Interface
//==================================================================

void USuspenseCoreItemFactory::EmitEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& Data)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (EventBus && EventTag.IsValid())
	{
		EventBus->Publish(EventTag, Data);
	}
}

USuspenseCoreEventBus* USuspenseCoreItemFactory::GetEventBus() const
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return nullptr;
	}

	USuspenseCoreEventBus* EventBus = GameInstance->GetSubsystem<USuspenseCoreEventBus>();
	if (EventBus)
	{
		CachedEventBus = EventBus;
	}

	return EventBus;
}

//==================================================================
// ISuspenseItemFactoryInterface Implementation
//==================================================================

AActor* USuspenseCoreItemFactory::CreatePickupFromItemID_Implementation(
	FName ItemID,
	UWorld* World,
	const FTransform& Transform,
	int32 Quantity)
{
	if (ItemID.IsNone() || !World)
	{
		UE_LOG(LogSuspenseCoreFactory, Warning, TEXT("CreatePickupFromItemID: Invalid parameters"));
		BroadcastSpawnFailed(ItemID, TEXT("Invalid parameters"));
		return nullptr;
	}

	// Get item data
	USuspenseItemManager* ItemManager = GetItemManager();
	if (!ItemManager)
	{
		UE_LOG(LogSuspenseCoreFactory, Error, TEXT("CreatePickupFromItemID: ItemManager not found"));
		BroadcastSpawnFailed(ItemID, TEXT("ItemManager not found"));
		return nullptr;
	}

	FSuspenseUnifiedItemData ItemData;
	if (!ItemManager->GetUnifiedItemData(ItemID, ItemData))
	{
		UE_LOG(LogSuspenseCoreFactory, Warning, TEXT("CreatePickupFromItemID: Item '%s' not found in DataTable"),
			*ItemID.ToString());
		BroadcastSpawnFailed(ItemID, TEXT("Item not found in DataTable"));
		return nullptr;
	}

	// Determine pickup class
	TSubclassOf<AActor> PickupClass = DefaultPickupClass;

	// Could override class based on item type here if needed
	// For example: if (ItemData.bIsWeapon) PickupClass = WeaponPickupClass;

	if (!PickupClass)
	{
		UE_LOG(LogSuspenseCoreFactory, Error, TEXT("CreatePickupFromItemID: No pickup class set"));
		BroadcastSpawnFailed(ItemID, TEXT("No pickup class configured"));
		return nullptr;
	}

	// Spawn actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* PickupActor = World->SpawnActor<AActor>(PickupClass, Transform, SpawnParams);
	if (!PickupActor)
	{
		UE_LOG(LogSuspenseCoreFactory, Error, TEXT("CreatePickupFromItemID: Failed to spawn pickup actor"));
		BroadcastSpawnFailed(ItemID, TEXT("Failed to spawn actor"));
		return nullptr;
	}

	// Configure pickup
	ConfigurePickup(PickupActor, ItemData, Quantity);

	// Update statistics
	TotalPickupsCreated++;

	// Broadcast creation event
	BroadcastItemCreated(PickupActor, ItemID, Quantity, Transform);

	UE_LOG(LogSuspenseCoreFactory, Log, TEXT("CreatePickupFromItemID: Created pickup for %s x%d"),
		*ItemID.ToString(), Quantity);

	return PickupActor;
}

TSubclassOf<AActor> USuspenseCoreItemFactory::GetDefaultPickupClass_Implementation() const
{
	return DefaultPickupClass;
}

//==================================================================
// Extended Factory Methods
//==================================================================

AActor* USuspenseCoreItemFactory::CreatePickupWithAmmo(
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
	USuspenseItemManager* ItemManager = GetItemManager();
	if (!ItemManager)
	{
		return PickupActor;
	}

	FSuspenseUnifiedItemData ItemData;
	if (!ItemManager->GetUnifiedItemData(ItemID, ItemData))
	{
		return PickupActor;
	}

	// Configure weapon-specific properties
	if (ItemData.bIsWeapon && bWithAmmoState)
	{
		ConfigureWeaponPickup(PickupActor, ItemData, bWithAmmoState, CurrentAmmo, RemainingAmmo);
	}

	return PickupActor;
}

AActor* USuspenseCoreItemFactory::CreatePickupWithProperties(
	FName ItemID,
	UWorld* World,
	const FTransform& Transform,
	int32 Quantity,
	const TMap<FName, float>& PresetProperties)
{
	// Create basic pickup
	AActor* PickupActor = CreatePickupFromItemID_Implementation(ItemID, World, Transform, Quantity);
	if (!PickupActor)
	{
		return nullptr;
	}

	// Apply preset properties
	if (PresetProperties.Num() > 0)
	{
		ApplyPresetProperties(PickupActor, PresetProperties);
	}

	return PickupActor;
}

void USuspenseCoreItemFactory::SetDefaultPickupClass(TSubclassOf<AActor> NewDefaultClass)
{
	DefaultPickupClass = NewDefaultClass;

	UE_LOG(LogSuspenseCoreFactory, Log, TEXT("SetDefaultPickupClass: Changed to %s"),
		*GetNameSafe(NewDefaultClass));
}

//==================================================================
// Internal Methods
//==================================================================

USuspenseItemManager* USuspenseCoreItemFactory::GetItemManager() const
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

	USuspenseItemManager* ItemManager = GameInstance->GetSubsystem<USuspenseItemManager>();
	if (ItemManager)
	{
		const_cast<USuspenseCoreItemFactory*>(this)->CachedItemManager = ItemManager;
	}

	return ItemManager;
}

void USuspenseCoreItemFactory::ConfigurePickup(
	AActor* PickupActor,
	const FSuspenseUnifiedItemData& ItemData,
	int32 Quantity)
{
	if (!PickupActor)
	{
		return;
	}

	// Configure through pickup interface
	if (PickupActor->GetClass()->ImplementsInterface(USuspensePickup::StaticClass()))
	{
		// Set item ID and quantity
		ISuspensePickup::Execute_SetItemID(PickupActor, ItemData.ItemID);
		ISuspensePickup::Execute_SetAmount(PickupActor, Quantity);

		// If it's our SuspenseCorePickupItem, we can set more properties
		ASuspenseCorePickupItem* CorePickup = Cast<ASuspenseCorePickupItem>(PickupActor);
		if (CorePickup)
		{
			// The pickup will load its data from ItemManager using ItemID
			// Additional configuration can be done here if needed
		}
	}
	else
	{
		UE_LOG(LogSuspenseCoreFactory, Warning,
			TEXT("ConfigurePickup: Actor doesn't implement pickup interface"));
	}
}

void USuspenseCoreItemFactory::ConfigureWeaponPickup(
	AActor* PickupActor,
	const FSuspenseUnifiedItemData& ItemData,
	bool bWithAmmoState,
	float CurrentAmmo,
	float RemainingAmmo)
{
	if (!PickupActor || !ItemData.bIsWeapon)
	{
		return;
	}

	// Configure ammo state through interface
	if (PickupActor->GetClass()->ImplementsInterface(USuspensePickup::StaticClass()))
	{
		if (bWithAmmoState)
		{
			ISuspensePickup::Execute_SetSavedAmmoState(PickupActor, CurrentAmmo, RemainingAmmo);
		}
	}

	// Direct access for our pickup class
	ASuspenseCorePickupItem* CorePickup = Cast<ASuspenseCorePickupItem>(PickupActor);
	if (CorePickup && bWithAmmoState)
	{
		CorePickup->SetAmmoState(true, CurrentAmmo, RemainingAmmo);
	}
}

void USuspenseCoreItemFactory::ApplyPresetProperties(
	AActor* PickupActor,
	const TMap<FName, float>& PresetProperties)
{
	ASuspenseCorePickupItem* CorePickup = Cast<ASuspenseCorePickupItem>(PickupActor);
	if (CorePickup)
	{
		CorePickup->SetPresetPropertiesFromMap(PresetProperties);
	}
}

//==================================================================
// EventBus Broadcasting
//==================================================================

void USuspenseCoreItemFactory::BroadcastItemCreated(
	AActor* CreatedActor,
	FName ItemID,
	int32 Quantity,
	const FTransform& Transform)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus || !CreatedActor)
	{
		return;
	}

	// Create event data
	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(
		this,
		ESuspenseCoreEventPriority::Normal
	);

	EventData.SetString(TEXT("ItemID"), ItemID.ToString());
	EventData.SetInt(TEXT("Quantity"), Quantity);
	EventData.SetObject(TEXT("PickupActor"), CreatedActor);
	EventData.SetVector(TEXT("Location"), Transform.GetLocation());

	// Broadcast creation event
	static const FGameplayTag ItemCreatedTag =
		FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Factory.ItemCreated"));

	EmitEvent(ItemCreatedTag, EventData);

	UE_LOG(LogSuspenseCoreFactory, Log,
		TEXT("Broadcast ItemCreated: ItemID=%s, Quantity=%d, Location=%s"),
		*ItemID.ToString(), Quantity, *Transform.GetLocation().ToString());
}

void USuspenseCoreItemFactory::BroadcastSpawnFailed(
	FName ItemID,
	const FString& Reason)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	// Create event data
	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(
		this,
		ESuspenseCoreEventPriority::High
	);

	EventData.SetString(TEXT("ItemID"), ItemID.ToString());
	EventData.SetString(TEXT("Reason"), Reason);

	// Broadcast failure event
	static const FGameplayTag SpawnFailedTag =
		FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Factory.SpawnFailed"));

	EmitEvent(SpawnFailedTag, EventData);

	UE_LOG(LogSuspenseCoreFactory, Warning,
		TEXT("Broadcast SpawnFailed: ItemID=%s, Reason=%s"),
		*ItemID.ToString(), *Reason);
}
