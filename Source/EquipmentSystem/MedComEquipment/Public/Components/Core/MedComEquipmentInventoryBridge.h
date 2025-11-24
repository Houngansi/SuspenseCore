// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/Equipment/IMedComEquipmentDataProvider.h"
#include "Interfaces/Equipment/IMedComEquipmentOperations.h"
#include "Interfaces/Equipment/IMedComInventoryBridge.h"
#include "Interfaces/Equipment/IMedComTransactionManager.h"
#include "Interfaces/Inventory/IMedComInventoryInterface.h"
#include "Services/EquipmentOperationServiceImpl.h"
#include "MedComEquipmentInventoryBridge.generated.h"

// Forward declaration
class UEventDelegateManager;

/**
 * Bridge component for seamless item transfer between inventory and equipment systems.
 * Provides atomic transactions, validation, and rollback support for all transfer operations.
 * 
 * NEW: Integrated with EventDelegateManager for UI-driven equipment operations.
 * Listens to equipment operation requests from UI and broadcasts results back.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class MEDCOMEQUIPMENT_API UMedComEquipmentInventoryBridge : public UActorComponent
{
    GENERATED_BODY()
    
public:
    UMedComEquipmentInventoryBridge();
    
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
        TScriptInterface<IMedComEquipmentDataProvider> InEquipmentData,
        TScriptInterface<IMedComEquipmentOperations> InEquipmentOps,
        TScriptInterface<IMedComTransactionManager> InTransactionMgr
    );
    
    /**
     * Set the inventory interface for bridge operations
     * @param InInventoryInterface - Inventory system interface
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Bridge")
    void SetInventoryInterface(TScriptInterface<IMedComInventoryInterface> InInventoryInterface);
    
    // ===== Transfer Operations =====
    
    /**
     * Transfer item from inventory to equipment slot
     * @param Request - Transfer request containing item and target slot info
     * @return Operation result with success/failure status
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Bridge")
    FInventoryOperationResult TransferFromInventory(const FInventoryTransferRequest& Request);
    
    /**
     * Transfer item from equipment slot to inventory
     * @param Request - Transfer request containing source slot info
     * @return Operation result with success/failure status
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Bridge")
    FInventoryOperationResult TransferToInventory(const FInventoryTransferRequest& Request);
    
    /**
     * Atomically swap items between inventory and equipment
     * @param InventoryItemInstanceID - Instance ID of item in inventory
     * @param EquipmentSlotIndex - Target equipment slot index
     * @return Operation result with affected items
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Bridge")
    FInventoryOperationResult SwapBetweenInventoryAndEquipment(
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
    bool CanEquipFromInventory(const FInventoryItemInstance& Item, int32 TargetSlot) const;
    
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
 void BroadcastEquippedEvent(const FInventoryItemInstance& Item, int32 SlotIndex);

 /**
  * Broadcasts both Unequipped and Equipped events for SWAP operations
  * Ensures proper cleanup of old visual and attachment of new visual
  * 
  * @param NewItem - Item being equipped (from inventory)
  * @param OldItem - Item being unequipped (to inventory)
  * @param SlotIndex - Equipment slot involved in swap
  */
 void BroadcastSwapEvents(
     const FInventoryItemInstance& NewItem, 
     const FInventoryItemInstance& OldItem, 
     int32 SlotIndex);
private:
    // ===== Dependencies =====
    
    UPROPERTY()
    TScriptInterface<IMedComEquipmentDataProvider> EquipmentDataProvider;
    
    UPROPERTY()
    TScriptInterface<IMedComEquipmentOperations> EquipmentOperations;
    
    UPROPERTY()
    TScriptInterface<IMedComTransactionManager> TransactionManager;
    
    UPROPERTY()
    TScriptInterface<IMedComInventoryInterface> InventoryInterface;
    
    UPROPERTY()
    TScriptInterface<IEquipmentOperationService> EquipmentService;
    
    // ===== EventDelegateManager Integration =====
    
    /** Reference to centralized event system for UI-driven operations */
    TWeakObjectPtr<UEventDelegateManager> EventDelegateManager;
     void BroadcastUnequippedEvent(const FInventoryItemInstance& Item, int32 SlotIndex);
    /** Handle for equipment operation request subscription */
    FDelegateHandle EquipmentOperationRequestHandle;
    
    /**
     * Handler for equipment operation requests from UI
     * Processes requests and broadcasts results back through EventDelegateManager
     * @param Request - Equipment operation request from UI layer
     */
    void HandleEquipmentOperationRequest(const FEquipmentOperationRequest& Request);
    
    // ===== Transaction Support =====
    
    /**
     * Internal transaction state for bridge operations
     */
    struct FBridgeTransaction
    {
        FGuid TransactionID;
        FInventoryItemInstance InventoryBackup;
        FInventoryItemInstance EquipmentBackup;
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
        FInventoryItemInstance ReservedItem;
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
    FInventoryOperationResult ExecuteTransfer_FromInventoryToEquip(const FInventoryTransferRequest& Request);
    
    /**
     * Execute transfer from equipment to inventory with rollback support
     */
    FInventoryOperationResult ExecuteTransfer_FromEquipToInventory(const FInventoryTransferRequest& Request);
    
    /**
     * Execute atomic swap between inventory and equipment systems
     */
    FInventoryOperationResult ExecuteSwap_InventoryToEquipment(const FGuid& InventoryInstanceID, int32 EquipmentSlot);
    
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
    bool ValidateInventorySpace(const FInventoryItemInstance& Item) const;
    
    /**
     * Validate equipment slot compatibility with item
     */
    bool ValidateEquipmentSlot(int32 SlotIndex, const FInventoryItemInstance& Item) const;
    
    /**
     * Check if inventory has space for item (simplified check)
     */
    bool InventoryHasSpace(const FInventoryItemInstance& Item) const;
    
    // ===== Helper Functions =====
    
    /**
     * Clean up expired reservations
     */
    void CleanupExpiredReservations();
    
    /**
     * Find item in inventory by item ID
     */
    bool FindItemInInventory(const FName& ItemId, FInventoryItemInstance& OutInstance) const;

    /** Flag to prevent double initialization and double subscription */
    UPROPERTY(Transient)
    bool bIsInitialized = false;
    
    /** Cache of processed operation IDs to prevent duplicate handling */
    UPROPERTY(Transient)
    TSet<FGuid> ProcessedOperationIds;
    
    /** Thread-safe lock for operation cache access */
    mutable FCriticalSection OperationCacheLock;
};