// MedComInventorySlotWidget.cpp
// Copyright Suspense Team Team. All Rights Reserved.

#include "Widgets/Inventory/SuspenseInventorySlotWidget.h"
#include "Widgets/Inventory/SuspenseInventoryWidget.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "GameplayTagContainer.h"
#include "Engine/Texture2D.h"
#include "Slate/SlateBrushAsset.h"
#include "Materials/MaterialInstanceDynamic.h"

USuspenseInventorySlotWidget::USuspenseInventorySlotWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Default values
    bShowGridCoordinates = false;
    bShowCoordinatesOverride = false;
    DefaultRarityColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);
    GridCoordinates = FIntPoint::ZeroValue;
    
    // Set default inventory cell size
    InventoryCellSize = 48.0f;
    CellSizeOverride = 0.0f; // 0 means use InventoryCellSize
    
    // Apply cell size to base class
    SlotSize = InventoryCellSize;
    
    // Multi-slot optimizations
    bOptimizeMultiSlotIcons = true;
    MultiSlotIconScale = 0.85f;
    bIconBrushDirty = true;
    bIsRenderingMultiSlot = false;
    CachedMultiSlotSize = FIntPoint(1, 1);
    
    // Durability settings
    DurabilityWarningThreshold = 0.25f;
    
    // Setup default rarity colors
    RarityColors.Add(FGameplayTag::RequestGameplayTag(TEXT("Item.Rarity.Common")), FLinearColor(0.5f, 0.5f, 0.5f, 1.0f));
    RarityColors.Add(FGameplayTag::RequestGameplayTag(TEXT("Item.Rarity.Uncommon")), FLinearColor(0.0f, 1.0f, 0.0f, 1.0f));
    RarityColors.Add(FGameplayTag::RequestGameplayTag(TEXT("Item.Rarity.Rare")), FLinearColor(0.0f, 0.5f, 1.0f, 1.0f));
    RarityColors.Add(FGameplayTag::RequestGameplayTag(TEXT("Item.Rarity.Epic")), FLinearColor(0.5f, 0.0f, 1.0f, 1.0f));
    RarityColors.Add(FGameplayTag::RequestGameplayTag(TEXT("Item.Rarity.Legendary")), FLinearColor(1.0f, 0.5f, 0.0f, 1.0f));
    
    // Initialize visual properties
    CachedInventoryVisuals = FInventorySlotVisualProperties();
    PendingInventoryVisuals = FInventorySlotVisualProperties();
}

void USuspenseInventorySlotWidget::NativePreConstruct()
{
    Super::NativePreConstruct();
    
    // Apply Blueprint constructor overrides
    if (CellSizeOverride > 0.0f)
    {
        InventoryCellSize = CellSizeOverride;
        SlotSize = CellSizeOverride;
    }
    
    if (bShowCoordinatesOverride)
    {
        bShowGridCoordinates = true;
    }
}

void USuspenseInventorySlotWidget::NativeConstruct()
{
    // Apply cell size before base class construction
    if (CellSizeOverride > 0.0f)
    {
        InventoryCellSize = CellSizeOverride;
        SlotSize = CellSizeOverride;
    }
    
    Super::NativeConstruct();
    
    // Apply size to RootSizeBox if not done in base class
    if (RootSizeBox && InventoryCellSize > 0.0f)
    {
        RootSizeBox->SetWidthOverride(InventoryCellSize);
        RootSizeBox->SetHeightOverride(InventoryCellSize);
    }
    
    // Initialize grid coordinate display
    if (bShowGridCoordinates && GridCoordText)
    {
        UpdateGridCoordinateDisplay();
        GridCoordText->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    else if (GridCoordText)
    {
        GridCoordText->SetVisibility(ESlateVisibility::Collapsed);
    }
    
    // Initialize rarity border
    if (RarityBorder)
    {
        RarityBorder->SetVisibility(ESlateVisibility::Collapsed);
    }
    
    // Initialize durability bar
    if (DurabilityBar)
    {
        DurabilityBar->SetVisibility(ESlateVisibility::Collapsed);
    }
    
    UE_LOG(LogTemp, Log, TEXT("[InventorySlot %d] Initialized with cell size: %.0f"), 
        CurrentSlotData.SlotIndex, InventoryCellSize);
}

void USuspenseInventorySlotWidget::InitializeInventorySlot(float InCellSize)
{
    // Update cell size
    if (InCellSize > 0.0f)
    {
        InventoryCellSize = InCellSize;
        SlotSize = InCellSize;
    }
    else if (CellSizeOverride > 0.0f)
    {
        // Use Blueprint override if set
        InventoryCellSize = CellSizeOverride;
        SlotSize = CellSizeOverride;
    }
    
    // Apply to size box immediately
    if (RootSizeBox)
    {
        RootSizeBox->SetWidthOverride(InventoryCellSize);
        RootSizeBox->SetHeightOverride(InventoryCellSize);
    }
    
    // Mark icon brush as dirty
    bIconBrushDirty = true;
    
    UE_LOG(LogTemp, Log, TEXT("[InventorySlot] Initialized with cell size: %.0f"), InventoryCellSize);
}

void USuspenseInventorySlotWidget::SetGridCoordinates(int32 X, int32 Y)
{
    GridCoordinates.X = X;
    GridCoordinates.Y = Y;
    
    // Update display if enabled
    if (bShowGridCoordinates)
    {
        UpdateGridCoordinateDisplay();
    }
}

FVector2D USuspenseInventorySlotWidget::GetEffectiveIconSize() const
{
    if (CurrentSlotData.bIsOccupied && CurrentSlotData.bIsAnchor && bOptimizeMultiSlotIcons)
    {
        return CalculateMultiSlotIconSize();
    }
    
    // Default single slot size
    return FVector2D(InventoryCellSize * 0.8f);
}

void USuspenseInventorySlotWidget::UpdateItemRarity(const FGameplayTag& ItemRarity)
{
    // Find rarity color
    FLinearColor RarityColor = DefaultRarityColor;
    
    if (const FLinearColor* ColorPtr = RarityColors.Find(ItemRarity))
    {
        RarityColor = *ColorPtr;
    }
    
    PendingInventoryVisuals.RarityColor = RarityColor;
    PendingInventoryVisuals.bRarityVisible = true;
    
    RequestVisualUpdate();
}
void USuspenseInventorySlotWidget::RequestVisualUpdate()
{
    // Планируем обновление визуала на следующий кадр
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
        {
            UpdateVisualState();
        });
    }
}
void USuspenseInventorySlotWidget::UpdateVisualState()
{
    // Call base implementation
    Super::UpdateVisualState();
    
    // Update inventory-specific visuals
    UpdateRarityDisplay();
    UpdateGridCoordinateDisplay();
    UpdateDurabilityDisplay();
    UpdateMultiSlotOverlay();
}

void USuspenseInventorySlotWidget::UpdateRarityDisplay()
{
    if (!RarityBorder)
    {
        return;
    }
    
    if (CurrentSlotData.bIsOccupied && CurrentSlotData.bIsAnchor)
    {
        // Find rarity color based on item type
        FLinearColor RarityColor = DefaultRarityColor;
        for (const auto& RarityPair : RarityColors)
        {
            if (CurrentItemData.ItemType.MatchesTag(RarityPair.Key))
            {
                RarityColor = RarityPair.Value;
                break;
            }
        }
        
        RarityBorder->SetBrushColor(RarityColor);
        RarityBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    else
    {
        RarityBorder->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void USuspenseInventorySlotWidget::UpdateItemIcon()
{
    if (!ItemIcon)
    {
        UE_LOG(LogTemp, Error, TEXT("[InventorySlot %d] ItemIcon widget component is null!"), CurrentSlotData.SlotIndex);
        return;
    }
    
    // Check if icon should be displayed
    if (CurrentSlotData.bIsOccupied && CurrentSlotData.bIsAnchor)
    {
        UTexture2D* IconTexture = CurrentItemData.GetIcon();
        
        if (IconTexture)
        {
            // Make widget visible
            ItemIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
            
            // Update cached brush if needed
            if (bIconBrushDirty || CachedIconBrush.GetResourceObject() != IconTexture)
            {
                UpdateCachedIconBrush();
            }
            
            // Apply brush
            ItemIcon->SetBrush(CachedIconBrush);
            
            // Set color and opacity
            if (bIsDragging)
            {
                ItemIcon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.5f));
            }
            else
            {
                ItemIcon->SetColorAndOpacity(FLinearColor::White);
            }
            
            // Handle rotation
            if (CurrentItemData.bIsRotated)
            {
                ItemIcon->SetRenderTransformAngle(90.0f);
                ItemIcon->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
            }
            else
            {
                ItemIcon->SetRenderTransformAngle(0.0f);
            }
            
            UE_LOG(LogTemp, VeryVerbose, TEXT("[InventorySlot %d] Icon updated: %s (Size: %.0fx%.0f)"), 
                CurrentSlotData.SlotIndex,
                *IconTexture->GetName(),
                CachedIconBrush.ImageSize.X, CachedIconBrush.ImageSize.Y);
        }
        else
        {
            // No texture - hide icon
            ItemIcon->SetVisibility(ESlateVisibility::Hidden);
            
            UE_LOG(LogTemp, Warning, TEXT("[InventorySlot %d] No icon texture for item %s"), 
                CurrentSlotData.SlotIndex,
                *CurrentItemData.ItemID.ToString());
        }
    }
    else
    {
        // Slot is empty or not anchor - hide icon
        ItemIcon->SetVisibility(ESlateVisibility::Hidden);
        
        // Reset rotation
        ItemIcon->SetRenderTransformAngle(0.0f);
    }
}

void USuspenseInventorySlotWidget::UpdateGridCoordinateDisplay()
{
    if (!GridCoordText || !bShowGridCoordinates)
    {
        return;
    }
    
    FText CoordText = FText::Format(
        NSLOCTEXT("Inventory", "GridCoordFormat", "{0},{1}"),
        FText::AsNumber(GridCoordinates.X),
        FText::AsNumber(GridCoordinates.Y)
    );
    GridCoordText->SetText(CoordText);
}

void USuspenseInventorySlotWidget::UpdateDurabilityDisplay()
{
    if (!DurabilityBar)
    {
        return;
    }
    
    // Durability в текущей архитектуре должна приходить через GAS AttributeSet
    // Поскольку FItemUIData не содержит durability, временно скрываем durability bar
    // TODO: Получать durability через Bridge из AttributeSet экипированного предмета
    
    DurabilityBar->SetVisibility(ESlateVisibility::Collapsed);
}

void USuspenseInventorySlotWidget::UpdateMultiSlotOverlay()
{
    if (!MultiSlotOverlay)
    {
        return;
    }
    
    // Show overlay for non-anchor parts of multi-slot items
    if (CurrentSlotData.bIsPartOfItem && !CurrentSlotData.bIsAnchor)
    {
        MultiSlotOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
        
        // Set semi-transparent overlay
        MultiSlotOverlay->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.3f));
    }
    else
    {
        MultiSlotOverlay->SetVisibility(ESlateVisibility::Collapsed);
    }
}

FVector2D USuspenseInventorySlotWidget::CalculateMultiSlotIconSize() const
{
    if (!bOptimizeMultiSlotIcons || !CurrentSlotData.bIsAnchor)
    {
        return FVector2D(InventoryCellSize * 0.8f);
    }
    
    // Get effective item size
    FIntPoint ItemSize = CurrentItemData.bIsRotated ? 
        FIntPoint(CurrentItemData.GridSize.Y, CurrentItemData.GridSize.X) : 
        CurrentItemData.GridSize;
    
    // Calculate icon size based on item grid size
    float IconWidth = ItemSize.X * InventoryCellSize * MultiSlotIconScale;
    float IconHeight = ItemSize.Y * InventoryCellSize * MultiSlotIconScale;
    
    return FVector2D(IconWidth, IconHeight);
}

FVector2D USuspenseInventorySlotWidget::CalculateIconOffset(const FVector2D& IconSize) const
{
    // Center icon in slot
    float OffsetX = (InventoryCellSize - IconSize.X) * 0.5f;
    float OffsetY = (InventoryCellSize - IconSize.Y) * 0.5f;
    
    return FVector2D(OffsetX, OffsetY);
}

void USuspenseInventorySlotWidget::UpdateCachedIconBrush()
{
    UTexture2D* IconTexture = CurrentItemData.GetIcon();
    if (!IconTexture)
    {
        return;
    }
    
    // Reset brush
    CachedIconBrush = FSlateBrush();
    CachedIconBrush.SetResourceObject(IconTexture);
    
    // Calculate size
    FVector2D IconSize = CalculateMultiSlotIconSize();
    CachedIconBrush.ImageSize = IconSize;
    
    // Set draw mode
    CachedIconBrush.DrawAs = ESlateBrushDrawType::Image;
    CachedIconBrush.Tiling = ESlateBrushTileType::NoTile;
    
    // Set margins for offset
    if (bOptimizeMultiSlotIcons && CurrentSlotData.bIsAnchor)
    {
        FVector2D Offset = CalculateIconOffset(IconSize);
        CachedIconBrush.Margin = FMargin(Offset.X, Offset.Y, 0.0f, 0.0f);
    }
    else
    {
        CachedIconBrush.Margin = FMargin(0.0f);
    }
    
    bIconBrushDirty = false;
    
    // Cache multi-slot size
    CachedMultiSlotSize = CurrentItemData.bIsRotated ? 
        FIntPoint(CurrentItemData.GridSize.Y, CurrentItemData.GridSize.X) : 
        CurrentItemData.GridSize;
}

bool USuspenseInventorySlotWidget::ShouldRenderAsMultiSlot() const
{
    return bOptimizeMultiSlotIcons && 
           CurrentSlotData.bIsAnchor && 
           (CurrentItemData.GridSize.X > 1 || CurrentItemData.GridSize.Y > 1);
}

bool USuspenseInventorySlotWidget::NeedsInventoryVisualUpdate() const
{
    // Create current visual state
    FInventorySlotVisualProperties CurrentState;
    CurrentState.bRarityVisible = CurrentSlotData.bIsOccupied && CurrentSlotData.bIsAnchor;
    CurrentState.bCoordTextVisible = bShowGridCoordinates;
    CurrentState.GridCoords = GridCoordinates;
    CurrentState.bIsPartOfMultiSlot = CurrentSlotData.bIsPartOfItem && !CurrentSlotData.bIsAnchor;
    
    if (CurrentState.bRarityVisible)
    {
        CurrentState.RarityColor = DefaultRarityColor;
        for (const auto& RarityPair : RarityColors)
        {
            if (CurrentItemData.ItemType.MatchesTag(RarityPair.Key))
            {
                CurrentState.RarityColor = RarityPair.Value;
                break;
            }
        }
    }
    
    if (bOptimizeMultiSlotIcons && CurrentSlotData.bIsAnchor)
    {
        CurrentState.IconSize = CalculateMultiSlotIconSize();
        CurrentState.IconOffset = CalculateIconOffset(CurrentState.IconSize);
    }
    
    // Compare with cached state
    return CurrentState.bRarityVisible != CachedInventoryVisuals.bRarityVisible ||
           CurrentState.RarityColor != CachedInventoryVisuals.RarityColor ||
           CurrentState.bCoordTextVisible != CachedInventoryVisuals.bCoordTextVisible ||
           CurrentState.GridCoords != CachedInventoryVisuals.GridCoords ||
           CurrentState.IconSize != CachedInventoryVisuals.IconSize ||
           CurrentState.IconOffset != CachedInventoryVisuals.IconOffset ||
           CurrentState.bIsPartOfMultiSlot != CachedInventoryVisuals.bIsPartOfMultiSlot;
}

void USuspenseInventorySlotWidget::ApplyInventoryVisualProperties(const FInventorySlotVisualProperties& Props)
{
    CachedInventoryVisuals = Props;
    
    // Apply rarity
    UpdateRarityDisplay();
    
    // Apply grid coordinates
    if (Props.bCoordTextVisible)
    {
        UpdateGridCoordinateDisplay();
    }
    
    // Apply durability
    UpdateDurabilityDisplay();
    
    // Apply multi-slot overlay
    UpdateMultiSlotOverlay();
}

void USuspenseInventorySlotWidget::ClearInventoryCaches()
{
    CachedInventoryVisuals = FInventorySlotVisualProperties();
    PendingInventoryVisuals = FInventorySlotVisualProperties();
    bIconBrushDirty = true;
    bIsRenderingMultiSlot = false;
    CachedMultiSlotSize = FIntPoint(1, 1);
}

void USuspenseInventorySlotWidget::InitializeSlot_Implementation(const FSlotUIData& SlotData, const FItemUIData& ItemData)
{
    // Call base implementation
    Super::InitializeSlot_Implementation(SlotData, ItemData);
    
    // Store grid coordinates
    GridCoordinates.X = SlotData.GridX;
    GridCoordinates.Y = SlotData.GridY;
    
    // Clear caches
    ClearInventoryCaches();
    
    // Force immediate update for inventory visuals
    UpdateRarityDisplay();
    UpdateGridCoordinateDisplay();
    UpdateDurabilityDisplay();
    UpdateMultiSlotOverlay();
    
    // Mark icon as dirty
    bIconBrushDirty = true;
    
    // Log for debugging
    if (SlotData.bIsOccupied && SlotData.bIsAnchor)
    {
        UE_LOG(LogTemp, Log, TEXT("[InventorySlot %d] Initialized at (%d,%d) with item %s"), 
            SlotData.SlotIndex,
            GridCoordinates.X,
            GridCoordinates.Y,
            *ItemData.ItemID.ToString());
    }
}

void USuspenseInventorySlotWidget::UpdateSlot_Implementation(const FSlotUIData& SlotData, const FItemUIData& ItemData)
{
    // Check what changed
    bool bGridChanged = GridCoordinates.X != SlotData.GridX || GridCoordinates.Y != SlotData.GridY;
    bool bItemSizeChanged = false;
    
    if (CurrentSlotData.bIsOccupied && SlotData.bIsOccupied)
    {
        FIntPoint OldSize = CurrentItemData.bIsRotated ? 
            FIntPoint(CurrentItemData.GridSize.Y, CurrentItemData.GridSize.X) : 
            CurrentItemData.GridSize;
            
        FIntPoint NewSize = ItemData.bIsRotated ? 
            FIntPoint(ItemData.GridSize.Y, ItemData.GridSize.X) : 
            ItemData.GridSize;
            
        bItemSizeChanged = OldSize != NewSize;
    }
    
    // Call base implementation
    Super::UpdateSlot_Implementation(SlotData, ItemData);
    
    // Update grid coordinates if changed
    if (bGridChanged)
    {
        GridCoordinates.X = SlotData.GridX;
        GridCoordinates.Y = SlotData.GridY;
        
        if (bShowGridCoordinates)
        {
            UpdateGridCoordinateDisplay();
        }
    }
    
    // Mark icon dirty if item size changed
    if (bItemSizeChanged)
    {
        bIconBrushDirty = true;
    }
    
    // Request inventory visual update
    if (NeedsInventoryVisualUpdate())
    {
        RequestVisualUpdate();
    }
}

bool USuspenseInventorySlotWidget::CanBeDragged_Implementation() const
{
    // Call base implementation first
    if (!Super::CanBeDragged_Implementation())
    {
        return false;
    }
    
    // Additional inventory-specific checks
    // Check if item is quest item or bound
    if (CurrentItemData.ItemType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Item.Type.Quest"))))
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventorySlot %d] Cannot drag quest items"), CurrentSlotData.SlotIndex);
        return false;
    }
    
    return true;
}

FDragDropUIData USuspenseInventorySlotWidget::GetDragData_Implementation() const
{
    // Create drag data with inventory-specific container type
    FGameplayTag InventoryContainerType = FGameplayTag::RequestGameplayTag(TEXT("Container.Inventory"));
    
    FDragDropUIData DragData = FDragDropUIData::CreateValidated(
        CurrentItemData,
        InventoryContainerType,
        CurrentSlotData.SlotIndex
    );
    
    UE_LOG(LogTemp, Log, TEXT("[InventorySlot %d] Created drag data for item %s (size %dx%d)"), 
        CurrentSlotData.SlotIndex, 
        *CurrentItemData.ItemID.ToString(),
        CurrentItemData.GridSize.X,
        CurrentItemData.GridSize.Y);
    
    return DragData;
}

FVector2D USuspenseInventorySlotWidget::CalculateDragOffset(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) const
{
    // For multi-slot items, calculate offset from anchor
    if (!CurrentSlotData.bIsAnchor && CurrentSlotData.bIsPartOfItem)
    {
        // Get inventory owner
        USuspenseInventoryWidget* InventoryOwner = GetInventoryOwner();
        if (!InventoryOwner)
        {
            return FVector2D(0.5f, 0.5f);
        }
        
        // Get anchor slot
        int32 AnchorSlotIndex = InventoryOwner->GetAnchorSlotForSlot(CurrentSlotData.SlotIndex);
        if (AnchorSlotIndex == CurrentSlotData.SlotIndex)
        {
            // We are the anchor, use default
            return FVector2D(0.5f, 0.5f);
        }
        
        // Get anchor slot widget
        USuspenseBaseSlotWidget* AnchorSlotWidget = InventoryOwner->GetSlotWidget(AnchorSlotIndex);
        if (!AnchorSlotWidget)
        {
            return FVector2D(0.5f, 0.5f);
        }
        
        // Calculate offset based on grid positions
        int32 AnchorX = AnchorSlotIndex % InventoryOwner->GetGridColumns();
        int32 AnchorY = AnchorSlotIndex / InventoryOwner->GetGridColumns();
        
        int32 DeltaX = GridCoordinates.X - AnchorX;
        int32 DeltaY = GridCoordinates.Y - AnchorY;
        
        // Get item size
        FIntPoint ItemSize = CurrentItemData.bIsRotated ? 
            FIntPoint(CurrentItemData.GridSize.Y, CurrentItemData.GridSize.X) : 
            CurrentItemData.GridSize;
        
        // Calculate normalized offset
        float NormalizedX = (DeltaX + 0.5f) / ItemSize.X;
        float NormalizedY = (DeltaY + 0.5f) / ItemSize.Y;
        
        return FVector2D(
            FMath::Clamp(NormalizedX, 0.0f, 1.0f),
            FMath::Clamp(NormalizedY, 0.0f, 1.0f)
        );
    }
    
    // Default to center for single slot or anchor slots
    return FVector2D(0.5f, 0.5f);
}

float USuspenseInventorySlotWidget::CalculateSnapStrength(const FVector2D& DragPosition) const
{
    if (!SnappingConfig.bEnableSnapping)
    {
        return 0.0f;
    }
    
    // Get slot center in screen space
    FVector2D SlotScreenPos = GetCachedGeometry().GetAbsolutePosition() + 
                             GetCachedGeometry().GetLocalSize() * 0.5f;
    
    // Calculate distance
    float Distance = FVector2D::Distance(DragPosition, SlotScreenPos);
    
    // Check if within snap range
    if (Distance > SnappingConfig.SnapDistance)
    {
        return 0.0f;
    }
    
    // Calculate snap strength
    float NormalizedDistance = Distance / SnappingConfig.SnapDistance;
    
    if (SnappingConfig.SnapStrengthCurve)
    {
        return SnappingConfig.SnapStrengthCurve->GetFloatValue(NormalizedDistance);
    }
    else
    {
        // Default quadratic falloff
        return FMath::Pow(1.0f - NormalizedDistance, 2.0f);
    }
}

bool USuspenseInventorySlotWidget::IsWithinSnapRange(const FVector2D& Position) const
{
    if (!SnappingConfig.bEnableSnapping)
    {
        return false;
    }
    
    // Get slot bounds
    FVector2D SlotScreenPos = GetCachedGeometry().GetAbsolutePosition();
    FVector2D SlotWidgetSize = GetCachedGeometry().GetLocalSize();
    
    // Expand bounds by snap distance
    FBox2D SnapBounds(
        SlotScreenPos - FVector2D(SnappingConfig.SnapDistance),
        SlotScreenPos + SlotWidgetSize + FVector2D(SnappingConfig.SnapDistance)
    );
    
    return SnapBounds.IsInside(Position);
}

USuspenseInventoryWidget* USuspenseInventorySlotWidget::GetInventoryOwner() const
{
    return Cast<USuspenseInventoryWidget>(OwningContainer);
}

FVector2D USuspenseInventorySlotWidget::GridToScreenPosition(const FIntPoint& GridPos) const
{
    // Calculate screen position based on grid coordinates
    float ScreenX = GridPos.X * InventoryCellSize;
    float ScreenY = GridPos.Y * InventoryCellSize;
    
    return FVector2D(ScreenX, ScreenY);
}

TArray<int32> USuspenseInventorySlotWidget::GetNeighboringSlots() const
{
    TArray<int32> Neighbors;
    
    USuspenseInventoryWidget* InventoryOwner = GetInventoryOwner();
    if (!InventoryOwner)
    {
        return Neighbors;
    }
    
    int32 GridColumns = InventoryOwner->GetGridColumns();
    int32 GridRows = InventoryOwner->GetGridRows();
    
    // Check all 8 directions
    const int32 Offsets[][2] = {
        {-1, -1}, {0, -1}, {1, -1},
        {-1,  0},          {1,  0},
        {-1,  1}, {0,  1}, {1,  1}
    };
    
    for (const auto& Offset : Offsets)
    {
        int32 NeighborX = GridCoordinates.X + Offset[0];
        int32 NeighborY = GridCoordinates.Y + Offset[1];
        
        // Check bounds
        if (NeighborX >= 0 && NeighborX < GridColumns &&
            NeighborY >= 0 && NeighborY < GridRows)
        {
            int32 NeighborIndex = NeighborY * GridColumns + NeighborX;
            Neighbors.Add(NeighborIndex);
        }
    }
    
    return Neighbors;
}

bool USuspenseInventorySlotWidget::IsAtGridEdge() const
{
    USuspenseInventoryWidget* InventoryOwner = GetInventoryOwner();
    if (!InventoryOwner)
    {
        return false;
    }
    
    int32 GridColumns = InventoryOwner->GetGridColumns();
    int32 GridRows = InventoryOwner->GetGridRows();
    
    return GridCoordinates.X == 0 || 
           GridCoordinates.X == GridColumns - 1 ||
           GridCoordinates.Y == 0 || 
           GridCoordinates.Y == GridRows - 1;
}