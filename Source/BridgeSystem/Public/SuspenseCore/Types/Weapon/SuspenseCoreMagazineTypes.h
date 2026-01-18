// SuspenseCoreMagazineTypes.h
// Tarkov-style magazine and ammo system types
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreMagazineTypes.generated.h"

/**
 * Magazine data from DataTable
 * Defines magazine properties and compatibility
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreMagazineData : public FTableRowBase
{
    GENERATED_BODY()

    //==================================================================
    // Identity
    //==================================================================

    /** Unique magazine identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine|Identity")
    FName MagazineID = NAME_None;

    /** Display name for UI */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine|Identity")
    FText DisplayName;

    /** Description */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine|Identity", meta = (MultiLine = "true"))
    FText Description;

    //==================================================================
    // Compatibility
    //==================================================================

    /** Caliber this magazine accepts (e.g., Item.Ammo.556x45) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine|Compatibility",
        meta = (Categories = "Item.Ammo"))
    FGameplayTag Caliber;

    /** Weapons compatible with this magazine */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine|Compatibility",
        meta = (Categories = "Weapon"))
    FGameplayTagContainer CompatibleWeapons;

    //==================================================================
    // Capacity
    //==================================================================

    /** Maximum rounds this magazine can hold */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine|Capacity",
        meta = (ClampMin = "1", ClampMax = "200"))
    int32 MaxCapacity = 30;

    //==================================================================
    // Stats
    //==================================================================

    /** Time to load one round into magazine (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine|Stats",
        meta = (ClampMin = "0.1", ClampMax = "5.0"))
    float LoadTimePerRound = 0.5f;

    /** Time to unload one round from magazine (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine|Stats",
        meta = (ClampMin = "0.1", ClampMax = "3.0"))
    float UnloadTimePerRound = 0.3f;

    /** Multiplier on weapon's base reload time */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine|Stats",
        meta = (ClampMin = "0.5", ClampMax = "2.0"))
    float ReloadTimeModifier = 1.0f;

    /** Ergonomics penalty when magazine is full (affects ADS time, etc.) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine|Stats",
        meta = (ClampMin = "0", ClampMax = "50"))
    int32 ErgonomicsPenalty = 0;

    //==================================================================
    // Reliability
    //==================================================================

    /** Chance of proper feed per shot (0.0 - 1.0) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine|Reliability",
        meta = (ClampMin = "0.9", ClampMax = "1.0"))
    float FeedReliability = 0.999f;

    /** Current durability */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine|Reliability",
        meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float Durability = 100.0f;

    /** Maximum durability */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine|Reliability",
        meta = (ClampMin = "1.0", ClampMax = "100.0"))
    float MaxDurability = 100.0f;

    //==================================================================
    // Inventory
    //==================================================================

    /** Size in inventory grid */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine|Inventory",
        meta = (ClampMin = "1", ClampMax = "5"))
    FIntPoint GridSize = FIntPoint(1, 2);

    /** Empty magazine weight (kg) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine|Inventory",
        meta = (ClampMin = "0.01", ClampMax = "2.0"))
    float EmptyWeight = 0.1f;

    /** Weight per round (kg) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine|Inventory",
        meta = (ClampMin = "0.001", ClampMax = "0.1"))
    float WeightPerRound = 0.012f;

    //==================================================================
    // Methods
    //==================================================================

    /** Calculate total weight with given round count */
    float GetWeightWithRounds(int32 RoundCount) const
    {
        return EmptyWeight + (WeightPerRound * FMath::Clamp(RoundCount, 0, MaxCapacity));
    }

    /** Check if magazine is compatible with weapon tag */
    bool IsCompatibleWithWeapon(const FGameplayTag& WeaponTag) const
    {
        return CompatibleWeapons.HasTag(WeaponTag);
    }

    /** Check if ammo caliber matches */
    bool IsCompatibleWithCaliber(const FGameplayTag& AmmoCaliber) const
    {
        return Caliber.MatchesTagExact(AmmoCaliber);
    }

    bool IsValid() const
    {
        return !MagazineID.IsNone() && MaxCapacity > 0 && Caliber.IsValid();
    }
};

/**
 * Runtime magazine instance
 * Tracks actual state of a magazine in the game
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreMagazineInstance
{
    GENERATED_BODY()

    //==================================================================
    // Identity
    //==================================================================

    /** DataTable row name for magazine data */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine")
    FName MagazineID = NAME_None;

    /** Unique runtime instance ID */
    UPROPERTY(BlueprintReadOnly, Category = "Magazine")
    FGuid InstanceGuid;

    //==================================================================
    // Contents
    //==================================================================

    /** Type of ammo currently in magazine (all rounds are same type) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine|Contents")
    FName LoadedAmmoID = NAME_None;

    /** Current number of rounds in magazine */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine|Contents",
        meta = (ClampMin = "0"))
    int32 CurrentRoundCount = 0;

    /** Cached max capacity from MagazineData */
    UPROPERTY(BlueprintReadOnly, Category = "Magazine|Contents")
    int32 MaxCapacity = 30;

    //==================================================================
    // State
    //==================================================================

    /** Current durability (affects reliability) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Magazine|State",
        meta = (ClampMin = "0.0", ClampMax = "100.0"))
    float CurrentDurability = 100.0f;

    /** Is this magazine currently inserted in a weapon */
    UPROPERTY(BlueprintReadOnly, Category = "Magazine|State")
    bool bIsInsertedInWeapon = false;

    /**
     * Source QuickSlot index where this magazine was taken from.
     * Used for returning magazine to original slot on eject.
     * -1 means magazine came from inventory or was spawned (not from QuickSlot).
     */
    UPROPERTY(BlueprintReadOnly, Category = "Magazine|State")
    int32 SourceQuickSlotIndex = -1;

    //==================================================================
    // Constructors
    //==================================================================

    FSuspenseCoreMagazineInstance()
    {
        InstanceGuid = FGuid::NewGuid();
    }

    FSuspenseCoreMagazineInstance(const FName& InMagazineID, int32 InMaxCapacity)
        : MagazineID(InMagazineID)
        , MaxCapacity(InMaxCapacity)
    {
        InstanceGuid = FGuid::NewGuid();
    }

    //==================================================================
    // Methods
    //==================================================================

    /** Check if magazine is empty */
    bool IsEmpty() const { return CurrentRoundCount <= 0; }

    /** Check if magazine is full */
    bool IsFull() const { return CurrentRoundCount >= MaxCapacity; }

    /** Get available space for more rounds */
    int32 GetAvailableSpace() const { return FMath::Max(0, MaxCapacity - CurrentRoundCount); }

    /** Check if magazine has ammo loaded */
    bool HasAmmo() const { return CurrentRoundCount > 0 && !LoadedAmmoID.IsNone(); }

    /** Check if this magazine is valid */
    bool IsValid() const { return !MagazineID.IsNone() && InstanceGuid.IsValid(); }

    /**
     * Load rounds into magazine
     * @param AmmoID Type of ammo to load
     * @param Count Number of rounds to load
     * @return Actual number of rounds loaded
     */
    int32 LoadRounds(const FName& AmmoID, int32 Count)
    {
        // Can only load if empty or same ammo type
        if (HasAmmo() && LoadedAmmoID != AmmoID)
        {
            return 0; // Different ammo type
        }

        int32 SpaceAvailable = GetAvailableSpace();
        int32 RoundsToLoad = FMath::Min(Count, SpaceAvailable);

        if (RoundsToLoad > 0)
        {
            LoadedAmmoID = AmmoID;
            CurrentRoundCount += RoundsToLoad;
        }

        return RoundsToLoad;
    }

    /**
     * Unload rounds from magazine
     * @param Count Number of rounds to unload (0 = all)
     * @return Actual number of rounds unloaded
     */
    int32 UnloadRounds(int32 Count = 0)
    {
        if (Count <= 0)
        {
            Count = CurrentRoundCount;
        }

        int32 RoundsToUnload = FMath::Min(Count, CurrentRoundCount);
        CurrentRoundCount -= RoundsToUnload;

        if (CurrentRoundCount <= 0)
        {
            LoadedAmmoID = NAME_None;
            CurrentRoundCount = 0;
        }

        return RoundsToUnload;
    }

    /**
     * Feed one round from magazine (for firing)
     * @return true if round was available and fed
     */
    bool FeedRound()
    {
        if (CurrentRoundCount > 0)
        {
            CurrentRoundCount--;
            if (CurrentRoundCount <= 0)
            {
                LoadedAmmoID = NAME_None;
            }
            return true;
        }
        return false;
    }

    /** Get fill percentage (0.0 - 1.0) */
    float GetFillPercentage() const
    {
        return MaxCapacity > 0 ? static_cast<float>(CurrentRoundCount) / static_cast<float>(MaxCapacity) : 0.0f;
    }

    /** Equality operator for finding in arrays */
    bool operator==(const FSuspenseCoreMagazineInstance& Other) const
    {
        return InstanceGuid == Other.InstanceGuid;
    }

    bool operator!=(const FSuspenseCoreMagazineInstance& Other) const
    {
        return !(*this == Other);
    }
};

/**
 * Chambered round data
 * Represents a single round in the weapon's chamber
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreChamberedRound
{
    GENERATED_BODY()

    /** Ammo type of the chambered round */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chamber")
    FName AmmoID = NAME_None;

    /** Condition of the round (0.0 - 1.0, affects reliability) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chamber",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Condition = 1.0f;

    /** Is there a round chambered */
    bool IsChambered() const { return !AmmoID.IsNone(); }

    /** Clear the chamber */
    void Clear()
    {
        AmmoID = NAME_None;
        Condition = 1.0f;
    }

    /** Set chambered round */
    void Chamber(const FName& InAmmoID, float InCondition = 1.0f)
    {
        AmmoID = InAmmoID;
        Condition = FMath::Clamp(InCondition, 0.0f, 1.0f);
    }
};

/**
 * Weapon ammo state (magazine + chamber)
 * Complete state of ammunition in a weapon
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreWeaponAmmoState
{
    GENERATED_BODY()

    /** Currently inserted magazine */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponAmmo")
    FSuspenseCoreMagazineInstance InsertedMagazine;

    /** Currently chambered round */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponAmmo")
    FSuspenseCoreChamberedRound ChamberedRound;

    /** Does this weapon have a magazine inserted */
    UPROPERTY(BlueprintReadOnly, Category = "WeaponAmmo")
    bool bHasMagazine = false;

    //==================================================================
    // Methods
    //==================================================================

    /** Check if weapon is ready to fire (has chambered round) */
    bool IsReadyToFire() const { return ChamberedRound.IsChambered(); }

    /** Check if weapon can fire (has ammo in mag or chamber) */
    bool CanFire() const { return ChamberedRound.IsChambered(); }

    /** Check if magazine is empty */
    bool IsMagazineEmpty() const { return !bHasMagazine || InsertedMagazine.IsEmpty(); }

    /** Get total rounds available (chamber + magazine) */
    int32 GetTotalRounds() const
    {
        int32 Total = ChamberedRound.IsChambered() ? 1 : 0;
        if (bHasMagazine)
        {
            Total += InsertedMagazine.CurrentRoundCount;
        }
        return Total;
    }

    /** Insert a magazine */
    bool InsertMagazine(const FSuspenseCoreMagazineInstance& Magazine)
    {
        if (bHasMagazine)
        {
            return false; // Already has magazine
        }

        InsertedMagazine = Magazine;
        InsertedMagazine.bIsInsertedInWeapon = true;
        bHasMagazine = true;
        return true;
    }

    /** Eject current magazine, returns the magazine instance */
    FSuspenseCoreMagazineInstance EjectMagazine()
    {
        if (!bHasMagazine)
        {
            return FSuspenseCoreMagazineInstance();
        }

        FSuspenseCoreMagazineInstance EjectedMag = InsertedMagazine;
        EjectedMag.bIsInsertedInWeapon = false;

        InsertedMagazine = FSuspenseCoreMagazineInstance();
        bHasMagazine = false;

        return EjectedMag;
    }

    /**
     * Chamber a round from magazine
     * @return true if round was chambered
     */
    bool ChamberFromMagazine()
    {
        if (ChamberedRound.IsChambered())
        {
            return false; // Already chambered
        }

        if (!bHasMagazine || InsertedMagazine.IsEmpty())
        {
            return false; // No ammo in magazine
        }

        // Feed round from magazine
        FName AmmoType = InsertedMagazine.LoadedAmmoID;
        if (InsertedMagazine.FeedRound())
        {
            ChamberedRound.Chamber(AmmoType);
            return true;
        }

        return false;
    }

    /**
     * Fire the weapon (consume chambered round)
     * @param bAutoChamber If true, automatically chamber next round from magazine
     * @return Ammo ID of the fired round, or NAME_None if couldn't fire
     */
    FName Fire(bool bAutoChamber = true)
    {
        if (!ChamberedRound.IsChambered())
        {
            return NAME_None;
        }

        FName FiredAmmoID = ChamberedRound.AmmoID;
        ChamberedRound.Clear();

        if (bAutoChamber)
        {
            ChamberFromMagazine();
        }

        return FiredAmmoID;
    }

    /** Eject the chambered round without firing */
    FSuspenseCoreChamberedRound EjectChamberedRound()
    {
        FSuspenseCoreChamberedRound Ejected = ChamberedRound;
        ChamberedRound.Clear();
        return Ejected;
    }

    /** Clear all ammo state */
    void Clear()
    {
        InsertedMagazine = FSuspenseCoreMagazineInstance();
        ChamberedRound.Clear();
        bHasMagazine = false;
    }
};

/**
 * QuickSlot data for fast magazine/item access
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreQuickSlot
{
    GENERATED_BODY()

    /** Slot index (0-3 for QuickSlots 1-4) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickSlot")
    int32 SlotIndex = 0;

    /** GameplayTag for this slot */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickSlot",
        meta = (Categories = "Equipment.QuickSlot"))
    FGameplayTag SlotTag;

    /** Instance ID of assigned item (from inventory) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickSlot")
    FGuid AssignedItemInstanceID;

    /** Cached item ID for quick access */
    UPROPERTY(BlueprintReadOnly, Category = "QuickSlot")
    FName AssignedItemID = NAME_None;

    /** Is this slot available for use */
    UPROPERTY(BlueprintReadOnly, Category = "QuickSlot")
    bool bIsAvailable = false;

    /** Cooldown remaining (for consumables) */
    UPROPERTY(BlueprintReadOnly, Category = "QuickSlot")
    float CooldownRemaining = 0.0f;

    //==================================================================
    // Methods
    //==================================================================

    /** Check if slot has an item assigned */
    bool HasItem() const { return AssignedItemInstanceID.IsValid(); }

    /** Check if slot is ready to use */
    bool IsReady() const { return HasItem() && bIsAvailable && CooldownRemaining <= 0.0f; }

    /** Clear the slot */
    void Clear()
    {
        AssignedItemInstanceID.Invalidate();
        AssignedItemID = NAME_None;
        bIsAvailable = false;
        CooldownRemaining = 0.0f;
    }

    /** Assign item to slot */
    void AssignItem(const FGuid& InstanceID, const FName& ItemID)
    {
        AssignedItemInstanceID = InstanceID;
        AssignedItemID = ItemID;
        bIsAvailable = true;
    }
};

/**
 * Reload type enum
 */
UENUM(BlueprintType)
enum class ESuspenseCoreReloadType : uint8
{
    /** Swap magazine while round is chambered */
    Tactical        UMETA(DisplayName = "Tactical Reload"),

    /** Insert magazine into empty weapon, chamber round */
    Empty           UMETA(DisplayName = "Empty Reload"),

    /** Drop current magazine, insert new, chamber */
    Emergency       UMETA(DisplayName = "Emergency Reload"),

    /** Just rack the slide/bolt to chamber */
    ChamberOnly     UMETA(DisplayName = "Chamber Only"),

    /** No reload possible */
    None            UMETA(DisplayName = "No Reload")
};

/**
 * Reload request data
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreReloadRequest
{
    GENERATED_BODY()

    /** Type of reload to perform */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reload")
    ESuspenseCoreReloadType ReloadType = ESuspenseCoreReloadType::None;

    /** Magazine to insert (if swapping) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reload")
    FSuspenseCoreMagazineInstance NewMagazine;

    /** QuickSlot index to use (-1 for inventory) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reload")
    int32 QuickSlotIndex = -1;

    /** Calculated reload time */
    UPROPERTY(BlueprintReadOnly, Category = "Reload")
    float ReloadDuration = 0.0f;

    /** Is this a valid reload request */
    bool IsValid() const
    {
        return ReloadType != ESuspenseCoreReloadType::None;
    }
};
