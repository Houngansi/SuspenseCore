// SuspenseCoreEquipmentUIProvider.cpp
// SuspenseCore - UI Data Provider Component Implementation
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
#include "GameFramework/PlayerState.h"

// EventBus tags for equipment events (published by EquipmentSystem)
namespace EquipmentEventTags
{
	// Primary events from EquipmentInventoryBridge
	static const FName Equipped = TEXT("Equipment.Event.Equipped");
	static const FName Unequipped = TEXT("Equipment.Event.Unequipped");
	// Alternative events from EquipmentComponentBase
	static const FName ItemEquipped = TEXT("Equipment.Event.ItemEquipped");
	static const FName ItemUnequipped = TEXT("Equipment.Event.ItemUnequipped");
	// Slot/general updates
	static const FName SlotUpdated = TEXT("Equipment.Slot.Updated");
	static const FName EquipmentUpdated = TEXT("Equipment.Updated");
}

//==================================================================
// Constructor
//==================================================================

USuspenseCoreEquipmentUIProvider::USuspenseCoreEquipmentUIProvider()
	: ProviderID(FGuid::NewGuid())
	, bIsInitialized(false)
{
	// This component should replicate
	SetIsReplicatedByDefault(true);
}

//==================================================================
// UActorComponent Interface
//==================================================================

void USuspenseCoreEquipmentUIProvider::BeginPlay()
{
	Super::BeginPlay();

	// Auto-initialize if not already done
	if (!bIsInitialized)
	{
		// Try to get LoadoutID from owning PlayerState
		FName EffectiveLoadoutID = LoadoutID;
		if (EffectiveLoadoutID.IsNone())
		{
			// Check if owner has a DefaultLoadoutID property (PlayerState)
			if (AActor* Owner = GetOwner())
			{
				// Use reflection to find DefaultLoadoutID property
				FProperty* LoadoutProp = Owner->GetClass()->FindPropertyByName(TEXT("DefaultLoadoutID"));
				if (LoadoutProp && LoadoutProp->IsA<FNameProperty>())
				{
					FNameProperty* NameProp = CastField<FNameProperty>(LoadoutProp);
					EffectiveLoadoutID = NameProp->GetPropertyValue_InContainer(Owner);
				}
			}
		}

		InitializeProvider(EffectiveLoadoutID);
	}

	// Setup EventBus subscriptions for push-based data sync
	SetupEventSubscriptions();

	UE_LOG(LogTemp, Log, TEXT("EquipmentUIProvider: BeginPlay on %s, Initialized=%s, SlotCount=%d"),
		GetOwner() ? *GetOwner()->GetName() : TEXT("None"),
		bIsInitialized ? TEXT("true") : TEXT("false"),
		SlotConfigs.Num());
}

void USuspenseCoreEquipmentUIProvider::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TeardownEventSubscriptions();
	Shutdown();
	Super::EndPlay(EndPlayReason);
}

//==================================================================
// Initialization
//==================================================================

bool USuspenseCoreEquipmentUIProvider::InitializeProvider(FName InLoadoutID)
{
	if (bIsInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipmentUIProvider: Already initialized"));
		return true;
	}

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
		UE_LOG(LogTemp, Log, TEXT("EquipmentUIProvider: No slot configs from loadout, using defaults (17 Tarkov-style slots)"));

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

	SlotConfigs.Empty();
	SlotTypeToIndex.Empty();
	CachedEquippedItems.Empty();
	CachedEventBus.Reset();
	CachedLoadoutManager.Reset();
	CachedDataManager.Reset();
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
	Items.Reserve(CachedEquippedItems.Num());

	USuspenseCoreDataManager* DataManager = GetDataManager();

	// Use cached equipped items (pushed via EventBus, not pulled from DataStore)
	for (const auto& Pair : CachedEquippedItems)
	{
		const int32 SlotIndex = Pair.Key;
		const FSuspenseCoreInventoryItemInstance& ItemInstance = Pair.Value;

		if (!ItemInstance.IsValid())
		{
			continue;
		}

		FSuspenseCoreItemUIData ItemData;
		ItemData.InstanceID = ItemInstance.InstanceID;
		ItemData.ItemID = ItemInstance.ItemID;
		ItemData.Quantity = ItemInstance.Quantity;
		ItemData.AnchorSlot = SlotIndex;
		ItemData.GridSize = FIntPoint(1, 1); // Equipment items are 1x1

		// Get additional data from DataManager if available
		if (DataManager)
		{
			FSuspenseCoreItemData ItemTableData;
			if (DataManager->GetItemData(ItemInstance.ItemID, ItemTableData))
			{
				ItemData.DisplayName = ItemTableData.Identity.DisplayName;
				ItemData.Description = ItemTableData.Identity.Description;
				ItemData.IconPath = ItemTableData.Identity.Icon.ToSoftObjectPath();
				ItemData.ItemType = ItemTableData.Classification.ItemType;
				ItemData.RarityTag = ItemTableData.Classification.Rarity;
				ItemData.bIsStackable = ItemTableData.InventoryProps.IsStackable();
				ItemData.MaxStackSize = ItemTableData.InventoryProps.MaxStackSize;
				ItemData.GridSize = ItemTableData.InventoryProps.GridSize;
				ItemData.UnitWeight = ItemTableData.InventoryProps.Weight;
				ItemData.TotalWeight = ItemTableData.InventoryProps.Weight * ItemInstance.Quantity;
				ItemData.BaseValue = ItemTableData.InventoryProps.BaseValue;
				ItemData.TotalValue = ItemTableData.InventoryProps.BaseValue * ItemInstance.Quantity;
			}
		}

		Items.Add(ItemData);
	}

	UE_LOG(LogTemp, Verbose, TEXT("EquipmentUIProvider::GetAllItemUIData - Returning %d cached items"), Items.Num());

	return Items;
}

bool USuspenseCoreEquipmentUIProvider::GetItemUIDataAtSlot(int32 SlotIndex, FSuspenseCoreItemUIData& OutItem) const
{
	if (!IsSlotValid(SlotIndex))
	{
		return false;
	}

	// Get item from cache (pushed via EventBus, not pulled from DataStore)
	const FSuspenseCoreInventoryItemInstance* ItemInstance = CachedEquippedItems.Find(SlotIndex);
	if (!ItemInstance || !ItemInstance->IsValid())
	{
		// Slot is empty
		return false;
	}

	// Convert to UI data
	OutItem.InstanceID = ItemInstance->InstanceID;
	OutItem.ItemID = ItemInstance->ItemID;
	OutItem.Quantity = ItemInstance->Quantity;
	OutItem.AnchorSlot = SlotIndex;
	OutItem.GridSize = FIntPoint(1, 1);

	// Get additional data from DataManager
	USuspenseCoreDataManager* DataManager = GetDataManager();
	if (DataManager)
	{
		FSuspenseCoreItemData ItemTableData;
		if (DataManager->GetItemData(ItemInstance->ItemID, ItemTableData))
		{
			OutItem.DisplayName = ItemTableData.Identity.DisplayName;
			OutItem.Description = ItemTableData.Identity.Description;
			OutItem.IconPath = ItemTableData.Identity.Icon.ToSoftObjectPath();
			OutItem.ItemType = ItemTableData.Classification.ItemType;
			OutItem.RarityTag = ItemTableData.Classification.Rarity;
			OutItem.bIsStackable = ItemTableData.InventoryProps.IsStackable();
			OutItem.MaxStackSize = ItemTableData.InventoryProps.MaxStackSize;
			OutItem.GridSize = ItemTableData.InventoryProps.GridSize;
			OutItem.UnitWeight = ItemTableData.InventoryProps.Weight;
			OutItem.TotalWeight = ItemTableData.InventoryProps.Weight * ItemInstance->Quantity;
			OutItem.BaseValue = ItemTableData.InventoryProps.BaseValue;
			OutItem.TotalValue = ItemTableData.InventoryProps.BaseValue * ItemInstance->Quantity;
		}
	}

	return true;
}

bool USuspenseCoreEquipmentUIProvider::FindItemUIData(const FGuid& InstanceID, FSuspenseCoreItemUIData& OutItem) const
{
	if (!InstanceID.IsValid())
	{
		return false;
	}

	// Search through all slots for the item
	for (int32 SlotIndex = 0; SlotIndex < SlotConfigs.Num(); ++SlotIndex)
	{
		FSuspenseCoreItemUIData ItemData;
		if (GetItemUIDataAtSlot(SlotIndex, ItemData))
		{
			if (ItemData.InstanceID == InstanceID)
			{
				OutItem = ItemData;
				return true;
			}
		}
	}

	return false;
}

int32 USuspenseCoreEquipmentUIProvider::GetItemCount() const
{
	// Use cached equipped items count
	return CachedEquippedItems.Num();
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
	if (UWorld* World = GetWorld())
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
			// Check cached equipped items (pushed via EventBus, not pulled from DataStore)
			const FSuspenseCoreInventoryItemInstance* CachedItem = CachedEquippedItems.Find(SlotIndex);
			if (CachedItem && CachedItem->IsValid())
			{
				SlotData.State = ESuspenseCoreUISlotState::Occupied;
			}
			else
			{
				SlotData.State = ESuspenseCoreUISlotState::Empty;
			}
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
	if (UWorld* World = GetWorld())
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

	return nullptr;
}

//==================================================================
// EventBus Handlers (Push-based data sync)
//==================================================================

void USuspenseCoreEquipmentUIProvider::SetupEventSubscriptions()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipmentUIProvider: Cannot setup subscriptions - EventBus not found"));
		return;
	}

	// Subscribe to Equipment.Event.Equipped (from EquipmentInventoryBridge)
	FGameplayTag EquippedTag = FGameplayTag::RequestGameplayTag(EquipmentEventTags::Equipped);
	if (EquippedTag.IsValid())
	{
		FSuspenseCoreSubscriptionHandle EquippedHandle = EventBus->SubscribeNative(
			EquippedTag,
			this,
			FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreEquipmentUIProvider::OnItemEquipped)
		);
		EventSubscriptions.Add(EquippedHandle);
	}

	// Also subscribe to Equipment.Event.ItemEquipped (from EquipmentComponentBase)
	FGameplayTag ItemEquippedTag = FGameplayTag::RequestGameplayTag(EquipmentEventTags::ItemEquipped);
	if (ItemEquippedTag.IsValid())
	{
		FSuspenseCoreSubscriptionHandle ItemEquippedHandle = EventBus->SubscribeNative(
			ItemEquippedTag,
			this,
			FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreEquipmentUIProvider::OnItemEquipped)
		);
		EventSubscriptions.Add(ItemEquippedHandle);
	}

	// Subscribe to Equipment.Event.Unequipped (from EquipmentInventoryBridge)
	FGameplayTag UnequippedTag = FGameplayTag::RequestGameplayTag(EquipmentEventTags::Unequipped);
	if (UnequippedTag.IsValid())
	{
		FSuspenseCoreSubscriptionHandle UnequippedHandle = EventBus->SubscribeNative(
			UnequippedTag,
			this,
			FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreEquipmentUIProvider::OnItemUnequipped)
		);
		EventSubscriptions.Add(UnequippedHandle);
	}

	// Also subscribe to Equipment.Event.ItemUnequipped (from EquipmentComponentBase)
	FGameplayTag ItemUnequippedTag = FGameplayTag::RequestGameplayTag(EquipmentEventTags::ItemUnequipped);
	if (ItemUnequippedTag.IsValid())
	{
		FSuspenseCoreSubscriptionHandle ItemUnequippedHandle = EventBus->SubscribeNative(
			ItemUnequippedTag,
			this,
			FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreEquipmentUIProvider::OnItemUnequipped)
		);
		EventSubscriptions.Add(ItemUnequippedHandle);
	}

	// Subscribe to Equipment.Slot.Updated for slot state changes
	FGameplayTag SlotUpdatedTag = FGameplayTag::RequestGameplayTag(EquipmentEventTags::SlotUpdated);
	if (SlotUpdatedTag.IsValid())
	{
		FSuspenseCoreSubscriptionHandle SlotHandle = EventBus->SubscribeNative(
			SlotUpdatedTag,
			this,
			FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreEquipmentUIProvider::OnSlotUpdated)
		);
		EventSubscriptions.Add(SlotHandle);
	}

	UE_LOG(LogTemp, Log, TEXT("EquipmentUIProvider: Subscribed to %d equipment events"), EventSubscriptions.Num());
}

void USuspenseCoreEquipmentUIProvider::TeardownEventSubscriptions()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (EventBus)
	{
		for (const FSuspenseCoreSubscriptionHandle& Handle : EventSubscriptions)
		{
			EventBus->Unsubscribe(Handle);
		}
	}
	EventSubscriptions.Empty();
}

void USuspenseCoreEquipmentUIProvider::OnItemEquipped(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// Extract slot index and item data from event
	// Try both "Slot" (Bridge) and "SlotIndex" (ComponentBase) field names
	int32 SlotIndex = EventData.GetInt(FName(TEXT("Slot")), INDEX_NONE);
	if (SlotIndex == INDEX_NONE)
	{
		SlotIndex = EventData.GetInt(FName(TEXT("SlotIndex")), INDEX_NONE);
	}

	FString ItemIDStr = EventData.GetString(FName(TEXT("ItemID")));
	FString InstanceIDStr = EventData.GetString(FName(TEXT("InstanceID")));

	// Create item instance from event data
	FSuspenseCoreInventoryItemInstance ItemInstance;
	ItemInstance.ItemID = FName(*ItemIDStr);
	FGuid::Parse(InstanceIDStr, ItemInstance.InstanceID);
	ItemInstance.Quantity = EventData.GetInt(FName(TEXT("Quantity")), 1);
	if (ItemInstance.Quantity <= 0)
	{
		ItemInstance.Quantity = 1;
	}

	if (SlotIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipmentUIProvider: OnItemEquipped - Invalid SlotIndex, cannot cache item %s"), *ItemIDStr);
		return;
	}

	// Add to cache
	CachedEquippedItems.Add(SlotIndex, ItemInstance);

	UE_LOG(LogTemp, Log, TEXT("EquipmentUIProvider: OnItemEquipped - Slot %d, Item %s, CacheSize=%d"),
		SlotIndex, *ItemIDStr, CachedEquippedItems.Num());

	// Broadcast UI update
	UIDataChangedDelegate.Broadcast(TAG_SuspenseCore_Event_UIProvider_DataChanged, ItemInstance.InstanceID);
}

void USuspenseCoreEquipmentUIProvider::OnItemUnequipped(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// Extract slot index from event
	// Try both "Slot" (Bridge) and "SlotIndex" (ComponentBase) field names
	int32 SlotIndex = EventData.GetInt(FName(TEXT("Slot")), INDEX_NONE);
	if (SlotIndex == INDEX_NONE)
	{
		SlotIndex = EventData.GetInt(FName(TEXT("SlotIndex")), INDEX_NONE);
	}

	if (SlotIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipmentUIProvider: OnItemUnequipped - Invalid SlotIndex"));
		return;
	}

	// Get instance ID before removing for delegate
	FGuid RemovedInstanceID;
	if (const FSuspenseCoreInventoryItemInstance* ExistingItem = CachedEquippedItems.Find(SlotIndex))
	{
		RemovedInstanceID = ExistingItem->InstanceID;
	}

	// Remove from cache
	CachedEquippedItems.Remove(SlotIndex);

	UE_LOG(LogTemp, Log, TEXT("EquipmentUIProvider: OnItemUnequipped - Slot %d, CacheSize=%d"),
		SlotIndex, CachedEquippedItems.Num());

	// Broadcast UI update
	UIDataChangedDelegate.Broadcast(TAG_SuspenseCore_Event_UIProvider_DataChanged, RemovedInstanceID);
}

void USuspenseCoreEquipmentUIProvider::OnSlotUpdated(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// This is a general slot update - refresh the affected slot
	// Try both "SlotIndex" and "Slot" field names
	int32 SlotIndex = EventData.GetInt(FName(TEXT("SlotIndex")), INDEX_NONE);
	if (SlotIndex == INDEX_NONE)
	{
		// Also try float (some systems may use float)
		SlotIndex = static_cast<int32>(EventData.GetFloat(FName(TEXT("SlotIndex")), -1.0f));
	}
	bool bOccupied = EventData.GetBool(FName(TEXT("Occupied")));

	UE_LOG(LogTemp, Log, TEXT("EquipmentUIProvider: OnSlotUpdated - Slot %d, Occupied=%s"),
		SlotIndex, bOccupied ? TEXT("true") : TEXT("false"));

	// CRITICAL: Update cache based on occupied state
	if (SlotIndex != INDEX_NONE && !bOccupied)
	{
		// Slot was cleared - remove from cache
		if (CachedEquippedItems.Contains(SlotIndex))
		{
			CachedEquippedItems.Remove(SlotIndex);
			UE_LOG(LogTemp, Log, TEXT("EquipmentUIProvider: OnSlotUpdated - Removed slot %d from cache, CacheSize=%d"),
				SlotIndex, CachedEquippedItems.Num());
		}
	}

	// Broadcast UI update for the slot
	UIDataChangedDelegate.Broadcast(TAG_SuspenseCore_Event_UIProvider_DataChanged, FGuid());
}

USuspenseCoreDataManager* USuspenseCoreEquipmentUIProvider::GetDataManager() const
{
	if (CachedDataManager.IsValid())
	{
		return CachedDataManager.Get();
	}

	// Get from GameInstance subsystem
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			USuspenseCoreDataManager* DataManager = GI->GetSubsystem<USuspenseCoreDataManager>();
			if (DataManager)
			{
				CachedDataManager = DataManager;
				return DataManager;
			}
		}
	}

	return nullptr;
}
