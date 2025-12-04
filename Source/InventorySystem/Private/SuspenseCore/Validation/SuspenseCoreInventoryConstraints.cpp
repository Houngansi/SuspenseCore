// SuspenseCoreInventoryConstraints.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Validation/SuspenseCoreInventoryConstraints.h"
#include "SuspenseCore/Components/SuspenseCoreInventoryComponent.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Base/SuspenseCoreInventoryLogs.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

USuspenseCoreInventoryConstraints::USuspenseCoreInventoryConstraints()
{
}

FSuspenseCoreValidationResult USuspenseCoreInventoryConstraints::ValidateOperation(
	const FSuspenseCoreConstraintContext& Context)
{
	FSuspenseCoreValidationResult Result;

	// Basic validation
	if (!Context.SourceInventory.IsValid())
	{
		Result.AddViolation(FName("NullInventory"),
			NSLOCTEXT("SuspenseCore", "NullInventory", "Inventory is null"));
		return Result;
	}

	if (!Context.Item.IsValid())
	{
		Result.AddViolation(FName("InvalidItem"),
			NSLOCTEXT("SuspenseCore", "InvalidItem", "Item is invalid"));
		return Result;
	}

	// Get item data
	FSuspenseCoreItemData ItemData;
	if (!GetItemDataForValidation(Context.Item.ItemID, ItemData))
	{
		// Allow operation even without item data (graceful degradation)
		UE_LOG(LogSuspenseCoreInventory, Warning,
			TEXT("ValidateOperation: No item data for %s, allowing by default"),
			*Context.Item.ItemID.ToString());
		return Result;
	}

	// Get rules for this inventory
	FSuspenseCoreInventoryRules Rules;
	GetInventoryRules(Context.SourceInventory.Get(), Rules);

	// Validate based on operation type
	switch (Context.OperationType)
	{
	case ESuspenseCoreOperationType::Add:
		ValidateItemType(Rules, ItemData, Result);
		if (Context.TargetSlot >= 0)
		{
			ValidateSlotConstraints(Rules, Context.TargetSlot, Context.Item, ItemData, Result);
		}
		ValidateWeight(Context.SourceInventory.Get(), Context.Item, ItemData, Result);
		ValidateQuantityLimits(Context.SourceInventory.Get(), Rules, Context.Item, Result);
		break;

	case ESuspenseCoreOperationType::Remove:
		// Usually no constraints on removal, but check locked slots
		if (Context.Item.SlotIndex >= 0 && IsSlotLocked(Context.SourceInventory.Get(), Context.Item.SlotIndex))
		{
			Result.AddViolation(FName("SlotLocked"),
				NSLOCTEXT("SuspenseCore", "SlotLocked", "Cannot remove from locked slot"));
		}
		break;

	case ESuspenseCoreOperationType::Move:
		if (Context.TargetSlot >= 0)
		{
			ValidateSlotConstraints(Rules, Context.TargetSlot, Context.Item, ItemData, Result);

			if (IsSlotLocked(Context.SourceInventory.Get(), Context.TargetSlot))
			{
				Result.AddViolation(FName("TargetSlotLocked"),
					NSLOCTEXT("SuspenseCore", "TargetSlotLocked", "Target slot is locked"));
			}
		}
		break;

	case ESuspenseCoreOperationType::Transfer:
		if (Context.TargetInventory.IsValid())
		{
			FSuspenseCoreInventoryRules TargetRules;
			GetInventoryRules(Context.TargetInventory.Get(), TargetRules);

			ValidateItemType(TargetRules, ItemData, Result);
			if (Context.TargetSlot >= 0)
			{
				ValidateSlotConstraints(TargetRules, Context.TargetSlot, Context.Item, ItemData, Result);
			}
			ValidateWeight(Context.TargetInventory.Get(), Context.Item, ItemData, Result);
		}
		break;

	case ESuspenseCoreOperationType::Stack:
		if (!Rules.bAllowStacking)
		{
			Result.AddViolation(FName("StackingDisabled"),
				NSLOCTEXT("SuspenseCore", "StackingDisabled", "Stacking is not allowed"));
		}
		break;

	case ESuspenseCoreOperationType::Rotate:
		if (!Rules.bAllowRotation)
		{
			Result.AddViolation(FName("RotationDisabled"),
				NSLOCTEXT("SuspenseCore", "RotationDisabled", "Rotation is not allowed"));
		}
		break;

	default:
		break;
	}

	// Custom validation hook
	FSuspenseCoreConstraintViolation CustomViolation;
	if (!CustomValidation(Context, CustomViolation))
	{
		Result.AddViolation(CustomViolation);
	}

	return Result;
}

FSuspenseCoreValidationResult USuspenseCoreInventoryConstraints::ValidateAddItem(
	USuspenseCoreInventoryComponent* Inventory,
	const FSuspenseCoreItemInstance& Item,
	int32 TargetSlot)
{
	FSuspenseCoreConstraintContext Context;
	Context.SourceInventory = Inventory;
	Context.Item = Item;
	Context.TargetSlot = TargetSlot;
	Context.OperationType = ESuspenseCoreOperationType::Add;

	return ValidateOperation(Context);
}

FSuspenseCoreValidationResult USuspenseCoreInventoryConstraints::ValidateRemoveItem(
	USuspenseCoreInventoryComponent* Inventory,
	const FSuspenseCoreItemInstance& Item,
	int32 Quantity)
{
	FSuspenseCoreConstraintContext Context;
	Context.SourceInventory = Inventory;
	Context.Item = Item;
	Context.OperationType = ESuspenseCoreOperationType::Remove;

	FSuspenseCoreValidationResult Result = ValidateOperation(Context);

	// Check quantity
	if (Quantity > 0 && Quantity > Item.Quantity)
	{
		Result.AddViolation(FName("InsufficientQuantity"),
			FText::Format(NSLOCTEXT("SuspenseCore", "InsufficientQuantity",
				"Cannot remove {0}, only {1} available"),
				FText::AsNumber(Quantity), FText::AsNumber(Item.Quantity)));
		Result.MaxAllowedQuantity = Item.Quantity;
	}

	return Result;
}

FSuspenseCoreValidationResult USuspenseCoreInventoryConstraints::ValidateMoveItem(
	USuspenseCoreInventoryComponent* Inventory,
	const FSuspenseCoreItemInstance& Item,
	int32 TargetSlot)
{
	FSuspenseCoreConstraintContext Context;
	Context.SourceInventory = Inventory;
	Context.Item = Item;
	Context.TargetSlot = TargetSlot;
	Context.OperationType = ESuspenseCoreOperationType::Move;

	return ValidateOperation(Context);
}

FSuspenseCoreValidationResult USuspenseCoreInventoryConstraints::ValidateTransfer(
	USuspenseCoreInventoryComponent* SourceInventory,
	USuspenseCoreInventoryComponent* TargetInventory,
	const FSuspenseCoreItemInstance& Item,
	int32 TargetSlot)
{
	FSuspenseCoreConstraintContext Context;
	Context.SourceInventory = SourceInventory;
	Context.TargetInventory = TargetInventory;
	Context.Item = Item;
	Context.TargetSlot = TargetSlot;
	Context.OperationType = ESuspenseCoreOperationType::Transfer;

	return ValidateOperation(Context);
}

bool USuspenseCoreInventoryConstraints::CanAcceptItemType(
	USuspenseCoreInventoryComponent* Inventory,
	FName ItemID)
{
	if (!Inventory)
	{
		return false;
	}

	FSuspenseCoreInventoryRules Rules;
	if (!GetInventoryRules(Inventory, Rules))
	{
		// No rules = accept all
		return true;
	}

	FSuspenseCoreItemData ItemData;
	if (!GetItemDataForValidation(ItemID, ItemData))
	{
		// No data = allow by default
		return true;
	}

	// Check blocked types
	if (!Rules.BlockedItemTypes.IsEmpty() && Rules.BlockedItemTypes.HasAny(ItemData.Identity.ItemTags))
	{
		return false;
	}

	// Check allowed types
	if (!Rules.AllowedItemTypes.IsEmpty() && !Rules.AllowedItemTypes.HasAny(ItemData.Identity.ItemTags))
	{
		return false;
	}

	return true;
}

bool USuspenseCoreInventoryConstraints::CanSlotAcceptItem(
	USuspenseCoreInventoryComponent* Inventory,
	int32 SlotIndex,
	const FSuspenseCoreItemInstance& Item)
{
	if (!Inventory || SlotIndex < 0)
	{
		return false;
	}

	// Check if slot is locked
	if (IsSlotLocked(Inventory, SlotIndex))
	{
		return false;
	}

	FSuspenseCoreInventoryRules Rules;
	if (!GetInventoryRules(Inventory, Rules))
	{
		return true;
	}

	const FSuspenseCoreSlotConstraint* SlotConstraint = Rules.GetSlotConstraint(SlotIndex);
	if (!SlotConstraint)
	{
		return true;
	}

	if (SlotConstraint->bIsLocked)
	{
		return false;
	}

	// Get item data for tag checking
	FSuspenseCoreItemData ItemData;
	if (GetItemDataForValidation(Item.ItemID, ItemData))
	{
		if (!SlotConstraint->AllowsItemType(ItemData.Identity.ItemTags))
		{
			return false;
		}

		// Check size constraint
		FIntPoint ItemSize = ItemData.InventoryProps.GridSize;
		if (Item.Rotation == 90 || Item.Rotation == 270)
		{
			ItemSize = FIntPoint(ItemSize.Y, ItemSize.X);
		}

		if (ItemSize.X > SlotConstraint->MaxItemSize.X || ItemSize.Y > SlotConstraint->MaxItemSize.Y)
		{
			return false;
		}
	}

	return true;
}

int32 USuspenseCoreInventoryConstraints::FindBestSlot(
	USuspenseCoreInventoryComponent* Inventory,
	const FSuspenseCoreItemInstance& Item)
{
	if (!Inventory)
	{
		return -1;
	}

	// Get all empty slots
	TArray<int32> EmptySlots;
	int32 TotalSlots = Inventory->GetGridWidth() * Inventory->GetGridHeight();

	for (int32 i = 0; i < TotalSlots; ++i)
	{
		if (Inventory->IsSlotEmpty(i) && CanSlotAcceptItem(Inventory, i, Item))
		{
			EmptySlots.Add(i);
		}
	}

	// Return first valid slot
	return EmptySlots.Num() > 0 ? EmptySlots[0] : -1;
}

int32 USuspenseCoreInventoryConstraints::GetMaxAddableQuantity(
	USuspenseCoreInventoryComponent* Inventory,
	FName ItemID)
{
	if (!Inventory)
	{
		return 0;
	}

	FSuspenseCoreItemData ItemData;
	if (!GetItemDataForValidation(ItemID, ItemData))
	{
		return MAX_int32; // Unknown item, no limit
	}

	FSuspenseCoreInventoryRules Rules;
	GetInventoryRules(Inventory, Rules);

	int32 MaxQuantity = MAX_int32;

	// Check weight limit
	float RemainingWeight = Inventory->Execute_GetMaxWeight(Inventory) - Inventory->Execute_GetCurrentWeight(Inventory);
	if (ItemData.InventoryProps.Weight > 0)
	{
		int32 ByWeight = FMath::FloorToInt(RemainingWeight / ItemData.InventoryProps.Weight);
		MaxQuantity = FMath::Min(MaxQuantity, ByWeight);
	}

	// Check total quantity limit
	if (Rules.MaxTotalQuantity > 0)
	{
		int32 CurrentTotal = 0;
		TArray<FSuspenseCoreItemInstance> Items = Inventory->GetAllItemInstances();
		for (const FSuspenseCoreItemInstance& Item : Items)
		{
			CurrentTotal += Item.Quantity;
		}
		int32 ByTotalQuantity = Rules.MaxTotalQuantity - CurrentTotal;
		MaxQuantity = FMath::Min(MaxQuantity, FMath::Max(0, ByTotalQuantity));
	}

	return MaxQuantity;
}

void USuspenseCoreInventoryConstraints::SetInventoryRules(
	USuspenseCoreInventoryComponent* Inventory,
	const FSuspenseCoreInventoryRules& Rules)
{
	if (Inventory)
	{
		InventoryRulesMap.Add(Inventory, Rules);
	}
}

bool USuspenseCoreInventoryConstraints::GetInventoryRules(
	USuspenseCoreInventoryComponent* Inventory,
	FSuspenseCoreInventoryRules& OutRules)
{
	if (!Inventory)
	{
		return false;
	}

	const FSuspenseCoreInventoryRules* Found = InventoryRulesMap.Find(Inventory);
	if (Found)
	{
		OutRules = *Found;
		return true;
	}

	return false;
}

void USuspenseCoreInventoryConstraints::ClearInventoryRules(USuspenseCoreInventoryComponent* Inventory)
{
	if (Inventory)
	{
		InventoryRulesMap.Remove(Inventory);
		LockedSlotsMap.Remove(Inventory);
	}
}

void USuspenseCoreInventoryConstraints::LockSlot(USuspenseCoreInventoryComponent* Inventory, int32 SlotIndex)
{
	if (Inventory && SlotIndex >= 0)
	{
		LockedSlotsMap.FindOrAdd(Inventory).Add(SlotIndex);
	}
}

void USuspenseCoreInventoryConstraints::UnlockSlot(USuspenseCoreInventoryComponent* Inventory, int32 SlotIndex)
{
	if (Inventory)
	{
		TSet<int32>* LockedSlots = LockedSlotsMap.Find(Inventory);
		if (LockedSlots)
		{
			LockedSlots->Remove(SlotIndex);
		}
	}
}

bool USuspenseCoreInventoryConstraints::IsSlotLocked(USuspenseCoreInventoryComponent* Inventory, int32 SlotIndex)
{
	if (!Inventory)
	{
		return false;
	}

	const TSet<int32>* LockedSlots = LockedSlotsMap.Find(Inventory);
	if (LockedSlots && LockedSlots->Contains(SlotIndex))
	{
		return true;
	}

	// Also check slot constraints
	FSuspenseCoreInventoryRules Rules;
	if (GetInventoryRules(Inventory, Rules))
	{
		const FSuspenseCoreSlotConstraint* SlotConstraint = Rules.GetSlotConstraint(SlotIndex);
		if (SlotConstraint && SlotConstraint->bIsLocked)
		{
			return true;
		}
	}

	return false;
}

bool USuspenseCoreInventoryConstraints::CustomValidation_Implementation(
	const FSuspenseCoreConstraintContext& Context,
	FSuspenseCoreConstraintViolation& OutViolation)
{
	// Default implementation always passes
	return true;
}

bool USuspenseCoreInventoryConstraints::ValidateItemType(
	const FSuspenseCoreInventoryRules& Rules,
	const FSuspenseCoreItemData& ItemData,
	FSuspenseCoreValidationResult& Result)
{
	// Check blocked types first
	if (!Rules.BlockedItemTypes.IsEmpty())
	{
		if (Rules.BlockedItemTypes.HasAny(ItemData.Identity.ItemTags))
		{
			Result.AddViolation(FName("BlockedItemType"),
				FText::Format(NSLOCTEXT("SuspenseCore", "BlockedItemType",
					"Item type {0} is not allowed in this inventory"),
					FText::FromName(ItemData.Identity.ItemID)));
			return false;
		}
	}

	// Check allowed types
	if (!Rules.AllowedItemTypes.IsEmpty())
	{
		if (!Rules.AllowedItemTypes.HasAny(ItemData.Identity.ItemTags))
		{
			Result.AddViolation(FName("ItemTypeNotAllowed"),
				FText::Format(NSLOCTEXT("SuspenseCore", "ItemTypeNotAllowed",
					"Item type {0} is not accepted by this inventory"),
					FText::FromName(ItemData.Identity.ItemID)));
			return false;
		}
	}

	// Check required tags
	if (!Rules.RequiredTagsForAdd.IsEmpty())
	{
		if (!ItemData.Identity.ItemTags.HasAll(Rules.RequiredTagsForAdd))
		{
			Result.AddViolation(FName("MissingRequiredTags"),
				NSLOCTEXT("SuspenseCore", "MissingRequiredTags",
					"Item missing required tags for this inventory"));
			return false;
		}
	}

	return true;
}

bool USuspenseCoreInventoryConstraints::ValidateSlotConstraints(
	const FSuspenseCoreInventoryRules& Rules,
	int32 SlotIndex,
	const FSuspenseCoreItemInstance& Item,
	const FSuspenseCoreItemData& ItemData,
	FSuspenseCoreValidationResult& Result)
{
	const FSuspenseCoreSlotConstraint* SlotConstraint = Rules.GetSlotConstraint(SlotIndex);
	if (!SlotConstraint)
	{
		return true; // No constraint = allowed
	}

	if (SlotConstraint->bIsLocked)
	{
		Result.AddViolation(FName("SlotLocked"),
			FText::Format(NSLOCTEXT("SuspenseCore", "SlotLockedAt",
				"Slot {0} is locked"),
				FText::AsNumber(SlotIndex)));
		return false;
	}

	// Check type constraints
	if (!SlotConstraint->AllowsItemType(ItemData.Identity.ItemTags))
	{
		Result.AddViolation(FName("SlotTypeRestriction"),
			FText::Format(NSLOCTEXT("SuspenseCore", "SlotTypeRestriction",
				"Slot {0} does not accept this item type"),
				FText::AsNumber(SlotIndex)));
		return false;
	}

	// Check size constraints
	FIntPoint ItemSize = ItemData.InventoryProps.GridSize;
	if (Item.Rotation == 90 || Item.Rotation == 270)
	{
		ItemSize = FIntPoint(ItemSize.Y, ItemSize.X);
	}

	if (ItemSize.X > SlotConstraint->MaxItemSize.X || ItemSize.Y > SlotConstraint->MaxItemSize.Y)
	{
		Result.AddViolation(FName("ItemTooLarge"),
			FText::Format(NSLOCTEXT("SuspenseCore", "ItemTooLargeForSlot",
				"Item ({0}x{1}) is too large for slot {2} (max {3}x{4})"),
				FText::AsNumber(ItemSize.X), FText::AsNumber(ItemSize.Y),
				FText::AsNumber(SlotIndex),
				FText::AsNumber(SlotConstraint->MaxItemSize.X),
				FText::AsNumber(SlotConstraint->MaxItemSize.Y)));
		return false;
	}

	return true;
}

bool USuspenseCoreInventoryConstraints::ValidateWeight(
	USuspenseCoreInventoryComponent* Inventory,
	const FSuspenseCoreItemInstance& Item,
	const FSuspenseCoreItemData& ItemData,
	FSuspenseCoreValidationResult& Result)
{
	if (!Inventory)
	{
		return false;
	}

	float ItemWeight = ItemData.InventoryProps.Weight * Item.Quantity;
	float CurrentWeight = Inventory->Execute_GetCurrentWeight(Inventory);
	float MaxWeight = Inventory->Execute_GetMaxWeight(Inventory);

	if (MaxWeight > 0 && (CurrentWeight + ItemWeight) > MaxWeight)
	{
		float RemainingCapacity = MaxWeight - CurrentWeight;
		int32 MaxQuantity = 0;
		if (ItemData.InventoryProps.Weight > 0)
		{
			MaxQuantity = FMath::FloorToInt(RemainingCapacity / ItemData.InventoryProps.Weight);
		}

		Result.AddViolation(FName("WeightExceeded"),
			FText::Format(NSLOCTEXT("SuspenseCore", "WeightExceeded",
				"Adding {0} would exceed weight limit ({1}/{2})"),
				FText::FromName(Item.ItemID),
				FText::AsNumber(CurrentWeight + ItemWeight),
				FText::AsNumber(MaxWeight)));

		Result.MaxAllowedQuantity = MaxQuantity;
		return false;
	}

	return true;
}

bool USuspenseCoreInventoryConstraints::ValidateQuantityLimits(
	USuspenseCoreInventoryComponent* Inventory,
	const FSuspenseCoreInventoryRules& Rules,
	const FSuspenseCoreItemInstance& Item,
	FSuspenseCoreValidationResult& Result)
{
	if (!Inventory)
	{
		return false;
	}

	// Check max unique items
	if (Rules.MaxUniqueItems > 0)
	{
		TArray<FSuspenseCoreItemInstance> CurrentItems = Inventory->GetAllItemInstances();

		TSet<FName> UniqueItemIDs;
		for (const FSuspenseCoreItemInstance& CurrentItem : CurrentItems)
		{
			UniqueItemIDs.Add(CurrentItem.ItemID);
		}

		// Add new item if it's not already present
		if (!UniqueItemIDs.Contains(Item.ItemID))
		{
			if (UniqueItemIDs.Num() >= Rules.MaxUniqueItems)
			{
				Result.AddViolation(FName("MaxUniqueItemsReached"),
					FText::Format(NSLOCTEXT("SuspenseCore", "MaxUniqueItemsReached",
						"Maximum unique item types ({0}) reached"),
						FText::AsNumber(Rules.MaxUniqueItems)));
				return false;
			}
		}
	}

	// Check max total quantity
	if (Rules.MaxTotalQuantity > 0)
	{
		TArray<FSuspenseCoreItemInstance> CurrentItems = Inventory->GetAllItemInstances();

		int32 TotalQuantity = Item.Quantity;
		for (const FSuspenseCoreItemInstance& CurrentItem : CurrentItems)
		{
			TotalQuantity += CurrentItem.Quantity;
		}

		if (TotalQuantity > Rules.MaxTotalQuantity)
		{
			int32 MaxAddable = Rules.MaxTotalQuantity - (TotalQuantity - Item.Quantity);
			Result.AddViolation(FName("MaxQuantityExceeded"),
				FText::Format(NSLOCTEXT("SuspenseCore", "MaxQuantityExceeded",
					"Total quantity ({0}) would exceed limit ({1})"),
					FText::AsNumber(TotalQuantity),
					FText::AsNumber(Rules.MaxTotalQuantity)));
			Result.MaxAllowedQuantity = FMath::Max(0, MaxAddable);
			return false;
		}
	}

	return true;
}

bool USuspenseCoreInventoryConstraints::GetItemDataForValidation(FName ItemID, FSuspenseCoreItemData& OutData)
{
	// Try to use cached DataManager
	if (DataManagerRef.IsValid())
	{
		return DataManagerRef->GetItemData(ItemID, OutData);
	}

	// Try to find DataManager
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			USuspenseCoreDataManager* DataMgr = GI->GetSubsystem<USuspenseCoreDataManager>();
			if (DataMgr)
			{
				DataManagerRef = DataMgr;
				return DataMgr->GetItemData(ItemID, OutData);
			}
		}
	}

	return false;
}

//==================================================================
// Constraint Presets
//==================================================================

FSuspenseCoreInventoryRules USuspenseCoreConstraintPresets::MakeWeaponOnlyRules()
{
	FSuspenseCoreInventoryRules Rules;
	Rules.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Item.Type.Weapon")));
	Rules.bAllowStacking = false; // Weapons typically don't stack
	Rules.bAllowRotation = true;
	Rules.bAllowDrop = true;
	return Rules;
}

FSuspenseCoreInventoryRules USuspenseCoreConstraintPresets::MakeArmorOnlyRules()
{
	FSuspenseCoreInventoryRules Rules;
	Rules.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Item.Type.Armor")));
	Rules.bAllowStacking = false;
	Rules.bAllowRotation = true;
	Rules.bAllowDrop = true;
	return Rules;
}

FSuspenseCoreInventoryRules USuspenseCoreConstraintPresets::MakeConsumablesOnlyRules()
{
	FSuspenseCoreInventoryRules Rules;
	Rules.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Item.Type.Consumable")));
	Rules.bAllowStacking = true;
	Rules.bAllowRotation = false; // Consumables usually 1x1
	Rules.bAllowDrop = true;
	return Rules;
}

FSuspenseCoreInventoryRules USuspenseCoreConstraintPresets::MakeJunkOnlyRules()
{
	FSuspenseCoreInventoryRules Rules;
	Rules.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Item.Type.Junk")));
	Rules.bAllowStacking = true;
	Rules.bAllowRotation = false;
	Rules.bAllowDrop = true;
	return Rules;
}

FSuspenseCoreInventoryRules USuspenseCoreConstraintPresets::MakeStorageRules(bool bAllowAll)
{
	FSuspenseCoreInventoryRules Rules;
	// No type restrictions for storage by default
	Rules.bAllowStacking = true;
	Rules.bAllowRotation = true;
	Rules.bAllowDrop = false; // Can't drop from storage containers
	return Rules;
}

FSuspenseCoreInventoryRules USuspenseCoreConstraintPresets::MakeQuestItemsRules()
{
	FSuspenseCoreInventoryRules Rules;
	Rules.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Item.Type.Quest")));
	Rules.bAllowStacking = true;
	Rules.bAllowRotation = false;
	Rules.bAllowDrop = false; // Quest items can't be dropped
	return Rules;
}
