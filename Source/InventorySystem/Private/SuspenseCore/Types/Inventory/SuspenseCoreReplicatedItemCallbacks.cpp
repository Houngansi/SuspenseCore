// SuspenseCoreReplicatedItemCallbacks.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.
//
// This file contains the implementation of FSuspenseCoreReplicatedItem delta replication callbacks.
// The callbacks are defined in BridgeSystem but implemented here in InventorySystem
// because they require access to USuspenseCoreInventoryComponent internals.

#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "SuspenseCore/Components/SuspenseCoreInventoryComponent.h"
#include "SuspenseCore/Base/SuspenseCoreInventoryLogs.h"

//==================================================================
// FSuspenseCoreReplicatedItem - Delta Replication Callbacks
//==================================================================

void FSuspenseCoreReplicatedItem::PreReplicatedRemove(const FSuspenseCoreReplicatedInventory& InArraySerializer)
{
	// Get owner component via OwnerComponent weak pointer
	USuspenseCoreInventoryComponent* OwnerComp = Cast<USuspenseCoreInventoryComponent>(InArraySerializer.OwnerComponent.Get());
	if (!OwnerComp)
	{
		UE_LOG(LogSuspenseCoreInventory, Verbose,
			TEXT("PreReplicatedRemove: OwnerComponent not available for item %s"),
			*InstanceID.ToString());
		return;
	}

	UE_LOG(LogSuspenseCoreInventory, Verbose,
		TEXT("PreReplicatedRemove: Removing item %s from slot %d"),
		*InstanceID.ToString(), SlotIndex);

	// Find and remove this specific item from local arrays
	FSuspenseCoreItemInstance* LocalInstance = OwnerComp->FindItemInstanceInternal(InstanceID);
	if (LocalInstance)
	{
		// Calculate weight to remove BEFORE removing
		float WeightToRemove = 0.0f;
		USuspenseCoreDataManager* DataManager = OwnerComp->GetDataManager();
		if (DataManager)
		{
			FSuspenseCoreItemData ItemData;
			if (DataManager->GetItemData(LocalInstance->ItemID, ItemData))
			{
				WeightToRemove = ItemData.InventoryProps.Weight * LocalInstance->Quantity;
			}
		}

		// Remove from grid slots
		OwnerComp->UpdateGridSlots(*LocalInstance, false);

		// Remove from items array
		int32 Index = OwnerComp->ItemInstances.IndexOfByPredicate(
			[this](const FSuspenseCoreItemInstance& Inst)
			{
				return Inst.UniqueInstanceID == InstanceID;
			});

		if (Index != INDEX_NONE)
		{
			OwnerComp->ItemInstances.RemoveAt(Index);
		}

		// Update weight incrementally
		OwnerComp->UpdateWeightDelta(-WeightToRemove);

		// Invalidate UI cache for this item
		OwnerComp->InvalidateItemUICache(InstanceID);
	}
}

void FSuspenseCoreReplicatedItem::PostReplicatedAdd(const FSuspenseCoreReplicatedInventory& InArraySerializer)
{
	// Get owner component
	USuspenseCoreInventoryComponent* OwnerComp = Cast<USuspenseCoreInventoryComponent>(InArraySerializer.OwnerComponent.Get());
	if (!OwnerComp)
	{
		UE_LOG(LogSuspenseCoreInventory, Verbose,
			TEXT("PostReplicatedAdd: OwnerComponent not available for item %s"),
			*InstanceID.ToString());
		return;
	}

	UE_LOG(LogSuspenseCoreInventory, Verbose,
		TEXT("PostReplicatedAdd: Adding item %s (ID: %s) to slot %d"),
		*ItemID.ToString(), *InstanceID.ToString(), SlotIndex);

	// Convert replicated item to local instance
	FSuspenseCoreItemInstance NewInstance = ToItemInstance();

	// Check if item already exists (shouldn't happen, but safety check)
	FSuspenseCoreItemInstance* Existing = OwnerComp->FindItemInstanceInternal(InstanceID);
	if (Existing)
	{
		UE_LOG(LogSuspenseCoreInventory, Warning,
			TEXT("PostReplicatedAdd: Item %s already exists! Updating instead."),
			*InstanceID.ToString());
		*Existing = NewInstance;
		return;
	}

	// Add to local items array
	OwnerComp->ItemInstances.Add(NewInstance);

	// Update grid slots
	OwnerComp->UpdateGridSlots(NewInstance, true);

	// Calculate and add weight
	float WeightToAdd = 0.0f;
	USuspenseCoreDataManager* DataManager = OwnerComp->GetDataManager();
	if (DataManager)
	{
		FSuspenseCoreItemData ItemData;
		if (DataManager->GetItemData(NewInstance.ItemID, ItemData))
		{
			WeightToAdd = ItemData.InventoryProps.Weight * NewInstance.Quantity;
		}
	}
	OwnerComp->UpdateWeightDelta(WeightToAdd);

	// Invalidate UI cache
	OwnerComp->InvalidateAllUICache();
}

void FSuspenseCoreReplicatedItem::PostReplicatedChange(const FSuspenseCoreReplicatedInventory& InArraySerializer)
{
	// Get owner component
	USuspenseCoreInventoryComponent* OwnerComp = Cast<USuspenseCoreInventoryComponent>(InArraySerializer.OwnerComponent.Get());
	if (!OwnerComp)
	{
		UE_LOG(LogSuspenseCoreInventory, Verbose,
			TEXT("PostReplicatedChange: OwnerComponent not available for item %s"),
			*InstanceID.ToString());
		return;
	}

	UE_LOG(LogSuspenseCoreInventory, Verbose,
		TEXT("PostReplicatedChange: Updating item %s, Qty: %d, Slot: %d"),
		*InstanceID.ToString(), Quantity, SlotIndex);

	// Find existing local instance
	FSuspenseCoreItemInstance* LocalInstance = OwnerComp->FindItemInstanceInternal(InstanceID);
	if (!LocalInstance)
	{
		UE_LOG(LogSuspenseCoreInventory, Warning,
			TEXT("PostReplicatedChange: Item %s not found locally! Adding instead."),
			*InstanceID.ToString());
		PostReplicatedAdd(InArraySerializer);
		return;
	}

	// Get item data for weight calculation
	USuspenseCoreDataManager* DataManager = OwnerComp->GetDataManager();
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
	bool bPositionChanged = (LocalInstance->SlotIndex != SlotIndex) ||
							(LocalInstance->GridPosition != GridPosition) ||
							(LocalInstance->Rotation != static_cast<int32>(Rotation));

	if (bPositionChanged)
	{
		// Remove from old grid position
		OwnerComp->UpdateGridSlots(*LocalInstance, false);
	}

	// Calculate weight delta from quantity change
	int32 OldQuantity = LocalInstance->Quantity;
	int32 QuantityDelta = Quantity - OldQuantity;

	// Update local instance fields
	LocalInstance->Quantity = Quantity;
	LocalInstance->SlotIndex = SlotIndex;
	LocalInstance->GridPosition = GridPosition;
	LocalInstance->Rotation = static_cast<int32>(Rotation);
	LocalInstance->RuntimeProperties = RuntimeProperties;

	// Update weapon state if present
	if (PackedFlags & 0x01)
	{
		LocalInstance->WeaponState.bHasState = true;
		LocalInstance->WeaponState.CurrentAmmo = CurrentAmmo;
		LocalInstance->WeaponState.ReserveAmmo = ReserveAmmo;
	}

	if (bPositionChanged)
	{
		// Add to new grid position
		OwnerComp->UpdateGridSlots(*LocalInstance, true);
	}

	// Update weight if quantity changed
	if (QuantityDelta != 0)
	{
		OwnerComp->UpdateWeightDelta(UnitWeight * QuantityDelta);
	}

	// Invalidate UI cache for this item
	OwnerComp->InvalidateItemUICache(InstanceID);
}
