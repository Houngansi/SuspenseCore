// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Components/Core/SuspenseCoreEquipmentInventoryBridge.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Services/SuspenseCoreServiceLocator.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreBridge, Log, All);

#define BRIDGE_LOG(Verbosity, Format, ...) \
	UE_LOG(LogSuspenseCoreBridge, Verbosity, TEXT("%s: " Format), *GetNameSafe(this), ##__VA_ARGS__)

USuspenseCoreEquipmentInventoryBridge::USuspenseCoreEquipmentInventoryBridge()
{
	bIsInitialized = false;
	LastSyncTime = 0.0f;
	TotalTransfers = 0;
	FailedTransfers = 0;
	TotalTransactions = 0;

	// Configuration defaults
	MaxActiveTransactions = 10;
	MaxTransactionHistory = 50;
}

bool USuspenseCoreEquipmentInventoryBridge::Initialize(
	USuspenseCoreServiceLocator* InServiceLocator,
	UObject* InInventoryComponent,
	UObject* InEquipmentComponent)
{
	if (!InServiceLocator)
	{
		BRIDGE_LOG(Error, TEXT("Initialize: Invalid ServiceLocator"));
		return false;
	}

	ServiceLocator = InServiceLocator;
	InventoryComponent = InInventoryComponent;
	EquipmentComponent = InEquipmentComponent;

	// Get EventBus from ServiceLocator
	EventBus = ServiceLocator->GetService<USuspenseCoreEventBus>();
	if (!EventBus.IsValid())
	{
		BRIDGE_LOG(Warning, TEXT("Initialize: EventBus not found in ServiceLocator"));
	}

	// Setup event subscriptions
	SetupEventSubscriptions();

	bIsInitialized = true;
	BRIDGE_LOG(Log, TEXT("Initialize: Success"));
	return true;
}

void USuspenseCoreEquipmentInventoryBridge::Shutdown()
{
	BRIDGE_LOG(Log, TEXT("Shutdown"));

	FScopeLock Lock(&TransactionCriticalSection);

	// Clear all transactions
	ActiveTransactions.Empty();
	TransactionHistory.Empty();

	// Cleanup
	ServiceLocator.Reset();
	EventBus.Reset();
	InventoryComponent.Reset();
	EquipmentComponent.Reset();

	bIsInitialized = false;
}

bool USuspenseCoreEquipmentInventoryBridge::TransferToInventory(
	const FSuspenseInventoryItemInstance& ItemInstance,
	int32 SlotIndex)
{
	if (!bIsInitialized)
	{
		BRIDGE_LOG(Error, TEXT("TransferToInventory: Bridge not initialized"));
		return false;
	}

	BRIDGE_LOG(Log, TEXT("TransferToInventory: Slot %d"), SlotIndex);

	// Create transaction
	FGameplayTag EquipmentTag = FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.System.Equipment")));
	FGameplayTag InventoryTag = FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.System.Inventory")));

	FSuspenseCoreEquipmentInventoryTransaction Transaction =
		FSuspenseCoreEquipmentInventoryTransaction::Create(ItemInstance, EquipmentTag, InventoryTag);

	// Begin transaction
	FGuid TransactionId = BeginTransaction(ItemInstance, EquipmentTag, InventoryTag);

	// Execute transfer
	bool bSuccess = ExecuteTransfer(Transaction);

	// Commit or rollback
	if (bSuccess)
	{
		CommitTransaction(TransactionId);
	}
	else
	{
		RollbackTransaction(TransactionId);
		FailedTransfers++;
	}

	TotalTransfers++;
	return bSuccess;
}

bool USuspenseCoreEquipmentInventoryBridge::ReturnEquippedItemToInventory(int32 SlotIndex)
{
	FSuspenseInventoryItemInstance ItemInstance;
	if (!GetEquippedItem(SlotIndex, ItemInstance))
	{
		BRIDGE_LOG(Error, TEXT("ReturnEquippedItemToInventory: No item in slot %d"), SlotIndex);
		return false;
	}

	return TransferToInventory(ItemInstance, SlotIndex);
}

bool USuspenseCoreEquipmentInventoryBridge::TransferToEquipment(
	const FSuspenseInventoryItemInstance& ItemInstance,
	int32 TargetSlotIndex)
{
	if (!bIsInitialized)
	{
		BRIDGE_LOG(Error, TEXT("TransferToEquipment: Bridge not initialized"));
		return false;
	}

	// Validate transfer
	FText ValidationError;
	if (!CanEquipItem(ItemInstance, TargetSlotIndex, ValidationError))
	{
		BRIDGE_LOG(Warning, TEXT("TransferToEquipment: Validation failed - %s"),
			*ValidationError.ToString());
		FailedTransfers++;
		return false;
	}

	BRIDGE_LOG(Log, TEXT("TransferToEquipment: Slot %d"), TargetSlotIndex);

	// Create transaction
	FGameplayTag InventoryTag = FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.System.Inventory")));
	FGameplayTag EquipmentTag = FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.System.Equipment")));

	FSuspenseCoreEquipmentInventoryTransaction Transaction =
		FSuspenseCoreEquipmentInventoryTransaction::Create(ItemInstance, InventoryTag, EquipmentTag);

	// Begin transaction
	FGuid TransactionId = BeginTransaction(ItemInstance, InventoryTag, EquipmentTag);

	// Execute transfer
	bool bSuccess = ExecuteTransfer(Transaction);

	// Commit or rollback
	if (bSuccess)
	{
		CommitTransaction(TransactionId);
	}
	else
	{
		RollbackTransaction(TransactionId);
		FailedTransfers++;
	}

	TotalTransfers++;
	return bSuccess;
}

bool USuspenseCoreEquipmentInventoryBridge::EquipFromInventory(
	int32 InventorySlotIndex,
	int32 EquipmentSlotIndex)
{
	// TODO: Get item from inventory slot
	FSuspenseInventoryItemInstance ItemInstance;

	return TransferToEquipment(ItemInstance, EquipmentSlotIndex);
}

FGuid USuspenseCoreEquipmentInventoryBridge::BeginTransaction(
	const FSuspenseInventoryItemInstance& Item,
	FGameplayTag Source,
	FGameplayTag Target)
{
	FScopeLock Lock(&TransactionCriticalSection);

	// Check transaction limit
	if (ActiveTransactions.Num() >= MaxActiveTransactions)
	{
		BRIDGE_LOG(Warning, TEXT("BeginTransaction: Max active transactions reached"));
		return FGuid();
	}

	// Create transaction
	FSuspenseCoreEquipmentInventoryTransaction Transaction =
		FSuspenseCoreEquipmentInventoryTransaction::Create(Item, Source, Target);

	ActiveTransactions.Add(Transaction);
	TotalTransactions++;

	BRIDGE_LOG(Verbose, TEXT("BeginTransaction: %s"), *Transaction.TransactionId.ToString());

	PublishTransferStarted(Transaction);

	return Transaction.TransactionId;
}

bool USuspenseCoreEquipmentInventoryBridge::CommitTransaction(FGuid TransactionId)
{
	FScopeLock Lock(&TransactionCriticalSection);

	FSuspenseCoreEquipmentInventoryTransaction* Transaction = FindTransaction(TransactionId);
	if (!Transaction)
	{
		BRIDGE_LOG(Error, TEXT("CommitTransaction: Transaction not found"));
		return false;
	}

	Transaction->bCompleted = true;

	// Move to history
	if (TransactionHistory.Num() >= MaxTransactionHistory)
	{
		TransactionHistory.RemoveAt(0);
	}
	TransactionHistory.Add(*Transaction);

	// Remove from active
	ActiveTransactions.RemoveAll([&](const FSuspenseCoreEquipmentInventoryTransaction& Trans)
	{
		return Trans.TransactionId == TransactionId;
	});

	BRIDGE_LOG(Verbose, TEXT("CommitTransaction: %s"), *TransactionId.ToString());

	PublishTransferCompleted(*Transaction, true);

	return true;
}

void USuspenseCoreEquipmentInventoryBridge::RollbackTransaction(FGuid TransactionId)
{
	FScopeLock Lock(&TransactionCriticalSection);

	FSuspenseCoreEquipmentInventoryTransaction* Transaction = FindTransaction(TransactionId);
	if (!Transaction)
	{
		BRIDGE_LOG(Error, TEXT("RollbackTransaction: Transaction not found"));
		return;
	}

	BRIDGE_LOG(Warning, TEXT("RollbackTransaction: %s"), *TransactionId.ToString());

	PublishTransferCompleted(*Transaction, false);

	// Remove from active
	ActiveTransactions.RemoveAll([&](const FSuspenseCoreEquipmentInventoryTransaction& Trans)
	{
		return Trans.TransactionId == TransactionId;
	});
}

void USuspenseCoreEquipmentInventoryBridge::SynchronizeSystems()
{
	BRIDGE_LOG(Log, TEXT("SynchronizeSystems"));

	LastSyncTime = 0.0f; // TODO: Get actual game time

	PublishSynchronization(true);
}

bool USuspenseCoreEquipmentInventoryBridge::ValidateConsistency(TArray<FText>& OutErrors) const
{
	// TODO: Implement consistency validation between equipment and inventory
	return true;
}

bool USuspenseCoreEquipmentInventoryBridge::GetEquippedItem(
	int32 SlotIndex,
	FSuspenseInventoryItemInstance& OutItem) const
{
	// TODO: Get item from equipment component
	return false;
}

bool USuspenseCoreEquipmentInventoryBridge::CanEquipItem(
	const FSuspenseInventoryItemInstance& ItemInstance,
	int32 TargetSlotIndex,
	FText& OutReason) const
{
	// TODO: Validate through equipment system
	return true;
}

void USuspenseCoreEquipmentInventoryBridge::ResetStatistics()
{
	TotalTransfers = 0;
	FailedTransfers = 0;
	TotalTransactions = 0;
	BRIDGE_LOG(Log, TEXT("ResetStatistics"));
}

void USuspenseCoreEquipmentInventoryBridge::OnItemEquipped(
	FGameplayTag EventTag,
	const FSuspenseCoreEventData& EventData)
{
	BRIDGE_LOG(Verbose, TEXT("OnItemEquipped"));
	// TODO: Handle equipment event
}

void USuspenseCoreEquipmentInventoryBridge::OnItemUnequipped(
	FGameplayTag EventTag,
	const FSuspenseCoreEventData& EventData)
{
	BRIDGE_LOG(Verbose, TEXT("OnItemUnequipped"));
	// TODO: Handle unequip event
}

void USuspenseCoreEquipmentInventoryBridge::OnInventoryChanged(
	FGameplayTag EventTag,
	const FSuspenseCoreEventData& EventData)
{
	BRIDGE_LOG(Verbose, TEXT("OnInventoryChanged"));
	// TODO: Handle inventory change event
}

void USuspenseCoreEquipmentInventoryBridge::PublishTransferStarted(
	const FSuspenseCoreEquipmentInventoryTransaction& Transaction)
{
	if (!EventBus.IsValid())
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
	EventData.SetString(TEXT("TransactionId"), Transaction.TransactionId.ToString());
	EventData.SetString(TEXT("SourceSystem"), Transaction.SourceSystem.ToString());
	EventData.SetString(TEXT("TargetSystem"), Transaction.TargetSystem.ToString());

	EventBus->Publish(
		FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.Bridge.Transfer.Started"))),
		EventData);
}

void USuspenseCoreEquipmentInventoryBridge::PublishTransferCompleted(
	const FSuspenseCoreEquipmentInventoryTransaction& Transaction,
	bool bSuccess)
{
	if (!EventBus.IsValid())
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
	EventData.SetString(TEXT("TransactionId"), Transaction.TransactionId.ToString());
	EventData.SetBool(TEXT("Success"), bSuccess);

	EventBus->Publish(
		FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.Bridge.Transfer.Completed"))),
		EventData);
}

void USuspenseCoreEquipmentInventoryBridge::PublishSynchronization(bool bSuccess)
{
	if (!EventBus.IsValid())
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
	EventData.SetBool(TEXT("Success"), bSuccess);

	EventBus->Publish(
		FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.Bridge.Synchronized"))),
		EventData);
}

bool USuspenseCoreEquipmentInventoryBridge::ExecuteTransfer(
	FSuspenseCoreEquipmentInventoryTransaction& Transaction)
{
	// Validate transfer
	FText ValidationError;
	if (!ValidateTransfer(Transaction, ValidationError))
	{
		BRIDGE_LOG(Warning, TEXT("ExecuteTransfer: Validation failed - %s"),
			*ValidationError.ToString());
		return false;
	}

	// TODO: Execute actual transfer logic
	BRIDGE_LOG(Log, TEXT("ExecuteTransfer: %s -> %s"),
		*Transaction.SourceSystem.ToString(),
		*Transaction.TargetSystem.ToString());

	return true;
}

bool USuspenseCoreEquipmentInventoryBridge::ValidateTransfer(
	const FSuspenseCoreEquipmentInventoryTransaction& Transaction,
	FText& OutError) const
{
	if (!Transaction.ItemInstance.IsValid())
	{
		OutError = FText::FromString(TEXT("Invalid item instance"));
		return false;
	}

	if (!Transaction.SourceSystem.IsValid() || !Transaction.TargetSystem.IsValid())
	{
		OutError = FText::FromString(TEXT("Invalid source or target system"));
		return false;
	}

	return true;
}

void USuspenseCoreEquipmentInventoryBridge::SetupEventSubscriptions()
{
	if (!EventBus.IsValid())
	{
		return;
	}

	// Subscribe to equipment events
	EventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.Equipment.ItemEquipped"))),
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(
			this,
			&USuspenseCoreEquipmentInventoryBridge::OnItemEquipped));

	EventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.Equipment.ItemUnequipped"))),
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(
			this,
			&USuspenseCoreEquipmentInventoryBridge::OnItemUnequipped));

	// Subscribe to inventory events
	EventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Event.Inventory.Changed"))),
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(
			this,
			&USuspenseCoreEquipmentInventoryBridge::OnInventoryChanged));

	BRIDGE_LOG(Log, TEXT("SetupEventSubscriptions: Complete"));
}

FSuspenseCoreEquipmentInventoryTransaction* USuspenseCoreEquipmentInventoryBridge::FindTransaction(
	FGuid TransactionId)
{
	for (FSuspenseCoreEquipmentInventoryTransaction& Trans : ActiveTransactions)
	{
		if (Trans.TransactionId == TransactionId)
		{
			return &Trans;
		}
	}

	return nullptr;
}
