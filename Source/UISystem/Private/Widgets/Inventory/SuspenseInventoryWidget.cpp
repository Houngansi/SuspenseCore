// MedComInventoryWidget.cpp
// Copyright Suspense Team Team. All Rights Reserved.

#include "Widgets/Inventory/SuspenseInventoryWidget.h"
#include "Widgets/Inventory/SuspenseInventorySlotWidget.h"
#include "Widgets/DragDrop/SuspenseDragDropOperation.h"
#include "DragDrop/SuspenseDragDropHandler.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "Blueprint/WidgetTree.h"
#include "Delegates/EventDelegateManager.h"
#include "TimerManager.h"
#include "Engine/World.h"

// Performance constants
static constexpr int32 MAX_SLOTS_PER_FRAME = 100;
static constexpr float GRID_UPDATE_THROTTLE = 0.033f; // 30 FPS for grid updates

USuspenseInventoryWidget::USuspenseInventoryWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Set default slot widget class
    static ConstructorHelpers::FClassFinder<USuspenseBaseSlotWidget> SlotWidgetClassFinder(
        TEXT("/Game/MEDCOM/UI/Inventory/W_SlotInventory"));
    if (SlotWidgetClassFinder.Succeeded())
    {
        SlotWidgetClass = SlotWidgetClassFinder.Class;
        InventorySlotClass = SlotWidgetClassFinder.Class;
    }

    // Default grid settings
    GridColumns = 10;
    GridRows = 5;
    DefaultGridColumns = 10;
    DefaultGridRows = 5;
    CellSize = 48.0f;
    DefaultCellSize = 48.0f;
    CellPadding = 2.0f;
    
    // Display settings
    bShowWeight = true;
    WeightWarningThreshold = 0.75f;
    bShowGridSnapVisualization = true;
    GridSnapVisualizationStrength = 0.5f;
    
    // Initialize state flags
    bGridInitialized = false;
    bIsFullyInitialized = false;
    LastGridUpdateTime = 0.0f;
    GridUpdateCounter = 0;
    
    // Set container type
    ContainerType = FGameplayTag::RequestGameplayTag(TEXT("Container.Inventory"));
    
    // Enable optimizations
    bEnableSmartDropZones = true;
    
    UE_LOG(LogTemp, Log, TEXT("[InventoryWidget] Constructor: Optimized grid %dx%d"), 
        GridColumns, GridRows);
}

void USuspenseInventoryWidget::NativePreConstruct()
{
    Super::NativePreConstruct();
    
    // Apply Blueprint overrides if specified
    if (DefaultGridColumns > 0)
    {
        GridColumns = DefaultGridColumns;
    }
    if (DefaultGridRows > 0)
    {
        GridRows = DefaultGridRows;
    }
    if (DefaultCellSize > 0.0f)
    {
        CellSize = DefaultCellSize;
    }
    if (InventorySlotClass)
    {
        SlotWidgetClass = InventorySlotClass;
    }
    
    // Pre-configure grid panel if available
    if (InventoryGrid)
    {
        InventoryGrid->ClearChildren();
        
        // Reserve grid layout
        for (int32 i = 0; i < GridColumns; i++)
        {
            InventoryGrid->SetColumnFill(i, 1.0f);
        }
        
        for (int32 i = 0; i < GridRows; i++)
        {
            InventoryGrid->SetRowFill(i, 1.0f);
        }
    }
}

void USuspenseInventoryWidget::NativeConstruct()
{
    UE_LOG(LogTemp, Log, TEXT("[%s] NativeConstruct START - Optimized initialization"), *GetName());
    
    // Critical: Set visibility BEFORE everything else
    SetVisibility(ESlateVisibility::Visible);
    SetRenderOpacity(1.0f);
    SetIsEnabled(true);
    
    // Call base implementation
    Super::NativeConstruct();
    
    // Validate critical components
    if (!ValidateCriticalComponents())
    {
        UE_LOG(LogTemp, Error, TEXT("[%s] Critical components validation failed!"), *GetName());
        return;
    }
    
    // Auto-bind components if needed
    AutoBindComponents();
    
    // Bind button delegates
    if (CloseButton)
    {
        CloseButton->OnClicked.RemoveDynamic(this, &USuspenseInventoryWidget::OnCloseButtonClicked);
        CloseButton->OnClicked.AddDynamic(this, &USuspenseInventoryWidget::OnCloseButtonClicked);
    }
    
    if (SortButton)
    {
        SortButton->OnClicked.RemoveDynamic(this, &USuspenseInventoryWidget::OnSortButtonClicked);
        SortButton->OnClicked.AddDynamic(this, &USuspenseInventoryWidget::OnSortButtonClicked);
    }

    // Subscribe to events
    SubscribeToInventoryEvents();
    
    // Request initial data refresh (deferred)
    if (UWorld* World = GetWorld())
    {
        FTimerHandle InitTimerHandle;
        World->GetTimerManager().SetTimerForNextTick([this]()
        {
            RequestInventoryRefresh();
        });
    }
    
    // Set widget focusable for keyboard input
    SetIsFocusable(true);
    
    UE_LOG(LogTemp, Log, TEXT("[%s] NativeConstruct END - Ready for container initialization"), *GetName());
}

void USuspenseInventoryWidget::RequestInventoryRefresh()
{
    UE_LOG(LogTemp, Log, TEXT("[InventoryWidget] RequestInventoryRefresh called"));
    
    // Call base implementation
    RequestDataRefresh_Implementation();
}

bool USuspenseInventoryWidget::ValidateSlotsPanel() const
{
    // Override base class validation to check for GridPanel specifically
    UPanelWidget* Panel = GetSlotsPanel();
    
    if (!Panel)
    {
        UE_LOG(LogTemp, Error, TEXT("[%s] GetSlotsPanel() returned null!"), *GetName());
        return false;
    }
    
    // For inventory, we specifically need a GridPanel
    UGridPanel* GridPanel = Cast<UGridPanel>(Panel);
    if (!GridPanel)
    {
        UE_LOG(LogTemp, Error, TEXT("[%s] Inventory panel must be a GridPanel, but got %s"), 
            *GetName(), *Panel->GetClass()->GetName());
        return false;
    }
    
    UE_LOG(LogTemp, Log, TEXT("[%s] Inventory grid panel validated: %s"), *GetName(), *Panel->GetName());
    return true;
}

FReply USuspenseInventoryWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    // Handle rotation key
    if (InKeyEvent.GetKey() == EKeys::R && !InKeyEvent.IsRepeat())
    {
        RequestRotateSelectedItem();
        return FReply::Handled();
    }
    
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void USuspenseInventoryWidget::InitializeContainer_Implementation(const FContainerUIData& ContainerData)
{
    UE_LOG(LogTemp, Log, TEXT("[%s] InitializeContainer - Grid size %dx%d, %d items"), 
        *GetName(), ContainerData.GridSize.X, ContainerData.GridSize.Y, ContainerData.Items.Num());
    
    // Validate critical components
    if (!ValidateCriticalComponents())
    {
        UE_LOG(LogTemp, Error, TEXT("[%s] Cannot initialize - critical components missing!"), *GetName());
        return;
    }
    
    // Validate container data
    if (ContainerData.GridSize.X <= 0 || ContainerData.GridSize.Y <= 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[%s] Invalid grid size in ContainerData: %dx%d"), 
            *GetName(), ContainerData.GridSize.X, ContainerData.GridSize.Y);
        return;
    }
    
    // Store container data
    CurrentContainerData = ContainerData;
    
    // Update dimensions from container data
    GridColumns = ContainerData.GridSize.X;
    GridRows = ContainerData.GridSize.Y;
    
    // Create slots if not already created (lazy initialization)
    if (!bGridInitialized)
    {
        UE_LOG(LogTemp, Log, TEXT("[%s] Creating inventory slots for first time"), *GetName());
        CreateSlotsOptimized();
        bGridInitialized = true;
    }
    else
    {
        // Check if size changed
        int32 ExpectedSlots = GridColumns * GridRows;
        if (SlotWidgets.Num() != ExpectedSlots)
        {
            UE_LOG(LogTemp, Warning, TEXT("[%s] Grid size changed from %d to %d slots, recreating"), 
                *GetName(), SlotWidgets.Num(), ExpectedSlots);
            ClearSlots();
            CreateSlotsOptimized();
        }
    }
    
    // Apply initial data with optimization
    ApplyDifferentialSlotUpdates(ContainerData);
    
    // Update UI elements
    if (InventoryTitle)
    {
        InventoryTitle->SetText(ContainerData.DisplayName);
        InventoryTitle->SetColorAndOpacity(FLinearColor::White);
    }
    
    UpdateWeightDisplay();
    UpdateSlotOccupancyMap();
    UpdateGridLayoutForMultiSlotItems();
    
    // Mark as fully initialized
    bIsFullyInitialized = true;
    bIsInitialized = true;
    
    // Force layout update
    if (InventoryGrid)
    {
        InventoryGrid->ForceLayoutPrepass();
    }
    
    UE_LOG(LogTemp, Log, TEXT("[%s] Inventory initialized with %d slots, %d items"), 
        *GetName(), SlotWidgets.Num(), ContainerData.Items.Num());
}

void USuspenseInventoryWidget::UpdateContainer_Implementation(const FContainerUIData& ContainerData)
{
    if (!bIsInitialized || !bIsFullyInitialized)
    {
        return;
    }
    
    GridUpdateCounter++;
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("[%s] UpdateContainer #%d with %d items"), 
        *GetName(), GridUpdateCounter, ContainerData.Items.Num());
    
    // Store new data
    CurrentContainerData = ContainerData;
    
    // Apply updates
    ApplyDifferentialSlotUpdates(ContainerData);
    
    // Update inventory-specific elements
    UpdateWeightDisplay();
    UpdateSlotOccupancyMap();
    UpdateGridLayoutForMultiSlotItems();
}

void USuspenseInventoryWidget::SetGridDimensions(int32 Columns, int32 Rows)
{
    if (Columns > 0 && Rows > 0)
    {
        GridColumns = Columns;
        GridRows = Rows;
        
        // Используем InventoryGrid напрямую - он уже UGridPanel*
        if (InventoryGrid)
        {
            // Update column fills
            for (int32 i = 0; i < GridColumns; i++)
            {
                InventoryGrid->SetColumnFill(i, 1.0f);
            }
            
            // Update row fills
            for (int32 i = 0; i < GridRows; i++)
            {
                InventoryGrid->SetRowFill(i, 1.0f);
            }
        }
        
        UE_LOG(LogTemp, Log, TEXT("[%s] Grid dimensions set to %dx%d"), 
            *GetName(), GridColumns, GridRows);
    }
}

void USuspenseInventoryWidget::CreateSlots()
{
    // Redirect to optimized version
    CreateSlotsOptimized();
}

void USuspenseInventoryWidget::CreateSlotsOptimized()
{
    UE_LOG(LogTemp, Log, TEXT("[%s] CreateSlotsOptimized START"), *GetName());
    
    if (!InventoryGrid)
    {
        UE_LOG(LogTemp, Error, TEXT("[%s] No valid GridPanel found!"), *GetName());
        return;
    }
    
    // Clear existing
    ClearSlots();
    InventoryGrid->ClearChildren();
    
    // Calculate total slots
    int32 TotalSlots = GridColumns * GridRows;
    
    // Reserve memory
    SlotWidgets.Reserve(TotalSlots);
    CachedGridSlotData.Reserve(TotalSlots);
    
    UE_LOG(LogTemp, Log, TEXT("[%s] Creating %d slots (%dx%d grid)"), 
        *GetName(), TotalSlots, GridColumns, GridRows);
    
    // Validate slot class
    TSubclassOf<USuspenseBaseSlotWidget> SlotClass = SlotWidgetClass;
    if (!SlotClass)
    {
        SlotClass = USuspenseInventorySlotWidget::StaticClass();
    }
    
    // Make grid panel visible
    InventoryGrid->SetVisibility(ESlateVisibility::Visible);
    
    // Create slots in batches to avoid hitches
    int32 SlotsCreated = 0;
    
    for (int32 Y = 0; Y < GridRows; Y++)
    {
        for (int32 X = 0; X < GridColumns; X++)
        {
            int32 SlotIndex = Y * GridColumns + X;
            
            // Create widget slot
            USuspenseInventorySlotWidget* NewSlot = CreateWidget<USuspenseInventorySlotWidget>(this, SlotClass);
            if (!NewSlot)
            {
                UE_LOG(LogTemp, Error, TEXT("[%s] Failed to create slot %d"), *GetName(), SlotIndex);
                continue;
            }
            
            // Set owning container BEFORE any other initialization
            NewSlot->SetOwningContainer(this);
            
            // Initialize slot
            NewSlot->SetVisibility(ESlateVisibility::Visible);
            NewSlot->InitializeInventorySlot(CellSize);
            NewSlot->SetGridCoordinates(X, Y);
            
            // Initialize slot data
            FSlotUIData SlotData;
            SlotData.SlotIndex = SlotIndex;
            SlotData.GridX = X;
            SlotData.GridY = Y;
            SlotData.bIsOccupied = false;
            SlotData.bIsAnchor = false;
            SlotData.bIsPartOfItem = false;
            SlotData.AllowedItemTypes = CurrentContainerData.AllowedItemTypes;
            
            // Initialize through interface
            if (NewSlot->GetClass()->ImplementsInterface(USuspenseSlotUI::StaticClass()))
            {
                ISuspenseSlotUIInterface::Execute_InitializeSlot(NewSlot, SlotData, FItemUIData());
            }
            
            // Add to grid
            UGridSlot* GridSlot = InventoryGrid->AddChildToGrid(NewSlot);
            if (GridSlot)
            {
                GridSlot->SetColumn(X);
                GridSlot->SetRow(Y);
                GridSlot->SetPadding(FMargin(CellPadding));
                GridSlot->SetHorizontalAlignment(HAlign_Fill);
                GridSlot->SetVerticalAlignment(VAlign_Fill);
                
                // Cache grid slot data
                FCachedGridSlotData& CachedData = CachedGridSlotData.FindOrAdd(SlotIndex);
                CachedData.GridSlot = GridSlot;
                CachedData.CurrentSpan = FIntPoint(1, 1);
                CachedData.bIsVisible = true;
                
                SlotsCreated++;
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("[%s] Failed to add slot %d to grid"), 
                    *GetName(), SlotIndex);
                NewSlot->RemoveFromParent();
                continue;
            }
            
            // Store reference
            SlotWidgets.Add(SlotIndex, NewSlot);
        }
    }
    
    // Single layout pass at the end
    InventoryGrid->ForceLayoutPrepass();
    
    UE_LOG(LogTemp, Log, TEXT("[%s] CreateSlotsOptimized END: %d slots created"), 
        *GetName(), SlotsCreated);
    
    // Broadcast event
    OnInventorySlotsNeeded.Broadcast(SlotsCreated);
}

void USuspenseInventoryWidget::ApplyDifferentialSlotUpdates(const FContainerUIData& ContainerData)
{
    // Create lookup maps for efficiency
    TMap<int32, const FSlotUIData*> SlotDataMap;
    TMap<FGuid, const FItemUIData*> ItemDataMap;
    TMap<int32, FItemUIData> AnchorToItemMap;
    
    // Build slot lookup
    for (const FSlotUIData& SlotData : ContainerData.Slots)
    {
        SlotDataMap.Add(SlotData.SlotIndex, &SlotData);
    }
    
    // Build item lookup and anchor map
    for (const FItemUIData& ItemData : ContainerData.Items)
    {
        ItemDataMap.Add(ItemData.ItemInstanceID, &ItemData);
        if (ItemData.AnchorSlotIndex != INDEX_NONE)
        {
            AnchorToItemMap.Add(ItemData.AnchorSlotIndex, ItemData);
        }
    }
    
    // Track multi-slot item occupancy
    TSet<int32> OccupiedSlots;
    
    // Process all items and calculate occupied slots
    for (const FItemUIData& Item : ContainerData.Items)
    {
        if (Item.AnchorSlotIndex == INDEX_NONE)
        {
            continue;
        }
        
        // Calculate all slots occupied by this item
        FIntPoint EffectiveSize = Item.bIsRotated ? 
            FIntPoint(Item.GridSize.Y, Item.GridSize.X) : 
            Item.GridSize;
        
        int32 StartX = Item.AnchorSlotIndex % GridColumns;
        int32 StartY = Item.AnchorSlotIndex / GridColumns;
        
        for (int32 Y = 0; Y < EffectiveSize.Y; Y++)
        {
            for (int32 X = 0; X < EffectiveSize.X; X++)
            {
                int32 SlotX = StartX + X;
                int32 SlotY = StartY + Y;
                
                if (SlotX < GridColumns && SlotY < GridRows)
                {
                    int32 SlotIndex = SlotY * GridColumns + SlotX;
                    OccupiedSlots.Add(SlotIndex);
                }
            }
        }
    }
    
    // Apply differential updates to slots
    for (const auto& SlotPair : SlotWidgets)
    {
        int32 SlotIndex = SlotPair.Key;
        USuspenseInventorySlotWidget* SlotWidget = Cast<USuspenseInventorySlotWidget>(SlotPair.Value);
        
        if (!SlotWidget)
        {
            continue;
        }
        
        const FSlotUIData** SlotDataPtr = SlotDataMap.Find(SlotIndex);
        if (!SlotDataPtr)
        {
            continue;
        }
        
        const FSlotUIData& SlotData = **SlotDataPtr;
        
        // Check if this is an anchor slot with item
        if (AnchorToItemMap.Contains(SlotIndex))
        {
            const FItemUIData& ItemData = AnchorToItemMap[SlotIndex];
            
            // Update slot with item data
            if (SlotWidget->GetClass()->ImplementsInterface(USuspenseSlotUI::StaticClass()))
            {
                ISuspenseSlotUIInterface::Execute_UpdateSlot(SlotWidget, SlotData, ItemData);
            }
            
            // Update grid span
            UpdateGridSlotSpan(SlotWidget, ItemData);
            
            // Ensure visibility
            SlotWidget->SetVisibility(ESlateVisibility::Visible);
        }
        else if (OccupiedSlots.Contains(SlotIndex) && !SlotData.bIsAnchor)
        {
            // This slot is occupied by multi-slot item but not anchor
            SlotWidget->SetVisibility(ESlateVisibility::Hidden);
            
            // Reset span
            if (UGridSlot* GridSlot = Cast<UGridSlot>(SlotWidget->Slot))
            {
                GridSlot->SetColumnSpan(1);
                GridSlot->SetRowSpan(1);
            }
        }
        else
        {
            // Empty slot
            SlotWidget->SetVisibility(ESlateVisibility::Visible);
            
            // Update as empty
            if (SlotWidget->GetClass()->ImplementsInterface(USuspenseSlotUI::StaticClass()))
            {
                ISuspenseSlotUIInterface::Execute_UpdateSlot(SlotWidget, SlotData, FItemUIData());
            }
            
            // Reset span
            if (UGridSlot* GridSlot = Cast<UGridSlot>(SlotWidget->Slot))
            {
                GridSlot->SetColumnSpan(1);
                GridSlot->SetRowSpan(1);
            }
        }
    }
}

void USuspenseInventoryWidget::UpdateGridLayoutForMultiSlotItems()
{
    // Update cached grid slot data for multi-slot items
    for (const FItemUIData& Item : CurrentContainerData.Items)
    {
        if (Item.AnchorSlotIndex == INDEX_NONE)
        {
            continue;
        }
        
        FIntPoint EffectiveSize = Item.bIsRotated ? 
            FIntPoint(Item.GridSize.Y, Item.GridSize.X) : 
            Item.GridSize;
        
        if (EffectiveSize.X > 1 || EffectiveSize.Y > 1)
        {
            // This is a multi-slot item
            if (USuspenseInventorySlotWidget* AnchorSlot = Cast<USuspenseInventorySlotWidget>(GetSlotWidget(Item.AnchorSlotIndex)))
            {
                if (UGridSlot* GridSlot = Cast<UGridSlot>(AnchorSlot->Slot))
                {
                    // Update span in cached data
                    FCachedGridSlotData& CachedData = CachedGridSlotData.FindOrAdd(Item.AnchorSlotIndex);
                    CachedData.CurrentSpan = EffectiveSize;
                    CachedData.LastItemInstance = Item.ItemInstanceID;
                    
                    // Apply span
                    GridSlot->SetColumnSpan(EffectiveSize.X);
                    GridSlot->SetRowSpan(EffectiveSize.Y);
                }
            }
        }
    }
}

void USuspenseInventoryWidget::UpdateSlotWidget(int32 SlotIndex, const FSlotUIData& SlotData, const FItemUIData& ItemData)
{
    // Get slot widget
    USuspenseInventorySlotWidget* SlotWidget = Cast<USuspenseInventorySlotWidget>(GetSlotWidget(SlotIndex));
    if (!SlotWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("[%s] UpdateSlotWidget: Slot %d not found!"), *GetName(), SlotIndex);
        return;
    }
    
    // Ensure owning container is set
    if (!SlotWidget->GetOwningContainer())
    {
        SlotWidget->SetOwningContainer(this);
    }
    
    // Get GridSlot for span management
    UGridSlot* GridSlot = Cast<UGridSlot>(SlotWidget->Slot);
    if (!GridSlot)
    {
        UE_LOG(LogTemp, Error, TEXT("[%s] UpdateSlotWidget: GridSlot not found for slot %d"), 
            *GetName(), SlotIndex);
        return;
    }
    
    // Process based on slot type
    if (SlotData.bIsAnchor && ItemData.IsValid())
    {
        // Anchor slot with item
        UE_LOG(LogTemp, VeryVerbose, TEXT("[%s] Updating anchor slot %d with item %s"), 
            *GetName(), SlotIndex, *ItemData.ItemID.ToString());
        
        // Update slot data
        if (SlotWidget->GetClass()->ImplementsInterface(USuspenseSlotUI::StaticClass()))
        {
            ISuspenseSlotUIInterface::Execute_UpdateSlot(SlotWidget, SlotData, ItemData);
        }
        
        // Update grid span
        UpdateGridSlotSpan(SlotWidget, ItemData);
        
        // Ensure slot is visible
        SlotWidget->SetVisibility(ESlateVisibility::Visible);
    }
    else if (SlotData.bIsPartOfItem && !SlotData.bIsAnchor)
    {
        // Slot is part of multi-slot item but not anchor
        UE_LOG(LogTemp, VeryVerbose, TEXT("[%s] Slot %d is part of item, hiding"), 
            *GetName(), SlotIndex);
        
        // Hide slot
        SlotWidget->SetVisibility(ESlateVisibility::Hidden);
        
        // Reset span
        GridSlot->SetColumnSpan(1);
        GridSlot->SetRowSpan(1);
        
        // Clear slot data
        if (SlotWidget->GetClass()->ImplementsInterface(USuspenseSlotUI::StaticClass()))
        {
            ISuspenseSlotUIInterface::Execute_UpdateSlot(SlotWidget, SlotData, FItemUIData());
        }
    }
    else
    {
        // Regular empty slot
        UE_LOG(LogTemp, VeryVerbose, TEXT("[%s] Slot %d is empty"), *GetName(), SlotIndex);
        
        // Ensure slot is visible
        SlotWidget->SetVisibility(ESlateVisibility::Visible);
        
        // Update as empty slot
        if (SlotWidget->GetClass()->ImplementsInterface(USuspenseSlotUI::StaticClass()))
        {
            ISuspenseSlotUIInterface::Execute_UpdateSlot(SlotWidget, SlotData, FItemUIData());
        }
        
        // Reset span to 1x1
        GridSlot->SetColumnSpan(1);
        GridSlot->SetRowSpan(1);
    }
}

void USuspenseInventoryWidget::UpdateGridSlotSpan(USuspenseInventorySlotWidget* SlotWidget, const FItemUIData& ItemData)
{
    if (!SlotWidget)
    {
        return;
    }
    
    UGridSlot* GridSlot = Cast<UGridSlot>(SlotWidget->Slot);
    if (!GridSlot)
    {
        return;
    }
    
    // Calculate effective size
    FIntPoint EffectiveSize = ItemData.bIsRotated ? 
        FIntPoint(ItemData.GridSize.Y, ItemData.GridSize.X) : 
        ItemData.GridSize;
    
    // Apply span
    GridSlot->SetColumnSpan(FMath::Max(1, EffectiveSize.X));
    GridSlot->SetRowSpan(FMath::Max(1, EffectiveSize.Y));
    
    // Update cached data - FIXED: Use Execute_ for interface call
    int32 SlotIndex = INDEX_NONE;
    if (SlotWidget->GetClass()->ImplementsInterface(USuspenseSlotUI::StaticClass()))
    {
        SlotIndex = ISuspenseSlotUIInterface::Execute_GetSlotIndex(SlotWidget);
    }
    
    if (SlotIndex != INDEX_NONE)
    {
        FCachedGridSlotData& CachedData = CachedGridSlotData.FindOrAdd(SlotIndex);
        CachedData.CurrentSpan = EffectiveSize;
        CachedData.LastItemInstance = ItemData.ItemInstanceID;
    }
}

void USuspenseInventoryWidget::SetVisibility(ESlateVisibility InVisibility)
{
    Super::SetVisibility(InVisibility);
    
    // If widget becomes visible and is initialized
    if (InVisibility == ESlateVisibility::Visible && bIsFullyInitialized)
    {
        UE_LOG(LogTemp, Log, TEXT("[InventoryWidget] Becoming visible, requesting refresh"));
        
        // Request data update
        FTimerHandle RefreshHandle;
        GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
        {
            RequestInventoryRefresh();
        });
    }
}

FSlotValidationResult USuspenseInventoryWidget::CanAcceptDrop_Implementation(
    const UDragDropOperation* DragOperation, 
    int32 TargetSlotIndex) const
{
    // Basic validation
    const USuspenseDragDropOperation* MedComDragOp = Cast<USuspenseDragDropOperation>(DragOperation);
    if (!MedComDragOp || !MedComDragOp->IsValidOperation())
    {
        return FSlotValidationResult::Failure(
            FText::FromString(TEXT("Invalid drag operation")));
    }
    
    // Get operation data
    const FDragDropUIData& DragData = MedComDragOp->GetDragData();
    
    // Check slot coordinates
    int32 GridX = 0, GridY = 0;
    if (!GetGridCoordsFromSlotIndex(TargetSlotIndex, GridX, GridY))
    {
        return FSlotValidationResult::Failure(
            FText::FromString(TEXT("Invalid target slot")));
    }
    
    // Get item size considering rotation
    const FIntPoint EffectiveSize = DragData.ItemData.bIsRotated
        ? FIntPoint(DragData.ItemData.GridSize.Y, DragData.ItemData.GridSize.X)
        : DragData.ItemData.GridSize;
    
    // Check if item fits within grid bounds
    if (!IsWithinGridBounds(GridX, GridY, EffectiveSize.X, EffectiveSize.Y))
    {
        return FSlotValidationResult::Failure(
            FText::FromString(TEXT("Item doesn't fit in grid")));
    }
    
    // Check if all required slots are free or occupied by the same item
    for (int32 Y = 0; Y < EffectiveSize.Y; Y++)
    {
        for (int32 X = 0; X < EffectiveSize.X; X++)
        {
            const int32 CheckSlotIndex = (GridY + Y) * GridColumns + (GridX + X);
            if (const USuspenseBaseSlotWidget* SlotWidget = GetSlotWidget(CheckSlotIndex))
            {
                bool bIsOccupied = false;
                if (SlotWidget->GetClass()->ImplementsInterface(USuspenseSlotUI::StaticClass()))
                {
                    bIsOccupied = ISuspenseSlotUIInterface::Execute_IsOccupied(SlotWidget);
                }

                if (bIsOccupied)
                {
                    FGuid OccupyingItemID;
                    if (SlotWidget->GetClass()->ImplementsInterface(USuspenseSlotUI::StaticClass()))
                    {
                        OccupyingItemID = ISuspenseSlotUIInterface::Execute_GetItemInstanceID(SlotWidget);
                    }

                    // occupied by another item → fail
                    if (OccupyingItemID.IsValid() && OccupyingItemID != DragData.ItemData.ItemInstanceID)
                    {
                        return FSlotValidationResult::Failure(
                            FText::FromString(TEXT("Slot is occupied")));
                    }
                }
            }
        }
    }
    
    // All checks passed
    return FSlotValidationResult::Success();
}

FSmartDropZone USuspenseInventoryWidget::FindBestDropZone(
    const FVector2D& ScreenPosition,
    const FIntPoint& ItemSize,
    bool bIsRotated) const
{
    FSmartDropZone BestZone;
    BestZone.Distance = SmartDropRadius;
    
    // Convert screen position to grid coordinates
    FIntPoint GridPos = ScreenToGridCoordinates(ScreenPosition);
    
    // Search radius in grid cells
    int32 SearchRadius = FMath::CeilToInt(SmartDropRadius / CellSize);
    
    // Find best valid position
    for (int32 dy = -SearchRadius; dy <= SearchRadius; dy++)
    {
        for (int32 dx = -SearchRadius; dx <= SearchRadius; dx++)
        {
            int32 TestX = GridPos.X + dx;
            int32 TestY = GridPos.Y + dy;
            
            // Check if position is valid
            if (!IsValidPlacementPosition(TestX, TestY, ItemSize))
            {
                continue;
            }
            
            int32 SlotIndex = GetSlotIndexFromGridCoords(TestX, TestY);
            
            // Calculate distance to test position
            FBox2D CellBounds = GetGridCellScreenBounds(TestX, TestY);
            FVector2D CellCenter = CellBounds.GetCenter();
            float Distance = FVector2D::Distance(ScreenPosition, CellCenter);
            
            if (Distance < BestZone.Distance)
            {
                BestZone.SlotIndex = SlotIndex;
                BestZone.Distance = Distance;
                BestZone.FeedbackPosition = CellCenter;
                BestZone.bIsValid = true;
                
                // Calculate snap strength
                float NormalizedDistance = Distance / SmartDropRadius;
                BestZone.SnapStrength = FMath::Pow(1.0f - NormalizedDistance, 2.0f);
            }
        }
    }
    
    return BestZone;
}

bool USuspenseInventoryWidget::CalculateOccupiedSlots(
    int32 TargetSlot, 
    FIntPoint ItemSize, 
    bool bIsRotated, 
    TArray<int32>& OutOccupiedSlots) const
{
    OutOccupiedSlots.Empty();
    
    // Get starting grid coordinates
    int32 StartX, StartY;
    if (!GetGridCoordsFromSlotIndex(TargetSlot, StartX, StartY))
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryWidget] Invalid target slot %d"), TargetSlot);
        return false;
    }
    
    // ВАЖНО: Сначала проверяем, помещается ли предмет в сетку
    bool bFitsInGrid = IsWithinGridBounds(StartX, StartY, ItemSize.X, ItemSize.Y);
    
    // Calculate all slots that would be occupied
    for (int32 Y = 0; Y < ItemSize.Y; Y++)
    {
        for (int32 X = 0; X < ItemSize.X; X++)
        {
            int32 SlotX = StartX + X;
            int32 SlotY = StartY + Y;
            
            // Add slot only if it's within bounds
            if (SlotX >= 0 && SlotX < GridColumns && SlotY >= 0 && SlotY < GridRows)
            {
                int32 SlotIndex = SlotY * GridColumns + SlotX;
                OutOccupiedSlots.Add(SlotIndex);
            }
        }
    }
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("[InventoryWidget] CalculateOccupiedSlots: Target=%d, Size=%dx%d, FitsInGrid=%s, OccupiedCount=%d"), 
        TargetSlot, ItemSize.X, ItemSize.Y, 
        bFitsInGrid ? TEXT("YES") : TEXT("NO"), 
        OutOccupiedSlots.Num());
    
    // Возвращаем true только если предмет полностью помещается
    return bFitsInGrid && (OutOccupiedSlots.Num() == ItemSize.X * ItemSize.Y);
}

FGameplayTag USuspenseInventoryWidget::GetContainerIdentifier_Implementation() const
{
    // Return specific tag for inventory container
    return FGameplayTag::RequestGameplayTag(TEXT("UI.Container.Inventory"));
}

int32 USuspenseInventoryWidget::GetAnchorSlotForSlot(int32 SlotIndex) const
{
    // Check if this slot is mapped to an anchor
    if (const int32* AnchorSlot = SlotToAnchorMap.Find(SlotIndex))
    {
        return *AnchorSlot;
    }
    
    // Return the same slot if not found in map
    return SlotIndex;
}

int32 USuspenseInventoryWidget::GetSlotIndexFromGridCoords(int32 GridX, int32 GridY) const
{
    if (GridX >= 0 && GridX < GridColumns && GridY >= 0 && GridY < GridRows)
    {
        return GridY * GridColumns + GridX;
    }
    
    return INDEX_NONE;
}

bool USuspenseInventoryWidget::GetGridCoordsFromSlotIndex(int32 SlotIndex, int32& OutGridX, int32& OutGridY) const
{
    if (SlotIndex >= 0 && SlotIndex < (GridColumns * GridRows))
    {
        OutGridX = SlotIndex % GridColumns;
        OutGridY = SlotIndex / GridColumns;
        return true;
    }
    
    OutGridX = 0;
    OutGridY = 0;
    return false;
}

bool USuspenseInventoryWidget::FindItemAtScreenPosition(const FVector2D& ScreenPosition, int32& OutAnchorSlot) const
{
    // First find which slot is under cursor
    if (USuspenseBaseSlotWidget* SlotWidget = GetSlotAtScreenPosition(ScreenPosition))
    {
        int32 SlotIndex = INDEX_NONE;
        
        // FIXED: Use Execute_ for interface call
        if (SlotWidget->GetClass()->ImplementsInterface(USuspenseSlotUI::StaticClass()))
        {
            SlotIndex = ISuspenseSlotUIInterface::Execute_GetSlotIndex(SlotWidget);
        }
        
        if (SlotIndex == INDEX_NONE)
        {
            OutAnchorSlot = INDEX_NONE;
            return false;
        }
        
        // Check if this slot is part of multi-slot item
        if (const int32* AnchorSlot = SlotToAnchorMap.Find(SlotIndex))
        {
            OutAnchorSlot = *AnchorSlot;
            return true;
        }
        
        // Check if slot itself is anchor with item
        bool bIsOccupied = false;
        if (SlotWidget->GetClass()->ImplementsInterface(USuspenseSlotUI::StaticClass()))
        {
            bIsOccupied = ISuspenseSlotUIInterface::Execute_IsOccupied(SlotWidget);
        }
        
        if (bIsOccupied)
        {
            OutAnchorSlot = SlotIndex;
            return true;
        }
    }
    
    OutAnchorSlot = INDEX_NONE;
    return false;
}

FGridSnapPoint USuspenseInventoryWidget::GetBestGridSnapPoint(const FVector2D& ScreenPosition, const FIntPoint& ItemSize) const
{
    FGridSnapPoint SnapPoint;
    
    // Convert screen position to grid coordinates
    FIntPoint GridPos = ScreenToGridCoordinates(ScreenPosition);
    
    // Check if position is valid
    if (IsValidPlacementPosition(GridPos.X, GridPos.Y, ItemSize))
    {
        SnapPoint.GridPosition = GridPos;
        SnapPoint.ScreenPosition = GetGridCellScreenBounds(GridPos.X, GridPos.Y).GetCenter();
        SnapPoint.bIsValid = true;
        
        // Calculate snap strength based on distance
        float Distance = FVector2D::Distance(ScreenPosition, SnapPoint.ScreenPosition);
        SnapPoint.SnapStrength = FMath::Clamp(1.0f - (Distance / (CellSize * 2.0f)), 0.0f, 1.0f);
    }
    
    return SnapPoint;
}

bool USuspenseInventoryWidget::IsValidPlacementPosition(int32 GridX, int32 GridY, const FIntPoint& ItemSize) const
{
    // Check bounds
    if (!IsWithinGridBounds(GridX, GridY, ItemSize.X, ItemSize.Y))
    {
        return false;
    }
    
    // Check if all slots are empty
    for (int32 Y = 0; Y < ItemSize.Y; Y++)
    {
        for (int32 X = 0; X < ItemSize.X; X++)
        {
            int32 SlotIndex = (GridY + Y) * GridColumns + (GridX + X);
            
            // Check if slot is occupied
            if (const USuspenseBaseSlotWidget* SlotWidget = GetSlotWidget(SlotIndex))
            {
                // FIXED: Use Execute_ for interface call
                if (SlotWidget->GetClass()->ImplementsInterface(USuspenseSlotUI::StaticClass()))
                {
                    bool bIsOccupied = ISuspenseSlotUIInterface::Execute_IsOccupied(SlotWidget);
                    if (bIsOccupied)
                    {
                        return false;
                    }
                }
            }
        }
    }
    
    return true;
}

bool USuspenseInventoryWidget::IsWithinGridBounds(int32 GridX, int32 GridY, int32 ItemWidth, int32 ItemHeight) const
{
    return GridX >= 0 && 
           GridY >= 0 && 
           (GridX + ItemWidth) <= GridColumns && 
           (GridY + ItemHeight) <= GridRows;
}

void USuspenseInventoryWidget::UpdateWeightDisplay()
{
    UE_LOG(LogTemp, VeryVerbose, TEXT("[%s] UpdateWeightDisplay - ShowWeight: %s, CurrentWeight: %.1f, MaxWeight: %.1f"), 
        *GetName(), 
        bShowWeight ? TEXT("Yes") : TEXT("No"),
        CurrentContainerData.CurrentWeight,
        CurrentContainerData.MaxWeight);
    
    if (!bShowWeight)
    {
        if (WeightBar)
        {
            WeightBar->SetVisibility(ESlateVisibility::Collapsed);
        }
        if (WeightText)
        {
            WeightText->SetVisibility(ESlateVisibility::Collapsed);
        }
        return;
    }
    
    // Update weight text
    if (WeightText)
    {
        WeightText->SetVisibility(ESlateVisibility::Visible);
        
        // Format weight with one decimal place
        FNumberFormattingOptions FormatOptions;
        FormatOptions.MinimumFractionalDigits = 1;
        FormatOptions.MaximumFractionalDigits = 1;
        
        FText WeightFormatText = FText::Format(
            NSLOCTEXT("Inventory", "WeightFormat", "{0} / {1} kg"),
            FText::AsNumber(CurrentContainerData.CurrentWeight, &FormatOptions),
            FText::AsNumber(CurrentContainerData.MaxWeight, &FormatOptions)
        );
        WeightText->SetText(WeightFormatText);
    }
    
    // Update weight progress bar
    if (WeightBar)
    {
        WeightBar->SetVisibility(ESlateVisibility::Visible);
        
        if (CurrentContainerData.MaxWeight > 0)
        {
            float WeightPercent = CurrentContainerData.CurrentWeight / CurrentContainerData.MaxWeight;
            WeightBar->SetPercent(FMath::Clamp(WeightPercent, 0.0f, 1.0f));
            
            // Change color based on weight threshold
            if (WeightPercent >= WeightWarningThreshold)
            {
                WeightBar->SetFillColorAndOpacity(FLinearColor(1.0f, 0.5f, 0.0f)); // Orange warning
            }
            else
            {
                WeightBar->SetFillColorAndOpacity(FLinearColor(0.2f, 0.8f, 0.2f)); // Green normal
            }
        }
        else
        {
            WeightBar->SetPercent(0.0f);
        }
    }
}

void USuspenseInventoryWidget::OnCloseButtonClicked()
{
    // Don't just hide widget, notify the system
    if (UEventDelegateManager* DelegateManager = GetDelegateManager())
    {
        FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TEXT("UI.Inventory.Closed"));
        DelegateManager->NotifyUIEvent(this, EventTag, TEXT(""));
    }
    
    UE_LOG(LogTemp, Log, TEXT("[%s] Close button clicked, notifying system"), *GetName());
}

void USuspenseInventoryWidget::OnSortButtonClicked()
{
    // Request inventory sort through event system
    if (UEventDelegateManager* DelegateManager = GetDelegateManager())
    {
        FGameplayTag InteractionType = FGameplayTag::RequestGameplayTag(TEXT("UI.Inventory.RequestSort"));
        ISuspenseContainerUIInterface::BroadcastSlotInteraction(this, INDEX_NONE, InteractionType);
    }
    
    UE_LOG(LogTemp, Log, TEXT("[%s] Sort requested by user"), *GetName());
}

void USuspenseInventoryWidget::RequestRotateSelectedItem()
{
    // Get selected slot
    int32 CurrentSelectedSlot = GetSelectedSlotIndex();
    
    if (CurrentSelectedSlot != INDEX_NONE)
    {
        // Check if there's actually an item in selected slot
        if (const int32* AnchorSlot = SlotToAnchorMap.Find(CurrentSelectedSlot))
        {
            OnRotateItemRequested(*AnchorSlot);
        }
        else if (USuspenseBaseSlotWidget* SlotWidget = GetSlotWidget(CurrentSelectedSlot))
        {
            // FIXED: Use Execute_ for interface call
            bool bIsOccupied = false;
            if (SlotWidget->GetClass()->ImplementsInterface(USuspenseSlotUI::StaticClass()))
            {
                bIsOccupied = ISuspenseSlotUIInterface::Execute_IsOccupied(SlotWidget);
            }
            
            if (bIsOccupied)
            {
                OnRotateItemRequested(CurrentSelectedSlot);
            }
        }
    }
}

void USuspenseInventoryWidget::OnInventoryDataUpdated(const FContainerUIData& NewData)
{
    // Use interface to update container
    ISuspenseContainerUIInterface::Execute_UpdateContainer(this, NewData);
}

void USuspenseInventoryWidget::OnRotateItemRequested(int32 SlotIndex)
{
    // Send rotation request through event system
    if (UEventDelegateManager* DelegateManager = GetDelegateManager())
    {
        FGameplayTag InteractionType = FGameplayTag::RequestGameplayTag(TEXT("UI.Inventory.RotateItem"));
        ISuspenseContainerUIInterface::BroadcastSlotInteraction(this, SlotIndex, InteractionType);
    }
    
    UE_LOG(LogTemp, Log, TEXT("[%s] Requested item rotation in slot %d"), 
        *GetName(), SlotIndex);
}

void USuspenseInventoryWidget::UpdateSlotOccupancyMap()
{
    SlotToAnchorMap.Empty();
    
    // Build map of which slots are occupied by multi-slot items
    for (const FItemUIData& Item : CurrentContainerData.Items)
    {
        if (Item.AnchorSlotIndex == INDEX_NONE)
        {
            continue;
        }
        
        TArray<int32> OccupiedSlots;
        if (CalculateOccupiedSlots(Item.AnchorSlotIndex, Item.GridSize, Item.bIsRotated, OccupiedSlots))
        {
            for (int32 SlotIdx : OccupiedSlots)
            {
                SlotToAnchorMap.Add(SlotIdx, Item.AnchorSlotIndex);
            }
        }
    }
    UE_LOG(LogTemp, VeryVerbose, TEXT("[%s] Updated slot occupancy map with %d occupied slots"), 
       *GetName(), SlotToAnchorMap.Num());
}

void USuspenseInventoryWidget::SubscribeToInventoryEvents()
{
   if (UEventDelegateManager* DelegateManager = GetDelegateManager())
   {
       // Subscribe to inventory-specific update events
       UE_LOG(LogTemp, Log, TEXT("[%s] Subscribed to inventory events"), *GetName());
   }
}

int32 USuspenseInventoryWidget::FindBestFitSlot(int32 TargetSlot, FIntPoint ItemSize, bool bIsRotated) const
{
   if (TargetSlot < 0 || TargetSlot >= (GridColumns * GridRows))
   {
       return INDEX_NONE;
   }
   
   // Get target coordinates
   int32 TargetX = TargetSlot % GridColumns;
   int32 TargetY = TargetSlot / GridColumns;
   
   // Try positions in expanding spiral from target
   const int32 MaxSearchRadius = FMath::Max(GridColumns, GridRows);
   
   for (int32 Radius = 0; Radius < MaxSearchRadius; ++Radius)
   {
       // Check positions at current radius
       for (int32 dx = -Radius; dx <= Radius; ++dx)
       {
           for (int32 dy = -Radius; dy <= Radius; ++dy)
           {
               // Skip positions not on the edge of current radius
               if (FMath::Abs(dx) != Radius && FMath::Abs(dy) != Radius)
               {
                   continue;
               }
               
               int32 TestX = TargetX + dx;
               int32 TestY = TargetY + dy;
               
               // Check if position is valid
               if (IsWithinGridBounds(TestX, TestY, ItemSize.X, ItemSize.Y))
               {
                   int32 TestSlot = TestY * GridColumns + TestX;
                   
                   // Additional validation could be added here
                   // For now, just return first valid position
                   return TestSlot;
               }
           }
       }
   }
   
   return INDEX_NONE;
}

FIntPoint USuspenseInventoryWidget::ScreenToGridCoordinates(const FVector2D& ScreenPos) const
{
   if (!InventoryGrid)
   {
       return FIntPoint(-1, -1);
   }
   
   // Get grid panel geometry
   FGeometry GridGeometry = InventoryGrid->GetCachedGeometry();
   
   // Convert screen position to local
   FVector2D LocalPos = GridGeometry.AbsoluteToLocal(ScreenPos);
   
   // Calculate grid coordinates
   int32 GridX = FMath::FloorToInt(LocalPos.X / (CellSize + CellPadding));
   int32 GridY = FMath::FloorToInt(LocalPos.Y / (CellSize + CellPadding));
   
   // Clamp to grid bounds
   GridX = FMath::Clamp(GridX, 0, GridColumns - 1);
   GridY = FMath::Clamp(GridY, 0, GridRows - 1);
   
   return FIntPoint(GridX, GridY);
}

FBox2D USuspenseInventoryWidget::GetGridCellScreenBounds(int32 GridX, int32 GridY) const
{
   if (!InventoryGrid)
   {
       return FBox2D(FVector2D::ZeroVector, FVector2D::ZeroVector);
   }
   
   // Get grid panel geometry
   FGeometry GridGeometry = InventoryGrid->GetCachedGeometry();
   
   // Calculate local position
   float LocalX = GridX * (CellSize + CellPadding);
   float LocalY = GridY * (CellSize + CellPadding);
   
   // Convert to screen space
   FVector2D MinPoint = GridGeometry.LocalToAbsolute(FVector2D(LocalX, LocalY));
   FVector2D MaxPoint = GridGeometry.LocalToAbsolute(FVector2D(LocalX + CellSize, LocalY + CellSize));
   
   return FBox2D(MinPoint, MaxPoint);
}

bool USuspenseInventoryWidget::ValidateCriticalComponents() const
{
   bool bValid = true;
   
   // Check if GetSlotsPanel returns valid GridPanel
   if (!ValidateSlotsPanel())
   {
       bValid = false;
   }
   
   // Check if InventoryGrid is bound
   if (!InventoryGrid)
   {
       UE_LOG(LogTemp, Error, TEXT("[%s] CRITICAL: InventoryGrid is not bound!"), *GetName());
       UE_LOG(LogTemp, Error, TEXT("[%s] Please bind a GridPanel named 'InventoryGrid' in your Blueprint"), *GetName());
       bValid = false;
   }
   
   if (!SlotWidgetClass)
   {
       UE_LOG(LogTemp, Error, TEXT("[%s] CRITICAL: SlotWidgetClass is null!"), *GetName());
       bValid = false;
   }
   
   if (GridColumns <= 0 || GridRows <= 0)
   {
       UE_LOG(LogTemp, Error, TEXT("[%s] CRITICAL: Invalid grid dimensions %dx%d!"), 
           *GetName(), GridColumns, GridRows);
       bValid = false;
   }
   
   return bValid;
}

void USuspenseInventoryWidget::AutoBindComponents()
{
   if (!WidgetTree)
   {
       return;
   }
   
   TArray<UWidget*> AllWidgets;
   WidgetTree->GetAllWidgets(AllWidgets);
   
   for (UWidget* Widget : AllWidgets)
   {
       FString WidgetName = Widget->GetName();
       
       // Auto-bind by name for optional components
       if (!InventoryTitle && WidgetName.Contains(TEXT("Title")) && Cast<UTextBlock>(Widget))
       {
           InventoryTitle = Cast<UTextBlock>(Widget);
           UE_LOG(LogTemp, Log, TEXT("[%s] Auto-bound InventoryTitle: %s"), *GetName(), *WidgetName);
       }
       else if (!WeightText && WidgetName.Contains(TEXT("WeightText")) && Cast<UTextBlock>(Widget))
       {
           WeightText = Cast<UTextBlock>(Widget);
           UE_LOG(LogTemp, Log, TEXT("[%s] Auto-bound WeightText: %s"), *GetName(), *WidgetName);
       }
       else if (!WeightBar && WidgetName.Contains(TEXT("WeightBar")) && Cast<UProgressBar>(Widget))
       {
           WeightBar = Cast<UProgressBar>(Widget);
           UE_LOG(LogTemp, Log, TEXT("[%s] Auto-bound WeightBar: %s"), *GetName(), *WidgetName);
       }
       else if (!CloseButton && WidgetName.Contains(TEXT("Close")) && Cast<UButton>(Widget))
       {
           CloseButton = Cast<UButton>(Widget);
           UE_LOG(LogTemp, Log, TEXT("[%s] Auto-bound CloseButton: %s"), *GetName(), *WidgetName);
       }
       else if (!SortButton && WidgetName.Contains(TEXT("Sort")) && Cast<UButton>(Widget))
       {
           SortButton = Cast<UButton>(Widget);
           UE_LOG(LogTemp, Log, TEXT("[%s] Auto-bound SortButton: %s"), *GetName(), *WidgetName);
       }
   }
}

float USuspenseInventoryWidget::GetDragVisualCellSize() const
{
    // Return actual cell size from inventory grid
    return CellSize > 0.0f ? CellSize : DefaultDragVisualCellSize;
}

void USuspenseInventoryWidget::DiagnoseWidget() const
{
   UE_LOG(LogTemp, Warning, TEXT("=== Inventory Widget Diagnostics ==="));
   UE_LOG(LogTemp, Warning, TEXT("Widget Name: %s"), *GetName());
   UE_LOG(LogTemp, Warning, TEXT("Widget Class: %s"), *GetClass()->GetName());
   UE_LOG(LogTemp, Warning, TEXT(""));
   UE_LOG(LogTemp, Warning, TEXT("=== State ==="));
   UE_LOG(LogTemp, Warning, TEXT("Fully Initialized: %s"), bIsFullyInitialized ? TEXT("Yes") : TEXT("No"));
   UE_LOG(LogTemp, Warning, TEXT("Grid Initialized: %s"), bGridInitialized ? TEXT("Yes") : TEXT("No"));
   UE_LOG(LogTemp, Warning, TEXT("Grid Size: %dx%d"), GridColumns, GridRows);
   UE_LOG(LogTemp, Warning, TEXT("Slot Count: %d"), SlotWidgets.Num());
   UE_LOG(LogTemp, Warning, TEXT("Item Count: %d"), CurrentContainerData.Items.Num());
   UE_LOG(LogTemp, Warning, TEXT(""));
   UE_LOG(LogTemp, Warning, TEXT("=== Components ==="));
   
   UPanelWidget* Panel = const_cast<USuspenseInventoryWidget*>(this)->GetSlotsPanel();
   UE_LOG(LogTemp, Warning, TEXT("GetSlotsPanel(): %s"), Panel ? *Panel->GetClass()->GetName() : TEXT("NULL"));
   
   UE_LOG(LogTemp, Warning, TEXT("InventoryGrid: %s"), InventoryGrid ? TEXT("Valid") : TEXT("NULL"));
   UE_LOG(LogTemp, Warning, TEXT("SlotWidgetClass: %s"), SlotWidgetClass ? *SlotWidgetClass->GetName() : TEXT("NULL"));
   UE_LOG(LogTemp, Warning, TEXT("InventoryTitle: %s"), InventoryTitle ? TEXT("Valid") : TEXT("NULL"));
   UE_LOG(LogTemp, Warning, TEXT("WeightText: %s"), WeightText ? TEXT("Valid") : TEXT("NULL"));
   UE_LOG(LogTemp, Warning, TEXT("WeightBar: %s"), WeightBar ? TEXT("Valid") : TEXT("NULL"));
   UE_LOG(LogTemp, Warning, TEXT("CloseButton: %s"), CloseButton ? TEXT("Valid") : TEXT("NULL"));
   UE_LOG(LogTemp, Warning, TEXT("SortButton: %s"), SortButton ? TEXT("Valid") : TEXT("NULL"));
   UE_LOG(LogTemp, Warning, TEXT(""));
   UE_LOG(LogTemp, Warning, TEXT("=== Visibility ==="));
   UE_LOG(LogTemp, Warning, TEXT("Visibility: %s"), *UEnum::GetValueAsString(GetVisibility()));
   UE_LOG(LogTemp, Warning, TEXT("Is Enabled: %s"), GetIsEnabled() ? TEXT("Yes") : TEXT("No"));
   UE_LOG(LogTemp, Warning, TEXT("In Viewport: %s"), IsInViewport() ? TEXT("Yes") : TEXT("No"));
   UE_LOG(LogTemp, Warning, TEXT("Render Opacity: %.2f"), GetRenderOpacity());
   
   if (InventoryGrid)
   {
       UE_LOG(LogTemp, Warning, TEXT(""));
       UE_LOG(LogTemp, Warning, TEXT("=== InventoryGrid Details ==="));
       UE_LOG(LogTemp, Warning, TEXT("InventoryGrid Class: %s"), *InventoryGrid->GetClass()->GetName());
       UE_LOG(LogTemp, Warning, TEXT("InventoryGrid Visibility: %s"), *UEnum::GetValueAsString(InventoryGrid->GetVisibility()));
       
       if (InventoryGrid->GetChildrenCount() > 0)
       {
           UE_LOG(LogTemp, Warning, TEXT("InventoryGrid Children: %d"), InventoryGrid->GetChildrenCount());
       }
   }
   
   UE_LOG(LogTemp, Warning, TEXT("=== End Diagnostics ==="));
}

void USuspenseInventoryWidget::LogGridPerformanceMetrics() const
{
   UE_LOG(LogTemp, Log, TEXT("[%s] Grid Performance Metrics:"), *GetName());
   UE_LOG(LogTemp, Log, TEXT("  - Total Updates: %d"), GridUpdateCounter);
   UE_LOG(LogTemp, Log, TEXT("  - Grid Size: %dx%d"), GridColumns, GridRows);
   UE_LOG(LogTemp, Log, TEXT("  - Total Slots: %d"), SlotWidgets.Num());
   UE_LOG(LogTemp, Log, TEXT("  - Occupied Slots: %d"), SlotToAnchorMap.Num());
   UE_LOG(LogTemp, Log, TEXT("  - Cached Grid Data: %d"), CachedGridSlotData.Num());
   UE_LOG(LogTemp, Log, TEXT("  - Pending Span Updates: %d"), PendingGridUpdateBatch.SlotSpanUpdates.Num());
   UE_LOG(LogTemp, Log, TEXT("  - Pending Visibility Updates: %d"), PendingGridUpdateBatch.SlotVisibilityUpdates.Num());
   UE_LOG(LogTemp, Log, TEXT("  - Smart Drop Zones: %s"), bEnableSmartDropZones ? TEXT("Enabled") : TEXT("Disabled"));
}