// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/UI/SuspenseCoreContainerUITypes.h"
#include "SuspenseCore/Types/Equipment/SuspenseCoreEquipmentTypes.h"
#include "ISuspenseCoreContainerUI.generated.h"

// Forward declarations
class USuspenseCoreEventManager;
class UDragDropOperation;

UINTERFACE(MinimalAPI, BlueprintType)
class USuspenseContainerUI : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for UI containers (inventory, equipment, storage, etc.)
 * Provides unified way to display and interact with item containers
 * 
 * ОБНОВЛЕНО: Используем базовый UDragDropOperation для избежания циклических зависимостей
 */
class BRIDGESYSTEM_API ISuspenseContainerUI
{
    GENERATED_BODY()

public:
    /**
     * Initialize container with UI data
     * @param ContainerData Data for displaying the container
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Container")
    void InitializeContainer(const FContainerUIData& ContainerData);

    /**
     * Update container display with new data
     * @param ContainerData Updated container data
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Container")
    void UpdateContainer(const FContainerUIData& ContainerData);

    /**
     * Get the container type this UI represents
     * @return Container type tag
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Container")
    FGameplayTag GetContainerType() const;

    /**
     * Request container data refresh from the system
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Container")
    void RequestDataRefresh();

    /**
     * Handle item clicked in container
     * @param SlotIndex Index of clicked slot
     * @param ItemInstanceID ID of clicked item (if any)
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Container")
    void OnSlotClicked(int32 SlotIndex, const FGuid& ItemInstanceID);

    /**
     * Handle item double-clicked
     * @param SlotIndex Index of clicked slot
     * @param ItemInstanceID ID of clicked item
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Container")
    void OnSlotDoubleClicked(int32 SlotIndex, const FGuid& ItemInstanceID);

    /**
     * Handle right-click on slot
     * @param SlotIndex Index of clicked slot
     * @param ItemInstanceID ID of clicked item (if any)
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Container")
    void OnSlotRightClicked(int32 SlotIndex, const FGuid& ItemInstanceID);

    /**
     * Check if container can accept dragged item
     * @param DragOperation Current drag operation (базовый класс)
     * @param TargetSlotIndex Target slot index
     * @return Validation result
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Container")
    FSlotValidationResult CanAcceptDrop(const UDragDropOperation* DragOperation, int32 TargetSlotIndex) const;

    /**
     * Handle item dropped into container
     * @param DragOperation Drag operation with item data (базовый класс)
     * @param TargetSlotIndex Target slot index
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "UI|Container")
    void HandleItemDropped(UDragDropOperation* DragOperation, int32 TargetSlotIndex);

    /**
     * Get delegate manager for event communication
     * @return Delegate manager instance
     */
    virtual USuspenseCoreEventManager* GetDelegateManager() const = 0;

    /**
     * Static helper to get delegate manager from world context
     * @param WorldContextObject Object with world context
     * @return Delegate manager or nullptr
     */
    static USuspenseCoreEventManager* GetDelegateManagerStatic(const UObject* WorldContextObject);

    /**
     * Static helper to broadcast container update request
     * @param Container Container requesting update
     * @param ContainerType Type of container
     */
    static void BroadcastContainerUpdateRequest(
        const UObject* Container,
        const FGameplayTag& ContainerType);

    /**
     * Static helper to broadcast slot interaction
     * @param Container Container where interaction occurred
     * @param SlotIndex Slot that was interacted with
     * @param InteractionType Type of interaction
     */
    static void BroadcastSlotInteraction(
        const UObject* Container,
        int32 SlotIndex,
        const FGameplayTag& InteractionType);
};

