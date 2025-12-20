// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Components/Core/SuspenseCoreEquipmentInventoryBridge.h"
#include "SuspenseCore/Components/Core/SuspenseCoreEquipmentOperationExecutor.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceLocator.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentDataProvider.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentOperations.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreTransactionManager.h"
#include "SuspenseCore/Interfaces/Inventory/ISuspenseCoreInventory.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/ItemSystem/SuspenseCoreItemManager.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreItemDataTable.h"
#include "SuspenseCore/Services/SuspenseCoreLoadoutManager.h"

// === Includes for EventBus and character resolution ===
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogEquipmentBridge, Log, All);

// ===== Type Conversion Helpers =====
// Bridge between FSuspenseCoreItemInstance (ISuspenseCoreInventory) and
// FSuspenseCoreInventoryItemInstance (ISuspenseCoreEquipmentDataProvider)

static FSuspenseCoreInventoryItemInstance ConvertToInventoryItemInstance(const FSuspenseCoreItemInstance& Source)
{
    FSuspenseCoreInventoryItemInstance Result;
    Result.ItemID = Source.ItemID;
    Result.InstanceID = Source.UniqueInstanceID;
    Result.Quantity = Source.Quantity;
    return Result;
}

static FSuspenseCoreItemInstance ConvertToItemInstance(const FSuspenseCoreInventoryItemInstance& Source)
{
    FSuspenseCoreItemInstance Result;
    Result.ItemID = Source.ItemID;
    Result.UniqueInstanceID = Source.InstanceID;
    Result.Quantity = Source.Quantity;
    return Result;
}

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
        EventDelegateManager->UnsubscribeFromEvent(EquipmentOperationRequestHandle);
        EquipmentOperationRequestHandle = FSuspenseCoreSubscriptionHandle();

        UE_LOG(LogEquipmentBridge, Log, TEXT("Unsubscribed from EventManager"));
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
        const FGameplayTag ServiceTag = FGameplayTag::RequestGameplayTag(TEXT("Service.Equipment.Operation"), /*bErrorIfNotFound*/ false);
        if (ServiceTag.IsValid())
        {
            if (UObject* SvcObj = Locator->GetService(ServiceTag))
            {
                if (SvcObj->GetClass()->ImplementsInterface(USuspenseCoreEquipmentOperationServiceInterface::StaticClass()))
                {
                    EquipmentService.SetObject(SvcObj);
                    EquipmentService.SetInterface(Cast<ISuspenseCoreEquipmentOperationServiceInterface>(SvcObj));
                    UE_LOG(LogEquipmentBridge, Log, TEXT("OperationService acquired via locator"));
                }
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

    if (!EventDelegateManager->GetEventBus())
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("FAILED - EventBus not available!"));
        return false;
    }

    // Unsubscribe from previous subscription if exists
    if (EquipmentOperationRequestHandle.IsValid())
    {
        UE_LOG(LogEquipmentBridge, Warning,
            TEXT("Found existing subscription handle - unsubscribing first"));

        EventDelegateManager->UnsubscribeFromEvent(EquipmentOperationRequestHandle);
        EquipmentOperationRequestHandle = FSuspenseCoreSubscriptionHandle();
    }

    // Subscribe to UI equip requests via EventBus (using native callback)
    // Tag is registered in DefaultGameplayTags.ini line 639
    if (EventDelegateManager->GetEventBus())
    {
        FSuspenseCoreNativeEventCallback Callback;
        Callback.BindLambda([this](FGameplayTag /*EventTag*/, const FSuspenseCoreEventData& EventData)
        {
            // Extract request from event data (standard UI request format)
            FEquipmentOperationRequest Request;
            Request.OperationType = EEquipmentOperationType::Equip;
            Request.TargetSlotIndex = EventData.GetInt(FName(TEXT("TargetSlot")));
            Request.SourceSlotIndex = EventData.GetInt(FName(TEXT("SourceSlot")));

            // Extract ItemInstance data from event (sent by EquipmentWidget)
            FString ItemIDStr = EventData.GetString(FName(TEXT("ItemID")));
            FString InstanceIDStr = EventData.GetString(FName(TEXT("InstanceID")));

            if (!ItemIDStr.IsEmpty())
            {
                Request.ItemInstance.ItemID = FName(*ItemIDStr);
            }
            if (!InstanceIDStr.IsEmpty())
            {
                FGuid::Parse(InstanceIDStr, Request.ItemInstance.UniqueInstanceID);
            }
            Request.ItemInstance.Quantity = 1;

            // Generate operation ID
            Request.OperationId = FGuid::NewGuid();

            UE_LOG(LogEquipmentBridge, Warning, TEXT("UIRequest.EquipItem received - ItemID: %s, InstanceID: %s, TargetSlot: %d"),
                *Request.ItemInstance.ItemID.ToString(),
                *Request.ItemInstance.UniqueInstanceID.ToString(),
                Request.TargetSlotIndex);

            HandleEquipmentOperationRequest(Request);
        });

        // Use RequestGameplayTag - tag is registered in DefaultGameplayTags.ini
        static const FGameplayTag EquipItemTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UIRequest.EquipItem"));
        EquipmentOperationRequestHandle = EventDelegateManager->GetEventBus()->SubscribeNative(
            EquipItemTag, this, Callback);

        UE_LOG(LogEquipmentBridge, Warning, TEXT("Subscribed to SuspenseCore.Event.UIRequest.EquipItem tag"));

        // Subscribe to TransferItem for Equipment→Inventory transfers (unequip via drag-drop)
        auto TransferCallback = FSuspenseCoreEventCallback::CreateUObject(this,
            &USuspenseCoreEquipmentInventoryBridge::OnTransferItemRequest);

        static const FGameplayTag TransferItemTag = FGameplayTag::RequestGameplayTag(
            FName("SuspenseCore.Event.UIRequest.TransferItem"), /*bErrorIfNotFound*/ false);

        if (TransferItemTag.IsValid())
        {
            TransferItemRequestHandle = EventDelegateManager->GetEventBus()->SubscribeNative(
                TransferItemTag, this, TransferCallback);
            UE_LOG(LogEquipmentBridge, Warning, TEXT("Subscribed to SuspenseCore.Event.UIRequest.TransferItem tag"));
        }

        // Subscribe to UnequipItem for context menu unequip
        auto UnequipCallback = FSuspenseCoreEventCallback::CreateUObject(this,
            &USuspenseCoreEquipmentInventoryBridge::OnUnequipItemRequest);

        static const FGameplayTag UnequipItemTag = FGameplayTag::RequestGameplayTag(
            FName("SuspenseCore.Event.UIRequest.UnequipItem"), /*bErrorIfNotFound*/ false);

        if (UnequipItemTag.IsValid())
        {
            UnequipItemRequestHandle = EventDelegateManager->GetEventBus()->SubscribeNative(
                UnequipItemTag, this, UnequipCallback);
            UE_LOG(LogEquipmentBridge, Warning, TEXT("Subscribed to SuspenseCore.Event.UIRequest.UnequipItem tag"));
        }
    }
    else
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("Failed to subscribe - EventBus null"));
    }

    UE_LOG(LogEquipmentBridge, Log, TEXT("EventBus subscription configured"));

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

// ===== TransferItem / UnequipItem Handlers =====

void USuspenseCoreEquipmentInventoryBridge::OnTransferItemRequest(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
    UE_LOG(LogEquipmentBridge, Warning, TEXT("=== OnTransferItemRequest received ==="));

    // Parse event data
    FString SourceContainerIDStr = EventData.GetString(FName("SourceContainerID"));
    int32 SourceSlot = EventData.GetInt(FName("SourceSlot"));
    FString TargetContainerIDStr = EventData.GetString(FName("TargetContainerID"));
    int32 TargetSlot = EventData.GetInt(FName("TargetSlot"));
    FString ItemInstanceIDStr = EventData.GetString(FName("ItemInstanceID"));

    UE_LOG(LogEquipmentBridge, Warning, TEXT("  SourceContainerID: %s"), *SourceContainerIDStr);
    UE_LOG(LogEquipmentBridge, Warning, TEXT("  SourceSlot: %d"), SourceSlot);
    UE_LOG(LogEquipmentBridge, Warning, TEXT("  TargetContainerID: %s"), *TargetContainerIDStr);
    UE_LOG(LogEquipmentBridge, Warning, TEXT("  TargetSlot: %d"), TargetSlot);
    UE_LOG(LogEquipmentBridge, Warning, TEXT("  ItemInstanceID: %s"), *ItemInstanceIDStr);

    // Check if this is an Equipment→Inventory transfer (unequip operation)
    // We need to identify if the source is our equipment provider
    // For now, check if SourceSlot is valid in our equipment data
    if (!EquipmentDataProvider.GetInterface())
    {
        UE_LOG(LogEquipmentBridge, Warning, TEXT("TransferItem: No EquipmentDataProvider - ignoring"));
        return;
    }

    // Try to get item from equipment slot
    FSuspenseCoreEquipmentSlotData SlotData;
    if (!EquipmentDataProvider->GetSlotData(SourceSlot, SlotData))
    {
        UE_LOG(LogEquipmentBridge, Verbose, TEXT("TransferItem: SourceSlot %d not valid in equipment - not our transfer"), SourceSlot);
        return;
    }

    // Check if slot has item matching the InstanceID
    FGuid ItemInstanceID;
    FGuid::Parse(ItemInstanceIDStr, ItemInstanceID);

    if (!SlotData.EquippedItem.IsValid() || SlotData.EquippedItem.UniqueInstanceID != ItemInstanceID)
    {
        UE_LOG(LogEquipmentBridge, Verbose, TEXT("TransferItem: Item in slot doesn't match InstanceID - not our transfer"));
        return;
    }

    UE_LOG(LogEquipmentBridge, Warning, TEXT("TransferItem: This is Equipment→Inventory unequip operation!"));

    // Create unequip request
    FSuspenseCoreInventoryTransferRequest Request;
    Request.Item = SlotData.EquippedItem;
    Request.SourceSlot = SourceSlot;
    Request.TargetSlot = TargetSlot;
    Request.bFromInventory = false;
    Request.bToInventory = true;

    // Execute unequip to inventory
    FSuspenseCoreInventorySimpleResult Result = TransferToInventory(Request);

    UE_LOG(LogEquipmentBridge, Warning, TEXT("TransferItem: Unequip result - Success: %s, Message: %s"),
        Result.bSuccess ? TEXT("YES") : TEXT("NO"),
        *Result.ErrorMessage.ToString());
}

void USuspenseCoreEquipmentInventoryBridge::OnUnequipItemRequest(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
    UE_LOG(LogEquipmentBridge, Warning, TEXT("=== OnUnequipItemRequest received ==="));

    // Parse event data
    int32 SlotIndex = EventData.GetInt(FName("SlotIndex"));
    FString ItemInstanceIDStr = EventData.GetString(FName("ItemInstanceID"));

    UE_LOG(LogEquipmentBridge, Warning, TEXT("  SlotIndex: %d"), SlotIndex);
    UE_LOG(LogEquipmentBridge, Warning, TEXT("  ItemInstanceID: %s"), *ItemInstanceIDStr);

    // Validate equipment data provider
    if (!EquipmentDataProvider.GetInterface())
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("UnequipItem: No EquipmentDataProvider"));
        return;
    }

    // Get item from slot
    FSuspenseCoreEquipmentSlotData SlotData;
    if (!EquipmentDataProvider->GetSlotData(SlotIndex, SlotData) || !SlotData.EquippedItem.IsValid())
    {
        UE_LOG(LogEquipmentBridge, Warning, TEXT("UnequipItem: No item in slot %d"), SlotIndex);
        return;
    }

    // Create unequip request
    FSuspenseCoreInventoryTransferRequest Request;
    Request.Item = SlotData.EquippedItem;
    Request.SourceSlot = SlotIndex;
    Request.TargetSlot = INDEX_NONE; // Auto-find slot in inventory
    Request.bFromInventory = false;
    Request.bToInventory = true;

    // Execute unequip
    FSuspenseCoreInventorySimpleResult Result = TransferToInventory(Request);

    UE_LOG(LogEquipmentBridge, Warning, TEXT("UnequipItem: Result - Success: %s, Message: %s"),
        Result.bSuccess ? TEXT("YES") : TEXT("NO"),
        *Result.ErrorMessage.ToString());
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
        *Request.ItemInstance.UniqueInstanceID.ToString());
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

            // Broadcast result via EventBus
            FSuspenseCoreEventData ResultData = FSuspenseCoreEventData::Create(this);
            ResultData.SetBool(FName(TEXT("Success")), FailureResult.bSuccess);
            ResultData.SetString(FName(TEXT("OperationId")), FailureResult.OperationId.ToString());
            ResultData.SetString(FName(TEXT("ErrorMessage")), FailureResult.ErrorMessage.ToString());
            EventDelegateManager->PublishEventWithData(
                FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Completed")),
                ResultData
            );
        }
        return;
    }

    // Process operation based on type
    FSuspenseCoreInventorySimpleResult InventoryResult;
    FEquipmentOperationResult EquipmentResult;

    switch (Request.OperationType)
    {
        case EEquipmentOperationType::Equip:
        {
            UE_LOG(LogEquipmentBridge, Log, TEXT("Processing EQUIP operation"));

            FSuspenseCoreInventoryTransferRequest TransferReq;
            TransferReq.Item = Request.ItemInstance;
            TransferReq.TargetSlot = Request.TargetSlotIndex;
            TransferReq.SourceSlot = INDEX_NONE;

            InventoryResult = ExecuteTransfer_FromInventoryToEquip(TransferReq);

            EquipmentResult.bSuccess = InventoryResult.bSuccess;
            EquipmentResult.OperationId = Request.OperationId;
            EquipmentResult.ErrorMessage = FText::FromString(InventoryResult.ErrorMessage);
            EquipmentResult.AffectedSlots.Add(Request.TargetSlotIndex);

            if (InventoryResult.bSuccess)
            {
                // Add affected item
                EquipmentResult.AffectedItems.Add(Request.ItemInstance);
                EquipmentResult.ResultMetadata.Add(TEXT("OperationType"), TEXT("Equip"));

                UE_LOG(LogEquipmentBridge, Log, TEXT("EQUIP operation completed successfully"));
            }
            else
            {
                EquipmentResult.FailureType = EEquipmentValidationFailure::SystemError;
                UE_LOG(LogEquipmentBridge, Error, TEXT("EQUIP operation failed: %s"),
                    *InventoryResult.ErrorMessage);
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
            FSuspenseCoreItemInstance UnequippedItem = ConvertToItemInstance(EquipmentDataProvider->GetSlotItem(Request.SourceSlotIndex));

            UE_LOG(LogEquipmentBridge, Warning, TEXT("Unequipping item: %s from slot %d"),
                *UnequippedItem.ItemID.ToString(), Request.SourceSlotIndex);

            // Create transfer request for the unequip operation
            FSuspenseCoreInventoryTransferRequest TransferReq;
            TransferReq.SourceSlot = Request.SourceSlotIndex;
            TransferReq.TargetSlot = INDEX_NONE; // Let inventory find best slot
            TransferReq.Item = UnequippedItem;

            // Execute the transfer from equipment to inventory
            InventoryResult = ExecuteTransfer_FromEquipToInventory(TransferReq);

            // Map inventory result to equipment result
            EquipmentResult.bSuccess = InventoryResult.bSuccess;
            EquipmentResult.OperationId = Request.OperationId;
            EquipmentResult.ErrorMessage = FText::FromString(InventoryResult.ErrorMessage);
            EquipmentResult.AffectedSlots.Add(Request.SourceSlotIndex);

            if (InventoryResult.bSuccess)
            {
                // Add affected item
                EquipmentResult.AffectedItems.Add(UnequippedItem);
                EquipmentResult.ResultMetadata.Add(TEXT("OperationType"), TEXT("Unequip"));

                UE_LOG(LogEquipmentBridge, Warning, TEXT("UNEQUIP operation completed successfully"));
            }
            else
            {
                EquipmentResult.FailureType = EEquipmentValidationFailure::SystemError;
                UE_LOG(LogEquipmentBridge, Error, TEXT("UNEQUIP operation failed: %s"),
                    *InventoryResult.ErrorMessage);
            }

            break;
        }

        case EEquipmentOperationType::Swap:
        {
            UE_LOG(LogEquipmentBridge, Log, TEXT("Processing SWAP operation"));

            InventoryResult = ExecuteSwap_InventoryToEquipment(
                Request.ItemInstance.UniqueInstanceID,
                Request.TargetSlotIndex
            );

            EquipmentResult.bSuccess = InventoryResult.bSuccess;
            EquipmentResult.OperationId = Request.OperationId;
            EquipmentResult.ErrorMessage = FText::FromString(InventoryResult.ErrorMessage);
            EquipmentResult.AffectedSlots.Add(Request.TargetSlotIndex);

            if (InventoryResult.bSuccess)
            {
                // Add affected item (the one being equipped)
                EquipmentResult.AffectedItems.Add(Request.ItemInstance);
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

            const FSuspenseCoreItemInstance DroppedItem = ConvertToItemInstance(EquipmentDataProvider->ClearSlot(
                Request.TargetSlotIndex, true));

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

    // Broadcast result back through EventBus
    if (EventDelegateManager.IsValid())
    {
        FSuspenseCoreEventData ResultData = FSuspenseCoreEventData::Create(this);
        ResultData.SetBool(FName(TEXT("Success")), EquipmentResult.bSuccess);
        ResultData.SetString(FName(TEXT("OperationId")), EquipmentResult.OperationId.ToString());
        ResultData.SetString(FName(TEXT("ErrorMessage")), EquipmentResult.ErrorMessage.ToString());
        EventDelegateManager->PublishEventWithData(
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Operation.Completed")),
            ResultData
        );

        UE_LOG(LogEquipmentBridge, Warning, TEXT("=== Operation Result Broadcasted ==="));
        UE_LOG(LogEquipmentBridge, Warning, TEXT("Success: %s"), EquipmentResult.bSuccess ? TEXT("YES") : TEXT("NO"));
    }
}

// ===== ExecuteTransfer_FromEquipToInventory - Complete Implementation =====

FSuspenseCoreInventorySimpleResult USuspenseCoreEquipmentInventoryBridge::ExecuteTransfer_FromEquipToInventory(
    const FSuspenseCoreInventoryTransferRequest& Request)
{
    UE_LOG(LogEquipmentBridge, Warning, TEXT("=== ExecuteTransfer_FromEquipToInventory START ==="));
    UE_LOG(LogEquipmentBridge, Warning, TEXT("Source Equipment Slot: %d"), Request.SourceSlot);

    // Validate that all required dependencies are available
    if (!EquipmentDataProvider.GetInterface() || !InventoryInterface.GetInterface())
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("Dependencies not available"));
        return FSuspenseCoreInventorySimpleResult::Failure(
            ESuspenseCoreInventoryResult::NotInitialized,
            TEXT("Bridge not initialized"));
    }

    // Validate that the source slot index is valid
    if (!EquipmentDataProvider->IsValidSlotIndex(Request.SourceSlot))
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("Invalid source slot: %d"), Request.SourceSlot);
        return FSuspenseCoreInventorySimpleResult::Failure(
            ESuspenseCoreInventoryResult::InvalidSlot,
            FString::Printf(TEXT("Invalid equipment slot: %d"), Request.SourceSlot));
    }

    // Get the equipped item from the slot before we start any operations
    FSuspenseCoreItemInstance EquippedItem = ConvertToItemInstance(EquipmentDataProvider->GetSlotItem(Request.SourceSlot));

    // If the slot is already empty, this is not an error, just return success
    if (!EquippedItem.IsValid())
    {
        UE_LOG(LogEquipmentBridge, Warning, TEXT("Slot %d is already empty"), Request.SourceSlot);
        return FSuspenseCoreInventorySimpleResult::Success(Request.SourceSlot);
    }

    UE_LOG(LogEquipmentBridge, Warning, TEXT("Item to transfer: %s (InstanceID: %s)"),
        *EquippedItem.ItemID.ToString(),
        *EquippedItem.UniqueInstanceID.ToString());

    // Check if inventory has space for the item we are about to unequip
    if (!InventoryHasSpace(EquippedItem))
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("No space in inventory"));
        return FSuspenseCoreInventorySimpleResult::Failure(
            ESuspenseCoreInventoryResult::NoSpace,
            TEXT("No space in inventory for unequipped item"));
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
                ClearOp.ItemBefore = ConvertToInventoryItemInstance(EquippedItem);
                ClearOp.ItemAfter = FSuspenseCoreInventoryItemInstance();
                ClearOp.bReversible = true;
                ClearOp.Timestamp = FPlatformTime::Seconds();
                ClearOp.Priority = ETransactionPriority::Normal;
                ClearOp.Metadata.Add(TEXT("Destination"), TEXT("Inventory"));
                ClearOp.Metadata.Add(TEXT("InstanceID"), EquippedItem.UniqueInstanceID.ToString());

                TransactionManager->RegisterOperation(EquipTxnId, ClearOp);
                TransactionManager->ApplyOperation(EquipTxnId, ClearOp);
            }

            // PHASE 1: Clear the equipment slot with notification enabled
            UE_LOG(LogEquipmentBridge, Warning, TEXT("Phase 1: Clearing equipment slot %d..."), Request.SourceSlot);
            BridgeTxn.bEquipmentModified = true;

            // This call will trigger notifications to UIConnector which will update UI
            FSuspenseCoreItemInstance ClearedItem = ConvertToItemInstance(EquipmentDataProvider->ClearSlot(Request.SourceSlot, true));

            if (!ClearedItem.IsValid())
            {
                UE_LOG(LogEquipmentBridge, Error, TEXT("Failed to clear equipment slot!"));
                TransactionManager->RollbackTransaction(EquipTxnId);
                RollbackBridgeTransaction(BridgeTxnId);

                return FSuspenseCoreInventorySimpleResult::Failure(
                    ESuspenseCoreInventoryResult::Unknown,
                    TEXT("Failed to clear equipment slot"));
            }

            UE_LOG(LogEquipmentBridge, Warning, TEXT("Equipment slot cleared successfully"));

            // PHASE 2: Add the item to inventory
            UE_LOG(LogEquipmentBridge, Warning, TEXT("Phase 2: Adding item to inventory..."));
            BridgeTxn.bInventoryModified = true;

            bool bAddSuccess = InventoryInterface->Execute_AddItemInstance(InventoryInterface.GetObject(), EquippedItem);

            if (!bAddSuccess)
            {
                UE_LOG(LogEquipmentBridge, Error, TEXT("Failed to add item to inventory"));

                // Rollback: restore item to equipment slot
                EquipmentDataProvider->SetSlotItem(Request.SourceSlot, ConvertToInventoryItemInstance(EquippedItem), true);
                TransactionManager->RollbackTransaction(EquipTxnId);
                RollbackBridgeTransaction(BridgeTxnId);

                return FSuspenseCoreInventorySimpleResult::Failure(
                    ESuspenseCoreInventoryResult::Unknown,
                    TEXT("Failed to add item to inventory"));
            }

            UE_LOG(LogEquipmentBridge, Warning, TEXT("Item added to inventory successfully"));

            // Validate the entire transaction before committing
            if (!TransactionManager->ValidateTransaction(EquipTxnId))
            {
                UE_LOG(LogEquipmentBridge, Error, TEXT("Transaction validation failed"));

                // Rollback both sides: remove from inventory and restore to equipment
                InventoryInterface->RemoveItemInstance(EquippedItem.UniqueInstanceID);
                EquipmentDataProvider->SetSlotItem(Request.SourceSlot, ConvertToInventoryItemInstance(EquippedItem), true);
                TransactionManager->RollbackTransaction(EquipTxnId);
                RollbackBridgeTransaction(BridgeTxnId);

                return FSuspenseCoreInventorySimpleResult::Failure(
                    ESuspenseCoreInventoryResult::Unknown,
                    TEXT("Transaction validation failed"));
            }

            // Commit the transaction to make changes permanent
            if (!TransactionManager->CommitTransaction(EquipTxnId))
            {
                UE_LOG(LogEquipmentBridge, Error, TEXT("Transaction commit failed"));
                RollbackBridgeTransaction(BridgeTxnId);

                return FSuspenseCoreInventorySimpleResult::Failure(
                    ESuspenseCoreInventoryResult::Unknown,
                    TEXT("Failed to commit transaction"));
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
        FSuspenseCoreItemInstance ClearedItem = ConvertToItemInstance(EquipmentDataProvider->ClearSlot(Request.SourceSlot, true));

        if (ClearedItem.IsValid())
        {
            // Add to inventory
            BridgeTxn.bInventoryModified = true;
            bool bAddSuccess = InventoryInterface->Execute_AddItemInstance(InventoryInterface.GetObject(), EquippedItem);

            if (bAddSuccess)
            {
                bOperationSuccess = true;
            }
            else
            {
                // Rollback: restore to equipment
                EquipmentDataProvider->SetSlotItem(Request.SourceSlot, ConvertToInventoryItemInstance(EquippedItem), true);
                RollbackBridgeTransaction(BridgeTxnId);
                return FSuspenseCoreInventorySimpleResult::Failure(
                    ESuspenseCoreInventoryResult::Unknown,
                    TEXT("Failed to add item to inventory"));
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
        return FSuspenseCoreInventorySimpleResult::Failure(
            ESuspenseCoreInventoryResult::Unknown,
            TEXT("Failed to unequip item"));
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
            FSuspenseCoreEquipmentSlotConfig Config = EquipmentDataProvider->GetSlotConfiguration(Request.SourceSlot);
            if (Config.IsValid())
            {
                SlotType = Config.SlotTag;
            }
        }

        UE_LOG(LogEquipmentBridge, Warning,
            TEXT("Notifying EventDelegateManager: Slot %d cleared (Type: %s)"),
            Request.SourceSlot, *SlotType.ToString());

        // Notify that the slot is now empty via EventBus
        FSuspenseCoreEventData SlotData = FSuspenseCoreEventData::Create(this);
        SlotData.SetFloat(FName(TEXT("SlotIndex")), static_cast<float>(Request.SourceSlot));
        SlotData.SetString(FName(TEXT("SlotType")), SlotType.ToString());
        SlotData.SetBool(FName(TEXT("Occupied")), false);
        EventDelegateManager->PublishEventWithData(
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Updated")),
            SlotData
        );
        EventDelegateManager->PublishEvent(
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Updated")),
            this
        );
    }

    UE_LOG(LogEquipmentBridge, Warning, TEXT("=== UNEQUIP SUCCESSFUL ==="));
    UE_LOG(LogEquipmentBridge, Warning, TEXT("Item %s moved from slot %d to inventory"),
        *EquippedItem.ItemID.ToString(), Request.SourceSlot);
    UE_LOG(LogEquipmentBridge, Warning, TEXT("=== ExecuteTransfer_FromEquipToInventory END ==="));

    // Create success result - FSuspenseCoreInventorySimpleResult stores single item info
    FSuspenseCoreInventorySimpleResult Result = FSuspenseCoreInventorySimpleResult::Success(
        Request.SourceSlot, EquippedItem.Quantity, EquippedItem.UniqueInstanceID);

    return Result;
}


// ===== Public Transfer Operations =====

FSuspenseCoreInventorySimpleResult USuspenseCoreEquipmentInventoryBridge::TransferFromInventory(const FSuspenseCoreInventoryTransferRequest& Request)
{
    return ExecuteTransfer_FromInventoryToEquip(Request);
}

FSuspenseCoreInventorySimpleResult USuspenseCoreEquipmentInventoryBridge::TransferToInventory(const FSuspenseCoreInventoryTransferRequest& Request)
{
    return ExecuteTransfer_FromEquipToInventory(Request);
}

FSuspenseCoreInventorySimpleResult USuspenseCoreEquipmentInventoryBridge::SwapBetweenInventoryAndEquipment(
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
    const TArray<FSuspenseCoreItemInstance> InvenItems = InventoryInterface->GetAllItemInstances();

    TSet<FGuid> InventoryInstanceIds;
    InventoryInstanceIds.Reserve(InvenItems.Num());
    for (const FSuspenseCoreItemInstance& Ii : InvenItems)
    {
        InventoryInstanceIds.Add(Ii.UniqueInstanceID);
    }

    for (const TPair<int32, FSuspenseCoreInventoryItemInstance>& SlotIt : Equipped)
    {
        const int32 SlotIdx = SlotIt.Key;
        const FSuspenseCoreInventoryItemInstance& EquippedItem = SlotIt.Value;

        if (EquippedItem.IsValid() && !InventoryInstanceIds.Contains(EquippedItem.InstanceID))
        {
            FSuspenseCoreItemInstance Found;
            if (FindItemInInventory(EquippedItem.ItemID, Found))
            {
                EquipmentDataProvider->SetSlotItem(SlotIdx, ConvertToInventoryItemInstance(Found), true);
            }
        }
    }
}

// ===== Validation Helpers =====

bool USuspenseCoreEquipmentInventoryBridge::CanEquipFromInventory(const FSuspenseCoreItemInstance& Item, int32 TargetSlot) const
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
        FSlotValidationResult ValidationResult = Executor->CanEquipItemToSlot(ConvertToInventoryItemInstance(Item), TargetSlot);
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

    FSuspenseCoreItemInstance EquippedItem = ConvertToItemInstance(EquipmentDataProvider->GetSlotItem(SourceSlot));
    return InventoryHasSpace(EquippedItem);
}

// ===== Private: Transfer Implementations =====

FSuspenseCoreInventorySimpleResult USuspenseCoreEquipmentInventoryBridge::ExecuteTransfer_FromInventoryToEquip(
    const FSuspenseCoreInventoryTransferRequest& Request)
{
    const FSuspenseCoreItemInstance& Item = Request.Item;

    UE_LOG(LogEquipmentBridge, Warning, TEXT("=== ExecuteTransfer_FromInventoryToEquip START ==="));
    UE_LOG(LogEquipmentBridge, Warning, TEXT("ItemID: %s"), *Item.ItemID.ToString());
    UE_LOG(LogEquipmentBridge, Warning, TEXT("InstanceID: %s"), *Item.UniqueInstanceID.ToString());
    UE_LOG(LogEquipmentBridge, Warning, TEXT("Quantity: %d"), Item.Quantity);
    UE_LOG(LogEquipmentBridge, Warning, TEXT("Target Slot: %d"), Request.TargetSlot);

    // Step 1: Validate item exists in inventory
    if (!ISuspenseCoreInventory::Execute_HasItem(InventoryInterface.GetObject(), Item.ItemID, FMath::Max(1, Item.Quantity)))
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("Item %s not found in inventory"), *Item.ItemID.ToString());
        return FSuspenseCoreInventorySimpleResult::Failure(
            ESuspenseCoreInventoryResult::ItemNotFound,
            FString::Printf(TEXT("Item %s not found in inventory"), *Item.ItemID.ToString()));
    }

    UE_LOG(LogEquipmentBridge, Log, TEXT("✓ Item exists in inventory"));

    // Step 2: Pre-validation - Check slot validity
    if (!EquipmentDataProvider->IsValidSlotIndex(Request.TargetSlot))
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("Invalid target slot index: %d"), Request.TargetSlot);
        return FSuspenseCoreInventorySimpleResult::Failure(
            ESuspenseCoreInventoryResult::InvalidSlot,
            FString::Printf(TEXT("Invalid equipment slot: %d"), Request.TargetSlot));
    }

    // Step 2b: Check if slot is occupied and auto-SWAP
    if (EquipmentDataProvider->IsSlotOccupied(Request.TargetSlot))
    {
        const FSuspenseCoreItemInstance OccupiedItem = ConvertToItemInstance(EquipmentDataProvider->GetSlotItem(Request.TargetSlot));

        UE_LOG(LogEquipmentBridge, Warning,
            TEXT("Target slot %d is occupied by %s - checking if SWAP is possible"),
            Request.TargetSlot, *OccupiedItem.ItemID.ToString());

        // Automatic SWAP logic
        if (EquipmentOperations.GetInterface())
        {
            if (USuspenseCoreEquipmentOperationExecutor* Executor =
                Cast<USuspenseCoreEquipmentOperationExecutor>(EquipmentOperations.GetObject()))
            {
                FSlotValidationResult NewItemValidation = Executor->CanEquipItemToSlot(ConvertToInventoryItemInstance(Item), Request.TargetSlot);

                const FString ErrStr = NewItemValidation.ErrorMessage.ToString();
                const bool bOnlyOccupiedReason =
                    !NewItemValidation.bIsValid &&
                    (ErrStr.Contains(TEXT("occupied"), ESearchCase::IgnoreCase) ||
                     ErrStr.Contains(TEXT("занят"),    ESearchCase::IgnoreCase));

                if (NewItemValidation.bIsValid || bOnlyOccupiedReason)
                {
                    UE_LOG(LogEquipmentBridge, Warning,
                        TEXT("✓ Items are swappable - executing automatic SWAP operation"));

                    return ExecuteSwap_InventoryToEquipment(Item.UniqueInstanceID, Request.TargetSlot);
                }
                else
                {
                    UE_LOG(LogEquipmentBridge, Warning,
                        TEXT("✗ New item %s is not compatible with slot %d: %s"),
                        *Item.ItemID.ToString(),
                        Request.TargetSlot,
                        *NewItemValidation.ErrorMessage.ToString());

                    return FSuspenseCoreInventorySimpleResult::Failure(
                        ESuspenseCoreInventoryResult::InvalidItem,
                        FString::Printf(TEXT("Cannot equip %s to slot %d: %s"),
                            *Item.ItemID.ToString(),
                            Request.TargetSlot,
                            *NewItemValidation.ErrorMessage.ToString()));
                }
            }
        }

        return FSuspenseCoreInventorySimpleResult::Failure(
            ESuspenseCoreInventoryResult::SlotOccupied,
            FString::Printf(TEXT("Equipment slot %d is occupied. Unequip it first."), Request.TargetSlot));
    }

    UE_LOG(LogEquipmentBridge, Log, TEXT("✓ Target slot %d is empty"), Request.TargetSlot);

    // Step 2c: Validate compatibility with empty slot
    if (EquipmentOperations.GetInterface())
    {
        if (USuspenseCoreEquipmentOperationExecutor* Executor =
            Cast<USuspenseCoreEquipmentOperationExecutor>(EquipmentOperations.GetObject()))
        {
            FSlotValidationResult ValidationResult = Executor->CanEquipItemToSlot(ConvertToInventoryItemInstance(Item), Request.TargetSlot);

            if (!ValidationResult.bIsValid)
            {
                UE_LOG(LogEquipmentBridge, Warning,
                    TEXT("Item %s incompatible with slot %d: %s"),
                    *Item.ItemID.ToString(), Request.TargetSlot, *ValidationResult.ErrorMessage.ToString());

                return FSuspenseCoreInventorySimpleResult::Failure(
                    ESuspenseCoreInventoryResult::InvalidItem,
                    ValidationResult.ErrorMessage.ToString());
            }

            UE_LOG(LogEquipmentBridge, Log, TEXT("✓ Item passed compatibility validation"));
        }
    }

    // Step 3: Remove from inventory
    UE_LOG(LogEquipmentBridge, Warning, TEXT("Removing item from inventory (InstanceID: %s)"), *Item.UniqueInstanceID.ToString());

    const bool bRemoveSuccess = InventoryInterface->RemoveItemInstance(Item.UniqueInstanceID);
    if (!bRemoveSuccess)
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("Failed to remove item from inventory"));
        return FSuspenseCoreInventorySimpleResult::Failure(
            ESuspenseCoreInventoryResult::Unknown,
            FString::Printf(TEXT("Failed to remove item %s from inventory"), *Item.ItemID.ToString()));
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
                EquipOp.ItemAfter = ConvertToInventoryItemInstance(Item);
                EquipOp.bReversible = true;
                EquipOp.Timestamp = FPlatformTime::Seconds();
                EquipOp.Priority = ETransactionPriority::Normal;
                EquipOp.Metadata.Add(TEXT("Source"), TEXT("Inventory"));
                EquipOp.Metadata.Add(TEXT("InstanceID"), Item.UniqueInstanceID.ToString());

                TransactionManager->RegisterOperation(TxnId, EquipOp);
                TransactionManager->ApplyOperation(TxnId, EquipOp);
            }

            UE_LOG(LogEquipmentBridge, Warning, TEXT("Writing item to equipment slot %d..."), Request.TargetSlot);

            // CRITICAL: Pass TRUE to broadcast notifications
            bEquipSuccess = EquipmentDataProvider->SetSlotItem(Request.TargetSlot, ConvertToInventoryItemInstance(Item), true);

            if (!bEquipSuccess)
            {
                UE_LOG(LogEquipmentBridge, Error, TEXT("✗ SetSlotItem FAILED"));

                TransactionManager->RollbackTransaction(TxnId);
                ISuspenseCoreInventory::Execute_AddItemInstance(InventoryInterface.GetObject(), Item);

                return FSuspenseCoreInventorySimpleResult::Failure(
                    ESuspenseCoreInventoryResult::Unknown,
                    TEXT("Failed to write item to equipment slot"));
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
                    ISuspenseCoreInventory::Execute_AddItemInstance(InventoryInterface.GetObject(), Item);
                    bEquipSuccess = false;
                }
            }
            else
            {
                UE_LOG(LogEquipmentBridge, Error, TEXT("✗ Transaction validation failed!"));
                EquipmentDataProvider->ClearSlot(Request.TargetSlot, true);
                TransactionManager->RollbackTransaction(TxnId);
                ISuspenseCoreInventory::Execute_AddItemInstance(InventoryInterface.GetObject(), Item);
                bEquipSuccess = false;
            }
        }
        else
        {
            UE_LOG(LogEquipmentBridge, Error, TEXT("✗ Failed to begin transaction!"));
            ISuspenseCoreInventory::Execute_AddItemInstance(InventoryInterface.GetObject(), Item);
            bEquipSuccess = false;
        }
    }
    else
    {
        UE_LOG(LogEquipmentBridge, Warning, TEXT("NO TransactionManager - using direct write"));
        // CRITICAL: Pass TRUE to broadcast notifications
        bEquipSuccess = EquipmentDataProvider->SetSlotItem(Request.TargetSlot, ConvertToInventoryItemInstance(Item), true);

        if (!bEquipSuccess)
        {
            ISuspenseCoreInventory::Execute_AddItemInstance(InventoryInterface.GetObject(), Item);
        }
    }

    // Step 5: Process result
    if (!bEquipSuccess)
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("=== EQUIP FAILED ==="));
        return FSuspenseCoreInventorySimpleResult::Failure(
            ESuspenseCoreInventoryResult::Unknown,
            TEXT("Failed to equip item"));
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
            FSuspenseCoreEquipmentSlotConfig Config = EquipmentDataProvider->GetSlotConfiguration(Request.TargetSlot);
            if (Config.IsValid())
            {
                SlotType = Config.SlotTag;
            }
        }

        UE_LOG(LogEquipmentBridge, Warning,
            TEXT("Notifying EventBus: Slot %d updated (Type: %s, Occupied: YES)"),
            Request.TargetSlot, *SlotType.ToString());

        FSuspenseCoreEventData SlotData = FSuspenseCoreEventData::Create(this);
        SlotData.SetFloat(FName(TEXT("SlotIndex")), static_cast<float>(Request.TargetSlot));
        SlotData.SetString(FName(TEXT("SlotType")), SlotType.ToString());
        SlotData.SetBool(FName(TEXT("Occupied")), true);
        EventDelegateManager->PublishEventWithData(
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Updated")),
            SlotData
        );
        EventDelegateManager->PublishEvent(
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Updated")),
            this
        );
    }

    UE_LOG(LogEquipmentBridge, Warning, TEXT("✓✓✓ TRANSFER SUCCESSFUL ✓✓✓"));
    UE_LOG(LogEquipmentBridge, Warning, TEXT("Item %s equipped to slot %d"), *Item.ItemID.ToString(), Request.TargetSlot);
    UE_LOG(LogEquipmentBridge, Warning, TEXT("=== ExecuteTransfer_FromInventoryToEquip END ==="));

    FSuspenseCoreInventorySimpleResult SuccessResult = FSuspenseCoreInventorySimpleResult::Success(Request.TargetSlot, Item.Quantity, Item.UniqueInstanceID);
    return SuccessResult;
}

void USuspenseCoreEquipmentInventoryBridge::BroadcastUnequippedEvent(
    const FSuspenseCoreItemInstance& Item,
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

    // Use EventBus via EventDelegateManager
    if (EventDelegateManager.IsValid())
    {
        FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
        EventData.SetObject(FName(TEXT("Target")), TargetActor);
        EventData.SetInt(FName(TEXT("Slot")), SlotIndex);
        EventData.SetString(FName(TEXT("ItemID")), Item.ItemID.ToString());
        EventData.SetString(FName(TEXT("InstanceID")), Item.UniqueInstanceID.ToString());

        UE_LOG(LogEquipmentBridge, Warning, TEXT("Broadcasting Equipment.Event.Unequipped"));
        UE_LOG(LogEquipmentBridge, Warning, TEXT("  Target: %s, Slot: %d"),
            *TargetActor->GetName(), SlotIndex);

        EventDelegateManager->PublishEventWithData(UnequippedTag, EventData);
        UE_LOG(LogEquipmentBridge, Warning, TEXT("Event broadcast successful"));
    }
    else
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("EventDelegateManager not available!"));
    }

    UE_LOG(LogEquipmentBridge, Warning, TEXT("=== BroadcastUnequippedEvent END ==="));
}
FSuspenseCoreInventorySimpleResult USuspenseCoreEquipmentInventoryBridge::ExecuteSwap_InventoryToEquipment(
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
        return FSuspenseCoreInventorySimpleResult::Failure(
            ESuspenseCoreInventoryResult::NotInitialized,
            TEXT("Bridge not initialized"));
    }

    // Get items from both sides
    FSuspenseCoreItemInstance InventoryItem;
    bool bFoundInInventory = false;

    const TArray<FSuspenseCoreItemInstance> AllItems = InventoryInterface->GetAllItemInstances();
    for (const FSuspenseCoreItemInstance& Item : AllItems)
    {
        if (Item.UniqueInstanceID == InventoryInstanceID)
        {
            InventoryItem = Item;
            bFoundInInventory = true;
            Transaction.InventoryBackup = Item;
            Transaction.InventorySlot = INDEX_NONE; // FSuspenseCoreItemInstance doesn't have AnchorIndex
            break;
        }
    }

    if (!bFoundInInventory)
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("✗ Item not found in inventory!"));
        RollbackBridgeTransaction(TransactionID);
        return FSuspenseCoreInventorySimpleResult::Failure(
            ESuspenseCoreInventoryResult::ItemNotFound,
            FString::Printf(TEXT("Item with InstanceID %s not found in inventory"), *InventoryInstanceID.ToString()));
    }

    UE_LOG(LogEquipmentBridge, Log, TEXT("✓ Found inventory item: %s"), *InventoryItem.ItemID.ToString());

    FSuspenseCoreItemInstance EquippedItem;
    if (EquipmentDataProvider->IsSlotOccupied(EquipmentSlot))
    {
        EquippedItem = ConvertToItemInstance(EquipmentDataProvider->GetSlotItem(EquipmentSlot));
        Transaction.EquipmentBackup = EquippedItem;
        UE_LOG(LogEquipmentBridge, Log, TEXT("✓ Slot %d occupied by: %s"), EquipmentSlot, *EquippedItem.ItemID.ToString());
    }
    Transaction.EquipmentSlot = EquipmentSlot;

    // Check inventory space for unequipped item
    if (EquippedItem.IsValid() && !InventoryHasSpace(EquippedItem))
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("✗ No space in inventory"));
        RollbackBridgeTransaction(TransactionID);
        return FSuspenseCoreInventorySimpleResult::Failure(
            ESuspenseCoreInventoryResult::NoSpace,
            TEXT("No space in inventory for unequipped item"));
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
                UnequipOp.ItemBefore = ConvertToInventoryItemInstance(EquippedItem);
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
            EquipOp.ItemBefore = EquippedItem.IsValid() ? ConvertToInventoryItemInstance(EquippedItem) : FSuspenseCoreInventoryItemInstance();
            EquipOp.ItemAfter = ConvertToInventoryItemInstance(InventoryItem);
            EquipOp.bReversible = true;
            EquipOp.Timestamp = CurrentTime + 0.001f;
            EquipOp.Priority = ETransactionPriority::Normal;
            EquipOp.Metadata.Add(TEXT("SourceInventoryInstance"), InventoryInstanceID.ToString());

            if (EquippedItem.IsValid())
            {
                EquipOp.Metadata.Add(TEXT("OperationContext"), TEXT("Swap"));
                EquipOp.SecondaryItemBefore = ConvertToInventoryItemInstance(EquippedItem);
                EquipOp.SecondaryItemAfter = FSuspenseCoreInventoryItemInstance();
            }

            TransactionManager->RegisterOperation(EquipTransID, EquipOp);
            TransactionManager->ApplyOperation(EquipTransID, EquipOp);
        }

        // Phase 1: Remove from inventory
        UE_LOG(LogEquipmentBridge, Warning, TEXT("Phase 1: Removing %s from inventory"), *InventoryItem.ItemID.ToString());
        Transaction.bInventoryModified = true;
        const bool bRemoveSuccess = InventoryInterface->RemoveItemInstance(InventoryInstanceID);

        if (!bRemoveSuccess)
        {
            UE_LOG(LogEquipmentBridge, Error, TEXT("✗ Failed to remove item from inventory"));
            TransactionManager->RollbackTransaction(EquipTransID);
            RollbackBridgeTransaction(TransactionID);
            return FSuspenseCoreInventorySimpleResult::Failure(
                ESuspenseCoreInventoryResult::Unknown,
                FString::Printf(TEXT("Failed to remove item %s from inventory"), *InventoryItem.ItemID.ToString()));
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
        if (!EquipmentDataProvider->SetSlotItem(EquipmentSlot, ConvertToInventoryItemInstance(InventoryItem), true))
        {
            UE_LOG(LogEquipmentBridge, Error, TEXT("✗ Failed to write item to equipment slot!"));
            TransactionManager->RollbackTransaction(EquipTransID);
            RollbackBridgeTransaction(TransactionID);

            return FSuspenseCoreInventorySimpleResult::Failure(
                ESuspenseCoreInventoryResult::Unknown,
                TEXT("Failed to equip item"));
        }

        // Phase 4: Add previously equipped item to inventory (if any)
        if (EquippedItem.IsValid())
        {
            UE_LOG(LogEquipmentBridge, Warning, TEXT("Phase 4: Adding %s back to inventory"),
                *EquippedItem.ItemID.ToString());

            const bool bAddSuccess = InventoryInterface->Execute_AddItemInstance(InventoryInterface.GetObject(), EquippedItem);

            if (!bAddSuccess)
            {
                UE_LOG(LogEquipmentBridge, Error, TEXT("✗ Failed to add unequipped item to inventory"));
                TransactionManager->RollbackTransaction(EquipTransID);
                RollbackBridgeTransaction(TransactionID);
                return FSuspenseCoreInventorySimpleResult::Failure(
                    ESuspenseCoreInventoryResult::Unknown,
                    FString::Printf(TEXT("Failed to add item %s to inventory"), *EquippedItem.ItemID.ToString()));
            }
        }

        // Validate and commit
        if (!TransactionManager->ValidateTransaction(EquipTransID))
        {
            UE_LOG(LogEquipmentBridge, Error, TEXT("✗ Transaction validation failed"));
            TransactionManager->RollbackTransaction(EquipTransID);
            RollbackBridgeTransaction(TransactionID);

            return FSuspenseCoreInventorySimpleResult::Failure(
                ESuspenseCoreInventoryResult::Unknown,
                TEXT("Transaction validation failed"));
        }

        if (!TransactionManager->CommitTransaction(EquipTransID))
        {
            UE_LOG(LogEquipmentBridge, Error, TEXT("✗ Transaction commit failed"));
            RollbackBridgeTransaction(TransactionID);
            return FSuspenseCoreInventorySimpleResult::Failure(
                ESuspenseCoreInventoryResult::Unknown,
                TEXT("Failed to commit transaction"));
        }

        UE_LOG(LogEquipmentBridge, Warning, TEXT("✓ Transaction committed successfully"));
    }
    else
    {
        // Fallback without transaction manager
        UE_LOG(LogEquipmentBridge, Warning, TEXT("⚠ No TransactionManager - using direct operations"));

        const bool bRemoveSuccess = InventoryInterface->RemoveItemInstance(InventoryInstanceID);
        if (!bRemoveSuccess)
        {
            RollbackBridgeTransaction(TransactionID);
            return FSuspenseCoreInventorySimpleResult::Failure(
                ESuspenseCoreInventoryResult::Unknown,
                FString::Printf(TEXT("Failed to remove item from inventory")));
        }
        Transaction.bInventoryModified = true;

        if (EquippedItem.IsValid())
        {
            EquipmentDataProvider->ClearSlot(EquipmentSlot, true);
            Transaction.bEquipmentModified = true;
        }

        if (!EquipmentDataProvider->SetSlotItem(EquipmentSlot, ConvertToInventoryItemInstance(InventoryItem), true))
        {
            RollbackBridgeTransaction(TransactionID);
            return FSuspenseCoreInventorySimpleResult::Failure(
                ESuspenseCoreInventoryResult::Unknown,
                TEXT("Failed to equip item"));
        }

        if (EquippedItem.IsValid())
        {
            const bool bAddSuccess = InventoryInterface->Execute_AddItemInstance(InventoryInterface.GetObject(), EquippedItem);
            if (!bAddSuccess)
            {
                RollbackBridgeTransaction(TransactionID);
                return FSuspenseCoreInventorySimpleResult::Failure(
                    ESuspenseCoreInventoryResult::Unknown,
                    FString::Printf(TEXT("Failed to add item %s to inventory"), *EquippedItem.ItemID.ToString()));
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
            FSuspenseCoreEquipmentSlotConfig Config = EquipmentDataProvider->GetSlotConfiguration(EquipmentSlot);
            if (Config.IsValid())
            {
                SlotType = Config.SlotTag;
            }
        }

        UE_LOG(LogEquipmentBridge, Warning,
            TEXT("Notifying EventBus: Slot %d swapped (Type: %s)"),
            EquipmentSlot, *SlotType.ToString());

        FSuspenseCoreEventData SlotData = FSuspenseCoreEventData::Create(this);
        SlotData.SetFloat(FName(TEXT("SlotIndex")), static_cast<float>(EquipmentSlot));
        SlotData.SetString(FName(TEXT("SlotType")), SlotType.ToString());
        SlotData.SetBool(FName(TEXT("Occupied")), true);
        EventDelegateManager->PublishEventWithData(
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Updated")),
            SlotData
        );
        EventDelegateManager->PublishEvent(
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Updated")),
            this
        );
    }

    UE_LOG(LogEquipmentBridge, Warning, TEXT("✓✓✓ SWAP COMPLETED SUCCESSFULLY ✓✓✓"));
    UE_LOG(LogEquipmentBridge, Warning, TEXT("  IN: %s → Slot %d"), *InventoryItem.ItemID.ToString(), EquipmentSlot);
    if (EquippedItem.IsValid())
    {
        UE_LOG(LogEquipmentBridge, Warning, TEXT("  OUT: %s → Inventory"), *EquippedItem.ItemID.ToString());
    }
    UE_LOG(LogEquipmentBridge, Warning, TEXT("=== SWAP END ==="));

    FSuspenseCoreInventorySimpleResult Result = FSuspenseCoreInventorySimpleResult::Success(EquipmentSlot, InventoryItem.Quantity, InventoryItem.UniqueInstanceID);

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
                ConvertToInventoryItemInstance(Transaction->EquipmentBackup),
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
            ISuspenseCoreInventory::Execute_AddItemInstance(InventoryInterface.GetObject(), Transaction->InventoryBackup);
        }
    }

    ActiveTransactions.Remove(TransactionID);
    return true;
}

// ===== Validation Utilities =====

bool USuspenseCoreEquipmentInventoryBridge::ValidateInventorySpace(const FSuspenseCoreItemInstance& Item) const
{
    if (!InventoryInterface.GetInterface())
    {
        return false;
    }

    return InventoryHasSpace(Item);
}

bool USuspenseCoreEquipmentInventoryBridge::ValidateEquipmentSlot(int32 SlotIndex, const FSuspenseCoreItemInstance& Item) const
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

bool USuspenseCoreEquipmentInventoryBridge::InventoryHasSpace(const FSuspenseCoreItemInstance& Item) const
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

bool USuspenseCoreEquipmentInventoryBridge::FindItemInInventory(const FName& ItemId, FSuspenseCoreItemInstance& OutInstance) const
{
    if (!InventoryInterface.GetInterface())
    {
        return false;
    }

    const TArray<FSuspenseCoreItemInstance> Items = InventoryInterface->GetAllItemInstances();
    for (const FSuspenseCoreItemInstance& It : Items)
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

void USuspenseCoreEquipmentInventoryBridge::BroadcastEquippedEvent(const FSuspenseCoreItemInstance& Item, int32 SlotIndex)
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

    // Use EventBus via EventDelegateManager
    if (EventDelegateManager.IsValid())
    {
        FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
        EventData.SetObject(FName(TEXT("Target")), TargetActor);
        EventData.SetInt(FName(TEXT("Slot")), SlotIndex);
        EventData.SetString(FName(TEXT("ItemID")), Item.ItemID.ToString());
        EventData.SetString(FName(TEXT("InstanceID")), Item.UniqueInstanceID.ToString());
        EventData.SetInt(FName(TEXT("Quantity")), Item.Quantity);

        UE_LOG(LogEquipmentBridge, Warning, TEXT("Broadcasting Equipment.Event.Equipped"));
        UE_LOG(LogEquipmentBridge, Warning, TEXT("  Target: %s"), *TargetActor->GetName());
        UE_LOG(LogEquipmentBridge, Warning, TEXT("  Slot: %d, ItemID: %s"), SlotIndex, *Item.ItemID.ToString());

        EventDelegateManager->PublishEventWithData(EquippedTag, EventData);
        UE_LOG(LogEquipmentBridge, Warning, TEXT("Event broadcast successful"));
    }
    else
    {
        UE_LOG(LogEquipmentBridge, Error, TEXT("EventDelegateManager not available!"));
    }

    UE_LOG(LogEquipmentBridge, Warning, TEXT("=== BroadcastEquippedEvent END ==="));
}

void USuspenseCoreEquipmentInventoryBridge::BroadcastSwapEvents(
    const FSuspenseCoreItemInstance& NewItem,
    const FSuspenseCoreItemInstance& OldItem,
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

        if (UnequippedTag.IsValid() && EventDelegateManager.IsValid())
        {
            FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
            EventData.SetObject(FName(TEXT("Target")), TargetActor);
            EventData.SetInt(FName(TEXT("Slot")), SlotIndex);
            EventData.SetString(FName(TEXT("ItemID")), OldItem.ItemID.ToString());
            EventData.SetString(FName(TEXT("InstanceID")), OldItem.UniqueInstanceID.ToString());

            EventDelegateManager->PublishEventWithData(UnequippedTag, EventData);
            UE_LOG(LogEquipmentBridge, Log, TEXT("[EquipmentBridge] Broadcasted Unequipped event for %s"), *OldItem.ItemID.ToString());
        }
    }

    // Broadcast Equipped event for new item
    BroadcastEquippedEvent(NewItem, SlotIndex);
}
