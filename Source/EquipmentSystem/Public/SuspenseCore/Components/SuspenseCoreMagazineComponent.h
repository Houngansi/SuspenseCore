// SuspenseCoreMagazineComponent.h
// Tarkov-style magazine management component
// Copyright Suspense Team. All Rights Reserved.
//
// ARCHITECTURE NOTE:
// This component implements the Tarkov-style physical magazine system with:
// - Unique magazine instances (FGuid-based tracking)
// - Chamber state management
// - Multiple reload types (tactical, empty, emergency, chamber-only)
//
// RELATIONSHIP WITH WeaponAmmoComponent:
// - MagazineComponent (THIS): Full Tarkov-style system with physical magazines,
//   durability, caliber compatibility - used for realistic MMO FPS games
// - WeaponAmmoComponent: Simplified arcade-style counter system with GAS integration -
//   can be used for casual shooters or as fallback
// Both implement different strategies but can coexist on WeaponActor.
// Currently, only MagazineComponent is used for persistence in SaveWeaponState/RestoreWeaponState.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Components/SuspenseCoreEquipmentComponentBase.h"
#include "SuspenseCore/Types/Weapon/SuspenseCoreMagazineTypes.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h" // SUSPENSECORE_QUICKSLOT_COUNT
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeapon.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreMagazineProvider.h"
#include "SuspenseCoreMagazineComponent.generated.h"

// Forward declarations
class USuspenseCoreDataManager;
class USuspenseCoreWeaponAttributeSet;
class USuspenseCoreAmmoAttributeSet;
class USuspenseCoreEquipmentAttributeComponent;
class USuspenseCoreQuickSlotComponent;
class UAbilitySystemComponent;

/**
 * Delegate for magazine state changes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMagazineStateChanged, const FSuspenseCoreWeaponAmmoState&, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMagazineChanged, const FSuspenseCoreMagazineInstance&, OldMagazine, const FSuspenseCoreMagazineInstance&, NewMagazine);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChamberStateChanged, bool, bHasChamberedRound);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnReloadStateChanged, bool, bIsReloading, ESuspenseCoreReloadType, ReloadType);

/**
 * Client prediction data for reload operations
 * Used for optimistic UI updates with rollback capability
 */
USTRUCT()
struct FSuspenseCoreMagazinePredictionData
{
    GENERATED_BODY()

    /** Unique prediction key for this operation */
    UPROPERTY()
    int32 PredictionKey = 0;

    /** Predicted reload type */
    UPROPERTY()
    ESuspenseCoreReloadType PredictedReloadType = ESuspenseCoreReloadType::None;

    /** Predicted reload duration */
    UPROPERTY()
    float PredictedDuration = 0.0f;

    /** Predicted pending magazine */
    UPROPERTY()
    FSuspenseCoreMagazineInstance PredictedMagazine;

    /** State snapshot before prediction (for rollback) */
    UPROPERTY()
    FSuspenseCoreWeaponAmmoState StateBeforePrediction;

    /** Time when prediction was made */
    UPROPERTY()
    float PredictionTimestamp = 0.0f;

    /** Whether this prediction is still active (not confirmed/rejected) */
    UPROPERTY()
    bool bIsActive = false;

    FSuspenseCoreMagazinePredictionData() = default;

    bool IsValid() const { return PredictionKey != 0 && bIsActive; }
    void Invalidate() { bIsActive = false; }
};

/**
 * Component that manages Tarkov-style magazine system
 * Implements ISuspenseCoreMagazineProvider for GAS ability access.
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
 * @see ISuspenseCoreMagazineProvider
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
    // Magazine Operations (Component-specific)
    //================================================

    /**
     * Swap magazine with one from QuickSlot
     * @param QuickSlotIndex QuickSlot to use (0-3)
     * @param bEmergencyDrop If true, drops current magazine instead of returning to slot
     * @return true if swap succeeded
     */
    UFUNCTION(BlueprintCallable, Category = "Magazine")
    bool SwapMagazineFromQuickSlot(int32 QuickSlotIndex, bool bEmergencyDrop = false);

    //================================================
    // Chamber Operations (Component-specific)
    //================================================

    /**
     * Fire the weapon (consumes chambered round)
     * @param bAutoChamber If true, automatically chambers next round
     * @return AmmoID of fired round, NAME_None if couldn't fire
     */
    UFUNCTION(BlueprintCallable, Category = "Magazine|Chamber")
    FName Fire(bool bAutoChamber = true);

    //================================================
    // Reload Operations (Component-specific)
    //================================================

    /**
     * Start reload process
     * @param Request Reload request parameters
     * @return true if reload started
     */
    UFUNCTION(BlueprintCallable, Category = "Magazine|Reload")
    bool StartReload(const FSuspenseCoreReloadRequest& Request);

    /**
     * Determine best reload type based on current state (with available magazine)
     * @param AvailableMagazine Magazine that would be used
     * @return Recommended reload type
     */
    UFUNCTION(BlueprintPure, Category = "Magazine|Reload")
    ESuspenseCoreReloadType DetermineReloadTypeForMagazine(const FSuspenseCoreMagazineInstance& AvailableMagazine) const;

    /**
     * Calculate reload duration for given type (with magazine data)
     * @param ReloadType Type of reload
     * @param MagazineData Magazine data (for modifiers)
     * @return Reload duration in seconds
     */
    UFUNCTION(BlueprintPure, Category = "Magazine|Reload")
    float CalculateReloadDurationWithData(ESuspenseCoreReloadType ReloadType, const FSuspenseCoreMagazineData& MagazineData) const;

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
    // State Queries (Component-specific, non-interface)
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

    /** Check if weapon has chambered round */
    UFUNCTION(BlueprintPure, Category = "Magazine|State")
    bool HasChamberedRound() const { return WeaponAmmoState.ChamberedRound.IsChambered(); }

    /** Check if magazine is empty */
    UFUNCTION(BlueprintPure, Category = "Magazine|State")
    bool IsMagazineEmpty() const { return WeaponAmmoState.IsMagazineEmpty(); }

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
    // Client Prediction (Optimistic UI)
    //================================================

    /**
     * Start predicted reload on client (optimistic UI update)
     * Creates local prediction that will be confirmed/rejected by server
     * @param Request Reload request parameters
     * @return Prediction key (0 if prediction failed to start)
     */
    UFUNCTION(BlueprintCallable, Category = "Magazine|Prediction")
    int32 PredictStartReload(const FSuspenseCoreReloadRequest& Request);

    /**
     * Confirm client prediction (called when server confirms)
     * @param PredictionKey Key returned from PredictStartReload
     */
    UFUNCTION(BlueprintCallable, Category = "Magazine|Prediction")
    void ConfirmPrediction(int32 PredictionKey);

    /**
     * Rollback client prediction (called when server rejects)
     * Restores state to pre-prediction snapshot
     * @param PredictionKey Key returned from PredictStartReload
     */
    UFUNCTION(BlueprintCallable, Category = "Magazine|Prediction")
    void RollbackPrediction(int32 PredictionKey);

    /**
     * Check if there's an active prediction
     * @return true if client has unconfirmed prediction
     */
    UFUNCTION(BlueprintPure, Category = "Magazine|Prediction")
    bool HasActivePrediction() const { return CurrentPrediction.IsValid(); }

    /**
     * Get current prediction key
     * @return Active prediction key, or 0 if none
     */
    UFUNCTION(BlueprintPure, Category = "Magazine|Prediction")
    int32 GetActivePredictionKey() const { return CurrentPrediction.PredictionKey; }

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
    // Note: Public methods are provided by the interface via BlueprintNativeEvent
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
    virtual void NotifyReloadStateChanged_Implementation(bool bInIsReloading, ESuspenseCoreReloadType ReloadType, float Duration) override;
    virtual bool IsMagazineCompatible_Implementation(const FSuspenseCoreMagazineInstance& Magazine) const override;
    virtual FGameplayTag GetWeaponCaliber_Implementation() const override;

    //================================================
    // Internal Operations
    //================================================

    /** Get weapon interface from owner */
    ISuspenseCoreWeapon* GetWeaponInterface() const;

    /** Get DataManager for magazine data */
    USuspenseCoreDataManager* GetDataManager() const;

    /**
     * Get cached WeaponAttributeSet from owner's ASC
     * Caches the result for performance
     * @return WeaponAttributeSet or nullptr if not available
     */
    const USuspenseCoreWeaponAttributeSet* GetCachedWeaponAttributeSet() const;

    /**
     * Get AbilitySystemComponent from owner
     * @return ASC or nullptr
     */
    UAbilitySystemComponent* GetOwnerASC() const;

    /** Broadcast state change events */
    void BroadcastStateChanged();

    /** Apply magazine modifier to weapon (ergonomics penalty, etc.) */
    void ApplyMagazineModifiers();

    /** Remove magazine modifier from weapon */
    void RemoveMagazineModifiers();

    /** Handle reload completion logic */
    void ProcessReloadCompletion();

    /** Internal insert magazine logic */
    bool InsertMagazineInternal(const FSuspenseCoreMagazineInstance& Magazine);

    /** Internal eject magazine logic */
    FSuspenseCoreMagazineInstance EjectMagazineInternal(bool bDropToGround);

    /** Internal chamber round logic */
    bool ChamberRoundInternal();

    /** Internal eject chambered round logic */
    FSuspenseCoreChamberedRound EjectChamberedRoundInternal();

    //================================================
    // Client Prediction Internal
    //================================================

    /** Generate next prediction key */
    int32 GeneratePredictionKey();

    /** Apply prediction locally (optimistic update) */
    void ApplyPredictionLocally(const FSuspenseCoreMagazinePredictionData& Prediction);

    /** Clean up expired predictions */
    void CleanupExpiredPredictions();

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

    UFUNCTION(Server, Reliable, WithValidation)
    void ServerSwapMagazineFromQuickSlot(int32 QuickSlotIndex, bool bEmergencyDrop);

    //================================================
    // Client RPCs (Prediction Confirmation)
    //================================================

    /**
     * Server confirms client prediction
     * @param PredictionKey Key to confirm
     * @param bSuccess Whether prediction was correct
     */
    UFUNCTION(Client, Reliable)
    void ClientConfirmReloadPrediction(int32 PredictionKey, bool bSuccess);

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

    //================================================
    // Cached GAS References
    //================================================

    /** Cached WeaponAttributeSet for reload time calculations */
    mutable TWeakObjectPtr<const USuspenseCoreWeaponAttributeSet> CachedWeaponAttributeSet;

    /** Whether we've attempted to cache the AttributeSet */
    mutable bool bAttributeSetCacheAttempted = false;

    //================================================
    // Client Prediction State
    //================================================

    /** Current active prediction (only one at a time) */
    FSuspenseCoreMagazinePredictionData CurrentPrediction;

    /** Next prediction key to assign */
    int32 NextPredictionKey = 1;

    /** Maximum time to wait for server confirmation before auto-rollback (seconds) */
    static constexpr float PredictionTimeoutSeconds = 3.0f;
};
