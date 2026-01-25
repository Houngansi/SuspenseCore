// GA_MedicalEquip.h
// Ability for equipping medical items (Tarkov-style flow)
// Copyright Suspense Team. All Rights Reserved.
//
// ARCHITECTURE:
// This ability handles the EQUIP phase of medical item usage.
// Flow: QuickSlot Press -> MedicalHandler -> GA_MedicalEquip -> Animation
//
// TARKOV-STYLE MEDICAL FLOW:
// 1. Press QuickSlot (5/6/7/etc) -> Equip medical item (this ability)
//    - Changes weapon stance to medical mode
//    - Plays draw montage
//    - Grants State.Medical.Equipped tag
//
// 2. Press Fire (LMB) -> Use medical item (GA_MedicalUse)
//    - Use animation with AnimNotify points
//    - Apply healing/cure effects at "Apply" notify
//    - Consume item on completion
//
// 3. Press QuickSlot again or Switch Weapon -> Unequip
//    - Play holster montage
//    - Remove State.Medical.Equipped tag
//    - Return to previous weapon
//
// USAGE:
// Activated by MedicalHandler when Context == QuickSlot
// Requires: State.Medical.Equipped NOT present (prevents re-equip spam)
// Grants: State.Medical.Equipped while active
//
// @see GA_MedicalUse
// @see SuspenseCoreMedicalUseHandler
// @see SuspenseCoreGrenadeEquipAbility (reference implementation)

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Abilities/Base/SuspenseCoreAbility.h"
#include "GameplayTagContainer.h"
#include "GA_MedicalEquip.generated.h"

// Forward declarations
class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;

/**
 * UGA_MedicalEquip
 *
 * Equips a medical item from QuickSlot, preparing for use.
 * Does NOT use the item - that's handled by GA_MedicalUse on Fire input.
 *
 * STATE MACHINE:
 * [Inactive] -> ActivateAbility() -> [Drawing] -> OnDrawComplete() -> [Equipped]
 * [Equipped] -> Fire Input -> GA_MedicalUse activates
 * [Equipped] -> Cancel/Switch -> EndAbility() -> [Inactive]
 *
 * REPLICATION:
 * - Server authoritative for state changes
 * - Client predicts montage playback
 *
 * @see UGA_MedicalUse
 * @see USuspenseCoreMedicalUseHandler
 */
UCLASS()
class GAS_API UGA_MedicalEquip : public USuspenseCoreAbility
{
	GENERATED_BODY()

public:
	UGA_MedicalEquip();

	//==================================================================
	// Configuration - Medical Item Identity
	//==================================================================

	/** Medical ItemID from QuickSlot (set by handler before activation) */
	UPROPERTY(BlueprintReadWrite, Category = "SuspenseCore|Medical")
	FName MedicalItemID;

	/** Medical type tag for animation selection (e.g., Item.Medical.Bandage) */
	UPROPERTY(BlueprintReadWrite, Category = "SuspenseCore|Medical")
	FGameplayTag MedicalTypeTag;

	/** QuickSlot index this item came from (for UI feedback) */
	UPROPERTY(BlueprintReadWrite, Category = "SuspenseCore|Medical")
	int32 SourceQuickSlotIndex = -1;

	//==================================================================
	// Configuration - Animation
	//==================================================================

	/** Draw montage (fallback if not in DataTable) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Medical|Animation")
	TObjectPtr<UAnimMontage> DefaultDrawMontage;

	/** Holster montage (fallback if not in DataTable) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Medical|Animation")
	TObjectPtr<UAnimMontage> DefaultHolsterMontage;

	/** Draw montage play rate */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Medical|Animation")
	float DrawMontagePlayRate = 1.0f;

	//==================================================================
	// Configuration - Timing
	//==================================================================

	/** Minimum time medical item must be equipped before use (prevents instant use) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Medical|Timing")
	float MinEquipTime = 0.2f;

	//==================================================================
	// State Query
	//==================================================================

	/** Check if medical item is fully equipped and ready for use */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Medical")
	bool IsMedicalReady() const { return bMedicalReady; }

	/** Get time since medical item was equipped */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Medical")
	float GetEquipTime() const;

	/** Get the currently equipped medical item ID */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Medical")
	FName GetMedicalItemID() const { return MedicalItemID; }

	//==================================================================
	// Actions
	//==================================================================

	/** Called by handler to set medical item info before activation */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Medical")
	void SetMedicalInfo(FName InMedicalItemID, FGameplayTag InMedicalTypeTag, int32 InSlotIndex);

	/** Request unequip (holster item, return to previous weapon) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Medical")
	void RequestUnequip();

	//==================================================================
	// Blueprint Events
	//==================================================================

	/** Called when draw animation completes and medical item is ready */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Medical|Events")
	void OnMedicalEquipped();

	/** Called when holster animation starts */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Medical|Events")
	void OnMedicalUnequipping();

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

	virtual void InputReleased(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo
	) override;

	//==================================================================
	// Internal Methods
	//==================================================================

	/**
	 * Request stance change via EventBus
	 * WeaponStanceComponent listens for these events
	 * This decouples GAS from EquipmentSystem
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

	/** Store previous weapon state for restore after unequip */
	void StorePreviousWeaponState();

	/** Restore previous weapon after unequip */
	void RestorePreviousWeaponState();

	/** Broadcast equip event to EventBus */
	void BroadcastEquipEvent(FGameplayTag EventTag);

private:
	//==================================================================
	// State
	//==================================================================

	/** True when draw montage completed and medical item is ready for use */
	bool bMedicalReady = false;

	/** True when unequip has been requested */
	bool bUnequipRequested = false;

	/** Time when medical item became equipped (for MinEquipTime check) */
	float EquipStartTime = 0.0f;

	/** Previous weapon type to restore after unequip */
	FGameplayTag PreviousWeaponType;

	/** Previous weapon drawn state */
	bool bPreviousWeaponDrawn = false;

	/** Active montage task (for cleanup) */
	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> ActiveMontageTask;
};
