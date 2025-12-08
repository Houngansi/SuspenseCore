// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Types/Weapon/SuspenseInventoryAmmoState.h"
#include "ISuspenseWeapon.generated.h"

// Forward declarations
class UGameplayEffect;
class UGameplayAbility;
class UAttributeSet;
class UAbilitySystemComponent;
class USuspenseCoreEventManager;
struct FSuspenseUnifiedItemData;
struct FWeaponFireParams;

/**
 * Weapon initialization result structure
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FWeaponInitializationResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    bool bSuccess = false;

    UPROPERTY(BlueprintReadOnly)
    FText ErrorMessage;

    UPROPERTY(BlueprintReadOnly)
    int32 FireModesLoaded = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 AbilitiesGranted = 0;
};

/**
 * Weapon fire parameters
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FWeaponFireParams
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FVector FireDirection = FVector::ForwardVector;

    UPROPERTY(BlueprintReadWrite)
    FVector FireOrigin = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite)
    AActor* TargetActor = nullptr;

    UPROPERTY(BlueprintReadWrite)
    float DamageMultiplier = 1.0f;

    UPROPERTY(BlueprintReadWrite)
    bool bAltFire = false;

    UPROPERTY(BlueprintReadWrite)
    FGameplayTagContainer FireTags;
};

/**
 * Weapon state flags
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FWeaponStateFlags
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    uint8 bIsFiring : 1;

    UPROPERTY(BlueprintReadWrite)
    uint8 bIsReloading : 1;

    UPROPERTY(BlueprintReadWrite)
    uint8 bIsAiming : 1;

    UPROPERTY(BlueprintReadWrite)
    uint8 bIsJammed : 1;

    UPROPERTY(BlueprintReadWrite)
    uint8 bIsOverheated : 1;

    UPROPERTY(BlueprintReadWrite)
    uint8 bIsSwitchingFireMode : 1;

    FWeaponStateFlags()
    {
        bIsFiring = false;
        bIsReloading = false;
        bIsAiming = false;
        bIsJammed = false;
        bIsOverheated = false;
        bIsSwitchingFireMode = false;
    }
};

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseWeapon : public UInterface
{
    GENERATED_BODY()
};

/**
 * Weapon-specific interface extending equipment functionality
 *
 * This interface handles all weapon-specific features:
 * - Ammunition management
 * - Fire modes and shooting
 * - Weapon attributes (damage, fire rate, etc.)
 * - Accuracy and spread
 *
 * Weapons should implement both ISuspenseEquipment and ISuspenseWeapon
 */
class BRIDGESYSTEM_API ISuspenseWeapon
{
    GENERATED_BODY()

public:
    //==================================================================
    // Weapon Initialization
    //==================================================================

    /**
     * Initialize weapon from item data
     * @param ItemInstance Item instance with weapon configuration
     * @return Initialization result with details
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Initialization")
    FWeaponInitializationResult InitializeFromItemData(const FSuspenseInventoryItemInstance& ItemInstance);

    /**
     * Get cached weapon data from DataTable
     * @param OutData Output weapon data
     * @return True if data is available
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Data")
    bool GetWeaponItemData(FSuspenseUnifiedItemData& OutData) const;

    /**
     * Get current item instance
     * @return Current weapon item instance
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Data")
    FSuspenseInventoryItemInstance GetItemInstance() const;

    //==================================================================
    // Core Weapon Actions
    //==================================================================

    /**
     * Fire the weapon
     * @param Params Fire parameters
     * @return True if successfully fired
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Actions")
    bool Fire(const FWeaponFireParams& Params);

    /**
     * Stop firing (for automatic weapons)
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Actions")
    void StopFire();

    /**
     * Reload the weapon
     * @param bForce Force reload even if magazine is full
     * @return True if reload started
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Actions")
    bool Reload(bool bForce = false);

    /**
     * Cancel reload in progress
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Actions")
    void CancelReload();

    //==================================================================
    // Weapon Properties from DataTable
    //==================================================================

    /**
     * Get weapon type tag
     * @return Weapon type (Item.Type.Weapon.*)
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Properties")
    FGameplayTag GetWeaponType() const;

    /**
     * Get weapon archetype tag
     * @return Archetype (Weapon.Type.Ranged.*, Weapon.Type.Melee.*)
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Properties")
    FGameplayTag GetWeaponArchetype() const;

    /**
     * Get ammo type tag
     * @return Ammo type (Item.Ammo.*)
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Properties")
    FGameplayTag GetAmmoType() const;

    //==================================================================
    // Socket Names from DataTable
    //==================================================================

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Sockets")
    FName GetMuzzleSocketName() const;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Sockets")
    FName GetSightSocketName() const;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Sockets")
    FName GetMagazineSocketName() const;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Sockets")
    FName GetGripSocketName() const;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Sockets")
    FName GetStockSocketName() const;

    //==================================================================
    // Weapon Attributes from AttributeSet
    //==================================================================

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Attributes")
    float GetWeaponDamage() const;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Attributes")
    float GetFireRate() const;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Attributes")
    float GetReloadTime() const;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Attributes")
    float GetRecoil() const;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Attributes")
    float GetRange() const;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Attributes")
    float GetBaseSpread() const;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Attributes")
    float GetMaxSpread() const;

    //==================================================================
    // Accuracy and Spread
    //==================================================================

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Accuracy")
    float GetCurrentSpread() const;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Accuracy")
    void SetCurrentSpread(float NewSpread);

    //==================================================================
    // Ammunition Management
    //==================================================================

    /**
     * Get current ammo in magazine
     * @return Current ammo count as float for energy weapons
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Ammo")
    float GetCurrentAmmo() const;

    /**
     * Get remaining ammo in reserve
     * @return Reserve ammo count
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Ammo")
    float GetRemainingAmmo() const;

    /**
     * Get magazine capacity
     * @return Magazine size
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Ammo")
    float GetMagazineSize() const;

    /**
     * Get complete ammo state
     * @return Ammo state structure
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Ammo")
    FSuspenseInventoryAmmoState GetAmmoState() const;

    /**
     * Set ammo state
     * @param NewState New ammo state
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Ammo")
    void SetAmmoState(const FSuspenseInventoryAmmoState& NewState);

    /**
     * Check if can reload
     * @return True if reload is possible
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Ammo")
    bool CanReload() const;

    /**
     * Check if magazine is full
     * @return True if magazine is at capacity
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Ammo")
    bool IsMagazineFull() const;

    //==================================================================
    // Weapon State
    //==================================================================

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|State")
    FWeaponStateFlags GetWeaponState() const;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|State")
    bool IsInWeaponState(const FWeaponStateFlags& State) const;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|State")
    void SetWeaponState(const FWeaponStateFlags& NewState, bool bEnabled);

    //==================================================================
    // Fire Modes
    //==================================================================

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|FireMode")
    TArray<FGameplayTag> GetAvailableFireModes() const;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|FireMode")
    FGameplayTag GetCurrentFireMode() const;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|FireMode")
    bool SetFireMode(const FGameplayTag& FireModeTag);

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|FireMode")
    bool CycleFireMode();

    //==================================================================
    // Abilities and Effects
    //==================================================================

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Abilities")
    TArray<TSubclassOf<UGameplayAbility>> GetGrantedAbilities() const;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Abilities")
    TArray<TSubclassOf<UGameplayEffect>> GetPassiveEffects() const;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon|Abilities")
    TSubclassOf<UGameplayEffect> GetWeaponEffect() const;

    //==================================================================
    // Event System Integration
    //==================================================================

    /**
     * Get delegate manager for weapon events
     */
    virtual USuspenseCoreEventManager* GetDelegateManager() const = 0;

    /**
     * Static helper to get delegate manager
     */
    static USuspenseCoreEventManager* GetDelegateManagerStatic(const UObject* WorldContextObject);

    /**
     * Broadcast weapon fired event
     */
    static void BroadcastWeaponFired(
        const UObject* Weapon,
        const FVector& Origin,
        const FVector& Impact,
        bool bSuccess,
        FName ShotType = NAME_None);

    /**
     * Broadcast ammo changed event
     */
    static void BroadcastAmmoChanged(
        const UObject* Weapon,
        float CurrentAmmo,
        float RemainingAmmo,
        float MagazineSize);

    /**
     * Broadcast reload started event
     */
    static void BroadcastReloadStarted(
        const UObject* Weapon,
        float ReloadDuration);

    /**
     * Broadcast reload completed event
     */
    static void BroadcastReloadCompleted(
        const UObject* Weapon,
        bool bSuccess);

    /**
     * Broadcast fire mode changed event
     */
    static void BroadcastFireModeChanged(
        const UObject* Weapon,
        const FGameplayTag& NewFireMode);
};
