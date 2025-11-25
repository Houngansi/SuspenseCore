// SuspenseInventory/Events/SuspenseInventoryEvents.h
// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/Inventory/InventoryTypes.h"
#include "Operations/InventoryResult.h"
#include "SuspenseInventoryEvents.generated.h"

// Core inventory events
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSuspenseInventoryInitializedEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSuspenseInventoryWeightChangedEvent, float, NewWeight);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSuspenseInventoryLockedChangedEvent, bool, bLocked);

// Item events
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSuspenseItemAddedEvent, FName, ItemID, int32, Amount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSuspenseItemRemovedEvent, FName, ItemID, int32, Amount);

// Instance-based events
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FSuspenseItemMovedEvent, FGuid, InstanceID, FName, ItemID, int32, FromSlot, int32, ToSlot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSuspenseItemStackedEvent, FGuid, SourceInstanceID, FGuid, TargetInstanceID, int32, TransferredAmount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FSuspenseItemSplitEvent, FGuid, SourceInstanceID, FGuid, NewInstanceID, int32, SplitAmount, int32, NewSlot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FSuspenseItemSwappedEvent, FGuid, FirstInstanceID, FGuid, SecondInstanceID, int32, FirstSlot, int32, SecondSlot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSuspenseItemRotatedEvent, FGuid, InstanceID, int32, SlotIndex, bool, bRotated);

/**
 * Component responsible for inventory events and notifications
 * Fully migrated to instance-based system - no more UObject* references!
 */
UCLASS(ClassGroup=(Suspense), meta=(BlueprintSpawnableComponent))
class SUSPENSEINVENTORY_API USuspenseInventoryEvents : public UActorComponent
{
    GENERATED_BODY()

public:
    USuspenseInventoryEvents();

    //==================================================================
    // Core Inventory Events
    //==================================================================

    /** Fired when inventory is initialized */
    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FSuspenseInventoryInitializedEvent OnInventoryInitialized;

    /** Fired when inventory weight changes */
    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FSuspenseInventoryWeightChangedEvent OnWeightChanged;

    /** Fired when inventory lock state changes */
    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FSuspenseInventoryLockedChangedEvent OnLockStateChanged;

    //==================================================================
    // Item Management Events
    //==================================================================

    /** Fired when item is added to inventory */
    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events|Items")
    FSuspenseItemAddedEvent OnItemAdded;

    /** Fired when item is removed from inventory */
    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events|Items")
    FSuspenseItemRemovedEvent OnItemRemoved;

    /** Fired when item is moved to new position */
    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events|Items")
    FSuspenseItemMovedEvent OnItemMoved;

    /** Fired when items are stacked together */
    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events|Items")
    FSuspenseItemStackedEvent OnItemStacked;

    /** Fired when item stack is split */
    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events|Items")
    FSuspenseItemSplitEvent OnItemSplit;

    /** Fired when two items swap positions */
    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events|Items")
    FSuspenseItemSwappedEvent OnItemSwapped;

    /** Fired when item is rotated */
    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events|Items")
    FSuspenseItemRotatedEvent OnItemRotated;

    //==================================================================
    // Broadcast Methods
    //==================================================================

    /**
     * Broadcasts inventory initialized event
     */
    void BroadcastInitialized();

    /**
     * Broadcasts weight changed event
     * @param NewWeight New weight value
     */
    void BroadcastWeightChanged(float NewWeight);

    /**
     * Broadcasts lock state changed event
     * @param bLocked New lock state
     */
    void BroadcastLockStateChanged(bool bLocked);

    /**
     * Broadcasts item added event
     * @param ItemID ID of the added item
     * @param Amount Amount added
     */
    void BroadcastItemAdded(FName ItemID, int32 Amount);

    /**
     * Broadcasts item removed event
     * @param ItemID ID of the removed item
     * @param Amount Amount removed
     */
    void BroadcastItemRemoved(FName ItemID, int32 Amount);

    /**
     * Broadcasts item moved event
     * @param InstanceID Unique instance identifier
     * @param ItemID Item type identifier
     * @param FromSlot Source slot index
     * @param ToSlot Target slot index
     */
    void BroadcastItemMoved(const FGuid& InstanceID, const FName& ItemID, int32 FromSlot, int32 ToSlot);

    /**
     * Broadcasts item stacked event
     * @param SourceInstanceID Source stack instance ID
     * @param TargetInstanceID Target stack instance ID
     * @param TransferredAmount Amount transferred from source to target
     */
    void BroadcastItemStacked(const FGuid& SourceInstanceID, const FGuid& TargetInstanceID, int32 TransferredAmount);

    /**
     * Broadcasts item split event
     * @param SourceInstanceID Original stack instance ID
     * @param NewInstanceID New stack instance ID
     * @param SplitAmount Amount moved to new stack
     * @param NewSlot Slot where new stack was placed
     */
    void BroadcastItemSplit(const FGuid& SourceInstanceID, const FGuid& NewInstanceID, int32 SplitAmount, int32 NewSlot);

    /**
     * Broadcasts item swapped event
     * @param FirstInstanceID First item instance ID
     * @param SecondInstanceID Second item instance ID
     * @param FirstSlot Original position of first item
     * @param SecondSlot Original position of second item
     */
    void BroadcastItemSwapped(const FGuid& FirstInstanceID, const FGuid& SecondInstanceID, int32 FirstSlot, int32 SecondSlot);

    /**
     * Broadcasts item rotated event
     * @param InstanceID Instance that was rotated
     * @param SlotIndex Current slot position
     * @param bRotated New rotation state
     */
    void BroadcastItemRotated(const FGuid& InstanceID, int32 SlotIndex, bool bRotated);

    /**
     * Logs an operation result
     * @param Result Operation result
     */
    void LogOperationResult(const FInventoryOperationResult& Result);
};
