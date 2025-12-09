// SuspenseCoreInventoryValidator.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Validation/SuspenseCoreInventoryValidator.h"
#include "SuspenseCore/Components/SuspenseCoreInventoryComponent.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Types/Items/SuspenseCoreItemTypes.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

USuspenseCoreInventoryValidator::USuspenseCoreInventoryValidator()
{
}

bool USuspenseCoreInventoryValidator::ValidateAddItem(
	USuspenseCoreInventoryComponent* Component,
	FName ItemID,
	int32 Quantity,
	FSuspenseCoreInventorySimpleResult& OutResult) const
{
	if (!Component)
	{
		OutResult = FSuspenseCoreInventorySimpleResult::Failure(
			ESuspenseCoreInventoryResult::Unknown, TEXT("Null component"));
		return false;
	}

	if (!Component->IsInitialized())
	{
		OutResult = FSuspenseCoreInventorySimpleResult::Failure(
			ESuspenseCoreInventoryResult::NotInitialized, TEXT("Inventory not initialized"));
		return false;
	}

	if (ItemID.IsNone())
	{
		OutResult = FSuspenseCoreInventorySimpleResult::Failure(
			ESuspenseCoreInventoryResult::InvalidItem, TEXT("Invalid ItemID"));
		return false;
	}

	if (Quantity <= 0)
	{
		OutResult = FSuspenseCoreInventorySimpleResult::Failure(
			ESuspenseCoreInventoryResult::InvalidItem, TEXT("Invalid quantity"));
		return false;
	}

	// Get item data
	FSuspenseCoreItemData ItemData;
	if (!GetItemData(Component, ItemID, ItemData))
	{
		OutResult = FSuspenseCoreInventorySimpleResult::Failure(
			ESuspenseCoreInventoryResult::ItemNotFound,
			FString::Printf(TEXT("Item %s not found in DataTable"), *ItemID.ToString()));
		return false;
	}

	// Check weight
	float ItemWeight = ItemData.InventoryProps.Weight * Quantity;
	float RemainingCapacity;
	if (!CheckWeightConstraint(Component, ItemWeight, RemainingCapacity))
	{
		OutResult = FSuspenseCoreInventorySimpleResult::Failure(
			ESuspenseCoreInventoryResult::WeightLimitExceeded,
			FString::Printf(TEXT("Weight limit exceeded (need %.1f, have %.1f)"), ItemWeight, RemainingCapacity));
		return false;
	}

	// Check type
	if (!CheckTypeConstraint(Component, ItemData.Classification.ItemType))
	{
		OutResult = FSuspenseCoreInventorySimpleResult::Failure(
			ESuspenseCoreInventoryResult::TypeNotAllowed,
			FString::Printf(TEXT("Item type %s not allowed"), *ItemData.Classification.ItemType.ToString()));
		return false;
	}

	// Check space
	int32 AvailableSlot;
	if (!CheckSpaceConstraint(Component, ItemData.InventoryProps.GridSize, INDEX_NONE, true, AvailableSlot))
	{
		// Check if can stack
		bool CanFullyStack;
		int32 Remainder;
		if (!CheckStackConstraint(Component, ItemID, Quantity, CanFullyStack, Remainder) || !CanFullyStack)
		{
			OutResult = FSuspenseCoreInventorySimpleResult::Failure(
				ESuspenseCoreInventoryResult::NoSpace, TEXT("No space available"));
			return false;
		}
	}

	// Custom validation
	if (!ValidateCustomRules(Component, ItemID, OutResult))
	{
		return false;
	}

	OutResult = FSuspenseCoreInventorySimpleResult::Success(AvailableSlot, Quantity);
	return true;
}

bool USuspenseCoreInventoryValidator::ValidateAddItemInstance(
	USuspenseCoreInventoryComponent* Component,
	const FSuspenseCoreItemInstance& ItemInstance,
	int32 TargetSlot,
	FSuspenseCoreInventorySimpleResult& OutResult) const
{
	if (!ItemInstance.IsValid())
	{
		OutResult = FSuspenseCoreInventorySimpleResult::Failure(
			ESuspenseCoreInventoryResult::InvalidItem, TEXT("Invalid item instance"));
		return false;
	}

	return ValidateAddItem(Component, ItemInstance.ItemID, ItemInstance.Quantity, OutResult);
}

bool USuspenseCoreInventoryValidator::ValidateRemoveItem(
	USuspenseCoreInventoryComponent* Component,
	FName ItemID,
	int32 Quantity,
	FSuspenseCoreInventorySimpleResult& OutResult) const
{
	if (!Component || !Component->IsInitialized())
	{
		OutResult = FSuspenseCoreInventorySimpleResult::Failure(
			ESuspenseCoreInventoryResult::NotInitialized, TEXT("Inventory not initialized"));
		return false;
	}

	int32 Available = Component->GetItemCountByID(ItemID);
	if (Available < Quantity)
	{
		OutResult = FSuspenseCoreInventorySimpleResult::Failure(
			ESuspenseCoreInventoryResult::InsufficientQuantity,
			FString::Printf(TEXT("Insufficient quantity (have %d, need %d)"), Available, Quantity));
		return false;
	}

	OutResult = FSuspenseCoreInventorySimpleResult::Success(-1, Quantity);
	return true;
}

bool USuspenseCoreInventoryValidator::ValidateMoveItem(
	USuspenseCoreInventoryComponent* Component,
	int32 FromSlot,
	int32 ToSlot,
	FSuspenseCoreInventorySimpleResult& OutResult) const
{
	if (!Component || !Component->IsInitialized())
	{
		OutResult = FSuspenseCoreInventorySimpleResult::Failure(
			ESuspenseCoreInventoryResult::NotInitialized, TEXT("Inventory not initialized"));
		return false;
	}

	if (FromSlot == ToSlot)
	{
		OutResult = FSuspenseCoreInventorySimpleResult::Success(ToSlot);
		return true;
	}

	FSuspenseCoreItemInstance Instance;
	if (!Component->GetItemInstanceAtSlot(FromSlot, Instance))
	{
		OutResult = FSuspenseCoreInventorySimpleResult::Failure(
			ESuspenseCoreInventoryResult::ItemNotFound, TEXT("No item at source slot"));
		return false;
	}

	FSuspenseCoreItemData ItemData;
	if (!GetItemData(Component, Instance.ItemID, ItemData))
	{
		OutResult = FSuspenseCoreInventorySimpleResult::Failure(
			ESuspenseCoreInventoryResult::ItemNotFound, TEXT("Item data not found"));
		return false;
	}

	// TODO: Check if can place at target (considering source item removal)

	OutResult = FSuspenseCoreInventorySimpleResult::Success(ToSlot);
	return true;
}

bool USuspenseCoreInventoryValidator::ValidateSwapItems(
	USuspenseCoreInventoryComponent* Component,
	int32 Slot1,
	int32 Slot2,
	FSuspenseCoreInventorySimpleResult& OutResult) const
{
	if (!Component || !Component->IsInitialized())
	{
		OutResult = FSuspenseCoreInventorySimpleResult::Failure(
			ESuspenseCoreInventoryResult::NotInitialized, TEXT("Inventory not initialized"));
		return false;
	}

	// Both slots must have items or be swappable
	// For now, simple validation
	OutResult = FSuspenseCoreInventorySimpleResult::Success();
	return true;
}

bool USuspenseCoreInventoryValidator::ValidateSplitStack(
	USuspenseCoreInventoryComponent* Component,
	int32 SourceSlot,
	int32 SplitQuantity,
	int32 TargetSlot,
	FSuspenseCoreInventorySimpleResult& OutResult) const
{
	if (!Component || !Component->IsInitialized())
	{
		OutResult = FSuspenseCoreInventorySimpleResult::Failure(
			ESuspenseCoreInventoryResult::NotInitialized, TEXT("Inventory not initialized"));
		return false;
	}

	FSuspenseCoreItemInstance Instance;
	if (!Component->GetItemInstanceAtSlot(SourceSlot, Instance))
	{
		OutResult = FSuspenseCoreInventorySimpleResult::Failure(
			ESuspenseCoreInventoryResult::ItemNotFound, TEXT("No item at source slot"));
		return false;
	}

	if (SplitQuantity <= 0 || SplitQuantity >= Instance.Quantity)
	{
		OutResult = FSuspenseCoreInventorySimpleResult::Failure(
			ESuspenseCoreInventoryResult::InsufficientQuantity,
			TEXT("Invalid split quantity"));
		return false;
	}

	// Check if target slot is available
	if (TargetSlot != INDEX_NONE && Component->IsSlotOccupied(TargetSlot))
	{
		OutResult = FSuspenseCoreInventorySimpleResult::Failure(
			ESuspenseCoreInventoryResult::SlotOccupied, TEXT("Target slot occupied"));
		return false;
	}

	OutResult = FSuspenseCoreInventorySimpleResult::Success(TargetSlot, SplitQuantity);
	return true;
}

bool USuspenseCoreInventoryValidator::CheckWeightConstraint(
	USuspenseCoreInventoryComponent* Component,
	float AdditionalWeight,
	float& OutRemainingCapacity) const
{
	if (!Component)
	{
		OutRemainingCapacity = 0.0f;
		return false;
	}

	float MaxWeight = Component->GetMaxWeight();
	float CurrentWeight = Component->GetCurrentWeight();
	OutRemainingCapacity = MaxWeight - CurrentWeight;

	return OutRemainingCapacity >= AdditionalWeight;
}

bool USuspenseCoreInventoryValidator::CheckTypeConstraint(
	USuspenseCoreInventoryComponent* Component,
	FGameplayTag ItemType) const
{
	if (!Component)
	{
		return false;
	}

	FGameplayTagContainer AllowedTypes = Component->GetAllowedItemTypes();

	// If no restrictions, all types allowed
	if (AllowedTypes.IsEmpty())
	{
		return true;
	}

	return AllowedTypes.HasTag(ItemType);
}

bool USuspenseCoreInventoryValidator::CheckSpaceConstraint(
	USuspenseCoreInventoryComponent* Component,
	FIntPoint ItemGridSize,
	int32 TargetSlot,
	bool bAllowRotation,
	int32& OutAvailableSlot) const
{
	if (!Component)
	{
		OutAvailableSlot = INDEX_NONE;
		return false;
	}

	if (TargetSlot != INDEX_NONE)
	{
		if (Component->CanPlaceItemAtSlot(ItemGridSize, TargetSlot, false))
		{
			OutAvailableSlot = TargetSlot;
			return true;
		}
		if (bAllowRotation && Component->CanPlaceItemAtSlot(ItemGridSize, TargetSlot, true))
		{
			OutAvailableSlot = TargetSlot;
			return true;
		}
		OutAvailableSlot = INDEX_NONE;
		return false;
	}

	OutAvailableSlot = Component->FindFreeSlot(ItemGridSize, bAllowRotation);
	return OutAvailableSlot != INDEX_NONE;
}

bool USuspenseCoreInventoryValidator::CheckStackConstraint(
	USuspenseCoreInventoryComponent* Component,
	FName ItemID,
	int32 AdditionalQuantity,
	bool& OutCanFullyStack,
	int32& OutRemainder) const
{
	OutCanFullyStack = false;
	OutRemainder = AdditionalQuantity;

	if (!Component)
	{
		return false;
	}

	FSuspenseCoreItemData ItemData;
	if (!GetItemData(Component, ItemID, ItemData))
	{
		return false;
	}

	if (!ItemData.InventoryProps.IsStackable())
	{
		return false;
	}

	TArray<FSuspenseCoreItemInstance> AllInstances = Component->GetAllItemInstances();
	int32 RemainingToStack = AdditionalQuantity;

	for (const FSuspenseCoreItemInstance& Instance : AllInstances)
	{
		if (Instance.ItemID == ItemID)
		{
			int32 SpaceInStack = ItemData.InventoryProps.MaxStackSize - Instance.Quantity;
			if (SpaceInStack > 0)
			{
				int32 ToStack = FMath::Min(SpaceInStack, RemainingToStack);
				RemainingToStack -= ToStack;
				if (RemainingToStack == 0)
				{
					break;
				}
			}
		}
	}

	OutRemainder = RemainingToStack;
	OutCanFullyStack = (RemainingToStack == 0);
	return true;
}

bool USuspenseCoreInventoryValidator::ValidateIntegrity(
	USuspenseCoreInventoryComponent* Component,
	TArray<FString>& OutErrors) const
{
	if (!Component)
	{
		OutErrors.Add(TEXT("Null component"));
		return false;
	}

	return Component->ValidateIntegrity(OutErrors);
}

int32 USuspenseCoreInventoryValidator::RepairIntegrity(
	USuspenseCoreInventoryComponent* Component,
	TArray<FString>& OutRepairLog)
{
	// Placeholder - would implement actual repair logic
	return 0;
}

bool USuspenseCoreInventoryValidator::GetItemData(
	USuspenseCoreInventoryComponent* Component,
	FName ItemID,
	FSuspenseCoreItemData& OutItemData) const
{
	USuspenseCoreDataManager* DataManager = GetDataManager(Component);
	if (!DataManager)
	{
		return false;
	}

	return DataManager->GetItemData(ItemID, OutItemData);
}

USuspenseCoreDataManager* USuspenseCoreInventoryValidator::GetDataManager(USuspenseCoreInventoryComponent* Component) const
{
	if (!Component)
	{
		return nullptr;
	}

	UWorld* World = Component->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		return nullptr;
	}

	return GI->GetSubsystem<USuspenseCoreDataManager>();
}

bool USuspenseCoreInventoryValidator::ValidateCustomRules_Implementation(
	USuspenseCoreInventoryComponent* Component,
	FName ItemID,
	FSuspenseCoreInventorySimpleResult& OutResult) const
{
	// Default: no custom rules, always pass
	return true;
}
