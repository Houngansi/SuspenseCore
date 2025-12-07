// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Net/UnrealNetwork.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Types/Loadout/SuspenseLoadoutSettings.h"
#include "Types/Loadout/SuspenseItemDataTable.h"
#include "Interfaces/Equipment/ISuspenseEquipment.h"
#include "SuspenseCoreEquipmentActor.generated.h"

class UAbilitySystemComponent;
class UAttributeSet;
class USuspenseCoreEquipmentMeshComponent;
class USuspenseCoreEquipmentAttributeComponent;
class USuspenseCoreEquipmentAttachmentComponent;
class USuspenseItemManager;

/**
 * SuspenseCore Equipment Actor - Modern event-driven equipment implementation.
 *
 * ARCHITECTURE (following BestPractices.md):
 * - Event Bus: All state changes broadcast through centralized EventBus
 * - DI: Dependencies injected through ServiceLocator, not hard-coded
 * - Tags: GameplayTags for all state and type identification
 * - Services: Business logic delegated to services, actor is thin data holder
 *
 * Key Principles:
 * - No direct GA/GE/Attach calls - services handle these
 * - Initializes components from SSOT (Single Source of Truth)
 * - Publishes events to EventBus for decoupled communication
 * - Provides read-only/proxy Interface methods
 */
UCLASS(Blueprintable, BlueprintType)
class EQUIPMENTSYSTEM_API ASuspenseCoreEquipmentActor : public AActor, public ISuspenseEquipment
{
    GENERATED_BODY()

public:
    ASuspenseCoreEquipmentActor();

    // AActor
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** Explicitly cache owner's ASC (should be called by coordinator before OnEquipped) */
    UFUNCTION(BlueprintCallable, Category="SuspenseCore|Equipment|GAS")
    void SetCachedASC(UAbilitySystemComponent* InASC);

    // ===== ISuspenseEquipment (BlueprintNativeEvent) overrides =====
    virtual void OnEquipped_Implementation(AActor* NewOwner) override;
    virtual void OnUnequipped_Implementation() override;

    virtual void OnItemInstanceEquipped_Implementation(const FSuspenseInventoryItemInstance& ItemInstance) override;
    virtual void OnItemInstanceUnequipped_Implementation(const FSuspenseInventoryItemInstance& ItemInstance) override;

    virtual FSuspenseInventoryItemInstance GetEquippedItemInstance_Implementation() const override;
    virtual FEquipmentSlotConfig   GetSlotConfiguration_Implementation() const override;
    virtual EEquipmentSlotType     GetEquipmentSlotType_Implementation() const override;
    virtual FGameplayTag           GetEquipmentSlotTag_Implementation() const override;
    virtual bool                   IsEquipped_Implementation() const override;
    virtual bool                   IsRequiredSlot_Implementation() const override;
    virtual FText                  GetSlotDisplayName_Implementation() const override;

    virtual FName      GetAttachmentSocket_Implementation() const override;
    virtual FTransform GetAttachmentOffset_Implementation() const override;

    virtual bool CanEquipItemInstance_Implementation(const FSuspenseInventoryItemInstance& ItemInstance) const override;
    virtual FGameplayTagContainer GetAllowedItemTypes_Implementation() const override;
    virtual bool ValidateEquipmentRequirements_Implementation(TArray<FString>& OutErrors) const override;

    virtual FSuspenseInventoryOperationResult EquipItemInstance_Implementation(const FSuspenseInventoryItemInstance& ItemInstance, bool bForceEquip) override;
    virtual FSuspenseInventoryOperationResult UnequipItem_Implementation(FSuspenseInventoryItemInstance& OutUnequippedInstance) override;
    virtual FSuspenseInventoryOperationResult SwapEquipmentWith_Implementation(const TScriptInterface<ISuspenseEquipment>& OtherEquipment) override;

    // GAS bridge (read-only / proxy)
    virtual UAbilitySystemComponent* GetAbilitySystemComponent_Implementation() const override;
    virtual UAttributeSet*           GetEquipmentAttributeSet_Implementation() const override;
    virtual TArray<TSubclassOf<class UGameplayAbility>> GetGrantedAbilities_Implementation() const override;
    virtual TArray<TSubclassOf<class UGameplayEffect>>  GetPassiveEffects_Implementation() const override;

    // Effects management entrypoints (delegated to services)
    virtual void ApplyEquipmentEffects_Implementation() override;
    virtual void RemoveEquipmentEffects_Implementation() override;

    // State interface
    virtual FGameplayTag GetCurrentEquipmentState_Implementation() const override;
    virtual bool SetEquipmentState_Implementation(const FGameplayTag& NewState, bool bForceTransition) override;
    virtual bool IsInEquipmentState_Implementation(const FGameplayTag& StateTag) const override;
    virtual TArray<FGameplayTag> GetAvailableStateTransitions_Implementation() const override;

    // Runtime properties passthrough
    virtual float GetEquipmentRuntimeProperty_Implementation(const FName& PropertyName, float DefaultValue) const override;
    virtual void  SetEquipmentRuntimeProperty_Implementation(const FName& PropertyName, float Value) override;
    virtual float GetEquipmentConditionPercent_Implementation() const override;

    // Weapon helpers (read-only)
    virtual bool         IsWeaponEquipment_Implementation() const override;
    virtual FGameplayTag GetWeaponArchetype_Implementation() const override;
    virtual bool         CanFireWeapon_Implementation() const override;

    // Utility for spawn-side initialization
    UFUNCTION(BlueprintCallable, Category="SuspenseCore|Equipment|Init")
    bool InitializeFromItemInstance(const FSuspenseInventoryItemInstance& ItemInstance);

protected:
    /** Initialize all internal components from item instance (SSOT-driven) */
    void InitializeEquipmentComponents(const FSuspenseInventoryItemInstance& ItemInstance);

    /** Setup visual mesh defaults (does not attach) */
    void SetupEquipmentMesh(const FSuspenseUnifiedItemData& ItemData);

    /** Get slot configuration if available (base returns nullptr => default slot config) */
    const FEquipmentSlotConfig* GetSlotConfigurationPtr() const;

    /** Internal helper to change state without re-entry checks */
    void SetEquipmentStateInternal(const FGameplayTag& NewState);

    /** Replication notify: minimal item data (ID/Qty/Condition) */
    UFUNCTION()
    void OnRep_ItemData();

    /** Publish equipment event with optional payload pointer (item instance) */
    void NotifyEquipmentEvent(const FGameplayTag& EventTag, const FSuspenseInventoryItemInstance* Payload = nullptr) const;

    /** Publish property-changed (State) via Equipment.Event.PropertyChanged */
    void NotifyEquipmentStateChanged(const FGameplayTag& NewState, bool bIsRefresh) const;

    /** SSOT helper */
    bool GetUnifiedItemData(FSuspenseUnifiedItemData& OutData) const;

    /** Subsystem accessor */
    USuspenseItemManager* GetItemManager() const;

    // Services handle GA/GE - these are NO-OP stubs for compatibility
    void GrantAbilitiesFromItemData();
    void ApplyPassiveEffectsFromItemData();
    void ApplyInitializationEffects();
    void RemoveGrantedAbilities();
    void RemoveAppliedEffects();

protected:
    // Components - using SuspenseCore prefixed components
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SuspenseCore|Equipment|Components")
    USuspenseCoreEquipmentMeshComponent*        MeshComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SuspenseCore|Equipment|Components")
    USuspenseCoreEquipmentAttributeComponent*   AttributeComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SuspenseCore|Equipment|Components")
    USuspenseCoreEquipmentAttachmentComponent*  AttachmentComponent;

    // Owner & GAS
    UPROPERTY(Replicated)
    AActor* OwnerActor;

    UPROPERTY(Transient)
    UAbilitySystemComponent* CachedASC;

    // SSOT & runtime
    UPROPERTY(ReplicatedUsing=OnRep_ItemData)
    FName ReplicatedItemID = NAME_None;

    UPROPERTY(ReplicatedUsing=OnRep_ItemData)
    int32 ReplicatedItemQuantity = 0;

    UPROPERTY(ReplicatedUsing=OnRep_ItemData)
    float ReplicatedItemCondition = 0.f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="SuspenseCore|Equipment|Runtime")
    FSuspenseInventoryItemInstance EquippedItemInstance;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SuspenseCore|Equipment|Slot")
    FGameplayTag EquipmentSlotTag;

    UPROPERTY(Replicated)
    FGameplayTag CurrentState;

    // Book-keeping
    bool  bIsInitialized     = false;
    bool  bFullyInitialized  = false;
    int32 EquipmentCycleCounter = 0;

    // Local GA/GE handles kept only for safe cleanup if legacy calls happened
    TArray<FGameplayAbilitySpecHandle>   GrantedAbilityHandles;
    TArray<FActiveGameplayEffectHandle>  AppliedEffectHandles;

    // Pending init aggregator (owner + asc + item instance)
    struct FPendingInit
    {
        TWeakObjectPtr<AActor>                  PendingOwner;
        TWeakObjectPtr<UAbilitySystemComponent> PendingASC;
        FSuspenseInventoryItemInstance          PendingItemInstance;

        bool bHasOwnerData = false;
        bool bHasItemData  = false;

        void Reset()
        {
            PendingOwner.Reset();
            PendingASC.Reset();
            PendingItemInstance = FSuspenseInventoryItemInstance();
            bHasOwnerData = false;
            bHasItemData  = false;
        }

        bool IsReadyToInitialize() const
        {
            return bHasOwnerData && bHasItemData && PendingOwner.IsValid();
        }
    };

    FPendingInit PendingInit;

private:
    // Logging helpers
    bool CheckAuthority(const TCHAR* Ctx) const
    {
        if (!HasAuthority())
        {
            UE_LOG(LogTemp, Verbose, TEXT("[%s] executed on non-authority, skipping"), Ctx);
            return false;
        }
        return true;
    }
};
