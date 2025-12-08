// Copyright Suspense Team. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreAbilityConnector.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentDataProvider.h"
#include "ItemSystem/SuspenseCoreItemManager.h"
#include "Types/Loadout/SuspenseCoreItemDataTable.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "SuspenseCoreEquipmentAbilityConnector.generated.h"

class UAbilitySystemComponent;
class UAttributeSet;

/** Record of a granted ability */
USTRUCT()
struct FSuspenseCoreGrantedAbilityRecord
{
    GENERATED_BODY()

    UPROPERTY() FGuid ItemInstanceId;
    UPROPERTY() int32 SlotIndex = INDEX_NONE;
    UPROPERTY() FGameplayAbilitySpecHandle AbilityHandle;
    UPROPERTY() TSubclassOf<class UGameplayAbility> AbilityClass;
    UPROPERTY() int32 AbilityLevel = 1;
    UPROPERTY() FGameplayTag InputTag;
    UPROPERTY() float GrantTime = 0.f;
    UPROPERTY() FString Source;
};

/** Record of an applied gameplay effect */
USTRUCT()
struct FSuspenseCoreAppliedEffectRecord
{
    GENERATED_BODY()

    UPROPERTY() FGuid ItemInstanceId;
    UPROPERTY() int32 SlotIndex = INDEX_NONE;
    UPROPERTY() FActiveGameplayEffectHandle EffectHandle;
    UPROPERTY() TSubclassOf<class UGameplayEffect> EffectClass;
    UPROPERTY() float ApplicationTime = 0.f;
    UPROPERTY() float Duration = 0.f; // -1 for infinite
    UPROPERTY() FString Source;
};

/** Managed attribute set info */
USTRUCT()
struct FSuspenseCoreManagedAttributeSet
{
    GENERATED_BODY()

    UPROPERTY() int32 SlotIndex = INDEX_NONE;
    UPROPERTY() TObjectPtr<UAttributeSet> AttributeSet = nullptr;
    UPROPERTY() TSubclassOf<UAttributeSet> AttributeClass;
    UPROPERTY() FGuid ItemInstanceId;
    UPROPERTY() bool bIsInitialized = false;
    UPROPERTY() FString AttributeType;
};

/**
 * Production connector component that bridges equipment system with GAS.
 */
UCLASS(ClassGroup=(MedCom), meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentAbilityConnector : public UActorComponent, public ISuspenseCoreAbilityConnector
{
    GENERATED_BODY()

public:
    USuspenseCoreEquipmentAbilityConnector();

    // UActorComponent
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // ISuspenseAbilityConnector (via ISuspenseCoreAbilityConnector alias)
    virtual bool Initialize(UAbilitySystemComponent* InASC, TScriptInterface<ISuspenseCoreEquipmentDataProvider> InDataProvider) override;
    virtual TArray<FGameplayAbilitySpecHandle> GrantEquipmentAbilities(const FSuspenseCoreInventoryItemInstance& ItemInstance) override;
    virtual int32 RemoveGrantedAbilities(const TArray<FGameplayAbilitySpecHandle>& Handles) override;
    virtual TArray<FActiveGameplayEffectHandle> ApplyEquipmentEffects(const FSuspenseCoreInventoryItemInstance& ItemInstance) override;
    virtual int32 RemoveAppliedEffects(const TArray<FActiveGameplayEffectHandle>& Handles) override;
    virtual bool UpdateEquipmentAttributes(const FSuspenseCoreInventoryItemInstance& ItemInstance) override;
    virtual UAttributeSet* GetEquipmentAttributeSet(int32 SlotIndex) const override;
    virtual bool ActivateEquipmentAbility(const FGameplayAbilitySpecHandle& AbilityHandle) override;
    virtual void ClearAll() override;
    virtual int32 CleanupInvalidHandles() override;
    virtual bool ValidateConnector(TArray<FString>& OutErrors) const override;
    virtual FString GetDebugInfo() const override;
    virtual void LogStatistics() const override;

    /** Slot-scoped helpers */
    TArray<FGameplayAbilitySpecHandle> GrantAbilitiesForSlot(int32 SlotIndex, const FSuspenseCoreInventoryItemInstance& ItemInstance);
    int32 RemoveAbilitiesForSlot(int32 SlotIndex);
    TArray<FActiveGameplayEffectHandle> ApplyEffectsForSlot(int32 SlotIndex, const FSuspenseCoreInventoryItemInstance& ItemInstance);
    int32 RemoveEffectsForSlot(int32 SlotIndex);

protected:
    // Internal operations used by public API
    TArray<FGameplayAbilitySpecHandle> GrantAbilitiesFromItemData(const struct FSuspenseCoreUnifiedItemData& ItemData, const FSuspenseCoreInventoryItemInstance& ItemInstance, int32 SlotIndex);
    TArray<FActiveGameplayEffectHandle> ApplyEffectsFromItemData(const struct FSuspenseCoreUnifiedItemData& ItemData, const FSuspenseCoreInventoryItemInstance& ItemInstance, int32 SlotIndex);
    UAttributeSet* CreateAttributeSetFromItemData(const struct FSuspenseCoreUnifiedItemData& ItemData, const FSuspenseCoreInventoryItemInstance& ItemInstance, int32 SlotIndex);
    FGameplayAbilitySpecHandle GrantSingleAbility(TSubclassOf<class UGameplayAbility> AbilityClass, int32 Level, const FGameplayTag& InputTag, UObject* SourceObject, const FString& Source);
    FActiveGameplayEffectHandle ApplySingleEffect(TSubclassOf<class UGameplayEffect> EffectClass, float Level, UObject* SourceObject, const FString& Source);
    bool InitializeAttributeSet(UAttributeSet* AttributeSet, TSubclassOf<class UGameplayEffect> InitEffect, const FSuspenseCoreInventoryItemInstance& ItemInstance);
    class USuspenseCoreItemManager* GetItemManager() const;
    bool EnsureValidExecution(const FString& FunctionName) const;

private:
    /** GAS */
    UPROPERTY(Transient) TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent = nullptr;

    /** External dependencies */
    UPROPERTY() TScriptInterface<ISuspenseCoreEquipmentDataProvider> DataProvider;

    /** State flags */
    UPROPERTY() bool bIsInitialized = false;
    UPROPERTY(EditAnywhere, Category="GAS") bool bServerOnly = true;

    /** Runtime tracking */
    UPROPERTY() TArray<FSuspenseCoreGrantedAbilityRecord> GrantedAbilities;
    UPROPERTY() TArray<FSuspenseCoreAppliedEffectRecord> AppliedEffects;
    UPROPERTY() TArray<FSuspenseCoreManagedAttributeSet> ManagedAttributeSets;

    // Stats
    mutable int32 TotalAbilitiesGranted = 0;
    mutable int32 TotalEffectsApplied = 0;
    mutable int32 TotalAttributeSetsCreated = 0;
    mutable int32 TotalActivations = 0;
    mutable int32 FailedGrantOperations = 0;
    mutable int32 FailedApplyOperations = 0;
    mutable int32 FailedActivateOperations = 0;

    // Cache
    mutable TWeakObjectPtr<USuspenseCoreItemManager> CachedItemManager;
    mutable float LastCacheTime = 0.f;
    static constexpr float CacheLifetime = 5.f;

    // Thread safety
    mutable FCriticalSection ConnectorCriticalSection;
};
