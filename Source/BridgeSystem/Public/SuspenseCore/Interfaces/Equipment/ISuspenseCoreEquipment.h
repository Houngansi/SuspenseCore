// ISuspenseCoreEquipment.h
// SuspenseCore Base Equipment Interface
// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryBaseTypes.h"
#include "SuspenseCore/Operations/SuspenseCoreInventoryResult.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreLoadoutSettings.h"
#include "ISuspenseCoreEquipment.generated.h"

// Forward declarations
class UAbilitySystemComponent;
class UAttributeSet;
class USuspenseCoreEventBus;
class UGameplayAbility;
class UGameplayEffect;
struct FGameplayAttribute;

/**
 * Equipment lifecycle event data
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreEquipmentLifecycleEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Lifecycle")
	TWeakObjectPtr<AActor> Owner;

	UPROPERTY(BlueprintReadOnly, Category = "Lifecycle")
	FSuspenseCoreInventoryItemInstance ItemInstance;

	UPROPERTY(BlueprintReadOnly, Category = "Lifecycle")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Lifecycle")
	FGameplayTag EventType;

	UPROPERTY(BlueprintReadOnly, Category = "Lifecycle")
	float EventTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Lifecycle")
	FGuid EventId;

	FSuspenseCoreEquipmentLifecycleEvent()
	{
		EventId = FGuid::NewGuid();
	}

	static FSuspenseCoreEquipmentLifecycleEvent Create(
		AActor* InOwner,
		const FSuspenseCoreInventoryItemInstance& InItem,
		int32 InSlotIndex,
		const FGameplayTag& InEventType)
	{
		FSuspenseCoreEquipmentLifecycleEvent Event;
		Event.Owner = InOwner;
		Event.ItemInstance = InItem;
		Event.SlotIndex = InSlotIndex;
		Event.EventType = InEventType;
		return Event;
	}
};

/**
 * Equipment state change event data
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreEquipmentStateChangeEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "State")
	FGameplayTag PreviousState;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	FGameplayTag NewState;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	bool bWasInterrupted = false;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	float TransitionDuration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	FGameplayTag TransitionReason;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	float EventTime = 0.0f;

	static FSuspenseCoreEquipmentStateChangeEvent Create(
		const FGameplayTag& OldState,
		const FGameplayTag& InNewState,
		bool bInterrupted = false)
	{
		FSuspenseCoreEquipmentStateChangeEvent Event;
		Event.PreviousState = OldState;
		Event.NewState = InNewState;
		Event.bWasInterrupted = bInterrupted;
		return Event;
	}
};

/**
 * Equipment property change event data
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreEquipmentPropertyChangeEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Property")
	FName PropertyName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Property")
	float OldValue = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Property")
	float NewValue = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Property")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Property")
	FGuid ItemInstanceId;

	static FSuspenseCoreEquipmentPropertyChangeEvent Create(
		const FName& InPropertyName,
		float InOldValue,
		float InNewValue)
	{
		FSuspenseCoreEquipmentPropertyChangeEvent Event;
		Event.PropertyName = InPropertyName;
		Event.OldValue = InOldValue;
		Event.NewValue = InNewValue;
		return Event;
	}
};

/**
 * Equipment operation event data
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreEquipmentOperationEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Operation")
	FGameplayTag OperationType;

	UPROPERTY(BlueprintReadOnly, Category = "Operation")
	FSuspenseCoreInventoryItemInstance ItemInstance;

	UPROPERTY(BlueprintReadOnly, Category = "Operation")
	int32 SourceSlot = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Operation")
	int32 TargetSlot = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Operation")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Operation")
	FText ErrorMessage;

	UPROPERTY(BlueprintReadOnly, Category = "Operation")
	FString AdditionalData;

	UPROPERTY(BlueprintReadOnly, Category = "Operation")
	float EventTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Operation")
	FGuid OperationId;

	FSuspenseCoreEquipmentOperationEvent()
	{
		OperationId = FGuid::NewGuid();
	}
};

/**
 * Equipment slot switch event data
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreSlotSwitchEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Switch")
	int32 PreviousSlot = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Switch")
	int32 NewSlot = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Switch")
	FSuspenseCoreInventoryItemInstance PreviousItem;

	UPROPERTY(BlueprintReadOnly, Category = "Switch")
	FSuspenseCoreInventoryItemInstance NewItem;

	UPROPERTY(BlueprintReadOnly, Category = "Switch")
	float SwitchDuration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Switch")
	bool bWasForced = false;

	UPROPERTY(BlueprintReadOnly, Category = "Switch")
	FGameplayTag SwitchReason;
};

/**
 * Equipment attachment configuration
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreEquipmentAttachment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment")
	FName SocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment")
	FTransform AttachmentOffset = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment")
	bool bUseSocketRotation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment")
	bool bSnapToSocket = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment")
	FGameplayTag AttachmentType;

	bool IsValid() const { return !SocketName.IsNone(); }
};

/**
 * Equipment validation context
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreEquipmentValidationContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Validation")
	TWeakObjectPtr<AActor> Owner;

	UPROPERTY(BlueprintReadWrite, Category = "Validation")
	int32 CharacterLevel = 1;

	UPROPERTY(BlueprintReadWrite, Category = "Validation")
	FGameplayTag CharacterClass;

	UPROPERTY(BlueprintReadWrite, Category = "Validation")
	FGameplayTagContainer CharacterTags;

	UPROPERTY(BlueprintReadWrite, Category = "Validation")
	bool bIgnoreLevelRequirements = false;

	UPROPERTY(BlueprintReadWrite, Category = "Validation")
	bool bIgnoreClassRequirements = false;
};

/**
 * Equipment validation result
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreEquipmentValidationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	bool bIsValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	TArray<FText> Errors;

	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	TArray<FText> Warnings;

	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	FGameplayTag FailureReason;

	static FSuspenseCoreEquipmentValidationResult Success()
	{
		FSuspenseCoreEquipmentValidationResult Result;
		Result.bIsValid = true;
		return Result;
	}

	static FSuspenseCoreEquipmentValidationResult Failure(const FText& Error, const FGameplayTag& Reason = FGameplayTag())
	{
		FSuspenseCoreEquipmentValidationResult Result;
		Result.bIsValid = false;
		Result.Errors.Add(Error);
		Result.FailureReason = Reason;
		return Result;
	}

	void AddError(const FText& Error) { Errors.Add(Error); bIsValid = false; }
	void AddWarning(const FText& Warning) { Warnings.Add(Warning); }
	bool HasErrors() const { return Errors.Num() > 0; }
	bool HasWarnings() const { return Warnings.Num() > 0; }
};

/**
 * Equipment debug info structure
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreEquipmentDebugInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	FString EquipmentClass;

	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	EEquipmentSlotType SlotType = EEquipmentSlotType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	FGameplayTag CurrentState;

	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	bool bIsEquipped = false;

	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	FName EquippedItemId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	float ConditionPercent = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	TMap<FName, float> RuntimeProperties;

	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	int32 GrantedAbilitiesCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	int32 ActiveEffectsCount = 0;

	FString ToString() const
	{
		return FString::Printf(TEXT("Equipment[%s] Slot:%d Type:%d State:%s Equipped:%s Item:%s Condition:%.1f%%"),
			*EquipmentClass, SlotIndex, (int32)SlotType,
			*CurrentState.ToString(), bIsEquipped ? TEXT("Yes") : TEXT("No"),
			*EquippedItemId.ToString(), ConditionPercent * 100.0f);
	}
};

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreEquipment : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISuspenseCoreEquipment
 *
 * Base interface for all equippable items in the SuspenseCore equipment system.
 * Defines the contract between the equipment system and specific equipment implementations.
 *
 * Architecture:
 * - Manages equipment lifecycle (equip/unequip)
 * - Provides GAS integration for abilities and effects
 * - Handles equipment state management
 * - Supports weapon slot management
 * - Integrates with EventBus for decoupled communication
 *
 * EventBus Events Published:
 * - SuspenseCore.Event.Equipment.Equipped
 * - SuspenseCore.Event.Equipment.Unequipped
 * - SuspenseCore.Event.Equipment.StateChanged
 * - SuspenseCore.Event.Equipment.PropertyChanged
 * - SuspenseCore.Event.Equipment.SlotSwitched
 * - SuspenseCore.Event.Equipment.EffectsApplied
 * - SuspenseCore.Event.Equipment.EffectsRemoved
 *
 * Design Principles:
 * - Separation of concerns: General equipment here, weapon specifics in ISuspenseCoreWeapon
 * - Interface defines "what" not "how"
 * - Thread-safe for read operations after initialization
 */
class BRIDGESYSTEM_API ISuspenseCoreEquipment
{
	GENERATED_BODY()

public:
	//========================================
	// Lifecycle Management
	//========================================

	/**
	 * Called when equipment is attached to an owner
	 * Entry point for equipment lifecycle - initializes state, applies passive effects
	 * @param NewOwner Actor equipping this item
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Lifecycle")
	void OnEquipped(AActor* NewOwner);

	/**
	 * Called when equipment is detached from owner
	 * Exit point - cleanup state, remove effects, unsubscribe from systems
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Lifecycle")
	void OnUnequipped();

	/**
	 * Called when a specific runtime item instance is equipped
	 * Receives full item instance data including unique properties
	 * @param ItemInstance Runtime item instance with unique properties
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Lifecycle")
	void OnItemInstanceEquipped(const FSuspenseCoreInventoryItemInstance& ItemInstance);

	/**
	 * Called when a specific runtime item instance is unequipped
	 * @param ItemInstance Runtime item instance being removed
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Lifecycle")
	void OnItemInstanceUnequipped(const FSuspenseCoreInventoryItemInstance& ItemInstance);

	//========================================
	// Properties and Configuration
	//========================================

	/**
	 * Get currently equipped item instance
	 * @return Current item instance or invalid if slot is empty
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Properties")
	FSuspenseCoreInventoryItemInstance GetEquippedItemInstance() const;

	/**
	 * Get copy of slot configuration (Blueprint-safe)
	 * @return Copy of equipment slot configuration
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Properties")
	FEquipmentSlotConfig GetSlotConfiguration() const;

	/**
	 * Get direct pointer to slot configuration (C++ optimization)
	 * @return Pointer to configuration or nullptr
	 */
	virtual const FEquipmentSlotConfig* GetSlotConfigurationPtr() const { return nullptr; }

	/**
	 * Get equipment slot type
	 * @return Slot type enum (weapon, armor, accessory, etc.)
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Properties")
	EEquipmentSlotType GetEquipmentSlotType() const;

	/**
	 * Get equipment slot as gameplay tag
	 * @return Gameplay tag representing slot type
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Properties")
	FGameplayTag GetEquipmentSlotTag() const;

	/**
	 * Check if slot currently has an equipped item
	 * @return True if slot is occupied
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Properties")
	bool IsEquipped() const;

	/**
	 * Check if this is a required slot for valid loadout
	 * @return True if slot is mandatory
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Properties")
	bool IsRequiredSlot() const;

	/**
	 * Get display name for this equipment slot
	 * @return Localized display name
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Properties")
	FText GetSlotDisplayName() const;

	/**
	 * Get socket name for equipment attachment
	 * @return Socket name or NAME_None
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Properties")
	FName GetAttachmentSocket() const;

	/**
	 * Get transform offset for equipment attachment
	 * @return Attachment transform offset
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Properties")
	FTransform GetAttachmentOffset() const;

	/**
	 * Get full attachment configuration
	 * @return Attachment configuration struct
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Properties")
	FSuspenseCoreEquipmentAttachment GetAttachmentConfiguration() const;

	//========================================
	// Item Compatibility and Validation
	//========================================

	/**
	 * Check if item instance can be equipped in this slot
	 * Performs comprehensive compatibility check
	 * @param ItemInstance Item to check
	 * @return True if item can be equipped
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Validation")
	bool CanEquipItemInstance(const FSuspenseCoreInventoryItemInstance& ItemInstance) const;

	/**
	 * Check compatibility with validation context
	 * @param ItemInstance Item to check
	 * @param Context Validation context with character info
	 * @return Detailed validation result
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Validation")
	FSuspenseCoreEquipmentValidationResult ValidateItemInstance(
		const FSuspenseCoreInventoryItemInstance& ItemInstance,
		const FSuspenseCoreEquipmentValidationContext& Context) const;

	/**
	 * Get allowed item types for this slot
	 * @return Container of allowed item type tags
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Validation")
	FGameplayTagContainer GetAllowedItemTypes() const;

	/**
	 * Validate equipment requirements for loadout
	 * @param OutErrors Array to receive validation errors
	 * @return True if equipment meets all requirements
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Validation")
	bool ValidateEquipmentRequirements(TArray<FString>& OutErrors) const;

	//========================================
	// Equipment Operations
	//========================================

	/**
	 * Equip item instance to this slot
	 * @param ItemInstance Item to equip
	 * @param bForceEquip Force equip even if validation fails
	 * @return Operation result
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Operations")
	FSuspenseInventoryOperationResult EquipItemInstance(
		const FSuspenseCoreInventoryItemInstance& ItemInstance,
		bool bForceEquip = false);

	/**
	 * Unequip current item from this slot
	 * @param OutUnequippedInstance Receives unequipped item instance
	 * @return Operation result
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Operations")
	FSuspenseInventoryOperationResult UnequipItem(FSuspenseCoreInventoryItemInstance& OutUnequippedInstance);

	/**
	 * Swap items between this slot and another
	 * @param OtherEquipment Other equipment slot to swap with
	 * @return Operation result
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Operations")
	FSuspenseInventoryOperationResult SwapEquipmentWith(
		const TScriptInterface<ISuspenseCoreEquipment>& OtherEquipment);

	//========================================
	// Gameplay Ability System Integration
	//========================================

	/**
	 * Get Ability System Component for this equipment
	 * @return ASC reference or nullptr
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|AbilitySystem")
	UAbilitySystemComponent* GetAbilitySystemComponent() const;

	/**
	 * Get equipment attribute set
	 * @return AttributeSet or nullptr
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|AbilitySystem")
	UAttributeSet* GetEquipmentAttributeSet() const;

	/**
	 * Get abilities granted by equipped item
	 * @return Array of granted ability classes
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|AbilitySystem")
	TArray<TSubclassOf<UGameplayAbility>> GetGrantedAbilities() const;

	/**
	 * Get passive effects applied by equipped item
	 * @return Array of passive effect classes
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|AbilitySystem")
	TArray<TSubclassOf<UGameplayEffect>> GetPassiveEffects() const;

	/**
	 * Apply equipment abilities and effects
	 * Called automatically on equip
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|AbilitySystem")
	void ApplyEquipmentEffects();

	/**
	 * Remove equipment abilities and effects
	 * Called automatically on unequip
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|AbilitySystem")
	void RemoveEquipmentEffects();

	//========================================
	// State Management
	//========================================

	/**
	 * Get current equipment state
	 * @return Current state tag (e.g., Equipment.State.Idle)
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|State")
	FGameplayTag GetCurrentEquipmentState() const;

	/**
	 * Set equipment state with validation
	 * @param NewState State to transition to
	 * @param bForceTransition Force transition even if invalid
	 * @return True if transition was successful
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|State")
	bool SetEquipmentState(const FGameplayTag& NewState, bool bForceTransition = false);

	/**
	 * Check if equipment is in specific state
	 * @param StateTag State to check
	 * @return True if in specified state
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|State")
	bool IsInEquipmentState(const FGameplayTag& StateTag) const;

	/**
	 * Get available state transitions from current state
	 * @return Array of valid target states
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|State")
	TArray<FGameplayTag> GetAvailableStateTransitions() const;

	//========================================
	// Runtime Properties
	//========================================

	/**
	 * Get runtime property value
	 * @param PropertyName Property name
	 * @param DefaultValue Default if not found
	 * @return Current property value
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Properties")
	float GetEquipmentRuntimeProperty(const FName& PropertyName, float DefaultValue = 0.0f) const;

	/**
	 * Set runtime property value
	 * @param PropertyName Property name
	 * @param Value New value
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Properties")
	void SetEquipmentRuntimeProperty(const FName& PropertyName, float Value);

	/**
	 * Get all runtime properties
	 * @return Map of property names to values
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Properties")
	TMap<FName, float> GetAllRuntimeProperties() const;

	/**
	 * Get equipment condition/durability percentage
	 * @return Condition from 0.0 to 1.0
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Properties")
	float GetEquipmentConditionPercent() const;

	//========================================
	// Weapon Type Detection
	//========================================

	/**
	 * Check if this equipment is a weapon
	 * @return True if equipped item is a weapon
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Type")
	bool IsWeaponEquipment() const;

	/**
	 * Get weapon archetype tag if this is a weapon
	 * @return Weapon archetype tag or empty
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Type")
	FGameplayTag GetWeaponArchetype() const;

	/**
	 * Check if weapon can fire (basic check)
	 * For detailed weapon functionality use ISuspenseCoreWeapon
	 * @return True if weapon is ready to fire
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Type")
	bool CanFireWeapon() const;

	//========================================
	// Weapon Slot Management
	//========================================

	/**
	 * Get currently active weapon slot index
	 * @return Active slot index or INDEX_NONE
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|WeaponSlots")
	int32 GetActiveWeaponSlotIndex() const;

	/**
	 * Switch to specific equipment slot
	 * @param SlotIndex Target slot index
	 * @return True if switch was initiated
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|WeaponSlots")
	bool SwitchToSlot(int32 SlotIndex);

	/**
	 * Switch to weapon slot with force option
	 * @param TargetSlotIndex Target slot
	 * @param bForceSwitch Force switch even if slot is empty
	 * @return True if switch started
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|WeaponSlots")
	bool SwitchToWeaponSlot(int32 TargetSlotIndex, bool bForceSwitch = false);

	/**
	 * Get weapon slots in priority order
	 * @return Array of slot indices sorted by priority
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|WeaponSlots")
	TArray<int32> GetWeaponSlotsByPriority() const;

	/**
	 * Get previous weapon slot for quick switching
	 * @return Previous slot index or INDEX_NONE
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|WeaponSlots")
	int32 GetPreviousWeaponSlot() const;

	/**
	 * Get total number of weapon slots
	 * @return Number of weapon slots
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|WeaponSlots")
	int32 GetWeaponSlotCount() const;

	/**
	 * Get total number of equipment slots
	 * @return Total slot count
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|WeaponSlots")
	int32 GetTotalSlotCount() const;

	/**
	 * Check if slot contains a weapon
	 * @param SlotIndex Slot to check
	 * @return True if slot contains a weapon
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|WeaponSlots")
	bool IsSlotWeapon(int32 SlotIndex) const;

	/**
	 * Check if slot is a weapon slot type
	 * @param SlotIndex Slot to check
	 * @return True if slot is designated for weapons
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|WeaponSlots")
	bool IsSlotWeaponSlot(int32 SlotIndex) const;

	/**
	 * Get item instance in specific slot
	 * @param SlotIndex Slot to query
	 * @return Item instance or invalid if empty
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|WeaponSlots")
	FSuspenseCoreInventoryItemInstance GetItemInSlot(int32 SlotIndex) const;

	/**
	 * Get all occupied weapon slot indices
	 * @return Array of slot indices containing weapons
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|WeaponSlots")
	TArray<int32> GetOccupiedWeaponSlots() const;

	/**
	 * Get last active weapon slot for quick switch
	 * @return Last active slot index or INDEX_NONE
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|WeaponSlots")
	int32 GetLastActiveWeaponSlot() const;

	/**
	 * Set last active weapon slot for tracking
	 * @param SlotIndex Slot index to record
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|WeaponSlots")
	void SetLastActiveWeaponSlot(int32 SlotIndex);

	//========================================
	// EventBus Integration
	//========================================

	/**
	 * Get EventBus used by this equipment
	 * @return EventBus instance or nullptr
	 */
	virtual USuspenseCoreEventBus* GetEventBus() const { return nullptr; }

	/**
	 * Set EventBus for this equipment
	 * @param InEventBus EventBus to use
	 */
	virtual void SetEventBus(USuspenseCoreEventBus* InEventBus) {}

	/**
	 * Broadcast equipment state change via EventBus
	 * @param OldState Previous state
	 * @param NewState New state
	 * @param bInterrupted Was transition interrupted
	 */
	virtual void BroadcastStateChanged(
		const FGameplayTag& OldState,
		const FGameplayTag& NewState,
		bool bInterrupted = false) {}

	/**
	 * Broadcast equipment operation event via EventBus
	 * @param EventTag Operation type tag
	 * @param ItemInstance Related item instance
	 * @param EventData Additional event data
	 */
	virtual void BroadcastOperationEvent(
		const FGameplayTag& EventTag,
		const FSuspenseCoreInventoryItemInstance* ItemInstance = nullptr,
		const FString& EventData = TEXT("")) {}

	/**
	 * Broadcast property change via EventBus
	 * @param PropertyName Changed property name
	 * @param OldValue Previous value
	 * @param NewValue New value
	 */
	virtual void BroadcastPropertyChanged(
		const FName& PropertyName,
		float OldValue,
		float NewValue) {}

	//========================================
	// Diagnostics and Debug
	//========================================

	/**
	 * Get detailed debug info about equipment state
	 * @return Formatted debug string
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Debug")
	FString GetEquipmentDebugInfo() const;

	/**
	 * Get structured debug info
	 * @return Debug info struct
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Debug")
	FSuspenseCoreEquipmentDebugInfo GetDebugInfoStruct() const;

	/**
	 * Validate equipment integrity
	 * @param OutErrors Array to receive validation errors
	 * @return True if equipment is in valid state
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Debug")
	bool ValidateEquipmentIntegrity(TArray<FString>& OutErrors) const;

	/**
	 * Get equipment statistics string
	 * @return Formatted statistics
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Debug")
	FString GetEquipmentStatistics() const;

	/**
	 * Reset equipment statistics
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Equipment|Debug")
	void ResetEquipmentStatistics();
};

