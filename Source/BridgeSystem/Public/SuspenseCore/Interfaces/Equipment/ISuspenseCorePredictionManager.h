// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Types/Equipment/SuspenseEquipmentTypes.h"
#include "Interfaces/Equipment/ISuspensePredictionManager.h"
#include "ISuspenseCorePredictionManager.generated.h"

/**
 * SuspenseCore Prediction data
 */
USTRUCT(BlueprintType)
struct FSuspenseCorePrediction
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

    /** Convert to legacy format */
    FEquipmentPrediction ToLegacy() const
    {
        FEquipmentPrediction Legacy;
        Legacy.PredictionId = PredictionId;
        Legacy.Operation = Operation;
        Legacy.StateBefore = StateBefore;
        Legacy.PredictedState = PredictedState;
        Legacy.PredictionTime = PredictionTime;
        Legacy.bConfirmed = bConfirmed;
        Legacy.bRolledBack = bRolledBack;
        return Legacy;
    }

    /** Create from legacy format */
    static FSuspenseCorePrediction FromLegacy(const FEquipmentPrediction& Legacy)
    {
        FSuspenseCorePrediction Result;
        Result.PredictionId = Legacy.PredictionId;
        Result.Operation = Legacy.Operation;
        Result.StateBefore = Legacy.StateBefore;
        Result.PredictedState = Legacy.PredictedState;
        Result.PredictionTime = Legacy.PredictionTime;
        Result.bConfirmed = Legacy.bConfirmed;
        Result.bRolledBack = Legacy.bRolledBack;
        return Result;
    }
};

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCorePredictionManager : public UInterface
{
    GENERATED_BODY()
};

/**
 * SuspenseCore Interface for client-side prediction
 *
 * Philosophy: Provides responsive UI through prediction.
 * Handles rollback and reconciliation with server.
 *
 * This is the SuspenseCore version of ISuspensePredictionManager.
 */
class BRIDGESYSTEM_API ISuspenseCorePredictionManager
{
    GENERATED_BODY()

public:
    /**
     * Create prediction
     * @param Operation Operation to predict
     * @return Prediction ID
     */
    virtual FGuid CreatePrediction(const FEquipmentOperationRequest& Operation) = 0;

    /**
     * Apply prediction locally
     * @param PredictionId Prediction to apply
     * @return True if applied
     */
    virtual bool ApplyPrediction(const FGuid& PredictionId) = 0;

    /**
     * Confirm prediction from server
     * @param PredictionId Prediction to confirm
     * @param ServerResult Server result
     * @return True if confirmed
     */
    virtual bool ConfirmPrediction(
        const FGuid& PredictionId,
        const FEquipmentOperationResult& ServerResult) = 0;

    /**
     * Rollback prediction
     * @param PredictionId Prediction to rollback
     * @param Reason Rollback reason
     * @return True if rolled back
     */
    virtual bool RollbackPrediction(
        const FGuid& PredictionId,
        const FText& Reason = FText::GetEmpty()) = 0;

    /**
     * Reconcile with server state
     * @param ServerState Authoritative server state
     */
    virtual void ReconcileWithServer(const FEquipmentStateSnapshot& ServerState) = 0;

    /**
     * Get active predictions
     * @return Array of active predictions
     */
    virtual TArray<FSuspenseCorePrediction> GetActivePredictions() const = 0;

    /**
     * Clear expired predictions
     * @param MaxAge Maximum age in seconds
     * @return Number cleared
     */
    virtual int32 ClearExpiredPredictions(float MaxAge = 2.0f) = 0;

    /**
     * Is prediction active
     * @param PredictionId Prediction to check
     * @return True if active
     */
    virtual bool IsPredictionActive(const FGuid& PredictionId) const = 0;

    /**
     * Get prediction confidence
     * @param PredictionId Prediction to check
     * @return Confidence from 0 to 1
     */
    virtual float GetPredictionConfidence(const FGuid& PredictionId) const = 0;

    /**
     * Enable/disable predictions
     * @param bEnabled New state
     */
    virtual void SetPredictionEnabled(bool bEnabled) = 0;
};

// Backward compatibility alias
using IPredictionManager = ISuspenseCorePredictionManager;
