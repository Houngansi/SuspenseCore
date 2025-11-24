// MedComRulesCoordinator.h
// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Interfaces/Equipment/IMedComEquipmentRules.h"
#include "GameplayTagContainer.h"
#include "Types/Rules/MedComRulesTypes.h"
#include "MedComRulesCoordinator.generated.h"

class UMedComWeightRulesEngine;
class UMedComRequirementRulesEngine;
class UMedComConflictRulesEngine;
class UMedComCompatibilityRulesEngine;
class IMedComEquipmentDataProvider;

// Forward declarations
struct FEquipmentStateSnapshot;

/**
 * Rule execution priority levels
 */
UENUM()
enum class ERuleExecutionPriority : uint8
{
    Critical = 0,    // Must pass for operation to proceed (compatibility, safety)
    High     = 1,    // Important validations (requirements, prerequisites)  
    Normal   = 2,    // Standard checks (weight, capacity)
    Low      = 3     // Advisory checks (conflicts, set bonuses)
};

/**
 * Rule engine registration data
 */
USTRUCT()
struct FRuleEngineRegistration
{
    GENERATED_BODY()

    /** Engine type identifier */
    UPROPERTY()
    FGameplayTag EngineType;

    /** Engine instance */
    UPROPERTY()
    UObject* Engine = nullptr;

    /** Execution priority */
    UPROPERTY()
    ERuleExecutionPriority Priority = ERuleExecutionPriority::Normal;

    /** Is engine enabled */
    UPROPERTY()
    bool bEnabled = true;
};

/**
 * Production Rules Coordinator - STATELESS для глобального использования
 * 
 * АРХИТЕКТУРНЫЙ ПРИНЦИП:
 * Coordinator не хранит состояние игрока - он работает как чистая функция.
 * Все данные игрока передаются через FMedComRuleContext.
 * DataProvider опционален и используется только для вспомогательных операций.
 * 
 * Philosophy: Orchestrates specialized rule engines in priority order.
 * Single point of truth for equipment validation with optimized pipeline.
 * 
 * Key Principles:
 * - Pipeline execution: Compatibility → Requirements → Weight → Conflict  
 * - Stateless operation: все данные в контексте
 * - DataProvider опционален (для fallback-операций)
 * - Early termination on critical failures
 * - Performance metrics and reporting
 * - Thread-safe for concurrent rule evaluation
 * 
 * Thread Safety: Safe for concurrent rule evaluations after initialization
 */
UCLASS(BlueprintType)
class MEDCOMEQUIPMENT_API UMedComRulesCoordinator : public UObject, public IMedComEquipmentRules
{
    GENERATED_BODY()

public:
    UMedComRulesCoordinator();

    //========================================
    // IMedComEquipmentRules Implementation
    //========================================
    
    virtual FRuleEvaluationResult EvaluateRules(const FEquipmentOperationRequest& Operation) const override;
    virtual FRuleEvaluationResult EvaluateRulesWithContext(
        const FEquipmentOperationRequest& Operation,
        const FMedComRuleContext& Context) const override;
    virtual FRuleEvaluationResult CheckItemCompatibility(
        const FInventoryItemInstance& ItemInstance,
        const FEquipmentSlotConfig& SlotConfig) const override;
    virtual FRuleEvaluationResult CheckCharacterRequirements(
        const AActor* Character,
        const FInventoryItemInstance& ItemInstance) const override;
    virtual FRuleEvaluationResult CheckWeightLimit(
        float CurrentWeight,
        float AdditionalWeight) const override;
    virtual FRuleEvaluationResult CheckConflictingEquipment(
        const TArray<FInventoryItemInstance>& ExistingItems,
        const FInventoryItemInstance& NewItem) const override;
    virtual TArray<FEquipmentRule> GetActiveRules() const override;
    virtual bool RegisterRule(const FEquipmentRule& Rule) override;
    virtual bool UnregisterRule(const FGameplayTag& RuleTag) override;
    virtual bool SetRuleEnabled(const FGameplayTag& RuleTag, bool bEnabled) override;
    virtual FString GenerateComplianceReport(const FEquipmentStateSnapshot& CurrentState) const override;
    virtual void ClearRuleCache() override;
    
    /**
     * Initialize coordinator - DataProvider теперь ОПЦИОНАЛЕН
     * @param InDataProvider Optional data provider (может быть nullptr для stateless режима)
     * @return true если инициализация успешна
     */
    virtual bool Initialize(TScriptInterface<IMedComEquipmentDataProvider> InDataProvider) override;
    virtual void ResetStatistics() override;

    //========================================
    // Engine Management
    //========================================
    
    /**
     * Register external rule engine
     * @param EngineType Engine type identifier
     * @param Engine Engine instance
     * @param Priority Execution priority
     * @return True if registered successfully
     */
    UFUNCTION(BlueprintCallable, Category="Rules")
    bool RegisterRuleEngine(const FGameplayTag& EngineType, UObject* Engine, ERuleExecutionPriority Priority);

    /**
     * Unregister rule engine
     * @param EngineType Engine type to remove
     * @return True if unregistered successfully
     */
    UFUNCTION(BlueprintCallable, Category="Rules")
    bool UnregisterRuleEngine(const FGameplayTag& EngineType);

    /**
     * Get all registered engines
     * @return Array of registered engines sorted by priority
     */
    TArray<FRuleEngineRegistration> GetRegisteredEngines() const;

    /**
     * Enable/disable specific engine
     * @param EngineType Engine type to modify
     * @param bEnabled Enable state
     * @return True if modified successfully
     */
    UFUNCTION(BlueprintCallable, Category="Rules")
    bool SetEngineEnabled(const FGameplayTag& EngineType, bool bEnabled);

    //========================================
    // Performance and Diagnostics
    //========================================
    
    /**
     * Get execution statistics
     * @return Performance metrics map
     */
    UFUNCTION(BlueprintCallable, Category="Rules")
    TMap<FString, FString> GetExecutionStatistics() const;
    
    /**
     * Get pipeline health status
     * @return Health check results
     */
    UFUNCTION(BlueprintCallable, Category="Rules")
    FString GetPipelineHealth() const;

protected:
    /**
     * Create and initialize specialized engines
     */
    void CreateSpecializedEngines();

    /**
     * Build shadow snapshot from context or DataProvider (fallback)
     * КРИТИЧЕСКОЕ ИЗМЕНЕНИЕ: работает БЕЗ обязательного DataProvider
     * @param Context Rule evaluation context с CurrentItems
     * @return Equipment state snapshot
     */
    FEquipmentStateSnapshot BuildShadowSnapshotFromContext(const FMedComRuleContext& Context) const;

    /**
     * Record engine execution metrics
     * @param EngineType Engine that was executed
     * @param DurationMs Execution time in milliseconds
     */
    void RecordEngineMetrics(const FGameplayTag& EngineType, double DurationMs) const;

    //========================================
    // Result Conversion (Legacy Compatibility)
    //========================================
    
    FRuleEvaluationResult ConvertToLegacyResult(const TArray<FMedComRuleCheckResult>& NewResults) const;
    FRuleEvaluationResult ConvertSingleResult(const FMedComRuleCheckResult& NewResult) const;
    FRuleEvaluationResult ConvertAggregatedResult(const FMedComAggregatedRuleResult& AggResult) const;
    TArray<FRuleEngineRegistration> GetSortedEngines() const;

private:
    //========================================
    // Core Components
    //========================================
    
    /** 
     * Data provider interface - ОПЦИОНАЛЕН
     * Используется только для fallback-операций, когда контекст не полон
     */
    UPROPERTY()
    TScriptInterface<IMedComEquipmentDataProvider> DataProvider;

    /** Specialized rule engines */
    UPROPERTY()
    UMedComWeightRulesEngine* WeightEngine = nullptr;

    UPROPERTY()
    UMedComRequirementRulesEngine* RequirementEngine = nullptr;

    UPROPERTY()
    UMedComConflictRulesEngine* ConflictEngine = nullptr;

    UPROPERTY()
    UMedComCompatibilityRulesEngine* CompatibilityEngine = nullptr;

    /** Registry of all engines */
    UPROPERTY()
    TMap<FGameplayTag, FRuleEngineRegistration> RegisteredEngines;

    //========================================
    // Global Rule System (Legacy Support)
    //========================================
    
    /** Global rules (for coordinator-level logic) */
    UPROPERTY()
    TArray<FEquipmentRule> GlobalRules;

    /** Disabled global rules */
    UPROPERTY()
    TSet<FGameplayTag> DisabledRules;

    //========================================
    // Performance Optimization
    //========================================
    
    /** Cached weight engine configuration for slot filtering */
    UPROPERTY(Transient)
    FGameplayTagContainer ExcludedSlotsCache;

    //========================================
    // Metrics (Thread-Safe)
    //========================================
    
    /** Engine execution counts */
    UPROPERTY(Transient)
    mutable TMap<FGameplayTag, int64> EngineExecCount;

    /** Engine execution times (milliseconds) */
    UPROPERTY(Transient)
    mutable TMap<FGameplayTag, double> EngineExecTimeMs;

    /** Total coordinator evaluations */
    UPROPERTY(Transient)
    mutable int64 TotalEvaluations = 0;

    /** Accumulated evaluation time */
    UPROPERTY(Transient)
    mutable double AccumulatedEvalMs = 0.0;

    /** Critical section for thread safety */
    mutable FCriticalSection RulesLock;

    //========================================
    // State Tracking
    //========================================
    
    /** Initialization timestamp */
    UPROPERTY(Transient)
    FDateTime InitializationTime;

    /** Last pipeline execution timestamp (optional) */
    UPROPERTY(Transient)
    mutable TOptional<FDateTime> LastExecutionTime;
};