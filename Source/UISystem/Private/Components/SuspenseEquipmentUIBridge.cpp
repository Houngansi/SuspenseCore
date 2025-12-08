// Copyright Suspense Team Team

#include "Components/SuspenseEquipmentUIBridge.h"

#include "Components/SuspenseUIManager.h"
#include "Delegates/SuspenseEventManager.h"
#include "ItemSystem/SuspenseItemManager.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceLocator.h"

#include "GameplayTagContainer.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogEquipmentUIBridge, Log, All);

// ===== Construction =====

USuspenseEquipmentUIBridge::USuspenseEquipmentUIBridge() {}

void USuspenseEquipmentUIBridge::Initialize(APlayerController* InPC)
{
    OwningPlayerController = InPC;
    if (!OwningPlayerController)
    {
        UE_LOG(LogEquipmentUIBridge, Warning, TEXT("Initialize: OwningPlayerController is null"));
        return;
    }

    // Get subsystems
    if (UGameInstance* GI = OwningPlayerController->GetGameInstance())
    {
        UIManager = GI->GetSubsystem<USuspenseUIManager>();
        EventManager = GI->GetSubsystem<USuspenseEventManager>();
        CachedItemManager = GI->GetSubsystem<USuspenseItemManager>();
    }

    // Register as global bridge instance
    RegisterBridge(this);

    UE_LOG(LogEquipmentUIBridge, Log, TEXT("UIBridge initialized - waiting for DataStore binding"));
}

void USuspenseEquipmentUIBridge::Shutdown()
{
    UE_LOG(LogEquipmentUIBridge, Log, TEXT("=== Shutdown START ==="));

    // Unsubscribe from DataStore
    // NOTE: We cannot access DataStore here as we don't store reference to it
    // The DataStore itself will clean up delegate handles in its destructor
    // This is intentional - Bridge doesn't own DataStore reference

    if (DataStoreSlotChangedHandle.IsValid())
    {
        UE_LOG(LogEquipmentUIBridge, Warning,
            TEXT("DataStore subscription handle still valid - DataStore should clean this up"));
        DataStoreSlotChangedHandle.Reset();
    }

    if (DataStoreResetHandle.IsValid())
    {
        DataStoreResetHandle.Reset();
    }

    // Clear coalescing timer
    if (UWorld* World = OwningPlayerController ? OwningPlayerController->GetWorld() : nullptr)
    {
        if (World->GetTimerManager().IsTimerActive(CoalesceTimerHandle))
        {
            World->GetTimerManager().ClearTimer(CoalesceTimerHandle);
        }
    }

    // Clear pending updates
    PendingSlotUpdates.Empty();

    // Unregister global instance
    UnregisterBridge(this);

    // Clear cached data
    CachedConfigs.Empty();
    CachedUIData.Empty();
    CachedItems.Empty();
    bHasSnapshot = false;

    // Clear service references
    UIManager = nullptr;
    EventManager = nullptr;
    CachedItemManager = nullptr;
    Operations = nullptr;
    bVisible = false;

    UE_LOG(LogEquipmentUIBridge, Log, TEXT("=== Shutdown END ==="));
}

// ===== NEW: Direct DataStore Binding =====

void USuspenseEquipmentUIBridge::BindToDataStore(const TScriptInterface<ISuspenseEquipmentDataProvider>& DataStore)
{
    UE_LOG(LogEquipmentUIBridge, Warning, TEXT("=== BindToDataStore START ==="));

    // Extract interface pointer from TScriptInterface
    ISuspenseEquipmentDataProvider* DataStoreInterface = DataStore.GetInterface();

    if (!DataStoreInterface)
    {
        UE_LOG(LogEquipmentUIBridge, Error, TEXT("BindToDataStore: DataStore interface is NULL!"));
        return;
    }

    // Unsubscribe from previous DataStore if any
    if (DataStoreSlotChangedHandle.IsValid())
    {
        UE_LOG(LogEquipmentUIBridge, Warning, TEXT("Removing previous DataStore subscription"));
        DataStoreSlotChangedHandle.Reset();
    }

    // CRITICAL: Subscribe directly to DataStore slot changed event
    DataStoreSlotChangedHandle = DataStoreInterface->OnSlotDataChanged().AddUObject(
        this, &USuspenseEquipmentUIBridge::HandleDataStoreSlotChanged
    );

    if (!DataStoreSlotChangedHandle.IsValid())
    {
        UE_LOG(LogEquipmentUIBridge, Error, TEXT("Failed to subscribe to DataStore!"));
        return;
    }

    // Subscribe to reset event for full cache rebuilds
    DataStoreResetHandle = DataStoreInterface->OnDataStoreReset().AddUObject(
        this, &USuspenseEquipmentUIBridge::HandleDataStoreReset
    );

    UE_LOG(LogEquipmentUIBridge, Warning, TEXT("✓ Subscribed to DataStore slot changed events"));

    // Get initial slot configurations (these rarely change)
    CachedConfigs = DataStoreInterface->GetAllSlotConfigurations();
    UE_LOG(LogEquipmentUIBridge, Log, TEXT("Cached %d slot configurations"), CachedConfigs.Num());

    // Get initial equipped items and build cache
    const TMap<int32, FSuspenseInventoryItemInstance> AllItems = DataStoreInterface->GetAllEquippedItems();

    // Resize UI cache to match slot count
    CachedUIData.SetNum(CachedConfigs.Num());

    // Initialize each slot in cache
    for (int32 i = 0; i < CachedConfigs.Num(); ++i)
    {
        const FSuspenseInventoryItemInstance* FoundItem = AllItems.Find(i);
        const FSuspenseInventoryItemInstance ItemInstance = FoundItem ? *FoundItem : FSuspenseInventoryItemInstance();

        UpdateCachedSlot(i, ItemInstance);

        // Also update legacy map for backward compatibility
        if (ItemInstance.IsValid())
        {
            CachedItems.Add(i, ItemInstance);
        }
    }

    bHasSnapshot = true;

    UE_LOG(LogEquipmentUIBridge, Warning, TEXT("✓✓✓ Initial cache built with %d slots ✓✓✓"),
        CachedUIData.Num());
    UE_LOG(LogEquipmentUIBridge, Warning, TEXT("=== BindToDataStore END ==="));

    // Notify widgets immediately with initial data
    OnEquipmentUIDataChanged.Broadcast(CachedUIData);
}

// ===== NEW: DataStore Event Handlers =====

void USuspenseEquipmentUIBridge::HandleDataStoreSlotChanged(
    int32 SlotIndex,
    const FSuspenseInventoryItemInstance& NewItem)
{
    UE_LOG(LogEquipmentUIBridge, Verbose,
        TEXT("DataStore slot %d changed: %s (InstanceID: %s)"),
        SlotIndex,
        *NewItem.ItemID.ToString(),
        *NewItem.InstanceID.ToString());

    // Update cache incrementally for this slot only
    UpdateCachedSlot(SlotIndex, NewItem);

    // Update legacy map
    if (NewItem.IsValid())
    {
        CachedItems.Add(SlotIndex, NewItem);
    }
    else
    {
        CachedItems.Remove(SlotIndex);
    }

    // Mark slot as pending update
    PendingSlotUpdates.Add(SlotIndex);

    // Schedule coalesced notification (batches rapid changes)
    ScheduleCoalescedNotification();
}

void USuspenseEquipmentUIBridge::HandleDataStoreReset()
{
    UE_LOG(LogEquipmentUIBridge, Warning, TEXT("DataStore reset - rebuilding full cache"));

    // Clear everything and rebuild
    CachedUIData.Empty();
    CachedItems.Empty();
    PendingSlotUpdates.Empty();

    // Rebuild will be done on next BindToDataStore or manual refresh
    bHasSnapshot = false;

    // Notify widgets of full reset
    OnEquipmentUIDataChanged.Broadcast(CachedUIData);
}

// ===== NEW: Cache Management =====

void USuspenseEquipmentUIBridge::UpdateCachedSlot(
    int32 SlotIndex,
    const FSuspenseInventoryItemInstance& NewItem)
{
    // Ensure cache is large enough
    if (!CachedUIData.IsValidIndex(SlotIndex))
    {
        CachedUIData.SetNum(FMath::Max(CachedUIData.Num(), SlotIndex + 1));
    }

    FEquipmentSlotUIData& UISlot = CachedUIData[SlotIndex];

    // Update basic slot info
    UISlot.SlotIndex = SlotIndex;
    UISlot.bIsOccupied = NewItem.IsValid();
    UISlot.ItemInstance = NewItem;

    // Get configuration from cached configs
    if (CachedConfigs.IsValidIndex(SlotIndex))
    {
        const FEquipmentSlotConfig& Config = CachedConfigs[SlotIndex];
        UISlot.SlotType = Config.SlotTag;
        UISlot.AllowedItemTypes = Config.AllowedItemTypes;
        UISlot.SlotName = Config.DisplayName;
        UISlot.bIsRequired = Config.bIsRequired;
        UISlot.GridSize = FIntPoint(1, 1); // Equipment slots are always 1x1
        UISlot.GridPosition = FIntPoint(SlotIndex % 3, SlotIndex / 3);
    }

    // Convert item to UI format if occupied
    if (UISlot.bIsOccupied)
    {
        FItemUIData ItemUI;
        if (ConvertItemInstanceToUIData(NewItem, ItemUI))
        {
            UISlot.EquippedItem = ItemUI;
        }
        else
        {
            UE_LOG(LogEquipmentUIBridge, Warning,
                TEXT("Failed to convert item to UI data for slot %d"), SlotIndex);
        }
    }
    else
    {
        // Clear item data
        UISlot.EquippedItem = FItemUIData();
    }

    UE_LOG(LogEquipmentUIBridge, VeryVerbose,
        TEXT("Updated cache for slot %d: Occupied=%s"),
        SlotIndex, UISlot.bIsOccupied ? TEXT("YES") : TEXT("NO"));
}

void USuspenseEquipmentUIBridge::RebuildUICache()
{
    UE_LOG(LogEquipmentUIBridge, Log, TEXT("Rebuilding full UI cache"));

    CachedUIData.Empty();
    CachedUIData.SetNum(CachedConfigs.Num());

    for (int32 i = 0; i < CachedConfigs.Num(); ++i)
    {
        const FSuspenseInventoryItemInstance* FoundItem = CachedItems.Find(i);
        const FSuspenseInventoryItemInstance ItemInstance = FoundItem ? *FoundItem : FSuspenseInventoryItemInstance();

        UpdateCachedSlot(i, ItemInstance);
    }

    UE_LOG(LogEquipmentUIBridge, Log, TEXT("Cache rebuilt: %d slots"), CachedUIData.Num());
}

// ===== NEW: Coalescing Logic =====

void USuspenseEquipmentUIBridge::ScheduleCoalescedNotification()
{
    if (!OwningPlayerController)
    {
        return;
    }

    UWorld* World = OwningPlayerController->GetWorld();
    if (!World)
    {
        return;
    }

    // If timer is already active, let it run (updates will accumulate)
    if (World->GetTimerManager().IsTimerActive(CoalesceTimerHandle))
    {
        UE_LOG(LogEquipmentUIBridge, VeryVerbose,
            TEXT("Coalescing timer already active - updates will batch"));
        return;
    }

    // Schedule notification after brief delay to batch rapid changes
    World->GetTimerManager().SetTimer(
        CoalesceTimerHandle,
        this,
        &USuspenseEquipmentUIBridge::CoalesceAndNotify,
        CoalescingInterval,
        false // Non-repeating
    );

    UE_LOG(LogEquipmentUIBridge, VeryVerbose,
        TEXT("Scheduled coalesced notification in %.3f seconds"),
        CoalescingInterval);
}

void USuspenseEquipmentUIBridge::CoalesceAndNotify()
{
    if (PendingSlotUpdates.Num() == 0)
    {
        UE_LOG(LogEquipmentUIBridge, VeryVerbose, TEXT("No pending updates to notify"));
        return;
    }

    UE_LOG(LogEquipmentUIBridge, Verbose,
        TEXT("Broadcasting equipment data changed: %d slots updated"),
        PendingSlotUpdates.Num());

    // Clear pending set
    PendingSlotUpdates.Empty();

    // CRITICAL: Broadcast full cached data to all subscribed widgets
    // Widgets receive ready-to-use data, no conversion needed
    OnEquipmentUIDataChanged.Broadcast(CachedUIData);
}

// ===== ISuspenseEquipmentUIBridgeInterfaceWidget Implementation =====

void USuspenseEquipmentUIBridge::ShowEquipmentUI_Implementation()
{
    bVisible = true;
    RefreshEquipmentUI_Implementation();
}

void USuspenseEquipmentUIBridge::HideEquipmentUI_Implementation()
{
    bVisible = false;
    RefreshEquipmentUI_Implementation();
}

void USuspenseEquipmentUIBridge::ToggleEquipmentUI_Implementation()
{
    bVisible = !bVisible;
    RefreshEquipmentUI_Implementation();
}

bool USuspenseEquipmentUIBridge::IsEquipmentUIVisible_Implementation() const
{
    return bVisible;
}

void USuspenseEquipmentUIBridge::RefreshEquipmentUI_Implementation()
{
    UE_LOG(LogEquipmentUIBridge, Verbose, TEXT("RefreshEquipmentUI called"));

    // In new architecture, we don't use EventDelegateManager for refresh
    // Instead, we directly notify subscribed widgets
    if (bHasSnapshot)
    {
        OnEquipmentUIDataChanged.Broadcast(CachedUIData);
    }
}

void USuspenseEquipmentUIBridge::OnEquipmentDataChanged_Implementation(const FGameplayTag& ChangeType)
{
    // Legacy compatibility - just trigger refresh
    RefreshEquipmentUI_Implementation();
}

bool USuspenseEquipmentUIBridge::IsEquipmentConnected_Implementation() const
{
    return bHasSnapshot;
}

bool USuspenseEquipmentUIBridge::GetEquipmentSlotsUIData_Implementation(
    TArray<FEquipmentSlotUIData>& OutSlots) const
{
    UE_LOG(LogEquipmentUIBridge, Verbose, TEXT("GetEquipmentSlotsUIData called"));

    if (!bHasSnapshot || CachedUIData.Num() == 0)
    {
        UE_LOG(LogEquipmentUIBridge, Warning, TEXT("No cached data available"));
        return false;
    }

    // Return only visible slots
    OutSlots.Reset();
    for (const FEquipmentSlotUIData& SlotData : CachedUIData)
    {
        // Check if slot is configured as visible
        if (CachedConfigs.IsValidIndex(SlotData.SlotIndex))
        {
            const FEquipmentSlotConfig& Config = CachedConfigs[SlotData.SlotIndex];
            if (Config.bIsVisible)
            {
                OutSlots.Add(SlotData);
            }
        }
    }

    UE_LOG(LogEquipmentUIBridge, Verbose,
        TEXT("Returned %d visible slots (from %d total)"),
        OutSlots.Num(), CachedUIData.Num());

    return OutSlots.Num() > 0;
}

bool USuspenseEquipmentUIBridge::ProcessEquipmentDrop_Implementation(
    int32 SlotIndex,
    const FDragDropUIData& DragData)
{
    UE_LOG(LogEquipmentUIBridge, Warning, TEXT("=== ProcessEquipmentDrop START ==="));
    UE_LOG(LogEquipmentUIBridge, Warning, TEXT("Target Slot: %d"), SlotIndex);
    UE_LOG(LogEquipmentUIBridge, Warning, TEXT("Item: %s (InstanceID: %s)"),
        *DragData.ItemData.ItemID.ToString(),
        *DragData.ItemData.ItemInstanceID.ToString());

    if (!EventManager)
    {
        UE_LOG(LogEquipmentUIBridge, Error, TEXT("EventManager not available"));
        return false;
    }

    // Validate drag data
    if (!DragData.IsValidDragData())
    {
        UE_LOG(LogEquipmentUIBridge, Warning, TEXT("Invalid drag data"));
        NotifyUser(TEXT("Invalid item data"), 2.0f);
        return false;
    }

    // Create item instance from drag data using factory method with specific InstanceID
    FSuspenseInventoryItemInstance Instance = FSuspenseInventoryItemInstance::CreateWithID(
        DragData.ItemData.ItemID,
        DragData.ItemData.ItemInstanceID,
        DragData.ItemData.Quantity
    );
    Instance.bIsRotated = DragData.ItemData.bIsRotated;

    if (DragData.SourceSlotIndex != INDEX_NONE)
    {
        Instance.AnchorIndex = DragData.SourceSlotIndex;
    }

    if (!Instance.IsValid() || !Instance.InstanceID.IsValid())
    {
        UE_LOG(LogEquipmentUIBridge, Error, TEXT("Invalid item instance"));
        NotifyUser(TEXT("Internal error: Invalid item"), 3.0f);
        return false;
    }

    // Build equipment operation request using factory method
    FEquipmentOperationRequest Request = FEquipmentOperationRequest::CreateRequest(
        EEquipmentOperationType::Equip,
        Instance,
        SlotIndex
    );

    Request.SourceSlotIndex = INDEX_NONE;
    Request.TargetSlotIndex = SlotIndex;
    Request.Priority = EEquipmentOperationPriority::Normal;
    Request.Timestamp = FPlatformTime::Seconds();
    // OperationId уже создан в CreateRequest(), не нужно перезаписывать

    Request.Parameters.Add(TEXT("UIOrigin"), TEXT("EquipmentBridge"));
    Request.Parameters.Add(TEXT("SourceContainer"), TEXT("Inventory"));
    Request.Parameters.Add(TEXT("OriginalInstanceID"), Instance.InstanceID.ToString());

    UE_LOG(LogEquipmentUIBridge, Warning,
        TEXT("Broadcasting equip request (OperationID: %s)"),
        *Request.OperationId.ToString());

    // Send request through event system
    EventManager->BroadcastEquipmentOperationRequest(Request);

    UE_LOG(LogEquipmentUIBridge, Warning, TEXT("=== ProcessEquipmentDrop END ==="));
    return true;
}

bool USuspenseEquipmentUIBridge::ProcessUnequipRequest_Implementation(
    int32 SlotIndex,
    int32 TargetInventorySlot)
{
    UE_LOG(LogEquipmentUIBridge, Warning, TEXT("=== ProcessUnequipRequest START ==="));
    UE_LOG(LogEquipmentUIBridge, Warning, TEXT("Source Slot: %d"), SlotIndex);

    if (!EventManager)
    {
        UE_LOG(LogEquipmentUIBridge, Error, TEXT("EventManager not available"));
        return false;
    }

    // Validate slot
    if (!CachedUIData.IsValidIndex(SlotIndex))
    {
        UE_LOG(LogEquipmentUIBridge, Error, TEXT("Invalid slot index: %d"), SlotIndex);
        return false;
    }

    // Get item from cache
    const FEquipmentSlotUIData& SlotData = CachedUIData[SlotIndex];
    if (!SlotData.bIsOccupied || !SlotData.ItemInstance.IsValid())
    {
        UE_LOG(LogEquipmentUIBridge, Warning, TEXT("Slot %d is empty"), SlotIndex);
        return false;
    }

    UE_LOG(LogEquipmentUIBridge, Warning, TEXT("Unequipping: %s"),
        *SlotData.ItemInstance.ItemID.ToString());

    // Create unequip request
    FEquipmentOperationRequest Request = FEquipmentOperationRequest::CreateRequest(
        EEquipmentOperationType::Unequip,
        SlotData.ItemInstance,
        INDEX_NONE
    );

    Request.SourceSlotIndex = SlotIndex;
    Request.TargetSlotIndex = INDEX_NONE;
    Request.Priority = EEquipmentOperationPriority::Normal;
    Request.Timestamp = FPlatformTime::Seconds();
    Request.OperationId = FGuid::NewGuid();

    Request.Parameters.Add(TEXT("UIOrigin"), TEXT("EquipmentBridge"));
    Request.Parameters.Add(TEXT("TargetContainer"), TEXT("Inventory"));

    if (TargetInventorySlot != INDEX_NONE)
    {
        Request.Parameters.Add(TEXT("PreferredInventorySlot"),
            FString::FromInt(TargetInventorySlot));
    }

    UE_LOG(LogEquipmentUIBridge, Warning,
        TEXT("Broadcasting unequip request (OperationID: %s)"),
        *Request.OperationId.ToString());

    EventManager->BroadcastEquipmentOperationRequest(Request);

    UE_LOG(LogEquipmentUIBridge, Warning, TEXT("=== ProcessUnequipRequest END ==="));
    return true;
}

void USuspenseEquipmentUIBridge::SetEquipmentInterface_Implementation(
    const TScriptInterface<ISuspenseEquipment>& InEquipment)
{
    GameEquipment = InEquipment;
}

TScriptInterface<ISuspenseEquipment> USuspenseEquipmentUIBridge::GetEquipmentInterface_Implementation() const
{
    return GameEquipment;
}

USuspenseItemManager* USuspenseEquipmentUIBridge::GetItemManager_Implementation() const
{
    if (!CachedItemManager.IsValid())
    {
        if (OwningPlayerController)
        {
            if (UGameInstance* GI = OwningPlayerController->GetGameInstance())
            {
                CachedItemManager = GI->GetSubsystem<USuspenseItemManager>();
            }
        }
    }
    return CachedItemManager.Get();
}

// ===== Helpers =====

bool USuspenseEquipmentUIBridge::ConvertItemInstanceToUIData(
    const FSuspenseInventoryItemInstance& ItemInstance,
    FItemUIData& OutUIData) const
{
    if (!ItemInstance.IsValid())
    {
        return false;
    }

    USuspenseItemManager* IM = GetItemManager_Implementation();
    if (!IM)
    {
        UE_LOG(LogEquipmentUIBridge, Error, TEXT("ItemManager not available"));
        return false;
    }

    FSuspenseUnifiedItemData Unified;
    if (!IM->GetUnifiedItemData(ItemInstance.ItemID, Unified))
    {
        UE_LOG(LogEquipmentUIBridge, Error,
            TEXT("Failed to get unified data for item %s"),
            *ItemInstance.ItemID.ToString());
        return false;
    }

    // Basic instance data
    OutUIData.ItemID = ItemInstance.ItemID;
    OutUIData.ItemInstanceID = ItemInstance.InstanceID;
    OutUIData.Quantity = ItemInstance.Quantity;
    OutUIData.bIsRotated = ItemInstance.bIsRotated;

    // Classification
    OutUIData.ItemType = Unified.ItemType;
    OutUIData.bIsEquippable = Unified.bIsEquippable;
    OutUIData.EquipmentSlotType = Unified.EquipmentSlot;

    // Display
    OutUIData.DisplayName = Unified.DisplayName;
    OutUIData.Description = Unified.Description;

    // Icon
    if (!Unified.Icon.IsNull())
    {
        if (UTexture2D* Icon = Unified.Icon.LoadSynchronous())
        {
            OutUIData.SetIcon(Icon);
        }
    }

    // Physical properties
    OutUIData.GridSize = FIntPoint(Unified.GridSize.X, Unified.GridSize.Y);
    OutUIData.Weight = Unified.Weight;
    OutUIData.MaxStackSize = Unified.MaxStackSize;

    return true;
}

ISuspenseEquipmentOperations* USuspenseEquipmentUIBridge::ResolveOperations() const
{
    if (Operations.GetInterface())
    {
        return Operations.GetInterface();
    }

    const UObject* Ctx = OwningPlayerController ?
        static_cast<const UObject*>(OwningPlayerController) : this;

    if (USuspenseEquipmentServiceLocator* Locator = USuspenseEquipmentServiceLocator::Get(Ctx))
    {
        const FGameplayTag DefaultTag = FGameplayTag::RequestGameplayTag(
            TEXT("Equipment.Service.Operation"), false);

        if (DefaultTag.IsValid())
        {
            if (ISuspenseEquipmentOperations* I =
                Locator->GetServiceAs<ISuspenseEquipmentOperations>(DefaultTag))
            {
                const_cast<USuspenseEquipmentUIBridge*>(this)->Operations.SetInterface(I);
                const_cast<USuspenseEquipmentUIBridge*>(this)->Operations.SetObject(
                    Cast<UObject>(I));
                return I;
            }
        }
    }
    return nullptr;
}

void USuspenseEquipmentUIBridge::NotifyUser(const FString& Text, float Time) const
{
    if (EventManager)
    {
        EventManager->NotifyUI(Text, Time);
    }
}

// ===== Static Registration =====

static TWeakObjectPtr<USuspenseEquipmentUIBridge> G_BridgeInstance;

void USuspenseEquipmentUIBridge::RegisterBridge(USuspenseEquipmentUIBridge* Bridge)
{
    G_BridgeInstance = Bridge;
    ISuspenseEquipmentUIBridgeInterface::SetGlobalEquipmentBridge(Bridge);
}

void USuspenseEquipmentUIBridge::UnregisterBridge(USuspenseEquipmentUIBridge* Bridge)
{
    if (G_BridgeInstance.Get() == Bridge)
    {
        G_BridgeInstance.Reset();
        ISuspenseEquipmentUIBridgeInterface::ClearGlobalEquipmentBridge();
    }
}
