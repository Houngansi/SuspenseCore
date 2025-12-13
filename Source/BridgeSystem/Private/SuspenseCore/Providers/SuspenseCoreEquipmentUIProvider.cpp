// SuspenseCoreEquipmentUIProvider.cpp
// SuspenseCore - UI Data Provider Adapter Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Providers/SuspenseCoreEquipmentUIProvider.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Services/SuspenseCoreLoadoutManager.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

// Native GameplayTags
#include "SuspenseCore/Events/SuspenseCoreUIEvents.h"

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

bool USuspenseCoreEquipmentUIProvider::Initialize(UActorComponent* InDataStore, FName InLoadoutID)
{
	if (bIsInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipmentUIProvider: Already initialized"));
		return true;
	}

	if (!InDataStore)
	{
		UE_LOG(LogTemp, Error, TEXT("EquipmentUIProvider: Invalid data store"));
		return false;
	}

	BoundDataStore = InDataStore;
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

	// Bind to data store events
	BindToDataStore();

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

	UnbindFromDataStore();

	BoundDataStore.Reset();
	SlotConfigs.Empty();
	SlotTypeToIndex.Empty();
	bIsInitialized = false;

	UE_LOG(LogTemp, Log, TEXT("EquipmentUIProvider: Shutdown complete"));
}

//==================================================================
// ISuspenseCoreUIDataProvider Interface
//==================================================================

FGameplayTag USuspenseCoreEquipmentUIProvider::GetContainerTypeTag() const
{
	return FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.UIProvider.Type.Equipment"), false);
}

FSuspenseCoreContainerUIData USuspenseCoreEquipmentUIProvider::GetContainerData() const
{
	FSuspenseCoreContainerUIData Data;

	Data.ContainerID = ProviderID;
	Data.ContainerType = ESuspenseCoreContainerType::Equipment;
	Data.ContainerTypeTag = GetContainerTypeTag();
	Data.DisplayName = FText::FromString(TEXT("Equipment"));
	Data.LayoutType = ESuspenseCoreSlotLayoutType::Named;
	Data.TotalSlots = SlotConfigs.Num();
	Data.bHasWeightLimit = false; // Equipment doesn't have weight limit on container level
	Data.bIsLocked = false;
	Data.bIsReadOnly = false;

	// Count occupied slots
	int32 OccupiedCount = 0;
	for (int32 Index = 0; Index < SlotConfigs.Num(); ++Index)
	{
		FSuspenseCoreSlotUIData SlotData = GetSlotData(Index);
		if (SlotData.IsOccupied())
		{
			OccupiedCount++;
		}
		Data.Slots.Add(SlotData);
	}
	Data.OccupiedSlots = OccupiedCount;

	return Data;
}

FSuspenseCoreSlotUIData USuspenseCoreEquipmentUIProvider::GetSlotData(int32 SlotIndex) const
{
	if (!SlotConfigs.IsValidIndex(SlotIndex))
	{
		return FSuspenseCoreSlotUIData();
	}

	return ConvertToSlotUIData(SlotConfigs[SlotIndex].SlotType, SlotIndex);
}

FSuspenseCoreItemUIData USuspenseCoreEquipmentUIProvider::GetItemData(const FGuid& InstanceID) const
{
	if (!InstanceID.IsValid())
	{
		return FSuspenseCoreItemUIData();
	}

	return ConvertToItemUIData(InstanceID);
}

TArray<FSuspenseCoreSlotUIData> USuspenseCoreEquipmentUIProvider::GetAllSlotData() const
{
	TArray<FSuspenseCoreSlotUIData> AllSlots;
	AllSlots.Reserve(SlotConfigs.Num());

	for (int32 Index = 0; Index < SlotConfigs.Num(); ++Index)
	{
		AllSlots.Add(GetSlotData(Index));
	}

	return AllSlots;
}

bool USuspenseCoreEquipmentUIProvider::CanPerformAction(const FGameplayTag& ActionTag, int32 SlotIndex) const
{
	if (!bIsInitialized || !BoundDataStore.IsValid())
	{
		return false;
	}

	// Check basic slot validity
	if (!SlotConfigs.IsValidIndex(SlotIndex))
	{
		return false;
	}

	// TODO: Implement action validation via EquipmentDataStore interface
	// For now, allow basic actions
	return true;
}

bool USuspenseCoreEquipmentUIProvider::RequestAction(const FGameplayTag& ActionTag, int32 SlotIndex, const FSuspenseCoreDragData* DragData)
{
	if (!CanPerformAction(ActionTag, SlotIndex))
	{
		return false;
	}

	// Publish action request via EventBus
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
		EventData.SetInt(FName("SlotIndex"), SlotIndex);

		if (SlotConfigs.IsValidIndex(SlotIndex))
		{
			EventData.SetTag(FName("SlotType"), SlotConfigs[SlotIndex].SlotTag);
		}

		if (DragData)
		{
			EventData.SetGuid(FName("ItemInstanceID"), DragData->Item.InstanceID);
			EventData.SetInt(FName("SourceSlot"), DragData->SourceSlot);
		}

		EventBus->Publish(ActionTag, EventData);
		return true;
	}

	return false;
}

//==================================================================
// Equipment-Specific API
//==================================================================

FSuspenseCoreSlotUIData USuspenseCoreEquipmentUIProvider::GetSlotDataByType(EEquipmentSlotType SlotType) const
{
	int32 SlotIndex = GetSlotIndexForType(SlotType);
	if (SlotIndex != INDEX_NONE)
	{
		return GetSlotData(SlotIndex);
	}
	return FSuspenseCoreSlotUIData();
}

FSuspenseCoreSlotUIData USuspenseCoreEquipmentUIProvider::GetSlotDataByTag(const FGameplayTag& SlotTag) const
{
	for (int32 Index = 0; Index < SlotConfigs.Num(); ++Index)
	{
		if (SlotConfigs[Index].SlotTag == SlotTag)
		{
			return GetSlotData(Index);
		}
	}
	return FSuspenseCoreSlotUIData();
}

FSuspenseCoreItemUIData USuspenseCoreEquipmentUIProvider::GetEquippedItemByType(EEquipmentSlotType SlotType) const
{
	FSuspenseCoreSlotUIData SlotData = GetSlotDataByType(SlotType);
	if (SlotData.IsOccupied() && SlotData.ItemInstanceID.IsValid())
	{
		return GetItemData(SlotData.ItemInstanceID);
	}
	return FSuspenseCoreItemUIData();
}

void USuspenseCoreEquipmentUIProvider::RefreshAllSlots()
{
	if (!bIsInitialized)
	{
		return;
	}

	// Broadcast full refresh
	UIDataChangedDelegate.Broadcast(
		FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.UI.FullRefresh"), false),
		FGuid()
	);
}

//==================================================================
// Data Store Binding
//==================================================================

void USuspenseCoreEquipmentUIProvider::BindToDataStore()
{
	if (!BoundDataStore.IsValid())
	{
		return;
	}

	// Subscribe to equipment events via EventBus
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		// Subscribe to equipment change events
		// TODO: Add specific equipment event subscriptions when EquipmentDataStore broadcasts them
		UE_LOG(LogTemp, Log, TEXT("EquipmentUIProvider: Bound to EventBus for equipment events"));
	}
}

void USuspenseCoreEquipmentUIProvider::UnbindFromDataStore()
{
	// Unsubscribe from EventBus events
	// TODO: Remove subscriptions when proper event system is in place
}

void USuspenseCoreEquipmentUIProvider::OnEquipmentSlotChanged(EEquipmentSlotType SlotType, const FGuid& ItemInstanceID)
{
	// Find slot index
	int32 SlotIndex = GetSlotIndexForType(SlotType);
	if (SlotIndex == INDEX_NONE)
	{
		return;
	}

	// Get slot tag for event
	FGameplayTag SlotTag;
	if (SlotConfigs.IsValidIndex(SlotIndex))
	{
		SlotTag = SlotConfigs[SlotIndex].SlotTag;
	}

	// Broadcast slot changed event
	UIDataChangedDelegate.Broadcast(SlotTag, ItemInstanceID);

	UE_LOG(LogTemp, Log, TEXT("EquipmentUIProvider: Slot %s changed (ItemID: %s)"),
		*SlotTag.ToString(), *ItemInstanceID.ToString());
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
		SlotData.bIsLocked = !Config.bIsVisible;
	}

	// TODO: Query actual equipped item from BoundDataStore
	// For now, return empty slot
	SlotData.State = ESuspenseCoreUISlotState::Empty;
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

USuspenseCoreEventBus* USuspenseCoreEquipmentUIProvider::GetEventBus() const
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	// Get from EventManager
	if (GEngine && GEngine->GameViewport)
	{
		if (UWorld* World = GEngine->GameViewport->GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				if (USuspenseCoreEventManager* EventManager = GI->GetSubsystem<USuspenseCoreEventManager>())
				{
					const_cast<USuspenseCoreEquipmentUIProvider*>(this)->CachedEventBus = EventManager->GetEventBus();
					return CachedEventBus.Get();
				}
			}
		}
	}

	return nullptr;
}

USuspenseCoreLoadoutManager* USuspenseCoreEquipmentUIProvider::GetLoadoutManager() const
{
	if (CachedLoadoutManager.IsValid())
	{
		return CachedLoadoutManager.Get();
	}

	// Get from GameInstance subsystem
	if (GEngine && GEngine->GameViewport)
	{
		if (UWorld* World = GEngine->GameViewport->GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				USuspenseCoreLoadoutManager* LoadoutManager = GI->GetSubsystem<USuspenseCoreLoadoutManager>();
				if (LoadoutManager)
				{
					const_cast<USuspenseCoreEquipmentUIProvider*>(this)->CachedLoadoutManager = LoadoutManager;
					return LoadoutManager;
				}
			}
		}
	}

	return nullptr;
}
