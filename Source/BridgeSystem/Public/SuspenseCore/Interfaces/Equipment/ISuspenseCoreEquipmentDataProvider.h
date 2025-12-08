// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Types/Equipment/SuspenseEquipmentTypes.h"
#include "ISuspenseCoreEquipmentDataProvider.generated.h"

// Delegates for data change notifications
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSuspenseCoreSlotDataChanged, int32 /*SlotIndex*/, const FSuspenseInventoryItemInstance& /*NewData*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSuspenseCoreSlotConfigurationChanged, int32 /*SlotIndex*/);
DECLARE_MULTICAST_DELEGATE(FOnSuspenseCoreDataStoreReset);

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreEquipmentDataProvider : public UInterface
{
    GENERATED_BODY()
};

/**
 * SuspenseCore Equipment Data Provider Interface
 *
 * Provides contract for equipment data storage and access.
 * Replaces legacy ISuspenseEquipmentDataProvider with SuspenseCore naming.
 *
 * Architecture:
 * - Contract for data access/modification
 * - State snapshots for transactions/rollback
 * - Observable events for changes
 *
 * Key methods are pure virtual, convenience queries have default implementations.
 */
class BRIDGESYSTEM_API ISuspenseCoreEquipmentDataProvider
{
    GENERATED_BODY()

public:
    //========================================
    // Slot Data Access (must implement)
    //========================================

    /** Get item in slot (or invalid instance) */
    virtual FSuspenseInventoryItemInstance GetSlotItem(int32 SlotIndex) const = 0;

    /** Get slot configuration */
    virtual FEquipmentSlotConfig GetSlotConfiguration(int32 SlotIndex) const = 0;

    /** Get all slot configurations */
    virtual TArray<FEquipmentSlotConfig> GetAllSlotConfigurations() const = 0;

    /** Get all equipped items: SlotIndex -> Instance */
    virtual TMap<int32, FSuspenseInventoryItemInstance> GetAllEquippedItems() const = 0;

    /** Get slot count */
    virtual int32 GetSlotCount() const = 0;

    /** Check if slot index is valid */
    virtual bool IsValidSlotIndex(int32 SlotIndex) const = 0;

    /** Check if slot is occupied */
    virtual bool IsSlotOccupied(int32 SlotIndex) const = 0;

    //========================================
    // Data Modification (must implement)
    //========================================

    /** Set item in slot */
    virtual bool SetSlotItem(int32 SlotIndex, const FSuspenseInventoryItemInstance& ItemInstance, bool bNotifyObservers = true) = 0;

    /** Clear slot and return removed item */
    virtual FSuspenseInventoryItemInstance ClearSlot(int32 SlotIndex, bool bNotifyObservers = true) = 0;

    /** Initialize slot configurations */
    virtual bool InitializeSlots(const TArray<FEquipmentSlotConfig>& Configurations) = 0;

    //========================================
    // State Management (must implement)
    //========================================

    virtual int32 GetActiveWeaponSlot() const = 0;
    virtual bool SetActiveWeaponSlot(int32 SlotIndex) = 0;
    virtual FGameplayTag GetCurrentEquipmentState() const = 0;
    virtual bool SetEquipmentState(const FGameplayTag& NewState) = 0;

    //========================================
    // Snapshot Management (must implement)
    //========================================

    /** Create full equipment state snapshot */
    virtual FEquipmentStateSnapshot CreateSnapshot() const = 0;

    /** Restore state from snapshot */
    virtual bool RestoreSnapshot(const FEquipmentStateSnapshot& Snapshot) = 0;

    /** Create single slot snapshot */
    virtual FEquipmentSlotSnapshot CreateSlotSnapshot(int32 SlotIndex) const = 0;

    //========================================
    // Events (must implement)
    //========================================

    virtual FOnSuspenseCoreSlotDataChanged& OnSlotDataChanged() = 0;
    virtual FOnSuspenseCoreSlotConfigurationChanged& OnSlotConfigurationChanged() = 0;
    virtual FOnSuspenseCoreDataStoreReset& OnDataStoreReset() = 0;

    //========================================
    // Queries (default implementation, can override)
    //========================================

    /**
     * Find slots compatible with item type (by config).
     * Does NOT check slot occupancy by default.
     */
    virtual TArray<int32> FindCompatibleSlots(const FGameplayTag& ItemType) const
    {
        TArray<int32> Out;
        const int32 Num = GetSlotCount();
        Out.Reserve(Num);

        for (int32 i = 0; i < Num; ++i)
        {
            if (!IsValidSlotIndex(i))
            {
                continue;
            }

            const FEquipmentSlotConfig Config = GetSlotConfiguration(i);
            if (Config.IsValid() && Config.CanEquipItemType(ItemType))
            {
                Out.Add(i);
            }
        }
        return Out;
    }

    /**
     * Get slot indices by type.
     */
    virtual TArray<int32> GetSlotsByType(EEquipmentSlotType EquipmentType) const
    {
        TArray<int32> Out;
        const int32 Num = GetSlotCount();
        Out.Reserve(Num);

        for (int32 i = 0; i < Num; ++i)
        {
            if (!IsValidSlotIndex(i))
            {
                continue;
            }

            const FEquipmentSlotConfig Config = GetSlotConfiguration(i);
            if (Config.SlotType == EquipmentType)
            {
                Out.Add(i);
            }
        }
        return Out;
    }

    /**
     * Find first empty slot of specified type.
     */
    virtual int32 GetFirstEmptySlotOfType(EEquipmentSlotType EquipmentType) const
    {
        const int32 Num = GetSlotCount();
        for (int32 i = 0; i < Num; ++i)
        {
            if (!IsValidSlotIndex(i))
            {
                continue;
            }

            const FEquipmentSlotConfig Config = GetSlotConfiguration(i);
            if (Config.SlotType == EquipmentType && !IsSlotOccupied(i))
            {
                return i;
            }
        }
        return INDEX_NONE;
    }

    //========================================
    // Utility Methods (default implementation)
    //========================================

    /**
     * Get total weight of equipped items.
     */
    virtual float GetTotalEquippedWeight() const
    {
        float Total = 0.0f;

        const int32 Num = GetSlotCount();
        for (int32 i = 0; i < Num; ++i)
        {
            if (!IsValidSlotIndex(i) || !IsSlotOccupied(i))
            {
                continue;
            }

            const FSuspenseInventoryItemInstance Item = GetSlotItem(i);
            Total += Item.GetRuntimeProperty(TEXT("Weight"), 0.0f);
        }

        return Total;
    }

    /**
     * Check item requirements for slot.
     * Base implementation allows all - override for custom logic.
     */
    virtual bool MeetsItemRequirements(const FSuspenseInventoryItemInstance& /*ItemInstance*/, int32 /*SlotIndex*/) const
    {
        return true;
    }

    /**
     * Get debug info string.
     */
    virtual FString GetDebugInfo() const
    {
        int32 Occupied = 0;
        const int32 Num = GetSlotCount();

        for (int32 i = 0; i < Num; ++i)
        {
            if (IsValidSlotIndex(i) && IsSlotOccupied(i))
            {
                ++Occupied;
            }
        }

        const FGameplayTag State = GetCurrentEquipmentState();

        return FString::Printf(
            TEXT("SuspenseCoreEquipmentDataProvider: Slots=%d, Occupied=%d, Weight=%.2f, State=%s"),
            Num,
            Occupied,
            GetTotalEquippedWeight(),
            *State.ToString()
        );
    }
};

