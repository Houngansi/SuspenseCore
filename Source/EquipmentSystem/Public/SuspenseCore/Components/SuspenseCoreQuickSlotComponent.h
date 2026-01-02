// SuspenseCoreQuickSlotComponent.h
// QuickSlot system for fast magazine/item access
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCoreEquipmentComponentBase.h"
#include "SuspenseCore/Types/Weapon/SuspenseCoreMagazineTypes.h"
#include "SuspenseCoreQuickSlotComponent.generated.h"

// Forward declarations
class USuspenseCoreMagazineComponent;
class USuspenseCoreInventoryComponent;

/** Number of quick slots available */
static constexpr int32 SUSPENSECORE_QUICKSLOT_COUNT = 4;

//==================================================================
// Delegates
//==================================================================

/** Fired when a quick slot's contents change */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FOnQuickSlotChanged,
    int32, SlotIndex,
    const FGuid&, OldItemInstanceID,
    const FGuid&, NewItemInstanceID
);

/** Fired when a quick slot is used */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnQuickSlotUsed,
    int32, SlotIndex,
    bool, bSuccess
);

/** Fired when cooldown updates */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnQuickSlotCooldownChanged,
    int32, SlotIndex,
    float, RemainingCooldown
);

/** Fired when quick slot availability changes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnQuickSlotAvailabilityChanged,
    int32, SlotIndex,
    bool, bIsAvailable
);

/**
 * USuspenseCoreQuickSlotComponent
 *
 * Manages QuickSlots 1-4 for fast access to magazines, consumables, and grenades.
 * Integrates with MagazineComponent for fast magazine swaps during reload.
 *
 * FEATURES:
 * - 4 QuickSlots (keyboard 1-4)
 * - Magazine quick-swap for fast reloads
 * - Consumable quick-use (bandages, medkits)
 * - Grenade quick-throw
 * - Per-slot cooldowns
 * - Full replication support
 *
 * USAGE:
 *   // Assign magazine to slot 1
 *   QuickSlotComp->AssignItemToSlot(0, MagazineInstanceID, "STANAG_30");
 *
 *   // Fast reload with slot 1 magazine
 *   QuickSlotComp->UseQuickSlot(0);
 *
 * @see USuspenseCoreMagazineComponent
 * @see FSuspenseCoreQuickSlot
 */
UCLASS(ClassGroup=(SuspenseCore), meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseCoreQuickSlotComponent : public USuspenseCoreEquipmentComponentBase
{
    GENERATED_BODY()

public:
    USuspenseCoreQuickSlotComponent();

    //==================================================================
    // UActorComponent Interface
    //==================================================================

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    //==================================================================
    // Initialization
    //==================================================================

    /**
     * Initialize with magazine component reference
     * @param InMagazineComponent Magazine component for fast reloads
     */
    UFUNCTION(BlueprintCallable, Category = "QuickSlot")
    void InitializeWithMagazineComponent(USuspenseCoreMagazineComponent* InMagazineComponent);

    /**
     * Set inventory component reference for item validation
     * @param InInventoryComponent Inventory component to query items from
     */
    UFUNCTION(BlueprintCallable, Category = "QuickSlot")
    void SetInventoryComponent(USuspenseCoreInventoryComponent* InInventoryComponent);

    //==================================================================
    // Slot Assignment
    //==================================================================

    /**
     * Assign an item to a quick slot
     * @param SlotIndex Slot index (0-3)
     * @param ItemInstanceID Instance ID from inventory
     * @param ItemID Item DataTable row name
     * @return true if assignment succeeded
     */
    UFUNCTION(BlueprintCallable, Category = "QuickSlot|Assignment")
    bool AssignItemToSlot(int32 SlotIndex, const FGuid& ItemInstanceID, FName ItemID);

    /**
     * Assign a magazine instance to a quick slot
     * @param SlotIndex Slot index (0-3)
     * @param Magazine Magazine instance to assign
     * @return true if assignment succeeded
     */
    UFUNCTION(BlueprintCallable, Category = "QuickSlot|Assignment")
    bool AssignMagazineToSlot(int32 SlotIndex, const FSuspenseCoreMagazineInstance& Magazine);

    /**
     * Clear a quick slot
     * @param SlotIndex Slot index (0-3)
     */
    UFUNCTION(BlueprintCallable, Category = "QuickSlot|Assignment")
    void ClearSlot(int32 SlotIndex);

    /**
     * Clear all quick slots
     */
    UFUNCTION(BlueprintCallable, Category = "QuickSlot|Assignment")
    void ClearAllSlots();

    /**
     * Swap items between two slots
     * @param SlotIndexA First slot
     * @param SlotIndexB Second slot
     * @return true if swap succeeded
     */
    UFUNCTION(BlueprintCallable, Category = "QuickSlot|Assignment")
    bool SwapSlots(int32 SlotIndexA, int32 SlotIndexB);

    //==================================================================
    // Slot Usage
    //==================================================================

    /**
     * Use the item in a quick slot
     * For magazines: triggers quick reload
     * For consumables: uses the item
     * For grenades: prepares throw
     * @param SlotIndex Slot index (0-3)
     * @return true if use was initiated
     */
    UFUNCTION(BlueprintCallable, Category = "QuickSlot|Usage")
    bool UseQuickSlot(int32 SlotIndex);

    /**
     * Quick swap to magazine in slot (for fast reloads)
     * @param SlotIndex Slot index containing magazine (0-3)
     * @param bEmergencyDrop If true, drops current magazine instead of storing
     * @return true if reload was initiated
     */
    UFUNCTION(BlueprintCallable, Category = "QuickSlot|Usage")
    bool QuickSwapMagazine(int32 SlotIndex, bool bEmergencyDrop = false);

    /**
     * Start cooldown on a slot (called after use)
     * @param SlotIndex Slot index (0-3)
     * @param CooldownDuration Duration in seconds
     */
    UFUNCTION(BlueprintCallable, Category = "QuickSlot|Usage")
    void StartSlotCooldown(int32 SlotIndex, float CooldownDuration);

    //==================================================================
    // Queries
    //==================================================================

    /**
     * Get quick slot data
     * @param SlotIndex Slot index (0-3)
     * @return QuickSlot data (check IsReady() for validity)
     */
    UFUNCTION(BlueprintPure, Category = "QuickSlot|Query")
    FSuspenseCoreQuickSlot GetQuickSlot(int32 SlotIndex) const;

    /**
     * Get all quick slots
     * @return Array of all 4 quick slots
     */
    UFUNCTION(BlueprintPure, Category = "QuickSlot|Query")
    TArray<FSuspenseCoreQuickSlot> GetAllQuickSlots() const;

    /**
     * Get slots containing magazines
     * @return Array of quick slots that have magazines assigned
     */
    UFUNCTION(BlueprintPure, Category = "QuickSlot|Query")
    TArray<FSuspenseCoreQuickSlot> GetMagazineSlots() const;

    /**
     * Get first available magazine slot index
     * @param OutSlotIndex Output slot index
     * @return true if a magazine slot was found
     */
    UFUNCTION(BlueprintPure, Category = "QuickSlot|Query")
    bool GetFirstMagazineSlotIndex(int32& OutSlotIndex) const;

    /**
     * Check if slot has an item
     * @param SlotIndex Slot index (0-3)
     * @return true if slot has an item assigned
     */
    UFUNCTION(BlueprintPure, Category = "QuickSlot|Query")
    bool HasItemInSlot(int32 SlotIndex) const;

    /**
     * Check if slot is ready to use (has item, available, no cooldown)
     * @param SlotIndex Slot index (0-3)
     * @return true if slot is ready
     */
    UFUNCTION(BlueprintPure, Category = "QuickSlot|Query")
    bool IsSlotReady(int32 SlotIndex) const;

    /**
     * Get remaining cooldown for a slot
     * @param SlotIndex Slot index (0-3)
     * @return Remaining cooldown in seconds (0 if ready)
     */
    UFUNCTION(BlueprintPure, Category = "QuickSlot|Query")
    float GetSlotCooldown(int32 SlotIndex) const;

    /**
     * Check if item is a magazine
     * @param ItemID Item DataTable row name
     * @return true if item is a magazine type
     */
    UFUNCTION(BlueprintPure, Category = "QuickSlot|Query")
    bool IsItemMagazine(FName ItemID) const;

    /**
     * Find slot containing specific item instance
     * @param ItemInstanceID Instance ID to find
     * @param OutSlotIndex Output slot index if found
     * @return true if item was found
     */
    UFUNCTION(BlueprintPure, Category = "QuickSlot|Query")
    bool FindSlotWithItem(const FGuid& ItemInstanceID, int32& OutSlotIndex) const;

    //==================================================================
    // Magazine Integration
    //==================================================================

    /**
     * Get the magazine instance from a slot (if it contains a magazine)
     * @param SlotIndex Slot index (0-3)
     * @param OutMagazine Output magazine instance
     * @return true if slot contains a valid magazine
     */
    UFUNCTION(BlueprintCallable, Category = "QuickSlot|Magazine")
    bool GetMagazineFromSlot(int32 SlotIndex, FSuspenseCoreMagazineInstance& OutMagazine) const;

    /**
     * Update magazine in slot (after reload, rounds changed, etc.)
     * @param SlotIndex Slot index (0-3)
     * @param UpdatedMagazine Updated magazine instance
     */
    UFUNCTION(BlueprintCallable, Category = "QuickSlot|Magazine")
    void UpdateMagazineInSlot(int32 SlotIndex, const FSuspenseCoreMagazineInstance& UpdatedMagazine);

    /**
     * Store ejected magazine from weapon into first available slot
     * @param EjectedMagazine Magazine that was ejected
     * @param OutSlotIndex Output slot index where magazine was stored
     * @return true if magazine was stored
     */
    UFUNCTION(BlueprintCallable, Category = "QuickSlot|Magazine")
    bool StoreEjectedMagazine(const FSuspenseCoreMagazineInstance& EjectedMagazine, int32& OutSlotIndex);

    //==================================================================
    // Events
    //==================================================================

    /** Fired when a slot's contents change */
    UPROPERTY(BlueprintAssignable, Category = "QuickSlot|Events")
    FOnQuickSlotChanged OnQuickSlotChanged;

    /** Fired when a slot is used */
    UPROPERTY(BlueprintAssignable, Category = "QuickSlot|Events")
    FOnQuickSlotUsed OnQuickSlotUsed;

    /** Fired when cooldown updates */
    UPROPERTY(BlueprintAssignable, Category = "QuickSlot|Events")
    FOnQuickSlotCooldownChanged OnQuickSlotCooldownChanged;

    /** Fired when slot availability changes */
    UPROPERTY(BlueprintAssignable, Category = "QuickSlot|Events")
    FOnQuickSlotAvailabilityChanged OnQuickSlotAvailabilityChanged;

protected:
    //==================================================================
    // Server RPCs
    //==================================================================

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_AssignItemToSlot(int32 SlotIndex, const FGuid& ItemInstanceID, FName ItemID);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_ClearSlot(int32 SlotIndex);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_UseQuickSlot(int32 SlotIndex);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_QuickSwapMagazine(int32 SlotIndex, bool bEmergencyDrop);

    //==================================================================
    // Replication Callbacks
    //==================================================================

    UFUNCTION()
    void OnRep_QuickSlots();

    //==================================================================
    // Internal Methods
    //==================================================================

    /** Validate slot index */
    bool IsValidSlotIndex(int32 SlotIndex) const;

    /** Initialize slots with default values */
    void InitializeSlots();

    /** Update cooldowns for all slots */
    void UpdateCooldowns(float DeltaTime);

    /** Check if item in slot is valid (exists in inventory) */
    bool ValidateSlotItem(int32 SlotIndex) const;

    /** Execute magazine swap with MagazineComponent */
    bool ExecuteMagazineSwap(int32 SlotIndex, bool bEmergencyDrop);

    /** Execute consumable use */
    bool ExecuteConsumableUse(int32 SlotIndex);

    /** Execute grenade preparation */
    bool ExecuteGrenadePrepare(int32 SlotIndex);

    /** Notify slot changed (broadcasts event) */
    void NotifySlotChanged(int32 SlotIndex, const FGuid& OldItemID, const FGuid& NewItemID);

    //==================================================================
    // State
    //==================================================================

    /** Quick slots array (replicated) */
    UPROPERTY(ReplicatedUsing = OnRep_QuickSlots)
    TArray<FSuspenseCoreQuickSlot> QuickSlots;

    /** Stored magazine instances for quick slots */
    UPROPERTY(Replicated)
    TArray<FSuspenseCoreMagazineInstance> StoredMagazines;

    /** Reference to magazine component for fast reloads */
    UPROPERTY()
    TWeakObjectPtr<USuspenseCoreMagazineComponent> MagazineComponent;

    /** Reference to inventory component for item validation */
    UPROPERTY()
    TWeakObjectPtr<USuspenseCoreInventoryComponent> InventoryComponent;

    /** Item category tags for type detection */
    UPROPERTY()
    FGameplayTag MagazineCategoryTag;

    UPROPERTY()
    FGameplayTag ConsumableCategoryTag;

    UPROPERTY()
    FGameplayTag GrenadeCategoryTag;
};
