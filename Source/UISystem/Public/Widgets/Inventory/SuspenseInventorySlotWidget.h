// MedComInventorySlotWidget.h
// Copyright Suspense Team Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Base/SuspenseBaseSlotWidget.h"
#include "Types/UI/ContainerUITypes.h"
#include "SuspenseInventorySlotWidget.generated.h"

// Forward declarations
class UBorder;
class UTextBlock;
class UImage;
class USuspenseInventoryWidget;
class USlateBrushAsset;
class UProgressBar;

/**
 * Inventory slot snapping configuration
 */
USTRUCT(BlueprintType)
struct FInventorySlotSnappingConfig
{
    GENERATED_BODY()

    /** Enable snapping */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableSnapping = true;
    
    /** Snap distance in pixels */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "10.0", ClampMax = "100.0"))
    float SnapDistance = 50.0f;
    
    /** Snap strength curve based on distance */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UCurveFloat* SnapStrengthCurve = nullptr;
};

/**
 * Inventory-specific slot visual properties
 * Extends base slot visuals with inventory features
 */
USTRUCT()
struct UISYSTEM_API FInventorySlotVisualProperties
{
    GENERATED_BODY()

    /** Rarity border visibility */
    UPROPERTY()
    bool bRarityVisible = false;
    
    /** Rarity color */
    UPROPERTY()
    FLinearColor RarityColor = FLinearColor::White;
    
    /** Grid coordinate visibility */
    UPROPERTY()
    bool bCoordTextVisible = false;
    
    /** Grid coordinates */
    UPROPERTY()
    FIntPoint GridCoords = FIntPoint::ZeroValue;
    
    /** Icon size for multi-slot items */
    UPROPERTY()
    FVector2D IconSize = FVector2D::ZeroVector;
    
    /** Icon offset for centered display */
    UPROPERTY()
    FVector2D IconOffset = FVector2D::ZeroVector;
    
    /** Whether slot is part of multi-slot item */
    UPROPERTY()
    bool bIsPartOfMultiSlot = false;

    FInventorySlotVisualProperties()
    {
        bRarityVisible = false;
        RarityColor = FLinearColor::White;
        bCoordTextVisible = false;
        GridCoords = FIntPoint::ZeroValue;
        IconSize = FVector2D::ZeroVector;
        IconOffset = FVector2D::ZeroVector;
        bIsPartOfMultiSlot = false;
    }
};

/**
 * Optimized inventory-specific slot widget with grid support
 * Features efficient multi-slot rendering and grid-aware interactions
 */
UCLASS()
class UISYSTEM_API USuspenseInventorySlotWidget : public USuspenseBaseSlotWidget
{
    GENERATED_BODY()

public:
    USuspenseInventorySlotWidget(const FObjectInitializer& ObjectInitializer);

    //==================================================================
    // Widget Events
    //==================================================================

    virtual void NativePreConstruct() override;
    virtual void NativeConstruct() override;

    //==================================================================
    // Inventory Slot Interface
    //==================================================================

    /**
     * Initialize inventory-specific properties
     * @param InCellSize Size of grid cell
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Slot")
    void InitializeInventorySlot(float InCellSize);

    /**
     * Set grid coordinates for this slot
     * @param X Grid X coordinate
     * @param Y Grid Y coordinate
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Slot")
    void SetGridCoordinates(int32 X, int32 Y);

    /**
     * Get grid coordinates
     * @return Grid position
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Slot")
    FIntPoint GetGridCoordinates() const { return GridCoordinates; }
    
    /**
     * Check if this slot is anchor for multi-slot item
     * @return True if anchor slot
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Slot")
    bool IsAnchorSlot() const { return CurrentSlotData.bIsAnchor; }
    
    /**
     * Get effective icon size for current item
     * @return Icon size in pixels
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|Slot")
    FVector2D GetEffectiveIconSize() const;
    
    /**
     * Update rarity based on current item
     * @param ItemRarity Rarity tag of item
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Slot")
    void UpdateItemRarity(const FGameplayTag& ItemRarity);

protected:
    //==================================================================
    // Widget Components
    //==================================================================

    /** Text widget for displaying grid coordinates */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, OptionalWidget = true))
    UTextBlock* GridCoordText;

    /** Border for item rarity display */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, OptionalWidget = true))
    UBorder* RarityBorder;

    /** Additional overlay for multi-slot items */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, OptionalWidget = true))
    UBorder* MultiSlotOverlay;
    
    /** Durability bar (optional) */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget, OptionalWidget = true))
    UProgressBar* DurabilityBar;

    //==================================================================
    // Configuration
    //==================================================================

    /** Cell size specific to inventory grid */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Config")
    float InventoryCellSize;

    /** Cell size override from Blueprint */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Config", meta = (ExposeOnSpawn = true))
    float CellSizeOverride;

    /** Whether to show grid coordinates */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Config")
    bool bShowGridCoordinates;

    /** Override for showing coordinates */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Config", meta = (ExposeOnSpawn = true))
    bool bShowCoordinatesOverride;

    /** Rarity color mappings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Config")
    TMap<FGameplayTag, FLinearColor> RarityColors;

    /** Default rarity color */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Config")
    FLinearColor DefaultRarityColor;
    
    /** Enable optimized icon rendering for multi-slot items */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Config")
    bool bOptimizeMultiSlotIcons;
    
    /** Icon scale for multi-slot items (0.5-1.0) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Config", meta = (ClampMin = "0.5", ClampMax = "1.0"))
    float MultiSlotIconScale;
    
    /** Durability warning threshold (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DurabilityWarningThreshold;
    
    /** Snapping configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Config")
    FInventorySlotSnappingConfig SnappingConfig;

    //==================================================================
    // State
    //==================================================================

    /** Grid coordinates of this slot */
    UPROPERTY(BlueprintReadOnly, Category = "Inventory|State")
    FIntPoint GridCoordinates;
    
    /** Cached inventory visual properties */
    FInventorySlotVisualProperties CachedInventoryVisuals;
    
    /** Pending inventory visual properties */
    FInventorySlotVisualProperties PendingInventoryVisuals;
    
    /** Cached icon brush for performance */
    FSlateBrush CachedIconBrush;
    
    /** Whether icon brush needs update */
    bool bIconBrushDirty;
    
    /** Cached multi-slot size */
    FIntPoint CachedMultiSlotSize;
    
    /** Whether this slot should handle multi-slot rendering */
    bool bIsRenderingMultiSlot;

    //==================================================================
    // Visual Updates - Optimized
    //==================================================================

    /** Update visual state with inventory features */
    virtual void UpdateVisualState() override;
    
    /** Update rarity border display */
    void UpdateRarityDisplay();

    /** Update item icon with optimizations */
    virtual void UpdateItemIcon() override;
    
    /** Update grid coordinate display */
    void UpdateGridCoordinateDisplay();
    
    /** Update durability display */
    void UpdateDurabilityDisplay();
    
    /** Update multi-slot overlay */
    void UpdateMultiSlotOverlay();
    
    /** Calculate icon transform for multi-slot items */
    FVector2D CalculateMultiSlotIconSize() const;
    
    /** Get icon offset for centered display */
    FVector2D CalculateIconOffset(const FVector2D& IconSize) const;
    
    /** Create optimized icon brush */
    void UpdateCachedIconBrush();
    
    /** Should render as multi-slot item */
    bool ShouldRenderAsMultiSlot() const;
    
    /** Request visual update */
    void RequestVisualUpdate();

    //==================================================================
    // Performance Helpers
    //==================================================================
    
    /** Check if inventory visuals need update */
    bool NeedsInventoryVisualUpdate() const;
    
    /** Apply inventory visual properties */
    void ApplyInventoryVisualProperties(const FInventorySlotVisualProperties& Props);
    
    /** Clear inventory-specific caches */
    void ClearInventoryCaches();

    //==================================================================
    // Interface Implementations
    //==================================================================

    virtual void InitializeSlot_Implementation(const FSlotUIData& SlotData, const FItemUIData& ItemData) override;
    virtual void UpdateSlot_Implementation(const FSlotUIData& SlotData, const FItemUIData& ItemData) override;
    
    // Override drag interface methods for inventory-specific behavior
    virtual bool CanBeDragged_Implementation() const override;
    virtual FDragDropUIData GetDragData_Implementation() const override;
    
    //==================================================================
    // Grid Helpers
    //==================================================================
    
    /** Get inventory widget owner */
    USuspenseInventoryWidget* GetInventoryOwner() const;
    
    /** Convert grid coords to screen position */
    FVector2D GridToScreenPosition(const FIntPoint& GridPos) const;
    
    /** Get neighboring slot indices */
    TArray<int32> GetNeighboringSlots() const;
    
    /** Check if slot is at grid edge */
    bool IsAtGridEdge() const;
    
    /** Calculate drag offset for grid-based dragging */
    FVector2D CalculateDragOffset(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) const;
    
    /** Calculate snap strength based on distance */
    float CalculateSnapStrength(const FVector2D& DragPosition) const;
    
    /** Check if position is within snap range */
    bool IsWithinSnapRange(const FVector2D& Position) const;
};