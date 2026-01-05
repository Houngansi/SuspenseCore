// SuspenseCoreGrenadeHandler.h
// Handler for grenade prepare and throw
// Copyright Suspense Team. All Rights Reserved.
//
// PURPOSE:
// Handles grenade preparation and throwing operations.
// Two-phase operation: Prepare (hold) → Throw (release)
//
// FLOW:
// 1. User presses grenade key or uses QuickSlot
// 2. ItemUseService routes to this handler
// 3. Handler starts prepare phase (pin pull animation)
// 4. OnOperationComplete() transitions to throw phase
// 5. Grenade spawned and thrown in world
//
// ARCHITECTURE:
// - Duration-based prepare phase
// - Cancellable (damage interrupts, puts pin back)
// - Spawns grenade actor on completion

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SuspenseCore/Interfaces/ItemUse/ISuspenseCoreItemUseHandler.h"
#include "SuspenseCore/Tags/SuspenseCoreItemUseNativeTags.h"
#include "SuspenseCoreGrenadeHandler.generated.h"

// Forward declarations
class USuspenseCoreDataManager;
class USuspenseCoreEventBus;

/**
 * Grenade type for different behaviors
 */
UENUM(BlueprintType)
enum class ESuspenseCoreGrenadeType : uint8
{
	Fragmentation   UMETA(DisplayName = "Fragmentation"),
	Smoke           UMETA(DisplayName = "Smoke"),
	Flashbang       UMETA(DisplayName = "Flashbang"),
	Incendiary      UMETA(DisplayName = "Incendiary"),
	Impact          UMETA(DisplayName = "Impact")
};

/**
 * USuspenseCoreGrenadeHandler
 *
 * Handler for grenade prepare and throw operations.
 *
 * SUPPORTED OPERATIONS:
 * - Hotkey: Direct grenade throw
 * - QuickSlot: Quick grenade from slot
 *
 * PHASES:
 * 1. Prepare: Pull pin, ready to throw
 * 2. Throw: Release, grenade spawned
 *
 * REQUIREMENTS:
 * - Source: Item with Item.Category.Grenade tag
 * - Player not stunned/dead
 *
 * @see ISuspenseCoreItemUseHandler
 */
UCLASS(BlueprintType)
class EQUIPMENTSYSTEM_API USuspenseCoreGrenadeHandler : public UObject, public ISuspenseCoreItemUseHandler
{
	GENERATED_BODY()

public:
	USuspenseCoreGrenadeHandler();

	//==================================================================
	// Initialization
	//==================================================================

	/**
	 * Initialize handler with dependencies
	 *
	 * @param InDataManager Data manager for grenade data
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
	 * Get grenade type from item tags
	 *
	 * @param ItemTags Item's gameplay tags
	 * @return Grenade type
	 */
	ESuspenseCoreGrenadeType GetGrenadeType(const FGameplayTagContainer& ItemTags) const;

	/**
	 * Get prepare duration for grenade type
	 *
	 * @param Type Grenade type
	 * @return Duration in seconds
	 */
	float GetPrepareDuration(ESuspenseCoreGrenadeType Type) const;

	/**
	 * Spawn and throw grenade
	 *
	 * @param Request The use request
	 * @param OwnerActor Actor throwing the grenade
	 * @return true if grenade was thrown
	 */
	bool ThrowGrenade(
		const FSuspenseCoreItemUseRequest& Request,
		AActor* OwnerActor);

	/**
	 * Publish grenade event
	 */
	void PublishGrenadeEvent(
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

	/** Time to prepare grenade (pull pin) */
	UPROPERTY(EditDefaultsOnly, Category = "Handler|Config")
	float PrepareDuration = 0.5f;

	/** Cooldown after throw (seconds) */
	UPROPERTY(EditDefaultsOnly, Category = "Handler|Config")
	float ThrowCooldown = 1.0f;

	/** Default throw force */
	UPROPERTY(EditDefaultsOnly, Category = "Handler|Config")
	float DefaultThrowForce = 1500.0f;
};
