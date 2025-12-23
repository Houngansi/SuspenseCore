// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Components/SuspenseCoreEquipmentComponentBase.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameFramework/PlayerState.h"
#include "GameplayAbilitySpec.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/ItemSystem/SuspenseCoreItemManager.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipment.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY(LogSuspenseCoreEquipment);

//================================================
// STAT Profiling Definitions
//================================================
DECLARE_CYCLE_STAT(TEXT("Equipment Initialize"), STAT_Equipment_Initialize, STATGROUP_SuspenseCoreEquipment);
DECLARE_CYCLE_STAT(TEXT("Equipment Cleanup"), STAT_Equipment_Cleanup, STATGROUP_SuspenseCoreEquipment);
DECLARE_CYCLE_STAT(TEXT("Equipment Update Item"), STAT_Equipment_UpdateItem, STATGROUP_SuspenseCoreEquipment);
DECLARE_CYCLE_STAT(TEXT("Equipment Set Item Instance"), STAT_Equipment_SetItemInstance, STATGROUP_SuspenseCoreEquipment);
DECLARE_CYCLE_STAT(TEXT("Equipment Get Item Manager"), STAT_Equipment_GetItemManager, STATGROUP_SuspenseCoreEquipment);
DECLARE_CYCLE_STAT(TEXT("Equipment Broadcast Event"), STAT_Equipment_BroadcastEvent, STATGROUP_SuspenseCoreEquipment);
DECLARE_CYCLE_STAT(TEXT("Equipment Client Prediction"), STAT_Equipment_ClientPrediction, STATGROUP_SuspenseCoreEquipment);
DECLARE_CYCLE_STAT(TEXT("Equipment Grant Ability"), STAT_Equipment_GrantAbility, STATGROUP_SuspenseCoreEquipment);

USuspenseCoreEquipmentComponentBase::USuspenseCoreEquipmentComponentBase()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);

    bIsInitialized = false;
    ComponentVersion = 1; // Current component version
    EquippedItemInstance = FSuspenseCoreInventoryItemInstance();
    CachedASC = nullptr;
    EquipmentCycleCounter = 0;
    BroadcastEventCounter = 0;
    NextPredictionKey = 1;
    LastCacheValidationTime = 0.0f;
}

void USuspenseCoreEquipmentComponentBase::BeginPlay()
{
    Super::BeginPlay();

    // Initialize core references on begin play
    InitializeCoreReferences();

    // Start periodic prediction cleanup on clients
    if (GetOwner() && !GetOwner()->HasAuthority() && GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            PredictionCleanupTimerHandle,
            this,
            &USuspenseCoreEquipmentComponentBase::CleanupExpiredPredictions,
            1.0f,
            true
        );
    }
}

void USuspenseCoreEquipmentComponentBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Clear prediction cleanup timer to prevent memory leak
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(PredictionCleanupTimerHandle);
    }

    // Clean up any active equipment
    if (bIsInitialized)
    {
        Cleanup();
    }

    // Clear cached references with thread safety
    {
        FScopeLock Lock(&CacheCriticalSection);
        CachedASC = nullptr;
        CachedItemManager.Reset();
        CachedDelegateManager.Reset();
    }

    Super::EndPlay(EndPlayReason);
}

void USuspenseCoreEquipmentComponentBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(USuspenseCoreEquipmentComponentBase, bIsInitialized);
    DOREPLIFETIME(USuspenseCoreEquipmentComponentBase, EquippedItemInstance);
    DOREPLIFETIME(USuspenseCoreEquipmentComponentBase, ComponentVersion);
    DOREPLIFETIME(USuspenseCoreEquipmentComponentBase, EquipmentCycleCounter);
}

void USuspenseCoreEquipmentComponentBase::Initialize(AActor* InOwner, UAbilitySystemComponent* InASC)
{
    SCOPE_CYCLE_COUNTER(STAT_Equipment_Initialize);

    if (!InOwner || !InASC)
    {
        EQUIPMENT_LOG(Error, TEXT("Initialize called with invalid parameters - Owner: %s, ASC: %s"),
            InOwner ? TEXT("Valid") : TEXT("NULL"),
            InASC ? TEXT("Valid") : TEXT("NULL"));
        return;
    }

    if (bIsInitialized)
    {
        EQUIPMENT_LOG(Warning, TEXT("Already initialized, cleaning up first"));
        Cleanup();
    }

    // Проверяем тип владельца для диагностики
    FString OwnerType = TEXT("Unknown");
    if (Cast<APlayerState>(InOwner))
    {
        OwnerType = TEXT("PlayerState");
    }
    else if (Cast<APawn>(InOwner))
    {
        OwnerType = TEXT("Pawn");
    }
    else if (Cast<AController>(InOwner))
    {
        OwnerType = TEXT("Controller");
    }

    EQUIPMENT_LOG(Log, TEXT("Initialize: Owner=%s (Type: %s), ASC=%s"),
        *InOwner->GetName(),
        *OwnerType,
        *InASC->GetName());

    EquipmentCycleCounter++;
    CachedASC = InASC;
    bIsInitialized = true;

    // Ensure core references are initialized
    InitializeCoreReferences();

    EQUIPMENT_LOG(Log, TEXT("Initialized (Cycle: %d, Version: %d)"), EquipmentCycleCounter, ComponentVersion);

    OnEquipmentInitialized();

    // Mark for replication
    if (InOwner->HasAuthority())
    {
        InOwner->ForceNetUpdate();
    }
}

void USuspenseCoreEquipmentComponentBase::InitializeWithItemInstance(AActor* InOwner, UAbilitySystemComponent* InASC, const FSuspenseCoreInventoryItemInstance& ItemInstance)
{
    // First do basic initialization
    Initialize(InOwner, InASC);

    if (!bIsInitialized)
    {
        EQUIPMENT_LOG(Error, TEXT("Failed to initialize base component"));
        return;
    }

    // Set the equipped item
    SetEquippedItemInstance(ItemInstance);
}

void USuspenseCoreEquipmentComponentBase::Cleanup()
{
    SCOPE_CYCLE_COUNTER(STAT_Equipment_Cleanup);

    if (!bIsInitialized)
    {
        return;
    }

    EQUIPMENT_LOG(Log, TEXT("Cleaning up (Cycle: %d)"), EquipmentCycleCounter);

    // Store old item for event
    FSuspenseCoreInventoryItemInstance OldItem = EquippedItemInstance;

    // Clear equipped item
    EquippedItemInstance = FSuspenseCoreInventoryItemInstance();

    // Clear predictions
    ActivePredictions.Empty();

    // Notify about change
    if (OldItem.IsValid())
    {
        OnEquippedItemChanged(OldItem, EquippedItemInstance);
    }

    bIsInitialized = false;
    CachedASC = nullptr;

    // Mark for replication
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        // Force net update on the owner actor
        if (AActor* Owner = GetOwner())
        {
            Owner->ForceNetUpdate();
        }
    }
}

void USuspenseCoreEquipmentComponentBase::UpdateEquippedItem(const FSuspenseCoreInventoryItemInstance& NewItemInstance)
{
    SCOPE_CYCLE_COUNTER(STAT_Equipment_UpdateItem);

    if (!bIsInitialized)
    {
        EQUIPMENT_LOG(Warning, TEXT("Cannot update equipped item - not initialized"));
        return;
    }

    if (!NewItemInstance.IsValid())
    {
        EQUIPMENT_LOG(Warning, TEXT("Cannot update with invalid item instance"));
        return;
    }

    FSuspenseCoreInventoryItemInstance OldItem = EquippedItemInstance;
    EquippedItemInstance = NewItemInstance;

    OnEquippedItemChanged(OldItem, NewItemInstance);

    BroadcastEquipmentUpdated();

    // Mark for replication
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        // Force net update on the owner actor
        if (AActor* Owner = GetOwner())
        {
            Owner->ForceNetUpdate();
        }
    }
}

void USuspenseCoreEquipmentComponentBase::SetEquippedItemInstance(const FSuspenseCoreInventoryItemInstance& ItemInstance)
{
    SCOPE_CYCLE_COUNTER(STAT_Equipment_SetItemInstance);

    FSuspenseCoreInventoryItemInstance OldItem = EquippedItemInstance;
    EquippedItemInstance = ItemInstance;

    // Notify about change
    OnEquippedItemChanged(OldItem, ItemInstance);

    // Broadcast appropriate event
    if (ItemInstance.IsValid())
    {
        // Get slot type from item data
        FSuspenseCoreUnifiedItemData ItemData;
        if (GetEquippedItemData(ItemData))
        {
            BroadcastItemEquipped(ItemInstance, ItemData.EquipmentSlot);
        }
    }
    else if (OldItem.IsValid())
    {
        // Get slot type from old item data
        FSuspenseCoreUnifiedItemData OldItemData;
        if (GetItemManager() && GetItemManager()->GetUnifiedItemData(OldItem.ItemID, OldItemData))
        {
            BroadcastItemUnequipped(OldItem, OldItemData.EquipmentSlot);
        }
    }

    // Mark for replication
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        // Force net update on the owner actor
        if (AActor* Owner = GetOwner())
        {
            Owner->ForceNetUpdate();
        }
    }
}

//================================================
// Client Prediction Implementation
//================================================

int32 USuspenseCoreEquipmentComponentBase::StartClientPrediction(const FSuspenseCoreInventoryItemInstance& PredictedInstance)
{
    SCOPE_CYCLE_COUNTER(STAT_Equipment_ClientPrediction);

    // Only allow predictions on clients
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        return 0;
    }

    // Limit concurrent predictions
    if (ActivePredictions.Num() >= MaxConcurrentPredictions)
    {
        EQUIPMENT_LOG(Warning, TEXT("Too many concurrent predictions (%d)"), ActivePredictions.Num());
        CleanupExpiredPredictions();

        if (ActivePredictions.Num() >= MaxConcurrentPredictions)
        {
            return 0;
        }
    }

    // Create new prediction
    FSuspenseCoreEquipmentComponentPredictionData NewPrediction;
    NewPrediction.PredictionKey = NextPredictionKey++;
    NewPrediction.PredictedItem = PredictedInstance;
    NewPrediction.PredictionTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    NewPrediction.bConfirmed = false;

    ActivePredictions.Add(NewPrediction);

    EQUIPMENT_LOG(Verbose, TEXT("Started client prediction %d for item %s"),
        NewPrediction.PredictionKey, *PredictedInstance.ItemID.ToString());

    return NewPrediction.PredictionKey;
}

void USuspenseCoreEquipmentComponentBase::ConfirmClientPrediction(int32 PredictionKey, bool bSuccess, const FSuspenseCoreInventoryItemInstance& ActualInstance)
{
    // Find prediction
    int32 PredictionIndex = ActivePredictions.IndexOfByPredicate(
        [PredictionKey](const FSuspenseCoreEquipmentComponentPredictionData& Data) { return Data.PredictionKey == PredictionKey; }
    );

    if (PredictionIndex == INDEX_NONE)
    {
        EQUIPMENT_LOG(VeryVerbose, TEXT("Prediction %d not found (may have expired)"), PredictionKey);
        return;
    }

    FSuspenseCoreEquipmentComponentPredictionData& Prediction = ActivePredictions[PredictionIndex];

    if (bSuccess)
    {
        Prediction.bConfirmed = true;
        EQUIPMENT_LOG(Verbose, TEXT("Prediction %d confirmed"), PredictionKey);
    }
    else
    {
        // Prediction failed - revert to actual state
        EQUIPMENT_LOG(Warning, TEXT("Prediction %d failed - reverting to server state"), PredictionKey);

        // Update to actual item
        if (ActualInstance.IsValid() || EquippedItemInstance.IsValid())
        {
            UpdateEquippedItem(ActualInstance);
        }
    }

    // Remove confirmed/failed prediction
    ActivePredictions.RemoveAt(PredictionIndex);
}

void USuspenseCoreEquipmentComponentBase::CleanupExpiredPredictions()
{
    if (!GetWorld())
    {
        return;
    }

    const float CurrentTime = GetWorld()->GetTimeSeconds();

    // Remove expired predictions
    ActivePredictions.RemoveAll([CurrentTime](const FSuspenseCoreEquipmentComponentPredictionData& Data)
    {
        return Data.IsExpired(CurrentTime, PredictionTimeoutSeconds);
    });
}

//================================================
// Thread-Safe Cache Access
//================================================

USuspenseCoreItemManager* USuspenseCoreEquipmentComponentBase::GetItemManager() const
{
    SCOPE_CYCLE_COUNTER(STAT_Equipment_GetItemManager);

    FScopeLock Lock(&CacheCriticalSection);

    // Validate cache periodically
    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    const bool bShouldValidate = (CurrentTime - LastCacheValidationTime) > 1.0f;

    if (!CachedItemManager.IsValid() || bShouldValidate)
    {
        LastCacheValidationTime = CurrentTime;

        if (UWorld* World = GetWorld())
        {
            if (UGameInstance* GameInstance = World->GetGameInstance())
            {
                CachedItemManager = GameInstance->GetSubsystem<USuspenseCoreItemManager>();
            }
        }
    }

    return CachedItemManager.Get();
}

USuspenseCoreEventManager* USuspenseCoreEquipmentComponentBase::GetDelegateManager() const
{
    FScopeLock Lock(&CacheCriticalSection);

    // Validate cache periodically
    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    const bool bShouldValidate = (CurrentTime - LastCacheValidationTime) > 1.0f;

    if (!CachedDelegateManager.IsValid() || bShouldValidate)
    {
        LastCacheValidationTime = CurrentTime;

        if (UWorld* World = GetWorld())
        {
            if (UGameInstance* GameInstance = World->GetGameInstance())
            {
                CachedDelegateManager = GameInstance->GetSubsystem<USuspenseCoreEventManager>();
            }
        }
    }

    return CachedDelegateManager.Get();
}

bool USuspenseCoreEquipmentComponentBase::GetEquippedItemData(FSuspenseCoreUnifiedItemData& OutItemData) const
{
    if (!EquippedItemInstance.IsValid())
    {
        return false;
    }

    USuspenseCoreItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        EQUIPMENT_LOG(Warning, TEXT("ItemManager not available"));
        return false;
    }

    return ItemManager->GetUnifiedItemData(EquippedItemInstance.ItemID, OutItemData);
}

float USuspenseCoreEquipmentComponentBase::GetEquippedItemProperty(const FName& PropertyName, float DefaultValue) const
{
    if (!EquippedItemInstance.IsValid())
    {
        return DefaultValue;
    }

    return EquippedItemInstance.GetRuntimeProperty(PropertyName, DefaultValue);
}

void USuspenseCoreEquipmentComponentBase::SetEquippedItemProperty(const FName& PropertyName, float Value)
{
    if (!EquippedItemInstance.IsValid())
    {
        EQUIPMENT_LOG(Warning, TEXT("Cannot set property - no item equipped"));
        return;
    }

    float OldValue = EquippedItemInstance.GetRuntimeProperty(PropertyName, 0.0f);
    EquippedItemInstance.SetRuntimeProperty(PropertyName, Value);

    // Broadcast property change
    BroadcastEquipmentPropertyChanged(PropertyName, OldValue, Value);

    // Mark for replication
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        // Force net update on the owner actor
        if (AActor* Owner = GetOwner())
        {
            Owner->ForceNetUpdate();
        }
    }
}

void USuspenseCoreEquipmentComponentBase::InitializeCoreReferences()
{
    // Pre-cache commonly used subsystems
    GetItemManager();
    GetDelegateManager();
}

bool USuspenseCoreEquipmentComponentBase::ValidateSystemReferences() const
{
    bool bValid = true;

    if (!GetItemManager())
    {
        EQUIPMENT_LOG(Error, TEXT("ItemManager subsystem not available"));
        bValid = false;
    }

    if (!GetDelegateManager())
    {
        EQUIPMENT_LOG(Error, TEXT("EventDelegateManager subsystem not available"));
        bValid = false;
    }

    return bValid;
}

bool USuspenseCoreEquipmentComponentBase::ValidateDelegateManager() const
{
    USuspenseCoreEventManager* Manager = GetDelegateManager();
    return IsValid(Manager);
}

void USuspenseCoreEquipmentComponentBase::OnEquipmentInitialized()
{
    // Base implementation - override in derived classes
}

void USuspenseCoreEquipmentComponentBase::OnEquippedItemChanged(const FSuspenseCoreInventoryItemInstance& OldItem, const FSuspenseCoreInventoryItemInstance& NewItem)
{
    // Base implementation - override in derived classes
    EQUIPMENT_LOG(Verbose, TEXT("Equipped item changed from %s to %s"),
        OldItem.IsValid() ? *OldItem.ItemID.ToString() : TEXT("None"),
        NewItem.IsValid() ? *NewItem.ItemID.ToString() : TEXT("None"));
}

bool USuspenseCoreEquipmentComponentBase::ExecuteOnServer(const FString& FuncName, TFunction<void()> ServerCode)
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        EQUIPMENT_LOG(Warning, TEXT("%s: Must be called on server"), *FuncName);
        return false;
    }

    ServerCode();
    return true;
}

void USuspenseCoreEquipmentComponentBase::LogEventBroadcast(const FString& EventName, bool bSuccess) const
{
    BroadcastEventCounter++;

    if (bSuccess)
    {
        EQUIPMENT_LOG(VeryVerbose, TEXT("Event broadcast: %s (Total: %d)"),
            *EventName, BroadcastEventCounter);
    }
    else
    {
        EQUIPMENT_LOG(Warning, TEXT("Failed to broadcast event: %s"), *EventName);
    }
}

//================================================
// Replication Callbacks
//================================================

void USuspenseCoreEquipmentComponentBase::OnRep_EquippedItemInstance(const FSuspenseCoreInventoryItemInstance& OldInstance)
{
    // Handle item change on clients
    OnEquippedItemChanged(OldInstance, EquippedItemInstance);

    // Broadcast update
    BroadcastEquipmentUpdated();

    EQUIPMENT_LOG(Verbose, TEXT("OnRep_EquippedItemInstance: %s -> %s"),
        OldInstance.IsValid() ? *OldInstance.ItemID.ToString() : TEXT("None"),
        EquippedItemInstance.IsValid() ? *EquippedItemInstance.ItemID.ToString() : TEXT("None"));
}

void USuspenseCoreEquipmentComponentBase::OnRep_ComponentState()
{
    EQUIPMENT_LOG(Verbose, TEXT("OnRep_ComponentState: Initialized=%s, Cycle=%d"),
        bIsInitialized ? TEXT("true") : TEXT("false"), EquipmentCycleCounter);
}

//================================================
// Enhanced Broadcast Methods Implementation
//================================================

void USuspenseCoreEquipmentComponentBase::BroadcastItemEquipped(const FSuspenseCoreInventoryItemInstance& ItemInstance, const FGameplayTag& SlotType)
{
    SCOPE_CYCLE_COUNTER(STAT_Equipment_BroadcastEvent);

    USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(this);
    if (!Manager || !Manager->GetEventBus())
    {
        LogEventBroadcast(TEXT("ItemEquipped"), false);
        return;
    }

    // Get slot index from comprehensive mapping
    const int32 SlotIndex = GetSlotIndexFromTag(SlotType);

    FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(GetOwner())
        .SetString(TEXT("ItemID"), ItemInstance.ItemID.ToString())
        .SetInt(TEXT("Quantity"), ItemInstance.Quantity)
        .SetString(TEXT("SlotType"), SlotType.ToString())
        .SetString(TEXT("InstanceID"), ItemInstance.InstanceID.ToString())
        .SetInt(TEXT("Slot"), SlotIndex);  // Slot index for VisualizationService

    // Set Target actor for VisualizationService (GetOwner() is the character)
    EventData.SetObject(FName("Target"), GetOwner());

    Manager->GetEventBus()->Publish(
        SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_ItemEquipped,
        EventData
    );

    LogEventBroadcast(TEXT("ItemEquipped"), true);
}

void USuspenseCoreEquipmentComponentBase::BroadcastItemUnequipped(const FSuspenseCoreInventoryItemInstance& ItemInstance, const FGameplayTag& SlotType)
{
    SCOPE_CYCLE_COUNTER(STAT_Equipment_BroadcastEvent);

    USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(this);
    if (!Manager || !Manager->GetEventBus())
    {
        LogEventBroadcast(TEXT("ItemUnequipped"), false);
        return;
    }

    // Get slot index from comprehensive mapping
    const int32 SlotIndex = GetSlotIndexFromTag(SlotType);

    FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(GetOwner())
        .SetString(TEXT("ItemID"), ItemInstance.ItemID.ToString())
        .SetInt(TEXT("Quantity"), ItemInstance.Quantity)
        .SetString(TEXT("SlotType"), SlotType.ToString())
        .SetString(TEXT("InstanceID"), ItemInstance.InstanceID.ToString())
        .SetInt(TEXT("Slot"), SlotIndex);

    // Set Target actor for VisualizationService
    EventData.SetObject(FName("Target"), GetOwner());

    Manager->GetEventBus()->Publish(
        SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_ItemUnequipped,
        EventData
    );

    LogEventBroadcast(TEXT("ItemUnequipped"), true);
}

void USuspenseCoreEquipmentComponentBase::BroadcastEquipmentPropertyChanged(const FName& PropertyName, float OldValue, float NewValue)
{
    USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(this);
    if (!Manager || !Manager->GetEventBus())
    {
        LogEventBroadcast(FString::Printf(TEXT("PropertyChanged:%s"), *PropertyName.ToString()), false);
        return;
    }

    FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this)
        .SetString(TEXT("PropertyName"), PropertyName.ToString())
        .SetFloat(TEXT("OldValue"), OldValue)
        .SetFloat(TEXT("NewValue"), NewValue);

    Manager->GetEventBus()->Publish(
        SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_PropertyChanged,
        EventData
    );

    LogEventBroadcast(FString::Printf(TEXT("PropertyChanged:%s"), *PropertyName.ToString()), true);
}

void USuspenseCoreEquipmentComponentBase::BroadcastEquipmentStateChanged(const FGameplayTag& OldState, const FGameplayTag& NewState, bool bInterrupted)
{
    USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(this);
    if (!Manager || !Manager->GetEventBus())
    {
        LogEventBroadcast(TEXT("StateChanged"), false);
        return;
    }

    FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(GetOwner())
        .SetString(TEXT("OldState"), OldState.ToString())
        .SetString(TEXT("NewState"), NewState.ToString())
        .SetBool(TEXT("Interrupted"), bInterrupted);

    Manager->GetEventBus()->Publish(
        SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Data_Changed,
        EventData
    );

    LogEventBroadcast(TEXT("StateChanged"), true);
}

void USuspenseCoreEquipmentComponentBase::BroadcastEquipmentEvent(const FGameplayTag& EventTag, const FString& EventDataStr)
{
    USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(this);
    if (!Manager || !Manager->GetEventBus())
    {
        LogEventBroadcast(EventTag.ToString(), false);
        return;
    }

    FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(GetOwner())
        .SetString(TEXT("Payload"), EventDataStr);

    Manager->GetEventBus()->Publish(EventTag, EventData);

    LogEventBroadcast(EventTag.ToString(), true);
}

void USuspenseCoreEquipmentComponentBase::BroadcastEquipmentUpdated()
{
    USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(this);
    if (!Manager || !Manager->GetEventBus())
    {
        LogEventBroadcast(TEXT("EquipmentUpdated"), false);
        return;
    }

    FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(GetOwner());

    Manager->GetEventBus()->Publish(
        SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Updated,
        EventData
    );

    LogEventBroadcast(TEXT("EquipmentUpdated"), true);
}

void USuspenseCoreEquipmentComponentBase::BroadcastAmmoChanged(float CurrentAmmo, float RemainingAmmo, float MagazineSize)
{
    USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(this);
    if (!Manager || !Manager->GetEventBus())
    {
        LogEventBroadcast(TEXT("AmmoChanged"), false);
        return;
    }

    FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(GetOwner())
        .SetFloat(TEXT("CurrentAmmo"), CurrentAmmo)
        .SetFloat(TEXT("RemainingAmmo"), RemainingAmmo)
        .SetFloat(TEXT("MagazineSize"), MagazineSize);

    Manager->GetEventBus()->Publish(
        SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Weapon_AmmoChanged,
        EventData
    );

    LogEventBroadcast(TEXT("AmmoChanged"), true);
}

void USuspenseCoreEquipmentComponentBase::BroadcastWeaponFired(const FVector& Origin, const FVector& Impact, bool bSuccess, const FGameplayTag& FireMode)
{
    USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(this);
    if (!Manager || !Manager->GetEventBus())
    {
        LogEventBroadcast(TEXT("WeaponFired"), false);
        return;
    }

    FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(GetOwner())
        .SetVector(TEXT("Origin"), Origin)
        .SetVector(TEXT("Impact"), Impact)
        .SetBool(TEXT("Success"), bSuccess)
        .SetString(TEXT("FireMode"), FireMode.ToString());

    Manager->GetEventBus()->Publish(
        SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Weapon_Fired,
        EventData
    );

    LogEventBroadcast(TEXT("WeaponFired"), true);
}

void USuspenseCoreEquipmentComponentBase::BroadcastFireModeChanged(const FGameplayTag& NewFireMode, const FText& FireModeDisplayName)
{
    USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(this);
    if (!Manager || !Manager->GetEventBus())
    {
        LogEventBroadcast(TEXT("FireModeChanged"), false);
        return;
    }

    // Get current spread from weapon data if available
    float CurrentSpread = 0.0f;
    if (EquippedItemInstance.IsValid())
    {
        CurrentSpread = GetEquippedItemProperty(TEXT("CurrentSpread"), 0.0f);
    }

    FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(GetOwner())
        .SetString(TEXT("FireMode"), NewFireMode.ToString())
        .SetString(TEXT("DisplayName"), FireModeDisplayName.ToString())
        .SetFloat(TEXT("Spread"), CurrentSpread);

    Manager->GetEventBus()->Publish(
        SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Weapon_FireModeChanged,
        EventData
    );

    LogEventBroadcast(TEXT("FireModeChanged"), true);
}

void USuspenseCoreEquipmentComponentBase::BroadcastWeaponReload(bool bStarted, float ReloadDuration)
{
    USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(this);
    if (!Manager || !Manager->GetEventBus())
    {
        LogEventBroadcast(TEXT("WeaponReload"), false);
        return;
    }

    FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(GetOwner())
        .SetBool(TEXT("Started"), bStarted)
        .SetFloat(TEXT("Duration"), ReloadDuration);

    FGameplayTag EventTag = bStarted
        ? SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Weapon_ReloadStart
        : SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Weapon_ReloadEnd;

    Manager->GetEventBus()->Publish(EventTag, EventData);

    LogEventBroadcast(TEXT("WeaponReload"), true);
}

void USuspenseCoreEquipmentComponentBase::BroadcastWeaponSpreadUpdated(float NewSpread, float MaxSpread)
{
    USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(this);
    if (!Manager || !Manager->GetEventBus())
    {
        LogEventBroadcast(TEXT("SpreadUpdated"), false);
        return;
    }

    float Percentage = MaxSpread > 0.0f ? (NewSpread / MaxSpread * 100.0f) : 0.0f;

    FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(GetOwner())
        .SetFloat(TEXT("CurrentSpread"), NewSpread)
        .SetFloat(TEXT("MaxSpread"), MaxSpread)
        .SetFloat(TEXT("Percentage"), Percentage);

    Manager->GetEventBus()->Publish(
        SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Weapon_SpreadUpdated,
        EventData
    );

    LogEventBroadcast(TEXT("SpreadUpdated"), true);
}

//================================================
// ISuspenseCoreAbilityProvider Implementation
//================================================

void USuspenseCoreEquipmentComponentBase::InitializeAbilityProvider_Implementation(UAbilitySystemComponent* InASC)
{
    CachedASC = InASC;
    EQUIPMENT_LOG(Log, TEXT("Ability provider initialized"));
}

FGameplayAbilitySpecHandle USuspenseCoreEquipmentComponentBase::GrantAbility_Implementation(TSubclassOf<UGameplayAbility> AbilityClass, int32 Level, int32 InputID)
{
    SCOPE_CYCLE_COUNTER(STAT_Equipment_GrantAbility);

    if (!CachedASC || !AbilityClass)
    {
        EQUIPMENT_LOG(Warning, TEXT("Cannot grant ability - ASC or ability class invalid"));
        return FGameplayAbilitySpecHandle();
    }

    FGameplayAbilitySpec AbilitySpec(AbilityClass, Level, InputID, this);
    FGameplayAbilitySpecHandle Handle = CachedASC->GiveAbility(AbilitySpec);

    EQUIPMENT_LOG(Log, TEXT("Granted ability: %s (Level: %d, InputID: %d)"),
        *GetNameSafe(AbilityClass), Level, InputID);

    return Handle;
}

void USuspenseCoreEquipmentComponentBase::RemoveAbility_Implementation(FGameplayAbilitySpecHandle AbilityHandle)
{
    if (!CachedASC || !AbilityHandle.IsValid())
    {
        return;
    }

    CachedASC->ClearAbility(AbilityHandle);
    EQUIPMENT_LOG(Log, TEXT("Removed ability handle"));
}

FActiveGameplayEffectHandle USuspenseCoreEquipmentComponentBase::ApplyEffectToSelf_Implementation(TSubclassOf<UGameplayEffect> EffectClass, float Level)
{
    if (!CachedASC || !EffectClass)
    {
        EQUIPMENT_LOG(Warning, TEXT("Cannot apply effect - ASC or effect class invalid"));
        return FActiveGameplayEffectHandle();
    }

    FGameplayEffectContextHandle Context = CachedASC->MakeEffectContext();
    Context.AddSourceObject(this);

    FGameplayEffectSpecHandle Spec = CachedASC->MakeOutgoingSpec(EffectClass, Level, Context);
    if (Spec.IsValid())
    {
        FActiveGameplayEffectHandle Handle = CachedASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
        EQUIPMENT_LOG(Log, TEXT("Applied effect: %s (Level: %.1f)"),
            *GetNameSafe(EffectClass), Level);
        return Handle;
    }

    return FActiveGameplayEffectHandle();
}

void USuspenseCoreEquipmentComponentBase::RemoveEffect_Implementation(FActiveGameplayEffectHandle EffectHandle)
{
    if (!CachedASC || !EffectHandle.IsValid())
    {
        return;
    }

    CachedASC->RemoveActiveGameplayEffect(EffectHandle);
    EQUIPMENT_LOG(Log, TEXT("Removed effect handle"));
}

//================================================
// Slot Index Mapping Implementation
// Based on SuspenseCoreItemDatabase.json - 17 actual equipment slots
// @see Documentation/EquipmentWidget_ImplementationPlan.md
//================================================

const TMap<FName, int32>& USuspenseCoreEquipmentComponentBase::GetSlotTypeMapping()
{
    // Static mapping initialized once and cached
    // Indices match EEquipmentSlotType enum and UI widget layout (0-16)
    static TMap<FName, int32> SlotMapping;
    static bool bInitialized = false;

    if (!bInitialized)
    {
        // Weapons (indices 0-3)
        SlotMapping.Add(FName(TEXT("Equipment.Slot.PrimaryWeapon")), 0);   // AK-74M, rifles
        SlotMapping.Add(FName(TEXT("Equipment.Slot.SecondaryWeapon")), 1); // MP5SD, SMGs
        SlotMapping.Add(FName(TEXT("Equipment.Slot.Holster")), 2);         // Glock 17, pistols
        SlotMapping.Add(FName(TEXT("Equipment.Slot.Scabbard")), 3);        // M9 Bayonet, knives

        // Head gear (indices 4-7)
        SlotMapping.Add(FName(TEXT("Equipment.Slot.Headwear")), 4);        // 6B47 helmet
        SlotMapping.Add(FName(TEXT("Equipment.Slot.Earpiece")), 5);        // Peltor ComTac
        SlotMapping.Add(FName(TEXT("Equipment.Slot.Eyewear")), 6);         // Glasses, NVG
        SlotMapping.Add(FName(TEXT("Equipment.Slot.FaceCover")), 7);       // Shemagh, masks

        // Body gear (indices 8-10)
        SlotMapping.Add(FName(TEXT("Equipment.Slot.BodyArmor")), 8);       // 6B13 armor
        SlotMapping.Add(FName(TEXT("Equipment.Slot.TacticalRig")), 9);     // TV-110 rig
        SlotMapping.Add(FName(TEXT("Equipment.Slot.Backpack")), 10);       // Bergen backpack

        // Special slots (indices 11-12)
        SlotMapping.Add(FName(TEXT("Equipment.Slot.SecureContainer")), 11); // Gamma container
        SlotMapping.Add(FName(TEXT("Equipment.Slot.Armband")), 12);         // Blue armband

        // Quick slots (indices 13-16)
        SlotMapping.Add(FName(TEXT("Equipment.Slot.QuickSlot1")), 13);     // IFAK, meds
        SlotMapping.Add(FName(TEXT("Equipment.Slot.QuickSlot2")), 14);     // F-1 grenade
        SlotMapping.Add(FName(TEXT("Equipment.Slot.QuickSlot3")), 15);     // Ammo
        SlotMapping.Add(FName(TEXT("Equipment.Slot.QuickSlot4")), 16);     // Ammo

        bInitialized = true;
    }

    return SlotMapping;
}

int32 USuspenseCoreEquipmentComponentBase::GetSlotIndexFromTag(const FGameplayTag& SlotType) const
{
    if (!SlotType.IsValid())
    {
        return 0; // Default to primary slot
    }

    // Get the tag name as FName for lookup
    const FName TagName = SlotType.GetTagName();

    // Look up in the static mapping
    const TMap<FName, int32>& Mapping = GetSlotTypeMapping();
    if (const int32* FoundIndex = Mapping.Find(TagName))
    {
        return *FoundIndex;
    }

    // Fallback: Check tag hierarchy by matching partial tags
    // This handles cases like Equipment.Slot.PrimaryWeapon.Rifle
    for (const auto& Pair : Mapping)
    {
        if (SlotType.MatchesTag(FGameplayTag::RequestGameplayTag(Pair.Key, false)))
        {
            return Pair.Value;
        }
    }

    // Default to primary slot if no match found
    EQUIPMENT_LOG(VeryVerbose, TEXT("Unknown slot type: %s, defaulting to index 0"), *SlotType.ToString());
    return 0;
}
