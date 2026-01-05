// SuspenseCoreMedicalUseHandler.h
// Handler for medical item usage
// Copyright Suspense Team. All Rights Reserved.
//
// PURPOSE:
// Handles medical item consumption (bandages, medkits, painkillers, etc.)
// Supports multiple use contexts: DoubleClick, QuickSlot, Hotkey.
//
// FLOW:
// 1. User double-clicks medical item or uses QuickSlot
// 2. ItemUseService routes to this handler
// 3. Handler calculates duration from item data
// 4. GAS ability applies InProgress effect and waits
// 5. OnOperationComplete() applies healing/effects
//
// ARCHITECTURE:
// - Duration-based (configurable per item type)
// - Cancellable (damage interrupts)
// - Uses ItemData for heal amount and duration

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SuspenseCore/Interfaces/ItemUse/ISuspenseCoreItemUseHandler.h"
#include "SuspenseCore/Tags/SuspenseCoreItemUseNativeTags.h"
#include "SuspenseCoreMedicalUseHandler.generated.h"

// Forward declarations
class USuspenseCoreDataManager;
class USuspenseCoreEventBus;
class UAbilitySystemComponent;

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
	 * Apply healing effect to actor
	 *
	 * @param Actor Actor to heal
	 * @param HealAmount Amount to heal
	 * @return true if healing was applied
	 */
	bool ApplyHealing(AActor* Actor, float HealAmount) const;

	/**
	 * Publish medical use event
	 */
	void PublishMedicalEvent(
		const FSuspenseCoreItemUseRequest& Request,
		const FSuspenseCoreItemUseResponse& Response,
		AActor* OwnerActor);

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
};
