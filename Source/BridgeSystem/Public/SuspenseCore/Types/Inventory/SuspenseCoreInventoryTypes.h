// SuspenseCoreInventoryTypes.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "SuspenseCore/Types/Items/SuspenseCoreItemTypes.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryBaseTypes.h"
#include "SuspenseCoreInventoryTypes.generated.h"

// Forward declarations
class USuspenseCoreInventoryComponent;
struct FSuspenseCoreReplicatedItem;
struct FSuspenseCoreReplicatedInventory;

//==================================================================
// Delta Replication Delegates
// These delegates allow InventorySystem to handle replication callbacks
// without creating circular module dependencies.
//==================================================================

/** Called when a replicated item is about to be removed */
DECLARE_DELEGATE_TwoParams(FOnReplicatedItemPreRemove, const FSuspenseCoreReplicatedItem&, const FSuspenseCoreReplicatedInventory&);
/** Called when a replicated item has been added */
DECLARE_DELEGATE_TwoParams(FOnReplicatedItemPostAdd, const FSuspenseCoreReplicatedItem&, const FSuspenseCoreReplicatedInventory&);
/** Called when a replicated item has changed */
DECLARE_DELEGATE_TwoParams(FOnReplicatedItemPostChange, const FSuspenseCoreReplicatedItem&, const FSuspenseCoreReplicatedInventory&);

/**
 * ESuspenseCoreInventoryResult
 *
 * Result codes for inventory operations.
 */
UENUM(BlueprintType)
enum class ESuspenseCoreInventoryResult : uint8
{
	Success = 0					UMETA(DisplayName = "Success"),
	NoSpace						UMETA(DisplayName = "No Space"),
	WeightLimitExceeded			UMETA(DisplayName = "Weight Limit Exceeded"),
	InvalidItem					UMETA(DisplayName = "Invalid Item"),
	ItemNotFound				UMETA(DisplayName = "Item Not Found"),
	InsufficientQuantity		UMETA(DisplayName = "Insufficient Quantity"),
	InvalidSlot					UMETA(DisplayName = "Invalid Slot"),
	SlotOccupied				UMETA(DisplayName = "Slot Occupied"),
	TypeNotAllowed				UMETA(DisplayName = "Item Type Not Allowed"),
	TransactionActive			UMETA(DisplayName = "Transaction Active"),
	NotInitialized				UMETA(DisplayName = "Not Initialized"),
	NetworkError				UMETA(DisplayName = "Network Error"),
	Unknown						UMETA(DisplayName = "Unknown Error")
};

/**
 * FSuspenseCoreInventorySimpleResult
 *
 * Simple result of an inventory operation for InventorySystem.
 * For full-featured result with AffectedItems, use FSuspenseInventoryOperationResult.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreInventorySimpleResult
{
	GENERATED_BODY()

	/** Operation success status */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bSuccess;

	/** Result code */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	ESuspenseCoreInventoryResult ResultCode;

	/** Affected slot index (-1 if N/A) */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 AffectedSlot;

	/** Affected quantity */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 AffectedQuantity;

	/** Error message (if failed) */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FString ErrorMessage;

	/** Affected instance ID */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FGuid AffectedInstanceID;

	FSuspenseCoreInventorySimpleResult()
		: bSuccess(false)
		, ResultCode(ESuspenseCoreInventoryResult::Unknown)
		, AffectedSlot(-1)
		, AffectedQuantity(0)
	{
	}

	static FSuspenseCoreInventorySimpleResult Success(int32 Slot = -1, int32 Quantity = 0, FGuid InstanceID = FGuid())
	{
		FSuspenseCoreInventorySimpleResult Result;
		Result.bSuccess = true;
		Result.ResultCode = ESuspenseCoreInventoryResult::Success;
		Result.AffectedSlot = Slot;
		Result.AffectedQuantity = Quantity;
		Result.AffectedInstanceID = InstanceID;
		return Result;
	}

	static FSuspenseCoreInventorySimpleResult Failure(ESuspenseCoreInventoryResult Code, const FString& Message = TEXT(""))
	{
		FSuspenseCoreInventorySimpleResult Result;
		Result.bSuccess = false;
		Result.ResultCode = Code;
		Result.ErrorMessage = Message;
		return Result;
	}
};

/**
 * FSuspenseCoreInventoryConfig
 *
 * Configuration for inventory initialization.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreInventoryConfig
{
	GENERATED_BODY()

	/** Grid width */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid",
		meta = (ClampMin = "1", ClampMax = "20"))
	int32 GridWidth;

	/** Grid height */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid",
		meta = (ClampMin = "1", ClampMax = "20"))
	int32 GridHeight;

	/** Maximum weight capacity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weight",
		meta = (ClampMin = "0.0"))
	float MaxWeight;

	/** Allowed item types (empty = all allowed) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Restrictions")
	FGameplayTagContainer AllowedItemTypes;

	/** Disallowed item types (overrides allowed) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Restrictions")
	FGameplayTagContainer DisallowedItemTypes;

	/** Allow auto-stacking on add */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	bool bAutoStack;

	/** Allow rotation when finding free slot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	bool bAllowRotation;

	FSuspenseCoreInventoryConfig()
		: GridWidth(10)
		, GridHeight(6)
		, MaxWeight(50.0f)
		, bAutoStack(true)
		, bAllowRotation(true)
	{
	}

	int32 GetTotalSlots() const { return GridWidth * GridHeight; }
};

/**
 * FSuspenseCoreInventorySlot
 *
 * Single inventory grid slot data.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreInventorySlot
{
	GENERATED_BODY()

	/** Instance ID of item in this slot (invalid = empty) */
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	FGuid InstanceID;

	/** Is this slot an anchor (top-left of multi-cell item) */
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	bool bIsAnchor;

	/** Offset from anchor slot for multi-cell items */
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	FIntPoint OffsetFromAnchor;

	FSuspenseCoreInventorySlot()
		: bIsAnchor(false)
		, OffsetFromAnchor(FIntPoint::ZeroValue)
	{
	}

	bool IsEmpty() const { return !InstanceID.IsValid(); }
	void Clear()
	{
		InstanceID.Invalidate();
		bIsAnchor = false;
		OffsetFromAnchor = FIntPoint::ZeroValue;
	}
};

/**
 * FSuspenseCoreReplicatedItem
 *
 * Network-replicated item data for FFastArraySerializer.
 * Optimized for bandwidth with packed data.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreReplicatedItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

	/** Unique instance ID */
	UPROPERTY()
	FGuid InstanceID;

	/** Item ID (DataTable row name) */
	UPROPERTY()
	FName ItemID;

	/** Stack quantity */
	UPROPERTY()
	int32 Quantity;

	/** Grid slot index */
	UPROPERTY()
	int32 SlotIndex;

	/** Grid position */
	UPROPERTY()
	FIntPoint GridPosition;

	/** Rotation (0, 90, 180, 270) */
	UPROPERTY()
	uint8 Rotation;

	/** Packed flags: bit 0 = has weapon state, bit 1-7 = reserved */
	UPROPERTY()
	uint8 PackedFlags;

	/** Weapon state: current ammo (if applicable) */
	UPROPERTY()
	float CurrentAmmo;

	/** Weapon state: reserve ammo (if applicable) */
	UPROPERTY()
	float ReserveAmmo;

	/** Runtime properties (replicated as array) */
	UPROPERTY()
	TArray<FSuspenseCoreRuntimeProperty> RuntimeProperties;

	FSuspenseCoreReplicatedItem()
		: ItemID(NAME_None)
		, Quantity(1)
		, SlotIndex(-1)
		, GridPosition(FIntPoint::NoneValue)
		, Rotation(0)
		, PackedFlags(0)
		, CurrentAmmo(0.0f)
		, ReserveAmmo(0.0f)
	{
	}

	/** Convert from FSuspenseCoreItemInstance */
	void FromItemInstance(const FSuspenseCoreItemInstance& Instance)
	{
		InstanceID = Instance.UniqueInstanceID;
		ItemID = Instance.ItemID;
		Quantity = Instance.Quantity;
		SlotIndex = Instance.SlotIndex;
		GridPosition = Instance.GridPosition;
		Rotation = static_cast<uint8>(Instance.Rotation);
		RuntimeProperties = Instance.RuntimeProperties;

		PackedFlags = 0;
		if (Instance.WeaponState.bHasState)
		{
			PackedFlags |= 0x01;
			CurrentAmmo = Instance.WeaponState.CurrentAmmo;
			ReserveAmmo = Instance.WeaponState.ReserveAmmo;
		}
	}

	/** Convert to FSuspenseCoreItemInstance */
	FSuspenseCoreItemInstance ToItemInstance() const
	{
		FSuspenseCoreItemInstance Instance;
		Instance.UniqueInstanceID = InstanceID;
		Instance.ItemID = ItemID;
		Instance.Quantity = Quantity;
		Instance.SlotIndex = SlotIndex;
		Instance.GridPosition = GridPosition;
		Instance.Rotation = static_cast<int32>(Rotation);
		Instance.RuntimeProperties = RuntimeProperties;

		if (PackedFlags & 0x01)
		{
			Instance.WeaponState.bHasState = true;
			Instance.WeaponState.CurrentAmmo = CurrentAmmo;
			Instance.WeaponState.ReserveAmmo = ReserveAmmo;
		}

		return Instance;
	}

	/**
	 * PreReplicatedRemove - Called on clients BEFORE item is removed from replicated array.
	 *
	 * DELTA REPLICATION (O(1) per item):
	 * - Removes item from Component->ItemInstances
	 * - Updates Component->GridSlots for this item only
	 * - Updates weight incrementally via UpdateWeightDelta()
	 * - Invalidates UI cache for this item
	 *
	 * @see SuspenseCoreInventoryTypes.cpp for implementation
	 */
	void PreReplicatedRemove(const struct FSuspenseCoreReplicatedInventory& InArraySerializer);

	/**
	 * PostReplicatedAdd - Called on clients AFTER item is added to replicated array.
	 *
	 * DELTA REPLICATION (O(1) per item):
	 * - Converts to FSuspenseCoreItemInstance and adds to Component->ItemInstances
	 * - Updates Component->GridSlots for this item only
	 * - Updates weight incrementally
	 * - Invalidates UI cache
	 *
	 * @see SuspenseCoreInventoryTypes.cpp for implementation
	 */
	void PostReplicatedAdd(const struct FSuspenseCoreReplicatedInventory& InArraySerializer);

	/**
	 * PostReplicatedChange - Called on clients AFTER item properties change.
	 *
	 * DELTA REPLICATION (O(1) per item):
	 * - Finds existing item in Component->ItemInstances
	 * - If position changed, updates GridSlots (remove old, add new)
	 * - If quantity changed, updates weight incrementally
	 * - Invalidates UI cache for this item only
	 *
	 * @see SuspenseCoreInventoryTypes.cpp for implementation
	 */
	void PostReplicatedChange(const struct FSuspenseCoreReplicatedInventory& InArraySerializer);
};

/**
 * FSuspenseCoreReplicatedInventory
 *
 * FFastArraySerializer for inventory replication.
 * Provides efficient delta replication of inventory state.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreReplicatedInventory : public FFastArraySerializer
{
	GENERATED_BODY()

	/** Replicated items */
	UPROPERTY()
	TArray<FSuspenseCoreReplicatedItem> Items;

	/** Grid configuration (replicated once) */
	UPROPERTY()
	int32 GridWidth;

	UPROPERTY()
	int32 GridHeight;

	UPROPERTY()
	float MaxWeight;

	/** Owner component (not replicated, set locally) */
	UPROPERTY(NotReplicated)
	TWeakObjectPtr<UActorComponent> OwnerComponent;

	//==================================================================
	// Delta Replication Delegates
	// Bound by InventoryComponent to handle replication events
	//==================================================================

	/** Delegate called before item removal */
	FOnReplicatedItemPreRemove OnPreRemoveDelegate;

	/** Delegate called after item addition */
	FOnReplicatedItemPostAdd OnPostAddDelegate;

	/** Delegate called after item change */
	FOnReplicatedItemPostChange OnPostChangeDelegate;

	FSuspenseCoreReplicatedInventory()
		: GridWidth(10)
		, GridHeight(6)
		, MaxWeight(50.0f)
	{
	}

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FSuspenseCoreReplicatedItem, FSuspenseCoreReplicatedInventory>(Items, DeltaParams, *this);
	}

	/** Add item to replicated array */
	void AddItem(const FSuspenseCoreItemInstance& Instance)
	{
		FSuspenseCoreReplicatedItem NewItem;
		NewItem.FromItemInstance(Instance);
		Items.Add(NewItem);
		MarkItemDirty(Items.Last());
	}

	/** Remove item from replicated array */
	void RemoveItem(const FGuid& InstanceID)
	{
		int32 Index = Items.IndexOfByPredicate([&InstanceID](const FSuspenseCoreReplicatedItem& Item)
		{
			return Item.InstanceID == InstanceID;
		});

		if (Index != INDEX_NONE)
		{
			Items.RemoveAt(Index);
			MarkArrayDirty();
		}
	}

	/** Update item in replicated array */
	void UpdateItem(const FSuspenseCoreItemInstance& Instance)
	{
		FSuspenseCoreReplicatedItem* Item = Items.FindByPredicate([&Instance](const FSuspenseCoreReplicatedItem& Item)
		{
			return Item.InstanceID == Instance.UniqueInstanceID;
		});

		if (Item)
		{
			Item->FromItemInstance(Instance);
			MarkItemDirty(*Item);
		}
	}

	/** Find item by instance ID */
	FSuspenseCoreReplicatedItem* FindItem(const FGuid& InstanceID)
	{
		return Items.FindByPredicate([&InstanceID](const FSuspenseCoreReplicatedItem& Item)
		{
			return Item.InstanceID == InstanceID;
		});
	}

	/** Clear all items */
	void ClearItems()
	{
		Items.Empty();
		MarkArrayDirty();
	}
};

/** Enable NetDeltaSerialize */
template<>
struct TStructOpsTypeTraits<FSuspenseCoreReplicatedInventory> : public TStructOpsTypeTraitsBase2<FSuspenseCoreReplicatedInventory>
{
	enum
	{
		WithNetDeltaSerializer = true
	};
};

/**
 * FSuspenseCoreInventorySnapshot
 *
 * Complete snapshot of inventory state for transactions.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreInventorySnapshot
{
	GENERATED_BODY()

	/** All item instances */
	UPROPERTY()
	TArray<FSuspenseCoreItemInstance> Items;

	/** Grid slots state */
	UPROPERTY()
	TArray<FSuspenseCoreInventorySlot> Slots;

	/** Current weight at snapshot time */
	UPROPERTY()
	float CurrentWeight;

	/** Timestamp of snapshot */
	UPROPERTY()
	float SnapshotTime;

	FSuspenseCoreInventorySnapshot()
		: CurrentWeight(0.0f)
		, SnapshotTime(0.0f)
	{
	}
};

/**
 * FSuspenseCoreInventoryChange
 *
 * Single inventory change for history tracking.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreInventoryChange
{
	GENERATED_BODY()

	/** Change type */
	UPROPERTY()
	FName ChangeType; // Add, Remove, Move, Rotate, Split, Stack

	/** Affected instance ID */
	UPROPERTY()
	FGuid InstanceID;

	/** Item ID */
	UPROPERTY()
	FName ItemID;

	/** Quantity affected */
	UPROPERTY()
	int32 Quantity;

	/** Previous slot (for moves) */
	UPROPERTY()
	int32 PreviousSlot;

	/** New slot */
	UPROPERTY()
	int32 NewSlot;

	/** Previous rotation (for rotates) */
	UPROPERTY()
	int32 PreviousRotation;

	/** New rotation */
	UPROPERTY()
	int32 NewRotation;

	/** Timestamp */
	UPROPERTY()
	float Timestamp;

	FSuspenseCoreInventoryChange()
		: ChangeType(NAME_None)
		, Quantity(0)
		, PreviousSlot(-1)
		, NewSlot(-1)
		, PreviousRotation(0)
		, NewRotation(0)
		, Timestamp(0.0f)
	{
	}
};

//==============================================================================
// Compatibility Aliases
// These aliases provide compatibility with EquipmentSystem components that
// use slightly different naming conventions
//==============================================================================

// Forward declaration - actual type defined in BridgeSystem's SuspenseInventoryTypes.h
// The alias is declared at the top of this file after includes
// See: Types/Inventory/SuspenseInventoryTypes.h for FSuspenseCoreInventoryItemInstance

