// ISuspenseCoreInventoryBridge.h
// SuspenseCore Equipment ↔ Inventory Bridge Interface
// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Operations/SuspenseInventoryResult.h"
#include "ISuspenseCoreInventoryBridge.generated.h"

// Forward declarations
class ISuspenseCoreInventory;
class USuspenseCoreEventBus;

/**
 * SuspenseCore Inventory Transfer Request
 *
 * Describes a transfer operation between inventory and equipment systems.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreBridgeTransferRequest
{
	GENERATED_BODY()

	/** Item instance being transferred */
	UPROPERTY(BlueprintReadWrite, Category = "Transfer")
	FSuspenseInventoryItemInstance Item;

	/** Source slot index (-1 if from inventory) */
	UPROPERTY(BlueprintReadWrite, Category = "Transfer")
	int32 SourceSlot = INDEX_NONE;

	/** Target slot index (-1 if to inventory) */
	UPROPERTY(BlueprintReadWrite, Category = "Transfer")
	int32 TargetSlot = INDEX_NONE;

	/** Is source from inventory system */
	UPROPERTY(BlueprintReadWrite, Category = "Transfer")
	bool bFromInventory = true;

	/** Is target the inventory system */
	UPROPERTY(BlueprintReadWrite, Category = "Transfer")
	bool bToInventory = false;

	/** Operation context tag */
	UPROPERTY(BlueprintReadWrite, Category = "Transfer")
	FGameplayTag ContextTag;

	/** Unique request ID for tracking */
	UPROPERTY(BlueprintReadOnly, Category = "Transfer")
	FGuid RequestId;

	FSuspenseCoreBridgeTransferRequest()
	{
		RequestId = FGuid::NewGuid();
	}

	/** Static factory: Create transfer FROM inventory */
	static FSuspenseCoreBridgeTransferRequest FromInventory(
		const FSuspenseInventoryItemInstance& InItem,
		int32 ToEquipmentSlot)
	{
		FSuspenseCoreBridgeTransferRequest Request;
		Request.Item = InItem;
		Request.SourceSlot = INDEX_NONE;
		Request.TargetSlot = ToEquipmentSlot;
		Request.bFromInventory = true;
		Request.bToInventory = false;
		return Request;
	}

	/** Static factory: Create transfer TO inventory */
	static FSuspenseCoreBridgeTransferRequest ToInventory(
		const FSuspenseInventoryItemInstance& InItem,
		int32 FromEquipmentSlot)
	{
		FSuspenseCoreBridgeTransferRequest Request;
		Request.Item = InItem;
		Request.SourceSlot = FromEquipmentSlot;
		Request.TargetSlot = INDEX_NONE;
		Request.bFromInventory = false;
		Request.bToInventory = true;
		return Request;
	}
};

/**
 * SuspenseCore Inventory Transfer Result
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreInventoryTransferResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FText ErrorMessage;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FGameplayTag ErrorTag;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FSuspenseInventoryItemInstance TransferredItem;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	int32 AffectedSlot = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FGuid RequestId;

	static FSuspenseCoreInventoryTransferResult Success(
		const FSuspenseInventoryItemInstance& Item,
		int32 Slot,
		const FGuid& InRequestId)
	{
		FSuspenseCoreInventoryTransferResult Result;
		Result.bSuccess = true;
		Result.TransferredItem = Item;
		Result.AffectedSlot = Slot;
		Result.RequestId = InRequestId;
		return Result;
	}

	static FSuspenseCoreInventoryTransferResult Failure(
		const FText& Error,
		const FGameplayTag& Tag = FGameplayTag(),
		const FGuid& InRequestId = FGuid())
	{
		FSuspenseCoreInventoryTransferResult Result;
		Result.bSuccess = false;
		Result.ErrorMessage = Error;
		Result.ErrorTag = Tag;
		Result.RequestId = InRequestId;
		return Result;
	}
};

/**
 * Space Reservation Handle for atomic operations
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreSpaceReservation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Reservation")
	FGuid ReservationId;

	UPROPERTY(BlueprintReadOnly, Category = "Reservation")
	FSuspenseInventoryItemInstance ReservedItem;

	UPROPERTY(BlueprintReadOnly, Category = "Reservation")
	float ExpirationTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Reservation")
	bool bIsValid = false;

	bool IsExpired(float CurrentTime) const
	{
		return CurrentTime > ExpirationTime;
	}
};

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreInventoryBridge : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISuspenseCoreInventoryBridge
 *
 * Bridge interface for Equipment ↔ Inventory system integration.
 * Handles item transfers, state synchronization, and space management.
 *
 * Architecture:
 * - Uses EventBus for decoupled communication
 * - Supports atomic operations with reservations
 * - Provides validation before transfer
 *
 * EventBus Events Published:
 * - SuspenseCore.Event.Inventory.TransferStarted
 * - SuspenseCore.Event.Inventory.TransferCompleted
 * - SuspenseCore.Event.Inventory.TransferFailed
 * - SuspenseCore.Event.Inventory.SpaceReserved
 * - SuspenseCore.Event.Inventory.ReservationReleased
 *
 * EventBus Events Subscribed:
 * - SuspenseCore.Event.Inventory.ItemAdded
 * - SuspenseCore.Event.Inventory.ItemRemoved
 * - SuspenseCore.Event.Inventory.StateChanged
 */
class BRIDGESYSTEM_API ISuspenseCoreInventoryBridge
{
	GENERATED_BODY()

public:
	//========================================
	// Core Transfer Operations
	//========================================

	/**
	 * Transfer item from inventory to equipment
	 * @param Request Transfer request details
	 * @return Transfer result with success/failure info
	 */
	virtual FSuspenseCoreInventoryTransferResult TransferFromInventory(
		const FSuspenseCoreBridgeTransferRequest& Request) = 0;

	/**
	 * Transfer item from equipment to inventory
	 * @param Request Transfer request details
	 * @return Transfer result with success/failure info
	 */
	virtual FSuspenseCoreInventoryTransferResult TransferToInventory(
		const FSuspenseCoreBridgeTransferRequest& Request) = 0;

	/**
	 * Validate transfer before execution (dry run)
	 * @param Request Transfer to validate
	 * @return Validation result (success = transfer would succeed)
	 */
	virtual FSuspenseCoreInventoryTransferResult ValidateTransfer(
		const FSuspenseCoreBridgeTransferRequest& Request) const = 0;

	//========================================
	// Space Management
	//========================================

	/**
	 * Check if inventory has space for item
	 * @param Item Item to check
	 * @return True if space is available
	 */
	virtual bool InventoryHasSpace(const FSuspenseInventoryItemInstance& Item) const = 0;

	/**
	 * Reserve inventory space for upcoming transfer (atomic operation support)
	 * @param Item Item to reserve space for
	 * @param TimeoutSeconds Reservation timeout (default: 5 seconds)
	 * @return Reservation handle or invalid on failure
	 */
	virtual FSuspenseCoreSpaceReservation ReserveInventorySpace(
		const FSuspenseInventoryItemInstance& Item,
		float TimeoutSeconds = 5.0f) = 0;

	/**
	 * Release a previously made reservation
	 * @param ReservationId Reservation to release
	 * @return True if released successfully
	 */
	virtual bool ReleaseReservation(const FGuid& ReservationId) = 0;

	/**
	 * Check if reservation is still valid
	 * @param ReservationId Reservation to check
	 * @return True if valid and not expired
	 */
	virtual bool IsReservationValid(const FGuid& ReservationId) const = 0;

	//========================================
	// Inventory Access
	//========================================

	/**
	 * Get inventory interface
	 * @return Inventory interface or nullptr if not connected
	 */
	virtual TScriptInterface<ISuspenseCoreInventory> GetInventoryInterface() const = 0;

	/**
	 * Find item in inventory by ID
	 * @param ItemId Item ID to find
	 * @param OutInstance Found item instance
	 * @return True if found
	 */
	virtual bool FindItemInInventory(
		const FName& ItemId,
		FSuspenseInventoryItemInstance& OutInstance) const = 0;

	/**
	 * Find item in inventory by instance ID
	 * @param InstanceId Unique instance ID
	 * @param OutInstance Found item instance
	 * @return True if found
	 */
	virtual bool FindItemByInstanceId(
		const FGuid& InstanceId,
		FSuspenseInventoryItemInstance& OutInstance) const = 0;

	/**
	 * Get all items in inventory matching type
	 * @param ItemType Type tag to match
	 * @return Array of matching items
	 */
	virtual TArray<FSuspenseInventoryItemInstance> GetInventoryItemsByType(
		const FGameplayTag& ItemType) const = 0;

	//========================================
	// Synchronization
	//========================================

	/**
	 * Force synchronization with inventory state
	 * Call when inventory state may have changed externally
	 */
	virtual void SynchronizeWithInventory() = 0;

	/**
	 * Check if bridge is synchronized with inventory
	 * @return True if synchronized
	 */
	virtual bool IsSynchronized() const = 0;

	/**
	 * Get last synchronization timestamp
	 * @return World time of last sync
	 */
	virtual float GetLastSyncTime() const = 0;

	//========================================
	// EventBus Integration
	//========================================

	/**
	 * Get EventBus used by this bridge
	 * @return EventBus instance (may be null if not initialized)
	 */
	virtual USuspenseCoreEventBus* GetEventBus() const = 0;

	/**
	 * Set EventBus for this bridge
	 * @param InEventBus EventBus to use
	 */
	virtual void SetEventBus(USuspenseCoreEventBus* InEventBus) = 0;

	//========================================
	// Diagnostics
	//========================================

	/**
	 * Get bridge statistics
	 * @return Formatted statistics string
	 */
	virtual FString GetBridgeStatistics() const = 0;

	/**
	 * Get active reservations count
	 * @return Number of active reservations
	 */
	virtual int32 GetActiveReservationsCount() const = 0;

	/**
	 * Clear expired reservations
	 * @return Number of cleared reservations
	 */
	virtual int32 ClearExpiredReservations() = 0;
};

