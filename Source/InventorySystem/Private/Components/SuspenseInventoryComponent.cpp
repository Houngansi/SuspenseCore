// MedComInventory/Components/MedComInventoryComponent.cpp
// Copyright Suspense Team. All Rights Reserved.

#include "Components/SuspenseInventoryComponent.h"
#include "Storage/SuspenseInventoryStorage.h"
#include "Validation/SuspenseInventoryValidator.h"
#include "Operations/SuspenseInventoryTransaction.h"
#include "Network/SuspenseInventoryReplicator.h"
#include "Events/SuspenseInventoryEvents.h"
#include "Serialization/SuspenseInventorySerializer.h"
#include "UI/SuspenseInventoryUIConnector.h"
#include "Abilities/Inventory/SuspenseInventoryGASIntegration.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Types/Loadout/SuspenseItemDataTable.h"
#include "Types/Loadout/SuspenseLoadoutSettings.h"
#include "Operations/SuspenseInventoryResult.h"
#include "Types/Inventory/SuspenseInventoryUtils.h"
#include "Base/SuspenseInventoryLogs.h"
#include "Base/SuspenseInventoryManager.h"
#include "ItemSystem/SuspenseItemManager.h"
#include "Delegates/SuspenseEventManager.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

USuspenseInventoryComponent::USuspenseInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f;

    bIsInitialized = false;
    MaxWeight = 100.0f;
    CurrentWeight = 0.0f;
    CurrentLoadoutID = NAME_None;
    CurrentInventoryName = NAME_None;

    SetIsReplicatedByDefault(true);
}

void USuspenseInventoryComponent::BeginPlay()
{
    Super::BeginPlay();

    // Initialize sub-components if not already done
    if (!StorageComponent)
    {
        InitializeSubComponents();
    }

    // Cache managers for performance
    CachedDelegateManager = GetDelegateManager();
    CachedItemManager = GetItemManager();
    CachedInventoryManager = GetInventoryManager();
}

void USuspenseInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Clear timer
    if (GetWorld() && ClientInitCheckTimer.IsValid())
    {
        GetWorld()->GetTimerManager().ClearTimer(ClientInitCheckTimer);
    }

    // Clean up transactions
    if (TransactionComponent && TransactionComponent->IsTransactionActive())
    {
        TransactionComponent->RollbackTransaction();
    }

    // Clear cached references
    CachedDelegateManager.Reset();
    CachedItemManager.Reset();
    CachedInventoryManager.Reset();
    CachedLoadoutConfig.Reset();

    Super::EndPlay(EndPlayReason);
}

void USuspenseInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(USuspenseInventoryComponent, bIsInitialized);
    DOREPLIFETIME(USuspenseInventoryComponent, MaxWeight);
    DOREPLIFETIME(USuspenseInventoryComponent, CurrentWeight);
    DOREPLIFETIME(USuspenseInventoryComponent, AllowedItemTypes);
    DOREPLIFETIME(USuspenseInventoryComponent, ReplicatedGridSize);
    DOREPLIFETIME(USuspenseInventoryComponent, bIsInitializedReplicated);
    DOREPLIFETIME(USuspenseInventoryComponent, CurrentLoadoutID);
    DOREPLIFETIME(USuspenseInventoryComponent, CurrentInventoryName);
}

void USuspenseInventoryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Update weight periodically on server
    if (GetOwnerRole() == ROLE_Authority && bIsInitialized)
    {
        static float WeightUpdateTimer = 0.0f;
        WeightUpdateTimer += DeltaTime;

        if (WeightUpdateTimer >= 1.0f)
        {
            UpdateCurrentWeight();
            WeightUpdateTimer = 0.0f;
        }
    }
}

//==================================================================
// ISuspenseInventory Implementation - Core Operations
//==================================================================

bool USuspenseInventoryComponent::AddItemByID_Implementation(FName ItemID, int32 Quantity)
{
    // Validate parameters
    if (ItemID.IsNone() || Quantity <= 0)
    {
        INVENTORY_LOG(Warning, TEXT("AddItemByID: Invalid parameters - ID: %s, Quantity: %d"),
            *ItemID.ToString(), Quantity);
        return false;
    }

    // Get unified data from ItemManager
    USuspenseItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        INVENTORY_LOG(Error, TEXT("AddItemByID: ItemManager not available"));
        return false;
    }

    FSuspenseUnifiedItemData UnifiedData;
    if (!ItemManager->GetUnifiedItemData(ItemID, UnifiedData))
    {
        INVENTORY_LOG(Warning, TEXT("AddItemByID: Item '%s' not found in DataTable"), *ItemID.ToString());

        // Broadcast error through delegate manager
        ISuspenseInventory::BroadcastInventoryError(this, ESuspenseInventoryErrorCode::InvalidItem,
            FString::Printf(TEXT("Unknown item: %s"), *ItemID.ToString()));
        return false;
    }

    // Add diagnostic logging for item type verification
    INVENTORY_LOG(Warning, TEXT("AddItemByID: Processing item:"));
    INVENTORY_LOG(Warning, TEXT("  - ItemID: %s"), *ItemID.ToString());
    INVENTORY_LOG(Warning, TEXT("  - ItemType: %s"), *UnifiedData.ItemType.ToString());
    INVENTORY_LOG(Warning, TEXT("  - Weight: %.2f"), UnifiedData.Weight);
    INVENTORY_LOG(Warning, TEXT("  - Grid Size: %dx%d"), UnifiedData.GridSize.X, UnifiedData.GridSize.Y);

    // Verify item type is in Item hierarchy
    static const FGameplayTag BaseItemTag = FGameplayTag::RequestGameplayTag(TEXT("Item"));
    if (!UnifiedData.ItemType.MatchesTag(BaseItemTag))
    {
        INVENTORY_LOG(Error, TEXT("AddItemByID: Item type %s is not in Item hierarchy!"),
            *UnifiedData.ItemType.ToString());

        ISuspenseInventory::BroadcastInventoryError(this, ESuspenseInventoryErrorCode::InvalidItem,
            FString::Printf(TEXT("Invalid item type: %s"), *UnifiedData.ItemType.ToString()));
        return false;
    }

    // Check allowed types with hierarchy support
    if (!AllowedItemTypes.IsEmpty())
    {
        bool bTypeAllowed = false;

        // Check exact match first
        if (AllowedItemTypes.HasTag(UnifiedData.ItemType))
        {
            bTypeAllowed = true;
        }
        else
        {
            // Check parent tags using hierarchy
            for (const FGameplayTag& AllowedTag : AllowedItemTypes)
            {
                if (UnifiedData.ItemType.MatchesTag(AllowedTag))
                {
                    INVENTORY_LOG(Log, TEXT("AddItemByID: Type %s matches allowed parent %s"),
                        *UnifiedData.ItemType.ToString(), *AllowedTag.ToString());
                    bTypeAllowed = true;
                    break;
                }
            }
        }

        if (!bTypeAllowed)
        {
            INVENTORY_LOG(Warning, TEXT("AddItemByID: Item type %s not allowed in inventory"),
                *UnifiedData.ItemType.ToString());

            INVENTORY_LOG(Warning, TEXT("  Allowed types:"));
            for (const FGameplayTag& Tag : AllowedItemTypes)
            {
                INVENTORY_LOG(Warning, TEXT("    - %s"), *Tag.ToString());
            }

            ISuspenseInventory::BroadcastInventoryError(this, ESuspenseInventoryErrorCode::InvalidItem,
                TEXT("Item type not allowed"));
            return false;
        }
    }

    // Use the legacy AddItem method with unified data
    return AddItem(UnifiedData, Quantity);
}

FSuspenseInventoryOperationResult USuspenseInventoryComponent::AddItemInstance(const FSuspenseInventoryItemInstance& ItemInstance)
{
    // Validate instance
    if (!ItemInstance.IsValid())
    {
        return FSuspenseInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::InvalidItem,
            FText::FromString(TEXT("Invalid item instance")),
            TEXT("AddItemInstance"),
            nullptr
        );
    }

    // Validate InstanceID
    if (!ItemInstance.InstanceID.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[InventoryComponent] AddItemInstance: Item %s has invalid InstanceID, this may cause issues"),
            *ItemInstance.ItemID.ToString());
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("[InventoryComponent] AddItemInstance: Adding item %s with InstanceID %s"),
            *ItemInstance.ItemID.ToString(),
            *ItemInstance.InstanceID.ToString());
    }

    // Check authority
    if (!CheckAuthority(TEXT("AddItemInstance")))
    {
        Server_AddItemByID(ItemInstance.ItemID, ItemInstance.Quantity);
        return FSuspenseInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::UnknownError,
            FText::FromString(TEXT("Client cannot add items directly")),
            TEXT("AddItemInstance"),
            nullptr
        );
    }

    // Validate components
    if (!StorageComponent || !bIsInitialized)
    {
        return FSuspenseInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::NotInitialized,
            FText::FromString(TEXT("Inventory not initialized")),
            TEXT("AddItemInstance"),
            nullptr
        );
    }

    // Get unified data for validation
    FSuspenseUnifiedItemData UnifiedData;
    if (!GetItemManager() || !GetItemManager()->GetUnifiedItemData(ItemInstance.ItemID, UnifiedData))
    {
        return FSuspenseInventoryOperationResult::ItemNotFound(TEXT("AddItemInstance"), ItemInstance.ItemID);
    }

    // Check constraints
    if (ConstraintsComponent)
    {
        // Check item type
        if (!AllowedItemTypes.IsEmpty() && !AllowedItemTypes.HasTag(UnifiedData.ItemType))
        {
            return FSuspenseInventoryOperationResult::Failure(
                ESuspenseInventoryErrorCode::InvalidItem,
                FText::FromString(TEXT("Item type not allowed in this inventory")),
                TEXT("AddItemInstance"),
                nullptr
            );
        }

        // Check weight
        float ItemWeight = UnifiedData.Weight * ItemInstance.Quantity;
        if (CurrentWeight + ItemWeight > MaxWeight)
        {
            return FSuspenseInventoryOperationResult::Failure(
                ESuspenseInventoryErrorCode::WeightLimit,
                FText::Format(FText::FromString(TEXT("Would exceed weight limit: {0} + {1} > {2}")),
                    FText::AsNumber(CurrentWeight),
                    FText::AsNumber(ItemWeight),
                    FText::AsNumber(MaxWeight)),
                TEXT("AddItemInstance"),
                nullptr
            );
        }
    }

    // Create copy with valid InstanceID if needed
    FSuspenseInventoryItemInstance InstanceToAdd = ItemInstance;
    if (!InstanceToAdd.InstanceID.IsValid())
    {
        InstanceToAdd.InstanceID = FGuid::NewGuid();
        UE_LOG(LogTemp, Warning, TEXT("[InventoryComponent] Generated new InstanceID %s for item %s"),
            *InstanceToAdd.InstanceID.ToString(),
            *InstanceToAdd.ItemID.ToString());
    }

    // Try to add to storage
    if (!StorageComponent->AddItemInstance(InstanceToAdd, true))
    {
        return FSuspenseInventoryOperationResult::NoSpace(TEXT("AddItemInstance"));
    }

    // Update weight
    CurrentWeight += UnifiedData.Weight * InstanceToAdd.Quantity;

    // Get placed instance for correct anchor index
    TArray<FSuspenseInventoryItemInstance> AllInstances = StorageComponent->GetAllItemInstances();
    const FSuspenseInventoryItemInstance* PlacedInstance = nullptr;

    for (const FSuspenseInventoryItemInstance& StoredInstance : AllInstances)
    {
        if (StoredInstance.InstanceID == InstanceToAdd.InstanceID)
        {
            PlacedInstance = &StoredInstance;
            break;
        }
    }

    if (PlacedInstance)
    {
        UE_LOG(LogTemp, Log, TEXT("[InventoryComponent] Item placed successfully: %s at slot %d with InstanceID %s"),
            *PlacedInstance->ItemID.ToString(),
            PlacedInstance->AnchorIndex,
            *PlacedInstance->InstanceID.ToString());

        // Notify replicator
        NotifyItemPlaced(*PlacedInstance, PlacedInstance->AnchorIndex);

        // Broadcast events
        ISuspenseInventory::BroadcastItemAdded(this, *PlacedInstance, PlacedInstance->AnchorIndex);

        if (EventsComponent)
        {
            EventsComponent->BroadcastItemAdded(InstanceToAdd.ItemID, InstanceToAdd.Quantity);
        }
    }

    BroadcastInventoryUpdated();

    // Create success result
    FSuspenseInventoryOperationResult Result = FSuspenseInventoryOperationResult::Success(TEXT("AddItemInstance"));
    Result.AddResultData(TEXT("ItemID"), InstanceToAdd.ItemID.ToString());
    Result.AddResultData(TEXT("Quantity"), FString::FromInt(InstanceToAdd.Quantity));
    Result.AddResultData(TEXT("InstanceID"), InstanceToAdd.InstanceID.ToString());
    if (PlacedInstance)
    {
        Result.AddResultData(TEXT("PlacementIndex"), FString::FromInt(PlacedInstance->AnchorIndex));
    }

    return Result;
}

FSuspenseInventoryOperationResult USuspenseInventoryComponent::AddItemInstanceToSlot(const FSuspenseInventoryItemInstance& ItemInstance, int32 TargetSlot)
{
    // Validate instance
    if (!ItemInstance.IsValid())
    {
        return FSuspenseInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::InvalidItem,
            FText::FromString(TEXT("Invalid item instance")),
            TEXT("AddItemInstanceToSlot"),
            nullptr
        );
    }

    // Check authority
    if (!CheckAuthority(TEXT("AddItemInstanceToSlot")))
    {
        return FSuspenseInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::UnknownError,
            FText::FromString(TEXT("Client cannot add items directly")),
            TEXT("AddItemInstanceToSlot"),
            nullptr
        );
    }

    // Validate components
    if (!StorageComponent || !bIsInitialized)
    {
        return FSuspenseInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::NotInitialized,
            FText::FromString(TEXT("Inventory not initialized")),
            TEXT("AddItemInstanceToSlot"),
            nullptr
        );
    }

    // Get unified data for validation
    FSuspenseUnifiedItemData UnifiedData;
    if (!GetItemManager() || !GetItemManager()->GetUnifiedItemData(ItemInstance.ItemID, UnifiedData))
    {
        return FSuspenseInventoryOperationResult::ItemNotFound(TEXT("AddItemInstanceToSlot"), ItemInstance.ItemID);
    }

    // Check constraints
    if (ConstraintsComponent)
    {
        // Check item type
        if (!AllowedItemTypes.IsEmpty() && !AllowedItemTypes.HasTag(UnifiedData.ItemType))
        {
            return FSuspenseInventoryOperationResult::Failure(
                ESuspenseInventoryErrorCode::InvalidItem,
                FText::FromString(TEXT("Item type not allowed in this inventory")),
                TEXT("AddItemInstanceToSlot"),
                nullptr
            );
        }

        // Check weight
        float ItemWeight = UnifiedData.Weight * ItemInstance.Quantity;
        if (CurrentWeight + ItemWeight > MaxWeight)
        {
            return FSuspenseInventoryOperationResult::Failure(
                ESuspenseInventoryErrorCode::WeightLimit,
                FText::Format(FText::FromString(TEXT("Would exceed weight limit: {0} + {1} > {2}")),
                    FText::AsNumber(CurrentWeight),
                    FText::AsNumber(ItemWeight),
                    FText::AsNumber(MaxWeight)),
                TEXT("AddItemInstanceToSlot"),
                nullptr
            );
        }
    }

    // Create copy with valid InstanceID if needed
    FSuspenseInventoryItemInstance InstanceToAdd = ItemInstance;
    if (!InstanceToAdd.InstanceID.IsValid())
    {
        InstanceToAdd.InstanceID = FGuid::NewGuid();
    }

    // CRITICAL: Set target slot
    InstanceToAdd.AnchorIndex = TargetSlot;

    // Check if target slot is free
    if (!StorageComponent->AreCellsFreeForItem(TargetSlot, InstanceToAdd.ItemID, false))
    {
        // If slot occupied, try to find alternative
        UE_LOG(LogInventory, Warning, TEXT("AddItemInstanceToSlot: Target slot %d is occupied, finding alternative"), TargetSlot);

        // Fall back to regular add
        return AddItemInstance(ItemInstance);
    }

    // Place in specific slot
    if (!StorageComponent->PlaceItemInstance(InstanceToAdd, TargetSlot))
    {
        return FSuspenseInventoryOperationResult::NoSpace(TEXT("AddItemInstanceToSlot"));
    }

    // Update weight
    CurrentWeight += UnifiedData.Weight * InstanceToAdd.Quantity;

    UE_LOG(LogInventory, Log, TEXT("AddItemInstanceToSlot: Item %s placed at requested slot %d"),
        *InstanceToAdd.ItemID.ToString(), TargetSlot);

    // Notify replicator
    NotifyItemPlaced(InstanceToAdd, TargetSlot);

    // Broadcast events
    ISuspenseInventory::BroadcastItemAdded(this, InstanceToAdd, TargetSlot);

    if (EventsComponent)
    {
        EventsComponent->BroadcastItemAdded(InstanceToAdd.ItemID, InstanceToAdd.Quantity);
    }

    BroadcastInventoryUpdated();

    // Create success result
    FSuspenseInventoryOperationResult Result = FSuspenseInventoryOperationResult::Success(TEXT("AddItemInstanceToSlot"));
    Result.AddResultData(TEXT("ItemID"), InstanceToAdd.ItemID.ToString());
    Result.AddResultData(TEXT("Quantity"), FString::FromInt(InstanceToAdd.Quantity));
    Result.AddResultData(TEXT("InstanceID"), InstanceToAdd.InstanceID.ToString());
    Result.AddResultData(TEXT("PlacementIndex"), FString::FromInt(TargetSlot));

    return Result;
}

bool USuspenseInventoryComponent::RemoveItemFromSlot(int32 SlotIndex, FSuspenseInventoryItemInstance& OutRemovedInstance)
{
    // Check authority
    if (!CheckAuthority(TEXT("RemoveItemFromSlot")))
    {
        return false;
    }

    // Validate components
    if (!StorageComponent || !bIsInitialized)
    {
        INVENTORY_LOG(Error, TEXT("RemoveItemFromSlot: Inventory not initialized"));
        return false;
    }

    // Get instance from slot
    FSuspenseInventoryItemInstance InstanceToRemove;
    if (!StorageComponent->GetItemInstanceAt(SlotIndex, InstanceToRemove))
    {
        INVENTORY_LOG(Warning, TEXT("RemoveItemFromSlot: No item found at slot %d"), SlotIndex);
        return false;
    }

    // Verify slot is anchor position
    if (InstanceToRemove.AnchorIndex != SlotIndex)
    {
        INVENTORY_LOG(Warning, TEXT("RemoveItemFromSlot: Slot %d is not the anchor position for item"), SlotIndex);
        return false;
    }

    // Begin transaction for safe removal
    BeginTransaction();

    // Remove from storage
    if (!StorageComponent->RemoveItemInstance(InstanceToRemove.InstanceID))
    {
        RollbackTransaction();
        INVENTORY_LOG(Error, TEXT("RemoveItemFromSlot: Failed to remove item from storage"));
        return false;
    }

    // Update weight
    if (USuspenseItemManager* ItemManager = GetItemManager())
    {
        FSuspenseUnifiedItemData ItemData;
        if (ItemManager->GetUnifiedItemData(InstanceToRemove.ItemID, ItemData))
        {
            CurrentWeight -= ItemData.Weight * InstanceToRemove.Quantity;
            CurrentWeight = FMath::Max(0.0f, CurrentWeight);
        }
    }

    // Commit transaction
    CommitTransaction();

    // Save removed instance for return
    OutRemovedInstance = InstanceToRemove;

    // Notify replicator
    NotifyItemRemoved(InstanceToRemove);

    // Broadcast events
    ISuspenseInventory::BroadcastItemRemoved(this, InstanceToRemove.ItemID, InstanceToRemove.Quantity, SlotIndex);

    if (EventsComponent)
    {
        EventsComponent->BroadcastItemRemoved(InstanceToRemove.ItemID, InstanceToRemove.Quantity);
    }

    BroadcastInventoryUpdated();

    INVENTORY_LOG(Log, TEXT("RemoveItemFromSlot: Successfully removed %s (x%d) from slot %d"),
        *InstanceToRemove.ItemID.ToString(), InstanceToRemove.Quantity, SlotIndex);

    return true;
}

bool USuspenseInventoryComponent::CanPlaceItemInstanceAtSlot(const FSuspenseInventoryItemInstance& ItemInstance, int32 SlotIndex) const
{
    // Validate input
    if (!ItemInstance.IsValid() || SlotIndex < 0)
    {
        return false;
    }

    if (!StorageComponent || !bIsInitialized)
    {
        return false;
    }

    // Get item data for size
    USuspenseItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        return false;
    }

    FSuspenseUnifiedItemData ItemData;
    if (!ItemManager->GetUnifiedItemData(ItemInstance.ItemID, ItemData))
    {
        return false;
    }

    // Check type restrictions
    if (!AllowedItemTypes.IsEmpty() && !AllowedItemTypes.HasTag(ItemData.ItemType))
    {
        INVENTORY_LOG(Verbose, TEXT("CanPlaceItemInstanceAtSlot: Item type %s not allowed"),
            *ItemData.ItemType.ToString());
        return false;
    }

    // Check weight restrictions
    float ItemWeight = ItemData.Weight * ItemInstance.Quantity;
    if (CurrentWeight + ItemWeight > MaxWeight)
    {
        INVENTORY_LOG(Verbose, TEXT("CanPlaceItemInstanceAtSlot: Would exceed weight limit"));
        return false;
    }

    // Get effective size with rotation
    FVector2D ItemSize(ItemData.GridSize.X, ItemData.GridSize.Y);
    if (ItemInstance.bIsRotated)
    {
        ItemSize = FVector2D(ItemSize.Y, ItemSize.X);
    }

    // Check slot availability
    return CanPlaceItemAtSlot(ItemSize, SlotIndex, true);
}

bool USuspenseInventoryComponent::PlaceItemInstanceAtSlot(const FSuspenseInventoryItemInstance& ItemInstance, int32 SlotIndex, bool bForcePlace)
{
    // Check authority
    if (!CheckAuthority(TEXT("PlaceItemInstanceAtSlot")))
    {
        return false;
    }

    // Validate data
    if (!ItemInstance.IsValid() || SlotIndex < 0)
    {
        INVENTORY_LOG(Warning, TEXT("PlaceItemInstanceAtSlot: Invalid parameters"));
        return false;
    }

    if (!StorageComponent || !bIsInitialized)
    {
        INVENTORY_LOG(Error, TEXT("PlaceItemInstanceAtSlot: Inventory not initialized"));
        return false;
    }

    // Check placement if not forced
    if (!bForcePlace && !CanPlaceItemInstanceAtSlot(ItemInstance, SlotIndex))
    {
        INVENTORY_LOG(Warning, TEXT("PlaceItemInstanceAtSlot: Cannot place item at slot %d"), SlotIndex);
        return false;
    }

    // Begin transaction
    BeginTransaction();

    // Create copy with correct anchor index
    FSuspenseInventoryItemInstance InstanceToPlace = ItemInstance;
    InstanceToPlace.AnchorIndex = SlotIndex;

    // Ensure valid InstanceID
    if (!InstanceToPlace.InstanceID.IsValid())
    {
        InstanceToPlace.InstanceID = FGuid::NewGuid();
    }

    // Place item
    bool bPlaced = false;
    if (bForcePlace)
    {
        // For force placement, clear target slots first
        USuspenseItemManager* ItemManager = GetItemManager();
        if (ItemManager)
        {
            FSuspenseUnifiedItemData ItemData;
            if (ItemManager->GetUnifiedItemData(InstanceToPlace.ItemID, ItemData))
            {
                FVector2D ItemSize(ItemData.GridSize.X, ItemData.GridSize.Y);
                if (InstanceToPlace.bIsRotated)
                {
                    ItemSize = FVector2D(ItemSize.Y, ItemSize.X);
                }

                // Get all slots this item will occupy
                TArray<int32> OccupiedSlots = GetOccupiedSlots(SlotIndex, ItemSize, InstanceToPlace.bIsRotated);

                // Remove items from these slots
                for (int32 OccupiedSlot : OccupiedSlots)
                {
                    FSuspenseInventoryItemInstance BlockingInstance;
                    if (StorageComponent->GetItemInstanceAt(OccupiedSlot, BlockingInstance))
                    {
                        StorageComponent->RemoveItemInstance(BlockingInstance.InstanceID);

                        // Update weight
                        FSuspenseUnifiedItemData BlockingItemData;
                        if (ItemManager->GetUnifiedItemData(BlockingInstance.ItemID, BlockingItemData))
                        {
                            CurrentWeight -= BlockingItemData.Weight * BlockingInstance.Quantity;
                        }

                        // Notify removal
                        NotifyItemRemoved(BlockingInstance);
                    }
                }
            }
        }
    }

    // Place item
    bPlaced = StorageComponent->PlaceItemInstance(InstanceToPlace, SlotIndex);

    if (bPlaced)
    {
        // Update weight
        USuspenseItemManager* ItemManager = GetItemManager();
        if (ItemManager)
        {
            FSuspenseUnifiedItemData ItemData;
            if (ItemManager->GetUnifiedItemData(InstanceToPlace.ItemID, ItemData))
            {
                CurrentWeight += ItemData.Weight * InstanceToPlace.Quantity;
            }
        }

        // Commit transaction
        CommitTransaction();

        // Notify replicator
        NotifyItemPlaced(InstanceToPlace, SlotIndex);

        // Broadcast events
        ISuspenseInventory::BroadcastItemAdded(this, InstanceToPlace, SlotIndex);

        if (EventsComponent)
        {
            EventsComponent->BroadcastItemAdded(InstanceToPlace.ItemID, InstanceToPlace.Quantity);
        }

        BroadcastInventoryUpdated();

        INVENTORY_LOG(Log, TEXT("PlaceItemInstanceAtSlot: Successfully placed %s at slot %d"),
            *InstanceToPlace.ItemID.ToString(), SlotIndex);

        return true;
    }
    else
    {
        // Rollback transaction
        RollbackTransaction();

        INVENTORY_LOG(Error, TEXT("PlaceItemInstanceAtSlot: Failed to place item at slot %d"), SlotIndex);
        return false;
    }
}

FSuspenseInventoryOperationResult USuspenseInventoryComponent::RemoveItemByID(const FName& ItemID, int32 Amount)
{
    // Validate parameters
    if (ItemID.IsNone() || Amount <= 0)
    {
        return FSuspenseInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::InvalidItem,
            FText::FromString(TEXT("Invalid parameters")),
            TEXT("RemoveItemByID"),
            nullptr
        );
    }

    // Check authority
    if (!CheckAuthority(TEXT("RemoveItemByID")))
    {
        Server_RemoveItem(ItemID, Amount);
        return FSuspenseInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::UnknownError,
            FText::FromString(TEXT("Client cannot remove items directly")),
            TEXT("RemoveItemByID"),
            nullptr
        );
    }

    // Use legacy RemoveItem
    bool bSuccess = RemoveItem(ItemID, Amount);

    if (bSuccess)
    {
        FSuspenseInventoryOperationResult Result = FSuspenseInventoryOperationResult::Success(TEXT("RemoveItemByID"));
        Result.AddResultData(TEXT("ItemID"), ItemID.ToString());
        Result.AddResultData(TEXT("Amount"), FString::FromInt(Amount));
        return Result;
    }
    else
    {
        return FSuspenseInventoryOperationResult::ItemNotFound(TEXT("RemoveItemByID"), ItemID);
    }
}

FSuspenseInventoryOperationResult USuspenseInventoryComponent::RemoveItemInstance(const FGuid& InstanceID)
{
    if (!InstanceID.IsValid())
    {
        return FSuspenseInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::InvalidItem,
            FText::FromString(TEXT("Invalid instance ID")),
            TEXT("RemoveItemInstance"),
            nullptr
        );
    }

    if (!CheckAuthority(TEXT("RemoveItemInstance")))
    {
        return FSuspenseInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::UnknownError,
            FText::FromString(TEXT("Client cannot remove items directly")),
            TEXT("RemoveItemInstance"),
            nullptr
        );
    }

    if (!StorageComponent || !bIsInitialized)
    {
        return FSuspenseInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::NotInitialized,
            FText::FromString(TEXT("Inventory not initialized")),
            TEXT("RemoveItemInstance"),
            nullptr
        );
    }

    // Get instance for information
    FSuspenseInventoryItemInstance Instance;
    if (!StorageComponent->GetItemInstance(InstanceID, Instance))
    {
        return FSuspenseInventoryOperationResult::ItemNotFound(TEXT("RemoveItemInstance"));
    }

    // Remove from storage
    if (!StorageComponent->RemoveItemInstance(InstanceID))
    {
        return FSuspenseInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::UnknownError,
            FText::FromString(TEXT("Failed to remove from storage")),
            TEXT("RemoveItemInstance"),
            nullptr
        );
    }

    // Update weight
    if (USuspenseItemManager* ItemManager = GetItemManager())
    {
        FSuspenseUnifiedItemData ItemData;
        if (ItemManager->GetUnifiedItemData(Instance.ItemID, ItemData))
        {
            CurrentWeight -= ItemData.Weight * Instance.Quantity;
        }
    }

    // Notify replicator
    NotifyItemRemoved(Instance);

    // Broadcast events
    if (EventsComponent)
    {
        EventsComponent->BroadcastItemRemoved(Instance.ItemID, Instance.Quantity);
    }

    BroadcastInventoryUpdated();

    FSuspenseInventoryOperationResult Result = FSuspenseInventoryOperationResult::Success(TEXT("RemoveItemInstance"));
    Result.AddResultData(TEXT("ItemID"), Instance.ItemID.ToString());
    Result.AddResultData(TEXT("Quantity"), FString::FromInt(Instance.Quantity));
    Result.AddResultData(TEXT("InstanceID"), InstanceID.ToString());

    return Result;
}

TArray<FSuspenseInventoryItemInstance> USuspenseInventoryComponent::GetAllItemInstances() const
{
    if (!StorageComponent || !bIsInitialized)
    {
        return TArray<FSuspenseInventoryItemInstance>();
    }

    return StorageComponent->GetAllItemInstances();
}

USuspenseItemManager* USuspenseInventoryComponent::GetItemManager() const
{
    // Check cached manager first
    if (CachedItemManager.IsValid())
    {
        return CachedItemManager.Get();
    }

    // Get from game instance through owner
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        INVENTORY_LOG(Warning, TEXT("GetItemManager: No valid owner"));
        return nullptr;
    }

    UGameInstance* GameInstance = Owner->GetGameInstance();
    if (!GameInstance)
    {
        INVENTORY_LOG(Warning, TEXT("GetItemManager: No valid game instance"));
        return nullptr;
    }

    USuspenseItemManager* Manager = GameInstance->GetSubsystem<USuspenseItemManager>();
    if (Manager)
    {
        CachedItemManager = Manager;
    }

    return Manager;
}

//==================================================================
// Advanced Item Management
//==================================================================

int32 USuspenseInventoryComponent::CreateItemsFromSpawnData(const TArray<FSuspensePickupSpawnData>& SpawnDataArray)
{
    if (!CheckAuthority(TEXT("CreateItemsFromSpawnData")))
    {
        return 0;
    }

    int32 SuccessCount = 0;

    for (const FSuspensePickupSpawnData& SpawnData : SpawnDataArray)
    {
        if (!SpawnData.IsValid())
        {
            continue;
        }

        // Create inventory instance from spawn data
        FSuspenseInventoryItemInstance Instance = SpawnData.CreateInventoryInstance();

        // Add to inventory
        FSuspenseInventoryOperationResult Result = AddItemInstance(Instance);
        if (Result.IsSuccess())
        {
            SuccessCount++;
        }
    }

    INVENTORY_LOG(Log, TEXT("CreateItemsFromSpawnData: Created %d/%d items"),
        SuccessCount, SpawnDataArray.Num());

    return SuccessCount;
}

int32 USuspenseInventoryComponent::ConsolidateStacks(const FName& ItemID)
{
    if (!CheckAuthority(TEXT("ConsolidateStacks")) || !StorageComponent)
    {
        return 0;
    }

    // Begin transaction
    BeginTransaction();

    int32 FreedSlots = 0;

    // Get all instances directly from storage
    TArray<FSuspenseInventoryItemInstance> AllInstances = StorageComponent->GetAllItemInstances();

    // Group instances by ID
    TMap<FName, TArray<int32>> InstanceIndicesByID;

    for (int32 i = 0; i < AllInstances.Num(); i++)
    {
        const FSuspenseInventoryItemInstance& Instance = AllInstances[i];
        if (!Instance.IsValid())
        {
            continue;
        }

        FName CurrentItemID = Instance.ItemID;

        // Filter by specific ID if provided
        if (!ItemID.IsNone() && CurrentItemID != ItemID)
        {
            continue;
        }

        InstanceIndicesByID.FindOrAdd(CurrentItemID).Add(i);
    }

    // Get ItemManager for max stack size
    USuspenseItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        RollbackTransaction();
        return 0;
    }

    // Consolidate each item type
    for (auto& Pair : InstanceIndicesByID)
    {
        TArray<int32>& Indices = Pair.Value;
        if (Indices.Num() <= 1)
        {
            continue; // Nothing to consolidate
        }

        // Get max stack size from DataTable
        FSuspenseUnifiedItemData ItemData;
        if (!ItemManager->GetUnifiedItemData(Pair.Key, ItemData))
        {
            continue;
        }

        int32 MaxStack = ItemData.MaxStackSize;
        if (MaxStack <= 1)
        {
            continue; // Not stackable
        }

        // Sort by quantity (descending)
        Indices.Sort([&AllInstances](const int32& A, const int32& B)
        {
            return AllInstances[A].Quantity > AllInstances[B].Quantity;
        });

        // Consolidate stacks
        for (int32 i = 0; i < Indices.Num() - 1; i++)
        {
            FSuspenseInventoryItemInstance& Target = AllInstances[Indices[i]];
            int32 TargetSpace = MaxStack - Target.Quantity;

            if (TargetSpace <= 0)
            {
                continue; // Target stack full
            }

            // Try to fill from other stacks
            for (int32 j = i + 1; j < Indices.Num(); j++)
            {
                FSuspenseInventoryItemInstance& Source = AllInstances[Indices[j]];
                if (Source.Quantity <= 0)
                {
                    continue; // Already empty
                }

                int32 TransferAmount = FMath::Min(Source.Quantity, TargetSpace);

                // Update quantities through storage
                Target.Quantity += TransferAmount;
                Source.Quantity -= TransferAmount;

                // Update instance in storage
                StorageComponent->UpdateItemInstance(Target);

                // If source stack empty, remove it
                if (Source.Quantity <= 0)
                {
                    if (StorageComponent->RemoveItemInstance(Source.InstanceID))
                    {
                        FreedSlots++;
                    }
                }
                else
                {
                    // Update quantity in storage
                    StorageComponent->UpdateItemInstance(Source);
                }

                TargetSpace -= TransferAmount;

                if (TargetSpace <= 0)
                {
                    break; // Target stack now full
                }
            }
        }
    }

    // Commit transaction
    CommitTransaction();

    // Update weight and broadcast
    UpdateCurrentWeight();
    BroadcastInventoryUpdated();

    INVENTORY_LOG(Log, TEXT("ConsolidateStacks: Freed %d slots"), FreedSlots);

    return FreedSlots;
}

FSuspenseInventoryOperationResult USuspenseInventoryComponent::SplitStack(int32 SourceSlot, int32 SplitQuantity, int32 TargetSlot)
{
    if (!CheckAuthority(TEXT("SplitStack")))
    {
        return FSuspenseInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::UnknownError,
            FText::FromString(TEXT("Client cannot split stacks directly")),
            TEXT("SplitStack"),
            nullptr
        );
    }

    if (!StorageComponent || !bIsInitialized)
    {
        return FSuspenseInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::NotInitialized,
            FText::FromString(TEXT("Inventory not initialized")),
            TEXT("SplitStack"),
            nullptr
        );
    }

    // Get source instance
    FSuspenseInventoryItemInstance SourceInstance;
    if (!StorageComponent->GetItemInstanceAt(SourceSlot, SourceInstance))
    {
        return FSuspenseInventoryOperationResult::ItemNotFound(TEXT("SplitStack"));
    }

    // Validate split quantity
    if (SplitQuantity <= 0 || SplitQuantity >= SourceInstance.Quantity)
    {
        return FSuspenseInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::InsufficientQuantity,
            FText::Format(FText::FromString(TEXT("Invalid split quantity: {0} (source has {1})")),
                SplitQuantity, SourceInstance.Quantity),
            TEXT("SplitStack"),
            nullptr
        );
    }

    // Check if item is stackable
    USuspenseItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        return FSuspenseInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::UnknownError,
            FText::FromString(TEXT("ItemManager not available")),
            TEXT("SplitStack"),
            nullptr
        );
    }

    FSuspenseUnifiedItemData ItemData;
    if (!ItemManager->GetUnifiedItemData(SourceInstance.ItemID, ItemData))
    {
        return FSuspenseInventoryOperationResult::ItemNotFound(TEXT("SplitStack"), SourceInstance.ItemID);
    }

    if (ItemData.MaxStackSize <= 1)
    {
        return FSuspenseInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::InvalidItem,
            FText::FromString(TEXT("Item is not stackable")),
            TEXT("SplitStack"),
            nullptr
        );
    }

    // Check target slot is free
    if (!StorageComponent->AreCellsFreeForItem(TargetSlot, SourceInstance.ItemID, SourceInstance.bIsRotated))
    {
        return FSuspenseInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::SlotOccupied,
            FText::FromString(TEXT("Target slot is occupied")),
            TEXT("SplitStack"),
            nullptr
        );
    }

    // Begin transaction
    BeginTransaction();

    // Create new instance for split stack
    FSuspenseInventoryItemInstance SplitInstance = SourceInstance;
    SplitInstance.InstanceID = FGuid::NewGuid();
    SplitInstance.Quantity = SplitQuantity;
    SplitInstance.AnchorIndex = TargetSlot;

    // Place new stack
    if (!StorageComponent->PlaceItemInstance(SplitInstance, TargetSlot))
    {
        RollbackTransaction();
        return FSuspenseInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::UnknownError,
            FText::FromString(TEXT("Failed to place split stack")),
            TEXT("SplitStack"),
            nullptr
        );
    }

    // Update source stack
    SourceInstance.Quantity -= SplitQuantity;
    if (!StorageComponent->UpdateItemInstance(SourceInstance))
    {
        RollbackTransaction();
        return FSuspenseInventoryOperationResult::Failure(
            ESuspenseInventoryErrorCode::UnknownError,
            FText::FromString(TEXT("Failed to update source stack")),
            TEXT("SplitStack"),
            nullptr
        );
    }

    // Commit transaction
    CommitTransaction();

    // Notify replicator
    NotifyItemPlaced(SplitInstance, TargetSlot);

    // Broadcast events
    ISuspenseInventory::BroadcastItemAdded(this, SplitInstance, TargetSlot);
    BroadcastInventoryUpdated();

    // Success result
    FSuspenseInventoryOperationResult Result = FSuspenseInventoryOperationResult::Success(TEXT("SplitStack"));
    Result.AddResultData(TEXT("SourceSlot"), FString::FromInt(SourceSlot));
    Result.AddResultData(TEXT("TargetSlot"), FString::FromInt(TargetSlot));
    Result.AddResultData(TEXT("SplitQuantity"), FString::FromInt(SplitQuantity));

    return Result;
}

//==================================================================
// Legacy Support Implementation
//==================================================================

bool USuspenseInventoryComponent::AddItem(const FSuspenseUnifiedItemData& ItemData, int32 Amount)
{
    ESuspenseInventoryErrorCode ErrorCode;
    return AddItemWithErrorCode(ItemData, Amount, ErrorCode);
}

bool USuspenseInventoryComponent::AddItemWithErrorCode(const FSuspenseUnifiedItemData& ItemData, int32 Amount, ESuspenseInventoryErrorCode& OutErrorCode)
{
    // Check authority
    if (!CheckAuthority(TEXT("AddItemWithErrorCode")))
    {
        Server_AddItemByID(ItemData.ItemID, Amount);
        OutErrorCode = ESuspenseInventoryErrorCode::UnknownError;
        return false;
    }

    // Validate components
    if (!StorageComponent || !ConstraintsComponent || !TransactionComponent)
    {
        OutErrorCode = ESuspenseInventoryErrorCode::NotInitialized;
        INVENTORY_LOG(Error, TEXT("Components not initialized"));
        return false;
    }

    // Begin transaction
    if (!TransactionComponent->BeginTransaction(EInventoryTransactionType::Add, TEXT("AddItem")))
    {
        OutErrorCode = ESuspenseInventoryErrorCode::UnknownError;
        INVENTORY_LOG(Error, TEXT("Failed to begin transaction"));
        return false;
    }

    // ARCHITECTURE FIX: Pass WorldContextObject (this) as first parameter to InventoryUtils
    // This ensures ItemManager can be properly accessed for item data and initialization
    // Component inherits from UObject so 'this' is valid WorldContextObject
    FSuspenseInventoryItemInstance NewInstance = InventoryUtils::CreateItemInstance(this, ItemData.ItemID, Amount);

    // Apply any special properties from unified data
    if (ItemData.bIsWeapon && ItemData.AmmoType.IsValid())
    {
        // Weapon-specific initialization already handled by InventoryUtils
        INVENTORY_LOG(VeryVerbose, TEXT("Weapon with ammo type: %s"), *ItemData.AmmoType.ToString());
    }

    // Add instance
    FSuspenseInventoryOperationResult Result = AddItemInstance(NewInstance);

    if (Result.IsSuccess())
    {
        TransactionComponent->CommitTransaction();
        OutErrorCode = ESuspenseInventoryErrorCode::Success;
        return true;
    }
    else
    {
        TransactionComponent->RollbackTransaction();
        OutErrorCode = Result.ErrorCode;
        return false;
    }
}

bool USuspenseInventoryComponent::TryAddItem_Implementation(const FSuspenseUnifiedItemData& ItemData, int32 Quantity)
{
    return AddItem(ItemData, Quantity);
}

bool USuspenseInventoryComponent::RemoveItem(const FName& ItemID, int32 Amount)
{
    // Check authority
    if (!CheckAuthority(TEXT("RemoveItem")))
    {
        Server_RemoveItem(ItemID, Amount);
        return false;
    }

    // Validate components
    if (!StorageComponent || !TransactionComponent)
    {
        INVENTORY_LOG(Error, TEXT("Components not initialized"));
        return false;
    }

    // Begin transaction
    if (!TransactionComponent->BeginTransaction(EInventoryTransactionType::Remove, TEXT("RemoveItem")))
    {
        INVENTORY_LOG(Error, TEXT("Failed to begin transaction"));
        return false;
    }

    TArray<FSuspenseInventoryItemInstance> AllInstances = GetAllItemInstances();
    TArray<FSuspenseInventoryItemInstance> MatchingInstances;
    int32 RemainingAmount = Amount;

    // Find matching instances
    for (const FSuspenseInventoryItemInstance& Instance : AllInstances)
    {
        if (Instance.IsValid() && Instance.ItemID == ItemID)
        {
            MatchingInstances.Add(Instance);
        }
    }

    if (MatchingInstances.Num() == 0)
    {
        TransactionComponent->RollbackTransaction();
        INVENTORY_LOG(Warning, TEXT("No items found with ID: %s"), *ItemID.ToString());

        ISuspenseInventory::BroadcastInventoryError(this, ESuspenseInventoryErrorCode::ItemNotFound,
            FString::Printf(TEXT("Item not found: %s"), *ItemID.ToString()));
        return false;
    }

    // Process removal
    float TotalWeightReduction = 0.0f;

    // Get ItemManager for weight data
    USuspenseItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        TransactionComponent->RollbackTransaction();
        return false;
    }

    for (const FSuspenseInventoryItemInstance& Instance : MatchingInstances)
    {
        if (RemainingAmount <= 0)
        {
            break;
        }

        // Get item data for weight
        FSuspenseUnifiedItemData ItemData;
        if (!ItemManager->GetUnifiedItemData(ItemID, ItemData))
        {
            continue;
        }

        if (Instance.Quantity <= RemainingAmount)
        {
            // Remove entire stack
            if (StorageComponent->RemoveItemInstance(Instance.InstanceID))
            {
                RemainingAmount -= Instance.Quantity;
                TotalWeightReduction += ItemData.Weight * Instance.Quantity;

                // Notify replicator
                NotifyItemRemoved(Instance);
            }
        }
        else
        {
            // Remove part of stack
            FSuspenseInventoryItemInstance UpdatedInstance = Instance;
            UpdatedInstance.Quantity -= RemainingAmount;

            if (StorageComponent->UpdateItemInstance(UpdatedInstance))
            {
                TotalWeightReduction += ItemData.Weight * RemainingAmount;
                RemainingAmount = 0;
            }
        }
    }

    // Update weight
    CurrentWeight = FMath::Max(0.0f, CurrentWeight - TotalWeightReduction);

    // Commit transaction
    if (!TransactionComponent->CommitTransaction())
    {
        INVENTORY_LOG(Error, TEXT("Failed to commit transaction"));
        return false;
    }

    // Broadcast events
    if (EventsComponent)
    {
        EventsComponent->BroadcastItemRemoved(ItemID, Amount - RemainingAmount);
    }

    BroadcastInventoryUpdated();

    return RemainingAmount == 0;
}

//==================================================================
// Item Reception and Validation
//==================================================================

bool USuspenseInventoryComponent::ReceiveItem_Implementation(const FSuspenseUnifiedItemData& ItemData, int32 Quantity)
{
    return AddItem(ItemData, Quantity);
}

bool USuspenseInventoryComponent::CanReceiveItem_Implementation(const FSuspenseUnifiedItemData& ItemData, int32 Quantity) const
{
    // Validate components
    if (!StorageComponent || !ConstraintsComponent)
    {
        return false;
    }

    // Check allowed types
    if (!AllowedItemTypes.IsEmpty() && !AllowedItemTypes.HasTag(ItemData.ItemType))
    {
        return false;
    }

    // Check weight limit
    float RequiredWeight = ItemData.Weight * Quantity;
    if (CurrentWeight + RequiredWeight > MaxWeight)
    {
        return false;
    }

    // Check space - using proper method for finding free space
    FVector2D ItemSize(ItemData.GridSize.X, ItemData.GridSize.Y);
    int32 FreeSpace = FindFreeSpaceForItem(ItemSize, true);
    return FreeSpace != INDEX_NONE;
}

FGameplayTagContainer USuspenseInventoryComponent::GetAllowedItemTypes_Implementation() const
{
    return AllowedItemTypes;
}

//==================================================================
// Grid Placement Operations
//==================================================================

void USuspenseInventoryComponent::SwapItemSlots(int32 SlotIndex1, int32 SlotIndex2)
{
    if (!CheckAuthority(TEXT("SwapItemSlots")))
    {
        return;
    }

    if (SlotIndex1 == SlotIndex2)
    {
        INVENTORY_LOG(Warning, TEXT("Cannot swap slot with itself"));
        return;
    }

    ESuspenseInventoryErrorCode ErrorCode;
    if (!ExecuteSlotSwap(SlotIndex1, SlotIndex2, ErrorCode))
    {
        ISuspenseInventory::BroadcastInventoryError(this, ErrorCode, TEXT("Swap failed"));
    }
}

int32 USuspenseInventoryComponent::FindFreeSpaceForItem(const FVector2D& ItemSize, bool bAllowRotation) const
{
    if (!StorageComponent)
    {
        return INDEX_NONE;
    }

    // Since we don't have ItemID, we must manually scan the grid
    FVector2D GridSize = GetInventorySize();
    int32 GridWidth = FMath::FloorToInt(GridSize.X);
    int32 GridHeight = FMath::FloorToInt(GridSize.Y);

    // Search for free space in normal orientation
    for (int32 Y = 0; Y <= GridHeight - FMath::CeilToInt(ItemSize.Y); Y++)
    {
        for (int32 X = 0; X <= GridWidth - FMath::CeilToInt(ItemSize.X); X++)
        {
            int32 TestIndex = Y * GridWidth + X;
            // Check all cells this item would occupy
            bool bAllCellsFree = true;

            for (int32 ItemY = 0; ItemY < FMath::CeilToInt(ItemSize.Y) && bAllCellsFree; ItemY++)
            {
                for (int32 ItemX = 0; ItemX < FMath::CeilToInt(ItemSize.X); ItemX++)
                {
                    int32 CellX = X + ItemX;
                    int32 CellY = Y + ItemY;

                    if (CellX >= GridWidth || CellY >= GridHeight)
                    {
                        bAllCellsFree = false;
                        break;
                    }

                    int32 CellIndex = CellY * GridWidth + CellX;

                    // Check if cell is occupied
                    FSuspenseInventoryItemInstance TestInstance;
                    if (StorageComponent->GetItemInstanceAt(CellIndex, TestInstance))
                    {
                        bAllCellsFree = false;
                        break;
                    }
                }
            }

            if (bAllCellsFree)
            {
                return TestIndex;
            }
        }
    }

    // Try rotated if allowed and normal didn't work
    if (bAllowRotation && ItemSize.X != ItemSize.Y)
    {
        FVector2D RotatedSize(ItemSize.Y, ItemSize.X);

        // Search for free space in rotated orientation
        for (int32 Y = 0; Y <= GridHeight - FMath::CeilToInt(RotatedSize.Y); Y++)
        {
            for (int32 X = 0; X <= GridWidth - FMath::CeilToInt(RotatedSize.X); X++)
            {
                int32 TestIndex = Y * GridWidth + X;
                // Check all cells for rotated item
                bool bAllCellsFree = true;

                for (int32 ItemY = 0; ItemY < FMath::CeilToInt(RotatedSize.Y) && bAllCellsFree; ItemY++)
                {
                    for (int32 ItemX = 0; ItemX < FMath::CeilToInt(RotatedSize.X); ItemX++)
                    {
                        int32 CellX = X + ItemX;
                        int32 CellY = Y + ItemY;

                        if (CellX >= GridWidth || CellY >= GridHeight)
                        {
                            bAllCellsFree = false;
                            break;
                        }

                        int32 CellIndex = CellY * GridWidth + CellX;

                        // Check if cell is occupied
                        FSuspenseInventoryItemInstance TestInstance;
                        if (StorageComponent->GetItemInstanceAt(CellIndex, TestInstance))
                        {
                            bAllCellsFree = false;
                            break;
                        }
                    }
                }

                if (bAllCellsFree)
                {
                    return TestIndex;
                }
            }
        }
    }

    return INDEX_NONE;
}

bool USuspenseInventoryComponent::CanPlaceItemAtSlot(const FVector2D& ItemSize, int32 SlotIndex, bool bIgnoreRotation) const
{
    if (!StorageComponent)
    {
        return false;
    }

    FVector2D GridSize = GetInventorySize();
    int32 GridWidth = FMath::FloorToInt(GridSize.X);
    int32 GridHeight = FMath::FloorToInt(GridSize.Y);

    // Get slot coordinates
    int32 SlotX = SlotIndex % GridWidth;
    int32 SlotY = SlotIndex / GridWidth;

    // Check boundaries
    if (SlotX + FMath::CeilToInt(ItemSize.X) > GridWidth ||
        SlotY + FMath::CeilToInt(ItemSize.Y) > GridHeight)
    {
        return false;
    }

    // Check all cells
    for (int32 Y = 0; Y < FMath::CeilToInt(ItemSize.Y); Y++)
    {
        for (int32 X = 0; X < FMath::CeilToInt(ItemSize.X); X++)
        {
            int32 CheckIndex = (SlotY + Y) * GridWidth + (SlotX + X);

            FSuspenseInventoryItemInstance TestInstance;
            if (StorageComponent->GetItemInstanceAt(CheckIndex, TestInstance))
            {
                return false; // Cell occupied
            }
        }
    }

    // Check rotated if allowed
    if (!bIgnoreRotation && ItemSize.X != ItemSize.Y)
    {
        FVector2D RotatedSize(ItemSize.Y, ItemSize.X);

        // Check boundaries for rotated item
        if (SlotX + FMath::CeilToInt(RotatedSize.X) > GridWidth ||
            SlotY + FMath::CeilToInt(RotatedSize.Y) > GridHeight)
        {
            return false;
        }

        // Check all cells for rotated item
        for (int32 Y = 0; Y < FMath::CeilToInt(RotatedSize.Y); Y++)
        {
            for (int32 X = 0; X < FMath::CeilToInt(RotatedSize.X); X++)
            {
                int32 CheckIndex = (SlotY + Y) * GridWidth + (SlotX + X);

                FSuspenseInventoryItemInstance TestInstance;
                if (StorageComponent->GetItemInstanceAt(CheckIndex, TestInstance))
                {
                    return false; // Cell occupied
                }
            }
        }
    }

    return true;
}

bool USuspenseInventoryComponent::TryAutoPlaceItemInstance(const FSuspenseInventoryItemInstance& ItemInstance)
{
    if (!CheckAuthority(TEXT("TryAutoPlaceItemInstance")) || !StorageComponent)
    {
        return false;
    }

    // Try to add through storage with automatic placement
    FSuspenseInventoryOperationResult Result = AddItemInstance(ItemInstance);
    return Result.IsSuccess();
}

//==================================================================
// Movement Operations
//==================================================================

bool USuspenseInventoryComponent::MoveItemBySlots_Implementation(int32 FromSlot, int32 ToSlot, bool bMaintainRotation)
{
    // Check for same slot move
    if (FromSlot == ToSlot)
    {
        return true;
    }

    // Check authority
    if (!CheckAuthority(TEXT("MoveItemBySlots")))
    {
        return false;
    }

    // Check initialization
    if (!StorageComponent || !bIsInitialized)
    {
        UE_LOG(LogInventory, Error, TEXT("MoveItemBySlots: Storage not initialized"));
        return false;
    }

    // Get instance from source slot
    FSuspenseInventoryItemInstance SourceInstance;
    if (!StorageComponent->GetItemInstanceAt(FromSlot, SourceInstance))
    {
        UE_LOG(LogInventory, Warning, TEXT("MoveItemBySlots: No item at source slot %d"), FromSlot);
        return false;
    }

    // Check if target slot has item
    FSuspenseInventoryItemInstance TargetInstance;
    bool bHasTargetItem = StorageComponent->GetItemInstanceAt(ToSlot, TargetInstance);

    if (bHasTargetItem)
    {
        // Target has item - execute swap
        UE_LOG(LogInventory, Log, TEXT("MoveItemBySlots: Swapping items between slots %d and %d"),
            FromSlot, ToSlot);

        // Use ExecuteSlotSwap for swap
        ESuspenseInventoryErrorCode ErrorCode;
        return ExecuteSlotSwap(FromSlot, ToSlot, ErrorCode);
    }
    else
    {
        // Target empty - simple move
        UE_LOG(LogInventory, Log, TEXT("MoveItemBySlots: Moving item from slot %d to empty slot %d"),
            FromSlot, ToSlot);

        // Determine desired rotation
        bool bDesiredRotation = bMaintainRotation ? SourceInstance.bIsRotated : false;

        // Use Storage method for move
        if (!StorageComponent->MoveItem(SourceInstance.InstanceID, ToSlot, !bMaintainRotation))
        {
            UE_LOG(LogInventory, Warning, TEXT("MoveItemBySlots: Move failed"));
            return false;
        }

        // Get updated instance
        FSuspenseInventoryItemInstance UpdatedInstance;
        if (StorageComponent->GetItemInstance(SourceInstance.InstanceID, UpdatedInstance))
        {
            // Notify replicator
            NotifyItemPlaced(UpdatedInstance, ToSlot);

            // Broadcast event
            if (EventsComponent)
            {
                EventsComponent->BroadcastItemMoved(SourceInstance.InstanceID, UpdatedInstance.ItemID, FromSlot, ToSlot);
            }

            // Send event through interface
            ISuspenseInventory::BroadcastItemMoved(this, UpdatedInstance.InstanceID, FromSlot, ToSlot, UpdatedInstance.bIsRotated);
        }
    }

    // Update weight (in case of modifiers)
    UpdateCurrentWeight();

    // Update UI
    BroadcastInventoryUpdated();

    UE_LOG(LogInventory, Log, TEXT("MoveItemBySlots: Successfully completed"));
    return true;
}

bool USuspenseInventoryComponent::MoveItemInstance(const FGuid& InstanceID, int32 NewSlot, bool bAllowRotation)
{
    if (!CheckAuthority(TEXT("MoveItemInstance")))
    {
        return false;
    }

    if (!InstanceID.IsValid() || !StorageComponent || !bIsInitialized)
    {
        return false;
    }

    // Get current instance
    FSuspenseInventoryItemInstance CurrentInstance;
    if (!StorageComponent->GetItemInstance(InstanceID, CurrentInstance))
    {
        return false;
    }

    int32 OldSlot = CurrentInstance.AnchorIndex;

    // Perform move
    if (StorageComponent->MoveItem(InstanceID, NewSlot, bAllowRotation))
    {
        // Get updated instance
        FSuspenseInventoryItemInstance UpdatedInstance;
        if (StorageComponent->GetItemInstance(InstanceID, UpdatedInstance))
        {
            // Notify replicator
            NotifyItemPlaced(UpdatedInstance, NewSlot);

            // Broadcast events
            if (EventsComponent)
            {
                EventsComponent->BroadcastItemMoved(InstanceID, UpdatedInstance.ItemID, OldSlot, NewSlot);
            }

            BroadcastInventoryUpdated();
            return true;
        }
    }

    return false;
}

bool USuspenseInventoryComponent::CanMoveItemToSlot_Implementation(int32 FromSlot, int32 ToSlot, bool bMaintainRotation) const
{
    if (FromSlot == ToSlot)
    {
        return true;
    }

    if (!StorageComponent || !bIsInitialized)
    {
        return false;
    }

    // Get instance from source slot
    FSuspenseInventoryItemInstance SourceInstance;
    if (!StorageComponent->GetItemInstanceAt(FromSlot, SourceInstance))
    {
        return false;
    }

    // Get item size
    USuspenseItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        return false;
    }

    FSuspenseUnifiedItemData ItemData;
    if (!ItemManager->GetUnifiedItemData(SourceInstance.ItemID, ItemData))
    {
        return false;
    }

    FVector2D ItemSize(ItemData.GridSize.X, ItemData.GridSize.Y);
    if (!bMaintainRotation && SourceInstance.bIsRotated)
    {
        // Will be unrotated
        ItemSize = FVector2D(ItemSize.Y, ItemSize.X);
    }

    return CanPlaceItemAtSlot(ItemSize, ToSlot, true);
}

//==================================================================
// Weight Management
//==================================================================

float USuspenseInventoryComponent::GetCurrentWeight_Implementation() const
{
    return CurrentWeight;
}

float USuspenseInventoryComponent::GetMaxWeight_Implementation() const
{
    return MaxWeight;
}

float USuspenseInventoryComponent::GetRemainingWeight_Implementation() const
{
    return MaxWeight - CurrentWeight;
}

bool USuspenseInventoryComponent::HasWeightCapacity_Implementation(float RequiredWeight) const
{
    return (CurrentWeight + RequiredWeight) <= MaxWeight;
}

//==================================================================
// Properties and Queries
//==================================================================

FVector2D USuspenseInventoryComponent::GetInventorySize() const
{
    // Prioritize replicated size for clients
    if (GetOwnerRole() != ROLE_Authority && ReplicatedGridSize.X > 0)
    {
        return ReplicatedGridSize;
    }

    if (StorageComponent)
    {
        return StorageComponent->GetGridSize();
    }

    return ReplicatedGridSize;
}

bool USuspenseInventoryComponent::GetItemInstanceAtSlot(int32 SlotIndex, FSuspenseInventoryItemInstance& OutInstance) const
{
    if (!StorageComponent || !bIsInitialized)
    {
        return false;
    }

    // Get instance from storage
    if (!StorageComponent->GetItemInstanceAt(SlotIndex, OutInstance))
    {
        return false;
    }

    // Verify slot is anchor position
    if (OutInstance.AnchorIndex != SlotIndex)
    {
        return false;
    }

    return true;
}

int32 USuspenseInventoryComponent::GetItemCountByID(const FName& ItemID) const
{
    int32 TotalCount = 0;

    if (!bIsInitialized || ItemID.IsNone())
    {
        return TotalCount;
    }

    TArray<FSuspenseInventoryItemInstance> AllInstances = GetAllItemInstances();

    for (const FSuspenseInventoryItemInstance& Instance : AllInstances)
    {
        if (Instance.IsValid() && Instance.ItemID == ItemID)
        {
            TotalCount += Instance.Quantity;
        }
    }

    return TotalCount;
}

TArray<FSuspenseInventoryItemInstance> USuspenseInventoryComponent::FindItemInstancesByType(const FGameplayTag& ItemType) const
{
    TArray<FSuspenseInventoryItemInstance> Result;

    if (!bIsInitialized || !ItemType.IsValid())
    {
        return Result;
    }

    TArray<FSuspenseInventoryItemInstance> AllInstances = GetAllItemInstances();

    USuspenseItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        return Result;
    }

    // For each instance check type
    for (const FSuspenseInventoryItemInstance& Instance : AllInstances)
    {
        if (!Instance.IsValid())
        {
            continue;
        }

        // Get item data to check type
        FSuspenseUnifiedItemData ItemData;
        if (ItemManager->GetUnifiedItemData(Instance.ItemID, ItemData))
        {
            if (ItemData.ItemType.MatchesTag(ItemType))
            {
                Result.Add(Instance);
            }
        }
    }

    return Result;
}

int32 USuspenseInventoryComponent::GetTotalItemCount() const
{
    if (!bIsInitialized)
    {
        return 0;
    }

    TArray<FSuspenseInventoryItemInstance> AllInstances = GetAllItemInstances();
    return AllInstances.Num();
}

bool USuspenseInventoryComponent::HasItem(const FName& ItemID, int32 Amount) const
{
    return GetItemCountByID(ItemID) >= Amount;
}

//==================================================================
// UI Support
//==================================================================

bool USuspenseInventoryComponent::SwapItemsInSlots(int32 Slot1, int32 Slot2, ESuspenseInventoryErrorCode& OutErrorCode)
{
    if (Slot1 == Slot2)
    {
        OutErrorCode = ESuspenseInventoryErrorCode::Success;
        return true;
    }

    if (!CheckAuthority(TEXT("SwapItemsInSlots")))
    {
        OutErrorCode = ESuspenseInventoryErrorCode::UnknownError;
        return false;
    }

    return ExecuteSlotSwap(Slot1, Slot2, OutErrorCode);
}

bool USuspenseInventoryComponent::CanSwapSlots(int32 Slot1, int32 Slot2) const
{
    if (!StorageComponent || Slot1 == Slot2)
    {
        return false;
    }

    // Get instances from slots
    FSuspenseInventoryItemInstance Instance1, Instance2;
    bool bHasItem1 = StorageComponent->GetItemInstanceAt(Slot1, Instance1);
    bool bHasItem2 = StorageComponent->GetItemInstanceAt(Slot2, Instance2);

    // If both empty
    if (!bHasItem1 && !bHasItem2)
    {
        return false;
    }

    // If one empty - just move
    if (!bHasItem1 || !bHasItem2)
    {
        return true;
    }

    // Both occupied - check swap possibility
    USuspenseItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        return false;
    }

    // Get item sizes
    FSuspenseUnifiedItemData Data1, Data2;
    if (!ItemManager->GetUnifiedItemData(Instance1.ItemID, Data1) ||
        !ItemManager->GetUnifiedItemData(Instance2.ItemID, Data2))
    {
        return false;
    }

    FVector2D Size1(Data1.GridSize.X, Data1.GridSize.Y);
    FVector2D Size2(Data2.GridSize.X, Data2.GridSize.Y);

    if (Instance1.bIsRotated)
    {
        Size1 = FVector2D(Size1.Y, Size1.X);
    }
    if (Instance2.bIsRotated)
    {
        Size2 = FVector2D(Size2.Y, Size2.X);
    }

    // Simple check for 1x1 items
    if (Size1.X == 1 && Size1.Y == 1 && Size2.X == 1 && Size2.Y == 1)
    {
        return true;
    }

    // More complex validation needed for larger items
    return true;
}

bool USuspenseInventoryComponent::RotateItemAtSlot(int32 SlotIndex)
{
    if (!CheckAuthority(TEXT("RotateItemAtSlot")) || !StorageComponent)
    {
        return false;
    }

    // Get instance from slot
    FSuspenseInventoryItemInstance Instance;
    if (!StorageComponent->GetItemInstanceAt(SlotIndex, Instance))
    {
        return false;
    }

    // Check if rotation possible
    if (!CanRotateItemAtSlot(SlotIndex))
    {
        return false;
    }

    // Update rotation state
    Instance.bIsRotated = !Instance.bIsRotated;

    // Apply changes
    if (StorageComponent->UpdateItemInstance(Instance))
    {
        // Broadcast events
        if (EventsComponent)
        {
            EventsComponent->BroadcastItemRotated(Instance.InstanceID, SlotIndex, Instance.bIsRotated);
        }

        BroadcastInventoryUpdated();
        return true;
    }

    return false;
}

bool USuspenseInventoryComponent::CanRotateItemAtSlot(int32 SlotIndex) const
{
    if (!StorageComponent)
    {
        return false;
    }

    // Get instance
    FSuspenseInventoryItemInstance Instance;
    if (!StorageComponent->GetItemInstanceAt(SlotIndex, Instance))
    {
        return false;
    }

    // Get item size
    USuspenseItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        return false;
    }

    FSuspenseUnifiedItemData ItemData;
    if (!ItemManager->GetUnifiedItemData(Instance.ItemID, ItemData))
    {
        return false;
    }

    // Square items cannot rotate
    if (ItemData.GridSize.X == ItemData.GridSize.Y)
    {
        return false;
    }

    // Check if rotated item fits
    FVector2D CurrentSize(ItemData.GridSize.X, ItemData.GridSize.Y);
    if (Instance.bIsRotated)
    {
        CurrentSize = FVector2D(CurrentSize.Y, CurrentSize.X);
    }

    FVector2D RotatedSize(CurrentSize.Y, CurrentSize.X);

    // Check with exclusion of same item
    return ValidateItemPlacement(RotatedSize, SlotIndex, Instance.InstanceID);
}

void USuspenseInventoryComponent::RefreshItemsUI()
{
    // Broadcast general update
    BroadcastInventoryUpdated();

    // Notify UI connector
    if (UIAdapter)
    {
        UIAdapter->RefreshUI();
    }
}

//==================================================================
// Transaction Support
//==================================================================

void USuspenseInventoryComponent::BeginTransaction()
{
    if (TransactionComponent)
    {
        TransactionComponent->BeginTransaction(EInventoryTransactionType::Custom, TEXT("UserTransaction"));
    }
}

void USuspenseInventoryComponent::CommitTransaction()
{
    if (TransactionComponent)
    {
        TransactionComponent->CommitTransaction();
    }
}

void USuspenseInventoryComponent::RollbackTransaction()
{
    if (TransactionComponent)
    {
        TransactionComponent->RollbackTransaction();
    }
}

bool USuspenseInventoryComponent::IsTransactionActive() const
{
    return TransactionComponent ? TransactionComponent->IsTransactionActive() : false;
}

//==================================================================
// Initialization
//==================================================================

bool USuspenseInventoryComponent::InitializeFromLoadout(const FName& LoadoutID, const FName& InventoryName)
{
    // Get inventory manager
    USuspenseInventoryManager* InvManager = GetInventoryManager();
    if (!InvManager)
    {
        INVENTORY_LOG(Error, TEXT("InitializeFromLoadout: InventoryManager not available"));
        return false;
    }

    // Get loadout configuration
    FLoadoutConfiguration LoadoutConfig;
    if (!InvManager->GetLoadoutConfiguration(LoadoutID, LoadoutConfig))
    {
        INVENTORY_LOG(Warning, TEXT("InitializeFromLoadout: Loadout '%s' not found"), *LoadoutID.ToString());
        return false;
    }

    // Get inventory config
    const FSuspenseInventoryConfig* InvConfig = LoadoutConfig.GetInventoryConfig(InventoryName);
    if (!InvConfig)
    {
        INVENTORY_LOG(Error, TEXT("InitializeFromLoadout: Inventory '%s' not found in loadout"), *InventoryName.ToString());
        return false;
    }

    // Initialize components
    if (!StorageComponent)
    {
        InitializeSubComponents();
    }

    // Server initialization
    if (GetOwnerRole() == ROLE_Authority)
    {
        // Initialize storage
        if (!StorageComponent->InitializeGrid(InvConfig->Width, InvConfig->Height, InvConfig->MaxWeight))
        {
            INVENTORY_LOG(Error, TEXT("Failed to initialize storage grid"));
            return false;
        }

        // Set properties
        MaxWeight = InvConfig->MaxWeight;
        CurrentWeight = 0.0f;
        AllowedItemTypes = InvConfig->AllowedItemTypes;
        CurrentLoadoutID = LoadoutID;
        CurrentInventoryName = InventoryName;

        // Store for replication
        ReplicatedGridSize = FVector2D(InvConfig->Width, InvConfig->Height);

        // Initialize constraints
        if (ConstraintsComponent)
        {
            ConstraintsComponent->Initialize(MaxWeight, AllowedItemTypes, InvConfig->Width, InvConfig->Height);
        }

        // Initialize replicator
        if (ReplicatorComponent)
        {
            ReplicatorComponent->Initialize(InvConfig->Width, InvConfig->Height);
        }

        // Mark as initialized
        bIsInitialized = true;
        bIsInitializedReplicated = true;

        // Cache loadout config
        CachedLoadoutConfig = LoadoutConfig;

        // Create starting items
        if (InvConfig->StartingItems.Num() > 0)
        {
            CreateItemsFromSpawnData(InvConfig->StartingItems);
        }

        // Broadcast initialization
        if (EventsComponent)
        {
            EventsComponent->BroadcastInitialized();
        }

        BroadcastInventoryUpdated();

        INVENTORY_LOG(Log, TEXT("Initialized from loadout '%s', inventory '%s'"),
            *LoadoutID.ToString(), *InventoryName.ToString());

        return true;
    }
    else
    {
        // Client waits for replication
        INVENTORY_LOG(Log, TEXT("Client: Waiting for loadout initialization from server"));
        return true;
    }
}

bool USuspenseInventoryComponent::InitializeWithSimpleSettings(int32 Width, int32 Height, float MaxWeightLimit, const FGameplayTagContainer& AllowedTypes)
{
    if (!CheckAuthority(TEXT("InitializeWithSimpleSettings")))
    {
        INVENTORY_LOG(Warning, TEXT("InitializeWithSimpleSettings: Only server can initialize inventory"));
        return false;
    }

    INVENTORY_LOG(Log, TEXT("InitializeWithSimpleSettings: Simple initialization %dx%d, weight:%.1f"), Width, Height, MaxWeightLimit);

    // Initialize components if needed
    if (!StorageComponent)
    {
        InitializeSubComponents();
    }

    // Validate parameters
    if (Width <= 0 || Height <= 0 || MaxWeightLimit <= 0.0f)
    {
        INVENTORY_LOG(Error, TEXT("InitializeWithSimpleSettings: Invalid parameters - Width:%d, Height:%d, Weight:%.1f"),
            Width, Height, MaxWeightLimit);
        return false;
    }

    // Initialize storage component
    if (!StorageComponent->InitializeGrid(Width, Height, MaxWeightLimit))
    {
        INVENTORY_LOG(Error, TEXT("InitializeWithSimpleSettings: Failed to initialize storage grid"));
        return false;
    }

    // Set basic properties
    MaxWeight = MaxWeightLimit;
    CurrentWeight = 0.0f;
    AllowedItemTypes = AllowedTypes;

    // Store for replication
    ReplicatedGridSize = FVector2D(Width, Height);

    // Initialize constraints using the proper method
    if (ConstraintsComponent)
    {
        ConstraintsComponent->Initialize(MaxWeight, AllowedItemTypes, Width, Height);
    }

    // Initialize replicator if available
    if (ReplicatorComponent)
    {
        USuspenseItemManager* ItemManager = GetItemManager();
        ReplicatorComponent->Initialize(Width, Height, ItemManager);
    }

    // Mark as initialized
    bIsInitialized = true;
    bIsInitializedReplicated = true;

    // Clear loadout info since this is simple initialization
    CurrentLoadoutID = NAME_None;
    CurrentInventoryName = NAME_None;

    // Broadcast initialization
    if (EventsComponent)
    {
        EventsComponent->BroadcastInitialized();
    }

    BroadcastInventoryUpdated();

    INVENTORY_LOG(Log, TEXT("InitializeWithSimpleSettings: Successfully initialized inventory %dx%d"), Width, Height);
    return true;
}

void USuspenseInventoryComponent::SetMaxWeight(float NewMaxWeight)
{
    if (!CheckAuthority(TEXT("SetMaxWeight")))
    {
        return;
    }

    if (NewMaxWeight <= 0.0f)
    {
        INVENTORY_LOG(Warning, TEXT("Invalid max weight: %.1f"), NewMaxWeight);
        return;
    }

    MaxWeight = NewMaxWeight;

    if (ConstraintsComponent)
    {
        ConstraintsComponent->SetMaxWeight(MaxWeight);
    }

    if (GASIntegration)
    {
        GASIntegration->UpdateWeightEffect(CurrentWeight);
    }

    // Broadcast weight change
    USuspenseEventManager* Manager = GetDelegateManager();
    if (Manager)
    {
        FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TEXT("Inventory.Event.WeightChanged"));
        FString EventData = FString::Printf(TEXT("Current:%.1f,Max:%.1f"), CurrentWeight, MaxWeight);
        Manager->NotifyEquipmentEvent(this, EventTag, EventData);
    }

    if (EventsComponent)
    {
        EventsComponent->BroadcastWeightChanged(CurrentWeight);
    }
}

bool USuspenseInventoryComponent::IsInventoryInitialized() const
{
    return bIsInitialized;
}

void USuspenseInventoryComponent::SetAllowedItemTypes(const FGameplayTagContainer& Types)
{
    if (!CheckAuthority(TEXT("SetAllowedItemTypes")))
    {
        return;
    }

    AllowedItemTypes = Types;

    // Update constraints component using the proper Initialize method
    // This follows the same pattern as InitializeFromLoadout
    if (ConstraintsComponent && bIsInitialized)
    {
        // Get current grid size for re-initialization
        FVector2D CurrentGridSize = GetInventorySize();
        int32 Width = FMath::FloorToInt(CurrentGridSize.X);
        int32 Height = FMath::FloorToInt(CurrentGridSize.Y);

        // Re-initialize constraints with new allowed types
        // This ensures all constraint rules are properly updated
        ConstraintsComponent->Initialize(MaxWeight, AllowedItemTypes, Width, Height);

        INVENTORY_LOG(Log, TEXT("SetAllowedItemTypes: Updated constraints with new allowed types"));
    }
    else if (!bIsInitialized)
    {
        INVENTORY_LOG(VeryVerbose, TEXT("SetAllowedItemTypes: Inventory not initialized yet, types will be applied during initialization"));
    }
}

void USuspenseInventoryComponent::InitializeInventory(const FSuspenseInventoryConfig& Config)
{
    // Check authority - only server can initialize inventory
    if (!CheckAuthority(TEXT("InitializeInventory")))
    {
        INVENTORY_LOG(Warning, TEXT("InitializeInventory: Only server can initialize inventory"));
        return;
    }

    // Log initialization start
    INVENTORY_LOG(Log, TEXT("InitializeInventory: Starting initialization - %s, Size: %dx%d, Weight: %.1f"),
        *Config.InventoryName.ToString(), Config.Width, Config.Height, Config.MaxWeight);

    // Validate configuration
    if (!Config.IsValid())
    {
        INVENTORY_LOG(Error, TEXT("InitializeInventory: Invalid inventory configuration"));
        return;
    }

    // Check if already initialized
    if (bIsInitialized)
    {
        INVENTORY_LOG(Warning, TEXT("InitializeInventory: Inventory already initialized. Skipping re-initialization."));
        return;
    }

    // Initialize components if not created
    if (!StorageComponent)
    {
        InitializeSubComponents();
    }

    // Initialize storage with sizes from configuration
    if (!StorageComponent->InitializeGrid(Config.Width, Config.Height, Config.MaxWeight))
    {
        INVENTORY_LOG(Error, TEXT("InitializeInventory: Failed to initialize storage grid"));
        return;
    }

    // Set basic properties from configuration
    MaxWeight = Config.MaxWeight;
    CurrentWeight = 0.0f;
    AllowedItemTypes = Config.AllowedItemTypes;

    // Save grid size for client replication
    ReplicatedGridSize = FVector2D(Config.Width, Config.Height);

    // Initialize constraint system
    if (ConstraintsComponent)
    {
        ConstraintsComponent->Initialize(MaxWeight, AllowedItemTypes, Config.Width, Config.Height);
    }

    // Initialize replicator for multiplayer
    if (ReplicatorComponent)
    {
        USuspenseItemManager* ItemManager = GetItemManager();
        ReplicatorComponent->Initialize(Config.Width, Config.Height, ItemManager);
    }

    // Set initialization flags
    bIsInitialized = true;
    bIsInitializedReplicated = true;

    // Clear loadout info since using direct initialization
    CurrentLoadoutID = NAME_None;
    CurrentInventoryName = NAME_None;

    // Create starting items if specified
    if (Config.StartingItems.Num() > 0)
    {
        INVENTORY_LOG(Log, TEXT("InitializeInventory: Creating %d starting items"),
            Config.StartingItems.Num());

        int32 CreatedCount = CreateItemsFromSpawnData(Config.StartingItems);

        if (CreatedCount != Config.StartingItems.Num())
        {
            INVENTORY_LOG(Warning, TEXT("InitializeInventory: Created only %d of %d starting items"),
                CreatedCount, Config.StartingItems.Num());
        }
    }

    // Apply additional restrictions from configuration
    if (!Config.DisallowedItemTypes.IsEmpty())
    {
        INVENTORY_LOG(Log, TEXT("InitializeInventory: Applied DisallowedItemTypes restrictions (%d types)"),
            Config.DisallowedItemTypes.Num());
    }

    // Notify subsystems about completion
    if (EventsComponent)
    {
        EventsComponent->BroadcastInitialized();
    }

    // Broadcast general inventory update
    BroadcastInventoryUpdated();

    // Final message about successful initialization
    INVENTORY_LOG(Log, TEXT("InitializeInventory: Inventory '%s' successfully initialized. Size: %dx%d, Max weight: %.1f kg"),
        *Config.InventoryName.ToString(), Config.Width, Config.Height, Config.MaxWeight);
}

//==================================================================
// Events & Delegates
//==================================================================

void USuspenseInventoryComponent::BroadcastInventoryUpdated()
{
    // Broadcast through delegate manager
    USuspenseEventManager* Manager = GetDelegateManager();
    if (Manager)
    {
        Manager->NotifyEquipmentUpdated();

        FGameplayTag UpdateTag = FGameplayTag::RequestGameplayTag(TEXT("Inventory.Event.Updated"));
        Manager->NotifyUIEvent(this, UpdateTag, TEXT(""));
    }

    // Broadcast local delegate
    OnInventoryUpdated.Broadcast();

    // Request replication update
    if (ReplicatorComponent)
    {
        ReplicatorComponent->RequestNetUpdate();
    }
}

USuspenseEventManager* USuspenseInventoryComponent::GetDelegateManager() const
{
    if (CachedDelegateManager.IsValid())
    {
        return CachedDelegateManager.Get();
    }

    USuspenseEventManager* Manager = ISuspenseInventory::GetDelegateManagerStatic(this);
    if (Manager)
    {
        CachedDelegateManager = Manager;
    }

    return Manager;
}

void USuspenseInventoryComponent::BindToInventoryUpdates(const FSuspenseOnInventoryUpdated::FDelegate& Delegate)
{
    OnInventoryUpdated.Add(Delegate);
}

void USuspenseInventoryComponent::UnbindFromInventoryUpdates(const FSuspenseOnInventoryUpdated::FDelegate& Delegate)
{
    OnInventoryUpdated.Remove(Delegate);
}

//==================================================================
// Debug & Utility
//==================================================================

bool USuspenseInventoryComponent::GetInventoryCoordinates(int32 Index, int32& OutX, int32& OutY) const
{
    if (StorageComponent)
    {
        return StorageComponent->GetGridCoordinates(Index, OutX, OutY);
    }

    OutX = -1;
    OutY = -1;
    return false;
}

int32 USuspenseInventoryComponent::GetIndexFromCoordinates(int32 X, int32 Y) const
{
    if (StorageComponent)
    {
        int32 Index;
        if (StorageComponent->GetLinearIndex(X, Y, Index))
        {
            return Index;
        }
    }

    return INDEX_NONE;
}

int32 USuspenseInventoryComponent::GetFlatIndexForItem(int32 AnchorIndex, const FVector2D& ItemSize, bool bIsRotated) const
{
    // This would calculate the flat index considering item size
    // For now, return anchor index
    return AnchorIndex;
}

TArray<int32> USuspenseInventoryComponent::GetOccupiedSlots(int32 AnchorIndex, const FVector2D& ItemSize, bool bIsRotated) const
{
    TArray<int32> OccupiedSlots;

    if (!StorageComponent || AnchorIndex == INDEX_NONE)
    {
        return OccupiedSlots;
    }

    // Get grid coordinates
    int32 AnchorX, AnchorY;
    if (!StorageComponent->GetGridCoordinates(AnchorIndex, AnchorX, AnchorY))
    {
        return OccupiedSlots;
    }

    // Calculate size
    FVector2D EffectiveSize = bIsRotated ? FVector2D(ItemSize.Y, ItemSize.X) : ItemSize;
    int32 Width = FMath::CeilToInt(EffectiveSize.X);
    int32 Height = FMath::CeilToInt(EffectiveSize.Y);

    // Get all occupied slots
    for (int32 Y = 0; Y < Height; Y++)
    {
        for (int32 X = 0; X < Width; X++)
        {
            int32 SlotIndex;
            if (StorageComponent->GetLinearIndex(AnchorX + X, AnchorY + Y, SlotIndex))
            {
                OccupiedSlots.Add(SlotIndex);
            }
        }
    }

    return OccupiedSlots;
}

FString USuspenseInventoryComponent::GetInventoryDebugInfo() const
{
    FString DebugInfo = FString::Printf(
        TEXT("=== Inventory Component Debug Info ===\n")
        TEXT("Initialized: %s\n")
        TEXT("Grid Size: %.0fx%.0f\n")
        TEXT("Weight: %.1f / %.1f\n")
        TEXT("Items: %d\n")
        TEXT("Loadout: %s\n")
        TEXT("Inventory Name: %s\n"),
        bIsInitialized ? TEXT("Yes") : TEXT("No"),
        ReplicatedGridSize.X, ReplicatedGridSize.Y,
        CurrentWeight, MaxWeight,
        GetTotalItemCount(),
        *CurrentLoadoutID.ToString(),
        *CurrentInventoryName.ToString()
    );

    // Add item details
    TArray<FSuspenseInventoryItemInstance> AllInstances = GetAllItemInstances();
    if (AllInstances.Num() > 0)
    {
        DebugInfo += TEXT("\nItems:\n");
        for (const FSuspenseInventoryItemInstance& Instance : AllInstances)
        {
            DebugInfo += FString::Printf(TEXT("  - %s\n"), *Instance.GetDebugString());
        }
    }

    return DebugInfo;
}

bool USuspenseInventoryComponent::ValidateInventoryIntegrity(TArray<FString>& OutErrors) const
{
    OutErrors.Empty();

    if (!bIsInitialized)
    {
        OutErrors.Add(TEXT("Inventory not initialized"));
        return false;
    }

    if (!StorageComponent)
    {
        OutErrors.Add(TEXT("Storage component missing"));
        return false;
    }

    // Validate weight
    float CalculatedWeight = 0.0f;

    TArray<FSuspenseInventoryItemInstance> AllInstances = GetAllItemInstances();

    USuspenseItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        OutErrors.Add(TEXT("ItemManager not available"));
        return false;
    }

    // Check data integrity for each instance
    for (const FSuspenseInventoryItemInstance& Instance : AllInstances)
    {
        if (!Instance.IsValid())
        {
            OutErrors.Add(TEXT("Invalid item instance found"));
            continue;
        }

        // Get item data
        FSuspenseUnifiedItemData ItemData;
        if (!ItemManager->GetUnifiedItemData(Instance.ItemID, ItemData))
        {
            OutErrors.Add(FString::Printf(TEXT("Item data not found for ID: %s"),
                *Instance.ItemID.ToString()));
            continue;
        }

        // Calculate weight
        CalculatedWeight += ItemData.Weight * Instance.Quantity;

        // Verify item actually exists in slot
        if (Instance.AnchorIndex != INDEX_NONE)
        {
            FSuspenseInventoryItemInstance TestInstance;
            if (!StorageComponent->GetItemInstanceAt(Instance.AnchorIndex, TestInstance))
            {
                OutErrors.Add(FString::Printf(TEXT("Item %s not found at expected slot %d"),
                    *Instance.ItemID.ToString(), Instance.AnchorIndex));
            }
        }
    }

    // Check weight consistency
    if (!FMath::IsNearlyEqual(CalculatedWeight, CurrentWeight, 0.01f))
    {
        OutErrors.Add(FString::Printf(TEXT("Weight mismatch: Calculated=%.2f, Current=%.2f"),
            CalculatedWeight, CurrentWeight));
    }

    return OutErrors.Num() == 0;
}

//==================================================================
// Extended API Implementation
//==================================================================

bool USuspenseInventoryComponent::UpdateItemAmount(const FName& ItemID, int32 NewAmount)
{
    // Check modification rights
    if (!CheckAuthority(TEXT("UpdateItemAmount")))
    {
        return false;
    }

    // Basic input validation
    if (!bIsInitialized || ItemID.IsNone() || NewAmount <= 0)
    {
        return false;
    }

    // Get all instances from storage component
    TArray<FSuspenseInventoryItemInstance> AllInstances = StorageComponent->GetAllItemInstances();

    // Find first instance with needed ItemID
    for (FSuspenseInventoryItemInstance& Instance : AllInstances)
    {
        if (Instance.ItemID == ItemID)
        {
            // Update quantity in instance
            Instance.Quantity = NewAmount;

            // Apply changes through storage
            if (StorageComponent->UpdateItemInstance(Instance))
            {
                // Recalculate weight after quantity change
                UpdateCurrentWeight();

                // Notify all subscribers about change
                BroadcastInventoryUpdated();

                INVENTORY_LOG(Log, TEXT("UpdateItemAmount: Updated %s to quantity %d"),
                    *ItemID.ToString(), NewAmount);

                return true;
            }
            else
            {
                INVENTORY_LOG(Error, TEXT("UpdateItemAmount: Failed to update instance in storage"));
            }

            // Update only first found instance
            break;
        }
    }

    INVENTORY_LOG(Warning, TEXT("UpdateItemAmount: Item %s not found in inventory"),
        *ItemID.ToString());

    return false;
}

TArray<FSuspenseInventoryItemInstance> USuspenseInventoryComponent::GetItemInstancesByType(const FGameplayTag& ItemType) const
{
    TArray<FSuspenseInventoryItemInstance> Result;

    if (!bIsInitialized || !ItemType.IsValid())
    {
        return Result;
    }

    USuspenseItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        return Result;
    }

    TArray<FSuspenseInventoryItemInstance> AllInstances = GetAllItemInstances();

    for (const FSuspenseInventoryItemInstance& Instance : AllInstances)
    {
        FSuspenseUnifiedItemData ItemData;
        if (ItemManager->GetUnifiedItemData(Instance.ItemID, ItemData))
        {
            if (ItemData.ItemType.MatchesTag(ItemType))
            {
                Result.Add(Instance);
            }
        }
    }

    return Result;
}

bool USuspenseInventoryComponent::SaveToFile(const FString& FilePath)
{
    if (!bIsInitialized || !SerializerComponent)
    {
        return false;
    }

    return USuspenseInventorySerializer::SaveInventoryToFile(this, FilePath);
}

bool USuspenseInventoryComponent::LoadFromFile(const FString& FilePath)
{
    if (!SerializerComponent)
    {
        return false;
    }

    return SerializerComponent->LoadInventoryFromFile(this, FilePath);
}

const FLoadoutConfiguration* USuspenseInventoryComponent::GetCurrentLoadoutConfig() const
{
    if (CurrentLoadoutID.IsNone())
    {
        return nullptr;
    }

    // Check cache first
    if (CachedLoadoutConfig.IsSet())
    {
        return &CachedLoadoutConfig.GetValue();
    }

    // Get from inventory manager
    USuspenseInventoryManager* InvManager = GetInventoryManager();
    if (InvManager)
    {
        FLoadoutConfiguration Config;
        if (InvManager->GetLoadoutConfiguration(CurrentLoadoutID, Config))
        {
            CachedLoadoutConfig = Config;
            return &CachedLoadoutConfig.GetValue();
        }
    }

    return nullptr;
}

void USuspenseInventoryComponent::LogInventoryStatistics(const FString& Context) const
{
    if (!bIsInitialized)
    {
        UE_LOG(LogInventory, Log, TEXT("[%s] Inventory not initialized"), *Context);
        return;
    }

    FVector2D GridSize = GetInventorySize();
    TArray<FSuspenseInventoryItemInstance> AllInstances = GetAllItemInstances();

    UE_LOG(LogInventory, Log, TEXT("[%s] Inventory Statistics:"), *Context);
    UE_LOG(LogInventory, Log, TEXT("  - Grid: %.0fx%.0f"), GridSize.X, GridSize.Y);
    UE_LOG(LogInventory, Log, TEXT("  - Weight: %.1f / %.1f"), CurrentWeight, MaxWeight);
    UE_LOG(LogInventory, Log, TEXT("  - Items: %d"), AllInstances.Num());
    UE_LOG(LogInventory, Log, TEXT("  - Loadout: %s"), *CurrentLoadoutID.ToString());

    for (const FSuspenseInventoryItemInstance& Instance : AllInstances)
    {
        UE_LOG(LogInventory, Log, TEXT("    - %s"), *Instance.GetDebugString());
    }
}

//==================================================================
// Replication Support
//==================================================================

void USuspenseInventoryComponent::NotifyItemPlaced(const FSuspenseInventoryItemInstance& ItemInstance, int32 AnchorIndex)
{
    if (!ReplicatorComponent || GetOwnerRole() != ROLE_Authority)
    {
        INVENTORY_LOG(VeryVerbose, TEXT("NotifyItemPlaced: Skipped (no replicator or not authority)"));
        return;
    }

    // Validate input parameters
    if (!ItemInstance.IsValid() || AnchorIndex < 0)
    {
        INVENTORY_LOG(Warning, TEXT("NotifyItemPlaced: Invalid parameters - ItemID:%s, Anchor:%d"),
            *ItemInstance.ItemID.ToString(), AnchorIndex);
        return;
    }

    // Use the new AddItemInstance method instead of the old AddItem
    // This method automatically handles size calculation from DataTable
    int32 MetaIndex = ReplicatorComponent->AddItemInstance(ItemInstance, AnchorIndex);

    if (MetaIndex != INDEX_NONE)
    {
        INVENTORY_LOG(Log, TEXT("NotifyItemPlaced: Added %s to replicator at meta index %d (instance:%s)"),
            *ItemInstance.ItemID.ToString(),
            MetaIndex,
            *ItemInstance.InstanceID.ToString().Left(8));

        // Network update is automatically requested by AddItemInstance
    }
    else
    {
        INVENTORY_LOG(Warning, TEXT("NotifyItemPlaced: Failed to add %s to replicator"),
            *ItemInstance.ItemID.ToString());
    }
}

void USuspenseInventoryComponent::NotifyItemRemoved(const FSuspenseInventoryItemInstance& ItemInstance)
{
    if (!ReplicatorComponent || GetOwnerRole() != ROLE_Authority)
    {
        INVENTORY_LOG(VeryVerbose, TEXT("NotifyItemRemoved: Skipped (no replicator or not authority)"));
        return;
    }

    if (!ItemInstance.IsValid())
    {
        INVENTORY_LOG(Warning, TEXT("NotifyItemRemoved: Invalid ItemInstance provided"));
        return;
    }

    // Find the item by instance ID for accurate removal
    int32 MetaIndex = ReplicatorComponent->FindItemByInstanceID(ItemInstance.InstanceID);

    if (MetaIndex != INDEX_NONE)
    {
        // Remove from replicator state
        if (ReplicatorComponent->GetReplicationState().RemoveItem(MetaIndex))
        {
            INVENTORY_LOG(Log, TEXT("NotifyItemRemoved: Removed %s from replicator (meta:%d, instance:%s)"),
                *ItemInstance.ItemID.ToString(),
                MetaIndex,
                *ItemInstance.InstanceID.ToString().Left(8));

            // Request network update to propagate removal
            ReplicatorComponent->RequestNetUpdate();
        }
        else
        {
            INVENTORY_LOG(Warning, TEXT("NotifyItemRemoved: Failed to remove %s from replicator state"),
                *ItemInstance.ItemID.ToString());
        }
    }
    else
    {
        INVENTORY_LOG(Warning, TEXT("NotifyItemRemoved: Could not find %s in replicator (instance:%s)"),
            *ItemInstance.ItemID.ToString(),
            *ItemInstance.InstanceID.ToString().Left(8));
    }
}

//==================================================================
// RPC Methods
//==================================================================

bool USuspenseInventoryComponent::Server_AddItemByID_Validate(const FName& ItemID, int32 Amount)
{
    return !ItemID.IsNone() && Amount > 0;
}

void USuspenseInventoryComponent::Server_AddItemByID_Implementation(const FName& ItemID, int32 Amount)
{
    AddItemByID_Implementation(ItemID, Amount);
}

bool USuspenseInventoryComponent::Server_RemoveItem_Validate(const FName& ItemID, int32 Amount)
{
    return !ItemID.IsNone() && Amount > 0;
}

void USuspenseInventoryComponent::Server_RemoveItem_Implementation(const FName& ItemID, int32 Amount)
{
    RemoveItem(ItemID, Amount);
}

bool USuspenseInventoryComponent::Server_UpdateInventoryState_Validate()
{
    return true;
}

void USuspenseInventoryComponent::Server_UpdateInventoryState_Implementation()
{
    BroadcastInventoryUpdated();
}

//==================================================================
// Internal Helper Methods
//==================================================================

void USuspenseInventoryComponent::InitializeSubComponents()
{
    // Create storage component
    StorageComponent = NewObject<USuspenseInventoryStorage>(this, TEXT("StorageComponent"));

    // Create constraints component
    ConstraintsComponent = NewObject<USuspenseInventoryValidator>(this, TEXT("ConstraintsComponent"));

    // Create transaction component
    TransactionComponent = NewObject<USuspenseInventoryTransaction>(this, TEXT("TransactionComponent"));

    // Create replicator component
    ReplicatorComponent = NewObject<USuspenseInventoryReplicator>(this, TEXT("ReplicatorComponent"));

    // Create events component
    EventsComponent = NewObject<USuspenseInventoryEvents>(this, TEXT("EventsComponent"));

    // Create serializer component
    SerializerComponent = NewObject<USuspenseInventorySerializer>(this, TEXT("SerializerComponent"));

    // Create UI adapter
    UIAdapter = NewObject<USuspenseInventoryUIConnector>(this, TEXT("UIAdapter"));

    // Create GAS integration
    GASIntegration = NewObject<USuspenseInventoryGASIntegration>(this, TEXT("GASIntegration"));

    // Initialize transaction component with proper ItemManager
    if (TransactionComponent)
    {
        // Get ItemManager for transaction component initialization
        USuspenseItemManager* ItemManager = GetItemManager();
        if (!ItemManager)
        {
            INVENTORY_LOG(Warning, TEXT("InitializeSubComponents: ItemManager not available during initialization"));
        }

        // Initialize with storage, constraints, and ItemManager (not events!)
        TransactionComponent->Initialize(StorageComponent, ConstraintsComponent, ItemManager);
    }

    // Initialize replicator component with ItemManager
    if (ReplicatorComponent)
    {
        USuspenseItemManager* ItemManager = GetItemManager();
        // Note: Grid size will be set later during LoadoutConfiguration initialization
        // For now we just set the ItemManager reference
        ReplicatorComponent->SetItemManager(ItemManager);
    }

    // Initialize UI adapter
    if (UIAdapter)
    {
        UIAdapter->SetInventoryComponent(this);
    }

    INVENTORY_LOG(Log, TEXT("Initialized sub-components"));
}

void USuspenseInventoryComponent::InitializeClientComponents()
{
    if (GetOwnerRole() == ROLE_Authority)
    {
        INVENTORY_LOG(Warning, TEXT("InitializeClientComponents called on server"));
        return;
    }

    // Validate replicated data
    int32 Width = FMath::FloorToInt(ReplicatedGridSize.X);
    int32 Height = FMath::FloorToInt(ReplicatedGridSize.Y);

    if (Width <= 0 || Height <= 0)
    {
        INVENTORY_LOG(Error, TEXT("Invalid replicated grid size: %dx%d"), Width, Height);
        return;
    }

    INVENTORY_LOG(Log, TEXT("Client: Initializing with replicated size %dx%d"), Width, Height);

    // Create components if needed
    if (!StorageComponent)
    {
        InitializeSubComponents();
    }

    // Initialize storage
    if (StorageComponent && !StorageComponent->IsInitialized())
    {
        if (!StorageComponent->InitializeGrid(Width, Height, MaxWeight))
        {
            INVENTORY_LOG(Error, TEXT("Client: Failed to initialize storage"));
            return;
        }
    }

    // Initialize constraints
    if (ConstraintsComponent)
    {
        ConstraintsComponent->Initialize(MaxWeight, AllowedItemTypes, Width, Height);
    }

    // Initialize replicator
    if (ReplicatorComponent)
    {
        ReplicatorComponent->Initialize(Width, Height);
    }

    // Mark as initialized
    bIsInitialized = true;

    // Subscribe to replicator updates
    if (!ReplicatorUpdateHandle.IsValid())
    {
        ReplicatorUpdateHandle = ReplicatorComponent->OnReplicationUpdated.AddLambda([this]()
        {
            INVENTORY_LOG(Log, TEXT("Client: Replicator updated"));
            SyncItemsFromReplicator();
        });
    }

    // Initial sync
    SyncItemsFromReplicator();

    // Notify about initialization
    if (EventsComponent)
    {
        EventsComponent->BroadcastInitialized();
    }

    BroadcastInventoryUpdated();

    INVENTORY_LOG(Log, TEXT("Client: Initialization complete"));
}

void USuspenseInventoryComponent::SyncItemsFromReplicator()
{
    if (!ReplicatorComponent || !StorageComponent || GetOwnerRole() == ROLE_Authority)
    {
        return;
    }

    INVENTORY_LOG(Log, TEXT("Client: Starting sync from replicator"));

    // Get replicated state
    const FInventoryReplicatedState& RepState = ReplicatorComponent->GetReplicationState();

    // Clear current items
    StorageComponent->ClearAllItems();

    // Process each replicated item
    int32 SyncedItems = 0;
    for (const FReplicatedItemMeta& Meta : RepState.ItemsState.Items)
    {
        if (Meta.ItemID.IsNone() || Meta.Stack <= 0 || Meta.AnchorIndex < 0)
        {
            continue;
        }

        // Create instance from metadata using factory method with specific InstanceID
        FSuspenseInventoryItemInstance Instance = FSuspenseInventoryItemInstance::CreateWithID(
            Meta.ItemID,
            Meta.InstanceID,
            Meta.Stack
        );
        Instance.bIsRotated = Meta.IsRotated();
        Instance.AnchorIndex = Meta.AnchorIndex;

        // Restore saved ammo state if exists
        if (Meta.HasSavedAmmoState())
        {
            Instance.SetRuntimeProperty(TEXT("SavedCurrentAmmo"), Meta.SavedCurrentAmmo);
            Instance.SetRuntimeProperty(TEXT("SavedRemainingAmmo"), Meta.SavedRemainingAmmo);
            Instance.SetRuntimeProperty(TEXT("HasSavedAmmoState"), 1.0f);
        }

        // Place in storage
        if (StorageComponent->PlaceItemInstance(Instance, Meta.AnchorIndex))
        {
            SyncedItems++;
            INVENTORY_LOG(Verbose, TEXT("Client: Placed %s at slot %d"),
                *Meta.ItemID.ToString(), Meta.AnchorIndex);
        }
        else
        {
            INVENTORY_LOG(Error, TEXT("Client: Failed to place %s at slot %d"),
                *Meta.ItemID.ToString(), Meta.AnchorIndex);
        }
    }

    // Update weight
    CurrentWeight = StorageComponent->GetCurrentWeight();

    INVENTORY_LOG(Log, TEXT("Client: Synced %d items"), SyncedItems);

    // Update UI
    BroadcastInventoryUpdated();
}

bool USuspenseInventoryComponent::CheckAuthority(const FString& FunctionName) const
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        INVENTORY_LOG(Verbose, TEXT("%s requires server authority"), *FunctionName);
        return false;
    }

    return true;
}

void USuspenseInventoryComponent::UpdateCurrentWeight()
{
    if (!bIsInitialized)
    {
        return;
    }

    // Clients use replicated weight
    if (GetOwnerRole() != ROLE_Authority)
    {
        if (EventsComponent)
        {
            EventsComponent->BroadcastWeightChanged(CurrentWeight);
        }

        if (USuspenseEventManager* Manager = GetDelegateManager())
        {
            const FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TEXT("Inventory.Event.WeightChanged"));
            Manager->NotifyUIEvent(this, EventTag, FString::Printf(TEXT("Current:%.2f"), CurrentWeight));
        }

        return;
    }

    // Server calculates weight
    float NewWeight = 0.0f;

    TArray<FSuspenseInventoryItemInstance> AllInstances = GetAllItemInstances();

    // Get ItemManager for weight data access
    USuspenseItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        INVENTORY_LOG(Warning, TEXT("UpdateCurrentWeight: ItemManager not available"));
        return;
    }

    // Calculate total weight
    for (const FSuspenseInventoryItemInstance& Instance : AllInstances)
    {
        if (!Instance.IsValid())
        {
            continue;
        }

        FSuspenseUnifiedItemData ItemData;
        if (ItemManager->GetUnifiedItemData(Instance.ItemID, ItemData))
        {
            // Weight = unit weight * quantity
            NewWeight += ItemData.Weight * Instance.Quantity;
        }
    }

    if (!FMath::IsNearlyEqual(CurrentWeight, NewWeight, 0.01f))
    {
        INVENTORY_LOG(Log, TEXT("Server: Weight changed from %.2f to %.2f"), CurrentWeight, NewWeight);
        CurrentWeight = NewWeight;

        BroadcastInventoryUpdated();

        if (EventsComponent)
        {
            EventsComponent->BroadcastWeightChanged(CurrentWeight);
        }

        if (USuspenseEventManager* Manager = GetDelegateManager())
        {
            const FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TEXT("Inventory.Event.WeightChanged"));
            Manager->NotifyUIEvent(this, EventTag, FString::Printf(TEXT("Current:%.2f"), CurrentWeight));
        }
    }
}

bool USuspenseInventoryComponent::ValidateItemPlacement(const FVector2D& ItemSize, int32 TargetSlot, const FGuid& ExcludeInstanceID) const
{
    if (!StorageComponent)
    {
        return false;
    }

    FVector2D GridSize = GetInventorySize();
    int32 GridWidth = FMath::FloorToInt(GridSize.X);
    int32 GridHeight = FMath::FloorToInt(GridSize.Y);

    // Get target slot coordinates
    int32 TargetX = TargetSlot % GridWidth;
    int32 TargetY = TargetSlot / GridWidth;

    // Check boundaries
    if (TargetX + FMath::CeilToInt(ItemSize.X) > GridWidth ||
        TargetY + FMath::CeilToInt(ItemSize.Y) > GridHeight)
    {
        return false;
    }

    // Check each cell
    for (int32 Y = 0; Y < FMath::CeilToInt(ItemSize.Y); Y++)
    {
        for (int32 X = 0; X < FMath::CeilToInt(ItemSize.X); X++)
        {
            int32 CheckIndex = (TargetY + Y) * GridWidth + (TargetX + X);

            FSuspenseInventoryItemInstance TestInstance;
            if (StorageComponent->GetItemInstanceAt(CheckIndex, TestInstance))
            {
                // Cell occupied - check if excluded item
                if (ExcludeInstanceID.IsValid() && TestInstance.InstanceID == ExcludeInstanceID)
                {
                    continue; // Ignore excluded item
                }

                return false; // Cell occupied by other item
            }
        }
    }

    return true;
}

bool USuspenseInventoryComponent::ExecuteSlotSwap(int32 Slot1, int32 Slot2, ESuspenseInventoryErrorCode& OutErrorCode)
{
    // Basic validation
    if (!StorageComponent)
    {
        OutErrorCode = ESuspenseInventoryErrorCode::NotInitialized;
        UE_LOG(LogInventory, Error, TEXT("ExecuteSlotSwap: Storage component not initialized"));
        return false;
    }

    // Validate slot indices
    FVector2D GridSize = GetInventorySize();
    int32 TotalSlots = FMath::RoundToInt(GridSize.X * GridSize.Y);

    if (Slot1 < 0 || Slot1 >= TotalSlots || Slot2 < 0 || Slot2 >= TotalSlots)
    {
        OutErrorCode = ESuspenseInventoryErrorCode::InvalidSlot;
        UE_LOG(LogInventory, Warning, TEXT("ExecuteSlotSwap: Invalid slot indices - Slot1:%d, Slot2:%d (Max:%d)"),
            Slot1, Slot2, TotalSlots - 1);
        return false;
    }

    UE_LOG(LogInventory, Log, TEXT("ExecuteSlotSwap: Starting swap - Slot %d <-> Slot %d"), Slot1, Slot2);

    // Get instances from slots
    FSuspenseInventoryItemInstance Instance1, Instance2;
    bool bHasItem1 = StorageComponent->GetItemInstanceAt(Slot1, Instance1);
    bool bHasItem2 = StorageComponent->GetItemInstanceAt(Slot2, Instance2);

    // If both slots empty, nothing to swap
    if (!bHasItem1 && !bHasItem2)
    {
        OutErrorCode = ESuspenseInventoryErrorCode::ItemNotFound;
        UE_LOG(LogInventory, Warning, TEXT("ExecuteSlotSwap: Both slots are empty"));
        return false;
    }

    // Begin transaction with more descriptive context
    FString TransactionContext = FString::Printf(TEXT("SwapSlots_%d_%d"), Slot1, Slot2);
    BeginTransaction();

    // Case 1: Moving to empty slot
    if (!bHasItem1 || !bHasItem2)
    {
        int32 SourceSlot = bHasItem1 ? Slot1 : Slot2;
        int32 TargetSlot = bHasItem1 ? Slot2 : Slot1;
        FSuspenseInventoryItemInstance& SourceInstance = bHasItem1 ? Instance1 : Instance2;

        UE_LOG(LogInventory, Log, TEXT("ExecuteSlotSwap: Moving item from slot %d to empty slot %d"),
            SourceSlot, TargetSlot);

        // Validate that item will fit at target position
        USuspenseItemManager* ItemManager = GetItemManager();
        if (ItemManager)
        {
            FSuspenseUnifiedItemData ItemData;
            if (ItemManager->GetUnifiedItemData(SourceInstance.ItemID, ItemData))
            {
                FVector2D ItemSize(ItemData.GridSize.X, ItemData.GridSize.Y);
                if (SourceInstance.bIsRotated)
                {
                    ItemSize = FVector2D(ItemSize.Y, ItemSize.X);
                }

                if (!CanPlaceItemAtSlot(ItemSize, TargetSlot, false))
                {
                    RollbackTransaction();
                    OutErrorCode = ESuspenseInventoryErrorCode::NoSpace;
                    UE_LOG(LogInventory, Warning, TEXT("ExecuteSlotSwap: Item doesn't fit at target slot"));
                    return false;
                }
            }
        }

        // Perform move
        if (StorageComponent->MoveItem(SourceInstance.InstanceID, TargetSlot, false))
        {
            CommitTransaction();

            // Get updated instance for notifications
            FSuspenseInventoryItemInstance UpdatedInstance;
            if (StorageComponent->GetItemInstance(SourceInstance.InstanceID, UpdatedInstance))
            {
                NotifyItemPlaced(UpdatedInstance, TargetSlot);
            }

            // Broadcast events
            if (EventsComponent)
            {
                EventsComponent->BroadcastItemMoved(SourceInstance.InstanceID, SourceInstance.ItemID, SourceSlot, TargetSlot);
            }

            ISuspenseInventory::BroadcastItemMoved(this, SourceInstance.InstanceID, SourceSlot, TargetSlot, false);

            BroadcastInventoryUpdated();

            OutErrorCode = ESuspenseInventoryErrorCode::Success;
            UE_LOG(LogInventory, Log, TEXT("ExecuteSlotSwap: Successfully moved item to empty slot"));
            return true;
        }
        else
        {
            RollbackTransaction();
            OutErrorCode = ESuspenseInventoryErrorCode::NoSpace;
            UE_LOG(LogInventory, Warning, TEXT("ExecuteSlotSwap: Failed to move item"));
            return false;
        }
    }

    // Case 2: Swapping two items
    UE_LOG(LogInventory, Log, TEXT("ExecuteSlotSwap: Swapping two items"));

    // Create backup for safety
    TArray<FSuspenseInventoryItemInstance> BackupInstances = StorageComponent->GetAllItemInstances();

    // Save copies for replicator
    FSuspenseInventoryItemInstance PreSwapInstance1 = Instance1;
    FSuspenseInventoryItemInstance PreSwapInstance2 = Instance2;

    // Temporarily remove both items
    if (!StorageComponent->RemoveItemInstance(Instance1.InstanceID) ||
        !StorageComponent->RemoveItemInstance(Instance2.InstanceID))
    {
        RollbackTransaction();
        OutErrorCode = ESuspenseInventoryErrorCode::UnknownError;
        UE_LOG(LogInventory, Error, TEXT("ExecuteSlotSwap: Failed to temporarily remove items"));
        return false;
    }

    // Update positions
    Instance1.AnchorIndex = Slot2;
    Instance2.AnchorIndex = Slot1;

    // Place items in new positions
    bool bPlaced1 = StorageComponent->PlaceItemInstance(Instance1, Slot2);
    bool bPlaced2 = StorageComponent->PlaceItemInstance(Instance2, Slot1);

    if (bPlaced1 && bPlaced2)
    {
        CommitTransaction();

        // Update replicator
        if (ReplicatorComponent)
        {
            UE_LOG(LogInventory, Log, TEXT("ExecuteSlotSwap: Updating replicator for swap"));
            NotifyItemRemoved(PreSwapInstance1);
            NotifyItemRemoved(PreSwapInstance2);
            NotifyItemPlaced(Instance1, Slot2);
            NotifyItemPlaced(Instance2, Slot1);
        }

        // Broadcast events
        if (EventsComponent)
        {
            EventsComponent->BroadcastItemSwapped(Instance1.InstanceID, Instance2.InstanceID, Slot1, Slot2);
        }

        ISuspenseInventory::BroadcastItemMoved(this, Instance1.InstanceID, Slot1, Slot2, false);
        ISuspenseInventory::BroadcastItemMoved(this, Instance2.InstanceID, Slot2, Slot1, false);

        BroadcastInventoryUpdated();

        // Schedule additional update for UI synchronization
        if (GetWorld())
        {
            FTimerHandle UpdateTimer;
            GetWorld()->GetTimerManager().SetTimer(UpdateTimer, [this]()
            {
                UE_LOG(LogInventory, VeryVerbose, TEXT("ExecuteSlotSwap: Delayed UI update after swap"));
                BroadcastInventoryUpdated();

                if (ReplicatorComponent)
                {
                    ReplicatorComponent->RequestNetUpdate();
                }
            }, 0.1f, false);
        }

        OutErrorCode = ESuspenseInventoryErrorCode::Success;
        UE_LOG(LogInventory, Log, TEXT("ExecuteSlotSwap: Successfully swapped items"));
        return true;
    }
    else
    {
        // Rollback on failure
        RollbackTransaction();

        // Double-check items are restored
        TArray<FSuspenseInventoryItemInstance> CurrentInstances = StorageComponent->GetAllItemInstances();
        bool bItem1Found = false;
        bool bItem2Found = false;

        for (const FSuspenseInventoryItemInstance& Instance : CurrentInstances)
        {
            if (Instance.InstanceID == Instance1.InstanceID)
            {
                bItem1Found = true;
            }
            if (Instance.InstanceID == Instance2.InstanceID)
            {
                bItem2Found = true;
            }
        }

        // Emergency restore if items are missing
        if (!bItem1Found || !bItem2Found)
        {
            UE_LOG(LogInventory, Error, TEXT("ExecuteSlotSwap: CRITICAL - Items lost during swap, attempting emergency restore"));

            // Clear and restore from backup
            StorageComponent->ClearAllItems();
            for (const FSuspenseInventoryItemInstance& BackupInstance : BackupInstances)
            {
                StorageComponent->AddItemInstance(BackupInstance, false);
            }
        }

        OutErrorCode = ESuspenseInventoryErrorCode::NoSpace;
        UE_LOG(LogInventory, Error, TEXT("ExecuteSlotSwap: Failed to place items in swapped positions"));
        return false;
    }
}

bool USuspenseInventoryComponent::TryMoveInstanceToSlot(const FGuid& InstanceID, int32 NewSlot, bool bAllowRotation, ESuspenseInventoryErrorCode& OutErrorCode)
{
    if (!InstanceID.IsValid())
    {
        OutErrorCode = ESuspenseInventoryErrorCode::InvalidItem;
        return false;
    }

    if (!CheckAuthority(TEXT("TryMoveInstanceToSlot")))
    {
        OutErrorCode = ESuspenseInventoryErrorCode::UnknownError;
        return false;
    }

    if (!StorageComponent || !bIsInitialized)
    {
        OutErrorCode = ESuspenseInventoryErrorCode::NotInitialized;
        return false;
    }

    // Получаем текущий экземпляр
    FSuspenseInventoryItemInstance CurrentInstance;
    if (!StorageComponent->GetItemInstance(InstanceID, CurrentInstance))
    {
        OutErrorCode = ESuspenseInventoryErrorCode::ItemNotFound;
        return false;
    }

    // Проверяем нужно ли что-то делать
    if (CurrentInstance.AnchorIndex == NewSlot)
    {
        OutErrorCode = ESuspenseInventoryErrorCode::Success;
        return true;
    }

    // Начинаем транзакцию
    BeginTransaction();

    // Сохраняем старые значения
    int32 OldSlot = CurrentInstance.AnchorIndex;

    // Пытаемся переместить
    if (StorageComponent->MoveItem(InstanceID, NewSlot, bAllowRotation))
    {
        // Коммитим транзакцию
        CommitTransaction();

        // Получаем обновленный экземпляр
        FSuspenseInventoryItemInstance UpdatedInstance;
        if (StorageComponent->GetItemInstance(InstanceID, UpdatedInstance))
        {
            // Уведомляем репликатор
            NotifyItemPlaced(UpdatedInstance, NewSlot);

            // Broadcast событие
            if (EventsComponent)
            {
                EventsComponent->BroadcastItemMoved(InstanceID, UpdatedInstance.ItemID, OldSlot, NewSlot);
            }

            ISuspenseInventory::BroadcastItemMoved(this, UpdatedInstance.InstanceID, OldSlot, NewSlot, UpdatedInstance.bIsRotated);
        }

        BroadcastInventoryUpdated();

        OutErrorCode = ESuspenseInventoryErrorCode::Success;
        return true;
    }
    else
    {
        // Откатываем транзакцию
        RollbackTransaction();
        OutErrorCode = ESuspenseInventoryErrorCode::NoSpace;
        return false;
    }
}

USuspenseInventoryManager* USuspenseInventoryComponent::GetInventoryManager() const
{
    if (CachedInventoryManager.IsValid())
    {
        return CachedInventoryManager.Get();
    }

    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return nullptr;
    }

    UGameInstance* GameInstance = Owner->GetGameInstance();
    if (!GameInstance)
    {
        return nullptr;
    }

    USuspenseInventoryManager* Manager = GameInstance->GetSubsystem<USuspenseInventoryManager>();
    if (Manager)
    {
        CachedInventoryManager = Manager;
    }

    return Manager;
}

UGameInstance* USuspenseInventoryComponent::GetGameInstance() const
{
    AActor* Owner = GetOwner();
    return Owner ? Owner->GetGameInstance() : nullptr;
}

void USuspenseInventoryComponent::OnRep_GridSize()
{
    INVENTORY_LOG(Log, TEXT("OnRep_GridSize: Grid size replicated as %.0fx%.0f"),
        ReplicatedGridSize.X, ReplicatedGridSize.Y);

    // If we're a client and not initialized yet, try to initialize
    if (GetOwnerRole() != ROLE_Authority && !bIsInitialized && ReplicatedGridSize.X > 0)
    {
        // Schedule initialization check
        if (GetWorld())
        {
            GetWorld()->GetTimerManager().SetTimer(
                ClientInitCheckTimer,
                [this]()
                {
                    if (bIsInitializedReplicated && !bIsInitialized)
                    {
                        InitializeClientComponents();
                    }
                },
                0.1f,
                false
            );
        }
    }
}

void USuspenseInventoryComponent::OnRep_InventoryState()
{
    INVENTORY_LOG(Log, TEXT("OnRep_InventoryState: Initialization state replicated as %s"),
        bIsInitializedReplicated ? TEXT("true") : TEXT("false"));

    // If server says we're initialized but we're not, initialize client components
    if (GetOwnerRole() != ROLE_Authority && bIsInitializedReplicated && !bIsInitialized)
    {
        if (ReplicatedGridSize.X > 0 && ReplicatedGridSize.Y > 0)
        {
            InitializeClientComponents();
        }
        else
        {
            INVENTORY_LOG(Warning, TEXT("OnRep_InventoryState: Waiting for grid size replication"));
        }
    }
}
