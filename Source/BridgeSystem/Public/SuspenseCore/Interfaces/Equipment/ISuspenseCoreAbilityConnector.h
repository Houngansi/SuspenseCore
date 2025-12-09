// ISuspenseCoreAbilityConnector.h
// SuspenseCore Equipment ↔ GAS Ability Connector Interface
// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryBaseTypes.h"
#include "ISuspenseCoreAbilityConnector.generated.h"

// Forward declarations
class UAbilitySystemComponent;
class UAttributeSet;
class UGameplayAbility;
class UGameplayEffect;
class ISuspenseCoreEquipmentDataProvider;
class USuspenseCoreEventBus;

/**
 * Record of a granted ability
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreGrantedAbility
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ability")
	FGameplayAbilitySpecHandle AbilityHandle;

	UPROPERTY(BlueprintReadOnly, Category = "Ability")
	TSubclassOf<UGameplayAbility> AbilityClass;

	UPROPERTY(BlueprintReadOnly, Category = "Ability")
	FGuid ItemInstanceId;

	UPROPERTY(BlueprintReadOnly, Category = "Ability")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Ability")
	int32 AbilityLevel = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Ability")
	FGameplayTag InputTag;

	UPROPERTY(BlueprintReadOnly, Category = "Ability")
	float GrantTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Ability")
	FString Source;

	bool IsValid() const { return AbilityHandle.IsValid(); }
};

/**
 * Record of an applied gameplay effect
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreAppliedEffect
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Effect")
	FActiveGameplayEffectHandle EffectHandle;

	UPROPERTY(BlueprintReadOnly, Category = "Effect")
	TSubclassOf<UGameplayEffect> EffectClass;

	UPROPERTY(BlueprintReadOnly, Category = "Effect")
	FGuid ItemInstanceId;

	UPROPERTY(BlueprintReadOnly, Category = "Effect")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Effect")
	float ApplicationTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Effect")
	float Duration = -1.0f; // -1 = infinite

	UPROPERTY(BlueprintReadOnly, Category = "Effect")
	FString Source;

	bool IsValid() const { return EffectHandle.IsValid(); }
};

/**
 * Managed attribute set record
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreConnectorAttributeSet
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Attribute")
	TObjectPtr<UAttributeSet> AttributeSet = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Attribute")
	TSubclassOf<UAttributeSet> AttributeClass;

	UPROPERTY(BlueprintReadOnly, Category = "Attribute")
	FGuid ItemInstanceId;

	UPROPERTY(BlueprintReadOnly, Category = "Attribute")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Attribute")
	bool bIsInitialized = false;

	UPROPERTY(BlueprintReadOnly, Category = "Attribute")
	FString AttributeType;

	bool IsValid() const { return AttributeSet != nullptr; }
};

/**
 * Ability connector statistics
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreAbilityConnectorStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 TotalAbilitiesGranted = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 TotalEffectsApplied = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 TotalAttributeSetsCreated = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 TotalActivations = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 FailedGrantOperations = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 FailedApplyOperations = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 FailedActivateOperations = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 CurrentActiveAbilities = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 CurrentActiveEffects = 0;
};

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreAbilityConnector : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISuspenseCoreAbilityConnector
 *
 * Bridge interface connecting equipment system with Gameplay Ability System (GAS).
 * Manages abilities, effects, and attributes granted by equipped items.
 *
 * Architecture:
 * - Grants/removes abilities based on equipment changes
 * - Applies/removes gameplay effects from items
 * - Manages item-specific attribute sets
 * - Integrates with EventBus for notifications
 *
 * EventBus Events Published:
 * - SuspenseCore.Event.Ability.Granted
 * - SuspenseCore.Event.Ability.Removed
 * - SuspenseCore.Event.Effect.Applied
 * - SuspenseCore.Event.Effect.Removed
 * - SuspenseCore.Event.Ability.Activated
 *
 * Thread Safety:
 * - Initialization must happen on game thread
 * - Runtime operations are thread-safe after init
 */
class BRIDGESYSTEM_API ISuspenseCoreAbilityConnector
{
	GENERATED_BODY()

public:
	//========================================
	// Initialization
	//========================================

	/**
	 * Initialize connector with ASC and data provider
	 * @param InASC Ability System Component to use
	 * @param InDataProvider Equipment data provider
	 * @return True if initialized successfully
	 */
	virtual bool Initialize(
		UAbilitySystemComponent* InASC,
		TScriptInterface<ISuspenseCoreEquipmentDataProvider> InDataProvider) = 0;

	/**
	 * Check if connector is initialized
	 * @return True if ready for operations
	 */
	virtual bool IsInitialized() const = 0;

	/**
	 * Shutdown connector and cleanup
	 */
	virtual void Shutdown() = 0;

	//========================================
	// Ability Management
	//========================================

	/**
	 * Grant abilities for equipped item
	 * @param ItemInstance Item granting abilities
	 * @return Array of granted ability handles
	 */
	virtual TArray<FGameplayAbilitySpecHandle> GrantEquipmentAbilities(
		const FSuspenseCoreInventoryItemInstance& ItemInstance) = 0;

	/**
	 * Grant abilities for specific slot
	 * @param SlotIndex Equipment slot
	 * @param ItemInstance Item in slot
	 * @return Array of granted ability handles
	 */
	virtual TArray<FGameplayAbilitySpecHandle> GrantAbilitiesForSlot(
		int32 SlotIndex,
		const FSuspenseCoreInventoryItemInstance& ItemInstance) = 0;

	/**
	 * Remove previously granted abilities
	 * @param Handles Ability handles to remove
	 * @return Number of abilities removed
	 */
	virtual int32 RemoveGrantedAbilities(
		const TArray<FGameplayAbilitySpecHandle>& Handles) = 0;

	/**
	 * Remove all abilities for slot
	 * @param SlotIndex Slot to clear
	 * @return Number of abilities removed
	 */
	virtual int32 RemoveAbilitiesForSlot(int32 SlotIndex) = 0;

	/**
	 * Activate granted ability by handle
	 * @param AbilityHandle Handle of ability to activate
	 * @return True if activated
	 */
	virtual bool ActivateEquipmentAbility(
		const FGameplayAbilitySpecHandle& AbilityHandle) = 0;

	/**
	 * Get all granted abilities
	 * @return Array of granted ability records
	 */
	virtual TArray<FSuspenseCoreGrantedAbility> GetGrantedAbilities() const = 0;

	/**
	 * Get abilities for specific slot
	 * @param SlotIndex Slot to query
	 * @return Array of ability handles
	 */
	virtual TArray<FGameplayAbilitySpecHandle> GetAbilitiesForSlot(
		int32 SlotIndex) const = 0;

	//========================================
	// Effect Management
	//========================================

	/**
	 * Apply passive effects for equipped item
	 * @param ItemInstance Item with effects
	 * @return Array of active effect handles
	 */
	virtual TArray<FActiveGameplayEffectHandle> ApplyEquipmentEffects(
		const FSuspenseCoreInventoryItemInstance& ItemInstance) = 0;

	/**
	 * Apply effects for specific slot
	 * @param SlotIndex Equipment slot
	 * @param ItemInstance Item in slot
	 * @return Array of effect handles
	 */
	virtual TArray<FActiveGameplayEffectHandle> ApplyEffectsForSlot(
		int32 SlotIndex,
		const FSuspenseCoreInventoryItemInstance& ItemInstance) = 0;

	/**
	 * Remove previously applied effects
	 * @param Handles Effect handles to remove
	 * @return Number of effects removed
	 */
	virtual int32 RemoveAppliedEffects(
		const TArray<FActiveGameplayEffectHandle>& Handles) = 0;

	/**
	 * Remove all effects for slot
	 * @param SlotIndex Slot to clear
	 * @return Number of effects removed
	 */
	virtual int32 RemoveEffectsForSlot(int32 SlotIndex) = 0;

	/**
	 * Get all applied effects
	 * @return Array of applied effect records
	 */
	virtual TArray<FSuspenseCoreAppliedEffect> GetAppliedEffects() const = 0;

	//========================================
	// Attribute Management
	//========================================

	/**
	 * Update attributes for equipped item
	 * @param ItemInstance Item with attributes
	 * @return True if attributes updated
	 */
	virtual bool UpdateEquipmentAttributes(
		const FSuspenseCoreInventoryItemInstance& ItemInstance) = 0;

	/**
	 * Get attribute set for slot
	 * @param SlotIndex Slot to query
	 * @return Attribute set or nullptr
	 */
	virtual UAttributeSet* GetEquipmentAttributeSet(int32 SlotIndex) const = 0;

	/**
	 * Get all managed attribute sets
	 * @return Array of managed attribute records
	 */
	virtual TArray<FSuspenseCoreConnectorAttributeSet> GetManagedAttributeSets() const = 0;

	//========================================
	// Cleanup
	//========================================

	/**
	 * Clear all abilities, effects, and attributes
	 */
	virtual void ClearAll() = 0;

	/**
	 * Cleanup invalid handles
	 * @return Number of handles cleaned
	 */
	virtual int32 CleanupInvalidHandles() = 0;

	/**
	 * Clear all for specific slot
	 * @param SlotIndex Slot to clear
	 */
	virtual void ClearSlot(int32 SlotIndex) = 0;

	//========================================
	// EventBus Integration
	//========================================

	/**
	 * Get EventBus used by this connector
	 * @return EventBus instance
	 */
	virtual USuspenseCoreEventBus* GetEventBus() const = 0;

	/**
	 * Set EventBus for this connector
	 * @param InEventBus EventBus to use
	 */
	virtual void SetEventBus(USuspenseCoreEventBus* InEventBus) = 0;

	//========================================
	// Diagnostics
	//========================================

	/**
	 * Validate connector state
	 * @param OutErrors Validation errors
	 * @return True if valid
	 */
	virtual bool ValidateConnector(TArray<FString>& OutErrors) const = 0;

	/**
	 * Get debug info string
	 * @return Formatted debug info
	 */
	virtual FString GetDebugInfo() const = 0;

	/**
	 * Get connector statistics
	 * @return Statistics struct
	 */
	virtual FSuspenseCoreAbilityConnectorStats GetStatistics() const = 0;

	/**
	 * Log statistics to output log
	 */
	virtual void LogStatistics() const = 0;

	/**
	 * Reset statistics
	 */
	virtual void ResetStatistics() = 0;
};


