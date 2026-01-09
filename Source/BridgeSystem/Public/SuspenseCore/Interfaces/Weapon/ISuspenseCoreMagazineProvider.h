// ISuspenseCoreMagazineProvider.h
// Interface for magazine system access from GAS abilities
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SuspenseCore/Types/Weapon/SuspenseCoreMagazineTypes.h"
#include "ISuspenseCoreMagazineProvider.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreMagazineProvider : public UInterface
{
    GENERATED_BODY()
};

/**
 * ISuspenseCoreMagazineProvider
 *
 * Interface for accessing magazine functionality from GAS abilities.
 * Implemented by USuspenseCoreMagazineComponent (EquipmentSystem).
 * Used by USuspenseCoreReloadAbility (GAS) to avoid circular dependencies.
 *
 * ARCHITECTURE:
 * - Defined in BridgeSystem (shared)
 * - Implemented in EquipmentSystem
 * - Used by GAS
 */
class BRIDGESYSTEM_API ISuspenseCoreMagazineProvider
{
    GENERATED_BODY()

public:
    //==================================================================
    // State Queries
    //==================================================================

    /** Get current weapon ammo state */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Magazine")
    FSuspenseCoreWeaponAmmoState GetAmmoState() const;

    /** Check if weapon has magazine inserted */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Magazine")
    bool HasMagazine() const;

    /** Check if weapon is ready to fire (round chambered) */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Magazine")
    bool IsReadyToFire() const;

    /** Check if currently reloading */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Magazine")
    bool IsReloading() const;

    //==================================================================
    // Magazine Operations
    //==================================================================

    /** Insert a magazine into the weapon */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Magazine")
    bool InsertMagazine(const FSuspenseCoreMagazineInstance& Magazine);

    /** Eject current magazine */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Magazine")
    FSuspenseCoreMagazineInstance EjectMagazine(bool bDropToGround);

    //==================================================================
    // Chamber Operations
    //==================================================================

    /** Chamber a round from magazine */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Magazine")
    bool ChamberRound();

    /** Eject chambered round */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Magazine")
    FSuspenseCoreChamberedRound EjectChamberedRound();

    //==================================================================
    // Reload Support
    //==================================================================

    /** Determine optimal reload type for current state */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Magazine|Reload")
    ESuspenseCoreReloadType DetermineReloadType() const;

    /** Calculate reload duration for given type */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Magazine|Reload")
    float CalculateReloadDuration(ESuspenseCoreReloadType ReloadType, const FSuspenseCoreMagazineInstance& NewMagazine) const;

    /** Notify reload state changed (for events) */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Magazine|Reload")
    void NotifyReloadStateChanged(bool bIsReloading, ESuspenseCoreReloadType ReloadType, float Duration);

    //==================================================================
    // Compatibility Checks
    //==================================================================

    /**
     * Check if a magazine is compatible with the current weapon's caliber
     * Uses SSOT DataManager to lookup magazine caliber and compare with weapon's AmmoType
     *
     * @param Magazine Magazine instance to check
     * @return true if magazine caliber matches weapon caliber
     * @see TarkovStyle_Ammo_System_Design.md - Caliber validation
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Magazine|Compatibility")
    bool IsMagazineCompatible(const FSuspenseCoreMagazineInstance& Magazine) const;

    /**
     * Get the weapon's caliber/ammo type tag
     * Used for UI display and external compatibility checks
     *
     * @return Weapon's AmmoType tag (e.g., Item.Ammo.556x45)
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Magazine|Compatibility")
    FGameplayTag GetWeaponCaliber() const;
};
