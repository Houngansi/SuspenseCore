// SuspenseRulesTypes.h
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryBaseTypes.h"
#include "SuspenseCoreRulesTypes.generated.h"

/**
 * Rule type enumeration for categorization
 */
UENUM(BlueprintType)
enum class ESuspenseRuleType : uint8
{
    Weight          UMETA(DisplayName = "Weight Rules"),
    Requirement     UMETA(DisplayName = "Requirement Rules"),
    Conflict        UMETA(DisplayName = "Conflict Rules"),
    Compatibility   UMETA(DisplayName = "Compatibility Rules"),
    Slot           UMETA(DisplayName = "Slot Rules"),
    Stacking       UMETA(DisplayName = "Stacking Rules"),
    Custom         UMETA(DisplayName = "Custom Rules")
};

/**
 * Severity level for rule violations
 */
UENUM(BlueprintType)
enum class ESuspenseRuleSeverity : uint8
{
    Info        UMETA(DisplayName = "Information"),
    Warning     UMETA(DisplayName = "Warning"),
    Error       UMETA(DisplayName = "Error"),
    Critical    UMETA(DisplayName = "Critical")
};

/**
 * Conflict resolution strategies
 */
UENUM(BlueprintType)
enum class ESuspenseConflictResolution : uint8
{
    Reject      UMETA(DisplayName = "Reject Operation"),
    Replace     UMETA(DisplayName = "Replace Existing"),
    Stack       UMETA(DisplayName = "Stack Items"),
    Prompt      UMETA(DisplayName = "Prompt User"),
    Auto        UMETA(DisplayName = "Auto Resolve")
};

/**
 * Extended rule check result with detailed information
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseRuleCheckResult
{
    GENERATED_BODY()

    /** Whether the rule passed */
    UPROPERTY(BlueprintReadOnly, Category = "Rules")
    bool bPassed = true;

    /** Severity if rule failed */
    UPROPERTY(BlueprintReadOnly, Category = "Rules")
    ESuspenseRuleSeverity Severity = ESuspenseRuleSeverity::Info;

    /** Human-readable message */
    UPROPERTY(BlueprintReadOnly, Category = "Rules")
    FText Message;

    /** Rule identifier */
    UPROPERTY(BlueprintReadOnly, Category = "Rules")
    FGameplayTag RuleTag;

    /** Rule type for categorization */
    UPROPERTY(BlueprintReadOnly, Category = "Rules")
    ESuspenseRuleType RuleType = ESuspenseRuleType::Custom;

    /** Additional context data */
    UPROPERTY(BlueprintReadOnly, Category = "Rules")
    TMap<FString, FString> Context;

    /** Confidence score (0.0 - 1.0) */
    UPROPERTY(BlueprintReadOnly, Category = "Rules")
    float ConfidenceScore = 1.0f;

    /** Can this rule be overridden? */
    UPROPERTY(BlueprintReadOnly, Category = "Rules")
    bool bCanOverride = false;

    /** Helper to create success result */
    static FSuspenseRuleCheckResult Success(const FText& InMessage = FText::GetEmpty())
    {
        FSuspenseRuleCheckResult Result;
        Result.bPassed = true;
        Result.Message = InMessage;
        Result.ConfidenceScore = 1.0f;
        return Result;
    }

    /** Helper to create failure result */
    static FSuspenseRuleCheckResult Failure(const FText& InMessage, ESuspenseRuleSeverity InSeverity = ESuspenseRuleSeverity::Error)
    {
        FSuspenseRuleCheckResult Result;
        Result.bPassed = false;
        Result.Message = InMessage;
        Result.Severity = InSeverity;
        Result.ConfidenceScore = 0.0f;
        return Result;
    }
};

/**
 * Aggregated rule evaluation result
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseAggregatedRuleResult
{
    GENERATED_BODY()

    /** Overall pass/fail */
    UPROPERTY(BlueprintReadOnly, Category = "Rules")
    bool bAllPassed = true;

    /** Individual rule results */
    UPROPERTY(BlueprintReadOnly, Category = "Rules")
    TArray<FSuspenseRuleCheckResult> Results;

    /** Critical failures that must be addressed */
    UPROPERTY(BlueprintReadOnly, Category = "Rules")
    TArray<FSuspenseRuleCheckResult> CriticalFailures;

    /** Warnings that don't block operation */
    UPROPERTY(BlueprintReadOnly, Category = "Rules")
    TArray<FSuspenseRuleCheckResult> Warnings;

    /** Combined confidence score */
    UPROPERTY(BlueprintReadOnly, Category = "Rules")
    float CombinedConfidence = 1.0f;

    /** Primary failure reason for UI */
    UPROPERTY(BlueprintReadOnly, Category = "Rules")
    FText PrimaryFailureReason;

    /** Add a rule result to aggregation */
    void AddResult(const FSuspenseRuleCheckResult& Result)
    {
        Results.Add(Result);

        if (!Result.bPassed)
        {
            bAllPassed = false;

            if (Result.Severity == ESuspenseRuleSeverity::Critical)
            {
                CriticalFailures.Add(Result);
                if (PrimaryFailureReason.IsEmpty())
                {
                    PrimaryFailureReason = Result.Message;
                }
            }
            else if (Result.Severity == ESuspenseRuleSeverity::Warning)
            {
                Warnings.Add(Result);
            }
        }

        // Update combined confidence
        CombinedConfidence *= Result.ConfidenceScore;
    }

    /** Check if there are any critical issues */
    bool HasCriticalIssues() const
    {
        return CriticalFailures.Num() > 0;
    }

    /** Get detailed report string */
    FString GetDetailedReport() const
    {
        FString Report;

        if (bAllPassed)
        {
            Report = TEXT("All rules passed successfully");
        }
        else
        {
            Report = FString::Printf(TEXT("Rules check failed: %d critical, %d warnings\n"),
                CriticalFailures.Num(), Warnings.Num());

            for (const auto& Critical : CriticalFailures)
            {
                Report += FString::Printf(TEXT("  [CRITICAL] %s\n"), *Critical.Message.ToString());
            }

            for (const auto& Warning : Warnings)
            {
                Report += FString::Printf(TEXT("  [WARNING] %s\n"), *Warning.Message.ToString());
            }
        }

        return Report;
    }
};

/**
 * Rule evaluation context with all necessary data
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseRuleContext
{
    GENERATED_BODY()

    /** Character being evaluated */
    UPROPERTY(BlueprintReadWrite, Category = "Context")
    AActor* Character = nullptr;

    /** Item being evaluated */
    UPROPERTY(BlueprintReadWrite, Category = "Context")
    FSuspenseInventoryItemInstance ItemInstance;

    /** Target slot index */
    UPROPERTY(BlueprintReadWrite, Category = "Context")
    int32 TargetSlotIndex = INDEX_NONE;

    /** Current equipped items */
    UPROPERTY(BlueprintReadWrite, Category = "Context")
    TArray<FSuspenseInventoryItemInstance> CurrentItems;

    /** Force operation even with warnings */
    UPROPERTY(BlueprintReadWrite, Category = "Context")
    bool bForceOperation = false;

    /** Additional metadata */
    UPROPERTY(BlueprintReadWrite, Category = "Context")
    TMap<FString, FString> Metadata;
};

