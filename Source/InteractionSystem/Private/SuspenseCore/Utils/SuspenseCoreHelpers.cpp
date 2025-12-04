// SuspenseCoreHelpers.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Utils/SuspenseCoreHelpers.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Types/Items/SuspenseCoreItemTypes.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Components/ActorComponent.h"
#include "ItemSystem/SuspenseItemManager.h"
#include "Interfaces/Inventory/ISuspenseInventory.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

// Log category definition
DEFINE_LOG_CATEGORY(LogSuspenseCoreInteraction);

//==================================================================
// EventBus Access Implementation
//==================================================================

USuspenseCoreEventBus* USuspenseCoreHelpers::GetEventBus(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

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

	return Object->GetClass()->ImplementsInterface(USuspenseInventory::StaticClass());
}

//==================================================================
// Item Operations Implementation
//==================================================================

bool USuspenseCoreHelpers::AddItemToInventoryByID(
	UObject* InventoryComponent,
	FName ItemID,
	int32 Quantity)
{
	if (!InventoryComponent || !ImplementsInventoryInterface(InventoryComponent))
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

	// Use the interface method directly
	bool bResult = ISuspenseInventory::Execute_AddItemByID(InventoryComponent, ItemID, Quantity);

	if (bResult)
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

	return bResult;
}

bool USuspenseCoreHelpers::AddItemInstanceToInventory(
	UObject* InventoryComponent,
	const FSuspenseInventoryItemInstance& ItemInstance)
{
	if (!InventoryComponent || !ImplementsInventoryInterface(InventoryComponent))
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

	// Get the interface implementation
	ISuspenseInventory* InventoryInterface = Cast<ISuspenseInventory>(InventoryComponent);
	if (!InventoryInterface)
	{
		UE_LOG(LogSuspenseCoreInteraction, Error,
			TEXT("AddItemInstanceToInventory: Failed to cast to interface"));
		return false;
	}

	// Add the instance
	FSuspenseInventoryOperationResult Result = InventoryInterface->AddItemInstance(ItemInstance);

	if (Result.bSuccess)
	{
		UE_LOG(LogSuspenseCoreInteraction, Log,
			TEXT("AddItemInstanceToInventory: Successfully added instance %s"),
			*ItemInstance.GetShortDebugString());
	}
	else
	{
		UE_LOG(LogSuspenseCoreInteraction, Warning,
			TEXT("AddItemInstanceToInventory: Failed with error %s"),
			*Result.ErrorMessage.ToString());
	}

	return Result.bSuccess;
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

	// Get inventory component
	UObject* InventoryComponent = FindInventoryComponent(Actor);
	if (!InventoryComponent || !ImplementsInventoryInterface(InventoryComponent))
	{
		UE_LOG(LogSuspenseCoreInteraction, Warning,
			TEXT("CanActorPickupItem: No valid inventory component found for actor %s"),
			*Actor->GetName());

		// Broadcast validation failure through EventBus
		BroadcastValidationFailed(Actor, Actor, ItemID, TEXT("No inventory component"));
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

	// Check if inventory can receive item using ItemID
	bool bCanReceive = ISuspenseInventory::Execute_CanReceiveItemByID(
		InventoryComponent,
		ItemID,
		Quantity
	);

	if (!bCanReceive)
	{
		// Determine specific reason and broadcast
		float CurrentWeight = ISuspenseInventory::Execute_GetCurrentWeight(InventoryComponent);
		float MaxWeight = ISuspenseInventory::Execute_GetMaxWeight(InventoryComponent);
		float RequiredWeight = ItemData.InventoryProps.Weight * Quantity;

		FString Reason;
		if (CurrentWeight + RequiredWeight > MaxWeight)
		{
			Reason = TEXT("Weight limit exceeded");
		}
		else
		{
			Reason = TEXT("No space or type not allowed");
		}

		UE_LOG(LogSuspenseCoreInteraction, Warning,
			TEXT("CanActorPickupItem: Inventory cannot receive item %s - %s"),
			*ItemID.ToString(), *Reason);

		BroadcastValidationFailed(Actor, Actor, ItemID, Reason);
	}

	return bCanReceive;
}

//==================================================================
// TODO: Item Instance Creation (Future SuspenseCore Implementation)
//==================================================================
//
// CreateItemInstance() is DEPRECATED in SuspenseCore architecture.
// Currently uses legacy FSuspenseInventoryItemInstance from BridgeSystem.
//
// Future implementation should:
// 1. Use FSuspenseCoreItemInstance (new SuspenseCore type)
// 2. Broadcast SuspenseCore.Event.Item.InstanceCreated
// 3. Initialize runtime properties via EventBus
//
// For now, use legacy USuspenseItemManager::CreateItemInstance() if needed.
//
//==================================================================

bool USuspenseCoreHelpers::CreateItemInstance(
	const UObject* WorldContextObject,
	FName ItemID,
	int32 Quantity,
	FSuspenseInventoryItemInstance& OutInstance)
{
	// LEGACY BRIDGE: Temporarily uses legacy ItemManager
	// TODO: Replace with SuspenseCore native implementation

	if (ItemID.IsNone() || Quantity <= 0)
	{
		UE_LOG(LogSuspenseCoreInteraction, Warning,
			TEXT("CreateItemInstance: Invalid parameters"));
		return false;
	}

	USuspenseItemManager* LegacyItemManager = GetItemManager(WorldContextObject);
	if (!LegacyItemManager)
	{
		UE_LOG(LogSuspenseCoreInteraction, Error,
			TEXT("CreateItemInstance: Legacy ItemManager not found. "
			     "This function requires migration to SuspenseCore types."));
		return false;
	}

	return LegacyItemManager->CreateItemInstance(ItemID, Quantity, OutInstance);
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
	return USuspenseCoreDataManager::Get(WorldContextObject);
}

USuspenseItemManager* USuspenseCoreHelpers::GetItemManager(const UObject* WorldContextObject)
{
	// Legacy bridge - deprecated, use GetDataManager() instead
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		return nullptr;
	}

	return GameInstance->GetSubsystem<USuspenseItemManager>();
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

	if (!InventoryComponent || !ImplementsInventoryInterface(InventoryComponent))
	{
		OutErrorMessage = TEXT("Invalid inventory component");
		return false;
	}

	// Get item data
	FSuspenseCoreItemData ItemData;
	AActor* Owner = Cast<AActor>(InventoryComponent->GetOuter());
	if (!GetItemData(Owner, ItemID, ItemData))
	{
		OutErrorMessage = FString::Printf(TEXT("Item %s not found"), *ItemID.ToString());
		return false;
	}

	// Check if inventory can receive item
	bool bCanReceive = ISuspenseInventory::Execute_CanReceiveItemByID(
		InventoryComponent, ItemID, Quantity);

	if (!bCanReceive)
	{
		OutErrorMessage = TEXT("Insufficient space or item type not allowed");
	}

	return bCanReceive;
}

bool USuspenseCoreHelpers::ValidateWeightCapacity(
	UObject* InventoryComponent,
	FName ItemID,
	int32 Quantity,
	float& OutRemainingCapacity)
{
	OutRemainingCapacity = 0.0f;

	if (!InventoryComponent || !ImplementsInventoryInterface(InventoryComponent))
	{
		return false;
	}

	// Get item weight
	AActor* Owner = Cast<AActor>(InventoryComponent->GetOuter());
	float ItemWeight = GetItemWeight(Owner, ItemID);
	float TotalWeight = ItemWeight * Quantity;

	// Get inventory weight capacity
	float CurrentWeight = ISuspenseInventory::Execute_GetCurrentWeight(InventoryComponent);
	float MaxWeight = ISuspenseInventory::Execute_GetMaxWeight(InventoryComponent);
	float RemainingWeight = MaxWeight - CurrentWeight;

	OutRemainingCapacity = RemainingWeight;

	return RemainingWeight >= TotalWeight;
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

	if (!InventoryComponent || !ImplementsInventoryInterface(InventoryComponent))
	{
		return;
	}

	// Get interface implementation
	ISuspenseInventory* InventoryInterface = Cast<ISuspenseInventory>(InventoryComponent);
	if (!InventoryInterface)
	{
		return;
	}

	// Get all items
	TArray<FSuspenseInventoryItemInstance> AllInstances = InventoryInterface->GetAllItemInstances();

	OutUsedSlots = AllInstances.Num();

	AActor* Owner = Cast<AActor>(InventoryComponent->GetOuter());

	for (const FSuspenseInventoryItemInstance& Instance : AllInstances)
	{
		OutTotalItems += Instance.Quantity;

		// Get item weight from DataTable
		FSuspenseCoreItemData ItemData;
		if (GetItemData(Owner, Instance.ItemID, ItemData))
		{
			OutTotalWeight += ItemData.InventoryProps.Weight * Instance.Quantity;
		}
	}

	UE_LOG(LogSuspenseCoreInteraction, Log,
		TEXT("Inventory Statistics: %d items, %.2f weight, %d slots used"),
		OutTotalItems, OutTotalWeight, OutUsedSlots);
}

void USuspenseCoreHelpers::LogInventoryContents(
	UObject* InventoryComponent,
	const FString& LogCategory)
{
	if (!InventoryComponent || !ImplementsInventoryInterface(InventoryComponent))
	{
		UE_LOG(LogSuspenseCoreInteraction, Warning,
			TEXT("LogInventoryContents: Invalid inventory component"));
		return;
	}

	// Get interface implementation
	ISuspenseInventory* InventoryInterface = Cast<ISuspenseInventory>(InventoryComponent);
	if (!InventoryInterface)
	{
		return;
	}

	AActor* Owner = Cast<AActor>(InventoryComponent->GetOuter());

	// Get all items
	TArray<FSuspenseInventoryItemInstance> AllInstances = InventoryInterface->GetAllItemInstances();

	UE_LOG(LogSuspenseCoreInteraction, Log, TEXT("=== Inventory Contents (%s) ==="), *LogCategory);
	UE_LOG(LogSuspenseCoreInteraction, Log, TEXT("Total slots used: %d"), AllInstances.Num());

	for (const FSuspenseInventoryItemInstance& Instance : AllInstances)
	{
		FString ItemName = Instance.ItemID.ToString();
		FText DisplayName = GetItemDisplayName(Owner, Instance.ItemID);

		UE_LOG(LogSuspenseCoreInteraction, Log,
			TEXT("  - %s (%s) x%d [Slot: %d, Rotated: %s]"),
			*DisplayName.ToString(),
			*ItemName,
			Instance.Quantity,
			Instance.AnchorIndex,
			Instance.bIsRotated ? TEXT("Yes") : TEXT("No"));

		// Log runtime properties if any
		if (Instance.RuntimeProperties.Num() > 0)
		{
			UE_LOG(LogSuspenseCoreInteraction, Log, TEXT("    Runtime Properties:"));
			for (const auto& PropPair : Instance.RuntimeProperties)
			{
				UE_LOG(LogSuspenseCoreInteraction, Log, TEXT("      %s: %.2f"),
					*PropPair.Key.ToString(), PropPair.Value);
			}
		}
	}

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
