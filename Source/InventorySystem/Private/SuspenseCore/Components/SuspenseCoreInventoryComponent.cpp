// SuspenseCoreInventoryComponent.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Components/SuspenseCoreInventoryComponent.h"
#include "SuspenseCore/Storage/SuspenseCoreInventoryStorage.h"
#include "SuspenseCore/Validation/SuspenseCoreInventoryValidator.h"
#include "SuspenseCore/Events/Inventory/SuspenseCoreInventoryEvents.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTemplateTypes.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Base/SuspenseCoreInventoryLogs.h"
#include "SuspenseCore/Security/SuspenseCoreSecurityValidator.h"
#include "SuspenseCore/Security/SuspenseCoreSecurityMacros.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

USuspenseCoreInventoryComponent::USuspenseCoreInventoryComponent()
	: CurrentWeight(0.0f)
	, bIsInitialized(false)
	, bTransactionActive(false)
	, ProviderID(FGuid::NewGuid())
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void USuspenseCoreInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	// Cache EventBus
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (USuspenseCoreEventManager* EventManager = GI->GetSubsystem<USuspenseCoreEventManager>())
			{
				CachedEventBus = EventManager->GetEventBus();
			}
			CachedDataManager = GI->GetSubsystem<USuspenseCoreDataManager>();
		}
	}

	// Subscribe to EventBus events
	SubscribeToEvents();

	// Auto-initialize if config is set
	if (Config.GridWidth > 0 && Config.GridHeight > 0 && !bIsInitialized)
	{
		Initialize(Config.GridWidth, Config.GridHeight, Config.MaxWeight);
	}
}

void USuspenseCoreInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnsubscribeFromEvents();
	Super::EndPlay(EndPlayReason);
}

void USuspenseCoreInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USuspenseCoreInventoryComponent, ReplicatedInventory);
}

//==================================================================
// ISuspenseCoreInventory - Add Operations
//==================================================================

bool USuspenseCoreInventoryComponent::AddItemByID_Implementation(FName ItemID, int32 Quantity)
{
	check(IsInGameThread());

	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("AddItemByID: Starting for %s x%d"), *ItemID.ToString(), Quantity);

	// Security: Check authority - redirect to Server RPC if client
	if (!CheckInventoryAuthority(TEXT("AddItemByID")))
	{
		UE_LOG(LogSuspenseCoreInventory, Log, TEXT("AddItemByID: No authority, sending to Server RPC"));
		Server_AddItemByID(ItemID, Quantity);
		return false; // Client returns false, server will process
	}

	if (!bIsInitialized)
	{
		UE_LOG(LogSuspenseCoreInventory, Warning, TEXT("AddItemByID: Inventory not initialized"));
		BroadcastErrorEvent(ESuspenseCoreInventoryResult::NotInitialized, TEXT("Inventory not initialized"));
		return false;
	}

	if (ItemID.IsNone() || Quantity <= 0)
	{
		UE_LOG(LogSuspenseCoreInventory, Warning, TEXT("AddItemByID: Invalid parameters (ItemID=%s, Quantity=%d)"),
			*ItemID.ToString(), Quantity);
		BroadcastErrorEvent(ESuspenseCoreInventoryResult::InvalidItem, TEXT("Invalid item or quantity"));
		return false;
	}

	// Create item instance
	FSuspenseCoreItemInstance NewInstance;
	if (!CreateItemInstance(ItemID, Quantity, NewInstance))
	{
		UE_LOG(LogSuspenseCoreInventory, Warning, TEXT("AddItemByID: Failed to create instance for %s"), *ItemID.ToString());
		BroadcastErrorEvent(ESuspenseCoreInventoryResult::InvalidItem, FString::Printf(TEXT("Failed to create instance for %s"), *ItemID.ToString()));
		return false;
	}

	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("AddItemByID: Instance created, calling AddItemInstance"));

	// Try to add
	bool bResult = AddItemInstance_Implementation(NewInstance);
	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("AddItemByID: AddItemInstance returned %s"), bResult ? TEXT("true") : TEXT("false"));
	return bResult;
}

bool USuspenseCoreInventoryComponent::AddItemInstance_Implementation(const FSuspenseCoreItemInstance& ItemInstance)
{
	return AddItemInstanceToSlot(ItemInstance, INDEX_NONE);
}

bool USuspenseCoreInventoryComponent::AddItemInstanceToSlot(const FSuspenseCoreItemInstance& ItemInstance, int32 TargetSlot)
{
	check(IsInGameThread());

	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("AddItemInstanceToSlot: ItemID=%s, TargetSlot=%d"),
		*ItemInstance.ItemID.ToString(), TargetSlot);

	if (!bIsInitialized)
	{
		UE_LOG(LogSuspenseCoreInventory, Warning, TEXT("AddItemInstanceToSlot: Not initialized"));
		BroadcastErrorEvent(ESuspenseCoreInventoryResult::NotInitialized, TEXT("Inventory not initialized"));
		return false;
	}

	if (!ItemInstance.IsValid())
	{
		UE_LOG(LogSuspenseCoreInventory, Warning, TEXT("AddItemInstanceToSlot: Invalid instance"));
		BroadcastErrorEvent(ESuspenseCoreInventoryResult::InvalidItem, TEXT("Invalid item instance"));
		return false;
	}

	// Get item data for validation
	FSuspenseCoreItemData ItemData;
	USuspenseCoreDataManager* DataManager = GetDataManager();
	if (!DataManager)
	{
		UE_LOG(LogSuspenseCoreInventory, Warning, TEXT("AddItemInstanceToSlot: DataManager is null"));
		BroadcastErrorEvent(ESuspenseCoreInventoryResult::ItemNotFound, TEXT("DataManager not available"));
		return false;
	}

	if (!DataManager->GetItemData(ItemInstance.ItemID, ItemData))
	{
		UE_LOG(LogSuspenseCoreInventory, Warning, TEXT("AddItemInstanceToSlot: Item %s not found in DataManager"),
			*ItemInstance.ItemID.ToString());
		BroadcastErrorEvent(ESuspenseCoreInventoryResult::ItemNotFound,
			FString::Printf(TEXT("Item %s not found in DataTable"), *ItemInstance.ItemID.ToString()));
		return false;
	}

	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("AddItemInstanceToSlot: ItemData loaded - GridSize=%dx%d, Weight=%.2f"),
		ItemData.InventoryProps.GridSize.X, ItemData.InventoryProps.GridSize.Y, ItemData.InventoryProps.Weight);

	// Check weight
	float ItemWeight = ItemData.InventoryProps.Weight * ItemInstance.Quantity;
	if (CurrentWeight + ItemWeight > Config.MaxWeight)
	{
		UE_LOG(LogSuspenseCoreInventory, Warning, TEXT("AddItemInstanceToSlot: Weight exceeded (%.1f + %.1f > %.1f)"),
			CurrentWeight, ItemWeight, Config.MaxWeight);
		BroadcastErrorEvent(ESuspenseCoreInventoryResult::WeightLimitExceeded,
			FString::Printf(TEXT("Weight limit exceeded (Current: %.1f, Adding: %.1f, Max: %.1f)"),
				CurrentWeight, ItemWeight, Config.MaxWeight));
		return false;
	}

	// Check type restrictions
	if (Config.AllowedItemTypes.Num() > 0 && !Config.AllowedItemTypes.HasTag(ItemData.Classification.ItemType))
	{
		UE_LOG(LogSuspenseCoreInventory, Warning, TEXT("AddItemInstanceToSlot: Type %s not in allowed types (%d types)"),
			*ItemData.Classification.ItemType.ToString(), Config.AllowedItemTypes.Num());
		BroadcastErrorEvent(ESuspenseCoreInventoryResult::TypeNotAllowed,
			FString::Printf(TEXT("Item type %s not allowed"), *ItemData.Classification.ItemType.ToString()));
		return false;
	}

	if (Config.DisallowedItemTypes.HasTag(ItemData.Classification.ItemType))
	{
		UE_LOG(LogSuspenseCoreInventory, Warning, TEXT("AddItemInstanceToSlot: Type %s is disallowed"),
			*ItemData.Classification.ItemType.ToString());
		BroadcastErrorEvent(ESuspenseCoreInventoryResult::TypeNotAllowed,
			FString::Printf(TEXT("Item type %s is disallowed"), *ItemData.Classification.ItemType.ToString()));
		return false;
	}

	// Try auto-stacking first
	if (Config.bAutoStack && ItemData.InventoryProps.IsStackable())
	{
		for (FSuspenseCoreItemInstance& ExistingInstance : ItemInstances)
		{
			if (ExistingInstance.CanStackWith(ItemInstance))
			{
				int32 SpaceInStack = ItemData.InventoryProps.MaxStackSize - ExistingInstance.Quantity;
				if (SpaceInStack > 0)
				{
					int32 ToAdd = FMath::Min(SpaceInStack, ItemInstance.Quantity);
					ExistingInstance.Quantity += ToAdd;

					// Update replication
					ReplicatedInventory.UpdateItem(ExistingInstance);

					// Broadcast event
					BroadcastItemEvent(SUSPENSE_INV_EVENT_ITEM_QTY_CHANGED, ExistingInstance, ExistingInstance.SlotIndex);

					if (ToAdd == ItemInstance.Quantity)
					{
						RecalculateWeight();
						BroadcastInventoryUpdated();
						return true;
					}
					// Continue with remaining quantity in a new stack
					FSuspenseCoreItemInstance RemainingInstance = ItemInstance;
					RemainingInstance.Quantity -= ToAdd;
					RemainingInstance.UniqueInstanceID = FGuid::NewGuid();
					return AddItemInstanceToSlot(RemainingInstance, TargetSlot);
				}
			}
		}
	}

	// Find slot
	int32 PlacementSlot = TargetSlot;
	if (PlacementSlot == INDEX_NONE)
	{
		UE_LOG(LogSuspenseCoreInventory, Log, TEXT("AddItemInstanceToSlot: Finding free slot for %dx%d item in %dx%d grid"),
			ItemData.InventoryProps.GridSize.X, ItemData.InventoryProps.GridSize.Y,
			Config.GridWidth, Config.GridHeight);
		PlacementSlot = FindFreeSlot(ItemData.InventoryProps.GridSize, Config.bAllowRotation);
	}

	if (PlacementSlot == INDEX_NONE)
	{
		UE_LOG(LogSuspenseCoreInventory, Warning, TEXT("AddItemInstanceToSlot: No free slot found"));
		BroadcastErrorEvent(ESuspenseCoreInventoryResult::NoSpace, TEXT("No space available in inventory"));
		return false;
	}

	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("AddItemInstanceToSlot: Using slot %d"), PlacementSlot);

	// Check placement validity
	if (!CanPlaceItemAtSlot(ItemData.InventoryProps.GridSize, PlacementSlot, false))
	{
		UE_LOG(LogSuspenseCoreInventory, Warning, TEXT("AddItemInstanceToSlot: Cannot place at slot %d"), PlacementSlot);
		BroadcastErrorEvent(ESuspenseCoreInventoryResult::SlotOccupied,
			FString::Printf(TEXT("Cannot place item at slot %d"), PlacementSlot));
		return false;
	}

	// Add the instance
	FSuspenseCoreItemInstance NewInstance = ItemInstance;
	NewInstance.SlotIndex = PlacementSlot;
	NewInstance.GridPosition = SlotToGridCoords(PlacementSlot);
	if (!NewInstance.UniqueInstanceID.IsValid())
	{
		NewInstance.UniqueInstanceID = FGuid::NewGuid();
	}

	ItemInstances.Add(NewInstance);
	UpdateGridSlots(NewInstance, true);
	ReplicatedInventory.AddItem(NewInstance);
	RecalculateWeight();

	// Broadcast events
	BroadcastItemEvent(SUSPENSE_INV_EVENT_ITEM_ADDED, NewInstance, PlacementSlot);
	BroadcastInventoryUpdated();

	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("Added item %s x%d to slot %d"),
		*ItemInstance.ItemID.ToString(), ItemInstance.Quantity, PlacementSlot);

	return true;
}

//==================================================================
// ISuspenseCoreInventory - Remove Operations
//==================================================================

bool USuspenseCoreInventoryComponent::RemoveItemByID_Implementation(FName ItemID, int32 Quantity)
{
	check(IsInGameThread());

	// Security: Check authority - redirect to Server RPC if client
	if (!CheckInventoryAuthority(TEXT("RemoveItemByID")))
	{
		Server_RemoveItemByID(ItemID, Quantity);
		return false;
	}

	if (!bIsInitialized || ItemID.IsNone() || Quantity <= 0)
	{
		return false;
	}

	int32 RemainingToRemove = Quantity;

	// Find and remove from stacks
	for (int32 i = ItemInstances.Num() - 1; i >= 0 && RemainingToRemove > 0; --i)
	{
		FSuspenseCoreItemInstance& Instance = ItemInstances[i];
		if (Instance.ItemID == ItemID)
		{
			if (Instance.Quantity <= RemainingToRemove)
			{
				// Remove entire instance
				RemainingToRemove -= Instance.Quantity;
				FSuspenseCoreItemInstance RemovedInstance;
				RemoveItemInternal(Instance.UniqueInstanceID, RemovedInstance);
			}
			else
			{
				// Partial removal
				Instance.Quantity -= RemainingToRemove;
				ReplicatedInventory.UpdateItem(Instance);
				BroadcastItemEvent(SUSPENSE_INV_EVENT_ITEM_QTY_CHANGED, Instance, Instance.SlotIndex);
				RemainingToRemove = 0;
			}
		}
	}

	RecalculateWeight();
	BroadcastInventoryUpdated();

	return RemainingToRemove == 0;
}

bool USuspenseCoreInventoryComponent::RemoveItemInstance(const FGuid& InstanceID)
{
	FSuspenseCoreItemInstance RemovedInstance;
	return RemoveItemInternal(InstanceID, RemovedInstance);
}

bool USuspenseCoreInventoryComponent::RemoveItemFromSlot(int32 SlotIndex, FSuspenseCoreItemInstance& OutRemovedInstance)
{
	if (!IsSlotOccupied(SlotIndex))
	{
		return false;
	}

	FGuid InstanceID = GridSlots[SlotIndex].InstanceID;
	return RemoveItemInternal(InstanceID, OutRemovedInstance);
}

//==================================================================
// ISuspenseCoreInventory - Query Operations
//==================================================================

TArray<FSuspenseCoreItemInstance> USuspenseCoreInventoryComponent::GetAllItemInstances() const
{
	return ItemInstances;
}

bool USuspenseCoreInventoryComponent::GetItemInstanceAtSlot(int32 SlotIndex, FSuspenseCoreItemInstance& OutInstance) const
{
	if (!IsValidGridCoords(SlotToGridCoords(SlotIndex)) || !IsSlotOccupied(SlotIndex))
	{
		return false;
	}

	const FGuid& InstanceID = GridSlots[SlotIndex].InstanceID;
	const FSuspenseCoreItemInstance* Instance = FindItemInstanceInternal(InstanceID);
	if (Instance)
	{
		OutInstance = *Instance;
		return true;
	}
	return false;
}

bool USuspenseCoreInventoryComponent::FindItemInstance(const FGuid& InstanceID, FSuspenseCoreItemInstance& OutInstance) const
{
	const FSuspenseCoreItemInstance* Instance = FindItemInstanceInternal(InstanceID);
	if (Instance)
	{
		OutInstance = *Instance;
		return true;
	}
	return false;
}

int32 USuspenseCoreInventoryComponent::GetItemCountByID_Implementation(FName ItemID) const
{
	int32 Count = 0;
	for (const FSuspenseCoreItemInstance& Instance : ItemInstances)
	{
		if (Instance.ItemID == ItemID)
		{
			Count += Instance.Quantity;
		}
	}
	return Count;
}

bool USuspenseCoreInventoryComponent::HasItem_Implementation(FName ItemID, int32 Quantity) const
{
	return GetItemCountByID_Implementation(ItemID) >= Quantity;
}

int32 USuspenseCoreInventoryComponent::GetTotalItemCount_Implementation() const
{
	return ItemInstances.Num();
}

TArray<FSuspenseCoreItemInstance> USuspenseCoreInventoryComponent::FindItemsByType(FGameplayTag ItemType) const
{
	TArray<FSuspenseCoreItemInstance> Result;
	USuspenseCoreDataManager* DataManager = GetDataManager();
	if (!DataManager)
	{
		return Result;
	}

	for (const FSuspenseCoreItemInstance& Instance : ItemInstances)
	{
		FSuspenseCoreItemData ItemData;
		if (DataManager->GetItemData(Instance.ItemID, ItemData))
		{
			if (ItemData.Classification.ItemType.MatchesTag(ItemType))
			{
				Result.Add(Instance);
			}
		}
	}
	return Result;
}

//==================================================================
// ISuspenseCoreInventory - Grid Operations
//==================================================================

FIntPoint USuspenseCoreInventoryComponent::GetGridSize_Implementation() const
{
	return FIntPoint(Config.GridWidth, Config.GridHeight);
}

bool USuspenseCoreInventoryComponent::MoveItem_Implementation(int32 FromSlot, int32 ToSlot)
{
	// Security: Check authority - redirect to Server RPC if client
	if (!CheckInventoryAuthority(TEXT("MoveItem")))
	{
		Server_MoveItem(FromSlot, ToSlot);
		return false;
	}

	if (!bIsInitialized || FromSlot == ToSlot)
	{
		return false;
	}

	FSuspenseCoreItemInstance Instance;
	if (!GetItemInstanceAtSlot(FromSlot, Instance))
	{
		return false;
	}

	FSuspenseCoreItemData ItemData;
	USuspenseCoreDataManager* DataManager = GetDataManager();
	if (!DataManager || !DataManager->GetItemData(Instance.ItemID, ItemData))
	{
		return false;
	}

	// Check if target is valid
	bool bRotated = Instance.Rotation != 0;
	if (!CanPlaceItemAtSlot(ItemData.InventoryProps.GridSize, ToSlot, bRotated))
	{
		return false;
	}

	// Remove from old position
	UpdateGridSlots(Instance, false);

	// Update position
	int32 OldSlot = Instance.SlotIndex;
	FSuspenseCoreItemInstance* InstancePtr = FindItemInstanceInternal(Instance.UniqueInstanceID);
	if (InstancePtr)
	{
		InstancePtr->SlotIndex = ToSlot;
		InstancePtr->GridPosition = SlotToGridCoords(ToSlot);
		UpdateGridSlots(*InstancePtr, true);
		ReplicatedInventory.UpdateItem(*InstancePtr);
		BroadcastItemEvent(SUSPENSE_INV_EVENT_ITEM_MOVED, *InstancePtr, ToSlot);
	}

	BroadcastInventoryUpdated();
	return true;
}

bool USuspenseCoreInventoryComponent::SwapItems(int32 Slot1, int32 Slot2)
{
	// Security: Check authority - redirect to Server RPC if client
	if (!CheckInventoryAuthority(TEXT("SwapItems")))
	{
		Server_SwapItems(Slot1, Slot2);
		return false;
	}

	if (!bIsInitialized || Slot1 == Slot2)
	{
		return false;
	}

	FSuspenseCoreItemInstance Instance1, Instance2;
	bool bHas1 = GetItemInstanceAtSlot(Slot1, Instance1);
	bool bHas2 = GetItemInstanceAtSlot(Slot2, Instance2);

	if (!bHas1 && !bHas2)
	{
		return false;
	}

	// Remove both from grid
	if (bHas1) UpdateGridSlots(Instance1, false);
	if (bHas2) UpdateGridSlots(Instance2, false);

	// Swap positions
	if (bHas1)
	{
		FSuspenseCoreItemInstance* Ptr1 = FindItemInstanceInternal(Instance1.UniqueInstanceID);
		if (Ptr1)
		{
			Ptr1->SlotIndex = Slot2;
			Ptr1->GridPosition = SlotToGridCoords(Slot2);
			UpdateGridSlots(*Ptr1, true);
			ReplicatedInventory.UpdateItem(*Ptr1);
		}
	}

	if (bHas2)
	{
		FSuspenseCoreItemInstance* Ptr2 = FindItemInstanceInternal(Instance2.UniqueInstanceID);
		if (Ptr2)
		{
			Ptr2->SlotIndex = Slot1;
			Ptr2->GridPosition = SlotToGridCoords(Slot1);
			UpdateGridSlots(*Ptr2, true);
			ReplicatedInventory.UpdateItem(*Ptr2);
		}
	}

	BroadcastInventoryUpdated();
	return true;
}

bool USuspenseCoreInventoryComponent::RotateItemAtSlot(int32 SlotIndex)
{
	FSuspenseCoreItemInstance* Instance = nullptr;
	for (FSuspenseCoreItemInstance& Inst : ItemInstances)
	{
		if (Inst.SlotIndex == SlotIndex)
		{
			Instance = &Inst;
			break;
		}
	}

	if (!Instance)
	{
		return false;
	}

	// Toggle rotation
	Instance->Rotation = (Instance->Rotation + 90) % 360;
	ReplicatedInventory.UpdateItem(*Instance);
	BroadcastItemEvent(SUSPENSE_INV_EVENT_ITEM_ROTATED, *Instance, SlotIndex);
	BroadcastInventoryUpdated();
	return true;
}

bool USuspenseCoreInventoryComponent::IsSlotOccupied(int32 SlotIndex) const
{
	if (SlotIndex < 0 || SlotIndex >= GridSlots.Num())
	{
		return false;
	}
	return !GridSlots[SlotIndex].IsEmpty();
}

int32 USuspenseCoreInventoryComponent::FindFreeSlot(FIntPoint ItemGridSize, bool bAllowRotation) const
{
	for (int32 SlotIndex = 0; SlotIndex < GridSlots.Num(); ++SlotIndex)
	{
		if (CanPlaceItemAtSlot(ItemGridSize, SlotIndex, false))
		{
			return SlotIndex;
		}
		if (bAllowRotation && CanPlaceItemAtSlot(ItemGridSize, SlotIndex, true))
		{
			return SlotIndex;
		}
	}
	return INDEX_NONE;
}

bool USuspenseCoreInventoryComponent::CanPlaceItemAtSlot(FIntPoint ItemGridSize, int32 SlotIndex, bool bRotated) const
{
	if (!bIsInitialized || SlotIndex < 0)
	{
		return false;
	}

	FIntPoint EffectiveSize = bRotated ? FIntPoint(ItemGridSize.Y, ItemGridSize.X) : ItemGridSize;
	FIntPoint StartCoords = SlotToGridCoords(SlotIndex);

	// Check bounds
	if (StartCoords.X + EffectiveSize.X > Config.GridWidth ||
		StartCoords.Y + EffectiveSize.Y > Config.GridHeight)
	{
		return false;
	}

	// Check each cell
	for (int32 Y = 0; Y < EffectiveSize.Y; ++Y)
	{
		for (int32 X = 0; X < EffectiveSize.X; ++X)
		{
			int32 CheckSlot = GridCoordsToSlot(FIntPoint(StartCoords.X + X, StartCoords.Y + Y));
			if (CheckSlot != INDEX_NONE && IsSlotOccupied(CheckSlot))
			{
				return false;
			}
		}
	}

	return true;
}

//==================================================================
// ISuspenseCoreInventory - Weight System
//==================================================================

float USuspenseCoreInventoryComponent::GetCurrentWeight_Implementation() const
{
	return CurrentWeight;
}

float USuspenseCoreInventoryComponent::GetMaxWeight_Implementation() const
{
	return Config.MaxWeight;
}

float USuspenseCoreInventoryComponent::GetRemainingWeight_Implementation() const
{
	return FMath::Max(0.0f, Config.MaxWeight - CurrentWeight);
}

bool USuspenseCoreInventoryComponent::HasWeightCapacity_Implementation(float AdditionalWeight) const
{
	return (CurrentWeight + AdditionalWeight) <= Config.MaxWeight;
}

void USuspenseCoreInventoryComponent::SetMaxWeight(float NewMaxWeight)
{
	Config.MaxWeight = FMath::Max(0.0f, NewMaxWeight);
	ReplicatedInventory.MaxWeight = Config.MaxWeight;
}

//==================================================================
// ISuspenseCoreInventory - Validation
//==================================================================

bool USuspenseCoreInventoryComponent::CanReceiveItem_Implementation(FName ItemID, int32 Quantity) const
{
	if (!bIsInitialized || ItemID.IsNone() || Quantity <= 0)
	{
		return false;
	}

	USuspenseCoreDataManager* DataManager = GetDataManager();
	if (!DataManager)
	{
		return false;
	}

	FSuspenseCoreItemData ItemData;
	if (!DataManager->GetItemData(ItemID, ItemData))
	{
		return false;
	}

	// Check weight
	float ItemWeight = ItemData.InventoryProps.Weight * Quantity;
	if (CurrentWeight + ItemWeight > Config.MaxWeight)
	{
		return false;
	}

	// Check type
	if (Config.AllowedItemTypes.Num() > 0 && !Config.AllowedItemTypes.HasTag(ItemData.Classification.ItemType))
	{
		return false;
	}

	if (Config.DisallowedItemTypes.HasTag(ItemData.Classification.ItemType))
	{
		return false;
	}

	// Check space (simplified - just check if any slot available)
	if (FindFreeSlot(ItemData.InventoryProps.GridSize, Config.bAllowRotation) == INDEX_NONE)
	{
		// Check if can stack
		if (ItemData.InventoryProps.IsStackable())
		{
			for (const FSuspenseCoreItemInstance& Instance : ItemInstances)
			{
				if (Instance.ItemID == ItemID)
				{
					int32 SpaceInStack = ItemData.InventoryProps.MaxStackSize - Instance.Quantity;
					if (SpaceInStack >= Quantity)
					{
						return true;
					}
				}
			}
		}
		return false;
	}

	return true;
}

FGameplayTagContainer USuspenseCoreInventoryComponent::GetAllowedItemTypes_Implementation() const
{
	return Config.AllowedItemTypes;
}

void USuspenseCoreInventoryComponent::SetAllowedItemTypes(const FGameplayTagContainer& AllowedTypes)
{
	Config.AllowedItemTypes = AllowedTypes;
}

bool USuspenseCoreInventoryComponent::ValidateIntegrity(TArray<FString>& OutErrors) const
{
	bool bValid = true;

	// Check grid slots match items
	for (const FSuspenseCoreItemInstance& Instance : ItemInstances)
	{
		if (Instance.SlotIndex < 0 || Instance.SlotIndex >= GridSlots.Num())
		{
			OutErrors.Add(FString::Printf(TEXT("Item %s has invalid slot %d"), *Instance.ItemID.ToString(), Instance.SlotIndex));
			bValid = false;
		}
		else if (GridSlots[Instance.SlotIndex].InstanceID != Instance.UniqueInstanceID)
		{
			OutErrors.Add(FString::Printf(TEXT("Grid slot %d doesn't match item %s"), Instance.SlotIndex, *Instance.ItemID.ToString()));
			bValid = false;
		}
	}

	return bValid;
}

//==================================================================
// ISuspenseCoreInventory - Transaction System
//==================================================================

void USuspenseCoreInventoryComponent::BeginTransaction()
{
	if (bTransactionActive)
	{
		UE_LOG(LogSuspenseCoreInventory, Warning, TEXT("Transaction already active"));
		return;
	}

	TransactionSnapshot.Items = ItemInstances;
	TransactionSnapshot.Slots = GridSlots;
	TransactionSnapshot.CurrentWeight = CurrentWeight;
	TransactionSnapshot.SnapshotTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	bTransactionActive = true;

	UE_LOG(LogSuspenseCoreInventory, Verbose, TEXT("Transaction started"));
}

void USuspenseCoreInventoryComponent::CommitTransaction()
{
	if (!bTransactionActive)
	{
		return;
	}

	bTransactionActive = false;
	TransactionSnapshot = FSuspenseCoreInventorySnapshot();

	UE_LOG(LogSuspenseCoreInventory, Verbose, TEXT("Transaction committed"));
}

void USuspenseCoreInventoryComponent::RollbackTransaction()
{
	if (!bTransactionActive)
	{
		return;
	}

	ItemInstances = TransactionSnapshot.Items;
	GridSlots = TransactionSnapshot.Slots;
	CurrentWeight = TransactionSnapshot.CurrentWeight;
	bTransactionActive = false;

	// Rebuild replicated data
	ReplicatedInventory.ClearItems();
	for (const FSuspenseCoreItemInstance& Instance : ItemInstances)
	{
		ReplicatedInventory.AddItem(Instance);
	}

	BroadcastInventoryUpdated();
	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("Transaction rolled back"));
}

bool USuspenseCoreInventoryComponent::IsTransactionActive() const
{
	return bTransactionActive;
}

//==================================================================
// ISuspenseCoreInventory - Stack Operations
//==================================================================

bool USuspenseCoreInventoryComponent::SplitStack(int32 SourceSlot, int32 SplitQuantity, int32 TargetSlot)
{
	// Security: Check authority - redirect to Server RPC if client
	if (!CheckInventoryAuthority(TEXT("SplitStack")))
	{
		Server_SplitStack(SourceSlot, SplitQuantity, TargetSlot);
		return false;
	}

	FSuspenseCoreItemInstance SourceInstance;
	if (!GetItemInstanceAtSlot(SourceSlot, SourceInstance))
	{
		return false;
	}

	if (SplitQuantity <= 0 || SplitQuantity >= SourceInstance.Quantity)
	{
		return false;
	}

	// Reduce source stack
	FSuspenseCoreItemInstance* SourcePtr = FindItemInstanceInternal(SourceInstance.UniqueInstanceID);
	if (!SourcePtr)
	{
		return false;
	}

	SourcePtr->Quantity -= SplitQuantity;
	ReplicatedInventory.UpdateItem(*SourcePtr);

	// Create new stack
	FSuspenseCoreItemInstance NewStack = SourceInstance;
	NewStack.UniqueInstanceID = FGuid::NewGuid();
	NewStack.Quantity = SplitQuantity;
	NewStack.SlotIndex = -1;

	return AddItemInstanceToSlot(NewStack, TargetSlot);
}

int32 USuspenseCoreInventoryComponent::ConsolidateStacks(FName ItemID)
{
	int32 Consolidated = 0;
	// Implementation would merge matching stacks
	// Simplified for now
	return Consolidated;
}

//==================================================================
// ISuspenseCoreInventory - Initialization
//==================================================================

bool USuspenseCoreInventoryComponent::InitializeFromLoadout(FName LoadoutID)
{
	// Get DataManager
	USuspenseCoreDataManager* DataManager = GetDataManager();
	if (!DataManager)
	{
		UE_LOG(LogSuspenseCoreInventory, Error, TEXT("InitializeFromLoadout: DataManager not available"));
		// Fallback to default config
		Initialize(Config.GridWidth, Config.GridHeight, Config.MaxWeight);
		return false;
	}

	// Get loadout DataTable
	UDataTable* LoadoutTable = DataManager->GetLoadoutDataTable();
	if (!LoadoutTable)
	{
		UE_LOG(LogSuspenseCoreInventory, Warning, TEXT("InitializeFromLoadout: LoadoutDataTable not configured"));
		// Fallback to default config
		Initialize(Config.GridWidth, Config.GridHeight, Config.MaxWeight);
		return false;
	}

	// Find loadout row
	static const FString ContextString(TEXT("InitializeFromLoadout"));
	FSuspenseCoreTemplateLoadout* LoadoutRow = LoadoutTable->FindRow<FSuspenseCoreTemplateLoadout>(LoadoutID, ContextString);

	if (!LoadoutRow)
	{
		UE_LOG(LogSuspenseCoreInventory, Warning, TEXT("InitializeFromLoadout: Loadout '%s' not found in DataTable"), *LoadoutID.ToString());
		// Fallback to default config
		Initialize(Config.GridWidth, Config.GridHeight, Config.MaxWeight);
		return false;
	}

	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("InitializeFromLoadout: Loading '%s' (Grid: %dx%d, MaxWeight: %.1f)"),
		*LoadoutID.ToString(),
		LoadoutRow->InventoryWidth,
		LoadoutRow->InventoryHeight,
		LoadoutRow->MaxWeight);

	// Initialize with loadout configuration
	Initialize(LoadoutRow->InventoryWidth, LoadoutRow->InventoryHeight, LoadoutRow->MaxWeight);

	// TODO: Add starting items from loadout
	// for (const FSuspenseCoreTemplateLoadoutSlot& Slot : LoadoutRow->EquipmentSlots)
	// {
	//     if (!Slot.ItemID.IsNone())
	//     {
	//         AddItemByID(Slot.ItemID, 1);
	//     }
	// }

	return true;
}

void USuspenseCoreInventoryComponent::Initialize(int32 GridWidth, int32 GridHeight, float InMaxWeight)
{
	Config.GridWidth = FMath::Clamp(GridWidth, 1, 20);
	Config.GridHeight = FMath::Clamp(GridHeight, 1, 20);
	Config.MaxWeight = FMath::Max(0.0f, InMaxWeight);

	int32 TotalSlots = Config.GridWidth * Config.GridHeight;
	GridSlots.SetNum(TotalSlots);
	for (FSuspenseCoreInventorySlot& Slot : GridSlots)
	{
		Slot.Clear();
	}

	ItemInstances.Empty();
	CurrentWeight = 0.0f;
	bIsInitialized = true;

	ReplicatedInventory.GridWidth = Config.GridWidth;
	ReplicatedInventory.GridHeight = Config.GridHeight;
	ReplicatedInventory.MaxWeight = Config.MaxWeight;
	ReplicatedInventory.OwnerComponent = this;

	// Broadcast initialization
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		FSuspenseCoreEventData EventData;
		EventData.Source = this;
		EventBus->Publish(SUSPENSE_INV_EVENT_INITIALIZED, EventData);
	}

	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("Inventory initialized: %dx%d grid, %.1f max weight"),
		Config.GridWidth, Config.GridHeight, Config.MaxWeight);
}

bool USuspenseCoreInventoryComponent::IsInitialized() const
{
	return bIsInitialized;
}

void USuspenseCoreInventoryComponent::Clear()
{
	ItemInstances.Empty();
	for (FSuspenseCoreInventorySlot& Slot : GridSlots)
	{
		Slot.Clear();
	}
	CurrentWeight = 0.0f;
	ReplicatedInventory.ClearItems();

	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		FSuspenseCoreEventData EventData;
		EventData.Source = this;
		EventBus->Publish(SUSPENSE_INV_EVENT_CLEARED, EventData);
	}

	BroadcastInventoryUpdated();
}

//==================================================================
// ISuspenseCoreInventory - EventBus
//==================================================================

USuspenseCoreEventBus* USuspenseCoreInventoryComponent::GetEventBus() const
{
	return CachedEventBus.Get();
}

void USuspenseCoreInventoryComponent::BroadcastInventoryUpdated()
{
	// Broadcast UI data changed for widget refresh
	static const FGameplayTag FullRefreshTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UIProvider.DataChanged.Full"));
	BroadcastUIDataChanged(FullRefreshTag, FGuid());

	// Also broadcast via EventBus
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		FSuspenseCoreEventData EventData;
		EventData.Source = const_cast<USuspenseCoreInventoryComponent*>(this);
		EventBus->Publish(SUSPENSE_INV_EVENT_UPDATED, EventData);
	}
}

//==================================================================
// ISuspenseCoreInventory - Debug
//==================================================================

FString USuspenseCoreInventoryComponent::GetDebugString() const
{
	return FString::Printf(TEXT("Inventory [%dx%d] Items: %d Weight: %.1f/%.1f"),
		Config.GridWidth, Config.GridHeight, ItemInstances.Num(), CurrentWeight, Config.MaxWeight);
}

void USuspenseCoreInventoryComponent::LogContents() const
{
	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("=== Inventory Contents ==="));
	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("%s"), *GetDebugString());
	for (const FSuspenseCoreItemInstance& Instance : ItemInstances)
	{
		UE_LOG(LogSuspenseCoreInventory, Log, TEXT("  [%d] %s x%d"),
			Instance.SlotIndex, *Instance.ItemID.ToString(), Instance.Quantity);
	}
}

//==================================================================
// Internal Operations
//==================================================================

USuspenseCoreDataManager* USuspenseCoreInventoryComponent::GetDataManager() const
{
	if (CachedDataManager.IsValid())
	{
		return CachedDataManager.Get();
	}

	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			return GI->GetSubsystem<USuspenseCoreDataManager>();
		}
	}
	return nullptr;
}

bool USuspenseCoreInventoryComponent::CreateItemInstance(FName ItemID, int32 Quantity, FSuspenseCoreItemInstance& OutInstance) const
{
	USuspenseCoreDataManager* DataManager = GetDataManager();
	if (!DataManager)
	{
		return false;
	}

	return DataManager->CreateItemInstance(ItemID, Quantity, OutInstance);
}

bool USuspenseCoreInventoryComponent::AddItemInternal(const FSuspenseCoreItemInstance& ItemInstance, int32 TargetSlot)
{
	FSuspenseCoreItemInstance NewInstance = ItemInstance;
	NewInstance.SlotIndex = TargetSlot;
	NewInstance.GridPosition = SlotToGridCoords(TargetSlot);

	ItemInstances.Add(NewInstance);
	UpdateGridSlots(NewInstance, true);
	ReplicatedInventory.AddItem(NewInstance);
	RecalculateWeight();

	return true;
}

bool USuspenseCoreInventoryComponent::RemoveItemInternal(const FGuid& InstanceID, FSuspenseCoreItemInstance& OutRemovedInstance)
{
	int32 Index = ItemInstances.IndexOfByPredicate([&InstanceID](const FSuspenseCoreItemInstance& Instance)
	{
		return Instance.UniqueInstanceID == InstanceID;
	});

	if (Index == INDEX_NONE)
	{
		return false;
	}

	OutRemovedInstance = ItemInstances[Index];
	UpdateGridSlots(OutRemovedInstance, false);
	ItemInstances.RemoveAt(Index);
	ReplicatedInventory.RemoveItem(InstanceID);
	RecalculateWeight();

	BroadcastItemEvent(SUSPENSE_INV_EVENT_ITEM_REMOVED, OutRemovedInstance, OutRemovedInstance.SlotIndex);

	return true;
}

void USuspenseCoreInventoryComponent::BroadcastItemEvent(FGameplayTag EventTag, const FSuspenseCoreItemInstance& Instance, int32 SlotIndex)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	FSuspenseCoreEventData EventData;
	EventData.Source = this;
	EventData.SetString(TEXT("InstanceID"), Instance.UniqueInstanceID.ToString());
	EventData.SetString(TEXT("ItemID"), Instance.ItemID.ToString());
	EventData.SetInt(TEXT("Quantity"), Instance.Quantity);
	EventData.SetInt(TEXT("SlotIndex"), SlotIndex);

	EventBus->Publish(EventTag, EventData);
}

void USuspenseCoreInventoryComponent::BroadcastErrorEvent(ESuspenseCoreInventoryResult ErrorCode, const FString& Context)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	FSuspenseCoreEventData EventData;
	EventData.Source = this;
	EventData.SetInt(TEXT("ErrorCode"), static_cast<int32>(ErrorCode));
	EventData.SetString(TEXT("ErrorMessage"), Context);

	EventBus->Publish(SUSPENSE_INV_EVENT_OPERATION_FAILED, EventData);
}

void USuspenseCoreInventoryComponent::OnRep_ReplicatedInventory()
{
	// Rebuild local state from replicated data
	ItemInstances.Empty();
	for (const FSuspenseCoreReplicatedItem& RepItem : ReplicatedInventory.Items)
	{
		ItemInstances.Add(RepItem.ToItemInstance());
	}

	Config.GridWidth = ReplicatedInventory.GridWidth;
	Config.GridHeight = ReplicatedInventory.GridHeight;
	Config.MaxWeight = ReplicatedInventory.MaxWeight;

	// Rebuild grid slots
	int32 TotalSlots = Config.GridWidth * Config.GridHeight;
	GridSlots.SetNum(TotalSlots);
	for (FSuspenseCoreInventorySlot& Slot : GridSlots)
	{
		Slot.Clear();
	}
	for (const FSuspenseCoreItemInstance& Instance : ItemInstances)
	{
		UpdateGridSlots(Instance, true);
	}

	RecalculateWeight();
	bIsInitialized = true;

	BroadcastInventoryUpdated();
}

void USuspenseCoreInventoryComponent::SubscribeToEvents()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	// Subscribe to add item request
	// (Implementation would use proper subscription mechanism)
}

void USuspenseCoreInventoryComponent::UnsubscribeFromEvents()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	for (FDelegateHandle& Handle : EventSubscriptions)
	{
		// Unsubscribe logic
	}
	EventSubscriptions.Empty();
}

void USuspenseCoreInventoryComponent::OnAddItemRequestEvent(const FSuspenseCoreEventData& EventData)
{
	// Handle add item request from EventBus
}

FIntPoint USuspenseCoreInventoryComponent::SlotToGridCoords(int32 SlotIndex) const
{
	if (Config.GridWidth <= 0)
	{
		return FIntPoint::NoneValue;
	}
	return FIntPoint(SlotIndex % Config.GridWidth, SlotIndex / Config.GridWidth);
}

int32 USuspenseCoreInventoryComponent::GridCoordsToSlot(FIntPoint Coords) const
{
	if (!IsValidGridCoords(Coords))
	{
		return INDEX_NONE;
	}
	return Coords.Y * Config.GridWidth + Coords.X;
}

bool USuspenseCoreInventoryComponent::IsValidGridCoords(FIntPoint Coords) const
{
	return Coords.X >= 0 && Coords.X < Config.GridWidth &&
		   Coords.Y >= 0 && Coords.Y < Config.GridHeight;
}

void USuspenseCoreInventoryComponent::RecalculateWeight()
{
	CurrentWeight = 0.0f;
	USuspenseCoreDataManager* DataManager = GetDataManager();
	if (!DataManager)
	{
		return;
	}

	for (const FSuspenseCoreItemInstance& Instance : ItemInstances)
	{
		FSuspenseCoreItemData ItemData;
		if (DataManager->GetItemData(Instance.ItemID, ItemData))
		{
			CurrentWeight += ItemData.InventoryProps.Weight * Instance.Quantity;
		}
	}
}

void USuspenseCoreInventoryComponent::UpdateGridSlots(const FSuspenseCoreItemInstance& Instance, bool bPlace)
{
	USuspenseCoreDataManager* DataManager = GetDataManager();
	if (!DataManager)
	{
		return;
	}

	FSuspenseCoreItemData ItemData;
	if (!DataManager->GetItemData(Instance.ItemID, ItemData))
	{
		return;
	}

	FIntPoint ItemSize = ItemData.InventoryProps.GridSize;
	bool bRotated = Instance.Rotation != 0;
	FIntPoint EffectiveSize = bRotated ? FIntPoint(ItemSize.Y, ItemSize.X) : ItemSize;
	FIntPoint StartCoords = SlotToGridCoords(Instance.SlotIndex);

	for (int32 Y = 0; Y < EffectiveSize.Y; ++Y)
	{
		for (int32 X = 0; X < EffectiveSize.X; ++X)
		{
			int32 SlotIdx = GridCoordsToSlot(FIntPoint(StartCoords.X + X, StartCoords.Y + Y));
			if (SlotIdx != INDEX_NONE && SlotIdx < GridSlots.Num())
			{
				if (bPlace)
				{
					GridSlots[SlotIdx].InstanceID = Instance.UniqueInstanceID;
					GridSlots[SlotIdx].bIsAnchor = (X == 0 && Y == 0);
					GridSlots[SlotIdx].OffsetFromAnchor = FIntPoint(X, Y);
				}
				else
				{
					GridSlots[SlotIdx].Clear();
				}
			}
		}
	}
}

FSuspenseCoreItemInstance* USuspenseCoreInventoryComponent::FindItemInstanceInternal(const FGuid& InstanceID)
{
	return ItemInstances.FindByPredicate([&InstanceID](const FSuspenseCoreItemInstance& Instance)
	{
		return Instance.UniqueInstanceID == InstanceID;
	});
}

const FSuspenseCoreItemInstance* USuspenseCoreInventoryComponent::FindItemInstanceInternal(const FGuid& InstanceID) const
{
	return ItemInstances.FindByPredicate([&InstanceID](const FSuspenseCoreItemInstance& Instance)
	{
		return Instance.UniqueInstanceID == InstanceID;
	});
}

//==================================================================
// Security Helpers (Phase 6)
//==================================================================

bool USuspenseCoreInventoryComponent::CheckInventoryAuthority(const FString& FunctionName) const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		UE_LOG(LogSuspenseCoreInventory, Verbose, TEXT("%s: No owner"), *FunctionName);
		return false;
	}

	if (!Owner->HasAuthority())
	{
		UE_LOG(LogSuspenseCoreInventory, Verbose, TEXT("%s: Client has no authority on %s"),
			*FunctionName, *Owner->GetName());
		return false;
	}

	return true;
}

//==================================================================
// Server RPCs - Validation (Phase 6)
//==================================================================

bool USuspenseCoreInventoryComponent::Server_AddItemByID_Validate(FName ItemID, int32 Quantity)
{
	// Basic parameter validation
	if (ItemID.IsNone())
	{
		UE_LOG(LogSuspenseCoreInventory, Warning, TEXT("Server_AddItemByID_Validate: ItemID is None"));
		return false;
	}

	if (Quantity <= 0 || Quantity > 9999)
	{
		UE_LOG(LogSuspenseCoreInventory, Warning, TEXT("Server_AddItemByID_Validate: Invalid quantity %d"), Quantity);
		return false;
	}

	// Rate limiting
	USuspenseCoreSecurityValidator* Security = USuspenseCoreSecurityValidator::Get(this);
	if (Security && !Security->CheckRateLimit(GetOwner(), TEXT("AddItem"), 10.0f))
	{
		UE_LOG(LogSuspenseCoreInventory, Warning, TEXT("Server_AddItemByID_Validate: Rate limited"));
		return false;
	}

	return true;
}

bool USuspenseCoreInventoryComponent::Server_RemoveItemByID_Validate(FName ItemID, int32 Quantity)
{
	if (ItemID.IsNone())
	{
		return false;
	}

	if (Quantity <= 0 || Quantity > 9999)
	{
		return false;
	}

	USuspenseCoreSecurityValidator* Security = USuspenseCoreSecurityValidator::Get(this);
	if (Security && !Security->CheckRateLimit(GetOwner(), TEXT("RemoveItem"), 10.0f))
	{
		return false;
	}

	return true;
}

bool USuspenseCoreInventoryComponent::Server_MoveItem_Validate(int32 FromSlot, int32 ToSlot)
{
	const int32 MaxSlots = GetMaxSlots();

	if (FromSlot < 0 || FromSlot >= MaxSlots)
	{
		return false;
	}

	if (ToSlot < 0 || ToSlot >= MaxSlots)
	{
		return false;
	}

	if (FromSlot == ToSlot)
	{
		return false;
	}

	USuspenseCoreSecurityValidator* Security = USuspenseCoreSecurityValidator::Get(this);
	if (Security && !Security->CheckRateLimit(GetOwner(), TEXT("MoveItem"), 20.0f))
	{
		return false;
	}

	return true;
}

bool USuspenseCoreInventoryComponent::Server_SwapItems_Validate(int32 Slot1, int32 Slot2)
{
	const int32 MaxSlots = GetMaxSlots();

	if (Slot1 < 0 || Slot1 >= MaxSlots || Slot2 < 0 || Slot2 >= MaxSlots)
	{
		return false;
	}

	if (Slot1 == Slot2)
	{
		return false;
	}

	USuspenseCoreSecurityValidator* Security = USuspenseCoreSecurityValidator::Get(this);
	if (Security && !Security->CheckRateLimit(GetOwner(), TEXT("SwapItems"), 20.0f))
	{
		return false;
	}

	return true;
}

bool USuspenseCoreInventoryComponent::Server_SplitStack_Validate(int32 SourceSlot, int32 SplitQuantity, int32 TargetSlot)
{
	const int32 MaxSlots = GetMaxSlots();

	if (SourceSlot < 0 || SourceSlot >= MaxSlots)
	{
		return false;
	}

	if (TargetSlot != INDEX_NONE && (TargetSlot < 0 || TargetSlot >= MaxSlots))
	{
		return false;
	}

	if (SplitQuantity <= 0 || SplitQuantity > 9999)
	{
		return false;
	}

	USuspenseCoreSecurityValidator* Security = USuspenseCoreSecurityValidator::Get(this);
	if (Security && !Security->CheckRateLimit(GetOwner(), TEXT("SplitStack"), 10.0f))
	{
		return false;
	}

	return true;
}

bool USuspenseCoreInventoryComponent::Server_RemoveItemFromSlot_Validate(int32 SlotIndex)
{
	const int32 MaxSlots = GetMaxSlots();

	if (SlotIndex < 0 || SlotIndex >= MaxSlots)
	{
		return false;
	}

	USuspenseCoreSecurityValidator* Security = USuspenseCoreSecurityValidator::Get(this);
	if (Security && !Security->CheckRateLimit(GetOwner(), TEXT("RemoveFromSlot"), 10.0f))
	{
		return false;
	}

	return true;
}

//==================================================================
// Server RPCs - Implementation (Phase 6)
//==================================================================

void USuspenseCoreInventoryComponent::Server_AddItemByID_Implementation(FName ItemID, int32 Quantity)
{
	// Execute on server
	FSuspenseCoreItemInstance NewInstance;
	if (CreateItemInstance(ItemID, Quantity, NewInstance))
	{
		AddItemInstance_Implementation(NewInstance);
	}
}

void USuspenseCoreInventoryComponent::Server_RemoveItemByID_Implementation(FName ItemID, int32 Quantity)
{
	// Execute on server - call internal logic directly (already validated)
	if (!bIsInitialized || ItemID.IsNone() || Quantity <= 0)
	{
		return;
	}

	int32 RemainingToRemove = Quantity;

	for (int32 i = ItemInstances.Num() - 1; i >= 0 && RemainingToRemove > 0; --i)
	{
		FSuspenseCoreItemInstance& Instance = ItemInstances[i];
		if (Instance.ItemID == ItemID)
		{
			if (Instance.Quantity <= RemainingToRemove)
			{
				RemainingToRemove -= Instance.Quantity;
				FSuspenseCoreItemInstance RemovedInstance;
				RemoveItemInternal(Instance.UniqueInstanceID, RemovedInstance);
			}
			else
			{
				Instance.Quantity -= RemainingToRemove;
				ReplicatedInventory.UpdateItem(Instance);
				BroadcastItemEvent(SUSPENSE_INV_EVENT_ITEM_QTY_CHANGED, Instance, Instance.SlotIndex);
				RemainingToRemove = 0;
			}
		}
	}

	RecalculateWeight();
	BroadcastInventoryUpdated();
}

void USuspenseCoreInventoryComponent::Server_MoveItem_Implementation(int32 FromSlot, int32 ToSlot)
{
	// Internal move logic
	if (!bIsInitialized || FromSlot == ToSlot)
	{
		return;
	}

	FSuspenseCoreItemInstance Instance;
	if (!GetItemInstanceAtSlot(FromSlot, Instance))
	{
		return;
	}

	FSuspenseCoreItemData ItemData;
	USuspenseCoreDataManager* DataManager = GetDataManager();
	if (!DataManager || !DataManager->GetItemData(Instance.ItemID, ItemData))
	{
		return;
	}

	bool bRotated = Instance.Rotation != 0;
	if (!CanPlaceItemAtSlot(ItemData.InventoryProps.GridSize, ToSlot, bRotated))
	{
		return;
	}

	UpdateGridSlots(Instance, false);

	FSuspenseCoreItemInstance* InstancePtr = FindItemInstanceInternal(Instance.UniqueInstanceID);
	if (InstancePtr)
	{
		InstancePtr->SlotIndex = ToSlot;
		InstancePtr->GridPosition = SlotToGridCoords(ToSlot);
		UpdateGridSlots(*InstancePtr, true);
		ReplicatedInventory.UpdateItem(*InstancePtr);
		BroadcastItemEvent(SUSPENSE_INV_EVENT_ITEM_MOVED, *InstancePtr, ToSlot);
	}

	BroadcastInventoryUpdated();
}

void USuspenseCoreInventoryComponent::Server_SwapItems_Implementation(int32 Slot1, int32 Slot2)
{
	if (!bIsInitialized || Slot1 == Slot2)
	{
		return;
	}

	FSuspenseCoreItemInstance Instance1, Instance2;
	bool bHas1 = GetItemInstanceAtSlot(Slot1, Instance1);
	bool bHas2 = GetItemInstanceAtSlot(Slot2, Instance2);

	if (!bHas1 && !bHas2)
	{
		return;
	}

	if (bHas1) UpdateGridSlots(Instance1, false);
	if (bHas2) UpdateGridSlots(Instance2, false);

	if (bHas1)
	{
		FSuspenseCoreItemInstance* Ptr1 = FindItemInstanceInternal(Instance1.UniqueInstanceID);
		if (Ptr1)
		{
			Ptr1->SlotIndex = Slot2;
			Ptr1->GridPosition = SlotToGridCoords(Slot2);
			UpdateGridSlots(*Ptr1, true);
			ReplicatedInventory.UpdateItem(*Ptr1);
		}
	}

	if (bHas2)
	{
		FSuspenseCoreItemInstance* Ptr2 = FindItemInstanceInternal(Instance2.UniqueInstanceID);
		if (Ptr2)
		{
			Ptr2->SlotIndex = Slot1;
			Ptr2->GridPosition = SlotToGridCoords(Slot1);
			UpdateGridSlots(*Ptr2, true);
			ReplicatedInventory.UpdateItem(*Ptr2);
		}
	}

	BroadcastInventoryUpdated();
}

void USuspenseCoreInventoryComponent::Server_SplitStack_Implementation(int32 SourceSlot, int32 SplitQuantity, int32 TargetSlot)
{
	FSuspenseCoreItemInstance SourceInstance;
	if (!GetItemInstanceAtSlot(SourceSlot, SourceInstance))
	{
		return;
	}

	if (SplitQuantity <= 0 || SplitQuantity >= SourceInstance.Quantity)
	{
		return;
	}

	FSuspenseCoreItemInstance* SourcePtr = FindItemInstanceInternal(SourceInstance.UniqueInstanceID);
	if (!SourcePtr)
	{
		return;
	}

	SourcePtr->Quantity -= SplitQuantity;
	ReplicatedInventory.UpdateItem(*SourcePtr);

	FSuspenseCoreItemInstance NewStack = SourceInstance;
	NewStack.UniqueInstanceID = FGuid::NewGuid();
	NewStack.Quantity = SplitQuantity;
	NewStack.SlotIndex = -1;

	AddItemInstanceToSlot(NewStack, TargetSlot);
}

void USuspenseCoreInventoryComponent::Server_RemoveItemFromSlot_Implementation(int32 SlotIndex)
{
	FSuspenseCoreItemInstance RemovedInstance;
	if (IsSlotOccupied(SlotIndex))
	{
		FGuid InstanceID = GridSlots[SlotIndex].InstanceID;
		RemoveItemInternal(InstanceID, RemovedInstance);
	}
}

//==================================================================
// ISuspenseCoreUIDataProvider - Implementation
//==================================================================

FGameplayTag USuspenseCoreInventoryComponent::GetContainerTypeTag() const
{
	return FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.UIProvider.Type.Inventory")));
}

FSuspenseCoreContainerUIData USuspenseCoreInventoryComponent::GetContainerUIData() const
{
	FSuspenseCoreContainerUIData Data;

	Data.ContainerID = ProviderID;
	Data.ContainerType = ESuspenseCoreContainerType::Inventory;
	Data.ContainerTypeTag = GetContainerTypeTag();
	Data.DisplayName = NSLOCTEXT("SuspenseCore", "Inventory", "INVENTORY");
	Data.LayoutType = ESuspenseCoreSlotLayoutType::Grid;
	Data.GridSize = FIntPoint(Config.GridWidth, Config.GridHeight);
	Data.TotalSlots = Config.GridWidth * Config.GridHeight;
	Data.OccupiedSlots = ItemInstances.Num();
	Data.bHasWeightLimit = Config.MaxWeight > 0.0f;
	Data.CurrentWeight = CurrentWeight;
	Data.MaxWeight = Config.MaxWeight;
	Data.WeightPercent = Config.MaxWeight > 0.0f ? CurrentWeight / Config.MaxWeight : 0.0f;
	Data.AllowedItemTypes = Config.AllowedItemTypes;
	Data.bIsLocked = false;
	Data.bIsReadOnly = false;

	// Fill slots data
	Data.Slots = GetAllSlotUIData();

	// Fill items data
	Data.Items = GetAllItemUIData();

	return Data;
}

TArray<FSuspenseCoreSlotUIData> USuspenseCoreInventoryComponent::GetAllSlotUIData() const
{
	TArray<FSuspenseCoreSlotUIData> Result;
	Result.Reserve(GridSlots.Num());

	for (int32 i = 0; i < GridSlots.Num(); ++i)
	{
		Result.Add(ConvertSlotToUIData(i));
	}

	return Result;
}

FSuspenseCoreSlotUIData USuspenseCoreInventoryComponent::GetSlotUIData(int32 SlotIndex) const
{
	if (!IsSlotValid(SlotIndex))
	{
		return FSuspenseCoreSlotUIData();
	}

	return ConvertSlotToUIData(SlotIndex);
}

bool USuspenseCoreInventoryComponent::IsSlotValid(int32 SlotIndex) const
{
	return SlotIndex >= 0 && SlotIndex < GridSlots.Num();
}

TArray<FSuspenseCoreItemUIData> USuspenseCoreInventoryComponent::GetAllItemUIData() const
{
	TArray<FSuspenseCoreItemUIData> Result;
	Result.Reserve(ItemInstances.Num());

	for (const FSuspenseCoreItemInstance& Instance : ItemInstances)
	{
		Result.Add(ConvertToUIData(Instance));
	}

	return Result;
}

bool USuspenseCoreInventoryComponent::GetItemUIDataAtSlot(int32 SlotIndex, FSuspenseCoreItemUIData& OutItem) const
{
	if (!IsSlotOccupied(SlotIndex))
	{
		return false;
	}

	const FGuid& InstanceID = GridSlots[SlotIndex].InstanceID;
	const FSuspenseCoreItemInstance* Instance = FindItemInstanceInternal(InstanceID);
	if (!Instance)
	{
		return false;
	}

	OutItem = ConvertToUIData(*Instance);
	return true;
}

bool USuspenseCoreInventoryComponent::FindItemUIData(const FGuid& InstanceID, FSuspenseCoreItemUIData& OutItem) const
{
	const FSuspenseCoreItemInstance* Instance = FindItemInstanceInternal(InstanceID);
	if (!Instance)
	{
		return false;
	}

	OutItem = ConvertToUIData(*Instance);
	return true;
}

FSuspenseCoreDropValidation USuspenseCoreInventoryComponent::ValidateDrop(
	const FSuspenseCoreDragData& DragData,
	int32 TargetSlot,
	bool bRotated) const
{
	// Check if slot is valid
	if (!IsSlotValid(TargetSlot))
	{
		return FSuspenseCoreDropValidation::Invalid(
			NSLOCTEXT("SuspenseCore", "InvalidSlot", "Invalid slot"));
	}

	// Get item size
	FIntPoint ItemSize = DragData.Item.GetEffectiveSize();
	if (bRotated)
	{
		ItemSize = FIntPoint(ItemSize.Y, ItemSize.X);
	}

	// Check if can place
	if (!CanPlaceItemAtSlot(ItemSize, TargetSlot, bRotated))
	{
		// Check if this is from same container and same slot - allow rotation in place
		if (DragData.SourceContainerType == ESuspenseCoreContainerType::Inventory &&
			DragData.SourceSlot == TargetSlot)
		{
			return FSuspenseCoreDropValidation::Valid();
		}

		return FSuspenseCoreDropValidation::Invalid(
			NSLOCTEXT("SuspenseCore", "NoSpace", "Not enough space"));
	}

	// Check weight
	if (HasWeightLimit())
	{
		float ItemWeight = DragData.Item.TotalWeight;
		// If from same container, weight is already counted
		if (DragData.SourceContainerType != ESuspenseCoreContainerType::Inventory)
		{
			if (CurrentWeight + ItemWeight > Config.MaxWeight)
			{
				return FSuspenseCoreDropValidation::Invalid(
					NSLOCTEXT("SuspenseCore", "WeightLimit", "Weight limit exceeded"));
			}
		}
	}

	// Check item type restrictions
	if (!CanAcceptItemType(DragData.Item.ItemType))
	{
		return FSuspenseCoreDropValidation::Invalid(
			NSLOCTEXT("SuspenseCore", "TypeNotAllowed", "Item type not allowed"));
	}

	return FSuspenseCoreDropValidation::Valid();
}

bool USuspenseCoreInventoryComponent::CanAcceptItemType(const FGameplayTag& ItemType) const
{
	if (Config.AllowedItemTypes.Num() > 0)
	{
		if (!Config.AllowedItemTypes.HasTag(ItemType))
		{
			return false;
		}
	}

	if (Config.DisallowedItemTypes.HasTag(ItemType))
	{
		return false;
	}

	return true;
}

int32 USuspenseCoreInventoryComponent::FindBestSlotForItem(FIntPoint ItemSize, bool bAllowRotation) const
{
	return FindFreeSlot(ItemSize, bAllowRotation);
}

//==================================================================
// ISuspenseCoreUIDataProvider - Grid Position Calculations
//==================================================================

int32 USuspenseCoreInventoryComponent::GetSlotAtLocalPosition(const FVector2D& LocalPos, float CellSize, float CellGap) const
{
	if (LocalPos.X < 0 || LocalPos.Y < 0)
	{
		return INDEX_NONE;
	}

	// Calculate which cell the position falls into
	float TotalCellSize = CellSize + CellGap;
	int32 Column = FMath::FloorToInt(LocalPos.X / TotalCellSize);
	int32 Row = FMath::FloorToInt(LocalPos.Y / TotalCellSize);

	// Validate bounds
	if (Column < 0 || Column >= Config.GridWidth || Row < 0 || Row >= Config.GridHeight)
	{
		return INDEX_NONE;
	}

	return GridCoordsToSlot(FIntPoint(Column, Row));
}

TArray<int32> USuspenseCoreInventoryComponent::GetOccupiedSlotsForItem(const FGuid& ItemInstanceID) const
{
	TArray<int32> Result;

	if (!ItemInstanceID.IsValid())
	{
		return Result;
	}

	// Find the item instance
	const FSuspenseCoreItemInstance* Instance = FindItemInstanceInternal(ItemInstanceID);
	if (!Instance)
	{
		return Result;
	}

	// Get item data for size
	USuspenseCoreDataManager* DataManager = GetDataManager();
	if (!DataManager)
	{
		Result.Add(Instance->SlotIndex);
		return Result;
	}

	FSuspenseCoreItemData ItemData;
	if (!DataManager->GetItemData(Instance->ItemID, ItemData))
	{
		Result.Add(Instance->SlotIndex);
		return Result;
	}

	// Calculate effective size (considering rotation)
	FIntPoint ItemSize = ItemData.InventoryProps.GridSize;
	bool bRotated = Instance->Rotation != 0;
	FIntPoint EffectiveSize = bRotated ? FIntPoint(ItemSize.Y, ItemSize.X) : ItemSize;
	FIntPoint StartCoords = SlotToGridCoords(Instance->SlotIndex);

	// Collect all occupied slots
	for (int32 Y = 0; Y < EffectiveSize.Y; ++Y)
	{
		for (int32 X = 0; X < EffectiveSize.X; ++X)
		{
			int32 SlotIdx = GridCoordsToSlot(FIntPoint(StartCoords.X + X, StartCoords.Y + Y));
			if (SlotIdx != INDEX_NONE)
			{
				Result.Add(SlotIdx);
			}
		}
	}

	return Result;
}

int32 USuspenseCoreInventoryComponent::GetAnchorSlotForPosition(int32 AnySlotIndex) const
{
	if (!IsSlotValid(AnySlotIndex))
	{
		return INDEX_NONE;
	}

	if (!IsSlotOccupied(AnySlotIndex))
	{
		return AnySlotIndex; // Empty slot - return as-is
	}

	// Get the instance ID at this slot
	const FGuid& InstanceID = GridSlots[AnySlotIndex].InstanceID;

	// Find the instance to get its anchor slot
	const FSuspenseCoreItemInstance* Instance = FindItemInstanceInternal(InstanceID);
	if (Instance)
	{
		return Instance->SlotIndex; // SlotIndex is always the anchor slot
	}

	return AnySlotIndex;
}

bool USuspenseCoreInventoryComponent::CanPlaceItemAtSlot(const FGuid& ItemID, int32 SlotIndex, bool bRotated) const
{
	if (!bIsInitialized || !IsSlotValid(SlotIndex))
	{
		return false;
	}

	// If no item ID specified, just check if slot is empty
	if (!ItemID.IsValid())
	{
		return !IsSlotOccupied(SlotIndex);
	}

	// Find the item to get its size
	const FSuspenseCoreItemInstance* Instance = FindItemInstanceInternal(ItemID);
	if (!Instance)
	{
		return false;
	}

	// Get item data for size
	USuspenseCoreDataManager* DataManager = GetDataManager();
	if (!DataManager)
	{
		return false;
	}

	FSuspenseCoreItemData ItemData;
	if (!DataManager->GetItemData(Instance->ItemID, ItemData))
	{
		return false;
	}

	FIntPoint ItemSize = ItemData.InventoryProps.GridSize;

	// Check placement using existing method
	return CanPlaceItemAtSlot(ItemSize, SlotIndex, bRotated);
}

bool USuspenseCoreInventoryComponent::RequestMoveItem(int32 FromSlot, int32 ToSlot, bool bRotate)
{
	// Use existing MoveItem which handles server RPC
	return MoveItem_Implementation(FromSlot, ToSlot);
}

bool USuspenseCoreInventoryComponent::RequestRotateItem(int32 SlotIndex)
{
	return RotateItemAtSlot(SlotIndex);
}

bool USuspenseCoreInventoryComponent::RequestUseItem(int32 SlotIndex)
{
	// TODO: Implement use item via EventBus
	FSuspenseCoreItemInstance Instance;
	if (!GetItemInstanceAtSlot(SlotIndex, Instance))
	{
		return false;
	}

	// Broadcast use item request via EventBus
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (EventBus)
	{
		FSuspenseCoreEventData EventData;
		EventData.Source = const_cast<USuspenseCoreInventoryComponent*>(this);
		EventData.SetString(TEXT("InstanceID"), Instance.UniqueInstanceID.ToString());
		EventData.SetString(TEXT("ItemID"), Instance.ItemID.ToString());
		EventData.SetInt(TEXT("SlotIndex"), SlotIndex);
		EventBus->Publish(FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.UIRequest.UseItem"))), EventData);
	}

	return true;
}

bool USuspenseCoreInventoryComponent::RequestDropItem(int32 SlotIndex, int32 Quantity)
{
	// TODO: Implement drop item via EventBus
	FSuspenseCoreItemInstance Instance;
	if (!GetItemInstanceAtSlot(SlotIndex, Instance))
	{
		return false;
	}

	// Broadcast drop item request
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (EventBus)
	{
		FSuspenseCoreEventData EventData;
		EventData.Source = const_cast<USuspenseCoreInventoryComponent*>(this);
		EventData.SetString(TEXT("InstanceID"), Instance.UniqueInstanceID.ToString());
		EventData.SetString(TEXT("ItemID"), Instance.ItemID.ToString());
		EventData.SetInt(TEXT("SlotIndex"), SlotIndex);
		EventData.SetInt(TEXT("Quantity"), Quantity > 0 ? Quantity : Instance.Quantity);
		EventBus->Publish(FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.UIRequest.DropItem"))), EventData);
	}

	return true;
}

bool USuspenseCoreInventoryComponent::RequestSplitStack(int32 SlotIndex, int32 SplitQuantity, int32 TargetSlot)
{
	return SplitStack(SlotIndex, SplitQuantity, TargetSlot);
}

bool USuspenseCoreInventoryComponent::RequestTransferItem(
	int32 SlotIndex,
	const FGuid& TargetProviderID,
	int32 TargetSlot,
	int32 Quantity)
{
	// Broadcast transfer request via EventBus
	FSuspenseCoreItemInstance Instance;
	if (!GetItemInstanceAtSlot(SlotIndex, Instance))
	{
		return false;
	}

	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (EventBus)
	{
		FSuspenseCoreEventData EventData;
		EventData.Source = const_cast<USuspenseCoreInventoryComponent*>(this);
		EventData.SetString(TEXT("InstanceID"), Instance.UniqueInstanceID.ToString());
		EventData.SetString(TEXT("ItemID"), Instance.ItemID.ToString());
		EventData.SetInt(TEXT("SourceSlot"), SlotIndex);
		EventData.SetString(TEXT("TargetProviderID"), TargetProviderID.ToString());
		EventData.SetInt(TEXT("TargetSlot"), TargetSlot);
		EventData.SetInt(TEXT("Quantity"), Quantity > 0 ? Quantity : Instance.Quantity);
		EventBus->Publish(FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.UIRequest.TransferItem"))), EventData);
	}

	return true;
}

TArray<FGameplayTag> USuspenseCoreInventoryComponent::GetItemContextActions(int32 SlotIndex) const
{
	TArray<FGameplayTag> Actions;

	FSuspenseCoreItemInstance Instance;
	if (!GetItemInstanceAtSlot(SlotIndex, Instance))
	{
		return Actions;
	}

	// Get item data for capabilities
	USuspenseCoreDataManager* DataManager = GetDataManager();
	if (!DataManager)
	{
		return Actions;
	}

	FSuspenseCoreItemData ItemData;
	if (!DataManager->GetItemData(Instance.ItemID, ItemData))
	{
		return Actions;
	}

	// Always available
	Actions.Add(FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.UIAction.Examine"))));

	// Use if usable
	if (ItemData.Behavior.bIsConsumable)
	{
		Actions.Add(FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.UIAction.Use"))));
	}

	// Equip if equippable
	if (ItemData.Behavior.bIsEquippable)
	{
		Actions.Add(FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.UIAction.Equip"))));
	}

	// Split if stackable and quantity > 1
	if (ItemData.InventoryProps.IsStackable() && Instance.Quantity > 1)
	{
		Actions.Add(FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.UIAction.Split"))));
	}

	// Drop
	Actions.Add(FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.UIAction.Drop"))));

	// Discard
	Actions.Add(FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.UIAction.Discard"))));

	return Actions;
}

bool USuspenseCoreInventoryComponent::ExecuteContextAction(int32 SlotIndex, const FGameplayTag& ActionTag)
{
	static const FGameplayTag UseTag = FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.UIAction.Use")));
	static const FGameplayTag EquipTag = FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.UIAction.Equip")));
	static const FGameplayTag DropTag = FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.UIAction.Drop")));
	static const FGameplayTag SplitTag = FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.UIAction.Split")));
	static const FGameplayTag DiscardTag = FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.UIAction.Discard")));
	static const FGameplayTag ExamineTag = FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.UIAction.Examine")));

	if (ActionTag == UseTag)
	{
		return RequestUseItem(SlotIndex);
	}
	else if (ActionTag == EquipTag)
	{
		// Broadcast equip request
		FSuspenseCoreItemInstance Instance;
		if (GetItemInstanceAtSlot(SlotIndex, Instance))
		{
			USuspenseCoreEventBus* EventBus = GetEventBus();
			if (EventBus)
			{
				FSuspenseCoreEventData EventData;
				EventData.Source = const_cast<USuspenseCoreInventoryComponent*>(this);
				EventData.SetString(TEXT("InstanceID"), Instance.UniqueInstanceID.ToString());
				EventData.SetInt(TEXT("SlotIndex"), SlotIndex);
				EventBus->Publish(FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.UIRequest.EquipItem"))), EventData);
			}
			return true;
		}
		return false;
	}
	else if (ActionTag == DropTag)
	{
		return RequestDropItem(SlotIndex, 0);
	}
	else if (ActionTag == SplitTag)
	{
		// Split half the stack to next available slot
		FSuspenseCoreItemInstance Instance;
		if (GetItemInstanceAtSlot(SlotIndex, Instance) && Instance.Quantity > 1)
		{
			int32 SplitQty = Instance.Quantity / 2;
			return RequestSplitStack(SlotIndex, SplitQty, INDEX_NONE);
		}
		return false;
	}
	else if (ActionTag == DiscardTag)
	{
		// Remove without dropping
		FSuspenseCoreItemInstance RemovedInstance;
		return RemoveItemFromSlot(SlotIndex, RemovedInstance);
	}
	else if (ActionTag == ExamineTag)
	{
		// Broadcast examine event for UI to show details
		FSuspenseCoreItemInstance Instance;
		if (GetItemInstanceAtSlot(SlotIndex, Instance))
		{
			USuspenseCoreEventBus* EventBus = GetEventBus();
			if (EventBus)
			{
				FSuspenseCoreEventData EventData;
				EventData.Source = const_cast<USuspenseCoreInventoryComponent*>(this);
				EventData.SetString(TEXT("InstanceID"), Instance.UniqueInstanceID.ToString());
				EventData.SetString(TEXT("ItemID"), Instance.ItemID.ToString());
				EventBus->Publish(FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.UIAction.Examine"))), EventData);
			}
			return true;
		}
		return false;
	}

	return false;
}

//==================================================================
// UI Data Provider - Conversion Helpers
//==================================================================

FSuspenseCoreItemUIData USuspenseCoreInventoryComponent::ConvertToUIData(const FSuspenseCoreItemInstance& Instance) const
{
	FSuspenseCoreItemUIData UIData;

	UIData.InstanceID = Instance.UniqueInstanceID;
	UIData.ItemID = Instance.ItemID;
	UIData.AnchorSlot = Instance.SlotIndex;
	UIData.Quantity = Instance.Quantity;
	UIData.bIsRotated = Instance.Rotation != 0;

	// Get static data from DataManager
	USuspenseCoreDataManager* DataManager = GetDataManager();
	if (DataManager)
	{
		FSuspenseCoreItemData ItemData;
		if (DataManager->GetItemData(Instance.ItemID, ItemData))
		{
			UIData.DisplayName = ItemData.Identity.DisplayName;
			UIData.Description = ItemData.Identity.Description;
			UIData.IconPath = ItemData.Identity.Icon.ToSoftObjectPath();
			UIData.ItemType = ItemData.Classification.ItemType;
			UIData.RarityTag = ItemData.Classification.Rarity;
			UIData.GridSize = ItemData.InventoryProps.GridSize;
			UIData.MaxStackSize = ItemData.InventoryProps.MaxStackSize;
			UIData.bIsStackable = ItemData.InventoryProps.IsStackable();
			UIData.UnitWeight = ItemData.InventoryProps.Weight;
			UIData.TotalWeight = ItemData.InventoryProps.Weight * Instance.Quantity;
			UIData.bIsEquippable = ItemData.Behavior.bIsEquippable;
			UIData.bIsUsable = ItemData.Behavior.bIsConsumable;
			UIData.bIsDroppable = ItemData.Behavior.bCanDrop;
			UIData.bIsTradeable = ItemData.Behavior.bCanTrade;
		}
	}

	return UIData;
}

FSuspenseCoreSlotUIData USuspenseCoreInventoryComponent::ConvertSlotToUIData(int32 SlotIndex) const
{
	FSuspenseCoreSlotUIData SlotData;
	SlotData.SlotIndex = SlotIndex;
	SlotData.GridPosition = SlotToGridCoords(SlotIndex);

	if (SlotIndex >= 0 && SlotIndex < GridSlots.Num())
	{
		const FSuspenseCoreInventorySlot& Slot = GridSlots[SlotIndex];
		SlotData.bIsAnchor = Slot.bIsAnchor;
		SlotData.bIsPartOfItem = !Slot.IsEmpty() && !Slot.bIsAnchor;
		SlotData.OccupyingItemID = Slot.InstanceID;

		if (Slot.IsEmpty())
		{
			SlotData.State = ESuspenseCoreUISlotState::Empty;
		}
		else
		{
			SlotData.State = ESuspenseCoreUISlotState::Occupied;
		}
	}
	else
	{
		SlotData.State = ESuspenseCoreUISlotState::Invalid;
	}

	return SlotData;
}

void USuspenseCoreInventoryComponent::BroadcastUIDataChanged(const FGameplayTag& ChangeType, const FGuid& AffectedItemID)
{
	UIDataChangedDelegate.Broadcast(ChangeType, AffectedItemID);

	// Also broadcast via EventBus
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (EventBus)
	{
		FSuspenseCoreEventData EventData;
		EventData.Source = const_cast<USuspenseCoreInventoryComponent*>(this);
		EventData.SetString(TEXT("ProviderID"), ProviderID.ToString());
		EventData.SetString(TEXT("AffectedItemID"), AffectedItemID.ToString());
		EventBus->Publish(FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.UIProvider.DataChanged"))), EventData);
	}
}
