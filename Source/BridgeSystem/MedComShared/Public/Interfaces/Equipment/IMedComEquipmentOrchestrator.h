// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Types/Loadout/LoadoutSettings.h"
#include "Types/Equipment/EquipmentTypes.h"
#include "Interfaces/Equipment/IMedComNetworkDispatcher.h"
#include "Interfaces/Equipment/IMedComEquipmentOperations.h"
#include "Interfaces/Equipment/IMedComEquipmentDataProvider.h"
#include "Interfaces/Equipment/IMedComTransactionManager.h"
#include "IMedComEquipmentOrchestrator.generated.h"

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
class UMedComEquipmentOrchestrator : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for system orchestration
 * 
 * Philosophy: Coordinates all equipment system components.
 * Manages initialization, lifecycle, and inter-component communication.
 */
class MEDCOMSHARED_API IMedComEquipmentOrchestrator
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
    virtual TScriptInterface<IMedComEquipmentDataProvider> GetDataProvider() const = 0;
    
    /**
     * Get operations executor interface
     * @return Operations executor or nullptr
     */
    virtual TScriptInterface<IMedComEquipmentOperations> GetOperationsExecutor() const = 0;
    
    /**
     * Get network dispatcher interface
     * @return Network dispatcher or nullptr
     */
    virtual TScriptInterface<IMedComNetworkDispatcher> GetNetworkDispatcher() const = 0;
    
    /**
     * Get transaction manager interface
     * @return Transaction manager or nullptr
     */
    virtual TScriptInterface<IMedComTransactionManager> GetTransactionManager() const = 0;
    
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