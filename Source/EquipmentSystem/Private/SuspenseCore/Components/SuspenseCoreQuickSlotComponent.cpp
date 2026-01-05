// SuspenseCoreQuickSlotComponent.cpp
// QuickSlot system for fast magazine/item access
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Components/SuspenseCoreQuickSlotComponent.h"
#include "SuspenseCore/Components/SuspenseCoreMagazineComponent.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Services/SuspenseCoreItemUseService.h"
#include "SuspenseCore/Services/SuspenseCoreServiceProvider.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "Engine/Texture2D.h"
#include "Net/UnrealNetwork.h"
#include "GameplayTagsManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreQuickSlot, Log, All);

#define QUICKSLOT_LOG(Verbosity, Format, ...) \
    UE_LOG(LogSuspenseCoreQuickSlot, Verbosity, TEXT("[%s] " Format), *GetNameSafe(GetOwner()), ##__VA_ARGS__)

//==================================================================
// Constructor
//==================================================================

USuspenseCoreQuickSlotComponent::USuspenseCoreQuickSlotComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    SetIsReplicatedByDefault(true);

    // Initialize category tags - using consistent Item.* namespace (matches SuspenseCoreGameplayTags)
    MagazineCategoryTag = FGameplayTag::RequestGameplayTag(FName("Item.Magazine"), false);
    ConsumableCategoryTag = FGameplayTag::RequestGameplayTag(FName("Item.Consumable"), false);
    GrenadeCategoryTag = FGameplayTag::RequestGameplayTag(FName("Item.Throwable"), false);
}

//==================================================================
// UActorComponent Interface
//==================================================================

void USuspenseCoreQuickSlotComponent::BeginPlay()
{
    Super::BeginPlay();

    InitializeSlots();

    QUICKSLOT_LOG(Log, TEXT("QuickSlotComponent initialized with %d slots"), SUSPENSECORE_QUICKSLOT_COUNT);
}

void USuspenseCoreQuickSlotComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Update cooldowns
    UpdateCooldowns(DeltaTime);
}

void USuspenseCoreQuickSlotComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(USuspenseCoreQuickSlotComponent, QuickSlots);
    DOREPLIFETIME(USuspenseCoreQuickSlotComponent, StoredMagazines);
}

//==================================================================
// Initialization
//==================================================================

void USuspenseCoreQuickSlotComponent::InitializeWithMagazineComponent(USuspenseCoreMagazineComponent* InMagazineComponent)
{
    if (InMagazineComponent)
    {
        MagazineComponent = InMagazineComponent;
        QUICKSLOT_LOG(Log, TEXT("MagazineComponent set for fast reloads"));
    }
}

void USuspenseCoreQuickSlotComponent::InitializeSlots()
{
    QuickSlots.SetNum(SUSPENSECORE_QUICKSLOT_COUNT);
    StoredMagazines.SetNum(SUSPENSECORE_QUICKSLOT_COUNT);

    for (int32 i = 0; i < SUSPENSECORE_QUICKSLOT_COUNT; ++i)
    {
        QuickSlots[i].SlotIndex = i;
        QuickSlots[i].SlotTag = FGameplayTag::RequestGameplayTag(
            FName(*FString::Printf(TEXT("Equipment.QuickSlot.%d"), i + 1)), false);
        QuickSlots[i].Clear();

        StoredMagazines[i] = FSuspenseCoreMagazineInstance();
    }
}

//==================================================================
// Slot Assignment
//==================================================================

bool USuspenseCoreQuickSlotComponent::AssignItemToSlot(int32 SlotIndex, const FGuid& ItemInstanceID, FName ItemID)
{
    if (!IsValidSlotIndex(SlotIndex))
    {
        QUICKSLOT_LOG(Warning, TEXT("AssignItemToSlot: Invalid slot index %d"), SlotIndex);
        return false;
    }

    if (!ItemInstanceID.IsValid() || ItemID.IsNone())
    {
        QUICKSLOT_LOG(Warning, TEXT("AssignItemToSlot: Invalid item data"));
        return false;
    }

    // On client, send to server
    if (!GetOwner()->HasAuthority())
    {
        Server_AssignItemToSlot(SlotIndex, ItemInstanceID, ItemID);
        // Optimistic local update
        FGuid OldID = QuickSlots[SlotIndex].AssignedItemInstanceID;
        QuickSlots[SlotIndex].AssignItem(ItemInstanceID, ItemID);
        NotifySlotChanged(SlotIndex, OldID, ItemInstanceID);
        return true;
    }

    // Server-side assignment
    FGuid OldInstanceID = QuickSlots[SlotIndex].AssignedItemInstanceID;
    QuickSlots[SlotIndex].AssignItem(ItemInstanceID, ItemID);

    NotifySlotChanged(SlotIndex, OldInstanceID, ItemInstanceID);

    QUICKSLOT_LOG(Log, TEXT("Assigned item %s to slot %d"), *ItemID.ToString(), SlotIndex);
    return true;
}

bool USuspenseCoreQuickSlotComponent::AssignMagazineToSlot(int32 SlotIndex, const FSuspenseCoreMagazineInstance& Magazine)
{
    if (!IsValidSlotIndex(SlotIndex))
    {
        QUICKSLOT_LOG(Warning, TEXT("AssignMagazineToSlot: Invalid slot index %d"), SlotIndex);
        return false;
    }

    if (!Magazine.IsValid())
    {
        QUICKSLOT_LOG(Warning, TEXT("AssignMagazineToSlot: Invalid magazine"));
        return false;
    }

    // Store magazine instance
    StoredMagazines[SlotIndex] = Magazine;

    // Assign to slot
    return AssignItemToSlot(SlotIndex, Magazine.InstanceGuid, Magazine.MagazineID);
}

void USuspenseCoreQuickSlotComponent::ClearSlotInternal(int32 SlotIndex)
{
    if (!IsValidSlotIndex(SlotIndex))
    {
        return;
    }

    FGuid OldInstanceID = QuickSlots[SlotIndex].AssignedItemInstanceID;
    QuickSlots[SlotIndex].Clear();
    StoredMagazines[SlotIndex] = FSuspenseCoreMagazineInstance();

    NotifySlotChanged(SlotIndex, OldInstanceID, FGuid());

    QUICKSLOT_LOG(Log, TEXT("Cleared slot %d"), SlotIndex);
}

void USuspenseCoreQuickSlotComponent::ClearAllSlots()
{
    for (int32 i = 0; i < SUSPENSECORE_QUICKSLOT_COUNT; ++i)
    {
        ClearSlotInternal(i);
    }
}

bool USuspenseCoreQuickSlotComponent::SwapSlots(int32 SlotIndexA, int32 SlotIndexB)
{
    if (!IsValidSlotIndex(SlotIndexA) || !IsValidSlotIndex(SlotIndexB))
    {
        return false;
    }

    if (SlotIndexA == SlotIndexB)
    {
        return true;
    }

    // Swap quick slot data
    FSuspenseCoreQuickSlot TempSlot = QuickSlots[SlotIndexA];
    QuickSlots[SlotIndexA] = QuickSlots[SlotIndexB];
    QuickSlots[SlotIndexB] = TempSlot;

    // Preserve slot indices
    QuickSlots[SlotIndexA].SlotIndex = SlotIndexA;
    QuickSlots[SlotIndexB].SlotIndex = SlotIndexB;

    // Swap stored magazines
    FSuspenseCoreMagazineInstance TempMag = StoredMagazines[SlotIndexA];
    StoredMagazines[SlotIndexA] = StoredMagazines[SlotIndexB];
    StoredMagazines[SlotIndexB] = TempMag;

    // Notify both slots changed
    OnQuickSlotChanged.Broadcast(SlotIndexA, QuickSlots[SlotIndexB].AssignedItemInstanceID, QuickSlots[SlotIndexA].AssignedItemInstanceID);
    OnQuickSlotChanged.Broadcast(SlotIndexB, QuickSlots[SlotIndexA].AssignedItemInstanceID, QuickSlots[SlotIndexB].AssignedItemInstanceID);

    QUICKSLOT_LOG(Log, TEXT("Swapped slots %d and %d"), SlotIndexA, SlotIndexB);
    return true;
}

//==================================================================
// Slot Usage
//==================================================================

void USuspenseCoreQuickSlotComponent::StartSlotCooldown(int32 SlotIndex, float CooldownDuration)
{
    if (!IsValidSlotIndex(SlotIndex))
    {
        return;
    }

    QuickSlots[SlotIndex].CooldownRemaining = FMath::Max(0.0f, CooldownDuration);
    OnQuickSlotCooldownChanged.Broadcast(SlotIndex, CooldownDuration);

    // Publish EventBus event for UI
    USuspenseCoreEventManager* EventManager = USuspenseCoreEventManager::Get(this);
    if (EventManager)
    {
        if (USuspenseCoreEventBus* EventBus = EventManager->GetEventBus())
        {
            FSuspenseCoreEventData EventData;
            EventData.SetInt(TEXT("SlotIndex"), SlotIndex);
            EventData.SetFloat(TEXT("Duration"), CooldownDuration);
            EventBus->Publish(SuspenseCoreEquipmentTags::QuickSlot::TAG_Equipment_Event_QuickSlot_CooldownStarted, EventData);
        }
    }
}

//==================================================================
// Queries
//==================================================================

TArray<FSuspenseCoreQuickSlot> USuspenseCoreQuickSlotComponent::GetAllQuickSlots() const
{
    return QuickSlots;
}

TArray<FSuspenseCoreQuickSlot> USuspenseCoreQuickSlotComponent::GetMagazineSlots() const
{
    TArray<FSuspenseCoreQuickSlot> MagazineSlots;

    for (int32 i = 0; i < QuickSlots.Num(); ++i)
    {
        if (StoredMagazines.IsValidIndex(i) && StoredMagazines[i].IsValid())
        {
            MagazineSlots.Add(QuickSlots[i]);
        }
        else if (QuickSlots[i].HasItem() && IsItemMagazine(QuickSlots[i].AssignedItemID))
        {
            MagazineSlots.Add(QuickSlots[i]);
        }
    }

    return MagazineSlots;
}

float USuspenseCoreQuickSlotComponent::GetSlotCooldown(int32 SlotIndex) const
{
    if (!IsValidSlotIndex(SlotIndex))
    {
        return 0.0f;
    }

    return QuickSlots[SlotIndex].CooldownRemaining;
}

bool USuspenseCoreQuickSlotComponent::IsItemMagazine(FName ItemID) const
{
    // TODO: Query DataManager to check item category
    // For now, check if we have a stored magazine with this ID
    for (const FSuspenseCoreMagazineInstance& Mag : StoredMagazines)
    {
        if (Mag.MagazineID == ItemID)
        {
            return true;
        }
    }

    // Check if item name contains "mag" or "magazine" (temporary heuristic)
    FString ItemStr = ItemID.ToString().ToLower();
    return ItemStr.Contains(TEXT("mag")) || ItemStr.Contains(TEXT("stanag")) || ItemStr.Contains(TEXT("pmag"));
}

bool USuspenseCoreQuickSlotComponent::FindSlotWithItem(const FGuid& ItemInstanceID, int32& OutSlotIndex) const
{
    for (int32 i = 0; i < QuickSlots.Num(); ++i)
    {
        if (QuickSlots[i].AssignedItemInstanceID == ItemInstanceID)
        {
            OutSlotIndex = i;
            return true;
        }
    }

    OutSlotIndex = -1;
    return false;
}

//==================================================================
// Magazine Integration
//==================================================================

void USuspenseCoreQuickSlotComponent::UpdateMagazineInSlot(int32 SlotIndex, const FSuspenseCoreMagazineInstance& UpdatedMagazine)
{
    if (!IsValidSlotIndex(SlotIndex))
    {
        return;
    }

    if (StoredMagazines.IsValidIndex(SlotIndex))
    {
        StoredMagazines[SlotIndex] = UpdatedMagazine;
        QUICKSLOT_LOG(Verbose, TEXT("Updated magazine in slot %d: %d/%d rounds"),
            SlotIndex, UpdatedMagazine.CurrentRoundCount, UpdatedMagazine.MaxCapacity);
    }
}

//==================================================================
// ISuspenseCoreQuickSlotProvider Interface Implementation
//==================================================================

FSuspenseCoreQuickSlot USuspenseCoreQuickSlotComponent::GetQuickSlot_Implementation(int32 SlotIndex) const
{
    if (!IsValidSlotIndex(SlotIndex))
    {
        return FSuspenseCoreQuickSlot();
    }

    return QuickSlots[SlotIndex];
}

bool USuspenseCoreQuickSlotComponent::IsSlotReady_Implementation(int32 SlotIndex) const
{
    if (!IsValidSlotIndex(SlotIndex))
    {
        return false;
    }

    return QuickSlots[SlotIndex].IsReady();
}

bool USuspenseCoreQuickSlotComponent::HasItemInSlot_Implementation(int32 SlotIndex) const
{
    if (!IsValidSlotIndex(SlotIndex))
    {
        return false;
    }

    return QuickSlots[SlotIndex].HasItem();
}

bool USuspenseCoreQuickSlotComponent::UseQuickSlot_Implementation(int32 SlotIndex)
{
    if (!IsValidSlotIndex(SlotIndex))
    {
        QUICKSLOT_LOG(Warning, TEXT("UseQuickSlot: Invalid slot index %d"), SlotIndex);
        return false;
    }

    if (!IsSlotReady_Implementation(SlotIndex))
    {
        QUICKSLOT_LOG(Verbose, TEXT("UseQuickSlot: Slot %d not ready"), SlotIndex);
        OnQuickSlotUsed.Broadcast(SlotIndex, false);
        return false;
    }

    // On client, send to server
    if (!GetOwner()->HasAuthority())
    {
        Server_UseQuickSlot(SlotIndex);
    }

    // Route through ItemUseService for unified handling
    bool bSuccess = false;

    USuspenseCoreServiceProvider* Provider = USuspenseCoreServiceProvider::Get(this);
    if (Provider)
    {
        if (USuspenseCoreItemUseServiceImpl* ItemUseService = Provider->GetService<USuspenseCoreItemUseServiceImpl>())
        {
            FSuspenseCoreItemUseResponse Response = ItemUseService->UseQuickSlot(SlotIndex, GetOwner());
            bSuccess = Response.IsSuccess() || Response.IsInProgress();

            if (bSuccess)
            {
                // Apply cooldown from handler response
                if (Response.Cooldown > 0.0f)
                {
                    StartSlotCooldown(SlotIndex, Response.Cooldown);
                }

                QUICKSLOT_LOG(Log, TEXT("UseQuickSlot: Routed slot %d through ItemUseService (Handler=%s, Duration=%.2fs)"),
                    SlotIndex,
                    *Response.HandlerTag.ToString(),
                    Response.Duration);
            }
            else
            {
                QUICKSLOT_LOG(Verbose, TEXT("UseQuickSlot: ItemUseService failed for slot %d: %s"),
                    SlotIndex,
                    *Response.Message.ToString());
            }
        }
        else
        {
            QUICKSLOT_LOG(Warning, TEXT("UseQuickSlot: ItemUseService not available, falling back to direct execution"));
            // Fallback to direct execution if service not available
            bSuccess = ExecuteQuickSlotDirect(SlotIndex);
        }
    }
    else
    {
        QUICKSLOT_LOG(Warning, TEXT("UseQuickSlot: ServiceProvider not available, falling back to direct execution"));
        // Fallback to direct execution if service provider not available
        bSuccess = ExecuteQuickSlotDirect(SlotIndex);
    }

    OnQuickSlotUsed.Broadcast(SlotIndex, bSuccess);

    // Publish EventBus event for UI
    if (bSuccess)
    {
        USuspenseCoreEventManager* EventManager = USuspenseCoreEventManager::Get(this);
        if (EventManager)
        {
            if (USuspenseCoreEventBus* EventBus = EventManager->GetEventBus())
            {
                FSuspenseCoreEventData EventData;
                EventData.SetInt(TEXT("SlotIndex"), SlotIndex);
                EventBus->Publish(SuspenseCoreEquipmentTags::QuickSlot::TAG_Equipment_Event_QuickSlot_Used, EventData);
            }
        }

        QUICKSLOT_LOG(Log, TEXT("Used QuickSlot %d successfully"), SlotIndex);
    }

    return bSuccess;
}

bool USuspenseCoreQuickSlotComponent::QuickSwapMagazine_Implementation(int32 SlotIndex, bool bEmergencyDrop)
{
    if (!IsValidSlotIndex(SlotIndex))
    {
        QUICKSLOT_LOG(Warning, TEXT("QuickSwapMagazine: Invalid slot index %d"), SlotIndex);
        return false;
    }

    if (!IsSlotReady_Implementation(SlotIndex))
    {
        QUICKSLOT_LOG(Verbose, TEXT("QuickSwapMagazine: Slot %d not ready"), SlotIndex);
        return false;
    }

    // On client, send to server
    if (!GetOwner()->HasAuthority())
    {
        Server_QuickSwapMagazine(SlotIndex, bEmergencyDrop);
    }

    return ExecuteMagazineSwap(SlotIndex, bEmergencyDrop);
}

bool USuspenseCoreQuickSlotComponent::GetMagazineFromSlot_Implementation(int32 SlotIndex, FSuspenseCoreMagazineInstance& OutMagazine) const
{
    if (!IsValidSlotIndex(SlotIndex))
    {
        return false;
    }

    if (StoredMagazines.IsValidIndex(SlotIndex) && StoredMagazines[SlotIndex].IsValid())
    {
        OutMagazine = StoredMagazines[SlotIndex];
        return true;
    }

    return false;
}

bool USuspenseCoreQuickSlotComponent::GetFirstMagazineSlotIndex_Implementation(int32& OutSlotIndex) const
{
    for (int32 i = 0; i < QuickSlots.Num(); ++i)
    {
        if (StoredMagazines.IsValidIndex(i) && StoredMagazines[i].IsValid() && StoredMagazines[i].HasAmmo())
        {
            OutSlotIndex = i;
            return true;
        }
    }

    OutSlotIndex = -1;
    return false;
}

bool USuspenseCoreQuickSlotComponent::StoreEjectedMagazine_Implementation(const FSuspenseCoreMagazineInstance& EjectedMagazine, int32& OutSlotIndex)
{
    if (!EjectedMagazine.IsValid())
    {
        OutSlotIndex = -1;
        return false;
    }

    // Find first empty slot or slot with same magazine type
    for (int32 i = 0; i < SUSPENSECORE_QUICKSLOT_COUNT; ++i)
    {
        if (!QuickSlots[i].HasItem())
        {
            // Empty slot found
            AssignMagazineToSlot(i, EjectedMagazine);
            OutSlotIndex = i;
            QUICKSLOT_LOG(Log, TEXT("Stored ejected magazine in slot %d (%d rounds)"),
                i, EjectedMagazine.CurrentRoundCount);
            return true;
        }
    }

    // No empty slot available
    OutSlotIndex = -1;
    QUICKSLOT_LOG(Warning, TEXT("No empty slot to store ejected magazine"));
    return false;
}

void USuspenseCoreQuickSlotComponent::ClearSlot_Implementation(int32 SlotIndex)
{
    // On client, send to server
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        Server_ClearSlot(SlotIndex);
    }

    ClearSlotInternal(SlotIndex);
}

//==================================================================
// Server RPCs
//==================================================================

void USuspenseCoreQuickSlotComponent::Server_AssignItemToSlot_Implementation(int32 SlotIndex, const FGuid& ItemInstanceID, FName ItemID)
{
    AssignItemToSlot(SlotIndex, ItemInstanceID, ItemID);
}

bool USuspenseCoreQuickSlotComponent::Server_AssignItemToSlot_Validate(int32 SlotIndex, const FGuid& ItemInstanceID, FName ItemID)
{
    return IsValidSlotIndex(SlotIndex) && ItemInstanceID.IsValid();
}

void USuspenseCoreQuickSlotComponent::Server_ClearSlot_Implementation(int32 SlotIndex)
{
    ClearSlotInternal(SlotIndex);
}

bool USuspenseCoreQuickSlotComponent::Server_ClearSlot_Validate(int32 SlotIndex)
{
    return IsValidSlotIndex(SlotIndex);
}

void USuspenseCoreQuickSlotComponent::Server_UseQuickSlot_Implementation(int32 SlotIndex)
{
    UseQuickSlot_Implementation(SlotIndex);
}

bool USuspenseCoreQuickSlotComponent::Server_UseQuickSlot_Validate(int32 SlotIndex)
{
    return IsValidSlotIndex(SlotIndex);
}

void USuspenseCoreQuickSlotComponent::Server_QuickSwapMagazine_Implementation(int32 SlotIndex, bool bEmergencyDrop)
{
    QuickSwapMagazine_Implementation(SlotIndex, bEmergencyDrop);
}

bool USuspenseCoreQuickSlotComponent::Server_QuickSwapMagazine_Validate(int32 SlotIndex, bool bEmergencyDrop)
{
    return IsValidSlotIndex(SlotIndex);
}

//==================================================================
// Replication Callbacks
//==================================================================

void USuspenseCoreQuickSlotComponent::OnRep_QuickSlots()
{
    // Notify UI that slots have been updated
    for (int32 i = 0; i < QuickSlots.Num(); ++i)
    {
        OnQuickSlotChanged.Broadcast(i, FGuid(), QuickSlots[i].AssignedItemInstanceID);
    }

    QUICKSLOT_LOG(Verbose, TEXT("QuickSlots replicated"));
}

//==================================================================
// Internal Methods
//==================================================================

bool USuspenseCoreQuickSlotComponent::IsValidSlotIndex(int32 SlotIndex) const
{
    return SlotIndex >= 0 && SlotIndex < SUSPENSECORE_QUICKSLOT_COUNT;
}

void USuspenseCoreQuickSlotComponent::UpdateCooldowns(float DeltaTime)
{
    for (int32 i = 0; i < QuickSlots.Num(); ++i)
    {
        if (QuickSlots[i].CooldownRemaining > 0.0f)
        {
            float OldCooldown = QuickSlots[i].CooldownRemaining;
            QuickSlots[i].CooldownRemaining = FMath::Max(0.0f, QuickSlots[i].CooldownRemaining - DeltaTime);

            // Notify when cooldown finishes
            if (OldCooldown > 0.0f && QuickSlots[i].CooldownRemaining <= 0.0f)
            {
                OnQuickSlotCooldownChanged.Broadcast(i, 0.0f);
                OnQuickSlotAvailabilityChanged.Broadcast(i, QuickSlots[i].HasItem());

                // Publish EventBus event for UI
                USuspenseCoreEventManager* EventManager = USuspenseCoreEventManager::Get(this);
                if (EventManager)
                {
                    if (USuspenseCoreEventBus* EventBus = EventManager->GetEventBus())
                    {
                        FSuspenseCoreEventData EventData;
                        EventData.SetInt(TEXT("SlotIndex"), i);
                        EventBus->Publish(SuspenseCoreEquipmentTags::QuickSlot::TAG_Equipment_Event_QuickSlot_CooldownEnded, EventData);
                    }
                }
            }
        }
    }
}

bool USuspenseCoreQuickSlotComponent::ValidateSlotItem(int32 SlotIndex) const
{
    if (!IsValidSlotIndex(SlotIndex))
    {
        return false;
    }

    // TODO: Query inventory component to validate item still exists
    return QuickSlots[SlotIndex].HasItem();
}

bool USuspenseCoreQuickSlotComponent::ExecuteMagazineSwap(int32 SlotIndex, bool bEmergencyDrop)
{
    USuspenseCoreMagazineComponent* MagComp = MagazineComponent.Get();

    // If no cached MagazineComponent, try to find one dynamically
    // This handles cases where weapon changes or InitializeWithMagazineComponent wasn't called
    if (!MagComp)
    {
        AActor* Owner = GetOwner();
        if (Owner)
        {
            // First try to find on owner
            MagComp = Owner->FindComponentByClass<USuspenseCoreMagazineComponent>();

            // If not found, search child actors (attached weapons)
            if (!MagComp)
            {
                TArray<AActor*> AttachedActors;
                Owner->GetAttachedActors(AttachedActors);
                for (AActor* Attached : AttachedActors)
                {
                    MagComp = Attached->FindComponentByClass<USuspenseCoreMagazineComponent>();
                    if (MagComp)
                    {
                        QUICKSLOT_LOG(Log, TEXT("ExecuteMagazineSwap: Found MagazineComponent on attached actor %s"),
                            *Attached->GetName());
                        break;
                    }
                }
            }

            // Cache it for future use
            if (MagComp)
            {
                MagazineComponent = MagComp;
                QUICKSLOT_LOG(Log, TEXT("ExecuteMagazineSwap: Cached MagazineComponent dynamically"));
            }
        }
    }

    if (!MagComp)
    {
        QUICKSLOT_LOG(Warning, TEXT("ExecuteMagazineSwap: No MagazineComponent available (not on owner or attached actors)"));
        return false;
    }

    FSuspenseCoreMagazineInstance NewMagazine;
    if (!GetMagazineFromSlot_Implementation(SlotIndex, NewMagazine))
    {
        QUICKSLOT_LOG(Warning, TEXT("ExecuteMagazineSwap: No magazine in slot %d"), SlotIndex);
        return false;
    }

    // Use MagazineComponent's swap function
    // NOTE: MagazineComponent handles slot clearing AND returns ejected magazine to the same slot
    // Do NOT clear slot here - that would delete the swapped-back magazine!
    bool bSuccess = MagComp->SwapMagazineFromQuickSlot(SlotIndex, bEmergencyDrop);

    return bSuccess;
}

bool USuspenseCoreQuickSlotComponent::ExecuteConsumableUse(int32 SlotIndex)
{
    // TODO: Implement consumable use through ability system
    QUICKSLOT_LOG(Log, TEXT("ExecuteConsumableUse: Slot %d (not implemented)"), SlotIndex);
    return false;
}

bool USuspenseCoreQuickSlotComponent::ExecuteGrenadePrepare(int32 SlotIndex)
{
    // TODO: Implement grenade preparation through ability system
    QUICKSLOT_LOG(Log, TEXT("ExecuteGrenadePrepare: Slot %d (not implemented)"), SlotIndex);
    return false;
}

bool USuspenseCoreQuickSlotComponent::ExecuteQuickSlotDirect(int32 SlotIndex)
{
    // Direct execution fallback - used when ItemUseService is not available
    // This preserves the original behavior for backwards compatibility

    if (!IsValidSlotIndex(SlotIndex))
    {
        return false;
    }

    if (StoredMagazines[SlotIndex].IsValid())
    {
        // It's a magazine - do quick reload
        return ExecuteMagazineSwap(SlotIndex, false);
    }

    // Check item category
    FName ItemID = QuickSlots[SlotIndex].AssignedItemID;
    if (IsItemMagazine(ItemID))
    {
        return ExecuteMagazineSwap(SlotIndex, false);
    }

    // Default: treat as consumable
    return ExecuteConsumableUse(SlotIndex);
}

void USuspenseCoreQuickSlotComponent::NotifySlotChanged(int32 SlotIndex, const FGuid& OldItemID, const FGuid& NewItemID)
{
    // Broadcast delegate events (for direct Blueprint bindings)
    OnQuickSlotChanged.Broadcast(SlotIndex, OldItemID, NewItemID);

    bool bHasItem = NewItemID.IsValid();
    OnQuickSlotAvailabilityChanged.Broadcast(SlotIndex, bHasItem);

    // Publish EventBus events (for UI system)
    USuspenseCoreEventManager* EventManager = USuspenseCoreEventManager::Get(this);
    if (!EventManager)
    {
        return;
    }

    USuspenseCoreEventBus* EventBus = EventManager->GetEventBus();
    if (!EventBus)
    {
        return;
    }

    using namespace SuspenseCoreEquipmentTags::QuickSlot;

    if (bHasItem && IsValidSlotIndex(SlotIndex))
    {
        // Slot assigned - publish with item data
        FSuspenseCoreEventData EventData;
        EventData.SetInt(TEXT("SlotIndex"), SlotIndex);
        EventData.SetString(TEXT("ItemID"), QuickSlots[SlotIndex].AssignedItemID.ToString());

        // Check if it's a magazine
        bool bIsMagazine = StoredMagazines.IsValidIndex(SlotIndex) && StoredMagazines[SlotIndex].IsValid();
        EventData.SetBool(TEXT("IsMagazine"), bIsMagazine);

        if (bIsMagazine)
        {
            EventData.SetInt(TEXT("MagazineRounds"), StoredMagazines[SlotIndex].CurrentRoundCount);
            EventData.SetInt(TEXT("MagazineCapacity"), StoredMagazines[SlotIndex].MaxCapacity);
        }
        else
        {
            EventData.SetInt(TEXT("Quantity"), 1); // TODO: Get actual quantity from inventory
        }

        EventData.SetBool(TEXT("IsAvailable"), QuickSlots[SlotIndex].bIsAvailable);

        // Try to get item icon from DataManager
        USuspenseCoreDataManager* DataManager = USuspenseCoreDataManager::Get(this);
        if (DataManager)
        {
            FSuspenseCoreUnifiedItemData ItemData;
            if (DataManager->GetUnifiedItemData(QuickSlots[SlotIndex].AssignedItemID, ItemData))
            {
                // Load soft pointer synchronously to get actual texture
                if (UTexture2D* IconTexture = ItemData.Icon.LoadSynchronous())
                {
                    EventData.SetObject(TEXT("Icon"), IconTexture);
                }
                EventData.SetString(TEXT("DisplayName"), ItemData.DisplayName.ToString());
            }
        }

        EventBus->Publish(TAG_Equipment_Event_QuickSlot_Assigned, EventData);
    }
    else
    {
        // Slot cleared
        FSuspenseCoreEventData EventData;
        EventData.SetInt(TEXT("SlotIndex"), SlotIndex);
        EventBus->Publish(TAG_Equipment_Event_QuickSlot_Cleared, EventData);
    }
}

#undef QUICKSLOT_LOG
