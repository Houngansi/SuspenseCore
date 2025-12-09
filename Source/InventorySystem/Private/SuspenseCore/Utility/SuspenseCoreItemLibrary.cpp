// SuspenseCoreItemLibrary.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Utility/SuspenseCoreItemLibrary.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

bool USuspenseCoreItemLibrary::CreateItemInstance(
	UObject* WorldContextObject,
	FName ItemID,
	int32 Quantity,
	FSuspenseCoreItemInstance& OutInstance)
{
	if (!WorldContextObject || ItemID.IsNone() || Quantity <= 0)
	{
		return false;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return false;
	}

	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		return false;
	}

	USuspenseCoreDataManager* DataMgr = GI->GetSubsystem<USuspenseCoreDataManager>();
	if (!DataMgr)
	{
		OutInstance = FSuspenseCoreItemInstance(ItemID, Quantity);
		return true;
	}

	FSuspenseCoreItemData ItemData;
	if (!DataMgr->GetItemData(ItemID, ItemData))
	{
		OutInstance = FSuspenseCoreItemInstance(ItemID, Quantity);
		return true;
	}

	OutInstance = FSuspenseCoreItemInstance(ItemID, Quantity);

	// Initialize weapon state if applicable
	if (ItemData.bIsWeapon)
	{
		OutInstance.WeaponState.bHasState = true;
		OutInstance.WeaponState.CurrentAmmo = ItemData.WeaponConfig.MagazineSize;
		OutInstance.WeaponState.ReserveAmmo = 0;
	}

	// Initialize durability if applicable
	if (ItemData.bIsWeapon || ItemData.bIsArmor)
	{
		OutInstance.SetProperty(FName("Durability"), 100.0f);
	}

	return true;
}

FSuspenseCoreItemInstance USuspenseCoreItemLibrary::MakeItemInstance(
	FName ItemID,
	int32 Quantity,
	const TMap<FName, float>& Properties)
{
	FSuspenseCoreItemInstance Instance(ItemID, Quantity);
	Instance.SetPropertiesFromMap(Properties);
	return Instance;
}

FSuspenseCoreItemInstance USuspenseCoreItemLibrary::CloneItemInstance(
	const FSuspenseCoreItemInstance& Source,
	bool bNewInstanceID)
{
	FSuspenseCoreItemInstance Clone = Source;
	if (bNewInstanceID)
	{
		Clone.UniqueInstanceID = FGuid::NewGuid();
	}
	return Clone;
}

bool USuspenseCoreItemLibrary::GetItemData(
	UObject* WorldContextObject,
	FName ItemID,
	FSuspenseCoreItemData& OutData)
{
	if (!WorldContextObject)
	{
		return false;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return false;
	}

	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		return false;
	}

	USuspenseCoreDataManager* DataMgr = GI->GetSubsystem<USuspenseCoreDataManager>();
	if (!DataMgr)
	{
		return false;
	}

	return DataMgr->GetItemData(ItemID, OutData);
}

bool USuspenseCoreItemLibrary::ItemExists(UObject* WorldContextObject, FName ItemID)
{
	FSuspenseCoreItemData Data;
	return GetItemData(WorldContextObject, ItemID, Data);
}

int32 USuspenseCoreItemLibrary::GetItemsOfType(
	UObject* WorldContextObject,
	FGameplayTag ItemType,
	TArray<FName>& OutItemIDs)
{
	OutItemIDs.Empty();
	// Implementation would query DataManager for all items with matching type
	return OutItemIDs.Num();
}

bool USuspenseCoreItemLibrary::IsInstanceValid(const FSuspenseCoreItemInstance& Instance)
{
	return Instance.IsValid();
}

bool USuspenseCoreItemLibrary::AreSameItemType(
	const FSuspenseCoreItemInstance& A,
	const FSuspenseCoreItemInstance& B)
{
	return A.ItemID == B.ItemID;
}

bool USuspenseCoreItemLibrary::CanInstancesStack(
	const FSuspenseCoreItemInstance& A,
	const FSuspenseCoreItemInstance& B)
{
	return A.CanStackWith(B);
}

float USuspenseCoreItemLibrary::GetInstanceProperty(
	const FSuspenseCoreItemInstance& Instance,
	FName PropertyName,
	float DefaultValue)
{
	return Instance.GetProperty(PropertyName, DefaultValue);
}

void USuspenseCoreItemLibrary::SetInstanceProperty(
	FSuspenseCoreItemInstance& Instance,
	FName PropertyName,
	float Value)
{
	Instance.SetProperty(PropertyName, Value);
}

TMap<FName, float> USuspenseCoreItemLibrary::GetAllProperties(const FSuspenseCoreItemInstance& Instance)
{
	return Instance.GetPropertiesAsMap();
}

bool USuspenseCoreItemLibrary::IsWeaponInstance(const FSuspenseCoreItemInstance& Instance)
{
	return Instance.WeaponState.bHasState;
}

bool USuspenseCoreItemLibrary::GetWeaponAmmo(
	const FSuspenseCoreItemInstance& Instance,
	int32& OutCurrentAmmo,
	int32& OutReserveAmmo)
{
	if (!Instance.WeaponState.bHasState)
	{
		OutCurrentAmmo = 0;
		OutReserveAmmo = 0;
		return false;
	}

	OutCurrentAmmo = FMath::RoundToInt(Instance.WeaponState.CurrentAmmo);
	OutReserveAmmo = FMath::RoundToInt(Instance.WeaponState.ReserveAmmo);
	return true;
}

void USuspenseCoreItemLibrary::SetWeaponAmmo(
	FSuspenseCoreItemInstance& Instance,
	int32 CurrentAmmo,
	int32 ReserveAmmo)
{
	Instance.WeaponState.bHasState = true;
	Instance.WeaponState.CurrentAmmo = static_cast<float>(CurrentAmmo);
	Instance.WeaponState.ReserveAmmo = static_cast<float>(ReserveAmmo);
}

int32 USuspenseCoreItemLibrary::ReloadWeapon(
	FSuspenseCoreItemInstance& Instance,
	int32 MagazineSize)
{
	if (!Instance.WeaponState.bHasState)
	{
		return 0;
	}

	int32 CurrentAmmo = FMath::RoundToInt(Instance.WeaponState.CurrentAmmo);
	int32 ReserveAmmo = FMath::RoundToInt(Instance.WeaponState.ReserveAmmo);

	int32 AmmoNeeded = MagazineSize - CurrentAmmo;
	int32 AmmoToLoad = FMath::Min(AmmoNeeded, ReserveAmmo);

	Instance.WeaponState.CurrentAmmo = static_cast<float>(CurrentAmmo + AmmoToLoad);
	Instance.WeaponState.ReserveAmmo = static_cast<float>(ReserveAmmo - AmmoToLoad);

	return AmmoToLoad;
}

float USuspenseCoreItemLibrary::GetDurability(const FSuspenseCoreItemInstance& Instance)
{
	return Instance.GetProperty(FName("Durability"), 100.0f);
}

void USuspenseCoreItemLibrary::SetDurability(
	FSuspenseCoreItemInstance& Instance,
	float Durability)
{
	Instance.SetProperty(FName("Durability"), FMath::Clamp(Durability, 0.0f, 100.0f));
}

bool USuspenseCoreItemLibrary::ApplyDurabilityDamage(
	FSuspenseCoreItemInstance& Instance,
	float Damage)
{
	float CurrentDurability = GetDurability(Instance);
	float NewDurability = FMath::Max(0.0f, CurrentDurability - Damage);
	SetDurability(Instance, NewDurability);
	return NewDurability <= 0.0f;
}

float USuspenseCoreItemLibrary::RepairItem(
	FSuspenseCoreItemInstance& Instance,
	float Amount)
{
	float CurrentDurability = GetDurability(Instance);
	float NewDurability = FMath::Min(100.0f, CurrentDurability + Amount);
	SetDurability(Instance, NewDurability);
	return NewDurability;
}

bool USuspenseCoreItemLibrary::IsBroken(const FSuspenseCoreItemInstance& Instance)
{
	return GetDurability(Instance) <= 0.0f;
}

FIntPoint USuspenseCoreItemLibrary::GetItemGridSize(UObject* WorldContextObject, FName ItemID)
{
	FSuspenseCoreItemData Data;
	if (GetItemData(WorldContextObject, ItemID, Data))
	{
		return Data.InventoryProps.GridSize;
	}
	return FIntPoint(1, 1);
}

FIntPoint USuspenseCoreItemLibrary::GetRotatedGridSize(
	UObject* WorldContextObject,
	const FSuspenseCoreItemInstance& Instance)
{
	FIntPoint Size = GetItemGridSize(WorldContextObject, Instance.ItemID);

	// 90 or 270 degrees swaps width and height
	int32 NormalizedRotation = ((Instance.Rotation % 360) + 360) % 360;
	if (NormalizedRotation == 90 || NormalizedRotation == 270)
	{
		return FIntPoint(Size.Y, Size.X);
	}

	return Size;
}

void USuspenseCoreItemLibrary::RotateInstance(FSuspenseCoreItemInstance& Instance)
{
	Instance.Rotation = (Instance.Rotation + 90) % 360;
}

int32 USuspenseCoreItemLibrary::CalculateItemValue(
	UObject* WorldContextObject,
	const FSuspenseCoreItemInstance& Instance,
	bool bIncludeDurability)
{
	FSuspenseCoreItemData Data;
	if (!GetItemData(WorldContextObject, Instance.ItemID, Data))
	{
		return 0;
	}

	int32 BaseValue = Data.InventoryProps.BaseValue;

	if (bIncludeDurability)
	{
		float Durability = GetDurability(Instance) / 100.0f;
		BaseValue = FMath::RoundToInt(BaseValue * Durability);
	}

	return BaseValue;
}

int32 USuspenseCoreItemLibrary::CalculateStackValue(
	UObject* WorldContextObject,
	const FSuspenseCoreItemInstance& Instance)
{
	return CalculateItemValue(WorldContextObject, Instance, true) * Instance.Quantity;
}

FString USuspenseCoreItemLibrary::GetInstanceDebugString(const FSuspenseCoreItemInstance& Instance)
{
	FString Result = FString::Printf(TEXT("[%s] %s x%d"),
		*Instance.UniqueInstanceID.ToString().Left(8),
		*Instance.ItemID.ToString(),
		Instance.Quantity);

	if (Instance.SlotIndex >= 0)
	{
		Result += FString::Printf(TEXT(" @ Slot %d"), Instance.SlotIndex);
	}

	if (Instance.Rotation != 0)
	{
		Result += FString::Printf(TEXT(" Rot: %d"), Instance.Rotation);
	}

	if (Instance.WeaponState.bHasState)
	{
		Result += FString::Printf(TEXT(" Ammo: %d/%d"),
			FMath::RoundToInt(Instance.WeaponState.CurrentAmmo),
			FMath::RoundToInt(Instance.WeaponState.ReserveAmmo));
	}

	float Durability = Instance.GetProperty(FName("Durability"), -1.0f);
	if (Durability >= 0.0f)
	{
		Result += FString::Printf(TEXT(" Dur: %.0f%%"), Durability);
	}

	return Result;
}

FString USuspenseCoreItemLibrary::GetItemDataDebugString(const FSuspenseCoreItemData& ItemData)
{
	FString Result = FString::Printf(TEXT("%s - %s"),
		*ItemData.Identity.ItemID.ToString(),
		*ItemData.Identity.DisplayName.ToString());

	Result += FString::Printf(TEXT("\n  Grid: %dx%d, Weight: %.2f, Stack: %d"),
		ItemData.InventoryProps.GridSize.X,
		ItemData.InventoryProps.GridSize.Y,
		ItemData.InventoryProps.Weight,
		ItemData.InventoryProps.MaxStackSize);

	if (ItemData.bIsWeapon)
	{
		Result += FString::Printf(TEXT("\n  Weapon: Mag %d, DMG %.1f"),
			ItemData.WeaponConfig.MagazineSize,
			ItemData.WeaponConfig.BaseDamage);
	}

	if (ItemData.bIsArmor)
	{
		Result += FString::Printf(TEXT("\n  Armor: Class %d, Dur %.0f"),
			ItemData.ArmorConfig.ArmorClass,
			ItemData.ArmorConfig.MaxDurability);
	}

	return Result;
}
