// SuspenseCoreUITypes.h
// SuspenseCore - UI Data Types for EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreUITypes.generated.h"

/**
 * ESuspenseCoreUISlotState
 * Visual state of a UI slot
 */
UENUM(BlueprintType)
enum class ESuspenseCoreUISlotState : uint8
{
	Empty = 0			UMETA(DisplayName = "Empty"),
	Occupied			UMETA(DisplayName = "Occupied"),
	Locked				UMETA(DisplayName = "Locked"),
	Highlighted			UMETA(DisplayName = "Highlighted"),
	Selected			UMETA(DisplayName = "Selected"),
	Invalid				UMETA(DisplayName = "Invalid"),
	DropTarget			UMETA(DisplayName = "Drop Target"),
	DropTargetValid		UMETA(DisplayName = "Drop Target Valid"),
	DropTargetInvalid	UMETA(DisplayName = "Drop Target Invalid")
};

/**
 * ESuspenseCoreUIFeedbackType
 * Types of UI feedback notifications
 */
UENUM(BlueprintType)
enum class ESuspenseCoreUIFeedbackType : uint8
{
	None = 0			UMETA(DisplayName = "None"),
	Success				UMETA(DisplayName = "Success"),
	Error				UMETA(DisplayName = "Error"),
	Warning				UMETA(DisplayName = "Warning"),
	Info				UMETA(DisplayName = "Info"),
	ItemPickedUp		UMETA(DisplayName = "Item Picked Up"),
	ItemDropped			UMETA(DisplayName = "Item Dropped"),
	InventoryFull		UMETA(DisplayName = "Inventory Full"),
	WeightExceeded		UMETA(DisplayName = "Weight Exceeded"),
	InvalidOperation	UMETA(DisplayName = "Invalid Operation")
};

/**
 * FSuspenseCoreItemUIData
 * UI-friendly item data for display purposes only.
 * Converted from FSuspenseCoreItemInstance by provider.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreItemUIData
{
	GENERATED_BODY()

	//==================================================================
	// Identity
	//==================================================================

	/** Unique runtime instance ID */
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FGuid InstanceID;

	/** Item definition ID (DataTable row name) */
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FName ItemID;

	//==================================================================
	// Display
	//==================================================================

	/** Localized display name */
	UPROPERTY(BlueprintReadOnly, Category = "Display")
	FText DisplayName;

	/** Localized description */
	UPROPERTY(BlueprintReadOnly, Category = "Display")
	FText Description;

	/** Icon asset path (for safe copying) */
	UPROPERTY(BlueprintReadOnly, Category = "Display")
	FSoftObjectPath IconPath;

	/** Item type tag (Item.Weapon.Rifle, etc.) */
	UPROPERTY(BlueprintReadOnly, Category = "Display")
	FGameplayTag ItemType;

	/** Rarity tag (Item.Rarity.Rare, etc.) */
	UPROPERTY(BlueprintReadOnly, Category = "Display")
	FGameplayTag RarityTag;

	//==================================================================
	// Grid Properties
	//==================================================================

	/** Size in grid cells */
	UPROPERTY(BlueprintReadOnly, Category = "Grid")
	FIntPoint GridSize;

	/** Anchor slot index in container */
	UPROPERTY(BlueprintReadOnly, Category = "Grid")
	int32 AnchorSlot;

	/** Is item rotated 90 degrees */
	UPROPERTY(BlueprintReadOnly, Category = "Grid")
	bool bIsRotated;

	//==================================================================
	// Stack Properties
	//==================================================================

	/** Current quantity in stack */
	UPROPERTY(BlueprintReadOnly, Category = "Stack")
	int32 Quantity;

	/** Maximum stack size */
	UPROPERTY(BlueprintReadOnly, Category = "Stack")
	int32 MaxStackSize;

	/** Is this item stackable */
	UPROPERTY(BlueprintReadOnly, Category = "Stack")
	bool bIsStackable;

	//==================================================================
	// Weight
	//==================================================================

	/** Weight per unit */
	UPROPERTY(BlueprintReadOnly, Category = "Weight")
	float UnitWeight;

	/** Total weight (UnitWeight * Quantity) */
	UPROPERTY(BlueprintReadOnly, Category = "Weight")
	float TotalWeight;

	//==================================================================
	// Capabilities
	//==================================================================

	/** Can be equipped */
	UPROPERTY(BlueprintReadOnly, Category = "Capabilities")
	bool bIsEquippable;

	/** Can be used/consumed */
	UPROPERTY(BlueprintReadOnly, Category = "Capabilities")
	bool bIsUsable;

	/** Can be dropped to world */
	UPROPERTY(BlueprintReadOnly, Category = "Capabilities")
	bool bIsDroppable;

	/** Can be traded */
	UPROPERTY(BlueprintReadOnly, Category = "Capabilities")
	bool bIsTradeable;

	/** Equipment slot type if equippable */
	UPROPERTY(BlueprintReadOnly, Category = "Capabilities")
	FGameplayTag EquipmentSlotType;

	//==================================================================
	// Weapon-Specific (optional)
	//==================================================================

	/** Has ammo display */
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	bool bHasAmmo;

	/** Current ammo in magazine */
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	int32 CurrentAmmo;

	/** Magazine capacity */
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	int32 MagazineSize;

	/** Reserve ammo */
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	int32 ReserveAmmo;

	//==================================================================
	// Durability (optional)
	//==================================================================

	/** Has durability */
	UPROPERTY(BlueprintReadOnly, Category = "Durability")
	bool bHasDurability;

	/** Current durability (0-1) */
	UPROPERTY(BlueprintReadOnly, Category = "Durability")
	float DurabilityPercent;

	//==================================================================
	// Constructor
	//==================================================================

	FSuspenseCoreItemUIData()
		: InstanceID()
		, ItemID(NAME_None)
		, DisplayName(FText::GetEmpty())
		, Description(FText::GetEmpty())
		, GridSize(1, 1)
		, AnchorSlot(INDEX_NONE)
		, bIsRotated(false)
		, Quantity(1)
		, MaxStackSize(1)
		, bIsStackable(false)
		, UnitWeight(0.0f)
		, TotalWeight(0.0f)
		, bIsEquippable(false)
		, bIsUsable(false)
		, bIsDroppable(true)
		, bIsTradeable(true)
		, bHasAmmo(false)
		, CurrentAmmo(0)
		, MagazineSize(0)
		, ReserveAmmo(0)
		, bHasDurability(false)
		, DurabilityPercent(1.0f)
	{
	}

	/** Check if data is valid */
	bool IsValid() const
	{
		return InstanceID.IsValid() && !ItemID.IsNone() && GridSize.X > 0 && GridSize.Y > 0;
	}

	/** Get effective size considering rotation */
	FIntPoint GetEffectiveSize() const
	{
		return bIsRotated ? FIntPoint(GridSize.Y, GridSize.X) : GridSize;
	}

	/** Create safe copy for drag operations */
	FSuspenseCoreItemUIData CreateDragCopy() const
	{
		FSuspenseCoreItemUIData Copy = *this;
		// Ensure FText members are properly copied
		Copy.DisplayName = FText::FromString(DisplayName.ToString());
		Copy.Description = FText::FromString(Description.ToString());
		return Copy;
	}
};

/**
 * FSuspenseCoreSlotUIData
 * UI-friendly slot data for display purposes
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreSlotUIData
{
	GENERATED_BODY()

	/** Slot index in container */
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	int32 SlotIndex;

	/** Grid coordinates */
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	FIntPoint GridPosition;

	/** Current visual state */
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	ESuspenseCoreUISlotState State;

	/** Is this slot an anchor for an item */
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	bool bIsAnchor;

	/** Is this slot part of a multi-cell item */
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	bool bIsPartOfItem;

	/** Instance ID of item in this slot (if occupied) */
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	FGuid OccupyingItemID;

	/** Slot type tag (for equipment slots) */
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	FGameplayTag SlotTypeTag;

	/** Allowed item types for this slot (empty = all) */
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	FGameplayTagContainer AllowedItemTypes;

	FSuspenseCoreSlotUIData()
		: SlotIndex(INDEX_NONE)
		, GridPosition(0, 0)
		, State(ESuspenseCoreUISlotState::Empty)
		, bIsAnchor(false)
		, bIsPartOfItem(false)
		, OccupyingItemID()
	{
	}

	bool IsOccupied() const
	{
		return State == ESuspenseCoreUISlotState::Occupied || bIsPartOfItem;
	}
};

/**
 * FSuspenseCoreUINotification
 * Notification data for UI feedback
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreUINotification
{
	GENERATED_BODY()

	/** Notification type */
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
	ESuspenseCoreUIFeedbackType Type;

	/** Message to display */
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
	FText Message;

	/** Optional icon path */
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
	FSoftObjectPath IconPath;

	/** Duration in seconds (0 = use default) */
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
	float Duration;

	/** Optional item data for item-related notifications */
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
	FSuspenseCoreItemUIData RelatedItem;

	/** Optional quantity for stack operations */
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
	int32 Quantity;

	FSuspenseCoreUINotification()
		: Type(ESuspenseCoreUIFeedbackType::None)
		, Message(FText::GetEmpty())
		, Duration(3.0f)
		, Quantity(0)
	{
	}

	static FSuspenseCoreUINotification CreateSuccess(const FText& InMessage, float InDuration = 3.0f)
	{
		FSuspenseCoreUINotification Notification;
		Notification.Type = ESuspenseCoreUIFeedbackType::Success;
		Notification.Message = InMessage;
		Notification.Duration = InDuration;
		return Notification;
	}

	static FSuspenseCoreUINotification CreateError(const FText& InMessage, float InDuration = 3.0f)
	{
		FSuspenseCoreUINotification Notification;
		Notification.Type = ESuspenseCoreUIFeedbackType::Error;
		Notification.Message = InMessage;
		Notification.Duration = InDuration;
		return Notification;
	}

	static FSuspenseCoreUINotification CreateItemPickup(const FSuspenseCoreItemUIData& Item, int32 InQuantity)
	{
		FSuspenseCoreUINotification Notification;
		Notification.Type = ESuspenseCoreUIFeedbackType::ItemPickedUp;
		Notification.Message = FText::Format(
			NSLOCTEXT("SuspenseCore", "ItemPickedUp", "Picked up {0} x{1}"),
			Item.DisplayName,
			FText::AsNumber(InQuantity)
		);
		Notification.RelatedItem = Item;
		Notification.Quantity = InQuantity;
		Notification.IconPath = Item.IconPath;
		return Notification;
	}
};

/**
 * FSuspenseCoreDropValidation
 * Result of drop validation check
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreDropValidation
{
	GENERATED_BODY()

	/** Is drop allowed */
	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	bool bIsValid;

	/** Reason if not valid */
	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	FText Reason;

	/** Suggested alternative slot */
	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	int32 AlternativeSlot;

	/** Would this be a swap operation */
	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	bool bWouldSwap;

	/** Would this be a stack merge */
	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	bool bWouldStack;

	/** Quantity that would be transferred in stack */
	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	int32 StackTransferAmount;

	FSuspenseCoreDropValidation()
		: bIsValid(false)
		, Reason(FText::GetEmpty())
		, AlternativeSlot(INDEX_NONE)
		, bWouldSwap(false)
		, bWouldStack(false)
		, StackTransferAmount(0)
	{
	}

	static FSuspenseCoreDropValidation Valid()
	{
		FSuspenseCoreDropValidation Result;
		Result.bIsValid = true;
		return Result;
	}

	static FSuspenseCoreDropValidation Invalid(const FText& InReason)
	{
		FSuspenseCoreDropValidation Result;
		Result.bIsValid = false;
		Result.Reason = InReason;
		return Result;
	}
};

