// SuspenseCoreLoadoutManager.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "Types/Loadout/SuspenseLoadoutSettings.h"
#include "SuspenseCoreLoadoutManager.generated.h"

class UDataTable;
class USuspenseCoreEventManager;
class ISuspenseCoreInventory;
class ISuspenseCoreEquipment;
class ISuspenseCoreLoadout;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FSuspenseCoreOnLoadoutChanged,
    const FName&, LoadoutID,
    APlayerState*, PlayerState,
    bool, bSuccess
);

/**
 * USuspenseCoreLoadoutManager
 *
 * Central manager for all loadout configurations.
 * Uses EventBus architecture for event broadcasting.
 */
UCLASS(BlueprintType, Blueprintable, Config=Game, defaultconfig)
class BRIDGESYSTEM_API USuspenseCoreLoadoutManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Loadout")
    int32 LoadLoadoutTable(UDataTable* InLoadoutTable);

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Loadout")
    void ReloadConfigurations();

    const FLoadoutConfiguration* GetLoadoutConfig(const FName& LoadoutID) const;

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Loadout", DisplayName = "Get Loadout Config")
    bool GetLoadoutConfigBP(const FName& LoadoutID, FLoadoutConfiguration& OutConfig) const;

    const FSuspenseInventoryConfig* GetInventoryConfig(const FName& LoadoutID, const FName& InventoryName = NAME_None) const;

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Loadout", DisplayName = "Get Inventory Config")
    bool GetInventoryConfigBP(const FName& LoadoutID, const FName& InventoryName, FSuspenseInventoryConfig& OutConfig) const;

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Loadout")
    TArray<FName> GetInventoryNames(const FName& LoadoutID) const;

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Loadout")
    TArray<FEquipmentSlotConfig> GetEquipmentSlots(const FName& LoadoutID) const;

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Loadout")
    bool IsLoadoutValid(const FName& LoadoutID) const;

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Loadout")
    TArray<FName> GetAllLoadoutIDs() const;

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Loadout")
    TArray<FName> GetLoadoutsForClass(const FGameplayTag& CharacterClass) const;

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Loadout")
    bool ApplyLoadoutToInventory(UObject* InventoryObject, const FName& LoadoutID, const FName& InventoryName = NAME_None) const;

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Loadout")
    bool ApplyLoadoutToEquipment(UObject* EquipmentObject, const FName& LoadoutID) const;

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Loadout")
    bool ApplyLoadoutToObject(UObject* LoadoutObject, const FName& LoadoutID, bool bForceApply = false) const;

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Loadout")
    FName GetDefaultLoadoutForClass(const FGameplayTag& CharacterClass) const;

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Loadout|Debug")
    bool ValidateAllConfigurations(TArray<FString>& OutErrors) const;

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Loadout|Stats")
    float GetTotalWeightCapacity(const FName& LoadoutID) const;

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Loadout|Stats")
    int32 GetTotalInventoryCells(const FName& LoadoutID) const;

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Loadout|Config")
    void SetDefaultDataTablePath(const FString& Path);

    void BroadcastLoadoutChange(const FName& LoadoutID, APlayerState* PlayerState, bool bSuccess) const;

    UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Loadout|Events")
    FSuspenseCoreOnLoadoutChanged OnLoadoutChanged;

protected:
    UPROPERTY(Config, EditAnywhere, Category = "SuspenseCore|Loadout|Config")
    FString DefaultLoadoutTablePath;

    UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Loadout")
    UDataTable* LoadedDataTable;

    UPROPERTY()
    TMap<FName, FLoadoutConfiguration> CachedConfigurations;

    UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Loadout|Defaults")
    TMap<FGameplayTag, FName> ClassDefaultLoadouts;

private:
    int32 CacheConfigurationsFromTable();
    bool ValidateConfiguration(const FLoadoutConfiguration& Config, TArray<FString>& OutErrors) const;
    void ClearCache();
    void TryLoadDefaultTable();
    void LogLoadoutStatistics() const;
    USuspenseCoreEventManager* GetEventManager() const;

    bool bIsInitialized = false;
    mutable FCriticalSection CacheCriticalSection;
};
