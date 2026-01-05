// SuspenseCoreAmmoToMagazineHandler.h
// Handler for loading ammo into magazines via DragDrop
// Copyright Suspense Team. All Rights Reserved.
//
// PURPOSE:
// Handles Ammo → Magazine loading operations triggered by drag&drop UI.
// Replaces tick-based AmmoLoadingService logic with GAS-driven timing.
//
// FLOW:
// 1. User drags ammo stack onto magazine in inventory
// 2. UI creates ItemUseRequest (Context = DragDrop)
// 3. ItemUseService routes to this handler
// 4. Handler calculates duration (TimePerRound * RoundsToLoad)
// 5. GAS ability applies InProgress effect and waits
// 6. OnOperationComplete() transfers rounds from ammo to magazine
//
// ARCHITECTURE:
// - Stateless handler (no member state)
// - Uses DataManager for magazine data
// - Duration-based (not instant)
// - Cancellable (damage interrupts)

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SuspenseCore/Interfaces/ItemUse/ISuspenseCoreItemUseHandler.h"
#include "SuspenseCore/Tags/SuspenseCoreItemUseNativeTags.h"
#include "SuspenseCoreAmmoToMagazineHandler.generated.h"

// Forward declarations
class USuspenseCoreDataManager;
class USuspenseCoreEventBus;
struct FSuspenseCoreMagazineData;

/**
 * USuspenseCoreAmmoToMagazineHandler
 *
 * Handler for loading ammo into magazines via drag&drop.
 *
 * SUPPORTED OPERATIONS:
 * - DragDrop: Ammo stack → Magazine
 * - Calculates duration based on TimePerRound * RoundsToLoad
 * - Transfers rounds from ammo stack to magazine
 *
 * REQUIREMENTS:
 * - Source: Item with Item.Ammo.* tag
 * - Target: Item with Item.Category.Magazine tag
 * - Ammo caliber must match magazine caliber
 * - Magazine must have space
 *
 * @see ISuspenseCoreItemUseHandler
 * @see FSuspenseCoreMagazineData
 */
UCLASS(BlueprintType)
class EQUIPMENTSYSTEM_API USuspenseCoreAmmoToMagazineHandler : public UObject, public ISuspenseCoreItemUseHandler
{
	GENERATED_BODY()

public:
	USuspenseCoreAmmoToMagazineHandler();

	//==================================================================
	// Initialization
	//==================================================================

	/**
	 * Initialize handler with dependencies
	 *
	 * @param InDataManager Data manager for magazine/ammo data
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
	virtual FGameplayTagContainer GetSupportedTargetTags() const override;
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
	 * Calculate number of rounds to load
	 *
	 * @param AmmoQuantity Available ammo quantity
	 * @param CurrentRounds Current rounds in magazine
	 * @param MaxCapacity Magazine max capacity
	 * @return Number of rounds to load
	 */
	int32 CalculateRoundsToLoad(int32 AmmoQuantity, int32 CurrentRounds, int32 MaxCapacity) const;

	/**
	 * Get magazine data from DataManager
	 *
	 * @param MagazineID Magazine ID
	 * @param OutData Output magazine data
	 * @return true if found
	 */
	bool GetMagazineData(FName MagazineID, FSuspenseCoreMagazineData& OutData) const;

	/**
	 * Check if ammo caliber is compatible with magazine
	 *
	 * @param AmmoCaliber Ammo caliber tag
	 * @param MagazineCaliber Magazine caliber tag
	 * @return true if compatible
	 */
	bool IsCaliberCompatible(const FGameplayTag& AmmoCaliber, const FGameplayTag& MagazineCaliber) const;

	/**
	 * Publish loading event to EventBus
	 *
	 * @param Request The request
	 * @param Response The response
	 * @param OwnerActor Owner actor
	 */
	void PublishLoadEvent(
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
	// Configuration
	//==================================================================

	/** Default cooldown after loading (seconds) */
	UPROPERTY(EditDefaultsOnly, Category = "Handler|Config")
	float DefaultCooldown = 0.5f;
};
