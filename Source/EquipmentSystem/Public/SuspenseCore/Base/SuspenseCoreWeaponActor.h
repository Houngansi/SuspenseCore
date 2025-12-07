// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Base/SuspenseCoreEquipmentActor.h"
#include "GameplayTagContainer.h"
#include "Interfaces/Weapon/ISuspenseWeapon.h"
#include "Interfaces/Weapon/ISuspenseFireModeProvider.h"
#include "Types/Weapon/SuspenseInventoryAmmoState.h"
#include "Types/Loadout/SuspenseItemDataTable.h"
#include "SuspenseCoreWeaponActor.generated.h"

// Forward declarations
class UCameraComponent;
class USuspenseCoreWeaponAmmoComponent;
class USuspenseCoreWeaponFireModeComponent;
class USuspenseCoreEquipmentAttributeComponent;
class USuspenseEventManager;
class USuspenseCoreEquipmentMeshComponent;

/**
 * SuspenseCore Weapon Actor - Modern event-driven weapon implementation.
 *
 * ARCHITECTURE (following BestPractices.md):
 * - Event Bus: Weapon events broadcast through centralized EventBus
 * - DI: Services inject via ServiceLocator for abilities, ammo management
 * - Tags: GameplayTags for weapon types, fire modes, ammo types
 * - Services: Abilities/effects delegated to AbilityService
 *
 * Key Principles:
 * - No GA/GE, no direct Attach or visual hacks
 * - Initializes components from SSOT and proxies calls to them
 * - Ammo state persistence happens via component; actor only mirrors to ItemInstance
 */
UCLASS()
class EQUIPMENTSYSTEM_API ASuspenseCoreWeaponActor : public ASuspenseCoreEquipmentActor,
                                         public ISuspenseWeapon,
                                         public ISuspenseFireModeProvider
{
    GENERATED_BODY()

public:
    ASuspenseCoreWeaponActor();

    //================================================
    // Engine Overrides
    //================================================
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual USuspenseEventManager* GetDelegateManager() const override;

    //================================================
    // ASuspenseCoreEquipmentActor overrides
    //================================================
    /** Weapon-specific init: extend base item-equip path with weapon setup */
    virtual void OnItemInstanceEquipped_Implementation(const FSuspenseInventoryItemInstance& ItemInstance) override;

    //================================================
    // ISuspenseWeapon (facade -> components)
    //================================================
    virtual FWeaponInitializationResult InitializeFromItemData_Implementation(const FSuspenseInventoryItemInstance& ItemInstance) override;
    virtual bool GetWeaponItemData_Implementation(FSuspenseUnifiedItemData& OutData) const override;
    virtual FSuspenseInventoryItemInstance GetItemInstance_Implementation() const override;

    // Basic actions
    virtual bool Fire_Implementation(const FWeaponFireParams& Params) override;
    virtual void StopFire_Implementation() override;
    virtual bool Reload_Implementation(bool bForce) override;
    virtual void CancelReload_Implementation() override;

    // Identity
    virtual FGameplayTag GetWeaponArchetype_Implementation() const override;
    virtual FGameplayTag GetWeaponType_Implementation() const override;
    virtual FGameplayTag GetAmmoType_Implementation() const override;

    // Sockets (from SSOT)
    virtual FName GetMuzzleSocketName_Implementation() const override;
    virtual FName GetSightSocketName_Implementation() const override;
    virtual FName GetMagazineSocketName_Implementation() const override;
    virtual FName GetGripSocketName_Implementation() const override;
    virtual FName GetStockSocketName_Implementation() const override;

    // Attributes (read-only)
    virtual float GetWeaponDamage_Implementation() const override;
    virtual float GetFireRate_Implementation() const override;
    virtual float GetReloadTime_Implementation() const override;
    virtual float GetRecoil_Implementation() const override;
    virtual float GetRange_Implementation() const override;

    // Spread
    virtual float GetBaseSpread_Implementation() const override;
    virtual float GetMaxSpread_Implementation() const override;
    virtual float GetCurrentSpread_Implementation() const override;
    virtual void  SetCurrentSpread_Implementation(float NewSpread) override;

    // Ammo (delegation to AmmoComponent)
    virtual float GetCurrentAmmo_Implementation() const override;
    virtual float GetRemainingAmmo_Implementation() const override;
    virtual float GetMagazineSize_Implementation() const override;
    virtual FSuspenseInventoryAmmoState GetAmmoState_Implementation() const override;
    virtual void SetAmmoState_Implementation(const FSuspenseInventoryAmmoState& NewState) override;
    virtual bool CanReload_Implementation() const override;
    virtual bool IsMagazineFull_Implementation() const override;

    // Weapon state (flags)
    virtual FWeaponStateFlags GetWeaponState_Implementation() const override;
    virtual bool IsInWeaponState_Implementation(const FWeaponStateFlags& State) const override;
    virtual void SetWeaponState_Implementation(const FWeaponStateFlags& NewState, bool bEnabled) override;

    //================================================
    // ISuspenseFireModeProvider (proxy to component)
    //================================================
    virtual bool InitializeFromWeaponData_Implementation(const FSuspenseUnifiedItemData& WeaponData) override;
    virtual void ClearFireModes_Implementation() override;
    virtual bool CycleToNextFireMode_Implementation() override;
    virtual bool CycleToPreviousFireMode_Implementation() override;
    virtual bool SetFireMode_Implementation(const FGameplayTag& FireModeTag) override;
    virtual bool SetFireModeByIndex_Implementation(int32 Index) override;
    virtual FGameplayTag GetCurrentFireMode_Implementation() const override;
    virtual FFireModeRuntimeData GetCurrentFireModeData_Implementation() const override;
    virtual bool IsFireModeAvailable_Implementation(const FGameplayTag& FireModeTag) const override;
    virtual TArray<FFireModeRuntimeData> GetAllFireModes_Implementation() const override;
    virtual TArray<FGameplayTag> GetAvailableFireModes_Implementation() const override;
    virtual int32 GetAvailableFireModeCount_Implementation() const override;
    virtual bool SetFireModeEnabled_Implementation(const FGameplayTag& FireModeTag, bool bEnabled) override;
    virtual void SetFireModeBlocked_Implementation(const FGameplayTag& FireModeTag, bool bBlocked) override;
    virtual bool IsFireModeBlocked_Implementation(const FGameplayTag& FireModeTag) const override;
    virtual bool GetFireModeData_Implementation(const FGameplayTag& FireModeTag, FFireModeRuntimeData& OutData) const override;
    virtual TSubclassOf<UGameplayAbility> GetFireModeAbility_Implementation(const FGameplayTag& FireModeTag) const override;
    virtual int32 GetFireModeInputID_Implementation(const FGameplayTag& FireModeTag) const override;

    //================================================
    // Utility
    //================================================
    UFUNCTION(BlueprintCallable, Category="Weapon|Geometry")
    FVector GetMuzzleLocation() const;

    UFUNCTION(BlueprintCallable, Category="Weapon|Geometry")
    FRotator GetMuzzleRotation() const;

    UFUNCTION(BlueprintCallable, Category="Weapon|Geometry")
    FTransform GetMuzzleTransform() const;

    // Persist/restore limited runtime state (ammo/firemode index) only
    UFUNCTION(BlueprintCallable, Category="Weapon|State")
    void SaveWeaponState();

    UFUNCTION(BlueprintCallable, Category="Weapon|State")
    void RestoreWeaponState();

protected:
    /** Setup components from SSOT (ASC is cached by base during equip) */
    void SetupComponentsFromItemData(const FSuspenseUnifiedItemData& ItemData);

    /** Read attribute with fallback default */
    float GetWeaponAttributeValue(const FName& AttributeName, float DefaultValue) const;

    /** Cached weapon data from SSOT */
    UPROPERTY(Transient)
    FSuspenseUnifiedItemData CachedItemData;

    /** Whether CachedItemData is valid */
    UPROPERTY(Transient)
    bool bHasCachedData = false;

    //================================================
    // Components (owned by actor) - SuspenseCore prefixed
    //================================================
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
    USuspenseCoreWeaponAmmoComponent* AmmoComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
    USuspenseCoreWeaponFireModeComponent* FireModeComponent;

    /** Optional local scope camera component */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
    UCameraComponent* ScopeCamera;
};
