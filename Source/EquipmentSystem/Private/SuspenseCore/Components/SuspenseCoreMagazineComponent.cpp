// SuspenseCoreMagazineComponent.cpp
// Tarkov-style magazine management component
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Components/SuspenseCoreMagazineComponent.h"
#include "SuspenseCore/Components/SuspenseCoreQuickSlotComponent.h"
#include "SuspenseCore/Components/Core/SuspenseCoreEquipmentDataStore.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeapon.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreQuickSlotProvider.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "SuspenseCore/Attributes/SuspenseCoreWeaponAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogMagazineComponent, Log, All);

// Stats for profiling hot paths
DECLARE_STATS_GROUP(TEXT("SuspenseCoreMagazine"), STATGROUP_SuspenseCoreMagazine, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Magazine Fire"), STAT_MagazineFire, STATGROUP_SuspenseCoreMagazine);
DECLARE_CYCLE_STAT(TEXT("Magazine ChamberRound"), STAT_MagazineChamberRound, STATGROUP_SuspenseCoreMagazine);
DECLARE_CYCLE_STAT(TEXT("Magazine InsertMagazine"), STAT_MagazineInsert, STATGROUP_SuspenseCoreMagazine);

USuspenseCoreMagazineComponent::USuspenseCoreMagazineComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    SetIsReplicatedByDefault(true);
}

void USuspenseCoreMagazineComponent::BeginPlay()
{
    Super::BeginPlay();
}

void USuspenseCoreMagazineComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Handle reload timer on server
    if (bIsReloading && GetOwner() && GetOwner()->HasAuthority())
    {
        float CurrentTime = GetWorld()->GetTimeSeconds();
        if (CurrentTime >= ReloadStartTime + ReloadDuration)
        {
            CompleteReload();
        }
    }

    // Cleanup expired predictions on client
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        CleanupExpiredPredictions();
    }
}

void USuspenseCoreMagazineComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(USuspenseCoreMagazineComponent, WeaponAmmoState);
    DOREPLIFETIME(USuspenseCoreMagazineComponent, bIsReloading);
    DOREPLIFETIME(USuspenseCoreMagazineComponent, CurrentReloadType);
    DOREPLIFETIME(USuspenseCoreMagazineComponent, ReloadStartTime);
    DOREPLIFETIME(USuspenseCoreMagazineComponent, ReloadDuration);
    DOREPLIFETIME(USuspenseCoreMagazineComponent, PendingMagazine);
}

void USuspenseCoreMagazineComponent::Cleanup()
{
    WeaponAmmoState.Clear();
    bIsReloading = false;
    CurrentReloadType = ESuspenseCoreReloadType::None;
    CachedWeaponInterface = nullptr;
    bMagazineDataCached = false;

    // Clear prediction state
    CurrentPrediction.Invalidate();

    // Clear cached AttributeSet reference
    CachedWeaponAttributeSet.Reset();
    bAttributeSetCacheAttempted = false;

    SetComponentTickEnabled(false);

    Super::Cleanup();
}

//================================================
// Initialization
//================================================

bool USuspenseCoreMagazineComponent::InitializeFromWeapon(
    TScriptInterface<ISuspenseCoreWeapon> WeaponInterface,
    FName InitialMagazineID,
    FName InitialAmmoID,
    int32 InitialRounds)
{
    if (!WeaponInterface)
    {
        UE_LOG(LogMagazineComponent, Warning, TEXT("InitializeFromWeapon: Invalid weapon interface"));
        return false;
    }

    CachedWeaponInterface = WeaponInterface;

    // Cache weapon data
    FSuspenseCoreUnifiedItemData WeaponData;
    if (ISuspenseCoreWeapon::Execute_GetWeaponItemData(WeaponInterface.GetObject(), WeaponData))
    {
        CachedWeaponCaliber = WeaponData.AmmoType;
        CachedWeaponType = WeaponData.WeaponArchetype;
    }

    // Create and insert initial magazine if specified
    if (!InitialMagazineID.IsNone())
    {
        USuspenseCoreDataManager* DataManager = GetDataManager();
        if (DataManager)
        {
            FSuspenseCoreMagazineInstance InitialMag;
            if (DataManager->CreateMagazineInstance(InitialMagazineID, InitialRounds, InitialAmmoID, InitialMag))
            {
                InsertMagazineInternal(InitialMag);

                // Auto-chamber if we have ammo
                if (!WeaponAmmoState.IsMagazineEmpty())
                {
                    ChamberRoundInternal();
                }
            }
        }
    }

    UE_LOG(LogMagazineComponent, Log, TEXT("Initialized magazine component: Mag=%s, Rounds=%d/%d, Chambered=%s"),
        *WeaponAmmoState.InsertedMagazine.MagazineID.ToString(),
        WeaponAmmoState.InsertedMagazine.CurrentRoundCount,
        WeaponAmmoState.InsertedMagazine.MaxCapacity,
        WeaponAmmoState.ChamberedRound.IsChambered() ? TEXT("Yes") : TEXT("No"));

    return true;
}

//================================================
// Magazine Operations (Internal)
//================================================

bool USuspenseCoreMagazineComponent::InsertMagazineInternal(const FSuspenseCoreMagazineInstance& Magazine)
{
    SCOPE_CYCLE_COUNTER(STAT_MagazineInsert);

    if (!Magazine.IsValid())
    {
        UE_LOG(LogMagazineComponent, Warning, TEXT("InsertMagazine: Invalid magazine"));
        return false;
    }

    if (WeaponAmmoState.bHasMagazine)
    {
        UE_LOG(LogMagazineComponent, Warning, TEXT("InsertMagazine: Magazine already inserted"));
        return false;
    }

    // Server authority check
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        ServerInsertMagazine(Magazine);
        return true;
    }

    FSuspenseCoreMagazineInstance OldMag; // Empty
    FSuspenseCoreMagazineInstance NewMag = Magazine;

    if (WeaponAmmoState.InsertMagazine(NewMag))
    {
        // Cache magazine data
        USuspenseCoreDataManager* DataManager = GetDataManager();
        if (DataManager && DataManager->GetMagazineData(Magazine.MagazineID, CachedMagazineData))
        {
            bMagazineDataCached = true;
        }

        ApplyMagazineModifiers();
        BroadcastStateChanged();
        OnMagazineChanged.Broadcast(OldMag, NewMag);

        // Publish EventBus event for UI widgets
        if (USuspenseCoreEventManager* EventManager = USuspenseCoreEventManager::Get(this))
        {
            if (USuspenseCoreEventBus* EventBus = EventManager->GetEventBus())
            {
                FSuspenseCoreEventData EventData;
                EventData.SetInt(TEXT("CurrentRounds"), WeaponAmmoState.InsertedMagazine.CurrentRoundCount);
                EventData.SetInt(TEXT("MaxCapacity"), WeaponAmmoState.InsertedMagazine.MaxCapacity);
                EventData.SetString(TEXT("LoadedAmmoType"), WeaponAmmoState.InsertedMagazine.LoadedAmmoID.ToString());
                EventData.SetString(TEXT("MagazineID"), WeaponAmmoState.InsertedMagazine.MagazineID.ToString());
                EventData.SetBool(TEXT("HasChamberedRound"), WeaponAmmoState.ChamberedRound.IsChambered());
                EventBus->Publish(SuspenseCoreEquipmentTags::Magazine::TAG_Equipment_Event_Magazine_Inserted, EventData);
            }
        }

        UE_LOG(LogMagazineComponent, Log, TEXT("Inserted magazine: %s (%d/%d rounds)"),
            *Magazine.MagazineID.ToString(),
            Magazine.CurrentRoundCount,
            Magazine.MaxCapacity);

        return true;
    }

    return false;
}

FSuspenseCoreMagazineInstance USuspenseCoreMagazineComponent::EjectMagazineInternal(bool bDropToGround)
{
    if (!WeaponAmmoState.bHasMagazine)
    {
        return FSuspenseCoreMagazineInstance();
    }

    // Server authority check
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        ServerEjectMagazine(bDropToGround);
        return WeaponAmmoState.InsertedMagazine; // Return copy, server will update
    }

    FSuspenseCoreMagazineInstance OldMag = WeaponAmmoState.InsertedMagazine;
    FSuspenseCoreMagazineInstance EjectedMag = WeaponAmmoState.EjectMagazine();

    RemoveMagazineModifiers();
    bMagazineDataCached = false;

    if (bDropToGround)
    {
        // TODO: Spawn pickup actor with magazine data
        UE_LOG(LogMagazineComponent, Log, TEXT("Dropped magazine: %s (%d rounds)"),
            *EjectedMag.MagazineID.ToString(),
            EjectedMag.CurrentRoundCount);
    }
    else
    {
        // TODO: Return to inventory/QuickSlot
        UE_LOG(LogMagazineComponent, Log, TEXT("Ejected magazine: %s (%d rounds)"),
            *EjectedMag.MagazineID.ToString(),
            EjectedMag.CurrentRoundCount);
    }

    BroadcastStateChanged();
    OnMagazineChanged.Broadcast(OldMag, FSuspenseCoreMagazineInstance());

    // Publish EventBus event for UI widgets
    if (USuspenseCoreEventManager* EventManager = USuspenseCoreEventManager::Get(this))
    {
        if (USuspenseCoreEventBus* EventBus = EventManager->GetEventBus())
        {
            FSuspenseCoreEventData EventData;
            EventData.SetString(TEXT("EjectedMagazineID"), EjectedMag.MagazineID.ToString());
            EventData.SetInt(TEXT("EjectedRounds"), EjectedMag.CurrentRoundCount);
            EventData.SetBool(TEXT("DroppedToGround"), bDropToGround);
            EventBus->Publish(SuspenseCoreEquipmentTags::Magazine::TAG_Equipment_Event_Magazine_Ejected, EventData);
        }
    }

    return EjectedMag;
}

bool USuspenseCoreMagazineComponent::SwapMagazineFromQuickSlot(int32 QuickSlotIndex, bool bEmergencyDrop)
{
    // Validate slot index (use constant from SuspenseCoreQuickSlotComponent.h)
    if (QuickSlotIndex < 0 || QuickSlotIndex >= static_cast<int32>(SUSPENSECORE_QUICKSLOT_COUNT))
    {
        UE_LOG(LogMagazineComponent, Warning, TEXT("SwapMagazineFromQuickSlot: Invalid slot index %d"), QuickSlotIndex);
        return false;
    }

    // Get owner (should be weapon actor with character owner)
    AActor* WeaponOwner = GetOwner();
    if (!WeaponOwner)
    {
        UE_LOG(LogMagazineComponent, Warning, TEXT("SwapMagazineFromQuickSlot: No weapon owner"));
        return false;
    }

    // Get character owner (the pawn that owns this weapon)
    AActor* CharacterOwner = WeaponOwner->GetOwner();
    if (!CharacterOwner)
    {
        // Try Instigator as fallback
        CharacterOwner = WeaponOwner->GetInstigator();
    }
    if (!CharacterOwner)
    {
        UE_LOG(LogMagazineComponent, Warning, TEXT("SwapMagazineFromQuickSlot: Cannot find character owner"));
        return false;
    }

    // Get QuickSlotComponent from character via interface
    TScriptInterface<ISuspenseCoreQuickSlotProvider> QuickSlotProvider;
    if (UActorComponent* QSComp = CharacterOwner->FindComponentByClass<USuspenseCoreQuickSlotComponent>())
    {
        QuickSlotProvider.SetObject(QSComp);
        QuickSlotProvider.SetInterface(Cast<ISuspenseCoreQuickSlotProvider>(QSComp));
    }

    if (!QuickSlotProvider)
    {
        UE_LOG(LogMagazineComponent, Warning, TEXT("SwapMagazineFromQuickSlot: Character has no QuickSlotComponent"));
        return false;
    }

    // Check if slot is ready and has a magazine
    if (!ISuspenseCoreQuickSlotProvider::Execute_IsSlotReady(QuickSlotProvider.GetObject(), QuickSlotIndex))
    {
        UE_LOG(LogMagazineComponent, Verbose, TEXT("SwapMagazineFromQuickSlot: Slot %d not ready"), QuickSlotIndex);
        return false;
    }

    // Get magazine from slot
    FSuspenseCoreMagazineInstance NewMagazine;
    if (!ISuspenseCoreQuickSlotProvider::Execute_GetMagazineFromSlot(QuickSlotProvider.GetObject(), QuickSlotIndex, NewMagazine))
    {
        UE_LOG(LogMagazineComponent, Warning, TEXT("SwapMagazineFromQuickSlot: No magazine in slot %d"), QuickSlotIndex);
        return false;
    }

    // Verify magazine is compatible with weapon caliber
    if (CachedWeaponCaliber.IsValid())
    {
        USuspenseCoreDataManager* DataManager = GetDataManager();
        if (DataManager)
        {
            FSuspenseCoreMagazineData MagData;
            if (DataManager->GetMagazineData(NewMagazine.MagazineID, MagData))
            {
                if (!MagData.IsCompatibleWithCaliber(CachedWeaponCaliber))
                {
                    UE_LOG(LogMagazineComponent, Warning, TEXT("SwapMagazineFromQuickSlot: Magazine %s not compatible with weapon caliber %s"),
                        *NewMagazine.MagazineID.ToString(), *CachedWeaponCaliber.ToString());
                    return false;
                }
            }
        }
    }

    // Server authority check - redirect to server RPC if client
    if (WeaponOwner->HasAuthority() == false)
    {
        ServerSwapMagazineFromQuickSlot(QuickSlotIndex, bEmergencyDrop);
        return true; // Assume success, server will replicate actual state
    }

    // === Server-side execution ===

    // Store old magazine for returning to QuickSlot
    FSuspenseCoreMagazineInstance OldMagazine;
    bool bHadMagazine = WeaponAmmoState.bHasMagazine;
    if (bHadMagazine)
    {
        OldMagazine = WeaponAmmoState.InsertedMagazine;
    }

    // Eject current magazine
    if (bHadMagazine)
    {
        EjectMagazineInternal(bEmergencyDrop);
    }

    // Insert new magazine from QuickSlot
    bool bInserted = InsertMagazineInternal(NewMagazine);
    if (!bInserted)
    {
        UE_LOG(LogMagazineComponent, Error, TEXT("SwapMagazineFromQuickSlot: Failed to insert magazine"));
        // Try to restore old magazine
        if (bHadMagazine)
        {
            InsertMagazineInternal(OldMagazine);
        }
        return false;
    }

    // Clear the QuickSlot (internal QuickSlotComponent data)
    ISuspenseCoreQuickSlotProvider::Execute_ClearSlot(QuickSlotProvider.GetObject(), QuickSlotIndex);

    // CRITICAL: Also clear the EquipmentDataStore to keep it in sync
    // QuickSlots are at equipment indices 13-16 (QuickSlotIndex 0-3)
    // This fixes the "SlotOccupied" error when trying to re-equip to the same slot
    const int32 EquipmentSlotIndex = QuickSlotIndex + 13; // QuickSlot mapping: 0->13, 1->14, 2->15, 3->16
    if (USuspenseCoreEquipmentDataStore* DataStore = CharacterOwner->FindComponentByClass<USuspenseCoreEquipmentDataStore>())
    {
        DataStore->ClearSlot(EquipmentSlotIndex, true);
        UE_LOG(LogMagazineComponent, Log, TEXT("SwapMagazineFromQuickSlot: Cleared DataStore slot %d"), EquipmentSlotIndex);
    }

    // Store ejected magazine back to QuickSlot (if not emergency drop)
    if (bHadMagazine && !bEmergencyDrop && OldMagazine.IsValid())
    {
        // Use the same slot we took the new magazine from
        USuspenseCoreQuickSlotComponent* QuickSlotComp = Cast<USuspenseCoreQuickSlotComponent>(QuickSlotProvider.GetObject());
        if (QuickSlotComp)
        {
            QuickSlotComp->AssignMagazineToSlot(QuickSlotIndex, OldMagazine);

            // CRITICAL: Also update EquipmentDataStore with the ejected magazine
            // This keeps DataStore in sync with QuickSlotComponent
            if (USuspenseCoreEquipmentDataStore* DataStore = CharacterOwner->FindComponentByClass<USuspenseCoreEquipmentDataStore>())
            {
                FSuspenseCoreInventoryItemInstance MagItem;
                MagItem.InstanceID = OldMagazine.InstanceGuid;
                MagItem.ItemID = OldMagazine.MagazineID;
                MagItem.Quantity = 1;
                MagItem.MagazineData.MagazineID = OldMagazine.MagazineID;
                MagItem.MagazineData.CurrentRoundCount = OldMagazine.CurrentRoundCount;
                MagItem.MagazineData.MaxCapacity = OldMagazine.MaxCapacity;
                MagItem.MagazineData.LoadedAmmoID = OldMagazine.LoadedAmmoID;
                MagItem.MagazineData.CurrentDurability = OldMagazine.Durability;

                DataStore->SetSlotItem(EquipmentSlotIndex, MagItem, true);
                UE_LOG(LogMagazineComponent, Log, TEXT("SwapMagazineFromQuickSlot: Stored ejected magazine to DataStore slot %d (%d rounds)"),
                    EquipmentSlotIndex, OldMagazine.CurrentRoundCount);
            }
        }
    }

    // Chamber round if needed (empty reload)
    if (!WeaponAmmoState.ChamberedRound.IsChambered() && !WeaponAmmoState.IsMagazineEmpty())
    {
        ChamberRoundInternal();
    }

    // Publish EventBus event (Magazine_Swapped with full data for UI)
    USuspenseCoreEventManager* EventManager = USuspenseCoreEventManager::Get(this);
    if (EventManager)
    {
        if (USuspenseCoreEventBus* EventBus = EventManager->GetEventBus())
        {
            FSuspenseCoreEventData EventData;
            EventData.SetInt(TEXT("QuickSlotIndex"), QuickSlotIndex);
            EventData.SetString(TEXT("NewMagazineID"), NewMagazine.MagazineID.ToString());
            EventData.SetInt(TEXT("NewMagazineRounds"), NewMagazine.CurrentRoundCount);
            // Include fields for UI widgets compatibility
            EventData.SetInt(TEXT("CurrentRounds"), WeaponAmmoState.InsertedMagazine.CurrentRoundCount);
            EventData.SetInt(TEXT("MaxCapacity"), WeaponAmmoState.InsertedMagazine.MaxCapacity);
            EventData.SetBool(TEXT("HasChamberedRound"), WeaponAmmoState.ChamberedRound.IsChambered());
            EventData.SetBool(TEXT("EmergencyDrop"), bEmergencyDrop);
            if (bHadMagazine)
            {
                EventData.SetString(TEXT("OldMagazineID"), OldMagazine.MagazineID.ToString());
                EventData.SetInt(TEXT("OldMagazineRounds"), OldMagazine.CurrentRoundCount);
            }
            EventBus->Publish(SuspenseCoreEquipmentTags::Magazine::TAG_Equipment_Event_Magazine_Swapped, EventData);
        }
    }

    UE_LOG(LogMagazineComponent, Log, TEXT("SwapMagazineFromQuickSlot: Swapped from slot %d - New: %s (%d/%d), Old: %s (%d)"),
        QuickSlotIndex,
        *NewMagazine.MagazineID.ToString(),
        WeaponAmmoState.InsertedMagazine.CurrentRoundCount,
        WeaponAmmoState.InsertedMagazine.MaxCapacity,
        bHadMagazine ? *OldMagazine.MagazineID.ToString() : TEXT("None"),
        bHadMagazine ? OldMagazine.CurrentRoundCount : 0);

    return true;
}

//================================================
// Chamber Operations
//================================================

bool USuspenseCoreMagazineComponent::ChamberRoundInternal()
{
    SCOPE_CYCLE_COUNTER(STAT_MagazineChamberRound);

    if (WeaponAmmoState.ChamberedRound.IsChambered())
    {
        return false; // Already chambered
    }

    bool bPrevChambered = WeaponAmmoState.ChamberedRound.IsChambered();

    if (WeaponAmmoState.ChamberFromMagazine())
    {
        BroadcastStateChanged();

        if (bPrevChambered != WeaponAmmoState.ChamberedRound.IsChambered())
        {
            OnChamberStateChanged.Broadcast(true);
        }

        UE_LOG(LogMagazineComponent, Verbose, TEXT("Chambered round: %s"),
            *WeaponAmmoState.ChamberedRound.AmmoID.ToString());

        return true;
    }

    return false;
}

FSuspenseCoreChamberedRound USuspenseCoreMagazineComponent::EjectChamberedRoundInternal()
{
    FSuspenseCoreChamberedRound Ejected = WeaponAmmoState.EjectChamberedRound();

    if (Ejected.IsChambered())
    {
        BroadcastStateChanged();
        OnChamberStateChanged.Broadcast(false);

        UE_LOG(LogMagazineComponent, Verbose, TEXT("Ejected chambered round: %s"),
            *Ejected.AmmoID.ToString());
    }

    return Ejected;
}

FName USuspenseCoreMagazineComponent::Fire(bool bAutoChamber)
{
    SCOPE_CYCLE_COUNTER(STAT_MagazineFire);

    if (!WeaponAmmoState.IsReadyToFire())
    {
        return NAME_None;
    }

    // Server authority check
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        ServerFire(bAutoChamber);
        return WeaponAmmoState.ChamberedRound.AmmoID; // Return current, server will update
    }

    FName FiredAmmoID = WeaponAmmoState.Fire(bAutoChamber);

    if (!FiredAmmoID.IsNone())
    {
        BroadcastStateChanged();

        // Check if we need to notify chamber state change
        if (!bAutoChamber || !WeaponAmmoState.ChamberedRound.IsChambered())
        {
            OnChamberStateChanged.Broadcast(WeaponAmmoState.ChamberedRound.IsChambered());
        }

        UE_LOG(LogMagazineComponent, Verbose, TEXT("Fired: %s, AutoChambered=%s"),
            *FiredAmmoID.ToString(),
            (bAutoChamber && WeaponAmmoState.ChamberedRound.IsChambered()) ? TEXT("Yes") : TEXT("No"));
    }

    return FiredAmmoID;
}

//================================================
// Reload Operations
//================================================

bool USuspenseCoreMagazineComponent::StartReload(const FSuspenseCoreReloadRequest& Request)
{
    if (!Request.IsValid())
    {
        return false;
    }

    if (bIsReloading)
    {
        UE_LOG(LogMagazineComponent, Warning, TEXT("StartReload: Already reloading"));
        return false;
    }

    // Server authority check
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        ServerStartReload(Request);
        return true;
    }

    bIsReloading = true;
    CurrentReloadType = Request.ReloadType;
    ReloadDuration = Request.ReloadDuration;
    ReloadStartTime = GetWorld()->GetTimeSeconds();
    PendingMagazine = Request.NewMagazine;

    // Enable tick for reload timer
    SetComponentTickEnabled(true);

    OnReloadStateChanged.Broadcast(true, CurrentReloadType);

    UE_LOG(LogMagazineComponent, Log, TEXT("Started reload: Type=%d, Duration=%.2f"),
        static_cast<int32>(CurrentReloadType), ReloadDuration);

    return true;
}

ESuspenseCoreReloadType USuspenseCoreMagazineComponent::DetermineReloadTypeForMagazine(const FSuspenseCoreMagazineInstance& AvailableMagazine) const
{
    // No magazine available for reload
    if (!AvailableMagazine.IsValid() || AvailableMagazine.IsEmpty())
    {
        // Can only rack if we have a magazine with ammo but no chambered round
        if (WeaponAmmoState.bHasMagazine && !WeaponAmmoState.IsMagazineEmpty() && !WeaponAmmoState.ChamberedRound.IsChambered())
        {
            return ESuspenseCoreReloadType::ChamberOnly;
        }
        return ESuspenseCoreReloadType::None;
    }

    // Has chambered round - tactical reload (faster, doesn't need to rack)
    if (WeaponAmmoState.ChamberedRound.IsChambered())
    {
        return ESuspenseCoreReloadType::Tactical;
    }

    // No chambered round - empty reload (need to rack after inserting mag)
    return ESuspenseCoreReloadType::Empty;
}

float USuspenseCoreMagazineComponent::CalculateReloadDurationWithData(ESuspenseCoreReloadType ReloadType, const FSuspenseCoreMagazineData& MagazineData) const
{
    // Get base times from weapon attributes (or use defaults)
    float TacticalTime = 2.1f;
    float FullTime = 2.8f;

    // Get from weapon AttributeSet if available
    if (const USuspenseCoreWeaponAttributeSet* WeaponAttributeSet = GetCachedWeaponAttributeSet())
    {
        TacticalTime = WeaponAttributeSet->GetTacticalReloadTime();
        FullTime = WeaponAttributeSet->GetFullReloadTime();
        UE_LOG(LogMagazineComponent, Verbose, TEXT("Using AttributeSet reload times: Tactical=%.2f, Full=%.2f"), TacticalTime, FullTime);
    }

    float BaseDuration = 0.0f;

    switch (ReloadType)
    {
        case ESuspenseCoreReloadType::Tactical:
            BaseDuration = TacticalTime;
            break;

        case ESuspenseCoreReloadType::Empty:
            BaseDuration = FullTime;
            break;

        case ESuspenseCoreReloadType::Emergency:
            BaseDuration = TacticalTime * 0.8f; // 20% faster but drops mag
            break;

        case ESuspenseCoreReloadType::ChamberOnly:
            BaseDuration = 0.5f; // Just rack the bolt
            break;

        default:
            return 0.0f;
    }

    // Apply magazine modifier
    if (MagazineData.IsValid())
    {
        BaseDuration *= MagazineData.ReloadTimeModifier;
    }

    return BaseDuration;
}

void USuspenseCoreMagazineComponent::CompleteReload()
{
    if (!bIsReloading)
    {
        return;
    }

    // Server authority check
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        ServerCompleteReload();
        return;
    }

    ProcessReloadCompletion();

    bIsReloading = false;
    ESuspenseCoreReloadType CompletedType = CurrentReloadType;
    CurrentReloadType = ESuspenseCoreReloadType::None;
    SetComponentTickEnabled(false);

    BroadcastStateChanged();
    OnReloadStateChanged.Broadcast(false, CompletedType);

    UE_LOG(LogMagazineComponent, Log, TEXT("Completed reload: Type=%d"), static_cast<int32>(CompletedType));
}

void USuspenseCoreMagazineComponent::CancelReload()
{
    if (!bIsReloading)
    {
        return;
    }

    // Server authority check
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        ServerCancelReload();
        return;
    }

    ESuspenseCoreReloadType CancelledType = CurrentReloadType;

    bIsReloading = false;
    CurrentReloadType = ESuspenseCoreReloadType::None;
    PendingMagazine = FSuspenseCoreMagazineInstance();
    SetComponentTickEnabled(false);

    OnReloadStateChanged.Broadcast(false, CancelledType);

    UE_LOG(LogMagazineComponent, Log, TEXT("Cancelled reload: Type=%d"), static_cast<int32>(CancelledType));
}

bool USuspenseCoreMagazineComponent::CanReload(const FSuspenseCoreMagazineInstance& NewMagazine) const
{
    if (bIsReloading)
    {
        return false;
    }

    // If checking with specific magazine, verify compatibility
    if (NewMagazine.IsValid())
    {
        // TODO: Check caliber compatibility
        return true;
    }

    // General check - can reload if not full or no chambered round
    if (!WeaponAmmoState.bHasMagazine)
    {
        return true; // Can insert new magazine
    }

    if (!WeaponAmmoState.ChamberedRound.IsChambered() && !WeaponAmmoState.IsMagazineEmpty())
    {
        return true; // Can chamber a round
    }

    if (!WeaponAmmoState.InsertedMagazine.IsFull())
    {
        return true; // Could swap for fuller magazine
    }

    return false;
}

//================================================
// State Queries
//================================================

float USuspenseCoreMagazineComponent::GetReloadProgress() const
{
    if (!bIsReloading || ReloadDuration <= 0.0f)
    {
        return 0.0f;
    }

    float CurrentTime = GetWorld()->GetTimeSeconds();
    float Elapsed = CurrentTime - ReloadStartTime;
    return FMath::Clamp(Elapsed / ReloadDuration, 0.0f, 1.0f);
}

int32 USuspenseCoreMagazineComponent::GetMagazineRoundCount() const
{
    return WeaponAmmoState.bHasMagazine ? WeaponAmmoState.InsertedMagazine.CurrentRoundCount : 0;
}

int32 USuspenseCoreMagazineComponent::GetMagazineCapacity() const
{
    return WeaponAmmoState.bHasMagazine ? WeaponAmmoState.InsertedMagazine.MaxCapacity : 0;
}

FName USuspenseCoreMagazineComponent::GetLoadedAmmoType() const
{
    if (WeaponAmmoState.ChamberedRound.IsChambered())
    {
        return WeaponAmmoState.ChamberedRound.AmmoID;
    }

    if (WeaponAmmoState.bHasMagazine && !WeaponAmmoState.InsertedMagazine.IsEmpty())
    {
        return WeaponAmmoState.InsertedMagazine.LoadedAmmoID;
    }

    return NAME_None;
}

//================================================
// Save/Load State
//================================================

void USuspenseCoreMagazineComponent::RestoreState(const FSuspenseCoreWeaponAmmoState& SavedState)
{
    WeaponAmmoState = SavedState;
    bMagazineDataCached = false;

    // Re-cache magazine data if we have a magazine
    if (WeaponAmmoState.bHasMagazine)
    {
        USuspenseCoreDataManager* DataManager = GetDataManager();
        if (DataManager && DataManager->GetMagazineData(WeaponAmmoState.InsertedMagazine.MagazineID, CachedMagazineData))
        {
            bMagazineDataCached = true;
        }
    }

    BroadcastStateChanged();

    UE_LOG(LogMagazineComponent, Log, TEXT("Restored state: Mag=%s, Rounds=%d, Chambered=%s"),
        *WeaponAmmoState.InsertedMagazine.MagazineID.ToString(),
        WeaponAmmoState.InsertedMagazine.CurrentRoundCount,
        WeaponAmmoState.ChamberedRound.IsChambered() ? TEXT("Yes") : TEXT("No"));
}

//================================================
// ISuspenseCoreMagazineProvider Interface Implementation
//================================================

FSuspenseCoreWeaponAmmoState USuspenseCoreMagazineComponent::GetAmmoState_Implementation() const
{
    return GetWeaponAmmoState();
}

bool USuspenseCoreMagazineComponent::HasMagazine_Implementation() const
{
    return WeaponAmmoState.bHasMagazine;
}

bool USuspenseCoreMagazineComponent::IsReadyToFire_Implementation() const
{
    return WeaponAmmoState.IsReadyToFire();
}

bool USuspenseCoreMagazineComponent::IsReloading_Implementation() const
{
    return bIsReloading;
}

bool USuspenseCoreMagazineComponent::InsertMagazine_Implementation(const FSuspenseCoreMagazineInstance& Magazine)
{
    return InsertMagazineInternal(Magazine);
}

FSuspenseCoreMagazineInstance USuspenseCoreMagazineComponent::EjectMagazine_Implementation(bool bDropToGround)
{
    return EjectMagazineInternal(bDropToGround);
}

bool USuspenseCoreMagazineComponent::ChamberRound_Implementation()
{
    return ChamberRoundInternal();
}

FSuspenseCoreChamberedRound USuspenseCoreMagazineComponent::EjectChamberedRound_Implementation()
{
    return EjectChamberedRoundInternal();
}

ESuspenseCoreReloadType USuspenseCoreMagazineComponent::DetermineReloadType_Implementation() const
{
    // Determine reload type without a specific available magazine
    return DetermineReloadTypeForMagazine(FSuspenseCoreMagazineInstance());
}

float USuspenseCoreMagazineComponent::CalculateReloadDuration_Implementation(ESuspenseCoreReloadType ReloadType, const FSuspenseCoreMagazineInstance& NewMagazine) const
{
    // Get magazine data for the new magazine
    FSuspenseCoreMagazineData MagData;
    USuspenseCoreDataManager* DataManager = GetDataManager();
    if (DataManager && NewMagazine.IsValid())
    {
        DataManager->GetMagazineData(NewMagazine.MagazineID, MagData);
    }

    return CalculateReloadDurationWithData(ReloadType, MagData);
}

void USuspenseCoreMagazineComponent::NotifyReloadStateChanged_Implementation(bool bInIsReloading, ESuspenseCoreReloadType ReloadType, float Duration)
{
    // This is called by abilities to sync reload state
    if (bInIsReloading)
    {
        bIsReloading = true;
        CurrentReloadType = ReloadType;
        ReloadDuration = Duration;
        ReloadStartTime = GetWorld()->GetTimeSeconds();
        SetComponentTickEnabled(true);
    }
    else
    {
        bIsReloading = false;
        CurrentReloadType = ESuspenseCoreReloadType::None;
        SetComponentTickEnabled(false);
    }

    OnReloadStateChanged.Broadcast(bInIsReloading, ReloadType);
}

//================================================
// Internal Operations
//================================================

ISuspenseCoreWeapon* USuspenseCoreMagazineComponent::GetWeaponInterface() const
{
    return CachedWeaponInterface.GetInterface();
}

USuspenseCoreDataManager* USuspenseCoreMagazineComponent::GetDataManager() const
{
    return USuspenseCoreDataManager::Get(this);
}

void USuspenseCoreMagazineComponent::BroadcastStateChanged()
{
    OnMagazineStateChanged.Broadcast(WeaponAmmoState);
}

void USuspenseCoreMagazineComponent::ApplyMagazineModifiers()
{
    if (!bMagazineDataCached || CachedMagazineData.ErgonomicsPenalty <= 0)
    {
        return;
    }

    // TODO: Apply ergonomics penalty to weapon
    UE_LOG(LogMagazineComponent, Verbose, TEXT("Applied magazine ergonomics penalty: %d"),
        CachedMagazineData.ErgonomicsPenalty);
}

void USuspenseCoreMagazineComponent::RemoveMagazineModifiers()
{
    // TODO: Remove ergonomics penalty from weapon
}

void USuspenseCoreMagazineComponent::ProcessReloadCompletion()
{
    switch (CurrentReloadType)
    {
        case ESuspenseCoreReloadType::Tactical:
        case ESuspenseCoreReloadType::Empty:
        case ESuspenseCoreReloadType::Emergency:
        {
            // Eject current magazine if present
            if (WeaponAmmoState.bHasMagazine)
            {
                FSuspenseCoreMagazineInstance OldMag = EjectMagazineInternal(CurrentReloadType == ESuspenseCoreReloadType::Emergency);
                // TODO: Return old magazine to inventory/QuickSlot
            }

            // Insert new magazine
            if (PendingMagazine.IsValid())
            {
                InsertMagazineInternal(PendingMagazine);
            }

            // Chamber round if needed
            if (CurrentReloadType == ESuspenseCoreReloadType::Empty && !WeaponAmmoState.ChamberedRound.IsChambered())
            {
                ChamberRoundInternal();
            }
            break;
        }

        case ESuspenseCoreReloadType::ChamberOnly:
        {
            ChamberRoundInternal();
            break;
        }

        default:
            break;
    }

    PendingMagazine = FSuspenseCoreMagazineInstance();
}

//================================================
// Server RPCs
//================================================

void USuspenseCoreMagazineComponent::ServerInsertMagazine_Implementation(const FSuspenseCoreMagazineInstance& Magazine)
{
    InsertMagazineInternal(Magazine);
}

bool USuspenseCoreMagazineComponent::ServerInsertMagazine_Validate(const FSuspenseCoreMagazineInstance& Magazine)
{
    return Magazine.IsValid();
}

void USuspenseCoreMagazineComponent::ServerEjectMagazine_Implementation(bool bDropToGround)
{
    EjectMagazineInternal(bDropToGround);
}

bool USuspenseCoreMagazineComponent::ServerEjectMagazine_Validate(bool bDropToGround)
{
    return true;
}

void USuspenseCoreMagazineComponent::ServerFire_Implementation(bool bAutoChamber)
{
    Fire(bAutoChamber);
}

bool USuspenseCoreMagazineComponent::ServerFire_Validate(bool bAutoChamber)
{
    return true;
}

void USuspenseCoreMagazineComponent::ServerStartReload_Implementation(const FSuspenseCoreReloadRequest& Request)
{
    StartReload(Request);
}

bool USuspenseCoreMagazineComponent::ServerStartReload_Validate(const FSuspenseCoreReloadRequest& Request)
{
    return Request.IsValid();
}

void USuspenseCoreMagazineComponent::ServerCompleteReload_Implementation()
{
    CompleteReload();
}

bool USuspenseCoreMagazineComponent::ServerCompleteReload_Validate()
{
    return bIsReloading;
}

void USuspenseCoreMagazineComponent::ServerCancelReload_Implementation()
{
    CancelReload();
}

bool USuspenseCoreMagazineComponent::ServerCancelReload_Validate()
{
    return true;
}

void USuspenseCoreMagazineComponent::ServerSwapMagazineFromQuickSlot_Implementation(int32 QuickSlotIndex, bool bEmergencyDrop)
{
    // Call the main function which will now execute on server
    SwapMagazineFromQuickSlot(QuickSlotIndex, bEmergencyDrop);
}

bool USuspenseCoreMagazineComponent::ServerSwapMagazineFromQuickSlot_Validate(int32 QuickSlotIndex, bool bEmergencyDrop)
{
    // Basic validation - slot index must be in range
    return QuickSlotIndex >= 0 && QuickSlotIndex < SUSPENSECORE_QUICKSLOT_COUNT;
}

//================================================
// Replication
//================================================

void USuspenseCoreMagazineComponent::OnRep_WeaponAmmoState()
{
    // Update cached magazine data
    if (WeaponAmmoState.bHasMagazine)
    {
        USuspenseCoreDataManager* DataManager = GetDataManager();
        if (DataManager && DataManager->GetMagazineData(WeaponAmmoState.InsertedMagazine.MagazineID, CachedMagazineData))
        {
            bMagazineDataCached = true;
        }
    }
    else
    {
        bMagazineDataCached = false;
    }

    BroadcastStateChanged();
}

void USuspenseCoreMagazineComponent::OnRep_ReloadState()
{
    OnReloadStateChanged.Broadcast(bIsReloading, CurrentReloadType);

    // Enable/disable tick based on reload state
    SetComponentTickEnabled(bIsReloading);

    // If server confirmed reload and we had a prediction, confirm it
    if (CurrentPrediction.IsValid())
    {
        if (bIsReloading && CurrentPrediction.PredictedReloadType == CurrentReloadType)
        {
            // Server confirmed our prediction
            ConfirmPrediction(CurrentPrediction.PredictionKey);
        }
        else if (!bIsReloading && CurrentPrediction.bIsActive)
        {
            // Server rejected or reload completed - check if we need rollback
            // This is handled by ClientConfirmReloadPrediction
        }
    }
}

//================================================
// GAS Integration
//================================================

UAbilitySystemComponent* USuspenseCoreMagazineComponent::GetOwnerASC() const
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return nullptr;
    }

    // First check if owner has ASC directly
    if (UAbilitySystemComponent* ASC = Owner->FindComponentByClass<UAbilitySystemComponent>())
    {
        return ASC;
    }

    // Check owner of owner (weapon -> character)
    if (AActor* CharacterOwner = Owner->GetOwner())
    {
        return CharacterOwner->FindComponentByClass<UAbilitySystemComponent>();
    }

    // Try Instigator
    if (APawn* Instigator = Owner->GetInstigator())
    {
        return Instigator->FindComponentByClass<UAbilitySystemComponent>();
    }

    return nullptr;
}

const USuspenseCoreWeaponAttributeSet* USuspenseCoreMagazineComponent::GetCachedWeaponAttributeSet() const
{
    // Return cached if still valid
    if (CachedWeaponAttributeSet.IsValid())
    {
        return CachedWeaponAttributeSet.Get();
    }

    // Only try to cache once per component lifetime (avoid repeated lookups if not found)
    if (bAttributeSetCacheAttempted)
    {
        return nullptr;
    }

    bAttributeSetCacheAttempted = true;

    // Get ASC and find AttributeSet
    if (UAbilitySystemComponent* ASC = GetOwnerASC())
    {
        const USuspenseCoreWeaponAttributeSet* AttributeSet = ASC->GetSet<USuspenseCoreWeaponAttributeSet>();
        if (AttributeSet)
        {
            CachedWeaponAttributeSet = AttributeSet;
            UE_LOG(LogMagazineComponent, Verbose, TEXT("Cached WeaponAttributeSet from ASC"));
            return AttributeSet;
        }
    }

    UE_LOG(LogMagazineComponent, Verbose, TEXT("WeaponAttributeSet not found, using default reload times"));
    return nullptr;
}

//================================================
// Client Prediction
//================================================

int32 USuspenseCoreMagazineComponent::PredictStartReload(const FSuspenseCoreReloadRequest& Request)
{
    // Only clients predict
    if (!GetOwner() || GetOwner()->HasAuthority())
    {
        return 0;
    }

    // Can't start new prediction if one is already active
    if (CurrentPrediction.IsValid())
    {
        UE_LOG(LogMagazineComponent, Warning, TEXT("PredictStartReload: Already have active prediction"));
        return 0;
    }

    // Validate request
    if (!Request.IsValid())
    {
        return 0;
    }

    // Generate prediction key
    int32 PredictionKey = GeneratePredictionKey();

    // Store prediction data
    CurrentPrediction.PredictionKey = PredictionKey;
    CurrentPrediction.PredictedReloadType = Request.ReloadType;
    CurrentPrediction.PredictedDuration = Request.ReloadDuration;
    CurrentPrediction.PredictedMagazine = Request.NewMagazine;
    CurrentPrediction.StateBeforePrediction = WeaponAmmoState;
    CurrentPrediction.PredictionTimestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    CurrentPrediction.bIsActive = true;

    // Apply prediction locally (optimistic update)
    ApplyPredictionLocally(CurrentPrediction);

    UE_LOG(LogMagazineComponent, Log, TEXT("Started reload prediction: Key=%d, Type=%d"),
        PredictionKey, static_cast<int32>(Request.ReloadType));

    return PredictionKey;
}

void USuspenseCoreMagazineComponent::ConfirmPrediction(int32 PredictionKey)
{
    if (!CurrentPrediction.IsValid() || CurrentPrediction.PredictionKey != PredictionKey)
    {
        return;
    }

    UE_LOG(LogMagazineComponent, Log, TEXT("Confirmed reload prediction: Key=%d"), PredictionKey);

    // Clear prediction - server state is now authoritative
    CurrentPrediction.Invalidate();
}

void USuspenseCoreMagazineComponent::RollbackPrediction(int32 PredictionKey)
{
    if (!CurrentPrediction.IsValid() || CurrentPrediction.PredictionKey != PredictionKey)
    {
        return;
    }

    UE_LOG(LogMagazineComponent, Warning, TEXT("Rolling back reload prediction: Key=%d"), PredictionKey);

    // Restore state from before prediction
    WeaponAmmoState = CurrentPrediction.StateBeforePrediction;
    bIsReloading = false;
    CurrentReloadType = ESuspenseCoreReloadType::None;
    SetComponentTickEnabled(false);

    // Broadcast state changes
    BroadcastStateChanged();
    OnReloadStateChanged.Broadcast(false, ESuspenseCoreReloadType::None);

    // Clear prediction
    CurrentPrediction.Invalidate();
}

int32 USuspenseCoreMagazineComponent::GeneratePredictionKey()
{
    return NextPredictionKey++;
}

void USuspenseCoreMagazineComponent::ApplyPredictionLocally(const FSuspenseCoreMagazinePredictionData& Prediction)
{
    // Apply optimistic UI update
    bIsReloading = true;
    CurrentReloadType = Prediction.PredictedReloadType;
    ReloadDuration = Prediction.PredictedDuration;
    ReloadStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    PendingMagazine = Prediction.PredictedMagazine;

    // Enable tick for progress tracking
    SetComponentTickEnabled(true);

    // Broadcast state change for UI
    OnReloadStateChanged.Broadcast(true, CurrentReloadType);
}

void USuspenseCoreMagazineComponent::CleanupExpiredPredictions()
{
    if (!CurrentPrediction.IsValid())
    {
        return;
    }

    float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    float ElapsedTime = CurrentTime - CurrentPrediction.PredictionTimestamp;

    if (ElapsedTime > PredictionTimeoutSeconds)
    {
        UE_LOG(LogMagazineComponent, Warning, TEXT("Prediction timeout - rolling back: Key=%d"), CurrentPrediction.PredictionKey);
        RollbackPrediction(CurrentPrediction.PredictionKey);
    }
}

//================================================
// Client RPCs
//================================================

void USuspenseCoreMagazineComponent::ClientConfirmReloadPrediction_Implementation(int32 PredictionKey, bool bSuccess)
{
    if (bSuccess)
    {
        ConfirmPrediction(PredictionKey);
    }
    else
    {
        RollbackPrediction(PredictionKey);
    }
}
