// SuspenseCoreInventoryComponent.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Components/SuspenseCoreInventoryComponent.h"
#include "SuspenseCore/Storage/SuspenseCoreInventoryStorage.h"
#include "SuspenseCore/Validation/SuspenseCoreInventoryValidator.h"
#include "SuspenseCore/Events/Inventory/SuspenseCoreInventoryEvents.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

DEFINE_LOG_CATEGORY(LogSuspenseCoreInventory);

USuspenseCoreInventoryComponent::USuspenseCoreInventoryComponent()
	: CurrentWeight(0.0f)
	, bIsInitialized(false)
	, bTransactionActive(false)
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

	// Try to add
	return AddItemInstance_Implementation(NewInstance);
}

bool USuspenseCoreInventoryComponent::AddItemInstance_Implementation(const FSuspenseCoreItemInstance& ItemInstance)
{
	return AddItemInstanceToSlot(ItemInstance, INDEX_NONE);
}

bool USuspenseCoreInventoryComponent::AddItemInstanceToSlot(const FSuspenseCoreItemInstance& ItemInstance, int32 TargetSlot)
{
	check(IsInGameThread());

	if (!bIsInitialized)
	{
		BroadcastErrorEvent(ESuspenseCoreInventoryResult::NotInitialized, TEXT("Inventory not initialized"));
		return false;
	}

	if (!ItemInstance.IsValid())
	{
		BroadcastErrorEvent(ESuspenseCoreInventoryResult::InvalidItem, TEXT("Invalid item instance"));
		return false;
	}

	// Get item data for validation
	FSuspenseCoreItemData ItemData;
	USuspenseCoreDataManager* DataManager = GetDataManager();
	if (!DataManager || !DataManager->GetItemData(ItemInstance.ItemID, ItemData))
	{
		BroadcastErrorEvent(ESuspenseCoreInventoryResult::ItemNotFound,
			FString::Printf(TEXT("Item %s not found in DataTable"), *ItemInstance.ItemID.ToString()));
		return false;
	}

	// Check weight
	float ItemWeight = ItemData.InventoryProps.Weight * ItemInstance.Quantity;
	if (CurrentWeight + ItemWeight > Config.MaxWeight)
	{
		BroadcastErrorEvent(ESuspenseCoreInventoryResult::WeightLimitExceeded,
			FString::Printf(TEXT("Weight limit exceeded (Current: %.1f, Adding: %.1f, Max: %.1f)"),
				CurrentWeight, ItemWeight, Config.MaxWeight));
		return false;
	}

	// Check type restrictions
	if (Config.AllowedItemTypes.Num() > 0 && !Config.AllowedItemTypes.HasTag(ItemData.Classification.ItemType))
	{
		BroadcastErrorEvent(ESuspenseCoreInventoryResult::TypeNotAllowed,
			FString::Printf(TEXT("Item type %s not allowed"), *ItemData.Classification.ItemType.ToString()));
		return false;
	}

	if (Config.DisallowedItemTypes.HasTag(ItemData.Classification.ItemType))
	{
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
		PlacementSlot = FindFreeSlot(ItemData.InventoryProps.GridSize, Config.bAllowRotation);
	}

	if (PlacementSlot == INDEX_NONE)
	{
		BroadcastErrorEvent(ESuspenseCoreInventoryResult::NoSpace, TEXT("No space available in inventory"));
		return false;
	}

	// Check placement validity
	if (!CanPlaceItemAtSlot(ItemData.InventoryProps.GridSize, PlacementSlot, false))
	{
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
	// TODO: Load from loadout DataTable
	Initialize(Config.GridWidth, Config.GridHeight, Config.MaxWeight);
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
