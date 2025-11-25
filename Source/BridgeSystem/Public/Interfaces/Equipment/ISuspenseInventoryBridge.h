// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Operations/SuspenseInventoryResult.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "ISuspenseInventoryBridge.generated.h"

/**
 * Inventory transfer request
 */
USTRUCT(BlueprintType)
struct FInventoryTransferRequest
{
    GENERATED_BODY()
    
    UPROPERTY()
    FSuspenseInventoryItemInstance Item;
    
    UPROPERTY()
    int32 SourceSlot = INDEX_NONE;
    
    UPROPERTY()
    int32 TargetSlot = INDEX_NONE;
    
    UPROPERTY()
    bool bFromInventory = true;
    
    UPROPERTY()
    bool bToInventory = false;
};

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseInventoryBridge : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for inventory system integration
 * 
 * Philosophy: Bridge between equipment and inventory systems.
 * Handles item transfers and state synchronization.
 */
class BRIDGESYSTEM_API ISuspenseInventoryBridge
{
    GENERATED_BODY()

public:
    /**
     * Transfer item from inventory to equipment
     * @param Request Transfer request
     * @return Operation result
     */
    virtual FSuspenseInventoryOperationResult TransferFromInventory(const FInventoryTransferRequest& Request) = 0;
    
    /**
     * Transfer item from equipment to inventory
     * @param Request Transfer request
     * @return Operation result
     */
    virtual FSuspenseInventoryOperationResult TransferToInventory(const FInventoryTransferRequest& Request) = 0;
    
    /**
     * Check if inventory has space
     * @param Item Item to check
     * @return True if has space
     */
    virtual bool InventoryHasSpace(const FSuspenseInventoryItemInstance& Item) const = 0;
    
    /**
     * Get inventory interface
     * @return Inventory interface or nullptr
     */
    virtual TScriptInterface<class ISuspenseInventory> GetInventoryInterface() const = 0;
    
    /**
     * Synchronize with inventory state
     */
    virtual void SynchronizeWithInventory() = 0;
    
    /**
     * Find item in inventory
     * @param ItemId Item to find
     * @param OutInstance Found instance
     * @return True if found
     */
    virtual bool FindItemInInventory(const FName& ItemId, FSuspenseInventoryItemInstance& OutInstance) const = 0;
    
    /**
     * Reserve inventory space
     * @param Item Item to reserve for
     * @return Reservation ID or invalid
     */
    virtual FGuid ReserveInventorySpace(const FSuspenseInventoryItemInstance& Item) = 0;
    
    /**
     * Release reservation
     * @param ReservationId Reservation to release
     * @return True if released
     */
    virtual bool ReleaseReservation(const FGuid& ReservationId) = 0;
};