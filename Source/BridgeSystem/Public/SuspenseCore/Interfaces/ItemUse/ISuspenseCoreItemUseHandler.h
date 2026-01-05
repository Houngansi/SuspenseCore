// ISuspenseCoreItemUseHandler.h
// Handler interface for Item Use operations
// Copyright Suspense Team. All Rights Reserved.
//
// ARCHITECTURE NOTE:
// This interface lives in BridgeSystem so both GAS and EquipmentSystem
// can reference it without creating circular dependencies.
//
// Handlers are registered with ItemUseService and process specific
// item type combinations (e.g., Ammo→Magazine, Medical use).
//
// USAGE:
//   class UMyHandler : public UObject, public ISuspenseCoreItemUseHandler
//   {
//       virtual FGameplayTag GetHandlerTag() const override;
//       virtual bool CanHandle(const FSuspenseCoreItemUseRequest& Request) const override;
//       virtual FSuspenseCoreItemUseResponse Execute(...) override;
//   };

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SuspenseCore/Types/ItemUse/SuspenseCoreItemUseTypes.h"
#include "ISuspenseCoreItemUseHandler.generated.h"

UINTERFACE(MinimalAPI, BlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class USuspenseCoreItemUseHandler : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISuspenseCoreItemUseHandler
 *
 * Interface for item use handlers.
 *
 * IMPLEMENTATION RULES (per SuspenseCoreArchitecture.md):
 * 1. Handlers MUST be stateless (no member variables that persist between calls)
 * 2. All state goes to ItemUseService or GAS ability
 * 3. Validation MUST be idempotent (same input = same output)
 * 4. Execute MUST publish events to EventBus on completion
 * 5. Time-based operations return InProgress and let ability handle timing
 *
 * BUILT-IN HANDLERS:
 * - AmmoToMagazineHandler: Load ammo into magazines (DragDrop)
 * - MagazineSwapHandler: QuickSlot magazine reload
 * - MedicalUseHandler: Medical item consumption
 * - GrenadeHandler: Grenade prepare/throw
 *
 * ARCHITECTURE:
 * - Defined in BridgeSystem (shared)
 * - Implemented in EquipmentSystem
 * - Used by GAS abilities
 */
class BRIDGESYSTEM_API ISuspenseCoreItemUseHandler
{
	GENERATED_BODY()

public:
	//==================================================================
	// Handler Identity
	//==================================================================

	/**
	 * Get unique handler tag
	 * Format: ItemUse.Handler.{HandlerName}
	 * Example: ItemUse.Handler.AmmoToMagazine
	 *
	 * @return Handler's unique GameplayTag
	 */
	virtual FGameplayTag GetHandlerTag() const = 0;

	/**
	 * Get handler priority for conflict resolution
	 * Higher priority handlers are checked first
	 * Default: Normal (50)
	 *
	 * @return Handler priority level
	 */
	virtual ESuspenseCoreHandlerPriority GetPriority() const
	{
		return ESuspenseCoreHandlerPriority::Normal;
	}

	/**
	 * Get display name for UI/debugging
	 *
	 * @return Human-readable handler name
	 */
	virtual FText GetDisplayName() const
	{
		return FText::FromString(GetHandlerTag().ToString());
	}

	//==================================================================
	// Supported Item Types
	//==================================================================

	/**
	 * Get item type tags this handler supports as SOURCE
	 * e.g., Item.Ammo for AmmoToMagazineHandler
	 * Return empty = handler doesn't filter by source type
	 *
	 * @return Container of supported source item tags
	 */
	virtual FGameplayTagContainer GetSupportedSourceTags() const = 0;

	/**
	 * Get item type tags this handler supports as TARGET (for DragDrop)
	 * Empty = handler doesn't require target (DoubleClick use)
	 * Return empty for single-item operations
	 *
	 * @return Container of supported target item tags
	 */
	virtual FGameplayTagContainer GetSupportedTargetTags() const
	{
		return FGameplayTagContainer();
	}

	/**
	 * Get supported use contexts
	 * Default: DoubleClick only
	 * Override for handlers that support multiple contexts
	 *
	 * @return Array of supported contexts
	 */
	virtual TArray<ESuspenseCoreItemUseContext> GetSupportedContexts() const
	{
		return { ESuspenseCoreItemUseContext::DoubleClick };
	}

	//==================================================================
	// Validation (MUST be idempotent)
	//==================================================================

	/**
	 * Quick check if this handler CAN process the request
	 * Called by ItemUseService to find appropriate handler
	 * MUST be fast - no complex validation here
	 *
	 * @param Request The use request to check
	 * @return true if this handler might be able to process this request
	 */
	virtual bool CanHandle(const FSuspenseCoreItemUseRequest& Request) const = 0;

	/**
	 * Full validation before execution
	 * Called after CanHandle returns true
	 *
	 * @param Request The use request
	 * @param OutResponse Pre-filled with error if validation fails
	 * @return true if request is valid for execution
	 */
	virtual bool ValidateRequest(
		const FSuspenseCoreItemUseRequest& Request,
		FSuspenseCoreItemUseResponse& OutResponse) const = 0;

	//==================================================================
	// Execution
	//==================================================================

	/**
	 * Execute the item use operation
	 * Called after ValidateRequest returns true
	 *
	 * For instant operations: Return Success, modify items immediately
	 * For time-based operations: Return InProgress with Duration set
	 *
	 * @param Request The validated use request
	 * @param OwnerActor Actor performing the use (for GAS/components)
	 * @return Response with result and modified items
	 */
	virtual FSuspenseCoreItemUseResponse Execute(
		const FSuspenseCoreItemUseRequest& Request,
		AActor* OwnerActor) = 0;

	/**
	 * Get expected duration for time-based operations
	 * Called before Execute to show UI feedback
	 *
	 * @param Request The use request
	 * @return Duration in seconds (0 = instant)
	 */
	virtual float GetDuration(const FSuspenseCoreItemUseRequest& Request) const
	{
		return 0.0f;
	}

	/**
	 * Get cooldown to apply after completion
	 * Used by GAS ability to apply cooldown effect
	 *
	 * @param Request The use request
	 * @return Cooldown in seconds (0 = no cooldown)
	 */
	virtual float GetCooldown(const FSuspenseCoreItemUseRequest& Request) const
	{
		return 0.0f;
	}

	/**
	 * Called when operation is cancelled mid-progress
	 * Only called for time-based operations (InProgress)
	 *
	 * @param RequestID The request to cancel
	 * @return true if cancelled, false if already completed or not found
	 */
	virtual bool CancelOperation(const FGuid& RequestID)
	{
		return false; // Default: not cancellable or instant
	}

	/**
	 * Is this handler's operations cancellable?
	 * If true, user can interrupt via damage/movement
	 *
	 * @return true if operations can be cancelled
	 */
	virtual bool IsCancellable() const { return false; }

	/**
	 * Called when time-based operation completes
	 * Override to finalize item state changes
	 *
	 * @param Request The original request
	 * @param OwnerActor Actor that performed the use
	 * @return Final response with modified items
	 */
	virtual FSuspenseCoreItemUseResponse OnOperationComplete(
		const FSuspenseCoreItemUseRequest& Request,
		AActor* OwnerActor)
	{
		return FSuspenseCoreItemUseResponse::Success(Request.RequestID);
	}
};
