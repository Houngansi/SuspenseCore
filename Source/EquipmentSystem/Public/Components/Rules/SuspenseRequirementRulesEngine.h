// SuspenseRequirementRulesEngine.h
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "Types/Rules/SuspenseRulesTypes.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "SuspenseRequirementRulesEngine.generated.h"

/**
 * Comparison operators for requirement checks.
 */
UENUM(BlueprintType)
enum class ESuspenseComparisonOp : uint8
{
    Equal           UMETA(DisplayName="=="),
    NotEqual        UMETA(DisplayName="!="),
    Greater         UMETA(DisplayName=">"),
    GreaterOrEqual  UMETA(DisplayName=">="),
    Less            UMETA(DisplayName="<"),
    LessOrEqual     UMETA(DisplayName="<=")
};

/**
 * Single attribute requirement (ASC-backed; reflection-based lookup).
 */
USTRUCT(BlueprintType)
struct FMedComAttributeRequirement
{
    GENERATED_BODY()

    /** Attribute name to read from ASC AttributeSets (float/int supported) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Requirement")
    FName AttributeName;

    /** Required value to compare against */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Requirement")
    float RequiredValue = 0.0f;

    /** Comparison operator */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Requirement")
    ESuspenseComparisonOp ComparisonOp = ESuspenseComparisonOp::GreaterOrEqual;

    /** Optional display name for UI */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Requirement")
    FText DisplayName;
};

/**
 * Complete item requirements bundle.
 * NOTE: The engine never mutates this structure.
 */
USTRUCT(BlueprintType)
struct FMedComItemRequirements
{
    GENERATED_BODY()

    /** Minimum character level to equip/use */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Requirements")
    int32 RequiredLevel = 0;

    /** Required character class/role */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Requirements", meta=(Categories="Character.Class"))
    FGameplayTag RequiredClass;

    /** Required gameplay tags on the character */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Requirements")
    FGameplayTagContainer RequiredTags;

    /** Attribute requirements (ASC-backed) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Requirements")
    TArray<FMedComAttributeRequirement> AttributeRequirements;

    /** Required abilities (must be granted on ASC) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Requirements")
    TArray<TSubclassOf<class UGameplayAbility>> RequiredAbilities;

    /** Required quest completions (integration-specific) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Requirements")
    TArray<FName> RequiredQuests;

    /** Required certifications/achievements (integration-specific) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Requirements")
    TArray<FName> RequiredCertifications;
};

/** Custom requirement validator delegate (extensibility hook) */
DECLARE_DELEGATE_RetVal_TwoParams(bool, FCustomRequirementValidator,
    const AActor* /*Character*/,
    const FString& /*Parameters*/);

/**
 * Rules engine responsible for owner/context requirements.
 * SRP: read-only evaluation against ASC/Tags/Abilities; no writes.
 */
UCLASS(BlueprintType)
class EQUIPMENTSYSTEM_API USuspenseRequirementRulesEngine : public UObject
{
    GENERATED_BODY()
public:
    USuspenseRequirementRulesEngine();

    /** Check all requirements; short-circuits on Error/Critical failure of a rule. */
    UFUNCTION(BlueprintCallable, Category="Requirement Rules")
    FSuspenseAggregatedRuleResult CheckAllRequirements(
        const AActor* Character,
        const FMedComItemRequirements& Requirements) const;

    /** Evaluate requirements using rule context (best-effort; read-only). */
    UFUNCTION(BlueprintCallable, Category="Requirement Rules")
    FSuspenseAggregatedRuleResult EvaluateRequirementRules(
        const FSuspenseRuleContext& Context) const;

    //==================== Primitive checks ====================

    UFUNCTION(BlueprintCallable, Category="Requirement Rules")
    FSuspenseRuleCheckResult CheckCharacterLevel(
        const AActor* Character,
        int32 RequiredLevel) const;

    UFUNCTION(BlueprintCallable, Category="Requirement Rules")
    FSuspenseRuleCheckResult CheckSkillLevel(
        const AActor* Character,
        const FGameplayTag& SkillTag,
        int32 RequiredLevel) const;

    UFUNCTION(BlueprintCallable, Category="Requirement Rules")
    FSuspenseRuleCheckResult CheckAttributeRequirements(
        const AActor* Character,
        const TArray<FMedComAttributeRequirement>& Requirements) const;

    UFUNCTION(BlueprintCallable, Category="Requirement Rules")
    FSuspenseRuleCheckResult CheckSingleAttribute(
        const AActor* Character,
        const FName& AttributeName,
        float RequiredValue,
        ESuspenseComparisonOp Op) const;

    UFUNCTION(BlueprintCallable, Category="Requirement Rules")
    FSuspenseRuleCheckResult CheckCharacterTags(
        const AActor* Character,
        const FGameplayTagContainer& RequiredTags) const;

    UFUNCTION(BlueprintCallable, Category="Requirement Rules")
    FSuspenseRuleCheckResult CheckRequiredAbilities(
        const AActor* Character,
        const TArray<TSubclassOf<class UGameplayAbility>>& RequiredAbilities) const;

    //==================== Progress / Estimation ====================

    /** 0..1 fraction of satisfied requirements (non-critical, best-effort). */
    UFUNCTION(BlueprintCallable, Category="Requirement Rules")
    float CalculateRequirementProgress(
        const AActor* Character,
        const FMedComItemRequirements& Requirements) const;

    /** Returns -1.0f if estimation is not supported. */
    UFUNCTION(BlueprintCallable, Category="Requirement Rules")
    float EstimateTimeToMeetRequirements(
        const AActor* Character,
        const FMedComItemRequirements& Requirements) const;

    //==================== Custom validators ====================

    void RegisterCustomRequirement(const FGameplayTag& RuleTag, FCustomRequirementValidator Validator);
    void UnregisterCustomRequirement(const FGameplayTag& RuleTag);

    UFUNCTION(BlueprintCallable, Category="Requirement Rules")
    FSuspenseRuleCheckResult CheckCustomRequirement(
        const AActor* Character,
        const FGameplayTag& RequirementTag,
        const FString& Parameters) const;

    //==================== Cache and Statistics ====================

    /** Clear any cached data (stateless engine - no-op) */
    UFUNCTION(BlueprintCallable, Category="Requirement Rules")
    void ClearCache();

    /** Reset internal statistics (stateless engine - no-op) */
    UFUNCTION(BlueprintCallable, Category="Requirement Rules")
    void ResetStatistics();

protected:
    //==================== Data access helpers (read-only) ====================

    /** Best-effort level read from ASC AttributeSets (defaults to 0 if missing). */
    int32 GetCharacterLevel(const AActor* Character) const;

    /** Reflective float attribute read across ASC AttributeSets; returns 0.0f if missing. */
    float GetAttributeValue(const AActor* Character, const FName& AttributeName) const;

    /** Owned tags from ASC or IGameplayTagAssetInterface. */
    FGameplayTagContainer GetCharacterTags(const AActor* Character) const;

    /** Numeric comparer. */
    bool CompareValues(float Value1, float Value2, ESuspenseComparisonOp Op) const;

private:
    /** Custom requirement validators (thread-safe map). */
    TMap<FGameplayTag, FCustomRequirementValidator> CustomValidators;

    /** Guard for validators map. */
    mutable FCriticalSection ValidatorLock;
};
