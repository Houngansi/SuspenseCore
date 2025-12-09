// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Components/SuspenseCoreEquipmentComponentBase.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/Weapon/SuspenseCoreInventoryAmmoState.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeapon.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreItemDataTable.h"
#include "SuspenseCore/Attributes/SuspenseCoreWeaponAttributeSet.h"
#include "SuspenseCore/Attributes/SuspenseCoreAmmoAttributeSet.h"
#include "SuspenseCoreWeaponAmmoComponent.generated.h"

// Forward declarations
class UGameplayEffect;
class UAttributeSet;

/**
 * Component that manages weapon ammunition state
 *
 * VERSION 5.0 - FULL GAS INTEGRATION:
 * - All weapon characteristics retrieved through AttributeSets
 * - Magazine size and reload time from WeaponAttributeSet
 * - Ammo properties from AmmoAttributeSet
 * - Seamless integration with EquipmentAttributeComponent
 * - No direct DataTable field access for runtime values
 */
UCLASS(ClassGroup=(Equipment), meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseCoreWeaponAmmoComponent : public USuspenseCoreEquipmentComponentBase
{
    GENERATED_BODY()

public:
    USuspenseCoreWeaponAmmoComponent();
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void Cleanup() override;

    //================================================
    // Initialization
    //================================================

    /**
     * Initialize from weapon interface
     * Gets all configuration from DataTable via weapon
     * @param WeaponInterface Weapon that owns this component
     * @return Success of initialization
     */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
    bool InitializeFromWeapon(TScriptInterface<ISuspenseCoreWeapon> WeaponInterface);

    //================================================
    // Core Ammo Operations
    //================================================

    /**
     * Consume ammo from magazine
     * @param Amount Amount to consume
     * @return True if successfully consumed
     */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
    bool ConsumeAmmo(float Amount = 1.0f);

    /**
     * Add ammo to reserve
     * @param Amount Amount to add
     * @return Actual amount added (may be less due to limits)
     */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
    float AddAmmo(float Amount);

    /**
     * Start reload process
     * @param bForce Force reload even if magazine not empty
     * @return True if reload started
     */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
    bool StartReload(bool bForce = false);

    /**
     * Complete reload process
     * Called by ability system when reload finishes
     */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
    void CompleteReload();

    /**
     * Cancel reload in progress
     */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
    void CancelReload();

    //================================================
    // State Queries
    //================================================

    /**
     * Get current ammo state
     * @return Complete ammo state for saving/loading
     */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
    FSuspenseCoreInventoryAmmoState GetAmmoState() const { return AmmoState; }

    /**
     * Set ammo state (for loading)
     * @param NewState State to apply
     */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
    void SetAmmoState(const FSuspenseCoreInventoryAmmoState& NewState);

    /**
     * Check if can reload
     * @return True if reload possible
     */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
    bool CanReload() const;

    /**
     * Check if has ammo in magazine
     * @return True if has ammo to fire
     */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
    bool HasAmmo() const;

    /**
     * Check if magazine is full
     * @return True if at capacity
     */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
    bool IsMagazineFull() const;

    /**
     * Check if currently reloading
     * @return True if reload in progress
     */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
    bool IsReloading() const { return bIsReloading; }

    /**
     * Get magazine size from weapon AttributeSet
     * @return Magazine capacity
     */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
    float GetMagazineSize() const;

    /**
     * Get reload time from weapon AttributeSet
     * @param bTactical True for tactical reload, false for full reload
     * @return Reload duration in seconds
     */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
    float GetReloadTime(bool bTactical = true) const;

    /**
     * Get ammo type from weapon data
     * @return Ammo type tag
     */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
    FGameplayTag GetAmmoType() const;

    //================================================
    // Convenience Accessors
    //================================================

    UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
    float GetCurrentAmmo() const { return AmmoState.CurrentAmmo; }

    UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
    float GetRemainingAmmo() const { return AmmoState.RemainingAmmo; }

    UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
    float GetTotalAmmo() const { return AmmoState.GetTotalAmmo(); }

    UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
    bool IsAmmoEmpty() const { return AmmoState.IsEmpty(); }

    //================================================
    // AttributeSet Integration
    //================================================

    /**
     * Link to attribute component for GAS integration
     * @param AttributeComponent Equipment attribute component
     */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo|GAS")
    void LinkAttributeComponent(class USuspenseCoreEquipmentAttributeComponent* AttributeComponent);

    /**
     * Get linked weapon AttributeSet
     * @return Weapon AttributeSet or null
     */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo|GAS")
    USuspenseCoreWeaponAttributeSet* GetWeaponAttributeSet() const;

    /**
     * Get linked ammo AttributeSet
     * @return Ammo AttributeSet or null
     */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo|GAS")
    USuspenseCoreAmmoAttributeSet* GetAmmoAttributeSet() const;

protected:
    /**
     * Get weapon interface from owner
     * @return Weapon interface or null
     */
    ISuspenseCoreWeapon* GetWeaponInterface() const;

    /**
     * Get weapon data from DataTable
     * @param OutData Output weapon data
     * @return True if data retrieved
     */
    bool GetWeaponData(FSuspenseCoreUnifiedItemData& OutData) const;

    /**
     * Broadcast ammo change event
     */
    void BroadcastAmmoChanged();

    /**
     * Apply reload effect if configured
     */
    void ApplyReloadEffect();

    /**
     * Update magazine size from attributes
     */
    void UpdateMagazineSizeFromAttributes();

    /**
     * Handle weapon durability impact on ammo operations
     */
    void ApplyDurabilityModifiers();

    /**
     * Server RPC for reload
     */
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerStartReload(bool bForce);

    /**
     * Server RPC for completing reload
     */
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerCompleteReload();

    //================================================
    // Replication Callbacks
    //================================================

    UFUNCTION()
    void OnRep_AmmoState();

    UFUNCTION()
    void OnRep_ReloadState();

    // Internal method to update ammo state without triggering callbacks
    void UpdateInternalAmmoState(const FSuspenseCoreInventoryAmmoState& NewState);

private:
    //================================================
    // Runtime State (Replicated)
    //================================================

    /** Current ammunition state */
    UPROPERTY(ReplicatedUsing = OnRep_AmmoState)
    FSuspenseCoreInventoryAmmoState AmmoState;

    /** Reload in progress */
    UPROPERTY(ReplicatedUsing = OnRep_ReloadState)
    bool bIsReloading;

    /** Time when reload started */
    UPROPERTY(Replicated)
    float ReloadStartTime;

    /** Is tactical reload (magazine not empty) */
    UPROPERTY(Replicated)
    bool bIsTacticalReload;

    //================================================
    // Cached References (Not Replicated)
    //================================================

    /** Cached weapon interface */
    UPROPERTY()
    TScriptInterface<ISuspenseCoreWeapon> CachedWeaponInterface;

    /** Cached reload effect handle */
    FActiveGameplayEffectHandle ReloadEffectHandle;

    /** Linked attribute component for GAS integration */
    UPROPERTY()
    class USuspenseCoreEquipmentAttributeComponent* LinkedAttributeComponent;

    /** Cached weapon AttributeSet for performance */
    UPROPERTY()
    USuspenseCoreWeaponAttributeSet* CachedWeaponAttributeSet;

    /** Cached ammo AttributeSet for performance */
    UPROPERTY()
    USuspenseCoreAmmoAttributeSet* CachedAmmoAttributeSet;

    /** Cached magazine size for performance */
    mutable float CachedMagazineSize;

    /** Cache validity flag */
    mutable bool bMagazineSizeCached;

 /**
     * Сохраняет текущее состояние патронов в ItemInstance через интерфейс оружия.
     * Это односторонняя операция - только для персистентности данных.
     * НЕ вызывает обратно SetAmmoState на компоненте, предотвращая рекурсию.
     */
 void SaveAmmoStateToWeapon();
};
