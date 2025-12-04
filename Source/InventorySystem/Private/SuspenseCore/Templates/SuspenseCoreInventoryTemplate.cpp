// SuspenseCoreInventoryTemplate.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Templates/SuspenseCoreInventoryTemplate.h"
#include "SuspenseCore/Components/SuspenseCoreInventoryComponent.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Base/SuspenseCoreInventoryLogs.h"
#include "Engine/DataTable.h"

USuspenseCoreInventoryTemplateManager::USuspenseCoreInventoryTemplateManager()
{
}

void USuspenseCoreInventoryTemplateManager::Initialize(UDataTable* TemplateTable, UDataTable* LoadoutTable)
{
	TemplateTableRef = TemplateTable;
	LoadoutTableRef = LoadoutTable;

	LoadTemplates();
	LoadLoadouts();

	UE_LOG(LogSuspenseCoreInventory, Log,
		TEXT("TemplateManager initialized: %d templates, %d loadouts"),
		CachedTemplates.Num(), CachedLoadouts.Num());
}

void USuspenseCoreInventoryTemplateManager::SetDataManager(USuspenseCoreDataManager* DataManager)
{
	DataManagerRef = DataManager;
}

bool USuspenseCoreInventoryTemplateManager::ApplyTemplate(
	USuspenseCoreInventoryComponent* Inventory,
	FName TemplateID,
	bool bClearFirst)
{
	FSuspenseCoreInventoryTemplate Template;
	if (!GetTemplate(TemplateID, Template))
	{
		UE_LOG(LogSuspenseCoreInventory, Warning,
			TEXT("Template not found: %s"), *TemplateID.ToString());
		return false;
	}

	return ApplyTemplateStruct(Inventory, Template, bClearFirst);
}

bool USuspenseCoreInventoryTemplateManager::ApplyTemplateStruct(
	USuspenseCoreInventoryComponent* Inventory,
	const FSuspenseCoreInventoryTemplate& Template,
	bool bClearFirst)
{
	if (!Inventory)
	{
		return false;
	}

	// Apply grid size override if specified
	if (Template.HasGridOverride())
	{
		Inventory->Initialize(
			Template.GridSizeOverride.X,
			Template.GridSizeOverride.Y,
			Template.HasWeightOverride() ? Template.MaxWeightOverride : Inventory->Execute_GetMaxWeight(Inventory)
		);
	}
	else if (bClearFirst)
	{
		Inventory->Clear();
	}

	int32 AddedCount = 0;

	// Add items based on template type
	if (Template.TemplateType == ESuspenseCoreTemplateType::LootTable)
	{
		TArray<FSuspenseCoreItemInstance> LootItems;
		AddedCount = RollLootItems(Template, LootItems);

		for (const FSuspenseCoreItemInstance& Item : LootItems)
		{
			Inventory->Execute_AddItemInstance(Inventory, Item);
		}
	}
	else
	{
		// Standard template - add all items
		for (const FSuspenseCoreTemplateItem& TemplateItem : Template.Items)
		{
			if (!TemplateItem.IsValid())
			{
				continue;
			}

			FSuspenseCoreItemInstance Instance;
			if (CreateItemFromTemplate(TemplateItem, Instance))
			{
				if (TemplateItem.PreferredSlot >= 0)
				{
					Inventory->AddItemInstanceToSlot(Instance, TemplateItem.PreferredSlot);
				}
				else
				{
					Inventory->Execute_AddItemInstance(Inventory, Instance);
				}
				++AddedCount;
			}
		}
	}

	UE_LOG(LogSuspenseCoreInventory, Log,
		TEXT("Applied template '%s': %d items added"),
		*Template.TemplateID.ToString(), AddedCount);

	return AddedCount > 0;
}

int32 USuspenseCoreInventoryTemplateManager::GenerateLoot(
	USuspenseCoreInventoryComponent* Inventory,
	FName LootTemplateID)
{
	FSuspenseCoreInventoryTemplate Template;
	if (!GetTemplate(LootTemplateID, Template))
	{
		return 0;
	}

	if (Template.TemplateType != ESuspenseCoreTemplateType::LootTable)
	{
		UE_LOG(LogSuspenseCoreInventory, Warning,
			TEXT("Template '%s' is not a loot table"), *LootTemplateID.ToString());
		return 0;
	}

	TArray<FSuspenseCoreItemInstance> LootItems;
	int32 Generated = RollLootItems(Template, LootItems);

	for (const FSuspenseCoreItemInstance& Item : LootItems)
	{
		Inventory->Execute_AddItemInstance(Inventory, Item);
	}

	return Generated;
}

bool USuspenseCoreInventoryTemplateManager::ApplyLoadout(
	USuspenseCoreInventoryComponent* Inventory,
	FName LoadoutID)
{
	FSuspenseCoreTemplateLoadout Loadout;
	if (!GetLoadout(LoadoutID, Loadout))
	{
		return false;
	}

	// Apply inventory template if specified
	if (!Loadout.InventoryTemplateID.IsNone())
	{
		ApplyTemplate(Inventory, Loadout.InventoryTemplateID, true);
	}

	// Equipment slots would be applied by Equipment system
	// This manager handles inventory only

	return true;
}

bool USuspenseCoreInventoryTemplateManager::GetDefaultLoadout(
	FGameplayTag CharacterClass,
	FSuspenseCoreTemplateLoadout& OutLoadout) const
{
	for (const auto& Pair : CachedLoadouts)
	{
		if (Pair.Value.CharacterClass == CharacterClass && Pair.Value.bIsDefault)
		{
			OutLoadout = Pair.Value;
			return true;
		}
	}
	return false;
}

TArray<FSuspenseCoreTemplateLoadout> USuspenseCoreInventoryTemplateManager::GetLoadoutsForClass(
	FGameplayTag CharacterClass) const
{
	TArray<FSuspenseCoreTemplateLoadout> Result;
	for (const auto& Pair : CachedLoadouts)
	{
		if (Pair.Value.CharacterClass == CharacterClass)
		{
			Result.Add(Pair.Value);
		}
	}
	return Result;
}

bool USuspenseCoreInventoryTemplateManager::GetTemplate(
	FName TemplateID,
	FSuspenseCoreInventoryTemplate& OutTemplate) const
{
	const FSuspenseCoreInventoryTemplate* Found = CachedTemplates.Find(TemplateID);
	if (Found)
	{
		OutTemplate = *Found;
		return true;
	}
	return false;
}

bool USuspenseCoreInventoryTemplateManager::GetLoadout(
	FName LoadoutID,
	FSuspenseCoreTemplateLoadout& OutLoadout) const
{
	const FSuspenseCoreTemplateLoadout* Found = CachedLoadouts.Find(LoadoutID);
	if (Found)
	{
		OutLoadout = *Found;
		return true;
	}
	return false;
}

TArray<FSuspenseCoreInventoryTemplate> USuspenseCoreInventoryTemplateManager::GetTemplatesByType(
	ESuspenseCoreTemplateType Type) const
{
	TArray<FSuspenseCoreInventoryTemplate> Result;
	for (const auto& Pair : CachedTemplates)
	{
		if (Pair.Value.TemplateType == Type)
		{
			Result.Add(Pair.Value);
		}
	}
	return Result;
}

TArray<FSuspenseCoreInventoryTemplate> USuspenseCoreInventoryTemplateManager::GetTemplatesByTag(
	FGameplayTag Tag) const
{
	TArray<FSuspenseCoreInventoryTemplate> Result;
	for (const auto& Pair : CachedTemplates)
	{
		if (Pair.Value.TemplateTags.HasTag(Tag))
		{
			Result.Add(Pair.Value);
		}
	}
	return Result;
}

TArray<FName> USuspenseCoreInventoryTemplateManager::GetAllTemplateIDs() const
{
	TArray<FName> Result;
	CachedTemplates.GetKeys(Result);
	return Result;
}

bool USuspenseCoreInventoryTemplateManager::HasTemplate(FName TemplateID) const
{
	return CachedTemplates.Contains(TemplateID);
}

bool USuspenseCoreInventoryTemplateManager::CreateItemFromTemplate(
	const FSuspenseCoreTemplateItem& TemplateItem,
	FSuspenseCoreItemInstance& OutInstance)
{
	if (!TemplateItem.IsValid())
	{
		return false;
	}

	int32 Quantity = TemplateItem.GetRandomQuantity();
	OutInstance = FSuspenseCoreItemInstance(TemplateItem.ItemID, Quantity);

	// Apply initial durability if specified
	if (TemplateItem.InitialDurability > 0.0f)
	{
		OutInstance.SetProperty(FName("Durability"), TemplateItem.InitialDurability * 100.0f);
	}

	// Get item data for weapon initialization
	if (DataManagerRef.IsValid())
	{
		FSuspenseCoreItemData ItemData;
		if (DataManagerRef->GetItemData(TemplateItem.ItemID, ItemData))
		{
			if (ItemData.bIsWeapon && TemplateItem.bFullAmmo)
			{
				OutInstance.WeaponState.bHasState = true;
				OutInstance.WeaponState.CurrentAmmo = ItemData.WeaponConfig.MagazineSize;
				OutInstance.WeaponState.ReserveAmmo = ItemData.WeaponConfig.MagazineSize * 2; // 2 extra mags
			}
		}
	}

	return true;
}

int32 USuspenseCoreInventoryTemplateManager::RollLootItems(
	const FSuspenseCoreInventoryTemplate& Template,
	TArray<FSuspenseCoreItemInstance>& OutItems)
{
	OutItems.Empty();

	if (Template.Items.Num() == 0)
	{
		return 0;
	}

	int32 TargetCount = Template.GetRandomLootCount();
	int32 Generated = 0;

	// Build weighted pool of items that pass spawn chance
	TArray<const FSuspenseCoreTemplateItem*> EligibleItems;
	for (const FSuspenseCoreTemplateItem& Item : Template.Items)
	{
		if (Item.IsValid() && Item.ShouldSpawn())
		{
			EligibleItems.Add(&Item);
		}
	}

	// Shuffle and take up to target count
	for (int32 i = EligibleItems.Num() - 1; i > 0; --i)
	{
		int32 j = FMath::RandRange(0, i);
		EligibleItems.Swap(i, j);
	}

	int32 ItemsToGenerate = FMath::Min(TargetCount, EligibleItems.Num());
	for (int32 i = 0; i < ItemsToGenerate; ++i)
	{
		FSuspenseCoreItemInstance Instance;
		if (CreateItemFromTemplate(*EligibleItems[i], Instance))
		{
			OutItems.Add(Instance);
			++Generated;
		}
	}

	return Generated;
}

void USuspenseCoreInventoryTemplateManager::LoadTemplates()
{
	CachedTemplates.Empty();

	if (!TemplateTableRef.IsValid())
	{
		return;
	}

	TArray<FSuspenseCoreInventoryTemplate*> Rows;
	TemplateTableRef->GetAllRows<FSuspenseCoreInventoryTemplate>(TEXT("LoadTemplates"), Rows);

	for (FSuspenseCoreInventoryTemplate* Row : Rows)
	{
		if (Row && Row->IsValid())
		{
			CachedTemplates.Add(Row->TemplateID, *Row);
		}
	}
}

void USuspenseCoreInventoryTemplateManager::LoadLoadouts()
{
	CachedLoadouts.Empty();

	if (!LoadoutTableRef.IsValid())
	{
		return;
	}

	TArray<FSuspenseCoreTemplateLoadout*> Rows;
	LoadoutTableRef->GetAllRows<FSuspenseCoreTemplateLoadout>(TEXT("LoadLoadouts"), Rows);

	for (FSuspenseCoreTemplateLoadout* Row : Rows)
	{
		if (Row && Row->IsValid())
		{
			CachedLoadouts.Add(Row->LoadoutID, *Row);
		}
	}
}
