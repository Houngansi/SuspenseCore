// SuspenseEquipmentPredictionSystem.h
// Copyright Suspense Team. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/Equipment/ISuspensePredictionManager.h"
#include "Interfaces/Equipment/ISuspenseEquipmentDataProvider.h"
#include "Interfaces/Equipment/ISuspenseEquipmentOperations.h"
#include "Interfaces/Equipment/ISuspenseReplicationProvider.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "GameplayTagContainer.h"
#include "SuspenseEquipmentPredictionSystem.generated.h"

class USuspenseEquipmentNetworkDispatcher;
class USuspenseEquipmentReplicationManager;

USTRUCT()
struct FPredictionTimelineEntry
{
    GENERATED_BODY()
    UPROPERTY() FGuid PredictionId;
    UPROPERTY() float Timestamp=0.0f;
    UPROPERTY() float ServerTimestamp=0.0f;
    UPROPERTY() FEquipmentStateSnapshot StateChange;
    UPROPERTY() bool bConfirmed=false;
    UPROPERTY() float Confidence=1.0f;
};

USTRUCT()
struct FPredictionConfidenceMetrics
{
    GENERATED_BODY()
    UPROPERTY() int32 TotalPredictions=0;
    UPROPERTY() int32 SuccessfulPredictions=0;
    UPROPERTY() int32 FailedPredictions=0;
    UPROPERTY() float SuccessRate=1.0f;
    UPROPERTY() float ConfidenceLevel=1.0f;
    UPROPERTY() float TimeSinceLastFailure=0.0f;
    void UpdateMetrics(bool bSuccess)
    {
        TotalPredictions++;
        if(bSuccess){SuccessfulPredictions++;TimeSinceLastFailure+=0.1f;}
        else{FailedPredictions++;TimeSinceLastFailure=0.0f;}
        const float Alpha=0.1f;
        const float InstantRate=bSuccess?1.0f:0.0f;
        SuccessRate=Alpha*InstantRate+(1.0f-Alpha)*SuccessRate;
        ConfidenceLevel=FMath::Clamp(SuccessRate*FMath::Min(1.0f,TimeSinceLastFailure/5.0f),0.1f,1.0f);
    }
};

USTRUCT()
struct FReconciliationState
{
    GENERATED_BODY()
    UPROPERTY() FEquipmentStateSnapshot ServerState;
    UPROPERTY() TArray<FEquipmentPrediction> PendingReapplication;
    UPROPERTY() bool bInProgress=false;
    UPROPERTY() float StartTime=0.0f;
    UPROPERTY() int32 ReconciliationCount=0;
};

USTRUCT(BlueprintType)
struct FPredictionStatistics
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 ActivePredictions=0;
    UPROPERTY(BlueprintReadOnly) int32 TotalCreated=0;
    UPROPERTY(BlueprintReadOnly) int32 TotalConfirmed=0;
    UPROPERTY(BlueprintReadOnly) int32 TotalRolledBack=0;
    UPROPERTY(BlueprintReadOnly) int32 ReconciliationCount=0;
    UPROPERTY(BlueprintReadOnly) float AverageLatency=0.0f;
    UPROPERTY(BlueprintReadOnly) float PredictionAccuracy=1.0f;
};

UCLASS(ClassGroup=(Equipment),meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseEquipmentPredictionSystem : public UActorComponent, public ISuspensePredictionManager
{
    GENERATED_BODY()
public:
    USuspenseEquipmentPredictionSystem();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;

    virtual FGuid CreatePrediction(const FEquipmentOperationRequest& Operation) override;
    virtual bool ApplyPrediction(const FGuid& PredictionId) override;
    virtual bool ConfirmPrediction(const FGuid& PredictionId,const FEquipmentOperationResult& ServerResult) override;
    virtual bool RollbackPrediction(const FGuid& PredictionId,const FText& Reason=FText::GetEmpty()) override;
    virtual void ReconcileWithServer(const FEquipmentStateSnapshot& ServerState) override;
    virtual TArray<FEquipmentPrediction> GetActivePredictions() const override;
    virtual int32 ClearExpiredPredictions(float MaxAge=2.0f) override;
    virtual bool IsPredictionActive(const FGuid& PredictionId) const override;
    virtual float GetPredictionConfidence(const FGuid& PredictionId) const override;
    virtual void SetPredictionEnabled(bool bEnabled) override;

    UFUNCTION(BlueprintCallable,Category="SuspenseCore|Equipment|Prediction")
    bool Initialize(TScriptInterface<ISuspenseEquipmentDataProvider> InDataProvider,TScriptInterface<ISuspenseEquipmentOperations> InOperationExecutor);
    UFUNCTION(BlueprintCallable,Category="SuspenseCore|Equipment|Prediction")
    void SetNetworkDispatcher(USuspenseEquipmentNetworkDispatcher* InDispatcher);
    UFUNCTION(BlueprintCallable,Category="SuspenseCore|Equipment|Prediction")
    void SetReplicationManager(USuspenseEquipmentReplicationManager* InReplicationManager);
    UFUNCTION(BlueprintCallable,Category="SuspenseCore|Equipment|Prediction")
    void SetMaxActivePredictions(int32 MaxPredictions){MaxActivePredictions=FMath::Max(1,MaxPredictions);}
    UFUNCTION(BlueprintCallable,Category="SuspenseCore|Equipment|Prediction")
    void SetPredictionTimeout(float Timeout){PredictionTimeout=FMath::Max(0.1f,Timeout);}
    UFUNCTION(BlueprintCallable,Category="SuspenseCore|Equipment|Prediction")
    FPredictionStatistics GetStatistics() const {return Statistics;}
    //UFUNCTION(BlueprintCallable,Category="SuspenseCore|Equipment|Prediction")
    FPredictionConfidenceMetrics GetConfidenceMetrics() const {return ConfidenceMetrics;}
    UFUNCTION(BlueprintCallable,Category="SuspenseCore|Equipment|Prediction")
    void ResetPredictionSystem();

    DECLARE_MULTICAST_DELEGATE_OneParam(FOnPredictionCreated,const FGuid&);
    FOnPredictionCreated OnPredictionCreated;
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnPredictionConfirmed,const FGuid&);
    FOnPredictionConfirmed OnPredictionConfirmed;
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPredictionRolledBack,const FGuid&,const FText&);
    FOnPredictionRolledBack OnPredictionRolledBack;
    DECLARE_MULTICAST_DELEGATE(FOnReconciliationStarted);
    FOnReconciliationStarted OnReconciliationStarted;
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnReconciliationCompleted,int32);
    FOnReconciliationCompleted OnReconciliationCompleted;

protected:
    UFUNCTION() void HandleServerResponse(const FGuid& OperationId,const FEquipmentOperationResult& Result);
    UFUNCTION() void HandleOperationTimeout(const FGuid& OperationId);
    UFUNCTION() void HandleReplicatedStateApplied(const FReplicatedEquipmentData& ReplicatedData);

    bool ExecutePredictionLocally(FEquipmentPrediction& Prediction);
    bool RewindPrediction(const FEquipmentPrediction& Prediction);
    int32 ReapplyPredictions(const TArray<FEquipmentPrediction>& Predictions);
    void UpdateConfidence(bool bSuccess);
    bool ShouldAllowPrediction(const FEquipmentOperationRequest& Operation) const;
    float CalculatePredictionPriority(const FEquipmentOperationRequest& Operation) const;
    void AddToTimeline(const FPredictionTimelineEntry& Entry);
    FPredictionTimelineEntry* FindTimelineEntry(const FGuid& PredictionId);
    void CleanupTimeline();
    bool ValidatePrediction(const FEquipmentPrediction& Prediction,const FEquipmentOperationResult& ServerResult) const;
    void HandlePredictionTimeout(const FGuid& PredictionId);
    void UpdateLatencyTracking(float Latency);
    float GetAdjustedConfidence(EEquipmentOperationType OperationType) const;
    void LogPredictionEvent(const FString& Event,const FGuid& PredictionId) const;
    void SubscribeToNetworkEvents();
    void UnsubscribeFromNetworkEvents();

private:
    UPROPERTY(EditDefaultsOnly,Category="Prediction|Config") int32 MaxActivePredictions=10;
    UPROPERTY(EditDefaultsOnly,Category="Prediction|Config") float PredictionTimeout=2.0f;
    UPROPERTY(EditDefaultsOnly,Category="Prediction|Config") int32 MaxTimelineEntries=100;
    UPROPERTY(EditDefaultsOnly,Category="Prediction|Config") bool bUseAdaptiveConfidence=true;
    UPROPERTY(EditDefaultsOnly,Category="Prediction|Config") float MinConfidenceThreshold=0.3f;
    UPROPERTY(EditDefaultsOnly,Category="Prediction|Config") bool bSmoothReconciliation=true;

    UPROPERTY() TScriptInterface<ISuspenseEquipmentDataProvider> DataProvider;
    UPROPERTY() TScriptInterface<ISuspenseEquipmentOperations> OperationExecutor;
    UPROPERTY() USuspenseEquipmentNetworkDispatcher* NetworkDispatcher=nullptr;
    UPROPERTY() USuspenseEquipmentReplicationManager* ReplicationManager=nullptr;

    UPROPERTY() TArray<FEquipmentPrediction> ActivePredictions;
    UPROPERTY() TMap<FGuid,FGuid> OperationToPredictionMap;
    UPROPERTY() TArray<FPredictionTimelineEntry> PredictionTimeline;
    UPROPERTY() FReconciliationState ReconciliationState;
    UPROPERTY() FPredictionConfidenceMetrics ConfidenceMetrics;
    UPROPERTY() FPredictionStatistics Statistics;

    UPROPERTY() bool bPredictionEnabled=true;
    float LastServerUpdateTime=0.0f;
    TArray<float> LatencySamples;
    static constexpr int32 MaxLatencySamples=20;

    mutable FCriticalSection PredictionLock;
    mutable FCriticalSection TimelineLock;
    mutable FCriticalSection StatisticsLock;
};
