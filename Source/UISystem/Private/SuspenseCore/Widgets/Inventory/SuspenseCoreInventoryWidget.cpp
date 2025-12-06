// SuspenseCoreInventoryWidget.cpp
// SuspenseCore - Grid-based Inventory Widget Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/Inventory/SuspenseCoreInventoryWidget.h"
#include "SuspenseCore/Widgets/Inventory/SuspenseCoreInventorySlotWidget.h"
#include "SuspenseCore/Subsystems/SuspenseCoreUIManager.h"
#include "SuspenseCore/Widgets/DragDrop/SuspenseCoreDragDropOperation.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreUIDataProvider.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreUIContainer.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/CanvasPanel.h"

//==================================================================
// Constructor
//==================================================================

USuspenseCoreInventoryWidget::USuspenseCoreInventoryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, GridSize(0, 0) // CRITICAL: Do not hardcode - get from provider via CachedContainerData.GridSize
	, SlotSizePixels(64.0f)
	, SlotGapPixels(2.0f)
	, RotateKey(EKeys::R)
	, QuickEquipKey(EKeys::E)
	, QuickTransferKey(EKeys::LeftControl)
	, HoveredSlotIndex(INDEX_NONE)
	, LastClickTime(0.0)
	, LastClickedSlot(INDEX_NONE)
	, DragSourceSlot(INDEX_NONE)
{
}

//==================================================================
// UUserWidget Interface
//==================================================================

void USuspenseCoreInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Make widget focusable for mouse/keyboard input
	SetIsFocusable(true);

	// Create initial slot widgets if we have a bound provider AND slots don't exist yet
	// IMPORTANT: Don't recreate if RefreshFromProvider already created them!
	if (IsBoundToProvider() && SlotWidgets.Num() == 0)
	{
		CreateSlotWidgets();
	}
}

void USuspenseCoreInventoryWidget::NativeDestruct()
{
	ClearSlotWidgets();
	Super::NativeDestruct();
}

FReply USuspenseCoreInventoryWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	FKey Key = InKeyEvent.GetKey();

	// Handle rotation key
	if (Key == RotateKey)
	{
		HandleRotationInput();
		return FReply::Handled();
	}

	// Handle quick equip on hovered slot
	if (Key == QuickEquipKey && HoveredSlotIndex != INDEX_NONE)
	{
		// Request equip via provider
		// This would be implemented based on your action system
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply USuspenseCoreInventoryWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FVector2D LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	int32 SlotIndex = GetSlotAtLocalPosition(LocalPos);

	if (SlotIndex != INDEX_NONE)
	{
		bool bRightClick = InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton;
		bool bLeftClick = InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;

		// Check for double click
		double CurrentTime = FPlatformTime::Seconds();
		if (SlotIndex == LastClickedSlot && (CurrentTime - LastClickTime) < DoubleClickThreshold)
		{
			K2_OnSlotDoubleClicked(SlotIndex);
			LastClickedSlot = INDEX_NONE;
			DragSourceSlot = INDEX_NONE;
			return FReply::Handled();
		}

		HandleSlotClicked(SlotIndex, bRightClick);
		LastClickedSlot = SlotIndex;
		LastClickTime = CurrentTime;

		// Left click on slot - check if item exists for potential drag
		if (bLeftClick && !IsReadOnly())
		{
			// Check if slot has an item that can be dragged
			if (SlotIndex >= 0 && SlotIndex < SlotWidgets.Num())
			{
				USuspenseCoreInventorySlotWidget* SlotWidget = SlotWidgets[SlotIndex];
				if (SlotWidget && !SlotWidget->IsEmpty())
				{
					// Store source slot for drag detection
					DragSourceSlot = SlotIndex;

					// Return with DetectDrag to enable drag detection
					return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
				}
			}
		}

		return FReply::Handled();
	}

	DragSourceSlot = INDEX_NONE;
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply USuspenseCoreInventoryWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply USuspenseCoreInventoryWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	FVector2D LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	int32 SlotIndex = GetSlotAtLocalPosition(LocalPos);

	if (SlotIndex != HoveredSlotIndex)
	{
		// Clear previous hover
		if (HoveredSlotIndex != INDEX_NONE)
		{
			SetSlotHighlight(HoveredSlotIndex, ESuspenseCoreUISlotState::Empty);
		}

		HoveredSlotIndex = SlotIndex;

		// Set new hover
		if (HoveredSlotIndex != INDEX_NONE)
		{
			HandleSlotHovered(HoveredSlotIndex);
		}
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void USuspenseCoreInventoryWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
}

void USuspenseCoreInventoryWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	// Clear hover state
	if (HoveredSlotIndex != INDEX_NONE)
	{
		SetSlotHighlight(HoveredSlotIndex, ESuspenseCoreUISlotState::Empty);
		HoveredSlotIndex = INDEX_NONE;
	}

	Super::NativeOnMouseLeave(InMouseEvent);
}

void USuspenseCoreInventoryWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	// Check if we have a valid drag source slot
	if (DragSourceSlot == INDEX_NONE)
	{
		UE_LOG(LogTemp, Verbose, TEXT("NativeOnDragDetected: No drag source slot"));
		return;
	}

	// Get the item data from the source slot
	if (!IsBoundToProvider())
	{
		UE_LOG(LogTemp, Warning, TEXT("NativeOnDragDetected: No bound provider"));
		DragSourceSlot = INDEX_NONE;
		return;
	}

	ISuspenseCoreUIDataProvider* ProviderInterface = GetBoundProvider().GetInterface();
	if (!ProviderInterface)
	{
		UE_LOG(LogTemp, Warning, TEXT("NativeOnDragDetected: Provider interface is null"));
		DragSourceSlot = INDEX_NONE;
		return;
	}

	// Get item data at the drag source slot
	FSuspenseCoreItemUIData ItemData;
	ProviderInterface->GetItemUIDataAtSlot(DragSourceSlot, ItemData);

	if (!ItemData.InstanceID.IsValid())
	{
		UE_LOG(LogTemp, Verbose, TEXT("NativeOnDragDetected: No item at slot %d"), DragSourceSlot);
		DragSourceSlot = INDEX_NONE;
		return;
	}

	// Create drag data
	FSuspenseCoreDragData DragData = FSuspenseCoreDragData::Create(
		ItemData,
		ProviderInterface->GetContainerType(),
		ProviderInterface->GetContainerTypeTag(),
		ProviderInterface->GetProviderID(),
		DragSourceSlot
	);

	if (!DragData.bIsValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("NativeOnDragDetected: Failed to create drag data"));
		DragSourceSlot = INDEX_NONE;
		return;
	}

	// Calculate drag offset from cursor
	FVector2D LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	FIntPoint GridPos = SlotIndexToGridPos(DragSourceSlot);
	float TotalSlotSize = SlotSizePixels + SlotGapPixels;
	FVector2D SlotTopLeft(GridPos.X * TotalSlotSize, GridPos.Y * TotalSlotSize);
	DragData.DragOffset = SlotTopLeft - LocalPos;

	// Create the drag-drop operation
	USuspenseCoreDragDropOperation* DragOperation = USuspenseCoreDragDropOperation::CreateDrag(
		GetOwningPlayer(),
		DragData,
		nullptr // Use default visual widget class
	);

	if (DragOperation)
	{
		OutOperation = DragOperation;
		UE_LOG(LogTemp, Log, TEXT("NativeOnDragDetected: Started drag for item '%s' from slot %d"),
			*ItemData.DisplayName.ToString(), DragSourceSlot);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("NativeOnDragDetected: Failed to create drag operation"));
	}

	// Clear drag source (operation will handle the rest)
	DragSourceSlot = INDEX_NONE;
}

void USuspenseCoreInventoryWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);

	// Cast to our drag operation type
	USuspenseCoreDragDropOperation* DragOp = Cast<USuspenseCoreDragDropOperation>(InOperation);
	if (!DragOp)
	{
		return;
	}

	UE_LOG(LogTemp, Verbose, TEXT("NativeOnDragEnter: Drag entered inventory widget"));
}

void USuspenseCoreInventoryWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);

	// Cast to our drag operation type
	USuspenseCoreDragDropOperation* DragOp = Cast<USuspenseCoreDragDropOperation>(InOperation);
	if (!DragOp)
	{
		return;
	}

	// Clear hover target when leaving
	DragOp->SetHoverTarget(nullptr, INDEX_NONE);

	// Clear all highlights
	ClearHighlights();

	UE_LOG(LogTemp, Verbose, TEXT("NativeOnDragLeave: Drag left inventory widget"));
}

bool USuspenseCoreInventoryWidget::NativeOnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent, UDragDropOperation* Operation)
{
	// Cast to our drag operation type
	USuspenseCoreDragDropOperation* DragOp = Cast<USuspenseCoreDragDropOperation>(Operation);
	if (!DragOp)
	{
		return Super::NativeOnDragOver(MyGeometry, DragDropEvent, Operation);
	}

	// Get slot under cursor
	FVector2D LocalPos = MyGeometry.AbsoluteToLocal(DragDropEvent.GetScreenSpacePosition());
	int32 SlotIndex = GetSlotAtLocalPosition(LocalPos);

	// Update hover target if changed
	if (SlotIndex != DragOp->GetHoverSlot())
	{
		// Set hover target on the drag operation
		TScriptInterface<ISuspenseCoreUIContainer> ContainerInterface;
		ContainerInterface.SetObject(this);
		ContainerInterface.SetInterface(this);
		DragOp->SetHoverTarget(ContainerInterface, SlotIndex);

		// Validate drop and highlight appropriately
		if (SlotIndex != INDEX_NONE && IsBoundToProvider())
		{
			ISuspenseCoreUIDataProvider* ProviderInterface = GetBoundProvider().GetInterface();
			if (ProviderInterface)
			{
				const FSuspenseCoreDragData& DragData = DragOp->GetDragData();
				FSuspenseCoreDropValidation Validation = ProviderInterface->ValidateDrop(DragData, SlotIndex, DragData.bIsRotatedDuringDrag);

				// Update visual validity indicator
				DragOp->UpdateDropValidity(Validation.bIsValid);

				// Highlight multi-cell drop target
				HighlightDropSlots(DragData.Item.GetEffectiveSize(), SlotIndex, Validation.bIsValid);
			}
		}
	}

	return true; // We handle the drag
}

bool USuspenseCoreInventoryWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	// Cast to our drag operation type
	USuspenseCoreDragDropOperation* DragOp = Cast<USuspenseCoreDragDropOperation>(InOperation);
	if (!DragOp)
	{
		UE_LOG(LogTemp, Warning, TEXT("NativeOnDrop: Invalid drag operation type"));
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}

	// Get slot under cursor
	FVector2D LocalPos = InGeometry.AbsoluteToLocal(InDragDropEvent.GetScreenSpacePosition());
	int32 SlotIndex = GetSlotAtLocalPosition(LocalPos);

	if (SlotIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Verbose, TEXT("NativeOnDrop: Drop outside valid slot"));
		ClearHighlights();
		return false;
	}

	// Handle the drop through base class
	const FSuspenseCoreDragData& DragData = DragOp->GetDragData();
	bool bSuccess = HandleDrop(DragData, SlotIndex);

	// Clear highlights
	ClearHighlights();

	UE_LOG(LogTemp, Log, TEXT("NativeOnDrop: Drop %s at slot %d"),
		bSuccess ? TEXT("succeeded") : TEXT("failed"), SlotIndex);

	return bSuccess;
}

//==================================================================
// Grid Configuration
//==================================================================

void USuspenseCoreInventoryWidget::SetGridSize(const FIntPoint& InGridSize)
{
	// Update both local cache and CachedContainerData for consistency
	if (CachedContainerData.GridSize != InGridSize)
	{
		CachedContainerData.GridSize = InGridSize;
		GridSize = InGridSize; // Keep local copy for backwards compatibility

		// Recreate slot widgets
		ClearSlotWidgets();
		CreateSlotWidgets();
	}
}

void USuspenseCoreInventoryWidget::SetSlotSizePixels(float InSize)
{
	if (!FMath::IsNearlyEqual(SlotSizePixels, InSize))
	{
		SlotSizePixels = InSize;
		// Update slot widget sizes if needed
	}
}

//==================================================================
// ISuspenseCoreUIContainer Overrides
//==================================================================

void USuspenseCoreInventoryWidget::RefreshFromProvider()
{
	// Call base implementation to refresh container data and slot widgets
	Super::RefreshFromProvider();

	// Update slot-to-anchor map for multi-cell item support
	UpdateSlotToAnchorMap();
}

UWidget* USuspenseCoreInventoryWidget::GetSlotWidget(int32 SlotIndex) const
{
	if (SlotIndex >= 0 && SlotIndex < SlotWidgets.Num())
	{
		return SlotWidgets[SlotIndex];
	}
	return nullptr;
}

TArray<UWidget*> USuspenseCoreInventoryWidget::GetAllSlotWidgets() const
{
	TArray<UWidget*> Result;
	for (USuspenseCoreInventorySlotWidget* SlotWidget : SlotWidgets)
	{
		Result.Add(SlotWidget);
	}
	UE_LOG(LogTemp, Verbose, TEXT("GetAllSlotWidgets [%s]: returning %d widgets"), *GetName(), Result.Num());
	return Result;
}

int32 USuspenseCoreInventoryWidget::GetSlotAtLocalPosition(const FVector2D& LocalPosition) const
{
	if (!SlotGrid || SlotWidgets.Num() == 0)
	{
		return INDEX_NONE;
	}

	// Transform local position from widget space to SlotGrid space
	// Get SlotGrid's position within the inventory widget hierarchy
	const FGeometry& WidgetGeometry = GetCachedGeometry();
	const FGeometry& GridGeometry = SlotGrid->GetCachedGeometry();

	// Transform from inventory widget space to grid space
	// The grid might be offset due to padding, borders, or other layout elements
	FVector2D GridLocalPos = GridGeometry.AbsoluteToLocal(
		WidgetGeometry.LocalToAbsolute(LocalPosition)
	);

	if (GridLocalPos.X < 0 || GridLocalPos.Y < 0)
	{
		return INDEX_NONE;
	}

	// Calculate grid position from grid-local coordinates
	float TotalSlotSize = SlotSizePixels + SlotGapPixels;
	int32 Column = FMath::FloorToInt(GridLocalPos.X / TotalSlotSize);
	int32 Row = FMath::FloorToInt(GridLocalPos.Y / TotalSlotSize);

	FIntPoint GridPos(Column, Row);
	return GridPosToSlotIndex(GridPos);
}

void USuspenseCoreInventoryWidget::SetSlotHighlight(int32 SlotIndex, ESuspenseCoreUISlotState State)
{
	if (SlotIndex >= 0 && SlotIndex < SlotWidgets.Num() && SlotWidgets[SlotIndex])
	{
		SlotWidgets[SlotIndex]->SetHighlightState(State);
	}
}

//==================================================================
// Grid Utilities
//==================================================================

FIntPoint USuspenseCoreInventoryWidget::SlotIndexToGridPos(int32 SlotIndex) const
{
	// Use provider's grid size as single source of truth
	const FIntPoint& EffectiveGridSize = CachedContainerData.GridSize;

	if (SlotIndex < 0 || EffectiveGridSize.X <= 0)
	{
		return FIntPoint(-1, -1);
	}

	int32 Column = SlotIndex % EffectiveGridSize.X;
	int32 Row = SlotIndex / EffectiveGridSize.X;

	return FIntPoint(Column, Row);
}

int32 USuspenseCoreInventoryWidget::GridPosToSlotIndex(const FIntPoint& GridPos) const
{
	if (!IsValidGridPos(GridPos))
	{
		return INDEX_NONE;
	}

	// Use provider's grid size as single source of truth
	const FIntPoint& EffectiveGridSize = CachedContainerData.GridSize;
	return GridPos.Y * EffectiveGridSize.X + GridPos.X;
}

bool USuspenseCoreInventoryWidget::IsValidGridPos(const FIntPoint& GridPos) const
{
	// Use provider's grid size as single source of truth
	const FIntPoint& EffectiveGridSize = CachedContainerData.GridSize;
	return GridPos.X >= 0 && GridPos.X < EffectiveGridSize.X &&
		   GridPos.Y >= 0 && GridPos.Y < EffectiveGridSize.Y;
}

TArray<int32> USuspenseCoreInventoryWidget::GetOccupiedSlots(const FIntPoint& GridPos, const FIntPoint& ItemSize) const
{
	TArray<int32> Result;

	for (int32 DY = 0; DY < ItemSize.Y; ++DY)
	{
		for (int32 DX = 0; DX < ItemSize.X; ++DX)
		{
			FIntPoint CheckPos = GridPos + FIntPoint(DX, DY);
			int32 SlotIndex = GridPosToSlotIndex(CheckPos);
			if (SlotIndex != INDEX_NONE)
			{
				Result.Add(SlotIndex);
			}
		}
	}

	return Result;
}

//==================================================================
// Drag Support
//==================================================================

void USuspenseCoreInventoryWidget::HighlightDropSlots(const FIntPoint& ItemSize, int32 TargetSlot, bool bIsValid)
{
	// Clear all highlights first
	ClearHighlights();

	if (TargetSlot == INDEX_NONE)
	{
		return;
	}

	// Get grid position of target
	FIntPoint GridPos = SlotIndexToGridPos(TargetSlot);

	// Get all slots that would be occupied
	TArray<int32> AffectedSlots = GetOccupiedSlots(GridPos, ItemSize);

	// Set highlight state
	ESuspenseCoreUISlotState State = bIsValid ? ESuspenseCoreUISlotState::DropTargetValid : ESuspenseCoreUISlotState::DropTargetInvalid;

	for (int32 AffectedSlotIndex : AffectedSlots)
	{
		SetSlotHighlight(AffectedSlotIndex, State);
	}
}

//==================================================================
// Override Points from Base
//==================================================================

void USuspenseCoreInventoryWidget::CreateSlotWidgets_Implementation()
{
	UE_LOG(LogTemp, Warning, TEXT("=== CreateSlotWidgets [%s] START === Existing=%d"),
		*GetName(), SlotWidgets.Num());

	if (!SlotGrid || !SlotWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateSlotWidgets: Missing SlotGrid or SlotWidgetClass!"));
		return;
	}

	// CRITICAL: Use grid size from cached container data (provider), not widget default
	if (CachedContainerData.GridSize.X > 0 && CachedContainerData.GridSize.Y > 0)
	{
		GridSize = CachedContainerData.GridSize;
		UE_LOG(LogTemp, Log, TEXT("CreateSlotWidgets: Using provider grid size %dx%d"), GridSize.X, GridSize.Y);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateSlotWidgets: Provider grid size invalid, using widget default %dx%d"), GridSize.X, GridSize.Y);
	}

	// Clear existing
	SlotGrid->ClearChildren();
	SlotWidgets.Empty();

	int32 TotalSlots = GridSize.X * GridSize.Y;
	SlotWidgets.Reserve(TotalSlots);

	UE_LOG(LogTemp, Log, TEXT("CreateSlotWidgets: Creating %d slots"), TotalSlots);

	// Create slot widget for each grid cell
	for (int32 SlotIndex = 0; SlotIndex < TotalSlots; ++SlotIndex)
	{
		FIntPoint GridPos = SlotIndexToGridPos(SlotIndex);

		USuspenseCoreInventorySlotWidget* SlotWidget = CreateWidget<USuspenseCoreInventorySlotWidget>(GetOwningPlayer(), SlotWidgetClass);
		if (!SlotWidget)
		{
			continue;
		}

		// Configure slot
		SlotWidget->SetSlotIndex(SlotIndex);
		SlotWidget->SetGridPosition(GridPos);
		SlotWidget->SetSlotSize(FVector2D(SlotSizePixels, SlotSizePixels));

		// Add to grid at correct position
		UUniformGridSlot* GridSlot = SlotGrid->AddChildToUniformGrid(SlotWidget, GridPos.Y, GridPos.X);
		if (GridSlot)
		{
			GridSlot->SetHorizontalAlignment(HAlign_Fill);
			GridSlot->SetVerticalAlignment(VAlign_Fill);
		}

		SlotWidgets.Add(SlotWidget);
	}

	UE_LOG(LogTemp, Log, TEXT("CreateSlotWidgets: Created %d slot widgets"), SlotWidgets.Num());

	// Update the slot-to-anchor map for multi-cell items
	UpdateSlotToAnchorMap();
}

void USuspenseCoreInventoryWidget::UpdateSlotWidget_Implementation(
	int32 SlotIndex,
	const FSuspenseCoreSlotUIData& SlotData,
	const FSuspenseCoreItemUIData& ItemData)
{
	if (SlotIndex < 0 || SlotIndex >= SlotWidgets.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("UpdateSlotWidget: SlotIndex %d out of range (0-%d)"), SlotIndex, SlotWidgets.Num() - 1);
		return;
	}

	USuspenseCoreInventorySlotWidget* SlotWidget = SlotWidgets[SlotIndex];
	if (!SlotWidget)
	{
		return;
	}

	// Debug logging for first few slots or if item exists
	if (SlotIndex < 3 || ItemData.InstanceID.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("UpdateSlotWidget[%d]: bIsAnchor=%d, ItemID=%s, IconPath=%s"),
			SlotIndex,
			SlotData.bIsAnchor ? 1 : 0,
			*ItemData.ItemID.ToString(),
			*ItemData.IconPath.ToString());
	}

	// Update slot with data
	SlotWidget->UpdateSlot(SlotData, ItemData);
}

void USuspenseCoreInventoryWidget::ClearSlotWidgets_Implementation()
{
	if (SlotGrid)
	{
		SlotGrid->ClearChildren();
	}
	SlotWidgets.Empty();
}

//==================================================================
// Input Handling
//==================================================================

void USuspenseCoreInventoryWidget::HandleSlotClicked(int32 SlotIndex, bool bRightClick)
{
	// Resolve to anchor slot for multi-cell items
	int32 AnchorSlot = SlotToAnchorMap.Contains(SlotIndex)
		? SlotToAnchorMap[SlotIndex]
		: SlotIndex;

	// Select the anchor slot
	SetSelectedSlot(AnchorSlot);

	// Notify Blueprint with anchor slot
	K2_OnSlotClicked(AnchorSlot, bRightClick);

	// If right click, show context menu
	if (bRightClick)
	{
		ShowContextMenu(AnchorSlot, FVector2D::ZeroVector); // Screen position would need to be passed
	}
}

void USuspenseCoreInventoryWidget::HandleSlotHovered(int32 HoveredIndex)
{
	// Resolve to anchor slot for multi-cell items
	int32 AnchorSlot = SlotToAnchorMap.Contains(HoveredIndex)
		? SlotToAnchorMap[HoveredIndex]
		: HoveredIndex;

	// Highlight all slots occupied by the item (if any)
	if (IsBoundToProvider() && SlotToAnchorMap.Contains(HoveredIndex))
	{
		ISuspenseCoreUIDataProvider* ProviderInterface = GetBoundProvider().GetInterface();
		if (ProviderInterface)
		{
			// Get the item's instance ID at the anchor slot
			FSuspenseCoreItemUIData ItemData;
			if (ProviderInterface->GetItemUIDataAtSlot(AnchorSlot, ItemData))
			{
				// Get all occupied slots for this item
				TArray<int32> OccupiedSlots = ProviderInterface->GetOccupiedSlotsForItem(ItemData.InstanceID);
				for (int32 OccupiedSlotIdx : OccupiedSlots)
				{
					SetSlotHighlight(OccupiedSlotIdx, ESuspenseCoreUISlotState::Highlighted);
				}
			}
		}
	}
	else
	{
		// Single slot highlight
		SetSlotHighlight(HoveredIndex, ESuspenseCoreUISlotState::Highlighted);
	}

	// Show tooltip for anchor slot content
	ShowSlotTooltip(AnchorSlot);
}

void USuspenseCoreInventoryWidget::UpdateSlotToAnchorMap()
{
	SlotToAnchorMap.Empty();

	if (!IsBoundToProvider())
	{
		return;
	}

	ISuspenseCoreUIDataProvider* ProviderInterface = GetBoundProvider().GetInterface();
	if (!ProviderInterface)
	{
		return;
	}

	// Iterate through all items and map their occupied slots to anchors
	TArray<FSuspenseCoreItemUIData> Items = ProviderInterface->GetAllItemUIData();
	for (const FSuspenseCoreItemUIData& Item : Items)
	{
		if (!Item.InstanceID.IsValid())
		{
			continue;
		}

		int32 AnchorSlot = Item.AnchorSlot;
		TArray<int32> OccupiedSlots = ProviderInterface->GetOccupiedSlotsForItem(Item.InstanceID);

		for (int32 OccupiedSlotIdx : OccupiedSlots)
		{
			SlotToAnchorMap.Add(OccupiedSlotIdx, AnchorSlot);
		}
	}

	UE_LOG(LogTemp, Verbose, TEXT("UpdateSlotToAnchorMap: Mapped %d slots to anchors"), SlotToAnchorMap.Num());
}

void USuspenseCoreInventoryWidget::HandleRotationInput()
{
	// If we're dragging, toggle rotation
	USuspenseCoreUIManager* UIManager = USuspenseCoreUIManager::Get(this);
	if (UIManager && UIManager->IsDragging())
	{
		// Toggle rotation on drag data
		// This would need access to modify the drag data
		K2_OnRotationToggled(true);
	}
}

//==================================================================
// Batch Update System
//==================================================================

void USuspenseCoreInventoryWidget::BeginBatchUpdate()
{
	if (!bIsBatchingUpdates)
	{
		bIsBatchingUpdates = true;
		PendingBatch.Clear();
	}
}

void USuspenseCoreInventoryWidget::CommitBatchUpdate()
{
	if (bIsBatchingUpdates)
	{
		bIsBatchingUpdates = false;

		if (PendingBatch.HasUpdates())
		{
			ApplyBatchUpdate(PendingBatch);
			PendingBatch.Clear();
		}
	}
}

void USuspenseCoreInventoryWidget::ApplyBatchUpdate(const FSuspenseCoreGridUpdateBatch& Batch)
{
	// If full refresh is needed, just call RefreshFromProvider
	if (Batch.bNeedsFullRefresh)
	{
		RefreshFromProvider();
		return;
	}

	ISuspenseCoreUIDataProvider* ProviderInterface = nullptr;
	if (IsBoundToProvider())
	{
		ProviderInterface = GetBoundProvider().GetInterface();
	}

	// Apply visibility updates
	for (const auto& VisibilityPair : Batch.SlotVisibilityUpdates)
	{
		int32 SlotIndex = VisibilityPair.Key;
		bool bVisible = VisibilityPair.Value;

		if (SlotIndex >= 0 && SlotIndex < SlotWidgets.Num() && SlotWidgets[SlotIndex])
		{
			SlotWidgets[SlotIndex]->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
		}
	}

	// Apply span updates (for multi-cell items)
	for (const auto& SpanPair : Batch.SlotSpanUpdates)
	{
		int32 SlotIndex = SpanPair.Key;
		const FIntPoint& Span = SpanPair.Value;

		// For UniformGridPanel, we can't actually span cells
		// This would need a CanvasPanel implementation for true multi-cell rendering
		// For now, log that a span update was requested
		UE_LOG(LogTemp, Verbose, TEXT("ApplyBatchUpdate: Slot %d requested span %dx%d (requires CanvasPanel implementation)"),
			SlotIndex, Span.X, Span.Y);
	}

	// Apply slot content refreshes
	for (int32 SlotIndex : Batch.SlotsToRefresh)
	{
		if (SlotIndex >= 0 && SlotIndex < SlotWidgets.Num())
		{
			// Get slot and item data
			FSuspenseCoreSlotUIData SlotData;
			SlotData.SlotIndex = SlotIndex;

			FSuspenseCoreItemUIData ItemData;
			if (ProviderInterface)
			{
				ProviderInterface->GetItemUIDataAtSlot(SlotIndex, ItemData);
				SlotData.State = ItemData.InstanceID.IsValid()
					? ESuspenseCoreUISlotState::Occupied
					: ESuspenseCoreUISlotState::Empty;
			}

			// Update the slot widget
			UpdateSlotWidget(SlotIndex, SlotData, ItemData);
		}
	}

	// Apply highlight state updates
	for (const auto& HighlightPair : Batch.SlotHighlightUpdates)
	{
		int32 SlotIndex = HighlightPair.Key;
		ESuspenseCoreUISlotState State = HighlightPair.Value;

		SetSlotHighlight(SlotIndex, State);
	}

	// Update slot-to-anchor map if any content changed
	if (Batch.SlotsToRefresh.Num() > 0)
	{
		UpdateSlotToAnchorMap();
	}

	UE_LOG(LogTemp, Verbose, TEXT("ApplyBatchUpdate: Applied %d visibility, %d span, %d refresh, %d highlight updates"),
		Batch.SlotVisibilityUpdates.Num(),
		Batch.SlotSpanUpdates.Num(),
		Batch.SlotsToRefresh.Num(),
		Batch.SlotHighlightUpdates.Num());
}
