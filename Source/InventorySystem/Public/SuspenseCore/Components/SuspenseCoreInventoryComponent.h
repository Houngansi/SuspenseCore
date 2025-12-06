// SuspenseCoreInventoryComponent.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SuspenseCore/Interfaces/Inventory/ISuspenseCoreInventory.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreUIDataProvider.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "SuspenseCore/Types/Items/SuspenseCoreItemTypes.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUITypes.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUIContainerTypes.h"
#include "SuspenseCore/Base/SuspenseCoreInventoryLogs.h"
#include "SuspenseCoreInventoryComponent.generated.h"

// Forward declarations
class USuspenseCoreEventBus;
class USuspenseCoreDataManager;
class USuspenseCoreInventoryStorage;
class USuspenseCoreInventoryValidator;
class USuspenseCoreSecurityValidator;

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
class INVENTORYSYSTEM_API USuspenseCoreInventoryComponent
	: public UActorComponent
	, public ISuspenseCoreInventory
	, public ISuspenseCoreUIDataProvider
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
	// ISuspenseCoreUIDataProvider - Identity
	//==================================================================

	virtual FGuid GetProviderID() const override { return ProviderID; }
	virtual ESuspenseCoreContainerType GetContainerType() const override { return ESuspenseCoreContainerType::Inventory; }
	virtual FGameplayTag GetContainerTypeTag() const override;
	virtual AActor* GetOwningActor() const override { return GetOwner(); }

	//==================================================================
	// ISuspenseCoreUIDataProvider - Container Data
	//==================================================================

	virtual FSuspenseCoreContainerUIData GetContainerUIData() const override;
	virtual FIntPoint GetGridSize() const override { return FIntPoint(Config.GridWidth, Config.GridHeight); }
	virtual int32 GetSlotCount() const override { return Config.GridWidth * Config.GridHeight; }

	//==================================================================
	// ISuspenseCoreUIDataProvider - Slot Data
	//==================================================================

	virtual TArray<FSuspenseCoreSlotUIData> GetAllSlotUIData() const override;
	virtual FSuspenseCoreSlotUIData GetSlotUIData(int32 SlotIndex) const override;
	virtual bool IsSlotValid(int32 SlotIndex) const override;

	//==================================================================
	// ISuspenseCoreUIDataProvider - Item Data
	//==================================================================

	virtual TArray<FSuspenseCoreItemUIData> GetAllItemUIData() const override;
	virtual bool GetItemUIDataAtSlot(int32 SlotIndex, FSuspenseCoreItemUIData& OutItem) const override;
	virtual bool FindItemUIData(const FGuid& InstanceID, FSuspenseCoreItemUIData& OutItem) const override;
	virtual int32 GetItemCount() const override { return ItemInstances.Num(); }

	//==================================================================
	// ISuspenseCoreUIDataProvider - Weight System
	//==================================================================

	virtual bool HasWeightLimit() const override { return Config.MaxWeight > 0.0f; }
	virtual float GetCurrentWeight() const override { return CurrentWeight; }
	virtual float GetMaxWeight() const override { return Config.MaxWeight; }

	//==================================================================
	// ISuspenseCoreUIDataProvider - Validation
	//==================================================================

	virtual FSuspenseCoreDropValidation ValidateDrop(
		const FSuspenseCoreDragData& DragData,
		int32 TargetSlot,
		bool bRotated = false) const override;
	virtual bool CanAcceptItemType(const FGameplayTag& ItemType) const override;
	virtual int32 FindBestSlotForItem(FIntPoint ItemSize, bool bAllowRotation = true) const override;

	//==================================================================
	// ISuspenseCoreUIDataProvider - Operations
	//==================================================================

	virtual bool RequestMoveItem(int32 FromSlot, int32 ToSlot, bool bRotate = false) override;
	virtual bool RequestRotateItem(int32 SlotIndex) override;
	virtual bool RequestUseItem(int32 SlotIndex) override;
	virtual bool RequestDropItem(int32 SlotIndex, int32 Quantity = 0) override;
	virtual bool RequestSplitStack(int32 SlotIndex, int32 SplitQuantity, int32 TargetSlot = INDEX_NONE) override;
	virtual bool RequestTransferItem(
		int32 SlotIndex,
		const FGuid& TargetProviderID,
		int32 TargetSlot = INDEX_NONE,
		int32 Quantity = 0) override;

	//==================================================================
	// ISuspenseCoreUIDataProvider - Context Menu
	//==================================================================

	virtual TArray<FGameplayTag> GetItemContextActions(int32 SlotIndex) const override;
	virtual bool ExecuteContextAction(int32 SlotIndex, const FGameplayTag& ActionTag) override;

	//==================================================================
	// ISuspenseCoreUIDataProvider - Delegate
	//==================================================================

	virtual FOnSuspenseCoreUIDataChanged& OnUIDataChanged() override { return UIDataChangedDelegate; }

	//==================================================================
	// Accessors
	//==================================================================

	/** Get grid width */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory")
	int32 GetGridWidth() const { return Config.GridWidth; }

	/** Get grid height */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory")
	int32 GetGridHeight() const { return Config.GridHeight; }

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

	//==================================================================
	// Server RPCs - Security Layer (Phase 6)
	//==================================================================

	/** Server: Add item by ID */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_AddItemByID(FName ItemID, int32 Quantity);
	bool Server_AddItemByID_Validate(FName ItemID, int32 Quantity);
	void Server_AddItemByID_Implementation(FName ItemID, int32 Quantity);

	/** Server: Remove item by ID */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RemoveItemByID(FName ItemID, int32 Quantity);
	bool Server_RemoveItemByID_Validate(FName ItemID, int32 Quantity);
	void Server_RemoveItemByID_Implementation(FName ItemID, int32 Quantity);

	/** Server: Move item between slots */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_MoveItem(int32 FromSlot, int32 ToSlot);
	bool Server_MoveItem_Validate(int32 FromSlot, int32 ToSlot);
	void Server_MoveItem_Implementation(int32 FromSlot, int32 ToSlot);

	/** Server: Swap items between slots */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SwapItems(int32 Slot1, int32 Slot2);
	bool Server_SwapItems_Validate(int32 Slot1, int32 Slot2);
	void Server_SwapItems_Implementation(int32 Slot1, int32 Slot2);

	/** Server: Split stack */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SplitStack(int32 SourceSlot, int32 SplitQuantity, int32 TargetSlot);
	bool Server_SplitStack_Validate(int32 SourceSlot, int32 SplitQuantity, int32 TargetSlot);
	void Server_SplitStack_Implementation(int32 SourceSlot, int32 SplitQuantity, int32 TargetSlot);

	/** Server: Remove item from slot */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RemoveItemFromSlot(int32 SlotIndex);
	bool Server_RemoveItemFromSlot_Validate(int32 SlotIndex);
	void Server_RemoveItemFromSlot_Implementation(int32 SlotIndex);

	//==================================================================
	// Security Helpers
	//==================================================================

	/** Check if component owner has server authority */
	bool CheckInventoryAuthority(const FString& FunctionName) const;

	/** Get max slots for validation */
	int32 GetMaxSlots() const { return Config.GridWidth * Config.GridHeight; }

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
	// UI Data Provider State
	//==================================================================

	/** Unique provider ID */
	UPROPERTY(Transient)
	FGuid ProviderID;

	/** UI data changed delegate */
	FOnSuspenseCoreUIDataChanged UIDataChangedDelegate;

	//==================================================================
	// UI Data Provider Helpers
	//==================================================================

	/** Convert internal item instance to UI data */
	FSuspenseCoreItemUIData ConvertToUIData(const FSuspenseCoreItemInstance& Instance) const;

	/** Convert internal slot to UI data */
	FSuspenseCoreSlotUIData ConvertSlotToUIData(int32 SlotIndex) const;

	/** Broadcast UI data changed event */
	void BroadcastUIDataChanged(const FGameplayTag& ChangeType, const FGuid& AffectedItemID = FGuid());

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
