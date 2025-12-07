// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Interfaces/Equipment/ISuspenseEquipmentService.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreEquipmentServiceMacros.h"
#include "SuspenseCoreEquipmentValidationService.generated.h"

// Forward declarations
class USuspenseEquipmentServiceLocator;
class USuspenseCoreEventBus;
class ISuspenseEquipmentRules;
struct FEquipmentOperationRequest;
struct FSlotValidationResult;

/**
 * Validation rule delegate
 * Returns true if validation passes, false otherwise
 */
DECLARE_DELEGATE_RetVal_OneParam(bool, FEquipmentValidationRule, const FEquipmentOperationRequest&);

/**
 * SuspenseCoreEquipmentValidationService
 *
 * Philosophy:
 * Centralizes all equipment validation rules and business logic.
 * Ensures consistent validation across the entire equipment system.
 *
 * Key Responsibilities:
 * - Equipment operation validation
 * - Slot compatibility checking
 * - Weight/capacity validation
 * - Requirement verification
 * - Conflict detection
 * - Custom rule registration
 * - Validation result caching
 *
 * Architecture Patterns:
 * - Event Bus: Publishes validation events
 * - Dependency Injection: Uses ServiceLocator
 * - GameplayTags: Rule categorization
 * - Strategy Pattern: Pluggable validation rules
 * - Cache: Validation result caching for performance
 *
 * Validation Rule Categories:
 * - Slot Rules: Slot compatibility, occupancy
 * - Weight Rules: Weight limits, capacity
 * - Requirement Rules: Level, stats, permissions
 * - Compatibility Rules: Item type compatibility
 * - Conflict Rules: Mutually exclusive items
 */
UCLASS(BlueprintType)
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentValidationService : public UObject, public IEquipmentValidationService
{
	GENERATED_BODY()

public:
	USuspenseCoreEquipmentValidationService();
	virtual ~USuspenseCoreEquipmentValidationService();

	//========================================
	// ISuspenseEquipmentService Interface
	//========================================

	virtual bool InitializeService(const FServiceInitParams& Params) override;
	virtual bool ShutdownService(bool bForce = false) override;
	virtual EServiceLifecycleState GetServiceState() const override;
	virtual bool IsServiceReady() const override;
	virtual FGameplayTag GetServiceTag() const override;
	virtual FGameplayTagContainer GetRequiredDependencies() const override;
	virtual bool ValidateService(TArray<FText>& OutErrors) const override;
	virtual void ResetService() override;
	virtual FString GetServiceStats() const override;

	//========================================
	// IEquipmentValidationService Interface
	//========================================

	virtual ISuspenseEquipmentRules* GetRulesEngine() override;
	virtual bool RegisterValidator(const FGameplayTag& ValidatorTag, TFunction<bool(const void*)> Validator) override;
	virtual void ClearValidationCache() override;

	//========================================
	// Validation Operations
	//========================================

	/**
	 * Validate equipment operation
	 * Checks all applicable rules and returns detailed result
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Validation")
	FSlotValidationResult ValidateOperation(const FEquipmentOperationRequest& Request);

	/**
	 * Validate slot compatibility
	 * Checks if item can be placed in slot
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Validation")
	bool ValidateSlotCompatibility(int32 SlotIndex, const struct FSuspenseInventoryItemInstance& Item, FText& OutReason) const;

	/**
	 * Validate weight constraints
	 * Checks if adding item would exceed weight limit
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Validation")
	bool ValidateWeightLimit(const struct FSuspenseInventoryItemInstance& Item, FText& OutReason) const;

	/**
	 * Validate requirements
	 * Checks level, stats, permissions requirements
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Validation")
	bool ValidateRequirements(const struct FSuspenseInventoryItemInstance& Item, FText& OutReason) const;

	/**
	 * Validate conflicts
	 * Checks for mutually exclusive items
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Validation")
	bool ValidateNoConflicts(const struct FSuspenseInventoryItemInstance& Item, FText& OutReason) const;

	/**
	 * Batch validate multiple operations
	 * More efficient than individual validations
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Validation")
	TArray<FSlotValidationResult> BatchValidateOperations(const TArray<FEquipmentOperationRequest>& Requests);

	//========================================
	// Rule Management
	//========================================

	/**
	 * Register custom validation rule
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Validation")
	bool RegisterValidationRule(FGameplayTag RuleTag, const FEquipmentValidationRule& Rule);

	/**
	 * Unregister validation rule
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Validation")
	bool UnregisterValidationRule(FGameplayTag RuleTag);

	/**
	 * Enable/disable validation rule
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Validation")
	void SetRuleEnabled(FGameplayTag RuleTag, bool bEnabled);

	/**
	 * Check if rule is enabled
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Equipment|Validation")
	bool IsRuleEnabled(FGameplayTag RuleTag) const;

	/**
	 * Get all registered rules
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Validation")
	TArray<FGameplayTag> GetRegisteredRules() const;

	//========================================
	// Cache Management
	//========================================

	/**
	 * Clear validation cache for specific request
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Validation")
	void InvalidateCacheForRequest(const FEquipmentOperationRequest& Request);

	/**
	 * Get cache statistics
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Validation")
	FString GetCacheStatistics() const;

	//========================================
	// Event Publishing
	//========================================

	/**
	 * Publish validation failed event
	 */
	void PublishValidationFailed(const FEquipmentOperationRequest& Request, const FText& Reason);

	/**
	 * Publish validation succeeded event
	 */
	void PublishValidationSucceeded(const FEquipmentOperationRequest& Request);

	/**
	 * Publish rules changed event
	 */
	void PublishRulesChanged(FGameplayTag RuleTag);

protected:
	//========================================
	// Service Lifecycle
	//========================================

	/** Initialize validation rules */
	bool InitializeValidationRules();

	/** Setup event subscriptions */
	void SetupEventSubscriptions();

	/** Cleanup resources */
	void CleanupResources();

	//========================================
	// Validation Logic
	//========================================

	/** Execute all enabled rules for operation */
	bool ExecuteValidationRules(const FEquipmentOperationRequest& Request, FText& OutReason);

	/** Check single validation rule */
	bool CheckRule(FGameplayTag RuleTag, const FEquipmentOperationRequest& Request, FText& OutReason) const;

	/** Generate cache key for request */
	uint32 GenerateCacheKey(const FEquipmentOperationRequest& Request) const;

	//========================================
	// Event Handlers
	//========================================

	/** Handle data changed event - invalidate affected cache entries */
	void OnDataChanged(const struct FSuspenseEquipmentEventData& EventData);

	/** Handle configuration changed event */
	void OnConfigurationChanged(const struct FSuspenseEquipmentEventData& EventData);

private:
	//========================================
	// Service State
	//========================================

	/** Current service lifecycle state */
	UPROPERTY()
	EServiceLifecycleState ServiceState;

	/** Service initialization timestamp */
	UPROPERTY()
	FDateTime InitializationTime;

	//========================================
	// Dependencies (via ServiceLocator)
	//========================================

	/** ServiceLocator for dependency injection */
	UPROPERTY()
	TWeakObjectPtr<USuspenseEquipmentServiceLocator> ServiceLocator;

	/** EventBus for event publishing */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> EventBus;

	/** Data service for current state queries */
	UPROPERTY()
	TWeakObjectPtr<UObject> DataService;

	//========================================
	// Validation Rules
	//========================================

	/** Registered validation rules */
	UPROPERTY()
	TMap<FGameplayTag, FEquipmentValidationRule> ValidationRules;

	/** Enabled/disabled state for rules */
	UPROPERTY()
	TMap<FGameplayTag, bool> RuleEnabledStates;

	//========================================
	// Validation Cache
	//========================================

	/** Cached validation results */
	UPROPERTY()
	TMap<uint32, FSlotValidationResult> ValidationCache;

	/** Cache timestamps for TTL */
	UPROPERTY()
	TMap<uint32, FDateTime> CacheTimestamps;

	//========================================
	// Thread Safety
	//========================================

	/** Lock for validation rules */
	mutable FRWLock RulesLock;

	/** Lock for cache access */
	mutable FRWLock CacheLock;

	//========================================
	// Configuration
	//========================================

	/** Enable validation caching */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	bool bEnableCaching;

	/** Cache TTL in seconds */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	float CacheTTL;

	/** Enable detailed logging */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	bool bEnableDetailedLogging;

	/** Strict validation mode */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	bool bStrictValidation;

	//========================================
	// Statistics
	//========================================

	/** Total validations performed */
	UPROPERTY()
	int32 TotalValidations;

	/** Total validations passed */
	UPROPERTY()
	int32 TotalValidationsPassed;

	/** Total validations failed */
	UPROPERTY()
	int32 TotalValidationsFailed;

	/** Cache hits */
	UPROPERTY()
	int32 CacheHits;

	/** Cache misses */
	UPROPERTY()
	int32 CacheMisses;
};
