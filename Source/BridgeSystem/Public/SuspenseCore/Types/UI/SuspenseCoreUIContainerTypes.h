// SuspenseCoreUIContainerTypes.h
// SuspenseCore - Container Types for UI Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreUITypes.h"
#include "SuspenseCoreUIContainerTypes.generated.h"

/**
 * ESuspenseCoreContainerType
 * Types of containers that can display items
 */
UENUM(BlueprintType)
enum class ESuspenseCoreContainerType : uint8
{
	None = 0			UMETA(DisplayName = "None"),
	Inventory			UMETA(DisplayName = "Inventory"),
	Equipment			UMETA(DisplayName = "Equipment"),
	Stash				UMETA(DisplayName = "Stash"),
	Trader				UMETA(DisplayName = "Trader"),
	Loot				UMETA(DisplayName = "Loot Container"),
	Ground				UMETA(DisplayName = "Ground"),
	Crafting			UMETA(DisplayName = "Crafting"),
	Storage				UMETA(DisplayName = "Storage")
};

/**
 * ESuspenseCoreSlotLayoutType
 * Layout type for container slots
 */
UENUM(BlueprintType)
enum class ESuspenseCoreSlotLayoutType : uint8
{
	Grid = 0			UMETA(DisplayName = "Grid"),
	Named				UMETA(DisplayName = "Named Slots"),
	List				UMETA(DisplayName = "List"),
	Freeform			UMETA(DisplayName = "Freeform")
};

/**
 * FSuspenseCoreContainerUIData
 * Complete UI data for a container
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreContainerUIData
{
	GENERATED_BODY()

	//==================================================================
	// Identity
	//==================================================================

	/** Unique container ID */
	UPROPERTY(BlueprintReadOnly, Category = "Container")
	FGuid ContainerID;

	/** Container type */
	UPROPERTY(BlueprintReadOnly, Category = "Container")
	ESuspenseCoreContainerType ContainerType;

	/** Container type as gameplay tag */
	UPROPERTY(BlueprintReadOnly, Category = "Container")
	FGameplayTag ContainerTypeTag;

	/** Display name */
	UPROPERTY(BlueprintReadOnly, Category = "Container")
	FText DisplayName;

	//==================================================================
	// Layout
	//==================================================================

	/** Slot layout type */
	UPROPERTY(BlueprintReadOnly, Category = "Layout")
	ESuspenseCoreSlotLayoutType LayoutType;

	/** Grid dimensions (for Grid layout) */
	UPROPERTY(BlueprintReadOnly, Category = "Layout")
	FIntPoint GridSize;

	/** Total slot count */
	UPROPERTY(BlueprintReadOnly, Category = "Layout")
	int32 TotalSlots;

	/** Occupied slot count */
	UPROPERTY(BlueprintReadOnly, Category = "Layout")
	int32 OccupiedSlots;

	//==================================================================
	// Weight System
	//==================================================================

	/** Has weight limit */
	UPROPERTY(BlueprintReadOnly, Category = "Weight")
	bool bHasWeightLimit;

	/** Current weight */
	UPROPERTY(BlueprintReadOnly, Category = "Weight")
	float CurrentWeight;

	/** Maximum weight */
	UPROPERTY(BlueprintReadOnly, Category = "Weight")
	float MaxWeight;

	/** Weight as percentage (0-1) */
	UPROPERTY(BlueprintReadOnly, Category = "Weight")
	float WeightPercent;

	//==================================================================
	// Restrictions
	//==================================================================

	/** Allowed item types (empty = all) */
	UPROPERTY(BlueprintReadOnly, Category = "Restrictions")
	FGameplayTagContainer AllowedItemTypes;

	/** Is container locked */
	UPROPERTY(BlueprintReadOnly, Category = "Restrictions")
	bool bIsLocked;

	/** Is container read-only (no modifications) */
	UPROPERTY(BlueprintReadOnly, Category = "Restrictions")
	bool bIsReadOnly;

	//==================================================================
	// Slots Data
	//==================================================================

	/** All slot data */
	UPROPERTY(BlueprintReadOnly, Category = "Slots")
	TArray<FSuspenseCoreSlotUIData> Slots;

	/** All item data */
	UPROPERTY(BlueprintReadOnly, Category = "Items")
	TArray<FSuspenseCoreItemUIData> Items;

	//==================================================================
	// Constructor
	//==================================================================

	FSuspenseCoreContainerUIData()
		: ContainerID()
		, ContainerType(ESuspenseCoreContainerType::None)
		, DisplayName(FText::GetEmpty())
		, LayoutType(ESuspenseCoreSlotLayoutType::Grid)
		, GridSize(10, 5)
		, TotalSlots(50)
		, OccupiedSlots(0)
		, bHasWeightLimit(false)
		, CurrentWeight(0.0f)
		, MaxWeight(0.0f)
		, WeightPercent(0.0f)
		, bIsLocked(false)
		, bIsReadOnly(false)
	{
	}

	/** Get remaining capacity */
	int32 GetFreeSlots() const
	{
		return TotalSlots - OccupiedSlots;
	}

	/** Get remaining weight capacity */
	float GetRemainingWeight() const
	{
		return bHasWeightLimit ? FMath::Max(0.0f, MaxWeight - CurrentWeight) : TNumericLimits<float>::Max();
	}

	/** Check if container is full */
	bool IsFull() const
	{
		return OccupiedSlots >= TotalSlots;
	}

	/** Check if weight limit exceeded */
	bool IsOverweight() const
	{
		return bHasWeightLimit && CurrentWeight > MaxWeight;
	}
};

/**
 * FSuspenseCoreDragData
 * Data for drag-drop operations between containers
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreDragData
{
	GENERATED_BODY()

	//==================================================================
	// Source Info
	//==================================================================

	/** Item being dragged */
	UPROPERTY(BlueprintReadOnly, Category = "Source")
	FSuspenseCoreItemUIData Item;

	/** Source container type */
	UPROPERTY(BlueprintReadOnly, Category = "Source")
	ESuspenseCoreContainerType SourceContainerType;

	/** Source container tag */
	UPROPERTY(BlueprintReadOnly, Category = "Source")
	FGameplayTag SourceContainerTag;

	/** Source container ID */
	UPROPERTY(BlueprintReadOnly, Category = "Source")
	FGuid SourceContainerID;

	/** Source slot index */
	UPROPERTY(BlueprintReadOnly, Category = "Source")
	int32 SourceSlot;

	//==================================================================
	// Drag State
	//==================================================================

	/** Quantity being dragged (for split stack) */
	UPROPERTY(BlueprintReadOnly, Category = "Drag")
	int32 DragQuantity;

	/** Is this a split stack drag */
	UPROPERTY(BlueprintReadOnly, Category = "Drag")
	bool bIsSplitStack;

	/** Original drag offset from cursor */
	UPROPERTY(BlueprintReadOnly, Category = "Drag")
	FVector2D DragOffset;

	/** Is item rotated during drag */
	UPROPERTY(BlueprintReadOnly, Category = "Drag")
	bool bIsRotatedDuringDrag;

	//==================================================================
	// Target Info (filled during drag)
	//==================================================================

	/** Current target container type */
	UPROPERTY(BlueprintReadWrite, Category = "Target")
	ESuspenseCoreContainerType TargetContainerType;

	/** Current target container tag */
	UPROPERTY(BlueprintReadWrite, Category = "Target")
	FGameplayTag TargetContainerTag;

	/** Current target slot */
	UPROPERTY(BlueprintReadWrite, Category = "Target")
	int32 TargetSlot;

	//==================================================================
	// Validation
	//==================================================================

	/** Is drag data valid */
	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	bool bIsValid;

	/** Unique drag operation ID */
	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	FGuid DragOperationID;

	FSuspenseCoreDragData()
		: SourceContainerType(ESuspenseCoreContainerType::None)
		, SourceSlot(INDEX_NONE)
		, DragQuantity(0)
		, bIsSplitStack(false)
		, DragOffset(FVector2D::ZeroVector)
		, bIsRotatedDuringDrag(false)
		, TargetContainerType(ESuspenseCoreContainerType::None)
		, TargetSlot(INDEX_NONE)
		, bIsValid(false)
	{
	}

	/** Get item instance ID */
	FGuid GetItemInstanceID() const { return Item.ItemInstanceID; }

	/** Convenience accessors */
	const FGuid& ItemInstanceID() const { return Item.ItemInstanceID; }
	int32 Quantity() const { return DragQuantity; }
	bool bIsRotated() const { return bIsRotatedDuringDrag; }

	/** Create validated drag data */
	static FSuspenseCoreDragData Create(
		const FSuspenseCoreItemUIData& InItem,
		ESuspenseCoreContainerType InSourceType,
		FGameplayTag InSourceTag,
		FGuid InSourceContainerID,
		int32 InSourceSlot)
	{
		FSuspenseCoreDragData Data;

		if (!InItem.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("FSuspenseCoreDragData::Create - Invalid item data"));
			return Data;
		}

		Data.Item = InItem.CreateDragCopy();
		Data.SourceContainerType = InSourceType;
		Data.SourceContainerTag = InSourceTag;
		Data.SourceContainerID = InSourceContainerID;
		Data.SourceSlot = InSourceSlot;
		Data.DragQuantity = InItem.Quantity;
		Data.DragOperationID = FGuid::NewGuid();
		Data.bIsValid = true;

		return Data;
	}

	/** Create split stack drag */
	static FSuspenseCoreDragData CreateSplit(
		const FSuspenseCoreItemUIData& InItem,
		ESuspenseCoreContainerType InSourceType,
		FGameplayTag InSourceTag,
		FGuid InSourceContainerID,
		int32 InSourceSlot,
		int32 InSplitQuantity)
	{
		FSuspenseCoreDragData Data = Create(InItem, InSourceType, InSourceTag, InSourceContainerID, InSourceSlot);
		Data.bIsSplitStack = true;
		Data.DragQuantity = FMath::Clamp(InSplitQuantity, 1, InItem.Quantity);
		return Data;
	}

	/** Toggle rotation during drag */
	void ToggleRotation()
	{
		bIsRotatedDuringDrag = !bIsRotatedDuringDrag;
		Item.bIsRotated = !Item.bIsRotated;
	}

	/** Get effective item size during drag */
	FIntPoint GetEffectiveDragSize() const
	{
		return Item.GetEffectiveSize();
	}
};

/**
 * FSuspenseCorePanelConfig
 * Configuration for a container panel in layout
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCorePanelConfig
{
	GENERATED_BODY()

	/** Panel identifier tag */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panel")
	FGameplayTag PanelTag;

	/** Display name for tab/button */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panel")
	FText DisplayName;

	/** Container types to show in this panel */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panel")
	TArray<ESuspenseCoreContainerType> ContainerTypes;

	/** Container tags for fine-grained control */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panel")
	TArray<FGameplayTag> ContainerTags;

	/** Icon for tab/button (optional) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panel")
	FSoftObjectPath IconPath;

	/** Sort order in tab bar */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panel")
	int32 SortOrder;

	/** Is this panel enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panel")
	bool bIsEnabled;

	/** Keyboard shortcut (optional) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panel")
	FKey ShortcutKey;

	/** Horizontal or vertical layout for containers */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panel")
	bool bHorizontalLayout;

	FSuspenseCorePanelConfig()
		: DisplayName(FText::GetEmpty())
		, SortOrder(0)
		, bIsEnabled(true)
		, bHorizontalLayout(true)
	{
	}

	/** Alias for DisplayName for clarity */
	const FText& GetPanelDisplayName() const { return DisplayName; }
};

/**
 * FSuspenseCoreContainerScreenConfig / FSuspenseCoreScreenConfig
 * Configuration for the entire container screen
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreScreenConfig
{
	GENERATED_BODY()

	/** All panel configurations */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screen")
	TArray<FSuspenseCorePanelConfig> Panels;

	/** Default panel to show */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screen")
	FGameplayTag DefaultPanelTag;

	/** Allow drag between panels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screen")
	bool bAllowCrossPanelDrag;

	/** Show weight in UI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screen")
	bool bShowWeight;

	/** Show currency */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screen")
	bool bShowCurrency;

	FSuspenseCoreContainerScreenConfig()
		: bAllowCrossPanelDrag(true)
		, bShowWeight(true)
		, bShowCurrency(true)
	{
	}

	/** Get panel config by tag */
	const FSuspenseCorePanelConfig* FindPanel(const FGameplayTag& PanelTag) const
	{
		return Panels.FindByPredicate([&PanelTag](const FSuspenseCorePanelConfig& Config)
		{
			return Config.PanelTag == PanelTag;
		});
	}

	/** Get sorted panels */
	TArray<FSuspenseCorePanelConfig> GetSortedPanels() const
	{
		TArray<FSuspenseCorePanelConfig> Sorted = Panels;
		Sorted.Sort([](const FSuspenseCorePanelConfig& A, const FSuspenseCorePanelConfig& B)
		{
			return A.SortOrder < B.SortOrder;
		});
		return Sorted;
	}
};

/**
 * FSuspenseCoreEquipmentSlotConfig
 * Configuration for a single equipment slot
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreEquipmentSlotConfig
{
	GENERATED_BODY()

	/** Slot type tag (Equipment.Slot.Primary, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	FGameplayTag SlotTypeTag;

	/** Display name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	FText DisplayName;

	/** Allowed item types for this slot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	FGameplayTagContainer AllowedItemTypes;

	/** Slot visual size in UI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	FIntPoint SlotSize;

	/** Position in equipment layout */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	FVector2D LayoutPosition;

	/** Empty slot icon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	FSoftObjectPath EmptySlotIcon;

	/** Is required for gameplay */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot")
	bool bIsRequired;

	FSuspenseCoreEquipmentSlotConfig()
		: DisplayName(FText::GetEmpty())
		, SlotSize(1, 1)
		, LayoutPosition(FVector2D::ZeroVector)
		, bIsRequired(false)
	{
	}
};

/**
 * Type alias for backwards compatibility
 */
using FSuspenseCoreContainerScreenConfig = FSuspenseCoreScreenConfig;
