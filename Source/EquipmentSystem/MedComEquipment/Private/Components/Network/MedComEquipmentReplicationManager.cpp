// Copyright MedCom Team. All Rights Reserved.
#include "Components/Network/MedComEquipmentReplicationManager.h"
#include "Services/EquipmentNetworkServiceImpl.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Services/EquipmentServiceMacros.h"
#include "Misc/SecureHash.h"
#include "DrawDebugHelpers.h"

// FReplicatedSlotItem
void FReplicatedSlotItem::PreReplicatedRemove(const FReplicatedSlotArray& InArraySerializer)
{
    UE_LOG(LogEquipmentReplication,Verbose,TEXT("PreReplicatedRemove: Slot %d removed"),SlotIndex);
}
void FReplicatedSlotItem::PostReplicatedAdd(const FReplicatedSlotArray& InArraySerializer)
{
    UE_LOG(LogEquipmentReplication,Verbose,TEXT("PostReplicatedAdd: Slot %d added"),SlotIndex);
    if(InArraySerializer.OwnerManager)
    {
        if(InArraySerializer.OwnerManager->bUseHMACSecurity && !ItemHMAC.IsEmpty())
        {
            if(!InArraySerializer.OwnerManager->VerifySlotHMAC(ItemInstance,ItemHMAC))
            {
                UE_LOG(LogEquipmentReplication,Error,TEXT("HMAC verification failed for slot %d"),SlotIndex);
                InArraySerializer.OwnerManager->Statistics.HMACFailures++;
                return;
            }
            InArraySerializer.OwnerManager->Statistics.HMACValidations++;
        }
    }
}
void FReplicatedSlotItem::PostReplicatedChange(const FReplicatedSlotArray& InArraySerializer)
{
    UE_LOG(LogEquipmentReplication,Verbose,TEXT("PostReplicatedChange: Slot %d changed"),SlotIndex);
    if(InArraySerializer.OwnerManager)
    {
        if(InArraySerializer.OwnerManager->bUseHMACSecurity && !ItemHMAC.IsEmpty())
        {
            if(!InArraySerializer.OwnerManager->VerifySlotHMAC(ItemInstance,ItemHMAC))
            {
                UE_LOG(LogEquipmentReplication,Error,TEXT("HMAC verification failed for slot %d"),SlotIndex);
                InArraySerializer.OwnerManager->Statistics.HMACFailures++;
                return;
            }
            InArraySerializer.OwnerManager->Statistics.HMACValidations++;
        }
    }
}

// FReplicatedSlotArray
void FReplicatedSlotArray::PostReplicatedAdd(const TArrayView<int32>& AddedIndices,int32 FinalSize)
{
    if(!OwnerManager)return;
    for(int32 Index:AddedIndices)
    {
        if(Items.IsValidIndex(Index))
        {
            if(OwnerManager->DataProvider.GetInterface())
            {
                OwnerManager->DataProvider->SetSlotItem(Items[Index].SlotIndex,Items[Index].ItemInstance,true);
            }
        }
    }
}
void FReplicatedSlotArray::PostReplicatedChange(const TArrayView<int32>& ChangedIndices,int32 FinalSize)
{
    if(!OwnerManager)return;
    for(int32 Index:ChangedIndices)
    {
        if(Items.IsValidIndex(Index))
        {
            if(OwnerManager->DataProvider.GetInterface())
            {
                OwnerManager->DataProvider->SetSlotItem(Items[Index].SlotIndex,Items[Index].ItemInstance,true);
            }
        }
    }
}
void FReplicatedSlotArray::PreReplicatedRemove(const TArrayView<int32>& RemovedIndices,int32 FinalSize)
{
    if(!OwnerManager)return;
    for(int32 Index:RemovedIndices)
    {
        if(Items.IsValidIndex(Index))
        {
            if(OwnerManager->DataProvider.GetInterface())
            {
                OwnerManager->DataProvider->ClearSlot(Items[Index].SlotIndex);
            }
        }
    }
}

// UMedComEquipmentReplicationManager
UMedComEquipmentReplicationManager::UMedComEquipmentReplicationManager()
{
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.TickInterval=0.1f;
    SetIsReplicatedByDefault(true);
    UpdateInterval=MinUpdateInterval;
    DynamicMaxDeltasBeforeFull=MaxDeltasBeforeFull;
    ReplicatedSlotArray.OwnerManager=this;
}

void UMedComEquipmentReplicationManager::BeginPlay()
{
    Super::BeginPlay();
    if(DataProvider.GetInterface())
    {
        DataProvider->OnSlotDataChanged().AddUObject(this,&UMedComEquipmentReplicationManager::OnDataChanged);
    }
    UE_LOG(LogEquipmentReplication,Log,TEXT("ReplicationManager: Initialized for %s with FFastArraySerializer"),*GetNameSafe(GetOwner()));
}

void UMedComEquipmentReplicationManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if(DataProvider.GetInterface())
    {
        DataProvider->OnSlotDataChanged().RemoveAll(this);
    }
    {FScopeLock SlotLock(&SlotStateLock);SlotStates.Empty();}
    {FScopeLock ClientLock(&ClientStateLock);ClientStates.Empty();}
    {FScopeLock DirtyLock(&DirtySlotLock);DirtySlots.Empty();}
    ReplicatedSlotArray.Items.Empty();
    VersionHistory.Empty();
    Super::EndPlay(EndPlayReason);
}

void UMedComEquipmentReplicationManager::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime,TickType,ThisTickFunction);
    if(GetOwnerRole()!=ROLE_Authority){return;}
    if(GetWorld())
    {
        float Now=GetWorld()->GetTimeSeconds();
        if(Now-LastReplicationTime>=UpdateInterval){ProcessReplication();LastReplicationTime=Now;}
    }
    static float LastCleanupTime=0.0f;
    if(GetWorld()&&GetWorld()->GetTimeSeconds()-LastCleanupTime>5.0f)
    {
        CleanupClientStates();
        if(VersionHistory.Num()>50)
        {
            TArray<uint32> Old;
            for(const auto& P:VersionHistory)
            {
                // фикс: сравнение в беззнаковом пространстве
                if(CurrentVersion - P.Key > 100u)
                {
                    Old.Add(P.Key);
                }
            }
            for(uint32 V:Old){VersionHistory.Remove(V);}
        }
        LastCleanupTime=GetWorld()->GetTimeSeconds();
    }
}

void UMedComEquipmentReplicationManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps)const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UMedComEquipmentReplicationManager,ReplicatedSlotArray);
    DOREPLIFETIME(UMedComEquipmentReplicationManager,CurrentVersion);
    DOREPLIFETIME(UMedComEquipmentReplicationManager,ReplicatedActiveWeaponSlot);
    DOREPLIFETIME(UMedComEquipmentReplicationManager,ReplicatedEquipmentState);
    DOREPLIFETIME_CONDITION(UMedComEquipmentReplicationManager,CompressedData,COND_Custom);
}

void UMedComEquipmentReplicationManager::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
    Super::PreReplication(ChangedPropertyTracker);
    bool bActiveCompressed=false;
    if(GetOwnerRole()==ROLE_Authority && bUseCompression)
    {
        const int32 SlotCount=DataProvider.GetInterface()?DataProvider->GetSlotCount():0;
        int32 DirtyCount=0;
        {FScopeLock Lock(&DirtySlotLock);DirtyCount=DirtySlots.Num();}
        bActiveCompressed=bForceFullReplication||(SlotCount>0 && DirtyCount>=FMath::Max(1,SlotCount/2));
        if(bActiveCompressed)
        {
            const FReplicatedEquipmentData Full=GetReplicatedData();
            CompressedData=CompressData(Full);
        }
    }
    DOREPLIFETIME_ACTIVE_OVERRIDE(UMedComEquipmentReplicationManager,CompressedData,bActiveCompressed);
}

// IMedComReplicationProvider
void UMedComEquipmentReplicationManager::MarkForReplication(int32 SlotIndex,bool bForceUpdate)
{
    if(!DataProvider.GetInterface()||!DataProvider->IsValidSlotIndex(SlotIndex))
    {
        UE_LOG(LogEquipmentReplication,Warning,TEXT("MarkForReplication: Invalid slot index %d"),SlotIndex);
        return;
    }
    {FScopeLock Lock(&DirtySlotLock);DirtySlots.Add(SlotIndex);}
    {FScopeLock Lock(&SlotStateLock);
        if(SlotStates.IsValidIndex(SlotIndex))
        {
            FSlotReplicationState& S=SlotStates[SlotIndex];
            S.bIsDirty=true;
            S.LastChangeTime=GetWorld()?GetWorld()->GetTimeSeconds():0.0f;
            S.ChangeCount++;
            UpdateSlotPriority(SlotIndex);
        }
    }
    CurrentDeltaMask.AddSlot(SlotIndex);
    if(bForceUpdate){bForceFullReplication=true;ProcessReplication();}
    CurrentVersion++;
    VersionHistory.Add(CurrentVersion,CurrentDeltaMask);
    MARK_PROPERTY_DIRTY_FROM_NAME(UMedComEquipmentReplicationManager,CurrentVersion,this);
    UE_LOG(LogEquipmentReplication,Verbose,TEXT("MarkForReplication: Slot %d dirty, version %d"),SlotIndex,CurrentVersion);
}

FReplicatedEquipmentData UMedComEquipmentReplicationManager::GetReplicatedData()const
{
    FScopeLock Lock(&SlotStateLock);
    FReplicatedEquipmentData Data;
    for(const FReplicatedSlotItem& Item:ReplicatedSlotArray.Items)
    {
        if(Data.SlotInstances.Num()<=Item.SlotIndex){Data.SlotInstances.SetNum(Item.SlotIndex+1);}
        Data.SlotInstances[Item.SlotIndex]=Item.ItemInstance;
    }
    Data.ActiveWeaponSlot=ReplicatedActiveWeaponSlot;
    Data.CurrentState=ReplicatedEquipmentState;
    Data.ReplicationVersion=CurrentVersion;
    Data.LastUpdateTime=GetWorld()?GetWorld()->GetTimeSeconds():0.0f;
    return Data;
}

void UMedComEquipmentReplicationManager::ApplyReplicatedData(const FReplicatedEquipmentData& Data,bool bIsInitialReplication)
{
    if(GetOwnerRole()==ROLE_Authority){return;}
    if(DataProvider.GetInterface())
    {
        for(int32 i=0;i<Data.SlotInstances.Num();i++)
        {
            if(Data.SlotInstances[i].IsValid()){DataProvider->SetSlotItem(i,Data.SlotInstances[i],true);}
        }
        DataProvider->SetActiveWeaponSlot(Data.ActiveWeaponSlot);
        DataProvider->SetEquipmentState(Data.CurrentState);
    }
    CurrentVersion=Data.ReplicationVersion;
    OnReplicatedStateApplied.Broadcast(Data);
    UE_LOG(LogEquipmentReplication,Verbose,TEXT("ApplyReplicatedData: Applied version %d, Initial: %s"),Data.ReplicationVersion,bIsInitialReplication?TEXT("Yes"):TEXT("No"));
}

void UMedComEquipmentReplicationManager::SetReplicationPolicy(EEquipmentReplicationPolicy Policy)
{
    CurrentPolicy=Policy;
    UE_LOG(LogEquipmentReplication,Log,TEXT("SetReplicationPolicy: %s"),*UEnum::GetValueAsString(Policy));
}

void UMedComEquipmentReplicationManager::ForceFullReplication()
{
    bForceFullReplication=true;
    {FScopeLock Lock(&DirtySlotLock);DirtySlots.Empty();if(DataProvider.GetInterface()){for(int32 i=0;i<DataProvider->GetSlotCount();i++){DirtySlots.Add(i);}}}
    {FScopeLock Lock(&SlotStateLock);for(FSlotReplicationState& S:SlotStates){S.bIsDirty=true;}}
    CurrentDeltaMask.Clear();
    CurrentVersion++;
    MARK_PROPERTY_DIRTY_FROM_NAME(UMedComEquipmentReplicationManager,CurrentVersion,this);
    ProcessReplication();
    UE_LOG(LogEquipmentReplication,Log,TEXT("ForceFullReplication: version %d"),CurrentVersion);
}

static APlayerController* ResolveOwnerPC(const AActor* Owner)
{
    if(!Owner)return nullptr;
    const AController* C=Owner->GetInstigatorController();
    return const_cast<APlayerController*>(Cast<APlayerController>(C));
}

bool UMedComEquipmentReplicationManager::ShouldReplicateTo(APlayerController* ViewTarget)const
{
    if(!ViewTarget){return false;}
    switch(CurrentPolicy)
    {
        case EEquipmentReplicationPolicy::Always: return true;
        case EEquipmentReplicationPolicy::OnlyToOwner:
        {
            APlayerController* OwnerPC=ResolveOwnerPC(GetOwner());
            return OwnerPC && OwnerPC==ViewTarget;
        }
        case EEquipmentReplicationPolicy::OnlyToRelevant:
        {
            const float R=bUseEnhancedRelevancy?CalculateEnhancedRelevancy(ViewTarget):CalculateEnhancedRelevancy(ViewTarget);
            return R>0.1f;
        }
        case EEquipmentReplicationPolicy::SkipOwner:
        {
            APlayerController* OwnerPC=ResolveOwnerPC(GetOwner());
            return !OwnerPC || OwnerPC!=ViewTarget;
        }
        case EEquipmentReplicationPolicy::Custom: return true;
        default: return true;
    }
}

bool UMedComEquipmentReplicationManager::GetReplicationPriority(APlayerController* ViewTarget,float& OutPriority)const
{
    if(!ShouldReplicateTo(ViewTarget)){return false;}
    OutPriority=1.0f;
    APlayerController* OwnerPC=ResolveOwnerPC(GetOwner());
    if(OwnerPC && OwnerPC==ViewTarget){OutPriority=10.0f;}
    float DirtyWeight=1.0f;
    {FScopeLock Lock(&DirtySlotLock);
        if(DirtySlots.Num()>0)
        {
            float Acc=0.0f;
            FScopeLock SLock(&SlotStateLock);
            for(int32 SlotIndex:DirtySlots)
            {
                if(SlotStates.IsValidIndex(SlotIndex)){Acc+=FMath::Max(1.0f,SlotStates[SlotIndex].ReplicationPriority);}
            }
            DirtyWeight+=Acc*0.2f;
        }
    }
    if(bForceFullReplication){DirtyWeight*=5.0f;}
    if(bUseEnhancedRelevancy)
    {
        const float R=FMath::Max(0.1f,CalculateEnhancedRelevancy(ViewTarget));
        OutPriority*=R;
    }
    OutPriority*=DirtyWeight;
    return true;
}

FReplicatedEquipmentData UMedComEquipmentReplicationManager::OptimizeReplicationData(const FReplicatedEquipmentData& Data)const
{
    FReplicatedEquipmentData O=Data;
    while(O.SlotInstances.Num()>0 && !O.SlotInstances.Last().IsValid()){O.SlotInstances.RemoveAt(O.SlotInstances.Num()-1);}
    for(FInventoryItemInstance& I:O.SlotInstances)
    {
        if(I.IsValid())
        {
            I.RuntimeProperties.Remove(TEXT("LastUsedTime"));
            I.RuntimeProperties.Remove(TEXT("ClientPrediction"));
            I.RuntimeProperties.Remove(TEXT("LocalCooldown"));
        }
    }
    return O;
}

FReplicatedEquipmentData UMedComEquipmentReplicationManager::GetReplicationDelta(uint32 LastVersion)const
{
    FScopeLock Lock(&SlotStateLock);
    if(LastVersion==CurrentVersion){return FReplicatedEquipmentData();}
    // фикс: каст к беззнаковому при сравнении со счетчиком дельт
    if(bForceFullReplication || (static_cast<uint32>(CurrentVersion - LastVersion) > static_cast<uint32>(DynamicMaxDeltasBeforeFull)))
    {
        return GetReplicatedData();
    }
    FReplicatedEquipmentData Delta;
    Delta.ReplicationVersion=CurrentVersion;
    Delta.ActiveWeaponSlot=ReplicatedActiveWeaponSlot;
    Delta.CurrentState=ReplicatedEquipmentState;
    Delta.LastUpdateTime=GetWorld()?GetWorld()->GetTimeSeconds():0.0f;
    TSet<int32> Changed;
    for(uint32 V=LastVersion+1;V<=CurrentVersion;V++)
    {
        if(const FReplicationDeltaMask* M=VersionHistory.Find(V))
        {
            for(int32 SlotIndex:M->DirtySlotIndices){Changed.Add(SlotIndex);}
        }
    }
    for(int32 SlotIndex:Changed)
    {
        if(Delta.SlotInstances.Num()<=SlotIndex){Delta.SlotInstances.SetNum(SlotIndex+1);}
        for(const FReplicatedSlotItem& Item:ReplicatedSlotArray.Items)
        {
            if(Item.SlotIndex==SlotIndex){Delta.SlotInstances[SlotIndex]=Item.ItemInstance;break;}
        }
    }
    return OptimizeReplicationData(Delta);
}

void UMedComEquipmentReplicationManager::OnReplicationCallback(const FName& PropertyName)
{
    if(PropertyName==GET_MEMBER_NAME_CHECKED(UMedComEquipmentReplicationManager,ReplicatedSlotArray)){OnRep_SlotArray();}
    else if(PropertyName==GET_MEMBER_NAME_CHECKED(UMedComEquipmentReplicationManager,CurrentVersion)){OnRep_Version();}
    else if(PropertyName==GET_MEMBER_NAME_CHECKED(UMedComEquipmentReplicationManager,ReplicatedActiveWeaponSlot)){OnRep_ActiveWeaponSlot();}
    else if(PropertyName==GET_MEMBER_NAME_CHECKED(UMedComEquipmentReplicationManager,ReplicatedEquipmentState)){OnRep_EquipmentState();}
}

// Configuration
bool UMedComEquipmentReplicationManager::Initialize(TScriptInterface<IMedComEquipmentDataProvider> InDataProvider)
{
    if(!InDataProvider.GetInterface())
    {
        UE_LOG(LogEquipmentReplication,Error,TEXT("Initialize: Invalid data provider"));
        return false;
    }
    DataProvider=InDataProvider;
    {FScopeLock Lock(&SlotStateLock);
        const int32 SlotCount=DataProvider->GetSlotCount();
        SlotStates.SetNum(SlotCount);
        for(int32 i=0;i<SlotCount;i++)
        {
            SlotStates[i].SlotIndex=i;
            SlotStates[i].LastReplicatedItem=DataProvider->GetSlotItem(i);
            SlotStates[i].LastReplicatedVersion=0;
            SlotStates[i].bIsDirty=false;
            SlotStates[i].ReplicationPriority=1.0f;
        }
    }
    if(GetOwnerRole()==ROLE_Authority)
    {
        ReplicatedSlotArray.Items.Empty();
        for(int32 i=0;i<DataProvider->GetSlotCount();i++)
        {
            FInventoryItemInstance SlotItem=DataProvider->GetSlotItem(i);
            if(SlotItem.IsValid())
            {
                FReplicatedSlotItem NewItem;NewItem.SlotIndex=i;NewItem.ItemInstance=SlotItem;NewItem.ItemVersion=1;
                if(bUseHMACSecurity && SecurityService){NewItem.ItemHMAC=GenerateSlotHMAC(SlotItem);}
                ReplicatedSlotArray.Items.Add(NewItem);
            }
        }
        ReplicatedActiveWeaponSlot=DataProvider->GetActiveWeaponSlot();
        ReplicatedEquipmentState=DataProvider->GetCurrentEquipmentState();
        CurrentVersion=1;
    }
    DataProvider->OnSlotDataChanged().AddUObject(this,&UMedComEquipmentReplicationManager::OnDataChanged);
    UE_LOG(LogEquipmentReplication,Log,TEXT("Initialize: %d slots with FastArray"),DataProvider->GetSlotCount());
    return true;
}

void UMedComEquipmentReplicationManager::SetSecurityService(UEquipmentNetworkServiceImpl* InSecurityService)
{
    SecurityService=InSecurityService;
    UE_LOG(LogEquipmentReplication,Log,TEXT("SetSecurityService: integrated"));
}

void UMedComEquipmentReplicationManager::SetUpdateRate(float UpdatesPerSecond)
{
    UpdatesPerSecond=FMath::Clamp(UpdatesPerSecond,1.0f,60.0f);
    UpdateInterval=1.0f/UpdatesPerSecond;
    UE_LOG(LogEquipmentReplication,Log,TEXT("SetUpdateRate: %.1f Hz"),UpdatesPerSecond);
}

void UMedComEquipmentReplicationManager::OnNetworkQualityUpdated(float Quality)
{
    CurrentNetworkQuality=FMath::Clamp(Quality,0.0f,1.0f);
    AdaptReplicationStrategy(CurrentNetworkQuality);
    UE_LOG(LogEquipmentReplication,Log,TEXT("OnNetworkQualityUpdated: %.2f"),CurrentNetworkQuality);
}

// OnRep callbacks
void UMedComEquipmentReplicationManager::OnRep_SlotArray()
{
    if(GetOwnerRole()!=ROLE_Authority)
    {
        const FReplicatedEquipmentData Data=GetReplicatedData();
        Statistics.TotalUpdates++;
        Statistics.DeltaUpdates++;
        APlayerController* LocalPC=GetWorld()?GetWorld()->GetFirstPlayerController():nullptr;
        OnDataReplicated.Broadcast(LocalPC,Data);
        OnReplicatedStateApplied.Broadcast(Data);
        UE_LOG(LogEquipmentReplication,Verbose,TEXT("OnRep_SlotArray: applied via FastArray, version %d"),CurrentVersion);
    }
}
void UMedComEquipmentReplicationManager::OnRep_Version()
{
    UE_LOG(LogEquipmentReplication,Verbose,TEXT("OnRep_Version: %d"),CurrentVersion);
}
void UMedComEquipmentReplicationManager::OnRep_ActiveWeaponSlot()
{
    if(GetOwnerRole()!=ROLE_Authority && DataProvider.GetInterface()){DataProvider->SetActiveWeaponSlot(ReplicatedActiveWeaponSlot);}
}
void UMedComEquipmentReplicationManager::OnRep_EquipmentState()
{
    if(GetOwnerRole()!=ROLE_Authority && DataProvider.GetInterface()){DataProvider->SetEquipmentState(ReplicatedEquipmentState);}
}

// Internal
void UMedComEquipmentReplicationManager::ProcessReplication()
{
    if(!DataProvider.GetInterface()||GetOwnerRole()!=ROLE_Authority){return;}
    {FScopeLock Lock(&SlotStateLock);
        bool bHasChanges=false;
        FScopeLock DirtyLock(&DirtySlotLock);
        for(int32 SlotIndex:DirtySlots)
        {
            FInventoryItemInstance NewItem=DataProvider->GetSlotItem(SlotIndex);
            FReplicatedSlotItem* Existing=ReplicatedSlotArray.Items.FindByPredicate([SlotIndex](const FReplicatedSlotItem& I){return I.SlotIndex==SlotIndex;});
            if(Existing)
            {
                // фикс: надёжное сравнение UStruct вместо operator!=
                const bool bEqual = FInventoryItemInstance::StaticStruct()
                    ->CompareScriptStruct(&Existing->ItemInstance, &NewItem, 0 /*PPF_None*/);
                if(!bEqual)
                {
                    Existing->ItemInstance=NewItem;
                    Existing->ItemVersion++;
                    if(bUseHMACSecurity && SecurityService){Existing->ItemHMAC=GenerateSlotHMAC(NewItem);}
                    ReplicatedSlotArray.MarkItemDirty(*Existing);
                    bHasChanges=true;
                }
            }
            else if(NewItem.IsValid())
            {
                FReplicatedSlotItem NewSlot;NewSlot.SlotIndex=SlotIndex;NewSlot.ItemInstance=NewItem;NewSlot.ItemVersion=1;
                if(bUseHMACSecurity && SecurityService){NewSlot.ItemHMAC=GenerateSlotHMAC(NewItem);}
                ReplicatedSlotArray.Items.Add(NewSlot);
                ReplicatedSlotArray.MarkItemDirty(ReplicatedSlotArray.Items.Last());
                bHasChanges=true;
            }
            if(SlotStates.IsValidIndex(SlotIndex))
            {
                SlotStates[SlotIndex].LastReplicatedItem=NewItem;
                SlotStates[SlotIndex].LastReplicatedVersion=CurrentVersion;
                SlotStates[SlotIndex].bIsDirty=false;
            }
        }
        ReplicatedActiveWeaponSlot=DataProvider->GetActiveWeaponSlot();
        ReplicatedEquipmentState=DataProvider->GetCurrentEquipmentState();
        if(bHasChanges)
        {
            DirtySlots.Empty();
            CurrentDeltaMask.Clear();
            bForceFullReplication=false;
            ReplicatedSlotArray.MarkArrayDirty();
        }
    }
    FScopeLock ClientLock(&ClientStateLock);
    for(FClientReplicationState& CS:ClientStates)
    {
        if(CS.Client && CS.Client->GetNetConnection()){UpdateClientReplication(CS);}
    }
    Statistics.ActiveClients=ClientStates.Num();
}

void UMedComEquipmentReplicationManager::UpdateClientReplication(FClientReplicationState& ClientState)
{
    if(!ClientState.Client || !ShouldReplicateTo(ClientState.Client)){return;}
    const float Now=GetWorld()?GetWorld()->GetTimeSeconds():0.0f;
    if(Now-ClientState.LastUpdateTime<UpdateInterval){return;}

    const uint32 LastAck = ClientState.LastAcknowledgedVersion;
    const uint32 DeltaCount = static_cast<uint32>(CurrentVersion - LastAck);

    const bool bNeedsFull =
        bForceFullReplication ||
        (LastAck==0u) ||
        (DeltaCount > static_cast<uint32>(DynamicMaxDeltasBeforeFull)) ||
        (ClientState.ConsecutiveDeltasSent >= DynamicMaxDeltasBeforeFull);

    if(bNeedsFull){ClientState.ConsecutiveDeltasSent=0;Statistics.FullUpdates++;}
    else{ClientState.ConsecutiveDeltasSent++;Statistics.DeltaUpdates++;}

    ClientState.LastUpdateTime=Now;
    ClientState.LastAcknowledgedVersion=CurrentVersion;
    ClientState.PendingSlots.Empty();
    Statistics.TotalUpdates++;
}

FReplicatedEquipmentData UMedComEquipmentReplicationManager::BuildReplicationData(APlayerController* Client,bool bForceFull)const
{
    if(bForceFull){return OptimizeReplicationData(GetReplicatedData());}
    const FClientReplicationState* CS=ClientStates.FindByPredicate([Client](const FClientReplicationState& S){return S.Client==Client;});
    if(!CS){return OptimizeReplicationData(GetReplicatedData());}
    return GetReplicationDelta(CS->LastAcknowledgedVersion);
}

FReplicationDeltaMask UMedComEquipmentReplicationManager::BuildDeltaMask(uint32 FromVersion,uint32 ToVersion)const
{
    FReplicationDeltaMask Mask;Mask.StartVersion=FromVersion;Mask.EndVersion=ToVersion;Mask.Timestamp=GetWorld()?GetWorld()->GetTimeSeconds():0.0f;
    for(uint32 V=FromVersion+1;V<=ToVersion;V++)
    {
        if(const FReplicationDeltaMask* VM=VersionHistory.Find(V))
        {
            for(int32 SlotIndex:VM->DirtySlotIndices){Mask.AddSlot(SlotIndex);}
        }
    }
    return Mask;
}

FString UMedComEquipmentReplicationManager::GenerateSlotHMAC(const FInventoryItemInstance& SlotData)const
{
    if(!SecurityService){return FString();}
    FString S=FString::Printf(TEXT("%s|%d|%d|%s"),*SlotData.ItemID.ToString(),SlotData.Quantity,SlotData.AnchorIndex,SlotData.bIsRotated?TEXT("R"):TEXT("N"));
    for(const auto& P:SlotData.RuntimeProperties){S+=FString::Printf(TEXT("|%s:%.2f"),*P.Key.ToString(),P.Value);}
    FSHAHash Hash;FSHA1::HashBuffer(TCHAR_TO_UTF8(*S),S.Len(),Hash.Hash);
    return BytesToHex(Hash.Hash,20);
}

bool UMedComEquipmentReplicationManager::VerifySlotHMAC(const FInventoryItemInstance& SlotData,const FString& HMACSignature)const
{
    if(!SecurityService || HMACSignature.IsEmpty()){return true;}
    const FString Expected=GenerateSlotHMAC(SlotData);
    return Expected.Equals(HMACSignature);
}

FCompressedReplicationData UMedComEquipmentReplicationManager::CompressData(const FReplicatedEquipmentData& Data)const
{
    FCompressedReplicationData C;

    // --- безопасная сериализация в Raw ---
    TArray<uint8> Raw;
    Raw.Reset(256);
    FMemoryWriter W(Raw, /*bIsPersistent=*/true);

    // helper: сериализация массива USTRUCT-ов
    auto WriteStructArray = [&W](UScriptStruct* Struct, TArray<FInventoryItemInstance>& Array)
    {
        int32 Count = Array.Num();
        W << Count;
        for(int32 i=0;i<Count;i++)
        {
            Struct->SerializeItem(W, &Array[i], nullptr);
        }
    };

    // SlotInstances
    {
        // делаем копию для сериализации, чтобы не снимать const
        TArray<FInventoryItemInstance> Copy = Data.SlotInstances;
        WriteStructArray(FInventoryItemInstance::StaticStruct(), Copy);
    }

    // ActiveWeaponSlot
    {
        int32 Copy = Data.ActiveWeaponSlot;
        W << Copy;
    }

    // CurrentState (FGameplayTag)
    {
        FGameplayTag TagCopy = Data.CurrentState;
        FGameplayTag::StaticStruct()->SerializeItem(W, &TagCopy, nullptr);
    }

    // ReplicationVersion
    {
        uint32 Copy = Data.ReplicationVersion;
        W << Copy;
    }

    // LastUpdateTime
    {
        float Copy = Data.LastUpdateTime;
        W << Copy;
    }

    C.OriginalSize=Raw.Num();

    if(Raw.Num()>CompressionThreshold)
    {
        C.CompressionType=1;
        int32 MaxSize=FCompression::CompressMemoryBound(NAME_Zlib,Raw.Num());
        C.CompressedBytes.SetNum(MaxSize);
        bool bOk=FCompression::CompressMemory(NAME_Zlib,C.CompressedBytes.GetData(),MaxSize,Raw.GetData(),Raw.Num());
        if(bOk)
        {
            C.CompressedBytes.SetNum(MaxSize);
            Statistics.BytesSaved+=(C.OriginalSize-MaxSize);
        }
        else
        {
            C.CompressionType=0;
            C.CompressedBytes=Raw;
        }
    }
    else
    {
        C.CompressionType=0;
        C.CompressedBytes=Raw;
    }

    C.Checksum=FCrc::MemCrc32(C.CompressedBytes.GetData(),C.CompressedBytes.Num());

    if(bUseHMACSecurity && SecurityService)
    {
        FSHAHash Hash;FSHA1::HashBuffer(C.CompressedBytes.GetData(),C.CompressedBytes.Num(),Hash.Hash);
        C.HMACSignature=BytesToHex(Hash.Hash,20);
    }

    Statistics.BytesSent+=C.CompressedBytes.Num();
    Statistics.CompressionRatio=C.OriginalSize>0?(float)C.CompressedBytes.Num()/C.OriginalSize:1.0f;
    return C;
}


bool UMedComEquipmentReplicationManager::DecompressData(const FCompressedReplicationData& Compressed,FReplicatedEquipmentData& OutData)const
{
    if(bUseHMACSecurity && !Compressed.HMACSignature.IsEmpty())
    {
        FSHAHash Hash;FSHA1::HashBuffer(Compressed.CompressedBytes.GetData(),Compressed.CompressedBytes.Num(),Hash.Hash);
        const FString Expected=BytesToHex(Hash.Hash,20);
        if(!Expected.Equals(Compressed.HMACSignature))
        {
            UE_LOG(LogEquipmentReplication,Error,TEXT("DecompressData: HMAC verification failed"));
            Statistics.HMACFailures++;
            return false;
        }
        Statistics.HMACValidations++;
    }
    const uint32 Crc=FCrc::MemCrc32(Compressed.CompressedBytes.GetData(),Compressed.CompressedBytes.Num());
    if(Crc!=Compressed.Checksum)
    {
        UE_LOG(LogEquipmentReplication,Error,TEXT("DecompressData: Checksum mismatch"));
        return false;
    }

    TArray<uint8> Raw;
    if(Compressed.CompressionType==1)
    {
        Raw.SetNum(Compressed.OriginalSize);
        const bool bOk=FCompression::UncompressMemory(NAME_Zlib,Raw.GetData(),Compressed.OriginalSize,Compressed.CompressedBytes.GetData(),Compressed.CompressedBytes.Num());
        if(!bOk){UE_LOG(LogEquipmentReplication,Error,TEXT("DecompressData: Decompression failed"));return false;}
    }
    else{Raw=Compressed.CompressedBytes;}

    FMemoryReader R(Raw, /*bIsPersistent=*/true);

    // helper: чтение массива USTRUCT-ов
    auto ReadStructArray = [&R](UScriptStruct* Struct, TArray<FInventoryItemInstance>& OutArray)
    {
        int32 Count = 0;
        R << Count;
        OutArray.SetNum(Count);
        for(int32 i=0;i<Count;i++)
        {
            Struct->SerializeItem(R, &OutArray[i], nullptr);
        }
    };

    // SlotInstances
    ReadStructArray(FInventoryItemInstance::StaticStruct(), OutData.SlotInstances);

    // ActiveWeaponSlot
    R << OutData.ActiveWeaponSlot;

    // CurrentState (FGameplayTag)
    FGameplayTag TagTmp;
    FGameplayTag::StaticStruct()->SerializeItem(R, &TagTmp, nullptr);
    OutData.CurrentState = TagTmp;

    // ReplicationVersion
    R << OutData.ReplicationVersion;

    // LastUpdateTime
    R << OutData.LastUpdateTime;

    return true;
}


float UMedComEquipmentReplicationManager::CalculateEnhancedRelevancy(APlayerController* ViewTarget) const
{
    const AActor* OwnerActor = GetOwner();
    if (!ViewTarget || !OwnerActor) { return 0.0f; }

    const APawn* ViewPawn = ViewTarget->GetPawn();
    if (!ViewPawn) { return 0.0f; }

    // Геометрия/видимость
    const float Dist   = FVector::Dist(ViewPawn->GetActorLocation(), OwnerActor->GetActorLocation());
    float DistRel      = 1.0f - FMath::Clamp(Dist / RelevancyDistance, 0.0f, 1.0f);
    DistRel            = FMath::Pow(DistRel, 0.5f);
    const float Vis    = ViewTarget->LineOfSightTo(OwnerActor) ? 2.0f : 1.0f;

    const FVector Dir  = ViewPawn->GetControlRotation().Vector();
    const FVector To   = (OwnerActor->GetActorLocation() - ViewPawn->GetActorLocation()).GetSafeNormal();
    const float   Dot  = FVector::DotProduct(Dir, To);
    const float   DirFactor = FMath::Max(0.5f, (Dot + 1.0f) * 0.5f);

    const float Combat = (ReplicatedActiveWeaponSlot != INDEX_NONE) ? 1.5f : 1.0f;

    // Игровое состояние владельца (без шаблонного Cast<>)
    const APawn* InstigatorPawn = OwnerActor->GetInstigator(); // APawn*
    const AController* InstigatorController = OwnerActor->GetInstigatorController();

    const APlayerState* OwnerState =
        (InstigatorPawn && InstigatorPawn->GetPlayerState())
            ? InstigatorPawn->GetPlayerState()
            : (InstigatorController ? InstigatorController->PlayerState.Get() : nullptr);

    // Состояние зрителя
    const APlayerState* ViewState = ViewTarget->GetPlayerState<APlayerState>();

    // Сам себе всегда релевантен
    if (OwnerState && ViewState && ViewState == OwnerState)
    {
        return 1.0f;
    }

    float Team = 1.0f;
    if (OwnerState && ViewState)
    {
        // Тут можно внедрить свою проверку "союзник/враг"
        Team = 1.2f;
    }

    float Rel = DistRel * Vis * DirFactor * Combat * Team;
    Rel *= FMath::Lerp(0.5f, 1.0f, CurrentNetworkQuality);
    return FMath::Clamp(Rel, 0.0f, 1.0f);
}


void UMedComEquipmentReplicationManager::UpdateSlotPriority(int32 SlotIndex)
{
    if(!SlotStates.IsValidIndex(SlotIndex)){return;}
    FSlotReplicationState& S=SlotStates[SlotIndex];
    S.ReplicationPriority=1.0f;
    if(DataProvider.GetInterface())
    {
        FEquipmentSlotConfig Cfg=DataProvider->GetSlotConfiguration(SlotIndex);
        if(Cfg.SlotType==EEquipmentSlotType::PrimaryWeapon||Cfg.SlotType==EEquipmentSlotType::SecondaryWeapon){S.ReplicationPriority*=2.0f;}
    }
    if(S.bIsDirty){S.ReplicationPriority*=3.0f;}
    if(S.ChangeCount>5){S.ReplicationPriority*=1.5f;}
    if(SlotIndex==ReplicatedActiveWeaponSlot){S.ReplicationPriority*=5.0f;}
}

bool UMedComEquipmentReplicationManager::SlotNeedsReplication(int32 SlotIndex,uint32 ClientVersion)const
{
    if(!SlotStates.IsValidIndex(SlotIndex)){return false;}
    const FSlotReplicationState& S=SlotStates[SlotIndex];
    if(S.bIsDirty){return true;}
    if(S.LastReplicatedVersion>ClientVersion){return true;}
    return false;
}

uint32 UMedComEquipmentReplicationManager::CalculateChecksum(const FReplicatedEquipmentData& Data)const
{
    TArray<uint8> Bytes;
    Bytes.Reset(256);
    FMemoryWriter W(Bytes, /*bIsPersistent=*/true);

    auto WriteStructArray = [&W](UScriptStruct* Struct, TArray<FInventoryItemInstance>& Array)
    {
        int32 Count = Array.Num();
        W << Count;
        for(int32 i=0;i<Count;i++)
        {
            Struct->SerializeItem(W, &Array[i], nullptr);
        }
    };

    // SlotInstances (через копию, чтобы не снимать const)
    {
        TArray<FInventoryItemInstance> Copy = Data.SlotInstances;
        WriteStructArray(FInventoryItemInstance::StaticStruct(), Copy);
    }

    // ActiveWeaponSlot
    {
        int32 Copy = Data.ActiveWeaponSlot;
        W << Copy;
    }

    // CurrentState
    {
        FGameplayTag TagCopy = Data.CurrentState;
        FGameplayTag::StaticStruct()->SerializeItem(W, &TagCopy, nullptr);
    }

    // ReplicationVersion
    {
        uint32 Copy = Data.ReplicationVersion;
        W << Copy;
    }

    // LastUpdateTime
    {
        float Copy = Data.LastUpdateTime;
        W << Copy;
    }

    return FCrc::MemCrc32(Bytes.GetData(), Bytes.Num());
}


void UMedComEquipmentReplicationManager::OnDataChanged(int32 SlotIndex,const FInventoryItemInstance& NewData)
{
    MarkForReplication(SlotIndex,false);
    UE_LOG(LogEquipmentReplication,Verbose,TEXT("OnDataChanged: Slot %d changed"),SlotIndex);
}

void UMedComEquipmentReplicationManager::CleanupClientStates()
{
    FScopeLock Lock(&ClientStateLock);
    ClientStates.RemoveAll([](const FClientReplicationState& S){return !S.Client||!S.Client->GetNetConnection();});
    for(FClientReplicationState& S:ClientStates)
    {
        if(S.Client && S.Client->GetNetConnection())
        {
            const float Ping=S.Client->GetNetConnection()->AvgLag;
            S.ConnectionQuality=FMath::Clamp(1.0f-(Ping/500.0f),0.0f,1.0f);
            S.AverageLatency=Ping*1000.0f;
            if(Ping>200.0f){S.PacketLossCount++;}
        }
    }
}

void UMedComEquipmentReplicationManager::UpdateStatistics(int32 BytesSent,bool bWasDelta)
{
    Statistics.TotalUpdates++;
    if(bWasDelta){Statistics.DeltaUpdates++;}else{Statistics.FullUpdates++;}
    Statistics.BytesSent+=BytesSent;
}

void UMedComEquipmentReplicationManager::AdaptReplicationStrategy(float NetworkQuality)
{
    if(NetworkQuality<0.3f){SetUpdateRate(5.0f);DynamicMaxDeltasBeforeFull=20;UE_LOG(LogEquipmentReplication,Log,TEXT("AdaptReplicationStrategy: poor network"));}
    else if(NetworkQuality<0.7f){SetUpdateRate(10.0f);DynamicMaxDeltasBeforeFull=MaxDeltasBeforeFull;UE_LOG(LogEquipmentReplication,Log,TEXT("AdaptReplicationStrategy: medium network"));}
    else{SetUpdateRate(20.0f);DynamicMaxDeltasBeforeFull=5;UE_LOG(LogEquipmentReplication,Log,TEXT("AdaptReplicationStrategy: good network"));}
}
