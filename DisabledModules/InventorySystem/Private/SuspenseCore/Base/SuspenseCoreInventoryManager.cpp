// SuspenseCoreInventoryManager.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Base/SuspenseCoreInventoryManager.h"
#include "SuspenseCore/Base/SuspenseCoreInventoryLogs.h"
#include "SuspenseCore/Components/SuspenseCoreInventoryComponent.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Events/Inventory/SuspenseCoreInventoryEvents.h"
#include "SuspenseCore/Types/Items/SuspenseCoreItemTypes.h"
#include "Engine/GameInstance.h"

void USuspenseCoreInventoryManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	bIsInitialized = true;

	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("SuspenseCoreInventoryManager initialized"));
}

void USuspenseCoreInventoryManager::Deinitialize()
{
	RegisteredInventories.Empty();
	CachedEventManager.Reset();
	CachedDataManager.Reset();
	bIsInitialized = false;

	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("SuspenseCoreInventoryManager deinitialized"));

	Super::Deinitialize();
}

void USuspenseCoreInventoryManager::RegisterInventory(USuspenseCoreInventoryComponent* Component)
{
	if (!Component)
	{
		return;
	}

	// Check if already registered
	for (const TWeakObjectPtr<USuspenseCoreInventoryComponent>& Existing : RegisteredInventories)
	{
		if (Existing.Get() == Component)
		{
			return;
		}
	}

	RegisteredInventories.Add(Component);

	UE_LOG(LogSuspenseCoreInventory, Verbose, TEXT("Registered inventory: %s"),
		Component->GetOwner() ? *Component->GetOwner()->GetName() : TEXT("Unknown"));
}

void USuspenseCoreInventoryManager::UnregisterInventory(USuspenseCoreInventoryComponent* Component)
{
	if (!Component)
	{
		return;
	}

	RegisteredInventories.RemoveAll([Component](const TWeakObjectPtr<USuspenseCoreInventoryComponent>& Weak)
	{
		return Weak.Get() == Component || !Weak.IsValid();
	});

	UE_LOG(LogSuspenseCoreInventory, Verbose, TEXT("Unregistered inventory: %s"),
		Component->GetOwner() ? *Component->GetOwner()->GetName() : TEXT("Unknown"));
}

TArray<USuspenseCoreInventoryComponent*> USuspenseCoreInventoryManager::GetAllInventories() const
{
	TArray<USuspenseCoreInventoryComponent*> Result;
	for (const TWeakObjectPtr<USuspenseCoreInventoryComponent>& Weak : RegisteredInventories)
	{
		if (USuspenseCoreInventoryComponent* Comp = Weak.Get())
		{
			Result.Add(Comp);
		}
	}
	return Result;
}

TArray<USuspenseCoreInventoryComponent*> USuspenseCoreInventoryManager::GetInventoriesByOwner(AActor* Owner) const
{
	TArray<USuspenseCoreInventoryComponent*> Result;
	if (!Owner)
	{
		return Result;
	}

	for (const TWeakObjectPtr<USuspenseCoreInventoryComponent>& Weak : RegisteredInventories)
	{
		if (USuspenseCoreInventoryComponent* Comp = Weak.Get())
		{
			if (Comp->GetOwner() == Owner)
			{
				Result.Add(Comp);
			}
		}
	}
	return Result;
}

USuspenseCoreInventoryComponent* USuspenseCoreInventoryManager::GetInventoryByTag(FGameplayTag InventoryTag) const
{
	for (const TWeakObjectPtr<USuspenseCoreInventoryComponent>& Weak : RegisteredInventories)
	{
		if (USuspenseCoreInventoryComponent* Comp = Weak.Get())
		{
			// Check component's inventory tag
			// Implementation depends on component having InventoryTag property
		}
	}
	return nullptr;
}

bool USuspenseCoreInventoryManager::TransferItem(
	USuspenseCoreInventoryComponent* Source,
	USuspenseCoreInventoryComponent* Target,
	FName ItemID,
	int32 Quantity,
	FSuspenseInventoryOperationResult& OutResult)
{
	if (!Source || !Target || ItemID.IsNone() || Quantity <= 0)
	{
		OutResult = FSuspenseInventoryOperationResult::Failure(
			ESuspenseCoreInventoryResult::InvalidItem,
			TEXT("Invalid transfer parameters"));
		return false;
	}

	// Check source has item
	int32 SourceCount = Source->Execute_GetItemCountByID(Source, ItemID);
	if (SourceCount < Quantity)
	{
		OutResult = FSuspenseInventoryOperationResult::Failure(
			ESuspenseCoreInventoryResult::InsufficientQuantity,
			FString::Printf(TEXT("Source has %d, requested %d"), SourceCount, Quantity));
		return false;
	}

	// Check target can receive
	if (!Target->Execute_CanReceiveItem(Target, ItemID, Quantity))
	{
		OutResult = FSuspenseInventoryOperationResult::Failure(
			ESuspenseCoreInventoryResult::NoSpace,
			TEXT("Target cannot receive item"));
		return false;
	}

	// Perform transfer
	if (!Source->Execute_RemoveItemByID(Source, ItemID, Quantity))
	{
		OutResult = FSuspenseInventoryOperationResult::Failure(
			ESuspenseCoreInventoryResult::Unknown,
			TEXT("Failed to remove from source"));
		return false;
	}

	if (!Target->Execute_AddItemByID(Target, ItemID, Quantity))
	{
		// Rollback - add back to source
		Source->Execute_AddItemByID(Source, ItemID, Quantity);
		OutResult = FSuspenseInventoryOperationResult::Failure(
			ESuspenseCoreInventoryResult::Unknown,
			TEXT("Failed to add to target"));
		return false;
	}

	OutResult = FSuspenseInventoryOperationResult::Success(-1, Quantity);

	// Broadcast transfer event
	BroadcastGlobalEvent(
		SUSPENSE_INV_EVENT_UPDATED,
		{
			{FName(TEXT("Action")), TEXT("Transfer")},
			{FName(TEXT("ItemID")), ItemID.ToString()},
			{FName(TEXT("Quantity")), FString::FromInt(Quantity)}
		}
	);

	return true;
}

bool USuspenseCoreInventoryManager::TransferItemInstance(
	USuspenseCoreInventoryComponent* Source,
	USuspenseCoreInventoryComponent* Target,
	const FGuid& InstanceID,
	int32 TargetSlot,
	FSuspenseInventoryOperationResult& OutResult)
{
	if (!Source || !Target || !InstanceID.IsValid())
	{
		OutResult = FSuspenseInventoryOperationResult::Failure(
			ESuspenseCoreInventoryResult::InvalidItem,
			TEXT("Invalid transfer parameters"));
		return false;
	}

	// Find instance in source
	FSuspenseCoreItemInstance Instance;
	if (!Source->FindItemInstance(InstanceID, Instance))
	{
		OutResult = FSuspenseInventoryOperationResult::Failure(
			ESuspenseCoreInventoryResult::ItemNotFound,
			TEXT("Instance not found in source"));
		return false;
	}

	// Check target can receive
	if (!Target->Execute_CanReceiveItem(Target, Instance.ItemID, Instance.Quantity))
	{
		OutResult = FSuspenseInventoryOperationResult::Failure(
			ESuspenseCoreInventoryResult::NoSpace,
			TEXT("Target cannot receive item"));
		return false;
	}

	// Remove from source
	FSuspenseCoreItemInstance RemovedInstance;
	if (!Source->RemoveItemInstance(InstanceID))
	{
		OutResult = FSuspenseInventoryOperationResult::Failure(
			ESuspenseCoreInventoryResult::Unknown,
			TEXT("Failed to remove from source"));
		return false;
	}

	// Add to target
	if (!Target->AddItemInstanceToSlot(Instance, TargetSlot))
	{
		// Rollback
		Source->Execute_AddItemInstance(Source, Instance);
		OutResult = FSuspenseInventoryOperationResult::Failure(
			ESuspenseCoreInventoryResult::Unknown,
			TEXT("Failed to add to target"));
		return false;
	}

	OutResult = FSuspenseInventoryOperationResult::Success(TargetSlot, Instance.Quantity, InstanceID);
	return true;
}

bool USuspenseCoreInventoryManager::ExecuteTransfer(
	const FSuspenseCoreTransferOperation& Operation,
	const FSuspenseCoreOperationContext& Context,
	FSuspenseInventoryOperationResult& OutResult)
{
	if (!Context.SourceInventory.IsValid() || !Context.TargetInventory.IsValid())
	{
		OutResult = FSuspenseInventoryOperationResult::Failure(
			ESuspenseCoreInventoryResult::NotInitialized,
			TEXT("Invalid inventory references in context"));
		return false;
	}

	USuspenseCoreInventoryComponent* SourceComp = Cast<USuspenseCoreInventoryComponent>(Context.SourceInventory.Get());
	USuspenseCoreInventoryComponent* TargetComp = Cast<USuspenseCoreInventoryComponent>(Context.TargetInventory.Get());

	if (!SourceComp || !TargetComp)
	{
		OutResult = FSuspenseInventoryOperationResult::Failure(
			ESuspenseCoreInventoryResult::NotInitialized,
			TEXT("Invalid inventory component cast"));
		return false;
	}

	if (Operation.InstanceID.IsValid())
	{
		return TransferItemInstance(
			SourceComp,
			TargetComp,
			Operation.InstanceID,
			Operation.TargetSlot,
			OutResult
		);
	}
	else
	{
		return TransferItem(
			SourceComp,
			TargetComp,
			Operation.ItemID,
			Operation.Quantity,
			OutResult
		);
	}
}

bool USuspenseCoreInventoryManager::ExecuteBatchOperation(
	FSuspenseCoreBatchOperation& Batch,
	const FSuspenseCoreOperationContext& Context)
{
	if (Batch.IsEmpty())
	{
		return true;
	}

	Batch.Results.Empty();

	USuspenseCoreInventoryComponent* SourceComp = Context.SourceInventory.IsValid() ?
		Cast<USuspenseCoreInventoryComponent>(Context.SourceInventory.Get()) : nullptr;

	// If atomic, we need transaction support
	if (Batch.bAtomic && SourceComp)
	{
		SourceComp->BeginTransaction();
	}

	bool bAllSucceeded = true;

	// Execute move operations
	for (const FSuspenseCoreMoveOperation& MoveOp : Batch.MoveOperations)
	{
		FSuspenseCoreOperationRecord Record;
		Record.OperationType = ESuspenseCoreOperationType::Move;
		Record.InstanceID = MoveOp.InstanceID;
		Record.PreviousSlot = MoveOp.FromSlot;
		Record.NewSlot = MoveOp.ToSlot;

		if (SourceComp)
		{
			Record.bSuccess = SourceComp->Execute_MoveItem(SourceComp, MoveOp.FromSlot, MoveOp.ToSlot);
			Record.ResultCode = Record.bSuccess ?
				ESuspenseCoreInventoryResult::Success :
				ESuspenseCoreInventoryResult::Unknown;
		}
		else
		{
			Record.bSuccess = false;
			Record.ResultCode = ESuspenseCoreInventoryResult::NotInitialized;
		}

		Batch.Results.Add(Record);
		if (!Record.bSuccess)
		{
			bAllSucceeded = false;
		}
	}

	// Execute swap operations
	for (const FSuspenseCoreSwapOperation& SwapOp : Batch.SwapOperations)
	{
		FSuspenseCoreOperationRecord Record;
		Record.OperationType = ESuspenseCoreOperationType::Swap;
		Record.InstanceID = SwapOp.InstanceID1;
		Record.SecondaryInstanceID = SwapOp.InstanceID2;
		Record.PreviousSlot = SwapOp.Slot1;
		Record.NewSlot = SwapOp.Slot2;

		if (SourceComp)
		{
			Record.bSuccess = SourceComp->SwapItems(SwapOp.Slot1, SwapOp.Slot2);
			Record.ResultCode = Record.bSuccess ?
				ESuspenseCoreInventoryResult::Success :
				ESuspenseCoreInventoryResult::Unknown;
		}
		else
		{
			Record.bSuccess = false;
			Record.ResultCode = ESuspenseCoreInventoryResult::NotInitialized;
		}

		Batch.Results.Add(Record);
		if (!Record.bSuccess)
		{
			bAllSucceeded = false;
		}
	}

	// Handle atomic transaction result
	if (Batch.bAtomic && SourceComp)
	{
		if (bAllSucceeded)
		{
			SourceComp->CommitTransaction();
		}
		else
		{
			SourceComp->RollbackTransaction();
		}
	}

	return bAllSucceeded;
}

bool USuspenseCoreInventoryManager::SortInventory(USuspenseCoreInventoryComponent* Inventory, FName SortMode)
{
	if (!Inventory)
	{
		return false;
	}

	// Get all items
	TArray<FSuspenseCoreItemInstance> Items = Inventory->GetAllItemInstances();
	if (Items.Num() <= 1)
	{
		return true;
	}

	// Sort based on mode
	if (SortMode == FName("Name"))
	{
		Items.Sort([](const FSuspenseCoreItemInstance& A, const FSuspenseCoreItemInstance& B)
		{
			return A.ItemID.LexicalLess(B.ItemID);
		});
	}
	else if (SortMode == FName("Quantity"))
	{
		Items.Sort([](const FSuspenseCoreItemInstance& A, const FSuspenseCoreItemInstance& B)
		{
			return A.Quantity > B.Quantity;
		});
	}
	// Add more sort modes as needed

	// Begin transaction
	Inventory->BeginTransaction();

	// Clear and re-add items
	Inventory->Clear();
	for (const FSuspenseCoreItemInstance& Item : Items)
	{
		Inventory->Execute_AddItemInstance(Inventory, Item);
	}

	Inventory->CommitTransaction();

	return true;
}

int32 USuspenseCoreInventoryManager::ConsolidateAllStacks(USuspenseCoreInventoryComponent* Inventory)
{
	if (!Inventory)
	{
		return 0;
	}

	return Inventory->ConsolidateStacks(NAME_None);
}

int32 USuspenseCoreInventoryManager::FindItemAcrossInventories(FName ItemID, TArray<USuspenseCoreInventoryComponent*>& OutInventories) const
{
	OutInventories.Empty();
	int32 TotalQuantity = 0;

	for (const TWeakObjectPtr<USuspenseCoreInventoryComponent>& Weak : RegisteredInventories)
	{
		if (USuspenseCoreInventoryComponent* Comp = Weak.Get())
		{
			int32 Count = Comp->Execute_GetItemCountByID(Comp, ItemID);
			if (Count > 0)
			{
				OutInventories.Add(Comp);
				TotalQuantity += Count;
			}
		}
	}

	return TotalQuantity;
}

int32 USuspenseCoreInventoryManager::GetTotalItemCount(FName ItemID, const TArray<USuspenseCoreInventoryComponent*>& Inventories) const
{
	int32 Total = 0;

	const TArray<USuspenseCoreInventoryComponent*>& ToCheck = Inventories.Num() > 0 ?
		Inventories : GetAllInventories();

	for (USuspenseCoreInventoryComponent* Comp : ToCheck)
	{
		if (Comp)
		{
			Total += Comp->Execute_GetItemCountByID(Comp, ItemID);
		}
	}

	return Total;
}

int32 USuspenseCoreInventoryManager::GetItemsByType(FGameplayTag ItemType, TArray<FSuspenseCoreItemInstance>& OutItems) const
{
	OutItems.Empty();

	for (const TWeakObjectPtr<USuspenseCoreInventoryComponent>& Weak : RegisteredInventories)
	{
		if (USuspenseCoreInventoryComponent* Comp = Weak.Get())
		{
			TArray<FSuspenseCoreItemInstance> TypeItems = Comp->FindItemsByType(ItemType);
			OutItems.Append(TypeItems);
		}
	}

	return OutItems.Num();
}

bool USuspenseCoreInventoryManager::CreateItemInstance(FName ItemID, int32 Quantity, FSuspenseCoreItemInstance& OutInstance)
{
	USuspenseCoreDataManager* DataMgr = GetDataManager();
	if (!DataMgr)
	{
		return false;
	}

	// Get item data
	FSuspenseCoreItemData ItemData;
	if (!DataMgr->GetItemData(ItemID, ItemData))
	{
		return false;
	}

	// Create instance
	OutInstance = FSuspenseCoreItemInstance(ItemID, Quantity);

	// Initialize durability if applicable
	if (ItemData.bIsWeapon || ItemData.bIsArmor)
	{
		OutInstance.SetProperty(FName("Durability"), 100.0f);
	}

	// Initialize weapon state if applicable
	if (ItemData.bIsWeapon)
	{
		OutInstance.WeaponState.bHasState = true;
		OutInstance.WeaponState.CurrentAmmo = ItemData.WeaponConfig.MagazineSize;
		OutInstance.WeaponState.ReserveAmmo = 0;
	}

	return true;
}

bool USuspenseCoreInventoryManager::GetItemData(FName ItemID, FSuspenseCoreItemData& OutData) const
{
	USuspenseCoreDataManager* DataMgr = const_cast<USuspenseCoreInventoryManager*>(this)->GetDataManager();
	if (!DataMgr)
	{
		return false;
	}

	return DataMgr->GetItemData(ItemID, OutData);
}

bool USuspenseCoreInventoryManager::ValidateAllInventories(TArray<FString>& OutErrors) const
{
	OutErrors.Empty();
	bool bAllValid = true;

	for (const TWeakObjectPtr<USuspenseCoreInventoryComponent>& Weak : RegisteredInventories)
	{
		if (USuspenseCoreInventoryComponent* Comp = Weak.Get())
		{
			TArray<FString> CompErrors;
			if (!Comp->ValidateIntegrity(CompErrors))
			{
				bAllValid = false;
				for (const FString& Error : CompErrors)
				{
					OutErrors.Add(FString::Printf(TEXT("[%s] %s"),
						Comp->GetOwner() ? *Comp->GetOwner()->GetName() : TEXT("Unknown"),
						*Error));
				}
			}
		}
	}

	return bAllValid;
}

int32 USuspenseCoreInventoryManager::RepairAllInventories(TArray<FString>& OutRepairLog)
{
	OutRepairLog.Empty();
	int32 TotalRepairs = 0;

	// Implementation would iterate inventories and call repair methods
	// This is a placeholder for the actual repair logic

	return TotalRepairs;
}

USuspenseCoreEventManager* USuspenseCoreInventoryManager::GetEventManager() const
{
	if (CachedEventManager.IsValid())
	{
		return CachedEventManager.Get();
	}

	UGameInstance* GI = GetGameInstance();
	if (GI)
	{
		USuspenseCoreEventManager* EventMgr = GI->GetSubsystem<USuspenseCoreEventManager>();
		const_cast<USuspenseCoreInventoryManager*>(this)->CachedEventManager = EventMgr;
		return EventMgr;
	}

	return nullptr;
}

USuspenseCoreDataManager* USuspenseCoreInventoryManager::GetDataManager() const
{
	if (CachedDataManager.IsValid())
	{
		return CachedDataManager.Get();
	}

	UGameInstance* GI = GetGameInstance();
	if (GI)
	{
		USuspenseCoreDataManager* DataMgr = GI->GetSubsystem<USuspenseCoreDataManager>();
		const_cast<USuspenseCoreInventoryManager*>(this)->CachedDataManager = DataMgr;
		return DataMgr;
	}

	return nullptr;
}

void USuspenseCoreInventoryManager::BroadcastGlobalEvent(FGameplayTag EventTag, const TMap<FName, FString>& Payload)
{
	USuspenseCoreEventManager* EventMgr = GetEventManager();
	if (!EventMgr)
	{
		return;
	}

	USuspenseCoreEventBus* EventBus = EventMgr->GetEventBus();
	if (EventBus)
	{
		FSuspenseCoreEventData EventData;
		EventData.Source = const_cast<USuspenseCoreInventoryManager*>(this);
		EventData.StringPayload = Payload;
		EventBus->Publish(EventTag, EventData);
	}
}

FString USuspenseCoreInventoryManager::GetStatistics() const
{
	const_cast<USuspenseCoreInventoryManager*>(this)->CleanupStaleReferences();

	FString Stats = FString::Printf(TEXT("SuspenseCoreInventoryManager Statistics:\n"));
	Stats += FString::Printf(TEXT("  Registered Inventories: %d\n"), RegisteredInventories.Num());

	int32 TotalItems = 0;
	float TotalWeight = 0.0f;

	for (const TWeakObjectPtr<USuspenseCoreInventoryComponent>& Weak : RegisteredInventories)
	{
		if (USuspenseCoreInventoryComponent* Comp = Weak.Get())
		{
			TotalItems += Comp->Execute_GetTotalItemCount(Comp);
			TotalWeight += Comp->Execute_GetCurrentWeight(Comp);
		}
	}

	Stats += FString::Printf(TEXT("  Total Items: %d\n"), TotalItems);
	Stats += FString::Printf(TEXT("  Total Weight: %.2f\n"), TotalWeight);

	return Stats;
}

void USuspenseCoreInventoryManager::CleanupStaleReferences()
{
	RegisteredInventories.RemoveAll([](const TWeakObjectPtr<USuspenseCoreInventoryComponent>& Weak)
	{
		return !Weak.IsValid();
	});
}
