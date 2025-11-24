// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Types/Equipment/EquipmentTypes.h"
#include "IMedComPredictionManager.generated.h"

/**
 * Prediction data
 */
USTRUCT(BlueprintType)
struct FEquipmentPrediction
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
class UMedComPredictionManager : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for client-side prediction
 *
 * Philosophy: Provides responsive UI through prediction.
 * Handles rollback and reconciliation with server.
 */
class MEDCOMSHARED_API IMedComPredictionManager
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
    virtual TArray<FEquipmentPrediction> GetActivePredictions() const = 0;

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
