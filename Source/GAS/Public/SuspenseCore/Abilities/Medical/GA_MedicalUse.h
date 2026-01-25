// GA_MedicalUse.h
// Medical item use ability with animation montage support
// Copyright Suspense Team. All Rights Reserved.
//
// ANIMATION NOTIFY POINTS:
// - "Start": Use animation started, item in hand
// - "Apply": Effects should be applied (healing, cure)
// - "Complete": Animation finished, ready for item consumption
//
// ARCHITECTURE:
// - Requires State.Medical.Equipped (granted by GA_MedicalEquip)
// - Uses ISuspenseCoreItemUseHandler via MedicalUseHandler for effects
// - Integrates with SuspenseCoreMedicalUseHandler for healing logic
// - No direct dependency on EquipmentSystem
//
// TARKOV-STYLE MEDICAL FLOW:
// 1. GA_MedicalEquip is active (State.Medical.Equipped granted)
// 2. Player presses Fire (LMB) -> GA_MedicalUse activates
// 3. Use montage plays with AnimNotify points
// 4. At "Apply" notify -> MedicalUseHandler applies healing/curing
// 5. At "Complete" notify -> Item consumed, GA_MedicalEquip cancelled
//
// CANCELLATION:
// - Damage interrupts use (Tarkov-style)
// - Movement/Sprint may interrupt (configurable)
// - If cancelled before "Apply", no effects applied, item not consumed
// - If cancelled after "Apply", effects applied, item consumed
//
// @see GA_MedicalEquip
// @see SuspenseCoreMedicalUseHandler
// @see SuspenseCoreGrenadeThrowAbility (reference implementation)

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Abilities/Base/SuspenseCoreAbility.h"
#include "SuspenseCore/Types/ItemUse/SuspenseCoreItemUseTypes.h"
#include "GameplayTagContainer.h"
#include "Animation/AnimInstance.h"
#include "GA_MedicalUse.generated.h"

// Forward declarations
class UAnimMontage;
class USuspenseCoreMedicalUseHandler;
class UGameplayEffect;
class USoundBase;

/**
 * UGA_MedicalUse
 *
 * GAS ability for using medical items with full animation support.
 * Handles the complete use sequence: start → apply effects → complete.
 *
 * USE PHASES:
 * 1. Start - Begin use animation, can be cancelled (item not consumed)
 * 2. Apply - Apply healing/cure effects (past point of no return for effects)
 * 3. Complete - Animation done, consume item
 *
 * BLOCKING TAGS:
 * - State.Firing (can't use while firing)
 * - State.Reloading (can't use while reloading)
 * - State.Dead, State.Stunned, State.Disabled
 *
 * REQUIRED TAGS:
 * - State.Medical.Equipped (must have medical item equipped first)
 *
 * @see GA_MedicalEquip - Equips medical item
 * @see SuspenseCoreMedicalUseHandler - Applies effects
 */
UCLASS()
class GAS_API UGA_MedicalUse : public USuspenseCoreAbility
{
	GENERATED_BODY()

public:
	UGA_MedicalUse();

	//==================================================================
	// Configuration - Montages
	//==================================================================

	/** Default use montage (fallback if not in DataTable) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Medical|Montages")
	TObjectPtr<UAnimMontage> DefaultUseMontage;

	/** Bandage-specific use montage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Medical|Montages")
	TObjectPtr<UAnimMontage> BandageUseMontage;

	/** Medkit-specific use montage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Medical|Montages")
	TObjectPtr<UAnimMontage> MedkitUseMontage;

	/** Injector/Stimulant use montage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Medical|Montages")
	TObjectPtr<UAnimMontage> InjectorUseMontage;

	//==================================================================
	// Configuration - Sounds
	//==================================================================

	/** Sound when use starts */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Medical|Sounds")
	TObjectPtr<USoundBase> UseStartSound;

	/** Sound when effects are applied */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Medical|Sounds")
	TObjectPtr<USoundBase> ApplySound;

	/** Sound when use is completed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Medical|Sounds")
	TObjectPtr<USoundBase> CompleteSound;

	/** Sound when use is cancelled */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Medical|Sounds")
	TObjectPtr<USoundBase> CancelSound;

	//==================================================================
	// Configuration - Effects
	//==================================================================

	/** GameplayEffect for speed debuff while using (optional) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Medical|Effects")
	TSubclassOf<UGameplayEffect> UseSpeedDebuffClass;

	/** Whether damage cancels the use (Tarkov-style) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Medical|Cancellation")
	bool bCancelOnDamage = true;

	/** Whether sprinting cancels the use */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Medical|Cancellation")
	bool bCancelOnSprint = true;

	//==================================================================
	// Runtime Accessors
	//==================================================================

	/** Check if use animation is in progress */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Medical")
	bool IsUsing() const { return bIsUsing; }

	/** Check if effects have been applied */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Medical")
	bool HasAppliedEffects() const { return bEffectsApplied; }

	/** Get time since use started */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Medical")
	float GetUseTime() const;

	/**
	 * Set medical item info from GA_MedicalEquip
	 * Called by handler when activating use from equipped state
	 *
	 * @param InMedicalItemID Item ID of the medical item
	 * @param InSlotIndex QuickSlot index the item came from
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Medical")
	void SetMedicalInfo(FName InMedicalItemID, int32 InSlotIndex);

	/** Check if medical info was set externally */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Medical")
	bool HasMedicalInfoSet() const { return bMedicalInfoSet; }

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
	// Animation Notify Handlers
	//==================================================================

	/** Called when "Start" notify fires - use animation begins */
	UFUNCTION()
	void OnStartNotify();

	/** Called when "Apply" notify fires - apply healing/cure effects */
	UFUNCTION()
	void OnApplyNotify();

	/** Called when "Complete" notify fires - consume item */
	UFUNCTION()
	void OnCompleteNotify();

	/** Called when montage ends */
	UFUNCTION()
	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** Called when montage blend out starts */
	UFUNCTION()
	void OnMontageBlendOut(UAnimMontage* Montage, bool bInterrupted);

	/** Handles all montage AnimNotify events */
	UFUNCTION()
	void OnAnimNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);

	//==================================================================
	// Blueprint Events
	//==================================================================

	/** Called when use starts */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Medical")
	void OnUseStarted();

	/** Called when effects are applied */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Medical")
	void OnEffectsApplied();

	/** Called when use completes successfully */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Medical")
	void OnUseCompleted();

	/** Called when use is cancelled */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Medical")
	void OnUseCancelled();

private:
	//==================================================================
	// Internal Methods
	//==================================================================

	/** Get montage for current medical item type */
	UAnimMontage* GetMontageForMedicalType() const;

	/** Start playing use montage */
	bool PlayUseMontage();

	/** Stop use montage */
	void StopUseMontage();

	/** Apply medical effects via MedicalUseHandler */
	bool ApplyMedicalEffects();

	/** Consume the medical item from QuickSlot */
	void ConsumeMedicalItem();

	/** Apply use effects (speed debuff) */
	void ApplyUseEffects();

	/** Remove use effects */
	void RemoveUseEffects();

	/** Play sound at owner location */
	void PlaySound(USoundBase* Sound);

	/** Broadcast medical events via EventBus */
	void BroadcastMedicalEvent(FGameplayTag EventTag);

	/** Get MedicalUseHandler from ServiceProvider */
	USuspenseCoreMedicalUseHandler* GetMedicalUseHandler() const;

	/** Cancel GA_MedicalEquip ability after use completes */
	void CancelEquipAbility();

	//==================================================================
	// Runtime State
	//==================================================================

	/** Is use animation in progress */
	UPROPERTY()
	bool bIsUsing = false;

	/** Have effects been applied */
	UPROPERTY()
	bool bEffectsApplied = false;

	/** Was medical info set externally */
	UPROPERTY()
	bool bMedicalInfoSet = false;

	/** Time when use started */
	UPROPERTY()
	float UseStartTime = 0.0f;

	/** QuickSlot index of current medical item */
	UPROPERTY()
	int32 CurrentSlotIndex = -1;

	/** ItemID of current medical item */
	UPROPERTY()
	FName CurrentMedicalItemID;

	/** Medical type tag for montage selection */
	UPROPERTY()
	FGameplayTag CurrentMedicalTypeTag;

	/** Cached ActorInfo */
	const FGameplayAbilityActorInfo* CachedActorInfo = nullptr;

	/** Cached ability handle */
	FGameplayAbilitySpecHandle CachedSpecHandle;

	/** Cached activation info */
	FGameplayAbilityActivationInfo CachedActivationInfo;

	/** Handle for speed debuff effect */
	FActiveGameplayEffectHandle UseSpeedEffectHandle;

	/** Cached AnimInstance for montage notify binding */
	UPROPERTY()
	TWeakObjectPtr<UAnimInstance> CachedAnimInstance;
};
