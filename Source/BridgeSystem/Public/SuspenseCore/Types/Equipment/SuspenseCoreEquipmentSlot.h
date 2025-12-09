// Copyright Suspense Team. All Rights Reserved.

#pragma once


#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryBaseTypes.h"          // FSuspenseInventoryItemInstance
#include "SuspenseCore/Types/Equipment/SuspenseCoreEquipmentTypes.h"          // FEquipmentSlotConfig and tags
#include "SuspenseCoreEquipmentSlot.generated.h"

/**
 * Lightweight data-only description of a single equipment slot that can host
 * a small grid (e.g., weapon with attachments area). This file intentionally
 * contains no business logic and is safe for use in UPROPERTY/replication.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseEquipmentSlot
{
    GENERATED_BODY()

    /** Slot type (e.g., Equipment.Slot.Weapon.Primary) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SuspenseCore|Equipment")
    FGameplayTag SlotType;

    /** Allowed item types for this slot */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SuspenseCore|Equipment")
    FGameplayTagContainer AllowedItemTypes;

    /** Logical grid width in cells (>=1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SuspenseCore|Equipment")
    int32 Width = 1;

    /** Logical grid height in cells (>=1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SuspenseCore|Equipment")
    int32 Height = 1;

    /** Optional configuration snapshot for this slot (from loadout) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SuspenseCore|Equipment")
    FEquipmentSlotConfig Configuration;

    /**
     * Cells of the slot grid. Each element corresponds to a cell and may hold
     * either a default/invalid instance (free cell) or a valid item instance.
     * For multi-cell items, the anchor is stored at the top-left cell and the
     * remaining covered cells hold default instances.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SuspenseCore|Equipment")
    TArray<FSuspenseInventoryItemInstance> GridItems;

    /** Returns linear index from (X,Y) coordinates in [0..Width-1],[0..Height-1] */
    int32 ToIndex(int32 X, int32 Y) const
    {
        return (X >= 0 && X < Width && Y >= 0 && Y < Height) ? (Y * Width + X) : INDEX_NONE;
    }

    /** Initialize or reinitialize the slot grid to empty state */
    void InitializeSlot()
    {
        const int32 TotalCells = FMath::Max(1, Width) * FMath::Max(1, Height);
        GridItems.Empty(TotalCells);
        GridItems.AddDefaulted(TotalCells);
    }
};


