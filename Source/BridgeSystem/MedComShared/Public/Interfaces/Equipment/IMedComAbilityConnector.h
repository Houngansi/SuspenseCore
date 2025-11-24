// Copyright MedCom Team. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"
#include "Types/Inventory/InventoryTypes.h"
#include "IMedComAbilityConnector.generated.h"

class UAbilitySystemComponent;
class UAttributeSet;

/**
 * Ability connector interface to bridge equipment/inventory with GAS.
 */
UINTERFACE(BlueprintType, MinimalAPI)
class UMedComAbilityConnector : public UInterface
{
    GENERATED_BODY()
};

class MEDCOMSHARED_API IMedComAbilityConnector
{
    GENERATED_BODY()

public:
    /** Initialize connector with ASC and data provider */
    virtual bool Initialize(class UAbilitySystemComponent* InASC, TScriptInterface<class IMedComEquipmentDataProvider> InDataProvider) = 0;

    /** Grant abilities for specific item */
    virtual TArray<FGameplayAbilitySpecHandle> GrantEquipmentAbilities(const struct FInventoryItemInstance& ItemInstance) = 0;
    /** Remove previously granted abilities */
    virtual int32 RemoveGrantedAbilities(const TArray<FGameplayAbilitySpecHandle>& Handles) = 0;

    /** Apply passive effects for specific item */
    virtual TArray<FActiveGameplayEffectHandle> ApplyEquipmentEffects(const struct FInventoryItemInstance& ItemInstance) = 0;
    /** Remove previously applied effects */
    virtual int32 RemoveAppliedEffects(const TArray<FActiveGameplayEffectHandle>& Handles) = 0;

    /** Update attributes for specific item (creates/initializes AttributeSet if needed) */
    virtual bool UpdateEquipmentAttributes(const struct FInventoryItemInstance& ItemInstance) = 0;

    /** Get AttributeSet for slot */
    virtual UAttributeSet* GetEquipmentAttributeSet(int32 SlotIndex) const = 0;

    /** Activate granted ability by handle */
    virtual bool ActivateEquipmentAbility(const FGameplayAbilitySpecHandle& AbilityHandle) = 0;

    /** Clear everything (abilities/effects/managed attributes) */
    virtual void ClearAll() = 0;

    /** Cleanup invalid handles */
    virtual int32 CleanupInvalidHandles() = 0;

    /** Validate connector state */
    virtual bool ValidateConnector(TArray<FString>& OutErrors) const = 0;

    /** Dump debug info string */
    virtual FString GetDebugInfo() const = 0;

    /** Log compact statistics to log */
    virtual void LogStatistics() const = 0;
};
