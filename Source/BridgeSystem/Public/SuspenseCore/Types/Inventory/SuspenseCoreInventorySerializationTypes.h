// SuspenseCoreInventorySerializationTypes.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCoreInventoryTypes.h"
#include "SuspenseCore/Types/Items/SuspenseCoreItemTypes.h"
#include "SuspenseCoreInventorySerializationTypes.generated.h"

/**
 * ESuspenseCoreSerializationFormat
 *
 * Serialization format options.
 */
UENUM(BlueprintType)
enum class ESuspenseCoreSerializationFormat : uint8
{
	Binary = 0			UMETA(DisplayName = "Binary"),
	Json				UMETA(DisplayName = "JSON"),
	CompressedBinary	UMETA(DisplayName = "Compressed Binary")
};

/**
 * FSuspenseCoreSerializedItem
 *
 * Serialized item data for save/load.
 * Optimized for storage and network transfer.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreSerializedItem
{
	GENERATED_BODY()

	/** Unique instance ID (as string for JSON) */
	UPROPERTY(BlueprintReadWrite, Category = "Serialized")
	FString InstanceID;

	/** Item definition ID */
	UPROPERTY(BlueprintReadWrite, Category = "Serialized")
	FString ItemID;

	/** Stack quantity */
	UPROPERTY(BlueprintReadWrite, Category = "Serialized")
	int32 Quantity;

	/** Slot index */
	UPROPERTY(BlueprintReadWrite, Category = "Serialized")
	int32 SlotIndex;

	/** Grid position X */
	UPROPERTY(BlueprintReadWrite, Category = "Serialized")
	int32 GridX;

	/** Grid position Y */
	UPROPERTY(BlueprintReadWrite, Category = "Serialized")
	int32 GridY;

	/** Rotation (0-3 for 0/90/180/270) */
	UPROPERTY(BlueprintReadWrite, Category = "Serialized")
	uint8 Rotation;

	/** Durability (0-100%) */
	UPROPERTY(BlueprintReadWrite, Category = "Serialized")
	float Durability;

	/** Current ammo (for weapons) */
	UPROPERTY(BlueprintReadWrite, Category = "Serialized")
	int32 CurrentAmmo;

	/** Reserve ammo (for weapons) */
	UPROPERTY(BlueprintReadWrite, Category = "Serialized")
	int32 ReserveAmmo;

	/** Custom properties as JSON string */
	UPROPERTY(BlueprintReadWrite, Category = "Serialized")
	FString CustomPropertiesJson;

	FSuspenseCoreSerializedItem()
		: Quantity(1)
		, SlotIndex(-1)
		, GridX(-1)
		, GridY(-1)
		, Rotation(0)
		, Durability(100.0f)
		, CurrentAmmo(0)
		, ReserveAmmo(0)
	{
	}

	/** Convert from FSuspenseCoreItemInstance */
	static FSuspenseCoreSerializedItem FromInstance(const FSuspenseCoreItemInstance& Instance)
	{
		FSuspenseCoreSerializedItem Serialized;
		Serialized.InstanceID = Instance.UniqueInstanceID.ToString();
		Serialized.ItemID = Instance.ItemID.ToString();
		Serialized.Quantity = Instance.Quantity;
		Serialized.SlotIndex = Instance.SlotIndex;
		Serialized.GridX = Instance.GridPosition.X;
		Serialized.GridY = Instance.GridPosition.Y;
		Serialized.Rotation = static_cast<uint8>(Instance.Rotation / 90);

		// Extract durability if present
		Serialized.Durability = Instance.GetProperty(FName("Durability"), 100.0f);

		// Weapon state
		if (Instance.WeaponState.bHasState)
		{
			Serialized.CurrentAmmo = FMath::RoundToInt(Instance.WeaponState.CurrentAmmo);
			Serialized.ReserveAmmo = FMath::RoundToInt(Instance.WeaponState.ReserveAmmo);
		}

		// Serialize custom properties to JSON
		if (Instance.RuntimeProperties.Num() > 0)
		{
			TSharedPtr<FJsonObject> JsonObj = MakeShared<FJsonObject>();
			for (const FSuspenseCoreRuntimeProperty& Prop : Instance.RuntimeProperties)
			{
				JsonObj->SetNumberField(Prop.PropertyName.ToString(), Prop.Value);
			}
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized.CustomPropertiesJson);
			FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);
		}

		return Serialized;
	}

	/** Convert to FSuspenseCoreItemInstance */
	FSuspenseCoreItemInstance ToInstance() const
	{
		FSuspenseCoreItemInstance Instance;
		FGuid::Parse(InstanceID, Instance.UniqueInstanceID);
		Instance.ItemID = FName(*ItemID);
		Instance.Quantity = Quantity;
		Instance.SlotIndex = SlotIndex;
		Instance.GridPosition = FIntPoint(GridX, GridY);
		Instance.Rotation = static_cast<int32>(Rotation) * 90;

		// Restore durability
		if (Durability < 100.0f)
		{
			Instance.SetProperty(FName("Durability"), Durability);
		}

		// Restore weapon state
		if (CurrentAmmo > 0 || ReserveAmmo > 0)
		{
			Instance.WeaponState.bHasState = true;
			Instance.WeaponState.CurrentAmmo = static_cast<float>(CurrentAmmo);
			Instance.WeaponState.ReserveAmmo = static_cast<float>(ReserveAmmo);
		}

		// Parse custom properties from JSON
		if (!CustomPropertiesJson.IsEmpty())
		{
			TSharedPtr<FJsonObject> JsonObj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(CustomPropertiesJson);
			if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
			{
				for (const auto& Pair : JsonObj->Values)
				{
					double Value = 0.0;
					if (Pair.Value->TryGetNumber(Value))
					{
						Instance.SetProperty(FName(*Pair.Key), static_cast<float>(Value));
					}
				}
			}
		}

		return Instance;
	}

	bool IsValid() const
	{
		return !ItemID.IsEmpty() && Quantity > 0;
	}
};

/**
 * FSuspenseCoreSerializedInventory
 *
 * Complete serialized inventory state.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreSerializedInventory
{
	GENERATED_BODY()

	/** Version for migration support */
	UPROPERTY(BlueprintReadWrite, Category = "Serialized")
	int32 Version;

	/** Owner identifier */
	UPROPERTY(BlueprintReadWrite, Category = "Serialized")
	FString OwnerID;

	/** Grid width */
	UPROPERTY(BlueprintReadWrite, Category = "Serialized")
	int32 GridWidth;

	/** Grid height */
	UPROPERTY(BlueprintReadWrite, Category = "Serialized")
	int32 GridHeight;

	/** Maximum weight */
	UPROPERTY(BlueprintReadWrite, Category = "Serialized")
	float MaxWeight;

	/** Current weight at save time */
	UPROPERTY(BlueprintReadWrite, Category = "Serialized")
	float CurrentWeight;

	/** Serialized items */
	UPROPERTY(BlueprintReadWrite, Category = "Serialized")
	TArray<FSuspenseCoreSerializedItem> Items;

	/** Timestamp when serialized */
	UPROPERTY(BlueprintReadWrite, Category = "Serialized")
	FDateTime SerializationTime;

	/** Checksum for integrity */
	UPROPERTY(BlueprintReadWrite, Category = "Serialized")
	FString Checksum;

	static constexpr int32 CURRENT_VERSION = 1;

	FSuspenseCoreSerializedInventory()
		: Version(CURRENT_VERSION)
		, GridWidth(10)
		, GridHeight(6)
		, MaxWeight(50.0f)
		, CurrentWeight(0.0f)
	{
	}

	/** Calculate checksum from items */
	void CalculateChecksum()
	{
		FString Data;
		Data.Append(FString::Printf(TEXT("%d%d%d%.2f"), Version, GridWidth, GridHeight, MaxWeight));
		for (const FSuspenseCoreSerializedItem& Item : Items)
		{
			Data.Append(Item.InstanceID);
			Data.Append(Item.ItemID);
			Data.Append(FString::FromInt(Item.Quantity));
		}

		// Simple hash
		uint32 Hash = GetTypeHash(Data);
		Checksum = FString::Printf(TEXT("%08X"), Hash);
	}

	/** Validate checksum */
	bool ValidateChecksum() const
	{
		FSuspenseCoreSerializedInventory Copy = *this;
		Copy.CalculateChecksum();
		return Copy.Checksum == Checksum;
	}

	bool IsValid() const
	{
		return Version > 0 && GridWidth > 0 && GridHeight > 0;
	}

	int32 GetItemCount() const
	{
		return Items.Num();
	}

	int32 GetTotalQuantity() const
	{
		int32 Total = 0;
		for (const FSuspenseCoreSerializedItem& Item : Items)
		{
			Total += Item.Quantity;
		}
		return Total;
	}
};

/**
 * FSuspenseCoreInventoryMigration
 *
 * Migration info for version upgrades.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreInventoryMigration
{
	GENERATED_BODY()

	/** From version */
	UPROPERTY(BlueprintReadOnly, Category = "Migration")
	int32 FromVersion;

	/** To version */
	UPROPERTY(BlueprintReadOnly, Category = "Migration")
	int32 ToVersion;

	/** Items that failed to migrate */
	UPROPERTY(BlueprintReadOnly, Category = "Migration")
	TArray<FSuspenseCoreSerializedItem> FailedItems;

	/** Migration warnings */
	UPROPERTY(BlueprintReadOnly, Category = "Migration")
	TArray<FString> Warnings;

	/** Migration was successful */
	UPROPERTY(BlueprintReadOnly, Category = "Migration")
	bool bSuccess;

	FSuspenseCoreInventoryMigration()
		: FromVersion(0)
		, ToVersion(0)
		, bSuccess(false)
	{
	}
};

/**
 * FSuspenseCoreInventoryDiff
 *
 * Difference between two inventory states.
 * Used for delta sync and debugging.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreInventoryDiff
{
	GENERATED_BODY()

	/** Items added since last state */
	UPROPERTY(BlueprintReadOnly, Category = "Diff")
	TArray<FSuspenseCoreSerializedItem> AddedItems;

	/** Items removed since last state */
	UPROPERTY(BlueprintReadOnly, Category = "Diff")
	TArray<FSuspenseCoreSerializedItem> RemovedItems;

	/** Items modified since last state */
	UPROPERTY(BlueprintReadOnly, Category = "Diff")
	TArray<FSuspenseCoreSerializedItem> ModifiedItems;

	/** Config changed */
	UPROPERTY(BlueprintReadOnly, Category = "Diff")
	bool bConfigChanged;

	/** Weight changed */
	UPROPERTY(BlueprintReadOnly, Category = "Diff")
	bool bWeightChanged;

	FSuspenseCoreInventoryDiff()
		: bConfigChanged(false)
		, bWeightChanged(false)
	{
	}

	bool HasChanges() const
	{
		return AddedItems.Num() > 0 || RemovedItems.Num() > 0 ||
			   ModifiedItems.Num() > 0 || bConfigChanged || bWeightChanged;
	}

	int32 GetChangeCount() const
	{
		return AddedItems.Num() + RemovedItems.Num() + ModifiedItems.Num();
	}
};

/**
 * FSuspenseCoreInventoryExport
 *
 * Inventory export data for external tools.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreInventoryExport
{
	GENERATED_BODY()

	/** Export format */
	UPROPERTY(BlueprintReadWrite, Category = "Export")
	ESuspenseCoreSerializationFormat Format;

	/** Serialized data */
	UPROPERTY(BlueprintReadOnly, Category = "Export")
	FSuspenseCoreSerializedInventory Data;

	/** Raw bytes (for binary format) */
	UPROPERTY()
	TArray<uint8> RawBytes;

	/** JSON string (for JSON format) */
	UPROPERTY(BlueprintReadOnly, Category = "Export")
	FString JsonString;

	/** Export timestamp */
	UPROPERTY(BlueprintReadOnly, Category = "Export")
	FDateTime ExportTime;

	FSuspenseCoreInventoryExport()
		: Format(ESuspenseCoreSerializationFormat::Json)
	{
	}
};

