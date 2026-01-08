// GA_WeaponSwitch.h
// Weapon slot switching ability
// Copyright Suspense Team. All Rights Reserved.
//
// ARCHITECTURE:
// - Uses ISuspenseCoreEquipmentDataProvider interface (BridgeSystem)
// - No direct dependency on EquipmentSystem
// - Follows same pattern as SuspenseCoreQuickSlotAbility
//
// USAGE:
// Concrete subclasses for each weapon slot are defined below:
// - UGA_WeaponSwitch_Primary (Key 1 → Slot 0)
// - UGA_WeaponSwitch_Secondary (Key 2 → Slot 1)
// - UGA_WeaponSwitch_Sidearm (Key 3 → Slot 2)
// - UGA_WeaponSwitch_Melee (Key V → Slot 3)

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Abilities/Base/SuspenseCoreAbility.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "GA_WeaponSwitch.generated.h"

class ISuspenseCoreEquipmentDataProvider;

/**
 * UGA_WeaponSwitch
 *
 * Base class for weapon slot switching abilities.
 * Triggers switch to assigned weapon slot via ISuspenseCoreEquipmentDataProvider.
 *
 * Pipeline:
 * 1. Input Key → GAS Activation
 * 2. CanActivate: Check slot has weapon && not already active
 * 3. Activate: SetActiveWeaponSlot() → EventBus publish
 * 4. End (instant)
 *
 * @see ISuspenseCoreEquipmentDataProvider
 * @see SuspenseCoreQuickSlotAbility (similar pattern)
 */
UCLASS(Abstract)
class GAS_API UGA_WeaponSwitch : public USuspenseCoreAbility
{
	GENERATED_BODY()

public:
	UGA_WeaponSwitch();

	/** Target weapon slot index (0-3) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|WeaponSwitch",
		meta = (ClampMin = "0", ClampMax = "3"))
	int32 TargetSlotIndex;

protected:
	//==================================================================
	// GameplayAbility Interface
	//==================================================================

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

private:
	/**
	 * Find ISuspenseCoreEquipmentDataProvider on PlayerState.
	 * DataStore component lives on PlayerState for persistence.
	 */
	ISuspenseCoreEquipmentDataProvider* GetEquipmentDataProvider() const;
};

//==================================================================
// Concrete Weapon Slot Abilities
//==================================================================

/**
 * Primary Weapon Switch (Key 1 → Slot 0)
 * Tag: SuspenseCore.Ability.WeaponSlot.Primary
 */
UCLASS()
class GAS_API UGA_WeaponSwitch_Primary : public UGA_WeaponSwitch
{
	GENERATED_BODY()

public:
	UGA_WeaponSwitch_Primary();
};

/**
 * Secondary Weapon Switch (Key 2 → Slot 1)
 * Tag: SuspenseCore.Ability.WeaponSlot.Secondary
 */
UCLASS()
class GAS_API UGA_WeaponSwitch_Secondary : public UGA_WeaponSwitch
{
	GENERATED_BODY()

public:
	UGA_WeaponSwitch_Secondary();
};

/**
 * Sidearm/Holster Switch (Key 3 → Slot 2)
 * Tag: SuspenseCore.Ability.WeaponSlot.Sidearm
 */
UCLASS()
class GAS_API UGA_WeaponSwitch_Sidearm : public UGA_WeaponSwitch
{
	GENERATED_BODY()

public:
	UGA_WeaponSwitch_Sidearm();
};

/**
 * Melee/Knife Switch (Key V → Slot 3)
 * Tag: SuspenseCore.Ability.WeaponSlot.Melee
 */
UCLASS()
class GAS_API UGA_WeaponSwitch_Melee : public UGA_WeaponSwitch
{
	GENERATED_BODY()

public:
	UGA_WeaponSwitch_Melee();
};
