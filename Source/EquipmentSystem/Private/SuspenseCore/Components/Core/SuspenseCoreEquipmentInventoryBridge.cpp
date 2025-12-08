// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Components/Core/SuspenseCoreEquipmentInventoryBridge.h"
#include "SuspenseCore/Components/Core/SuspenseCoreEquipmentOperationExecutor.h"
#include "Core/Services/SuspenseCoreEquipmentServiceLocator.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentDataProvider.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentOperations.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreTransactionManager.h"
#include "SuspenseCore/Interfaces/Inventory/ISuspenseCoreInventory.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreInventoryBridge.h"
#include "ItemSystem/SuspenseCoreItemManager.h"
#include "Types/Loadout/SuspenseCoreItemDataTable.h"
#include "Types/Loadout/SuspenseCoreLoadoutManager.h"

// === Includes for EventBus and character resolution ===
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogEquipmentBridge, Log, All);

// ===== Constructor / Lifecycle =====

USuspenseCoreEquipmentInventoryBridge::USuspenseCoreEquipmentInventoryBridge()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void USuspenseCoreEquipmentInventoryBridge::BeginPlay()
{
    Super::BeginPlay();
}

void USuspenseCoreEquipmentInventoryBridge::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (EventDelegateManager.IsValid() && EquipmentOperationRequestHandle.IsValid())
    {
        EventDelegateManager->UniversalUnsubscribe(EquipmentOperationRequestHandle);
        EquipmentOperationRequestHandle.Reset();

        UE_LOG(LogEquipmentBridge, Log, TEXT("Unsubscribed from EventDelegateManager"));
    }

    {
        FScopeLock Lock(&TransactionLock);

        TArray<FGuid> TransactionIDs;
        ActiveTransactions.GetKeys(TransactionIDs);
        for (const FGuid& ID : TransactionIDs)
        {
            RollbackBridgeTransaction(ID);
        }

        ActiveTransactions.Reset();
    }

    ActiveReservations.Reset();
    Super::EndPlay(EndPlayReason);
}

// ===== Initialization =====

bool USuspenseCoreEquipmentInventoryBridge::Initialize(
    TScriptInterface<ISuspenseCoreEquipmentDataProvider> InEquipmentData,
    TScriptInterface<ISuspenseCoreEquipmentOperations> InEquipmentOps,
    TScriptInterface<ISuspenseCoreTransactionManager> InTransactionMgr)
{
    UE_LOG(LogEquipmentBridge, Warning, TEXT("=== Initialize CALLED ==="));

    // Prevent double initialization
    if (bIsInitialized)
    {
        UE_LOG(LogEquipmentBridge, Warning,
            TEXT("Already initialized - skipping re-initialization"));

        const bool bDependenciesValid =
            EquipmentDataProvider.GetInterface() &&
            EquipmentOperations.GetInterface() &&
            TransactionManager.GetInterface() &&
            EventDelegateManager.IsValid() &&
            EquipmentOperationRequestHandle.IsValid();

        if (bDependenciesValid)
        {
            UE_LOG(LogEquipmentBridge, Log, TEXT("All existing dependencies are still valid"));
            return true;
        }
        else
        {
            UE_LOG(LogEquipmentBridge, Warning,
                TEXT("Some dependencies became invalid - forcing re-initialization"));
            bIsInitialized = false;
        }
    }

    UE_LOG(LogEquipmentBridge, Warning, TEXT("Initializing bridge with dependencies..."));

    // Store and validate base dependencies
    EquipmentDataProvider = InEquipmentData;
    EquipmentOperations = InEquipmentOps;
    TransactionManager = InTransactionMgr;

    const bool bBaseOk =
        EquipmentDataProvider.GetInterface() &&
        EquipmentOperations.GetInterface() &&
        TransactionManager.GetInterface();

    if (!bBaseOk)
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("Initialize() missing dependencies"));
        return false;
    }

    UE_LOG(LogEquipmentBridge, Log, TEXT("Base dependencies OK"));

    // Try to acquire OperationService via service locator (optional)
    if (USuspenseCoreEquipmentServiceLocator* Locator = USuspenseCoreEquipmentServiceLocator::Get(GetWorld()))
    {
        const FGameplayTag ServiceTag = FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Operation"));
        if (UObject* SvcObj = Locator->GetService(ServiceTag))
        {
            if (SvcObj->GetClass()->ImplementsInterface(UEquipmentOperationService::StaticClass()))
            {
                EquipmentService.SetObject(SvcObj);
                EquipmentService.SetInterface(Cast<IEquipmentOperationService>(SvcObj));
                UE_LOG(LogEquipmentBridge, Log, TEXT("OperationService acquired via locator"));
            }
        }
    }

    // Get EventDelegateManager with validation
    UE_LOG(LogEquipmentBridge, Warning, TEXT("Attempting to get EventDelegateManager..."));

    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("FAILED - No World!"));
        return false;
    }

    UGameInstance* GameInstance = World->GetGameInstance();
    if (!GameInstance)
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("FAILED - No GameInstance!"));
        return false;
    }

    EventDelegateManager = GameInstance->GetSubsystem<USuspenseCoreEventManager>();

    if (!EventDelegateManager.IsValid())
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("FAILED - EventDelegateManager not available!"));
        return false;
    }

    if (!EventDelegateManager->IsSystemReady())
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("FAILED - EventDelegateManager not initialized!"));
        return false;
    }

    // Unsubscribe from previous subscription if exists
    if (EquipmentOperationRequestHandle.IsValid())
    {
        UE_LOG(LogEquipmentBridge, Warning,
            TEXT("Found existing subscription handle - unsubscribing first"));

        EventDelegateManager->UniversalUnsubscribe(EquipmentOperationRequestHandle);
        EquipmentOperationRequestHandle.Reset();
    }

    // Subscribe to equipment operation requests from UI
    EquipmentOperationRequestHandle = EventDelegateManager->SubscribeToEquipmentOperationRequest(
        [this](const FEquipmentOperationRequest& Request)
        {
            UE_LOG(LogEquipmentBridge, Log, TEXT("Lambda called - Operation: %s"),
                *StaticEnum<EEquipmentOperationType>()->GetValueAsString(Request.OperationType));

            HandleEquipmentOperationRequest(Request);
        }
    );

    if (!EquipmentOperationRequestHandle.IsValid())
    {
        UE_LOG(LogEquipmentBridge, Error,
            TEXT("CRITICAL FAILURE - Subscription returned invalid handle!"));
        return false;
    }

    UE_LOG(LogEquipmentBridge, Warning, TEXT("✓✓✓ SUBSCRIPTION SUCCESSFUL ✓✓✓"));

    // Mark as initialized to prevent future re-initialization
    bIsInitialized = true;
    ProcessedOperationIds.Reset();

    UE_LOG(LogEquipmentBridge, Warning, TEXT("=== Initialize END - SUCCESS ==="));

    return true;
}

void USuspenseCoreEquipmentInventoryBridge::SetInventoryInterface(TScriptInterface<ISuspenseCoreInventory> InInventoryInterface)
{
    InventoryInterface = InInventoryInterface;
}

// ===== EventDelegateManager Integration =====

void USuspenseCoreEquipmentInventoryBridge::HandleEquipmentOperationRequest(
    const FEquipmentOperationRequest& Request)
{
    UE_LOG(LogEquipmentBridge, Warning, TEXT("=== Processing Equipment Operation Request ==="));
    UE_LOG(LogEquipmentBridge, Warning, TEXT("Operation: %s"),
        *StaticEnum<EEquipmentOperationType>()->GetValueAsString(Request.OperationType));
    UE_LOG(LogEquipmentBridge, Warning, TEXT("Item: %s (Instance: %s)"),
        *Request.ItemInstance.ItemID.ToString(),
        *Request.ItemInstance.InstanceID.ToString());
    UE_LOG(LogEquipmentBridge, Warning, TEXT("Target Slot: %d"), Request.TargetSlotIndex);
    UE_LOG(LogEquipmentBridge, Warning, TEXT("Operation ID: %s"), *Request.OperationId.ToString());

    // Check for duplicate operation processing
    if (Request.OperationId.IsValid())
    {
        bool bAlreadyProcessed = false;
        {
            FScopeLock Lock(&OperationCacheLock);

            if (ProcessedOperationIds.Contains(Request.OperationId))
            {
                bAlreadyProcessed = true;
            }
            else
            {
                ProcessedOperationIds.Add(Request.OperationId);

                if (ProcessedOperationIds.Num() > 200)
                {
                    TArray<FGuid> AllOps = ProcessedOperationIds.Array();
                    for (int32 i = 0; i < 100; i++)
                    {
                        ProcessedOperationIds.Remove(AllOps[i]);
                    }
                }
            }
        }

        if (bAlreadyProcessed)
        {
            UE_LOG(LogEquipmentBridge, Warning,
                TEXT("!!! DUPLICATE OPERATION DETECTED - IGNORING !!!"));
            return;
        }
    }

    // Validate inventory interface is set
    if (!InventoryInterface.GetInterface())
    {
        UE_LOG(LogEquipmentBridge, Error,
            TEXT("InventoryInterface not set - cannot process equipment operations"));

        if (EventDelegateManager.IsValid())
        {
            FEquipmentOperationResult FailureResult;
            FailureResult.bSuccess = false;
            FailureResult.OperationId = Request.OperationId;
            FailureResult.ErrorMessage = FText::FromString(TEXT("Inventory interface not initialized"));
            FailureResult.FailureType = EEquipmentValidationFailure::SystemError;

            EventDelegateManager->BroadcastEquipmentOperationCompleted(FailureResult);
        }
        return;
    }

    // Process operation based on type
    FSuspenseCoreInventoryOperationResult InventoryResult;
    FEquipmentOperationResult EquipmentResult;

    switch (Request.OperationType)
    {
        case EEquipmentOperationType::Equip:
        {
            UE_LOG(LogEquipmentBridge, Log, TEXT("Processing EQUIP operation"));

            FInventoryTransferRequest TransferReq;
            TransferReq.Item = Request.ItemInstance;
            TransferReq.TargetSlot = Request.TargetSlotIndex;
            TransferReq.SourceSlot = INDEX_NONE;

            InventoryResult = ExecuteTransfer_FromInventoryToEquip(TransferReq);

            EquipmentResult.bSuccess = InventoryResult.bSuccess;
            EquipmentResult.OperationId = Request.OperationId;
            EquipmentResult.ErrorMessage = InventoryResult.ErrorMessage;
            EquipmentResult.AffectedSlots.Add(Request.TargetSlotIndex);

            if (InventoryResult.bSuccess)
            {
                EquipmentResult.AffectedItems = InventoryResult.AffectedItems;
                EquipmentResult.ResultMetadata.Add(TEXT("OperationType"), TEXT("Equip"));

                UE_LOG(LogEquipmentBridge, Log, TEXT("EQUIP operation completed successfully"));
            }
            else
            {
                EquipmentResult.FailureType = EEquipmentValidationFailure::SystemError;
                UE_LOG(LogEquipmentBridge, Error, TEXT("EQUIP operation failed: %s"),
                    *InventoryResult.ErrorMessage.ToString());
            }

            break;
        }

        case EEquipmentOperationType::Unequip:
        {
            UE_LOG(LogEquipmentBridge, Warning, TEXT("Processing UNEQUIP operation"));
            UE_LOG(LogEquipmentBridge, Warning, TEXT("Source Slot: %d"), Request.SourceSlotIndex);

            // Validate that we have equipment data provider
            if (!EquipmentDataProvider.GetInterface())
            {
                UE_LOG(LogEquipmentBridge, Error, TEXT("EquipmentDataProvider not available"));

                EquipmentResult.bSuccess = false;
                EquipmentResult.OperationId = Request.OperationId;
                EquipmentResult.ErrorMessage = FText::FromString(TEXT("Equipment system not initialized"));
                EquipmentResult.FailureType = EEquipmentValidationFailure::SystemError;
                break;
            }

            // Validate source slot index
            if (!EquipmentDataProvider->IsValidSlotIndex(Request.SourceSlotIndex))
            {
                UE_LOG(LogEquipmentBridge, Error, TEXT("Invalid source slot: %d"), Request.SourceSlotIndex);

                EquipmentResult.bSuccess = false;
                EquipmentResult.OperationId = Request.OperationId;
                EquipmentResult.ErrorMessage = FText::Format(
                    FText::FromString(TEXT("Invalid equipment slot index: {0}")),
                    FText::AsNumber(Request.SourceSlotIndex)
                );
                EquipmentResult.FailureType = EEquipmentValidationFailure::InvalidSlot;
                break;
            }

            // Check if slot is actually occupied
            if (!EquipmentDataProvider->IsSlotOccupied(Request.SourceSlotIndex))
            {
                UE_LOG(LogEquipmentBridge, Warning, TEXT("Slot %d is already empty"), Request.SourceSlotIndex);

                // This is not an error, just a no-op
                EquipmentResult.bSuccess = true;
                EquipmentResult.OperationId = Request.OperationId;
                EquipmentResult.AffectedSlots.Add(Request.SourceSlotIndex);
                break;
            }

            // Get item from slot before we start the transfer
            FSuspenseCoreInventoryItemInstance UnequippedItem = EquipmentDataProvider->GetSlotItem(Request.SourceSlotIndex);

            UE_LOG(LogEquipmentBridge, Warning, TEXT("Unequipping item: %s from slot %d"),
                *UnequippedItem.ItemID.ToString(), Request.SourceSlotIndex);

            // Create transfer request for the unequip operation
            FInventoryTransferRequest TransferReq;
            TransferReq.SourceSlot = Request.SourceSlotIndex;
            TransferReq.TargetSlot = INDEX_NONE; // Let inventory find best slot
            TransferReq.Item = UnequippedItem;

            // Execute the transfer from equipment to inventory
            InventoryResult = ExecuteTransfer_FromEquipToInventory(TransferReq);

            // Map inventory result to equipment result
            EquipmentResult.bSuccess = InventoryResult.bSuccess;
            EquipmentResult.OperationId = Request.OperationId;
            EquipmentResult.ErrorMessage = InventoryResult.ErrorMessage;
            EquipmentResult.AffectedSlots.Add(Request.SourceSlotIndex);

            if (InventoryResult.bSuccess)
            {
                EquipmentResult.AffectedItems = InventoryResult.AffectedItems;
                EquipmentResult.ResultMetadata.Add(TEXT("OperationType"), TEXT("Unequip"));

                UE_LOG(LogEquipmentBridge, Warning, TEXT("UNEQUIP operation completed successfully"));
            }
            else
            {
                EquipmentResult.FailureType = EEquipmentValidationFailure::SystemError;
                UE_LOG(LogEquipmentBridge, Error, TEXT("UNEQUIP operation failed: %s"),
                    *InventoryResult.ErrorMessage.ToString());
            }

            break;
        }

        case EEquipmentOperationType::Swap:
        {
            UE_LOG(LogEquipmentBridge, Log, TEXT("Processing SWAP operation"));

            InventoryResult = ExecuteSwap_InventoryToEquipment(
                Request.ItemInstance.InstanceID,
                Request.TargetSlotIndex
            );

            EquipmentResult.bSuccess = InventoryResult.bSuccess;
            EquipmentResult.OperationId = Request.OperationId;
            EquipmentResult.ErrorMessage = InventoryResult.ErrorMessage;
            EquipmentResult.AffectedSlots.Add(Request.TargetSlotIndex);

            if (InventoryResult.bSuccess)
            {
                EquipmentResult.AffectedItems = InventoryResult.AffectedItems;
                EquipmentResult.ResultMetadata.Add(TEXT("OperationType"), TEXT("Swap"));
            }
            else
            {
                EquipmentResult.FailureType = EEquipmentValidationFailure::SystemError;
            }

            break;
        }

        case EEquipmentOperationType::Drop:
        {
            UE_LOG(LogEquipmentBridge, Log, TEXT("Processing DROP operation for slot %d"),
                Request.TargetSlotIndex);

            if (!EquipmentDataProvider.GetInterface())
            {
                UE_LOG(LogEquipmentBridge, Error, TEXT("Drop failed: EquipmentDataProvider not available"));

                EquipmentResult.bSuccess = false;
                EquipmentResult.OperationId = Request.OperationId;
                EquipmentResult.ErrorMessage = FText::FromString(TEXT("Equipment system not initialized"));
                EquipmentResult.FailureType = EEquipmentValidationFailure::SystemError;
                break;
            }

            if (!EquipmentDataProvider->IsValidSlotIndex(Request.TargetSlotIndex))
            {
                EquipmentResult.bSuccess = false;
                EquipmentResult.OperationId = Request.OperationId;
                EquipmentResult.ErrorMessage = FText::Format(
                    FText::FromString(TEXT("Invalid equipment slot index: {0}")),
                    FText::AsNumber(Request.TargetSlotIndex)
                );
                EquipmentResult.FailureType = EEquipmentValidationFailure::InvalidSlot;
                break;
            }

            if (!EquipmentDataProvider->IsSlotOccupied(Request.TargetSlotIndex))
            {
                EquipmentResult.bSuccess = false;
                EquipmentResult.OperationId = Request.OperationId;
                EquipmentResult.ErrorMessage = FText::Format(
                    FText::FromString(TEXT("Equipment slot {0} is already empty")),
                    FText::AsNumber(Request.TargetSlotIndex)
                );
                EquipmentResult.FailureType = EEquipmentValidationFailure::InvalidSlot;
                break;
            }

            const FSuspenseCoreInventoryItemInstance DroppedItem = EquipmentDataProvider->ClearSlot(
                Request.TargetSlotIndex, true);

            if (DroppedItem.IsValid())
            {
                EquipmentResult.bSuccess = true;
                EquipmentResult.OperationId = Request.OperationId;
                EquipmentResult.AffectedSlots.Add(Request.TargetSlotIndex);
                EquipmentResult.AffectedItems.Add(DroppedItem);
                EquipmentResult.ResultMetadata.Add(TEXT("OperationType"), TEXT("Drop"));
            }
            else
            {
                EquipmentResult.bSuccess = false;
                EquipmentResult.OperationId = Request.OperationId;
                EquipmentResult.ErrorMessage = FText::FromString(TEXT("Failed to drop item - system error"));
                EquipmentResult.FailureType = EEquipmentValidationFailure::SystemError;
            }

            break;
        }

        default:
        {
            EquipmentResult.bSuccess = false;
            EquipmentResult.OperationId = Request.OperationId;
            EquipmentResult.ErrorMessage = FText::FromString(TEXT("Unsupported operation type"));
            EquipmentResult.FailureType = EEquipmentValidationFailure::SystemError;
            break;
        }
    }

    // Broadcast result back through EventDelegateManager
    if (EventDelegateManager.IsValid())
    {
        EventDelegateManager->BroadcastEquipmentOperationCompleted(EquipmentResult);

        UE_LOG(LogEquipmentBridge, Warning, TEXT("=== Operation Result Broadcasted ==="));
        UE_LOG(LogEquipmentBridge, Warning, TEXT("Success: %s"), EquipmentResult.bSuccess ? TEXT("YES") : TEXT("NO"));
    }
}

// ===== ExecuteTransfer_FromEquipToInventory - Complete Implementation =====

FSuspenseCoreInventoryOperationResult USuspenseCoreEquipmentInventoryBridge::ExecuteTransfer_FromEquipToInventory(
    const FInventoryTransferRequest& Request)
{
    UE_LOG(LogEquipmentBridge, Warning, TEXT("=== ExecuteTransfer_FromEquipToInventory START ==="));
    UE_LOG(LogEquipmentBridge, Warning, TEXT("Source Equipment Slot: %d"), Request.SourceSlot);

    // Validate that all required dependencies are available
    if (!EquipmentDataProvider.GetInterface() || !InventoryInterface.GetInterface())
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("Dependencies not available"));
        return FSuspenseCoreInventoryOperationResult::Failure(
            ESuspenseCoreInventoryErrorCode::NotInitialized,
            FText::FromString(TEXT("Bridge not initialized")),
            TEXT("TransferFromEquipToInventory"),
            nullptr
        );
    }

    // Validate that the source slot index is valid
    if (!EquipmentDataProvider->IsValidSlotIndex(Request.SourceSlot))
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("Invalid source slot: %d"), Request.SourceSlot);
        return FSuspenseCoreInventoryOperationResult::Failure(
            ESuspenseCoreInventoryErrorCode::InvalidSlot,
            FText::Format(FText::FromString(TEXT("Invalid equipment slot: {0}")),
                FText::AsNumber(Request.SourceSlot)),
            TEXT("TransferFromEquipToInventory"),
            nullptr
        );
    }

    // Get the equipped item from the slot before we start any operations
    FSuspenseCoreInventoryItemInstance EquippedItem = EquipmentDataProvider->GetSlotItem(Request.SourceSlot);

    // If the slot is already empty, this is not an error, just return success
    if (!EquippedItem.IsValid())
    {
        UE_LOG(LogEquipmentBridge, Warning, TEXT("Slot %d is already empty"), Request.SourceSlot);
        return FSuspenseCoreInventoryOperationResult::Success(TEXT("TransferFromEquipToInventory"), nullptr);
    }

    UE_LOG(LogEquipmentBridge, Warning, TEXT("Item to transfer: %s (InstanceID: %s)"),
        *EquippedItem.ItemID.ToString(),
        *EquippedItem.InstanceID.ToString());

    // Check if inventory has space for the item we are about to unequip
    if (!InventoryHasSpace(EquippedItem))
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("No space in inventory"));
        return FSuspenseCoreInventoryOperationResult::Failure(
            ESuspenseCoreInventoryErrorCode::NoSpace,
            FText::FromString(TEXT("No space in inventory for unequipped item")),
            TEXT("TransferFromEquipToInventory"),
            nullptr
        );
    }

    UE_LOG(LogEquipmentBridge, Log, TEXT("Inventory has space"));

    // Begin bridge transaction for atomic rollback capability
    const FGuid BridgeTxnId = BeginBridgeTransaction();
    FBridgeTransaction& BridgeTxn = ActiveTransactions.FindChecked(BridgeTxnId);

    // Store backup data for potential rollback
    BridgeTxn.EquipmentSlot = Request.SourceSlot;
    BridgeTxn.EquipmentBackup = EquippedItem;

    bool bOperationSuccess = false;

    // If we have transaction manager, use it for extra safety
    if (TransactionManager.GetInterface())
    {
        UE_LOG(LogEquipmentBridge, Warning, TEXT("Using TransactionManager for atomic unequip"));

        const FString TxnDesc = FString::Printf(TEXT("Unequip_%s_from_Slot_%d"),
            *EquippedItem.ItemID.ToString(), Request.SourceSlot);
        const FGuid EquipTxnId = TransactionManager->BeginTransaction(TxnDesc);

        if (EquipTxnId.IsValid())
        {
            // If transaction manager supports extended operations, register them
            if (TransactionManager->SupportsExtendedOps())
            {
                FTransactionOperation ClearOp;
                ClearOp.OperationId = FGuid::NewGuid();
                ClearOp.OperationType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Clear"));
                ClearOp.SlotIndex = Request.SourceSlot;
                ClearOp.ItemBefore = EquippedItem;
                ClearOp.ItemAfter = FSuspenseCoreInventoryItemInstance();
                ClearOp.bReversible = true;
                ClearOp.Timestamp = FPlatformTime::Seconds();
                ClearOp.Priority = ETransactionPriority::Normal;
                ClearOp.Metadata.Add(TEXT("Destination"), TEXT("Inventory"));
                ClearOp.Metadata.Add(TEXT("InstanceID"), EquippedItem.InstanceID.ToString());

                TransactionManager->RegisterOperation(EquipTxnId, ClearOp);
                TransactionManager->ApplyOperation(EquipTxnId, ClearOp);
            }

            // PHASE 1: Clear the equipment slot with notification enabled
            UE_LOG(LogEquipmentBridge, Warning, TEXT("Phase 1: Clearing equipment slot %d..."), Request.SourceSlot);
            BridgeTxn.bEquipmentModified = true;

            // This call will trigger notifications to UIConnector which will update UI
            FSuspenseCoreInventoryItemInstance ClearedItem = EquipmentDataProvider->ClearSlot(Request.SourceSlot, true);

            if (!ClearedItem.IsValid())
            {
                UE_LOG(LogEquipmentBridge, Error, TEXT("Failed to clear equipment slot!"));
                TransactionManager->RollbackTransaction(EquipTxnId);
                RollbackBridgeTransaction(BridgeTxnId);

                return FSuspenseCoreInventoryOperationResult::Failure(
                    ESuspenseCoreInventoryErrorCode::UnknownError,
                    FText::FromString(TEXT("Failed to clear equipment slot")),
                    TEXT("TransferFromEquipToInventory"),
                    nullptr
                );
            }

            UE_LOG(LogEquipmentBridge, Warning, TEXT("Equipment slot cleared successfully"));

            // PHASE 2: Add the item to inventory
            UE_LOG(LogEquipmentBridge, Warning, TEXT("Phase 2: Adding item to inventory..."));
            BridgeTxn.bInventoryModified = true;

            FSuspenseCoreInventoryOperationResult AddResult = InventoryInterface->AddItemInstance(EquippedItem);

            if (!AddResult.bSuccess)
            {
                UE_LOG(LogEquipmentBridge, Error, TEXT("Failed to add to inventory: %s"),
                    *AddResult.ErrorMessage.ToString());

                // Rollback: restore item to equipment slot
                EquipmentDataProvider->SetSlotItem(Request.SourceSlot, EquippedItem, true);
                TransactionManager->RollbackTransaction(EquipTxnId);
                RollbackBridgeTransaction(BridgeTxnId);

                return AddResult;
            }

            UE_LOG(LogEquipmentBridge, Warning, TEXT("Item added to inventory successfully"));

            // Validate the entire transaction before committing
            if (!TransactionManager->ValidateTransaction(EquipTxnId))
            {
                UE_LOG(LogEquipmentBridge, Error, TEXT("Transaction validation failed"));

                // Rollback both sides: remove from inventory and restore to equipment
                InventoryInterface->RemoveItemInstance(EquippedItem.InstanceID);
                EquipmentDataProvider->SetSlotItem(Request.SourceSlot, EquippedItem, true);
                TransactionManager->RollbackTransaction(EquipTxnId);
                RollbackBridgeTransaction(BridgeTxnId);

                return FSuspenseCoreInventoryOperationResult::Failure(
                    ESuspenseCoreInventoryErrorCode::UnknownError,
                    FText::FromString(TEXT("Transaction validation failed")),
                    TEXT("TransferFromEquipToInventory"),
                    nullptr
                );
            }

            // Commit the transaction to make changes permanent
            if (!TransactionManager->CommitTransaction(EquipTxnId))
            {
                UE_LOG(LogEquipmentBridge, Error, TEXT("Transaction commit failed"));
                RollbackBridgeTransaction(BridgeTxnId);

                return FSuspenseCoreInventoryOperationResult::Failure(
                    ESuspenseCoreInventoryErrorCode::UnknownError,
                    FText::FromString(TEXT("Failed to commit transaction")),
                    TEXT("TransferFromEquipToInventory"),
                    nullptr
                );
            }

            UE_LOG(LogEquipmentBridge, Warning, TEXT("Transaction committed successfully"));
            bOperationSuccess = true;
        }
        else
        {
            UE_LOG(LogEquipmentBridge, Error, TEXT("Failed to begin equipment transaction"));
            RollbackBridgeTransaction(BridgeTxnId);
            bOperationSuccess = false;
        }
    }
    else
    {
        // Fallback path without transaction manager
        UE_LOG(LogEquipmentBridge, Warning, TEXT("NO TransactionManager - using direct operations"));

        // Clear equipment slot
        BridgeTxn.bEquipmentModified = true;
        FSuspenseCoreInventoryItemInstance ClearedItem = EquipmentDataProvider->ClearSlot(Request.SourceSlot, true);

        if (ClearedItem.IsValid())
        {
            // Add to inventory
            BridgeTxn.bInventoryModified = true;
            FSuspenseCoreInventoryOperationResult AddResult = InventoryInterface->AddItemInstance(EquippedItem);

            if (AddResult.bSuccess)
            {
                bOperationSuccess = true;
            }
            else
            {
                // Rollback: restore to equipment
                EquipmentDataProvider->SetSlotItem(Request.SourceSlot, EquippedItem, true);
                RollbackBridgeTransaction(BridgeTxnId);
                return AddResult;
            }
        }
        else
        {
            RollbackBridgeTransaction(BridgeTxnId);
            bOperationSuccess = false;
        }
    }

    // Check if the operation succeeded
    if (!bOperationSuccess)
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("=== UNEQUIP FAILED ==="));
        return FSuspenseCoreInventoryOperationResult::Failure(
            ESuspenseCoreInventoryErrorCode::UnknownError,
            FText::FromString(TEXT("Failed to unequip item")),
            TEXT("TransferFromEquipToInventory"),
            nullptr
        );
    }

    // Commit the bridge transaction since everything succeeded
    CommitBridgeTransaction(BridgeTxnId);

    // Broadcast visualization event for 3D character model update
    BroadcastUnequippedEvent(EquippedItem, Request.SourceSlot);

    // Notify EventDelegateManager for global UI updates
    if (EventDelegateManager.IsValid())
    {
        FGameplayTag SlotType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Unknown"));
        if (EquipmentDataProvider.GetInterface())
        {
            FEquipmentSlotConfig Config = EquipmentDataProvider->GetSlotConfiguration(Request.SourceSlot);
            if (Config.IsValid())
            {
                SlotType = Config.SlotTag;
            }
        }

        UE_LOG(LogEquipmentBridge, Warning,
            TEXT("Notifying EventDelegateManager: Slot %d cleared (Type: %s)"),
            Request.SourceSlot, *SlotType.ToString());

        // Notify that the slot is now empty (false = not occupied)
        EventDelegateManager->NotifyEquipmentSlotUpdated(Request.SourceSlot, SlotType, false);
        EventDelegateManager->NotifyEquipmentUpdated();
    }

    UE_LOG(LogEquipmentBridge, Warning, TEXT("=== UNEQUIP SUCCESSFUL ==="));
    UE_LOG(LogEquipmentBridge, Warning, TEXT("Item %s moved from slot %d to inventory"),
        *EquippedItem.ItemID.ToString(), Request.SourceSlot);
    UE_LOG(LogEquipmentBridge, Warning, TEXT("=== ExecuteTransfer_FromEquipToInventory END ==="));

    // Create success result with affected items list
    FSuspenseCoreInventoryOperationResult Result = FSuspenseCoreInventoryOperationResult::Success(
        TEXT("TransferFromEquipToInventory"), nullptr);
    Result.AffectedItems.Add(EquippedItem);

    return Result;
}


// ===== Public Transfer Operations =====

FSuspenseCoreInventoryOperationResult USuspenseCoreEquipmentInventoryBridge::TransferFromInventory(const FInventoryTransferRequest& Request)
{
    return ExecuteTransfer_FromInventoryToEquip(Request);
}

FSuspenseCoreInventoryOperationResult USuspenseCoreEquipmentInventoryBridge::TransferToInventory(const FInventoryTransferRequest& Request)
{
    return ExecuteTransfer_FromEquipToInventory(Request);
}

FSuspenseCoreInventoryOperationResult USuspenseCoreEquipmentInventoryBridge::SwapBetweenInventoryAndEquipment(
    const FGuid& InventoryItemInstanceID,
    int32 EquipmentSlotIndex)
{
    return ExecuteSwap_InventoryToEquipment(InventoryItemInstanceID, EquipmentSlotIndex);
}

// ===== Synchronization =====

void USuspenseCoreEquipmentInventoryBridge::SynchronizeWithInventory()
{
    if (!InventoryInterface.GetInterface() || !EquipmentDataProvider.GetInterface())
    {
        return;
    }

    const TMap<int32, FSuspenseCoreInventoryItemInstance> Equipped = EquipmentDataProvider->GetAllEquippedItems();
    const TArray<FSuspenseCoreInventoryItemInstance> InvenItems = InventoryInterface->GetAllItemInstances();

    TSet<FGuid> InventoryInstanceIds;
    InventoryInstanceIds.Reserve(InvenItems.Num());
    for (const FSuspenseCoreInventoryItemInstance& Ii : InvenItems)
    {
        InventoryInstanceIds.Add(Ii.InstanceID);
    }

    for (const TPair<int32, FSuspenseCoreInventoryItemInstance>& SlotIt : Equipped)
    {
        const int32 SlotIdx = SlotIt.Key;
        const FSuspenseCoreInventoryItemInstance& EquippedItem = SlotIt.Value;

        if (EquippedItem.IsValid() && !InventoryInstanceIds.Contains(EquippedItem.InstanceID))
        {
            FSuspenseCoreInventoryItemInstance Found;
            if (FindItemInInventory(EquippedItem.ItemID, Found))
            {
                EquipmentDataProvider->SetSlotItem(SlotIdx, Found, true);
            }
        }
    }
}

// ===== Validation Helpers =====

bool USuspenseCoreEquipmentInventoryBridge::CanEquipFromInventory(const FSuspenseCoreInventoryItemInstance& Item, int32 TargetSlot) const
{
    if (!EquipmentDataProvider.GetInterface() || !EquipmentOperations.GetInterface())
    {
        return false;
    }

    if (!EquipmentDataProvider->IsValidSlotIndex(TargetSlot))
    {
        return false;
    }

    if (EquipmentDataProvider->IsSlotOccupied(TargetSlot))
    {
        return false;
    }

    if (USuspenseCoreEquipmentOperationExecutor* Executor =
        Cast<USuspenseCoreEquipmentOperationExecutor>(EquipmentOperations.GetObject()))
    {
        FSlotValidationResult ValidationResult = Executor->CanEquipItemToSlot(Item, TargetSlot);
        return ValidationResult.bIsValid;
    }

    return true;
}

bool USuspenseCoreEquipmentInventoryBridge::CanUnequipToInventory(int32 SourceSlot) const
{
    if (!EquipmentDataProvider.GetInterface() || !InventoryInterface.GetInterface())
    {
        return false;
    }

    if (!EquipmentDataProvider->IsValidSlotIndex(SourceSlot))
    {
        return false;
    }

    if (!EquipmentDataProvider->IsSlotOccupied(SourceSlot))
    {
        return false;
    }

    FSuspenseCoreInventoryItemInstance EquippedItem = EquipmentDataProvider->GetSlotItem(SourceSlot);
    return InventoryHasSpace(EquippedItem);
}

// ===== Private: Transfer Implementations =====

FSuspenseCoreInventoryOperationResult USuspenseCoreEquipmentInventoryBridge::ExecuteTransfer_FromInventoryToEquip(
    const FInventoryTransferRequest& Request)
{
    const FSuspenseCoreInventoryItemInstance& Item = Request.Item;

    UE_LOG(LogEquipmentBridge, Warning, TEXT("=== ExecuteTransfer_FromInventoryToEquip START ==="));
    UE_LOG(LogEquipmentBridge, Warning, TEXT("ItemID: %s"), *Item.ItemID.ToString());
    UE_LOG(LogEquipmentBridge, Warning, TEXT("InstanceID: %s"), *Item.InstanceID.ToString());
    UE_LOG(LogEquipmentBridge, Warning, TEXT("Quantity: %d"), Item.Quantity);
    UE_LOG(LogEquipmentBridge, Warning, TEXT("Target Slot: %d"), Request.TargetSlot);

    // Step 1: Validate item exists in inventory
    if (!InventoryInterface->HasItem(Item.ItemID, FMath::Max(1, Item.Quantity)))
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("Item %s not found in inventory"), *Item.ItemID.ToString());
        return FSuspenseCoreInventoryOperationResult::ItemNotFound(TEXT("TransferFromInventory"), Item.ItemID);
    }

    UE_LOG(LogEquipmentBridge, Log, TEXT("✓ Item exists in inventory"));

    // Step 2: Pre-validation - Check slot validity
    if (!EquipmentDataProvider->IsValidSlotIndex(Request.TargetSlot))
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("Invalid target slot index: %d"), Request.TargetSlot);
        return FSuspenseCoreInventoryOperationResult::Failure(
            ESuspenseCoreInventoryErrorCode::InvalidSlot,
            FText::Format(FText::FromString(TEXT("Invalid equipment slot: {0}")), FText::AsNumber(Request.TargetSlot)),
            TEXT("TransferFromInventory"),
            nullptr
        );
    }

    // Step 2b: Check if slot is occupied and auto-SWAP
    if (EquipmentDataProvider->IsSlotOccupied(Request.TargetSlot))
    {
        const FSuspenseCoreInventoryItemInstance OccupiedItem = EquipmentDataProvider->GetSlotItem(Request.TargetSlot);

        UE_LOG(LogEquipmentBridge, Warning,
            TEXT("Target slot %d is occupied by %s - checking if SWAP is possible"),
            Request.TargetSlot, *OccupiedItem.ItemID.ToString());

        // Automatic SWAP logic
        if (EquipmentOperations.GetInterface())
        {
            if (USuspenseCoreEquipmentOperationExecutor* Executor =
                Cast<USuspenseCoreEquipmentOperationExecutor>(EquipmentOperations.GetObject()))
            {
                FSlotValidationResult NewItemValidation = Executor->CanEquipItemToSlot(Item, Request.TargetSlot);

                const FString ErrStr = NewItemValidation.ErrorMessage.ToString();
                const bool bOnlyOccupiedReason =
                    !NewItemValidation.bIsValid &&
                    (ErrStr.Contains(TEXT("occupied"), ESearchCase::IgnoreCase) ||
                     ErrStr.Contains(TEXT("занят"),    ESearchCase::IgnoreCase));

                if (NewItemValidation.bIsValid || bOnlyOccupiedReason)
                {
                    UE_LOG(LogEquipmentBridge, Warning,
                        TEXT("✓ Items are swappable - executing automatic SWAP operation"));

                    return ExecuteSwap_InventoryToEquipment(Item.InstanceID, Request.TargetSlot);
                }
                else
                {
                    UE_LOG(LogEquipmentBridge, Warning,
                        TEXT("✗ New item %s is not compatible with slot %d: %s"),
                        *Item.ItemID.ToString(),
                        Request.TargetSlot,
                        *NewItemValidation.ErrorMessage.ToString());

                    return FSuspenseCoreInventoryOperationResult::Failure(
                        ESuspenseCoreInventoryErrorCode::InvalidItem,
                        FText::Format(
                            FText::FromString(TEXT("Cannot equip {0} to slot {1}: {2}")),
                            FText::FromName(Item.ItemID),
                            FText::AsNumber(Request.TargetSlot),
                            NewItemValidation.ErrorMessage),
                        TEXT("TransferFromInventory"),
                        nullptr
                    );
                }
            }
        }

        return FSuspenseCoreInventoryOperationResult::Failure(
            ESuspenseCoreInventoryErrorCode::SlotOccupied,
            FText::Format(
                FText::FromString(TEXT("Equipment slot {0} is occupied. Unequip it first.")),
                FText::AsNumber(Request.TargetSlot)),
            TEXT("TransferFromInventory"),
            nullptr
        );
    }

    UE_LOG(LogEquipmentBridge, Log, TEXT("✓ Target slot %d is empty"), Request.TargetSlot);

    // Step 2c: Validate compatibility with empty slot
    if (EquipmentOperations.GetInterface())
    {
        if (USuspenseCoreEquipmentOperationExecutor* Executor =
            Cast<USuspenseCoreEquipmentOperationExecutor>(EquipmentOperations.GetObject()))
        {
            FSlotValidationResult ValidationResult = Executor->CanEquipItemToSlot(Item, Request.TargetSlot);

            if (!ValidationResult.bIsValid)
            {
                UE_LOG(LogEquipmentBridge, Warning,
                    TEXT("Item %s incompatible with slot %d: %s"),
                    *Item.ItemID.ToString(), Request.TargetSlot, *ValidationResult.ErrorMessage.ToString());

                return FSuspenseCoreInventoryOperationResult::Failure(
                    ESuspenseCoreInventoryErrorCode::InvalidItem,
                    ValidationResult.ErrorMessage,
                    TEXT("TransferFromInventory"),
                    nullptr
                );
            }

            UE_LOG(LogEquipmentBridge, Log, TEXT("✓ Item passed compatibility validation"));
        }
    }

    // Step 3: Remove from inventory
    UE_LOG(LogEquipmentBridge, Warning, TEXT("Removing item from inventory (InstanceID: %s)"), *Item.InstanceID.ToString());

    const FSuspenseCoreInventoryOperationResult RemoveRes = InventoryInterface->RemoveItemInstance(Item.InstanceID);
    if (!RemoveRes.bSuccess)
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("Failed to remove item from inventory: %s"), *RemoveRes.ErrorMessage.ToString());
        return RemoveRes;
    }

    UE_LOG(LogEquipmentBridge, Log, TEXT("✓ Item removed from inventory"));

    // Step 4: Write item to equipment slot with transaction
    bool bEquipSuccess = false;

    if (TransactionManager.GetInterface())
    {
        UE_LOG(LogEquipmentBridge, Warning, TEXT("Using TransactionManager for atomic equip operation"));

        const FString TxnDesc = FString::Printf(TEXT("Equip_%s_to_Slot_%d"), *Item.ItemID.ToString(), Request.TargetSlot);
        const FGuid TxnId = TransactionManager->BeginTransaction(TxnDesc);

        if (TxnId.IsValid())
        {
            if (TransactionManager->SupportsExtendedOps())
            {
                FTransactionOperation EquipOp;
                EquipOp.OperationId = FGuid::NewGuid();
                EquipOp.OperationType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Set"));
                EquipOp.SlotIndex = Request.TargetSlot;
                EquipOp.ItemBefore = FSuspenseCoreInventoryItemInstance();
                EquipOp.ItemAfter = Item;
                EquipOp.bReversible = true;
                EquipOp.Timestamp = FPlatformTime::Seconds();
                EquipOp.Priority = ETransactionPriority::Normal;
                EquipOp.Metadata.Add(TEXT("Source"), TEXT("Inventory"));
                EquipOp.Metadata.Add(TEXT("InstanceID"), Item.InstanceID.ToString());

                TransactionManager->RegisterOperation(TxnId, EquipOp);
                TransactionManager->ApplyOperation(TxnId, EquipOp);
            }

            UE_LOG(LogEquipmentBridge, Warning, TEXT("Writing item to equipment slot %d..."), Request.TargetSlot);

            // CRITICAL: Pass TRUE to broadcast notifications
            bEquipSuccess = EquipmentDataProvider->SetSlotItem(Request.TargetSlot, Item, true);

            if (!bEquipSuccess)
            {
                UE_LOG(LogEquipmentBridge, Error, TEXT("✗ SetSlotItem FAILED"));

                TransactionManager->RollbackTransaction(TxnId);
                InventoryInterface->AddItemInstance(Item);

                return FSuspenseCoreInventoryOperationResult::Failure(
                    ESuspenseCoreInventoryErrorCode::UnknownError,
                    FText::FromString(TEXT("Failed to write item to equipment slot")),
                    TEXT("TransferFromInventory"),
                    nullptr
                );
            }

            UE_LOG(LogEquipmentBridge, Warning, TEXT("✓ Item written to slot %d"), Request.TargetSlot);

            if (TransactionManager->ValidateTransaction(TxnId))
            {
                const bool bCommitSuccess = TransactionManager->CommitTransaction(TxnId);

                if (bCommitSuccess)
                {
                    UE_LOG(LogEquipmentBridge, Warning, TEXT("✓ Transaction committed successfully"));
                }
                else
                {
                    UE_LOG(LogEquipmentBridge, Error, TEXT("✗ Transaction commit failed!"));
                    EquipmentDataProvider->ClearSlot(Request.TargetSlot, true);
                    InventoryInterface->AddItemInstance(Item);
                    bEquipSuccess = false;
                }
            }
            else
            {
                UE_LOG(LogEquipmentBridge, Error, TEXT("✗ Transaction validation failed!"));
                EquipmentDataProvider->ClearSlot(Request.TargetSlot, true);
                TransactionManager->RollbackTransaction(TxnId);
                InventoryInterface->AddItemInstance(Item);
                bEquipSuccess = false;
            }
        }
        else
        {
            UE_LOG(LogEquipmentBridge, Error, TEXT("✗ Failed to begin transaction!"));
            InventoryInterface->AddItemInstance(Item);
            bEquipSuccess = false;
        }
    }
    else
    {
        UE_LOG(LogEquipmentBridge, Warning, TEXT("NO TransactionManager - using direct write"));
        // CRITICAL: Pass TRUE to broadcast notifications
        bEquipSuccess = EquipmentDataProvider->SetSlotItem(Request.TargetSlot, Item, true);

        if (!bEquipSuccess)
        {
            InventoryInterface->AddItemInstance(Item);
        }
    }

    // Step 5: Process result
    if (!bEquipSuccess)
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("=== EQUIP FAILED ==="));
        return FSuspenseCoreInventoryOperationResult::Failure(
            ESuspenseCoreInventoryErrorCode::UnknownError,
            FText::FromString(TEXT("Failed to equip item")),
            TEXT("TransferFromInventory"),
            nullptr
        );
    }

    // CRITICAL: Broadcast visualization event
    BroadcastEquippedEvent(Item, Request.TargetSlot);

    // CRITICAL: Notify EventDelegateManager for global UI updates
    if (EventDelegateManager.IsValid())
    {
        // Get slot configuration for SlotType
        FGameplayTag SlotType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Unknown"));
        if (EquipmentDataProvider.GetInterface())
        {
            FEquipmentSlotConfig Config = EquipmentDataProvider->GetSlotConfiguration(Request.TargetSlot);
            if (Config.IsValid())
            {
                SlotType = Config.SlotTag;
            }
        }

        UE_LOG(LogEquipmentBridge, Warning,
            TEXT("Notifying EventDelegateManager: Slot %d updated (Type: %s, Occupied: YES)"),
            Request.TargetSlot, *SlotType.ToString());

        EventDelegateManager->NotifyEquipmentSlotUpdated(Request.TargetSlot, SlotType, true);
        EventDelegateManager->NotifyEquipmentUpdated();
    }

    UE_LOG(LogEquipmentBridge, Warning, TEXT("✓✓✓ TRANSFER SUCCESSFUL ✓✓✓"));
    UE_LOG(LogEquipmentBridge, Warning, TEXT("Item %s equipped to slot %d"), *Item.ItemID.ToString(), Request.TargetSlot);
    UE_LOG(LogEquipmentBridge, Warning, TEXT("=== ExecuteTransfer_FromInventoryToEquip END ==="));

    FSuspenseCoreInventoryOperationResult SuccessResult = FSuspenseCoreInventoryOperationResult::Success(TEXT("TransferFromInventory"), nullptr);
    SuccessResult.AffectedItems.Add(Item);
    return SuccessResult;
}

void USuspenseCoreEquipmentInventoryBridge::BroadcastUnequippedEvent(
    const FSuspenseCoreInventoryItemInstance& Item,
    int32 SlotIndex)
{
    UE_LOG(LogEquipmentBridge, Warning, TEXT("=== BroadcastUnequippedEvent START ==="));
    UE_LOG(LogEquipmentBridge, Warning, TEXT("Item: %s, Slot: %d"),
        *Item.ItemID.ToString(), SlotIndex);

    AActor* TargetActor = ResolveCharacterTarget();

    if (!TargetActor || !TargetActor->IsA<APawn>())
    {
        UE_LOG(LogEquipmentBridge, Warning,
            TEXT("Cannot broadcast Unequipped event - no valid Character"));
        return;
    }

    const FGameplayTag UnequippedTag = FGameplayTag::RequestGameplayTag(
        TEXT("Equipment.Event.Unequipped"),
        /*ErrorIfNotFound*/ false);

    if (!UnequippedTag.IsValid())
    {
        UE_LOG(LogEquipmentBridge, Error,
            TEXT("Equipment.Event.Unequipped tag not registered!"));
        return;
    }

    FSuspenseCoreEquipmentEventData EvOut;
    EvOut.EventType = UnequippedTag;
    EvOut.Target    = TargetActor;
    EvOut.Source    = this;
    EvOut.Timestamp = FPlatformTime::Seconds();

    EvOut.AddMetadata(TEXT("Slot"),       FString::FromInt(SlotIndex));
    EvOut.AddMetadata(TEXT("ItemID"),     Item.ItemID.ToString());
    EvOut.AddMetadata(TEXT("InstanceID"), Item.InstanceID.ToString());

    UE_LOG(LogEquipmentBridge, Warning, TEXT("Broadcasting Equipment.Event.Unequipped"));
    UE_LOG(LogEquipmentBridge, Warning, TEXT("  Target: %s, Slot: %d"),
        *TargetActor->GetName(), SlotIndex);

    TSharedPtr<FSuspenseCoreEquipmentEventBus> EventBus = FSuspenseCoreEquipmentEventBus::Get();
    if (EventBus.IsValid())
    {
        EventBus->Broadcast(EvOut);
        UE_LOG(LogEquipmentBridge, Warning, TEXT("Event broadcast successful"));
    }
    else
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("EventBus not available!"));
    }

    UE_LOG(LogEquipmentBridge, Warning, TEXT("=== BroadcastUnequippedEvent END ==="));
}
FSuspenseCoreInventoryOperationResult USuspenseCoreEquipmentInventoryBridge::ExecuteSwap_InventoryToEquipment(
    const FGuid& InventoryInstanceID,
    int32 EquipmentSlot)
{
    const FGuid TransactionID = BeginBridgeTransaction();
    FBridgeTransaction& Transaction = ActiveTransactions.FindChecked(TransactionID);

    UE_LOG(LogEquipmentBridge, Warning, TEXT("=== SWAP Inventory <-> Equipment START ==="));
    UE_LOG(LogEquipmentBridge, Warning, TEXT("Inventory InstanceID: %s"), *InventoryInstanceID.ToString());
    UE_LOG(LogEquipmentBridge, Warning, TEXT("Equipment Slot: %d"), EquipmentSlot);

    if (!InventoryInterface.GetInterface() || !EquipmentDataProvider.GetInterface())
    {
        RollbackBridgeTransaction(TransactionID);
        return FSuspenseCoreInventoryOperationResult::Failure(
            ESuspenseCoreInventoryErrorCode::NotInitialized,
            FText::FromString(TEXT("Bridge not initialized")),
            TEXT("SwapInventoryEquipment"),
            nullptr
        );
    }

    // Get items from both sides
    FSuspenseCoreInventoryItemInstance InventoryItem;
    bool bFoundInInventory = false;

    const TArray<FSuspenseCoreInventoryItemInstance> AllItems = InventoryInterface->GetAllItemInstances();
    for (const FSuspenseCoreInventoryItemInstance& Item : AllItems)
    {
        if (Item.InstanceID == InventoryInstanceID)
        {
            InventoryItem = Item;
            bFoundInInventory = true;
            Transaction.InventoryBackup = Item;
            Transaction.InventorySlot = Item.AnchorIndex;
            break;
        }
    }

    if (!bFoundInInventory)
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("✗ Item not found in inventory!"));
        RollbackBridgeTransaction(TransactionID);
        return FSuspenseCoreInventoryOperationResult::ItemNotFound(TEXT("SwapInventoryEquipment"), FName("Unknown"));
    }

    UE_LOG(LogEquipmentBridge, Log, TEXT("✓ Found inventory item: %s"), *InventoryItem.ItemID.ToString());

    FSuspenseCoreInventoryItemInstance EquippedItem;
    if (EquipmentDataProvider->IsSlotOccupied(EquipmentSlot))
    {
        EquippedItem = EquipmentDataProvider->GetSlotItem(EquipmentSlot);
        Transaction.EquipmentBackup = EquippedItem;
        UE_LOG(LogEquipmentBridge, Log, TEXT("✓ Slot %d occupied by: %s"), EquipmentSlot, *EquippedItem.ItemID.ToString());
    }
    Transaction.EquipmentSlot = EquipmentSlot;

    // Check inventory space for unequipped item
    if (EquippedItem.IsValid() && !InventoryHasSpace(EquippedItem))
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("✗ No space in inventory"));
        RollbackBridgeTransaction(TransactionID);
        return FSuspenseCoreInventoryOperationResult::Failure(
            ESuspenseCoreInventoryErrorCode::NoSpace,
            FText::FromString(TEXT("No space in inventory for unequipped item")),
            TEXT("SwapInventoryEquipment"),
            nullptr
        );
    }

    // Execute atomic swap with transaction
    if (TransactionManager.GetInterface())
    {
        const FString TransactionDesc = FString::Printf(
            TEXT("Swap_Inventory_Equipment: %s <-> Slot_%d"),
            *InventoryItem.ItemID.ToString(),
            EquipmentSlot
        );
        const FGuid EquipTransID = TransactionManager->BeginTransaction(TransactionDesc);

        if (TransactionManager->SupportsExtendedOps())
        {
            const float CurrentTime = FPlatformTime::Seconds();

            if (EquippedItem.IsValid())
            {
                FTransactionOperation UnequipOp;
                UnequipOp.OperationId = FGuid::NewGuid();
                UnequipOp.OperationType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Clear"));
                UnequipOp.SlotIndex = EquipmentSlot;
                UnequipOp.ItemBefore = EquippedItem;
                UnequipOp.ItemAfter = FSuspenseCoreInventoryItemInstance();
                UnequipOp.bReversible = true;
                UnequipOp.Timestamp = CurrentTime;
                UnequipOp.Priority = ETransactionPriority::High;

                TransactionManager->RegisterOperation(EquipTransID, UnequipOp);
                TransactionManager->ApplyOperation(EquipTransID, UnequipOp);
            }

            FTransactionOperation EquipOp;
            EquipOp.OperationId = FGuid::NewGuid();
            EquipOp.OperationType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Set"));
            EquipOp.SlotIndex = EquipmentSlot;
            EquipOp.ItemBefore = EquippedItem.IsValid() ? EquippedItem : FSuspenseCoreInventoryItemInstance();
            EquipOp.ItemAfter = InventoryItem;
            EquipOp.bReversible = true;
            EquipOp.Timestamp = CurrentTime + 0.001f;
            EquipOp.Priority = ETransactionPriority::Normal;
            EquipOp.Metadata.Add(TEXT("SourceInventoryInstance"), InventoryInstanceID.ToString());

            if (EquippedItem.IsValid())
            {
                EquipOp.Metadata.Add(TEXT("OperationContext"), TEXT("Swap"));
                EquipOp.SecondaryItemBefore = EquippedItem;
                EquipOp.SecondaryItemAfter = FSuspenseCoreInventoryItemInstance();
            }

            TransactionManager->RegisterOperation(EquipTransID, EquipOp);
            TransactionManager->ApplyOperation(EquipTransID, EquipOp);
        }

        // Phase 1: Remove from inventory
        UE_LOG(LogEquipmentBridge, Warning, TEXT("Phase 1: Removing %s from inventory"), *InventoryItem.ItemID.ToString());
        Transaction.bInventoryModified = true;
        FSuspenseCoreInventoryOperationResult RemoveResult = InventoryInterface->RemoveItemInstance(InventoryInstanceID);

        if (!RemoveResult.bSuccess)
        {
            UE_LOG(LogEquipmentBridge, Error, TEXT("✗ Failed to remove item from inventory"));
            TransactionManager->RollbackTransaction(EquipTransID);
            RollbackBridgeTransaction(TransactionID);
            return RemoveResult;
        }

        // Phase 2: Clear equipment slot if occupied
        if (EquippedItem.IsValid())
        {
            UE_LOG(LogEquipmentBridge, Warning, TEXT("Phase 2: Clearing equipment slot %d"), EquipmentSlot);
            Transaction.bEquipmentModified = true;
            // CRITICAL: Pass TRUE to broadcast notifications
            EquipmentDataProvider->ClearSlot(EquipmentSlot, true);
        }

        // Phase 3: Place inventory item in equipment slot
        UE_LOG(LogEquipmentBridge, Warning, TEXT("Phase 3: Placing %s in equipment slot %d"),
            *InventoryItem.ItemID.ToString(), EquipmentSlot);

        // CRITICAL: Pass TRUE to broadcast notifications
        if (!EquipmentDataProvider->SetSlotItem(EquipmentSlot, InventoryItem, true))
        {
            UE_LOG(LogEquipmentBridge, Error, TEXT("✗ Failed to write item to equipment slot!"));
            TransactionManager->RollbackTransaction(EquipTransID);
            RollbackBridgeTransaction(TransactionID);

            return FSuspenseCoreInventoryOperationResult::Failure(
                ESuspenseCoreInventoryErrorCode::UnknownError,
                FText::FromString(TEXT("Failed to equip item")),
                TEXT("SwapInventoryEquipment"),
                nullptr
            );
        }

        // Phase 4: Add previously equipped item to inventory (if any)
        if (EquippedItem.IsValid())
        {
            UE_LOG(LogEquipmentBridge, Warning, TEXT("Phase 4: Adding %s back to inventory"),
                *EquippedItem.ItemID.ToString());

            FSuspenseCoreInventoryOperationResult AddResult = InventoryInterface->AddItemInstance(EquippedItem);

            if (!AddResult.bSuccess)
            {
                UE_LOG(LogEquipmentBridge, Error, TEXT("✗ Failed to add unequipped item to inventory"));
                TransactionManager->RollbackTransaction(EquipTransID);
                RollbackBridgeTransaction(TransactionID);
                return AddResult;
            }
        }

        // Validate and commit
        if (!TransactionManager->ValidateTransaction(EquipTransID))
        {
            UE_LOG(LogEquipmentBridge, Error, TEXT("✗ Transaction validation failed"));
            TransactionManager->RollbackTransaction(EquipTransID);
            RollbackBridgeTransaction(TransactionID);

            return FSuspenseCoreInventoryOperationResult::Failure(
                ESuspenseCoreInventoryErrorCode::UnknownError,
                FText::FromString(TEXT("Transaction validation failed")),
                TEXT("SwapInventoryEquipment"),
                nullptr
            );
        }

        if (!TransactionManager->CommitTransaction(EquipTransID))
        {
            UE_LOG(LogEquipmentBridge, Error, TEXT("✗ Transaction commit failed"));
            RollbackBridgeTransaction(TransactionID);
            return FSuspenseCoreInventoryOperationResult::Failure(
                ESuspenseCoreInventoryErrorCode::UnknownError,
                FText::FromString(TEXT("Failed to commit transaction")),
                TEXT("SwapInventoryEquipment"),
                nullptr
            );
        }

        UE_LOG(LogEquipmentBridge, Warning, TEXT("✓ Transaction committed successfully"));
    }
    else
    {
        // Fallback without transaction manager
        UE_LOG(LogEquipmentBridge, Warning, TEXT("⚠ No TransactionManager - using direct operations"));

        FSuspenseCoreInventoryOperationResult RemoveResult = InventoryInterface->RemoveItemInstance(InventoryInstanceID);
        if (!RemoveResult.bSuccess)
        {
            RollbackBridgeTransaction(TransactionID);
            return RemoveResult;
        }
        Transaction.bInventoryModified = true;

        if (EquippedItem.IsValid())
        {
            EquipmentDataProvider->ClearSlot(EquipmentSlot, true);
            Transaction.bEquipmentModified = true;
        }

        if (!EquipmentDataProvider->SetSlotItem(EquipmentSlot, InventoryItem, true))
        {
            RollbackBridgeTransaction(TransactionID);
            return FSuspenseCoreInventoryOperationResult::Failure(
                ESuspenseCoreInventoryErrorCode::UnknownError,
                FText::FromString(TEXT("Failed to equip item")),
                TEXT("SwapInventoryEquipment"),
                nullptr
            );
        }

        if (EquippedItem.IsValid())
        {
            FSuspenseCoreInventoryOperationResult AddResult = InventoryInterface->AddItemInstance(EquippedItem);
            if (!AddResult.bSuccess)
            {
                RollbackBridgeTransaction(TransactionID);
                return AddResult;
            }
        }
    }

    CommitBridgeTransaction(TransactionID);

    // CRITICAL: Broadcast visualization events
    BroadcastSwapEvents(InventoryItem, EquippedItem, EquipmentSlot);

    // CRITICAL: Notify EventDelegateManager
    if (EventDelegateManager.IsValid())
    {
        FGameplayTag SlotType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Unknown"));
        if (EquipmentDataProvider.GetInterface())
        {
            FEquipmentSlotConfig Config = EquipmentDataProvider->GetSlotConfiguration(EquipmentSlot);
            if (Config.IsValid())
            {
                SlotType = Config.SlotTag;
            }
        }

        UE_LOG(LogEquipmentBridge, Warning,
            TEXT("Notifying EventDelegateManager: Slot %d swapped (Type: %s)"),
            EquipmentSlot, *SlotType.ToString());

        EventDelegateManager->NotifyEquipmentSlotUpdated(EquipmentSlot, SlotType, true);
        EventDelegateManager->NotifyEquipmentUpdated();
    }

    UE_LOG(LogEquipmentBridge, Warning, TEXT("✓✓✓ SWAP COMPLETED SUCCESSFULLY ✓✓✓"));
    UE_LOG(LogEquipmentBridge, Warning, TEXT("  IN: %s → Slot %d"), *InventoryItem.ItemID.ToString(), EquipmentSlot);
    if (EquippedItem.IsValid())
    {
        UE_LOG(LogEquipmentBridge, Warning, TEXT("  OUT: %s → Inventory"), *EquippedItem.ItemID.ToString());
    }
    UE_LOG(LogEquipmentBridge, Warning, TEXT("=== SWAP END ==="));

    FSuspenseCoreInventoryOperationResult Result = FSuspenseCoreInventoryOperationResult::Success(TEXT("SwapInventoryEquipment"), nullptr);
    Result.AffectedItems.Add(InventoryItem);
    if (EquippedItem.IsValid())
    {
        Result.AffectedItems.Add(EquippedItem);
    }

    return Result;
}

// ===== Transaction Management =====

FGuid USuspenseCoreEquipmentInventoryBridge::BeginBridgeTransaction()
{
    FScopeLock Lock(&TransactionLock);

    FBridgeTransaction NewTransaction;
    NewTransaction.TransactionID = FGuid::NewGuid();
    ActiveTransactions.Add(NewTransaction.TransactionID, NewTransaction);

    return NewTransaction.TransactionID;
}

bool USuspenseCoreEquipmentInventoryBridge::CommitBridgeTransaction(const FGuid& TransactionID)
{
    FScopeLock Lock(&TransactionLock);

    if (!ActiveTransactions.Contains(TransactionID))
    {
        return false;
    }

    ActiveTransactions.Remove(TransactionID);

    return true;
}

bool USuspenseCoreEquipmentInventoryBridge::RollbackBridgeTransaction(const FGuid& TransactionID)
{
    FScopeLock Lock(&TransactionLock);

    FBridgeTransaction* Transaction = ActiveTransactions.Find(TransactionID);
    if (!Transaction)
    {
        return false;
    }

    // Rollback in reverse order
    if (Transaction->bEquipmentModified && EquipmentDataProvider.GetInterface())
    {
        if (Transaction->EquipmentBackup.IsValid())
        {
            EquipmentDataProvider->SetSlotItem(
                Transaction->EquipmentSlot,
                Transaction->EquipmentBackup,
                false
            );
        }
        else
        {
            EquipmentDataProvider->ClearSlot(Transaction->EquipmentSlot, false);
        }
    }

    if (Transaction->bInventoryModified && InventoryInterface.GetInterface())
    {
        if (Transaction->InventoryBackup.IsValid())
        {
            InventoryInterface->AddItemInstance(Transaction->InventoryBackup);
        }
    }

    ActiveTransactions.Remove(TransactionID);
    return true;
}

// ===== Validation Utilities =====

bool USuspenseCoreEquipmentInventoryBridge::ValidateInventorySpace(const FSuspenseCoreInventoryItemInstance& Item) const
{
    if (!InventoryInterface.GetInterface())
    {
        return false;
    }

    return InventoryHasSpace(Item);
}

bool USuspenseCoreEquipmentInventoryBridge::ValidateEquipmentSlot(int32 SlotIndex, const FSuspenseCoreInventoryItemInstance& Item) const
{
    if (!EquipmentDataProvider.GetInterface())
    {
        return false;
    }

    if (!EquipmentDataProvider->IsValidSlotIndex(SlotIndex))
    {
        return false;
    }

    return true;
}

bool USuspenseCoreEquipmentInventoryBridge::InventoryHasSpace(const FSuspenseCoreInventoryItemInstance& Item) const
{
    return InventoryInterface.GetInterface() != nullptr;
}

// ===== Helper Functions =====

void USuspenseCoreEquipmentInventoryBridge::CleanupExpiredReservations()
{
    if (ActiveReservations.Num() == 0)
    {
        return;
    }

    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    TArray<FGuid> ExpiredReservations;
    for (const auto& Pair : ActiveReservations)
    {
        if (Pair.Value.ExpirationTime <= Now)
        {
            ExpiredReservations.Add(Pair.Key);
        }
    }

    for (const FGuid& ExpiredID : ExpiredReservations)
    {
        ActiveReservations.Remove(ExpiredID);
    }
}

bool USuspenseCoreEquipmentInventoryBridge::FindItemInInventory(const FName& ItemId, FSuspenseCoreInventoryItemInstance& OutInstance) const
{
    if (!InventoryInterface.GetInterface())
    {
        return false;
    }

    const TArray<FSuspenseCoreInventoryItemInstance> Items = InventoryInterface->GetAllItemInstances();
    for (const FSuspenseCoreInventoryItemInstance& It : Items)
    {
        if (It.ItemID == ItemId)
        {
            OutInstance = It;
            return true;
        }
    }
    return false;
}

// ========================================================================
// CRITICAL: Character Resolution and Event Broadcasting
// ========================================================================

AActor* USuspenseCoreEquipmentInventoryBridge::ResolveCharacterTarget() const
{
    AActor* Owner = GetOwner();

    if (!Owner)
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("[EquipmentBridge] Owner is NULL!"));
        return nullptr;
    }

    // PRIMARY STRATEGY: Owner is PlayerState (expected for this component)
    if (APlayerState* PS = Cast<APlayerState>(Owner))
    {
        UE_LOG(LogEquipmentBridge, Log, TEXT("[EquipmentBridge] Owner is PlayerState, searching for Pawn..."));

        // Sub-strategy 1: Direct GetPawn() - fastest and most reliable
        if (APawn* DirectPawn = PS->GetPawn())
        {
            UE_LOG(LogEquipmentBridge, Log, TEXT("[EquipmentBridge] Found Pawn via PlayerState::GetPawn() - %s"),
                *DirectPawn->GetName());
            return DirectPawn;
        }

        // Sub-strategy 2: Try via PlayerController
        if (APlayerController* PC = Cast<APlayerController>(PS->GetOwner()))
        {
            if (APawn* PCPawn = PC->GetPawn())
            {
                UE_LOG(LogEquipmentBridge, Log, TEXT("[EquipmentBridge] Found Pawn via PlayerController - %s"),
                    *PCPawn->GetName());
                return PCPawn;
            }
        }

        // Sub-strategy 3: World iteration (expensive but handles all cases)
        if (UWorld* World = GetWorld())
        {
            for (TActorIterator<APawn> It(World, APawn::StaticClass()); It; ++It)
            {
                APawn* CandidatePawn = *It;

                if (!IsValid(CandidatePawn) || CandidatePawn->IsPendingKillPending())
                {
                    continue;
                }

                if (CandidatePawn->GetPlayerState() == PS)
                {
                    UE_LOG(LogEquipmentBridge, Log, TEXT("[EquipmentBridge] Found Pawn via world iteration - %s"),
                        *CandidatePawn->GetName());
                    return CandidatePawn;
                }
            }
        }

        UE_LOG(LogEquipmentBridge, Warning, TEXT("[EquipmentBridge] No Pawn found for PlayerState (Character may not be spawned yet)"));
        return nullptr;
    }

    // FALLBACK: Owner is directly a Pawn
    if (APawn* DirectPawn = Cast<APawn>(Owner))
    {
        UE_LOG(LogEquipmentBridge, Warning, TEXT("[EquipmentBridge] Owner is Pawn directly - %s (unusual configuration)"),
            *DirectPawn->GetName());
        return DirectPawn;
    }

    // FALLBACK: Owner is PlayerController
    if (APlayerController* PC = Cast<APlayerController>(Owner))
    {
        UE_LOG(LogEquipmentBridge, Warning, TEXT("[EquipmentBridge] Owner is PlayerController (unusual configuration)"));

        if (APawn* PCPawn = PC->GetPawn())
        {
            UE_LOG(LogEquipmentBridge, Log, TEXT("[EquipmentBridge] Found Pawn via PC - %s"), *PCPawn->GetName());
            return PCPawn;
        }
    }

    UE_LOG(LogEquipmentBridge, Error, TEXT("[EquipmentBridge] Could not resolve Character - Owner type: %s"),
        *Owner->GetClass()->GetName());
    return nullptr;
}

void USuspenseCoreEquipmentInventoryBridge::BroadcastEquippedEvent(const FSuspenseCoreInventoryItemInstance& Item, int32 SlotIndex)
{
    UE_LOG(LogEquipmentBridge, Warning, TEXT("=== BroadcastEquippedEvent START ==="));
    UE_LOG(LogEquipmentBridge, Warning, TEXT("Item: %s, Slot: %d"),
        *Item.ItemID.ToString(), SlotIndex);

    AActor* TargetActor = ResolveCharacterTarget();

    if (!TargetActor)
    {
        UE_LOG(LogEquipmentBridge, Warning,
            TEXT("[EquipmentBridge] Cannot broadcast Equipped event - Character not found"));
        UE_LOG(LogEquipmentBridge, Warning,
            TEXT("  Checking owner type..."));

        if (AActor* Owner = GetOwner())
        {
            UE_LOG(LogEquipmentBridge, Warning, TEXT("  Owner exists: %s"),
                *Owner->GetClass()->GetName());

            if (APlayerState* PS = Cast<APlayerState>(Owner))
            {
                UE_LOG(LogEquipmentBridge, Warning, TEXT("  Owner is PlayerState"));
                UE_LOG(LogEquipmentBridge, Warning, TEXT("  PlayerState has pawn: %s"),
                    PS->GetPawn() ? TEXT("YES") : TEXT("NO"));
            }
        }
        else
        {
            UE_LOG(LogEquipmentBridge, Error, TEXT("  Owner is NULL!"));
        }

        return;
    }

    UE_LOG(LogEquipmentBridge, Warning, TEXT("Target actor found: %s"),
        *TargetActor->GetName());

    if (!TargetActor->IsA<APawn>())
    {
        UE_LOG(LogEquipmentBridge, Error,
            TEXT("[EquipmentBridge] Target is not a Pawn! Type: %s"),
            *TargetActor->GetClass()->GetName());
        return;
    }

    // Verify tag registration
    const FGameplayTag EquippedTag = FGameplayTag::RequestGameplayTag(
        TEXT("Equipment.Event.Equipped"),
        /*ErrorIfNotFound*/ false);

    if (!EquippedTag.IsValid())
    {
        UE_LOG(LogEquipmentBridge, Error,
            TEXT("[EquipmentBridge] Equipment.Event.Equipped tag not registered!"));
        UE_LOG(LogEquipmentBridge, Error,
            TEXT("  Make sure GameplayTags are properly configured in project settings"));
        return;
    }

    // Create and populate event
    FSuspenseCoreEquipmentEventData EvIn;
    EvIn.EventType = EquippedTag;
    EvIn.Target    = TargetActor;
    EvIn.Source    = this;
    EvIn.Timestamp = FPlatformTime::Seconds();

    // Add metadata
    EvIn.AddMetadata(TEXT("Slot"),       FString::FromInt(SlotIndex));
    EvIn.AddMetadata(TEXT("ItemID"),     Item.ItemID.ToString());
    EvIn.AddMetadata(TEXT("InstanceID"), Item.InstanceID.ToString());
    EvIn.AddMetadata(TEXT("Quantity"),   FString::FromInt(Item.Quantity));

    UE_LOG(LogEquipmentBridge, Warning, TEXT("Broadcasting Equipment.Event.Equipped"));
    UE_LOG(LogEquipmentBridge, Warning, TEXT("  Target: %s"), *TargetActor->GetName());
    UE_LOG(LogEquipmentBridge, Warning, TEXT("  Slot: %d, ItemID: %s"), SlotIndex, *Item.ItemID.ToString());

    // FIXED: Correctly work with TSharedPtr
    TSharedPtr<FSuspenseCoreEquipmentEventBus> EventBus = FSuspenseCoreEquipmentEventBus::Get();
    if (EventBus.IsValid())
    {
        EventBus->Broadcast(EvIn);
        UE_LOG(LogEquipmentBridge, Warning, TEXT("Event broadcast successful"));
    }
    else
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("EventBus not available!"));
    }

    UE_LOG(LogEquipmentBridge, Warning, TEXT("=== BroadcastEquippedEvent END ==="));
}

void USuspenseCoreEquipmentInventoryBridge::BroadcastSwapEvents(
    const FSuspenseCoreInventoryItemInstance& NewItem,
    const FSuspenseCoreInventoryItemInstance& OldItem,
    int32 SlotIndex)
{
    AActor* TargetActor = ResolveCharacterTarget();

    if (!TargetActor || !TargetActor->IsA<APawn>())
    {
        UE_LOG(LogEquipmentBridge, Warning, TEXT("[EquipmentBridge] Cannot broadcast Swap events - no valid Character"));
        return;
    }

    // Broadcast Unequipped event for old item (if exists)
    if (OldItem.IsValid())
    {
        const FGameplayTag UnequippedTag = FGameplayTag::RequestGameplayTag(
            TEXT("Equipment.Event.Unequipped"),
            /*ErrorIfNotFound*/ false);

        if (UnequippedTag.IsValid())
        {
            FSuspenseCoreEquipmentEventData EvOut;
            EvOut.EventType = UnequippedTag;
            EvOut.Target    = TargetActor;
            EvOut.Source    = this;
            EvOut.Timestamp = FPlatformTime::Seconds();
            EvOut.AddMetadata(TEXT("Slot"),   FString::FromInt(SlotIndex));
            EvOut.AddMetadata(TEXT("ItemID"), OldItem.ItemID.ToString());

            FSuspenseCoreEquipmentEventBus::Get()->Broadcast(EvOut);
            UE_LOG(LogEquipmentBridge, Log, TEXT("[EquipmentBridge] Broadcasted Unequipped event for %s"), *OldItem.ItemID.ToString());
        }
    }

    // Broadcast Equipped event for new item
    BroadcastEquippedEvent(NewItem, SlotIndex);
}

FSlotValidationResult USuspenseCoreEquipmentOperationExecutor::CanEquipItemToSlot(
    const FSuspenseCoreInventoryItemInstance& ItemInstance,
    int32 TargetSlotIndex) const
{
    FEquipmentOperationRequest TempRequest;
    TempRequest.OperationType = EEquipmentOperationType::Equip;
    TempRequest.ItemInstance = ItemInstance;
    TempRequest.TargetSlotIndex = TargetSlotIndex;
    TempRequest.OperationId = FGuid::NewGuid();
    TempRequest.Timestamp = FPlatformTime::Seconds();

    FSlotValidationResult Result = ValidateEquip(TempRequest);

    return Result;
}
