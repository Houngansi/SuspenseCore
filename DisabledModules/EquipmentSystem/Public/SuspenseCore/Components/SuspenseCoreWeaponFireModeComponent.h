// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Components/SuspenseCoreEquipmentComponentBase.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreFireModeProvider.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeapon.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreItemDataTable.h"
#include "SuspenseCoreWeaponFireModeComponent.generated.h"

// Forward declarations
class UGameplayAbility;
struct FSuspenseCoreFireModeRuntimeData;

/**
 * Weapon Fire Mode Management Component
 * 
 * VERSION 4.0 - COMPLETE REWRITE:
 * - All fire mode definitions come from DataTable via weapon interface
 * - No local storage of fire mode configurations
 * - Uses FSuspenseCoreFireModeRuntimeData for runtime state only
 * - Fully integrated with ISuspenseCoreWeapon and ISuspenseCoreFireModeProvider
 * - Simplified architecture with single source of truth
 */
UCLASS(ClassGroup=(Equipment), meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseCoreWeaponFireModeComponent : public USuspenseCoreEquipmentComponentBase, public ISuspenseCoreFireModeProvider
{
    GENERATED_BODY()

public:
    USuspenseCoreWeaponFireModeComponent();
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void Cleanup() override;

    //================================================
    // Initialization
    //================================================
    
    /**
     * Initialize from weapon interface
     * Loads all fire modes from DataTable
     * @param WeaponInterface Weapon that owns this component
     * @return Success of initialization
     */
    UFUNCTION(BlueprintCallable, Category = "Weapon|FireMode")
    bool InitializeFromWeapon(TScriptInterface<ISuspenseCoreWeapon> WeaponInterface);

    //================================================
    // ISuspenseCoreFireModeProvider Implementation
    //================================================

    virtual bool InitializeFromWeaponData_Implementation(const FSuspenseCoreUnifiedItemData& WeaponData) override;
    virtual void ClearFireModes_Implementation() override;
    virtual bool CycleToNextFireMode_Implementation() override;
    virtual bool CycleToPreviousFireMode_Implementation() override;
    virtual bool SetFireMode_Implementation(const FGameplayTag& FireModeTag) override;
    virtual bool SetFireModeByIndex_Implementation(int32 Index) override;
    virtual FGameplayTag GetCurrentFireMode_Implementation() const override;
    virtual FSuspenseCoreFireModeRuntimeData GetCurrentFireModeData_Implementation() const override;
    virtual bool IsFireModeAvailable_Implementation(const FGameplayTag& FireModeTag) const override;
    virtual TArray<FSuspenseCoreFireModeRuntimeData> GetAllFireModes_Implementation() const override;
    virtual TArray<FGameplayTag> GetAvailableFireModes_Implementation() const override;
    virtual int32 GetAvailableFireModeCount_Implementation() const override;
    virtual bool SetFireModeEnabled_Implementation(const FGameplayTag& FireModeTag, bool bEnabled) override;
    virtual void SetFireModeBlocked_Implementation(const FGameplayTag& FireModeTag, bool bBlocked) override;
    virtual bool IsFireModeBlocked_Implementation(const FGameplayTag& FireModeTag) const override;
    virtual bool GetFireModeData_Implementation(const FGameplayTag& FireModeTag, FSuspenseCoreFireModeRuntimeData& OutData) const override;
    virtual TSubclassOf<UGameplayAbility> GetFireModeAbility_Implementation(const FGameplayTag& FireModeTag) const override;
    virtual int32 GetFireModeInputID_Implementation(const FGameplayTag& FireModeTag) const override;
    virtual USuspenseCoreEventManager* GetDelegateManager() const override;

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
     * Load fire modes from weapon data
     * @param WeaponData Data from DataTable
     */
    void LoadFireModesFromData(const FSuspenseCoreUnifiedItemData& WeaponData);

    /**
     * Grant abilities for all fire modes
     */
    void GrantFireModeAbilities();

    /**
     * Remove all granted abilities
     */
    void RemoveFireModeAbilities();

    /**
     * Find fire mode index by tag
     * @param FireModeTag Tag to find
     * @return Index or INDEX_NONE
     */
    int32 FindFireModeIndex(const FGameplayTag& FireModeTag) const;

    /**
     * Broadcast fire mode change
     */
    void BroadcastFireModeChanged();

    /**
     * Network replication handler
     */
    UFUNCTION()
    void OnRep_CurrentFireModeIndex();

private:
    //================================================
    // Runtime State (Minimal Replication)
    //================================================

    /** Current fire mode index in array */
    UPROPERTY(ReplicatedUsing = OnRep_CurrentFireModeIndex)
    int32 CurrentFireModeIndex;

    /** Fire modes loaded from DataTable */
    UPROPERTY()
    TArray<FSuspenseCoreFireModeRuntimeData> FireModes;

    /** Blocked fire modes (temporary state) */
    UPROPERTY()
    TSet<FGameplayTag> BlockedFireModes;

    /** Granted ability handles for cleanup */
    UPROPERTY()
    TMap<FGameplayTag, FGameplayAbilitySpecHandle> AbilityHandles;

    /** Cached weapon interface */
    UPROPERTY()
    TScriptInterface<ISuspenseCoreWeapon> CachedWeaponInterface;

    /** Flag to prevent recursion during switching */
    bool bIsSwitching;
};