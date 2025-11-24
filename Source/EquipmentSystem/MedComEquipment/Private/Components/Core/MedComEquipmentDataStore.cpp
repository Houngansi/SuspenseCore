// MedComEquipmentDataStore.cpp
// Copyright MedCom Team. All Rights Reserved.

#include "Components/Core/MedComEquipmentDataStore.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Delegates/EventDelegateManager.h"
#include "GameFramework/PlayerState.h"
#include "Types/Loadout/MedComLoadoutManager.h"

// Define logging category
DEFINE_LOG_CATEGORY(LogEquipmentDataStore);

// Constructor
UMedComEquipmentDataStore::UMedComEquipmentDataStore()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(false); // Data store itself doesn't replicate
}

// Destructor
UMedComEquipmentDataStore::~UMedComEquipmentDataStore()
{
    // Ensure critical section is properly cleaned up
}

//========================================
// UActorComponent Interface
//========================================

void UMedComEquipmentDataStore::BeginPlay()
{
    Super::BeginPlay();
    
    UE_LOG(LogEquipmentDataStore, Log, 
        TEXT("DataStore initialized with %d slots on %s"), 
        DataStorage.SlotConfigurations.Num(),
        GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
}

void UMedComEquipmentDataStore::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Clear all delegates
    OnSlotDataChangedDelegate.Clear();
    OnSlotConfigurationChangedDelegate.Clear();
    OnDataStoreResetDelegate.Clear();
    OnEquipmentDeltaDelegate.Clear();
    
    UE_LOG(LogEquipmentDataStore, Log, 
        TEXT("DataStore shutdown (reason: %s)"), 
        *UEnum::GetValueAsString(EndPlayReason));
    
    Super::EndPlay(EndPlayReason);
}

//========================================
// Pure Data Access Implementation (Read-only, thread-safe)
//========================================

FInventoryItemInstance UMedComEquipmentDataStore::GetSlotItem(int32 SlotIndex) const
{
    FScopeLock Lock(&DataCriticalSection);
    
    if (!ValidateSlotIndexInternal(SlotIndex, TEXT("GetSlotItem")))
    {
        return FInventoryItemInstance();
    }
    
    return DataStorage.SlotItems[SlotIndex];
}

FEquipmentSlotConfig UMedComEquipmentDataStore::GetSlotConfiguration(int32 SlotIndex) const
{
    // CRITICAL FIX: Always try to get fresh configuration from LoadoutManager
    FEquipmentSlotConfig FreshConfig = GetFreshSlotConfiguration(SlotIndex);
    if (FreshConfig.IsValid())
    {
        return FreshConfig;
    }
    
    // Fallback to cached version only if LoadoutManager unavailable
    FScopeLock Lock(&DataCriticalSection);
    
    if (!ValidateSlotIndexInternal(SlotIndex, TEXT("GetSlotConfiguration")))
    {
        return FEquipmentSlotConfig();
    }
    
    UE_LOG(LogEquipmentDataStore, Warning, 
        TEXT("GetSlotConfiguration: Using cached config for slot %d (LoadoutManager unavailable)"), 
        SlotIndex);
    
    return DataStorage.SlotConfigurations[SlotIndex];
}

FEquipmentSlotConfig UMedComEquipmentDataStore::GetFreshSlotConfiguration(int32 SlotIndex) const
{
    // Validate slot index first
    if (SlotIndex < 0)
    {
        return FEquipmentSlotConfig();
    }
    
    // Get LoadoutManager from GameInstance
    UWorld* World = GetWorld();
    if (!World)
    {
        return FEquipmentSlotConfig();
    }
    
    UGameInstance* GameInstance = World->GetGameInstance();
    if (!GameInstance)
    {
        return FEquipmentSlotConfig();
    }
    
    UMedComLoadoutManager* LoadoutManager = GameInstance->GetSubsystem<UMedComLoadoutManager>();
    if (!LoadoutManager)
    {
        return FEquipmentSlotConfig();
    }
    
    // Determine which loadout to use
    FName LoadoutToUse = CurrentLoadoutID;
    
    if (LoadoutToUse.IsNone())
    {
        // Try to get loadout ID from owner's PlayerState
        if (AActor* Owner = GetOwner())
        {
            // If owner is a Pawn, get its PlayerState
            if (APawn* OwnerPawn = Cast<APawn>(Owner))
            {
                if (APlayerState* PS = OwnerPawn->GetPlayerState())
                {
                    // Check if PlayerState implements loadout interface
                    if (PS->GetClass()->ImplementsInterface(UMedComLoadoutInterface::StaticClass()))
                    {
                        LoadoutToUse = IMedComLoadoutInterface::Execute_GetCurrentLoadoutID(PS);
                    }
                }
            }
            // Or if owner is PlayerState directly
            else if (APlayerState* PS = Cast<APlayerState>(Owner))
            {
                if (PS->GetClass()->ImplementsInterface(UMedComLoadoutInterface::StaticClass()))
                {
                    LoadoutToUse = IMedComLoadoutInterface::Execute_GetCurrentLoadoutID(PS);
                }
            }
        }
    }
    
    // Final fallback to default loadout
    if (LoadoutToUse.IsNone())
    {
        LoadoutToUse = FName(TEXT("Default_Soldier"));
    }
    
    // Get fresh slot configurations from LoadoutManager
    TArray<FEquipmentSlotConfig> FreshSlots = LoadoutManager->GetEquipmentSlots(LoadoutToUse);
    
    if (FreshSlots.IsValidIndex(SlotIndex))
    {
        UE_LOG(LogEquipmentDataStore, Verbose, 
            TEXT("GetFreshSlotConfiguration: Retrieved fresh config for slot %d from LoadoutManager (Loadout: %s)"), 
            SlotIndex, *LoadoutToUse.ToString());
        
        return FreshSlots[SlotIndex];
    }
    
    UE_LOG(LogEquipmentDataStore, Warning, 
        TEXT("GetFreshSlotConfiguration: Failed to get config for slot %d from LoadoutManager"), 
        SlotIndex);
    
    // Return invalid config if can't get fresh data
    return FEquipmentSlotConfig();
}

void UMedComEquipmentDataStore::RefreshSlotConfigurations()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    
    UGameInstance* GameInstance = World->GetGameInstance();
    if (!GameInstance)
    {
        return;
    }
    
    UMedComLoadoutManager* LoadoutManager = GameInstance->GetSubsystem<UMedComLoadoutManager>();
    if (!LoadoutManager)
    {
        return;
    }
    
    // Determine loadout ID
    FName LoadoutToUse = CurrentLoadoutID;
    if (LoadoutToUse.IsNone())
    {
        LoadoutToUse = FName(TEXT("Default_Soldier"));
    }
    
    TArray<FEquipmentSlotConfig> FreshSlots = LoadoutManager->GetEquipmentSlots(LoadoutToUse);
    
    if (FreshSlots.Num() > 0)
    {
        // Update cached configurations
        ModifyDataWithEvents(
            [this, FreshSlots](FEquipmentDataStorage& Data, TArray<FPendingEventData>& PendingEvents) -> bool
            {
                int32 OldCount = Data.SlotConfigurations.Num();
                Data.SlotConfigurations = FreshSlots;
                Data.DataVersion++;
                
                // Resize item storage if needed
                if (Data.SlotItems.Num() != FreshSlots.Num())
                {
                    Data.SlotItems.SetNum(FreshSlots.Num());
                }
                
                UE_LOG(LogEquipmentDataStore, Log, 
                    TEXT("RefreshSlotConfigurations: Updated %d slots (was %d)"), 
                    FreshSlots.Num(), OldCount);
                
                // Queue configuration changed event
                FPendingEventData Event;
                Event.Type = FPendingEventData::ConfigChanged;
                PendingEvents.Add(Event);
                
                return true;
            },
            true // Notify observers
        );
    }
}

void UMedComEquipmentDataStore::SetCurrentLoadoutID(const FName& LoadoutID)
{
    if (CurrentLoadoutID != LoadoutID)
    {
        CurrentLoadoutID = LoadoutID;
        
        UE_LOG(LogEquipmentDataStore, Log, 
            TEXT("SetCurrentLoadoutID: Changed to %s"), 
            *LoadoutID.ToString());
        
        // Refresh configurations with new loadout
        RefreshSlotConfigurations();
    }
}

TArray<FEquipmentSlotConfig> UMedComEquipmentDataStore::GetAllSlotConfigurations() const
{
    // Try to get all fresh configurations
    UWorld* World = GetWorld();
    if (World)
    {
        if (UGameInstance* GameInstance = World->GetGameInstance())
        {
            if (UMedComLoadoutManager* LoadoutManager = GameInstance->GetSubsystem<UMedComLoadoutManager>())
            {
                FName LoadoutToUse = CurrentLoadoutID.IsNone() ? 
                    FName(TEXT("Default_Soldier")) : CurrentLoadoutID;
                    
                TArray<FEquipmentSlotConfig> FreshSlots = LoadoutManager->GetEquipmentSlots(LoadoutToUse);
                
                if (FreshSlots.Num() > 0)
                {
                    return FreshSlots;
                }
            }
        }
    }
    
    // Fallback to cached version
    FScopeLock Lock(&DataCriticalSection);
    return DataStorage.SlotConfigurations;
}

TMap<int32, FInventoryItemInstance> UMedComEquipmentDataStore::GetAllEquippedItems() const
{
    FScopeLock Lock(&DataCriticalSection);
    
    TMap<int32, FInventoryItemInstance> EquippedItems;
    
    for (int32 i = 0; i < DataStorage.SlotItems.Num(); i++)
    {
        if (DataStorage.SlotItems[i].IsValid())
        {
            EquippedItems.Add(i, DataStorage.SlotItems[i]);
        }
    }
    
    return EquippedItems;
}

int32 UMedComEquipmentDataStore::GetSlotCount() const
{
    FScopeLock Lock(&DataCriticalSection);
    return DataStorage.SlotConfigurations.Num();
}

bool UMedComEquipmentDataStore::IsValidSlotIndex(int32 SlotIndex) const
{
    FScopeLock Lock(&DataCriticalSection);
    return SlotIndex >= 0 && SlotIndex < DataStorage.SlotConfigurations.Num();
}

bool UMedComEquipmentDataStore::IsSlotOccupied(int32 SlotIndex) const
{
    FScopeLock Lock(&DataCriticalSection);
    
    if (!ValidateSlotIndexInternal(SlotIndex, TEXT("IsSlotOccupied")))
    {
        return false;
    }
    
    return DataStorage.SlotItems[SlotIndex].IsValid();
}

//========================================
// Data Modification Implementation (Thread-safe with deferred events)
//========================================

bool UMedComEquipmentDataStore::SetSlotItem(int32 SlotIndex, const FInventoryItemInstance& ItemInstance, bool bNotifyObservers)
{
    return ModifyDataWithEvents(
        [this, SlotIndex, ItemInstance](FEquipmentDataStorage& Data, TArray<FPendingEventData>& PendingEvents) -> bool
        {
            if (SlotIndex < 0 || SlotIndex >= Data.SlotItems.Num())
            {
                UE_LOG(LogEquipmentDataStore, Warning, 
                    TEXT("SetSlotItem: Invalid slot index %d"), SlotIndex);
                return false;
            }
            
            // Store previous item for comparison
            FInventoryItemInstance PreviousItem = Data.SlotItems[SlotIndex];
            
            // Check if item actually changed
            if (PreviousItem == ItemInstance)
            {
                // No change needed
                return true;
            }
            
            // Set new item - NO VALIDATION, just store
            Data.SlotItems[SlotIndex] = ItemInstance;
            
            // Log the change
            LogDataModification(TEXT("SetSlotItem"), 
                FString::Printf(TEXT("Slot %d: %s -> %s"), 
                    SlotIndex,
                    PreviousItem.IsValid() ? *PreviousItem.ItemID.ToString() : TEXT("Empty"),
                    ItemInstance.IsValid() ? *ItemInstance.ItemID.ToString() : TEXT("Empty")));
            
            // Create delta event
            FEquipmentDelta Delta = CreateDelta(
                FGameplayTag::RequestGameplayTag(TEXT("Equipment.Delta.ItemSet")),
                SlotIndex,
                PreviousItem,
                ItemInstance,
                FGameplayTag::RequestGameplayTag(TEXT("Equipment.Reason.DirectSet"))
            );
            Delta.SourceTransactionId = Data.ActiveTransactionId;
            
            // Queue delta event
            FPendingEventData DeltaEvent;
            DeltaEvent.Type = FPendingEventData::EquipmentDelta;
            DeltaEvent.DeltaData = Delta;
            PendingEvents.Add(DeltaEvent);
            
            // Queue traditional event for backward compatibility
            FPendingEventData Event;
            Event.Type = FPendingEventData::SlotChanged;
            Event.SlotIndex = SlotIndex;
            Event.ItemData = ItemInstance;
            PendingEvents.Add(Event);
            
            return true;
        }, 
        bNotifyObservers
    );
}

FInventoryItemInstance UMedComEquipmentDataStore::ClearSlot(int32 SlotIndex, bool bNotifyObservers)
{
    FInventoryItemInstance RemovedItem;
    
    ModifyDataWithEvents(
        [this, SlotIndex, &RemovedItem](FEquipmentDataStorage& Data, TArray<FPendingEventData>& PendingEvents) -> bool
        {
            if (SlotIndex < 0 || SlotIndex >= Data.SlotItems.Num())
            {
                UE_LOG(LogEquipmentDataStore, Warning, 
                    TEXT("ClearSlot: Invalid slot index %d"), SlotIndex);
                return false;
            }
            
            // Store removed item
            RemovedItem = Data.SlotItems[SlotIndex];
            
            if (!RemovedItem.IsValid())
            {
                // Slot already empty
                return true;
            }
            
            // Clear slot - NO VALIDATION, just clear
            Data.SlotItems[SlotIndex] = FInventoryItemInstance();
            
            // Log the change
            LogDataModification(TEXT("ClearSlot"), 
                FString::Printf(TEXT("Slot %d cleared: %s"), 
                    SlotIndex,
                    RemovedItem.IsValid() ? *RemovedItem.ItemID.ToString() : TEXT("Already Empty")));
            
            // Create delta event
            FEquipmentDelta Delta = CreateDelta(
                FGameplayTag::RequestGameplayTag(TEXT("Equipment.Delta.ItemClear")),
                SlotIndex,
                RemovedItem,
                FInventoryItemInstance(),
                FGameplayTag::RequestGameplayTag(TEXT("Equipment.Reason.DirectClear"))
            );
            Delta.SourceTransactionId = Data.ActiveTransactionId;
            
            // Queue delta event
            FPendingEventData DeltaEvent;
            DeltaEvent.Type = FPendingEventData::EquipmentDelta;
            DeltaEvent.DeltaData = Delta;
            PendingEvents.Add(DeltaEvent);
            
            // Queue traditional event
            FPendingEventData Event;
            Event.Type = FPendingEventData::SlotChanged;
            Event.SlotIndex = SlotIndex;
            Event.ItemData = FInventoryItemInstance(); // Empty item
            PendingEvents.Add(Event);
            
            return true;
        }, 
        bNotifyObservers
    );
    
    return RemovedItem;
}

bool UMedComEquipmentDataStore::InitializeSlots(const TArray<FEquipmentSlotConfig>& Configurations)
{
    return ModifyDataWithEvents(
        [this, Configurations](FEquipmentDataStorage& Data, TArray<FPendingEventData>& PendingEvents) -> bool
        {
            // Store previous state for logging
            int32 PreviousSlotCount = Data.SlotConfigurations.Num();
            
            // Clear existing data
            Data.SlotConfigurations = Configurations;
            Data.SlotItems.Empty(Configurations.Num());
            Data.SlotItems.SetNum(Configurations.Num());
            
            // Reset active weapon slot
            Data.ActiveWeaponSlot = INDEX_NONE;
            
            // Log the change
            LogDataModification(TEXT("InitializeSlots"), 
                FString::Printf(TEXT("Initialized %d slots (previous: %d)"), 
                    Configurations.Num(), PreviousSlotCount));
            
            // Create delta for reset
            FEquipmentDelta Delta = CreateDelta(
                FGameplayTag::RequestGameplayTag(TEXT("Equipment.Delta.Initialize")),
                INDEX_NONE,
                FInventoryItemInstance(),
                FInventoryItemInstance(),
                FGameplayTag::RequestGameplayTag(TEXT("Equipment.Reason.Initialize"))
            );
            Delta.Metadata.Add(TEXT("SlotCount"), FString::FromInt(Configurations.Num()));
            Delta.Metadata.Add(TEXT("PreviousCount"), FString::FromInt(PreviousSlotCount));
            
            // Queue delta event
            FPendingEventData DeltaEvent;
            DeltaEvent.Type = FPendingEventData::EquipmentDelta;
            DeltaEvent.DeltaData = Delta;
            PendingEvents.Add(DeltaEvent);
            
            // Queue configuration change events for each slot
            for (int32 i = 0; i < Configurations.Num(); i++)
            {
                FPendingEventData Event;
                Event.Type = FPendingEventData::ConfigChanged;
                Event.SlotIndex = i;
                PendingEvents.Add(Event);
            }
            
            // Also queue a reset event since this is a major structural change
            FPendingEventData ResetEvent;
            ResetEvent.Type = FPendingEventData::StoreReset;
            PendingEvents.Add(ResetEvent);
            
            return true;
        }, 
        true // Always notify on initialization
    );
}

//========================================
// State Management Implementation
//========================================

int32 UMedComEquipmentDataStore::GetActiveWeaponSlot() const
{
    FScopeLock Lock(&DataCriticalSection);
    return DataStorage.ActiveWeaponSlot;
}

bool UMedComEquipmentDataStore::SetActiveWeaponSlot(int32 SlotIndex)
{
    return ModifyDataWithEvents(
        [this, SlotIndex](FEquipmentDataStorage& Data, TArray<FPendingEventData>& PendingEvents) -> bool
        {
            // Allow INDEX_NONE to clear active weapon
            if (SlotIndex != INDEX_NONE && (SlotIndex < 0 || SlotIndex >= Data.SlotItems.Num()))
            {
                UE_LOG(LogEquipmentDataStore, Warning, 
                    TEXT("SetActiveWeaponSlot: Invalid slot index %d"), SlotIndex);
                return false;
            }
            
            int32 PreviousSlot = Data.ActiveWeaponSlot;
            
            if (PreviousSlot == SlotIndex)
            {
                // No change needed
                return true;
            }
            
            Data.ActiveWeaponSlot = SlotIndex;
            
            LogDataModification(TEXT("SetActiveWeaponSlot"), 
                FString::Printf(TEXT("Active weapon slot: %d -> %d"), 
                    PreviousSlot, SlotIndex));
            
            // Create delta for active weapon change
            FEquipmentDelta Delta = CreateDelta(
                FGameplayTag::RequestGameplayTag(TEXT("Equipment.Delta.ActiveWeapon")),
                SlotIndex,
                FInventoryItemInstance(),
                FInventoryItemInstance(),
                FGameplayTag::RequestGameplayTag(TEXT("Equipment.Reason.ActiveChange"))
            );
            Delta.Metadata.Add(TEXT("PreviousSlot"), FString::FromInt(PreviousSlot));
            Delta.Metadata.Add(TEXT("NewSlot"), FString::FromInt(SlotIndex));
            
            // Queue delta event
            FPendingEventData DeltaEvent;
            DeltaEvent.Type = FPendingEventData::EquipmentDelta;
            DeltaEvent.DeltaData = Delta;
            PendingEvents.Add(DeltaEvent);
            
            // Queue state change event
            FPendingEventData Event;
            Event.Type = FPendingEventData::StateChanged;
            Event.SlotIndex = SlotIndex;
            PendingEvents.Add(Event);
            
            return true;
        }, 
        true
    );
}

FGameplayTag UMedComEquipmentDataStore::GetCurrentEquipmentState() const
{
    FScopeLock Lock(&DataCriticalSection);
    return DataStorage.CurrentState;
}

bool UMedComEquipmentDataStore::SetEquipmentState(const FGameplayTag& NewState)
{
    return ModifyDataWithEvents(
        [this, NewState](FEquipmentDataStorage& Data, TArray<FPendingEventData>& PendingEvents) -> bool
        {
            FGameplayTag PreviousState = Data.CurrentState;
            
            if (PreviousState == NewState)
            {
                // No change needed
                return true;
            }
            
            Data.CurrentState = NewState;
            
            LogDataModification(TEXT("SetEquipmentState"), 
                FString::Printf(TEXT("State: %s -> %s"), 
                    *PreviousState.ToString(), *NewState.ToString()));
            
            // Create delta for state change
            FEquipmentDelta Delta = CreateDelta(
                FGameplayTag::RequestGameplayTag(TEXT("Equipment.Delta.StateChange")),
                INDEX_NONE,
                FInventoryItemInstance(),
                FInventoryItemInstance(),
                FGameplayTag::RequestGameplayTag(TEXT("Equipment.Reason.StateTransition"))
            );
            Delta.Metadata.Add(TEXT("PreviousState"), PreviousState.ToString());
            Delta.Metadata.Add(TEXT("NewState"), NewState.ToString());
            
            // Queue delta event
            FPendingEventData DeltaEvent;
            DeltaEvent.Type = FPendingEventData::EquipmentDelta;
            DeltaEvent.DeltaData = Delta;
            PendingEvents.Add(DeltaEvent);
            
            // Queue state change event
            FPendingEventData Event;
            Event.Type = FPendingEventData::StateChanged;
            Event.StateTag = NewState;
            PendingEvents.Add(Event);
            
            return true;
        }, 
        true
    );
}

//========================================
// Snapshot Management Implementation
//========================================

FEquipmentStateSnapshot UMedComEquipmentDataStore::CreateSnapshot() const
{
    FScopeLock Lock(&DataCriticalSection);
    
    FEquipmentStateSnapshot Snapshot;
    Snapshot.SnapshotId = FGuid::NewGuid();
    Snapshot.Timestamp = FDateTime::Now();
    Snapshot.ActiveWeaponSlotIndex = DataStorage.ActiveWeaponSlot;
    
    // ИСПРАВЛЕНО: Сохраняем FGameplayTag в правильное поле CurrentStateTag
    Snapshot.CurrentStateTag = DataStorage.CurrentState;
    
    // Опционально: конвертируем тег в enum для обратной совместимости
    // Это требует маппинга между тегами и enum значениями
    Snapshot.CurrentState = ConvertTagToEquipmentState(DataStorage.CurrentState);
    
    // Create slot snapshots
    for (int32 i = 0; i < DataStorage.SlotConfigurations.Num(); i++)
    {
        FEquipmentSlotSnapshot SlotSnapshot;
        SlotSnapshot.SlotIndex = i;
        SlotSnapshot.ItemInstance = DataStorage.SlotItems[i];
        SlotSnapshot.Configuration = DataStorage.SlotConfigurations[i];
        SlotSnapshot.Timestamp = Snapshot.Timestamp;
        SlotSnapshot.SnapshotId = Snapshot.SnapshotId;
        
        Snapshot.SlotSnapshots.Add(SlotSnapshot);
    }
    
    UE_LOG(LogEquipmentDataStore, Verbose, 
        TEXT("Created snapshot %s with %d slots, State: %s"), 
        *Snapshot.SnapshotId.ToString(), 
        Snapshot.SlotSnapshots.Num(),
        *DataStorage.CurrentState.ToString());
    
    return Snapshot;
}

bool UMedComEquipmentDataStore::RestoreSnapshot(const FEquipmentStateSnapshot& Snapshot)
{
    if (!Snapshot.IsValid())
    {
        UE_LOG(LogEquipmentDataStore, Warning, 
            TEXT("RestoreSnapshot: Invalid snapshot"));
        return false;
    }
    
    return ModifyDataWithEvents(
        [this, Snapshot](FEquipmentDataStorage& Data, TArray<FPendingEventData>& PendingEvents) -> bool
        {
            // Validate snapshot compatibility
            if (Snapshot.SlotSnapshots.Num() != Data.SlotConfigurations.Num())
            {
                UE_LOG(LogEquipmentDataStore, Warning, 
                    TEXT("RestoreSnapshot: Slot count mismatch (%d vs %d)"), 
                    Snapshot.SlotSnapshots.Num(), Data.SlotConfigurations.Num());
                return false;
            }
            
            // Collect changed slots for events
            TArray<int32> ChangedSlots;
            
            // Restore slot data
            for (const FEquipmentSlotSnapshot& SlotSnapshot : Snapshot.SlotSnapshots)
            {
                if (SlotSnapshot.SlotIndex >= 0 && SlotSnapshot.SlotIndex < Data.SlotItems.Num())
                {
                    // Check if slot actually changed
                    FInventoryItemInstance OldItem = Data.SlotItems[SlotSnapshot.SlotIndex];
                    if (OldItem != SlotSnapshot.ItemInstance)
                    {
                        Data.SlotItems[SlotSnapshot.SlotIndex] = SlotSnapshot.ItemInstance;
                        ChangedSlots.Add(SlotSnapshot.SlotIndex);
                        
                        // Create delta for each changed slot
                        FEquipmentDelta Delta = CreateDelta(
                            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Delta.SnapshotRestore")),
                            SlotSnapshot.SlotIndex,
                            OldItem,
                            SlotSnapshot.ItemInstance,
                            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Reason.SnapshotRestore"))
                        );
                        Delta.Metadata.Add(TEXT("SnapshotId"), Snapshot.SnapshotId.ToString());
                        
                        FPendingEventData DeltaEvent;
                        DeltaEvent.Type = FPendingEventData::EquipmentDelta;
                        DeltaEvent.DeltaData = Delta;
                        PendingEvents.Add(DeltaEvent);
                    }
                }
            }
            
            // ИСПРАВЛЕНО: Восстанавливаем состояние из правильного поля
            // Сохраняем старое состояние для события
            FGameplayTag OldState = Data.CurrentState;
            
            // Восстанавливаем активный слот оружия
            Data.ActiveWeaponSlot = Snapshot.ActiveWeaponSlotIndex;
            
            // Восстанавливаем состояние из CurrentStateTag (FGameplayTag)
            if (Snapshot.CurrentStateTag.IsValid())
            {
                Data.CurrentState = Snapshot.CurrentStateTag;
            }
            else
            {
                // Fallback: если CurrentStateTag не заполнен, пытаемся конвертировать из enum
                Data.CurrentState = ConvertEquipmentStateToTag(Snapshot.CurrentState);
            }
            
            // Если состояние изменилось, добавляем событие
            if (OldState != Data.CurrentState)
            {
                FPendingEventData StateEvent;
                StateEvent.Type = FPendingEventData::StateChanged;
                StateEvent.StateTag = Data.CurrentState;
                PendingEvents.Add(StateEvent);
            }
            
            LogDataModification(TEXT("RestoreSnapshot"), 
                FString::Printf(TEXT("Restored snapshot %s, %d slots changed, State: %s"), 
                    *Snapshot.SnapshotId.ToString(), 
                    ChangedSlots.Num(),
                    *Data.CurrentState.ToString()));
            
            // Queue events for changed slots
            for (int32 SlotIndex : ChangedSlots)
            {
                FPendingEventData Event;
                Event.Type = FPendingEventData::SlotChanged;
                Event.SlotIndex = SlotIndex;
                Event.ItemData = Data.SlotItems[SlotIndex];
                PendingEvents.Add(Event);
            }
            
            // Queue reset event since this is a major restore operation
            FPendingEventData ResetEvent;
            ResetEvent.Type = FPendingEventData::StoreReset;
            PendingEvents.Add(ResetEvent);
            
            return true;
        }, 
        true
    );
}

EEquipmentState UMedComEquipmentDataStore::ConvertTagToEquipmentState(const FGameplayTag& StateTag) const
{
    // Маппинг тегов на enum значения
    if (StateTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Idle"))))
        return EEquipmentState::Idle;
    else if (StateTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Equipping"))))
        return EEquipmentState::Equipping;
    else if (StateTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Unequipping"))))
        return EEquipmentState::Unequipping;
    else if (StateTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Switching"))))
        return EEquipmentState::Switching;
    else if (StateTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Reloading"))))
        return EEquipmentState::Reloading;
    else if (StateTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Inspecting"))))
        return EEquipmentState::Inspecting;
    else if (StateTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Repairing"))))
        return EEquipmentState::Repairing;
    else if (StateTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Upgrading"))))
        return EEquipmentState::Upgrading;
    else if (StateTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Locked"))))
        return EEquipmentState::Locked;
    else if (StateTag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Error"))))
        return EEquipmentState::Error;
    else
        return EEquipmentState::Idle; // Default
}

FGameplayTag UMedComEquipmentDataStore::ConvertEquipmentStateToTag(EEquipmentState State) const
{
    // Маппинг enum значений на теги
    switch (State)
    {
        case EEquipmentState::Idle:
            return FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Idle"));
        case EEquipmentState::Equipping:
            return FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Equipping"));
        case EEquipmentState::Unequipping:
            return FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Unequipping"));
        case EEquipmentState::Switching:
            return FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Switching"));
        case EEquipmentState::Reloading:
            return FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Reloading"));
        case EEquipmentState::Inspecting:
            return FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Inspecting"));
        case EEquipmentState::Repairing:
            return FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Repairing"));
        case EEquipmentState::Upgrading:
            return FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Upgrading"));
        case EEquipmentState::Locked:
            return FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Locked"));
        case EEquipmentState::Error:
            return FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Error"));
        default:
            return FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Idle"));
    }
}

FEquipmentSlotSnapshot UMedComEquipmentDataStore::CreateSlotSnapshot(int32 SlotIndex) const
{
    FScopeLock Lock(&DataCriticalSection);
    
    FEquipmentSlotSnapshot Snapshot;
    
    if (!ValidateSlotIndexInternal(SlotIndex, TEXT("CreateSlotSnapshot")))
    {
        return Snapshot;
    }
    
    Snapshot.SlotIndex = SlotIndex;
    Snapshot.ItemInstance = DataStorage.SlotItems[SlotIndex];
    Snapshot.Configuration = DataStorage.SlotConfigurations[SlotIndex];
    Snapshot.Timestamp = FDateTime::Now();
    Snapshot.SnapshotId = FGuid::NewGuid();
    
    return Snapshot;
}

//========================================
// Transaction Support
//========================================

void UMedComEquipmentDataStore::SetActiveTransaction(const FGuid& TransactionId)
{
    FScopeLock Lock(&DataCriticalSection);
    DataStorage.ActiveTransactionId = TransactionId;
}

void UMedComEquipmentDataStore::ClearActiveTransaction()
{
    FScopeLock Lock(&DataCriticalSection);
    DataStorage.ActiveTransactionId.Invalidate();
}

FGuid UMedComEquipmentDataStore::GetActiveTransaction() const
{
    FScopeLock Lock(&DataCriticalSection);
    return DataStorage.ActiveTransactionId;
}

void UMedComEquipmentDataStore::ClearActiveTransactionIfMatches(const FGuid& TxnId)
{
    FScopeLock Lock(&DataCriticalSection);
    if (DataStorage.ActiveTransactionId == TxnId)
    {
        DataStorage.ActiveTransactionId.Invalidate();
    }
}
//========================================
// Transaction Delta Handler
//========================================

//========================================
// Transaction Delta Handler
//========================================

void UMedComEquipmentDataStore::OnTransactionDelta(const TArray<FEquipmentDelta>& Deltas)
{
    if (Deltas.Num() == 0)
    {
        UE_LOG(LogEquipmentDataStore, Verbose, TEXT("OnTransactionDelta: Empty delta array"));
        return;
    }
    
    // Thread-safe check
    FScopeLock Lock(&DataCriticalSection);
    
    UE_LOG(LogEquipmentDataStore, Verbose, 
        TEXT("OnTransactionDelta: Processing %d deltas"),
        Deltas.Num());
    
    // Increment version ONCE for entire batch
    DataStorage.DataVersion++;
    DataStorage.LastModified = FDateTime::Now();
    
    // Update statistics
    TotalDeltasGenerated += Deltas.Num();
    
    // Collect events for broadcasting (outside lock)
    TArray<FPendingEventData> PendingEvents;
    
    // Process each delta
    for (const FEquipmentDelta& Delta : Deltas)
    {
        UE_LOG(LogEquipmentDataStore, Verbose, 
            TEXT("  Delta: Type=%s, SlotIndex=%d, SourceTxnId=%s"),
            *Delta.ChangeType.ToString(),
            Delta.SlotIndex,
            *Delta.SourceTransactionId.ToString());
        
        // Queue delta event
        FPendingEventData DeltaEvent;
        DeltaEvent.Type = FPendingEventData::EquipmentDelta;
        DeltaEvent.DeltaData = Delta;
        PendingEvents.Add(DeltaEvent);
        
        // Also create a slot changed event if specific slot affected
        if (Delta.SlotIndex != INDEX_NONE)
        {
            FPendingEventData SlotEvent;
            SlotEvent.Type = FPendingEventData::SlotChanged;
            SlotEvent.SlotIndex = Delta.SlotIndex;
            SlotEvent.ItemData = Delta.ItemAfter;
            PendingEvents.Add(SlotEvent);
        }
    }
    
    // Release lock before broadcasting
    Lock.Unlock();
    
    // Broadcast all collected events
    BroadcastPendingEvents(PendingEvents);
}

//========================================
// Additional Public Methods
//========================================

uint32 UMedComEquipmentDataStore::GetDataVersion() const
{
    FScopeLock Lock(&DataCriticalSection);
    return DataStorage.DataVersion;
}

FDateTime UMedComEquipmentDataStore::GetLastModificationTime() const
{
    FScopeLock Lock(&DataCriticalSection);
    return DataStorage.LastModified;
}

void UMedComEquipmentDataStore::ResetToDefault()
{
    ModifyDataWithEvents(
        [this](FEquipmentDataStorage& Data, TArray<FPendingEventData>& PendingEvents) -> bool
        {
            // Clear all items
            for (int32 i = 0; i < Data.SlotItems.Num(); i++)
            {
                if (Data.SlotItems[i].IsValid())
                {
                    // Create delta for each cleared item
                    FEquipmentDelta Delta = CreateDelta(
                        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Delta.Reset")),
                        i,
                        Data.SlotItems[i],
                        FInventoryItemInstance(),
                        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Reason.ResetToDefault"))
                    );
                    
                    FPendingEventData DeltaEvent;
                    DeltaEvent.Type = FPendingEventData::EquipmentDelta;
                    DeltaEvent.DeltaData = Delta;
                    PendingEvents.Add(DeltaEvent);
                }
                
                Data.SlotItems[i] = FInventoryItemInstance();
            }
            
            // Reset state
            Data.ActiveWeaponSlot = INDEX_NONE;
            Data.CurrentState = FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Idle"));
            Data.DataVersion = 0;
            Data.LastModified = FDateTime::Now();
            Data.ActiveTransactionId.Invalidate();
            
            LogDataModification(TEXT("ResetToDefault"), 
                TEXT("Data store reset to default state"));
            
            // Queue reset event
            FPendingEventData ResetEvent;
            ResetEvent.Type = FPendingEventData::StoreReset;
            PendingEvents.Add(ResetEvent);
            
            return true;
        }, 
        true // Always notify on reset
    );
}

int32 UMedComEquipmentDataStore::GetMemoryUsage() const
{
    FScopeLock Lock(&DataCriticalSection);
    
    int32 TotalBytes = sizeof(FEquipmentDataStorage);
    
    // Add slot configurations size
    TotalBytes += DataStorage.SlotConfigurations.Num() * sizeof(FEquipmentSlotConfig);
    
    // Add slot items size
    TotalBytes += DataStorage.SlotItems.Num() * sizeof(FInventoryItemInstance);
    
    // Add snapshot history size
    TotalBytes += SnapshotHistory.Num() * sizeof(FEquipmentStateSnapshot);
    
    return TotalBytes;
}

//========================================
// Protected Methods
//========================================

bool UMedComEquipmentDataStore::ModifyDataWithEvents(
    TFunction<bool(FEquipmentDataStorage&, TArray<FPendingEventData>&)> ModificationFunc, 
    bool bNotifyObservers)
{
    // This is the critical method that ensures events are never broadcast under lock
    
    TArray<FPendingEventData> PendingEvents;
    bool bSuccess = false;
    
    // Phase 1: Perform modification under lock and collect events
    {
        FScopeLock Lock(&DataCriticalSection);
        
        // Create backup for potential rollback
        FEquipmentDataStorage Backup = DataStorage;
        
        // Execute modification and collect events
        bSuccess = ModificationFunc(DataStorage, PendingEvents);
        
        if (bSuccess)
        {
            // Update metadata
            IncrementVersion();
            DataStorage.LastModified = FDateTime::Now();
            UpdateStatistics();
        }
        else
        {
            // Rollback on failure
            DataStorage = Backup;
            UE_LOG(LogEquipmentDataStore, Warning, 
                TEXT("ModifyDataWithEvents: Modification failed, rolled back"));
        }
    }
    // Lock is released here!
    
    // Phase 2: Broadcast events OUTSIDE of lock to prevent deadlocks
    if (bSuccess && bNotifyObservers && PendingEvents.Num() > 0)
    {
        BroadcastPendingEvents(PendingEvents);
    }
    
    return bSuccess;
}

FEquipmentDelta UMedComEquipmentDataStore::CreateDelta(
    const FGameplayTag& ChangeType,
    int32 SlotIndex,
    const FInventoryItemInstance& Before,
    const FInventoryItemInstance& After,
    const FGameplayTag& Reason)
{
    FEquipmentDelta Delta;
    Delta.ChangeType = ChangeType;
    Delta.SlotIndex = SlotIndex;
    Delta.ItemBefore = Before;
    Delta.ItemAfter = After;
    Delta.ReasonTag = Reason;
    Delta.Timestamp = FDateTime::Now();
    Delta.OperationId = FGuid::NewGuid();
    
    TotalDeltasGenerated++;
    
    return Delta;
}

void UMedComEquipmentDataStore::BroadcastPendingEvents(const TArray<FPendingEventData>& PendingEvents)
{
    // This method is called AFTER releasing DataCriticalSection
    // It's safe for subscribers to take any locks they need
    
    UE_LOG(LogEquipmentDataStore, Warning, TEXT("=== BroadcastPendingEvents: %d events ==="), 
        PendingEvents.Num());
    
    for (const FPendingEventData& Event : PendingEvents)
    {
        switch (Event.Type)
        {
            case FPendingEventData::SlotChanged:
            {
                UE_LOG(LogEquipmentDataStore, Warning, 
                    TEXT("Broadcasting SlotChanged: Slot %d, Item %s"),
                    Event.SlotIndex,
                    *Event.ItemData.ItemID.ToString());
                
                // First, broadcast to direct subscribers (UIConnector, etc)
                OnSlotDataChangedDelegate.Broadcast(Event.SlotIndex, Event.ItemData);
                
                // CRITICAL: Also notify EventDelegateManager for global UI updates
                if (UWorld* World = GetWorld())
                {
                    if (UGameInstance* GI = World->GetGameInstance())
                    {
                        if (UEventDelegateManager* EDM = GI->GetSubsystem<UEventDelegateManager>())
                        {
                            // Get slot configuration SAFELY through public method
                            // This will take the lock internally if needed
                            FEquipmentSlotConfig Config = GetSlotConfiguration(Event.SlotIndex);
                            
                            FGameplayTag SlotType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Unknown"));
                            if (Config.IsValid())
                            {
                                SlotType = Config.SlotTag;
                            }
                            
                            const bool bOccupied = Event.ItemData.IsValid();
                            
                            UE_LOG(LogEquipmentDataStore, Log, 
                                TEXT("Notifying EventDelegateManager: Slot %d (Type: %s, Occupied: %s)"),
                                Event.SlotIndex, *SlotType.ToString(), bOccupied ? TEXT("YES") : TEXT("NO"));
                            
                            // Broadcast to global event system
                            EDM->NotifyEquipmentSlotUpdated(Event.SlotIndex, SlotType, bOccupied);
                            EDM->NotifyEquipmentUpdated();
                        }
                    }
                }
                break;
            }
                
            case FPendingEventData::ConfigChanged:
                OnSlotConfigurationChangedDelegate.Broadcast(Event.SlotIndex);
                UE_LOG(LogEquipmentDataStore, Verbose, 
                    TEXT("Configuration changed event broadcasted for slot %d"), Event.SlotIndex);
                break;
                
            case FPendingEventData::StoreReset:
                OnDataStoreResetDelegate.Broadcast();
                UE_LOG(LogEquipmentDataStore, Verbose, 
                    TEXT("Data store reset event broadcasted"));
                break;
                
            case FPendingEventData::StateChanged:
                // State change notifications (could add specific delegate if needed)
                UE_LOG(LogEquipmentDataStore, Verbose, 
                    TEXT("State changed event broadcasted"));
                break;
                
            case FPendingEventData::EquipmentDelta:
                OnEquipmentDeltaDelegate.Broadcast(Event.DeltaData);
                UE_LOG(LogEquipmentDataStore, Verbose, 
                    TEXT("Equipment delta broadcasted: %s"), 
                    *Event.DeltaData.ToString());
                break;
        }
    }
    
    UE_LOG(LogEquipmentDataStore, Warning, TEXT("=== BroadcastPendingEvents END ==="));
}

bool UMedComEquipmentDataStore::ValidateSlotIndexInternal(int32 SlotIndex, const FString& FunctionName) const
{
    // Assumes caller already holds DataCriticalSection
    if (SlotIndex < 0 || SlotIndex >= DataStorage.SlotConfigurations.Num())
    {
        UE_LOG(LogEquipmentDataStore, Warning, 
            TEXT("%s: Invalid slot index %d (max: %d)"), 
            *FunctionName, SlotIndex, DataStorage.SlotConfigurations.Num() - 1);
        return false;
    }
    return true;
}

FEquipmentDataStorage UMedComEquipmentDataStore::CreateDataSnapshot() const
{
    FScopeLock Lock(&DataCriticalSection);
    return DataStorage;
}

bool UMedComEquipmentDataStore::ApplyDataSnapshot(const FEquipmentDataStorage& Snapshot, bool bNotifyObservers)
{
    return ModifyDataWithEvents(
        [Snapshot](FEquipmentDataStorage& Data, TArray<FPendingEventData>& PendingEvents) -> bool
        {
            Data = Snapshot;
            
            // Queue a reset event since we're replacing all data
            FPendingEventData ResetEvent;
            ResetEvent.Type = FPendingEventData::StoreReset;
            PendingEvents.Add(ResetEvent);
            
            return true;
        }, 
        bNotifyObservers
    );
}

void UMedComEquipmentDataStore::IncrementVersion()
{
    // Assumes caller already holds DataCriticalSection
    DataStorage.DataVersion++;
    
    // Handle overflow
    if (DataStorage.DataVersion == 0)
    {
        DataStorage.DataVersion = 1;
        UE_LOG(LogEquipmentDataStore, Warning, 
            TEXT("Data version overflow, reset to 1"));
    }
}

void UMedComEquipmentDataStore::LogDataModification(const FString& ModificationType, const FString& Details) const
{
    UE_LOG(LogEquipmentDataStore, Verbose, 
        TEXT("DataStore[%s]: %s - %s"), 
        *GetName(), *ModificationType, *Details);
}

//========================================
// Private Methods - Statistics
//========================================

void UMedComEquipmentDataStore::UpdateStatistics()
{
    // Assumes caller already holds DataCriticalSection
    TotalModifications++;
    
    if (GetWorld())
    {
        float CurrentTime = GetWorld()->GetTimeSeconds();
        float DeltaTime = CurrentTime - LastRateCalculationTime;
        
        if (DeltaTime > 1.0f)
        {
            ModificationRate = TotalModifications / DeltaTime;
            LastRateCalculationTime = CurrentTime;
            TotalModifications = 0;
        }
    }
}


// === IMedComEquipmentDataProvider high-level queries ===

TArray<int32> UMedComEquipmentDataStore::FindCompatibleSlots(const FGameplayTag& ItemSlotTag) const
{
    TArray<int32> Result;
    if (!ItemSlotTag.IsValid()) { return Result; }

    // Try map tag name to slot type enum
    EEquipmentSlotType MappedType = EEquipmentSlotType::None;
    if (const UEnum* Enum = StaticEnum<EEquipmentSlotType>())
    {
        const int64 Val = Enum->GetValueByName(ItemSlotTag.GetTagName());
        if (Val != INDEX_NONE)
        {
            MappedType = static_cast<EEquipmentSlotType>(Val);
        }
    }
    if (MappedType != EEquipmentSlotType::None)
    {
        return GetSlotsByType(MappedType);
    }

    // Fallback: return all slots; detailed validation will filter
    const int32 Num = GetSlotCount();
    Result.Reserve(Num);
    for (int32 i = 0; i < Num; ++i) { Result.Add(i); }
    return Result;
}

TArray<int32> UMedComEquipmentDataStore::GetSlotsByType(EEquipmentSlotType SlotType) const
{
    TArray<int32> Out;
    const int32 Num = GetSlotCount();

    for (int32 i = 0; i < Num; ++i)
    {
        const FEquipmentSlotConfig Config = GetSlotConfiguration(i); // <-- возвращает по значению
        // Если в конфиге поле SlotType присутствует (как в ваших LoadoutSettings) — сравниваем:
        if (Config.SlotType == SlotType)
        {
            Out.Add(i);
        }
    }
    return Out;
}

int32 UMedComEquipmentDataStore::GetFirstEmptySlotOfType(EEquipmentSlotType SlotType) const
{
    const TArray<int32> Slots = GetSlotsByType(SlotType);
    for (int32 Index : Slots)
    {
        if (!IsSlotOccupied(Index))
        {
            return Index;
        }
    }
    return INDEX_NONE;
}

float UMedComEquipmentDataStore::GetTotalEquippedWeight() const
{
    // If project exposes item weight via data provider, wire it here.
    // For now, provide conservative default.
    float Total = 0.0f;
    const int32 Num = GetSlotCount();
    for (int32 i = 0; i < Num; ++i)
    {
        if (IsSlotOccupied(i))
        {
            // TODO: replace with real per-item weight lookup if available
            // FInventoryItemInstance Item;
            // if (GetItemAtSlot(i, Item)) { Total += Item.GetWeight(); }
        }
    }
    return Total;
}

bool UMedComEquipmentDataStore::MeetsItemRequirements(const FInventoryItemInstance& /*Item*/, int32 /*TargetSlotIndex*/) const
{
    // Validation service performs heavy checks; datastore returns permissive default.
    return true;
}

FString UMedComEquipmentDataStore::GetDebugInfo() const
{
    const int32 Num = GetSlotCount();
    int32 Occupied = 0;
    for (int32 i = 0; i < Num; ++i)
    {
        if (IsSlotOccupied(i)) { ++Occupied; }
    }
    return FString::Printf(TEXT("Slots: %d, Occupied: %d"), Num, Occupied);
}

