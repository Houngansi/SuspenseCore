// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Interfaces/UI/ISuspenseInventoryUIBridge.h"
#include "SuspenseInventoryUIConnector.generated.h"

// Forward declarations
class USuspenseInventoryComponent;
class ISuspenseInventoryUIBridgeInterface;
class UUserWidget;
class USuspenseItemManager;
class USuspenseEventManager;
struct FSuspenseUnifiedItemData;
struct FSuspenseInventoryItemInstance;

/**
 * UI representation of inventory cell
 * Fully integrated with DataTable system
 */
USTRUCT(BlueprintType)
struct FInventoryCellUI
{
    GENERATED_BODY()
    
    /** Cell index in grid */
    UPROPERTY(BlueprintReadWrite, Category = "UI")
    int32 Index = INDEX_NONE;
    
    /** Item ID from DataTable */
    UPROPERTY(BlueprintReadWrite, Category = "UI")
    FName ItemID = NAME_None;
    
    /** Localized display name */
    UPROPERTY(BlueprintReadWrite, Category = "UI")
    FText ItemName;
    
    /** Current quantity in stack */
    UPROPERTY(BlueprintReadWrite, Category = "UI")
    int32 Quantity = 0;
    
    /** Total weight of stack */
    UPROPERTY(BlueprintReadWrite, Category = "UI")
    float Weight = 0.0f;
    
    /** Item size in grid cells */
    UPROPERTY(BlueprintReadWrite, Category = "UI")
    FVector2D GridSize = FVector2D::ZeroVector;
    
    /** Position in grid (X,Y) */
    UPROPERTY(BlueprintReadWrite, Category = "UI")
    FVector2D Position = FVector2D::ZeroVector;
    
    /** Anchor cell index */
    UPROPERTY(BlueprintReadWrite, Category = "UI")
    int32 AnchorIndex = INDEX_NONE;
    
    /** Is item rotated 90 degrees */
    UPROPERTY(BlueprintReadWrite, Category = "UI")
    bool bIsRotated = false;
    
    /** Reference to actual item object */
    UPROPERTY(BlueprintReadWrite, Category = "UI")
    UObject* ItemObject = nullptr;
    
    /** Cached icon texture */
    UPROPERTY(BlueprintReadWrite, Category = "UI")
    UTexture2D* ItemIcon = nullptr;
    
    /** Rarity color for UI */
    UPROPERTY(BlueprintReadWrite, Category = "UI")
    FLinearColor RarityColor = FLinearColor::White;
    
    /** Durability percentage (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "UI")
    float DurabilityPercent = 1.0f;
    
    /** Is this a stackable item */
    UPROPERTY(BlueprintReadWrite, Category = "UI")
    bool bIsStackable = false;
    
    /** Maximum stack size */
    UPROPERTY(BlueprintReadWrite, Category = "UI")
    int32 MaxStackSize = 1;
    
    /** Instance ID for tracking */
    UPROPERTY(BlueprintReadWrite, Category = "UI")
    FGuid InstanceID;
    
    FInventoryCellUI() = default;
};

/**
 * Component connecting inventory system to UI
 * Fully integrated with DataTable and EventDelegateManager
 * 
 * ARCHITECTURAL PRINCIPLES:
 * - All item data comes from DataTable via ItemManager
 * - Uses EventDelegateManager for all notifications
 * - Works with FSuspenseInventoryItemInstance for runtime data
 * - Supports inventory operations through FSuspenseInventoryOperation system
 */
UCLASS(ClassGroup=(MedCom), meta=(BlueprintSpawnableComponent))
class INVENTORYSYSTEM_API USuspenseInventoryUIConnector : public UActorComponent
{
    GENERATED_BODY()

public:
    USuspenseInventoryUIConnector();

    //~ Begin UActorComponent Interface
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    //~ End UActorComponent Interface

    //==================================================================
    // Core Setup
    //==================================================================
    
    /**
     * Set inventory component to connect
     * @param InInventoryComponent Component to connect
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
    void SetInventoryComponent(USuspenseInventoryComponent* InInventoryComponent);
    
    /**
     * Set UI bridge interface
     * @param InBridge Bridge interface
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
    void SetUIBridge(TScriptInterface<ISuspenseInventoryUIBridgeInterface> InBridge);

    /**
     * Get current UI bridge
     * @return Bridge interface
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
    TScriptInterface<ISuspenseInventoryUIBridgeInterface> GetUIBridge() const { return UIBridge; }

    //==================================================================
    // UI Display Data
    //==================================================================
    
    /**
     * Get all cells for UI grid display
     * @return Array of UI cell data with full DataTable information
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI|Display")
    TArray<FInventoryCellUI> GetAllCellsForUI();
    
    /**
     * Get specific cell data
     * @param CellIndex Grid cell index
     * @return Cell UI data
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI|Display")
    FInventoryCellUI GetCellData(int32 CellIndex) const;
    
    /**
     * Get inventory grid size
     * @return Grid dimensions (width x height)
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI|Display")
    FVector2D GetInventoryGridSize() const;
    
    /**
     * Get current weight info
     * @param OutCurrentWeight Current total weight
     * @param OutMaxWeight Maximum weight capacity
     * @param OutPercentUsed Weight percentage used (0-1)
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI|Display")
    void GetWeightInfo(float& OutCurrentWeight, float& OutMaxWeight, float& OutPercentUsed) const;

    //==================================================================
    // UI Actions
    //==================================================================
    
    /**
     * Show inventory UI
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI|Actions")
    void ShowInventory();
    
    /**
     * Hide inventory UI
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI|Actions")
    void HideInventory();
    
    /**
     * Toggle inventory visibility
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI|Actions")
    void ToggleInventory();
    
    /**
     * Force UI refresh
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI|Actions")
    void RefreshUI();

    //==================================================================
    // Drag & Drop Operations
    //==================================================================
    
    /**
     * Start drag operation
     * @param ItemObject Item being dragged
     * @param FromCellIndex Source cell index
     * @return true if drag is allowed
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI|DragDrop")
    bool StartDragOperation(UObject* ItemObject, int32 FromCellIndex);
    
    /**
     * Preview drop at location
     * @param ItemObject Item being dragged
     * @param TargetCellIndex Target cell
     * @param bWantRotate Whether to preview with rotation
     * @return true if drop would be valid
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI|DragDrop")
    bool PreviewDrop(UObject* ItemObject, int32 TargetCellIndex, bool bWantRotate);
    
    /**
     * Complete drop operation
     * @param ItemObject Item being dropped
     * @param TargetCellIndex Target cell
     * @param bWantRotate Whether to rotate item
     * @return true if drop successful
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI|DragDrop")
    bool CompleteDrop(UObject* ItemObject, int32 TargetCellIndex, bool bWantRotate);
    
    /**
     * Cancel drag operation
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI|DragDrop")
    void CancelDrag();

    //==================================================================
    // Stack Operations
    //==================================================================
    
    /**
     * Try to stack items
     * @param SourceItem Source item
     * @param TargetItem Target item
     * @param Amount Amount to stack (-1 for all)
     * @return true if stack successful
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI|Stack")
    bool TryStackItems(UObject* SourceItem, UObject* TargetItem, int32 Amount = -1);
    
    /**
     * Split item stack
     * @param SourceItem Item to split
     * @param SplitAmount Amount to split off
     * @param TargetCellIndex Where to place split stack
     * @return true if split successful
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI|Stack")
    bool SplitItemStack(UObject* SourceItem, int32 SplitAmount, int32 TargetCellIndex);
    
    /**
     * Check if items can stack
     * @param Item1 First item
     * @param Item2 Second item
     * @return true if items are stackable
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI|Stack")
    bool CanItemsStack(UObject* Item1, UObject* Item2) const;

    //==================================================================
    // Item Information from DataTable
    //==================================================================
    
    /**
     * Get item icon texture
     * @param ItemID Item identifier
     * @return Icon texture or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI|ItemInfo")
    UTexture2D* GetItemIcon(const FName& ItemID) const;
    
    /**
     * Get complete item display info
     * @param ItemID Item identifier
     * @param OutDisplayName Localized name
     * @param OutDescription Localized description
     * @param OutRarityColor Rarity color
     * @return true if found
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI|ItemInfo")
    bool GetItemDisplayInfo(const FName& ItemID, FText& OutDisplayName, 
        FText& OutDescription, FLinearColor& OutRarityColor) const;
    
    /**
     * Get item tooltip data
     * @param ItemObject Item to get tooltip for
     * @param OutTooltipText Formatted tooltip text
     * @param OutRarityColor Rarity color
     * @return true if tooltip generated
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI|ItemInfo")
    bool GetItemTooltip(UObject* ItemObject, FText& OutTooltipText, 
        FLinearColor& OutRarityColor) const;

    //==================================================================
    // Utility Functions
    //==================================================================
    
    /**
     * Format weight for display
     * @param Weight Weight value
     * @return Formatted text (e.g. "15.5 kg")
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI|Utility")
    FText FormatWeight(float Weight) const;
    
    /**
     * Format stack quantity
     * @param Current Current amount
     * @param Max Maximum amount
     * @return Formatted text (e.g. "25/50")
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI|Utility")
    FText FormatStackQuantity(int32 Current, int32 Max) const;
    
    /**
     * Get cells occupied by item
     * @param ItemObject Item to check
     * @return Array of occupied cell indices
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI|Utility")
    TArray<int32> GetItemOccupiedCells(UObject* ItemObject) const;

protected:
    /** Connected inventory component */
    UPROPERTY()
    USuspenseInventoryComponent* InventoryComponent;
    
    /** UI bridge interface */
    UPROPERTY()
    TScriptInterface<ISuspenseInventoryUIBridgeInterface> UIBridge;

    /** Cached item manager */
    mutable TWeakObjectPtr<USuspenseItemManager> CachedItemManager;

    /** Cached event delegate manager */
    mutable TWeakObjectPtr<USuspenseEventManager> CachedDelegateManager;
    
    /** Current drag operation data */
    struct FDragOperationData
    {
        TWeakObjectPtr<UObject> DraggedItem;
        int32 OriginalCellIndex = INDEX_NONE;
        bool bIsActive = false;
        
        void Reset()
        {
            DraggedItem.Reset();
            OriginalCellIndex = INDEX_NONE;
            bIsActive = false;
        }
    } CurrentDragOperation;

    //==================================================================
    // Internal Helpers
    //==================================================================
    
    /** Get item manager */
    USuspenseItemManager* GetItemManager() const;

    /** Get event delegate manager */
    USuspenseEventManager* GetDelegateManager() const;

    /** Convert item instance to UI cell data */
    FInventoryCellUI ConvertItemToUICell(const FSuspenseInventoryItemInstance& Instance,
        UObject* ItemObject, int32 CellIndex) const;

    /** Build tooltip text for item */
    FText BuildItemTooltip(const FSuspenseInventoryItemInstance& Instance,
        const FSuspenseUnifiedItemData& ItemData) const;
    
    /** Subscribe to inventory events */
    void SubscribeToEvents();
    
    /** Unsubscribe from inventory events */
    void UnsubscribeFromEvents();
    
    /** Handle inventory update notification */
    UFUNCTION()
    void OnInventoryUpdated();
    
    /** Handle item added notification */
    void OnItemAdded(const FSuspenseInventoryItemInstance& Instance, int32 SlotIndex);
    
    /** Handle item removed notification */
    void OnItemRemoved(const FName& ItemID, int32 Quantity, int32 SlotIndex);
    
    /** Handle item moved notification */
    void OnItemMoved(UObject* Item, int32 OldSlot, int32 NewSlot, bool bRotated);

private:
    /** Icon texture cache */
    mutable TMap<FName, TWeakObjectPtr<UTexture2D>> IconCache;
    
    /** Delegate handles for event system */
    FDelegateHandle InventoryUpdateHandle;
    FDelegateHandle ItemAddedHandle;
    FDelegateHandle ItemRemovedHandle;
    FDelegateHandle ItemMovedHandle;
};