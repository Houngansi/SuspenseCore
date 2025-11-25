// SuspenseInventory/Events/SuspenseInventoryEvents.cpp
// Copyright SuspenseCore Team. All Rights Reserved.

#include "Events/SuspenseInventoryEvents.h"
#include "Base/InventoryLogs.h"

USuspenseInventoryEvents::USuspenseInventoryEvents()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void USuspenseInventoryEvents::BroadcastInitialized()
{
    UE_LOG(LogSuspenseInventory, Log, TEXT("Inventory initialized"));
    OnInventoryInitialized.Broadcast();
}

void USuspenseInventoryEvents::BroadcastWeightChanged(float NewWeight)
{
    UE_LOG(LogSuspenseInventory, VeryVerbose, TEXT("Inventory weight changed: %.2f"), NewWeight);
    OnWeightChanged.Broadcast(NewWeight);
}

void USuspenseInventoryEvents::BroadcastLockStateChanged(bool bLocked)
{
    UE_LOG(LogSuspenseInventory, Log, TEXT("Inventory lock state changed: %s"),
        bLocked ? TEXT("Locked") : TEXT("Unlocked"));
    OnLockStateChanged.Broadcast(bLocked);
}

void USuspenseInventoryEvents::BroadcastItemAdded(FName ItemID, int32 Amount)
{
    UE_LOG(LogSuspenseInventory, Log, TEXT("Item added: %s x%d"), *ItemID.ToString(), Amount);
    OnItemAdded.Broadcast(ItemID, Amount);
}

void USuspenseInventoryEvents::BroadcastItemRemoved(FName ItemID, int32 Amount)
{
    UE_LOG(LogSuspenseInventory, Log, TEXT("Item removed: %s x%d"), *ItemID.ToString(), Amount);
    OnItemRemoved.Broadcast(ItemID, Amount);
}

void USuspenseInventoryEvents::BroadcastItemMoved(const FGuid& InstanceID, const FName& ItemID, int32 FromSlot, int32 ToSlot)
{
    UE_LOG(LogSuspenseInventory, Verbose, TEXT("Item moved: %s (Instance: %s) from slot %d to slot %d"),
        *ItemID.ToString(),
        *InstanceID.ToString().Left(8),
        FromSlot,
        ToSlot);
    OnItemMoved.Broadcast(InstanceID, ItemID, FromSlot, ToSlot);
}

void USuspenseInventoryEvents::BroadcastItemStacked(const FGuid& SourceInstanceID, const FGuid& TargetInstanceID, int32 TransferredAmount)
{
    UE_LOG(LogSuspenseInventory, Verbose, TEXT("Items stacked: %d units transferred from %s to %s"),
        TransferredAmount,
        *SourceInstanceID.ToString().Left(8),
        *TargetInstanceID.ToString().Left(8));
    OnItemStacked.Broadcast(SourceInstanceID, TargetInstanceID, TransferredAmount);
}

void USuspenseInventoryEvents::BroadcastItemSplit(const FGuid& SourceInstanceID, const FGuid& NewInstanceID, int32 SplitAmount, int32 NewSlot)
{
    UE_LOG(LogSuspenseInventory, Verbose, TEXT("Item split: %d units from %s -> new stack %s at slot %d"),
        SplitAmount,
        *SourceInstanceID.ToString().Left(8),
        *NewInstanceID.ToString().Left(8),
        NewSlot);
    OnItemSplit.Broadcast(SourceInstanceID, NewInstanceID, SplitAmount, NewSlot);
}

void USuspenseInventoryEvents::BroadcastItemSwapped(const FGuid& FirstInstanceID, const FGuid& SecondInstanceID, int32 FirstSlot, int32 SecondSlot)
{
    UE_LOG(LogSuspenseInventory, Verbose, TEXT("Items swapped: %s (slot %d) <-> %s (slot %d)"),
        *FirstInstanceID.ToString().Left(8),
        FirstSlot,
        *SecondInstanceID.ToString().Left(8),
        SecondSlot);
    OnItemSwapped.Broadcast(FirstInstanceID, SecondInstanceID, FirstSlot, SecondSlot);
}

void USuspenseInventoryEvents::BroadcastItemRotated(const FGuid& InstanceID, int32 SlotIndex, bool bRotated)
{
    UE_LOG(LogSuspenseInventory, Verbose, TEXT("Item rotation changed: %s at slot %d - %s"),
        *InstanceID.ToString().Left(8),
        SlotIndex,
        bRotated ? TEXT("Rotated") : TEXT("Not rotated"));
    OnItemRotated.Broadcast(InstanceID, SlotIndex, bRotated);
}

void USuspenseInventoryEvents::LogOperationResult(const FInventoryOperationResult& Result)
{
    if (Result.IsSuccess())
    {
        UE_LOG(LogSuspenseInventory, Verbose, TEXT("Operation [%s] succeeded"),
            *Result.Context.ToString());
    }
    else
    {
        UE_LOG(LogSuspenseInventory, Warning, TEXT("Operation [%s] failed: %s (Error: %d)"),
            *Result.Context.ToString(),
            *Result.ErrorMessage.ToString(),
            (int32)Result.ErrorCode);
    }
}
