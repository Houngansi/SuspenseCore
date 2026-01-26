// ISuspenseCoreQuickSlotProvider.h
// Interface for QuickSlot system access from GAS abilities
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SuspenseCore/Types/Weapon/SuspenseCoreMagazineTypes.h"
#include "ISuspenseCoreQuickSlotProvider.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreQuickSlotProvider : public UInterface
{
    GENERATED_BODY()
};

/**
 * ISuspenseCoreQuickSlotProvider
 *
 * Interface for accessing QuickSlot functionality from GAS abilities.
 * Implemented by USuspenseCoreQuickSlotComponent (EquipmentSystem).
 * Used by USuspenseCoreQuickSlotAbility (GAS) to avoid circular dependencies.
 *
 * ARCHITECTURE:
 * - Defined in BridgeSystem (shared)
 * - Implemented in EquipmentSystem
 * - Used by GAS
 */
class BRIDGESYSTEM_API ISuspenseCoreQuickSlotProvider
{
    GENERATED_BODY()

public:
    //==================================================================
    // Slot Queries
    //==================================================================

    /** Get QuickSlot data for specified index */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "QuickSlot")
    FSuspenseCoreQuickSlot GetQuickSlot(int32 SlotIndex) const;

    /** Check if slot is ready to use */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "QuickSlot")
    bool IsSlotReady(int32 SlotIndex) const;

    /** Check if slot has an item */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "QuickSlot")
    bool HasItemInSlot(int32 SlotIndex) const;

    //==================================================================
    // Slot Usage
    //==================================================================

    /** Use the item in specified QuickSlot */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "QuickSlot")
    bool UseQuickSlot(int32 SlotIndex);

    /** Quick swap to magazine in slot */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "QuickSlot")
    bool QuickSwapMagazine(int32 SlotIndex, bool bEmergencyDrop);

    //==================================================================
    // Magazine Access
    //==================================================================

    /** Get magazine from slot (if slot contains a magazine) */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "QuickSlot|Magazine")
    bool GetMagazineFromSlot(int32 SlotIndex, FSuspenseCoreMagazineInstance& OutMagazine) const;

    /** Get first available magazine slot index */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "QuickSlot|Magazine")
    bool GetFirstMagazineSlotIndex(int32& OutSlotIndex) const;

    /** Store ejected magazine in first available slot */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "QuickSlot|Magazine")
    bool StoreEjectedMagazine(const FSuspenseCoreMagazineInstance& EjectedMagazine, int32& OutSlotIndex);

    /** Clear specified slot */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "QuickSlot")
    void ClearSlot(int32 SlotIndex);

    /**
     * Consume one use from a consumable in the slot
     * @param SlotIndex Slot index (0-3)
     * @return true if uses remain (slot still valid), false if depleted (slot was cleared)
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "QuickSlot")
    bool ConsumeSlotUse(int32 SlotIndex);
};
