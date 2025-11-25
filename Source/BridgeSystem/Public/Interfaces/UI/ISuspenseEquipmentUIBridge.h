// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Types/UI/SuspenseEquipmentUITypes.h"
#include "ISuspenseEquipmentUIBridge.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class USuspenseEquipmentUIBridgeWidget : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for equipment UI bridge widget
 * Provides unified communication between equipment system and UI
 * 
 * UPDATED VERSION 3.0:
 * - Full FSuspenseInventoryItemInstance integration
 * - Uses FEquipmentSlotUIData for all slot operations
 * - Removed all legacy dependencies
 * - Unified DataTable architecture support
 */
class BRIDGESYSTEM_API ISuspenseEquipmentUIBridge
{
    GENERATED_BODY()

public:
    /**
     * Show equipment UI
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Equipment")
    void ShowEquipmentUI();

    /**
     * Hide equipment UI
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Equipment")
    void HideEquipmentUI();

    /**
     * Toggle equipment UI visibility
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Equipment")
    void ToggleEquipmentUI();

    /**
     * Check if equipment UI is visible
     * @return True if visible
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Equipment")
    bool IsEquipmentUIVisible() const;

    /**
     * Refresh equipment UI with current data
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Equipment")
    void RefreshEquipmentUI();

    /**
     * Notify about equipment data changes
     * @param ChangeType Type of change that occurred
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Equipment")
    void OnEquipmentDataChanged(const FGameplayTag& ChangeType);

    /**
     * Check if equipment is connected
     * @return True if connected
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Equipment")
    bool IsEquipmentConnected() const;

    /**
     * Get equipment slots UI data using new architecture
     * @param OutSlots Array of equipment slot UI data
     * @return True if successful
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Equipment")
    bool GetEquipmentSlotsUIData(TArray<FEquipmentSlotUIData>& OutSlots) const;

    /**
     * Process item drop on equipment slot
     * @param SlotIndex Target slot index
     * @param DragData Drag data containing item info
     * @return True if drop was handled
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Equipment")
    bool ProcessEquipmentDrop(int32 SlotIndex, const FDragDropUIData& DragData);

    /**
     * Process unequip request
     * @param SlotIndex Slot to unequip from
     * @param TargetInventorySlot Target inventory slot (-1 for auto)
     * @return True if unequip was successful
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Equipment")
    bool ProcessUnequipRequest(int32 SlotIndex, int32 TargetInventorySlot = -1);

    /**
     * Set the equipment interface to connect with
     * @param InEquipment Equipment interface to use
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Equipment|UI")
    void SetEquipmentInterface(const TScriptInterface<ISuspenseEquipment>& InEquipment);

    /**
     * Get connected equipment interface
     * @return Equipment interface or invalid script interface
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Equipment|UI")
    TScriptInterface<ISuspenseEquipment> GetEquipmentInterface() const;

    /**
     * Get item manager for data access
     * @return Item manager or nullptr
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Equipment|UI")
    class USuspenseItemManager* GetItemManager() const;

    /**
     * Get global equipment UI bridge instance
     * @param WorldContext Any object with valid world context
     * @return Bridge instance or nullptr
     */
    static ISuspenseEquipmentUIBridge* GetEquipmentUIBridge(const UObject* WorldContext);

    /**
     * Set global bridge instance
     * @param Bridge Bridge to set
     */
    static void SetGlobalEquipmentBridge(ISuspenseEquipmentUIBridge* Bridge);

    /**
     * Clear global bridge instance
     */
    static void ClearGlobalEquipmentBridge();
};