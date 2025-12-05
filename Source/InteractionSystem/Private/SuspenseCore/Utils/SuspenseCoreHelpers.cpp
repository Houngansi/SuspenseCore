// SuspenseCoreHelpers.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Utils/SuspenseCoreHelpers.h"
#include "SuspenseCore/Services/SuspenseCoreServiceProvider.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Types/Items/SuspenseCoreItemTypes.h"
#include "SuspenseCore/SuspenseCoreInterfaces.h"
#include "SuspenseCore/Interfaces/Inventory/ISuspenseCoreInventory.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Components/ActorComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

// Log category definition
DEFINE_LOG_CATEGORY(LogSuspenseCoreInteraction);

//==================================================================
// ServiceProvider Access Implementation (NEW - Preferred)
//==================================================================

USuspenseCoreServiceProvider* USuspenseCoreHelpers::GetServiceProvider(const UObject* WorldContextObject)
{
	return USuspenseCoreServiceProvider::Get(WorldContextObject);
}

//==================================================================
// EventBus Access Implementation
//==================================================================

USuspenseCoreEventBus* USuspenseCoreHelpers::GetEventBus(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	// Prefer ServiceProvider for centralized access
	if (USuspenseCoreServiceProvider* Provider = GetServiceProvider(WorldContextObject))
	{
		return Provider->GetEventBus();
	}

	// Fallback to EventManager (for backwards compatibility during migration)
	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(WorldContextObject);
	if (!Manager)
	{
		return nullptr;
	}

	return Manager->GetEventBus();
}

bool USuspenseCoreHelpers::BroadcastSimpleEvent(
	const UObject* WorldContextObject,
	FGameplayTag EventTag,
	UObject* Source)
{
	USuspenseCoreEventBus* EventBus = GetEventBus(WorldContextObject);
	if (!EventBus || !EventTag.IsValid())
	{
		return false;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(Source);
	EventBus->Publish(EventTag, EventData);

	return true;
}

//==================================================================
// Component Discovery Implementation
//==================================================================

UObject* USuspenseCoreHelpers::FindInventoryComponent(AActor* Actor)
{
	if (!Actor)
	{
		UE_LOG(LogSuspenseCoreInteraction, Warning, TEXT("FindInventoryComponent: Actor is null"));
		return nullptr;
	}

	// Find PlayerState first
	APlayerState* PS = FindPlayerState(Actor);
	if (!PS)
	{
		UE_LOG(LogSuspenseCoreInteraction, Warning,
			TEXT("FindInventoryComponent: PlayerState not found for actor %s"),
			*Actor->GetName());
		return nullptr;
	}

	// Check all components in PlayerState
	for (UActorComponent* Component : PS->GetComponents())
	{
		if (Component && ImplementsInventoryInterface(Component))
		{
			UE_LOG(LogSuspenseCoreInteraction, Log,
				TEXT("FindInventoryComponent: Found inventory component %s in PlayerState"),
				*Component->GetName());
			return Component;
		}
	}

	// If not found in PlayerState, check the actor itself
	if (Actor != PS)
	{
		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (Component && ImplementsInventoryInterface(Component))
			{
				UE_LOG(LogSuspenseCoreInteraction, Log,
					TEXT("FindInventoryComponent: Found inventory component %s in Actor"),
					*Component->GetName());
				return Component;
			}
		}

		// Check controller if actor is a character
		if (ACharacter* Character = Cast<ACharacter>(Actor))
		{
			if (AController* Controller = Character->GetController())
			{
				for (UActorComponent* Component : Controller->GetComponents())
				{
					if (Component && ImplementsInventoryInterface(Component))
					{
						UE_LOG(LogSuspenseCoreInteraction, Log,
							TEXT("FindInventoryComponent: Found inventory component %s in Controller"),
							*Component->GetName());
						return Component;
					}
				}
			}
		}
	}

	UE_LOG(LogSuspenseCoreInteraction, Warning,
		TEXT("FindInventoryComponent: No inventory component found for actor %s"),
		*Actor->GetName());
	return nullptr;
}

APlayerState* USuspenseCoreHelpers::FindPlayerState(AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	// Direct cast if actor is PlayerState
	if (APlayerState* PS = Cast<APlayerState>(Actor))
	{
		return PS;
	}

	// Check if actor is controller
	if (AController* Controller = Cast<AController>(Actor))
	{
		if (APlayerController* PC = Cast<APlayerController>(Controller))
		{
			return PC->PlayerState;
		}
	}

	// Check if actor is pawn
	if (APawn* Pawn = Cast<APawn>(Actor))
	{
		if (AController* Controller = Pawn->GetController())
		{
			if (APlayerController* PC = Cast<APlayerController>(Controller))
			{
				return PC->PlayerState;
			}
		}
	}

	// Check InstigatorController
	if (AController* Controller = Actor->GetInstigatorController())
	{
		if (APlayerController* PC = Cast<APlayerController>(Controller))
		{
			return PC->PlayerState;
		}
	}

	return nullptr;
}

bool USuspenseCoreHelpers::ImplementsInventoryInterface(UObject* Object)
{
	if (!Object)
	{
		return false;
	}

	// Check for ISuspenseCoreInventory interface
	return Object->GetClass()->ImplementsInterface(USuspenseCoreInventory::StaticClass());
}

//==================================================================
// Item Operations Implementation
//==================================================================

bool USuspenseCoreHelpers::AddItemToInventoryByID(
	UObject* InventoryComponent,
	FName ItemID,
	int32 Quantity)
{
	if (!InventoryComponent)
	{
		UE_LOG(LogSuspenseCoreInteraction, Warning,
			TEXT("AddItemToInventoryByID: Invalid inventory component"));
		return false;
	}

	if (ItemID.IsNone() || Quantity <= 0)
	{
		UE_LOG(LogSuspenseCoreInteraction, Warning,
			TEXT("AddItemToInventoryByID: Invalid ItemID or Quantity"));
		return false;
	}

	// Check for ISuspenseCoreInventory interface
	if (!ImplementsInventoryInterface(InventoryComponent))
	{
		UE_LOG(LogSuspenseCoreInteraction, Warning,
			TEXT("AddItemToInventoryByID: Component doesn't implement ISuspenseCoreInventory"));
		return false;
	}

	// Call interface method directly
	bool bSuccess = ISuspenseCoreInventory::Execute_AddItemByID(InventoryComponent, ItemID, Quantity);

	if (bSuccess)
	{
		UE_LOG(LogSuspenseCoreInteraction, Log,
			TEXT("AddItemToInventoryByID: Successfully added %s x%d"),
			*ItemID.ToString(), Quantity);
	}
	else
	{
		UE_LOG(LogSuspenseCoreInteraction, Warning,
			TEXT("AddItemToInventoryByID: Failed to add %s x%d"),
			*ItemID.ToString(), Quantity);
	}

	return bSuccess;
}

bool USuspenseCoreHelpers::AddItemInstanceToInventory(
	UObject* InventoryComponent,
	const FSuspenseCoreItemInstance& ItemInstance)
{
	if (!InventoryComponent)
	{
		UE_LOG(LogSuspenseCoreInteraction, Warning,
			TEXT("AddItemInstanceToInventory: Invalid inventory component"));
		return false;
	}

	if (!ItemInstance.IsValid())
	{
		UE_LOG(LogSuspenseCoreInteraction, Warning,
			TEXT("AddItemInstanceToInventory: Invalid item instance"));
		return false;
	}

	// Check for ISuspenseCoreInventory interface
	if (!ImplementsInventoryInterface(InventoryComponent))
	{
		UE_LOG(LogSuspenseCoreInteraction, Warning,
			TEXT("AddItemInstanceToInventory: Component doesn't implement ISuspenseCoreInventory"));
		return false;
	}

	// Call interface method directly
	bool bSuccess = ISuspenseCoreInventory::Execute_AddItemInstance(InventoryComponent, ItemInstance);

	if (bSuccess)
	{
		UE_LOG(LogSuspenseCoreInteraction, Log,
			TEXT("AddItemInstanceToInventory: Successfully added instance %s"),
			*ItemInstance.ItemID.ToString());
	}
	else
	{
		UE_LOG(LogSuspenseCoreInteraction, Warning,
			TEXT("AddItemInstanceToInventory: Failed to add instance %s"),
			*ItemInstance.ItemID.ToString());
	}

	return bSuccess;
}

bool USuspenseCoreHelpers::CanActorPickupItem(AActor* Actor, FName ItemID, int32 Quantity)
{
	if (!Actor || ItemID.IsNone() || Quantity <= 0)
	{
		UE_LOG(LogSuspenseCoreInteraction, Warning,
			TEXT("CanActorPickupItem: Invalid parameters - Actor:%s, ItemID:%s, Quantity:%d"),
			*GetNameSafe(Actor), *ItemID.ToString(), Quantity);
		return false;
	}

	// Get DataManager (SuspenseCore architecture)
	USuspenseCoreDataManager* DataManager = GetDataManager(Actor);
	if (!DataManager)
	{
		UE_LOG(LogSuspenseCoreInteraction, Warning,
			TEXT("CanActorPickupItem: DataManager not found"));
		BroadcastValidationFailed(Actor, Actor, ItemID, TEXT("DataManager not found"));
		return false;
	}

	// Get item data
	FSuspenseCoreItemData ItemData;
	if (!DataManager->GetItemData(ItemID, ItemData))
	{
		UE_LOG(LogSuspenseCoreInteraction, Warning,
			TEXT("CanActorPickupItem: Item %s not found in DataTable"),
			*ItemID.ToString());
		BroadcastValidationFailed(Actor, Actor, ItemID, TEXT("Item not found in DataTable"));
		return false;
	}

	// Validate item type hierarchy
	static const FGameplayTag BaseItemTag = FGameplayTag::RequestGameplayTag(TEXT("Item"));
	if (!ItemData.Classification.ItemType.MatchesTag(BaseItemTag))
	{
		UE_LOG(LogSuspenseCoreInteraction, Error,
			TEXT("CanActorPickupItem: Item type %s is not in Item.* hierarchy!"),
			*ItemData.Classification.ItemType.ToString());
		BroadcastValidationFailed(Actor, Actor, ItemID, TEXT("Invalid item type hierarchy"));
		return false;
	}

	// TODO: Implement inventory capacity check via ISuspenseCoreInventory when created
	// For now, validation passes if item exists in DataTable
	UE_LOG(LogSuspenseCoreInteraction, Log,
		TEXT("CanActorPickupItem: Item %s validated (inventory check pending ISuspenseCoreInventory)"),
		*ItemID.ToString());

	return true;
}

//==================================================================
// Item Instance Creation - SuspenseCore Native
//==================================================================

bool USuspenseCoreHelpers::CreateItemInstance(
	const UObject* WorldContextObject,
	FName ItemID,
	int32 Quantity,
	FSuspenseCoreItemInstance& OutInstance)
{
	if (ItemID.IsNone() || Quantity <= 0)
	{
		UE_LOG(LogSuspenseCoreInteraction, Warning,
			TEXT("CreateItemInstance: Invalid parameters"));
		return false;
	}

	USuspenseCoreDataManager* DataManager = GetDataManager(WorldContextObject);
	if (!DataManager)
	{
		UE_LOG(LogSuspenseCoreInteraction, Error,
			TEXT("CreateItemInstance: DataManager not found"));
		return false;
	}

	return DataManager->CreateItemInstance(ItemID, Quantity, OutInstance);
}

//==================================================================
// Item Information Implementation
//==================================================================

bool USuspenseCoreHelpers::GetItemData(
	const UObject* WorldContextObject,
	FName ItemID,
	FSuspenseCoreItemData& OutItemData)
{
	if (ItemID.IsNone())
	{
		return false;
	}

	// Use DataManager (SuspenseCore architecture)
	USuspenseCoreDataManager* DataManager = GetDataManager(WorldContextObject);
	if (!DataManager)
	{
		return false;
	}

	return DataManager->GetItemData(ItemID, OutItemData);
}

FText USuspenseCoreHelpers::GetItemDisplayName(const UObject* WorldContextObject, FName ItemID)
{
	FSuspenseCoreItemData ItemData;
	if (GetItemData(WorldContextObject, ItemID, ItemData))
	{
		return ItemData.Identity.DisplayName;
	}

	return FText::FromString(ItemID.ToString());
}

float USuspenseCoreHelpers::GetItemWeight(const UObject* WorldContextObject, FName ItemID)
{
	FSuspenseCoreItemData ItemData;
	if (GetItemData(WorldContextObject, ItemID, ItemData))
	{
		return ItemData.InventoryProps.Weight;
	}

	return 0.0f;
}

bool USuspenseCoreHelpers::IsItemStackable(const UObject* WorldContextObject, FName ItemID)
{
	FSuspenseCoreItemData ItemData;
	if (GetItemData(WorldContextObject, ItemID, ItemData))
	{
		return ItemData.InventoryProps.MaxStackSize > 1;
	}

	return false;
}

//==================================================================
// Subsystem Access Implementation
//==================================================================

USuspenseCoreDataManager* USuspenseCoreHelpers::GetDataManager(const UObject* WorldContextObject)
{
	// Prefer ServiceProvider for centralized access
	if (USuspenseCoreServiceProvider* Provider = GetServiceProvider(WorldContextObject))
	{
		return Provider->GetDataManager();
	}

	// Fallback to direct access (for backwards compatibility)
	return USuspenseCoreDataManager::Get(WorldContextObject);
}

//==================================================================
// Inventory Validation Implementation
//==================================================================

bool USuspenseCoreHelpers::ValidateInventorySpace(
	UObject* InventoryComponent,
	FName ItemID,
	int32 Quantity,
	FString& OutErrorMessage)
{
	OutErrorMessage.Empty();

	if (!InventoryComponent)
	{
		OutErrorMessage = TEXT("Invalid inventory component");
		return false;
	}

	// Get item data to validate item exists
	FSuspenseCoreItemData ItemData;
	AActor* Owner = Cast<AActor>(InventoryComponent->GetOuter());
	if (!GetItemData(Owner, ItemID, ItemData))
	{
		OutErrorMessage = FString::Printf(TEXT("Item %s not found"), *ItemID.ToString());
		return false;
	}

	// TODO: Implement inventory space check via ISuspenseCoreInventory when created
	// For now, validation passes if item exists
	UE_LOG(LogSuspenseCoreInteraction, Log,
		TEXT("ValidateInventorySpace: Item %s exists (space check pending ISuspenseCoreInventory)"),
		*ItemID.ToString());

	return true;
}

bool USuspenseCoreHelpers::ValidateWeightCapacity(
	UObject* InventoryComponent,
	FName ItemID,
	int32 Quantity,
	float& OutRemainingCapacity)
{
	OutRemainingCapacity = 0.0f;

	if (!InventoryComponent)
	{
		return false;
	}

	// Get item weight
	AActor* Owner = Cast<AActor>(InventoryComponent->GetOuter());
	float ItemWeight = GetItemWeight(Owner, ItemID);
	float TotalWeight = ItemWeight * Quantity;

	// TODO: Implement weight capacity check via ISuspenseCoreInventory when created
	// For now, assume unlimited capacity
	OutRemainingCapacity = FLT_MAX;

	UE_LOG(LogSuspenseCoreInteraction, Log,
		TEXT("ValidateWeightCapacity: Item %s weight=%.2f (capacity check pending ISuspenseCoreInventory)"),
		*ItemID.ToString(), TotalWeight);

	return true;
}

//==================================================================
// Utility Functions Implementation
//==================================================================

void USuspenseCoreHelpers::GetInventoryStatistics(
	UObject* InventoryComponent,
	int32& OutTotalItems,
	float& OutTotalWeight,
	int32& OutUsedSlots)
{
	OutTotalItems = 0;
	OutTotalWeight = 0.0f;
	OutUsedSlots = 0;

	if (!InventoryComponent)
	{
		UE_LOG(LogSuspenseCoreInteraction, Warning,
			TEXT("GetInventoryStatistics: Invalid inventory component"));
		return;
	}

	// TODO: Implement via ISuspenseCoreInventory when created
	// For now, return zeros and log warning
	UE_LOG(LogSuspenseCoreInteraction, Log,
		TEXT("GetInventoryStatistics: Pending ISuspenseCoreInventory implementation"));
}

void USuspenseCoreHelpers::LogInventoryContents(
	UObject* InventoryComponent,
	const FString& LogCategory)
{
	if (!InventoryComponent)
	{
		UE_LOG(LogSuspenseCoreInteraction, Warning,
			TEXT("LogInventoryContents: Invalid inventory component"));
		return;
	}

	// TODO: Implement via ISuspenseCoreInventory when created
	UE_LOG(LogSuspenseCoreInteraction, Log, TEXT("=== Inventory Contents (%s) ==="), *LogCategory);
	UE_LOG(LogSuspenseCoreInteraction, Log, TEXT("(Pending ISuspenseCoreInventory implementation)"));
	UE_LOG(LogSuspenseCoreInteraction, Log, TEXT("=== End Inventory Contents ==="));
}

//==================================================================
// EventBus Event Broadcasting Implementation
//==================================================================

void USuspenseCoreHelpers::BroadcastValidationFailed(
	const UObject* WorldContextObject,
	AActor* Actor,
	FName ItemID,
	const FString& Reason)
{
	USuspenseCoreEventBus* EventBus = GetEventBus(WorldContextObject);
	if (!EventBus)
	{
		return;
	}

	// Create event data
	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(
		Actor,
		ESuspenseCoreEventPriority::Normal
	);

	EventData.SetString(TEXT("ItemID"), ItemID.ToString());
	EventData.SetString(TEXT("Reason"), Reason);
	EventData.SetObject(TEXT("Actor"), Actor);

	// Broadcast validation failure
	static const FGameplayTag ValidationFailedTag =
		FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Interaction.ValidationFailed"));

	EventBus->Publish(ValidationFailedTag, EventData);

	UE_LOG(LogSuspenseCoreInteraction, Log,
		TEXT("Broadcast ValidationFailed: Actor=%s, ItemID=%s, Reason=%s"),
		*GetNameSafe(Actor), *ItemID.ToString(), *Reason);
}
