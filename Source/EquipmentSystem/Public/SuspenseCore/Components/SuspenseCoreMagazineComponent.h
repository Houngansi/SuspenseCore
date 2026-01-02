// SuspenseCoreMagazineComponent.h
// Tarkov-style magazine management component
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Components/SuspenseCoreEquipmentComponentBase.h"
#include "SuspenseCore/Types/Weapon/SuspenseCoreMagazineTypes.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeapon.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreMagazineProvider.h"
#include "SuspenseCoreMagazineComponent.generated.h"

// Forward declarations
class USuspenseCoreDataManager;
class USuspenseCoreWeaponAttributeSet;
class USuspenseCoreAmmoAttributeSet;
class USuspenseCoreEquipmentAttributeComponent;

/**
 * Delegate for magazine state changes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMagazineStateChanged, const FSuspenseCoreWeaponAmmoState&, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMagazineChanged, const FSuspenseCoreMagazineInstance&, OldMagazine, const FSuspenseCoreMagazineInstance&, NewMagazine);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChamberStateChanged, bool, bHasChamberedRound);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnReloadStateChanged, bool, bIsReloading, ESuspenseCoreReloadType, ReloadType);

/**
 * Component that manages Tarkov-style magazine system
 *
 * Features:
 * - Physical magazine tracking (not just ammo counters)
 * - Single ammo type per magazine
 * - Chamber state management
 * - Multiple reload types (tactical, empty, emergency)
 * - Magazine swap with dropped magazine tracking
 * - Integration with QuickSlots and inventory
 *
 * @see FSuspenseCoreMagazineInstance
 * @see FSuspenseCoreWeaponAmmoState
 * @see Documentation/Plans/TarkovStyle_Ammo_System_Design.md
 */
UCLASS(ClassGroup=(Equipment), meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseCoreMagazineComponent : public USuspenseCoreEquipmentComponentBase, public ISuspenseCoreMagazineProvider
{
    GENERATED_BODY()

public:
    USuspenseCoreMagazineComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void Cleanup() override;

    //================================================
    // Initialization
    //================================================

    /**
     * Initialize from weapon interface
     * @param WeaponInterface Weapon that owns this component
     * @param InitialMagazineID Optional initial magazine to insert
     * @param InitialAmmoID Optional ammo type to load
     * @param InitialRounds Number of rounds to load
     * @return Success of initialization
     */
    UFUNCTION(BlueprintCallable, Category = "Magazine")
    bool InitializeFromWeapon(
        TScriptInterface<ISuspenseCoreWeapon> WeaponInterface,
        FName InitialMagazineID = NAME_None,
        FName InitialAmmoID = NAME_None,
        int32 InitialRounds = 0);

    //================================================
    // Magazine Operations
    //================================================

    /**
     * Insert a magazine into the weapon
     * @param Magazine Magazine instance to insert
     * @return true if magazine was inserted
     */
    UFUNCTION(BlueprintCallable, Category = "Magazine")
    bool InsertMagazine(const FSuspenseCoreMagazineInstance& Magazine);

    /**
     * Eject current magazine from weapon
     * @param bDropToGround If true, spawns pickup in world; if false, returns to inventory
     * @return Ejected magazine instance (empty if no magazine was inserted)
     */
    UFUNCTION(BlueprintCallable, Category = "Magazine")
    FSuspenseCoreMagazineInstance EjectMagazine(bool bDropToGround = false);

    /**
     * Swap magazine with one from QuickSlot
     * @param QuickSlotIndex QuickSlot to use (0-3)
     * @param bEmergencyDrop If true, drops current magazine instead of returning to slot
     * @return true if swap succeeded
     */
    UFUNCTION(BlueprintCallable, Category = "Magazine")
    bool SwapMagazineFromQuickSlot(int32 QuickSlotIndex, bool bEmergencyDrop = false);

    //================================================
    // Chamber Operations
    //================================================

    /**
     * Chamber a round from the magazine
     * @return true if round was chambered
     */
    UFUNCTION(BlueprintCallable, Category = "Magazine|Chamber")
    bool ChamberRound();

    /**
     * Eject the chambered round without firing
     * @return The ejected round data
     */
    UFUNCTION(BlueprintCallable, Category = "Magazine|Chamber")
    FSuspenseCoreChamberedRound EjectChamberedRound();

    /**
     * Fire the weapon (consumes chambered round)
     * @param bAutoChamber If true, automatically chambers next round
     * @return AmmoID of fired round, NAME_None if couldn't fire
     */
    UFUNCTION(BlueprintCallable, Category = "Magazine|Chamber")
    FName Fire(bool bAutoChamber = true);

    //================================================
    // Reload Operations
    //================================================

    /**
     * Start reload process
     * @param Request Reload request parameters
     * @return true if reload started
     */
    UFUNCTION(BlueprintCallable, Category = "Magazine|Reload")
    bool StartReload(const FSuspenseCoreReloadRequest& Request);

    /**
     * Determine best reload type based on current state
     * @param AvailableMagazine Magazine that would be used (optional)
     * @return Recommended reload type
     */
    UFUNCTION(BlueprintPure, Category = "Magazine|Reload")
    ESuspenseCoreReloadType DetermineReloadType(const FSuspenseCoreMagazineInstance& AvailableMagazine) const;

    /**
     * Calculate reload duration for given type
     * @param ReloadType Type of reload
     * @param MagazineData Magazine data (for modifiers)
     * @return Reload duration in seconds
     */
    UFUNCTION(BlueprintPure, Category = "Magazine|Reload")
    float CalculateReloadDuration(ESuspenseCoreReloadType ReloadType, const FSuspenseCoreMagazineData& MagazineData) const;

    /**
     * Complete reload process (called by animation notify or timer)
     */
    UFUNCTION(BlueprintCallable, Category = "Magazine|Reload")
    void CompleteReload();

    /**
     * Cancel reload in progress
     */
    UFUNCTION(BlueprintCallable, Category = "Magazine|Reload")
    void CancelReload();

    /**
     * Check if can reload
     * @param NewMagazine Magazine to reload with (optional check)
     * @return true if reload possible
     */
    UFUNCTION(BlueprintPure, Category = "Magazine|Reload")
    bool CanReload(const FSuspenseCoreMagazineInstance& NewMagazine = FSuspenseCoreMagazineInstance()) const;

    //================================================
    // State Queries
    //================================================

    /** Get complete weapon ammo state */
    UFUNCTION(BlueprintPure, Category = "Magazine|State")
    const FSuspenseCoreWeaponAmmoState& GetWeaponAmmoState() const { return WeaponAmmoState; }

    /** Get current magazine instance */
    UFUNCTION(BlueprintPure, Category = "Magazine|State")
    const FSuspenseCoreMagazineInstance& GetCurrentMagazine() const { return WeaponAmmoState.InsertedMagazine; }

    /** Get chambered round */
    UFUNCTION(BlueprintPure, Category = "Magazine|State")
    const FSuspenseCoreChamberedRound& GetChamberedRound() const { return WeaponAmmoState.ChamberedRound; }

    /** Check if weapon has magazine inserted */
    UFUNCTION(BlueprintPure, Category = "Magazine|State")
    bool HasMagazine() const { return WeaponAmmoState.bHasMagazine; }

    /** Check if weapon has chambered round */
    UFUNCTION(BlueprintPure, Category = "Magazine|State")
    bool HasChamberedRound() const { return WeaponAmmoState.ChamberedRound.IsChambered(); }

    /** Check if weapon is ready to fire */
    UFUNCTION(BlueprintPure, Category = "Magazine|State")
    bool IsReadyToFire() const { return WeaponAmmoState.IsReadyToFire(); }

    /** Check if magazine is empty */
    UFUNCTION(BlueprintPure, Category = "Magazine|State")
    bool IsMagazineEmpty() const { return WeaponAmmoState.IsMagazineEmpty(); }

    /** Check if currently reloading */
    UFUNCTION(BlueprintPure, Category = "Magazine|State")
    bool IsReloading() const { return bIsReloading; }

    /** Get current reload type */
    UFUNCTION(BlueprintPure, Category = "Magazine|State")
    ESuspenseCoreReloadType GetCurrentReloadType() const { return CurrentReloadType; }

    /** Get reload progress (0.0 - 1.0) */
    UFUNCTION(BlueprintPure, Category = "Magazine|State")
    float GetReloadProgress() const;

    /** Get current ammo in magazine */
    UFUNCTION(BlueprintPure, Category = "Magazine|State")
    int32 GetMagazineRoundCount() const;

    /** Get magazine capacity */
    UFUNCTION(BlueprintPure, Category = "Magazine|State")
    int32 GetMagazineCapacity() const;

    /** Get total rounds (chamber + magazine) */
    UFUNCTION(BlueprintPure, Category = "Magazine|State")
    int32 GetTotalRounds() const { return WeaponAmmoState.GetTotalRounds(); }

    /** Get current ammo type in magazine */
    UFUNCTION(BlueprintPure, Category = "Magazine|State")
    FName GetLoadedAmmoType() const;

    //================================================
    // Save/Load State
    //================================================

    /**
     * Get serializable state for saving
     * @return Complete ammo state
     */
    UFUNCTION(BlueprintCallable, Category = "Magazine|Persistence")
    FSuspenseCoreWeaponAmmoState GetSerializableState() const { return WeaponAmmoState; }

    /**
     * Restore state from saved data
     * @param SavedState State to restore
     */
    UFUNCTION(BlueprintCallable, Category = "Magazine|Persistence")
    void RestoreState(const FSuspenseCoreWeaponAmmoState& SavedState);

    //================================================
    // Events
    //================================================

    /** Fired when magazine state changes (insert, eject, fire, reload) */
    UPROPERTY(BlueprintAssignable, Category = "Magazine|Events")
    FOnMagazineStateChanged OnMagazineStateChanged;

    /** Fired when magazine is inserted or ejected */
    UPROPERTY(BlueprintAssignable, Category = "Magazine|Events")
    FOnMagazineChanged OnMagazineChanged;

    /** Fired when chamber state changes */
    UPROPERTY(BlueprintAssignable, Category = "Magazine|Events")
    FOnChamberStateChanged OnChamberStateChanged;

    /** Fired when reload state changes */
    UPROPERTY(BlueprintAssignable, Category = "Magazine|Events")
    FOnReloadStateChanged OnReloadStateChanged;

protected:
    //================================================
    // ISuspenseCoreMagazineProvider Interface Implementation
    //================================================

    virtual FSuspenseCoreWeaponAmmoState GetAmmoState_Implementation() const override;
    virtual bool HasMagazine_Implementation() const override;
    virtual bool IsReadyToFire_Implementation() const override;
    virtual bool IsReloading_Implementation() const override;
    virtual bool InsertMagazine_Implementation(const FSuspenseCoreMagazineInstance& Magazine) override;
    virtual FSuspenseCoreMagazineInstance EjectMagazine_Implementation(bool bDropToGround) override;
    virtual bool ChamberRound_Implementation() override;
    virtual FSuspenseCoreChamberedRound EjectChamberedRound_Implementation() override;
    virtual ESuspenseCoreReloadType DetermineReloadType_Implementation() const override;
    virtual float CalculateReloadDuration_Implementation(ESuspenseCoreReloadType ReloadType, const FSuspenseCoreMagazineInstance& NewMagazine) const override;
    virtual void NotifyReloadStateChanged_Implementation(bool bIsReloading, ESuspenseCoreReloadType ReloadType, float Duration) override;

    //================================================
    // Internal Operations
    //================================================

    /** Get weapon interface from owner */
    ISuspenseCoreWeapon* GetWeaponInterface() const;

    /** Get DataManager for magazine data */
    USuspenseCoreDataManager* GetDataManager() const;

    /** Broadcast state change events */
    void BroadcastStateChanged();

    /** Apply magazine modifier to weapon (ergonomics penalty, etc.) */
    void ApplyMagazineModifiers();

    /** Remove magazine modifier from weapon */
    void RemoveMagazineModifiers();

    /** Handle reload completion logic */
    void ProcessReloadCompletion();

    //================================================
    // Server RPCs
    //================================================

    UFUNCTION(Server, Reliable, WithValidation)
    void ServerInsertMagazine(const FSuspenseCoreMagazineInstance& Magazine);

    UFUNCTION(Server, Reliable, WithValidation)
    void ServerEjectMagazine(bool bDropToGround);

    UFUNCTION(Server, Reliable, WithValidation)
    void ServerFire(bool bAutoChamber);

    UFUNCTION(Server, Reliable, WithValidation)
    void ServerStartReload(const FSuspenseCoreReloadRequest& Request);

    UFUNCTION(Server, Reliable, WithValidation)
    void ServerCompleteReload();

    UFUNCTION(Server, Reliable, WithValidation)
    void ServerCancelReload();

    //================================================
    // Replication
    //================================================

    UFUNCTION()
    void OnRep_WeaponAmmoState();

    UFUNCTION()
    void OnRep_ReloadState();

private:
    //================================================
    // Replicated State
    //================================================

    /** Complete weapon ammo state (magazine + chamber) */
    UPROPERTY(ReplicatedUsing = OnRep_WeaponAmmoState)
    FSuspenseCoreWeaponAmmoState WeaponAmmoState;

    /** Is reload in progress */
    UPROPERTY(ReplicatedUsing = OnRep_ReloadState)
    bool bIsReloading = false;

    /** Current reload type */
    UPROPERTY(Replicated)
    ESuspenseCoreReloadType CurrentReloadType = ESuspenseCoreReloadType::None;

    /** Reload start time (for progress calculation) */
    UPROPERTY(Replicated)
    float ReloadStartTime = 0.0f;

    /** Reload duration (for progress calculation) */
    UPROPERTY(Replicated)
    float ReloadDuration = 0.0f;

    /** Magazine being reloaded with (for completion) */
    UPROPERTY(Replicated)
    FSuspenseCoreMagazineInstance PendingMagazine;

    //================================================
    // Cached Data
    //================================================

    /** Cached weapon interface */
    UPROPERTY()
    TScriptInterface<ISuspenseCoreWeapon> CachedWeaponInterface;

    /** Cached weapon caliber tag */
    UPROPERTY()
    FGameplayTag CachedWeaponCaliber;

    /** Cached weapon type tag */
    UPROPERTY()
    FGameplayTag CachedWeaponType;

    /** Magazine data for currently inserted magazine */
    FSuspenseCoreMagazineData CachedMagazineData;

    /** Is magazine data cached */
    bool bMagazineDataCached = false;
};
