// SuspenseCoreInventoryEvents.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreInventoryEvents.generated.h"

/**
 * SuspenseCore Inventory EventBus Tags
 *
 * All inventory events use the prefix: SuspenseCore.Event.Inventory.*
 *
 * USAGE:
 * - Subscribe via EventBus->Subscribe(Tag, Callback)
 * - Publish via EventBus->Publish(Tag, EventData)
 *
 * EVENT DATA KEYS:
 * Standard keys used in FSuspenseCoreEventData for inventory events:
 * - "InstanceID" (FGuid as string)
 * - "ItemID" (FName as string)
 * - "Quantity" (int32)
 * - "SlotIndex" (int32)
 * - "PreviousSlot" (int32)
 * - "NewSlot" (int32)
 * - "CurrentWeight" (float)
 * - "MaxWeight" (float)
 * - "ErrorCode" (int32 - ESuspenseCoreInventoryResult)
 * - "ErrorMessage" (FString)
 */

/**
 * USuspenseCoreInventoryEventTags
 *
 * Static class providing GameplayTag constants for inventory events.
 * Use these instead of hardcoding tag strings.
 */
UCLASS()
class INVENTORYSYSTEM_API USuspenseCoreInventoryEventTags : public UObject
{
	GENERATED_BODY()

public:
	//==================================================================
	// Item Events
	//==================================================================

	/** Item was added to inventory */
	static FGameplayTag ItemAdded()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Inventory.ItemAdded"));
		return Tag;
	}

	/** Item was removed from inventory */
	static FGameplayTag ItemRemoved()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Inventory.ItemRemoved"));
		return Tag;
	}

	/** Item was moved within inventory */
	static FGameplayTag ItemMoved()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Inventory.ItemMoved"));
		return Tag;
	}

	/** Item was rotated */
	static FGameplayTag ItemRotated()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Inventory.ItemRotated"));
		return Tag;
	}

	/** Item quantity changed (partial add/remove) */
	static FGameplayTag ItemQuantityChanged()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Inventory.ItemQuantityChanged"));
		return Tag;
	}

	/** Items were swapped between slots */
	static FGameplayTag ItemsSwapped()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Inventory.ItemsSwapped"));
		return Tag;
	}

	//==================================================================
	// Stack Events
	//==================================================================

	/** Stack was split */
	static FGameplayTag StackSplit()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Inventory.StackSplit"));
		return Tag;
	}

	/** Stacks were merged */
	static FGameplayTag StacksMerged()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Inventory.StacksMerged"));
		return Tag;
	}

	/** Stacks were consolidated */
	static FGameplayTag StacksConsolidated()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Inventory.StacksConsolidated"));
		return Tag;
	}

	//==================================================================
	// Inventory State Events
	//==================================================================

	/** Inventory was updated (general change) */
	static FGameplayTag Updated()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Inventory.Updated"));
		return Tag;
	}

	/** Inventory was initialized */
	static FGameplayTag Initialized()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Inventory.Initialized"));
		return Tag;
	}

	/** Inventory was cleared */
	static FGameplayTag Cleared()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Inventory.Cleared"));
		return Tag;
	}

	/** Inventory configuration changed (grid size, max weight) */
	static FGameplayTag ConfigChanged()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Inventory.ConfigChanged"));
		return Tag;
	}

	//==================================================================
	// Weight Events
	//==================================================================

	/** Weight limit was exceeded (operation failed) */
	static FGameplayTag WeightLimitExceeded()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Inventory.WeightLimitExceeded"));
		return Tag;
	}

	/** Weight changed */
	static FGameplayTag WeightChanged()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Inventory.WeightChanged"));
		return Tag;
	}

	//==================================================================
	// Request Events (for decoupled operations)
	//==================================================================

	/** Request to add item (from pickup, reward, etc.) */
	static FGameplayTag AddItemRequest()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Inventory.AddItemRequest"));
		return Tag;
	}

	/** Request to remove item */
	static FGameplayTag RemoveItemRequest()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Inventory.RemoveItemRequest"));
		return Tag;
	}

	/** Request to drop item to world */
	static FGameplayTag DropItemRequest()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Inventory.DropItemRequest"));
		return Tag;
	}

	/** Request to use/consume item */
	static FGameplayTag UseItemRequest()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Inventory.UseItemRequest"));
		return Tag;
	}

	//==================================================================
	// Transaction Events
	//==================================================================

	/** Transaction started */
	static FGameplayTag TransactionStarted()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Inventory.TransactionStarted"));
		return Tag;
	}

	/** Transaction committed */
	static FGameplayTag TransactionCommitted()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Inventory.TransactionCommitted"));
		return Tag;
	}

	/** Transaction rolled back */
	static FGameplayTag TransactionRolledBack()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Inventory.TransactionRolledBack"));
		return Tag;
	}

	//==================================================================
	// Error Events
	//==================================================================

	/** Operation failed */
	static FGameplayTag OperationFailed()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Inventory.OperationFailed"));
		return Tag;
	}

	/** Validation failed */
	static FGameplayTag ValidationFailed()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Inventory.ValidationFailed"));
		return Tag;
	}

	//==================================================================
	// Network Events
	//==================================================================

	/** Inventory state replicated from server */
	static FGameplayTag Replicated()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Inventory.Replicated"));
		return Tag;
	}

	/** Item replicated (individual item update) */
	static FGameplayTag ItemReplicated()
	{
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Inventory.ItemReplicated"));
		return Tag;
	}
};

/**
 * Inventory Event Tag Macros
 *
 * Convenience macros for common inventory event tags.
 * Use these for cleaner code.
 */
#define SUSPENSE_INV_EVENT_ITEM_ADDED			USuspenseCoreInventoryEventTags::ItemAdded()
#define SUSPENSE_INV_EVENT_ITEM_REMOVED			USuspenseCoreInventoryEventTags::ItemRemoved()
#define SUSPENSE_INV_EVENT_ITEM_MOVED			USuspenseCoreInventoryEventTags::ItemMoved()
#define SUSPENSE_INV_EVENT_ITEM_ROTATED			USuspenseCoreInventoryEventTags::ItemRotated()
#define SUSPENSE_INV_EVENT_ITEM_QTY_CHANGED		USuspenseCoreInventoryEventTags::ItemQuantityChanged()
#define SUSPENSE_INV_EVENT_ITEMS_SWAPPED		USuspenseCoreInventoryEventTags::ItemsSwapped()

#define SUSPENSE_INV_EVENT_STACK_SPLIT			USuspenseCoreInventoryEventTags::StackSplit()
#define SUSPENSE_INV_EVENT_STACKS_MERGED		USuspenseCoreInventoryEventTags::StacksMerged()
#define SUSPENSE_INV_EVENT_STACKS_CONSOLIDATED	USuspenseCoreInventoryEventTags::StacksConsolidated()

#define SUSPENSE_INV_EVENT_UPDATED				USuspenseCoreInventoryEventTags::Updated()
#define SUSPENSE_INV_EVENT_INITIALIZED			USuspenseCoreInventoryEventTags::Initialized()
#define SUSPENSE_INV_EVENT_CLEARED				USuspenseCoreInventoryEventTags::Cleared()
#define SUSPENSE_INV_EVENT_CONFIG_CHANGED		USuspenseCoreInventoryEventTags::ConfigChanged()

#define SUSPENSE_INV_EVENT_WEIGHT_EXCEEDED		USuspenseCoreInventoryEventTags::WeightLimitExceeded()
#define SUSPENSE_INV_EVENT_WEIGHT_CHANGED		USuspenseCoreInventoryEventTags::WeightChanged()

#define SUSPENSE_INV_EVENT_ADD_REQUEST			USuspenseCoreInventoryEventTags::AddItemRequest()
#define SUSPENSE_INV_EVENT_REMOVE_REQUEST		USuspenseCoreInventoryEventTags::RemoveItemRequest()
#define SUSPENSE_INV_EVENT_DROP_REQUEST			USuspenseCoreInventoryEventTags::DropItemRequest()
#define SUSPENSE_INV_EVENT_USE_REQUEST			USuspenseCoreInventoryEventTags::UseItemRequest()

#define SUSPENSE_INV_EVENT_TXN_STARTED			USuspenseCoreInventoryEventTags::TransactionStarted()
#define SUSPENSE_INV_EVENT_TXN_COMMITTED		USuspenseCoreInventoryEventTags::TransactionCommitted()
#define SUSPENSE_INV_EVENT_TXN_ROLLED_BACK		USuspenseCoreInventoryEventTags::TransactionRolledBack()

#define SUSPENSE_INV_EVENT_OPERATION_FAILED		USuspenseCoreInventoryEventTags::OperationFailed()
#define SUSPENSE_INV_EVENT_VALIDATION_FAILED	USuspenseCoreInventoryEventTags::ValidationFailed()

#define SUSPENSE_INV_EVENT_REPLICATED			USuspenseCoreInventoryEventTags::Replicated()
#define SUSPENSE_INV_EVENT_ITEM_REPLICATED		USuspenseCoreInventoryEventTags::ItemReplicated()
