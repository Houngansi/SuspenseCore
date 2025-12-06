// SuspenseCoreBaseContainerWidget.cpp
// SuspenseCore - Base Container Widget Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/Base/SuspenseCoreBaseContainerWidget.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreUIDataProvider.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Services/SuspenseCoreServiceProvider.h"

//==================================================================
// Constructor
//==================================================================

USuspenseCoreBaseContainerWidget::USuspenseCoreBaseContainerWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, SelectedSlotIndex(INDEX_NONE)
	, bIsReadOnly(false)
{
}

//==================================================================
// UUserWidget Interface
//==================================================================

void USuspenseCoreBaseContainerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Cache EventBus reference
	if (USuspenseCoreServiceProvider* ServiceProvider = USuspenseCoreServiceProvider::Get(this))
	{
		CachedEventBus = ServiceProvider->GetEventBus();
	}
}

void USuspenseCoreBaseContainerWidget::NativeDestruct()
{
	// Unbind from provider on destruction
	UnbindFromProvider();

	Super::NativeDestruct();
}

void USuspenseCoreBaseContainerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
}

//==================================================================
// ISuspenseCoreUIContainer - Provider Binding
//==================================================================

void USuspenseCoreBaseContainerWidget::BindToProvider(TScriptInterface<ISuspenseCoreUIDataProvider> Provider)
{
	// Unbind existing provider first
	if (BoundProvider)
	{
		UnbindFromProvider();
	}

	if (!Provider)
	{
		return;
	}

	BoundProvider = Provider;

	// Subscribe to provider data changes
	if (ISuspenseCoreUIDataProvider* ProviderInterface = BoundProvider.GetInterface())
	{
		ProviderInterface->OnUIDataChanged().AddUObject(this, &USuspenseCoreBaseContainerWidget::OnProviderDataChanged);
	}

	// Initial refresh
	RefreshFromProvider();

	// Notify Blueprint
	K2_OnProviderBound();
}

void USuspenseCoreBaseContainerWidget::UnbindFromProvider()
{
	if (!BoundProvider)
	{
		return;
	}

	// Unsubscribe from provider events
	if (ISuspenseCoreUIDataProvider* ProviderInterface = BoundProvider.GetInterface())
	{
		ProviderInterface->OnUIDataChanged().RemoveAll(this);
	}

	// Clear widgets
	ClearSlotWidgets();

	// Clear cached data
	CachedContainerData = FSuspenseCoreContainerUIData();
	SelectedSlotIndex = INDEX_NONE;

	BoundProvider = nullptr;

	// Notify Blueprint
	K2_OnProviderUnbound();
}

bool USuspenseCoreBaseContainerWidget::IsBoundToProvider() const
{
	return BoundProvider != nullptr;
}

TScriptInterface<ISuspenseCoreUIDataProvider> USuspenseCoreBaseContainerWidget::GetBoundProvider() const
{
	return BoundProvider;
}

//==================================================================
// ISuspenseCoreUIContainer - Refresh
//==================================================================

void USuspenseCoreBaseContainerWidget::RefreshFromProvider()
{
	if (!BoundProvider)
	{
		return;
	}

	ISuspenseCoreUIDataProvider* ProviderInterface = BoundProvider.GetInterface();
	if (!ProviderInterface)
	{
		return;
	}

	// Get container data from provider
	CachedContainerData = ProviderInterface->GetContainerUIData();

	// Clear and recreate slot widgets if slot count changed
	TArray<UWidget*> CurrentSlots = GetAllSlotWidgets();
	if (CurrentSlots.Num() != CachedContainerData.TotalSlots)
	{
		ClearSlotWidgets();
		CreateSlotWidgets();
	}

	// Get all item data
	TArray<FSuspenseCoreItemUIData> ItemsData = ProviderInterface->GetAllItemUIData();

	// Create lookup map for items by anchor slot
	TMap<int32, FSuspenseCoreItemUIData> ItemsBySlot;
	for (const FSuspenseCoreItemUIData& ItemData : ItemsData)
	{
		// For grid items, they may occupy multiple slots - store in anchor slot
		ItemsBySlot.Add(ItemData.AnchorSlot, ItemData);
	}

	// Update each slot
	for (int32 CurrentSlotIndex = 0; CurrentSlotIndex < CachedContainerData.TotalSlots; ++CurrentSlotIndex)
	{
		// Find slot data
		FSuspenseCoreSlotUIData SlotData;
		if (const FSuspenseCoreSlotUIData* FoundSlot = CachedContainerData.Slots.FindByPredicate(
			[CurrentSlotIndex](const FSuspenseCoreSlotUIData& SlotEntry) { return SlotEntry.SlotIndex == CurrentSlotIndex; }))
		{
			SlotData = *FoundSlot;
		}
		else
		{
			SlotData.SlotIndex = CurrentSlotIndex;
			SlotData.State = ESuspenseCoreUISlotState::Empty;
		}

		// Find item data if slot is occupied
		FSuspenseCoreItemUIData ItemData;
		if (const FSuspenseCoreItemUIData* FoundItem = ItemsBySlot.Find(CurrentSlotIndex))
		{
			ItemData = *FoundItem;
		}

		UpdateSlotWidget(CurrentSlotIndex, SlotData, ItemData);
	}

	// Notify Blueprint
	K2_OnRefresh();
}

void USuspenseCoreBaseContainerWidget::RefreshSlot(int32 TargetSlotIndex)
{
	if (!BoundProvider || TargetSlotIndex < 0 || TargetSlotIndex >= CachedContainerData.TotalSlots)
	{
		return;
	}

	ISuspenseCoreUIDataProvider* ProviderInterface = BoundProvider.GetInterface();
	if (!ProviderInterface)
	{
		return;
	}

	// Get specific item data from provider
	FSuspenseCoreItemUIData ItemData;
	ProviderInterface->GetItemUIDataAtSlot(TargetSlotIndex, ItemData);

	// Get slot data
	FSuspenseCoreSlotUIData SlotData;
	if (const FSuspenseCoreSlotUIData* FoundSlot = CachedContainerData.Slots.FindByPredicate(
		[TargetSlotIndex](const FSuspenseCoreSlotUIData& SlotEntry) { return SlotEntry.SlotIndex == TargetSlotIndex; }))
	{
		SlotData = *FoundSlot;
	}
	else
	{
		SlotData.SlotIndex = TargetSlotIndex;
		SlotData.State = ItemData.InstanceID.IsValid() ? ESuspenseCoreUISlotState::Occupied : ESuspenseCoreUISlotState::Empty;
	}

	UpdateSlotWidget(TargetSlotIndex, SlotData, ItemData);
}

void USuspenseCoreBaseContainerWidget::RefreshItem(const FGuid& ItemInstanceID)
{
	if (!BoundProvider || !ItemInstanceID.IsValid())
	{
		return;
	}

	ISuspenseCoreUIDataProvider* ProviderInterface = BoundProvider.GetInterface();
	if (!ProviderInterface)
	{
		return;
	}

	// Find item by instance ID
	TArray<FSuspenseCoreItemUIData> AllItems = ProviderInterface->GetAllItemUIData();
	const FSuspenseCoreItemUIData* FoundItem = AllItems.FindByPredicate(
		[&ItemInstanceID](const FSuspenseCoreItemUIData& Item) { return Item.InstanceID == ItemInstanceID; });

	if (FoundItem)
	{
		RefreshSlot(FoundItem->AnchorSlot);
	}
}

//==================================================================
// ISuspenseCoreUIContainer - Slot Access (Virtual - override in subclass)
//==================================================================

UWidget* USuspenseCoreBaseContainerWidget::GetSlotWidget(int32 SlotIndex) const
{
	// Default implementation - override in subclass
	return nullptr;
}

TArray<UWidget*> USuspenseCoreBaseContainerWidget::GetAllSlotWidgets() const
{
	// Default implementation - override in subclass
	return TArray<UWidget*>();
}

int32 USuspenseCoreBaseContainerWidget::GetSlotAtPosition(const FVector2D& ScreenPosition) const
{
	// Convert screen to local and delegate
	FVector2D LocalPosition;
	if (GetCachedGeometry().IsUnderLocation(FVector2D(ScreenPosition)))
	{
		LocalPosition = GetCachedGeometry().AbsoluteToLocal(ScreenPosition);
		return GetSlotAtLocalPosition(LocalPosition);
	}
	return INDEX_NONE;
}

int32 USuspenseCoreBaseContainerWidget::GetSlotAtLocalPosition(const FVector2D& LocalPosition) const
{
	// Default implementation - override in subclass
	return INDEX_NONE;
}

//==================================================================
// ISuspenseCoreUIContainer - Selection
//==================================================================

void USuspenseCoreBaseContainerWidget::SetSelectedSlot(int32 NewSelectedSlot)
{
	if (SelectedSlotIndex == NewSelectedSlot)
	{
		return;
	}

	// Clear previous selection highlight
	if (SelectedSlotIndex != INDEX_NONE)
	{
		SetSlotHighlight(SelectedSlotIndex, ESuspenseCoreUISlotState::Empty);
	}

	SelectedSlotIndex = NewSelectedSlot;

	// Apply new selection highlight
	if (SelectedSlotIndex != INDEX_NONE)
	{
		SetSlotHighlight(SelectedSlotIndex, ESuspenseCoreUISlotState::Selected);
	}

	// Notify Blueprint
	K2_OnSlotSelected(NewSelectedSlot);
}

int32 USuspenseCoreBaseContainerWidget::GetSelectedSlot() const
{
	return SelectedSlotIndex;
}

void USuspenseCoreBaseContainerWidget::ClearSelection()
{
	SetSelectedSlot(INDEX_NONE);
}

//==================================================================
// ISuspenseCoreUIContainer - Highlighting (Virtual - override in subclass)
//==================================================================

void USuspenseCoreBaseContainerWidget::SetSlotHighlight(int32 SlotIndex, ESuspenseCoreUISlotState State)
{
	// Default implementation - override in subclass
}

void USuspenseCoreBaseContainerWidget::HighlightDropTarget(const FSuspenseCoreDragData& DragData, int32 HoverSlot)
{
	if (!BoundProvider)
	{
		return;
	}

	ISuspenseCoreUIDataProvider* ProviderInterface = BoundProvider.GetInterface();
	if (!ProviderInterface)
	{
		return;
	}

	// Clear previous highlights first
	ClearHighlights();

	if (HoverSlot == INDEX_NONE)
	{
		return;
	}

	// Validate drop at this position
	FSuspenseCoreDropValidation Validation = ProviderInterface->ValidateDrop(DragData, HoverSlot, DragData.bIsRotatedDuringDrag);

	// Set appropriate highlight based on validation
	ESuspenseCoreUISlotState HighlightState = Validation.bIsValid ?
		ESuspenseCoreUISlotState::DropTargetValid : ESuspenseCoreUISlotState::DropTargetInvalid;

	// Highlight target slot (for simple containers, just highlight the hover slot)
	// Subclasses can override to highlight multi-cell items
	SetSlotHighlight(HoverSlot, HighlightState);
}

void USuspenseCoreBaseContainerWidget::ClearHighlights()
{
	// Reset all slots to empty/default state (except selected)
	TArray<UWidget*> AllSlots = GetAllSlotWidgets();
	for (int32 SlotIdx = 0; SlotIdx < AllSlots.Num(); ++SlotIdx)
	{
		if (SlotIdx != SelectedSlotIndex)
		{
			SetSlotHighlight(SlotIdx, ESuspenseCoreUISlotState::Empty);
		}
	}
}

//==================================================================
// ISuspenseCoreUIContainer - Drag-Drop
//==================================================================

bool USuspenseCoreBaseContainerWidget::AcceptsDrop() const
{
	return !bIsReadOnly;
}

bool USuspenseCoreBaseContainerWidget::StartDragFromSlot(int32 DragSlotIndex, bool bSplitStack)
{
	if (bIsReadOnly || !BoundProvider)
	{
		return false;
	}

	ISuspenseCoreUIDataProvider* ProviderInterface = BoundProvider.GetInterface();
	if (!ProviderInterface)
	{
		return false;
	}

	// Get item data at slot
	FSuspenseCoreItemUIData ItemData;
	ProviderInterface->GetItemUIDataAtSlot(DragSlotIndex, ItemData);
	if (!ItemData.InstanceID.IsValid())
	{
		return false;
	}

	// Drag will be started by the drag-drop system
	// This just validates that we CAN drag from this slot
	return true;
}

bool USuspenseCoreBaseContainerWidget::HandleDrop(const FSuspenseCoreDragData& DragData, int32 TargetSlot)
{
	if (bIsReadOnly || !BoundProvider)
	{
		K2_OnDropReceived(DragData, TargetSlot, false);
		return false;
	}

	ISuspenseCoreUIDataProvider* ProviderInterface = BoundProvider.GetInterface();
	if (!ProviderInterface)
	{
		K2_OnDropReceived(DragData, TargetSlot, false);
		return false;
	}

	// Validate the drop first
	FSuspenseCoreDropValidation Validation = ProviderInterface->ValidateDrop(DragData, TargetSlot, DragData.bIsRotatedDuringDrag);
	if (!Validation.bIsValid)
	{
		K2_OnDropReceived(DragData, TargetSlot, false);
		return false;
	}

	// Check if this is same container or cross-container transfer
	bool bSuccess = false;
	if (DragData.SourceContainerID == ProviderInterface->GetProviderID())
	{
		// Same container - move item
		bSuccess = ProviderInterface->RequestMoveItem(DragData.SourceSlot, TargetSlot, DragData.bIsRotatedDuringDrag);
	}
	else
	{
		// Cross-container transfer - request transfer through EventBus
		// The UIManager will handle finding source provider and coordinating the transfer
		if (USuspenseCoreEventBus* EventBus = GetEventBus())
		{
			FSuspenseCoreEventData EventData;
			EventData.SetString(FName("SourceContainerID"), DragData.SourceContainerID.ToString());
			EventData.SetInt(FName("SourceSlot"), DragData.SourceSlot);
			EventData.SetString(FName("TargetContainerID"), ProviderInterface->GetProviderID().ToString());
			EventData.SetInt(FName("TargetSlot"), TargetSlot);
			EventData.SetString(FName("ItemInstanceID"), DragData.Item.InstanceID.ToString());
			EventData.SetInt(FName("Quantity"), DragData.DragQuantity);
			EventData.SetBool(FName("IsRotated"), DragData.bIsRotatedDuringDrag);

			EventBus->Publish(FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UIRequest.TransferItem")), EventData);
			bSuccess = true; // Assume success - actual result comes async
		}
	}

	K2_OnDropReceived(DragData, TargetSlot, bSuccess);
	return bSuccess;
}

void USuspenseCoreBaseContainerWidget::HandleDragCancelled()
{
	ClearHighlights();
}

//==================================================================
// ISuspenseCoreUIContainer - Configuration
//==================================================================

ESuspenseCoreContainerType USuspenseCoreBaseContainerWidget::GetContainerType() const
{
	if (BoundProvider)
	{
		if (ISuspenseCoreUIDataProvider* ProviderInterface = BoundProvider.GetInterface())
		{
			return ProviderInterface->GetContainerType();
		}
	}
	return ESuspenseCoreContainerType::None;
}

FGameplayTag USuspenseCoreBaseContainerWidget::GetContainerTypeTag() const
{
	ESuspenseCoreContainerType Type = GetContainerType();
	// Convert enum to tag - implementation depends on your tag system
	return FGameplayTag();
}

bool USuspenseCoreBaseContainerWidget::IsReadOnly() const
{
	return bIsReadOnly;
}

void USuspenseCoreBaseContainerWidget::SetReadOnly(bool bReadOnly)
{
	bIsReadOnly = bReadOnly;
}

//==================================================================
// ISuspenseCoreUIContainer - Tooltip (Virtual - override in subclass)
//==================================================================

void USuspenseCoreBaseContainerWidget::ShowSlotTooltip(int32 SlotIndex)
{
	// Default implementation - override in subclass or handle via UIManager
}

void USuspenseCoreBaseContainerWidget::HideTooltip()
{
	// Default implementation - override in subclass or handle via UIManager
}

//==================================================================
// ISuspenseCoreUIContainer - Context Menu (Virtual - override in subclass)
//==================================================================

void USuspenseCoreBaseContainerWidget::ShowContextMenu(int32 SlotIndex, const FVector2D& ScreenPosition)
{
	// Default implementation - override in subclass or handle via UIManager
}

void USuspenseCoreBaseContainerWidget::HideContextMenu()
{
	// Default implementation - override in subclass or handle via UIManager
}

//==================================================================
// Provider Event Handlers
//==================================================================

void USuspenseCoreBaseContainerWidget::OnProviderDataChanged(const FGameplayTag& ChangeType, const FGuid& AffectedItemID)
{
	// Handle different change types
	FName TagName = ChangeType.GetTagName();

	if (TagName == FName("SuspenseCore.Event.UIProvider.DataChanged.Full"))
	{
		// Full refresh needed
		RefreshFromProvider();
	}
	else if (TagName == FName("SuspenseCore.Event.UIProvider.DataChanged.Slot"))
	{
		// Specific item changed - find its slot and refresh
		RefreshItem(AffectedItemID);
	}
	else
	{
		// Default to full refresh for unknown change types
		RefreshFromProvider();
	}
}

//==================================================================
// Protected Accessors
//==================================================================

USuspenseCoreEventBus* USuspenseCoreBaseContainerWidget::GetEventBus() const
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	if (USuspenseCoreServiceProvider* ServiceProvider = USuspenseCoreServiceProvider::Get(this))
	{
		return ServiceProvider->GetEventBus();
	}

	return nullptr;
}
