// SuspenseCoreQuickSlotAbility.h
// QuickSlot ability for fast magazine/item access
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Abilities/Base/SuspenseCoreAbility.h"
#include "SuspenseCoreQuickSlotAbility.generated.h"

// Forward declaration - use interface instead of concrete type
class ISuspenseCoreQuickSlotProvider;

/**
 * USuspenseCoreQuickSlotAbility
 *
 * Base class for QuickSlot abilities (1-4).
 * Triggers quick use of assigned items:
 * - Magazines: Fast reload
 * - Consumables: Quick use (bandages, etc.)
 * - Grenades: Prepare throw
 *
 * ARCHITECTURE:
 * - Uses ISuspenseCoreQuickSlotProvider interface (BridgeSystem)
 * - No direct dependency on EquipmentSystem
 *
 * USAGE:
 * Create 4 instances with different SlotIndex (0-3)
 * Bind to Input QuickSlot1-4
 *
 * @see ISuspenseCoreQuickSlotProvider
 */
UCLASS()
class GAS_API USuspenseCoreQuickSlotAbility : public USuspenseCoreAbility
{
    GENERATED_BODY()

public:
    USuspenseCoreQuickSlotAbility();

    //==================================================================
    // Configuration
    //==================================================================

    /** Which QuickSlot this ability controls (0-3) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|QuickSlot",
        meta = (ClampMin = "0", ClampMax = "3"))
    int32 SlotIndex;

protected:
    //==================================================================
    // GameplayAbility Interface
    //==================================================================

    virtual bool CanActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags,
        const FGameplayTagContainer* TargetTags,
        FGameplayTagContainer* OptionalRelevantTags
    ) const override;

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData
    ) override;

private:
    /** Get QuickSlotProvider interface from owner */
    ISuspenseCoreQuickSlotProvider* GetQuickSlotProvider() const;
};

/**
 * Concrete implementations for each QuickSlot
 */
UCLASS()
class GAS_API USuspenseCoreQuickSlot1Ability : public USuspenseCoreQuickSlotAbility
{
    GENERATED_BODY()
public:
    USuspenseCoreQuickSlot1Ability()
    {
        SlotIndex = 0;
        AbilityInputID = ESuspenseCoreAbilityInputID::QuickSlot1;
    }
};

UCLASS()
class GAS_API USuspenseCoreQuickSlot2Ability : public USuspenseCoreQuickSlotAbility
{
    GENERATED_BODY()
public:
    USuspenseCoreQuickSlot2Ability()
    {
        SlotIndex = 1;
        AbilityInputID = ESuspenseCoreAbilityInputID::QuickSlot2;
    }
};

UCLASS()
class GAS_API USuspenseCoreQuickSlot3Ability : public USuspenseCoreQuickSlotAbility
{
    GENERATED_BODY()
public:
    USuspenseCoreQuickSlot3Ability()
    {
        SlotIndex = 2;
        AbilityInputID = ESuspenseCoreAbilityInputID::QuickSlot3;
    }
};

UCLASS()
class GAS_API USuspenseCoreQuickSlot4Ability : public USuspenseCoreQuickSlotAbility
{
    GENERATED_BODY()
public:
    USuspenseCoreQuickSlot4Ability()
    {
        SlotIndex = 3;
        AbilityInputID = ESuspenseCoreAbilityInputID::QuickSlot4;
    }
};
