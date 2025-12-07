// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Types/Network/SuspenseNetworkTypes.h"
#include "Types/Equipment/SuspenseEquipmentTypes.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Net/UnrealNetwork.h"
#include "ISuspenseCoreNetworkInterfaces.generated.h"

//========================================
// NETWORK DISPATCHER INTERFACE
//========================================

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreNetworkDispatcher : public UInterface
{
    GENERATED_BODY()
};

/**
 * SuspenseCore Network Dispatcher Interface
 *
 * Central point for all network operations.
 * Handles RPC calls, batching, and reliability.
 */
class EQUIPMENTSYSTEM_API ISuspenseCoreNetworkDispatcher
{
    GENERATED_BODY()

public:
    /** Send operation to server */
    virtual FGuid SendOperationToServer(const FNetworkOperationRequest& Request) = 0;

    /** Send operation to clients */
    virtual void SendOperationToClients(
        const FNetworkOperationRequest& Request,
        const TArray<APlayerController*>& TargetClients) = 0;

    /** Handle server response */
    virtual void HandleServerResponse(const FNetworkOperationResponse& Response) = 0;

    /** Batch multiple operations */
    virtual FGuid BatchOperations(const TArray<FNetworkOperationRequest>& Operations) = 0;

    /** Cancel pending operation */
    virtual bool CancelOperation(const FGuid& RequestId) = 0;

    /** Retry failed operation */
    virtual bool RetryOperation(const FGuid& RequestId) = 0;

    /** Get pending operations */
    virtual TArray<FNetworkOperationRequest> GetPendingOperations() const = 0;

    /** Flush pending operations */
    virtual void FlushPendingOperations(bool bForce = false) = 0;

    /** Set operation timeout */
    virtual void SetOperationTimeout(float Seconds) = 0;

    /** Get network statistics */
    virtual FString GetNetworkStatistics() const = 0;

    /** Is operation pending */
    virtual bool IsOperationPending(const FGuid& RequestId) const = 0;
};

//========================================
// PREDICTION MANAGER INTERFACE
//========================================

/**
 * Prediction data - SuspenseCore version
 */
USTRUCT(BlueprintType)
struct EQUIPMENTSYSTEM_API FSuspenseCorePrediction
{
    GENERATED_BODY()

    UPROPERTY()
    FGuid PredictionId{};

    UPROPERTY()
    FEquipmentOperationRequest Operation{};

    UPROPERTY()
    FEquipmentStateSnapshot StateBefore{};

    UPROPERTY()
    FEquipmentStateSnapshot PredictedState{};

    UPROPERTY()
    float PredictionTime = 0.0f;

    UPROPERTY()
    bool bConfirmed = false;

    UPROPERTY()
    bool bRolledBack = false;
};

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCorePredictionManager : public UInterface
{
    GENERATED_BODY()
};

/**
 * SuspenseCore Prediction Manager Interface
 *
 * Provides responsive UI through client-side prediction.
 * Handles rollback and reconciliation with server.
 */
class EQUIPMENTSYSTEM_API ISuspenseCorePredictionManager
{
    GENERATED_BODY()

public:
    /** Create prediction */
    virtual FGuid CreatePrediction(const FEquipmentOperationRequest& Operation) = 0;

    /** Apply prediction locally */
    virtual bool ApplyPrediction(const FGuid& PredictionId) = 0;

    /** Confirm prediction from server */
    virtual bool ConfirmPrediction(
        const FGuid& PredictionId,
        const FEquipmentOperationResult& ServerResult) = 0;

    /** Rollback prediction */
    virtual bool RollbackPrediction(
        const FGuid& PredictionId,
        const FText& Reason = FText::GetEmpty()) = 0;

    /** Reconcile with server state */
    virtual void ReconcileWithServer(const FEquipmentStateSnapshot& ServerState) = 0;

    /** Get active predictions */
    virtual TArray<FSuspenseCorePrediction> GetActivePredictions() const = 0;

    /** Clear expired predictions */
    virtual int32 ClearExpiredPredictions(float MaxAge = 2.0f) = 0;

    /** Is prediction active */
    virtual bool IsPredictionActive(const FGuid& PredictionId) const = 0;

    /** Get prediction confidence */
    virtual float GetPredictionConfidence(const FGuid& PredictionId) const = 0;

    /** Enable/disable predictions */
    virtual void SetPredictionEnabled(bool bEnabled) = 0;
};

//========================================
// REPLICATION PROVIDER INTERFACE
//========================================

/**
 * Replication policy - SuspenseCore version
 */
UENUM(BlueprintType)
enum class ESuspenseCoreReplicationPolicy : uint8
{
    Always = 0,
    OnlyToOwner,
    OnlyToRelevant,
    SkipOwner,
    Custom
};

/**
 * Replicated equipment data - SuspenseCore version
 */
USTRUCT(BlueprintType)
struct EQUIPMENTSYSTEM_API FSuspenseCoreReplicatedData
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FSuspenseInventoryItemInstance> SlotInstances;

    UPROPERTY()
    int32 ActiveWeaponSlot = INDEX_NONE;

    UPROPERTY()
    FGameplayTag CurrentState;

    UPROPERTY()
    uint32 ReplicationVersion = 0;

    UPROPERTY()
    float LastUpdateTime = 0.0f;
};

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreReplicationProvider : public UInterface
{
    GENERATED_BODY()
};

/**
 * SuspenseCore Replication Provider Interface
 *
 * Manages state synchronization across network.
 * Optimizes bandwidth and ensures consistency.
 */
class EQUIPMENTSYSTEM_API ISuspenseCoreReplicationProvider
{
    GENERATED_BODY()

public:
    /** Mark data for replication */
    virtual void MarkForReplication(int32 SlotIndex, bool bForceUpdate = false) = 0;

    /** Get replicated data */
    virtual FSuspenseCoreReplicatedData GetReplicatedData() const = 0;

    /** Apply replicated data */
    virtual void ApplyReplicatedData(const FSuspenseCoreReplicatedData& Data, bool bIsInitialReplication = false) = 0;

    /** Set replication policy */
    virtual void SetReplicationPolicy(ESuspenseCoreReplicationPolicy Policy) = 0;

    /** Force full state replication */
    virtual void ForceFullReplication() = 0;

    /** Check if needs replication */
    virtual bool ShouldReplicateTo(APlayerController* ViewTarget) const = 0;

    /** Get replication priority */
    virtual bool GetReplicationPriority(APlayerController* ViewTarget, float& OutPriority) const = 0;

    /** Optimize replication data */
    virtual FSuspenseCoreReplicatedData OptimizeReplicationData(const FSuspenseCoreReplicatedData& Data) const = 0;

    /** Get delta since last replication */
    virtual FSuspenseCoreReplicatedData GetReplicationDelta(uint32 LastVersion) const = 0;

    /** Handle replication callback */
    virtual void OnReplicationCallback(const FName& PropertyName) = 0;
};
