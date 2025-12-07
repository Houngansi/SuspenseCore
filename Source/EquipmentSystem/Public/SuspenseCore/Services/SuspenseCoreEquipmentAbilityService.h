// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Interfaces/Equipment/ISuspenseEquipmentService.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreEquipmentServiceMacros.h"
#include "SuspenseCoreEquipmentAbilityService.generated.h"

// Forward declarations
class USuspenseEquipmentServiceLocator;
class USuspenseCoreEventBus;
class UAbilitySystemComponent;
class UGameplayAbility;
class UGameplayEffect;
struct FGameplayAbilitySpec;
struct FActiveGameplayEffectHandle;

/**
 * SuspenseCoreEquipmentAbilityService
 *
 * Philosophy:
 * Integrates equipment system with Gameplay Ability System (GAS).
 * Manages ability grants, effect applications, and attribute modifications.
 *
 * Key Responsibilities:
 * - Grant abilities when equipment is equipped
 * - Remove abilities when equipment is unequipped
 * - Apply gameplay effects from equipment
 * - Manage attribute modifiers
 * - Handle ability cooldowns and costs
 * - Sync equipment state with GAS
 *
 * Architecture Patterns:
 * - Event Bus: Subscribes to equipment events
 * - Dependency Injection: Uses ServiceLocator
 * - GameplayTags: Ability/Effect identification
 * - GAS Integration: Full Gameplay Ability System integration
 * - Observer Pattern: Reacts to equipment changes
 *
 * GAS Integration Flow:
 * 1. Equipment equipped → Grant abilities/effects
 * 2. Equipment unequipped → Remove abilities/effects
 * 3. Equipment modified → Update ability specs
 * 4. Ability activated → Track cooldowns/costs
 * 5. Effect expired → Notify equipment system
 *
 * Best Practices:
 * - All gameplay logic uses GAS (no direct stat modification)
 * - GameplayTags for ability identification
 * - GameplayEffects for stat modifications
 * - Proper replication support
 * - Clean ability/effect lifecycle management
 */
UCLASS(BlueprintType)
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentAbilityService : public UObject, public ISuspenseEquipmentService
{
	GENERATED_BODY()

public:
	USuspenseCoreEquipmentAbilityService();
	virtual ~USuspenseCoreEquipmentAbilityService();

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
	// Ability Management
	//========================================

	/**
	 * Grant abilities from equipment
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Abilities")
	TArray<struct FGameplayAbilitySpecHandle> GrantEquipmentAbilities(int32 SlotIndex, const struct FSuspenseInventoryItemInstance& Item);

	/**
	 * Remove abilities from equipment
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Abilities")
	bool RemoveEquipmentAbilities(int32 SlotIndex);

	/**
	 * Get granted ability handles for slot
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Equipment|Abilities")
	TArray<struct FGameplayAbilitySpecHandle> GetAbilitiesForSlot(int32 SlotIndex) const;

	/**
	 * Check if ability is granted from equipment
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Equipment|Abilities")
	bool IsAbilityFromEquipment(const struct FGameplayAbilitySpecHandle& AbilityHandle) const;

	/**
	 * Try activate equipment ability
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Abilities")
	bool TryActivateEquipmentAbility(int32 SlotIndex, FGameplayTag AbilityTag);

	/**
	 * Cancel equipment ability
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Abilities")
	void CancelEquipmentAbility(int32 SlotIndex, FGameplayTag AbilityTag);

	//========================================
	// Gameplay Effect Management
	//========================================

	/**
	 * Apply gameplay effects from equipment
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Effects")
	TArray<FActiveGameplayEffectHandle> ApplyEquipmentEffects(int32 SlotIndex, const struct FSuspenseInventoryItemInstance& Item);

	/**
	 * Remove gameplay effects from equipment
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Effects")
	bool RemoveEquipmentEffects(int32 SlotIndex);

	/**
	 * Get active effect handles for slot
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Equipment|Effects")
	TArray<FActiveGameplayEffectHandle> GetEffectsForSlot(int32 SlotIndex) const;

	/**
	 * Update effect magnitude
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Effects")
	bool UpdateEffectMagnitude(FActiveGameplayEffectHandle EffectHandle, float NewMagnitude);

	/**
	 * Get effect remaining duration
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Equipment|Effects")
	float GetEffectRemainingDuration(FActiveGameplayEffectHandle EffectHandle) const;

	//========================================
	// Attribute Management
	//========================================

	/**
	 * Apply attribute modifiers from equipment
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Attributes")
	bool ApplyAttributeModifiers(int32 SlotIndex, const struct FSuspenseInventoryItemInstance& Item);

	/**
	 * Remove attribute modifiers from equipment
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Attributes")
	bool RemoveAttributeModifiers(int32 SlotIndex);

	/**
	 * Get attribute value with equipment modifiers
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Equipment|Attributes")
	float GetAttributeValue(FGameplayAttribute Attribute) const;

	/**
	 * Calculate total modifier for attribute from all equipment
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Equipment|Attributes")
	float CalculateTotalModifier(FGameplayAttribute Attribute) const;

	//========================================
	// GAS Component Access
	//========================================

	/**
	 * Set ability system component
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment|Abilities")
	void SetAbilitySystemComponent(UAbilitySystemComponent* ASC);

	/**
	 * Get ability system component
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Equipment|Abilities")
	UAbilitySystemComponent* GetAbilitySystemComponent() const;

	//========================================
	// Event Publishing
	//========================================

	/**
	 * Publish ability granted event
	 */
	void PublishAbilityGranted(int32 SlotIndex, FGameplayTag AbilityTag);

	/**
	 * Publish ability removed event
	 */
	void PublishAbilityRemoved(int32 SlotIndex, FGameplayTag AbilityTag);

	/**
	 * Publish effect applied event
	 */
	void PublishEffectApplied(int32 SlotIndex, FGameplayTag EffectTag);

	/**
	 * Publish effect removed event
	 */
	void PublishEffectRemoved(int32 SlotIndex, FGameplayTag EffectTag);

protected:
	//========================================
	// Service Lifecycle
	//========================================

	/** Initialize GAS integration */
	bool InitializeGASIntegration();

	/** Setup event subscriptions */
	void SetupEventSubscriptions();

	/** Cleanup granted abilities and effects */
	void CleanupGrantedAbilitiesAndEffects();

	//========================================
	// Ability Operations
	//========================================

	/** Grant single ability */
	struct FGameplayAbilitySpecHandle GrantAbilityInternal(TSubclassOf<UGameplayAbility> AbilityClass, int32 Level);

	/** Remove single ability */
	bool RemoveAbilityInternal(const struct FGameplayAbilitySpecHandle& AbilityHandle);

	/** Get ability specs for equipment item */
	TArray<TSubclassOf<UGameplayAbility>> GetAbilityClassesForItem(const struct FSuspenseInventoryItemInstance& Item) const;

	//========================================
	// Effect Operations
	//========================================

	/** Apply single effect */
	FActiveGameplayEffectHandle ApplyEffectInternal(TSubclassOf<UGameplayEffect> EffectClass, float Level);

	/** Remove single effect */
	bool RemoveEffectInternal(FActiveGameplayEffectHandle EffectHandle);

	/** Get effect specs for equipment item */
	TArray<TSubclassOf<UGameplayEffect>> GetEffectClassesForItem(const struct FSuspenseInventoryItemInstance& Item) const;

	//========================================
	// Event Handlers
	//========================================

	/** Handle equipment equipped event */
	void OnEquipmentEquipped(const struct FSuspenseEquipmentEventData& EventData);

	/** Handle equipment unequipped event */
	void OnEquipmentUnequipped(const struct FSuspenseEquipmentEventData& EventData);

	/** Handle equipment modified event */
	void OnEquipmentModified(const struct FSuspenseEquipmentEventData& EventData);

	/** Handle ability activated */
	void OnAbilityActivated(UGameplayAbility* Ability);

	/** Handle ability ended */
	void OnAbilityEnded(UGameplayAbility* Ability);

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

	/** EventBus for event subscription/publishing */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> EventBus;

	/** Data service for equipment state queries */
	UPROPERTY()
	TWeakObjectPtr<UObject> DataService;

	//========================================
	// GAS Integration
	//========================================

	/** Ability System Component */
	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/** Map of slot index to granted ability handles */
	UPROPERTY()
	TMap<int32, TArray<struct FGameplayAbilitySpecHandle>> GrantedAbilities;

	/** Map of slot index to active effect handles */
	UPROPERTY()
	TMap<int32, TArray<FActiveGameplayEffectHandle>> ActiveEffects;

	/** Map of ability handle to slot index (reverse lookup) */
	UPROPERTY()
	TMap<struct FGameplayAbilitySpecHandle, int32> AbilityToSlotMap;

	//========================================
	// Configuration
	//========================================

	/** Auto-activate abilities on grant */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	bool bAutoActivateAbilities;

	/** Remove effects on unequip */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	bool bRemoveEffectsOnUnequip;

	/** Enable detailed logging */
	UPROPERTY(EditDefaultsOnly, Category = "Configuration")
	bool bEnableDetailedLogging;

	//========================================
	// Statistics
	//========================================

	/** Total abilities granted */
	UPROPERTY()
	int32 TotalAbilitiesGranted;

	/** Total abilities removed */
	UPROPERTY()
	int32 TotalAbilitiesRemoved;

	/** Total effects applied */
	UPROPERTY()
	int32 TotalEffectsApplied;

	/** Total effects removed */
	UPROPERTY()
	int32 TotalEffectsRemoved;

	/** Currently active abilities */
	UPROPERTY()
	int32 ActiveAbilityCount;

	/** Currently active effects */
	UPROPERTY()
	int32 ActiveEffectCount;
};
