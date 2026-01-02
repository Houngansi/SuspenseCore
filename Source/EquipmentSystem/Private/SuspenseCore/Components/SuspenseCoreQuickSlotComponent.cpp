// SuspenseCoreQuickSlotComponent.cpp
// QuickSlot system for fast magazine/item access
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Components/SuspenseCoreQuickSlotComponent.h"
#include "SuspenseCore/Components/SuspenseCoreMagazineComponent.h"
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

    // Initialize category tags
    MagazineCategoryTag = FGameplayTag::RequestGameplayTag(FName("Item.Category.Magazine"), false);
    ConsumableCategoryTag = FGameplayTag::RequestGameplayTag(FName("Item.Category.Consumable"), false);
    GrenadeCategoryTag = FGameplayTag::RequestGameplayTag(FName("Item.Category.Grenade"), false);
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

void USuspenseCoreQuickSlotComponent::SetInventoryComponent(USuspenseCoreInventoryComponent* InInventoryComponent)
{
    if (InInventoryComponent)
    {
        InventoryComponent = InInventoryComponent;
        QUICKSLOT_LOG(Log, TEXT("InventoryComponent set for item validation"));
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

void USuspenseCoreQuickSlotComponent::ClearSlot(int32 SlotIndex)
{
    if (!IsValidSlotIndex(SlotIndex))
    {
        return;
    }

    // On client, send to server
    if (!GetOwner()->HasAuthority())
    {
        Server_ClearSlot(SlotIndex);
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
        ClearSlot(i);
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

bool USuspenseCoreQuickSlotComponent::UseQuickSlot(int32 SlotIndex)
{
    if (!IsValidSlotIndex(SlotIndex))
    {
        QUICKSLOT_LOG(Warning, TEXT("UseQuickSlot: Invalid slot index %d"), SlotIndex);
        return false;
    }

    if (!IsSlotReady(SlotIndex))
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

    // Determine item type and execute appropriate action
    bool bSuccess = false;

    if (StoredMagazines[SlotIndex].IsValid())
    {
        // It's a magazine - do quick reload
        bSuccess = ExecuteMagazineSwap(SlotIndex, false);
    }
    else
    {
        // Check item category
        FName ItemID = QuickSlots[SlotIndex].AssignedItemID;
        if (IsItemMagazine(ItemID))
        {
            bSuccess = ExecuteMagazineSwap(SlotIndex, false);
        }
        else
        {
            // TODO: Check for consumable or grenade
            // For now, treat as consumable
            bSuccess = ExecuteConsumableUse(SlotIndex);
        }
    }

    OnQuickSlotUsed.Broadcast(SlotIndex, bSuccess);

    if (bSuccess)
    {
        QUICKSLOT_LOG(Log, TEXT("Used QuickSlot %d successfully"), SlotIndex);
    }

    return bSuccess;
}

bool USuspenseCoreQuickSlotComponent::QuickSwapMagazine(int32 SlotIndex, bool bEmergencyDrop)
{
    if (!IsValidSlotIndex(SlotIndex))
    {
        QUICKSLOT_LOG(Warning, TEXT("QuickSwapMagazine: Invalid slot index %d"), SlotIndex);
        return false;
    }

    if (!IsSlotReady(SlotIndex))
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

void USuspenseCoreQuickSlotComponent::StartSlotCooldown(int32 SlotIndex, float CooldownDuration)
{
    if (!IsValidSlotIndex(SlotIndex))
    {
        return;
    }

    QuickSlots[SlotIndex].CooldownRemaining = FMath::Max(0.0f, CooldownDuration);
    OnQuickSlotCooldownChanged.Broadcast(SlotIndex, CooldownDuration);
}

//==================================================================
// Queries
//==================================================================

FSuspenseCoreQuickSlot USuspenseCoreQuickSlotComponent::GetQuickSlot(int32 SlotIndex) const
{
    if (!IsValidSlotIndex(SlotIndex))
    {
        return FSuspenseCoreQuickSlot();
    }

    return QuickSlots[SlotIndex];
}

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

bool USuspenseCoreQuickSlotComponent::GetFirstMagazineSlotIndex(int32& OutSlotIndex) const
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

bool USuspenseCoreQuickSlotComponent::HasItemInSlot(int32 SlotIndex) const
{
    if (!IsValidSlotIndex(SlotIndex))
    {
        return false;
    }

    return QuickSlots[SlotIndex].HasItem();
}

bool USuspenseCoreQuickSlotComponent::IsSlotReady(int32 SlotIndex) const
{
    if (!IsValidSlotIndex(SlotIndex))
    {
        return false;
    }

    return QuickSlots[SlotIndex].IsReady();
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

bool USuspenseCoreQuickSlotComponent::GetMagazineFromSlot(int32 SlotIndex, FSuspenseCoreMagazineInstance& OutMagazine) const
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

bool USuspenseCoreQuickSlotComponent::StoreEjectedMagazine(const FSuspenseCoreMagazineInstance& EjectedMagazine, int32& OutSlotIndex)
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
    ClearSlot(SlotIndex);
}

bool USuspenseCoreQuickSlotComponent::Server_ClearSlot_Validate(int32 SlotIndex)
{
    return IsValidSlotIndex(SlotIndex);
}

void USuspenseCoreQuickSlotComponent::Server_UseQuickSlot_Implementation(int32 SlotIndex)
{
    UseQuickSlot(SlotIndex);
}

bool USuspenseCoreQuickSlotComponent::Server_UseQuickSlot_Validate(int32 SlotIndex)
{
    return IsValidSlotIndex(SlotIndex);
}

void USuspenseCoreQuickSlotComponent::Server_QuickSwapMagazine_Implementation(int32 SlotIndex, bool bEmergencyDrop)
{
    QuickSwapMagazine(SlotIndex, bEmergencyDrop);
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
    if (!MagComp)
    {
        QUICKSLOT_LOG(Warning, TEXT("ExecuteMagazineSwap: No MagazineComponent available"));
        return false;
    }

    FSuspenseCoreMagazineInstance NewMagazine;
    if (!GetMagazineFromSlot(SlotIndex, NewMagazine))
    {
        QUICKSLOT_LOG(Warning, TEXT("ExecuteMagazineSwap: No magazine in slot %d"), SlotIndex);
        return false;
    }

    // Use MagazineComponent's swap function
    bool bSuccess = MagComp->SwapMagazineFromQuickSlot(SlotIndex, bEmergencyDrop);

    if (bSuccess)
    {
        // Clear the slot (magazine is now in weapon)
        ClearSlot(SlotIndex);
    }

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

void USuspenseCoreQuickSlotComponent::NotifySlotChanged(int32 SlotIndex, const FGuid& OldItemID, const FGuid& NewItemID)
{
    OnQuickSlotChanged.Broadcast(SlotIndex, OldItemID, NewItemID);

    bool bHasItem = NewItemID.IsValid();
    OnQuickSlotAvailabilityChanged.Broadcast(SlotIndex, bHasItem);
}

//==================================================================
// ISuspenseCoreQuickSlotProvider Interface Implementation
//==================================================================

FSuspenseCoreQuickSlot USuspenseCoreQuickSlotComponent::GetQuickSlot_Implementation(int32 SlotIndex) const
{
    return GetQuickSlot(SlotIndex);
}

bool USuspenseCoreQuickSlotComponent::IsSlotReady_Implementation(int32 SlotIndex) const
{
    return IsSlotReady(SlotIndex);
}

bool USuspenseCoreQuickSlotComponent::HasItemInSlot_Implementation(int32 SlotIndex) const
{
    return HasItemInSlot(SlotIndex);
}

bool USuspenseCoreQuickSlotComponent::UseQuickSlot_Implementation(int32 SlotIndex)
{
    return UseQuickSlot(SlotIndex);
}

bool USuspenseCoreQuickSlotComponent::QuickSwapMagazine_Implementation(int32 SlotIndex, bool bEmergencyDrop)
{
    return QuickSwapMagazine(SlotIndex, bEmergencyDrop);
}

bool USuspenseCoreQuickSlotComponent::GetMagazineFromSlot_Implementation(int32 SlotIndex, FSuspenseCoreMagazineInstance& OutMagazine) const
{
    return GetMagazineFromSlot(SlotIndex, OutMagazine);
}

bool USuspenseCoreQuickSlotComponent::GetFirstMagazineSlotIndex_Implementation(int32& OutSlotIndex) const
{
    return GetFirstMagazineSlotIndex(OutSlotIndex);
}

bool USuspenseCoreQuickSlotComponent::StoreEjectedMagazine_Implementation(const FSuspenseCoreMagazineInstance& EjectedMagazine, int32& OutSlotIndex)
{
    return StoreEjectedMagazine(EjectedMagazine, OutSlotIndex);
}

void USuspenseCoreQuickSlotComponent::ClearSlot_Implementation(int32 SlotIndex)
{
    ClearSlot(SlotIndex);
}

#undef QUICKSLOT_LOG
