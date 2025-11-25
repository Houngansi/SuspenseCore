// ISuspenseEquipmentRules.h
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Types/Equipment/SuspenseEquipmentTypes.h"
#include "Types/Rules/SuspenseRulesTypes.h"
#include "ISuspenseEquipmentRules.generated.h"

/**
 * Rule evaluation result (Legacy format for backward compatibility)
 */
USTRUCT(BlueprintType)
struct FRuleEvaluationResult
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
    FRuleEvaluationResult()
    {
        bPassed = false;
        ConfidenceScore = 1.0f;
        FailureReason = FText::FromString(TEXT("No evaluation performed"));
    }

    /** Create success result */
    static FRuleEvaluationResult Success(const FText& Message = FText::FromString(TEXT("Rule passed")))
    {
        FRuleEvaluationResult Result;
        Result.bPassed = true;
        Result.FailureReason = Message;
        Result.ConfidenceScore = 1.0f;
        return Result;
    }

    /** Create failure result */
    static FRuleEvaluationResult Failure(const FText& Reason, float Confidence = 0.0f)
    {
        FRuleEvaluationResult Result;
        Result.bPassed = false;
        Result.FailureReason = Reason;
        Result.ConfidenceScore = Confidence;
        return Result;
    }
};

/**
 * Equipment rule definition (Legacy format)
 */
USTRUCT(BlueprintType)
struct FEquipmentRule
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
    FEquipmentRule()
    {
        Priority = 0;
        bIsStrict = true;
        Description = FText::FromString(TEXT("Equipment rule"));
    }
};

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseEquipmentRules : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for equipment rules engine (Facade over specialized engines)
 *
 * Contract:
 * - Pure validation (no state changes, no notifications)
 * - Thread-safe for concurrent Evaluate* calls after initialization
 * - EvaluateRulesWithContext MUST NOT read live DataProvider; it uses provided Context
 * - Legacy compatibility maintained through result conversion
 * 
 * Production Implementation: UMedComRulesCoordinator
 * Development Fallback: UMedComEquipmentRulesEngine (disabled by default)
 */
class BRIDGESYSTEM_API ISuspenseEquipmentRules
{
    GENERATED_BODY()

public:
    //========================================
    // Primary Evaluation Interface
    //========================================

    /**
     * Evaluate all rules for operation using LIVE provider (legacy/compat path)
     * @param Operation Equipment operation to validate
     * @return Legacy evaluation result
     */
    virtual FRuleEvaluationResult EvaluateRules(const FEquipmentOperationRequest& Operation) const = 0;

    /** 
     * Evaluate all rules for operation using EXPLICIT context (shadow snapshot).
     * MUST NOT access live DataProvider inside implementation - uses provided context only.
     * @param Operation Equipment operation to validate
     * @param Context Explicit rule context with snapshot data
     * @return Legacy evaluation result
     */
    virtual FRuleEvaluationResult EvaluateRulesWithContext(
        const FEquipmentOperationRequest& Operation,
        const FMedComRuleContext& Context) const = 0;

    //========================================
    // Specialized Checkers (Direct Usage)
    //========================================

    /**
     * Check item compatibility with equipment slot
     * @param ItemInstance Item to check
     * @param SlotConfig Target slot configuration
     * @return Compatibility check result
     */
    virtual FRuleEvaluationResult CheckItemCompatibility(
        const FSuspenseInventoryItemInstance& ItemInstance,
        const FEquipmentSlotConfig& SlotConfig) const = 0;

    /**
     * Check character requirements for item
     * @param Character Character actor
     * @param ItemInstance Item to check requirements for
     * @return Requirements check result
     */
    virtual FRuleEvaluationResult CheckCharacterRequirements(
        const AActor* Character,
        const FSuspenseInventoryItemInstance& ItemInstance) const = 0;

    /**
     * Check weight capacity limits
     * @param CurrentWeight Current carried weight
     * @param AdditionalWeight Additional weight being added
     * @return Weight limit check result
     */
    virtual FRuleEvaluationResult CheckWeightLimit(
        float CurrentWeight,
        float AdditionalWeight) const = 0;

    /**
     * Check for equipment conflicts
     * @param ExistingItems Currently equipped items
     * @param NewItem New item being equipped
     * @return Conflict check result
     */
    virtual FRuleEvaluationResult CheckConflictingEquipment(
        const TArray<FSuspenseInventoryItemInstance>& ExistingItems,
        const FSuspenseInventoryItemInstance& NewItem) const = 0;

    //========================================
    // Runtime Rule Management (Legacy)
    //========================================

    /**
     * Get currently active rules
     * @return Array of active equipment rules
     */
    virtual TArray<FEquipmentRule> GetActiveRules() const = 0;

    /**
     * Register new equipment rule
     * @param Rule Rule definition to register
     * @return True if registered successfully
     */
    virtual bool RegisterRule(const FEquipmentRule& Rule) = 0;

    /**
     * Unregister equipment rule
     * @param RuleTag Rule identifier to remove
     * @return True if unregistered successfully
     */
    virtual bool UnregisterRule(const FGameplayTag& RuleTag) = 0;

    /**
     * Enable or disable specific rule
     * @param RuleTag Rule identifier
     * @param bEnabled New enabled state
     * @return True if modified successfully
     */
    virtual bool SetRuleEnabled(const FGameplayTag& RuleTag, bool bEnabled) = 0;

    //========================================
    // Reporting and Diagnostics
    //========================================

    /**
     * Generate comprehensive compliance report
     * @param CurrentState Current equipment state snapshot
     * @return Formatted compliance report
     */
    virtual FString GenerateComplianceReport(const FEquipmentStateSnapshot& CurrentState) const = 0;

    //========================================
    // Optional Implementation Hooks
    //========================================

    /**
     * Clear internal caches (optional)
     */
    virtual void ClearRuleCache() {}

    /**
     * Initialize with data provider (optional)
     * @param DataProvider Equipment data provider interface
     * @return True if initialized successfully
     */
    virtual bool Initialize(TScriptInterface<class ISuspenseEquipmentDataProvider> DataProvider) { return true; }

    /**
     * Reset performance statistics (optional)
     */
    virtual void ResetStatistics() {}

    //========================================
    // Utility Methods
    //========================================

    /**
     * Check if rules engine is properly initialized
     * @return True if ready for use
     */
    virtual bool IsInitialized() const { return true; }

    /**
     * Get engine version or type identifier
     * @return Engine identification string
     */
    virtual FString GetEngineInfo() const { return TEXT("Generic Rules Engine"); }

    /**
     * Get performance metrics
     * @return Key-value pairs of performance data
     */
    virtual TMap<FString, FString> GetPerformanceMetrics() const { return TMap<FString, FString>(); }
};