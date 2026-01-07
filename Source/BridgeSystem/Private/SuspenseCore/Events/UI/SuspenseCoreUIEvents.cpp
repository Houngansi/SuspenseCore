// SuspenseCoreUIEvents.cpp
// SuspenseCore - UI EventBus Events Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Events/UI/SuspenseCoreUIEvents.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Services/SuspenseCoreServiceProvider.h"

//==================================================================
// Native Gameplay Tags Definition
//==================================================================

// UI Provider Events
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIProvider, "SuspenseCore.Event.UIProvider");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIProvider_Registered, "SuspenseCore.Event.UIProvider.Registered");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIProvider_Unregistered, "SuspenseCore.Event.UIProvider.Unregistered");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIProvider_DataChanged, "SuspenseCore.Event.UIProvider.DataChanged");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIProvider_DataChanged_Slot, "SuspenseCore.Event.UIProvider.DataChanged.Slot");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIProvider_BindRequested, "SuspenseCore.Event.UIProvider.BindRequested");

// UI Provider Types
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_UIProvider_Type, "SuspenseCore.UIProvider.Type");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_UIProvider_Type_Inventory, "SuspenseCore.UIProvider.Type.Inventory");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_UIProvider_Type_Equipment, "SuspenseCore.UIProvider.Type.Equipment");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_UIProvider_Type_Stash, "SuspenseCore.UIProvider.Type.Stash");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_UIProvider_Type_Trader, "SuspenseCore.UIProvider.Type.Trader");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_UIProvider_Type_Loot, "SuspenseCore.UIProvider.Type.Loot");

// UI Container Events
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIContainer, "SuspenseCore.Event.UIContainer");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIContainer_Opened, "SuspenseCore.Event.UIContainer.Opened");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIContainer_Closed, "SuspenseCore.Event.UIContainer.Closed");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIContainer_SlotClicked, "SuspenseCore.Event.UIContainer.SlotClicked");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIContainer_SlotHovered, "SuspenseCore.Event.UIContainer.SlotHovered");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIContainer_SlotRightClicked, "SuspenseCore.Event.UIContainer.SlotRightClicked");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIContainer_DragStarted, "SuspenseCore.Event.UIContainer.DragStarted");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIContainer_DragEnded, "SuspenseCore.Event.UIContainer.DragEnded");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIContainer_DropCompleted, "SuspenseCore.Event.UIContainer.DropCompleted");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIContainer_DropFailed, "SuspenseCore.Event.UIContainer.DropFailed");

// UI Feedback Events
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIFeedback, "SuspenseCore.Event.UIFeedback");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIFeedback_Success, "SuspenseCore.Event.UIFeedback.Success");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIFeedback_Error, "SuspenseCore.Event.UIFeedback.Error");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIFeedback_Warning, "SuspenseCore.Event.UIFeedback.Warning");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIFeedback_ItemPickedUp, "SuspenseCore.Event.UIFeedback.ItemPickedUp");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIFeedback_InventoryFull, "SuspenseCore.Event.UIFeedback.InventoryFull");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIFeedback_WeightExceeded, "SuspenseCore.Event.UIFeedback.WeightExceeded");

// UI Request Events
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIRequest, "SuspenseCore.Event.UIRequest");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIRequest_MoveItem, "SuspenseCore.Event.UIRequest.MoveItem");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIRequest_RotateItem, "SuspenseCore.Event.UIRequest.RotateItem");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIRequest_UseItem, "SuspenseCore.Event.UIRequest.UseItem");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIRequest_DropItem, "SuspenseCore.Event.UIRequest.DropItem");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIRequest_SplitStack, "SuspenseCore.Event.UIRequest.SplitStack");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIRequest_TransferItem, "SuspenseCore.Event.UIRequest.TransferItem");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIRequest_EquipItem, "SuspenseCore.Event.UIRequest.EquipItem");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIRequest_UnequipItem, "SuspenseCore.Event.UIRequest.UnequipItem");

// UI Panel Events
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIPanel, "SuspenseCore.Event.UIPanel");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIPanel_Inventory, "SuspenseCore.Event.UIPanel.Inventory");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIPanel_Equipment, "SuspenseCore.Event.UIPanel.Equipment");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIPanel_Stash, "SuspenseCore.Event.UIPanel.Stash");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Event_UIPanel_Trader, "SuspenseCore.Event.UIPanel.Trader");

// Container Type Tags (for Drag-Drop source/target identification)
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Container, "SuspenseCore.Container");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Container_Inventory, "SuspenseCore.Container.Inventory");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Container_Equipment, "SuspenseCore.Container.Equipment");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Container_Stash, "SuspenseCore.Container.Stash");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Container_Trader, "SuspenseCore.Container.Trader");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_Container_Loot, "SuspenseCore.Container.Loot");

// Context Menu Actions
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_UIAction, "SuspenseCore.UIAction");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_UIAction_Use, "SuspenseCore.UIAction.Use");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_UIAction_Equip, "SuspenseCore.UIAction.Equip");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_UIAction_Unequip, "SuspenseCore.UIAction.Unequip");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_UIAction_Drop, "SuspenseCore.UIAction.Drop");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_UIAction_Split, "SuspenseCore.UIAction.Split");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_UIAction_Examine, "SuspenseCore.UIAction.Examine");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_UIAction_Discard, "SuspenseCore.UIAction.Discard");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_UIAction_Sell, "SuspenseCore.UIAction.Sell");
UE_DEFINE_GAMEPLAY_TAG(TAG_SuspenseCore_UIAction_Buy, "SuspenseCore.UIAction.Buy");

//==================================================================
// USuspenseCoreUIEventHelpers Implementation
//==================================================================

FGameplayTag USuspenseCoreUIEventHelpers::GetContainerTypeTag(ESuspenseCoreContainerType ContainerType)
{
	switch (ContainerType)
	{
	case ESuspenseCoreContainerType::Inventory:
		return TAG_SuspenseCore_UIProvider_Type_Inventory;
	case ESuspenseCoreContainerType::Equipment:
		return TAG_SuspenseCore_UIProvider_Type_Equipment;
	case ESuspenseCoreContainerType::Stash:
		return TAG_SuspenseCore_UIProvider_Type_Stash;
	case ESuspenseCoreContainerType::Trader:
		return TAG_SuspenseCore_UIProvider_Type_Trader;
	case ESuspenseCoreContainerType::Loot:
		return TAG_SuspenseCore_UIProvider_Type_Loot;
	default:
		return TAG_SuspenseCore_UIProvider_Type;
	}
}

ESuspenseCoreContainerType USuspenseCoreUIEventHelpers::GetContainerTypeFromTag(const FGameplayTag& Tag)
{
	if (Tag == TAG_SuspenseCore_UIProvider_Type_Inventory)
	{
		return ESuspenseCoreContainerType::Inventory;
	}
	if (Tag == TAG_SuspenseCore_UIProvider_Type_Equipment)
	{
		return ESuspenseCoreContainerType::Equipment;
	}
	if (Tag == TAG_SuspenseCore_UIProvider_Type_Stash)
	{
		return ESuspenseCoreContainerType::Stash;
	}
	if (Tag == TAG_SuspenseCore_UIProvider_Type_Trader)
	{
		return ESuspenseCoreContainerType::Trader;
	}
	if (Tag == TAG_SuspenseCore_UIProvider_Type_Loot)
	{
		return ESuspenseCoreContainerType::Loot;
	}
	return ESuspenseCoreContainerType::None;
}

FGameplayTag USuspenseCoreUIEventHelpers::GetFeedbackTypeTag(ESuspenseCoreUIFeedbackType FeedbackType)
{
	switch (FeedbackType)
	{
	case ESuspenseCoreUIFeedbackType::Success:
		return TAG_SuspenseCore_Event_UIFeedback_Success;
	case ESuspenseCoreUIFeedbackType::Error:
		return TAG_SuspenseCore_Event_UIFeedback_Error;
	case ESuspenseCoreUIFeedbackType::Warning:
		return TAG_SuspenseCore_Event_UIFeedback_Warning;
	case ESuspenseCoreUIFeedbackType::ItemPickedUp:
		return TAG_SuspenseCore_Event_UIFeedback_ItemPickedUp;
	case ESuspenseCoreUIFeedbackType::InventoryFull:
		return TAG_SuspenseCore_Event_UIFeedback_InventoryFull;
	case ESuspenseCoreUIFeedbackType::WeightExceeded:
		return TAG_SuspenseCore_Event_UIFeedback_WeightExceeded;
	default:
		return TAG_SuspenseCore_Event_UIFeedback;
	}
}

void USuspenseCoreUIEventHelpers::BroadcastUIRequest(
	const UObject* WorldContext,
	const FGameplayTag& RequestTag,
	const FSuspenseCoreUIEventPayload& Payload)
{
	if (!WorldContext)
	{
		return;
	}

	USuspenseCoreServiceProvider* ServiceProvider = USuspenseCoreServiceProvider::Get(WorldContext);
	if (!ServiceProvider)
	{
		return;
	}

	USuspenseCoreEventBus* EventBus = ServiceProvider->GetEventBus();
	if (!EventBus)
	{
		return;
	}

	// Create event data from payload
	FSuspenseCoreEventData EventData;
	EventData.SetString(FName("SourceContainerID"), Payload.SourceContainerID.ToString());
	EventData.SetInt(FName("SourceSlot"), Payload.SourceSlot);
	EventData.SetString(FName("TargetContainerID"), Payload.TargetContainerID.ToString());
	EventData.SetInt(FName("TargetSlot"), Payload.TargetSlot);
	EventData.SetString(FName("ItemInstanceID"), Payload.ItemInstanceID.ToString());
	EventData.SetInt(FName("Quantity"), Payload.Quantity);
	EventData.SetBool(FName("IsRotated"), Payload.bIsRotated);

	EventBus->Publish(RequestTag, EventData);
}

void USuspenseCoreUIEventHelpers::BroadcastUIFeedback(
	const UObject* WorldContext,
	ESuspenseCoreUIFeedbackType FeedbackType,
	const FText& Message,
	const FGuid& RelatedItemID)
{
	if (!WorldContext)
	{
		return;
	}

	USuspenseCoreServiceProvider* ServiceProvider = USuspenseCoreServiceProvider::Get(WorldContext);
	if (!ServiceProvider)
	{
		return;
	}

	USuspenseCoreEventBus* EventBus = ServiceProvider->GetEventBus();
	if (!EventBus)
	{
		return;
	}

	FGameplayTag FeedbackTag = GetFeedbackTypeTag(FeedbackType);

	FSuspenseCoreEventData EventData;
	EventData.SetString(FName("Message"), Message.ToString());
	EventData.SetString(FName("ItemInstanceID"), RelatedItemID.ToString());
	EventData.SetInt(FName("FeedbackType"), static_cast<int32>(FeedbackType));

	EventBus->Publish(FeedbackTag, EventData);
}
