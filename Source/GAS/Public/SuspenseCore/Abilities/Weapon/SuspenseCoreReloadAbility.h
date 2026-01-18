// SuspenseCoreReloadAbility.h
// Tarkov-style reload ability with magazine management
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Abilities/Base/SuspenseCoreAbility.h"
#include "SuspenseCore/Types/Weapon/SuspenseCoreMagazineTypes.h"
#include "Animation/AnimInstance.h"  // For FBranchingPointNotifyPayload
#include "SuspenseCoreReloadAbility.generated.h"

// Forward declarations - use interfaces instead of concrete types
class ISuspenseCoreMagazineProvider;
class ISuspenseCoreQuickSlotProvider;
class ISuspenseCoreInventory;
class UAnimMontage;

/**
 * USuspenseCoreReloadAbility
 *
 * Tarkov-style reload ability supporting multiple reload types:
 * - Tactical: Swap magazine while round is chambered (fastest)
 * - Empty: Insert magazine and chamber round
 * - Emergency: Drop current magazine, fast insert (loses magazine)
 * - ChamberOnly: Just rack the slide/bolt to chamber round
 *
 * ANIMATION NOTIFY POINTS:
 * - "MagOut": Old magazine detached from weapon
 * - "MagIn": New magazine inserted into weapon
 * - "RackStart": Begin chambering animation
 * - "RackEnd": Round chambered, weapon ready
 *
 * ARCHITECTURE:
 * - Uses ISuspenseCoreMagazineProvider interface (BridgeSystem)
 * - Uses ISuspenseCoreQuickSlotProvider interface (BridgeSystem)
 * - No direct dependency on EquipmentSystem
 *
 * BLOCKING TAGS:
 * - State.Sprinting (can't reload while sprinting)
 * - State.Firing (can't reload while firing)
 * - State.Dead, State.Stunned, State.Disabled
 *
 * @see ISuspenseCoreMagazineProvider
 * @see ISuspenseCoreQuickSlotProvider
 */
UCLASS()
class GAS_API USuspenseCoreReloadAbility : public USuspenseCoreAbility
{
    GENERATED_BODY()

public:
    USuspenseCoreReloadAbility();

    //==================================================================
    // Configuration
    //==================================================================

    /** Montage for tactical reload (magazine swap with chambered round) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Reload|Montages")
    TObjectPtr<UAnimMontage> TacticalReloadMontage;

    /** Montage for empty reload (magazine insert + chamber) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Reload|Montages")
    TObjectPtr<UAnimMontage> EmptyReloadMontage;

    /** Montage for emergency reload (drop mag, fast insert) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Reload|Montages")
    TObjectPtr<UAnimMontage> EmergencyReloadMontage;

    /** Montage for chambering only (no magazine swap) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Reload|Montages")
    TObjectPtr<UAnimMontage> ChamberOnlyMontage;

    /** GameplayEffect for movement speed reduction while reloading */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Reload|Effects")
    TSubclassOf<UGameplayEffect> ReloadSpeedDebuffClass;

    /** Base tactical reload time (modified by weapon/magazine) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Reload|Timing")
    float BaseTacticalReloadTime = 2.0f;

    /** Base empty reload time (modified by weapon/magazine) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Reload|Timing")
    float BaseEmptyReloadTime = 2.5f;

    /** Emergency reload time multiplier (relative to tactical) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Reload|Timing",
        meta = (ClampMin = "0.5", ClampMax = "1.0"))
    float EmergencyReloadTimeMultiplier = 0.8f;

    /** Chamber only time */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Reload|Timing")
    float ChamberOnlyTime = 0.8f;

    //==================================================================
    // Reload Sounds
    //==================================================================

    /** Sound to play when reload starts */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Reload|Sounds")
    TObjectPtr<USoundBase> ReloadStartSound;

    /** Sound to play when magazine is ejected */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Reload|Sounds")
    TObjectPtr<USoundBase> MagOutSound;

    /** Sound to play when new magazine is inserted */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Reload|Sounds")
    TObjectPtr<USoundBase> MagInSound;

    /** Sound to play when bolt/slide is racked */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Reload|Sounds")
    TObjectPtr<USoundBase> ChamberSound;

    /** Sound to play when reload completes successfully */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Reload|Sounds")
    TObjectPtr<USoundBase> ReloadCompleteSound;

    //==================================================================
    // Runtime Accessors
    //==================================================================

    /** Get the current reload type being performed */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Reload")
    ESuspenseCoreReloadType GetCurrentReloadType() const { return CurrentReloadType; }

    /** Get the calculated reload duration */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Reload")
    float GetReloadDuration() const { return ReloadDuration; }

    /** Check if currently reloading */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Reload")
    bool IsReloading() const { return bIsReloading; }

    /** Get reload progress (0.0 - 1.0) */
    UFUNCTION(BlueprintPure, Category = "SuspenseCore|Reload")
    float GetReloadProgress() const;

protected:
    //==================================================================
    // GameplayAbility Interface
    //==================================================================

    virtual bool CanActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags,
        const FGameplayTagContainer* TargetTags,
        FGameplayTagContainer* OptionalRelevantTags
    ) const override;

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData
    ) override;

    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility,
        bool bWasCancelled
    ) override;

    //==================================================================
    // Reload Logic
    //==================================================================

    /**
     * Determine the appropriate reload type based on current state
     * @return Optimal reload type for current weapon/magazine state
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Reload")
    ESuspenseCoreReloadType DetermineReloadType() const;

    /**
     * Calculate reload duration based on type and modifiers
     * @param ReloadType Type of reload being performed
     * @return Total reload duration in seconds
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Reload")
    float CalculateReloadDuration(ESuspenseCoreReloadType ReloadType) const;

    /**
     * Get the appropriate montage for reload type
     * @param ReloadType Type of reload
     * @return Animation montage to play
     */
    UAnimMontage* GetMontageForReloadType(ESuspenseCoreReloadType ReloadType) const;

    /**
     * Find the best available magazine for reload
     * @param OutQuickSlotIndex Output: QuickSlot index containing magazine (-1 if from inventory)
     * @param OutMagazine Output: Magazine instance to use
     * @return true if a suitable magazine was found
     */
    bool FindBestMagazine(int32& OutQuickSlotIndex, FSuspenseCoreMagazineInstance& OutMagazine) const;

    //==================================================================
    // Network RPCs (Multiplayer Support)
    //==================================================================

    /**
     * Server RPC to validate and execute reload
     * @param ReloadType Type of reload to perform
     * @param MagazineSlotIndex QuickSlot index (-1 if from inventory)
     * @param Magazine Magazine instance to use
     */
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_ExecuteReload(ESuspenseCoreReloadType ReloadType, int32 MagazineSlotIndex, const FSuspenseCoreMagazineInstance& Magazine);
    bool Server_ExecuteReload_Validate(ESuspenseCoreReloadType ReloadType, int32 MagazineSlotIndex, const FSuspenseCoreMagazineInstance& Magazine);
    void Server_ExecuteReload_Implementation(ESuspenseCoreReloadType ReloadType, int32 MagazineSlotIndex, const FSuspenseCoreMagazineInstance& Magazine);

    // NOTE: NetMulticast removed - GameplayAbilities are not replicated to Simulated Proxies.
    // Animation replication is handled by UE AnimMontage replication system automatically.
    // Sounds are played locally via notify handlers which fire on each client running the ability.

    //==================================================================
    // Animation Notify Handlers
    //==================================================================

    /** Called when "MagOut" notify fires - old magazine detached */
    UFUNCTION()
    void OnMagOutNotify();

    /** Called when "MagIn" notify fires - new magazine inserted */
    UFUNCTION()
    void OnMagInNotify();

    /** Called when "RackStart" notify fires - begin chambering */
    UFUNCTION()
    void OnRackStartNotify();

    /** Called when "RackEnd" notify fires - round chambered */
    UFUNCTION()
    void OnRackEndNotify();

    /** Called when montage ends (success or interrupted) */
    UFUNCTION()
    void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    /** Called when montage blend out starts */
    UFUNCTION()
    void OnMontageBlendOut(UAnimMontage* Montage, bool bInterrupted);

private:
    //==================================================================
    // Internal Methods (Interface-based)
    //==================================================================

    /** Get MagazineProvider interface from owner */
    ISuspenseCoreMagazineProvider* GetMagazineProvider() const;

    /** Get QuickSlotProvider interface from owner */
    ISuspenseCoreQuickSlotProvider* GetQuickSlotProvider() const;

    /** Get Inventory interface from owner (for magazine search) */
    ISuspenseCoreInventory* GetInventoryProvider() const;

    /** Apply reload effects (speed debuff, etc.) */
    void ApplyReloadEffects(const FGameplayAbilityActorInfo* ActorInfo);

    /** Remove reload effects */
    void RemoveReloadEffects(const FGameplayAbilityActorInfo* ActorInfo);

    /** Start playing reload montage */
    bool PlayReloadMontage();

    /** Stop reload montage */
    void StopReloadMontage();

    /** Broadcast reload started event */
    void BroadcastReloadStarted();

    /** Broadcast reload completed event */
    void BroadcastReloadCompleted();

    /** Broadcast reload cancelled event */
    void BroadcastReloadCancelled();

    /**
     * Execute magazine swap when montage completes
     * Fallback when AnimNotifies (MagOut/MagIn) don't fire in the montage
     */
    void ExecuteReloadOnMontageComplete();

    /**
     * Play reload sound at owner location
     * @param Sound Sound to play
     */
    void PlayReloadSound(USoundBase* Sound);

    //==================================================================
    // Runtime State
    //==================================================================

    /** Current reload type being performed */
    UPROPERTY()
    ESuspenseCoreReloadType CurrentReloadType;

    /** Calculated reload duration */
    UPROPERTY()
    float ReloadDuration;

    /** Time when reload started */
    UPROPERTY()
    float ReloadStartTime;

    /** Is currently reloading */
    UPROPERTY()
    bool bIsReloading;

    /** Magazine being inserted (from QuickSlot or inventory) */
    UPROPERTY()
    FSuspenseCoreMagazineInstance NewMagazine;

    /** QuickSlot index of new magazine (-1 if from inventory) */
    UPROPERTY()
    int32 NewMagazineQuickSlotIndex;

    /** Inventory item instance ID (valid when NewMagazineQuickSlotIndex == -1) */
    /** Mutable for FindBestMagazine which is const but must track found magazine */
    UPROPERTY()
    mutable FGuid NewMagazineInventoryInstanceID;

    /** Ejected magazine (to be stored or dropped) */
    UPROPERTY()
    FSuspenseCoreMagazineInstance EjectedMagazine;

    /** Handle for speed debuff effect */
    FActiveGameplayEffectHandle ReloadSpeedEffectHandle;

    /** Cached ActorInfo */
    const FGameplayAbilityActorInfo* CachedActorInfo;

    /** Cached ability handle */
    FGameplayAbilitySpecHandle CachedSpecHandle;

    /** Cached activation info */
    FGameplayAbilityActivationInfo CachedActivationInfo;

    /** Cached MagazineProvider interface - avoids repeated lookups */
    mutable TWeakObjectPtr<UObject> CachedMagazineProvider;

    /** Cached QuickSlotProvider interface - avoids repeated lookups */
    mutable TWeakObjectPtr<UObject> CachedQuickSlotProvider;

    /** Cached InventoryProvider interface - avoids repeated lookups */
    mutable TWeakObjectPtr<UObject> CachedInventoryProvider;

    /** Cached AnimInstance for montage notify binding */
    UPROPERTY()
    TWeakObjectPtr<UAnimInstance> CachedAnimInstance;

    /** Handles montage AnimNotify events and dispatches to phase handlers */
    UFUNCTION()
    void OnAnimNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);
};
