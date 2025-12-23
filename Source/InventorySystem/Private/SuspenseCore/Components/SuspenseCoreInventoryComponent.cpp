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

//==================================================================
// Performance Profiling (STAT Group)
//==================================================================
DECLARE_STATS_GROUP(TEXT("SuspenseCore"), STATGROUP_SuspenseCore, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Inventory AddItem"), STAT_Inventory_AddItem, STATGROUP_SuspenseCore);
DECLARE_CYCLE_STAT(TEXT("Inventory RemoveItem"), STAT_Inventory_RemoveItem, STATGROUP_SuspenseCore);
DECLARE_CYCLE_STAT(TEXT("Inventory FindFreeSlot"), STAT_Inventory_FindFreeSlot, STATGROUP_SuspenseCore);
DECLARE_CYCLE_STAT(TEXT("Inventory GetUIData"), STAT_Inventory_GetUIData, STATGROUP_SuspenseCore);
DECLARE_CYCLE_STAT(TEXT("Inventory OnRep"), STAT_Inventory_OnRep, STATGROUP_SuspenseCore);
DECLARE_CYCLE_STAT(TEXT("Inventory RecalculateWeight"), STAT_Inventory_RecalculateWeight, STATGROUP_SuspenseCore);

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

//==================================================================
// Save Data Versioning
//==================================================================

void USuspenseCoreInventoryComponent::PostLoad()
{
	Super::PostLoad();

	// Check if save data needs migration
	if (SaveDataVersion < SUSPENSECORE_INVENTORY_SAVE_VERSION)
	{
		UE_LOG(LogSuspenseCoreInventory, Log,
			TEXT("PostLoad: Migrating inventory save data from v%d to v%d"),
			SaveDataVersion, SUSPENSECORE_INVENTORY_SAVE_VERSION);

		MigrateSaveData(SaveDataVersion, SUSPENSECORE_INVENTORY_SAVE_VERSION);
		SaveDataVersion = SUSPENSECORE_INVENTORY_SAVE_VERSION;
	}
}

void USuspenseCoreInventoryComponent::MigrateSaveData(int32 FromVersion, int32 ToVersion)
{
	UE_LOG(LogSuspenseCoreInventory, Log,
		TEXT("MigrateSaveData: Starting migration from v%d to v%d"),
		FromVersion, ToVersion);

	// Version 0 -> 1: Ensure all items have valid UniqueInstanceID
	if (FromVersion == 0 && ToVersion >= 1)
	{
		int32 MigratedCount = 0;
		for (FSuspenseCoreItemInstance& Instance : ItemInstances)
		{
			if (!Instance.UniqueInstanceID.IsValid())
			{
				Instance.UniqueInstanceID = FGuid::NewGuid();
				++MigratedCount;
			}
		}

		if (MigratedCount > 0)
		{
			UE_LOG(LogSuspenseCoreInventory, Log,
				TEXT("MigrateSaveData v0->v1: Generated %d missing InstanceIDs"),
				MigratedCount);
		}

		// Rebuild grid slots to match items
		if (Config.GridWidth > 0 && Config.GridHeight > 0)
		{
			// Initialize GridStorage (SSOT)
			EnsureStorageInitialized();

			for (const FSuspenseCoreItemInstance& Instance : ItemInstances)
			{
				UpdateGridSlots(Instance, true);
			}
		}

		// Recalculate weight
		RecalculateWeight();
	}

	// FUTURE MIGRATIONS:
	// Version 1 -> 2: Example - add new RuntimeProperties field
	// if (FromVersion <= 1 && ToVersion >= 2)
	// {
	//     for (FSuspenseCoreItemInstance& Instance : ItemInstances)
	//     {
	//         Instance.RuntimeProperties.Add(TEXT("MigratedFromV1"), TEXT("true"));
	//     }
	// }

	UE_LOG(LogSuspenseCoreInventory, Log,
		TEXT("MigrateSaveData: Completed migration to v%d. Items: %d"),
		ToVersion, ItemInstances.Num());
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
	SCOPE_CYCLE_COUNTER(STAT_Inventory_AddItem);
	check(IsInGameThread());

	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("AddItemInstanceToSlot: ItemID=%s, Quantity=%d, TargetSlot=%d"),
		*ItemInstance.ItemID.ToString(), ItemInstance.Quantity, TargetSlot);

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

	// Get item data for validation (single lookup, cached for iteration)
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

	// Cache item weight for incremental updates
	const float UnitWeight = ItemData.InventoryProps.Weight;
	const int32 MaxStackSize = ItemData.InventoryProps.MaxStackSize;

	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("AddItemInstanceToSlot: ItemData loaded - GridSize=%dx%d, Weight=%.2f"),
		ItemData.InventoryProps.GridSize.X, ItemData.InventoryProps.GridSize.Y, UnitWeight);

	// Check total weight for entire quantity
	float TotalItemWeight = UnitWeight * ItemInstance.Quantity;
	if (CurrentWeight + TotalItemWeight > Config.MaxWeight)
	{
		UE_LOG(LogSuspenseCoreInventory, Warning, TEXT("AddItemInstanceToSlot: Weight exceeded (%.1f + %.1f > %.1f)"),
			CurrentWeight, TotalItemWeight, Config.MaxWeight);
		BroadcastErrorEvent(ESuspenseCoreInventoryResult::WeightLimitExceeded,
			FString::Printf(TEXT("Weight limit exceeded (Current: %.1f, Adding: %.1f, Max: %.1f)"),
				CurrentWeight, TotalItemWeight, Config.MaxWeight));
		return false;
	}

	// Check type restrictions
	if (Config.AllowedItemTypes.Num() > 0 && !Config.AllowedItemTypes.HasTag(ItemData.Classification.ItemType))
	{
		FString AllowedTypesStr;
		for (const FGameplayTag& Tag : Config.AllowedItemTypes)
		{
			if (!AllowedTypesStr.IsEmpty()) AllowedTypesStr += TEXT(", ");
			AllowedTypesStr += Tag.ToString();
		}
		UE_LOG(LogSuspenseCoreInventory, Warning, TEXT("AddItemInstanceToSlot: Type %s not in allowed types. Allowed: [%s]"),
			*ItemData.Classification.ItemType.ToString(), *AllowedTypesStr);
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

	// ITERATIVE APPROACH (replaced recursion for stack safety)
	// Tracks remaining quantity to add across multiple stacks/slots
	int32 RemainingQuantity = ItemInstance.Quantity;
	int32 TotalAdded = 0;
	int32 CurrentTargetSlot = TargetSlot;

	// Infinite loop protection
	int32 MaxIterations = ItemInstance.Quantity + GetTotalSlotCount();
	int32 IterationCount = 0;

	while (RemainingQuantity > 0 && IterationCount < MaxIterations)
	{
		++IterationCount;

		// Try auto-stacking first
		if (Config.bAutoStack && ItemData.InventoryProps.IsStackable())
		{
			for (FSuspenseCoreItemInstance& ExistingInstance : ItemInstances)
			{
				if (ExistingInstance.CanStackWith(ItemInstance))
				{
					int32 SpaceInStack = MaxStackSize - ExistingInstance.Quantity;
					if (SpaceInStack > 0)
					{
						int32 ToAdd = FMath::Min(SpaceInStack, RemainingQuantity);
						ExistingInstance.Quantity += ToAdd;
						RemainingQuantity -= ToAdd;
						TotalAdded += ToAdd;

						// Update replication
						ReplicatedInventory.UpdateItem(ExistingInstance);

						// Incremental weight update (O(1) instead of O(n))
						UpdateWeightDelta(UnitWeight * ToAdd);

						// Broadcast event
						BroadcastItemEvent(SUSPENSE_INV_EVENT_ITEM_QTY_CHANGED, ExistingInstance, ExistingInstance.SlotIndex);

						if (RemainingQuantity == 0)
						{
							// Invalidate UI cache and notify
							InvalidateAllUICache();
							BroadcastInventoryUpdated();

							UE_LOG(LogSuspenseCoreInventory, Log, TEXT("Added item %s x%d (stacked into existing)"),
								*ItemInstance.ItemID.ToString(), TotalAdded);
							return true;
						}
					}
				}
			}
		}

		// Need to create new stack - find slot
		int32 PlacementSlot = CurrentTargetSlot;
		if (PlacementSlot == INDEX_NONE)
		{
			PlacementSlot = FindFreeSlot(ItemData.InventoryProps.GridSize, Config.bAllowRotation);
		}

		if (PlacementSlot == INDEX_NONE)
		{
			// No more space - partial success
			if (TotalAdded > 0)
			{
				UE_LOG(LogSuspenseCoreInventory, Warning,
					TEXT("AddItemInstanceToSlot: Partial add - added %d of %d items (no more space)"),
					TotalAdded, ItemInstance.Quantity);
				InvalidateAllUICache();
				BroadcastInventoryUpdated();
				return true; // Partial success
			}

			UE_LOG(LogSuspenseCoreInventory, Warning, TEXT("AddItemInstanceToSlot: No free slot found"));
			BroadcastErrorEvent(ESuspenseCoreInventoryResult::NoSpace, TEXT("No space available in inventory"));
			return false;
		}

		// Check placement validity
		if (!CanPlaceItemAtSlot(ItemData.InventoryProps.GridSize, PlacementSlot, false))
		{
			if (CurrentTargetSlot != INDEX_NONE)
			{
				// Specific slot requested but occupied - try auto-find
				CurrentTargetSlot = INDEX_NONE;
				continue;
			}

			UE_LOG(LogSuspenseCoreInventory, Warning, TEXT("AddItemInstanceToSlot: Cannot place at slot %d"), PlacementSlot);
			BroadcastErrorEvent(ESuspenseCoreInventoryResult::SlotOccupied,
				FString::Printf(TEXT("Cannot place item at slot %d"), PlacementSlot));
			return TotalAdded > 0;
		}

		// Calculate quantity for this stack
		int32 QuantityForThisStack = FMath::Min(RemainingQuantity, MaxStackSize);

		// Create new instance for this stack
		FSuspenseCoreItemInstance NewInstance = ItemInstance;
		NewInstance.UniqueInstanceID = FGuid::NewGuid();
		NewInstance.Quantity = QuantityForThisStack;
		NewInstance.SlotIndex = PlacementSlot;
		NewInstance.GridPosition = SlotToGridCoords(PlacementSlot);

		// Add to inventory
		ItemInstances.Add(NewInstance);
		UpdateGridSlots(NewInstance, true);
		ReplicatedInventory.AddItem(NewInstance);

		// Incremental weight update
		UpdateWeightDelta(UnitWeight * QuantityForThisStack);

		// Update counters
		RemainingQuantity -= QuantityForThisStack;
		TotalAdded += QuantityForThisStack;

		// Broadcast events
		BroadcastItemEvent(SUSPENSE_INV_EVENT_ITEM_ADDED, NewInstance, PlacementSlot);

		UE_LOG(LogSuspenseCoreInventory, Log, TEXT("Added item %s x%d to slot %d"),
			*ItemInstance.ItemID.ToString(), QuantityForThisStack, PlacementSlot);

		// Next iteration should auto-find slot
		CurrentTargetSlot = INDEX_NONE;
	}

	// Check for infinite loop detection
	if (IterationCount >= MaxIterations)
	{
		UE_LOG(LogSuspenseCoreInventory, Error,
			TEXT("AddItemInstanceToSlot: Infinite loop detected! Added %d of %d items before abort."),
			TotalAdded, ItemInstance.Quantity);
	}

	// Invalidate UI cache and broadcast final update
	InvalidateAllUICache();
	BroadcastInventoryUpdated();

#if !UE_BUILD_SHIPPING
	// Validate integrity in development builds
	ValidateInventoryIntegrityInternal(TEXT("AddItemInstanceToSlot"));
#endif

	return TotalAdded > 0;
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

	FGuid InstanceID = GetInstanceIDAtSlot(SlotIndex);
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
	if (!IsValidSlotIndex(SlotIndex) || !IsSlotOccupied(SlotIndex))
	{
		return false;
	}

	const FGuid InstanceID = GetInstanceIDAtSlot(SlotIndex);
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

	// Invalidate UI cache before broadcasting update
	InvalidateAllUICache();
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

	// Invalidate UI cache before broadcasting update
	InvalidateAllUICache();
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

	// Invalidate UI cache before broadcasting update
	InvalidateAllUICache();
	BroadcastInventoryUpdated();
	return true;
}

bool USuspenseCoreInventoryComponent::IsSlotOccupied(int32 SlotIndex) const
{
	// Use GridStorage as SSOT
	if (GridStorage && GridStorage->IsInitialized())
	{
		return GridStorage->IsSlotOccupied(SlotIndex);
	}
	return false;
}

int32 USuspenseCoreInventoryComponent::FindFreeSlot(FIntPoint ItemGridSize, bool bAllowRotation) const
{
	SCOPE_CYCLE_COUNTER(STAT_Inventory_FindFreeSlot);

	// Delegate to Storage if available (uses FreeSlotBitmap for faster queries)
	if (GridStorage && GridStorage->IsInitialized())
	{
		bool bRotated = false;
		int32 FoundSlot = GridStorage->FindFreeSlot(ItemGridSize, bAllowRotation, bRotated);
		if (FoundSlot != INDEX_NONE)
		{
			LastFreeSlotHint = FoundSlot;
		}
		return FoundSlot;
	}

	// Fallback to local implementation (for backwards compatibility)
	const int32 TotalSlots = GetTotalSlotCount();
	if (TotalSlots == 0)
	{
		return INDEX_NONE;
	}

	// HEURISTIC OPTIMIZATION: Start search from last successful slot
	// This dramatically improves performance when adding items sequentially
	const int32 StartSlot = FMath::Clamp(LastFreeSlotHint, 0, TotalSlots - 1);

	// First pass: from hint to end
	for (int32 i = 0; i < TotalSlots; ++i)
	{
		const int32 SlotIndex = (StartSlot + i) % TotalSlots;

		// Try normal orientation first
		if (CanPlaceItemAtSlot(ItemGridSize, SlotIndex, false))
		{
			LastFreeSlotHint = SlotIndex; // Update hint for next search
			return SlotIndex;
		}

		// Try rotated if allowed
		if (bAllowRotation && CanPlaceItemAtSlot(ItemGridSize, SlotIndex, true))
		{
			LastFreeSlotHint = SlotIndex;
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
	const int32 TotalSlots = GetTotalSlotCount();

	// Check grid slots match items (using GridStorage as SSOT)
	for (const FSuspenseCoreItemInstance& Instance : ItemInstances)
	{
		if (Instance.SlotIndex < 0 || Instance.SlotIndex >= TotalSlots)
		{
			OutErrors.Add(FString::Printf(TEXT("Item %s has invalid slot %d"), *Instance.ItemID.ToString(), Instance.SlotIndex));
			bValid = false;
		}
		else if (GetInstanceIDAtSlot(Instance.SlotIndex) != Instance.UniqueInstanceID)
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

	// Capture slot state from GridStorage (SSOT)
	const int32 TotalSlots = GetTotalSlotCount();
	TransactionSnapshot.Slots.SetNum(TotalSlots);
	for (int32 i = 0; i < TotalSlots; ++i)
	{
		TransactionSnapshot.Slots[i] = GetGridSlot(i);
	}

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
	CurrentWeight = TransactionSnapshot.CurrentWeight;
	bTransactionActive = false;

	// Rebuild GridStorage from snapshot
	EnsureStorageInitialized();
	if (GridStorage && GridStorage->IsInitialized())
	{
		GridStorage->Clear();
		for (const FSuspenseCoreItemInstance& Instance : ItemInstances)
		{
			UpdateGridSlots(Instance, true);
		}
	}

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
	SCOPE_CYCLE_COUNTER(STAT_Inventory_AddItem); // Reuse AddItem stat for consolidation

	if (!bIsInitialized)
	{
		return 0;
	}

	USuspenseCoreDataManager* DataManager = GetDataManager();
	if (!DataManager)
	{
		return 0;
	}

	int32 TotalConsolidated = 0;

	// Build a map of ItemID -> array of instance indices for stackable items
	TMap<FName, TArray<int32>> StackableItemGroups;

	for (int32 i = 0; i < ItemInstances.Num(); ++i)
	{
		const FSuspenseCoreItemInstance& Instance = ItemInstances[i];

		// Filter by ItemID if specified
		if (!ItemID.IsNone() && Instance.ItemID != ItemID)
		{
			continue;
		}

		// Check if item is stackable
		FSuspenseCoreItemData ItemData;
		if (!DataManager->GetItemData(Instance.ItemID, ItemData))
		{
			continue;
		}

		if (!ItemData.InventoryProps.IsStackable())
		{
			continue;
		}

		StackableItemGroups.FindOrAdd(Instance.ItemID).Add(i);
	}

	// Process each group of stackable items
	for (auto& Pair : StackableItemGroups)
	{
		const FName& CurrentItemID = Pair.Key;
		TArray<int32>& InstanceIndices = Pair.Value;

		// Skip if only one stack
		if (InstanceIndices.Num() <= 1)
		{
			continue;
		}

		// Get max stack size for this item
		FSuspenseCoreItemData ItemData;
		if (!DataManager->GetItemData(CurrentItemID, ItemData))
		{
			continue;
		}
		const int32 MaxStackSize = ItemData.InventoryProps.MaxStackSize;
		const float UnitWeight = ItemData.InventoryProps.Weight;

		// Sort by quantity descending (fill larger stacks first)
		InstanceIndices.Sort([this](int32 A, int32 B)
		{
			return ItemInstances[A].Quantity > ItemInstances[B].Quantity;
		});

		// Track instances to remove after consolidation
		TArray<FGuid> InstancesToRemove;

		// Merge smaller stacks into larger ones
		for (int32 i = 0; i < InstanceIndices.Num(); ++i)
		{
			FSuspenseCoreItemInstance& TargetInstance = ItemInstances[InstanceIndices[i]];

			// Skip if already marked for removal or already full
			if (InstancesToRemove.Contains(TargetInstance.UniqueInstanceID))
			{
				continue;
			}

			if (TargetInstance.Quantity >= MaxStackSize)
			{
				continue;
			}

			// Try to absorb from other stacks
			for (int32 j = i + 1; j < InstanceIndices.Num(); ++j)
			{
				FSuspenseCoreItemInstance& SourceInstance = ItemInstances[InstanceIndices[j]];

				// Skip if already marked for removal
				if (InstancesToRemove.Contains(SourceInstance.UniqueInstanceID))
				{
					continue;
				}

				int32 SpaceInTarget = MaxStackSize - TargetInstance.Quantity;
				if (SpaceInTarget <= 0)
				{
					break; // Target is full
				}

				int32 ToTransfer = FMath::Min(SpaceInTarget, SourceInstance.Quantity);
				if (ToTransfer <= 0)
				{
					continue;
				}

				// Transfer quantity
				TargetInstance.Quantity += ToTransfer;
				SourceInstance.Quantity -= ToTransfer;
				++TotalConsolidated;

				UE_LOG(LogSuspenseCoreInventory, Verbose,
					TEXT("ConsolidateStacks: Transferred %d of %s from slot %d to slot %d"),
					ToTransfer, *CurrentItemID.ToString(), SourceInstance.SlotIndex, TargetInstance.SlotIndex);

				// Mark empty source for removal
				if (SourceInstance.Quantity <= 0)
				{
					InstancesToRemove.Add(SourceInstance.UniqueInstanceID);
				}
			}
		}

		// Remove emptied stacks (iterate backwards to avoid index shifting)
		for (const FGuid& InstanceIDToRemove : InstancesToRemove)
		{
			for (int32 i = ItemInstances.Num() - 1; i >= 0; --i)
			{
				if (ItemInstances[i].UniqueInstanceID == InstanceIDToRemove)
				{
					// Clear grid slots
					UpdateGridSlots(ItemInstances[i], false);

					// Remove from replication
					ReplicatedInventory.RemoveItem(InstanceIDToRemove);

					// Remove from array
					ItemInstances.RemoveAt(i);

					UE_LOG(LogSuspenseCoreInventory, Verbose,
						TEXT("ConsolidateStacks: Removed empty stack %s"), *InstanceIDToRemove.ToString());
					break;
				}
			}
		}

		// Update replicated data for modified stacks
		for (int32 Idx : InstanceIndices)
		{
			if (Idx < ItemInstances.Num())
			{
				const FSuspenseCoreItemInstance& Instance = ItemInstances[Idx];
				if (!InstancesToRemove.Contains(Instance.UniqueInstanceID))
				{
					ReplicatedInventory.UpdateItem(Instance);
				}
			}
		}
	}

	if (TotalConsolidated > 0)
	{
		// Recalculate weight for consistency
		RecalculateWeight();

		// Invalidate UI cache
		InvalidateAllUICache();

		// Broadcast update
		BroadcastInventoryUpdated();

		UE_LOG(LogSuspenseCoreInventory, Log,
			TEXT("ConsolidateStacks: Completed %d consolidations. Items remaining: %d"),
			TotalConsolidated, ItemInstances.Num());
	}

#if !UE_BUILD_SHIPPING
	ValidateInventoryIntegrityInternal(TEXT("ConsolidateStacks"));
#endif

	return TotalConsolidated;
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

	// Clear item type restrictions - they should be set explicitly after initialization
	// via SetAllowedItemTypes() if needed. This ensures a clean slate.
	Config.AllowedItemTypes.Reset();
	Config.DisallowedItemTypes.Reset();

	// Initialize GridStorage (SSOT for all grid operations)
	EnsureStorageInitialized();

	// Sync to legacy array for replication compatibility
	SyncStorageToLegacyArray();

	ItemInstances.Empty();
	CurrentWeight = 0.0f;
	bIsInitialized = true;

	// Reset search hint
	LastFreeSlotHint = 0;

	ReplicatedInventory.GridWidth = Config.GridWidth;
	ReplicatedInventory.GridHeight = Config.GridHeight;
	ReplicatedInventory.MaxWeight = Config.MaxWeight;
	ReplicatedInventory.OwnerComponent = this;

	// Bind delta replication delegates
	BindReplicationDelegates();

	// Broadcast initialization
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		FSuspenseCoreEventData EventData;
		EventData.Source = this;
		EventBus->Publish(SUSPENSE_INV_EVENT_INITIALIZED, EventData);
	}

	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("Inventory initialized: %dx%d grid, %.1f max weight (Storage: %s)"),
		Config.GridWidth, Config.GridHeight, Config.MaxWeight,
		GridStorage ? TEXT("Created") : TEXT("Failed"));
}

//==================================================================
// Storage Delegation
//==================================================================

void USuspenseCoreInventoryComponent::EnsureStorageInitialized()
{
	if (!GridStorage)
	{
		GridStorage = NewObject<USuspenseCoreInventoryStorage>(this, NAME_None, RF_Transient);
	}

	if (GridStorage && (!GridStorage->IsInitialized() ||
		GridStorage->GetGridWidth() != Config.GridWidth ||
		GridStorage->GetGridHeight() != Config.GridHeight))
	{
		GridStorage->Initialize(Config.GridWidth, Config.GridHeight);
	}
}

//==================================================================
// Grid Storage Accessors (SSOT)
//==================================================================

FSuspenseCoreInventorySlot USuspenseCoreInventoryComponent::GetGridSlot(int32 SlotIndex) const
{
	if (GridStorage && GridStorage->IsInitialized())
	{
		return GridStorage->GetSlot(SlotIndex);
	}
	return FSuspenseCoreInventorySlot();
}

FGuid USuspenseCoreInventoryComponent::GetInstanceIDAtSlot(int32 SlotIndex) const
{
	if (GridStorage && GridStorage->IsInitialized())
	{
		return GridStorage->GetInstanceIDAtSlot(SlotIndex);
	}
	return FGuid();
}

bool USuspenseCoreInventoryComponent::IsValidSlotIndex(int32 SlotIndex) const
{
	if (GridStorage && GridStorage->IsInitialized())
	{
		return GridStorage->IsValidSlot(SlotIndex);
	}
	return SlotIndex >= 0 && SlotIndex < Config.GridWidth * Config.GridHeight;
}

int32 USuspenseCoreInventoryComponent::GetTotalSlotCount() const
{
	if (GridStorage && GridStorage->IsInitialized())
	{
		return GridStorage->GetTotalSlots();
	}
	return Config.GridWidth * Config.GridHeight;
}

void USuspenseCoreInventoryComponent::SyncStorageToLegacyArray()
{
	if (!GridStorage || !GridStorage->IsInitialized())
	{
		return;
	}

	int32 TotalSlots = GridStorage->GetTotalSlots();
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	GridSlots_DEPRECATED.SetNum(TotalSlots);
	for (int32 i = 0; i < TotalSlots; ++i)
	{
		GridSlots_DEPRECATED[i] = GridStorage->GetSlot(i);
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

bool USuspenseCoreInventoryComponent::IsInitialized() const
{
	return bIsInitialized;
}

void USuspenseCoreInventoryComponent::Clear()
{
	ItemInstances.Empty();

	// Clear GridStorage (SSOT)
	if (GridStorage && GridStorage->IsInitialized())
	{
		GridStorage->Clear();
	}

	// Sync to legacy array for replication
	SyncStorageToLegacyArray();

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
	SCOPE_CYCLE_COUNTER(STAT_Inventory_RemoveItem);

	int32 Index = ItemInstances.IndexOfByPredicate([&InstanceID](const FSuspenseCoreItemInstance& Instance)
	{
		return Instance.UniqueInstanceID == InstanceID;
	});

	if (Index == INDEX_NONE)
	{
		return false;
	}

	OutRemovedInstance = ItemInstances[Index];

	// Calculate weight delta BEFORE removal (incremental update)
	float WeightToRemove = 0.0f;
	USuspenseCoreDataManager* DataManager = GetDataManager();
	if (DataManager)
	{
		FSuspenseCoreItemData ItemData;
		if (DataManager->GetItemData(OutRemovedInstance.ItemID, ItemData))
		{
			WeightToRemove = ItemData.InventoryProps.Weight * OutRemovedInstance.Quantity;
		}
	}

	// Remove from data structures
	UpdateGridSlots(OutRemovedInstance, false);
	ItemInstances.RemoveAt(Index);
	ReplicatedInventory.RemoveItem(InstanceID);

	// Incremental weight update (O(1) instead of O(n))
	UpdateWeightDelta(-WeightToRemove);

	// Invalidate UI cache
	InvalidateItemUICache(InstanceID);

	// Broadcast item event via EventBus
	BroadcastItemEvent(SUSPENSE_INV_EVENT_ITEM_REMOVED, OutRemovedInstance, OutRemovedInstance.SlotIndex);

	// Notify UI widgets about data change
	BroadcastInventoryUpdated();

#if !UE_BUILD_SHIPPING
	ValidateInventoryIntegrityInternal(TEXT("RemoveItemInternal"));
#endif

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
	SCOPE_CYCLE_COUNTER(STAT_Inventory_OnRep);

	// DELTA REPLICATION ARCHITECTURE:
	// FFastArraySerializer calls delta callbacks for individual items:
	// - FSuspenseCoreReplicatedItem::PreReplicatedRemove  (O(1) per item)
	// - FSuspenseCoreReplicatedItem::PostReplicatedAdd    (O(1) per item)
	// - FSuspenseCoreReplicatedItem::PostReplicatedChange (O(1) per item)
	//
	// This OnRep function handles:
	// 1. Initial sync (when OwnerComponent not yet set - full rebuild needed)
	// 2. Config changes (grid size, max weight)
	// 3. Final broadcast to notify UI

	// Ensure OwnerComponent is set for delta callbacks
	if (!ReplicatedInventory.OwnerComponent.IsValid())
	{
		ReplicatedInventory.OwnerComponent = this;
	}

	// Ensure delegates are bound for delta callbacks
	BindReplicationDelegates();

	// Check if this is initial sync (no local state yet) or config changed
	bool bNeedFullRebuild = !bIsInitialized;

	// Update config if changed
	if (Config.GridWidth != ReplicatedInventory.GridWidth ||
		Config.GridHeight != ReplicatedInventory.GridHeight ||
		!FMath::IsNearlyEqual(Config.MaxWeight, ReplicatedInventory.MaxWeight))
	{
		Config.GridWidth = ReplicatedInventory.GridWidth;
		Config.GridHeight = ReplicatedInventory.GridHeight;
		Config.MaxWeight = ReplicatedInventory.MaxWeight;
		bNeedFullRebuild = true; // Grid size changed - need full rebuild
	}

	if (bNeedFullRebuild)
	{
		UE_LOG(LogSuspenseCoreInventory, Log,
			TEXT("OnRep_ReplicatedInventory: Full rebuild (initial sync or config change)"));

		// Full rebuild - this happens on initial connect or grid resize
		// Initialize GridStorage as SSOT
		EnsureStorageInitialized();

		ItemInstances.Empty(ReplicatedInventory.Items.Num());
		for (const FSuspenseCoreReplicatedItem& RepItem : ReplicatedInventory.Items)
		{
			ItemInstances.Add(RepItem.ToItemInstance());
		}

		for (const FSuspenseCoreItemInstance& Instance : ItemInstances)
		{
			UpdateGridSlots(Instance, true);
		}

		RecalculateWeight();
		bIsInitialized = true;
		LastFreeSlotHint = 0;
	}
	else
	{
		// Delta updates already applied by callbacks
		// Just verify consistency in development builds
		UE_LOG(LogSuspenseCoreInventory, Verbose,
			TEXT("OnRep_ReplicatedInventory: Delta update (callbacks handled changes)"));
	}

	// Invalidate UI caches (delta callbacks also do this, but be safe)
	InvalidateAllUICache();

	// Notify observers
	BroadcastInventoryUpdated();

	UE_LOG(LogSuspenseCoreInventory, Verbose,
		TEXT("OnRep_ReplicatedInventory: Items=%d, Slots=%d, Weight=%.2f, FullRebuild=%s"),
		ItemInstances.Num(), GetTotalSlotCount(), CurrentWeight,
		bNeedFullRebuild ? TEXT("Yes") : TEXT("No"));
}

//==================================================================
// Delta Replication Delegate Handlers
//==================================================================

void USuspenseCoreInventoryComponent::BindReplicationDelegates()
{
	// Only bind if not already bound
	if (!ReplicatedInventory.OnPreRemoveDelegate.IsBound())
	{
		ReplicatedInventory.OnPreRemoveDelegate.BindUObject(this, &USuspenseCoreInventoryComponent::HandleReplicatedItemRemove);
	}
	if (!ReplicatedInventory.OnPostAddDelegate.IsBound())
	{
		ReplicatedInventory.OnPostAddDelegate.BindUObject(this, &USuspenseCoreInventoryComponent::HandleReplicatedItemAdd);
	}
	if (!ReplicatedInventory.OnPostChangeDelegate.IsBound())
	{
		ReplicatedInventory.OnPostChangeDelegate.BindUObject(this, &USuspenseCoreInventoryComponent::HandleReplicatedItemChange);
	}
}

void USuspenseCoreInventoryComponent::HandleReplicatedItemRemove(const FSuspenseCoreReplicatedItem& Item, const FSuspenseCoreReplicatedInventory& ArraySerializer)
{
	UE_LOG(LogSuspenseCoreInventory, Verbose,
		TEXT("HandleReplicatedItemRemove: Removing item %s from slot %d"),
		*Item.InstanceID.ToString(), Item.SlotIndex);

	// Find and remove this specific item from local arrays
	FSuspenseCoreItemInstance* LocalInstance = FindItemInstanceInternal(Item.InstanceID);
	if (LocalInstance)
	{
		// Calculate weight to remove BEFORE removing
		float WeightToRemove = 0.0f;
		USuspenseCoreDataManager* DataManager = GetDataManager();
		if (DataManager)
		{
			FSuspenseCoreItemData ItemData;
			if (DataManager->GetItemData(LocalInstance->ItemID, ItemData))
			{
				WeightToRemove = ItemData.InventoryProps.Weight * LocalInstance->Quantity;
			}
		}

		// Remove from grid slots
		UpdateGridSlots(*LocalInstance, false);

		// Remove from items array
		int32 Index = ItemInstances.IndexOfByPredicate(
			[&Item](const FSuspenseCoreItemInstance& Inst)
			{
				return Inst.UniqueInstanceID == Item.InstanceID;
			});

		if (Index != INDEX_NONE)
		{
			ItemInstances.RemoveAt(Index);
		}

		// Update weight incrementally
		UpdateWeightDelta(-WeightToRemove);

		// Invalidate UI cache for this item
		InvalidateItemUICache(Item.InstanceID);
	}
}

void USuspenseCoreInventoryComponent::HandleReplicatedItemAdd(const FSuspenseCoreReplicatedItem& Item, const FSuspenseCoreReplicatedInventory& ArraySerializer)
{
	UE_LOG(LogSuspenseCoreInventory, Verbose,
		TEXT("HandleReplicatedItemAdd: Adding item %s (ID: %s) to slot %d"),
		*Item.ItemID.ToString(), *Item.InstanceID.ToString(), Item.SlotIndex);

	// Convert replicated item to local instance
	FSuspenseCoreItemInstance NewInstance = Item.ToItemInstance();

	// Check if item already exists (shouldn't happen, but safety check)
	FSuspenseCoreItemInstance* Existing = FindItemInstanceInternal(Item.InstanceID);
	if (Existing)
	{
		UE_LOG(LogSuspenseCoreInventory, Warning,
			TEXT("HandleReplicatedItemAdd: Item %s already exists! Updating instead."),
			*Item.InstanceID.ToString());
		*Existing = NewInstance;
		return;
	}

	// Add to local items array
	ItemInstances.Add(NewInstance);

	// Update grid slots
	UpdateGridSlots(NewInstance, true);

	// Calculate and add weight
	float WeightToAdd = 0.0f;
	USuspenseCoreDataManager* DataManager = GetDataManager();
	if (DataManager)
	{
		FSuspenseCoreItemData ItemData;
		if (DataManager->GetItemData(NewInstance.ItemID, ItemData))
		{
			WeightToAdd = ItemData.InventoryProps.Weight * NewInstance.Quantity;
		}
	}
	UpdateWeightDelta(WeightToAdd);

	// Invalidate UI cache
	InvalidateAllUICache();
}

void USuspenseCoreInventoryComponent::HandleReplicatedItemChange(const FSuspenseCoreReplicatedItem& Item, const FSuspenseCoreReplicatedInventory& ArraySerializer)
{
	UE_LOG(LogSuspenseCoreInventory, Verbose,
		TEXT("HandleReplicatedItemChange: Updating item %s, Qty: %d, Slot: %d"),
		*Item.InstanceID.ToString(), Item.Quantity, Item.SlotIndex);

	// Find existing local instance
	FSuspenseCoreItemInstance* LocalInstance = FindItemInstanceInternal(Item.InstanceID);
	if (!LocalInstance)
	{
		UE_LOG(LogSuspenseCoreInventory, Warning,
			TEXT("HandleReplicatedItemChange: Item %s not found locally! Adding instead."),
			*Item.InstanceID.ToString());
		HandleReplicatedItemAdd(Item, ArraySerializer);
		return;
	}

	// Get item data for weight calculation
	USuspenseCoreDataManager* DataManager = GetDataManager();
	float UnitWeight = 0.0f;
	if (DataManager)
	{
		FSuspenseCoreItemData ItemData;
		if (DataManager->GetItemData(LocalInstance->ItemID, ItemData))
		{
			UnitWeight = ItemData.InventoryProps.Weight;
		}
	}

	// Check if position changed
	bool bPositionChanged = (LocalInstance->SlotIndex != Item.SlotIndex) ||
							(LocalInstance->GridPosition != Item.GridPosition) ||
							(LocalInstance->Rotation != static_cast<int32>(Item.Rotation));

	if (bPositionChanged)
	{
		// Remove from old grid position
		UpdateGridSlots(*LocalInstance, false);
	}

	// Calculate weight delta from quantity change
	int32 OldQuantity = LocalInstance->Quantity;
	int32 QuantityDelta = Item.Quantity - OldQuantity;

	// Update local instance fields
	LocalInstance->Quantity = Item.Quantity;
	LocalInstance->SlotIndex = Item.SlotIndex;
	LocalInstance->GridPosition = Item.GridPosition;
	LocalInstance->Rotation = static_cast<int32>(Item.Rotation);
	LocalInstance->RuntimeProperties = Item.RuntimeProperties;

	// Update weapon state if present
	if (Item.PackedFlags & 0x01)
	{
		LocalInstance->WeaponState.bHasState = true;
		LocalInstance->WeaponState.CurrentAmmo = Item.CurrentAmmo;
		LocalInstance->WeaponState.ReserveAmmo = Item.ReserveAmmo;
	}

	if (bPositionChanged)
	{
		// Add to new grid position
		UpdateGridSlots(*LocalInstance, true);
	}

	// Update weight if quantity changed
	if (QuantityDelta != 0)
	{
		UpdateWeightDelta(UnitWeight * QuantityDelta);
	}

	// Invalidate UI cache for this item
	InvalidateItemUICache(Item.InstanceID);
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
	SCOPE_CYCLE_COUNTER(STAT_Inventory_RecalculateWeight);

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

void USuspenseCoreInventoryComponent::UpdateWeightDelta(float WeightDelta)
{
	// O(1) incremental weight update - use instead of RecalculateWeight() in hotpath
	CurrentWeight = FMath::Max(0.0f, CurrentWeight + WeightDelta);

#if !UE_BUILD_SHIPPING
	// Periodic validation in development builds
	++ValidationOperationCounter;
	if (ValidationOperationCounter % 100 == 0) // Every 100 operations
	{
		float CalculatedWeight = 0.0f;
		USuspenseCoreDataManager* DataManager = GetDataManager();
		if (DataManager)
		{
			for (const FSuspenseCoreItemInstance& Instance : ItemInstances)
			{
				FSuspenseCoreItemData ItemData;
				if (DataManager->GetItemData(Instance.ItemID, ItemData))
				{
					CalculatedWeight += ItemData.InventoryProps.Weight * Instance.Quantity;
				}
			}
		}

		if (!FMath::IsNearlyEqual(CurrentWeight, CalculatedWeight, 0.01f))
		{
			UE_LOG(LogSuspenseCoreInventory, Error,
				TEXT("Weight desync detected! Tracked: %.2f, Calculated: %.2f - Force syncing"),
				CurrentWeight, CalculatedWeight);
			CurrentWeight = CalculatedWeight; // Force sync to correct value
		}
	}
#endif
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

	// Update GridStorage FIRST (SSOT for all grid operations)
	if (GridStorage && GridStorage->IsInitialized())
	{
		if (bPlace)
		{
			GridStorage->PlaceItem(Instance.UniqueInstanceID, ItemSize, Instance.SlotIndex, bRotated);
		}
		else
		{
			GridStorage->RemoveItem(Instance.UniqueInstanceID);
		}
	}

	// Sync to legacy array for replication compatibility
	SyncStorageToLegacyArray();
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

	// Validate ItemID exists in DataTable (prevents garbage/invalid IDs from wasting server resources)
	USuspenseCoreDataManager* DataManager = GetDataManager();
	if (DataManager)
	{
		FSuspenseCoreItemData ItemData;
		if (!DataManager->GetItemData(ItemID, ItemData))
		{
			UE_LOG(LogSuspenseCoreInventory, Warning,
				TEXT("Server_AddItemByID_Validate: ItemID '%s' not found in DataTable - rejected"),
				*ItemID.ToString());
			return false;
		}
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
	// Basic parameter validation
	if (ItemID.IsNone())
	{
		UE_LOG(LogSuspenseCoreInventory, Warning, TEXT("Server_RemoveItemByID_Validate: ItemID is None"));
		return false;
	}

	if (Quantity <= 0 || Quantity > 9999)
	{
		UE_LOG(LogSuspenseCoreInventory, Warning,
			TEXT("Server_RemoveItemByID_Validate: Invalid quantity %d"), Quantity);
		return false;
	}

	// Validate ItemID exists in DataTable
	USuspenseCoreDataManager* DataManager = GetDataManager();
	if (DataManager)
	{
		FSuspenseCoreItemData ItemData;
		if (!DataManager->GetItemData(ItemID, ItemData))
		{
			UE_LOG(LogSuspenseCoreInventory, Warning,
				TEXT("Server_RemoveItemByID_Validate: ItemID '%s' not found in DataTable - rejected"),
				*ItemID.ToString());
			return false;
		}
	}

	// Rate limiting
	USuspenseCoreSecurityValidator* Security = USuspenseCoreSecurityValidator::Get(this);
	if (Security && !Security->CheckRateLimit(GetOwner(), TEXT("RemoveItem"), 10.0f))
	{
		UE_LOG(LogSuspenseCoreInventory, Warning, TEXT("Server_RemoveItemByID_Validate: Rate limited"));
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

	// Invalidate UI cache before broadcasting update
	InvalidateAllUICache();
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

	// Invalidate UI cache before broadcasting update
	InvalidateAllUICache();
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
		FGuid InstanceID = GetInstanceIDAtSlot(SlotIndex);
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
	// Use cached data if available
	if (bSlotUICacheDirty)
	{
		RebuildSlotUICache();
		bSlotUICacheDirty = false;
	}

	return CachedSlotUIData;
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
	return IsValidSlotIndex(SlotIndex);
}

TArray<FSuspenseCoreItemUIData> USuspenseCoreInventoryComponent::GetAllItemUIData() const
{
	SCOPE_CYCLE_COUNTER(STAT_Inventory_GetUIData);

	// Use cached data if available (avoids regeneration every frame)
	if (bItemUICacheDirty)
	{
		RebuildItemUICache();
		bItemUICacheDirty = false;
	}

	// Convert map to array
	TArray<FSuspenseCoreItemUIData> Result;
	CachedItemUIData.GenerateValueArray(Result);
	return Result;
}

bool USuspenseCoreInventoryComponent::GetItemUIDataAtSlot(int32 SlotIndex, FSuspenseCoreItemUIData& OutItem) const
{
	if (!IsSlotOccupied(SlotIndex))
	{
		return false;
	}

	const FGuid InstanceID = GetInstanceIDAtSlot(SlotIndex);
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

	// Get the instance ID at this slot (using GridStorage SSOT)
	const FGuid InstanceID = GetInstanceIDAtSlot(AnySlotIndex);

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
			UIData.BaseValue = ItemData.InventoryProps.BaseValue;
			UIData.TotalValue = ItemData.InventoryProps.BaseValue * Instance.Quantity;
			UIData.bIsEquippable = ItemData.Behavior.bIsEquippable;
			UIData.bIsUsable = ItemData.Behavior.bIsConsumable;
			UIData.bIsDroppable = ItemData.Behavior.bCanDrop;
			UIData.bIsTradeable = ItemData.Behavior.bCanTrade;
		}
		else
		{
			UE_LOG(LogSuspenseCoreInventory, Warning, TEXT("ConvertToUIData: Failed to get ItemData for %s"),
				*Instance.ItemID.ToString());
		}
	}

	return UIData;
}

FSuspenseCoreSlotUIData USuspenseCoreInventoryComponent::ConvertSlotToUIData(int32 SlotIndex) const
{
	FSuspenseCoreSlotUIData SlotData;
	SlotData.SlotIndex = SlotIndex;
	SlotData.GridPosition = SlotToGridCoords(SlotIndex);

	if (IsValidSlotIndex(SlotIndex))
	{
		// Use GridStorage accessor (SSOT)
		const FSuspenseCoreInventorySlot Slot = GetGridSlot(SlotIndex);
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

//==================================================================
// UI Data Cache Methods (Performance Optimization)
//==================================================================

void USuspenseCoreInventoryComponent::InvalidateItemUICache(const FGuid& ItemID)
{
	CachedItemUIData.Remove(ItemID);
	bItemUICacheDirty = true;
	bSlotUICacheDirty = true; // Slot data also affected
}

void USuspenseCoreInventoryComponent::InvalidateAllUICache()
{
	bItemUICacheDirty = true;
	bSlotUICacheDirty = true;
}

void USuspenseCoreInventoryComponent::RebuildItemUICache() const
{
	CachedItemUIData.Empty(ItemInstances.Num());

	for (const FSuspenseCoreItemInstance& Instance : ItemInstances)
	{
		CachedItemUIData.Add(Instance.UniqueInstanceID, ConvertToUIData(Instance));
	}
}

void USuspenseCoreInventoryComponent::RebuildSlotUICache() const
{
	const int32 TotalSlots = GetTotalSlotCount();
	CachedSlotUIData.SetNum(TotalSlots);

	for (int32 i = 0; i < TotalSlots; ++i)
	{
		CachedSlotUIData[i] = ConvertSlotToUIData(i);
	}
}

//==================================================================
// Validation Layer (Development builds only)
//==================================================================

#if !UE_BUILD_SHIPPING

void USuspenseCoreInventoryComponent::ValidateInventoryIntegrityInternal(const FString& Context) const
{
	TArray<FString> Errors;

	const int32 TotalSlots = GetTotalSlotCount();

	// 1. Check grid slots match items (using GridStorage as SSOT)
	TSet<FGuid> ItemIDsInGrid;
	for (int32 i = 0; i < TotalSlots; ++i)
	{
		const FSuspenseCoreInventorySlot Slot = GetGridSlot(i);
		if (!Slot.IsEmpty())
		{
			ItemIDsInGrid.Add(Slot.InstanceID);
		}
	}

	TSet<FGuid> ItemIDsInArray;
	for (const FSuspenseCoreItemInstance& Instance : ItemInstances)
	{
		ItemIDsInArray.Add(Instance.UniqueInstanceID);

		// Check slot index valid
		if (Instance.SlotIndex < 0 || Instance.SlotIndex >= TotalSlots)
		{
			Errors.Add(FString::Printf(TEXT("[%s] Item %s has invalid SlotIndex %d (max: %d)"),
				*Context, *Instance.ItemID.ToString(), Instance.SlotIndex, TotalSlots - 1));
		}

		// Check quantity valid
		if (Instance.Quantity <= 0)
		{
			Errors.Add(FString::Printf(TEXT("[%s] Item %s has invalid Quantity %d"),
				*Context, *Instance.ItemID.ToString(), Instance.Quantity));
		}
	}

	// 2. Check sets match (grid <-> items array)
	for (const FGuid& ID : ItemIDsInGrid)
	{
		if (!ItemIDsInArray.Contains(ID))
		{
			Errors.Add(FString::Printf(TEXT("[%s] Grid contains item %s not in ItemInstances"),
				*Context, *ID.ToString()));
		}
	}

	for (const FGuid& ID : ItemIDsInArray)
	{
		if (!ItemIDsInGrid.Contains(ID))
		{
			Errors.Add(FString::Printf(TEXT("[%s] ItemInstances contains %s not in grid"),
				*Context, *ID.ToString()));
		}
	}

	// 3. Check weight consistency
	float CalculatedWeight = 0.0f;
	USuspenseCoreDataManager* DataManager = GetDataManager();
	if (DataManager)
	{
		for (const FSuspenseCoreItemInstance& Instance : ItemInstances)
		{
			FSuspenseCoreItemData ItemData;
			if (DataManager->GetItemData(Instance.ItemID, ItemData))
			{
				CalculatedWeight += ItemData.InventoryProps.Weight * Instance.Quantity;
			}
		}
	}

	if (!FMath::IsNearlyEqual(CurrentWeight, CalculatedWeight, 0.01f))
	{
		Errors.Add(FString::Printf(TEXT("[%s] Weight mismatch: Tracked %.2f, Calculated %.2f"),
			*Context, CurrentWeight, CalculatedWeight));
	}

	// Log errors if any
	if (Errors.Num() > 0)
	{
		UE_LOG(LogSuspenseCoreInventory, Error, TEXT("=== INVENTORY INTEGRITY VIOLATION [%s] ==="), *Context);
		for (const FString& Error : Errors)
		{
			UE_LOG(LogSuspenseCoreInventory, Error, TEXT("  - %s"), *Error);
		}

		// In Editor builds, optionally trigger assert for debugging
		// Uncomment for stricter development validation:
		// ensureAlwaysMsgf(false, TEXT("Inventory integrity check failed! See log for details."));
	}
}

#endif // !UE_BUILD_SHIPPING
