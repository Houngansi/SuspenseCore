// MedComInventoryWidget.h
// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Base/MedComBaseContainerWidget.h"
#include "Types/UI/ContainerUITypes.h"
#include "Components/PanelWidget.h"
#include "Components/GridPanel.h"
#include "MedComInventoryWidget.generated.h"

// Forward declarations
class UGridPanel;
class UTextBlock;
class UProgressBar;
class UButton;
class UGridSlot;
class UMedComInventorySlotWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInventoryGridSizeReceived, int32, Columns, int32, Rows);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventorySlotsNeeded, int32, SlotCount);

/**
 * Grid update optimization data
 */
USTRUCT()
struct FInventoryGridUpdateBatch
{
    GENERATED_BODY()

    /** Slots that need span updates */
    TMap<int32, FIntPoint> SlotSpanUpdates;
    
    /** Slots that need visibility updates */
    TMap<int32, bool> SlotVisibilityUpdates;
    
    /** Whether grid needs full refresh */
    bool bNeedsFullGridRefresh = false;
    
    /** Clear batch */
    void Clear()
    {
        SlotSpanUpdates.Empty();
        SlotVisibilityUpdates.Empty();
        bNeedsFullGridRefresh = false;
    }
    
    /** Check if has updates */
    bool HasUpdates() const
    {
        return SlotSpanUpdates.Num() > 0 || 
               SlotVisibilityUpdates.Num() > 0 || 
               bNeedsFullGridRefresh;
    }
};

/**
 * Cached grid slot data for optimization
 */
USTRUCT()
struct FCachedGridSlotData
{
    GENERATED_BODY()
    
    /** Current span */
    UPROPERTY()
    FIntPoint CurrentSpan = FIntPoint(1, 1);
    
    /** Is currently visible */
    UPROPERTY()
    bool bIsVisible = true;
    
    /** Last known item instance */
    UPROPERTY()
    FGuid LastItemInstance;
    
    /** Grid slot reference */
    UPROPERTY()
    UGridSlot* GridSlot = nullptr;
};

/**
 * Grid snap point for magnetic alignment
 */
USTRUCT(BlueprintType)
struct FGridSnapPoint
{
    GENERATED_BODY()
    
 UPROPERTY(BlueprintReadWrite)
 FVector2D GridPosition = FVector2D::ZeroVector; 
    
 UPROPERTY(BlueprintReadWrite)
 FVector2D ScreenPosition = FVector2D::ZeroVector;
 
    
    /** Snap strength */
    UPROPERTY(BlueprintReadOnly)
    float SnapStrength = 0.0f;
    
    /** Is valid snap point */
    UPROPERTY(BlueprintReadOnly)
    bool bIsValid = false;
};

/**
 * Highly optimized inventory widget with intelligent grid management
 * Features differential updates, magnetic snapping, and efficient multi-slot handling
 */
UCLASS()
class MEDCOMUI_API UMedComInventoryWidget : public UMedComBaseContainerWidget
{
    GENERATED_BODY()

public:
    UMedComInventoryWidget(const FObjectInitializer& ObjectInitializer);

    //~ Begin UUserWidget Interface
    virtual void NativeConstruct() override;
    virtual void NativePreConstruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
    virtual void SetVisibility(ESlateVisibility InVisibility) override;
    //~ End UUserWidget Interface

    //~ Begin UMedComBaseContainerWidget Interface
    virtual void InitializeContainer_Implementation(const FContainerUIData& ContainerData) override;
    virtual void UpdateContainer_Implementation(const FContainerUIData& ContainerData) override;
    
    virtual FSlotValidationResult CanAcceptDrop_Implementation(
        const UDragDropOperation* DragOperation, 
        int32 TargetSlotIndex) const override;
 
    virtual UPanelWidget* GetSlotsPanel() const override { return Cast<UPanelWidget>(InventoryGrid); }
    virtual float GetCellSize() const override { return CellSize; }
    
    virtual FSmartDropZone FindBestDropZone(
        const FVector2D& ScreenPosition,
        const FIntPoint& ItemSize,
        bool bIsRotated) const override;
    
    virtual bool CalculateOccupiedSlots(
        int32 TargetSlot, 
        FIntPoint ItemSize, 
        bool bIsRotated, 
        TArray<int32>& OutOccupiedSlots) const override;
    //~ End UMedComBaseContainerWidget Interface

    /**
     * Get container identifier tag
     * @return Container identifier
     */
    virtual FGameplayTag GetContainerIdentifier_Implementation() const;

    /**
     * Force refresh inventory data
     * Used when widget first becomes visible
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void RequestInventoryRefresh();

    /**
     * Set visual grid dimensions
     * @param Columns Number of columns in grid
     * @param Rows Number of rows in grid
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void SetGridDimensions(int32 Columns, int32 Rows);

    /**
     * Get slot index from grid coordinates
     * @param GridX X coordinate in grid
     * @param GridY Y coordinate in grid
     * @return Slot index or INDEX_NONE if invalid
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    int32 GetSlotIndexFromGridCoords(int32 GridX, int32 GridY) const;

    /**
     * Get grid coordinates from slot index
     * @param SlotIndex Slot index
     * @param OutGridX Output X coordinate
     * @param OutGridY Output Y coordinate
     * @return True if valid slot index
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool GetGridCoordsFromSlotIndex(int32 SlotIndex, int32& OutGridX, int32& OutGridY) const;

    /**
     * Find item at screen position accounting for multi-slot items
     * @param ScreenPosition Screen coordinates
     * @param OutAnchorSlot Returns anchor slot of item at position
     * @return True if item found
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool FindItemAtScreenPosition(const FVector2D& ScreenPosition, int32& OutAnchorSlot) const;
    
    /**
     * Get best grid snap point near position
     * @param ScreenPosition Screen position to check
     * @param ItemSize Size of item being dragged
     * @return Grid snap point data
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    FGridSnapPoint GetBestGridSnapPoint(const FVector2D& ScreenPosition, const FIntPoint& ItemSize) const;

    /**
     * Check if widget is fully initialized
     * @return True if initialized with proper data
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|State")
    bool IsFullyInitialized() const { return bIsFullyInitialized; }

    /**
     * Check if grid slots have been created
     * @return True if grid is initialized
     */
    UFUNCTION(BlueprintPure, Category = "Inventory|State")
    bool IsGridInitialized() const { return bGridInitialized; }

    /**
     * Run widget diagnostics for debugging
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug", meta = (DevelopmentOnly))
    void DiagnoseWidget() const;

    /**
     * Get grid performance metrics
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug", meta = (DevelopmentOnly))
    void LogGridPerformanceMetrics() const;

    /**
     * Get the inventory grid directly
     * Useful for Blueprint access
     */
    UFUNCTION(BlueprintPure, Category = "Inventory")
    UGridPanel* GetInventoryGrid() const { return InventoryGrid; }

    /**
     * Get grid columns for calculations
     * @return Number of columns
     */
    UFUNCTION(BlueprintPure, Category = "Inventory")
    int32 GetGridColumns() const { return GridColumns; }

    /**
     * Get grid rows for calculations
     * @return Number of rows
     */
    UFUNCTION(BlueprintPure, Category = "Inventory")
    int32 GetGridRows() const { return GridRows; }

    /**
     * Get anchor slot for any slot
     * @param SlotIndex Slot to check
     * @return Anchor slot index if part of multi-slot item, or same index
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    int32 GetAnchorSlotForSlot(int32 SlotIndex) const;
    
    /**
     * Check if position is valid for item placement
     * @param GridX Start X position
     * @param GridY Start Y position  
     * @param ItemSize Size of item
     * @return True if item fits at position
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool IsValidPlacementPosition(int32 GridX, int32 GridY, const FIntPoint& ItemSize) const;
    
    /**
     * Get selected slot index
     * @return Currently selected slot index
     */
    int32 GetSelectedSlotIndex() const { return SelectedSlotIndex; }
 
protected:
    /** 
     * Grid panel for slot layout
     * MUST be bound in Blueprint!
     */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UGridPanel* InventoryGrid;

    //~ Inventory-specific UI components
    
    /** Text showing current/max weight */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* WeightText;

    /** Progress bar for weight visualization */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UProgressBar* WeightBar;

    /** Text showing inventory title/name */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* InventoryTitle;

    /** Close button */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UButton* CloseButton;

    /** Sort button */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UButton* SortButton;

    //~ Configuration
    
    /** Slot class to create (can be overridden in Blueprint) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|EventBinding", meta = (ExposeOnSpawn = true))
    TSubclassOf<UMedComBaseSlotWidget> InventorySlotClass;

    /** Default grid dimensions */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|EventBinding", meta = (ExposeOnSpawn = true))
    int32 DefaultGridColumns;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|EventBinding", meta = (ExposeOnSpawn = true))
    int32 DefaultGridRows;

    /** Default cell size */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|EventBinding", meta = (ExposeOnSpawn = true))
    float DefaultCellSize;
    
    /** Enable grid snap visualization */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Grid")
    bool bShowGridSnapVisualization;
    
    /** Grid snap visualization strength */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Grid", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float GridSnapVisualizationStrength;

    /** Enable smart drop zones */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Grid")
    bool bEnableSmartDropZones;
    
    /** Smart drop search radius in pixels */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory|Grid", meta = (ClampMin = "50.0", ClampMax = "200.0"))
    float SmartDropRadius;

    //~ Events

    /** Called when grid size received from Bridge */
    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnInventoryGridSizeReceived OnInventoryGridSizeReceived;

    /** Called when slots need to be created */
    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnInventorySlotsNeeded OnInventorySlotsNeeded;

    //~ Settings
    
    /** Number of columns in inventory grid */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Layout")
    int32 GridColumns;

    /** Number of rows in inventory grid */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Layout")
    int32 GridRows;
 
    /** Padding between cells */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Layout")
    float CellPadding;

    /** Whether to show weight info */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Display")
    bool bShowWeight;

    /** Percentage at which weight warning starts */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Display", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float WeightWarningThreshold;

    //~ Overridden base methods
    virtual void CreateSlots() override;
    virtual void UpdateSlotWidget(int32 SlotIndex, const FSlotUIData& SlotData, const FItemUIData& ItemData) override;
    virtual bool ValidateSlotsPanel() const override;

    /**
     * Update weight display based on container data
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void UpdateWeightDisplay();

    /**
     * Close button click handler
     */
    UFUNCTION()
    void OnCloseButtonClicked();

    /**
     * Sort button click handler
     */
    UFUNCTION()
    void OnSortButtonClicked();

    /**
     * Request rotation of selected item
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void RequestRotateSelectedItem();

    /**
     * Check if item fits within grid bounds
     * @param GridX Starting X position
     * @param GridY Starting Y position
     * @param ItemWidth Item width in cells
     * @param ItemHeight Item height in cells
     * @return True if item fits in grid
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool IsWithinGridBounds(int32 GridX, int32 GridY, int32 ItemWidth, int32 ItemHeight) const;
 // =====================================================
 // Drag & Drop Visual Override
 // =====================================================
    
 /**
  * Get cell size for drag visual from inventory grid
  * @return Actual cell size used in grid
  */
 virtual float GetDragVisualCellSize() const override;
private:
    /** Optimized slot creation method */
    void CreateSlotsOptimized();
    
    /** Differential slot updates */
    void ApplyDifferentialSlotUpdates(const FContainerUIData& ContainerData);
    
    /** Update grid layout for multi-slot items */
    void UpdateGridLayoutForMultiSlotItems();
    
    /**
     * Update grid slot span for multi-slot items
     * @param SlotWidget The slot widget to update
     * @param ItemData The item data containing size information
     */
    void UpdateGridSlotSpan(UMedComInventorySlotWidget* SlotWidget, const FItemUIData& ItemData);

    /** Cache which slots are occupied by multi-slot items */
    TMap<int32, int32> SlotToAnchorMap; // Maps any occupied slot to its item's anchor slot
    
    /** Cached grid slot data for optimization */
    TMap<int32, FCachedGridSlotData> CachedGridSlotData;
    
    /** Pending grid update batch */
    FInventoryGridUpdateBatch PendingGridUpdateBatch;

    /** Update slot-anchor mapping based on current items */
    void UpdateSlotOccupancyMap();

    /** Subscribe to inventory-specific events */
    void SubscribeToInventoryEvents();

    /** Inventory data update handler from event system */
    UFUNCTION()
    void OnInventoryDataUpdated(const FContainerUIData& NewData);

    /** Item rotation request handler */
    UFUNCTION()
    void OnRotateItemRequested(int32 SlotIndex);

    /** Find best fit position for item near target */
    int32 FindBestFitSlot(int32 TargetSlot, FIntPoint ItemSize, bool bIsRotated) const;
    
    /** Convert screen position to grid coordinates */
    FIntPoint ScreenToGridCoordinates(const FVector2D& ScreenPos) const;
    
    /** Get screen bounds of grid cell */
    FBox2D GetGridCellScreenBounds(int32 GridX, int32 GridY) const;
    
    /** Validate critical components */
    bool ValidateCriticalComponents() const;
    
    /** Auto-bind components by name */
    void AutoBindComponents();

    /** Flag indicating grid has been initialized */
    bool bGridInitialized;
    
    /** Flag indicating widget is fully initialized with proper data */
    bool bIsFullyInitialized;
    
    /** Last grid update timestamp */
    float LastGridUpdateTime;
    
    /** Grid update counter for metrics */
    int32 GridUpdateCounter;
    
    /** Active grid snap point for visualization */
    FGridSnapPoint ActiveGridSnapPoint;
};