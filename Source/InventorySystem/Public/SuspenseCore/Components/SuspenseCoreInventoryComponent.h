// SuspenseCoreInventoryComponent.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SuspenseCore/Interfaces/Inventory/ISuspenseCoreInventory.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "SuspenseCore/Types/Items/SuspenseCoreItemTypes.h"
#include "SuspenseCoreInventoryComponent.generated.h"

// Forward declarations
class USuspenseCoreEventBus;
class USuspenseCoreDataManager;
class USuspenseCoreInventoryStorage;
class USuspenseCoreInventoryValidator;

// Log category
INVENTORYSYSTEM_API DECLARE_LOG_CATEGORY_EXTERN(LogSuspenseCoreInventory, Log, All);

/**
 * USuspenseCoreInventoryComponent
 *
 * Main inventory component implementing ISuspenseCoreInventory.
 * Uses EventBus for all notifications, FSuspenseCoreItemData/Instance types.
 *
 * ARCHITECTURE:
 * - Implements ISuspenseCoreInventory interface
 * - Uses USuspenseCoreInventoryStorage for grid management
 * - Uses USuspenseCoreInventoryValidator for validation
 * - Broadcasts via EventBus (SuspenseCore.Event.Inventory.*)
 * - Network replication via FFastArraySerializer
 *
 * LIFECYCLE:
 * 1. Component created on PlayerState or Actor
 * 2. Initialize() or InitializeFromLoadout() called
 * 3. Storage and validator set up
 * 4. Ready for operations
 *
 * NETWORK:
 * - Server authoritative
 * - Client receives replicated state via FFastArraySerializer
 * - Operations are server-validated
 *
 * @see ISuspenseCoreInventory
 * @see USuspenseCoreInventoryStorage
 * @see USuspenseCoreInventoryValidator
 */
UCLASS(ClassGroup = (SuspenseCore), meta = (BlueprintSpawnableComponent))
class INVENTORYSYSTEM_API USuspenseCoreInventoryComponent : public UActorComponent, public ISuspenseCoreInventory
{
	GENERATED_BODY()

public:
	USuspenseCoreInventoryComponent();

	//~ UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//==================================================================
	// ISuspenseCoreInventory - Add Operations
	//==================================================================

	virtual bool AddItemByID_Implementation(FName ItemID, int32 Quantity) override;
	virtual bool AddItemInstance_Implementation(const FSuspenseCoreItemInstance& ItemInstance) override;
	virtual bool AddItemInstanceToSlot(const FSuspenseCoreItemInstance& ItemInstance, int32 TargetSlot) override;

	//==================================================================
	// ISuspenseCoreInventory - Remove Operations
	//==================================================================

	virtual bool RemoveItemByID_Implementation(FName ItemID, int32 Quantity) override;
	virtual bool RemoveItemInstance(const FGuid& InstanceID) override;
	virtual bool RemoveItemFromSlot(int32 SlotIndex, FSuspenseCoreItemInstance& OutRemovedInstance) override;

	//==================================================================
	// ISuspenseCoreInventory - Query Operations
	//==================================================================

	virtual TArray<FSuspenseCoreItemInstance> GetAllItemInstances() const override;
	virtual bool GetItemInstanceAtSlot(int32 SlotIndex, FSuspenseCoreItemInstance& OutInstance) const override;
	virtual bool FindItemInstance(const FGuid& InstanceID, FSuspenseCoreItemInstance& OutInstance) const override;
	virtual int32 GetItemCountByID_Implementation(FName ItemID) const override;
	virtual bool HasItem_Implementation(FName ItemID, int32 Quantity) const override;
	virtual int32 GetTotalItemCount_Implementation() const override;
	virtual TArray<FSuspenseCoreItemInstance> FindItemsByType(FGameplayTag ItemType) const override;

	//==================================================================
	// ISuspenseCoreInventory - Grid Operations
	//==================================================================

	virtual FIntPoint GetGridSize_Implementation() const override;
	virtual bool MoveItem_Implementation(int32 FromSlot, int32 ToSlot) override;
	virtual bool SwapItems(int32 Slot1, int32 Slot2) override;
	virtual bool RotateItemAtSlot(int32 SlotIndex) override;
	virtual bool IsSlotOccupied(int32 SlotIndex) const override;
	virtual int32 FindFreeSlot(FIntPoint ItemGridSize, bool bAllowRotation = true) const override;
	virtual bool CanPlaceItemAtSlot(FIntPoint ItemGridSize, int32 SlotIndex, bool bRotated = false) const override;

	//==================================================================
	// ISuspenseCoreInventory - Weight System
	//==================================================================

	virtual float GetCurrentWeight_Implementation() const override;
	virtual float GetMaxWeight_Implementation() const override;
	virtual float GetRemainingWeight_Implementation() const override;
	virtual bool HasWeightCapacity_Implementation(float AdditionalWeight) const override;
	virtual void SetMaxWeight(float NewMaxWeight) override;

	//==================================================================
	// ISuspenseCoreInventory - Validation
	//==================================================================

	virtual bool CanReceiveItem_Implementation(FName ItemID, int32 Quantity) const override;
	virtual FGameplayTagContainer GetAllowedItemTypes_Implementation() const override;
	virtual void SetAllowedItemTypes(const FGameplayTagContainer& AllowedTypes) override;
	virtual bool ValidateIntegrity(TArray<FString>& OutErrors) const override;

	//==================================================================
	// ISuspenseCoreInventory - Transaction System
	//==================================================================

	virtual void BeginTransaction() override;
	virtual void CommitTransaction() override;
	virtual void RollbackTransaction() override;
	virtual bool IsTransactionActive() const override;

	//==================================================================
	// ISuspenseCoreInventory - Stack Operations
	//==================================================================

	virtual bool SplitStack(int32 SourceSlot, int32 SplitQuantity, int32 TargetSlot) override;
	virtual int32 ConsolidateStacks(FName ItemID = NAME_None) override;

	//==================================================================
	// ISuspenseCoreInventory - Initialization
	//==================================================================

	virtual bool InitializeFromLoadout(FName LoadoutID) override;
	virtual void Initialize(int32 GridWidth, int32 GridHeight, float InMaxWeight) override;
	virtual bool IsInitialized() const override;
	virtual void Clear() override;

	//==================================================================
	// ISuspenseCoreInventory - EventBus
	//==================================================================

	virtual USuspenseCoreEventBus* GetEventBus() const override;
	virtual void BroadcastInventoryUpdated() override;

	//==================================================================
	// ISuspenseCoreInventory - Debug
	//==================================================================

	virtual FString GetDebugString() const override;
	virtual void LogContents() const override;

	//==================================================================
	// Accessors
	//==================================================================

	/** Get grid width */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory")
	int32 GetGridWidth() const { return Config.GridSize.X; }

	/** Get grid height */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory")
	int32 GetGridHeight() const { return Config.GridSize.Y; }

	/** Check if slot is empty */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory")
	bool IsSlotEmpty(int32 SlotIndex) const { return !IsSlotOccupied(SlotIndex); }

protected:
	//==================================================================
	// Internal Operations
	//==================================================================

	/** Get DataManager subsystem */
	USuspenseCoreDataManager* GetDataManager() const;

	/** Create item instance from ItemID */
	bool CreateItemInstance(FName ItemID, int32 Quantity, FSuspenseCoreItemInstance& OutInstance) const;

	/** Internal add operation (no validation) */
	bool AddItemInternal(const FSuspenseCoreItemInstance& ItemInstance, int32 TargetSlot);

	/** Internal remove operation (no validation) */
	bool RemoveItemInternal(const FGuid& InstanceID, FSuspenseCoreItemInstance& OutRemovedInstance);

	/** Broadcast item event via EventBus */
	void BroadcastItemEvent(FGameplayTag EventTag, const FSuspenseCoreItemInstance& Instance, int32 SlotIndex);

	/** Broadcast error event via EventBus */
	void BroadcastErrorEvent(ESuspenseCoreInventoryResult ErrorCode, const FString& Context);

	/** Handle replicated inventory update */
	UFUNCTION()
	void OnRep_ReplicatedInventory();

	/** Subscribe to EventBus events */
	void SubscribeToEvents();

	/** Unsubscribe from EventBus events */
	void UnsubscribeFromEvents();

	/** Handle add item request event */
	void OnAddItemRequestEvent(const struct FSuspenseCoreEventData& EventData);

private:
	//==================================================================
	// Configuration
	//==================================================================

	/** Inventory configuration */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration",
		meta = (AllowPrivateAccess = "true"))
	FSuspenseCoreInventoryConfig Config;

	//==================================================================
	// State
	//==================================================================

	/** All item instances in inventory */
	UPROPERTY(Transient)
	TArray<FSuspenseCoreItemInstance> ItemInstances;

	/** Grid slots */
	UPROPERTY(Transient)
	TArray<FSuspenseCoreInventorySlot> GridSlots;

	/** Replicated inventory data */
	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedInventory)
	FSuspenseCoreReplicatedInventory ReplicatedInventory;

	/** Current total weight */
	UPROPERTY(Transient)
	float CurrentWeight;

	/** Is inventory initialized */
	UPROPERTY(Transient)
	bool bIsInitialized;

	//==================================================================
	// Transaction State
	//==================================================================

	/** Transaction snapshot */
	UPROPERTY(Transient)
	FSuspenseCoreInventorySnapshot TransactionSnapshot;

	/** Is transaction active */
	UPROPERTY(Transient)
	bool bTransactionActive;

	//==================================================================
	// Cached References
	//==================================================================

	/** Cached EventBus */
	UPROPERTY(Transient)
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	/** Cached DataManager */
	UPROPERTY(Transient)
	TWeakObjectPtr<USuspenseCoreDataManager> CachedDataManager;

	/** EventBus subscription handles */
	TArray<FDelegateHandle> EventSubscriptions;

	//==================================================================
	// Helpers
	//==================================================================

	/** Convert slot index to grid coordinates */
	FIntPoint SlotToGridCoords(int32 SlotIndex) const;

	/** Convert grid coordinates to slot index */
	int32 GridCoordsToSlot(FIntPoint Coords) const;

	/** Check if coordinates are valid */
	bool IsValidGridCoords(FIntPoint Coords) const;

	/** Recalculate current weight */
	void RecalculateWeight();

	/** Update grid slots for item placement */
	void UpdateGridSlots(const FSuspenseCoreItemInstance& Instance, bool bPlace);

	/** Find item instance by ID (internal) */
	FSuspenseCoreItemInstance* FindItemInstanceInternal(const FGuid& InstanceID);
	const FSuspenseCoreItemInstance* FindItemInstanceInternal(const FGuid& InstanceID) const;
};
