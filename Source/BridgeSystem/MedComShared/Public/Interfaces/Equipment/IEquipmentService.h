// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "IEquipmentService.generated.h"

// Forward declarations to avoid circular dependencies
class UMedComEquipmentDataStore;
class UMedComEquipmentTransactionProcessor;
class UMedComEquipmentSlotValidator;

struct FEquipmentOperationRequest;
struct FEquipmentOperationResult;

/**
 * Service lifecycle state
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
 * Service initialization parameters
 */
USTRUCT(BlueprintType)
struct FServiceInitParams
{
    GENERATED_BODY()
    
    UPROPERTY()
    UObject* Owner = nullptr;
	
	UPROPERTY()
	TObjectPtr<class UEquipmentServiceLocator> ServiceLocator = nullptr;
	
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
class UEquipmentService : public UInterface
{
    GENERATED_BODY()
};

/**
 * Base interface for all equipment services
 * 
 * Philosophy: Common lifecycle and dependency management for all services.
 * Enables proper initialization order and graceful shutdown.
 */
class MEDCOMSHARED_API IEquipmentService
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

// Data Service Interface
UINTERFACE(MinimalAPI, NotBlueprintable)
class UEquipmentDataService : public UEquipmentService
{
    GENERATED_BODY()
};

/**
 * Interface for equipment data management service
 * 
 * CRITICAL ARCHITECTURE NOTE:
 * This interface now supports dependency injection pattern.
 * ServiceLocator will call InjectComponents BEFORE InitializeService
 * to provide pre-created components from PlayerState.
 * 
 * Initialization Order (MUST BE FOLLOWED):
 * 1. Service instance created by ServiceLocator
 * 2. InjectComponents() called with DataStore and TransactionProcessor
 * 3. SetValidator() called if validator available (optional)
 * 4. InitializeService() called to complete initialization
 * 
 * This pattern ensures components created in PlayerState constructor
 * are properly reused by the service system.
 */
class MEDCOMSHARED_API IEquipmentDataService : public IEquipmentService
{
    GENERATED_BODY()
    
public:
    //========================================
    // Component Injection (NEW - Critical for ServiceLocator integration)
    //========================================
    
    /**
     * Inject pre-created components into the service
     * 
     * This method MUST be called by ServiceLocator BEFORE InitializeService().
     * It receives components already created by PlayerState in its constructor.
     * 
     * Architecture rationale:
     * - PlayerState creates components as UActorComponents (for replication)
     * - DataService receives these components via injection
     * - Service wraps them with proper interfaces and lifecycle management
     * 
     * @param InDataStore Pre-created DataStore component from PlayerState
     * @param InTxnProcessor Pre-created TransactionProcessor from PlayerState
     */
	virtual void InjectComponents(UObject* InDataStore, UObject* InTransactionProcessor) = 0;

    
    /**
     * Set optional validator for slot operations
     * 
     * This method should be called AFTER InjectComponents but BEFORE InitializeService.
     * Validator is optional - service can operate without it (with reduced validation).
     * 
     * @param InValidator Slot validator component (can be nullptr)
     */
	virtual void SetValidator(UObject* InValidator) = 0;
    
    //========================================
    // Data Provider Interface
    //========================================
    
    /**
     * Get data provider interface for direct data access
     * @return Data provider interface (typically the DataStore)
     */
    virtual class IMedComEquipmentDataProvider* GetDataProvider() = 0;
    
    /**
     * Get transaction manager interface for ACID operations
     * @return Transaction manager interface (typically the TransactionProcessor)
     */
    virtual class IMedComTransactionManager* GetTransactionManager() = 0;
};

// Operation Service Interface
UINTERFACE(MinimalAPI, Blueprintable)
class UEquipmentOperationService : public UEquipmentService
{
    GENERATED_BODY()
};

/**
 * Interface for equipment operations service
 */
class MEDCOMSHARED_API IEquipmentOperationService : public IEquipmentService
{
    GENERATED_BODY()

public:
    // Forward to IMedComEquipmentOperations methods
    virtual class IMedComEquipmentOperations* GetOperationsExecutor() = 0;
    virtual bool QueueOperation(const struct FEquipmentOperationRequest& Request) = 0;
    virtual void ProcessOperationQueue() = 0;

    /**
     * Execute single request synchronously with validation, apply and commit.
     * Server-authoritative: implementation must route to server when needed.
     */
    virtual FEquipmentOperationResult ExecuteImmediate(const FEquipmentOperationRequest& Request) = 0;
};

// Validation Service Interface
UINTERFACE(MinimalAPI, Blueprintable)
class UEquipmentValidationService : public UEquipmentService
{
    GENERATED_BODY()
};

/**
 * Interface for equipment validation service
 */
class MEDCOMSHARED_API IEquipmentValidationService : public IEquipmentService
{
    GENERATED_BODY()
    
public:
    // Forward to IMedComEquipmentRules methods
    virtual class IMedComEquipmentRules* GetRulesEngine() = 0;
    virtual bool RegisterValidator(const FGameplayTag& ValidatorTag, TFunction<bool(const void*)> Validator) = 0;
    virtual void ClearValidationCache() = 0;
};

// Visualization Service Interface
UINTERFACE(MinimalAPI, Blueprintable)
class UEquipmentVisualizationService : public UEquipmentService
{
    GENERATED_BODY()
};

/**
 * Interface for equipment visualization service
 */
class MEDCOMSHARED_API IEquipmentVisualizationService : public IEquipmentService
{
    GENERATED_BODY()
    
public:
    // Forward to IMedComVisualProvider methods
    virtual class IMedComVisualProvider* GetVisualProvider() = 0;
    virtual class IMedComActorFactory* GetActorFactory() = 0;
    virtual class IMedComAttachmentProvider* GetAttachmentProvider() = 0;
};

// Network Service Interface
UINTERFACE(MinimalAPI, Blueprintable)
class UEquipmentNetworkService : public UEquipmentService
{
    GENERATED_BODY()
};

/**
 * Interface for equipment network service
 */
class MEDCOMSHARED_API IEquipmentNetworkService : public IEquipmentService
{
    GENERATED_BODY()
    
public:
    // Forward to IMedComNetworkDispatcher methods
    virtual class IMedComNetworkDispatcher* GetNetworkDispatcher() = 0;
    virtual class IMedComPredictionManager* GetPredictionManager() = 0;
    virtual class IMedComReplicationProvider* GetReplicationProvider() = 0;
};