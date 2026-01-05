// SuspenseCoreMagazineSwapHandler.h
// Handler for quick magazine swap via QuickSlot
// Copyright Suspense Team. All Rights Reserved.
//
// PURPOSE:
// Handles QuickSlot magazine swap operations (keys 4-7).
// Swaps current weapon magazine with magazine from QuickSlot.
//
// FLOW:
// 1. User presses QuickSlot key (4-7)
// 2. GA_QuickSlotUse creates request (Context = QuickSlot)
// 3. ItemUseService routes to this handler
// 4. Handler validates compatibility and executes swap
// 5. Current magazine goes to inventory/QuickSlot, new magazine equipped
//
// ARCHITECTURE:
// - Uses ISuspenseCoreQuickSlotProvider interface
// - Uses ISuspenseCoreMagazineComponent interface
// - Instant operation (no duration)

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SuspenseCore/Interfaces/ItemUse/ISuspenseCoreItemUseHandler.h"
#include "SuspenseCore/Tags/SuspenseCoreItemUseNativeTags.h"
#include "SuspenseCoreMagazineSwapHandler.generated.h"

// Forward declarations
class USuspenseCoreEventBus;
class ISuspenseCoreQuickSlotProvider;

/**
 * USuspenseCoreMagazineSwapHandler
 *
 * Handler for quick magazine swap via QuickSlot keys.
 *
 * SUPPORTED OPERATIONS:
 * - QuickSlot: Magazine in slot → Current weapon
 * - Swaps current magazine back to QuickSlot or inventory
 *
 * REQUIREMENTS:
 * - Source: Magazine in QuickSlot
 * - Magazine must be compatible with current weapon
 * - Weapon must have magazine slot
 *
 * @see ISuspenseCoreItemUseHandler
 * @see ISuspenseCoreQuickSlotProvider
 */
UCLASS(BlueprintType)
class EQUIPMENTSYSTEM_API USuspenseCoreMagazineSwapHandler : public UObject, public ISuspenseCoreItemUseHandler
{
	GENERATED_BODY()

public:
	USuspenseCoreMagazineSwapHandler();

	//==================================================================
	// Initialization
	//==================================================================

	/**
	 * Initialize handler with dependencies
	 *
	 * @param InEventBus EventBus for publishing events
	 */
	UFUNCTION(BlueprintCallable, Category = "ItemUse|Handler")
	void Initialize(USuspenseCoreEventBus* InEventBus);

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

	virtual bool IsCancellable() const override { return false; } // Instant operation

protected:
	//==================================================================
	// Internal Methods
	//==================================================================

	/**
	 * Get QuickSlotProvider from actor
	 *
	 * @param Actor Actor to search
	 * @return Provider interface or nullptr
	 */
	ISuspenseCoreQuickSlotProvider* GetQuickSlotProvider(AActor* Actor) const;

	/**
	 * Publish swap event to EventBus
	 */
	void PublishSwapEvent(
		const FSuspenseCoreItemUseRequest& Request,
		const FSuspenseCoreItemUseResponse& Response,
		AActor* OwnerActor);

private:
	//==================================================================
	// Dependencies
	//==================================================================

	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> EventBus;

	//==================================================================
	// Configuration
	//==================================================================

	/** Cooldown after swap (seconds) */
	UPROPERTY(EditDefaultsOnly, Category = "Handler|Config")
	float SwapCooldown = 0.5f;
};
