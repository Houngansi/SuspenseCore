// MedComEquipmentPredictionSystem.cpp
// Copyright Suspense Team. All Rights Reserved.
#include "SuspenseCore/Components/Network/SuspenseCoreEquipmentPredictionSystem.h"
#include "SuspenseCore/Components/Network/SuspenseCoreEquipmentNetworkDispatcher.h"
#include "SuspenseCore/Components/Network/SuspenseCoreEquipmentReplicationManager.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceMacros.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"

USuspenseCoreEquipmentPredictionSystem::USuspenseCoreEquipmentPredictionSystem()
{
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.TickInterval=0.033f;
    ConfidenceMetrics.ConfidenceLevel=1.0f;
    ConfidenceMetrics.SuccessRate=1.0f;
}

void USuspenseCoreEquipmentPredictionSystem::BeginPlay()
{
    Super::BeginPlay();
    if(GetOwnerRole()==ROLE_Authority)
    {
        bPredictionEnabled=false;
        SetComponentTickEnabled(false);
        UE_LOG(LogEquipmentPrediction,Log,TEXT("PredictionSystem: Disabled on server"));
        return;
    }
    SubscribeToNetworkEvents();
    UE_LOG(LogEquipmentPrediction,Log,TEXT("PredictionSystem: Initialized for client prediction"));
}

void USuspenseCoreEquipmentPredictionSystem::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UnsubscribeFromNetworkEvents();
    {
        FScopeLock Lock(&PredictionLock);
        for(const FEquipmentPrediction& P:ActivePredictions)
        {
            if(!P.bConfirmed && !P.bRolledBack){RollbackPrediction(P.PredictionId,FText::FromString(TEXT("System shutdown")));}
        }
        ActivePredictions.Empty();
        OperationToPredictionMap.Empty();
    }
    {
        FScopeLock Lock(&TimelineLock);
        PredictionTimeline.Empty();
    }
    Super::EndPlay(EndPlayReason);
}

void USuspenseCoreEquipmentPredictionSystem::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime,TickType,ThisTickFunction);
    if(!bPredictionEnabled){return;}
    if(bUseAdaptiveConfidence)
    {
        ConfidenceMetrics.TimeSinceLastFailure+=DeltaTime;
        if(ConfidenceMetrics.TimeSinceLastFailure>1.0f)
        {
            const float RecoveryRate=0.1f*DeltaTime;
            ConfidenceMetrics.ConfidenceLevel=FMath::Min(ConfidenceMetrics.SuccessRate,ConfidenceMetrics.ConfidenceLevel+RecoveryRate);
        }
    }
    const float Now=GetWorld()?GetWorld()->GetTimeSeconds():0.0f;
    {
        FScopeLock Lock(&PredictionLock);
        for(FEquipmentPrediction& P:ActivePredictions)
        {
            if(!P.bConfirmed && !P.bRolledBack)
            {
                const float Age=Now-P.PredictionTime;
                if(Age>PredictionTimeout){HandlePredictionTimeout(P.PredictionId);}
            }
        }
    }
    static float LastCleanupTime=0.0f;
    if(Now-LastCleanupTime>1.0f)
    {
        ClearExpiredPredictions();
        CleanupTimeline();
        LastCleanupTime=Now;
    }
}

FGuid USuspenseCoreEquipmentPredictionSystem::CreatePrediction(const FEquipmentOperationRequest& Operation)
{
    if(!bPredictionEnabled || GetOwnerRole()==ROLE_Authority){return FGuid();}
    if(!ShouldAllowPrediction(Operation))
    {
        UE_LOG(LogEquipmentPrediction,Verbose,TEXT("CreatePrediction: denied for %s"),*UEnum::GetValueAsString(Operation.OperationType));
        return FGuid();
    }
    FScopeLock Lock(&PredictionLock);
    if(ActivePredictions.Num()>=MaxActivePredictions)
    {
        UE_LOG(LogEquipmentPrediction,Warning,TEXT("CreatePrediction: limit reached %d"),MaxActivePredictions);
        return FGuid();
    }
    FEquipmentPrediction NewPrediction;
    NewPrediction.PredictionId=FGuid::NewGuid();
    NewPrediction.Operation=Operation;
    NewPrediction.PredictionTime=GetWorld()?GetWorld()->GetTimeSeconds():0.0f;
    if(DataProvider.GetInterface()){NewPrediction.StateBefore=DataProvider->CreateSnapshot();}
    if(ExecutePredictionLocally(NewPrediction))
    {
        if(DataProvider.GetInterface()){NewPrediction.PredictedState=DataProvider->CreateSnapshot();}
        ActivePredictions.Add(NewPrediction);
        if(Operation.OperationId.IsValid()){OperationToPredictionMap.Add(Operation.OperationId,NewPrediction.PredictionId);}
        FSuspenseCorePredictionTimelineEntry E;E.PredictionId=NewPrediction.PredictionId;E.Timestamp=NewPrediction.PredictionTime;E.ServerTimestamp=LastServerUpdateTime;E.StateChange=NewPrediction.PredictedState;E.Confidence=GetAdjustedConfidence(Operation.OperationType);
        AddToTimeline(E);
        {
            FScopeLock StatsLock(&StatisticsLock);
            Statistics.ActivePredictions=ActivePredictions.Num();
            Statistics.TotalCreated++;
        }
        OnPredictionCreated.Broadcast(NewPrediction.PredictionId);
        LogPredictionEvent(TEXT("Created"),NewPrediction.PredictionId);
        return NewPrediction.PredictionId;
    }
    UE_LOG(LogEquipmentPrediction,Warning,TEXT("CreatePrediction: local execution failed"));
    return FGuid();
}

bool USuspenseCoreEquipmentPredictionSystem::ApplyPrediction(const FGuid& PredictionId)
{
    FScopeLock Lock(&PredictionLock);
    FEquipmentPrediction* P=ActivePredictions.FindByPredicate([&PredictionId](const FEquipmentPrediction& X){return X.PredictionId==PredictionId;});
    if(!P){UE_LOG(LogEquipmentPrediction,Warning,TEXT("ApplyPrediction: not found %s"),*PredictionId.ToString());return false;}
    if(DataProvider.GetInterface())
    {
        const bool bOk=DataProvider->RestoreSnapshot(P->PredictedState);
        if(bOk){LogPredictionEvent(TEXT("Applied"),PredictionId);}
        return bOk;
    }
    return false;
}

bool USuspenseCoreEquipmentPredictionSystem::ConfirmPrediction(const FGuid& PredictionId,const FEquipmentOperationResult& ServerResult)
{
    FScopeLock Lock(&PredictionLock);
    const int32 Index=ActivePredictions.IndexOfByPredicate([&PredictionId](const FEquipmentPrediction& X){return X.PredictionId==PredictionId;});
    if(Index==INDEX_NONE)
    {
        UE_LOG(LogEquipmentPrediction,Verbose,TEXT("ConfirmPrediction: %s not found"),*PredictionId.ToString());
        return false;
    }
    FEquipmentPrediction& P=ActivePredictions[Index];
    const bool bValid=ValidatePrediction(P,ServerResult);
    if(bValid)
    {
        P.bConfirmed=true;
        UpdateConfidence(true);
        if(FSuspenseCorePredictionTimelineEntry* T=FindTimelineEntry(PredictionId)){T->bConfirmed=true;}
        {
            FScopeLock StatsLock(&StatisticsLock);
            Statistics.TotalConfirmed++;
            Statistics.PredictionAccuracy=(float)Statistics.TotalConfirmed/FMath::Max(1,Statistics.TotalCreated);
        }
        const float Lat=GetWorld()?GetWorld()->GetTimeSeconds()-P.PredictionTime:0.0f;
        UpdateLatencyTracking(Lat);
        OnPredictionConfirmed.Broadcast(PredictionId);
        LogPredictionEvent(TEXT("Confirmed"),PredictionId);
    }
    else
    {
        UE_LOG(LogEquipmentPrediction,Warning,TEXT("ConfirmPrediction: mismatch %s"),*PredictionId.ToString());
        RollbackPrediction(PredictionId,FText::FromString(TEXT("Server result mismatch")));
    }
    ActivePredictions.RemoveAt(Index);
    Statistics.ActivePredictions=ActivePredictions.Num();
    for(auto It=OperationToPredictionMap.CreateIterator();It;++It){if(It.Value()==PredictionId){It.RemoveCurrent();break;}}
    return bValid;
}

bool USuspenseCoreEquipmentPredictionSystem::RollbackPrediction(const FGuid& PredictionId,const FText& Reason)
{
    FScopeLock Lock(&PredictionLock);
    const int32 Index=ActivePredictions.IndexOfByPredicate([&PredictionId](const FEquipmentPrediction& X){return X.PredictionId==PredictionId;});
    if(Index==INDEX_NONE){return false;}
    FEquipmentPrediction& P=ActivePredictions[Index];
    if(P.bRolledBack){return true;}
    const bool bOk=RewindPrediction(P);
    if(bOk)
    {
        P.bRolledBack=true;
        UpdateConfidence(false);
        {
            FScopeLock StatsLock(&StatisticsLock);
            Statistics.TotalRolledBack++;
        }
        OnPredictionRolledBack.Broadcast(PredictionId,Reason);
        LogPredictionEvent(FString::Printf(TEXT("Rolled back: %s"),*Reason.ToString()),PredictionId);
        TArray<FEquipmentPrediction> Later;
        for(const FEquipmentPrediction& O:ActivePredictions)
        {
            if(O.PredictionTime>P.PredictionTime && !O.bRolledBack && O.PredictionId!=PredictionId){Later.Add(O);}
        }
        if(Later.Num()>0)
        {
            const int32 Reapplied=ReapplyPredictions(Later);
            UE_LOG(LogEquipmentPrediction,Verbose,TEXT("RollbackPrediction: reapplied %d"),Reapplied);
        }
    }
    else{UE_LOG(LogEquipmentPrediction,Error,TEXT("RollbackPrediction: rewind failed %s"),*PredictionId.ToString());}
    return bOk;
}

void USuspenseCoreEquipmentPredictionSystem::ReconcileWithServer(const FEquipmentStateSnapshot& ServerState)
{
    if(!bPredictionEnabled || !DataProvider.GetInterface()){return;}
    ReconciliationState.ServerState=ServerState;
    ReconciliationState.bInProgress=true;
    ReconciliationState.StartTime=GetWorld()?GetWorld()->GetTimeSeconds():0.0f;
    ReconciliationState.ReconciliationCount++;
    OnReconciliationStarted.Broadcast();
    UE_LOG(LogEquipmentPrediction,Log,TEXT("ReconcileWithServer: start #%d"),ReconciliationState.ReconciliationCount);
    FScopeLock Lock(&PredictionLock);
    ReconciliationState.PendingReapplication.Empty();
    for(const FEquipmentPrediction& P:ActivePredictions){if(!P.bConfirmed && !P.bRolledBack){ReconciliationState.PendingReapplication.Add(P);}}
    DataProvider->RestoreSnapshot(ServerState);
    LastServerUpdateTime=GetWorld()?GetWorld()->GetTimeSeconds():0.0f;
    int32 Reapplied=0;
    if(ReconciliationState.PendingReapplication.Num()>0)
    {
        ReconciliationState.PendingReapplication.Sort([](const FEquipmentPrediction& A,const FEquipmentPrediction& B){return A.PredictionTime<B.PredictionTime;});
        if(bSmoothReconciliation)
        {
            for(const FEquipmentPrediction& P:ReconciliationState.PendingReapplication)
            {
                if(ShouldAllowPrediction(P.Operation))
                {
                    FEquipmentPrediction Tmp=P;
                    if(ExecutePredictionLocally(Tmp)){Reapplied++;}
                }
            }
        }
        else{Reapplied=ReapplyPredictions(ReconciliationState.PendingReapplication);}
    }
    {
        FScopeLock StatsLock(&StatisticsLock);
        Statistics.ReconciliationCount++;
    }
    ReconciliationState.bInProgress=false;
    ReconciliationState.PendingReapplication.Empty();
    OnReconciliationCompleted.Broadcast(Reapplied);
    UE_LOG(LogEquipmentPrediction,Log,TEXT("ReconcileWithServer: done, reapplied %d"),Reapplied);
}

TArray<FEquipmentPrediction> USuspenseCoreEquipmentPredictionSystem::GetActivePredictions() const
{
    FScopeLock Lock(&PredictionLock);
    return ActivePredictions;
}

int32 USuspenseCoreEquipmentPredictionSystem::ClearExpiredPredictions(float MaxAge)
{
    if(!GetWorld()){return 0;}
    const float Now=GetWorld()->GetTimeSeconds();
    FScopeLock Lock(&PredictionLock);
    TArray<FGuid> Expired;
    for(const FEquipmentPrediction& P:ActivePredictions){if((Now-P.PredictionTime)>MaxAge){Expired.Add(P.PredictionId);}}
    int32 Removed=0;
    for(const FGuid& Id:Expired)
    {
        ActivePredictions.RemoveAll([&Id](const FEquipmentPrediction& X){return X.PredictionId==Id;});
        for(auto It=OperationToPredictionMap.CreateIterator();It;++It){if(It.Value()==Id){It.RemoveCurrent();break;}}
        Removed++;
    }
    if(Removed>0)
    {
        Statistics.ActivePredictions=ActivePredictions.Num();
        UE_LOG(LogEquipmentPrediction,Verbose,TEXT("ClearExpiredPredictions: removed %d"),Removed);
    }
    return Removed;
}

bool USuspenseCoreEquipmentPredictionSystem::IsPredictionActive(const FGuid& PredictionId) const
{
    FScopeLock Lock(&PredictionLock);
    return ActivePredictions.ContainsByPredicate([&PredictionId](const FEquipmentPrediction& X){return X.PredictionId==PredictionId;});
}

float USuspenseCoreEquipmentPredictionSystem::GetPredictionConfidence(const FGuid& PredictionId) const
{
    FScopeLock Lock(&PredictionLock);
    const FEquipmentPrediction* P=ActivePredictions.FindByPredicate([&PredictionId](const FEquipmentPrediction& X){return X.PredictionId==PredictionId;});
    if(!P || !GetWorld()){return 0.0f;}
    float C=ConfidenceMetrics.ConfidenceLevel;
    const float Age=GetWorld()->GetTimeSeconds()-P->PredictionTime;
    const float AgePenalty=FMath::Clamp(Age/PredictionTimeout,0.0f,1.0f);
    C*=(1.0f-AgePenalty*0.5f);
    if(Statistics.AverageLatency>0.1f)
    {
        const float LatPenalty=FMath::Clamp(Statistics.AverageLatency/0.5f,0.0f,1.0f);
        C*=(1.0f-LatPenalty*0.3f);
    }
    return FMath::Clamp(C,0.0f,1.0f);
}

void USuspenseCoreEquipmentPredictionSystem::SetPredictionEnabled(bool bEnabled)
{
    bPredictionEnabled=bEnabled;
    if(!bEnabled)
    {
        FScopeLock Lock(&PredictionLock);
        for(const FEquipmentPrediction& P:ActivePredictions){if(!P.bConfirmed && !P.bRolledBack){RollbackPrediction(P.PredictionId,FText::FromString(TEXT("Prediction disabled")));}}
        ActivePredictions.Empty();
        OperationToPredictionMap.Empty();
        Statistics.ActivePredictions=0;
    }
    UE_LOG(LogEquipmentPrediction,Log,TEXT("SetPredictionEnabled: %s"),bEnabled?TEXT("enabled"):TEXT("disabled"));
}

bool USuspenseCoreEquipmentPredictionSystem::Initialize(TScriptInterface<ISuspenseCoreEquipmentDataProvider> InDataProvider,TScriptInterface<ISuspenseCoreEquipmentOperations> InOperationExecutor)
{
    if(!InDataProvider.GetInterface()||!InOperationExecutor.GetInterface())
    {
        UE_LOG(LogEquipmentPrediction,Error,TEXT("Initialize: invalid deps"));
        return false;
    }
    DataProvider=InDataProvider;
    OperationExecutor=InOperationExecutor;
    ResetPredictionSystem();
    UE_LOG(LogEquipmentPrediction,Log,TEXT("Initialize: ok"));
    return true;
}

void USuspenseCoreEquipmentPredictionSystem::SetNetworkDispatcher(USuspenseCoreEquipmentNetworkDispatcher* InDispatcher)
{
    if(NetworkDispatcher)
    {
        NetworkDispatcher->OnServerResponse.RemoveAll(this);
        NetworkDispatcher->OnOperationTimeout.RemoveAll(this);
    }
    NetworkDispatcher=InDispatcher;
    if(HasBegunPlay()){SubscribeToNetworkEvents();}
    UE_LOG(LogEquipmentPrediction,Log,TEXT("SetNetworkDispatcher: updated"));
}

void USuspenseCoreEquipmentPredictionSystem::SetReplicationManager(USuspenseCoreEquipmentReplicationManager* InReplicationManager)
{
    if(ReplicationManager){ReplicationManager->OnReplicatedStateApplied.RemoveAll(this);}
    ReplicationManager=InReplicationManager;
    if(HasBegunPlay()){SubscribeToNetworkEvents();}
    UE_LOG(LogEquipmentPrediction,Log,TEXT("SetReplicationManager: updated"));
}

void USuspenseCoreEquipmentPredictionSystem::ResetPredictionSystem()
{
    {
        FScopeLock Lock(&PredictionLock);
        ActivePredictions.Empty();
        OperationToPredictionMap.Empty();
    }
    {
        FScopeLock Lock(&TimelineLock);
        PredictionTimeline.Empty();
    }
    ConfidenceMetrics=FSuspenseCorePredictionConfidenceMetrics();
    ConfidenceMetrics.ConfidenceLevel=1.0f;
    ConfidenceMetrics.SuccessRate=1.0f;
    Statistics=FSuspenseCorePredictionStatistics();
    ReconciliationState=FSuspenseCoreReconciliationState();
    LatencySamples.Empty();
    UE_LOG(LogEquipmentPrediction,Log,TEXT("ResetPredictionSystem: clean"));
}

void USuspenseCoreEquipmentPredictionSystem::HandleServerResponse(const FGuid& OperationId,const FEquipmentOperationResult& Result)
{
    FGuid PredictionId;
    {
        FScopeLock Lock(&PredictionLock);
        if(const FGuid* Found=OperationToPredictionMap.Find(OperationId)){PredictionId=*Found;}
        else if(Result.OperationId.IsValid())
        {
            if(const FGuid* Found2=OperationToPredictionMap.Find(Result.OperationId)){PredictionId=*Found2;}
        }
    }
    if(!PredictionId.IsValid())
    {
        UE_LOG(LogEquipmentPrediction,Verbose,TEXT("HandleServerResponse: no mapping for op=%s"),*OperationId.ToString());
        return;
    }
    if(Result.bSuccess){ConfirmPrediction(PredictionId,Result);}
    else
    {
        RollbackPrediction(PredictionId,Result.ErrorMessage.IsEmpty()?FText::FromString(TEXT("Server rejected operation")):Result.ErrorMessage);
        FScopeLock Lock(&PredictionLock);
        OperationToPredictionMap.Remove(OperationId);
    }
    UE_LOG(LogEquipmentPrediction,Verbose,TEXT("HandleServerResponse: processed op=%s"),*OperationId.ToString());
}

void USuspenseCoreEquipmentPredictionSystem::HandleOperationTimeout(const FGuid& OperationId)
{
    FScopeLock Lock(&PredictionLock);
    if(const FGuid* Pred=OperationToPredictionMap.Find(OperationId))
    {
        HandlePredictionTimeout(*Pred);
        OperationToPredictionMap.Remove(OperationId);
        UE_LOG(LogEquipmentPrediction,Warning,TEXT("HandleOperationTimeout: op=%s"),*OperationId.ToString());
    }
}

void USuspenseCoreEquipmentPredictionSystem::HandleReplicatedStateApplied(const FReplicatedEquipmentData& ReplicatedData)
{
    if(!DataProvider.GetInterface()){return;}
    FEquipmentStateSnapshot ServerState=DataProvider->CreateSnapshot();
    ReconcileWithServer(ServerState);
    UE_LOG(LogEquipmentPrediction,Verbose,TEXT("HandleReplicatedStateApplied: version %d"),ReplicatedData.ReplicationVersion);
}

bool USuspenseCoreEquipmentPredictionSystem::ExecutePredictionLocally(FEquipmentPrediction& Prediction)
{
    if(!OperationExecutor.GetInterface()){return false;}
    const FEquipmentOperationResult R=OperationExecutor->ExecuteOperation(Prediction.Operation);
    return R.bSuccess;
}

bool USuspenseCoreEquipmentPredictionSystem::RewindPrediction(const FEquipmentPrediction& Prediction)
{
    if(!DataProvider.GetInterface()){return false;}
    return DataProvider->RestoreSnapshot(Prediction.StateBefore);
}

int32 USuspenseCoreEquipmentPredictionSystem::ReapplyPredictions(const TArray<FEquipmentPrediction>& Predictions)
{
    int32 Count=0;
    for(const FEquipmentPrediction& P:Predictions)
    {
        FEquipmentPrediction Tmp=P;
        if(ExecutePredictionLocally(Tmp)){Count++;}
        else{UE_LOG(LogEquipmentPrediction,Warning,TEXT("ReapplyPredictions: failed %s"),*P.PredictionId.ToString());}
    }
    return Count;
}

void USuspenseCoreEquipmentPredictionSystem::UpdateConfidence(bool bSuccess)
{
    ConfidenceMetrics.UpdateMetrics(bSuccess);
    UE_LOG(LogEquipmentPrediction,Verbose,TEXT("UpdateConfidence: SR=%.2f C=%.2f"),ConfidenceMetrics.SuccessRate,ConfidenceMetrics.ConfidenceLevel);
}

bool USuspenseCoreEquipmentPredictionSystem::ShouldAllowPrediction(const FEquipmentOperationRequest& Operation) const
{
    if(GetOwnerRole()==ROLE_Authority){return false;}
    if(bUseAdaptiveConfidence)
    {
        const float C=GetAdjustedConfidence(Operation.OperationType);
        if(C<MinConfidenceThreshold){return false;}
    }
    if(Operation.OperationType==EEquipmentOperationType::QuickSwitch){return true;}
    const float P=CalculatePredictionPriority(Operation);
    return P>=0.5f;
}

float USuspenseCoreEquipmentPredictionSystem::CalculatePredictionPriority(const FEquipmentOperationRequest& Operation) const
{
    float P=0.5f;
    switch(Operation.OperationType)
    {
        case EEquipmentOperationType::QuickSwitch: P=1.0f; break;
        case EEquipmentOperationType::Equip:
        case EEquipmentOperationType::Unequip: P=0.8f; break;
        case EEquipmentOperationType::Swap:
        case EEquipmentOperationType::Move: P=0.6f; break;
        case EEquipmentOperationType::Drop: P=0.4f; break;
        default: P=0.5f; break;
    }
    P*=ConfidenceMetrics.ConfidenceLevel;
    return FMath::Clamp(P,0.0f,1.0f);
}

void USuspenseCoreEquipmentPredictionSystem::AddToTimeline(const FSuspenseCorePredictionTimelineEntry& Entry)
{
    FScopeLock Lock(&TimelineLock);
    PredictionTimeline.Add(Entry);
    if(PredictionTimeline.Num()>MaxTimelineEntries){PredictionTimeline.RemoveAt(0);}
}

FSuspenseCorePredictionTimelineEntry* USuspenseCoreEquipmentPredictionSystem::FindTimelineEntry(const FGuid& PredictionId)
{
    FScopeLock Lock(&TimelineLock);
    return PredictionTimeline.FindByPredicate([&PredictionId](const FSuspenseCorePredictionTimelineEntry& E){return E.PredictionId==PredictionId;});
}

void USuspenseCoreEquipmentPredictionSystem::CleanupTimeline()
{
    if(!GetWorld()){return;}
    const float Now=GetWorld()->GetTimeSeconds();
    const float MaxAge=PredictionTimeout*2.0f;
    FScopeLock Lock(&TimelineLock);
    PredictionTimeline.RemoveAll([Now,MaxAge](const FSuspenseCorePredictionTimelineEntry& E){return (Now-E.Timestamp)>MaxAge;});
}

bool USuspenseCoreEquipmentPredictionSystem::ValidatePrediction(const FEquipmentPrediction& Prediction,const FEquipmentOperationResult& ServerResult) const
{
    if(!ServerResult.bSuccess){return false;}
    return true;
}

void USuspenseCoreEquipmentPredictionSystem::HandlePredictionTimeout(const FGuid& PredictionId)
{
    UE_LOG(LogEquipmentPrediction,Warning,TEXT("HandlePredictionTimeout: %s"),*PredictionId.ToString());
    RollbackPrediction(PredictionId,FText::FromString(TEXT("Timeout")));
    UpdateConfidence(false);
}

void USuspenseCoreEquipmentPredictionSystem::UpdateLatencyTracking(float Latency)
{
    LatencySamples.Add(Latency);
    if(LatencySamples.Num()>MaxLatencySamples){LatencySamples.RemoveAt(0);}
    float Sum=0.0f;for(float S:LatencySamples){Sum+=S;}
    FScopeLock Lock(&StatisticsLock);
    Statistics.AverageLatency=LatencySamples.Num()>0?Sum/(float)LatencySamples.Num():0.0f;
}

float USuspenseCoreEquipmentPredictionSystem::GetAdjustedConfidence(EEquipmentOperationType OperationType) const
{
    const float Base=ConfidenceMetrics.ConfidenceLevel;
    switch(OperationType)
    {
        case EEquipmentOperationType::QuickSwitch: return FMath::Max(0.8f,Base);
        case EEquipmentOperationType::Drop: return Base*0.7f;
        default: return Base;
    }
}

void USuspenseCoreEquipmentPredictionSystem::LogPredictionEvent(const FString& Event,const FGuid& PredictionId) const
{
    UE_LOG(LogEquipmentPrediction,Verbose,TEXT("[%s] Prediction %s C=%.2f Active=%d"),*Event,*PredictionId.ToString(),ConfidenceMetrics.ConfidenceLevel,Statistics.ActivePredictions);
}

void USuspenseCoreEquipmentPredictionSystem::SubscribeToNetworkEvents()
{
    if(NetworkDispatcher)
    {
        NetworkDispatcher->OnServerResponse.RemoveAll(this);
        NetworkDispatcher->OnOperationTimeout.RemoveAll(this);
        NetworkDispatcher->OnServerResponse.AddUObject(this,&USuspenseCoreEquipmentPredictionSystem::HandleServerResponse);
        NetworkDispatcher->OnOperationTimeout.AddUObject(this,&USuspenseCoreEquipmentPredictionSystem::HandleOperationTimeout);
    }
    if(ReplicationManager)
    {
        ReplicationManager->OnReplicatedStateApplied.RemoveAll(this);
        ReplicationManager->OnReplicatedStateApplied.AddUObject(this,&USuspenseCoreEquipmentPredictionSystem::HandleReplicatedStateApplied);
    }
}

void USuspenseCoreEquipmentPredictionSystem::UnsubscribeFromNetworkEvents()
{
    if(NetworkDispatcher)
    {
        NetworkDispatcher->OnServerResponse.RemoveAll(this);
        NetworkDispatcher->OnOperationTimeout.RemoveAll(this);
    }
    if(ReplicationManager){ReplicationManager->OnReplicatedStateApplied.RemoveAll(this);}
}
