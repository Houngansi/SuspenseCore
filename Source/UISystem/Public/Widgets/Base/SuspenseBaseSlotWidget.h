// MedComBaseSlotWidget.h - Optimized version with tooltip support through EventDelegateManager
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreSlotUI.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreDraggable.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreDropTarget.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreTooltipSource.h"
#include "Widgets/Tooltip/SuspenseItemTooltipWidget.h"
#include "SuspenseCore/Types/UI/SuspenseCoreContainerUITypes.h"
#include "Engine/StreamableManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseBaseSlotWidget.generated.h"

// Forward declarations
class UBorder;
class UImage;
class UTextBlock;
class USizeBox;
class USuspenseBaseContainerWidget;
class USuspenseDragDropHandler;
class USuspenseEventManager;
class UTexture2D;

/**
 * Performance-optimized slot widget with caching and smart updates
 * Implements widget pooling support and deferred visual updates
 * Integrated tooltip support through EventDelegateManager
 */
UCLASS(Abstract)
class UISYSTEM_API USuspenseBaseSlotWidget : public UUserWidget,
    public ISuspenseCoreSlotUI,
    public ISuspenseCoreDraggable,
    public ISuspenseCoreDropTarget,
    public ISuspenseCoreTooltipSource
{
    GENERATED_BODY()

public:
    USuspenseBaseSlotWidget(const FObjectInitializer& ObjectInitializer);

    // ========================================
    // Widget Pooling Support
    // ========================================

    /**
     * Reset slot to empty state for reuse
     * Called when returning widget to pool
     */
    UFUNCTION(BlueprintCallable, Category = "Slot|Pooling")
    virtual void ResetForPool();

    /**
     * Check if slot can be returned to pool
     * @return True if slot is ready for pooling
     */
    UFUNCTION(BlueprintPure, Category = "Slot|Pooling")
    virtual bool CanBePooled() const;

    /**
     * Mark slot as pooled
     * @param bPooled Whether slot is in pool
     */
    UFUNCTION(BlueprintCallable, Category = "Slot|Pooling")
    void SetPooled(bool bPooled) { bIsPooled = bPooled; }

protected:
    //========================================
    // Widget Components
    //========================================

    /** Background border for slot styling */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UBorder* BackgroundBorder;

    /** Icon image for displaying item */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UImage* ItemIcon;

    /** Text for item quantity */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* QuantityText;

    /** Size box for controlling slot dimensions */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    USizeBox* RootSizeBox;

    /** Optional highlight border */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UBorder* HighlightBorder;

    /** Optional selection border */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UBorder* SelectionBorder;

    //========================================
    // Configuration
    //========================================

    /** Size of the slot in pixels */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot Config")
    float SlotSize = 48.0f;

    /** Colors for different slot states */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot Config|Colors")
    FLinearColor EmptySlotColor = FLinearColor(0.1f, 0.1f, 0.1f, 0.5f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot Config|Colors")
    FLinearColor OccupiedSlotColor = FLinearColor(0.15f, 0.15f, 0.15f, 0.8f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot Config|Colors")
    FLinearColor SelectedColor = FLinearColor(0.2f, 0.5f, 1.0f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot Config|Colors")
    FLinearColor HoveredColor = FLinearColor(0.3f, 0.3f, 0.3f, 0.8f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot Config|Colors")
    FLinearColor LockedColor = FLinearColor(0.5f, 0.1f, 0.1f, 0.8f);

    //========================================
    // Tooltip Configuration
    //========================================

    /** Whether tooltips are enabled for this slot */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slot Config|Tooltip")
    bool bEnableTooltip = true;

    /** Delay before showing tooltip in seconds */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slot Config|Tooltip", meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float TooltipDelay = 0.5f;

    /** Custom tooltip widget class for this slot type */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slot Config|Tooltip")
    TSubclassOf<USuspenseItemTooltipWidget> CustomTooltipClass;

    //========================================
    // State
    //========================================

    /** Current slot data */
    UPROPERTY(BlueprintReadOnly, Category = "Slot State")
    FSlotUIData CurrentSlotData;

    /** Current item data (if any) */
    UPROPERTY(BlueprintReadOnly, Category = "Slot State")
    FItemUIData CurrentItemData;

    /** Visual states */
    UPROPERTY(BlueprintReadOnly, Category = "Slot State")
    bool bIsSelected;

    UPROPERTY(BlueprintReadOnly, Category = "Slot State")
    bool bIsHovered;

    UPROPERTY(BlueprintReadOnly, Category = "Slot State")
    bool bIsHighlighted;

    UPROPERTY(BlueprintReadOnly, Category = "Slot State")
    bool bIsLocked;

    UPROPERTY(BlueprintReadOnly, Category = "Slot State")
    bool bIsDragging;

    /** Whether tooltip is currently shown for this slot */
    UPROPERTY(BlueprintReadOnly, Category = "Slot State")
    bool bIsTooltipActive;

    /** Owning container widget */
    UPROPERTY()
    USuspenseBaseContainerWidget* OwningContainer;

    /** Cached drag drop handler */
    UPROPERTY()
    USuspenseDragDropHandler* CachedDragDropHandler;

    /** Cached event manager */
    UPROPERTY()
    USuspenseCoreEventManager* CachedEventManager;

    //========================================
    // Native Widget Overrides
    //========================================

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    /** Drag & drop overrides */
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

public:
    //========================================
    // ISuspenseCoreSlotUIInterface
    //========================================

    virtual void InitializeSlot_Implementation(const FSlotUIData& SlotData, const FItemUIData& ItemData) override;
    virtual void UpdateSlot_Implementation(const FSlotUIData& SlotData, const FItemUIData& ItemData) override;
    virtual void SetSelected_Implementation(bool bInIsSelected) override;
    virtual void SetHighlighted_Implementation(bool bInIsHighlighted, const FLinearColor& HighlightColor) override;
    virtual void SetLocked_Implementation(bool bInIsLocked) override;

    virtual int32 GetSlotIndex_Implementation() const override { return CurrentSlotData.SlotIndex; }
    virtual FGuid GetItemInstanceID_Implementation() const override { return CurrentItemData.ItemInstanceID; }
    virtual bool IsOccupied_Implementation() const override { return CurrentSlotData.bIsOccupied; }

    //========================================
    // ISuspenseCoreDraggableInterface
    //========================================

    virtual bool CanBeDragged_Implementation() const override;
    virtual FDragDropUIData GetDragData_Implementation() const override;
    virtual void OnDragStarted_Implementation() override;
    virtual void OnDragEnded_Implementation(bool bWasDropped) override;
    virtual void UpdateDragVisual_Implementation(bool bIsValidTarget) override;

    //========================================
    // ISuspenseCoreDropTargetInterface
    //========================================

    virtual FSlotValidationResult CanAcceptDrop_Implementation(const UDragDropOperation* DragOperation) const override;
    virtual bool HandleDrop_Implementation(UDragDropOperation* DragOperation) override;
    virtual void NotifyDragEnter_Implementation(UDragDropOperation* DragOperation) override;
    virtual void NotifyDragLeave_Implementation() override;
    virtual int32 GetDropTargetSlot_Implementation() const override { return CurrentSlotData.SlotIndex; }

    //========================================
    // ISuspenseCoreTooltipSourceInterface
    //========================================

    virtual FItemUIData GetTooltipData_Implementation() const override;
    virtual bool CanShowTooltip_Implementation() const override;
    virtual float GetTooltipDelay_Implementation() const override;
    virtual void OnTooltipShown_Implementation() override;
    virtual void OnTooltipHidden_Implementation() override;

    //========================================
    // Public API
    //========================================

    /** Set the owning container widget */
    UFUNCTION(BlueprintCallable, Category = "Slot")
    void SetOwningContainer(USuspenseBaseContainerWidget* Container);

    /** Get the owning container widget */
    UFUNCTION(BlueprintPure, Category = "Slot")
    USuspenseBaseContainerWidget* GetOwningContainer() const { return OwningContainer; }

    /** Set snap target feedback */
    UFUNCTION(BlueprintCallable, Category = "Slot")
    void SetSnapTarget(bool bIsTarget, float SnapStrength = 0.0f);

protected:
    //========================================
    // Visual Updates
    //========================================

    /** Update visual state based on current state */
    virtual void UpdateVisualState();

    /** Update item icon display */
    virtual void UpdateItemIcon();

    /** Update quantity text display */
    virtual void UpdateQuantityText();

    /** Update background visual */
    virtual void UpdateBackgroundVisual();

    /** Update highlight visual */
    virtual void UpdateHighlightVisual();

    /** Update selection visual */
    virtual void UpdateSelectionVisual();

    /** Get background color based on state */
    virtual FLinearColor GetBackgroundColor() const;

    //========================================
    // Interaction Handlers
    //========================================

    /** Handle click on slot */
    virtual void HandleClick();

    /** Handle double click on slot */
    virtual void HandleDoubleClick();

    /** Handle right click on slot */
    virtual void HandleRightClick();

    //========================================
    // Tooltip Handling
    //========================================

    /** Start tooltip show timer */
    void StartTooltipTimer();

    /** Cancel tooltip show timer */
    void CancelTooltipTimer();

    /** Show tooltip (called by timer) */
    void ShowTooltipDelayed();

    /** Hide tooltip immediately */
    void HideTooltip();

    /** Update tooltip position if active */
    void UpdateTooltipPosition();

    //========================================
    // Utility
    //========================================

    /** Get drag drop handler */
    USuspenseDragDropHandler* GetDragDropHandler() const;

    /** Get event manager */
    USuspenseCoreEventManager* GetEventManager() const;

    /** Get event bus */
    class USuspenseCoreEventBus* GetEventBus() const;

    /** Validate widget bindings */
    bool ValidateWidgetBindings() const;

    /** Current highlight color */
    FLinearColor CurrentHighlightColor;

    /** Cached colors for performance */
    FLinearColor CachedBackgroundColor;
    FLinearColor CachedHighlightColor;

private:
    //========================================
    // Performance Optimizations
    //========================================

    /** Whether slot is currently in widget pool */
    bool bIsPooled = false;

    /** Cached geometry for performance */
    mutable FGeometry CachedGeometry;
    mutable bool bGeometryCached = false;
    mutable float GeometryCacheTime = 0.0f;

    /** Pending visual updates */
    bool bNeedsVisualUpdate = false;
    float LastVisualUpdateTime = 0.0f;

    /** Icon loading handle */
    TSharedPtr<FStreamableHandle> IconStreamingHandle;

    /** Cached icon texture */
    UPROPERTY(Transient)
    UTexture2D* CachedIconTexture = nullptr;

    /** Tooltip show timer */
    FTimerHandle TooltipShowTimer;

    /** Visual update throttling */
    static constexpr float VISUAL_UPDATE_THROTTLE = 0.016f; // ~60 FPS
    static constexpr float GEOMETRY_CACHE_LIFETIME = 1.0f; // seconds

    /** Schedule visual update */
    void ScheduleVisualUpdate();

    /** Process pending visual updates */
    void ProcessPendingVisualUpdates();

    /** Load icon texture asynchronously */
    void LoadIconAsync(const FString& IconPath);

    /** Icon loaded callback */
    void OnIconLoaded();

    /** Update cached geometry */
    void UpdateCachedGeometry(const FGeometry& NewGeometry);

    /** Get cached or current geometry */
    const FGeometry& GetCachedOrCurrentGeometry() const;
};
