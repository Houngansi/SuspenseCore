// SuspenseCoreQuickSlotComponent.h
// QuickSlot system for fast magazine/item access
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCoreEquipmentComponentBase.h"
#include "SuspenseCore/Types/Weapon/SuspenseCoreMagazineTypes.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h" // SUSPENSECORE_QUICKSLOT_COUNT
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreQuickSlotProvider.h"
#include "SuspenseCoreQuickSlotComponent.generated.h"

// Forward declarations
class USuspenseCoreMagazineComponent;

// SUSPENSECORE_QUICKSLOT_COUNT is now defined in SuspenseCoreTypes.h

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
 * Implements ISuspenseCoreQuickSlotProvider for GAS ability access.
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
 *   // Fast reload with slot 1 magazine (via interface)
 *   ISuspenseCoreQuickSlotProvider::Execute_UseQuickSlot(QuickSlotComp, 0);
 *
 * @see USuspenseCoreMagazineComponent
 * @see FSuspenseCoreQuickSlot
 * @see ISuspenseCoreQuickSlotProvider
 */
UCLASS(ClassGroup=(SuspenseCore), meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseCoreQuickSlotComponent : public USuspenseCoreEquipmentComponentBase, public ISuspenseCoreQuickSlotProvider
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


    //==================================================================
    // Slot Assignment (Component-specific, not in interface)
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
    // Slot Usage (Component-specific helpers)
    //==================================================================

    /**
     * Start cooldown on a slot (called after use)
     * @param SlotIndex Slot index (0-3)
     * @param CooldownDuration Duration in seconds
     */
    UFUNCTION(BlueprintCallable, Category = "QuickSlot|Usage")
    void StartSlotCooldown(int32 SlotIndex, float CooldownDuration);

    /**
     * Consume one use from a consumable in the slot
     * @param SlotIndex Slot index (0-3)
     * @return true if uses remain (slot still valid), false if depleted (slot will be cleared)
     */
    UFUNCTION(BlueprintCallable, Category = "QuickSlot|Usage")
    bool ConsumeSlotUse(int32 SlotIndex);

    /**
     * Get remaining uses for a consumable in the slot
     * @param SlotIndex Slot index (0-3)
     * @return Remaining uses (-1 if not a multi-use consumable, 0 if depleted)
     */
    UFUNCTION(BlueprintPure, Category = "QuickSlot|Query")
    int32 GetSlotRemainingUses(int32 SlotIndex) const;

    //==================================================================
    // Queries (Component-specific, not in interface)
    //==================================================================

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
    // Magazine Integration (Component-specific)
    //==================================================================

    /**
     * Update magazine in slot (after reload, rounds changed, etc.)
     * @param SlotIndex Slot index (0-3)
     * @param UpdatedMagazine Updated magazine instance
     */
    UFUNCTION(BlueprintCallable, Category = "QuickSlot|Magazine")
    void UpdateMagazineInSlot(int32 SlotIndex, const FSuspenseCoreMagazineInstance& UpdatedMagazine);

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
    // ISuspenseCoreQuickSlotProvider Interface Implementation
    // Note: Public methods are provided by the interface via BlueprintNativeEvent
    //==================================================================

    virtual FSuspenseCoreQuickSlot GetQuickSlot_Implementation(int32 SlotIndex) const override;
    virtual bool IsSlotReady_Implementation(int32 SlotIndex) const override;
    virtual bool HasItemInSlot_Implementation(int32 SlotIndex) const override;
    virtual bool UseQuickSlot_Implementation(int32 SlotIndex) override;
    virtual bool QuickSwapMagazine_Implementation(int32 SlotIndex, bool bEmergencyDrop) override;
    virtual bool GetMagazineFromSlot_Implementation(int32 SlotIndex, FSuspenseCoreMagazineInstance& OutMagazine) const override;
    virtual bool GetFirstMagazineSlotIndex_Implementation(int32& OutSlotIndex) const override;
    virtual bool StoreEjectedMagazine_Implementation(const FSuspenseCoreMagazineInstance& EjectedMagazine, int32& OutSlotIndex) override;
    virtual void ClearSlot_Implementation(int32 SlotIndex) override;

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

    /**
     * Direct execution fallback when ItemUseService is not available.
     * NOTE: Only magazine swap is supported in fallback mode.
     * Consumables/grenades require ItemUseService (GAS handlers).
     */
    bool ExecuteQuickSlotDirect(int32 SlotIndex);

    /** Notify slot changed (broadcasts event) */
    void NotifySlotChanged(int32 SlotIndex, const FGuid& OldItemID, const FGuid& NewItemID);

    /** Internal clear slot logic */
    void ClearSlotInternal(int32 SlotIndex);

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

    // TODO: Add inventory interface when InventorySystem is integrated
    // For now, item validation is done locally

    /** Item category tags for type detection */
    UPROPERTY()
    FGameplayTag MagazineCategoryTag;

    UPROPERTY()
    FGameplayTag ConsumableCategoryTag;

    UPROPERTY()
    FGameplayTag GrenadeCategoryTag;
};
