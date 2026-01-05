// ISuspenseCoreItemUseService.h
// Service interface for Item Use System
// Copyright Suspense Team. All Rights Reserved.
//
// SSOT (Single Source of Truth) for all item use operations
// Per SuspenseCoreArchitecture.md §SSOT Pattern
//
// ARCHITECTURE:
// - Interface defined in BridgeSystem
// - Implementation in EquipmentSystem (USuspenseCoreItemUseService)
// - Accessed via ServiceProvider
//
// USAGE:
//   USuspenseCoreServiceProvider* Provider = USuspenseCoreServiceProvider::Get(this);
//   ISuspenseCoreItemUseService* Service = Provider->GetItemUseService();
//
//   FSuspenseCoreItemUseRequest Request;
//   Request.SourceItem = MyItem;
//   Request.Context = ESuspenseCoreItemUseContext::QuickSlot;
//
//   if (Service->CanUseItem(Request))
//   {
//       FSuspenseCoreItemUseResponse Response = Service->UseItem(Request, GetOwner());
//   }

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SuspenseCore/Types/ItemUse/SuspenseCoreItemUseTypes.h"
#include "ISuspenseCoreItemUseService.generated.h"

// Forward declaration
class ISuspenseCoreItemUseHandler;

UINTERFACE(MinimalAPI, BlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class USuspenseCoreItemUseService : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISuspenseCoreItemUseService
 *
 * Interface for Item Use Service - SSOT for all item use operations.
 *
 * RESPONSIBILITIES:
 * - Register/unregister item use handlers
 * - Route use requests to appropriate handlers
 * - Manage active time-based operations
 * - Publish events to EventBus
 * - Provide validation before execution
 *
 * ALL item use requests MUST go through this service.
 * This ensures consistent validation, logging, and event publishing.
 *
 * ACCESS:
 *   // Via ServiceProvider (recommended)
 *   USuspenseCoreServiceProvider* Provider = USuspenseCoreServiceProvider::Get(this);
 *   ISuspenseCoreItemUseService* Service = Provider->GetItemUseService();
 *
 *   // Or via static Get() on implementation
 *   USuspenseCoreItemUseService* Service = USuspenseCoreItemUseService::Get(this);
 */
class BRIDGESYSTEM_API ISuspenseCoreItemUseService
{
	GENERATED_BODY()

public:
	//==================================================================
	// Handler Registration
	//==================================================================

	/**
	 * Register a handler with the service
	 * Handlers are sorted by priority after registration
	 *
	 * @param Handler Handler instance to register
	 * @return true if registered successfully
	 */
	virtual bool RegisterHandler(TScriptInterface<ISuspenseCoreItemUseHandler> Handler) = 0;

	/**
	 * Unregister a handler by its tag
	 *
	 * @param HandlerTag Tag of handler to unregister
	 * @return true if handler was found and removed
	 */
	virtual bool UnregisterHandler(FGameplayTag HandlerTag) = 0;

	/**
	 * Get all registered handler tags
	 *
	 * @return Array of all registered handler tags
	 */
	virtual TArray<FGameplayTag> GetRegisteredHandlers() const = 0;

	/**
	 * Check if a handler is registered
	 *
	 * @param HandlerTag Tag to check
	 * @return true if handler is registered
	 */
	virtual bool IsHandlerRegistered(FGameplayTag HandlerTag) const = 0;

	//==================================================================
	// Validation
	//==================================================================

	/**
	 * Check if an item can be used
	 * Use before showing "Use" option in UI
	 *
	 * @param Request The use request to validate
	 * @return true if a handler can process this request
	 */
	virtual bool CanUseItem(const FSuspenseCoreItemUseRequest& Request) const = 0;

	/**
	 * Get detailed validation result
	 * Returns failure reason if item cannot be used
	 *
	 * @param Request The use request to validate
	 * @return Response with validation result (Success or failure reason)
	 */
	virtual FSuspenseCoreItemUseResponse ValidateUseRequest(
		const FSuspenseCoreItemUseRequest& Request) const = 0;

	/**
	 * Get expected duration for an operation
	 * Returns 0 for instant operations
	 *
	 * @param Request The use request
	 * @return Duration in seconds
	 */
	virtual float GetUseDuration(const FSuspenseCoreItemUseRequest& Request) const = 0;

	/**
	 * Get expected cooldown for an operation
	 *
	 * @param Request The use request
	 * @return Cooldown in seconds
	 */
	virtual float GetUseCooldown(const FSuspenseCoreItemUseRequest& Request) const = 0;

	//==================================================================
	// Execution
	//==================================================================

	/**
	 * Execute item use operation
	 *
	 * This is the main entry point for all item usage.
	 * Routes to appropriate handler based on item types.
	 *
	 * @param Request The use request
	 * @param OwnerActor Actor performing the use
	 * @return Response with result
	 */
	virtual FSuspenseCoreItemUseResponse UseItem(
		const FSuspenseCoreItemUseRequest& Request,
		AActor* OwnerActor) = 0;

	/**
	 * Cancel an in-progress operation
	 *
	 * @param RequestID ID of the request to cancel
	 * @return true if cancelled, false if not found or already completed
	 */
	virtual bool CancelUse(const FGuid& RequestID) = 0;

	/**
	 * Check if an operation is in progress
	 *
	 * @param RequestID ID of the request to check
	 * @return true if operation is currently in progress
	 */
	virtual bool IsOperationInProgress(const FGuid& RequestID) const = 0;

	/**
	 * Get current progress of an operation
	 *
	 * @param RequestID ID of the request
	 * @param OutProgress Progress value (0.0 - 1.0)
	 * @return true if operation found
	 */
	virtual bool GetOperationProgress(const FGuid& RequestID, float& OutProgress) const = 0;

	//==================================================================
	// QuickSlot Helpers
	//==================================================================

	/**
	 * Use item from QuickSlot by index
	 * Builds request automatically from QuickSlot data
	 *
	 * @param QuickSlotIndex Slot index (0-3)
	 * @param OwnerActor Actor with QuickSlotComponent
	 * @return Response with result
	 */
	virtual FSuspenseCoreItemUseResponse UseQuickSlot(int32 QuickSlotIndex, AActor* OwnerActor) = 0;

	/**
	 * Check if QuickSlot item can be used
	 *
	 * @param QuickSlotIndex Slot index (0-3)
	 * @param OwnerActor Actor with QuickSlotComponent
	 * @return true if slot has usable item
	 */
	virtual bool CanUseQuickSlot(int32 QuickSlotIndex, AActor* OwnerActor) const = 0;

	//==================================================================
	// Handler Query
	//==================================================================

	/**
	 * Find handler that would process a request
	 * Does not execute - just finds the handler
	 *
	 * @param Request The use request
	 * @return Handler tag, or empty if no handler found
	 */
	virtual FGameplayTag FindHandlerForRequest(const FSuspenseCoreItemUseRequest& Request) const = 0;
};
