// MedComBaseContainerWidget.h - Optimized version
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/UI/ISuspenseContainerUI.h"
#include "Interfaces/UI/ISuspenseUIWidget.h"
#include "Widgets/DragDrop/SuspenseDragVisualWidget.h"
#include "Types/UI/SuspenseContainerUITypes.h"
#include "SuspenseBaseContainerWidget.generated.h"

// Forward declarations
class UPanelWidget;
class USuspenseBaseSlotWidget;
class USuspenseDragDropOperation;
class USuspenseDragDropHandler;
class USuspenseEventManager;

/**
 * Widget pool for slot reuse
 */
USTRUCT()
struct FSlotWidgetPool
{
	GENERATED_BODY()

	/** Available slots for reuse */
	UPROPERTY()
	TArray<USuspenseBaseSlotWidget*> AvailableSlots;

	/** All created slots */
	UPROPERTY()
	TArray<USuspenseBaseSlotWidget*> AllSlots;

	/** Get or create slot - изменяем тип параметра */
	USuspenseBaseSlotWidget* AcquireSlot(UUserWidget* Outer, TSubclassOf<USuspenseBaseSlotWidget> SlotClass);

	/** Return slot to pool */
	void ReleaseSlot(USuspenseBaseSlotWidget* Slot);

	/** Clear pool */
	void Clear();
};

/**
 * Optimized container widget with slot pooling and deferred updates
 * Focuses on performance through intelligent caching and update batching
 */
UCLASS(Abstract)
class UISYSTEM_API USuspenseBaseContainerWidget : public UUserWidget,
    public ISuspenseContainerUI,
    public ISuspenseUIWidget
{
    GENERATED_BODY()

public:
    USuspenseBaseContainerWidget(const FObjectInitializer& ObjectInitializer);

    // =====================================================
    // Performance Settings
    // =====================================================

    /**
     * Enable slot widget pooling
     * @param bEnable Whether to enable pooling
     */
    UFUNCTION(BlueprintCallable, Category = "Container|Performance")
    void SetSlotPoolingEnabled(bool bEnable) { bEnableSlotPooling = bEnable; }

    /**
     * Get whether slot pooling is enabled
     * @return True if pooling is enabled
     */
    UFUNCTION(BlueprintPure, Category = "Container|Performance")
    bool IsSlotPoolingEnabled() const { return bEnableSlotPooling; }

    /**
     * Set update batching delay
     * @param Delay Delay in seconds
     */
    UFUNCTION(BlueprintCallable, Category = "Container|Performance")
    void SetUpdateBatchDelay(float Delay) { UpdateBatchDelay = FMath::Max(0.0f, Delay); }

    // =====================================================
    // Drag & Drop Visual Methods
    // =====================================================

    /**
     * Get the drag visual widget class
     * Can be overridden in Blueprint for custom visuals
     * @return Drag visual widget class
     */
    UFUNCTION(BlueprintPure, Category = "Container|DragDrop")
    virtual TSubclassOf<USuspenseDragVisualWidget> GetDragVisualWidgetClass() const;

    /**
     * Create drag visual widget for drag operation
     * @param DragData Data for the drag operation
     * @return Created widget or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "Container|DragDrop")
    virtual USuspenseDragVisualWidget* CreateDragVisualWidget(const FDragDropUIData& DragData);

    /**
     * Get cell size for drag visual
     * @return Cell size in pixels
     */
    UFUNCTION(BlueprintPure, Category = "Container|DragDrop")
    virtual float GetDragVisualCellSize() const;

protected:
    //========================================
    // Configuration
    //========================================

    /** Slot widget class to use */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Container Config")
    TSubclassOf<USuspenseBaseSlotWidget> SlotWidgetClass;

    /** Container type tag */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Container Config")
    FGameplayTag ContainerType;

    /** Cell size in pixels */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Container Config", meta = (ClampMin = "32", ClampMax = "128"))
    float CellSize = 64.0f;

    //========================================
    // Performance Settings
    //========================================

    /** Enable slot widget pooling */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Container|Performance")
    bool bEnableSlotPooling = true;

    /** Maximum slots to keep in pool */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Container|Performance", meta = (ClampMin = "0", ClampMax = "1000"))
    int32 MaxPooledSlots = 200;

    /** Update batch delay in seconds */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Container|Performance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float UpdateBatchDelay = 0.033f; // ~30 FPS

    //========================================
    // State
    //========================================

    /** Is widget fully initialized */
    UPROPERTY(BlueprintReadOnly, Category = "Container State")
    bool bIsInitialized;

    /** Current container data */
    UPROPERTY(BlueprintReadOnly, Category = "Container State")
    FContainerUIData CurrentContainerData;

    /** Map of slot index to slot widget */
    UPROPERTY()
    TMap<int32, USuspenseBaseSlotWidget*> SlotWidgets;

    /** Currently selected slot index */
    UPROPERTY(BlueprintReadOnly, Category = "Container State")
    int32 SelectedSlotIndex;

    /** Cached drag drop handler */
    UPROPERTY()
    USuspenseDragDropHandler* CachedDragDropHandler;

    /** Cached delegate manager */
    UPROPERTY()
    USuspenseEventManager* CachedDelegateManager;

    //========================================
    // Native Widget Overrides
    //========================================

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

public:
    //========================================
    // ISuspenseUIWidgetInterface
    //========================================

    virtual void InitializeWidget_Implementation() override;
    virtual void UninitializeWidget_Implementation() override;
    virtual void UpdateWidget_Implementation(float DeltaTime) override;
    virtual FGameplayTag GetWidgetTag_Implementation() const override;
    virtual USuspenseEventManager* GetDelegateManager() const override;

    virtual bool IsFullyInitialized() const { return bIsInitialized; }

    //========================================
    // ISuspenseContainerUIInterface
    //========================================

    virtual void InitializeContainer_Implementation(const FContainerUIData& ContainerData) override;
    virtual void UpdateContainer_Implementation(const FContainerUIData& ContainerData) override;
    virtual FGameplayTag GetContainerType_Implementation() const override { return ContainerType; }
    virtual void RequestDataRefresh_Implementation() override;

    /** Slot interaction handlers */
    virtual void OnSlotClicked_Implementation(int32 SlotIndex, const FGuid& ItemInstanceID) override;
    virtual void OnSlotDoubleClicked_Implementation(int32 SlotIndex, const FGuid& ItemInstanceID) override;
    virtual void OnSlotRightClicked_Implementation(int32 SlotIndex, const FGuid& ItemInstanceID) override;

    /** Simplified drag & drop - just validation */
    virtual FSlotValidationResult CanAcceptDrop_Implementation(
        const UDragDropOperation* DragOperation,
        int32 TargetSlotIndex) const override;

    virtual void HandleItemDropped_Implementation(
        UDragDropOperation* DragOperation,
        int32 TargetSlotIndex) override;

    //========================================
    // Slot Management
    //========================================

    /** Get slot widget by index */
    UFUNCTION(BlueprintPure, Category = "Container")
    USuspenseBaseSlotWidget* GetSlotWidget(int32 SlotIndex) const;

    /** Get all slot widgets */
    UFUNCTION(BlueprintPure, Category = "Container")
    TArray<USuspenseBaseSlotWidget*> GetAllSlotWidgets() const;

    /** Get slot at screen position */
    UFUNCTION(BlueprintPure, Category = "Container")
    USuspenseBaseSlotWidget* GetSlotAtScreenPosition(const FVector2D& ScreenPosition) const;

    /** Get slots in region */
    UFUNCTION(BlueprintCallable, Category = "Container")
    TArray<USuspenseBaseSlotWidget*> GetSlotsInRegion(const FVector2D& Center, float Radius) const;

    /** Get cell size */
    virtual float GetCellSize() const { return CellSize; }

    /** Get current container data */
    UFUNCTION(BlueprintPure, Category = "Container")
    const FContainerUIData& GetCurrentContainerData() const { return CurrentContainerData; }

    /** Handle slot selection change */
    virtual void OnSlotSelectionChanged(int32 SlotIndex, bool bIsSelected);

    /** Calculate occupied slots for item placement */
    virtual bool CalculateOccupiedSlots(
        int32 TargetSlot,
        FIntPoint ItemSize,
        bool bIsRotated,
        TArray<int32>& OutOccupiedSlots) const;

    /** Find best drop zone (simplified) */
    virtual FSmartDropZone FindBestDropZone(
        const FVector2D& ScreenPosition,
        const FIntPoint& ItemSize,
        bool bIsRotated) const;

    //========================================
    // Simplified Drag Drop Interface
    //========================================

    /** Process drop operation - delegates to handler */
    bool ProcessDropOnSlot(
        USuspenseDragDropOperation* DragOperation,
        USuspenseBaseSlotWidget* SlotWidget,
        const FVector2D& ScreenPosition,
        const FGeometry& SlotGeometry);

    /** Process drag over - delegates to handler */
    bool ProcessDragOverSlot(
        USuspenseDragDropOperation* DragOperation,
        USuspenseBaseSlotWidget* SlotWidget,
        const FVector2D& ScreenPosition,
        const FGeometry& SlotGeometry);

    /** Process drag enter - delegates to handler */
    void ProcessDragEnterSlot(
        const USuspenseDragDropOperation* DragOperation,
        USuspenseBaseSlotWidget* SlotWidget);

    /** Clear slot highlights - delegates to handler */
    void ClearSlotHighlights();

protected:
    //========================================
    // Slot Creation and Updates
    //========================================

    /** Create slot widgets based on container data */
    virtual void CreateSlots();

    /** Clear all slots */
    virtual void ClearSlots();

    /** Update individual slot widget */
    virtual void UpdateSlotWidget(int32 SlotIndex, const FSlotUIData& SlotData, const FItemUIData& ItemData);

    /** Get the panel widget that contains slots */
    virtual UPanelWidget* GetSlotsPanel() const { return nullptr; }

    /** Validate slots panel */
    virtual bool ValidateSlotsPanel() const;

    /** Find slot data by index */
    const FSlotUIData* FindSlotData(int32 SlotIndex) const;

    /** Find item data by slot */
    const FItemUIData* FindItemDataForSlot(int32 SlotIndex) const;

    //========================================
    // Event Handling
    //========================================

    /** Subscribe to relevant events */
    virtual void SubscribeToEvents();

    /** Unsubscribe from events */
    virtual void UnsubscribeFromEvents();

    /** Get drag drop handler */
    USuspenseDragDropHandler* GetDragDropHandler() const;

    // =====================================================
    // Drag & Drop Visual Configuration
    // =====================================================

    /**
     * Class to use for drag visual widget
     * Set this in Blueprint to customize drag appearance
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Container|DragDrop",
        meta = (DisplayName = "Drag Visual Widget Class"))
    TSubclassOf<USuspenseDragVisualWidget> DragVisualWidgetClass;

    /**
     * Default cell size for drag visual
     * Used when actual cell size is not available
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Container|DragDrop",
        meta = (ClampMin = "16", ClampMax = "256"))
    float DefaultDragVisualCellSize = 64.0f;

    /**
     * Whether to show quantity text on drag visual
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Container|DragDrop")
    bool bShowQuantityOnDrag = true;

    /**
     * Whether to show item rarity on drag visual
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Container|DragDrop")
    bool bShowRarityOnDrag = true;

private:
    //========================================
    // Performance Optimization
    //========================================

    /** Slot widget pool */
    FSlotWidgetPool SlotPool;

    /** Pending slot updates */
    TMap<int32, TPair<FSlotUIData, FItemUIData>> PendingSlotUpdates;

    /** Update batch timer */
    FTimerHandle UpdateBatchTimer;

    /** Last update time */
    float LastUpdateTime = 0.0f;

    /** Process batched updates */
    void ProcessBatchedUpdates();

    /** Schedule slot update */
    void ScheduleSlotUpdate(int32 SlotIndex, const FSlotUIData& SlotData, const FItemUIData& ItemData);

    /** Create slot with pooling support */
    USuspenseBaseSlotWidget* CreateOrAcquireSlot();

	/** Return slot to pool */
	void ReleaseSlot(USuspenseBaseSlotWidget* SlotWidget);
};
