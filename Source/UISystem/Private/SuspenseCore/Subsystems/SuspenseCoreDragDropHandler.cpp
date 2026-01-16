// SuspenseCoreDragDropHandler.cpp
// SuspenseCore - Centralized Drag-Drop Handler Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Subsystems/SuspenseCoreDragDropHandler.h"
#include "SuspenseCore/Subsystems/SuspenseCoreUIManager.h"
#include "SuspenseCore/Widgets/DragDrop/SuspenseCoreDragDropOperation.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreUIContainer.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreUIDataProvider.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/UI/SuspenseCoreUIEvents.h"
#include "Blueprint/UserWidget.h"
#include "Components/Widget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Layout/Geometry.h"

//==================================================================
// Static Access
//==================================================================

USuspenseCoreDragDropHandler* USuspenseCoreDragDropHandler::Get(const UObject* WorldContext)
{
	if (!WorldContext)
	{
		return nullptr;
	}

	UWorld* World = WorldContext->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		return nullptr;
	}

	return GI->GetSubsystem<USuspenseCoreDragDropHandler>();
}

//==================================================================
// Lifecycle
//==================================================================

void USuspenseCoreDragDropHandler::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Initialize default config
	SmartDropConfig = FSuspenseCoreSmartDropConfig();

	UE_LOG(LogTemp, Log, TEXT("USuspenseCoreDragDropHandler: Initialized"));
}

void USuspenseCoreDragDropHandler::Deinitialize()
{
	// Cleanup
	ActiveOperation.Reset();
	HighlightedContainer.Reset();
	CurrentHighlightedSlots.Empty();
	CachedEventBus.Reset();

	Super::Deinitialize();

	UE_LOG(LogTemp, Log, TEXT("USuspenseCoreDragDropHandler: Deinitialized"));
}

//==================================================================
// Core Drag-Drop Operations
//==================================================================

USuspenseCoreDragDropOperation* USuspenseCoreDragDropHandler::StartDragOperation(
	TScriptInterface<ISuspenseCoreUIContainer> SourceContainer,
	int32 SourceSlot,
	const FPointerEvent& MouseEvent)
{
	if (!SourceContainer.GetInterface())
	{
		UE_LOG(LogTemp, Warning, TEXT("StartDragOperation: Invalid source container"));
		return nullptr;
	}

	// Get provider from container
	TScriptInterface<ISuspenseCoreUIDataProvider> Provider = SourceContainer->GetBoundProvider();
	if (!Provider.GetInterface())
	{
		UE_LOG(LogTemp, Warning, TEXT("StartDragOperation: Container has no bound provider"));
		return nullptr;
	}

	// Get item data at slot
	FSuspenseCoreItemUIData ItemData;
	if (!Provider->GetItemUIDataAtSlot(SourceSlot, ItemData))
	{
		UE_LOG(LogTemp, Verbose, TEXT("StartDragOperation: No item at slot %d"), SourceSlot);
		return nullptr;
	}

	// Create drag data
	FSuspenseCoreDragData DragData = FSuspenseCoreDragData::Create(
		ItemData,
		Provider->GetContainerType(),
		Provider->GetContainerTypeTag(),
		Provider->GetProviderID(),
		SourceSlot
	);

	// Calculate DragOffset for proper visual positioning
	// DragOffset = SlotAbsolutePos - CursorAbsolutePos
	// This ensures the drag visual appears at the slot's position, not at cursor
	FVector2D CursorAbsolutePos = MouseEvent.GetScreenSpacePosition();
	FVector2D SlotAbsolutePos = CursorAbsolutePos; // Default to cursor if slot not found

	// Get slot widget from container to calculate absolute position
	UWidget* SlotWidget = SourceContainer->GetSlotWidget(SourceSlot);
	if (SlotWidget)
	{
		FGeometry SlotGeometry = SlotWidget->GetCachedGeometry();
		SlotAbsolutePos = SlotGeometry.GetAbsolutePosition();
	}

	// Offset is negative: from cursor to slot top-left
	DragData.DragOffset = SlotAbsolutePos - CursorAbsolutePos;

	if (!DragData.bIsValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("StartDragOperation: Failed to create drag data"));
		return nullptr;
	}

	// Get owning player controller
	UWidget* WidgetObj = Cast<UWidget>(SourceContainer.GetObject());
	APlayerController* PC = nullptr;
	if (WidgetObj)
	{
		PC = WidgetObj->GetOwningPlayer();
	}

	// Create drag-drop operation
	USuspenseCoreDragDropOperation* DragOperation = USuspenseCoreDragDropOperation::CreateDrag(
		PC,
		DragData,
		nullptr // Use default visual widget class
	);

	if (DragOperation)
	{
		ActiveOperation = DragOperation;

		// Publish drag start feedback via EventBus
		PublishDragStartFeedback(DragData);

		UE_LOG(LogTemp, Log, TEXT("StartDragOperation: Started drag for item '%s' from slot %d"),
			*ItemData.DisplayName.ToString(), SourceSlot);
	}

	return DragOperation;
}

FSuspenseCoreDropResult USuspenseCoreDragDropHandler::ProcessDrop(
	USuspenseCoreDragDropOperation* DragOperation,
	TScriptInterface<ISuspenseCoreUIContainer> TargetContainer,
	int32 TargetSlot)
{
	if (!DragOperation)
	{
		return FSuspenseCoreDropResult::Failure(
			NSLOCTEXT("SuspenseCore", "InvalidDragOp", "Invalid drag operation"));
	}

	if (!TargetContainer.GetInterface())
	{
		return FSuspenseCoreDropResult::Failure(
			NSLOCTEXT("SuspenseCore", "InvalidTarget", "Invalid drop target"));
	}

	// Get target provider
	TScriptInterface<ISuspenseCoreUIDataProvider> TargetProvider = TargetContainer->GetBoundProvider();
	if (!TargetProvider.GetInterface())
	{
		return FSuspenseCoreDropResult::Failure(
			NSLOCTEXT("SuspenseCore", "NoTargetProvider", "Target has no provider"));
	}

	// Build drop request
	const FSuspenseCoreDragData& DragData = DragOperation->GetDragData();

	FSuspenseCoreDropRequest Request;
	Request.SourceContainerTag = DragData.SourceContainerTag;
	Request.TargetContainerTag = TargetProvider->GetContainerTypeTag();
	Request.SourceProviderID = DragData.SourceContainerID;
	Request.TargetProviderID = TargetProvider->GetProviderID();
	Request.SourceSlot = DragData.SourceSlot;
	Request.TargetSlot = TargetSlot;
	Request.DragData = DragData;

	// Process the request
	FSuspenseCoreDropResult Result = ProcessDropRequest(Request);

	// Publish feedback via EventBus (AAA-level responsiveness)
	PublishDropFeedback(Result, Request);

	// Clear active operation
	if (ActiveOperation.Get() == DragOperation)
	{
		ActiveOperation.Reset();
	}

	// Clear highlights
	ClearAllHighlights();

	return Result;
}

FSuspenseCoreDropResult USuspenseCoreDragDropHandler::ProcessDropRequest(const FSuspenseCoreDropRequest& Request)
{
	// Route the operation
	return RouteDropOperation(Request);
}

void USuspenseCoreDragDropHandler::CancelDragOperation()
{
	if (ActiveOperation.IsValid())
	{
		// Publish cancel feedback via EventBus
		PublishDragCancelFeedback();

		// Notify operation cancelled
		// (The operation itself handles visual cleanup)
		ActiveOperation.Reset();
	}

	ClearAllHighlights();

	UE_LOG(LogTemp, Log, TEXT("CancelDragOperation: Drag cancelled"));
}

//==================================================================
// Drop Target Calculation
//==================================================================

FSuspenseCoreDropTargetInfo USuspenseCoreDragDropHandler::CalculateDropTarget(
	const FVector2D& ScreenPosition,
	const FIntPoint& ItemSize,
	bool bIsRotated) const
{
	FSuspenseCoreDropTargetInfo Result;

	// TODO: Implement finding container at screen position
	// This would need to query all registered containers
	// For now, return empty result - containers handle their own hit testing

	return Result;
}

FSuspenseCoreDropTargetInfo USuspenseCoreDragDropHandler::FindBestDropTarget(
	const FVector2D& ScreenPosition,
	const FIntPoint& ItemSize,
	bool bIsRotated) const
{
	FSuspenseCoreDropTargetInfo Result;

	if (!SmartDropConfig.bEnableSmartDrop)
	{
		return CalculateDropTarget(ScreenPosition, ItemSize, bIsRotated);
	}

	// Get base target
	Result = CalculateDropTarget(ScreenPosition, ItemSize, bIsRotated);

	if (Result.bIsValid)
	{
		return Result;
	}

	// TODO: Search nearby slots within DetectionRadius for valid placement
	// This is a more complex feature that would need container registration

	return Result;
}

//==================================================================
// Visual Feedback
//==================================================================

void USuspenseCoreDragDropHandler::UpdateDragVisual(
	USuspenseCoreDragDropOperation* DragOperation,
	bool bIsValidTarget)
{
	if (!DragOperation)
	{
		return;
	}

	DragOperation->UpdateDropValidity(bIsValidTarget);
}

void USuspenseCoreDragDropHandler::HighlightDropSlots(
	TScriptInterface<ISuspenseCoreUIContainer> Container,
	const TArray<int32>& Slots,
	bool bIsValid)
{
	if (!Container.GetInterface())
	{
		return;
	}

	// Clear previous highlights if different container
	UObject* ContainerObj = Container.GetObject();
	if (HighlightedContainer.Get() != ContainerObj)
	{
		ClearAllHighlights();
		HighlightedContainer = ContainerObj;
	}

	// Clear previous slots in this container
	for (int32 SlotIndex : CurrentHighlightedSlots)
	{
		Container->SetSlotHighlight(SlotIndex, ESuspenseCoreUISlotState::Empty);
	}
	CurrentHighlightedSlots.Empty();

	// Set new highlights
	ESuspenseCoreUISlotState State = bIsValid
		? ESuspenseCoreUISlotState::DropTargetValid
		: ESuspenseCoreUISlotState::DropTargetInvalid;

	for (int32 SlotIndex : Slots)
	{
		Container->SetSlotHighlight(SlotIndex, State);
		CurrentHighlightedSlots.Add(SlotIndex);
	}
}

void USuspenseCoreDragDropHandler::ClearAllHighlights()
{
	if (HighlightedContainer.IsValid())
	{
		ISuspenseCoreUIContainer* Container = Cast<ISuspenseCoreUIContainer>(HighlightedContainer.Get());
		if (Container)
		{
			Container->ClearHighlights();
		}
	}

	HighlightedContainer.Reset();
	CurrentHighlightedSlots.Empty();
}

//==================================================================
// Rotation Support
//==================================================================

bool USuspenseCoreDragDropHandler::ToggleRotation()
{
	if (!ActiveOperation.IsValid())
	{
		return false;
	}

	ActiveOperation->ToggleRotation();
	return ActiveOperation->IsRotated();
}

bool USuspenseCoreDragDropHandler::IsCurrentDragRotated() const
{
	if (!ActiveOperation.IsValid())
	{
		return false;
	}

	return ActiveOperation->IsRotated();
}

//==================================================================
// Drop Routing
//==================================================================

FSuspenseCoreDropResult USuspenseCoreDragDropHandler::RouteDropOperation(const FSuspenseCoreDropRequest& Request)
{
	// Use FGameplayTag::RequestGameplayTag for cross-module compatibility
	static const FGameplayTag InventoryTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.UIProvider.Type.Inventory"));
	static const FGameplayTag EquipmentTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.UIProvider.Type.Equipment"));

	// Route based on source/target types
	bool bSourceIsInventory = Request.SourceContainerTag.MatchesTag(InventoryTag);
	bool bSourceIsEquipment = Request.SourceContainerTag.MatchesTag(EquipmentTag);
	bool bTargetIsInventory = Request.TargetContainerTag.MatchesTag(InventoryTag);
	bool bTargetIsEquipment = Request.TargetContainerTag.MatchesTag(EquipmentTag);

	// Inventory -> Inventory
	if (bSourceIsInventory && bTargetIsInventory)
	{
		return HandleInventoryToInventory(Request);
	}

	// Inventory -> Equipment
	if (bSourceIsInventory && bTargetIsEquipment)
	{
		return HandleInventoryToEquipment(Request);
	}

	// Equipment -> Inventory
	if (bSourceIsEquipment && bTargetIsInventory)
	{
		return HandleEquipmentToInventory(Request);
	}

	// Equipment -> Equipment (swap or move between equipment slots)
	if (bSourceIsEquipment && bTargetIsEquipment)
	{
		return HandleEquipmentToEquipment(Request);
	}

	// Default: Try generic drop
	return ExecuteDrop(Request);
}

FSuspenseCoreDropResult USuspenseCoreDragDropHandler::HandleInventoryToInventory(
	const FSuspenseCoreDropRequest& Request)
{
	// Same container - move item
	if (Request.SourceProviderID == Request.TargetProviderID)
	{
		TScriptInterface<ISuspenseCoreUIDataProvider> Provider = FindProviderByID(Request.SourceProviderID);
		if (!Provider.GetInterface())
		{
			return FSuspenseCoreDropResult::Failure(
				NSLOCTEXT("SuspenseCore", "ProviderNotFound", "Provider not found"));
		}

		bool bRotate = Request.DragData.bIsRotatedDuringDrag;
		bool bSuccess = Provider->RequestMoveItem(Request.SourceSlot, Request.TargetSlot, bRotate);

		if (bSuccess)
		{
			return FSuspenseCoreDropResult::Success();
		}
		else
		{
			return FSuspenseCoreDropResult::Failure(
				NSLOCTEXT("SuspenseCore", "MoveFailed", "Failed to move item"));
		}
	}

	// Different containers - transfer
	return ExecuteDrop(Request);
}

FSuspenseCoreDropResult USuspenseCoreDragDropHandler::HandleInventoryToEquipment(
	const FSuspenseCoreDropRequest& Request)
{
	// Transfer from inventory to equipment slot
	TScriptInterface<ISuspenseCoreUIDataProvider> SourceProvider = FindProviderByID(Request.SourceProviderID);
	if (!SourceProvider.GetInterface())
	{
		return FSuspenseCoreDropResult::Failure(
			NSLOCTEXT("SuspenseCore", "SourceNotFound", "Source provider not found"));
	}

	bool bSuccess = SourceProvider->RequestTransferItem(
		Request.SourceSlot,
		Request.TargetProviderID,
		Request.TargetSlot,
		0 // Transfer all
	);

	if (bSuccess)
	{
		return FSuspenseCoreDropResult::Success(
			NSLOCTEXT("SuspenseCore", "Equipped", "Item equipped"));
	}

	return FSuspenseCoreDropResult::Failure(
		NSLOCTEXT("SuspenseCore", "EquipFailed", "Failed to equip item"));
}

FSuspenseCoreDropResult USuspenseCoreDragDropHandler::HandleEquipmentToInventory(
	const FSuspenseCoreDropRequest& Request)
{
	// Transfer from equipment to inventory
	TScriptInterface<ISuspenseCoreUIDataProvider> SourceProvider = FindProviderByID(Request.SourceProviderID);
	if (!SourceProvider.GetInterface())
	{
		return FSuspenseCoreDropResult::Failure(
			NSLOCTEXT("SuspenseCore", "SourceNotFound", "Source provider not found"));
	}

	bool bSuccess = SourceProvider->RequestTransferItem(
		Request.SourceSlot,
		Request.TargetProviderID,
		Request.TargetSlot,
		0 // Transfer all
	);

	if (bSuccess)
	{
		return FSuspenseCoreDropResult::Success(
			NSLOCTEXT("SuspenseCore", "Unequipped", "Item unequipped"));
	}

	return FSuspenseCoreDropResult::Failure(
		NSLOCTEXT("SuspenseCore", "UnequipFailed", "Failed to unequip item"));
}

FSuspenseCoreDropResult USuspenseCoreDragDropHandler::HandleEquipmentToEquipment(
	const FSuspenseCoreDropRequest& Request)
{
	// Swap or move between equipment slots (same or different providers)
	TScriptInterface<ISuspenseCoreUIDataProvider> SourceProvider = FindProviderByID(Request.SourceProviderID);
	TScriptInterface<ISuspenseCoreUIDataProvider> TargetProvider = FindProviderByID(Request.TargetProviderID);

	if (!SourceProvider.GetInterface())
	{
		return FSuspenseCoreDropResult::Failure(
			NSLOCTEXT("SuspenseCore", "SourceNotFound", "Source equipment provider not found"));
	}

	if (!TargetProvider.GetInterface())
	{
		return FSuspenseCoreDropResult::Failure(
			NSLOCTEXT("SuspenseCore", "TargetNotFound", "Target equipment provider not found"));
	}

	// Validate that target slot can accept this item type
	FSuspenseCoreDropValidation Validation = TargetProvider->ValidateDrop(
		Request.DragData,
		Request.TargetSlot,
		false // Equipment items don't rotate
	);

	if (!Validation.bIsValid)
	{
		return FSuspenseCoreDropResult::Failure(Validation.Reason);
	}

	// Same provider - swap slots
	if (Request.SourceProviderID == Request.TargetProviderID)
	{
		bool bSuccess = SourceProvider->RequestMoveItem(
			Request.SourceSlot,
			Request.TargetSlot,
			false // No rotation for equipment
		);

		if (bSuccess)
		{
			return FSuspenseCoreDropResult::Success(
				NSLOCTEXT("SuspenseCore", "EquipmentSwapped", "Equipment slots swapped"));
		}

		return FSuspenseCoreDropResult::Failure(
			NSLOCTEXT("SuspenseCore", "SwapFailed", "Failed to swap equipment"));
	}

	// Different providers - transfer between characters/loadouts
	bool bSuccess = SourceProvider->RequestTransferItem(
		Request.SourceSlot,
		Request.TargetProviderID,
		Request.TargetSlot,
		0 // Transfer all (equipment is always quantity 1)
	);

	if (bSuccess)
	{
		return FSuspenseCoreDropResult::Success(
			NSLOCTEXT("SuspenseCore", "EquipmentTransferred", "Equipment transferred"));
	}

	return FSuspenseCoreDropResult::Failure(
		NSLOCTEXT("SuspenseCore", "TransferFailed", "Failed to transfer equipment"));
}

FSuspenseCoreDropResult USuspenseCoreDragDropHandler::ExecuteDrop(const FSuspenseCoreDropRequest& Request)
{
	// Generic transfer between containers
	TScriptInterface<ISuspenseCoreUIDataProvider> SourceProvider = FindProviderByID(Request.SourceProviderID);
	TScriptInterface<ISuspenseCoreUIDataProvider> TargetProvider = FindProviderByID(Request.TargetProviderID);

	if (!SourceProvider.GetInterface() || !TargetProvider.GetInterface())
	{
		return FSuspenseCoreDropResult::Failure(
			NSLOCTEXT("SuspenseCore", "ProviderError", "Provider not available"));
	}

	// Validate the drop on target
	FSuspenseCoreDropValidation Validation = TargetProvider->ValidateDrop(
		Request.DragData,
		Request.TargetSlot,
		Request.DragData.bIsRotatedDuringDrag
	);

	if (!Validation.bIsValid)
	{
		return FSuspenseCoreDropResult::Failure(Validation.Reason);
	}

	// Execute the transfer
	bool bSuccess = SourceProvider->RequestTransferItem(
		Request.SourceSlot,
		Request.TargetProviderID,
		Request.TargetSlot,
		Request.DragData.DragQuantity
	);

	if (bSuccess)
	{
		return FSuspenseCoreDropResult::Success();
	}

	return FSuspenseCoreDropResult::Failure(
		NSLOCTEXT("SuspenseCore", "TransferFailed", "Transfer failed"));
}

//==================================================================
// Internal Helpers
//==================================================================

TScriptInterface<ISuspenseCoreUIDataProvider> USuspenseCoreDragDropHandler::FindProviderByID(
	const FGuid& ProviderID) const
{
	// Delegate to UIManager which maintains the provider registry
	USuspenseCoreUIManager* UIManager = USuspenseCoreUIManager::Get(this);
	if (UIManager)
	{
		return UIManager->FindProviderByID(ProviderID);
	}

	UE_LOG(LogTemp, Warning, TEXT("FindProviderByID: UIManager not available"));
	return TScriptInterface<ISuspenseCoreUIDataProvider>();
}

USuspenseCoreEventBus* USuspenseCoreDragDropHandler::GetEventBus() const
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (USuspenseCoreEventManager* EventManager = GI->GetSubsystem<USuspenseCoreEventManager>())
		{
			CachedEventBus = EventManager->GetEventBus();
			return CachedEventBus.Get();
		}
	}

	return nullptr;
}

//==================================================================
// EventBus Feedback (AAA-Level UI Responsiveness)
//==================================================================

void USuspenseCoreDragDropHandler::PublishDropFeedback(
	const FSuspenseCoreDropResult& Result,
	const FSuspenseCoreDropRequest& Request)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	FSuspenseCoreEventData EventData;
	EventData.SetBool(FName("Success"), Result.bSuccess);

	if (!Result.ResultMessage.IsEmpty())
	{
		EventData.SetString(FName("Message"), Result.ResultMessage.ToString());
	}

	// Add context about the operation
	EventData.SetInt(FName("SourceSlot"), Request.SourceSlot);
	EventData.SetInt(FName("TargetSlot"), Request.TargetSlot);

	if (Request.DragData.Item.InstanceID.IsValid())
	{
		EventData.SetString(FName("ItemName"), Request.DragData.Item.DisplayName.ToString());
	}

	// Publish to appropriate tag
	FGameplayTag FeedbackTag = Result.bSuccess
		? TAG_SuspenseCore_Event_UIFeedback_Success
		: TAG_SuspenseCore_Event_UIFeedback_Error;

	EventBus->Publish(FeedbackTag, EventData);

	UE_LOG(LogTemp, Log, TEXT("PublishDropFeedback: %s - %s"),
		Result.bSuccess ? TEXT("SUCCESS") : TEXT("FAILED"),
		*Result.ResultMessage.ToString());
}

void USuspenseCoreDragDropHandler::PublishDragStartFeedback(const FSuspenseCoreDragData& DragData)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	FSuspenseCoreEventData EventData;
	EventData.SetString(FName("ItemName"), DragData.Item.DisplayName.ToString());
	EventData.SetInt(FName("SourceSlot"), DragData.SourceSlot);
	EventData.SetInt(FName("Quantity"), DragData.DragQuantity);

	// Use generic UIFeedback tag for drag start
	EventBus->Publish(TAG_SuspenseCore_Event_UIFeedback, EventData);
}

void USuspenseCoreDragDropHandler::PublishDragCancelFeedback()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	FSuspenseCoreEventData EventData;
	EventData.SetBool(FName("Cancelled"), true);

	EventBus->Publish(TAG_SuspenseCore_Event_UIFeedback, EventData);
}

bool USuspenseCoreDragDropHandler::CalculateOccupiedSlots(
	TScriptInterface<ISuspenseCoreUIContainer> Container,
	int32 AnchorSlot,
	const FIntPoint& ItemSize,
	bool bIsRotated,
	TArray<int32>& OutSlots) const
{
	OutSlots.Empty();

	if (!Container.GetInterface())
	{
		return false;
	}

	TScriptInterface<ISuspenseCoreUIDataProvider> Provider = Container->GetBoundProvider();
	if (!Provider.GetInterface())
	{
		OutSlots.Add(AnchorSlot);
		return true;
	}

	FIntPoint GridSize = Provider->GetGridSize();
	if (GridSize.X <= 0 || GridSize.Y <= 0)
	{
		OutSlots.Add(AnchorSlot);
		return true;
	}

	// Calculate effective size
	FIntPoint EffectiveSize = bIsRotated ? FIntPoint(ItemSize.Y, ItemSize.X) : ItemSize;

	// Get anchor grid position
	int32 AnchorCol = AnchorSlot % GridSize.X;
	int32 AnchorRow = AnchorSlot / GridSize.X;

	// Check bounds
	if (AnchorCol + EffectiveSize.X > GridSize.X || AnchorRow + EffectiveSize.Y > GridSize.Y)
	{
		return false;
	}

	// Calculate all slots
	for (int32 Y = 0; Y < EffectiveSize.Y; ++Y)
	{
		for (int32 X = 0; X < EffectiveSize.X; ++X)
		{
			int32 SlotIndex = (AnchorRow + Y) * GridSize.X + (AnchorCol + X);
			OutSlots.Add(SlotIndex);
		}
	}

	return true;
}
