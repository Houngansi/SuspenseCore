// SuspenseCoreInventorySerializer.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Serialization/SuspenseCoreInventorySerializer.h"
#include "SuspenseCore/Components/SuspenseCoreInventoryComponent.h"
#include "SuspenseCore/Base/SuspenseCoreInventoryLogs.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "JsonObjectConverter.h"

USuspenseCoreInventorySerializer::USuspenseCoreInventorySerializer()
{
}

bool USuspenseCoreInventorySerializer::SerializeToSaveState(
	USuspenseCoreInventoryComponent* Component,
	FSuspenseCoreInventoryState& OutSaveState)
{
	if (!Component)
	{
		return false;
	}

	OutSaveState.Items.Empty();
	OutSaveState.Currencies.Empty();

	// Get grid size
	FIntPoint GridSize = Component->Execute_GetGridSize(Component);
	OutSaveState.InventorySize = GridSize.X * GridSize.Y;

	// Convert all items
	TArray<FSuspenseCoreItemInstance> Items = Component->GetAllItemInstances();
	for (const FSuspenseCoreItemInstance& Instance : Items)
	{
		FSuspenseCoreRuntimeItem RuntimeItem = InstanceToRuntimeItem(Instance);
		OutSaveState.Items.Add(RuntimeItem);
	}

	FSuspenseCoreInventoryLogHelper::LogSave(TEXT("ToSaveState"), true);

	return true;
}

bool USuspenseCoreInventorySerializer::DeserializeFromSaveState(
	const FSuspenseCoreInventoryState& SaveState,
	USuspenseCoreInventoryComponent* Component)
{
	if (!Component)
	{
		return false;
	}

	// Clear current inventory
	Component->Clear();

	// Restore all items
	for (const FSuspenseCoreRuntimeItem& RuntimeItem : SaveState.Items)
	{
		FSuspenseCoreItemInstance Instance = RuntimeItemToInstance(RuntimeItem);
		Component->AddItemInstanceToSlot(Instance, Instance.SlotIndex);
	}

	FSuspenseCoreInventoryLogHelper::LogLoad(TEXT("FromSaveState"), true, SaveState.Items.Num());

	return true;
}

bool USuspenseCoreInventorySerializer::SerializeInventory(
	USuspenseCoreInventoryComponent* Component,
	FSuspenseCoreSerializedInventory& OutData)
{
	if (!Component)
	{
		return false;
	}

	OutData = FSuspenseCoreSerializedInventory();
	OutData.Version = FSuspenseCoreSerializedInventory::CURRENT_VERSION;
	OutData.SerializationTime = FDateTime::UtcNow();

	// Get config
	FIntPoint GridSize = Component->Execute_GetGridSize(Component);
	OutData.GridWidth = GridSize.X;
	OutData.GridHeight = GridSize.Y;
	OutData.MaxWeight = Component->Execute_GetMaxWeight(Component);
	OutData.CurrentWeight = Component->Execute_GetCurrentWeight(Component);

	// Owner ID
	if (AActor* Owner = Component->GetOwner())
	{
		OutData.OwnerID = Owner->GetName();
	}

	// Serialize items
	TArray<FSuspenseCoreItemInstance> Items = Component->GetAllItemInstances();
	for (const FSuspenseCoreItemInstance& Instance : Items)
	{
		OutData.Items.Add(FSuspenseCoreSerializedItem::FromInstance(Instance));
	}

	// Calculate checksum
	OutData.CalculateChecksum();

	return true;
}

bool USuspenseCoreInventorySerializer::DeserializeInventory(
	const FSuspenseCoreSerializedInventory& Data,
	USuspenseCoreInventoryComponent* Component)
{
	if (!Component || !Data.IsValid())
	{
		return false;
	}

	// Validate checksum
	if (!Data.ValidateChecksum())
	{
		UE_LOG(LogSuspenseCoreInventorySave, Warning, TEXT("Checksum validation failed"));
		// Continue anyway, but log warning
	}

	// Check version
	if (Data.Version != FSuspenseCoreSerializedInventory::CURRENT_VERSION)
	{
		FSuspenseCoreSerializedInventory MigratedData = Data;
		FSuspenseCoreInventoryMigration Migration;
		if (!MigrateToCurrentVersion(MigratedData, Migration))
		{
			UE_LOG(LogSuspenseCoreInventorySave, Error, TEXT("Version migration failed"));
			return false;
		}
		return DeserializeInventory(MigratedData, Component);
	}

	// Initialize inventory with saved config
	Component->Initialize(Data.GridWidth, Data.GridHeight, Data.MaxWeight);

	// Clear and restore items
	Component->Clear();
	for (const FSuspenseCoreSerializedItem& SerializedItem : Data.Items)
	{
		if (SerializedItem.IsValid())
		{
			FSuspenseCoreItemInstance Instance = SerializedItem.ToInstance();
			Component->AddItemInstanceToSlot(Instance, Instance.SlotIndex);
		}
	}

	return true;
}

FSuspenseCoreRuntimeItem USuspenseCoreInventorySerializer::InstanceToRuntimeItem(
	const FSuspenseCoreItemInstance& Instance)
{
	FSuspenseCoreRuntimeItem RuntimeItem;
	RuntimeItem.InstanceId = Instance.UniqueInstanceID.ToString();
	RuntimeItem.DefinitionId = Instance.ItemID.ToString();
	RuntimeItem.Quantity = Instance.Quantity;
	RuntimeItem.SlotIndex = Instance.SlotIndex;
	RuntimeItem.Durability = Instance.GetProperty(FName("Durability"), 1.0f);
	RuntimeItem.UpgradeLevel = static_cast<int32>(Instance.GetProperty(FName("UpgradeLevel"), 0.0f));

	// Custom data as JSON
	if (Instance.RuntimeProperties.Num() > 0)
	{
		TSharedPtr<FJsonObject> JsonObj = MakeShared<FJsonObject>();
		for (const FSuspenseCoreRuntimeProperty& Prop : Instance.RuntimeProperties)
		{
			JsonObj->SetNumberField(Prop.PropertyName.ToString(), Prop.Value);
		}

		// Add weapon state
		if (Instance.WeaponState.bHasState)
		{
			JsonObj->SetNumberField(TEXT("CurrentAmmo"), Instance.WeaponState.CurrentAmmo);
			JsonObj->SetNumberField(TEXT("ReserveAmmo"), Instance.WeaponState.ReserveAmmo);
			JsonObj->SetNumberField(TEXT("FireModeIndex"), Instance.WeaponState.FireModeIndex);
		}

		// Add grid info
		JsonObj->SetNumberField(TEXT("GridX"), Instance.GridPosition.X);
		JsonObj->SetNumberField(TEXT("GridY"), Instance.GridPosition.Y);
		JsonObj->SetNumberField(TEXT("Rotation"), Instance.Rotation);

		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RuntimeItem.CustomData);
		FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);
	}

	return RuntimeItem;
}

FSuspenseCoreItemInstance USuspenseCoreInventorySerializer::RuntimeItemToInstance(
	const FSuspenseCoreRuntimeItem& RuntimeItem)
{
	FSuspenseCoreItemInstance Instance;
	FGuid::Parse(RuntimeItem.InstanceId, Instance.UniqueInstanceID);
	Instance.ItemID = FName(*RuntimeItem.DefinitionId);
	Instance.Quantity = RuntimeItem.Quantity;
	Instance.SlotIndex = RuntimeItem.SlotIndex;

	if (RuntimeItem.Durability < 1.0f)
	{
		Instance.SetProperty(FName("Durability"), RuntimeItem.Durability);
	}

	if (RuntimeItem.UpgradeLevel > 0)
	{
		Instance.SetProperty(FName("UpgradeLevel"), static_cast<float>(RuntimeItem.UpgradeLevel));
	}

	// Parse custom data JSON
	if (!RuntimeItem.CustomData.IsEmpty())
	{
		TSharedPtr<FJsonObject> JsonObj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RuntimeItem.CustomData);
		if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
		{
			// Grid position
			double GridX = 0, GridY = 0, Rotation = 0;
			JsonObj->TryGetNumberField(TEXT("GridX"), GridX);
			JsonObj->TryGetNumberField(TEXT("GridY"), GridY);
			JsonObj->TryGetNumberField(TEXT("Rotation"), Rotation);
			Instance.GridPosition = FIntPoint(static_cast<int32>(GridX), static_cast<int32>(GridY));
			Instance.Rotation = static_cast<int32>(Rotation);

			// Weapon state
			double CurrentAmmo = 0, ReserveAmmo = 0, FireModeIndex = 0;
			if (JsonObj->TryGetNumberField(TEXT("CurrentAmmo"), CurrentAmmo))
			{
				Instance.WeaponState.bHasState = true;
				Instance.WeaponState.CurrentAmmo = static_cast<float>(CurrentAmmo);
				JsonObj->TryGetNumberField(TEXT("ReserveAmmo"), ReserveAmmo);
				Instance.WeaponState.ReserveAmmo = static_cast<float>(ReserveAmmo);
				JsonObj->TryGetNumberField(TEXT("FireModeIndex"), FireModeIndex);
				Instance.WeaponState.FireModeIndex = static_cast<int32>(FireModeIndex);
			}

			// Other properties
			for (const auto& Pair : JsonObj->Values)
			{
				if (Pair.Key != TEXT("GridX") && Pair.Key != TEXT("GridY") &&
					Pair.Key != TEXT("Rotation") && Pair.Key != TEXT("CurrentAmmo") &&
					Pair.Key != TEXT("ReserveAmmo") && Pair.Key != TEXT("FireModeIndex"))
				{
					double Value = 0;
					if (Pair.Value->TryGetNumber(Value))
					{
						Instance.SetProperty(FName(*Pair.Key), static_cast<float>(Value));
					}
				}
			}
		}
	}

	return Instance;
}

FSuspenseCoreSerializedItem USuspenseCoreInventorySerializer::InstanceToSerializedItem(
	const FSuspenseCoreItemInstance& Instance)
{
	return FSuspenseCoreSerializedItem::FromInstance(Instance);
}

FSuspenseCoreItemInstance USuspenseCoreInventorySerializer::SerializedItemToInstance(
	const FSuspenseCoreSerializedItem& SerializedItem)
{
	return SerializedItem.ToInstance();
}

bool USuspenseCoreInventorySerializer::SerializeToJson(
	USuspenseCoreInventoryComponent* Component,
	FString& OutJson)
{
	FSuspenseCoreSerializedInventory Data;
	if (!SerializeInventory(Component, Data))
	{
		return false;
	}

	return FJsonObjectConverter::UStructToJsonObjectString(Data, OutJson);
}

bool USuspenseCoreInventorySerializer::DeserializeFromJson(
	const FString& Json,
	USuspenseCoreInventoryComponent* Component)
{
	FSuspenseCoreSerializedInventory Data;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Data))
	{
		return false;
	}

	return DeserializeInventory(Data, Component);
}

bool USuspenseCoreInventorySerializer::ItemToJson(
	const FSuspenseCoreItemInstance& Instance,
	FString& OutJson)
{
	FSuspenseCoreSerializedItem SerializedItem = FSuspenseCoreSerializedItem::FromInstance(Instance);
	return FJsonObjectConverter::UStructToJsonObjectString(SerializedItem, OutJson);
}

bool USuspenseCoreInventorySerializer::JsonToItem(
	const FString& Json,
	FSuspenseCoreItemInstance& OutInstance)
{
	FSuspenseCoreSerializedItem SerializedItem;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &SerializedItem))
	{
		return false;
	}

	OutInstance = SerializedItem.ToInstance();
	return true;
}

bool USuspenseCoreInventorySerializer::SerializeToBinary(
	USuspenseCoreInventoryComponent* Component,
	TArray<uint8>& OutBytes)
{
	FSuspenseCoreSerializedInventory Data;
	if (!SerializeInventory(Component, Data))
	{
		return false;
	}

	FMemoryWriter Ar(OutBytes);
	Ar << Data.Version;
	Ar << Data.GridWidth;
	Ar << Data.GridHeight;
	Ar << Data.MaxWeight;
	Ar << Data.CurrentWeight;

	int32 ItemCount = Data.Items.Num();
	Ar << ItemCount;

	for (const FSuspenseCoreSerializedItem& Item : Data.Items)
	{
		FString InstanceID = Item.InstanceID;
		FString ItemID = Item.ItemID;
		Ar << InstanceID;
		Ar << ItemID;
		Ar << const_cast<FSuspenseCoreSerializedItem&>(Item).Quantity;
		Ar << const_cast<FSuspenseCoreSerializedItem&>(Item).SlotIndex;
		Ar << const_cast<FSuspenseCoreSerializedItem&>(Item).GridX;
		Ar << const_cast<FSuspenseCoreSerializedItem&>(Item).GridY;
		Ar << const_cast<FSuspenseCoreSerializedItem&>(Item).Rotation;
		Ar << const_cast<FSuspenseCoreSerializedItem&>(Item).Durability;
		Ar << const_cast<FSuspenseCoreSerializedItem&>(Item).CurrentAmmo;
		Ar << const_cast<FSuspenseCoreSerializedItem&>(Item).ReserveAmmo;
	}

	return true;
}

bool USuspenseCoreInventorySerializer::DeserializeFromBinary(
	const TArray<uint8>& Bytes,
	USuspenseCoreInventoryComponent* Component)
{
	if (Bytes.Num() == 0 || !Component)
	{
		return false;
	}

	FMemoryReader Ar(Bytes);

	FSuspenseCoreSerializedInventory Data;
	Ar << Data.Version;
	Ar << Data.GridWidth;
	Ar << Data.GridHeight;
	Ar << Data.MaxWeight;
	Ar << Data.CurrentWeight;

	int32 ItemCount = 0;
	Ar << ItemCount;

	for (int32 i = 0; i < ItemCount; ++i)
	{
		FSuspenseCoreSerializedItem Item;
		Ar << Item.InstanceID;
		Ar << Item.ItemID;
		Ar << Item.Quantity;
		Ar << Item.SlotIndex;
		Ar << Item.GridX;
		Ar << Item.GridY;
		Ar << Item.Rotation;
		Ar << Item.Durability;
		Ar << Item.CurrentAmmo;
		Ar << Item.ReserveAmmo;
		Data.Items.Add(Item);
	}

	return DeserializeInventory(Data, Component);
}

bool USuspenseCoreInventorySerializer::MigrateToCurrentVersion(
	FSuspenseCoreSerializedInventory& Data,
	FSuspenseCoreInventoryMigration& OutMigration)
{
	OutMigration.FromVersion = Data.Version;
	OutMigration.ToVersion = FSuspenseCoreSerializedInventory::CURRENT_VERSION;
	OutMigration.bSuccess = true;

	if (Data.Version == FSuspenseCoreSerializedInventory::CURRENT_VERSION)
	{
		return true;
	}

	// Version migrations would go here
	// Example:
	// if (Data.Version == 0) { MigrateV0ToV1(Data); Data.Version = 1; }
	// if (Data.Version == 1) { MigrateV1ToV2(Data); Data.Version = 2; }

	if (Data.Version < FSuspenseCoreSerializedInventory::CURRENT_VERSION)
	{
		// For now, just update version number
		OutMigration.Warnings.Add(FString::Printf(
			TEXT("Migrated from version %d to %d"),
			Data.Version, FSuspenseCoreSerializedInventory::CURRENT_VERSION));
		Data.Version = FSuspenseCoreSerializedInventory::CURRENT_VERSION;
	}

	return OutMigration.bSuccess;
}

bool USuspenseCoreInventorySerializer::ValidateSerializedData(
	const FSuspenseCoreSerializedInventory& Data,
	TArray<FString>& OutErrors)
{
	OutErrors.Empty();

	if (!Data.IsValid())
	{
		OutErrors.Add(TEXT("Invalid serialized data structure"));
		return false;
	}

	if (Data.Version > FSuspenseCoreSerializedInventory::CURRENT_VERSION)
	{
		OutErrors.Add(FString::Printf(TEXT("Future version %d not supported"), Data.Version));
		return false;
	}

	if (Data.GridWidth <= 0 || Data.GridHeight <= 0)
	{
		OutErrors.Add(TEXT("Invalid grid dimensions"));
	}

	// Validate items
	for (const FSuspenseCoreSerializedItem& Item : Data.Items)
	{
		if (!Item.IsValid())
		{
			OutErrors.Add(FString::Printf(TEXT("Invalid item: %s"), *Item.ItemID));
		}
	}

	if (!Data.ValidateChecksum())
	{
		OutErrors.Add(TEXT("Checksum mismatch - data may be corrupted"));
	}

	return OutErrors.Num() == 0;
}

void USuspenseCoreInventorySerializer::CalculateDiff(
	const FSuspenseCoreSerializedInventory& OldState,
	const FSuspenseCoreSerializedInventory& NewState,
	FSuspenseCoreInventoryDiff& OutDiff)
{
	OutDiff = FSuspenseCoreInventoryDiff();

	// Check config changes
	if (OldState.GridWidth != NewState.GridWidth ||
		OldState.GridHeight != NewState.GridHeight ||
		!FMath::IsNearlyEqual(OldState.MaxWeight, NewState.MaxWeight))
	{
		OutDiff.bConfigChanged = true;
	}

	if (!FMath::IsNearlyEqual(OldState.CurrentWeight, NewState.CurrentWeight))
	{
		OutDiff.bWeightChanged = true;
	}

	// Build lookup maps
	TMap<FString, const FSuspenseCoreSerializedItem*> OldItems;
	for (const FSuspenseCoreSerializedItem& Item : OldState.Items)
	{
		OldItems.Add(Item.InstanceID, &Item);
	}

	TMap<FString, const FSuspenseCoreSerializedItem*> NewItems;
	for (const FSuspenseCoreSerializedItem& Item : NewState.Items)
	{
		NewItems.Add(Item.InstanceID, &Item);
	}

	// Find added and modified
	for (const FSuspenseCoreSerializedItem& NewItem : NewState.Items)
	{
		const FSuspenseCoreSerializedItem** OldItemPtr = OldItems.Find(NewItem.InstanceID);
		if (!OldItemPtr)
		{
			OutDiff.AddedItems.Add(NewItem);
		}
		else
		{
			// Check if modified
			const FSuspenseCoreSerializedItem& OldItem = **OldItemPtr;
			if (NewItem.Quantity != OldItem.Quantity ||
				NewItem.SlotIndex != OldItem.SlotIndex ||
				NewItem.Rotation != OldItem.Rotation ||
				!FMath::IsNearlyEqual(NewItem.Durability, OldItem.Durability))
			{
				OutDiff.ModifiedItems.Add(NewItem);
			}
		}
	}

	// Find removed
	for (const FSuspenseCoreSerializedItem& OldItem : OldState.Items)
	{
		if (!NewItems.Contains(OldItem.InstanceID))
		{
			OutDiff.RemovedItems.Add(OldItem);
		}
	}
}
