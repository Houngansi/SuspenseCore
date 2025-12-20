// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentDataProvider.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentOperations.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreTransactionManager.h"
#include "SuspenseCore/Interfaces/Inventory/ISuspenseCoreInventory.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentOperationService.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Types/Items/SuspenseCoreItemTypes.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "SuspenseCore/Types/Equipment/SuspenseCoreEquipmentTypes.h"
#include "SuspenseCoreEquipmentInventoryBridge.generated.h"

// Forward declaration
class USuspenseCoreEventManager;

/**
 * Transfer request for SuspenseCore inventory-equipment bridge operations
 */
USTRUCT(BlueprintType)
struct EQUIPMENTSYSTEM_API FSuspenseCoreInventoryTransferRequest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Transfer")
    FSuspenseCoreItemInstance Item;

    UPROPERTY(BlueprintReadWrite, Category = "Transfer")
    int32 SourceSlot = INDEX_NONE;

    UPROPERTY(BlueprintReadWrite, Category = "Transfer")
    int32 TargetSlot = INDEX_NONE;

    UPROPERTY(BlueprintReadWrite, Category = "Transfer")
    bool bFromInventory = true;

    UPROPERTY(BlueprintReadWrite, Category = "Transfer")
    bool bToInventory = false;

    bool IsValid() const { return Item.IsValid(); }
};

/**
 * Bridge component for seamless item transfer between inventory and equipment systems.
 * Provides atomic transactions, validation, and rollback support for all transfer operations.
 *
 * NEW: Integrated with EventDelegateManager for UI-driven equipment operations.
 * Listens to equipment operation requests from UI and broadcasts results back.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentInventoryBridge : public UActorComponent
{
    GENERATED_BODY()

public:
    USuspenseCoreEquipmentInventoryBridge();

    // ===== Initialization =====

    /**
     * Initialize the bridge with required equipment system dependencies
     * @param InEquipmentData - Equipment data provider interface
     * @param InEquipmentOps - Equipment operations executor interface
     * @param InTransactionMgr - Transaction manager for atomic operations
     * @return true if initialization successful
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Bridge")
    bool Initialize(
        TScriptInterface<ISuspenseCoreEquipmentDataProvider> InEquipmentData,
        TScriptInterface<ISuspenseCoreEquipmentOperations> InEquipmentOps,
        TScriptInterface<ISuspenseCoreTransactionManager> InTransactionMgr
    );

    /**
     * Set the inventory interface for bridge operations
     * @param InInventoryInterface - Inventory system interface
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Bridge")
    void SetInventoryInterface(TScriptInterface<ISuspenseCoreInventory> InInventoryInterface);

    // ===== Transfer Operations =====

    /**
     * Transfer item from inventory to equipment slot
     * @param Request - Transfer request containing item and target slot info
     * @return Operation result with success/failure status
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Bridge")
    FSuspenseCoreInventorySimpleResult TransferFromInventory(const FSuspenseCoreInventoryTransferRequest& Request);

    /**
     * Transfer item from equipment slot to inventory
     * @param Request - Transfer request containing source slot info
     * @return Operation result with success/failure status
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Bridge")
    FSuspenseCoreInventorySimpleResult TransferToInventory(const FSuspenseCoreInventoryTransferRequest& Request);

    /**
     * Atomically swap items between inventory and equipment
     * @param InventoryItemInstanceID - Instance ID of item in inventory
     * @param EquipmentSlotIndex - Target equipment slot index
     * @return Operation result with affected items
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Bridge")
    FSuspenseCoreInventorySimpleResult SwapBetweenInventoryAndEquipment(
        const FGuid& InventoryItemInstanceID,
        int32 EquipmentSlotIndex
    );

    // ===== Synchronization =====

    /**
     * Synchronize equipment state with current inventory contents
     * Updates equipped items if their instances changed in inventory
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Bridge")
    void SynchronizeWithInventory();

    // ===== Validation Helpers =====

    /**
     * Check if item from inventory can be equipped to target slot
     * @param Item - Item instance to validate
     * @param TargetSlot - Target equipment slot index
     * @return true if item can be equipped
     */
    UFUNCTION(BlueprintPure, Category = "Equipment|Bridge")
    bool CanEquipFromInventory(const FSuspenseCoreItemInstance& Item, int32 TargetSlot) const;

    /**
     * Check if item can be unequipped to inventory
     * @param SourceSlot - Source equipment slot index
     * @return true if item can be unequipped and inventory has space
     */
    UFUNCTION(BlueprintPure, Category = "Equipment|Bridge")
    bool CanUnequipToInventory(int32 SourceSlot) const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Возвращает персонажа (Pawn/Character) для визуализации; фоллбэк — владелец компонента */
    AActor* ResolveCharacterTarget() const;
 /**
  * Broadcasts Equipment.Event.Equipped to EventBus for visualization
  * Triggers attachment of visual equipment actor to character mesh
  *
  * @param Item - Item instance that was equipped
  * @param SlotIndex - Equipment slot index where item was placed
  */
 void BroadcastEquippedEvent(const FSuspenseCoreItemInstance& Item, int32 SlotIndex);

 /**
  * Broadcasts both Unequipped and Equipped events for SWAP operations
  * Ensures proper cleanup of old visual and attachment of new visual
  *
  * @param NewItem - Item being equipped (from inventory)
  * @param OldItem - Item being unequipped (to inventory)
  * @param SlotIndex - Equipment slot involved in swap
  */
 void BroadcastSwapEvents(
     const FSuspenseCoreItemInstance& NewItem,
     const FSuspenseCoreItemInstance& OldItem,
     int32 SlotIndex);
private:
    // ===== Dependencies =====

    UPROPERTY()
    TScriptInterface<ISuspenseCoreEquipmentDataProvider> EquipmentDataProvider;

    UPROPERTY()
    TScriptInterface<ISuspenseCoreEquipmentOperations> EquipmentOperations;

    UPROPERTY()
    TScriptInterface<ISuspenseCoreTransactionManager> TransactionManager;

    UPROPERTY()
    TScriptInterface<ISuspenseCoreInventory> InventoryInterface;

    UPROPERTY()
    TScriptInterface<ISuspenseCoreEquipmentOperationServiceInterface> EquipmentService;

    // ===== EventDelegateManager Integration =====

    /** Reference to centralized event system for UI-driven operations */
    TWeakObjectPtr<USuspenseCoreEventManager> EventDelegateManager;
     void BroadcastUnequippedEvent(const FSuspenseCoreItemInstance& Item, int32 SlotIndex);
    /** Handle for equipment operation request subscription */
    FSuspenseCoreSubscriptionHandle EquipmentOperationRequestHandle;

    /** Handle for TransferItem request subscription (Equipment→Inventory drag-drop) */
    FSuspenseCoreSubscriptionHandle TransferItemRequestHandle;

    /** Handle for UnequipItem request subscription (context menu unequip) */
    FSuspenseCoreSubscriptionHandle UnequipItemRequestHandle;

    /**
     * Handler for equipment operation requests from UI
     * Processes requests and broadcasts results back through EventDelegateManager
     * @param Request - Equipment operation request from UI layer
     */
    void HandleEquipmentOperationRequest(const FEquipmentOperationRequest& Request);

    /**
     * Handler for TransferItem requests (cross-container drag-drop)
     * Called when item is dragged from equipment to inventory
     */
    void OnTransferItemRequest(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

    /**
     * Handler for UnequipItem requests (context menu)
     * Called when user selects "Unequip" from context menu
     */
    void OnUnequipItemRequest(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

    // ===== Transaction Support =====

    /**
     * Internal transaction state for bridge operations
     */
    struct FBridgeTransaction
    {
        FGuid TransactionID;
        FSuspenseCoreItemInstance InventoryBackup;
        FSuspenseCoreItemInstance EquipmentBackup;
        int32 InventorySlot;
        int32 EquipmentSlot;
        bool bInventoryModified;
        bool bEquipmentModified;

        FBridgeTransaction()
            : InventorySlot(INDEX_NONE)
            , EquipmentSlot(INDEX_NONE)
            , bInventoryModified(false)
            , bEquipmentModified(false)
        {}
    };

    /** Active bridge transactions for rollback support */
    TMap<FGuid, FBridgeTransaction> ActiveTransactions;

    /** Critical section for thread-safe transaction operations */
    mutable FCriticalSection TransactionLock;

    // ===== Legacy Reservation System =====

    /**
     * Item reservation for two-phase operations (kept for compatibility)
     */
    struct FItemReservation
    {
        FGuid ReservationID;
        FSuspenseCoreItemInstance ReservedItem;
        int32 TargetSlot;
        float ExpirationTime;
    };

    /** Active item reservations */
    TMap<FGuid, FItemReservation> ActiveReservations;

    /** Default reservation timeout in seconds */
    static constexpr float ReservationTimeout = 5.0f;

    // ===== Internal Transfer Implementations =====

    /**
     * Execute transfer from inventory to equipment with full validation
     */
    FSuspenseCoreInventorySimpleResult ExecuteTransfer_FromInventoryToEquip(const FSuspenseCoreInventoryTransferRequest& Request);

    /**
     * Execute transfer from equipment to inventory with rollback support
     */
    FSuspenseCoreInventorySimpleResult ExecuteTransfer_FromEquipToInventory(const FSuspenseCoreInventoryTransferRequest& Request);

    /**
     * Execute atomic swap between inventory and equipment systems
     */
    FSuspenseCoreInventorySimpleResult ExecuteSwap_InventoryToEquipment(const FGuid& InventoryInstanceID, int32 EquipmentSlot);

    // ===== Transaction Management =====

    /**
     * Begin a new bridge transaction
     * @return Transaction ID for tracking
     */
    FGuid BeginBridgeTransaction();

    /**
     * Commit a bridge transaction
     * @param TransactionID - Transaction to commit
     * @return true if commit successful
     */
    bool CommitBridgeTransaction(const FGuid& TransactionID);

    /**
     * Rollback a bridge transaction, restoring original state
     * @param TransactionID - Transaction to rollback
     * @return true if rollback successful
     */
    bool RollbackBridgeTransaction(const FGuid& TransactionID);

    // ===== Validation Utilities =====

    /**
     * Validate that inventory has space for item
     */
    bool ValidateInventorySpace(const FSuspenseCoreItemInstance& Item) const;

    /**
     * Validate equipment slot compatibility with item
     */
    bool ValidateEquipmentSlot(int32 SlotIndex, const FSuspenseCoreItemInstance& Item) const;

    /**
     * Check if inventory has space for item (simplified check)
     */
    bool InventoryHasSpace(const FSuspenseCoreItemInstance& Item) const;

    // ===== Helper Functions =====

    /**
     * Clean up expired reservations
     */
    void CleanupExpiredReservations();

    /**
     * Find item in inventory by item ID
     */
    bool FindItemInInventory(const FName& ItemId, FSuspenseCoreItemInstance& OutInstance) const;

    /** Flag to prevent double initialization and double subscription */
    UPROPERTY(Transient)
    bool bIsInitialized = false;

    /** Cache of processed operation IDs to prevent duplicate handling */
    UPROPERTY(Transient)
    TSet<FGuid> ProcessedOperationIds;

    /** Thread-safe lock for operation cache access */
    mutable FCriticalSection OperationCacheLock;
};
