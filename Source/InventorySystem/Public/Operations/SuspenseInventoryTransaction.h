// MedComInventory/Operations/SuspenseInventoryTransaction.h
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Types/Loadout/SuspenseItemDataTable.h"
#include "Operations/InventoryResult.h"
#include "Storage/SuspenseInventoryStorage.h"
#include "Validation/SuspenseInventoryValidator.h"
#include "SuspenseInventoryTransaction.generated.h"

// Forward declarations
class USuspenseInventoryEvents;
class USuspenseItemManager;

/**
 * Transaction types for inventory operations
 */
UENUM(BlueprintType)
enum class EInventoryTransactionType : uint8
{
    Add         UMETA(DisplayName = "Add"),
    Remove      UMETA(DisplayName = "Remove"),
    Move        UMETA(DisplayName = "Move"),
    Swap        UMETA(DisplayName = "Swap"),
    Stack       UMETA(DisplayName = "Stack"),
    Split       UMETA(DisplayName = "Split"),
    Rotate      UMETA(DisplayName = "Rotate"),
    Update      UMETA(DisplayName = "Update"),
    Custom      UMETA(DisplayName = "Custom")
};

/**
 * ОБНОВЛЕНО: Handles atomic operations on inventory to ensure consistency and rollback capability
 * Полностью интегрирован с новой DataTable архитектурой и FSuspenseInventoryItemInstance системой
 * 
 * АРХИТЕКТУРНЫЕ УЛУЧШЕНИЯ:
 * - Работа с FSuspenseUnifiedItemData из DataTable как источника истины
 * - Использование FSuspenseInventoryItemInstance для runtime данных
 * - Интеграция с ItemManager для централизованного управления данными
 * - Расширенная система backup с поддержкой runtime properties
 * - Улучшенная валидация и error handling
 */
UCLASS(BlueprintType)
class INVENTORYSYSTEM_API USuspenseInventoryTransaction : public UObject
{
    GENERATED_BODY()
    
public:
    USuspenseInventoryTransaction();

    /**
     * ОБНОВЛЕНО: Initializes the transaction handler with ItemManager integration
     * @param InStorage Storage component to operate on
     * @param InConstraints Constraints component for validation
     * @param InItemManager ItemManager for DataTable access
     * @param InEvents Optional events component for notifications
     */
    void Initialize(USuspenseInventoryStorage* InStorage,
                   USuspenseInventoryValidator* InConstraints,
                   USuspenseItemManager* InItemManager,
                   USuspenseInventoryEvents* InEvents = nullptr);
    
    /**
     * Begins a new transaction
     * @param Type Transaction type
     * @param Context Context name for logging
     * @return True if transaction was successfully started
     */
    UFUNCTION(BlueprintCallable, Category = "InventoryTransaction")
    bool BeginTransaction(EInventoryTransactionType Type, const FName& Context);
    
    /**
     * Commits the current transaction
     * @return True if transaction was successfully committed
     */
    UFUNCTION(BlueprintCallable, Category = "InventoryTransaction")
    bool CommitTransaction();
    
    /**
     * Rolls back the current transaction
     */
    UFUNCTION(BlueprintCallable, Category = "InventoryTransaction")
    void RollbackTransaction();
    
    //==================================================================
    // ОБНОВЛЕННЫЕ МЕТОДЫ: DataTable Integration
    //==================================================================
    
    /**
     * ОБНОВЛЕНО: Adds an item to the inventory using DataTable ID
     * @param ItemID ID of the item in DataTable
     * @param Amount Amount to add
     * @return Operation result with created FSuspenseInventoryItemInstance
     */
    UFUNCTION(BlueprintCallable, Category = "InventoryTransaction")
    FSuspenseInventoryOperationResult AddItemByID(const FName& ItemID, int32 Amount);
    
    /**
     * НОВОЕ: Adds an item using unified DataTable structure
     * @param ItemData Unified item data from DataTable
     * @param Amount Amount to add
     * @return Operation result
     */
    UFUNCTION(BlueprintCallable, Category = "InventoryTransaction")
    FSuspenseInventoryOperationResult AddItemFromData(const FSuspenseUnifiedItemData& ItemData, int32 Amount);
    
    /**
     * НОВОЕ: Adds a fully constructed item instance
     * @param ItemInstance Complete item instance with runtime properties
     * @return Operation result
     */
    UFUNCTION(BlueprintCallable, Category = "InventoryTransaction")
    FSuspenseInventoryOperationResult AddItemInstance(const FSuspenseInventoryItemInstance& ItemInstance);

    /**
     * Removes an item from the inventory
     * @param ItemID ID of the item to remove
     * @param Amount Amount to remove
     * @return Operation result
     */
    UFUNCTION(BlueprintCallable, Category = "InventoryTransaction")
    FSuspenseInventoryOperationResult RemoveItem(const FName& ItemID, int32 Amount);

    /**
     * НОВОЕ: Removes a specific item instance
     * @param InstanceID Unique instance ID to remove
     * @return Operation result
     */
    UFUNCTION(BlueprintCallable, Category = "InventoryTransaction")
    FSuspenseInventoryOperationResult RemoveItemInstance(const FGuid& InstanceID);

    /**
     * Moves an item to a new position
     * @param ItemObject Item to move
     * @param NewAnchorIndex New position
     * @param bShouldRotate Whether to rotate the item
     * @return Operation result
     */
    UFUNCTION(BlueprintCallable, Category = "InventoryTransaction")
    FSuspenseInventoryOperationResult MoveItem(UObject* ItemObject, int32 NewAnchorIndex, bool bShouldRotate);
    
    /**
     * НОВОЕ: Moves an item instance by ID
     * @param InstanceID Instance to move
     * @param NewAnchorIndex New position
     * @param bShouldRotate Whether to rotate the item
     * @return Operation result
     */
    UFUNCTION(BlueprintCallable, Category = "InventoryTransaction")
    FSuspenseInventoryOperationResult MoveItemInstance(const FGuid& InstanceID, int32 NewAnchorIndex, bool bShouldRotate);
    
    /**
     * Rotates an item
     * @param ItemObject Item to rotate
     * @param bDesiredRotation Desired rotation state
     * @return Operation result
     */
    UFUNCTION(BlueprintCallable, Category = "InventoryTransaction")
    FSuspenseInventoryOperationResult RotateItem(UObject* ItemObject, bool bDesiredRotation);
    
    /**
     * Stacks items together
     * @param SourceItem Source item
     * @param TargetItem Target item
     * @param Amount Amount to transfer
     * @return Operation result
     */
    UFUNCTION(BlueprintCallable, Category = "InventoryTransaction")
    FSuspenseInventoryOperationResult StackItems(UObject* SourceItem, UObject* TargetItem, int32 Amount);
    
    /**
     * НОВОЕ: Stacks item instances by ID
     * @param SourceInstanceID Source instance ID
     * @param TargetInstanceID Target instance ID
     * @param Amount Amount to transfer
     * @return Operation result
     */
    UFUNCTION(BlueprintCallable, Category = "InventoryTransaction")
    FSuspenseInventoryOperationResult StackItemInstances(const FGuid& SourceInstanceID, const FGuid& TargetInstanceID, int32 Amount);
    
    /**
     * Splits a stack of items
     * @param SourceItem Source stack
     * @param TargetPosition Target position for the new stack
     * @param Amount Amount to split
     * @return Operation result
     */
    UFUNCTION(BlueprintCallable, Category = "InventoryTransaction")
    FSuspenseInventoryOperationResult SplitStack(UObject* SourceItem, int32 TargetPosition, int32 Amount);
    
    /**
     * Swaps two items
     * @param FirstItem First item
     * @param SecondItem Second item
     * @return Operation result
     */
    UFUNCTION(BlueprintCallable, Category = "InventoryTransaction")
    FSuspenseInventoryOperationResult SwapItems(UObject* FirstItem, UObject* SecondItem);
    
    /**
     * НОВОЕ: Updates runtime properties of an item instance
     * @param InstanceID Instance to update
     * @param NewProperties New runtime properties
     * @return Operation result
     */
    UFUNCTION(BlueprintCallable, Category = "InventoryTransaction")
    FSuspenseInventoryOperationResult UpdateItemProperties(const FGuid& InstanceID, const TMap<FName, float>& NewProperties);
    
    //==================================================================
    // STATUS AND UTILITY METHODS
    //==================================================================
    
    /**
     * Checks if a transaction is in progress
     * @return True if transaction is active
     */
    UFUNCTION(BlueprintPure, Category = "InventoryTransaction")
    bool IsTransactionActive() const { return bTransactionActive; }
    
    /**
     * Gets the current transaction context
     * @return Current context or NAME_None if no transaction
     */
    UFUNCTION(BlueprintPure, Category = "InventoryTransaction")
    FName GetCurrentContext() const { return CurrentContext; }
    
    /**
     * Gets the current transaction type
     * @return Current transaction type
     */
    UFUNCTION(BlueprintPure, Category = "InventoryTransaction")
    EInventoryTransactionType GetCurrentType() const { return CurrentType; }
    
    /**
     * НОВОЕ: Gets the ItemManager reference
     * @return ItemManager for DataTable operations
     */
    UFUNCTION(BlueprintPure, Category = "InventoryTransaction")
    USuspenseItemManager* GetItemManager() const { return ItemManager; }
    
    /**
     * НОВОЕ: Gets transaction statistics
     * @return Debug string with transaction info
     */
    UFUNCTION(BlueprintCallable, Category = "InventoryTransaction")
    FString GetTransactionDebugInfo() const;
    
private:
    /** Storage component to operate on */
    UPROPERTY()
    USuspenseInventoryStorage* Storage;
    
    /** Constraints component for validation */
    UPROPERTY()
    USuspenseInventoryValidator* Constraints;
    
    /** НОВОЕ: ItemManager for DataTable access */
    UPROPERTY()
    USuspenseItemManager* ItemManager;
    
    /** Events component for notifications */
    UPROPERTY()
    USuspenseInventoryEvents* Events;
    
    /** Whether a transaction is currently active */
    bool bTransactionActive;
    
    /** Current transaction type */
    EInventoryTransactionType CurrentType;
    
    /** Current transaction context (for logging) */
    FName CurrentContext;
    
    /** ОБНОВЛЕНО: Backup of item instances with full runtime state */
    UPROPERTY()
    TArray<FSuspenseInventoryItemInstance> BackupItemInstances;
    
    /** Backup of the item objects before transaction (legacy support) */
    UPROPERTY()
    TArray<UObject*> BackupItemObjects;
    
    /** НОВОЕ: Backup of grid cell states */
    UPROPERTY()
    TArray<FInventoryCell> BackupCells;
    
    /** Items created during this transaction (for cleanup on rollback) */
    UPROPERTY()
    TArray<UObject*> CreatedItems;
    
    /** НОВОЕ: Item instances created during transaction */
    UPROPERTY()
    TArray<FSuspenseInventoryItemInstance> CreatedInstances;
    
    /** НОВОЕ: Transaction start time for performance tracking */
    double TransactionStartTime;
    
    /** НОВОЕ: Number of operations in current transaction */
    int32 OperationCount;
    
    //==================================================================
    // INTERNAL HELPER METHODS
    //==================================================================
    
    /**
     * ОБНОВЛЕНО: Creates a backup of the current storage state
     */
    void CreateStorageBackup();
    
    /**
     * ОБНОВЛЕНО: Restores storage from backup
     */
    void RestoreStorageFromBackup();
    
    /**
     * ОБНОВЛЕНО: Destroys any items created during the transaction
     */
    void DestroyCreatedItems();
    
    /**
     * НОВОЕ: Creates an FSuspenseInventoryItemInstance from DataTable ID
     * @param ItemID ID in DataTable
     * @param Amount Quantity for the instance
     * @param Context Operation context
     * @return Operation result with created instance
     */
    FSuspenseInventoryOperationResult CreateItemInstanceFromID(const FName& ItemID, int32 Amount, const FName& Context);
    
    /**
     * НОВОЕ: Creates an FSuspenseInventoryItemInstance from unified data
     * @param ItemData Unified item data
     * @param Amount Quantity for the instance
     * @param Context Operation context
     * @return Operation result with created instance
     */
    FSuspenseInventoryOperationResult CreateItemInstanceFromData(const FSuspenseUnifiedItemData& ItemData, int32 Amount, const FName& Context);
    
    /**
     * УСТАРЕЛО: Creates an item object (legacy support)
     * @param ItemData Item data to create from
     * @param Amount Amount for the new item
     * @param Context Context name for logging
     * @return Operation result
     */
    FSuspenseInventoryOperationResult CreateItemObject(const FSuspenseUnifiedItemData& ItemData, int32 Amount, const FName& Context);
    
    /**
     * НОВОЕ: Validates item instance before operations
     * @param Instance Instance to validate
     * @param Context Operation context
     * @return Validation result
     */
    FSuspenseInventoryOperationResult ValidateItemInstance(const FSuspenseInventoryItemInstance& Instance, const FName& Context);
    
    /**
     * НОВОЕ: Finds free space for item instance
     * @param Instance Item instance to place
     * @return Anchor index or INDEX_NONE if no space
     */
    int32 FindFreeSpaceForInstance(const FSuspenseInventoryItemInstance& Instance);
    
    /**
     * НОВОЕ: Places item instance in storage
     * @param Instance Instance to place
     * @param AnchorIndex Position to place at
     * @return Success result
     */
    FSuspenseInventoryOperationResult PlaceItemInstanceInStorage(const FSuspenseInventoryItemInstance& Instance, int32 AnchorIndex);
    
    /**
     * Logs a transaction operation
     * @param Action Description of the action
     * @param Result Operation result
     */
    void LogTransactionOperation(const FString& Action, const FSuspenseInventoryOperationResult& Result);
    
    /**
     * НОВОЕ: Updates transaction statistics
     */
    void UpdateTransactionStats();
    
    /**
     * НОВОЕ: Validates transaction prerequisites
     * @return True if transaction can proceed
     */
    bool ValidateTransactionPrerequisites() const;
};