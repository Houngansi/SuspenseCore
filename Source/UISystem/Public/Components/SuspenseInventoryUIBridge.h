// Copyright Suspense Team Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Interfaces/UI/ISuspenseInventoryUIBridge.h"
#include "Interfaces/Inventory/ISuspenseInventoryInterface.h"
#include "Types/UI/ContainerUITypes.h"
#include "Types/Inventory/InventoryTypes.h"
#include "GameplayTagContainer.h"
#include "SuspenseInventoryUIBridge.generated.h"

// Forward declarations
class USuspenseUIManager;
class USuspenseInventoryWidget;
class UEventDelegateManager;
class APlayerController;
struct FSuspenseUnifiedItemData;
struct FSuspenseInventoryItemInstance;

/**
 * UI inventory bridge implementation
 * Handles all UI-specific operations for inventory
 * 
 * Architecture:
 * - Bridge pattern implementation for UI-game communication
 * - Manages data transformation between game and UI formats
 * - Handles all inventory UI events and updates
 * - Works with new DataTable architecture through interfaces
 * 
 * TODO: Future refactoring targets:
 * - Extract data conversion logic to InventoryDataConverter class
 * - Move drag&drop handling to dedicated DragDropManager
 * - Separate Character Screen management into its own component
 * - Create EventSubscriptionManager for centralized event handling
 */
UCLASS()
class UISYSTEM_API USuspenseInventoryUIBridge : public UObject, public ISuspenseInventoryUIBridgeInterface
{
    GENERATED_BODY()

public:
    USuspenseInventoryUIBridge();

    // =====================================================
    // Core Initialization & Lifecycle
    // =====================================================
    
    /**
     * Initialize bridge with player controller
     * Sets up all necessary connections and references
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Bridge")
    bool Initialize(APlayerController* InPlayerController);

    /**
     * Set the game inventory interface to display
     * This connects the bridge to the actual inventory data
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Bridge")
    void SetInventoryInterface(TScriptInterface<ISuspenseInventoryInterface> InInventory);

    /**
     * Clean shutdown of the bridge
     * Unsubscribes from events and clears references
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Bridge")
    void Shutdown();

    /**
     * Set custom inventory widget class
     * Allows Blueprint override of the widget class
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Bridge")
    void SetInventoryWidgetClass(TSubclassOf<USuspenseInventoryWidget> WidgetClass);

    // =====================================================
    // External Bridge Operations
    // =====================================================
    
    /**
     * Remove item from inventory slot (used by equipment bridge)
     * @param SlotIndex Slot to remove from
     * @param OutRemovedInstance Removed item instance data
     * @return True if successful
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Bridge")
    bool RemoveItemFromInventorySlot(int32 SlotIndex, FSuspenseInventoryItemInstance& OutRemovedInstance);

    /**
     * Restore item instance to inventory
     * @param ItemInstance Item instance to restore
     * @return true if item successfully restored
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Bridge")
    bool RestoreItemToInventory(const FSuspenseInventoryItemInstance& ItemInstance);

    // =====================================================
    // ISuspenseInventoryUIBridgeInterface Interface
    // =====================================================
    
    virtual void ShowInventoryUI_Implementation() override;
    virtual void HideInventoryUI_Implementation() override;
    virtual void ToggleInventoryUI_Implementation() override;
    virtual bool IsInventoryUIVisible_Implementation() const override;
    virtual void RefreshInventoryUI_Implementation() override;
    virtual void OnInventoryDataChanged_Implementation(const FGameplayTag& ChangeType) override;
    virtual bool IsInventoryConnected_Implementation() const override;
    virtual bool GetInventoryGridSize_Implementation(int32& OutColumns, int32& OutRows) const override;
    virtual int32 GetInventorySlotCount_Implementation() const override;
    
    virtual void ShowCharacterScreenWithTab_Implementation(const FGameplayTag& TabTag) override;
    virtual void HideCharacterScreen_Implementation() override;
    virtual void ToggleCharacterScreen_Implementation() override;
    virtual bool IsCharacterScreenVisible_Implementation() const override;

    // =====================================================
    // Static Registration
    // =====================================================
    
    /**
     * Static helper to register bridge instance globally
     */
    static void RegisterBridge(USuspenseInventoryUIBridge* Bridge);

    /**
     * Static helper to unregister bridge
     */
    static void UnregisterBridge();

    // =====================================================
    // Widget Discovery & Management
    // TODO: Move to WidgetManager in future refactoring
    // =====================================================
    
    /**
     * Find inventory widget within the character screen
     * Searches through the tab system to locate the inventory widget
     */
    USuspenseInventoryWidget* FindInventoryWidgetInCharacterScreen() const;

    /**
     * Initialize inventory widget with current game data
     */
    void InitializeInventoryWidgetWithData(USuspenseInventoryWidget* Widget);

    /**
     * Update inventory widget with latest data
     */
    void UpdateInventoryWidgetData(USuspenseInventoryWidget* Widget);

    // =====================================================
    // Drag & Drop Operations
    // TODO: Extract to DragDropManager class
    // =====================================================
    
    /**
     * Process complete drop operation
     * Centralized method for all drop scenarios
     */
    FInventoryOperationResult ProcessInventoryDrop(
        const FDragDropUIData& DragData,
        const FVector2D& ScreenPosition,
        UUserWidget* TargetWidget = nullptr);

    /**
     * Calculate target slot for drop operation
     */
    int32 CalculateDropTargetSlot(
        const FVector2D& ScreenPosition,
        const FVector2D& DragOffset,
        const FIntPoint& ItemSize,
        bool bIsRotated) const;
    
    /**
     * Validate drop placement
     */
    bool ValidateDropPlacement(
        int32 TargetSlot,
        const FIntPoint& ItemSize,
        bool bIsRotated,
        TArray<int32>& OutOccupiedSlots) const;
    
    /**
     * Find nearest valid slot for placement
     */
    int32 FindNearestValidSlot(
        int32 PreferredSlot,
        const FIntPoint& ItemSize,
        bool bIsRotated,
        int32 SearchRadius = 5) const;

protected:
    // =====================================================
    // Core References
    // =====================================================
    
    /** Player controller that owns this bridge */
    UPROPERTY()
    APlayerController* OwningPlayerController;

    /** Reference to UI manager */
    UPROPERTY()
    USuspenseUIManager* UIManager;

    /** Event delegate manager for UI events */
    UPROPERTY()
    UEventDelegateManager* EventManager;

    /** Game inventory interface we're displaying */
    TScriptInterface<ISuspenseInventoryInterface> GameInventory;

    /** Widget class to use for inventory display */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Bridge")
    TSubclassOf<USuspenseInventoryWidget> InventoryWidgetClass;

    // =====================================================
    // State Management
    // =====================================================
    
    /** Initialization state */
    bool bIsInitialized;

    /** Current drag operation data */
    FDragDropUIData CurrentDragData;

    /** Cached inventory widget reference with validation */
    mutable TWeakObjectPtr<USuspenseInventoryWidget> CachedInventoryWidget;
    
    /** Time when widget cache was last validated */
    mutable float LastWidgetCacheValidationTime;
    
    /** Cache lifetime in seconds */
    static constexpr float WIDGET_CACHE_LIFETIME = 2.0f;

    /** Pending UI updates for batching */
    TArray<FGameplayTag> PendingUIUpdates;
    
    /** Timer handle for batched updates */
    FTimerHandle BatchedUpdateTimerHandle;
    
    /** Update batching delay in seconds */
    static constexpr float UPDATE_BATCH_DELAY = 0.1f;

private:
    // =====================================================
    // Data Conversion
    // TODO: Extract to InventoryDataConverter class
    // =====================================================
    
    /**
     * Convert game inventory data to UI format
     */
    bool ConvertInventoryToUIData(FContainerUIData& OutContainerData) const;

    /**
     * Convert item instance to UI format
     */
    bool ConvertItemInstanceToUIData(
        const FSuspenseInventoryItemInstance& Instance, 
        int32 SlotIndex, 
        FItemUIData& OutItemData) const;

    // =====================================================
    // Event Management
    // TODO: Extract to EventSubscriptionManager
    // =====================================================
    
    /** Subscribe to all necessary events */
    void SubscribeToEvents();

    /** Unsubscribe from all events */
    void UnsubscribeFromEvents();

    /** Event handlers */
    UFUNCTION()
    void OnGameInventoryUpdated();

    UFUNCTION()
    void OnUIRequestingUpdate(UUserWidget* Widget, const FGameplayTag& ContainerType);

    UFUNCTION()
    void OnUISlotInteraction(UUserWidget* Widget, int32 SlotIndex, const FGameplayTag& InteractionType);

    UFUNCTION()
    void OnUIDragStarted(UUserWidget* SourceWidget, const FDragDropUIData& DragData);

    UFUNCTION()
    void OnUIDragCompleted(UUserWidget* SourceWidget, UUserWidget* TargetWidget, bool bSuccess);

    UFUNCTION()
    void OnUIItemDropped(UUserWidget* ContainerWidget, const FDragDropUIData& DragData, int32 TargetSlot);

    UFUNCTION()
    void OnInventoryItemMoved(const FGuid& ItemID, int32 FromSlot, int32 ToSlot, bool bSuccess);

    void OnInventoryUIClosed();

    UFUNCTION()
    void OnEquipmentOperationCompleted(const FEquipmentOperationResult& Result);
    // =====================================================
    // UI Update Management
    // =====================================================
    
    /** Process batched UI updates */
    void ProcessBatchedUIUpdates();
    
    /** Schedule UI update with batching */
    void ScheduleUIUpdate(const FGameplayTag& UpdateType);
    
    /** Refresh all widgets in active layout */
    void RefreshAllWidgetsInActiveLayout();
    
    /** Force immediate full inventory refresh */
    void ForceFullInventoryRefresh();

    // =====================================================
    // Drop Operation Handlers
    // TODO: Consolidate into unified drop handler
    // =====================================================
    
    /** Handle drop within inventory */
    void HandleInventoryToInventoryDrop(const FDragDropUIData& DragData, int32 TargetSlot);

    /** Handle drop from external container to inventory */
    void HandleExternalToInventoryDrop(
        UUserWidget* ContainerWidget, 
        const FDragDropUIData& DragData, 
        int32 TargetSlot);
    /**
      * Handles drop operation from inventory to equipment container.
      * Delegates the operation to EquipmentUIBridge for proper equipment slot validation and execution.
      * 
      * @param ContainerWidget - The equipment container widget where item is being dropped
      * @param DragData - Complete drag operation data including source item information
      * @param TargetSlot - Target equipment slot index
      */
    void HandleInventoryToEquipmentDrop(UUserWidget* ContainerWidget, const FDragDropUIData& DragData, int32 TargetSlot);
 /**
  * Handles invalid or unsupported drag-drop operations.
  * Used as a fallback when the source/target combination is not valid
  * or the drag data is corrupted. Only uses UI-level delegates, no
  * references to equipment or inventory modules.
  */
 UFUNCTION()
 void HandleInvalidDrop(UUserWidget* ContainerWidget, const FDragDropUIData& DragData, int32 TargetSlot);

 // =====================================================
    // Processing Methods
    // TODO: Move to dedicated processors
    // =====================================================
    
    /** Process item move request */
    void ProcessItemMoveRequest(const FGuid& ItemInstanceID, int32 TargetSlotIndex, bool bIsRotated);

    /** Process item rotation request */
    void ProcessItemRotationRequest(int32 SlotIndex);

    /** Process sort request */
    void ProcessSortRequest();

    /** Process item double click */
    void ProcessItemDoubleClick(int32 SlotIndex, const FGuid& ItemInstanceID);

    // =====================================================
    // Utility Methods
    // =====================================================
    
    /** Validate that inventory connection is valid */
    bool ValidateInventoryConnection() const;
    
    /** Get cached inventory widget with validation */
    USuspenseInventoryWidget* GetCachedInventoryWidget() const;
    
    /** Invalidate widget cache */
    void InvalidateWidgetCache();
    
    /** Get human-readable error string */
    FString GetErrorCodeString(EInventoryErrorCode ErrorCode) const;
    
    /** Get UI manager reference */
    class USuspenseUIManager* GetUIManager() const;

    // =====================================================
    // Grid Calculation Helpers
    // TODO: Extract to GridCalculationHelper
    // =====================================================
    
    /** Get inventory grid parameters */
    bool GetInventoryGridParams(
        int32& OutColumns,
        int32& OutRows,
        float& OutCellSize,
        FGeometry& OutGridGeometry) const;
    
    /** Convert screen coordinates to grid coordinates */
    FVector2D ScreenToGridCoordinates(
        const FVector2D& ScreenPos,
        const FGeometry& GridGeometry) const;
    
    /** Check if required slots are free */
    bool AreRequiredSlotsFree(
        int32 AnchorSlot,
        const FIntPoint& ItemSize,
        int32 GridColumns,
        const FGuid& ExcludeItemID = FGuid()) const;

    // =====================================================
    // Diagnostics
    // TODO: Extract to InventoryDiagnostics utility
    // =====================================================
    
    /** Diagnose drop position for debugging */
    void DiagnoseDropPosition(const FVector2D& ScreenPosition) const;
    
    /** Diagnose inventory state for debugging */
    void DiagnoseInventoryState() const;

    // =====================================================
    // Event Subscription Handles
    // =====================================================
    
    /** Native delegate handles */
    FDelegateHandle ItemDroppedNativeHandle;
    FDelegateHandle InventoryRefreshHandle;
    
};