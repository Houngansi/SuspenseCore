// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Net/UnrealNetwork.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreLoadoutSettings.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreItemDataTable.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipment.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUIContainerTypes.h"
#include "SuspenseCoreEquipmentActor.generated.h"

class UAbilitySystemComponent;
class UAttributeSet;
class USuspenseCoreEquipmentMeshComponent;
class USuspenseCoreEquipmentAttributeComponent;
class USuspenseCoreEquipmentAttachmentComponent;
class USuspenseCoreItemManager;

/**
 * Thin equipment actor (S3): bridge between SSOT/data and services.
 * - No direct GA/GE/Attach calls here.
 * - Initializes its components from SSOT.
 * - Publishes events to EventBus.
 * - Provides read-only/proxy Interface methods.
 */
UCLASS(Blueprintable, BlueprintType)
class EQUIPMENTSYSTEM_API ASuspenseCoreEquipmentActor : public AActor, public ISuspenseCoreEquipment
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

    // ===== ISuspenseCoreEquipment (BlueprintNativeEvent) overrides =====
    virtual void OnEquipped_Implementation(AActor* NewOwner) override;
    virtual void OnUnequipped_Implementation() override;

    virtual void OnItemInstanceEquipped_Implementation(const FSuspenseCoreInventoryItemInstance& ItemInstance) override;
    virtual void OnItemInstanceUnequipped_Implementation(const FSuspenseCoreInventoryItemInstance& ItemInstance) override;

    virtual FSuspenseCoreInventoryItemInstance GetEquippedItemInstance_Implementation() const override;
    virtual FSuspenseCoreEquipmentSlotConfig   GetSlotConfiguration_Implementation() const override;
    virtual ESuspenseCoreEquipmentSlotType     GetEquipmentSlotType_Implementation() const override;
    virtual FGameplayTag           GetEquipmentSlotTag_Implementation() const override;
    virtual bool                   IsEquipped_Implementation() const override;
    virtual bool                   IsRequiredSlot_Implementation() const override;
    virtual FText                  GetSlotDisplayName_Implementation() const override;

    virtual FName      GetAttachmentSocket_Implementation() const override;
    virtual FTransform GetAttachmentOffset_Implementation() const override;

    virtual bool CanEquipItemInstance_Implementation(const FSuspenseCoreInventoryItemInstance& ItemInstance) const override;
    virtual FGameplayTagContainer GetAllowedItemTypes_Implementation() const override;
    virtual bool ValidateEquipmentRequirements_Implementation(TArray<FString>& OutErrors) const override;

    virtual FSuspenseInventoryOperationResult EquipItemInstance_Implementation(const FSuspenseCoreInventoryItemInstance& ItemInstance, bool bForceEquip) override;
    virtual FSuspenseInventoryOperationResult UnequipItem_Implementation(FSuspenseCoreInventoryItemInstance& OutUnequippedInstance) override;
    virtual FSuspenseInventoryOperationResult SwapEquipmentWith_Implementation(const TScriptInterface<ISuspenseCoreEquipment>& OtherEquipment) override;

    // GAS bridge (read-only / proxy)
    virtual UAbilitySystemComponent* GetAbilitySystemComponent_Implementation() const override;
    virtual UAttributeSet*           GetEquipmentAttributeSet_Implementation() const override;
    virtual TArray<TSubclassOf<class UGameplayAbility>> GetGrantedAbilities_Implementation() const override;
    virtual TArray<TSubclassOf<class UGameplayEffect>>  GetPassiveEffects_Implementation() const override;

    // Effects management entrypoints (now NO-OP for S3)
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
    bool InitializeFromItemInstance(const FSuspenseCoreInventoryItemInstance& ItemInstance);

protected:
    /** Initialize all internal components from item instance (SSOT-driven) */
    void InitializeEquipmentComponents(const FSuspenseCoreInventoryItemInstance& ItemInstance);

    /** Setup visual mesh defaults (does not attach) */
    void SetupEquipmentMesh(const FSuspenseCoreUnifiedItemData& ItemData);

    /** Get slot configuration if available (base returns nullptr => default slot config) */
    const FSuspenseCoreEquipmentSlotConfig* GetSlotConfigurationPtr() const;

    /** Internal helper to change state without re-entry checks */
    void SetEquipmentStateInternal(const FGameplayTag& NewState);

    /** Replication notify: minimal item data (ID/Qty/Condition) */
    UFUNCTION()
    void OnRep_ItemData();

    /** Publish equipment event with optional payload pointer (item instance) */
    void NotifyEquipmentEvent(const FGameplayTag& EventTag, const FSuspenseCoreInventoryItemInstance* Payload = nullptr) const;

    /** Publish property-changed (State) via Equipment.Event.PropertyChanged */
    void NotifyEquipmentStateChanged(const FGameplayTag& NewState, bool bIsRefresh) const;

    /** SSOT helper */
    bool GetUnifiedItemData(FSuspenseCoreUnifiedItemData& OutData) const;

    /** Subsystem accessor */
    USuspenseCoreItemManager* GetItemManager() const;

    // ===== S3: GA/GE hooks now NO-OP (keep for compatibility) =====
    void GrantAbilitiesFromItemData();          // NO-OP
    void ApplyPassiveEffectsFromItemData();     // NO-OP
    void ApplyInitializationEffects();          // NO-OP
    void RemoveGrantedAbilities();              // NO-OP
    void RemoveAppliedEffects();                // NO-OP

protected:
    // Components
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
    FSuspenseCoreInventoryItemInstance EquippedItemInstance;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SuspenseCore|Equipment|Slot")
    FGameplayTag EquipmentSlotTag;

    UPROPERTY(Replicated)
    FGameplayTag CurrentState;

    // Book-keeping
    bool  bIsInitialized     = false;
    bool  bFullyInitialized  = false;
    int32 EquipmentCycleCounter = 0;

    // Local GA/GE handles kept only for safe cleanup if legacy calls happened (will be empty in S3)
    TArray<FGameplayAbilitySpecHandle>   GrantedAbilityHandles;
    TArray<FActiveGameplayEffectHandle>  AppliedEffectHandles;

    // Pending init aggregator (owner + asc + item instance)
    struct FPendingInit
    {
        TWeakObjectPtr<AActor>                  PendingOwner;
        TWeakObjectPtr<UAbilitySystemComponent> PendingASC;
        FSuspenseCoreInventoryItemInstance                  PendingItemInstance;

        bool bHasOwnerData = false;
        bool bHasItemData  = false;

        void Reset()
        {
            PendingOwner.Reset();
            PendingASC.Reset();
            PendingItemInstance = FSuspenseCoreInventoryItemInstance();
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
    // Logging helpers (category minimal)
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
