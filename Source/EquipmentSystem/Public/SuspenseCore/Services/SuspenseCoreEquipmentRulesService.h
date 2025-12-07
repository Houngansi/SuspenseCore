// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/Equipment/ISuspenseEquipmentService.h"
#include "Interfaces/Equipment/ISuspenseCoreEquipmentRules.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceMacros.h"
#include "Core/Utils/SuspenseCoreEquipmentEventBus.h"
#include "Types/Rules/SuspenseCoreRulesTypes.h"
#include "SuspenseCoreEquipmentRulesService.generated.h"

// Forward declarations
class USuspenseCoreRulesCoordinator;
class ISuspenseCoreEquipmentDataProvider;

DECLARE_LOG_CATEGORY_EXTERN(LogSuspenseCoreEquipmentRules, Log, All);

//========================================
// RULES SERVICE CONFIGURATION
//========================================

USTRUCT(BlueprintType)
struct FRulesServiceConfig
{
    GENERATED_BODY()

    /** Enable validation result caching */
    UPROPERTY(EditAnywhere, Config, Category = "Cache")
    bool bEnableCaching = true;

    /** Cache TTL in seconds */
    UPROPERTY(EditAnywhere, Config, Category = "Cache")
    float CacheTTLSeconds = 5.0f;

    /** Maximum cache entries */
    UPROPERTY(EditAnywhere, Config, Category = "Cache")
    int32 MaxCacheEntries = 1000;

    /** Enable parallel rule evaluation */
    UPROPERTY(EditAnywhere, Config, Category = "Performance")
    bool bEnableParallelEvaluation = false;

    /** Broadcast validation events via EventBus */
    UPROPERTY(EditAnywhere, Config, Category = "Events")
    bool bBroadcastValidationEvents = true;

    /** Log detailed validation results */
    UPROPERTY(EditAnywhere, Config, Category = "Debug")
    bool bLogDetailedResults = false;

    static FRulesServiceConfig LoadFromConfig(const FString& ConfigSection = TEXT("EquipmentRules"));
};

//========================================
// RULES SERVICE METRICS
//========================================

struct EQUIPMENTSYSTEM_API FRulesServiceMetrics
{
    TAtomic<uint64> TotalEvaluations{0};
    TAtomic<uint64> CacheHits{0};
    TAtomic<uint64> CacheMisses{0};
    TAtomic<uint64> ValidationsPassed{0};
    TAtomic<uint64> ValidationsFailed{0};
    TAtomic<uint64> AverageEvaluationTimeUs{0};
    TAtomic<uint64> PeakEvaluationTimeUs{0};

    FString ToString() const;
    void Reset();

    float GetCacheHitRate() const
    {
        const uint64 Total = CacheHits.Load() + CacheMisses.Load();
        return Total > 0 ? (static_cast<float>(CacheHits.Load()) / Total) * 100.0f : 0.0f;
    }
};

//========================================
// RULES SERVICE
//========================================

/**
 * Equipment Rules Service
 *
 * Single Responsibility: Equipment validation rules orchestration
 * - Owns and manages RulesCoordinator
 * - Provides IEquipmentService lifecycle
 * - Integrates with EventBus for validation events
 * - Caches validation results for performance
 *
 * Extracted from ValidationService to separate concerns:
 * - ValidationService: Orchestrates validation pipeline, threading, batching
 * - RulesService: Pure rule evaluation logic
 */
UCLASS()
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentRulesService : public UObject,
    public IEquipmentService,
    public ISuspenseCoreEquipmentRules
{
    GENERATED_BODY()

public:
    USuspenseCoreEquipmentRulesService();
    virtual ~USuspenseCoreEquipmentRulesService();

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

    //~ Begin ISuspenseCoreEquipmentRules Interface (delegated to RulesCoordinator)
    virtual FRuleEvaluationResult EvaluateRules(const FEquipmentOperationRequest& Operation) const override;
    virtual FRuleEvaluationResult EvaluateRulesWithContext(
        const FEquipmentOperationRequest& Operation,
        const FSuspenseCoreRuleContext& Context) const override;
    virtual FRuleEvaluationResult CheckItemCompatibility(
        const FSuspenseCoreInventoryItemInstance& ItemInstance,
        const FEquipmentSlotConfig& SlotConfig) const override;
    virtual FRuleEvaluationResult CheckCharacterRequirements(
        const AActor* Character,
        const FSuspenseCoreInventoryItemInstance& ItemInstance) const override;
    virtual FRuleEvaluationResult CheckWeightLimit(
        float CurrentWeight,
        float AdditionalWeight) const override;
    virtual FRuleEvaluationResult CheckConflictingEquipment(
        const TArray<FSuspenseCoreInventoryItemInstance>& ExistingItems,
        const FSuspenseCoreInventoryItemInstance& NewItem) const override;
    virtual TArray<FEquipmentRule> GetActiveRules() const override;
    virtual bool RegisterRule(const FEquipmentRule& Rule) override;
    virtual bool UnregisterRule(const FGameplayTag& RuleTag) override;
    virtual bool SetRuleEnabled(const FGameplayTag& RuleTag, bool bEnabled) override;
    virtual FString GenerateComplianceReport(const FEquipmentStateSnapshot& CurrentState) const override;
    virtual void ClearRuleCache() override;
    virtual bool Initialize(TScriptInterface<ISuspenseCoreEquipmentDataProvider> InDataProvider) override;
    virtual void ResetStatistics() override;
    //~ End ISuspenseCoreEquipmentRules Interface

    /** Get underlying coordinator (for advanced use) */
    USuspenseCoreRulesCoordinator* GetCoordinator() const { return RulesCoordinator; }

    /** Get metrics */
    const FRulesServiceMetrics& GetMetrics() const { return Metrics; }

    /** Get configuration */
    const FRulesServiceConfig& GetConfiguration() const { return Config; }

    /** Invalidate cache for specific operation type */
    UFUNCTION(BlueprintCallable, Category = "Rules")
    void InvalidateCache(EEquipmentOperationType OperationType = EEquipmentOperationType::None);

protected:
    // Service state
    EServiceLifecycleState ServiceState = EServiceLifecycleState::Uninitialized;
    FServiceInitParams ServiceParams;

    // Configuration
    FRulesServiceConfig Config;

    // Core rules coordinator (owned)
    UPROPERTY()
    USuspenseCoreRulesCoordinator* RulesCoordinator = nullptr;

    // EventBus integration
    TWeakPtr<FSuspenseCoreEquipmentEventBus> EventBus;
    TArray<FEventSubscriptionHandle> EventSubscriptions;

    // Event tags
    FGameplayTag Tag_Validation_Started;
    FGameplayTag Tag_Validation_Passed;
    FGameplayTag Tag_Validation_Failed;

    // Validation result cache
    struct FCachedResult
    {
        FRuleEvaluationResult Result;
        double CacheTime;
        uint32 RequestHash;
    };
    mutable TMap<uint32, FCachedResult> ResultCache;
    mutable FCriticalSection CacheLock;

    // Metrics
    mutable FRulesServiceMetrics Metrics;
    mutable FServiceMetrics ServiceMetrics;

private:
    // Cache helpers
    uint32 ComputeRequestHash(const FEquipmentOperationRequest& Request) const;
    bool TryGetCachedResult(uint32 Hash, FRuleEvaluationResult& OutResult) const;
    void CacheResult(uint32 Hash, const FRuleEvaluationResult& Result) const;
    void CleanupExpiredCache() const;

    // EventBus helpers
    void SetupEventBus();
    void TeardownEventBus();
    void BroadcastValidationStarted(const FEquipmentOperationRequest& Request) const;
    void BroadcastValidationResult(const FEquipmentOperationRequest& Request, const FRuleEvaluationResult& Result) const;

    // Metrics
    void UpdateMetrics(double EvaluationStartTime, bool bPassed) const;
};
