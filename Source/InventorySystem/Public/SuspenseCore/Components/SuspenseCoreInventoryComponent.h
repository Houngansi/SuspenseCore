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
#include "SuspenseCore/Types/SuspenseCoreTypes.h"  // FSuspenseCoreSubscriptionHandle, FSuspenseCoreNativeEventCallback
#include "SuspenseCore/Base/SuspenseCoreInventoryLogs.h"
#include "SuspenseCoreInventoryComponent.generated.h"

//==================================================================
// Save Data Versioning
//==================================================================

/** Current inventory save data version. Increment when save format changes. */
#define SUSPENSECORE_INVENTORY_SAVE_VERSION 1

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

	// Allow delta callbacks to access protected members
	friend struct FSuspenseCoreReplicatedItem;

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
	// ISuspenseCoreUIDataProvider - Grid Position Calculations
	//==================================================================

	virtual int32 GetSlotAtLocalPosition(const FVector2D& LocalPos, float CellSize, float CellGap) const override;
	virtual TArray<int32> GetOccupiedSlotsForItem(const FGuid& ItemInstanceID) const override;
	virtual int32 GetAnchorSlotForPosition(int32 AnySlotIndex) const override;
	virtual bool CanPlaceItemAtSlot(const FGuid& ItemID, int32 SlotIndex, bool bRotated) const override;

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

	//==================================================================
	// Magazine Operations
	// @see TarkovStyle_Ammo_System_Design.md - Tarkov-style ammo loading
	// @see SuspenseCoreArchitecture.md - EventBus integration
	//==================================================================

	/**
	 * Get item instance by UniqueInstanceID (pointer for modification)
	 * Use for internal operations that need to modify item state.
	 * @param InstanceID Unique ID of the item instance
	 * @return Pointer to item instance, nullptr if not found
	 */
	FSuspenseCoreItemInstance* GetItemByInstanceID(const FGuid& InstanceID);

	/**
	 * Get item instance by UniqueInstanceID (const version)
	 * @param InstanceID Unique ID of the item instance
	 * @return Const pointer to item instance, nullptr if not found
	 */
	const FSuspenseCoreItemInstance* GetItemByInstanceID(const FGuid& InstanceID) const;

	/**
	 * Update magazine data for an item
	 * Called by EventBus handler when ammo is loaded/unloaded.
	 * Triggers replication and UI update.
	 * @param ItemInstanceID UniqueInstanceID of the magazine item
	 * @param NewMagData Updated magazine data
	 * @return true if update was successful
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Magazine")
	bool UpdateMagazineData(const FGuid& ItemInstanceID, const FSuspenseCoreMagazineInstance& NewMagData);

	/**
	 * Get magazine data for an item
	 * @param ItemInstanceID UniqueInstanceID of the magazine item
	 * @param OutMagData Output magazine data
	 * @return true if item exists and is a magazine
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Inventory|Magazine")
	bool GetMagazineData(const FGuid& ItemInstanceID, FSuspenseCoreMagazineInstance& OutMagData) const;

	/**
	 * Load rounds into a magazine item
	 * Convenience method that calls UpdateMagazineData internally.
	 * @param MagazineInstanceID UniqueInstanceID of the magazine item
	 * @param AmmoID Type of ammo to load
	 * @param RoundCount Number of rounds to load
	 * @return Actual number of rounds loaded
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory|Magazine")
	int32 LoadRoundsIntoMagazine(const FGuid& MagazineInstanceID, FName AmmoID, int32 RoundCount);

	/**
	 * Mark item as dirty for replication
	 * Forces FastArraySerializer to replicate the item on next tick.
	 * @param InstanceID UniqueInstanceID of the item to mark dirty
	 */
	void MarkItemDirty(const FGuid& InstanceID);

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

	//==================================================================
	// Delta Replication Delegate Handlers
	// These handle the actual logic for FastArraySerializer callbacks
	//==================================================================

	/** Bind replication delegates to ReplicatedInventory */
	void BindReplicationDelegates();

	/** Handle item removal from replicated array */
	void HandleReplicatedItemRemove(const FSuspenseCoreReplicatedItem& Item, const FSuspenseCoreReplicatedInventory& ArraySerializer);

	/** Handle item addition to replicated array */
	void HandleReplicatedItemAdd(const FSuspenseCoreReplicatedItem& Item, const FSuspenseCoreReplicatedInventory& ArraySerializer);

	/** Handle item change in replicated array */
	void HandleReplicatedItemChange(const FSuspenseCoreReplicatedItem& Item, const FSuspenseCoreReplicatedInventory& ArraySerializer);

	/** Subscribe to EventBus events */
	void SubscribeToEvents();

	/** Unsubscribe from EventBus events */
	void UnsubscribeFromEvents();

	/** Handle add item request event */
	void OnAddItemRequestEvent(const struct FSuspenseCoreEventData& EventData);

	//==================================================================
	// Magazine Event Handlers
	// @see TarkovStyle_Ammo_System_Design.md - EventBus integration
	//==================================================================

	/**
	 * Handle single round loaded into magazine event
	 * Called by EventBus when AmmoLoadingService loads a round.
	 * Updates the magazine item data and triggers replication.
	 * @param EventTag The event tag (TAG_Equipment_Event_Ammo_RoundLoaded)
	 * @param EventData Event data containing MagazineInstanceID, AmmoID, NewRoundCount
	 */
	void OnMagazineRoundLoaded(FGameplayTag EventTag, const struct FSuspenseCoreEventData& EventData);

	/**
	 * Handle single round unloaded from magazine event
	 * @param EventTag The event tag (TAG_Equipment_Event_Ammo_RoundUnloaded)
	 * @param EventData Event data containing MagazineInstanceID, NewRoundCount
	 */
	void OnMagazineRoundUnloaded(FGameplayTag EventTag, const struct FSuspenseCoreEventData& EventData);

	/**
	 * Handle ammo loading completed event
	 * @param EventTag The event tag (TAG_Equipment_Event_Ammo_LoadCompleted)
	 * @param EventData Event data containing MagazineInstanceID, final state
	 */
	void OnMagazineLoadCompleted(FGameplayTag EventTag, const struct FSuspenseCoreEventData& EventData);

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

	//==================================================================
	// Grid Storage (SSOT for all Grid Operations)
	//==================================================================

	/**
	 * Grid storage manager - Single Source of Truth for ALL spatial operations.
	 * All grid queries and modifications MUST go through GridStorage.
	 * Direct GridSlots access is DEPRECATED and will be removed in future versions.
	 */
	UPROPERTY(Transient)
	TObjectPtr<USuspenseCoreInventoryStorage> GridStorage;

	/** Ensure Storage is created and initialized */
	void EnsureStorageInitialized();

	//==================================================================
	// Grid Slot Accessors (delegates to GridStorage)
	//==================================================================

	/** Get slot data from GridStorage (SSOT) */
	FSuspenseCoreInventorySlot GetGridSlot(int32 SlotIndex) const;

	/** Get instance ID at slot from GridStorage (SSOT) */
	FGuid GetInstanceIDAtSlot(int32 SlotIndex) const;

	/** Check if slot is valid via GridStorage */
	bool IsValidSlotIndex(int32 SlotIndex) const;

	/** Get total slot count from GridStorage */
	int32 GetTotalSlotCount() const;

	//==================================================================
	// DEPRECATED: Legacy GridSlots Array
	//==================================================================

	/**
	 * @deprecated Use GridStorage accessors instead (GetGridSlot, GetInstanceIDAtSlot, etc.)
	 * This array is maintained ONLY for serialization/replication compatibility.
	 * DO NOT access directly in new code - use GridStorage methods.
	 * Will be removed in SuspenseCore 2.0
	 */
	UE_DEPRECATED(5.0, "Use GridStorage accessors instead. Direct GridSlots access will be removed in SuspenseCore 2.0")
	UPROPERTY(Transient)
	TArray<FSuspenseCoreInventorySlot> GridSlots_DEPRECATED;

	/**
	 * Internal: Sync GridStorage to legacy array for replication.
	 * Called automatically - do not call directly.
	 */
	void SyncStorageToLegacyArray();

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
	// Save Data Versioning
	//==================================================================

	/** Save data version for migration support */
	UPROPERTY(SaveGame)
	int32 SaveDataVersion = SUSPENSECORE_INVENTORY_SAVE_VERSION;

	/** Migrate save data from older versions */
	void MigrateSaveData(int32 FromVersion, int32 ToVersion);

	/** Called after loading to handle version migration */
	virtual void PostLoad() override;

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

	/** EventBus subscription handles for magazine loading events */
	TArray<FSuspenseCoreSubscriptionHandle> EventSubscriptions;

	//==================================================================
	// UI Data Provider State
	//==================================================================

	/** Unique provider ID */
	UPROPERTY(Transient)
	FGuid ProviderID;

	/** UI data changed delegate */
	FOnSuspenseCoreUIDataChanged UIDataChangedDelegate;

	//==================================================================
	// UI Data Cache (Performance Optimization)
	//==================================================================

	/** Cached item UI data - avoids regeneration every frame */
	mutable TMap<FGuid, FSuspenseCoreItemUIData> CachedItemUIData;

	/** Cached slot UI data */
	mutable TArray<FSuspenseCoreSlotUIData> CachedSlotUIData;

	/** Item UI cache dirty flag */
	mutable bool bItemUICacheDirty = true;

	/** Slot UI cache dirty flag */
	mutable bool bSlotUICacheDirty = true;

	/** Invalidate specific item in UI cache */
	void InvalidateItemUICache(const FGuid& ItemID);

	/** Invalidate all UI cache */
	void InvalidateAllUICache();

	/** Rebuild item UI cache */
	void RebuildItemUICache() const;

	/** Rebuild slot UI cache */
	void RebuildSlotUICache() const;

	//==================================================================
	// Spatial Search Optimization
	//==================================================================

	/** Last successful free slot hint for heuristic search */
	mutable int32 LastFreeSlotHint = 0;

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

	/** Recalculate current weight (full recalculation - use for validation) */
	void RecalculateWeight();

	/** Incremental weight update (O(1) - use in hotpath) */
	void UpdateWeightDelta(float WeightDelta);

	/** Update grid slots for item placement */
	void UpdateGridSlots(const FSuspenseCoreItemInstance& Instance, bool bPlace);

	//==================================================================
	// Validation Layer (Development builds only)
	//==================================================================

#if !UE_BUILD_SHIPPING
	/** Validate inventory integrity - checks for desync between data structures */
	void ValidateInventoryIntegrityInternal(const FString& Context) const;

	/** Operation counter for periodic validation */
	mutable int32 ValidationOperationCounter = 0;
#endif

	/** Find item instance by ID (internal) */
	FSuspenseCoreItemInstance* FindItemInstanceInternal(const FGuid& InstanceID);
	const FSuspenseCoreItemInstance* FindItemInstanceInternal(const FGuid& InstanceID) const;
};
