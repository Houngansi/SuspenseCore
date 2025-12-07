// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/Equipment/ISuspenseEquipmentService.h"
#include "Interfaces/Equipment/ISuspenseNetworkDispatcher.h"
#include "Interfaces/Equipment/ISuspensePredictionManager.h"
#include "Interfaces/Equipment/ISuspenseReplicationProvider.h"
#include "Components/Network/SuspenseEquipmentReplicationManager.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceMacros.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentSecurityService.h"
#include "Core/Utils/SuspenseEquipmentEventBus.h"
#include "Types/Network/SuspenseNetworkTypes.h"
#include "Types/Equipment/SuspenseEquipmentTypes.h"
#include "HAL/CriticalSection.h"
#include "Templates/SharedPointer.h"
#include "Containers/Queue.h"
#include "HAL/ThreadSafeBool.h"
#include "UObject/Interface.h"
#include "SuspenseCoreEquipmentNetworkService.generated.h"

// Forward declarations for dependency resolution
class USuspenseCoreEquipmentNetworkDispatcher;
class USuspenseCoreEquipmentPredictionSystem;
class USuspenseCoreEquipmentReplicationManager;
class USuspenseCoreEquipmentServiceLocator;
class USuspenseCoreEquipmentSecurityService;
class ISuspenseEquipmentDataProvider;
class ISuspenseEquipmentOperations;
struct FUniqueNetIdRepl;

/**
 * Equipment Network Service Implementation
 */
UCLASS()
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentNetworkService : public UObject,
    public IEquipmentNetworkService
{
    GENERATED_BODY()

public:
    USuspenseCoreEquipmentNetworkService();
    virtual ~USuspenseCoreEquipmentNetworkService();

    //~ Begin IEquipmentService Interface
    virtual bool InitializeService(const FServiceInitParams& Params) override;
    virtual bool ShutdownService(bool bForce = false) override;
    virtual EServiceLifecycleState GetServiceState() const override { return ServiceState; }
    virtual bool IsServiceReady() const override { return ServiceState == EServiceLifecycleState::Ready; }
    virtual FGameplayTag GetServiceTag() const override;
    virtual FGameplayTagContainer GetRequiredDependencies() const override;
    virtual bool ValidateService(TArray<FText>& OutErrors) const override;
    virtual void ResetService() override;
    virtual FString GetServiceStats() const override;
    //~ End IEquipmentService Interface

    //~ Begin IEquipmentNetworkService Interface
    virtual ISuspenseNetworkDispatcher* GetNetworkDispatcher() override
    {
        return NetworkDispatcher.GetInterface();
    }

    virtual ISuspensePredictionManager* GetPredictionManager() override
    {
        return PredictionManager.GetInterface();
    }

    virtual ISuspenseReplicationProvider* GetReplicationProvider() override
    {
        if (!ReplicationProvider) return nullptr;
        if (ReplicationProvider->GetClass()->ImplementsInterface(USuspenseReplicationProvider::StaticClass()))
        {
            return Cast<ISuspenseReplicationProvider>(ReplicationProvider);
        }
        return nullptr;
    }
    //~ End IEquipmentNetworkService Interface

    UFUNCTION(BlueprintCallable, Category = "Network|Operations")
    FGuid SendEquipmentOperation(const FEquipmentOperationRequest& Request, APlayerController* PlayerController);

    bool ReceiveEquipmentOperation(const FNetworkOperationRequest& NetworkRequest, APlayerController* SendingPlayer);

    UFUNCTION(BlueprintCallable, Category = "Network|Quality")
    void SetNetworkQuality(float Quality);

    //UFUNCTION(BlueprintCallable, Category = "Network|Metrics")
    FLatencyCompensationData GetNetworkMetrics() const;

    UFUNCTION(BlueprintCallable, Category = "Network|Sync")
    void ForceSynchronization(APlayerController* PlayerController);

    /** Get security metrics (delegated to SecurityService) */
    const FSecurityServiceMetrics* GetSecurityMetrics() const;

    /** Export security metrics (delegated to SecurityService) */
    bool ExportSecurityMetrics(const FString& FilePath) const;

    UFUNCTION(BlueprintCallable, Category = "Network|Metrics")
    bool ExportMetricsToCSV(const FString& FilePath) const;

    UFUNCTION(BlueprintCallable, Category = "Network|Security")
    void ReloadSecurityConfig();

    /** Get SecurityService reference */
    USuspenseCoreEquipmentSecurityService* GetSecurityService() const { return SecurityService; }

private:
    //========================================
    // Service State
    //========================================
    EServiceLifecycleState ServiceState;
    FServiceInitParams ServiceParams;

    //========================================
    // Network Components
    //========================================
    TScriptInterface<ISuspenseNetworkDispatcher> NetworkDispatcher;
    TScriptInterface<ISuspensePredictionManager> PredictionManager;

    UPROPERTY(Transient)
    TObjectPtr<USuspenseCoreEquipmentReplicationManager> ReplicationProvider = nullptr;

    //========================================
    // Security Service (Delegated)
    // All security logic delegated to SecurityService following SRP
    //========================================
    UPROPERTY()
    USuspenseCoreEquipmentSecurityService* SecurityService = nullptr;

    //========================================
    // Metrics
    //========================================
    mutable FServiceMetrics ServiceMetrics;
    mutable float AverageLatency;
    mutable int32 TotalOperationsSent;
    mutable int32 TotalOperationsRejected;
    float NetworkQualityLevel;

    //========================================
    // Timers
    //========================================
    FTimerHandle MetricsUpdateTimer;

    //========================================
    // Dispatcher Delegate Handles
    //========================================
    FDelegateHandle DispatcherSuccessHandle;
    FDelegateHandle DispatcherFailureHandle;
    FDelegateHandle DispatcherTimeoutHandle;

    //========================================
    // EventBus Integration
    //========================================
    TWeakPtr<FSuspenseCoreEquipmentEventBus> EventBus;
    TArray<FEventSubscriptionHandle> EventSubscriptions;

    // Event tags for network events
    FGameplayTag Tag_NetworkResult;
    FGameplayTag Tag_NetworkTimeout;
    FGameplayTag Tag_SecurityViolation;
    FGameplayTag Tag_OperationCompleted;

    //========================================
    // Internal Methods
    //========================================

    /**
     * Internal non-virtual cleanup method safe to call from destructor
     */
    void InternalShutdown(bool bForce, bool bFromDestructor);

    // ---- Dependency Resolution ----
    bool ResolveDependencies(
        UWorld* World,
        TScriptInterface<ISuspenseEquipmentDataProvider>& OutDataProvider,
        TScriptInterface<ISuspenseEquipmentOperations>& OutOperationExecutor);

    // ---- Component Creation ----
    USuspenseCoreEquipmentNetworkDispatcher* CreateAndInitNetworkDispatcher(
        AActor* OwnerActor,
        const TScriptInterface<ISuspenseEquipmentOperations>& OperationExecutor);

    USuspenseCoreEquipmentPredictionSystem* CreateAndInitPredictionSystem(
        AActor* OwnerActor,
        const TScriptInterface<ISuspenseEquipmentDataProvider>& DataProvider,
        const TScriptInterface<ISuspenseEquipmentOperations>& OperationExecutor);

    USuspenseCoreEquipmentReplicationManager* CreateAndInitReplicationManager(
        AActor* OwnerActor,
        const TScriptInterface<ISuspenseEquipmentDataProvider>& DataProvider);

    void BindDispatcherToPrediction(
        USuspenseCoreEquipmentNetworkDispatcher* Dispatcher,
        ISuspensePredictionManager* Prediction);

    void StartMonitoringTimers(UWorld* World);

    // ---- Player Identification ----
    FGuid CreatePlayerGuid(const FUniqueNetIdRepl& UniqueId) const;

    // ---- Network Metrics ----
    void UpdateNetworkMetrics();
    void AdaptNetworkStrategies();

    // ---- EventBus Integration ----
    void SetupEventSubscriptions();
    void TeardownEventSubscriptions();

    void BroadcastNetworkResult(bool bSuccess, const FGuid& OperationId, const FString& ErrorMessage = TEXT(""));
    void BroadcastSecurityViolation(const FString& ViolationType, APlayerController* PlayerController, const FString& Details);

    void OnOperationCompleted(const FSuspenseCoreEquipmentEventData& EventData);
};
