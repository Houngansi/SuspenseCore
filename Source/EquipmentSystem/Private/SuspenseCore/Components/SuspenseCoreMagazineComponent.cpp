// SuspenseCoreMagazineComponent.cpp
// Tarkov-style magazine management component
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Components/SuspenseCoreMagazineComponent.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeapon.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogMagazineComponent, Log, All);

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
    if (WeaponInterface->GetWeaponData(WeaponData))
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

    return EjectedMag;
}

bool USuspenseCoreMagazineComponent::SwapMagazineFromQuickSlot(int32 QuickSlotIndex, bool bEmergencyDrop)
{
    // TODO: Implement QuickSlot integration
    UE_LOG(LogMagazineComponent, Warning, TEXT("SwapMagazineFromQuickSlot not yet implemented"));
    return false;
}

//================================================
// Chamber Operations
//================================================

bool USuspenseCoreMagazineComponent::ChamberRoundInternal()
{
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

    // TODO: Get from weapon AttributeSet
    // if (WeaponAttributeSet)
    // {
    //     TacticalTime = WeaponAttributeSet->GetTacticalReloadTime();
    //     FullTime = WeaponAttributeSet->GetFullReloadTime();
    // }

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
}
