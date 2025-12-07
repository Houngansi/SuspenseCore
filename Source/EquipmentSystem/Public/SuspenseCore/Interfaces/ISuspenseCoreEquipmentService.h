// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "ISuspenseCoreEquipmentService.generated.h"

/**
 * Service lifecycle state - SuspenseCore version
 */
UENUM(BlueprintType)
enum class EServiceLifecycleState : uint8
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
struct EQUIPMENTSYSTEM_API FServiceInitParams
{
    GENERATED_BODY()

    UPROPERTY()
    UObject* Owner = nullptr;

    UPROPERTY()
    TObjectPtr<class USuspenseCoreEquipmentServiceLocator> ServiceLocator = nullptr;

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
class EQUIPMENTSYSTEM_API ISuspenseCoreEquipmentService
{
    GENERATED_BODY()

public:
    /**
     * Initialize service with parameters
     * @param Params Initialization parameters
     * @return True if initialization started successfully
     */
    virtual bool InitializeService(const FServiceInitParams& Params) = 0;

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
    virtual EServiceLifecycleState GetServiceState() const = 0;

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
class USuspenseCoreEquipmentDataService : public USuspenseCoreEquipmentService
{
    GENERATED_BODY()
};

class EQUIPMENTSYSTEM_API ISuspenseCoreEquipmentDataService : public ISuspenseCoreEquipmentService
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
class USuspenseCoreEquipmentNetworkService : public USuspenseCoreEquipmentService
{
    GENERATED_BODY()
};

class EQUIPMENTSYSTEM_API ISuspenseCoreEquipmentNetworkService : public ISuspenseCoreEquipmentService
{
    GENERATED_BODY()

public:
    virtual ISuspenseCoreNetworkDispatcher* GetNetworkDispatcher() = 0;
    virtual ISuspenseCorePredictionManager* GetPredictionManager() = 0;
    virtual ISuspenseCoreReplicationProvider* GetReplicationProvider() = 0;
};
