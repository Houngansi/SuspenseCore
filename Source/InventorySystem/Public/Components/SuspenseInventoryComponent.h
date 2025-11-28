// SuspenseInventory/Components/SuspenseInventoryComponent.h
// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/Inventory/ISuspenseInventory.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Types/Loadout/SuspenseLoadoutSettings.h"
#include "GameplayTagContainer.h"
#include "SuspenseInventoryComponent.generated.h"

// Forward declarations
class USuspenseInventoryStorage;
class USuspenseInventoryConstraints;
class USuspenseInventoryTransaction;
class USuspenseInventoryReplicator;
class USuspenseInventoryEvents;
class USuspenseInventorySerializer;
class USuspenseInventoryUIConnector;
class USuspenseInventoryGASIntegration;
class USuspenseEventManager;
class USuspenseItemManager;
class USuspenseInventoryManager;
struct FSuspenseInventoryOperationResult;
struct FLoadoutConfiguration;

/**
 * Primary component for inventory management with DataTable architecture
 *
 * ARCHITECTURAL IMPROVEMENTS:
 * - Fully works with FSuspenseInventoryItemInstance and FGuid
 * - Does not create item objects (UObject/Actor)
 * - Uses DataTable as single source of truth
 * - Integrated with LoadoutSettings for configuration
 * - Thread-safe operations with transaction support
 */
UCLASS(ClassGroup=(Suspense), meta=(BlueprintSpawnableComponent))
class INVENTORYSYSTEM_API USuspenseInventoryComponent : public UActorComponent, public ISuspenseInventory
{
    GENERATED_BODY()

public:
    USuspenseInventoryComponent();

    // UActorComponent interface
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    //==================================================================
    // ISuspenseInventoryInterface Implementation - Core Operations
    //==================================================================

    // Core item operations (DataTable-based)
    virtual bool AddItemByID_Implementation(FName ItemID, int32 Quantity) override;
    virtual FSuspenseInventoryOperationResult AddItemInstance(const FSuspenseInventoryItemInstance& ItemInstance) override;
    virtual FSuspenseInventoryOperationResult AddItemInstanceToSlot(const FSuspenseInventoryItemInstance& ItemInstance, int32 TargetSlot) override;
    virtual FSuspenseInventoryOperationResult RemoveItemByID(const FName& ItemID, int32 Amount) override;
    virtual FSuspenseInventoryOperationResult RemoveItemInstance(const FGuid& InstanceID) override;
    virtual bool RemoveItemFromSlot(int32 SlotIndex, FSuspenseInventoryItemInstance& OutRemovedInstance) override;
    virtual TArray<FSuspenseInventoryItemInstance> GetAllItemInstances() const override;
    virtual USuspenseItemManager* GetItemManager() const override;

    // Advanced item management
    virtual int32 CreateItemsFromSpawnData(const TArray<FSuspensePickupSpawnData>& SpawnDataArray) override;
    virtual int32 ConsolidateStacks(const FName& ItemID = NAME_None) override;
    virtual FSuspenseInventoryOperationResult SplitStack(int32 SourceSlot, int32 SplitQuantity, int32 TargetSlot) override;

    //==================================================================
    // DataTable Operations
    //==================================================================

    virtual bool AddItem(const FSuspenseUnifiedItemData& ItemData, int32 Amount) override;
    virtual bool AddItemWithErrorCode(const FSuspenseUnifiedItemData& ItemData, int32 Amount, ESuspenseInventoryErrorCode& OutErrorCode) override;
    virtual bool TryAddItem_Implementation(const FSuspenseUnifiedItemData& ItemData, int32 Quantity) override;
    virtual bool RemoveItem(const FName& ItemID, int32 Amount) override;

    // Validation and item reception
    virtual bool ReceiveItem_Implementation(const FSuspenseUnifiedItemData& ItemData, int32 Quantity) override;
    virtual bool CanReceiveItem_Implementation(const FSuspenseUnifiedItemData& ItemData, int32 Quantity) const override;
    virtual FGameplayTagContainer GetAllowedItemTypes_Implementation() const override;

    // Grid placement operations
    virtual void SwapItemSlots(int32 SlotIndex1, int32 SlotIndex2) override;
    virtual int32 FindFreeSpaceForItem(const FVector2D& ItemSize, bool bAllowRotation = true) const override;
    virtual bool CanPlaceItemAtSlot(const FVector2D& ItemSize, int32 SlotIndex, bool bIgnoreRotation = false) const override;
    virtual bool CanPlaceItemInstanceAtSlot(const FSuspenseInventoryItemInstance& ItemInstance, int32 SlotIndex) const override;
    virtual bool PlaceItemInstanceAtSlot(const FSuspenseInventoryItemInstance& ItemInstance, int32 SlotIndex, bool bForcePlace = false) override;
    virtual bool TryAutoPlaceItemInstance(const FSuspenseInventoryItemInstance& ItemInstance) override;

    // Movement operations
    virtual bool MoveItemBySlots_Implementation(int32 FromSlot, int32 ToSlot, bool bMaintainRotation) override;
    virtual bool CanMoveItemToSlot_Implementation(int32 FromSlot, int32 ToSlot, bool bMaintainRotation) const override;

    // Weight management
    virtual float GetCurrentWeight_Implementation() const override;
    virtual float GetMaxWeight_Implementation() const override;
    virtual float GetRemainingWeight_Implementation() const override;
    virtual bool HasWeightCapacity_Implementation(float RequiredWeight) const override;

    // Properties and queries
    virtual FVector2D GetInventorySize() const override;
    virtual bool GetItemInstanceAtSlot(int32 SlotIndex, FSuspenseInventoryItemInstance& OutInstance) const override;
    virtual int32 GetItemCountByID(const FName& ItemID) const override;
    virtual TArray<FSuspenseInventoryItemInstance> FindItemInstancesByType(const FGameplayTag& ItemType) const override;
    virtual int32 GetTotalItemCount() const override;
    virtual bool HasItem(const FName& ItemID, int32 Amount = 1) const override;

    // UI support
    virtual bool SwapItemsInSlots(int32 Slot1, int32 Slot2, ESuspenseInventoryErrorCode& OutErrorCode) override;
    virtual bool CanSwapSlots(int32 Slot1, int32 Slot2) const override;
    virtual bool RotateItemAtSlot(int32 SlotIndex) override;
    virtual bool CanRotateItemAtSlot(int32 SlotIndex) const override;
    virtual void RefreshItemsUI() override;

    // Transaction support
    virtual void BeginTransaction() override;
    virtual void CommitTransaction() override;
    virtual void RollbackTransaction() override;
    virtual bool IsTransactionActive() const override;

    // Initialization
    virtual bool InitializeFromLoadout(const FName& LoadoutID, const FName& InventoryName = NAME_None) override;
    UFUNCTION(BlueprintCallable, Category = "Inventory|Initialization")
    bool InitializeWithSimpleSettings(int32 Width, int32 Height, float MaxWeightLimit, const FGameplayTagContainer& AllowedTypes = FGameplayTagContainer());
    virtual void SetMaxWeight(float NewMaxWeight) override;
    virtual bool IsInventoryInitialized() const override;
    virtual void SetAllowedItemTypes(const FGameplayTagContainer& Types) override;
    virtual void InitializeInventory(const struct FSuspenseInventoryConfig& Config) override;

    // Events & delegates
    virtual void BroadcastInventoryUpdated() override;
    virtual USuspenseEventManager* GetDelegateManager() const override;
    virtual void BindToInventoryUpdates(const FSuspenseOnInventoryUpdated::FDelegate& Delegate) override;
    virtual void UnbindFromInventoryUpdates(const FSuspenseOnInventoryUpdated::FDelegate& Delegate) override;

    // Debug & utility
    virtual bool GetInventoryCoordinates(int32 Index, int32& OutX, int32& OutY) const override;
    virtual int32 GetIndexFromCoordinates(int32 X, int32 Y) const override;
    virtual int32 GetFlatIndexForItem(int32 AnchorIndex, const FVector2D& ItemSize, bool bIsRotated) const override;
    virtual TArray<int32> GetOccupiedSlots(int32 AnchorIndex, const FVector2D& ItemSize, bool bIsRotated) const override;
    virtual FString GetInventoryDebugInfo() const override;
    virtual bool ValidateInventoryIntegrity(TArray<FString>& OutErrors) const override;

    //==================================================================
    // Extended API - Public Methods
    //==================================================================

    UFUNCTION(BlueprintCallable, Category = "Inventory|Items")
    bool UpdateItemAmount(const FName& ItemID, int32 NewAmount);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Items")
    TArray<FSuspenseInventoryItemInstance> GetItemInstancesByType(const FGameplayTag& ItemType) const;

    UFUNCTION(BlueprintCallable, Category = "Inventory|Items")
    bool MoveItemInstance(const FGuid& InstanceID, int32 NewSlot, bool bAllowRotation = false);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Persistence")
    bool SaveToFile(const FString& FilePath);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Persistence")
    bool LoadFromFile(const FString& FilePath);

    UFUNCTION(BlueprintCallable, Category = "Inventory|Integration")
    USuspenseInventoryEvents* GetEventsComponent() const { return EventsComponent; }

    UFUNCTION(BlueprintCallable, Category = "Inventory|Integration")
    USuspenseInventoryUIConnector* GetUIAdapter() const { return UIAdapter; }

    UFUNCTION(BlueprintCallable, Category = "Inventory|Integration")
    USuspenseInventoryGASIntegration* GetGASIntegration() const { return GASIntegration; }

    UFUNCTION(BlueprintCallable, Category = "Inventory|Debug", meta=(DevelopmentOnly))
    void LogInventoryStatistics(const FString& Context) const;

    /** Get current loadout configuration if initialized from loadout */
    const FLoadoutConfiguration* GetCurrentLoadoutConfig() const;

    /** Check if inventory was initialized from a loadout */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Loadout")
    bool IsInitializedFromLoadout() const { return !CurrentLoadoutID.IsNone(); }

    /** Get the loadout ID this inventory was initialized from */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Loadout")
    FName GetCurrentLoadoutID() const { return CurrentLoadoutID; }

    /** Get the inventory name within the loadout */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Loadout")
    FName GetInventoryNameInLoadout() const { return CurrentInventoryName; }

    /** Notify replicator about item placement */
    void NotifyItemPlaced(const FSuspenseInventoryItemInstance& ItemInstance, int32 AnchorIndex);

    /** Notify replicator about item removal */
    void NotifyItemRemoved(const FSuspenseInventoryItemInstance& ItemInstance);

    /** Get replicator component */
    USuspenseInventoryReplicator* GetReplicatorComponent() const { return ReplicatorComponent; }

protected:
    //==================================================================
    // Replicated Properties
    //==================================================================

    /** Replicated grid size for client initialization */
    UPROPERTY(ReplicatedUsing=OnRep_GridSize)
    FVector2D ReplicatedGridSize;

    /** Replicated initialization state */
    UPROPERTY(ReplicatedUsing=OnRep_InventoryState)
    bool bIsInitializedReplicated;

    /** Whether inventory is initialized locally */
    UPROPERTY(Replicated)
    bool bIsInitialized;

    /** Maximum weight capacity */
    UPROPERTY(Replicated)
    float MaxWeight;

    /** Current total weight */
    UPROPERTY(Replicated)
    float CurrentWeight;

    /** Allowed item types */
    UPROPERTY(Replicated)
    FGameplayTagContainer AllowedItemTypes;

    /** Current loadout ID if initialized from loadout */
    UPROPERTY(Replicated)
    FName CurrentLoadoutID;

    /** Current inventory name within loadout */
    UPROPERTY(Replicated)
    FName CurrentInventoryName;

    //==================================================================
    // Internal Component Properties
    //==================================================================

    UPROPERTY()
    USuspenseInventoryStorage* StorageComponent;

    UPROPERTY()
    USuspenseInventoryConstraints* ConstraintsComponent;

    UPROPERTY()
    USuspenseInventoryTransaction* TransactionComponent;

    UPROPERTY()
    USuspenseInventoryReplicator* ReplicatorComponent;

    UPROPERTY()
    USuspenseInventoryEvents* EventsComponent;

    UPROPERTY()
    USuspenseInventorySerializer* SerializerComponent;

    UPROPERTY()
    USuspenseInventoryUIConnector* UIAdapter;

    UPROPERTY()
    USuspenseInventoryGASIntegration* GASIntegration;

    /** Cached ItemManager for performance */
    mutable TWeakObjectPtr<USuspenseItemManager> CachedItemManager;

    /** Cached InventoryManager for loadout access */
    mutable TWeakObjectPtr<USuspenseInventoryManager> CachedInventoryManager;

    /** Cached delegate manager for performance */
    mutable TWeakObjectPtr<USuspenseEventManager> CachedDelegateManager;

    /** Base inventory update delegate */
    UPROPERTY(BlueprintAssignable)
    FSuspenseOnInventoryUpdated OnInventoryUpdated;

    /** Timer handle for client initialization check */
    FTimerHandle ClientInitCheckTimer;

    /** Handle for replicator update subscription */
    FDelegateHandle ReplicatorUpdateHandle;

    /** Cached loadout configuration */
    mutable TOptional<FLoadoutConfiguration> CachedLoadoutConfig;

    //==================================================================
    // Replication Callbacks
    //==================================================================

    UFUNCTION()
    void OnRep_GridSize();

    UFUNCTION()
    void OnRep_InventoryState();

    //==================================================================
    // RPC Methods
    //==================================================================

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_AddItemByID(const FName& ItemID, int32 Amount);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_RemoveItem(const FName& ItemID, int32 Amount);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_UpdateInventoryState();

private:
    //==================================================================
    // Internal Helper Methods
    //==================================================================

    /** Creates and initializes sub-components */
    void InitializeSubComponents();

    /** Initialize client-side components after replication */
    void InitializeClientComponents();

    /** Sync items from replicator (client-side) */
    void SyncItemsFromReplicator();

    /** Handles authority checks for operations */
    bool CheckAuthority(const FString& FunctionName) const;

    /** Updates current weight based on items */
    void UpdateCurrentWeight();

    /** Helper method to validate item placement at specific position */
    bool ValidateItemPlacement(const FVector2D& ItemSize, int32 TargetSlot, const FGuid& ExcludeInstanceID = FGuid()) const;

    /** Execute item swap between two positions */
    bool ExecuteSlotSwap(int32 Slot1, int32 Slot2, ESuspenseInventoryErrorCode& OutErrorCode);

    /** Get inventory manager subsystem */
    USuspenseInventoryManager* GetInventoryManager() const;

    /** Helper method to get game instance safely */
    UGameInstance* GetGameInstance() const;

    /**
     * Try to move item instance to specific slot
     * @param InstanceID Instance to move
     * @param NewSlot Target grid slot
     * @param bAllowRotation Allow rotation for fit
     * @param OutErrorCode Detailed error code on failure
     * @return true if item was successfully moved
     */
    bool TryMoveInstanceToSlot(const FGuid& InstanceID, int32 NewSlot, bool bAllowRotation, ESuspenseInventoryErrorCode& OutErrorCode);
};
