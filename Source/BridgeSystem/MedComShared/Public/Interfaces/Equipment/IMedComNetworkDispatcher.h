// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Types/Network/MedComNetworkTypes.h"
#include "IMedComNetworkDispatcher.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UMedComNetworkDispatcher : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for network operations dispatching
 * 
 * Philosophy: Central point for all network operations.
 * Handles RPC calls, batching, and reliability.
 */
class MEDCOMSHARED_API IMedComNetworkDispatcher
{
    GENERATED_BODY()

public:
    /**
     * Send operation to server
     * @param Request Operation request
     * @return Request ID for tracking
     */
    virtual FGuid SendOperationToServer(const FNetworkOperationRequest& Request) = 0;
    
    /**
     * Send operation to clients
     * @param Request Operation request
     * @param TargetClients Specific clients or broadcast
     */
    virtual void SendOperationToClients(
        const FNetworkOperationRequest& Request,
        const TArray<APlayerController*>& TargetClients) = 0;
    
    /**
     * Handle server response
     * @param Response Server response
     */
    virtual void HandleServerResponse(const FNetworkOperationResponse& Response) = 0;
    
    /**
     * Batch multiple operations
     * @param Operations Operations to batch
     * @return Batch request ID
     */
    virtual FGuid BatchOperations(const TArray<FNetworkOperationRequest>& Operations) = 0;
    
    /**
     * Cancel pending operation
     * @param RequestId Request to cancel
     * @return True if cancelled
     */
    virtual bool CancelOperation(const FGuid& RequestId) = 0;
    
    /**
     * Retry failed operation
     * @param RequestId Request to retry
     * @return True if retrying
     */
    virtual bool RetryOperation(const FGuid& RequestId) = 0;
    
    /**
     * Get pending operations
     * @return Array of pending requests
     */
    virtual TArray<FNetworkOperationRequest> GetPendingOperations() const = 0;
    
    /**
     * Flush pending operations
     * @param bForce Force immediate send
     */
    virtual void FlushPendingOperations(bool bForce = false) = 0;
    
    /**
     * Set operation timeout
     * @param Seconds Timeout in seconds
     */
    virtual void SetOperationTimeout(float Seconds) = 0;
    
    /**
     * Get network statistics
     * @return Statistics string
     */
    virtual FString GetNetworkStatistics() const = 0;
    
    /**
     * Is operation pending
     * @param RequestId Request to check
     * @return True if pending
     */
    virtual bool IsOperationPending(const FGuid& RequestId) const = 0;
};