// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GameplayTagContainer.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "SuspenseCoreEquipmentInventoryBridge.generated.h"

// Forward declarations
class USuspenseCoreEventBus;
class USuspenseCoreServiceLocator;
class UInventoryComponent;
class UEquipmentComponent;
struct FSuspenseCoreEventData;
struct FEquipmentOperationRequest;

/**
 * FSuspenseCoreEquipmentInventoryTransaction
 *
 * Represents a coordinated transaction between equipment and inventory systems
 */
USTRUCT(BlueprintType)
struct FSuspenseCoreEquipmentInventoryTransaction
{
	GENERATED_BODY()

	/** Unique transaction ID */
	UPROPERTY(BlueprintReadOnly, Category = "Transaction")
	FGuid TransactionId;

	/** Item being transferred */
	UPROPERTY(BlueprintReadWrite, Category = "Transaction")
	FSuspenseInventoryItemInstance ItemInstance;

	/** Source system (Equipment or Inventory) */
	UPROPERTY(BlueprintReadWrite, Category = "Transaction")
	FGameplayTag SourceSystem;

	/** Target system (Equipment or Inventory) */
	UPROPERTY(BlueprintReadWrite, Category = "Transaction")
	FGameplayTag TargetSystem;

	/** Transaction state */
	UPROPERTY(BlueprintReadOnly, Category = "Transaction")
	bool bCompleted = false;

	/** Transaction timestamp */
	UPROPERTY(BlueprintReadOnly, Category = "Transaction")
	float Timestamp = 0.0f;

	/** Create new transaction */
	static FSuspenseCoreEquipmentInventoryTransaction Create(
		const FSuspenseInventoryItemInstance& Item,
		FGameplayTag Source,
		FGameplayTag Target)
	{
		FSuspenseCoreEquipmentInventoryTransaction Transaction;
		Transaction.TransactionId = FGuid::NewGuid();
		Transaction.ItemInstance = Item;
		Transaction.SourceSystem = Source;
		Transaction.TargetSystem = Target;
		Transaction.Timestamp = 0.0f;
		return Transaction;
	}
};

/**
 * USuspenseCoreEquipmentInventoryBridge
 *
 * Bridge between equipment and inventory systems with event-driven synchronization.
 *
 * Architecture:
 * - EventBus: Coordinates communication between equipment and inventory
 * - ServiceLocator: Dependency injection for system access
 * - GameplayTags: System identification and event routing
 * - Transactions: Atomic operations with rollback support
 *
 * Responsibilities:
 * - Coordinate item transfers between equipment and inventory
 * - Ensure data consistency across both systems
 * - Handle equipment/unequipment with inventory updates
 * - Publish bridge events through EventBus
 * - Support rollback on transaction failures
 */
UCLASS(BlueprintType)
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentInventoryBridge : public UObject
{
	GENERATED_BODY()

public:
	USuspenseCoreEquipmentInventoryBridge();

	//================================================
	// Initialization
	//================================================

	/**
	 * Initialize bridge with dependencies
	 * @param InServiceLocator ServiceLocator for dependency injection
	 * @param InInventoryComponent Inventory component to bridge
	 * @param InEquipmentComponent Equipment component to bridge
	 * @return True if initialized successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Bridge")
	bool Initialize(USuspenseCoreServiceLocator* InServiceLocator,
					UObject* InInventoryComponent,
					UObject* InEquipmentComponent);

	/**
	 * Shutdown bridge and cleanup
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Bridge")
	void Shutdown();

	/**
	 * Check if bridge is initialized
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Bridge")
	bool IsInitialized() const { return bIsInitialized; }

	//================================================
	// Equipment -> Inventory Operations
	//================================================

	/**
	 * Transfer item from equipment to inventory (unequip)
	 * @param ItemInstance Item to transfer
	 * @param SlotIndex Equipment slot index
	 * @return True if transfer succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Bridge|Transfer")
	bool TransferToInventory(const FSuspenseInventoryItemInstance& ItemInstance, int32 SlotIndex);

	/**
	 * Return equipped item to inventory
	 * @param SlotIndex Equipment slot to unequip
	 * @return True if return succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Bridge|Transfer")
	bool ReturnEquippedItemToInventory(int32 SlotIndex);

	//================================================
	// Inventory -> Equipment Operations
	//================================================

	/**
	 * Transfer item from inventory to equipment (equip)
	 * @param ItemInstance Item to transfer
	 * @param TargetSlotIndex Target equipment slot
	 * @return True if transfer succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Bridge|Transfer")
	bool TransferToEquipment(const FSuspenseInventoryItemInstance& ItemInstance, int32 TargetSlotIndex);

	/**
	 * Equip item from inventory
	 * @param InventorySlotIndex Inventory slot containing item
	 * @param EquipmentSlotIndex Target equipment slot
	 * @return True if equip succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Bridge|Transfer")
	bool EquipFromInventory(int32 InventorySlotIndex, int32 EquipmentSlotIndex);

	//================================================
	// Transaction Management
	//================================================

	/**
	 * Begin atomic transaction
	 * @param Item Item for transaction
	 * @param Source Source system
	 * @param Target Target system
	 * @return Transaction ID
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Bridge|Transaction")
	FGuid BeginTransaction(const FSuspenseInventoryItemInstance& Item, FGameplayTag Source, FGameplayTag Target);

	/**
	 * Commit transaction
	 * @param TransactionId Transaction to commit
	 * @return True if commit succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Bridge|Transaction")
	bool CommitTransaction(FGuid TransactionId);

	/**
	 * Rollback transaction
	 * @param TransactionId Transaction to rollback
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Bridge|Transaction")
	void RollbackTransaction(FGuid TransactionId);

	/**
	 * Get active transaction count
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Bridge|Transaction")
	int32 GetActiveTransactionCount() const { return ActiveTransactions.Num(); }

	//================================================
	// Synchronization
	//================================================

	/**
	 * Synchronize equipment and inventory states
	 * Ensures both systems are in consistent state
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Bridge|Sync")
	void SynchronizeSystems();

	/**
	 * Validate consistency between systems
	 * @param OutErrors Array to receive error messages
	 * @return True if systems are consistent
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Bridge|Sync")
	bool ValidateConsistency(TArray<FText>& OutErrors) const;

	/**
	 * Get item instance from equipment
	 * @param SlotIndex Equipment slot
	 * @param OutItem Output item instance
	 * @return True if item retrieved
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Bridge|Query")
	bool GetEquippedItem(int32 SlotIndex, FSuspenseInventoryItemInstance& OutItem) const;

	/**
	 * Check if item can be equipped
	 * @param ItemInstance Item to check
	 * @param TargetSlotIndex Target slot
	 * @param OutReason Reason if not allowed
	 * @return True if item can be equipped
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Bridge|Query")
	bool CanEquipItem(const FSuspenseInventoryItemInstance& ItemInstance, int32 TargetSlotIndex, FText& OutReason) const;

	//================================================
	// Statistics
	//================================================

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Bridge|Stats")
	int32 GetTotalTransfers() const { return TotalTransfers; }

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Bridge|Stats")
	int32 GetFailedTransfers() const { return FailedTransfers; }

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Bridge|Stats")
	void ResetStatistics();

protected:
	//================================================
	// Event Handlers
	//================================================

	/** Handle item equipped event from equipment system */
	void OnItemEquipped(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Handle item unequipped event from equipment system */
	void OnItemUnequipped(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Handle inventory changed event */
	void OnInventoryChanged(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	//================================================
	// Event Publishing
	//================================================

	/** Publish transfer started event */
	void PublishTransferStarted(const FSuspenseCoreEquipmentInventoryTransaction& Transaction);

	/** Publish transfer completed event */
	void PublishTransferCompleted(const FSuspenseCoreEquipmentInventoryTransaction& Transaction, bool bSuccess);

	/** Publish synchronization event */
	void PublishSynchronization(bool bSuccess);

	//================================================
	// Internal Methods
	//================================================

	/** Execute transfer operation */
	bool ExecuteTransfer(FSuspenseCoreEquipmentInventoryTransaction& Transaction);

	/** Validate transfer operation */
	bool ValidateTransfer(const FSuspenseCoreEquipmentInventoryTransaction& Transaction, FText& OutError) const;

	/** Setup event subscriptions */
	void SetupEventSubscriptions();

	/** Get transaction by ID */
	FSuspenseCoreEquipmentInventoryTransaction* FindTransaction(FGuid TransactionId);

private:
	//================================================
	// Dependencies (Injected)
	//================================================

	/** ServiceLocator for dependency injection */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreServiceLocator> ServiceLocator;

	/** EventBus for event coordination */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> EventBus;

	/** Inventory component reference */
	UPROPERTY()
	TWeakObjectPtr<UObject> InventoryComponent;

	/** Equipment component reference */
	UPROPERTY()
	TWeakObjectPtr<UObject> EquipmentComponent;

	//================================================
	// Transaction State
	//================================================

	/** Active transactions */
	UPROPERTY()
	TArray<FSuspenseCoreEquipmentInventoryTransaction> ActiveTransactions;

	/** Transaction history (limited size) */
	UPROPERTY()
	TArray<FSuspenseCoreEquipmentInventoryTransaction> TransactionHistory;

	//================================================
	// State
	//================================================

	/** Initialization flag */
	UPROPERTY()
	bool bIsInitialized;

	/** Last sync time */
	UPROPERTY()
	float LastSyncTime;

	//================================================
	// Statistics
	//================================================

	/** Total transfers executed */
	UPROPERTY()
	int32 TotalTransfers;

	/** Failed transfers */
	UPROPERTY()
	int32 FailedTransfers;

	/** Total transactions */
	UPROPERTY()
	int32 TotalTransactions;

	//================================================
	// Configuration
	//================================================

	/** Maximum active transactions */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	int32 MaxActiveTransactions;

	/** Transaction history size */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	int32 MaxTransactionHistory;

	//================================================
	// Thread Safety
	//================================================

	/** Critical section for thread-safe transaction access */
	mutable FCriticalSection TransactionCriticalSection;
};
