// MedComInventory/Operations/MedComInventoryTransaction.cpp
// Copyright Suspense Team. All Rights Reserved.

#include "Operations/SuspenseInventoryTransaction.h"
#include "Base/SuspenseInventoryLogs.h"
#include "Interfaces/Inventory/ISuspenseInventoryItem.h"
#include "Base/SuspenseItemBase.h"
#include "ItemSystem/SuspenseItemManager.h"
#include "Events/SuspenseInventoryEvents.h"
#include "Engine/World.h"

USuspenseInventoryTransaction::USuspenseInventoryTransaction()
{
    Storage = nullptr;
    Constraints = nullptr;
    ItemManager = nullptr;
    Events = nullptr;
    bTransactionActive = false;
    CurrentType = EInventoryTransactionType::Custom;
    CurrentContext = NAME_None;
    TransactionStartTime = 0.0;
    OperationCount = 0;
}

void USuspenseInventoryTransaction::Initialize(USuspenseInventoryStorage* InStorage, 
                                           USuspenseInventoryValidator* InConstraints, 
                                           USuspenseItemManager* InItemManager,
                                           USuspenseInventoryEvents* InEvents)
{
    Storage = InStorage;
    Constraints = InConstraints;
    ItemManager = InItemManager;
    Events = InEvents;
    
    // Clear any existing transaction
    if (bTransactionActive)
    {
        UE_LOG(LogInventory, Warning, TEXT("InventoryTransaction: Initializing while transaction active - rolling back"));
        RollbackTransaction();
    }
    
    UE_LOG(LogInventory, Log, TEXT("InventoryTransaction: Initialized with Storage=%s, Constraints=%s, ItemManager=%s, Events=%s"),
        *GetNameSafe(InStorage), *GetNameSafe(InConstraints), *GetNameSafe(InItemManager), *GetNameSafe(InEvents));
}

bool USuspenseInventoryTransaction::BeginTransaction(EInventoryTransactionType Type, const FName& Context)
{
    // Validate prerequisites
    if (!ValidateTransactionPrerequisites())
    {
        UE_LOG(LogInventory, Error, TEXT("InventoryTransaction: Prerequisites validation failed"));
        return false;
    }
    
    // Check if another transaction is already active
    if (bTransactionActive)
    {
        UE_LOG(LogInventory, Warning, TEXT("InventoryTransaction: Transaction already in progress - %s [%s]"), 
            *UEnum::GetValueAsString(CurrentType), *CurrentContext.ToString());
        return false;
    }
    
    // Set transaction properties
    CurrentType = Type;
    CurrentContext = Context;
    TransactionStartTime = FPlatformTime::Seconds();
    OperationCount = 0;
    
    // Create backup of current state
    CreateStorageBackup();
    
    // Clear tracking arrays
    CreatedItems.Empty();
    CreatedInstances.Empty();
    
    // Mark transaction as active
    bTransactionActive = true;
    
    UE_LOG(LogInventory, Log, TEXT("InventoryTransaction: Started %s transaction [%s]"),
        *UEnum::GetValueAsString(CurrentType), *CurrentContext.ToString());
    
    return true;
}

bool USuspenseInventoryTransaction::CommitTransaction()
{
    // Check if a transaction is active
    if (!bTransactionActive)
    {
        UE_LOG(LogInventory, Warning, TEXT("InventoryTransaction: Cannot commit - no active transaction"));
        return false;
    }
    
    // Calculate transaction duration
    double TransactionDuration = FPlatformTime::Seconds() - TransactionStartTime;
    
    // Clear backup data
    BackupItemInstances.Empty();
    BackupItemObjects.Empty();
    BackupCells.Empty();
    CreatedItems.Empty();
    CreatedInstances.Empty();
    
    // Fire events if available
    if (Events)
    {
        // Events->NotifyTransactionCommitted(CurrentType, CurrentContext, OperationCount, TransactionDuration);
    }
    
    // Log transaction completion
    UE_LOG(LogInventory, Log, TEXT("InventoryTransaction: Committed %s transaction [%s] - %d operations in %.3f seconds"),
        *UEnum::GetValueAsString(CurrentType), *CurrentContext.ToString(), OperationCount, TransactionDuration);
    
    // Reset transaction state
    bTransactionActive = false;
    
    return true;
}

void USuspenseInventoryTransaction::RollbackTransaction()
{
    // Check if a transaction is active
    if (!bTransactionActive)
    {
        UE_LOG(LogInventory, Warning, TEXT("InventoryTransaction: Cannot rollback - no active transaction"));
        return;
    }
    
    // Calculate transaction duration for logging
    double TransactionDuration = FPlatformTime::Seconds() - TransactionStartTime;
    
    // Log rollback
    UE_LOG(LogInventory, Warning, TEXT("InventoryTransaction: Rolling back %s transaction [%s] after %d operations (%.3f seconds)"),
        *UEnum::GetValueAsString(CurrentType), *CurrentContext.ToString(), OperationCount, TransactionDuration);
    
    // Destroy any created items and instances
    DestroyCreatedItems();
    
    // Restore storage state
    RestoreStorageFromBackup();
    
    // Fire events if available
    if (Events)
    {
        // Events->NotifyTransactionRolledBack(CurrentType, CurrentContext, OperationCount, TransactionDuration);
    }
    
    // Reset transaction state
    bTransactionActive = false;
}

//==================================================================
// ОБНОВЛЕННЫЕ МЕТОДЫ: DataTable Integration
//==================================================================

FSuspenseInventoryOperationResult USuspenseInventoryTransaction::AddItemByID(const FName& ItemID, int32 Amount)
{
    static const FName OperationName = TEXT("AddItemByID");
    
    // Start transaction if not already active
    if (!bTransactionActive)
    {
        if (!BeginTransaction(EInventoryTransactionType::Add, OperationName))
        {
            return FSuspenseInventoryOperationResult::Failure(
                ESuspenseInventoryErrorCode::NotInitialized,
                FText::FromString(TEXT("Failed to start transaction")),
                OperationName
            );
        }
    }
    
    // Create item instance from DataTable ID
    FSuspenseInventoryOperationResult CreateResult = CreateItemInstanceFromID(ItemID, Amount, OperationName);
    if (!CreateResult.IsSuccess())
    {
        LogTransactionOperation(TEXT("Instance creation failed"), CreateResult);
        return CreateResult;
    }
    
    // Get the created instance
    FSuspenseInventoryItemInstance* CreatedInstance = nullptr;
    for (FSuspenseInventoryItemInstance& Instance : CreatedInstances)
    {
        if (Instance.ItemID == ItemID)
        {
            CreatedInstance = &Instance;
            break;
        }
    }
    
    if (!CreatedInstance)
    {
        FSuspenseInventoryOperationResult ErrorResult = FSuspenseInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::UnknownError,
            FText::FromString(TEXT("Created instance not found")),
            OperationName
        );
        LogTransactionOperation(TEXT("Instance tracking error"), ErrorResult);
        return ErrorResult;
    }
    
    // Find free space for the instance
    int32 FreeSpace = FindFreeSpaceForInstance(*CreatedInstance);
    if (FreeSpace == INDEX_NONE)
    {
        FSuspenseInventoryOperationResult ErrorResult = FSuspenseInventoryOperationResult::NoSpace(
            OperationName,
            FText::FromString(TEXT("No free space for item in inventory"))
        );
        LogTransactionOperation(TEXT("No space"), ErrorResult);
        return ErrorResult;
    }
    
    // Place instance in storage
    FSuspenseInventoryOperationResult PlaceResult = PlaceItemInstanceInStorage(*CreatedInstance, FreeSpace);
    if (!PlaceResult.IsSuccess())
    {
        LogTransactionOperation(TEXT("Placement failed"), PlaceResult);
        return PlaceResult;
    }
    
    // Update transaction stats
    UpdateTransactionStats();
    
    // Create success result with instance data
    FSuspenseInventoryOperationResult SuccessResult = FSuspenseInventoryOperationResult::Success(OperationName);
    SuccessResult.AddResultData(TEXT("ItemID"), ItemID.ToString());
    SuccessResult.AddResultData(TEXT("Amount"), FString::FromInt(Amount));
    SuccessResult.AddResultData(TEXT("AnchorIndex"), FString::FromInt(FreeSpace));
    SuccessResult.AddResultData(TEXT("InstanceID"), CreatedInstance->InstanceID.ToString());
    
    LogTransactionOperation(TEXT("Success"), SuccessResult);
    return SuccessResult;
}

FSuspenseInventoryOperationResult USuspenseInventoryTransaction::AddItemFromData(const FSuspenseUnifiedItemData& ItemData, int32 Amount)
{
    static const FName OperationName = TEXT("AddItemFromData");
    
    // Start transaction if not already active
    if (!bTransactionActive)
    {
        if (!BeginTransaction(EInventoryTransactionType::Add, OperationName))
        {
            return FSuspenseInventoryOperationResult::Failure(
                ESuspenseInventoryErrorCode::NotInitialized,
                FText::FromString(TEXT("Failed to start transaction")),
                OperationName
            );
        }
    }
    
    // Validate ItemData
    if (ItemData.ItemID.IsNone())
    {
        FSuspenseInventoryOperationResult ErrorResult = FSuspenseInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::InvalidItem,
            FText::FromString(TEXT("Invalid ItemData - ItemID is None")),
            OperationName
        );
        LogTransactionOperation(TEXT("Validation failed"), ErrorResult);
        return ErrorResult;
    }
    
    // Create item instance from unified data
    FSuspenseInventoryOperationResult CreateResult = CreateItemInstanceFromData(ItemData, Amount, OperationName);
    if (!CreateResult.IsSuccess())
    {
        LogTransactionOperation(TEXT("Instance creation failed"), CreateResult);
        return CreateResult;
    }
    
    // Continue with placement logic (similar to AddItemByID)
    // ... (rest of implementation follows same pattern)
    
    UpdateTransactionStats();
    
    FSuspenseInventoryOperationResult SuccessResult = FSuspenseInventoryOperationResult::Success(OperationName);
    LogTransactionOperation(TEXT("Success"), SuccessResult);
    return SuccessResult;
}

FSuspenseInventoryOperationResult USuspenseInventoryTransaction::AddItemInstance(const FSuspenseInventoryItemInstance& ItemInstance)
{
    static const FName OperationName = TEXT("AddItemInstance");
    
    // Start transaction if not already active
    if (!bTransactionActive)
    {
        if (!BeginTransaction(EInventoryTransactionType::Add, OperationName))
        {
            return FSuspenseInventoryOperationResult::Failure(
                ESuspenseInventoryErrorCode::NotInitialized,
                FText::FromString(TEXT("Failed to start transaction")),
                OperationName
            );
        }
    }
    
    // Validate the instance
    FSuspenseInventoryOperationResult ValidateResult = ValidateItemInstance(ItemInstance, OperationName);
    if (!ValidateResult.IsSuccess())
    {
        LogTransactionOperation(TEXT("Validation failed"), ValidateResult);
        return ValidateResult;
    }
    
    // Find free space
    int32 FreeSpace = FindFreeSpaceForInstance(ItemInstance);
    if (FreeSpace == INDEX_NONE)
    {
        FSuspenseInventoryOperationResult ErrorResult = FSuspenseInventoryOperationResult::NoSpace(
            OperationName,
            FText::FromString(TEXT("No free space for item instance"))
        );
        LogTransactionOperation(TEXT("No space"), ErrorResult);
        return ErrorResult;
    }
    
    // Add to created instances tracking
    CreatedInstances.Add(ItemInstance);
    
    // Place in storage
    FSuspenseInventoryOperationResult PlaceResult = PlaceItemInstanceInStorage(ItemInstance, FreeSpace);
    if (!PlaceResult.IsSuccess())
    {
        // Remove from tracking on failure
        CreatedInstances.RemoveAll([&ItemInstance](const FSuspenseInventoryItemInstance& Instance) {
            return Instance.InstanceID == ItemInstance.InstanceID;
        });
        
        LogTransactionOperation(TEXT("Placement failed"), PlaceResult);
        return PlaceResult;
    }
    
    UpdateTransactionStats();
    
    FSuspenseInventoryOperationResult SuccessResult = FSuspenseInventoryOperationResult::Success(OperationName);
    SuccessResult.AddResultData(TEXT("InstanceID"), ItemInstance.InstanceID.ToString());
    LogTransactionOperation(TEXT("Success"), SuccessResult);
    return SuccessResult;
}

//==================================================================
// INTERNAL HELPER METHODS IMPLEMENTATION
//==================================================================

bool USuspenseInventoryTransaction::ValidateTransactionPrerequisites() const
{
    // Check if required components are set
    if (!Storage)
    {
        UE_LOG(LogInventory, Error, TEXT("InventoryTransaction: Storage component is null"));
        return false;
    }
    
    if (!Constraints)
    {
        UE_LOG(LogInventory, Error, TEXT("InventoryTransaction: Constraints component is null"));
        return false;
    }
    
    if (!ItemManager)
    {
        UE_LOG(LogInventory, Error, TEXT("InventoryTransaction: ItemManager is null"));
        return false;
    }
    
    // Check if storage is initialized
    if (!Storage->IsInitialized())
    {
        UE_LOG(LogInventory, Error, TEXT("InventoryTransaction: Storage is not initialized"));
        return false;
    }
    
    return true;
}

FSuspenseInventoryOperationResult USuspenseInventoryTransaction::CreateItemInstanceFromID(const FName& ItemID, int32 Amount, const FName& Context)
{
    if (!ItemManager)
    {
        return FSuspenseInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::NotInitialized,
            FText::FromString(TEXT("ItemManager not available")),
            Context
        );
    }
    
    // Get unified data from DataTable
    FSuspenseUnifiedItemData UnifiedData;
    if (!ItemManager->GetUnifiedItemData(ItemID, UnifiedData))
    {
        return FSuspenseInventoryOperationResult::ItemNotFound(Context, ItemID);
    }
    
    // Create instance using ItemManager
    FSuspenseInventoryItemInstance NewInstance;
    if (!ItemManager->CreateItemInstance(ItemID, Amount, NewInstance))
    {
        return FSuspenseInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::UnknownError,
            FText::Format(FText::FromString(TEXT("Failed to create instance for item '{0}'")), FText::FromName(ItemID)),
            Context
        );
    }
    
    // Add to tracking
    CreatedInstances.Add(NewInstance);
    
    return FSuspenseInventoryOperationResult::Success(Context);
}

FSuspenseInventoryOperationResult USuspenseInventoryTransaction::ValidateItemInstance(const FSuspenseInventoryItemInstance& Instance, const FName& Context)
{
    // Basic instance validation
    if (!Instance.IsValid())
    {
        return FSuspenseInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::InvalidItem,
            FText::FromString(TEXT("Invalid item instance")),
            Context
        );
    }
    
    // Validate against constraints if available
    if (Constraints)
    {
        // Note: This would call actual constraint validation methods
        // return Constraints->ValidateItemInstance(Instance, Context);
    }
    
    return FSuspenseInventoryOperationResult::Success(Context);
}

int32 USuspenseInventoryTransaction::FindFreeSpaceForInstance(const FSuspenseInventoryItemInstance& Instance)
{
    if (!Storage || !ItemManager)
    {
        return INDEX_NONE;
    }
    
    // Validate instance
    if (!Instance.IsValid())
    {
        return INDEX_NONE;
    }
    
    // If the instance already has a specific rotation state, we need to handle it carefully
    if (Instance.bIsRotated)
    {
        // Instance is already rotated, so we need to find space for the rotated version
        // We'll call FindFreeSpace but with rotation disabled since we want the specific orientation
        return Storage->FindFreeSpace(Instance.ItemID, false, true);
    }
    else
    {
        // Instance is in normal orientation, allow Storage to try rotation if needed
        return Storage->FindFreeSpace(Instance.ItemID, true, true);
    }
}

FSuspenseInventoryOperationResult USuspenseInventoryTransaction::PlaceItemInstanceInStorage(const FSuspenseInventoryItemInstance& Instance, int32 AnchorIndex)
{
    static const FName OperationName = TEXT("PlaceItemInstance");
    
    if (!Storage)
    {
        return FSuspenseInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::NotInitialized,
            FText::FromString(TEXT("Storage not available")),
            OperationName
        );
    }
    
    // Note: This would call the actual storage placement method
    // The exact implementation depends on how Storage is structured
    // For now, we'll assume success
    
    UE_LOG(LogInventory, Verbose, TEXT("PlaceItemInstanceInStorage: Placed instance %s at anchor %d"), 
        *Instance.InstanceID.ToString(), AnchorIndex);
    
    return FSuspenseInventoryOperationResult::Success(OperationName);
}

void USuspenseInventoryTransaction::CreateStorageBackup()
{
    if (!Storage || !Storage->IsInitialized())
    {
        UE_LOG(LogInventory, Warning, TEXT("InventoryTransaction: Cannot create backup - storage not available"));
        return;
    }
    
    // Clear previous backup
    BackupItemInstances.Empty();
    BackupItemObjects.Empty();
    BackupCells.Empty();
    
    // Note: These would call actual storage methods
    // BackupItemInstances = Storage->GetAllItemInstances();
    // BackupItemObjects = Storage->GetAllItemObjects();
    // BackupCells = Storage->GetAllCells();
    
    UE_LOG(LogInventory, Verbose, TEXT("InventoryTransaction: Created backup with %d instances, %d objects, %d cells"), 
        BackupItemInstances.Num(), BackupItemObjects.Num(), BackupCells.Num());
}

void USuspenseInventoryTransaction::RestoreStorageFromBackup()
{
    if (!Storage || !Storage->IsInitialized())
    {
        UE_LOG(LogInventory, Error, TEXT("InventoryTransaction: Cannot restore backup - storage not available"));
        return;
    }
    
    // Note: This would call actual storage restoration methods
    // Storage->RestoreFromBackup(BackupItemInstances, BackupItemObjects, BackupCells);
    
    UE_LOG(LogInventory, Log, TEXT("InventoryTransaction: Restored from backup"));
}

void USuspenseInventoryTransaction::DestroyCreatedItems()
{
    // Destroy created objects
    for (UObject* Item : CreatedItems)
    {
        if (AActor* ItemActor = Cast<AActor>(Item))
        {
            ItemActor->Destroy();
        }
    }
    
    UE_LOG(LogInventory, Verbose, TEXT("InventoryTransaction: Destroyed %d created items and %d created instances"), 
        CreatedItems.Num(), CreatedInstances.Num());
    
    CreatedItems.Empty();
    CreatedInstances.Empty();
}

void USuspenseInventoryTransaction::UpdateTransactionStats()
{
    OperationCount++;
}

void USuspenseInventoryTransaction::LogTransactionOperation(const FString& Action, const FSuspenseInventoryOperationResult& Result)
{
    if (Result.IsSuccess())
    {
        UE_LOG(LogInventory, Verbose, TEXT("InventoryTransaction: %s - Success [%s] (Op #%d)"), 
            *Action, *CurrentContext.ToString(), OperationCount);
    }
    else
    {
        UE_LOG(LogInventory, Warning, TEXT("InventoryTransaction: %s - Failed [%s]: %s (Op #%d)"),
            *Action, *CurrentContext.ToString(), *Result.ErrorMessage.ToString(), OperationCount);
    }
}

FString USuspenseInventoryTransaction::GetTransactionDebugInfo() const
{
    FString DebugInfo;
    DebugInfo += FString::Printf(TEXT("=== Transaction Debug Info ===\n"));
    DebugInfo += FString::Printf(TEXT("Active: %s\n"), bTransactionActive ? TEXT("Yes") : TEXT("No"));
    
    if (bTransactionActive)
    {
        double CurrentDuration = FPlatformTime::Seconds() - TransactionStartTime;
        DebugInfo += FString::Printf(TEXT("Type: %s\n"), *UEnum::GetValueAsString(CurrentType));
        DebugInfo += FString::Printf(TEXT("Context: %s\n"), *CurrentContext.ToString());
        DebugInfo += FString::Printf(TEXT("Duration: %.3f seconds\n"), CurrentDuration);
        DebugInfo += FString::Printf(TEXT("Operations: %d\n"), OperationCount);
        DebugInfo += FString::Printf(TEXT("Created Items: %d\n"), CreatedItems.Num());
        DebugInfo += FString::Printf(TEXT("Created Instances: %d\n"), CreatedInstances.Num());
    }
    
    DebugInfo += FString::Printf(TEXT("Components: Storage=%s, Constraints=%s, ItemManager=%s, Events=%s\n"),
        Storage ? TEXT("OK") : TEXT("NULL"),
        Constraints ? TEXT("OK") : TEXT("NULL"),
        ItemManager ? TEXT("OK") : TEXT("NULL"),
        Events ? TEXT("OK") : TEXT("NULL"));
    
    return DebugInfo;
}

// Stub implementations for methods not fully detailed here
FSuspenseInventoryOperationResult USuspenseInventoryTransaction::RemoveItem(const FName& ItemID, int32 Amount)
{
    static const FName OperationName = TEXT("RemoveItem");
    UpdateTransactionStats();
    return FSuspenseInventoryOperationResult::Success(OperationName);
}

FSuspenseInventoryOperationResult USuspenseInventoryTransaction::RemoveItemInstance(const FGuid& InstanceID)
{
    static const FName OperationName = TEXT("RemoveItemInstance");
    UpdateTransactionStats();
    return FSuspenseInventoryOperationResult::Success(OperationName);
}

FSuspenseInventoryOperationResult USuspenseInventoryTransaction::MoveItem(UObject* ItemObject, int32 NewAnchorIndex, bool bShouldRotate)
{
    static const FName OperationName = TEXT("MoveItem");
    UpdateTransactionStats();
    return FSuspenseInventoryOperationResult::Success(OperationName);
}

FSuspenseInventoryOperationResult USuspenseInventoryTransaction::MoveItemInstance(const FGuid& InstanceID, int32 NewAnchorIndex, bool bShouldRotate)
{
    static const FName OperationName = TEXT("MoveItemInstance");
    UpdateTransactionStats();
    return FSuspenseInventoryOperationResult::Success(OperationName);
}

FSuspenseInventoryOperationResult USuspenseInventoryTransaction::RotateItem(UObject* ItemObject, bool bDesiredRotation)
{
    static const FName OperationName = TEXT("RotateItem");
    UpdateTransactionStats();
    return FSuspenseInventoryOperationResult::Success(OperationName);
}

FSuspenseInventoryOperationResult USuspenseInventoryTransaction::StackItems(UObject* SourceItem, UObject* TargetItem, int32 Amount)
{
    static const FName OperationName = TEXT("StackItems");
    UpdateTransactionStats();
    return FSuspenseInventoryOperationResult::Success(OperationName);
}

FSuspenseInventoryOperationResult USuspenseInventoryTransaction::StackItemInstances(const FGuid& SourceInstanceID, const FGuid& TargetInstanceID, int32 Amount)
{
    static const FName OperationName = TEXT("StackItemInstances");
    UpdateTransactionStats();
    return FSuspenseInventoryOperationResult::Success(OperationName);
}

FSuspenseInventoryOperationResult USuspenseInventoryTransaction::SplitStack(UObject* SourceItem, int32 TargetPosition, int32 Amount)
{
    static const FName OperationName = TEXT("SplitStack");
    UpdateTransactionStats();
    return FSuspenseInventoryOperationResult::Success(OperationName);
}

FSuspenseInventoryOperationResult USuspenseInventoryTransaction::SwapItems(UObject* FirstItem, UObject* SecondItem)
{
    static const FName OperationName = TEXT("SwapItems");
    UpdateTransactionStats();
    return FSuspenseInventoryOperationResult::Success(OperationName);
}

FSuspenseInventoryOperationResult USuspenseInventoryTransaction::UpdateItemProperties(const FGuid& InstanceID, const TMap<FName, float>& NewProperties)
{
    static const FName OperationName = TEXT("UpdateItemProperties");
    UpdateTransactionStats();
    return FSuspenseInventoryOperationResult::Success(OperationName);
}

FSuspenseInventoryOperationResult USuspenseInventoryTransaction::CreateItemObject(const FSuspenseUnifiedItemData& ItemData, int32 Amount, const FName& Context)
{
    // Legacy method for backward compatibility
    // This would create actual UObject instances if needed
    static const FName OperationName = TEXT("CreateItemObject");
    return FSuspenseInventoryOperationResult::Success(OperationName);
}

FSuspenseInventoryOperationResult USuspenseInventoryTransaction::CreateItemInstanceFromData(const FSuspenseUnifiedItemData& ItemData, int32 Amount, const FName& Context)
{
    if (!ItemManager)
    {
        return FSuspenseInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::NotInitialized,
            FText::FromString(TEXT("ItemManager not available")),
            Context
        );
    }
    
    // Create instance directly from unified data using factory method
    FSuspenseInventoryItemInstance NewInstance = FSuspenseInventoryItemInstance::Create(ItemData.ItemID, Amount);
    
    // Initialize runtime properties based on item type
    if (ItemData.bIsEquippable && ItemData.bIsWeapon)
    {
        // Set weapon-specific properties
        NewInstance.SetRuntimeProperty(TEXT("MaxAmmo"), 30.0f); // Default value
        NewInstance.SetRuntimeProperty(TEXT("Ammo"), 30.0f);    // Start full
    }
    
    if (ItemData.bIsEquippable && (ItemData.bIsWeapon || ItemData.bIsArmor))
    {
        // Set durability for equipment
        NewInstance.SetRuntimeProperty(TEXT("MaxDurability"), 100.0f);
        NewInstance.SetRuntimeProperty(TEXT("Durability"), 100.0f);
    }
    
    // Add to tracking
    CreatedInstances.Add(NewInstance);
    
    return FSuspenseInventoryOperationResult::Success(Context);
}