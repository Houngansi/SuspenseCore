// SuspenseCoreInventoryManager.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryOperationTypes.h"
#include "SuspenseCoreInventoryManager.generated.h"

// Forward declarations
class USuspenseCoreInventoryComponent;
class USuspenseCoreEventBus;
class USuspenseCoreEventManager;
class USuspenseCoreDataManager;
struct FSuspenseCoreItemInstance;
struct FSuspenseCoreItemData;

/**
 * USuspenseCoreInventoryManager
 *
 * Central manager for all inventory operations in SuspenseCore.
 * GameInstanceSubsystem for global access.
 *
 * ARCHITECTURE:
 * - Single point of access for inventory operations
 * - Integrates with USuspenseCoreEventManager for event routing
 * - Uses USuspenseCoreDataManager for item data
 * - Tracks all active inventory components
 *
 * USAGE:
 * USuspenseCoreInventoryManager* Manager = GetGameInstance()->GetSubsystem<USuspenseCoreInventoryManager>();
 * Manager->TransferItem(SourceInv, TargetInv, ItemID, Quantity);
 */
UCLASS()
class INVENTORYSYSTEM_API USuspenseCoreInventoryManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//==================================================================
	// Subsystem Lifecycle
	//==================================================================

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	//==================================================================
	// Component Registration
	//==================================================================

	/**
	 * Register inventory component.
	 * @param Component Component to register
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Manager")
	void RegisterInventory(USuspenseCoreInventoryComponent* Component);

	/**
	 * Unregister inventory component.
	 * @param Component Component to unregister
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Manager")
	void UnregisterInventory(USuspenseCoreInventoryComponent* Component);

	/**
	 * Get all registered inventories.
	 * @return Array of registered components
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Manager")
	TArray<USuspenseCoreInventoryComponent*> GetAllInventories() const;

	/**
	 * Get inventories by owner.
	 * @param Owner Owning actor
	 * @return Array of matching components
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Manager")
	TArray<USuspenseCoreInventoryComponent*> GetInventoriesByOwner(AActor* Owner) const;

	/**
	 * Get inventory by tag.
	 * @param InventoryTag Tag to search for
	 * @return First matching component
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Manager")
	USuspenseCoreInventoryComponent* GetInventoryByTag(FGameplayTag InventoryTag) const;

	//==================================================================
	// Transfer Operations
	//==================================================================

	/**
	 * Transfer item between inventories.
	 * @param Source Source inventory
	 * @param Target Target inventory
	 * @param ItemID Item to transfer
	 * @param Quantity Amount to transfer
	 * @param OutResult Detailed result
	 * @return true if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Manager")
	bool TransferItem(
		USuspenseCoreInventoryComponent* Source,
		USuspenseCoreInventoryComponent* Target,
		FName ItemID,
		int32 Quantity,
		FSuspenseCoreInventorySimpleResult& OutResult
	);

	/**
	 * Transfer item instance between inventories.
	 * @param Source Source inventory
	 * @param Target Target inventory
	 * @param InstanceID Instance to transfer
	 * @param TargetSlot Target slot (-1 for auto)
	 * @param OutResult Detailed result
	 * @return true if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Manager")
	bool TransferItemInstance(
		USuspenseCoreInventoryComponent* Source,
		USuspenseCoreInventoryComponent* Target,
		const FGuid& InstanceID,
		int32 TargetSlot,
		FSuspenseCoreInventorySimpleResult& OutResult
	);

	/**
	 * Execute transfer operation.
	 * @param Operation Transfer parameters
	 * @param Context Operation context
	 * @param OutResult Detailed result
	 * @return true if successful
	 */
	bool ExecuteTransfer(
		const FSuspenseCoreTransferOperation& Operation,
		const FSuspenseCoreOperationContext& Context,
		FSuspenseCoreInventorySimpleResult& OutResult
	);

	//==================================================================
	// Batch Operations
	//==================================================================

	/**
	 * Execute batch operation.
	 * @param Batch Batch operation data
	 * @param Context Operation context
	 * @return true if all operations succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Manager")
	bool ExecuteBatchOperation(
		UPARAM(ref) FSuspenseCoreBatchOperation& Batch,
		const FSuspenseCoreOperationContext& Context
	);

	/**
	 * Sort inventory items.
	 * @param Inventory Inventory to sort
	 * @param SortMode Sort mode (Name, Type, Rarity, Weight)
	 * @return true if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Manager")
	bool SortInventory(USuspenseCoreInventoryComponent* Inventory, FName SortMode);

	/**
	 * Consolidate all stacks in inventory.
	 * @param Inventory Inventory to consolidate
	 * @return Number of stacks merged
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Manager")
	int32 ConsolidateAllStacks(USuspenseCoreInventoryComponent* Inventory);

	//==================================================================
	// Query Operations
	//==================================================================

	/**
	 * Find item across all inventories.
	 * @param ItemID Item to find
	 * @param OutInventories Inventories containing item
	 * @return Total quantity found
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Manager")
	int32 FindItemAcrossInventories(FName ItemID, TArray<USuspenseCoreInventoryComponent*>& OutInventories) const;

	/**
	 * Get total item count across inventories.
	 * @param ItemID Item to count
	 * @param Inventories Inventories to check (empty = all)
	 * @return Total quantity
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Manager")
	int32 GetTotalItemCount(FName ItemID, const TArray<USuspenseCoreInventoryComponent*>& Inventories) const;

	/**
	 * Get items by type across inventories.
	 * @param ItemType Type tag to filter
	 * @param OutItems Found instances
	 * @return Number of items found
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Manager")
	int32 GetItemsByType(FGameplayTag ItemType, TArray<FSuspenseCoreItemInstance>& OutItems) const;

	//==================================================================
	// Item Creation
	//==================================================================

	/**
	 * Create item instance from DataManager.
	 * @param ItemID DataTable row name
	 * @param Quantity Initial quantity
	 * @param OutInstance Created instance
	 * @return true if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Manager")
	bool CreateItemInstance(FName ItemID, int32 Quantity, FSuspenseCoreItemInstance& OutInstance);

	/**
	 * Get item data from DataManager.
	 * @param ItemID DataTable row name
	 * @param OutData Item data
	 * @return true if found
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Manager")
	bool GetItemData(FName ItemID, FSuspenseCoreItemData& OutData) const;

	//==================================================================
	// Validation
	//==================================================================

	/**
	 * Validate all inventories.
	 * @param OutErrors Error messages
	 * @return true if all valid
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Manager")
	bool ValidateAllInventories(TArray<FString>& OutErrors) const;

	/**
	 * Repair all inventories.
	 * @param OutRepairLog Repair actions taken
	 * @return Number of repairs
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Manager")
	int32 RepairAllInventories(TArray<FString>& OutRepairLog);

	//==================================================================
	// EventBus Integration
	//==================================================================

	/**
	 * Get EventManager.
	 * @return Event manager instance
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Manager")
	USuspenseCoreEventManager* GetEventManager() const;

	/**
	 * Get DataManager.
	 * @return Data manager instance
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Manager")
	USuspenseCoreDataManager* GetDataManager() const;

	/**
	 * Broadcast global inventory event.
	 * @param EventTag Event tag
	 * @param Payload Event data
	 */
	void BroadcastGlobalEvent(FGameplayTag EventTag, const TMap<FName, FString>& Payload);

	//==================================================================
	// Statistics
	//==================================================================

	/**
	 * Get manager statistics.
	 * @return Stats string
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Manager")
	FString GetStatistics() const;

	/**
	 * Get registered inventory count.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Manager")
	int32 GetInventoryCount() const { return RegisteredInventories.Num(); }

protected:
	/** Registered inventory components */
	UPROPERTY()
	TArray<TWeakObjectPtr<USuspenseCoreInventoryComponent>> RegisteredInventories;

	/** Cached EventManager reference */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventManager> CachedEventManager;

	/** Cached DataManager reference */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreDataManager> CachedDataManager;

	/** Is initialized */
	bool bIsInitialized;

	/** Clean up stale references */
	void CleanupStaleReferences();
};
