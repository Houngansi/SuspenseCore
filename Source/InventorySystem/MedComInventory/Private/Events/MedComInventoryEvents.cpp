// MedComInventory/Events/MedComInventoryEvents.cpp
// Copyright MedCom Team. All Rights Reserved.

#include "Events/MedComInventoryEvents.h"
#include "Base/InventoryLogs.h"

UMedComInventoryEvents::UMedComInventoryEvents()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UMedComInventoryEvents::BroadcastInitialized()
{
    UE_LOG(LogInventory, Log, TEXT("Inventory initialized"));
    OnInventoryInitialized.Broadcast();
}

void UMedComInventoryEvents::BroadcastWeightChanged(float NewWeight)
{
    UE_LOG(LogInventory, VeryVerbose, TEXT("Inventory weight changed: %.2f"), NewWeight);
    OnWeightChanged.Broadcast(NewWeight);
}

void UMedComInventoryEvents::BroadcastLockStateChanged(bool bLocked)
{
    UE_LOG(LogInventory, Log, TEXT("Inventory lock state changed: %s"), 
        bLocked ? TEXT("Locked") : TEXT("Unlocked"));
    OnLockStateChanged.Broadcast(bLocked);
}

void UMedComInventoryEvents::BroadcastItemAdded(FName ItemID, int32 Amount)
{
    UE_LOG(LogInventory, Log, TEXT("Item added: %s x%d"), *ItemID.ToString(), Amount);
    OnItemAdded.Broadcast(ItemID, Amount);
}

void UMedComInventoryEvents::BroadcastItemRemoved(FName ItemID, int32 Amount)
{
    UE_LOG(LogInventory, Log, TEXT("Item removed: %s x%d"), *ItemID.ToString(), Amount);
    OnItemRemoved.Broadcast(ItemID, Amount);
}

void UMedComInventoryEvents::BroadcastItemMoved(const FGuid& InstanceID, const FName& ItemID, int32 FromSlot, int32 ToSlot)
{
    UE_LOG(LogInventory, Verbose, TEXT("Item moved: %s (Instance: %s) from slot %d to slot %d"), 
        *ItemID.ToString(), 
        *InstanceID.ToString().Left(8), // Показываем только первые 8 символов GUID для читаемости
        FromSlot, 
        ToSlot);
    OnItemMoved.Broadcast(InstanceID, ItemID, FromSlot, ToSlot);
}

void UMedComInventoryEvents::BroadcastItemStacked(const FGuid& SourceInstanceID, const FGuid& TargetInstanceID, int32 TransferredAmount)
{
    UE_LOG(LogInventory, Verbose, TEXT("Items stacked: %d units transferred from %s to %s"), 
        TransferredAmount,
        *SourceInstanceID.ToString().Left(8),
        *TargetInstanceID.ToString().Left(8));
    OnItemStacked.Broadcast(SourceInstanceID, TargetInstanceID, TransferredAmount);
}

void UMedComInventoryEvents::BroadcastItemSplit(const FGuid& SourceInstanceID, const FGuid& NewInstanceID, int32 SplitAmount, int32 NewSlot)
{
    UE_LOG(LogInventory, Verbose, TEXT("Item split: %d units from %s -> new stack %s at slot %d"), 
        SplitAmount,
        *SourceInstanceID.ToString().Left(8),
        *NewInstanceID.ToString().Left(8),
        NewSlot);
    OnItemSplit.Broadcast(SourceInstanceID, NewInstanceID, SplitAmount, NewSlot);
}

void UMedComInventoryEvents::BroadcastItemSwapped(const FGuid& FirstInstanceID, const FGuid& SecondInstanceID, int32 FirstSlot, int32 SecondSlot)
{
    UE_LOG(LogInventory, Verbose, TEXT("Items swapped: %s (slot %d) <-> %s (slot %d)"),
        *FirstInstanceID.ToString().Left(8),
        FirstSlot,
        *SecondInstanceID.ToString().Left(8),
        SecondSlot);
    OnItemSwapped.Broadcast(FirstInstanceID, SecondInstanceID, FirstSlot, SecondSlot);
}

void UMedComInventoryEvents::BroadcastItemRotated(const FGuid& InstanceID, int32 SlotIndex, bool bRotated)
{
    UE_LOG(LogInventory, Verbose, TEXT("Item rotation changed: %s at slot %d - %s"), 
        *InstanceID.ToString().Left(8),
        SlotIndex,
        bRotated ? TEXT("Rotated") : TEXT("Not rotated"));
    OnItemRotated.Broadcast(InstanceID, SlotIndex, bRotated);
}

void UMedComInventoryEvents::LogOperationResult(const FInventoryOperationResult& Result)
{
    if (Result.IsSuccess())
    {
        UE_LOG(LogInventory, Verbose, TEXT("Operation [%s] succeeded"), 
            *Result.Context.ToString());
    }
    else
    {
        UE_LOG(LogInventory, Warning, TEXT("Operation [%s] failed: %s (Error: %d)"), 
            *Result.Context.ToString(), 
            *Result.ErrorMessage.ToString(),
            (int32)Result.ErrorCode);
    }
}