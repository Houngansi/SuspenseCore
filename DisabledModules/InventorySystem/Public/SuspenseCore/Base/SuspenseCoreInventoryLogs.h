// SuspenseCoreInventoryLogs.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * SuspenseCore Inventory Logging Categories
 *
 * USAGE:
 * UE_LOG(LogSuspenseCoreInventory, Log, TEXT("Message"));
 * UE_LOG(LogSuspenseCoreInventoryOps, Verbose, TEXT("Operation details"));
 * UE_LOG(LogSuspenseCoreInventoryNet, Warning, TEXT("Network issue"));
 */

// Primary inventory log category
INVENTORYSYSTEM_API DECLARE_LOG_CATEGORY_EXTERN(LogSuspenseCoreInventory, Log, All);

// Operations (add, remove, move, swap, etc.)
INVENTORYSYSTEM_API DECLARE_LOG_CATEGORY_EXTERN(LogSuspenseCoreInventoryOps, Log, All);

// Network replication
INVENTORYSYSTEM_API DECLARE_LOG_CATEGORY_EXTERN(LogSuspenseCoreInventoryNet, Log, All);

// Serialization and save/load
INVENTORYSYSTEM_API DECLARE_LOG_CATEGORY_EXTERN(LogSuspenseCoreInventorySave, Log, All);

// Validation
INVENTORYSYSTEM_API DECLARE_LOG_CATEGORY_EXTERN(LogSuspenseCoreInventoryValidation, Log, All);

// Transactions
INVENTORYSYSTEM_API DECLARE_LOG_CATEGORY_EXTERN(LogSuspenseCoreInventoryTxn, Log, All);

/**
 * FSuspenseCoreInventoryLogHelper
 *
 * Static helper for formatted inventory logging.
 */
struct INVENTORYSYSTEM_API FSuspenseCoreInventoryLogHelper
{
	/** Log item added */
	static void LogItemAdded(const FName& ItemID, int32 Quantity, int32 SlotIndex)
	{
		UE_LOG(LogSuspenseCoreInventoryOps, Log,
			TEXT("ItemAdded: %s x%d at Slot %d"),
			*ItemID.ToString(), Quantity, SlotIndex);
	}

	/** Log item removed */
	static void LogItemRemoved(const FName& ItemID, int32 Quantity, int32 SlotIndex)
	{
		UE_LOG(LogSuspenseCoreInventoryOps, Log,
			TEXT("ItemRemoved: %s x%d from Slot %d"),
			*ItemID.ToString(), Quantity, SlotIndex);
	}

	/** Log item moved */
	static void LogItemMoved(const FGuid& InstanceID, int32 FromSlot, int32 ToSlot)
	{
		UE_LOG(LogSuspenseCoreInventoryOps, Log,
			TEXT("ItemMoved: %s from Slot %d to Slot %d"),
			*InstanceID.ToString(), FromSlot, ToSlot);
	}

	/** Log validation failure */
	static void LogValidationFailed(const FString& Reason)
	{
		UE_LOG(LogSuspenseCoreInventoryValidation, Warning,
			TEXT("Validation Failed: %s"), *Reason);
	}

	/** Log transaction started */
	static void LogTransactionStarted(const FGuid& TxnID)
	{
		UE_LOG(LogSuspenseCoreInventoryTxn, Verbose,
			TEXT("Transaction Started: %s"), *TxnID.ToString());
	}

	/** Log transaction committed */
	static void LogTransactionCommitted(const FGuid& TxnID)
	{
		UE_LOG(LogSuspenseCoreInventoryTxn, Log,
			TEXT("Transaction Committed: %s"), *TxnID.ToString());
	}

	/** Log transaction rolled back */
	static void LogTransactionRolledBack(const FGuid& TxnID)
	{
		UE_LOG(LogSuspenseCoreInventoryTxn, Warning,
			TEXT("Transaction Rolled Back: %s"), *TxnID.ToString());
	}

	/** Log network replication */
	static void LogReplication(const FString& Action, int32 ItemCount)
	{
		UE_LOG(LogSuspenseCoreInventoryNet, Verbose,
			TEXT("Replication %s: %d items"), *Action, ItemCount);
	}

	/** Log save operation */
	static void LogSave(const FString& SlotName, bool bSuccess)
	{
		if (bSuccess)
		{
			UE_LOG(LogSuspenseCoreInventorySave, Log,
				TEXT("Inventory saved to: %s"), *SlotName);
		}
		else
		{
			UE_LOG(LogSuspenseCoreInventorySave, Error,
				TEXT("Failed to save inventory to: %s"), *SlotName);
		}
	}

	/** Log load operation */
	static void LogLoad(const FString& SlotName, bool bSuccess, int32 ItemCount)
	{
		if (bSuccess)
		{
			UE_LOG(LogSuspenseCoreInventorySave, Log,
				TEXT("Inventory loaded from: %s (%d items)"), *SlotName, ItemCount);
		}
		else
		{
			UE_LOG(LogSuspenseCoreInventorySave, Error,
				TEXT("Failed to load inventory from: %s"), *SlotName);
		}
	}

	/** Log debug dump */
	static void LogInventoryDump(const TArray<FString>& DebugLines)
	{
		UE_LOG(LogSuspenseCoreInventory, Log, TEXT("=== Inventory Dump ==="));
		for (const FString& Line : DebugLines)
		{
			UE_LOG(LogSuspenseCoreInventory, Log, TEXT("  %s"), *Line);
		}
		UE_LOG(LogSuspenseCoreInventory, Log, TEXT("====================="));
	}
};

/**
 * Conditional logging macros
 */
#if !UE_BUILD_SHIPPING
	#define SUSPENSE_INV_LOG(CategoryName, Verbosity, Format, ...) \
		UE_LOG(CategoryName, Verbosity, Format, ##__VA_ARGS__)
#else
	#define SUSPENSE_INV_LOG(CategoryName, Verbosity, Format, ...)
#endif

// Convenience macros
#define SUSPENSE_INV_LOG_ADD(ItemID, Quantity, Slot) \
	FSuspenseCoreInventoryLogHelper::LogItemAdded(ItemID, Quantity, Slot)

#define SUSPENSE_INV_LOG_REMOVE(ItemID, Quantity, Slot) \
	FSuspenseCoreInventoryLogHelper::LogItemRemoved(ItemID, Quantity, Slot)

#define SUSPENSE_INV_LOG_MOVE(InstanceID, FromSlot, ToSlot) \
	FSuspenseCoreInventoryLogHelper::LogItemMoved(InstanceID, FromSlot, ToSlot)

#define SUSPENSE_INV_LOG_VALIDATION_FAIL(Reason) \
	FSuspenseCoreInventoryLogHelper::LogValidationFailed(Reason)
