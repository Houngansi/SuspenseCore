// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interfaces/Equipment/ISuspenseEquipment.h"
#include "ISuspenseEquipmentFacade.generated.h"

class ISuspenseEquipmentOrchestrator;

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseEquipmentFacade : public USuspenseEquipmentInterface
{
    GENERATED_BODY()
};

/**
 * Facade interface for equipment system
 * 
 * Philosophy: Simple, unified interface to complex system.
 * Maintains backward compatibility while hiding internal complexity.
 * This interface extends the existing ISuspenseEquipment
 * to ensure full compatibility with existing code.
 */
class BRIDGESYSTEM_API ISuspenseEquipmentFacade : public ISuspenseEquipment
{
    GENERATED_BODY()

public:
    /**
     * Initialize facade with orchestrator
     * @param Orchestrator System orchestrator
     * @return True if initialized
     */
    virtual bool InitializeFacade(TScriptInterface<ISuspenseEquipmentOrchestrator> Orchestrator) = 0;
    
    /**
     * Simple equip item method
     * @param ItemInstance Item to equip
     * @param PreferredSlot Preferred slot (-1 for auto)
     * @return True if equipped
     */
    virtual bool SimpleEquipItem(const FSuspenseInventoryItemInstance& ItemInstance, int32 PreferredSlot = -1) = 0;
    
    /**
     * Simple unequip item method
     * @param SlotIndex Slot to unequip
     * @return True if unequipped
     */
    virtual bool SimpleUnequipItem(int32 SlotIndex) = 0;
    
    /**
     * Quick weapon switch
     * @return True if switched
     */
    virtual bool QuickSwitch() = 0;
    
    /**
     * Get equipped items summary
     * @return Summary string for UI
     */
    virtual FString GetEquipmentSummary() const = 0;
    
    /**
     * Validate facade state
     * @return True if valid
     */
    virtual bool ValidateFacade() const = 0;
    
    /**
     * Get system health status
     * @return Health status report
     */
    virtual FString GetSystemHealthStatus() const = 0;
};