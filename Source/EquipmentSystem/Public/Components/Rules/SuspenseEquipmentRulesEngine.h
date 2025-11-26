// SuspenseEquipmentRulesEngine.h
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/Equipment/ISuspenseEquipmentRules.h"
#include "Interfaces/Equipment/ISuspenseEquipmentDataProvider.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Types/Loadout/SuspenseLoadoutSettings.h"
#include "GameplayTagContainer.h"
#include "SuspenseEquipmentRulesEngine.generated.h"

/**
 * Rule execution context
 */
USTRUCT()
struct FRuleExecutionContext
{
    GENERATED_BODY()

    /** Character being evaluated */
    UPROPERTY()
    AActor* Character = nullptr;

    /** Current equipment state */
    UPROPERTY()
    FEquipmentStateSnapshot CurrentState;

    /** Operation being evaluated */
    UPROPERTY()
    FEquipmentOperationRequest Operation;

    /** Execution timestamp */
    UPROPERTY()
    float Timestamp = 0.0f;

    /** Additional context data */
    TMap<FString, FString> Metadata;
};

/**
 * Rule violation record
 */
USTRUCT()
struct FRuleViolation
{
    GENERATED_BODY()

    /** Rule that was violated */
    UPROPERTY()
    FSuspenseEquipmentRule ViolatedRule;

    /** Violation details */
    UPROPERTY()
    FSuspenseRuleEvaluationResult EvaluationResult;

    /** When violation occurred */
    UPROPERTY()
    FDateTime ViolationTime;

    /** Context of violation */
    UPROPERTY()
    FString Context;

    /** Severity level (0-10) */
    UPROPERTY()
    int32 Severity = 5;
};

/**
 * Rule statistics
 */
USTRUCT()
struct FRuleStatistics
{
    GENERATED_BODY()

    /** Total evaluations */
    UPROPERTY()
    int32 TotalEvaluations = 0;

    /** Passed evaluations */
    UPROPERTY()
    int32 PassedEvaluations = 0;

    /** Failed evaluations */
    UPROPERTY()
    int32 FailedEvaluations = 0;

    /** Average evaluation time (ms) */
    UPROPERTY()
    float AverageEvaluationTime = 0.0f;

    /** Last evaluation time */
    UPROPERTY()
    FDateTime LastEvaluationTime;
};

/**
 * Character requirements data
 */
USTRUCT()
struct FCharacterRequirements
{
    GENERATED_BODY()

    /** Required level */
    UPROPERTY()
    int32 RequiredLevel = 0;

    /** Required attributes */
    UPROPERTY()
    TMap<FName, float> RequiredAttributes;

    /** Required gameplay tags */
    UPROPERTY()
    FGameplayTagContainer RequiredTags;

    /** Required abilities */
    UPROPERTY()
    TArray<TSubclassOf<class UGameplayAbility>> RequiredAbilities;

    /** Required certifications/achievements */
    UPROPERTY()
    TArray<FName> RequiredCertifications;
};

/**
 * Weight limit configuration
 */
USTRUCT()
struct FWeightLimitConfig
{
    GENERATED_BODY()

    /** Base weight limit */
    UPROPERTY()
    float BaseWeightLimit = 100.0f;

    /** Weight limit per strength point */
    UPROPERTY()
    float WeightPerStrength = 5.0f;

    /** Encumbrance thresholds */
    UPROPERTY()
    TMap<float, FGameplayTag> EncumbranceThresholds;

    /** Current encumbrance effects */
    UPROPERTY()
    TArray<TSubclassOf<class UGameplayEffect>> EncumbranceEffects;
};

/**
 * Equipment Rules Engine Component (DEVELOPMENT FALLBACK ONLY)
 *
 * IMPORTANT: This is a legacy monolithic rules engine kept for development/debugging purposes only.
 * Production path uses USuspenseRulesCoordinator with specialized engines for better performance
 * and maintainability.
 *
 * Philosophy: Centralized business rules engine for equipment validation.
 * Evaluates complex conditions, enforces game rules, and provides detailed feedback.
 *
 * Key Design Principles:
 * - DEV TOOL ONLY - disabled by default in production
 * - Rule-based validation with priority and precedence
 * - Extensible rule system with custom expressions
 * - Comprehensive conflict detection
 * - Performance optimization through caching
 * - Detailed violation tracking and reporting
 * - Support for soft and hard constraints
 * - Integration with game systems (attributes, abilities, etc.)
 *
 * Thread Safety: Safe for concurrent reads after initialization
 */
UCLASS(ClassGroup=(Equipment), meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseEquipmentRulesEngine : public UActorComponent, public ISuspenseEquipmentRules
{
    GENERATED_BODY()

public:
    USuspenseEquipmentRulesEngine();
    virtual ~USuspenseEquipmentRulesEngine();

    //~ Begin UActorComponent Interface
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    //~ End UActorComponent Interface

    //========================================
    // ISuspenseEquipmentRules Implementation (DEV FALLBACK)
    //========================================

    virtual FSuspenseRuleEvaluationResult EvaluateRules(const FEquipmentOperationRequest& Operation) const override;
    virtual FSuspenseRuleEvaluationResult EvaluateRulesWithContext(
        const FEquipmentOperationRequest& Operation,
        const FSuspenseRuleContext& Context) const override;
    virtual FSuspenseRuleEvaluationResult CheckItemCompatibility(
        const FSuspenseInventoryItemInstance& ItemInstance,
        const FEquipmentSlotConfig& SlotConfig) const override;
    virtual FSuspenseRuleEvaluationResult CheckCharacterRequirements(
        const AActor* Character,
        const FSuspenseInventoryItemInstance& ItemInstance) const override;
    virtual FSuspenseRuleEvaluationResult CheckWeightLimit(
        float CurrentWeight,
        float AdditionalWeight) const override;
    virtual FSuspenseRuleEvaluationResult CheckConflictingEquipment(
        const TArray<FSuspenseInventoryItemInstance>& ExistingItems,
        const FSuspenseInventoryItemInstance& NewItem) const override;
    virtual TArray<FSuspenseEquipmentRule> GetActiveRules() const override;
    virtual bool RegisterRule(const FSuspenseEquipmentRule& Rule) override;
    virtual bool UnregisterRule(const FGameplayTag& RuleTag) override;
    virtual bool SetRuleEnabled(const FGameplayTag& RuleTag, bool bEnabled) override;
    virtual FString GenerateComplianceReport(const FEquipmentStateSnapshot& CurrentState) const override;

    //========================================
    // Extended Rule Management (DEV MODE)
    //========================================

    /**
     * Initialize rules engine with data provider
     * @param InDataProvider Data provider interface
     * @return True if initialized
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Rules")
    bool Initialize(TScriptInterface<ISuspenseEquipmentDataProvider> InDataProvider) override;

    /**
     * Load rules from data table
     * @param RulesTable Data table with rules
     * @return Number of rules loaded
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Rules")
    int32 LoadRulesFromDataTable(class UDataTable* RulesTable);

    /**
     * Evaluate specific rule
     * @param RuleTag Rule to evaluate
     * @param Context Execution context
     * @return Evaluation result
     */
    //UFUNCTION(BlueprintCallable, Category = "Equipment|Rules")
    FSuspenseRuleEvaluationResult EvaluateSpecificRule(const FGameplayTag& RuleTag, const FRuleExecutionContext& Context) const;

    /**
     * Batch evaluate rules
     * @param RuleTags Rules to evaluate
     * @param Context Execution context
     * @return Array of results
     */
    //UFUNCTION(BlueprintCallable, Category = "Equipment|Rules")
    TArray<FSuspenseRuleEvaluationResult> BatchEvaluateRules(const TArray<FGameplayTag>& RuleTags, const FRuleExecutionContext& Context) const;

    /**
     * Clear all rules
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Rules")
    void ClearAllRules();

    /**
     * Reset rule statistics
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Rules")
    void ResetStatistics() override;

    //========================================
    // Development Controls
    //========================================

    /**
     * Enable/disable dev fallback mode
     * @param bEnabled Enable dev mode
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Dev")
    void SetDevFallbackEnabled(bool bEnabled);

    /**
     * Check if dev fallback is active
     * @return True if dev mode active
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Dev")
    bool IsDevFallbackEnabled() const;

    //========================================
    // Advanced Validation (DEV MODE)
    //========================================

    /**
     * Check item durability requirements
     * @param ItemInstance Item to check
     * @return Evaluation result
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Validation")
    FSuspenseRuleEvaluationResult CheckItemDurability(const FSuspenseInventoryItemInstance& ItemInstance) const;

    /**
     * Check ammunition compatibility
     * @param WeaponInstance Weapon item
     * @param AmmoInstance Ammo item
     * @return Evaluation result
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Validation")
    FSuspenseRuleEvaluationResult CheckAmmoCompatibility(
        const FSuspenseInventoryItemInstance& WeaponInstance,
        const FSuspenseInventoryItemInstance& AmmoInstance) const;

    /**
     * Check modification compatibility
     * @param BaseItem Base item
     * @param Modification Modification item
     * @return Evaluation result
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Validation")
    FSuspenseRuleEvaluationResult CheckModificationCompatibility(
        const FSuspenseInventoryItemInstance& BaseItem,
        const FSuspenseInventoryItemInstance& Modification) const;

    /**
     * Validate loadout configuration
     * @param LoadoutItems Items in loadout
     * @return Evaluation result
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Validation")
    FSuspenseRuleEvaluationResult ValidateLoadout(const TArray<FSuspenseInventoryItemInstance>& LoadoutItems) const;

    //========================================
    // Character Requirements (DEV MODE)
    //========================================

    /**
     * Get character requirements for item
     * @param ItemInstance Item to check
     * @return Requirements data
     */
    //UFUNCTION(BlueprintCallable, Category = "Equipment|Requirements")
    FCharacterRequirements GetItemRequirements(const FSuspenseInventoryItemInstance& ItemInstance) const;

    /**
     * Check if character meets requirements
     * @param Character Character to check
     * @param Requirements Requirements to meet
     * @return Evaluation result
     */
    //UFUNCTION(BlueprintCallable, Category = "Equipment|Requirements")
    FSuspenseRuleEvaluationResult CheckCharacterMeetsRequirements(
        const AActor* Character,
        const FCharacterRequirements& Requirements) const;

    /**
     * Calculate character's weight capacity
     * @param Character Character to check
     * @return Weight capacity
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Requirements")
    float CalculateWeightCapacity(const AActor* Character) const;

    //========================================
    // Conflict Detection (DEV MODE)
    //========================================

    /**
     * Find all conflicts for item
     * @param ItemInstance Item to check
     * @param CurrentItems Current equipment
     * @return Array of conflicts
     */
    //UFUNCTION(BlueprintCallable, Category = "Equipment|Conflicts")
    TArray<FRuleViolation> FindItemConflicts(
        const FSuspenseInventoryItemInstance& ItemInstance,
        const TArray<FSuspenseInventoryItemInstance>& CurrentItems) const;

    /**
     * Resolve equipment conflicts
     * @param Conflicts Conflicts to resolve
     * @param ResolutionStrategy Strategy (0=Remove conflicting, 1=Abort, 2=Force)
     * @return True if resolved
     */
    //UFUNCTION(BlueprintCallable, Category = "Equipment|Conflicts")
    bool ResolveConflicts(const TArray<FRuleViolation>& Conflicts, int32 ResolutionStrategy);

    //========================================
    // Reporting and Analytics (DEV MODE)
    //========================================

    /**
     * Get rule statistics
     * @param RuleTag Rule to get stats for
     * @return Statistics
     */
    //UFUNCTION(BlueprintCallable, Category = "Equipment|Analytics")
    FRuleStatistics GetRuleStatistics(const FGameplayTag& RuleTag) const;

    /**
     * Get all violations
     * @param MaxCount Maximum violations to return
     * @return Array of violations
     */
    //UFUNCTION(BlueprintCallable, Category = "Equipment|Analytics")
    TArray<FRuleViolation> GetViolationHistory(int32 MaxCount = 100) const;

    /**
     * Export rules to JSON
     * @return JSON string
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Analytics")
    FString ExportRulesToJSON() const;

    /**
     * Import rules from JSON
     * @param JSONString JSON data
     * @return Number of rules imported
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Analytics")
    int32 ImportRulesFromJSON(const FString& JSONString);

    /**
     * Get debug information
     * @return Debug string
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Debug")
    FString GetDebugInfo() const;

protected:
    //========================================
    // Internal Rule Evaluation (DEV MODE)
    //========================================

    /**
     * Check if dev fallback mode is active (CVar + property)
     * @return True if dev mode should be used
     */
    bool ShouldUseDevFallback() const;

    /**
     * Create disabled result for production mode
     * @param MethodName Method that was called
     * @return Disabled result
     */
    FSuspenseRuleEvaluationResult CreateDisabledResult(const FString& MethodName) const;

    /**
     * Evaluate rule expression
     * @param Expression Rule expression
     * @param Context Execution context
     * @return True if expression passes
     */
    bool EvaluateExpression(const FString& Expression, const FRuleExecutionContext& Context) const;

    /**
     * Parse rule expression
     * @param Expression Expression to parse
     * @param OutTokens Output tokens
     * @return True if parsed successfully
     */
    bool ParseExpression(const FString& Expression, TArray<FString>& OutTokens) const;

    /**
     * Execute rule logic
     * @param Rule Rule to execute
     * @param Context Execution context
     * @return Evaluation result
     */
    FSuspenseRuleEvaluationResult ExecuteRule(const FSuspenseEquipmentRule& Rule, const FRuleExecutionContext& Context) const;

    /**
     * Check rule preconditions
     * @param Rule Rule to check
     * @param Context Execution context
     * @return True if preconditions met
     */
    bool CheckPreconditions(const FSuspenseEquipmentRule& Rule, const FRuleExecutionContext& Context) const;

    /**
     * Apply rule priority
     * @param Rules Rules to prioritize
     * @return Sorted rules
     */
    TArray<FSuspenseEquipmentRule> PrioritizeRules(const TArray<FSuspenseEquipmentRule>& Rules) const;

    /**
     * Cache rule result
     * @param RuleTag Rule tag
     * @param Result Result to cache
     */
    void CacheRuleResult(const FGameplayTag& RuleTag, const FSuspenseRuleEvaluationResult& Result) const;

    /**
     * Get cached rule result
     * @param RuleTag Rule tag
     * @param OutResult Output result
     * @return True if found in cache
     */
    bool GetCachedResult(const FGameplayTag& RuleTag, FSuspenseRuleEvaluationResult& OutResult) const;

    //========================================
    // Helper Methods
    //========================================

    /**
     * Get item data from provider (no world access)
     * @param ItemID Item identifier
     * @param OutItemData Output data
     * @return True if found
     */
    bool GetItemData(FName ItemID, struct FSuspenseUnifiedItemData& OutItemData) const;

    /**
     * Get character level
     * @param Character Character to check
     * @return Character level
     */
    int32 GetCharacterLevel(const AActor* Character) const;

    /**
     * Get character attributes
     * @param Character Character to check
     * @return Attribute map
     */
    TMap<FName, float> GetCharacterAttributes(const AActor* Character) const;

    /**
     * Get character tags
     * @param Character Character to check
     * @return Gameplay tags
     */
    FGameplayTagContainer GetCharacterTags(const AActor* Character) const;

    /**
     * Calculate total equipment weight
     * @param Items Items to calculate
     * @return Total weight
     */
    float CalculateTotalWeight(const TArray<FSuspenseInventoryItemInstance>& Items) const;

    /**
     * Record violation
     * @param Violation Violation to record
     */
    void RecordViolation(const FRuleViolation& Violation) const;

    /**
     * Update statistics
     * @param RuleTag Rule that was evaluated
     * @param bPassed Whether evaluation passed
     * @param EvaluationTime Time taken (ms)
     */
    void UpdateStatistics(const FGameplayTag& RuleTag, bool bPassed, float EvaluationTime) const;

private:
    //========================================
    // Rule Storage
    //========================================

    /** All registered rules */
    UPROPERTY()
    TMap<FGameplayTag, FSuspenseEquipmentRule> RegisteredRules;

    /** Enabled rules */
    UPROPERTY()
    TSet<FGameplayTag> EnabledRules;

    /** Rule priorities */
    UPROPERTY()
    TMap<FGameplayTag, int32> RulePriorities;

    /** Rule dependencies */
    UPROPERTY()
    TMap<FGameplayTag, FGameplayTagContainer> RuleDependencies;

    //========================================
    // Configuration
    //========================================

    /** Data provider interface - single source of truth */
    UPROPERTY()
    TScriptInterface<ISuspenseEquipmentDataProvider> DataProvider;

    /** Weight limit configuration */
    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    FWeightLimitConfig WeightConfig;

    /** Maximum rule evaluation depth */
    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    int32 MaxEvaluationDepth = 10;

    /** Enable rule caching */
    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    bool bEnableCaching = true;

    /** Cache duration in seconds */
    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    float CacheDuration = 5.0f;

    /** Enable detailed logging */
    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    bool bEnableDetailedLogging = false;

    /** Maximum violation history */
    UPROPERTY(EditDefaultsOnly, Category = "Configuration")
    int32 MaxViolationHistory = 1000;

    /** Enable dev fallback mode (default: false for production) */
    UPROPERTY(EditDefaultsOnly, Category = "Development", meta = (DisplayName = "Enable Dev Fallback Mode"))
    bool bDevFallbackEnabled = false;

    //========================================
    // Runtime Data
    //========================================

    /** Violation history */
    UPROPERTY()
    mutable TArray<FRuleViolation> ViolationHistory;

    /** Rule statistics */
    mutable TMap<FGameplayTag, FRuleStatistics> RuleStats;

    /** Result cache */
    mutable TMap<uint32, FSuspenseRuleEvaluationResult> ResultCache;

    /** Cache timestamps */
    mutable TMap<uint32, float> CacheTimestamps;

    /** Current evaluation depth */
    mutable int32 CurrentEvaluationDepth = 0;

    /** Critical section for thread safety */
    mutable FCriticalSection RuleCriticalSection;

    //========================================
    // State
    //========================================

    /** Is engine initialized */
    UPROPERTY()
    bool bIsInitialized = false;

    /** Engine version */
    UPROPERTY()
    int32 EngineVersion = 1;

    /** Last update time */
    UPROPERTY()
    FDateTime LastUpdateTime;

    /**
     * Register default rules
     */
    void RegisterDefaultRules();
};
