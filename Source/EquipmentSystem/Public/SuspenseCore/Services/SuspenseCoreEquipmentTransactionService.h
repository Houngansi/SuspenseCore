// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentService.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreTransactionManager.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceMacros.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "Types/Transaction/SuspenseTransactionTypes.h"
#include "SuspenseCoreEquipmentTransactionService.generated.h"

// Forward declarations
class USuspenseCoreEquipmentTransactionProcessor;
class ISuspenseCoreEquipmentDataProvider;

DECLARE_LOG_CATEGORY_EXTERN(LogSuspenseCoreEquipmentTransaction, Log, All);

//========================================
// TRANSACTION SERVICE CONFIGURATION
//========================================

USTRUCT(BlueprintType)
struct FTransactionServiceConfig
{
    GENERATED_BODY()

    /** Transaction timeout in seconds */
    UPROPERTY(EditAnywhere, Config, Category = "Timing")
    float TransactionTimeout = 30.0f;

    /** Maximum nested transaction depth */
    UPROPERTY(EditAnywhere, Config, Category = "Limits")
    int32 MaxNestedDepth = 5;

    /** Maximum transaction history size */
    UPROPERTY(EditAnywhere, Config, Category = "Limits")
    int32 MaxHistorySize = 100;

    /** Enable auto-recovery on failures */
    UPROPERTY(EditAnywhere, Config, Category = "Recovery")
    bool bAutoRecovery = true;

    /** Enable transaction logging */
    UPROPERTY(EditAnywhere, Config, Category = "Debug")
    bool bEnableLogging = true;

    /** Enable delta generation */
    UPROPERTY(EditAnywhere, Config, Category = "Features")
    bool bGenerateDeltas = true;

    /** Broadcast transaction events via EventBus */
    UPROPERTY(EditAnywhere, Config, Category = "Events")
    bool bBroadcastTransactionEvents = true;

    /** Cleanup interval in seconds */
    UPROPERTY(EditAnywhere, Config, Category = "Maintenance")
    float CleanupInterval = 60.0f;

    static FTransactionServiceConfig LoadFromConfig(const FString& ConfigSection = TEXT("EquipmentTransaction"));
};

//========================================
// TRANSACTION SERVICE METRICS
//========================================

struct EQUIPMENTSYSTEM_API FTransactionServiceMetrics
{
    TAtomic<uint64> TotalTransactionsStarted{0};
    TAtomic<uint64> TotalTransactionsCommitted{0};
    TAtomic<uint64> TotalTransactionsRolledBack{0};
    TAtomic<uint64> TotalTransactionsFailed{0};
    TAtomic<uint64> TotalOperationsProcessed{0};
    TAtomic<uint64> TotalConflictsResolved{0};
    TAtomic<uint64> TotalDeltasGenerated{0};
    TAtomic<uint64> ActiveTransactionCount{0};
    TAtomic<uint64> AverageTransactionTimeUs{0};
    TAtomic<uint64> PeakTransactionTimeUs{0};

    FString ToString() const;
    void Reset();

    float GetCommitRate() const
    {
        const uint64 Total = TotalTransactionsCommitted.Load() + TotalTransactionsRolledBack.Load() + TotalTransactionsFailed.Load();
        return Total > 0 ? (static_cast<float>(TotalTransactionsCommitted.Load()) / Total) * 100.0f : 0.0f;
    }
};

//========================================
// TRANSACTION SERVICE
//========================================

/**
 * Equipment Transaction Service
 *
 * Single Responsibility: Transaction lifecycle management
 * - Owns and manages TransactionProcessor component
 * - Provides IEquipmentService lifecycle
 * - Integrates with EventBus for transaction events
 * - ACID compliance (Atomicity, Consistency, Isolation, Durability)
 *
 * Extracted from Component layer to Service layer:
 * - Service handles lifecycle and integration
 * - Processor handles actual transaction logic
 */
UCLASS()
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentTransactionService : public UObject,
    public IEquipmentService,
    public ISuspenseCoreTransactionManager
{
    GENERATED_BODY()

public:
    USuspenseCoreEquipmentTransactionService();
    virtual ~USuspenseCoreEquipmentTransactionService();

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

    //~ Begin ISuspenseCoreTransactionManager Interface
    virtual FGuid BeginTransaction(const FString& Description = TEXT("")) override;
    virtual bool CommitTransaction(const FGuid& TransactionId) override;
    virtual bool RollbackTransaction(const FGuid& TransactionId) override;
    virtual FGuid CreateSavepoint(const FString& SavepointName) override;
    virtual bool RollbackToSavepoint(const FGuid& SavepointId) override;
    virtual FEquipmentTransaction GetCurrentTransaction() const override;
    virtual bool IsTransactionActive() const override;
    virtual FEquipmentTransaction GetTransaction(const FGuid& TransactionId) const override;
    virtual FGuid BeginNestedTransaction(const FString& Description = TEXT("")) override;
    virtual bool RegisterOperation(const FGuid& OperationId) override;
    virtual bool ValidateTransaction(const FGuid& TransactionId) const override;
    virtual TArray<FEquipmentTransaction> GetTransactionHistory(int32 MaxCount = 10) const override;
    virtual bool SupportsExtendedOps() const override { return true; }
    virtual bool RegisterOperation(const FGuid& TransactionId, const FTransactionOperation& Operation) override;
    virtual bool ApplyOperation(const FGuid& TransactionId, const FTransactionOperation& Operation) override;
    virtual TArray<FEquipmentDelta> GetTransactionDeltas(const FGuid& TransactionId) const override;
    //~ End ISuspenseCoreTransactionManager Interface

    //========================================
    // Extended API
    //========================================

    /** Commit all pending transactions */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Transaction")
    int32 CommitAllTransactions();

    /** Rollback all pending transactions */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Transaction")
    int32 RollbackAllTransactions();

    /** Get current transaction ID */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Transaction")
    FGuid GetCurrentTransactionId() const;

    /** Get active transaction count */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Transaction")
    int32 GetActiveTransactionCount() const;

    /** Clear transaction history */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Transaction")
    void ClearTransactionHistory(bool bKeepActive = true);

    /** Inject data provider dependency */
    bool InjectDataProvider(TScriptInterface<ISuspenseCoreEquipmentDataProvider> InDataProvider);

    /** Get metrics */
    const FTransactionServiceMetrics& GetMetrics() const { return Metrics; }

    /** Get configuration */
    const FTransactionServiceConfig& GetConfiguration() const { return Config; }

protected:
    // Service state
    EServiceLifecycleState ServiceState = EServiceLifecycleState::Uninitialized;
    FServiceInitParams ServiceParams;

    // Configuration
    FTransactionServiceConfig Config;

    // Transaction processor (internal implementation)
    // Note: Using raw storage since TransactionProcessor is ActorComponent
    // In production, consider refactoring to UObject-based processor
    TMap<FGuid, FEquipmentTransaction> PendingTransactions;
    TArray<FGuid> TransactionStack;
    TArray<FEquipmentTransaction> TransactionHistory;
    mutable FCriticalSection TransactionLock;

    // Data provider reference
    UPROPERTY()
    TScriptInterface<ISuspenseCoreEquipmentDataProvider> DataProvider;

    // EventBus integration
    TWeakPtr<FSuspenseCoreEquipmentEventBus> EventBus;
    TArray<FEventSubscriptionHandle> EventSubscriptions;

    // Event tags
    FGameplayTag Tag_Transaction_Started;
    FGameplayTag Tag_Transaction_Committed;
    FGameplayTag Tag_Transaction_RolledBack;
    FGameplayTag Tag_Transaction_Failed;

    // Metrics
    mutable FTransactionServiceMetrics Metrics;
    mutable FServiceMetrics ServiceMetrics;

    // Transaction timing
    TMap<FGuid, double> TransactionStartTimes;

private:
    // Internal helpers
    FEquipmentTransaction* FindTransaction(const FGuid& TransactionId);
    const FEquipmentTransaction* FindTransaction(const FGuid& TransactionId) const;

    // EventBus helpers
    void SetupEventBus();
    void TeardownEventBus();
    void BroadcastTransactionStarted(const FGuid& TransactionId, const FString& Description) const;
    void BroadcastTransactionCommitted(const FGuid& TransactionId) const;
    void BroadcastTransactionRolledBack(const FGuid& TransactionId) const;
    void BroadcastTransactionFailed(const FGuid& TransactionId, const FString& Reason) const;

    // Metrics
    void UpdateMetrics(const FGuid& TransactionId, bool bCommitted) const;
    void CleanupExpiredTransactions();
};
