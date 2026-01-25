// SuspenseCoreMedicalUseHandler.h
// Handler for medical item usage
// Copyright Suspense Team. All Rights Reserved.
//
// PURPOSE:
// Handles medical item consumption (bandages, medkits, painkillers, etc.)
// Supports multiple use contexts: DoubleClick, QuickSlot, Hotkey.
//
// FLOW (Timer-based - Legacy):
// 1. User double-clicks medical item or uses QuickSlot
// 2. ItemUseService routes to this handler
// 3. Handler calculates duration from item data
// 4. GAS ability applies InProgress effect and waits
// 5. OnOperationComplete() applies healing/effects via GAS
//
// FLOW (Animation-Driven - Tarkov-style):
// 1. QuickSlot Press -> GA_MedicalEquip -> State.Medical.Equipped
// 2. Fire (LMB) -> GA_MedicalUse -> AnimNotify "Apply" -> EventBus event
// 3. MedicalUseHandler.OnAnimationApplyEffect() applies healing/cure effects
// 4. GA_MedicalUse consumes item and cancels GA_MedicalEquip
//
// ARCHITECTURE:
// - Duration-based (configurable per item type)
// - Cancellable (damage interrupts)
// - Uses ItemData for heal amount and duration
// - Applies healing via GE_InstantHeal and GE_HealOverTime
// - Cures status effects (bleeding, fractures) based on item capabilities
//
// @see GE_InstantHeal, GE_HealOverTime, StatusEffects_DeveloperGuide.md

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SuspenseCore/Interfaces/ItemUse/ISuspenseCoreItemUseHandler.h"
#include "SuspenseCore/Tags/SuspenseCoreItemUseNativeTags.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreMedicalUseHandler.generated.h"

// Forward declarations
class USuspenseCoreDataManager;
class USuspenseCoreEventBus;
class UAbilitySystemComponent;
class UGameplayEffect;

/**
 * Medical item type for duration/effect calculation
 */
UENUM(BlueprintType)
enum class ESuspenseCoreMedicalType : uint8
{
	Bandage         UMETA(DisplayName = "Bandage"),
	Medkit          UMETA(DisplayName = "Medkit"),
	Painkiller      UMETA(DisplayName = "Painkiller"),
	Stimulant       UMETA(DisplayName = "Stimulant"),
	Splint          UMETA(DisplayName = "Splint"),
	Surgical        UMETA(DisplayName = "Surgical Kit")
};

/**
 * USuspenseCoreMedicalUseHandler
 *
 * Handler for medical item consumption.
 *
 * SUPPORTED OPERATIONS:
 * - DoubleClick: Use medical item from inventory
 * - QuickSlot: Quick use from QuickSlot
 * - Hotkey: Direct medical item use
 *
 * EFFECTS:
 * - Heals HP (amount from item data)
 * - May apply buffs (stimulants)
 * - May cure conditions (splints, surgical)
 * - Consumes item on completion
 *
 * @see ISuspenseCoreItemUseHandler
 */
UCLASS(BlueprintType)
class EQUIPMENTSYSTEM_API USuspenseCoreMedicalUseHandler : public UObject, public ISuspenseCoreItemUseHandler
{
	GENERATED_BODY()

public:
	USuspenseCoreMedicalUseHandler();

	//==================================================================
	// Initialization
	//==================================================================

	/**
	 * Initialize handler with dependencies
	 *
	 * @param InDataManager Data manager for item data
	 * @param InEventBus EventBus for publishing events
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemUse|Handler")
	void Initialize(USuspenseCoreDataManager* InDataManager, USuspenseCoreEventBus* InEventBus);

	//==================================================================
	// ISuspenseCoreItemUseHandler - Identity
	//==================================================================

	virtual FGameplayTag GetHandlerTag() const override;
	virtual ESuspenseCoreHandlerPriority GetPriority() const override;
	virtual FText GetDisplayName() const override;

	//==================================================================
	// ISuspenseCoreItemUseHandler - Supported Types
	//==================================================================

	virtual FGameplayTagContainer GetSupportedSourceTags() const override;
	virtual TArray<ESuspenseCoreItemUseContext> GetSupportedContexts() const override;

	//==================================================================
	// ISuspenseCoreItemUseHandler - Validation
	//==================================================================

	virtual bool CanHandle(const FSuspenseCoreItemUseRequest& Request) const override;
	virtual bool ValidateRequest(
		const FSuspenseCoreItemUseRequest& Request,
		FSuspenseCoreItemUseResponse& OutResponse) const override;

	//==================================================================
	// ISuspenseCoreItemUseHandler - Execution
	//==================================================================

	virtual FSuspenseCoreItemUseResponse Execute(
		const FSuspenseCoreItemUseRequest& Request,
		AActor* OwnerActor) override;

	virtual float GetDuration(const FSuspenseCoreItemUseRequest& Request) const override;
	virtual float GetCooldown(const FSuspenseCoreItemUseRequest& Request) const override;

	virtual bool CancelOperation(const FGuid& RequestID) override;
	virtual bool IsCancellable() const override { return true; }

	virtual FSuspenseCoreItemUseResponse OnOperationComplete(
		const FSuspenseCoreItemUseRequest& Request,
		AActor* OwnerActor) override;

protected:
	//==================================================================
	// Internal Methods
	//==================================================================

	/**
	 * Get medical type from item tags
	 *
	 * @param ItemTags Item's gameplay tags
	 * @return Medical type
	 */
	ESuspenseCoreMedicalType GetMedicalType(const FGameplayTagContainer& ItemTags) const;

	/**
	 * Get use duration for medical type
	 *
	 * @param Type Medical item type
	 * @return Duration in seconds
	 */
	float GetMedicalDuration(ESuspenseCoreMedicalType Type) const;

	/**
	 * Get heal amount from item data
	 *
	 * @param ItemID Item ID
	 * @return Heal amount
	 */
	float GetHealAmount(FName ItemID) const;

	/**
	 * Apply healing effect to actor via GAS
	 * Uses GE_InstantHeal GameplayEffect for immediate healing.
	 *
	 * @param Actor Actor to heal
	 * @param HealAmount Amount to heal
	 * @return true if healing was applied
	 */
	bool ApplyHealing(AActor* Actor, float HealAmount) const;

	/**
	 * Apply heal over time effect to actor
	 * Uses GE_HealOverTime GameplayEffect for sustained healing.
	 * HoT is interrupted by taking damage (Tarkov-style).
	 *
	 * @param Actor Actor to heal
	 * @param HealPerTick HP per tick (1s intervals)
	 * @param Duration Total HoT duration in seconds
	 * @return true if HoT was applied
	 */
	bool ApplyHealOverTime(AActor* Actor, float HealPerTick, float Duration) const;

	/**
	 * Cure bleeding effects based on medical item capabilities.
	 * Removes active bleeding effects matching the item's cure tags.
	 *
	 * @param Actor Actor to cure
	 * @param bCanCureLightBleed Can cure light bleeding
	 * @param bCanCureHeavyBleed Can cure heavy bleeding
	 * @return Number of bleeding effects removed
	 */
	int32 CureBleedingEffect(AActor* Actor, bool bCanCureLightBleed, bool bCanCureHeavyBleed) const;

	/**
	 * Cure fracture effects
	 *
	 * @param Actor Actor to cure
	 * @return Number of fracture effects removed
	 */
	int32 CureFractureEffect(AActor* Actor) const;

	/**
	 * Get medical capabilities from item data
	 * Returns what conditions the medical item can cure.
	 *
	 * @param ItemID Item to check
	 * @param OutCanCureLightBleed Output: can cure light bleed
	 * @param OutCanCureHeavyBleed Output: can cure heavy bleed
	 * @param OutCanCureFracture Output: can cure fractures
	 * @param OutHoTAmount Output: heal over time amount (0 if no HoT)
	 * @param OutHoTDuration Output: HoT duration in seconds
	 */
	void GetMedicalCapabilities(
		FName ItemID,
		bool& OutCanCureLightBleed,
		bool& OutCanCureHeavyBleed,
		bool& OutCanCureFracture,
		float& OutHoTAmount,
		float& OutHoTDuration) const;

	/**
	 * Publish medical use event
	 */
	void PublishMedicalEvent(
		const FSuspenseCoreItemUseRequest& Request,
		const FSuspenseCoreItemUseResponse& Response,
		AActor* OwnerActor);

	//==================================================================
	// Animation-Driven Flow Support (Tarkov-style)
	// Called by GA_MedicalUse when "Apply" AnimNotify fires
	// @see GA_MedicalUse, GA_MedicalEquip
	//==================================================================

	/**
	 * Subscribe to animation-driven events from EventBus
	 * Called during Initialize()
	 */
	void SetupAnimationEventSubscription();

	/**
	 * Unsubscribe from animation-driven events
	 */
	void TeardownAnimationEventSubscription();

	/**
	 * Handler for ApplyEffect event from GA_MedicalUse
	 * Called when "Apply" AnimNotify fires during use animation.
	 * Extracts item info from event data and applies healing/curing effects.
	 *
	 * @param EventTag The event tag (TAG_Event_Medical_ApplyEffect)
	 * @param EventData Event data containing MedicalItemID, OwnerActor, etc.
	 */
	void OnAnimationApplyEffect(FGameplayTag EventTag, const struct FSuspenseCoreEventData& EventData);

	/**
	 * Apply all medical effects for animation-driven flow
	 * Convenience method that applies instant heal, HoT, and cures in one call.
	 *
	 * @param Actor Target actor
	 * @param ItemID Medical item ID for capability lookup
	 */
	void ApplyMedicalEffectsFromAnimation(AActor* Actor, FName ItemID);

private:
	//==================================================================
	// Dependencies
	//==================================================================

	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreDataManager> DataManager;

	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> EventBus;

	//==================================================================
	// Configuration - Duration per type (seconds)
	//==================================================================

	UPROPERTY(EditDefaultsOnly, Category = "Handler|Config")
	float BandageDuration = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Handler|Config")
	float MedkitDuration = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Handler|Config")
	float PainkillerDuration = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Handler|Config")
	float StimulantDuration = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Handler|Config")
	float SplintDuration = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Handler|Config")
	float SurgicalDuration = 15.0f;

	/** Default cooldown after use (seconds) */
	UPROPERTY(EditDefaultsOnly, Category = "Handler|Config")
	float DefaultCooldown = 1.0f;

	//==================================================================
	// GameplayEffect Classes
	//==================================================================

	/** Instant heal effect class */
	UPROPERTY(EditDefaultsOnly, Category = "Handler|Effects")
	TSubclassOf<UGameplayEffect> InstantHealEffectClass;

	/** Heal over time effect class */
	UPROPERTY(EditDefaultsOnly, Category = "Handler|Effects")
	TSubclassOf<UGameplayEffect> HealOverTimeEffectClass;

	//==================================================================
	// HoT Configuration per Medical Type
	//==================================================================

	/** HoT heal per tick for Medkit */
	UPROPERTY(EditDefaultsOnly, Category = "Handler|HoT")
	float MedkitHoTPerTick = 5.0f;

	/** HoT duration for Medkit */
	UPROPERTY(EditDefaultsOnly, Category = "Handler|HoT")
	float MedkitHoTDuration = 10.0f;

	/** HoT heal per tick for Surgical Kit */
	UPROPERTY(EditDefaultsOnly, Category = "Handler|HoT")
	float SurgicalHoTPerTick = 10.0f;

	/** HoT duration for Surgical Kit */
	UPROPERTY(EditDefaultsOnly, Category = "Handler|HoT")
	float SurgicalHoTDuration = 15.0f;

	//==================================================================
	// Animation-Driven Flow State
	//==================================================================

	/** Subscription handle for ApplyEffect event */
	FSuspenseCoreSubscriptionHandle ApplyEffectSubscriptionHandle;
};
