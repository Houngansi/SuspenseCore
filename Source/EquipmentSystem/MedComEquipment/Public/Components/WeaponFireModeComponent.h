// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/EquipmentComponentBase.h"
#include "GameplayTagContainer.h"
#include "Interfaces/Weapon/IMedComFireModeProviderInterface.h"
#include "WeaponFireModeComponent.generated.h"

// Forward declarations
class UGameplayAbility;
class IMedComWeaponInterface;
struct FMedComUnifiedItemData;
struct FWeaponFireModeData;

/**
 * Weapon Fire Mode Management Component
 * 
 * VERSION 4.0 - COMPLETE REWRITE:
 * - All fire mode definitions come from DataTable via weapon interface
 * - No local storage of fire mode configurations
 * - Uses FFireModeRuntimeData for runtime state only
 * - Fully integrated with IMedComWeaponInterface and IMedComFireModeProviderInterface
 * - Simplified architecture with single source of truth
 */
UCLASS(ClassGroup=(Equipment), meta=(BlueprintSpawnableComponent))
class MEDCOMEQUIPMENT_API UWeaponFireModeComponent : public UEquipmentComponentBase, public IMedComFireModeProviderInterface
{
    GENERATED_BODY()

public:
    UWeaponFireModeComponent();
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
    bool InitializeFromWeapon(TScriptInterface<IMedComWeaponInterface> WeaponInterface);

    //================================================
    // IMedComFireModeProviderInterface Implementation
    //================================================

    virtual bool InitializeFromWeaponData_Implementation(const FMedComUnifiedItemData& WeaponData) override;
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
    virtual UEventDelegateManager* GetDelegateManager() const override;

protected:
    /**
     * Get weapon interface from owner
     * @return Weapon interface or null
     */
    IMedComWeaponInterface* GetWeaponInterface() const;

    /**
     * Get weapon data from DataTable
     * @param OutData Output weapon data
     * @return True if data retrieved
     */
    bool GetWeaponData(FMedComUnifiedItemData& OutData) const;

    /**
     * Load fire modes from weapon data
     * @param WeaponData Data from DataTable
     */
    void LoadFireModesFromData(const FMedComUnifiedItemData& WeaponData);

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
    TArray<FFireModeRuntimeData> FireModes;

    /** Blocked fire modes (temporary state) */
    UPROPERTY()
    TSet<FGameplayTag> BlockedFireModes;

    /** Granted ability handles for cleanup */
    UPROPERTY()
    TMap<FGameplayTag, FGameplayAbilitySpecHandle> AbilityHandles;

    /** Cached weapon interface */
    UPROPERTY()
    TScriptInterface<IMedComWeaponInterface> CachedWeaponInterface;

    /** Flag to prevent recursion during switching */
    bool bIsSwitching;
};