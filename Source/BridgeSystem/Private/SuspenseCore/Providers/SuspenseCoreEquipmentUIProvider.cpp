// SuspenseCoreEquipmentUIProvider.cpp
// SuspenseCore - UI Data Provider Adapter Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Providers/SuspenseCoreEquipmentUIProvider.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Services/SuspenseCoreLoadoutManager.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Events/UI/SuspenseCoreUIEvents.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"

//==================================================================
// Constructor
//==================================================================

USuspenseCoreEquipmentUIProvider::USuspenseCoreEquipmentUIProvider()
	: ProviderID(FGuid::NewGuid())
	, bIsInitialized(false)
{
}

//==================================================================
// Initialization
//==================================================================

bool USuspenseCoreEquipmentUIProvider::Initialize(AActor* InOwningActor, FName InLoadoutID)
{
	if (bIsInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipmentUIProvider: Already initialized"));
		return true;
	}

	OwningActor = InOwningActor;
	LoadoutID = InLoadoutID;

	// Get slot configurations from LoadoutManager
	USuspenseCoreLoadoutManager* LoadoutManager = GetLoadoutManager();
	if (LoadoutManager)
	{
		// Use provided loadout ID or find default
		FName EffectiveLoadoutID = LoadoutID;
		if (EffectiveLoadoutID.IsNone())
		{
			TArray<FName> AllLoadouts = LoadoutManager->GetAllLoadoutIDs();
			if (AllLoadouts.Num() > 0)
			{
				EffectiveLoadoutID = AllLoadouts[0];
			}
		}

		if (!EffectiveLoadoutID.IsNone())
		{
			SlotConfigs = LoadoutManager->GetEquipmentSlots(EffectiveLoadoutID);
			LoadoutID = EffectiveLoadoutID;

			// Build slot type to index map
			for (int32 Index = 0; Index < SlotConfigs.Num(); ++Index)
			{
				SlotTypeToIndex.Add(SlotConfigs[Index].SlotType, Index);
			}

			UE_LOG(LogTemp, Log, TEXT("EquipmentUIProvider: Loaded %d slot configs from loadout '%s'"),
				SlotConfigs.Num(), *LoadoutID.ToString());
		}
	}

	// If no configs from loadout, create default
	if (SlotConfigs.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipmentUIProvider: No slot configs from loadout, using defaults"));

		// Create default slot configs for all 17 slots
		FLoadoutConfiguration DefaultConfig;
		SlotConfigs = DefaultConfig.EquipmentSlots;

		for (int32 Index = 0; Index < SlotConfigs.Num(); ++Index)
		{
			SlotTypeToIndex.Add(SlotConfigs[Index].SlotType, Index);
		}
	}

	bIsInitialized = true;

	UE_LOG(LogTemp, Log, TEXT("EquipmentUIProvider: Initialized with %d equipment slots"), SlotConfigs.Num());

	return true;
}

void USuspenseCoreEquipmentUIProvider::Shutdown()
{
	if (!bIsInitialized)
	{
		return;
	}

	OwningActor.Reset();
	SlotConfigs.Empty();
	SlotTypeToIndex.Empty();
	bIsInitialized = false;

	UE_LOG(LogTemp, Log, TEXT("EquipmentUIProvider: Shutdown complete"));
}

//==================================================================
// ISuspenseCoreUIDataProvider Interface - Identity
//==================================================================

FGameplayTag USuspenseCoreEquipmentUIProvider::GetContainerTypeTag() const
{
	return TAG_SuspenseCore_UIProvider_Type_Equipment;
}

//==================================================================
// ISuspenseCoreUIDataProvider Interface - Container Data
//==================================================================

FSuspenseCoreContainerUIData USuspenseCoreEquipmentUIProvider::GetContainerUIData() const
{
	FSuspenseCoreContainerUIData Data;

	Data.ContainerID = ProviderID;
	Data.ContainerType = ESuspenseCoreContainerType::Equipment;
	Data.ContainerTypeTag = GetContainerTypeTag();
	Data.DisplayName = FText::FromString(TEXT("Equipment"));
	Data.LayoutType = ESuspenseCoreSlotLayoutType::Named;
	Data.TotalSlots = SlotConfigs.Num();
	Data.bHasWeightLimit = false;
	Data.bIsLocked = false;
	Data.bIsReadOnly = false;

	// Get all slot data
	Data.Slots = GetAllSlotUIData();

	// Count occupied slots
	int32 OccupiedCount = 0;
	for (const FSuspenseCoreSlotUIData& SlotData : Data.Slots)
	{
		if (SlotData.IsOccupied())
		{
			OccupiedCount++;
		}
	}
	Data.OccupiedSlots = OccupiedCount;

	// Get all items
	Data.Items = GetAllItemUIData();

	return Data;
}

FIntPoint USuspenseCoreEquipmentUIProvider::GetGridSize() const
{
	// Equipment uses named slots, not grid
	// Return slot count as "grid" for compatibility
	return FIntPoint(SlotConfigs.Num(), 1);
}

//==================================================================
// ISuspenseCoreUIDataProvider Interface - Slot Data
//==================================================================

TArray<FSuspenseCoreSlotUIData> USuspenseCoreEquipmentUIProvider::GetAllSlotUIData() const
{
	TArray<FSuspenseCoreSlotUIData> AllSlots;
	AllSlots.Reserve(SlotConfigs.Num());

	for (int32 Index = 0; Index < SlotConfigs.Num(); ++Index)
	{
		AllSlots.Add(GetSlotUIData(Index));
	}

	return AllSlots;
}

FSuspenseCoreSlotUIData USuspenseCoreEquipmentUIProvider::GetSlotUIData(int32 SlotIndex) const
{
	if (!SlotConfigs.IsValidIndex(SlotIndex))
	{
		return FSuspenseCoreSlotUIData();
	}

	return ConvertToSlotUIData(SlotConfigs[SlotIndex].SlotType, SlotIndex);
}

bool USuspenseCoreEquipmentUIProvider::IsSlotValid(int32 SlotIndex) const
{
	return SlotConfigs.IsValidIndex(SlotIndex);
}

//==================================================================
// ISuspenseCoreUIDataProvider Interface - Item Data
//==================================================================

TArray<FSuspenseCoreItemUIData> USuspenseCoreEquipmentUIProvider::GetAllItemUIData() const
{
	TArray<FSuspenseCoreItemUIData> Items;

	// TODO: Query actual equipped items from EquipmentDataStore
	// For now, return empty array

	return Items;
}

bool USuspenseCoreEquipmentUIProvider::GetItemUIDataAtSlot(int32 SlotIndex, FSuspenseCoreItemUIData& OutItem) const
{
	if (!IsSlotValid(SlotIndex))
	{
		return false;
	}

	// TODO: Query actual equipped item from EquipmentDataStore
	// For now, return false (empty slot)

	return false;
}

bool USuspenseCoreEquipmentUIProvider::FindItemUIData(const FGuid& InstanceID, FSuspenseCoreItemUIData& OutItem) const
{
	if (!InstanceID.IsValid())
	{
		return false;
	}

	// TODO: Query item from EquipmentDataStore
	// For now, return false

	return false;
}

int32 USuspenseCoreEquipmentUIProvider::GetItemCount() const
{
	// TODO: Count actual equipped items
	return 0;
}

//==================================================================
// ISuspenseCoreUIDataProvider Interface - Validation
//==================================================================

FSuspenseCoreDropValidation USuspenseCoreEquipmentUIProvider::ValidateDrop(
	const FSuspenseCoreDragData& DragData,
	int32 TargetSlot,
	bool bRotated) const
{
	if (!IsSlotValid(TargetSlot))
	{
		return FSuspenseCoreDropValidation::Invalid(FText::FromString(TEXT("Invalid slot")));
	}

	const FEquipmentSlotConfig& SlotConfig = SlotConfigs[TargetSlot];

	// Check if item type is allowed in this slot
	if (!SlotConfig.AllowedItemTypes.IsEmpty())
	{
		if (!SlotConfig.AllowedItemTypes.HasTag(DragData.Item.ItemType))
		{
			return FSuspenseCoreDropValidation::Invalid(FText::FromString(TEXT("Item type not allowed in this slot")));
		}
	}

	// Check disallowed types
	if (SlotConfig.DisallowedItemTypes.HasTag(DragData.Item.ItemType))
	{
		return FSuspenseCoreDropValidation::Invalid(FText::FromString(TEXT("Item type is not allowed")));
	}

	// Valid drop
	FSuspenseCoreDropValidation Validation = FSuspenseCoreDropValidation::Valid();
	Validation.AlternativeSlot = TargetSlot;
	Validation.bWouldSwap = false; // TODO: Check if slot is occupied

	return Validation;
}

bool USuspenseCoreEquipmentUIProvider::CanAcceptItemType(const FGameplayTag& ItemType) const
{
	// Check if any slot can accept this item type
	for (const FEquipmentSlotConfig& SlotConfig : SlotConfigs)
	{
		if (SlotConfig.AllowedItemTypes.IsEmpty() || SlotConfig.AllowedItemTypes.HasTag(ItemType))
		{
			if (!SlotConfig.DisallowedItemTypes.HasTag(ItemType))
			{
				return true;
			}
		}
	}
	return false;
}

int32 USuspenseCoreEquipmentUIProvider::FindBestSlotForItem(FIntPoint ItemSize, bool bAllowRotation) const
{
	// Equipment slots are fixed - this method is more relevant for grid inventory
	// Return INDEX_NONE as equipment requires explicit slot selection
	return INDEX_NONE;
}

//==================================================================
// ISuspenseCoreUIDataProvider Interface - Grid Position Calculations
//==================================================================

int32 USuspenseCoreEquipmentUIProvider::GetSlotAtLocalPosition(const FVector2D& LocalPos, float CellSize, float CellGap) const
{
	// Equipment uses named slots, not grid positions
	// This would be handled by the widget's hit testing
	return INDEX_NONE;
}

TArray<int32> USuspenseCoreEquipmentUIProvider::GetOccupiedSlotsForItem(const FGuid& ItemInstanceID) const
{
	TArray<int32> OccupiedSlots;

	// Equipment items occupy exactly one slot
	for (int32 Index = 0; Index < SlotConfigs.Num(); ++Index)
	{
		FSuspenseCoreItemUIData ItemData;
		if (GetItemUIDataAtSlot(Index, ItemData) && ItemData.InstanceID == ItemInstanceID)
		{
			OccupiedSlots.Add(Index);
			break;
		}
	}

	return OccupiedSlots;
}

int32 USuspenseCoreEquipmentUIProvider::GetAnchorSlotForPosition(int32 AnySlotIndex) const
{
	// Equipment slots are always anchors (no multi-cell spanning)
	return AnySlotIndex;
}

bool USuspenseCoreEquipmentUIProvider::CanPlaceItemAtSlot(const FGuid& ItemID, int32 SlotIndex, bool bRotated) const
{
	if (!IsSlotValid(SlotIndex))
	{
		return false;
	}

	// TODO: Check if slot is occupied and if swap is possible
	return true;
}

//==================================================================
// ISuspenseCoreUIDataProvider Interface - Operations
//==================================================================

bool USuspenseCoreEquipmentUIProvider::RequestMoveItem(int32 FromSlot, int32 ToSlot, bool bRotate)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return false;
	}

	// Create event payload
	FSuspenseCoreUIEventPayload Payload = FSuspenseCoreUIEventPayload::CreateMoveRequest(
		ProviderID,
		ESuspenseCoreContainerType::Equipment,
		FromSlot,
		ToSlot,
		bRotate
	);

	// Publish via EventBus
	FSuspenseCoreEventData EventData;
	EventData.Source = const_cast<USuspenseCoreEquipmentUIProvider*>(this);
	EventBus->Publish(TAG_SuspenseCore_Event_UIRequest_MoveItem, EventData);

	return true;
}

bool USuspenseCoreEquipmentUIProvider::RequestRotateItem(int32 SlotIndex)
{
	// Equipment items don't rotate
	return false;
}

bool USuspenseCoreEquipmentUIProvider::RequestUseItem(int32 SlotIndex)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return false;
	}

	FSuspenseCoreEventData EventData;
	EventData.Source = const_cast<USuspenseCoreEquipmentUIProvider*>(this);
	EventData.IntPayload.Add(TEXT("SlotIndex"), SlotIndex);
	EventData.StringPayload.Add(TEXT("ProviderID"), ProviderID.ToString());

	EventBus->Publish(TAG_SuspenseCore_Event_UIRequest_UseItem, EventData);

	return true;
}

bool USuspenseCoreEquipmentUIProvider::RequestDropItem(int32 SlotIndex, int32 Quantity)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return false;
	}

	FSuspenseCoreUIEventPayload Payload = FSuspenseCoreUIEventPayload::CreateDropRequest(
		ProviderID,
		ESuspenseCoreContainerType::Equipment,
		SlotIndex,
		Quantity
	);

	FSuspenseCoreEventData EventData;
	EventData.Source = const_cast<USuspenseCoreEquipmentUIProvider*>(this);
	EventBus->Publish(TAG_SuspenseCore_Event_UIRequest_DropItem, EventData);

	return true;
}

bool USuspenseCoreEquipmentUIProvider::RequestSplitStack(int32 SlotIndex, int32 SplitQuantity, int32 TargetSlot)
{
	// Equipment items don't stack
	return false;
}

bool USuspenseCoreEquipmentUIProvider::RequestTransferItem(int32 SlotIndex, const FGuid& TargetProviderID, int32 TargetSlot, int32 Quantity)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return false;
	}

	FSuspenseCoreEventData EventData;
	EventData.Source = const_cast<USuspenseCoreEquipmentUIProvider*>(this);
	EventData.IntPayload.Add(TEXT("SourceSlot"), SlotIndex);
	EventData.StringPayload.Add(TEXT("SourceProviderID"), ProviderID.ToString());
	EventData.StringPayload.Add(TEXT("TargetProviderID"), TargetProviderID.ToString());
	EventData.IntPayload.Add(TEXT("TargetSlot"), TargetSlot);
	EventData.IntPayload.Add(TEXT("Quantity"), Quantity);

	EventBus->Publish(TAG_SuspenseCore_Event_UIRequest_TransferItem, EventData);

	return true;
}

//==================================================================
// ISuspenseCoreUIDataProvider Interface - Context Menu
//==================================================================

TArray<FGameplayTag> USuspenseCoreEquipmentUIProvider::GetItemContextActions(int32 SlotIndex) const
{
	TArray<FGameplayTag> Actions;

	if (!IsSlotValid(SlotIndex))
	{
		return Actions;
	}

	FSuspenseCoreItemUIData ItemData;
	if (GetItemUIDataAtSlot(SlotIndex, ItemData))
	{
		// Item is equipped - show unequip, drop, examine
		Actions.Add(TAG_SuspenseCore_UIAction_Unequip);
		Actions.Add(TAG_SuspenseCore_UIAction_Drop);
		Actions.Add(TAG_SuspenseCore_UIAction_Examine);

		// If item is usable (like meds in quick slots)
		if (ItemData.bIsUsable)
		{
			Actions.Add(TAG_SuspenseCore_UIAction_Use);
		}
	}

	return Actions;
}

bool USuspenseCoreEquipmentUIProvider::ExecuteContextAction(int32 SlotIndex, const FGameplayTag& ActionTag)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return false;
	}

	FSuspenseCoreItemUIData ItemData;
	GetItemUIDataAtSlot(SlotIndex, ItemData);

	FSuspenseCoreUIEventPayload Payload = FSuspenseCoreUIEventPayload::CreateActionRequest(
		ProviderID,
		ESuspenseCoreContainerType::Equipment,
		SlotIndex,
		ItemData.InstanceID,
		ActionTag
	);

	FSuspenseCoreEventData EventData;
	EventData.Source = const_cast<USuspenseCoreEquipmentUIProvider*>(this);

	// Route to appropriate handler
	if (ActionTag == TAG_SuspenseCore_UIAction_Unequip)
	{
		EventBus->Publish(TAG_SuspenseCore_Event_UIRequest_UnequipItem, EventData);
	}
	else if (ActionTag == TAG_SuspenseCore_UIAction_Drop)
	{
		EventBus->Publish(TAG_SuspenseCore_Event_UIRequest_DropItem, EventData);
	}
	else if (ActionTag == TAG_SuspenseCore_UIAction_Use)
	{
		EventBus->Publish(TAG_SuspenseCore_Event_UIRequest_UseItem, EventData);
	}

	return true;
}

//==================================================================
// ISuspenseCoreUIDataProvider Interface - EventBus
//==================================================================

USuspenseCoreEventBus* USuspenseCoreEquipmentUIProvider::GetEventBus() const
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	// Get from EventManager
	if (OwningActor.IsValid())
	{
		if (UWorld* World = OwningActor->GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				if (USuspenseCoreEventManager* EventManager = GI->GetSubsystem<USuspenseCoreEventManager>())
				{
					CachedEventBus = EventManager->GetEventBus();
					return CachedEventBus.Get();
				}
			}
		}
	}

	return nullptr;
}

//==================================================================
// Equipment-Specific API
//==================================================================

FSuspenseCoreSlotUIData USuspenseCoreEquipmentUIProvider::GetSlotDataByType(EEquipmentSlotType SlotType) const
{
	int32 SlotIndex = GetSlotIndexForType(SlotType);
	if (SlotIndex != INDEX_NONE)
	{
		return GetSlotUIData(SlotIndex);
	}
	return FSuspenseCoreSlotUIData();
}

void USuspenseCoreEquipmentUIProvider::RefreshAllSlots()
{
	if (!bIsInitialized)
	{
		return;
	}

	// Broadcast full refresh
	UIDataChangedDelegate.Broadcast(
		TAG_SuspenseCore_Event_UIProvider_DataChanged,
		FGuid()
	);
}

//==================================================================
// Data Conversion
//==================================================================

FSuspenseCoreSlotUIData USuspenseCoreEquipmentUIProvider::ConvertToSlotUIData(EEquipmentSlotType SlotType, int32 SlotIndex) const
{
	FSuspenseCoreSlotUIData SlotData;

	SlotData.SlotIndex = SlotIndex;

	if (SlotConfigs.IsValidIndex(SlotIndex))
	{
		const FEquipmentSlotConfig& Config = SlotConfigs[SlotIndex];
		SlotData.SlotTypeTag = Config.SlotTag;
		SlotData.AllowedItemTypes = Config.AllowedItemTypes;

		// Set locked state if slot is not visible
		if (!Config.bIsVisible)
		{
			SlotData.State = ESuspenseCoreUISlotState::Locked;
		}
		else
		{
			// TODO: Query actual equipped item from BoundDataStore
			// For now, return empty slot
			SlotData.State = ESuspenseCoreUISlotState::Empty;
		}
	}
	else
	{
		SlotData.State = ESuspenseCoreUISlotState::Empty;
	}
	SlotData.bIsAnchor = true; // Equipment slots are always anchor

	return SlotData;
}

FSuspenseCoreItemUIData USuspenseCoreEquipmentUIProvider::ConvertToItemUIData(const FGuid& ItemInstanceID) const
{
	FSuspenseCoreItemUIData ItemData;

	if (!ItemInstanceID.IsValid())
	{
		return ItemData;
	}

	ItemData.InstanceID = ItemInstanceID;

	// TODO: Query item data from DataManager using ItemID from equipped item
	// For now, return basic data
	ItemData.Quantity = 1;
	ItemData.GridSize = FIntPoint(1, 1);

	return ItemData;
}

int32 USuspenseCoreEquipmentUIProvider::GetSlotIndexForType(EEquipmentSlotType SlotType) const
{
	if (const int32* FoundIndex = SlotTypeToIndex.Find(SlotType))
	{
		return *FoundIndex;
	}
	return INDEX_NONE;
}

EEquipmentSlotType USuspenseCoreEquipmentUIProvider::GetSlotTypeForIndex(int32 SlotIndex) const
{
	if (SlotConfigs.IsValidIndex(SlotIndex))
	{
		return SlotConfigs[SlotIndex].SlotType;
	}
	return EEquipmentSlotType::None;
}

//==================================================================
// Private Helpers
//==================================================================

USuspenseCoreLoadoutManager* USuspenseCoreEquipmentUIProvider::GetLoadoutManager() const
{
	if (CachedLoadoutManager.IsValid())
	{
		return CachedLoadoutManager.Get();
	}

	// Get from GameInstance subsystem
	if (OwningActor.IsValid())
	{
		if (UWorld* World = OwningActor->GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				USuspenseCoreLoadoutManager* LoadoutManager = GI->GetSubsystem<USuspenseCoreLoadoutManager>();
				if (LoadoutManager)
				{
					CachedLoadoutManager = LoadoutManager;
					return LoadoutManager;
				}
			}
		}
	}

	return nullptr;
}
