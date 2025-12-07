// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Interfaces/Abilities/ISuspenseAbilityProvider.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Types/Loadout/SuspenseItemDataTable.h"
#include "SuspenseCoreEquipmentComponentBase.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UGameplayEffect;
class USuspenseEventManager;
class USuspenseItemManager;

DECLARE_LOG_CATEGORY_EXTERN(LogSuspenseCoreEquipment, Log, All);

#define SUSPENSECORE_LOG(Verbosity, Format, ...) \
UE_LOG(LogSuspenseCoreEquipment, Verbosity, TEXT("%s: " Format), *GetNameSafe(this), ##__VA_ARGS__)

/**
 * Client prediction data structure for SuspenseCore
 */
USTRUCT()
struct FSuspenseCorePredictionData
{
    GENERATED_BODY()

    UPROPERTY()
    int32 PredictionKey = 0;

    UPROPERTY()
    FSuspenseInventoryItemInstance PredictedItem;

    UPROPERTY()
    float PredictionTime = 0.0f;

    UPROPERTY()
    bool bConfirmed = false;

    bool IsExpired(float CurrentTime, float TimeoutSeconds = 2.0f) const
    {
        return (CurrentTime - PredictionTime) > TimeoutSeconds;
    }
};

/**
 * SuspenseCore Equipment Component Base - Modern architecture following BestPractices.md
 *
 * ARCHITECTURE:
 * - Event Bus: All events through centralized EventBus for decoupled communication
 * - DI: Dependencies via ServiceLocator, not hard-coded references
 * - Tags: GameplayTags for state identification and event routing
 * - Thread Safety: Critical sections for shared state access
 *
 * Key Features:
 * - Full replication support
 * - Client prediction infrastructure
 * - Thread-safe caching
 * - Enhanced validation and error handling
 */
UCLASS(Abstract)
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentComponentBase : public UActorComponent, public ISuspenseAbilityProvider
{
    GENERATED_BODY()

public:
    USuspenseCoreEquipmentComponentBase();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** Initialize component with owner and ASC */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment")
    virtual void Initialize(AActor* InOwner, UAbilitySystemComponent* InASC);

    /** Initialize with item instance data */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment")
    virtual void InitializeWithItemInstance(AActor* InOwner, UAbilitySystemComponent* InASC, const FSuspenseInventoryItemInstance& ItemInstance);

    /** Comprehensive resource cleanup */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment")
    virtual void Cleanup();

    /** Update equipment with new item instance */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment")
    virtual void UpdateEquippedItem(const FSuspenseInventoryItemInstance& NewItemInstance);

    //================================================
    // Client Prediction Support
    //================================================

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Prediction")
    int32 StartClientPrediction(const FSuspenseInventoryItemInstance& PredictedInstance);

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Prediction")
    void ConfirmClientPrediction(int32 PredictionKey, bool bSuccess, const FSuspenseInventoryItemInstance& ActualInstance);

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Prediction")
    void CleanupExpiredPredictions();

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Prediction")
    bool IsInPredictionMode() const { return ActivePredictions.Num() > 0; }

    //================================================
    // Data Access Methods
    //================================================

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Data")
    USuspenseItemManager* GetItemManager() const;

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Data")
    const FSuspenseInventoryItemInstance& GetEquippedItemInstance() const { return EquippedItemInstance; }

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Data")
    virtual void SetEquippedItemInstance(const FSuspenseInventoryItemInstance& ItemInstance);

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Data")
    bool GetEquippedItemData(FSuspenseUnifiedItemData& OutItemData) const;

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Data")
    bool HasEquippedItem() const { return EquippedItemInstance.IsValid(); }

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Data")
    FName GetEquippedItemID() const { return EquippedItemInstance.ItemID; }

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Data")
    float GetEquippedItemProperty(const FName& PropertyName, float DefaultValue = 0.0f) const;

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Data")
    void SetEquippedItemProperty(const FName& PropertyName, float Value);

    //================================================
    // System Access Methods
    //================================================

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|System")
    UAbilitySystemComponent* GetCachedASC() const { return CachedASC; }

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|System")
    USuspenseEventManager* GetDelegateManager() const;

    //================================================
    // Broadcast Methods (EventBus Integration)
    //================================================

    void BroadcastItemEquipped(const FSuspenseInventoryItemInstance& ItemInstance, const FGameplayTag& SlotType);
    void BroadcastItemUnequipped(const FSuspenseInventoryItemInstance& ItemInstance, const FGameplayTag& SlotType);
    void BroadcastEquipmentPropertyChanged(const FName& PropertyName, float OldValue, float NewValue);
    void BroadcastEquipmentStateChanged(const FGameplayTag& OldState, const FGameplayTag& NewState, bool bInterrupted = false);
    void BroadcastEquipmentEvent(const FGameplayTag& EventTag, const FString& EventData);
    void BroadcastEquipmentUpdated();

    // Weapon-specific broadcasts
    void BroadcastAmmoChanged(float CurrentAmmo, float RemainingAmmo, float MagazineSize);
    void BroadcastWeaponFired(const FVector& Origin, const FVector& Impact, bool bSuccess, const FGameplayTag& FireMode);
    void BroadcastFireModeChanged(const FGameplayTag& NewFireMode, const FText& FireModeDisplayName);
    void BroadcastWeaponReload(bool bStarted, float ReloadDuration = 0.0f);
    void BroadcastWeaponSpreadUpdated(float NewSpread, float MaxSpread);

    //================================================
    // ISuspenseAbilityProvider Implementation
    //================================================

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Abilities")
    UAbilitySystemComponent* GetAbilitySystemComponent() const;
    virtual UAbilitySystemComponent* GetAbilitySystemComponent_Implementation() const override { return CachedASC; }

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Abilities")
    void InitializeAbilityProvider(UAbilitySystemComponent* InASC);
    virtual void InitializeAbilityProvider_Implementation(UAbilitySystemComponent* InASC) override;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Abilities")
    FGameplayAbilitySpecHandle GrantAbility(TSubclassOf<UGameplayAbility> AbilityClass, int32 Level, int32 InputID);
    virtual FGameplayAbilitySpecHandle GrantAbility_Implementation(TSubclassOf<UGameplayAbility> AbilityClass, int32 Level, int32 InputID) override;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Abilities")
    void RemoveAbility(FGameplayAbilitySpecHandle AbilityHandle);
    virtual void RemoveAbility_Implementation(FGameplayAbilitySpecHandle AbilityHandle) override;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Abilities")
    FActiveGameplayEffectHandle ApplyEffectToSelf(TSubclassOf<UGameplayEffect> EffectClass, float Level);
    virtual FActiveGameplayEffectHandle ApplyEffectToSelf_Implementation(TSubclassOf<UGameplayEffect> EffectClass, float Level) override;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Abilities")
    void RemoveEffect(FActiveGameplayEffectHandle EffectHandle);
    virtual void RemoveEffect_Implementation(FActiveGameplayEffectHandle EffectHandle) override;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment")
    bool IsInitialized() const;
    virtual bool IsInitialized_Implementation() const override { return bIsInitialized; }

protected:
    virtual void OnEquipmentInitialized();
    virtual void OnEquippedItemChanged(const FSuspenseInventoryItemInstance& OldItem, const FSuspenseInventoryItemInstance& NewItem);
    bool ExecuteOnServer(const FString& FuncName, TFunction<void()> ServerCode);
    bool ValidateDelegateManager() const;
    void InitializeCoreReferences();
    bool ValidateSystemReferences() const;
    void LogEventBroadcast(const FString& EventName, bool bSuccess) const;

    UFUNCTION()
    void OnRep_EquippedItemInstance(const FSuspenseInventoryItemInstance& OldInstance);

    UFUNCTION()
    void OnRep_ComponentState();

protected:
    UPROPERTY(ReplicatedUsing=OnRep_ComponentState)
    bool bIsInitialized;

    UPROPERTY()
    FSuspenseInventoryItemInstance EquippedItemInstance;

    UPROPERTY(Replicated)
    uint8 ComponentVersion;

    UPROPERTY()
    UAbilitySystemComponent* CachedASC;

    UPROPERTY(Replicated)
    int32 EquipmentCycleCounter;

    UPROPERTY()
    mutable int32 BroadcastEventCounter;

    mutable FCriticalSection CacheCriticalSection;
    mutable TWeakObjectPtr<USuspenseItemManager> CachedItemManager;
    mutable TWeakObjectPtr<USuspenseEventManager> CachedDelegateManager;
    mutable float LastCacheValidationTime;

    UPROPERTY()
    TArray<FSuspenseCorePredictionData> ActivePredictions;

    UPROPERTY()
    int32 NextPredictionKey;

    static constexpr int32 MaxConcurrentPredictions = 5;
    static constexpr float PredictionTimeoutSeconds = 2.0f;
};
