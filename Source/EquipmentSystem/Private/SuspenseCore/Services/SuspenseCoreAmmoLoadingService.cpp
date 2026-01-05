// SuspenseCoreAmmoLoadingService.cpp
// Tarkov-style ammo loading service for magazines
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Services/SuspenseCoreAmmoLoadingService.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Services/SuspenseCoreServiceProvider.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceLocator.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogAmmoLoading, Log, All);

#define AMMO_LOG(Verbosity, Format, ...) \
    UE_LOG(LogAmmoLoading, Verbosity, TEXT("[AmmoLoadingService] " Format), ##__VA_ARGS__)

//==================================================================
// Constructor
//==================================================================

USuspenseCoreAmmoLoadingService::USuspenseCoreAmmoLoadingService()
{
}

//==================================================================
// ISuspenseCoreEquipmentService Implementation
//==================================================================

bool USuspenseCoreAmmoLoadingService::InitializeService(const FSuspenseCoreServiceInitParams& Params)
{
    if (ServiceState != ESuspenseCoreServiceLifecycleState::Uninitialized)
    {
        AMMO_LOG(Warning, TEXT("InitializeService: Already initialized"));
        return false;
    }

    ServiceState = ESuspenseCoreServiceLifecycleState::Initializing;

    AMMO_LOG(Log, TEXT("InitializeService: Starting initialization..."));

    // Get EventBus from ServiceProvider (following SuspenseCore architecture)
    if (USuspenseCoreServiceProvider* Provider = USuspenseCoreServiceProvider::Get(this))
    {
        EventBus = Provider->GetEventBus();
        AMMO_LOG(Log, TEXT("InitializeService: Got EventBus from ServiceProvider (%s)"),
            EventBus.IsValid() ? TEXT("Valid") : TEXT("NULL"));
    }
    else
    {
        AMMO_LOG(Warning, TEXT("InitializeService: ServiceProvider not found, trying GameInstance..."));

        // Fallback: try to get from GameInstance directly
        if (Params.ServiceLocator)
        {
            if (UGameInstance* GI = Params.ServiceLocator->GetGameInstance())
            {
                if (USuspenseCoreServiceProvider* GIProvider = GI->GetSubsystem<USuspenseCoreServiceProvider>())
                {
                    EventBus = GIProvider->GetEventBus();
                    AMMO_LOG(Log, TEXT("InitializeService: Got EventBus from GameInstance->ServiceProvider"));
                }
            }
        }
    }

    // Subscribe to events
    SubscribeToEvents();

    ServiceState = ESuspenseCoreServiceLifecycleState::Ready;

    AMMO_LOG(Log, TEXT("InitializeService: Service ready (EventBus=%s)"),
        EventBus.IsValid() ? TEXT("Valid") : TEXT("NULL"));

    return true;
}

bool USuspenseCoreAmmoLoadingService::ShutdownService(bool bForce)
{
    if (ServiceState == ESuspenseCoreServiceLifecycleState::Shutdown)
    {
        return true;
    }

    ServiceState = ESuspenseCoreServiceLifecycleState::Shutting;

    // Cancel all active operations
    ActiveOperations.Empty();

    // Unsubscribe from events
    UnsubscribeFromEvents();

    EventBus.Reset();
    DataManager.Reset();

    ServiceState = ESuspenseCoreServiceLifecycleState::Shutdown;

    AMMO_LOG(Log, TEXT("ShutdownService: Service shutdown complete"));

    return true;
}

FGameplayTag USuspenseCoreAmmoLoadingService::GetServiceTag() const
{
    return FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore.Service.AmmoLoading")), false);
}

FGameplayTagContainer USuspenseCoreAmmoLoadingService::GetRequiredDependencies() const
{
    // No strict dependencies - EventBus is obtained from EventManager
    return FGameplayTagContainer();
}

bool USuspenseCoreAmmoLoadingService::ValidateService(TArray<FText>& OutErrors) const
{
    bool bValid = true;

    if (!EventBus.IsValid())
    {
        OutErrors.Add(FText::FromString(TEXT("AmmoLoadingService: EventBus not available")));
        bValid = false;
    }

    return bValid;
}

void USuspenseCoreAmmoLoadingService::ResetService()
{
    // Cancel all operations
    ActiveOperations.Empty();
    ManagedMagazines.Empty();

    AMMO_LOG(Log, TEXT("ResetService: Service reset"));
}

FString USuspenseCoreAmmoLoadingService::GetServiceStats() const
{
    return FString::Printf(TEXT("AmmoLoadingService: ActiveOperations=%d, ManagedMagazines=%d, State=%s"),
        ActiveOperations.Num(),
        ManagedMagazines.Num(),
        *UEnum::GetValueAsString(ServiceState));
}

//==================================================================
// Legacy Initialization
//==================================================================

void USuspenseCoreAmmoLoadingService::Initialize(USuspenseCoreEventBus* InEventBus, USuspenseCoreDataManager* InDataManager)
{
    EventBus = InEventBus;
    DataManager = InDataManager;

    // Subscribe to ammo loading events
    SubscribeToEvents();

    // Mark as ready for legacy initialization path
    ServiceState = ESuspenseCoreServiceLifecycleState::Ready;

    AMMO_LOG(Log, TEXT("Initialized with EventBus=%s, DataManager=%s"),
        InEventBus ? TEXT("Valid") : TEXT("NULL"),
        InDataManager ? TEXT("Valid") : TEXT("NULL"));
}

void USuspenseCoreAmmoLoadingService::SubscribeToEvents()
{
    if (!EventBus.IsValid())
    {
        AMMO_LOG(Warning, TEXT("SubscribeToEvents: EventBus not valid"));
        return;
    }

    using namespace SuspenseCoreEquipmentTags::Magazine;

    // Subscribe to ammo load requested event (from UI drag&drop)
    LoadRequestedEventHandle = EventBus->SubscribeNative(
        TAG_Equipment_Event_Ammo_LoadRequested,
        this,
        FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreAmmoLoadingService::OnAmmoLoadRequestedEvent),
        ESuspenseCoreEventPriority::Normal
    );

    AMMO_LOG(Log, TEXT("Subscribed to Ammo.LoadRequested event"));
}

void USuspenseCoreAmmoLoadingService::UnsubscribeFromEvents()
{
    if (EventBus.IsValid() && LoadRequestedEventHandle.IsValid())
    {
        EventBus->Unsubscribe(LoadRequestedEventHandle);
        LoadRequestedEventHandle = FSuspenseCoreSubscriptionHandle();
    }
}

void USuspenseCoreAmmoLoadingService::OnAmmoLoadRequestedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
    AMMO_LOG(Log, TEXT("OnAmmoLoadRequestedEvent: Received event!"));

    // Extract data from event - UI uses MagazineInstanceID for the GUID
    FString MagazineInstanceIDString = EventData.GetString(TEXT("MagazineInstanceID"));
    FString MagazineIDString = EventData.GetString(TEXT("MagazineID")); // ItemID (not GUID)
    FString AmmoIDString = EventData.GetString(TEXT("AmmoID"));
    int32 Quantity = EventData.GetInt(TEXT("Quantity"));
    int32 SourceSlot = EventData.GetInt(TEXT("SourceSlot"));

    AMMO_LOG(Log, TEXT("OnAmmoLoadRequestedEvent: MagInstanceID=%s, MagID=%s, Ammo=%s, Qty=%d, Slot=%d"),
        *MagazineInstanceIDString, *MagazineIDString, *AmmoIDString, Quantity, SourceSlot);

    // Parse magazine instance ID (GUID)
    FGuid MagazineInstanceID;
    if (!FGuid::Parse(MagazineInstanceIDString, MagazineInstanceID))
    {
        AMMO_LOG(Warning, TEXT("OnAmmoLoadRequestedEvent: Invalid MagazineInstanceID GUID: %s"), *MagazineInstanceIDString);
        return;
    }

    // Create load request
    FSuspenseCoreAmmoLoadRequest Request;
    Request.MagazineInstanceID = MagazineInstanceID;
    Request.AmmoID = FName(*AmmoIDString);
    Request.RoundsToLoad = Quantity > 0 ? Quantity : 0; // 0 means load max
    Request.SourceInventorySlot = SourceSlot;
    Request.bIsQuickLoad = false;

    // Get owner actor from event Source
    AActor* OwnerActor = nullptr;
    if (EventData.Source)
    {
        OwnerActor = Cast<AActor>(EventData.Source);
        if (!OwnerActor)
        {
            // Try to get from component
            if (UActorComponent* Comp = Cast<UActorComponent>(EventData.Source))
            {
                OwnerActor = Comp->GetOwner();
            }
        }
    }

    // Start loading
    bool bSuccess = StartLoading(Request, OwnerActor);

    AMMO_LOG(Log, TEXT("OnAmmoLoadRequestedEvent: StartLoading result=%s"),
        bSuccess ? TEXT("SUCCESS") : TEXT("FAILED"));
}

//==================================================================
// Loading Operations
//==================================================================

bool USuspenseCoreAmmoLoadingService::StartLoading(const FSuspenseCoreAmmoLoadRequest& Request, AActor* OwnerActor)
{
    if (!Request.IsValid())
    {
        AMMO_LOG(Warning, TEXT("StartLoading: Invalid request"));
        return false;
    }

    // Check if already loading this magazine
    if (ActiveOperations.Contains(Request.MagazineInstanceID))
    {
        AMMO_LOG(Verbose, TEXT("StartLoading: Magazine %s already has active operation"),
            *Request.MagazineInstanceID.ToString());
        return false;
    }

    // Get magazine data for validation
    FSuspenseCoreMagazineData MagData;
    if (!GetMagazineData(ManagedMagazines.Contains(Request.MagazineInstanceID) ?
        ManagedMagazines[Request.MagazineInstanceID].MagazineID : NAME_None, MagData))
    {
        // Try to find magazine by instance ID in managed magazines
        // For now, use a default time per round
        MagData.LoadTimePerRound = 0.5f;
    }

    // Validate ammo compatibility
    if (!ValidateAmmoCompatibility(MagData, Request.AmmoID))
    {
        AMMO_LOG(Warning, TEXT("StartLoading: Ammo %s not compatible with magazine"),
            *Request.AmmoID.ToString());
        return false;
    }

    // Get magazine instance
    FSuspenseCoreMagazineInstance* MagInstance = ManagedMagazines.Find(Request.MagazineInstanceID);
    if (!MagInstance)
    {
        // Create a new tracked instance
        FSuspenseCoreMagazineInstance NewInstance;
        NewInstance.InstanceGuid = Request.MagazineInstanceID;
        NewInstance.MagazineID = MagData.MagazineID;
        NewInstance.MaxCapacity = MagData.MaxCapacity;
        ManagedMagazines.Add(Request.MagazineInstanceID, NewInstance);
        MagInstance = ManagedMagazines.Find(Request.MagazineInstanceID);
    }

    // Check if magazine is full
    if (MagInstance->IsFull())
    {
        AMMO_LOG(Verbose, TEXT("StartLoading: Magazine is already full"));
        return false;
    }

    // Calculate rounds to load
    int32 RoundsToLoad = Request.RoundsToLoad;
    if (RoundsToLoad <= 0)
    {
        RoundsToLoad = MagInstance->GetAvailableSpace();
    }
    RoundsToLoad = FMath::Min(RoundsToLoad, MagInstance->GetAvailableSpace());

    if (RoundsToLoad <= 0)
    {
        AMMO_LOG(Verbose, TEXT("StartLoading: No space in magazine"));
        return false;
    }

    // Create active operation
    FSuspenseCoreActiveLoadOperation Operation;
    Operation.Request = Request;
    Operation.TargetMagazine = *MagInstance;
    Operation.State = ESuspenseCoreAmmoLoadingState::Loading;
    Operation.StartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    Operation.TimePerRound = MagData.LoadTimePerRound;
    Operation.RoundsRemaining = RoundsToLoad;
    Operation.RoundsProcessed = 0;
    Operation.TotalDuration = MagData.LoadTimePerRound * RoundsToLoad;

    ActiveOperations.Add(Request.MagazineInstanceID, Operation);

    // Publish start event
    PublishLoadingEvent(SuspenseCoreEquipmentTags::Magazine::TAG_Equipment_Event_Ammo_LoadStarted,
        Request.MagazineInstanceID, Operation);

    // Broadcast delegate
    OnLoadingStateChanged.Broadcast(Request.MagazineInstanceID, ESuspenseCoreAmmoLoadingState::Loading, 0.0f);

    AMMO_LOG(Log, TEXT("StartLoading: Started loading %d rounds of %s into magazine %s (%.2fs total)"),
        RoundsToLoad, *Request.AmmoID.ToString(), *Request.MagazineInstanceID.ToString().Left(8),
        Operation.TotalDuration);

    return true;
}

bool USuspenseCoreAmmoLoadingService::StartUnloading(const FGuid& MagazineInstanceID, int32 RoundsToUnload, AActor* OwnerActor)
{
    if (!MagazineInstanceID.IsValid())
    {
        return false;
    }

    // Check if already has active operation
    if (ActiveOperations.Contains(MagazineInstanceID))
    {
        AMMO_LOG(Verbose, TEXT("StartUnloading: Magazine already has active operation"));
        return false;
    }

    // Get magazine instance
    FSuspenseCoreMagazineInstance* MagInstance = ManagedMagazines.Find(MagazineInstanceID);
    if (!MagInstance || MagInstance->IsEmpty())
    {
        AMMO_LOG(Verbose, TEXT("StartUnloading: Magazine not found or empty"));
        return false;
    }

    // Get magazine data
    FSuspenseCoreMagazineData MagData;
    GetMagazineData(MagInstance->MagazineID, MagData);

    // Calculate rounds to unload
    if (RoundsToUnload <= 0)
    {
        RoundsToUnload = MagInstance->CurrentRoundCount;
    }
    RoundsToUnload = FMath::Min(RoundsToUnload, MagInstance->CurrentRoundCount);

    if (RoundsToUnload <= 0)
    {
        return false;
    }

    // Create active operation
    FSuspenseCoreActiveLoadOperation Operation;
    Operation.Request.MagazineInstanceID = MagazineInstanceID;
    Operation.Request.AmmoID = MagInstance->LoadedAmmoID;
    Operation.Request.RoundsToLoad = RoundsToUnload;
    Operation.TargetMagazine = *MagInstance;
    Operation.State = ESuspenseCoreAmmoLoadingState::Unloading;
    Operation.StartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    Operation.TimePerRound = MagData.UnloadTimePerRound;
    Operation.RoundsRemaining = RoundsToUnload;
    Operation.RoundsProcessed = 0;
    Operation.TotalDuration = MagData.UnloadTimePerRound * RoundsToUnload;

    ActiveOperations.Add(MagazineInstanceID, Operation);

    // Publish start event
    PublishLoadingEvent(SuspenseCoreEquipmentTags::Magazine::TAG_Equipment_Event_Ammo_UnloadStarted,
        MagazineInstanceID, Operation);

    OnLoadingStateChanged.Broadcast(MagazineInstanceID, ESuspenseCoreAmmoLoadingState::Unloading, 0.0f);

    AMMO_LOG(Log, TEXT("StartUnloading: Started unloading %d rounds from magazine %s"),
        RoundsToUnload, *MagazineInstanceID.ToString().Left(8));

    return true;
}

void USuspenseCoreAmmoLoadingService::CancelOperation(const FGuid& MagazineInstanceID)
{
    FSuspenseCoreActiveLoadOperation* Operation = ActiveOperations.Find(MagazineInstanceID);
    if (!Operation || !Operation->IsActive())
    {
        return;
    }

    Operation->State = ESuspenseCoreAmmoLoadingState::Cancelled;

    // Publish cancel event
    PublishLoadingEvent(SuspenseCoreEquipmentTags::Magazine::TAG_Equipment_Event_Ammo_LoadCancelled,
        MagazineInstanceID, *Operation);

    // Complete with partial success
    CompleteOperation(MagazineInstanceID, Operation->RoundsProcessed > 0,
        Operation->RoundsProcessed > 0 ? TEXT("") : TEXT("Cancelled before any rounds processed"));

    AMMO_LOG(Log, TEXT("CancelOperation: Cancelled operation for magazine %s (%d rounds processed)"),
        *MagazineInstanceID.ToString().Left(8), Operation->RoundsProcessed);
}

void USuspenseCoreAmmoLoadingService::CancelAllOperations(AActor* OwnerActor)
{
    TArray<FGuid> ToCancel;
    for (const auto& Pair : ActiveOperations)
    {
        if (Pair.Value.IsActive())
        {
            ToCancel.Add(Pair.Key);
        }
    }

    for (const FGuid& MagID : ToCancel)
    {
        CancelOperation(MagID);
    }
}

//==================================================================
// Quick Load
//==================================================================

bool USuspenseCoreAmmoLoadingService::QuickLoadAmmo(FName AmmoID, int32 AmmoCount, AActor* OwnerActor, FGuid& OutMagazineID)
{
    OutMagazineID = FGuid();

    if (AmmoID.IsNone() || AmmoCount <= 0)
    {
        return false;
    }

    // Find best magazine: not full, compatible caliber, most empty first
    FSuspenseCoreMagazineInstance* BestMag = nullptr;
    int32 MostSpace = 0;

    for (auto& Pair : ManagedMagazines)
    {
        FSuspenseCoreMagazineInstance& Mag = Pair.Value;

        // Skip if already loading
        if (ActiveOperations.Contains(Mag.InstanceGuid))
        {
            continue;
        }

        // Skip if full
        if (Mag.IsFull())
        {
            continue;
        }

        // Skip if inserted in weapon
        if (Mag.bIsInsertedInWeapon)
        {
            continue;
        }

        // Check compatibility (same ammo type or empty)
        if (Mag.HasAmmo() && Mag.LoadedAmmoID != AmmoID)
        {
            continue;
        }

        // Check for most space
        int32 Space = Mag.GetAvailableSpace();
        if (Space > MostSpace)
        {
            MostSpace = Space;
            BestMag = &Mag;
        }
    }

    if (!BestMag)
    {
        AMMO_LOG(Verbose, TEXT("QuickLoadAmmo: No suitable magazine found for %s"), *AmmoID.ToString());
        return false;
    }

    // Create loading request
    FSuspenseCoreAmmoLoadRequest Request;
    Request.MagazineInstanceID = BestMag->InstanceGuid;
    Request.AmmoID = AmmoID;
    Request.RoundsToLoad = FMath::Min(AmmoCount, MostSpace);
    Request.bIsQuickLoad = true;

    OutMagazineID = BestMag->InstanceGuid;

    return StartLoading(Request, OwnerActor);
}

//==================================================================
// Queries
//==================================================================

bool USuspenseCoreAmmoLoadingService::CanLoadAmmo(FName MagazineID, FName AmmoID) const
{
    FSuspenseCoreMagazineData MagData;
    if (!GetMagazineData(MagazineID, MagData))
    {
        return false;
    }

    return ValidateAmmoCompatibility(MagData, AmmoID);
}

float USuspenseCoreAmmoLoadingService::GetLoadingDuration(FName MagazineID, int32 RoundCount) const
{
    FSuspenseCoreMagazineData MagData;
    if (!GetMagazineData(MagazineID, MagData))
    {
        return RoundCount * 0.5f; // Default 0.5s per round
    }

    return MagData.LoadTimePerRound * RoundCount;
}

bool USuspenseCoreAmmoLoadingService::IsLoading(const FGuid& MagazineInstanceID) const
{
    const FSuspenseCoreActiveLoadOperation* Operation = ActiveOperations.Find(MagazineInstanceID);
    return Operation && Operation->IsActive();
}

float USuspenseCoreAmmoLoadingService::GetLoadingProgress(const FGuid& MagazineInstanceID) const
{
    const FSuspenseCoreActiveLoadOperation* Operation = ActiveOperations.Find(MagazineInstanceID);
    if (!Operation || !Operation->IsActive() || Operation->TotalDuration <= 0.0f)
    {
        return 0.0f;
    }

    int32 TotalRounds = Operation->RoundsProcessed + Operation->RoundsRemaining;
    if (TotalRounds <= 0)
    {
        return 1.0f;
    }

    return static_cast<float>(Operation->RoundsProcessed) / static_cast<float>(TotalRounds);
}

bool USuspenseCoreAmmoLoadingService::GetActiveOperation(const FGuid& MagazineInstanceID,
    FSuspenseCoreActiveLoadOperation& OutOperation) const
{
    const FSuspenseCoreActiveLoadOperation* Operation = ActiveOperations.Find(MagazineInstanceID);
    if (Operation)
    {
        OutOperation = *Operation;
        return true;
    }
    return false;
}

//==================================================================
// Tick
//==================================================================

void USuspenseCoreAmmoLoadingService::Tick(float DeltaTime)
{
    TArray<FGuid> CompletedOperations;

    for (auto& Pair : ActiveOperations)
    {
        FSuspenseCoreActiveLoadOperation& Operation = Pair.Value;

        if (!Operation.IsActive())
        {
            CompletedOperations.Add(Pair.Key);
            continue;
        }

        ProcessLoadingTick(Operation, DeltaTime);

        // Check if completed
        if (Operation.RoundsRemaining <= 0)
        {
            CompletedOperations.Add(Pair.Key);
        }
    }

    // Complete finished operations
    for (const FGuid& MagID : CompletedOperations)
    {
        CompleteOperation(MagID, true);
    }
}

//==================================================================
// Internal Methods
//==================================================================

void USuspenseCoreAmmoLoadingService::ProcessLoadingTick(FSuspenseCoreActiveLoadOperation& Operation, float DeltaTime)
{
    if (Operation.TimePerRound <= 0.0f)
    {
        // Instant load all
        Operation.RoundsProcessed += Operation.RoundsRemaining;
        Operation.RoundsRemaining = 0;
        return;
    }

    // Calculate time for this tick
    static float AccumulatedTime = 0.0f;
    AccumulatedTime += DeltaTime;

    // Process rounds based on accumulated time
    while (AccumulatedTime >= Operation.TimePerRound && Operation.RoundsRemaining > 0)
    {
        AccumulatedTime -= Operation.TimePerRound;
        Operation.RoundsProcessed++;
        Operation.RoundsRemaining--;

        // Update managed magazine
        FSuspenseCoreMagazineInstance* Mag = ManagedMagazines.Find(Operation.Request.MagazineInstanceID);
        if (Mag)
        {
            if (Operation.State == ESuspenseCoreAmmoLoadingState::Loading)
            {
                Mag->LoadRounds(Operation.Request.AmmoID, 1);
            }
            else if (Operation.State == ESuspenseCoreAmmoLoadingState::Unloading)
            {
                Mag->UnloadRounds(1);
            }
        }
    }

    // Broadcast progress
    float Progress = GetLoadingProgress(Operation.Request.MagazineInstanceID);
    OnLoadingStateChanged.Broadcast(Operation.Request.MagazineInstanceID, Operation.State, Progress);
}

void USuspenseCoreAmmoLoadingService::CompleteOperation(const FGuid& MagazineInstanceID, bool bSuccess,
    const FString& ErrorMessage)
{
    FSuspenseCoreActiveLoadOperation* Operation = ActiveOperations.Find(MagazineInstanceID);
    if (!Operation)
    {
        return;
    }

    // Build result
    FSuspenseCoreAmmoLoadResult Result;
    Result.bSuccess = bSuccess;
    Result.RoundsProcessed = Operation->RoundsProcessed;
    Result.ErrorMessage = ErrorMessage;
    Result.Duration = GetWorld() ? (GetWorld()->GetTimeSeconds() - Operation->StartTime) : 0.0f;

    // Publish completion event
    FGameplayTag CompletionTag = (Operation->State == ESuspenseCoreAmmoLoadingState::Loading) ?
        SuspenseCoreEquipmentTags::Magazine::TAG_Equipment_Event_Ammo_LoadCompleted :
        SuspenseCoreEquipmentTags::Magazine::TAG_Equipment_Event_Ammo_UnloadCompleted;

    PublishLoadingEvent(CompletionTag, MagazineInstanceID, *Operation);

    // Broadcast delegate
    OnLoadingCompleted.Broadcast(MagazineInstanceID, Result);

    AMMO_LOG(Log, TEXT("CompleteOperation: Magazine %s - %s, %d rounds in %.2fs"),
        *MagazineInstanceID.ToString().Left(8),
        bSuccess ? TEXT("Success") : TEXT("Failed"),
        Result.RoundsProcessed, Result.Duration);

    // Remove from active operations
    ActiveOperations.Remove(MagazineInstanceID);
}

void USuspenseCoreAmmoLoadingService::PublishLoadingEvent(const FGameplayTag& EventTag,
    const FGuid& MagazineInstanceID, const FSuspenseCoreActiveLoadOperation& Operation)
{
    USuspenseCoreEventBus* Bus = EventBus.Get();
    if (!Bus)
    {
        return;
    }

    FSuspenseCoreEventData EventData;
    EventData.SetString(TEXT("MagazineInstanceID"), MagazineInstanceID.ToString());
    EventData.SetString(TEXT("AmmoID"), Operation.Request.AmmoID.ToString());
    EventData.SetInt(TEXT("RoundsProcessed"), Operation.RoundsProcessed);
    EventData.SetInt(TEXT("RoundsRemaining"), Operation.RoundsRemaining);
    EventData.SetFloat(TEXT("Progress"), GetLoadingProgress(MagazineInstanceID));
    EventData.SetFloat(TEXT("TotalDuration"), Operation.TotalDuration);
    EventData.SetBool(TEXT("IsQuickLoad"), Operation.Request.bIsQuickLoad);

    Bus->Publish(EventTag, EventData);
}

bool USuspenseCoreAmmoLoadingService::GetMagazineData(FName MagazineID, FSuspenseCoreMagazineData& OutData) const
{
    USuspenseCoreDataManager* DM = DataManager.Get();
    if (!DM || MagazineID.IsNone())
    {
        return false;
    }

    return DM->GetMagazineData(MagazineID, OutData);
}

bool USuspenseCoreAmmoLoadingService::ValidateAmmoCompatibility(const FSuspenseCoreMagazineData& MagData,
    FName AmmoID) const
{
    if (!MagData.IsValid() || AmmoID.IsNone())
    {
        return true; // Permissive by default if no data
    }

    // Check caliber match
    // AmmoID format: "556x45_M855" -> caliber tag: "Item.Ammo.556x45"
    FString AmmoStr = AmmoID.ToString();
    int32 UnderscoreIndex;
    if (AmmoStr.FindChar('_', UnderscoreIndex))
    {
        FString CaliberPart = AmmoStr.Left(UnderscoreIndex);
        FGameplayTag AmmoCaliber = FGameplayTag::RequestGameplayTag(
            FName(*FString::Printf(TEXT("Item.Ammo.%s"), *CaliberPart)), false);

        if (AmmoCaliber.IsValid() && MagData.Caliber.IsValid())
        {
            return MagData.IsCompatibleWithCaliber(AmmoCaliber);
        }
    }

    return true; // Permissive if caliber parsing fails
}

#undef AMMO_LOG
