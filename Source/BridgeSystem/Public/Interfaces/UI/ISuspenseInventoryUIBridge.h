// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Operations/SuspenseInventoryResult.h"
#include "ISuspenseInventoryUIBridge.generated.h"

// Forward declarations
struct FDragDropUIData;

UINTERFACE(MinimalAPI, BlueprintType)
class USuspenseInventoryUIBridge : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface bridge between inventory system and UI widget system
 * Supports both direct inventory opening and opening through Character Screen
 * 
 * This interface provides a unified way to communicate between game inventory logic
 * and UI representation, handling all inventory-related UI operations
 */
class BRIDGESYSTEM_API ISuspenseInventoryUIBridge
{
    GENERATED_BODY()

public:
    // =====================================================
    // Legacy Methods (kept for backward compatibility)
    // =====================================================
    
    /**
     * Show inventory UI (deprecated, use ShowCharacterScreenWithTab)
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Bridge")
    void ShowInventoryUI();

    /**
     * Hide inventory UI (deprecated, use HideCharacterScreen)
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Bridge")
    void HideInventoryUI();

    /**
     * Toggle inventory UI visibility
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Bridge")
    void ToggleInventoryUI();

    /**
     * Check if inventory UI is visible
     * @return True if visible
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Bridge")
    bool IsInventoryUIVisible() const;

    // =====================================================
    // Character Screen Methods (preferred approach)
    // =====================================================
    
    /**
     * Show character screen with specific tab
     * @param TabTag Tag of tab to open
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Bridge")
    void ShowCharacterScreenWithTab(const FGameplayTag& TabTag);

    /**
     * Hide character screen
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Bridge")
    void HideCharacterScreen();

    /**
     * Toggle character screen visibility
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Bridge")
    void ToggleCharacterScreen();

    /**
     * Check if character screen is visible
     * @return True if visible
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Bridge")
    bool IsCharacterScreenVisible() const;

    // =====================================================
    // UI Update Methods
    // =====================================================
    
    /**
     * Request UI refresh with current inventory state
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Bridge")
    void RefreshInventoryUI();

    /**
     * Notify about inventory data change
     * @param ChangeType Type of change that occurred
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Bridge")
    void OnInventoryDataChanged(const FGameplayTag& ChangeType);

    // =====================================================
    // Inventory Info Methods
    // =====================================================
    
    /**
     * Get inventory grid size from PlayerState
     * @param OutColumns Number of columns in grid
     * @param OutRows Number of rows in grid
     * @return True if data retrieved successfully
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Bridge")
    bool GetInventoryGridSize(int32& OutColumns, int32& OutRows) const;

    /**
     * Get total inventory slot count
     * @return Number of slots
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Bridge")
    int32 GetInventorySlotCount() const;

    /**
     * Check if inventory is connected
     * @return True if inventory is connected
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Bridge")
    bool IsInventoryConnected() const;

    // =====================================================
    // Item Operation Methods
    // =====================================================
    
    /**
     * Add item to inventory
     * @param ItemInstance Item instance to add
     * @return Operation result
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Bridge|Operations")
    FSuspenseInventoryOperationResult AddItemToInventory(const FSuspenseInventoryItemInstance& ItemInstance);

    /**
     * Remove item from inventory by InstanceID
     * @param ItemInstanceID Item instance ID
     * @return Operation result
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Bridge|Operations")
    FSuspenseInventoryOperationResult RemoveItemFromInventory(const FGuid& ItemInstanceID);

    /**
     * Check if inventory can accept item
     * @param ItemInstance Item instance to check
     * @return True if inventory can accept item
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Bridge|Operations")
    bool CanAddItemToInventory(const FSuspenseInventoryItemInstance& ItemInstance) const;

    /**
     * Get all item instances in inventory
     * @return Array of all item instances
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Bridge|Operations")
    TArray<FSuspenseInventoryItemInstance> GetAllItemInstances() const;

    // =====================================================
    // Drag & Drop Support Methods
    // =====================================================
    
    /**
     * Process drop operation on inventory
     * @param DragData Data about dragged item
     * @param ScreenPosition Screen position of drop
     * @param TargetWidget Target widget (optional)
     * @return Operation result
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Bridge|DragDrop")
    FSuspenseInventoryOperationResult ProcessInventoryDrop(
        const FDragDropUIData& DragData,
        const FVector2D& ScreenPosition,
        UWidget* TargetWidget);

    /**
     * Calculate drop target slot
     * @param ScreenPosition Screen position
     * @param DragOffset Drag offset
     * @param ItemSize Item size
     * @param bIsRotated Item rotation state
     * @return Target slot index or INDEX_NONE
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Bridge|DragDrop")
    int32 CalculateDropTargetSlot(
        const FVector2D& ScreenPosition,
        const FVector2D& DragOffset,
        const FIntPoint& ItemSize,
        bool bIsRotated) const;

    // =====================================================
    // Static Methods
    // =====================================================
    
    /**
     * Get UI bridge implementation from world context
     * @param WorldContext Context for finding bridge
     * @return Bridge implementation or nullptr
     */
    static ISuspenseInventoryUIBridge* GetInventoryUIBridge(const UObject* WorldContext);
    
    /**
     * Get global bridge instance as TScriptInterface
     * This is the preferred method for getting bridge in gameplay code
     * @param WorldContext Optional world context for fallback search
     * @return Global bridge wrapped in TScriptInterface
     */
    static TScriptInterface<ISuspenseInventoryUIBridge> GetGlobalBridge(const UObject* WorldContext = nullptr);
 
    /**
     * Set global bridge instance
     * @param Bridge Bridge instance to set
     */
    static void SetGlobalBridge(ISuspenseInventoryUIBridge* Bridge);
    
    /**
     * Clear global bridge instance
     */
    static void ClearGlobalBridge();
    
    /**
     * Helper to create TScriptInterface from raw interface pointer
     * @param RawInterface Raw interface pointer
     * @return TScriptInterface wrapper
     */
    static TScriptInterface<ISuspenseInventoryUIBridge> MakeScriptInterface(ISuspenseInventoryUIBridge* RawInterface);
};