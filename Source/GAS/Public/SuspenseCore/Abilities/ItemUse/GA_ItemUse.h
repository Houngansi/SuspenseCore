// GA_ItemUse.h
// Base Gameplay Ability for Item Use System
// Copyright Suspense Team. All Rights Reserved.
//
// ARCHITECTURE:
// - Uses ISuspenseCoreItemUseService via ServiceProvider
// - Handles both instant and time-based operations
// - Applies GE_ItemUse_InProgress during duration
// - Applies GE_ItemUse_Cooldown after completion
// - Can be cancelled by damage/movement tags
//
// USAGE:
// - Subclass or use GA_QuickSlotUse for QuickSlot integration
// - Override GetItemUseRequest() to customize request building
// - Configure blocking/cancel tags for interruption behavior

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Abilities/Base/SuspenseCoreAbility.h"
#include "SuspenseCore/Types/ItemUse/SuspenseCoreItemUseTypes.h"
#include "GA_ItemUse.generated.h"

// Forward declarations
class ISuspenseCoreItemUseService;
class UGameplayEffect;

/**
 * UGA_ItemUse
 *
 * Base ability for all item use operations.
 * Integrates with ItemUseService and handles both instant and time-based uses.
 *
 * FEATURES:
 * - Automatic service lookup via ServiceProvider
 * - Duration handling via GE_ItemUse_InProgress
 * - Cooldown handling via GE_ItemUse_Cooldown
 * - Cancellation support (damage/movement)
 * - EventBus integration
 *
 * TIME-BASED FLOW:
 * 1. ActivateAbility() calls ItemUseService->UseItem()
 * 2. If Duration > 0: Apply InProgress effect, start timer
 * 3. OnDurationComplete(): Call ItemUseService->CompleteOperation()
 * 4. Apply Cooldown effect
 * 5. EndAbility()
 *
 * INSTANT FLOW:
 * 1. ActivateAbility() calls ItemUseService->UseItem()
 * 2. If Duration == 0: Apply Cooldown immediately
 * 3. EndAbility()
 *
 * @see ISuspenseCoreItemUseService
 * @see GE_ItemUse_InProgress
 * @see GE_ItemUse_Cooldown
 */
UCLASS(Abstract, Blueprintable)
class GAS_API UGA_ItemUse : public USuspenseCoreAbility
{
	GENERATED_BODY()

public:
	UGA_ItemUse();

	//==================================================================
	// Configuration
	//==================================================================

	/** GameplayEffect class for in-progress state */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ItemUse|Effects")
	TSubclassOf<UGameplayEffect> InProgressEffectClass;

	/** GameplayEffect class for cooldown */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ItemUse|Effects")
	TSubclassOf<UGameplayEffect> CooldownEffectClass;

	/** Tags that will cancel the ability mid-use */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ItemUse|Cancellation")
	FGameplayTagContainer CancelOnTagsAdded;

	/** Whether this ability can be cancelled */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ItemUse|Cancellation")
	bool bIsCancellable = true;

	/** Whether to apply cooldown on cancel (vs only on success) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ItemUse|Cancellation")
	bool bApplyCooldownOnCancel = false;

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

	virtual void CancelAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateCancelAbility
	) override;

	//==================================================================
	// Request Building (Override in subclasses)
	//==================================================================

	/**
	 * Build the item use request for this ability activation.
	 * Override in subclasses to customize request building.
	 *
	 * @param ActorInfo Actor info from activation
	 * @param TriggerEventData Optional event data from trigger
	 * @return Built request, or invalid request to cancel activation
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "ItemUse")
	FSuspenseCoreItemUseRequest BuildItemUseRequest(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* TriggerEventData) const;

	/**
	 * Called when item use operation completes successfully.
	 * Override to add custom completion logic.
	 *
	 * @param Response Response from ItemUseService
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "ItemUse")
	void OnItemUseCompleted(const FSuspenseCoreItemUseResponse& Response);

	/**
	 * Called when item use operation fails.
	 * Override to add custom failure handling.
	 *
	 * @param Response Response from ItemUseService with failure reason
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "ItemUse")
	void OnItemUseFailed(const FSuspenseCoreItemUseResponse& Response);

	/**
	 * Called when item use operation is cancelled.
	 * Override to add custom cancellation handling.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "ItemUse")
	void OnItemUseCancelled();

	//==================================================================
	// Effects Application
	//==================================================================

	/**
	 * Apply the in-progress effect with specified duration.
	 *
	 * @param Duration Duration for the effect
	 * @return Handle to the applied effect
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemUse|Effects")
	FActiveGameplayEffectHandle ApplyInProgressEffect(float Duration);

	/**
	 * Apply the cooldown effect with specified duration.
	 *
	 * @param Cooldown Cooldown duration
	 * @return Handle to the applied effect
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemUse|Effects")
	FActiveGameplayEffectHandle ApplyCooldownEffect(float Cooldown);

	/**
	 * Remove the in-progress effect (on completion or cancel).
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemUse|Effects")
	void RemoveInProgressEffect();

	//==================================================================
	// Service Access
	//==================================================================

	/**
	 * Get ItemUseService via ServiceProvider.
	 * @return Service or nullptr
	 */
	ISuspenseCoreItemUseService* GetItemUseService() const;

	//==================================================================
	// Timer Handling
	//==================================================================

	/**
	 * Called when duration timer completes.
	 */
	UFUNCTION()
	void OnDurationTimerComplete();

private:
	//==================================================================
	// Active Operation State
	//==================================================================

	/** Current request being processed */
	UPROPERTY()
	FSuspenseCoreItemUseRequest CurrentRequest;

	/** Current response (updated as operation progresses) */
	UPROPERTY()
	FSuspenseCoreItemUseResponse CurrentResponse;

	/** Handle to in-progress effect */
	UPROPERTY()
	FActiveGameplayEffectHandle InProgressEffectHandle;

	/** Timer handle for duration */
	FTimerHandle DurationTimerHandle;

	/** Whether we're currently in a time-based operation */
	bool bIsInProgress = false;
};
