// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "Core/Services/SuspenseEquipmentServiceLocator.h"
#include "ISuspenseCoreEquipmentService.generated.h"

/**
 * Service lifecycle state - SuspenseCore version
 */
UENUM(BlueprintType)
enum class ESuspenseCoreServiceLifecycleState : uint8
{
    Uninitialized,
    Initializing,
    Ready,
    Shutting,
    Shutdown,
    Failed
};

/**
 * Service initialization parameters - SuspenseCore version
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreServiceInitParams
{
    GENERATED_BODY()

    UPROPERTY()
    UObject* Owner = nullptr;

    UPROPERTY()
    TObjectPtr<class USuspenseEquipmentServiceLocator> ServiceLocator = nullptr;

    UPROPERTY()
    FGameplayTagContainer RequiredServices;

    UPROPERTY()
    TMap<FString, FString> Configuration;

    UPROPERTY()
    bool bAutoStart = true;

    UPROPERTY()
    int32 Priority = 0;
};

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreEquipmentService : public UInterface
{
    GENERATED_BODY()
};

/**
 * Base interface for all SuspenseCore equipment services
 *
 * Philosophy: Common lifecycle and dependency management for all services.
 * Enables proper initialization order and graceful shutdown.
 *
 * This is the SuspenseCore version that replaces the legacy ISuspenseEquipmentService.
 */
class BRIDGESYSTEM_API ISuspenseCoreEquipmentService
{
    GENERATED_BODY()

public:
    /**
     * Initialize service with parameters
     * @param Params Initialization parameters
     * @return True if initialization started successfully
     */
    virtual bool InitializeService(const FSuspenseCoreServiceInitParams& Params) = 0;

    /**
     * Shutdown service gracefully
     * @param bForce Force immediate shutdown
     * @return True if shutdown initiated
     */
    virtual bool ShutdownService(bool bForce = false) = 0;

    /**
     * Get current service state
     * @return Current lifecycle state
     */
    virtual ESuspenseCoreServiceLifecycleState GetServiceState() const = 0;

    /**
     * Check if service is ready
     * @return True if service is in Ready state
     */
    virtual bool IsServiceReady() const = 0;

    /**
     * Get service identifier tag
     * @return Service identification tag
     */
    virtual FGameplayTag GetServiceTag() const = 0;

    /**
     * Get required dependencies
     * @return Tags of required services
     */
    virtual FGameplayTagContainer GetRequiredDependencies() const = 0;

    /**
     * Validate service integrity
     * @param OutErrors Validation errors
     * @return True if service is valid
     */
    virtual bool ValidateService(TArray<FText>& OutErrors) const = 0;

    /**
     * Reset service to initial state
     */
    virtual void ResetService() = 0;

    /**
     * Get service statistics
     * @return Statistics as string
     */
    virtual FString GetServiceStats() const = 0;
};

// Alias for backward compatibility with existing code
using IEquipmentService = ISuspenseCoreEquipmentService;

//========================================
// Specialized Service Interfaces
//========================================

// Forward declarations
class ISuspenseCoreEquipmentDataProvider;
class ISuspenseCoreTransactionManager;
class ISuspenseCoreEquipmentRules;
class ISuspenseCoreNetworkDispatcher;
class ISuspenseCorePredictionManager;
class ISuspenseCoreReplicationProvider;

/**
 * Data Service Interface - SuspenseCore version
 */
UINTERFACE(MinimalAPI, NotBlueprintable)
class USuspenseCoreEquipmentDataServiceInterface : public USuspenseCoreEquipmentService
{
    GENERATED_BODY()
};

class BRIDGESYSTEM_API ISuspenseCoreEquipmentDataServiceInterface : public ISuspenseCoreEquipmentService
{
    GENERATED_BODY()

public:
    /**
     * Inject pre-created components into the service
     * @param InDataStore Pre-created DataStore component
     * @param InTransactionProcessor Pre-created TransactionProcessor
     */
    virtual void InjectComponents(UObject* InDataStore, UObject* InTransactionProcessor) = 0;

    /**
     * Set optional validator
     * @param InValidator Slot validator component
     */
    virtual void SetValidator(UObject* InValidator) = 0;

    /**
     * Get data provider interface
     * @return Data provider interface
     */
    virtual ISuspenseCoreEquipmentDataProvider* GetDataProvider() = 0;

    /**
     * Get transaction manager interface
     * @return Transaction manager interface
     */
    virtual ISuspenseCoreTransactionManager* GetTransactionManager() = 0;
};

/**
 * Network Service Interface - SuspenseCore version
 */
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreEquipmentNetworkServiceInterface : public USuspenseCoreEquipmentService
{
    GENERATED_BODY()
};

class BRIDGESYSTEM_API ISuspenseCoreEquipmentNetworkServiceInterface : public ISuspenseCoreEquipmentService
{
    GENERATED_BODY()

public:
    virtual ISuspenseCoreNetworkDispatcher* GetNetworkDispatcher() = 0;
    virtual ISuspenseCorePredictionManager* GetPredictionManager() = 0;
    virtual ISuspenseCoreReplicationProvider* GetReplicationProvider() = 0;
};

/**
 * Validation Service Interface - SuspenseCore version
 */
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreEquipmentValidationServiceInterface : public USuspenseCoreEquipmentService
{
    GENERATED_BODY()
};

class BRIDGESYSTEM_API ISuspenseCoreEquipmentValidationServiceInterface : public ISuspenseCoreEquipmentService
{
    GENERATED_BODY()

public:
    /**
     * Get rules engine interface
     * @return Rules engine interface
     */
    virtual ISuspenseCoreEquipmentRules* GetRulesEngine() = 0;

    /**
     * Register custom validator
     * @param ValidatorTag Validator identifier
     * @param Validator Validation function
     * @return True if registered
     */
    virtual bool RegisterValidator(const FGameplayTag& ValidatorTag, TFunction<bool(const void*)> Validator) = 0;

    /**
     * Clear validation cache
     */
    virtual void ClearValidationCache() = 0;
};

// Forward declaration for operations
class ISuspenseCoreEquipmentOperations;
struct FEquipmentOperationRequest;
struct FEquipmentOperationResult;

/**
 * Operation Service Interface - SuspenseCore version
 */
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreEquipmentOperationServiceInterface : public USuspenseCoreEquipmentService
{
    GENERATED_BODY()
};

class BRIDGESYSTEM_API ISuspenseCoreEquipmentOperationServiceInterface : public ISuspenseCoreEquipmentService
{
    GENERATED_BODY()

public:
    /**
     * Get operations executor
     * @return Operations executor interface
     */
    virtual ISuspenseCoreEquipmentOperations* GetOperationsExecutor() = 0;

    /**
     * Queue operation for processing
     * @param Request Operation request
     * @return True if queued
     */
    virtual bool QueueOperation(const FEquipmentOperationRequest& Request) = 0;

    /**
     * Process operation queue
     */
    virtual void ProcessOperationQueue() = 0;

    /**
     * Execute operation immediately
     * @param Request Operation request
     * @return Operation result
     */
    virtual FEquipmentOperationResult ExecuteImmediate(const FEquipmentOperationRequest& Request) = 0;
};
