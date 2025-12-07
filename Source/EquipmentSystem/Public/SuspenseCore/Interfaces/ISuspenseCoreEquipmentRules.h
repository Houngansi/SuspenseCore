// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Types/Equipment/SuspenseEquipmentTypes.h"
#include "Types/Rules/SuspenseRulesTypes.h"
#include "ISuspenseCoreEquipmentRules.generated.h"

// Forward declarations
class ISuspenseCoreEquipmentDataProvider;

/**
 * Rule evaluation result - SuspenseCore version
 */
USTRUCT(BlueprintType)
struct EQUIPMENTSYSTEM_API FSuspenseCoreRuleResult
{
    GENERATED_BODY()

    /** Whether the rule passed */
    UPROPERTY(BlueprintReadOnly, Category = "Rules")
    bool bPassed = false;

    /** Reason for failure or success message */
    UPROPERTY(BlueprintReadOnly, Category = "Rules")
    FText FailureReason;

    /** Type of rule that was evaluated */
    UPROPERTY(BlueprintReadOnly, Category = "Rules")
    FGameplayTag RuleType;

    /** Confidence in the result (0.0 - 1.0) */
    UPROPERTY(BlueprintReadOnly, Category = "Rules")
    float ConfidenceScore = 1.0f;

    /** Additional details and context */
    UPROPERTY(BlueprintReadOnly, Category = "Rules")
    TArray<FString> Details;

    /** Default constructor */
    FSuspenseCoreRuleResult()
    {
        bPassed = false;
        ConfidenceScore = 1.0f;
        FailureReason = FText::FromString(TEXT("No evaluation performed"));
    }

    /** Create success result */
    static FSuspenseCoreRuleResult Success(const FText& Message = FText::FromString(TEXT("Rule passed")))
    {
        FSuspenseCoreRuleResult Result;
        Result.bPassed = true;
        Result.FailureReason = Message;
        Result.ConfidenceScore = 1.0f;
        return Result;
    }

    /** Create failure result */
    static FSuspenseCoreRuleResult Failure(const FText& Reason, float Confidence = 0.0f)
    {
        FSuspenseCoreRuleResult Result;
        Result.bPassed = false;
        Result.FailureReason = Reason;
        Result.ConfidenceScore = Confidence;
        return Result;
    }

    /** Check if passed */
    bool IsValid() const { return bPassed; }
};

/**
 * Equipment rule definition - SuspenseCore version
 */
USTRUCT(BlueprintType)
struct EQUIPMENTSYSTEM_API FSuspenseCoreEquipmentRule
{
    GENERATED_BODY()

    /** Unique rule identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
    FGameplayTag RuleTag;

    /** Rule expression or condition */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
    FString RuleExpression;

    /** Execution priority (higher = earlier) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
    int32 Priority = 0;

    /** Is this a strict rule (hard failure) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
    bool bIsStrict = true;

    /** Human-readable description */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
    FText Description;

    /** Default constructor */
    FSuspenseCoreEquipmentRule()
    {
        Priority = 0;
        bIsStrict = true;
        Description = FText::FromString(TEXT("Equipment rule"));
    }
};

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreEquipmentRules : public UInterface
{
    GENERATED_BODY()
};

/**
 * SuspenseCore Equipment Rules Interface
 *
 * Provides contract for equipment rules evaluation engine.
 * Replaces legacy ISuspenseEquipmentRules with SuspenseCore naming.
 *
 * Contract:
 * - Pure validation (no state changes, no notifications)
 * - Thread-safe for concurrent Evaluate* calls after initialization
 * - EvaluateRulesWithContext MUST NOT read live DataProvider
 */
class EQUIPMENTSYSTEM_API ISuspenseCoreEquipmentRules
{
    GENERATED_BODY()

public:
    //========================================
    // Primary Evaluation Interface
    //========================================

    /**
     * Evaluate all rules for operation using LIVE provider
     * @param Operation Equipment operation to validate
     * @return Evaluation result
     */
    virtual FSuspenseCoreRuleResult EvaluateRules(const FEquipmentOperationRequest& Operation) const = 0;

    /**
     * Evaluate rules using EXPLICIT context (snapshot).
     * MUST NOT access live DataProvider.
     * @param Operation Equipment operation to validate
     * @param Context Explicit rule context with snapshot data
     * @return Evaluation result
     */
    virtual FSuspenseCoreRuleResult EvaluateRulesWithContext(
        const FEquipmentOperationRequest& Operation,
        const FSuspenseRuleContext& Context) const = 0;

    //========================================
    // Specialized Checkers
    //========================================

    /**
     * Check item compatibility with equipment slot
     */
    virtual FSuspenseCoreRuleResult CheckItemCompatibility(
        const FSuspenseInventoryItemInstance& ItemInstance,
        const FEquipmentSlotConfig& SlotConfig) const = 0;

    /**
     * Check character requirements for item
     */
    virtual FSuspenseCoreRuleResult CheckCharacterRequirements(
        const AActor* Character,
        const FSuspenseInventoryItemInstance& ItemInstance) const = 0;

    /**
     * Check weight capacity limits
     */
    virtual FSuspenseCoreRuleResult CheckWeightLimit(
        float CurrentWeight,
        float AdditionalWeight) const = 0;

    /**
     * Check for equipment conflicts
     */
    virtual FSuspenseCoreRuleResult CheckConflictingEquipment(
        const TArray<FSuspenseInventoryItemInstance>& ExistingItems,
        const FSuspenseInventoryItemInstance& NewItem) const = 0;

    //========================================
    // Runtime Rule Management
    //========================================

    /**
     * Get currently active rules
     */
    virtual TArray<FSuspenseCoreEquipmentRule> GetActiveRules() const = 0;

    /**
     * Register new equipment rule
     */
    virtual bool RegisterRule(const FSuspenseCoreEquipmentRule& Rule) = 0;

    /**
     * Unregister equipment rule
     */
    virtual bool UnregisterRule(const FGameplayTag& RuleTag) = 0;

    /**
     * Enable or disable specific rule
     */
    virtual bool SetRuleEnabled(const FGameplayTag& RuleTag, bool bEnabled) = 0;

    //========================================
    // Reporting and Diagnostics
    //========================================

    /**
     * Generate comprehensive compliance report
     */
    virtual FString GenerateComplianceReport(const FEquipmentStateSnapshot& CurrentState) const = 0;

    //========================================
    // Optional Implementation Hooks
    //========================================

    /** Clear internal caches */
    virtual void ClearRuleCache() {}

    /** Initialize with data provider */
    virtual bool Initialize(TScriptInterface<ISuspenseCoreEquipmentDataProvider> DataProvider) { return true; }

    /** Reset performance statistics */
    virtual void ResetStatistics() {}

    //========================================
    // Utility Methods
    //========================================

    /** Check if rules engine is initialized */
    virtual bool IsInitialized() const { return true; }

    /** Get engine version or type identifier */
    virtual FString GetEngineInfo() const { return TEXT("SuspenseCore Rules Engine"); }

    /** Get performance metrics */
    virtual TMap<FString, FString> GetPerformanceMetrics() const { return TMap<FString, FString>(); }
};
