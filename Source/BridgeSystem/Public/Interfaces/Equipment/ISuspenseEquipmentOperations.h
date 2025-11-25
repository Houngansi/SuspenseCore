// ISuspenseEquipmentOperations.h
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Types/Equipment/SuspenseEquipmentTypes.h"
#include "Delegates/SuspenseEventManager.h"
#include "GameplayTagContainer.h"
#include "ISuspenseEquipmentOperations.generated.h"

// Убираем дублирующие структуры - они теперь в SuspenseEquipmentTypes.h

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseEquipmentOperations : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for equipment operations execution
 * 
 * Philosophy: Pure business logic for equipment operations.
 * No knowledge of UI, networking, or persistence.
 */
class BRIDGESYSTEM_API ISuspenseEquipmentOperations
{
    GENERATED_BODY()

public:
    /**
     * Execute equipment operation
     * @param Request Operation request
     * @return Operation result
     */
    virtual FEquipmentOperationResult ExecuteOperation(const FEquipmentOperationRequest& Request) = 0;
    
    /**
     * Validate operation before execution
     * @param Request Operation to validate
     * @return Validation result
     */
    virtual FSlotValidationResult ValidateOperation(const FEquipmentOperationRequest& Request) const = 0;
    
    // Остальные методы остаются без изменений...
    virtual FEquipmentOperationResult EquipItem(
        const FSuspenseInventoryItemInstance& ItemInstance,
        int32 SlotIndex) = 0;
    
    virtual FEquipmentOperationResult UnequipItem(int32 SlotIndex) = 0;
    virtual FEquipmentOperationResult SwapItems(int32 SlotIndexA, int32 SlotIndexB) = 0;
    virtual FEquipmentOperationResult MoveItem(int32 SourceSlot, int32 TargetSlot) = 0;
    virtual FEquipmentOperationResult DropItem(int32 SlotIndex) = 0;
    virtual FEquipmentOperationResult QuickSwitchWeapon() = 0;
    virtual TArray<FEquipmentOperationResult> GetOperationHistory(int32 MaxCount = 10) const = 0;
    virtual bool CanUndoLastOperation() const = 0;
    virtual FEquipmentOperationResult UndoLastOperation() = 0;
};