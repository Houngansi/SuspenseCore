// SuspenseCoreInventoryWidget.cpp
// SuspenseCore - Grid-based Inventory Widget Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/Inventory/SuspenseCoreInventoryWidget.h"
#include "SuspenseCore/Widgets/Inventory/SuspenseCoreInventorySlotWidget.h"
#include "SuspenseCore/Subsystems/SuspenseCoreUIManager.h"
#include "SuspenseCore/Widgets/DragDrop/SuspenseCoreDragDropOperation.h"
#include "SuspenseCore/Widgets/DragDrop/SuspenseCoreDragVisualWidget.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreUIDataProvider.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreUIContainer.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreMagazineInspectionWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/TextBlock.h"
#include "Framework/Application/SlateApplication.h"

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
	, DragStartMousePosition(FVector2D::ZeroVector)
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
			// Handle double click - check if this is a magazine for inspection
			HandleSlotDoubleClicked(SlotIndex);
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
					// Store source slot and INITIAL click position for drag detection
					// This is critical: NativeOnDragDetected is called AFTER cursor moves,
					// so we need the original click position for correct DragOffset calculation
					DragSourceSlot = SlotIndex;
					DragStartMousePosition = InMouseEvent.GetScreenSpacePosition();

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

	// Hide tooltip when leaving widget
	HideTooltip();

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

	// Calculate DragOffset: difference between slot's top-left and initial click position
	// This makes the item appear to be "picked up" from where it was clicked
	if (DragSourceSlot >= 0 && DragSourceSlot < SlotWidgets.Num() && SlotWidgets[DragSourceSlot])
	{
		// Get slot widget's cached geometry
		FGeometry SlotGeometry = SlotWidgets[DragSourceSlot]->GetCachedGeometry();
		FVector2D SlotAbsolutePos = SlotGeometry.GetAbsolutePosition();
		FVector2D SlotLocalSize = SlotGeometry.GetLocalSize();

		// Check if geometry is valid (not cached yet on first frame after widget creation)
		// Invalid geometry has position (0,0) AND zero/near-zero size
		const bool bGeometryValid = !SlotAbsolutePos.IsNearlyZero() || !SlotLocalSize.IsNearlyZero();

		if (bGeometryValid)
		{
			// DragOffset = SlotTopLeft - ClickPosition (negative offset to move widget up/left from cursor)
			DragData.DragOffset = SlotAbsolutePos - DragStartMousePosition;

			UE_LOG(LogTemp, Log, TEXT("DragOffset calculation: SlotPos=(%.1f, %.1f), ClickPos=(%.1f, %.1f), Offset=(%.1f, %.1f)"),
				SlotAbsolutePos.X, SlotAbsolutePos.Y,
				DragStartMousePosition.X, DragStartMousePosition.Y,
				DragData.DragOffset.X, DragData.DragOffset.Y);
		}
		else
		{
			// Geometry not yet cached (first frame) - use zero offset so drag visual stays at cursor
			DragData.DragOffset = FVector2D::ZeroVector;
			UE_LOG(LogTemp, Warning, TEXT("DragOffset calculation: Geometry not cached yet, using zero offset"));
		}
	}
	else
	{
		DragData.DragOffset = FVector2D::ZeroVector;
	}

	// Create the drag-drop operation
	// Access protected member from base class - use explicit base class qualification
	TSubclassOf<USuspenseCoreDragVisualWidget> VisualClass = USuspenseCoreBaseContainerWidget::DragVisualWidgetClass;
	checkf(VisualClass, TEXT("%s: DragVisualWidgetClass must be set! Configure it in Blueprint defaults."), *GetClass()->GetName());
	USuspenseCoreDragDropOperation* DragOperation = USuspenseCoreDragDropOperation::CreateDrag(
		GetOwningPlayer(),
		DragData,
		VisualClass
	);

	if (DragOperation)
	{
		// DragOffset is passed via DragData to InitializeDrag - visual widget uses SetRenderTranslation
		// NO need to set UDragDropOperation::Pivot/Offset - we use legacy manual positioning approach
		OutOperation = DragOperation;
		UE_LOG(LogTemp, Log, TEXT("NativeOnDragDetected: Started drag for item '%s' from slot %d (Offset: %.1f, %.1f)"),
			*ItemData.DisplayName.ToString(), DragSourceSlot, DragData.DragOffset.X, DragData.DragOffset.Y);
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

	const FSuspenseCoreDragData& DragData = DragOp->GetDragData();

	// Check for ammo-to-magazine drop
	if (TryHandleAmmoToMagazineDrop(DragData, SlotIndex))
	{
		ClearHighlights();
		return true;
	}

	// Handle the drop through base class
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

	// Update weight display
	if (WeightText)
	{
		if (CachedContainerData.bHasWeightLimit && CachedContainerData.MaxWeight > 0.0f)
		{
			FNumberFormattingOptions FormatOptions;
			FormatOptions.MinimumFractionalDigits = 1;
			FormatOptions.MaximumFractionalDigits = 1;

			FText WeightFormatText = FText::Format(
				NSLOCTEXT("SuspenseCore", "WeightFormat", "{0} / {1} kg"),
				FText::AsNumber(CachedContainerData.CurrentWeight, &FormatOptions),
				FText::AsNumber(CachedContainerData.MaxWeight, &FormatOptions)
			);
			WeightText->SetText(WeightFormatText);
			WeightText->SetVisibility(ESlateVisibility::Visible);

			UE_LOG(LogTemp, Log, TEXT("WeightText updated: %.1f / %.1f kg"),
				CachedContainerData.CurrentWeight, CachedContainerData.MaxWeight);
		}
		else
		{
			WeightText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Update slot count display
	if (SlotCountText)
	{
		FText SlotCountFormatText = FText::Format(
			NSLOCTEXT("SuspenseCore", "SlotCountFormat", "{0} / {1}"),
			FText::AsNumber(CachedContainerData.OccupiedSlots),
			FText::AsNumber(CachedContainerData.TotalSlots)
		);
		SlotCountText->SetText(SlotCountFormatText);
	}
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
	// Get active grid panel (either GridPanel or UniformGridPanel)
	UPanelWidget* ActiveGrid = const_cast<USuspenseCoreInventoryWidget*>(this)->GetActiveGridPanel();
	if (!ActiveGrid || SlotWidgets.Num() == 0)
	{
		return INDEX_NONE;
	}

	// Transform local position from widget space to grid space
	const FGeometry& WidgetGeometry = GetCachedGeometry();
	const FGeometry& GridGeometry = ActiveGrid->GetCachedGeometry();

	// Transform from inventory widget space to grid space
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

	// Check which grid panel type is available
	// PREFER SlotGridPanel (UGridPanel) for multi-cell spanning support
	UPanelWidget* ActiveGrid = GetActiveGridPanel();
	if (!ActiveGrid)
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateSlotWidgets: No grid panel bound! Need SlotGridPanel or SlotGrid."));
		return;
	}

	if (!SlotWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateSlotWidgets: Missing SlotWidgetClass!"));
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
	ActiveGrid->ClearChildren();
	SlotWidgets.Empty();
	CachedGridSlots.Empty();

	int32 TotalSlots = GridSize.X * GridSize.Y;
	SlotWidgets.Reserve(TotalSlots);

	UE_LOG(LogTemp, Log, TEXT("CreateSlotWidgets: Creating %d slots (using %s)"),
		TotalSlots, bUsingGridPanel ? TEXT("GridPanel") : TEXT("UniformGridPanel"));

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
		SlotWidget->SetCellSize(SlotSizePixels); // For multi-cell icon sizing

		// Add to grid at correct position based on grid type
		if (bUsingGridPanel && SlotGridPanel)
		{
			// Use GridPanel with full spanning support
			UGridSlot* GSlot = SlotGridPanel->AddChildToGrid(SlotWidget, GridPos.Y, GridPos.X);
			if (GSlot)
			{
				GSlot->SetRow(GridPos.Y);
				GSlot->SetColumn(GridPos.X);
				GSlot->SetRowSpan(1);
				GSlot->SetColumnSpan(1);
				GSlot->SetHorizontalAlignment(HAlign_Fill);
				GSlot->SetVerticalAlignment(VAlign_Fill);
				GSlot->SetPadding(FMargin(SlotGapPixels * 0.5f));

				// Cache for later span updates
				CachedGridSlots.Add(SlotIndex, GSlot);
			}
		}
		else if (SlotGrid)
		{
			// Fallback to UniformGridPanel (no spanning support)
			UUniformGridSlot* UGSlot = SlotGrid->AddChildToUniformGrid(SlotWidget, GridPos.Y, GridPos.X);
			if (UGSlot)
			{
				UGSlot->SetHorizontalAlignment(HAlign_Fill);
				UGSlot->SetVerticalAlignment(VAlign_Fill);
			}
		}

		SlotWidgets.Add(SlotWidget);
	}

	UE_LOG(LogTemp, Log, TEXT("CreateSlotWidgets: Created %d slot widgets (cached %d GridSlots)"),
		SlotWidgets.Num(), CachedGridSlots.Num());

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
		UE_LOG(LogTemp, Log, TEXT("UpdateSlotWidget[%d]: bIsAnchor=%d, bIsPartOfItem=%d, ItemID=%s, Size=%dx%d"),
			SlotIndex,
			SlotData.bIsAnchor ? 1 : 0,
			SlotData.bIsPartOfItem ? 1 : 0,
			*ItemData.ItemID.ToString(),
			ItemData.GridSize.X, ItemData.GridSize.Y);
	}

	// Handle multi-cell item visibility and spanning
	if (SlotData.bIsAnchor && ItemData.InstanceID.IsValid())
	{
		// ANCHOR SLOT: Visible, with span matching item size
		SlotWidget->SetVisibility(ESlateVisibility::Visible);

		// Update grid slot span (GridPanel only)
		UpdateGridSlotSpan(SlotIndex, ItemData.GridSize, ItemData.bIsRotated);

		// Tell slot widget to render for multi-cell
		SlotWidget->SetMultiCellItemSize(ItemData.GetEffectiveSize());
	}
	else if (SlotData.bIsPartOfItem && !SlotData.bIsAnchor)
	{
		// NON-ANCHOR OCCUPIED SLOT: Hidden (covered by anchor's span)
		SlotWidget->SetVisibility(ESlateVisibility::Hidden);

		// Reset span to 1x1
		ResetGridSlotSpan(SlotIndex);
	}
	else
	{
		// EMPTY SLOT: Visible, 1x1
		SlotWidget->SetVisibility(ESlateVisibility::Visible);

		// Reset span
		ResetGridSlotSpan(SlotIndex);

		// Reset multi-cell size
		SlotWidget->SetMultiCellItemSize(FIntPoint(1, 1));
	}

	// Update slot content with data
	SlotWidget->UpdateSlotData(SlotData, ItemData);
}

void USuspenseCoreInventoryWidget::ClearSlotWidgets_Implementation()
{
	UPanelWidget* ActiveGrid = GetActiveGridPanel();
	if (ActiveGrid)
	{
		ActiveGrid->ClearChildren();
	}
	SlotWidgets.Empty();
	CachedGridSlots.Empty();
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

void USuspenseCoreInventoryWidget::HandleSlotDoubleClicked(int32 SlotIndex)
{
	// Resolve to anchor slot for multi-cell items
	int32 AnchorSlot = SlotToAnchorMap.Contains(SlotIndex)
		? SlotToAnchorMap[SlotIndex]
		: SlotIndex;

	if (!IsBoundToProvider())
	{
		return;
	}

	ISuspenseCoreUIDataProvider* ProviderInterface = GetBoundProvider().GetInterface();
	if (!ProviderInterface)
	{
		return;
	}

	// Get item data at slot
	FSuspenseCoreItemUIData ItemData;
	if (!ProviderInterface->GetItemUIDataAtSlot(AnchorSlot, ItemData))
	{
		return;
	}

	// Check if item is a magazine - open inspection
	USuspenseCoreUIManager* UIManager = USuspenseCoreUIManager::Get(this);
	if (!UIManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("HandleSlotDoubleClicked: UIManager not available"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("HandleSlotDoubleClicked: Slot=%d, Item=%s, ItemType=%s"),
		SlotIndex, *ItemData.ItemID.ToString(), *ItemData.ItemType.ToString());

	if (UIManager->IsMagazineItem(ItemData))
	{
		UE_LOG(LogTemp, Log, TEXT("HandleSlotDoubleClicked: Item IS a magazine, opening inspection"));
		// Build inspection data from item
		FSuspenseCoreMagazineInspectionData InspectionData;
		InspectionData.MagazineInstanceID = ItemData.InstanceID;
		InspectionData.MagazineID = ItemData.ItemID;
		InspectionData.DisplayName = ItemData.DisplayName;
		InspectionData.RarityTag = ItemData.RarityTag;
		// Note: Icon should be loaded from IconPath if needed

		// Try to get magazine-specific data from item payload or MagazineComponent
		// For now use placeholder values - real implementation should get from MagazineComponent
		InspectionData.MaxCapacity = 30; // Default, should come from item data
		InspectionData.CurrentRounds = 0;
		InspectionData.CaliberDisplayName = FText::FromString(TEXT("Unknown"));

		// Build round slots
		for (int32 i = 0; i < InspectionData.MaxCapacity; ++i)
		{
			FSuspenseCoreRoundSlotData RoundSlot;
			RoundSlot.SlotIndex = i;
			RoundSlot.bIsOccupied = false;
			RoundSlot.bCanUnload = false;
			InspectionData.RoundSlots.Add(RoundSlot);
		}

		// Open inspection
		UIManager->OpenMagazineInspection(InspectionData);
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

//==================================================================
// Tooltip Support
//==================================================================

void USuspenseCoreInventoryWidget::ShowSlotTooltip(int32 SlotIndex)
{
	if (!IsBoundToProvider())
	{
		return;
	}

	ISuspenseCoreUIDataProvider* ProviderInterface = GetBoundProvider().GetInterface();
	if (!ProviderInterface)
	{
		return;
	}

	// Get item data at slot
	FSuspenseCoreItemUIData ItemData;
	if (!ProviderInterface->GetItemUIDataAtSlot(SlotIndex, ItemData))
	{
		// No item at this slot
		HideTooltip();
		return;
	}

	// Check if item is valid
	if (!ItemData.InstanceID.IsValid() || ItemData.ItemID.IsNone())
	{
		HideTooltip();
		return;
	}

	// Request tooltip from UIManager
	if (USuspenseCoreUIManager* UIManager = USuspenseCoreUIManager::Get(this))
	{
		// Get mouse position for tooltip placement
		FVector2D MousePosition = FSlateApplication::Get().GetCursorPos();

		// Show tooltip through UIManager
		UIManager->ShowItemTooltip(ItemData, MousePosition);
	}
}

void USuspenseCoreInventoryWidget::HideTooltip()
{
	if (USuspenseCoreUIManager* UIManager = USuspenseCoreUIManager::Get(this))
	{
		UIManager->HideTooltip();
	}
}

//==================================================================
// Multi-Cell Item Support
//==================================================================

UPanelWidget* USuspenseCoreInventoryWidget::GetActiveGridPanel() const
{
	// Prefer GridPanel (supports spanning) over UniformGridPanel
	if (SlotGridPanel)
	{
		// Cast away const for bUsingGridPanel - it's effectively configuration
		const_cast<USuspenseCoreInventoryWidget*>(this)->bUsingGridPanel = true;
		return SlotGridPanel;
	}

	if (SlotGrid)
	{
		const_cast<USuspenseCoreInventoryWidget*>(this)->bUsingGridPanel = false;
		return SlotGrid;
	}

	return nullptr;
}

void USuspenseCoreInventoryWidget::UpdateGridSlotSpan(int32 AnchorSlotIndex, const FIntPoint& ItemSize, bool bIsRotated)
{
	// Only works with GridPanel
	if (!bUsingGridPanel || !SlotGridPanel)
	{
		return;
	}

	// Get cached GridSlot
	TObjectPtr<UGridSlot>* FoundSlot = CachedGridSlots.Find(AnchorSlotIndex);
	if (!FoundSlot || !(*FoundSlot))
	{
		UE_LOG(LogTemp, Warning, TEXT("UpdateGridSlotSpan: No cached GridSlot for slot %d"), AnchorSlotIndex);
		return;
	}

	UGridSlot* GSlot = *FoundSlot;

	// Calculate effective size (considering rotation)
	FIntPoint EffectiveSize = bIsRotated ? FIntPoint(ItemSize.Y, ItemSize.X) : ItemSize;

	// Apply span
	GSlot->SetColumnSpan(FMath::Max(1, EffectiveSize.X));
	GSlot->SetRowSpan(FMath::Max(1, EffectiveSize.Y));

	UE_LOG(LogTemp, Log, TEXT("UpdateGridSlotSpan: Slot %d span set to %dx%d (rotated=%d)"),
		AnchorSlotIndex, EffectiveSize.X, EffectiveSize.Y, bIsRotated ? 1 : 0);
}

void USuspenseCoreInventoryWidget::ResetGridSlotSpan(int32 SlotIndex)
{
	// Only works with GridPanel
	if (!bUsingGridPanel || !SlotGridPanel)
	{
		return;
	}

	// Get cached GridSlot
	TObjectPtr<UGridSlot>* FoundSlot = CachedGridSlots.Find(SlotIndex);
	if (!FoundSlot || !(*FoundSlot))
	{
		return;
	}

	UGridSlot* GSlot = *FoundSlot;

	// Reset to 1x1
	GSlot->SetColumnSpan(1);
	GSlot->SetRowSpan(1);
}

void USuspenseCoreInventoryWidget::UpdateMultiCellSlotVisibility(int32 AnchorSlotIndex, const FIntPoint& ItemSize, bool bIsRotated)
{
	// Calculate effective size
	FIntPoint EffectiveSize = bIsRotated ? FIntPoint(ItemSize.Y, ItemSize.X) : ItemSize;

	// Get anchor grid position
	FIntPoint AnchorGridPos = SlotIndexToGridPos(AnchorSlotIndex);

	// Iterate all slots occupied by this item
	for (int32 DY = 0; DY < EffectiveSize.Y; ++DY)
	{
		for (int32 DX = 0; DX < EffectiveSize.X; ++DX)
		{
			FIntPoint CurrentPos = AnchorGridPos + FIntPoint(DX, DY);
			int32 CurrentSlotIndex = GridPosToSlotIndex(CurrentPos);

			if (CurrentSlotIndex == INDEX_NONE || CurrentSlotIndex >= SlotWidgets.Num())
			{
				continue;
			}

			if (CurrentSlotIndex == AnchorSlotIndex)
			{
				// Anchor slot - visible with span
				if (SlotWidgets[CurrentSlotIndex])
				{
					SlotWidgets[CurrentSlotIndex]->SetVisibility(ESlateVisibility::Visible);
				}
				UpdateGridSlotSpan(CurrentSlotIndex, ItemSize, bIsRotated);
			}
			else
			{
				// Non-anchor slot - hidden (covered by anchor's span)
				if (SlotWidgets[CurrentSlotIndex])
				{
					SlotWidgets[CurrentSlotIndex]->SetVisibility(ESlateVisibility::Hidden);
				}
				ResetGridSlotSpan(CurrentSlotIndex);
			}
		}
	}
}

//==================================================================
// Ammo to Magazine Drag & Drop
//==================================================================

bool USuspenseCoreInventoryWidget::TryHandleAmmoToMagazineDrop(const FSuspenseCoreDragData& DragData, int32 TargetSlot)
{
	// Get UIManager for checking item types
	USuspenseCoreUIManager* UIManager = USuspenseCoreUIManager::Get(this);
	if (!UIManager)
	{
		return false;
	}

	// Check if dragged item is ammo
	if (!IsAmmoItem(DragData.Item))
	{
		return false;
	}

	// Get item at target slot
	if (!IsBoundToProvider())
	{
		return false;
	}

	ISuspenseCoreUIDataProvider* ProviderInterface = GetBoundProvider().GetInterface();
	if (!ProviderInterface)
	{
		return false;
	}

	// Resolve anchor slot for multi-cell items
	int32 AnchorSlot = SlotToAnchorMap.Contains(TargetSlot) ? SlotToAnchorMap[TargetSlot] : TargetSlot;

	FSuspenseCoreItemUIData TargetItemData;
	if (!ProviderInterface->GetItemUIDataAtSlot(AnchorSlot, TargetItemData))
	{
		return false; // No item at target slot
	}

	// Check if target item is a magazine
	if (!UIManager->IsMagazineItem(TargetItemData))
	{
		return false; // Target is not a magazine
	}

	// We have ammo being dropped onto a magazine - trigger loading!
	UE_LOG(LogTemp, Log, TEXT("TryHandleAmmoToMagazineDrop: Dropping ammo %s onto magazine %s"),
		*DragData.Item.ItemID.ToString(), *TargetItemData.ItemID.ToString());

	// Publish ammo load request event
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		UE_LOG(LogTemp, Warning, TEXT("TryHandleAmmoToMagazineDrop: EventBus not available"));
		return false;
	}

	using namespace SuspenseCoreEquipmentTags::Magazine;

	FSuspenseCoreEventData EventData;
	EventData.SetString(TEXT("MagazineInstanceID"), TargetItemData.InstanceID.ToString());
	EventData.SetString(TEXT("MagazineID"), TargetItemData.ItemID.ToString());
	EventData.SetString(TEXT("AmmoID"), DragData.Item.ItemID.ToString());
	EventData.SetString(TEXT("AmmoInstanceID"), DragData.Item.InstanceID.ToString());
	EventData.SetInt(TEXT("Quantity"), DragData.Item.Quantity > 0 ? DragData.Item.Quantity : 1);
	EventData.SetString(TEXT("SourceContainerID"), DragData.SourceContainerID.ToString());
	EventData.SetInt(TEXT("SourceSlot"), DragData.SourceSlot);

	EventBus->Publish(TAG_Equipment_Event_Ammo_LoadRequested, EventData);

	UE_LOG(LogTemp, Log, TEXT("TryHandleAmmoToMagazineDrop: Published Ammo.LoadRequested event"));
	return true;
}

bool USuspenseCoreInventoryWidget::IsAmmoItem(const FSuspenseCoreItemUIData& ItemData) const
{
	// Check for ammo type tags
	static const FGameplayTag AmmoTag = FGameplayTag::RequestGameplayTag(FName("Item.Ammo"), false);
	static const FGameplayTag AmmoCategoryTag = FGameplayTag::RequestGameplayTag(FName("Item.Category.Ammo"), false);

	if (AmmoTag.IsValid() && ItemData.ItemType.MatchesTag(AmmoTag))
	{
		return true;
	}

	if (AmmoCategoryTag.IsValid() && ItemData.ItemType.MatchesTag(AmmoCategoryTag))
	{
		return true;
	}

	// Fallback: check if tag string contains "Ammo"
	FString TagString = ItemData.ItemType.ToString();
	if (TagString.Contains(TEXT("Ammo"), ESearchCase::IgnoreCase))
	{
		return true;
	}

	return false;
}
