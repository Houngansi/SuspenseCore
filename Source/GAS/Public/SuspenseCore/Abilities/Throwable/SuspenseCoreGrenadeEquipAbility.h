// SuspenseCoreGrenadeEquipAbility.h
// Ability for equipping grenades (Tarkov-style flow)
// Copyright Suspense Team. All Rights Reserved.
//
// ARCHITECTURE:
// This ability handles the EQUIP phase of grenade usage.
// Flow: QuickSlot Press -> GrenadeHandler -> GA_GrenadeEquip -> WeaponStance change
//
// TARKOV-STYLE GRENADE FLOW:
// 1. Press QuickSlot (4) -> Equip grenade (this ability)
//    - Changes WeaponStance to grenade type
//    - Plays Draw montage
//    - Grants State.GrenadeEquipped tag
//
// 2. Press Fire (LMB) -> Throw grenade (GA_GrenadeThrow)
//    - Pin pull animation
//    - Cooking mechanic
//    - Spawn projectile on release
//
// 3. Press QuickSlot again or Switch Weapon -> Unequip
//    - Play Holster montage
//    - Remove State.GrenadeEquipped tag
//    - Return to previous weapon
//
// USAGE:
// Activated by GrenadeHandler when Context == QuickSlot
// Requires: State.GrenadeEquipped NOT present (prevents re-equip spam)
// Grants: State.GrenadeEquipped while active

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Abilities/Base/SuspenseCoreAbility.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreGrenadeEquipAbility.generated.h"

// Forward declarations
class UAnimMontage;

/**
 * USuspenseCoreGrenadeEquipAbility
 *
 * Equips a grenade from QuickSlot, preparing for throw.
 * Does NOT throw the grenade - that's handled by GA_GrenadeThrow on Fire input.
 *
 * STATE MACHINE:
 * [Inactive] -> ActivateAbility() -> [Drawing] -> OnDrawComplete() -> [Equipped]
 * [Equipped] -> Fire Input -> GA_GrenadeThrow activates
 * [Equipped] -> Cancel/Switch -> EndAbility() -> [Inactive]
 *
 * REPLICATION:
 * - Server authoritative for state changes
 * - Client predicts montage playback
 * - WeaponStanceComponent handles replication of CurrentWeaponType
 *
 * @see USuspenseCoreGrenadeThrowAbility
 * @see USuspenseCoreGrenadeHandler
 * @see USuspenseCoreWeaponStanceComponent
 */
UCLASS()
class GAS_API USuspenseCoreGrenadeEquipAbility : public USuspenseCoreAbility
{
	GENERATED_BODY()

public:
	USuspenseCoreGrenadeEquipAbility();

	//==================================================================
	// Configuration - Grenade Identity
	//==================================================================

	/** Grenade ItemID from QuickSlot (set by GrenadeHandler before activation) */
	UPROPERTY(BlueprintReadWrite, Category = "SuspenseCore|Grenade")
	FName GrenadeID;

	/** Grenade type tag for animation selection (e.g., Weapon.Grenade.Frag) */
	UPROPERTY(BlueprintReadWrite, Category = "SuspenseCore|Grenade")
	FGameplayTag GrenadeTypeTag;

	/** QuickSlot index this grenade came from (for UI feedback) */
	UPROPERTY(BlueprintReadWrite, Category = "SuspenseCore|Grenade")
	int32 SourceQuickSlotIndex = -1;

	//==================================================================
	// Configuration - Animation
	//==================================================================

	/** Draw montage (fallback if not in DataTable) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Grenade|Animation")
	TObjectPtr<UAnimMontage> DefaultDrawMontage;

	/** Holster montage (fallback if not in DataTable) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Grenade|Animation")
	TObjectPtr<UAnimMontage> DefaultHolsterMontage;

	/** Draw montage play rate */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Grenade|Animation")
	float DrawMontagePlayRate = 1.0f;

	//==================================================================
	// Configuration - Timing
	//==================================================================

	/** Minimum time grenade must be equipped before throw (prevents instant throw) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Grenade|Timing")
	float MinEquipTime = 0.3f;

	//==================================================================
	// State Query
	//==================================================================

	/** Check if grenade is fully equipped and ready for throw */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Grenade")
	bool IsGrenadeReady() const { return bGrenadeReady; }

	/** Get time since grenade was equipped */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Grenade")
	float GetEquipTime() const;

	//==================================================================
	// Actions
	//==================================================================

	/** Called by GrenadeHandler to set grenade info before activation */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Grenade")
	void SetGrenadeInfo(FName InGrenadeID, FGameplayTag InGrenadeTypeTag, int32 InSlotIndex);

	/** Request unequip (holster grenade, return to previous weapon) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Grenade")
	void RequestUnequip();

	//==================================================================
	// Blueprint Events
	//==================================================================

	/** Called when draw animation completes and grenade is ready */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Grenade|Events")
	void OnGrenadeEquipped();

	/** Called when holster animation starts */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Grenade|Events")
	void OnGrenadeUnequipping();

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

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled
	) override;

	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;

	//==================================================================
	// Internal Methods
	//==================================================================

	/**
	 * Request stance change via EventBus
	 * WeaponStanceComponent listens for these events
	 * This decouples GAS from EquipmentSystem (avoids circular dependency)
	 */
	void RequestStanceChange(bool bEquipping);

	/** Play draw montage and wait for completion */
	void PlayDrawMontage();

	/** Called when draw montage completes */
	UFUNCTION()
	void OnDrawMontageCompleted();

	/** Called when draw montage is interrupted */
	UFUNCTION()
	void OnDrawMontageInterrupted();

	/** Play holster montage for unequip */
	void PlayHolsterMontage();

	/** Called when holster montage completes */
	UFUNCTION()
	void OnHolsterMontageCompleted();

	/** Store previous weapon type for return after unequip */
	void StorePreviousWeaponState();

	/** Restore previous weapon after unequip */
	void RestorePreviousWeaponState();

	/** Broadcast equip event to EventBus */
	void BroadcastEquipEvent(FGameplayTag EventTag);

private:
	//==================================================================
	// State
	//==================================================================

	/** True when draw montage completed and grenade is ready for throw */
	bool bGrenadeReady = false;

	/** True when unequip has been requested */
	bool bUnequipRequested = false;

	/** Time when grenade became equipped (for MinEquipTime check) */
	float EquipStartTime = 0.0f;

	/** Previous weapon type to restore after unequip */
	FGameplayTag PreviousWeaponType;

	/** Previous weapon drawn state */
	bool bPreviousWeaponDrawn = false;

	/** Montage end delegate handle */
	FDelegateHandle MontageEndedHandle;

	/** Active montage task (for cleanup) */
	UPROPERTY()
	TObjectPtr<class UAbilityTask_PlayMontageAndWait> ActiveMontageTask;
};
