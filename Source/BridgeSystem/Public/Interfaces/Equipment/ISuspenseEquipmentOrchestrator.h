// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Types/Loadout/SuspenseLoadoutSettings.h"
#include "Types/Equipment/SuspenseEquipmentTypes.h"
#include "Interfaces/Equipment/ISuspenseNetworkDispatcher.h"
#include "Interfaces/Equipment/ISuspenseEquipmentOperations.h"
#include "Interfaces/Equipment/ISuspenseEquipmentDataProvider.h"
#include "Interfaces/Equipment/ISuspenseTransactionManager.h"
#include "ISuspenseEquipmentOrchestrator.generated.h"

/**
 * System initialization parameters
 */
USTRUCT(BlueprintType)
struct FEquipmentSystemInitParams
{
    GENERATED_BODY()
    
    UPROPERTY()
    AActor* Owner = nullptr;
    
    UPROPERTY()
    class UAbilitySystemComponent* AbilitySystem = nullptr;
    
    UPROPERTY()
    TArray<FEquipmentSlotConfig> SlotConfigurations;
    
    UPROPERTY()
    FName InitialLoadout = NAME_None;
    
    UPROPERTY()
    bool bAutoInitialize = true;
};

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseEquipmentOrchestrator : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for system orchestration
 * 
 * Philosophy: Coordinates all equipment system components.
 * Manages initialization, lifecycle, and inter-component communication.
 */
class BRIDGESYSTEM_API ISuspenseEquipmentOrchestrator
{
    GENERATED_BODY()

public:
    /**
     * Initialize equipment system
     * @param Params Initialization parameters
     * @return True if initialized
     */
    virtual bool InitializeSystem(const FEquipmentSystemInitParams& Params) = 0;
    
    /**
     * Shutdown equipment system
     * @param bSaveState Save state before shutdown
     */
    virtual void ShutdownSystem(bool bSaveState = true) = 0;
    
    /**
     * Execute equipment operation
     * @param Operation Operation to execute
     * @return Operation result
     */
    virtual FEquipmentOperationResult ExecuteOperation(const FEquipmentOperationRequest& Operation) = 0;
    
    /**
     * Get data provider interface
     * @return Data provider or nullptr
     */
    virtual TScriptInterface<ISuspenseEquipmentDataProvider> GetDataProvider() const = 0;
    
    /**
     * Get operations executor interface
     * @return Operations executor or nullptr
     */
    virtual TScriptInterface<ISuspenseEquipmentOperations> GetOperationsExecutor() const = 0;
    
    /**
     * Get network dispatcher interface
     * @return Network dispatcher or nullptr
     */
    virtual TScriptInterface<ISuspenseNetworkDispatcher> GetNetworkDispatcher() const = 0;
    
    /**
     * Get transaction manager interface
     * @return Transaction manager or nullptr
     */
    virtual TScriptInterface<ISuspenseTransactionManager> GetTransactionManager() const = 0;
    
    /**
     * Register component
     * @param InterfaceType Type of interface
     * @param Component Component to register
     * @return True if registered
     */
    virtual bool RegisterComponent(const FGameplayTag& InterfaceType, UObject* Component) = 0;
    
    /**
     * Unregister component
     * @param InterfaceType Type of interface
     * @return True if unregistered
     */
    virtual bool UnregisterComponent(const FGameplayTag& InterfaceType) = 0;
    
    /**
     * Get registered component
     * @param InterfaceType Type of interface
     * @return Component or nullptr
     */
    virtual UObject* GetComponent(const FGameplayTag& InterfaceType) const = 0;
    
    /**
     * Validate system integrity
     * @param OutErrors Validation errors
     * @return True if valid
     */
    virtual bool ValidateSystemIntegrity(TArray<FText>& OutErrors) const = 0;
    
    /**
     * Reset system to default state
     */
    virtual void ResetSystem() = 0;
};