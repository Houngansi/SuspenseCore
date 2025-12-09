// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Components/SuspenseCoreEquipmentComponentBase.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryLegacyTypes.h"
#include "Types/Loadout/SuspenseItemDataTable.h"
#include "AttributeSet.h"
#include "SuspenseCoreEquipmentAttributeComponent.generated.h"

// Forward declarations
class UAttributeSet;
class UGameplayEffect;
class UAbilitySystemComponent;
struct FGameplayAttribute;
struct FGameplayEffectSpec;

/**
 * Replicated attribute data for synchronization
 */
USTRUCT()
struct FSuspenseCoreReplicatedAttributeData
{
    GENERATED_BODY()

    /** Attribute name for identification */
    UPROPERTY()
    FString AttributeName;

    /** Base value of the attribute */
    UPROPERTY()
    float BaseValue;

    /** Current value of the attribute */
    UPROPERTY()
    float CurrentValue;

    FSuspenseCoreReplicatedAttributeData()
    {
        AttributeName = "";
        BaseValue = 0.0f;
        CurrentValue = 0.0f;
    }
};

/**
 * Prediction data for attribute changes
 */
USTRUCT()
struct FSuspenseCoreAttributePredictionData
{
    GENERATED_BODY()

    /** Unique prediction key */
    UPROPERTY()
    int32 PredictionKey;

    /** Attribute being predicted */
    UPROPERTY()
    FString AttributeName;

    /** Predicted value */
    UPROPERTY()
    float PredictedValue;

    /** Original value before prediction */
    UPROPERTY()
    float OriginalValue;

    /** Time when prediction was made */
    UPROPERTY()
    float PredictionTime;

    FSuspenseCoreAttributePredictionData()
    {
        PredictionKey = 0;
        AttributeName = "";
        PredictedValue = 0.0f;
        OriginalValue = 0.0f;
        PredictionTime = 0.0f;
    }
};

/**
 * Component that manages equipment attributes through GAS
 *
 * VERSION 6.0 - FULL DATATABLE INTEGRATION:
 * - Creates AttributeSets from DataTable configuration
 * - Applies initialization GameplayEffects automatically
 * - Supports weapon, armor, ammo, and general equipment attributes
 * - Full multiplayer support with prediction and replication
 * - Thread-safe attribute access with caching
 */
UCLASS(ClassGroup=(Equipment), meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentAttributeComponent : public USuspenseCoreEquipmentComponentBase
{
    GENERATED_BODY()

public:
    USuspenseCoreEquipmentAttributeComponent();
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /**
     * Initialize component with item instance
     * Creates AttributeSets based on DataTable configuration
     * @param InOwner Owner actor
     * @param InASC Ability system component
     * @param ItemInstance Item instance with runtime data
     */
    virtual void InitializeWithItemInstance(AActor* InOwner, UAbilitySystemComponent* InASC, const FSuspenseCoreInventoryItemInstance& ItemInstance) override;

    /**
     * Clean up resources and attribute sets
     */
    virtual void Cleanup() override;

    /**
     * Update attributes when item properties change
     * @param ItemInstance Updated item instance
     */
    virtual void UpdateEquippedItem(const FSuspenseCoreInventoryItemInstance& ItemInstance) override;

    /**
     * Apply all item effects and abilities
     * @param ItemData Unified item data from DataTable
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attributes")
    void ApplyItemEffects(const FSuspenseCoreUnifiedItemData& ItemData);

    /**
     * Remove all applied item effects
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attributes")
    void RemoveItemEffects();

    /**
     * Get current primary attribute set
     * @return Active attribute set or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attributes")
    UAttributeSet* GetAttributeSet() const { return CurrentAttributeSet; }

    /**
     * Get weapon-specific attribute set
     * @return Weapon attribute set or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attributes")
    UAttributeSet* GetWeaponAttributeSet() const { return WeaponAttributeSet; }

    /**
     * Get armor-specific attribute set
     * @return Armor attribute set or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attributes")
    UAttributeSet* GetArmorAttributeSet() const { return ArmorAttributeSet; }

    /**
     * Get ammo management attribute set
     * @return Ammo attribute set or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attributes")
    UAttributeSet* GetAmmoAttributeSet() const { return AmmoAttributeSet; }

    //================================================
    // Client Prediction Methods
    //================================================

    /**
     * Predict attribute change on client
     * @param AttributeName Name of attribute to change
     * @param NewValue Predicted new value
     * @return Prediction key for tracking
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attributes|Prediction")
    int32 PredictAttributeChange(const FString& AttributeName, float NewValue);

    /**
     * Confirm or reject attribute prediction
     * @param PredictionKey Key from PredictAttributeChange
     * @param bSuccess Whether prediction was correct
     * @param ActualValue Actual value from server
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attributes|Prediction")
    void ConfirmAttributePrediction(int32 PredictionKey, bool bSuccess, float ActualValue);

    /**
     * Get current attribute value (with prediction)
     * @param AttributeName Name of attribute
     * @param OutValue Current value (predicted if applicable)
     * @return True if attribute exists
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attributes")
    bool GetAttributeValue(const FString& AttributeName, float& OutValue) const;

    /**
     * Set attribute value (with replication)
     * @param AttributeName Name of attribute
     * @param NewValue New value to set
     * @param bForceReplication Force immediate replication
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attributes")
    void SetAttributeValue(const FString& AttributeName, float NewValue, bool bForceReplication = false);

    //================================================
    // Attribute Query Methods
    //================================================

    /**
     * Get all current attribute values
     * @return Map of attribute names to current values
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attributes")
    TMap<FString, float> GetAllAttributeValues() const;

    /**
     * Check if attribute exists
     * @param AttributeName Name of attribute to check
     * @return True if attribute exists in any attribute set
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attributes")
    bool HasAttribute(const FString& AttributeName) const;

    /**
     * Get attribute by gameplay tag
     * @param AttributeTag Tag identifying the attribute
     * @param OutValue Attribute value
     * @return True if found
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attributes")
    bool GetAttributeByTag(const FGameplayTag& AttributeTag, float& OutValue) const;

    /**
     * Force collection and replication of attributes
     * Useful after batch modifications
     */
    UFUNCTION(BlueprintCallable, Category = "Equipment|Attributes")
    void ForceAttributeReplication();

protected:
    /**
     * Called when equipment is initialized
     */
    virtual void OnEquipmentInitialized() override;

    /**
     * Called when equipped item changes
     */
    virtual void OnEquippedItemChanged(const FSuspenseCoreInventoryItemInstance& OldItem, const FSuspenseCoreInventoryItemInstance& NewItem) override;

    /**
     * Create appropriate attribute sets based on item type
     * @param ItemData Item data from DataTable
     */
    void CreateAttributeSetsForItem(const FSuspenseCoreUnifiedItemData& ItemData);

    /**
     * Clean up all active attribute sets
     */
    void CleanupAttributeSets();

    /**
     * Apply initialization effect to newly created AttributeSet
     * @param AttributeSet Set to initialize
     * @param InitEffect Effect class to apply
     * @param ItemData Item data for context
     */
    void ApplyInitializationEffect(UAttributeSet* AttributeSet, TSubclassOf<UGameplayEffect> InitEffect, const FSuspenseCoreUnifiedItemData& ItemData);

    /**
     * Apply passive effects from item data
     * @param ItemData Item data containing effect classes
     */
    void ApplyPassiveEffects(const FSuspenseCoreUnifiedItemData& ItemData);

    /**
     * Apply granted abilities from item data
     * @param ItemData Item data containing abilities
     */
    void ApplyGrantedAbilities(const FSuspenseCoreUnifiedItemData& ItemData);

    /**
     * Collect all attributes for replication
     */
    void CollectReplicatedAttributes();

    /**
     * Apply replicated attributes to local attribute sets
     */
    void ApplyReplicatedAttributes();

    /**
     * Find attribute property by name
     * @param AttributeSet Set to search in
     * @param AttributeName Name to find
     * @return Property or nullptr
     */
    FProperty* FindAttributeProperty(UAttributeSet* AttributeSet, const FString& AttributeName) const;

    /**
     * Get attribute value from property via reflection
     * @param AttributeSet Set containing the attribute
     * @param Property Attribute property
     * @return Current value
     */
    float GetAttributeValueFromProperty(UAttributeSet* AttributeSet, FProperty* Property) const;

    /**
     * Set attribute value to property via reflection
     * @param AttributeSet Set containing the attribute
     * @param Property Attribute property
     * @param Value New value
     */
    void SetAttributeValueToProperty(UAttributeSet* AttributeSet, FProperty* Property, float Value);

    /**
     * Get GameplayAttribute from property for GAS operations
     * @param AttributeSet Set containing the attribute
     * @param Property Attribute property
     * @return GameplayAttribute for use in effects
     */
    FGameplayAttribute GetGameplayAttributeFromProperty(UAttributeSet* AttributeSet, FProperty* Property) const;

    //================================================
    // Replication Callbacks
    //================================================

    UFUNCTION()
    void OnRep_ReplicatedAttributes();

    UFUNCTION()
    void OnRep_AttributeSetClasses();

    //================================================
    // Server RPCs
    //================================================

    UFUNCTION(Server, Reliable, WithValidation)
    void ServerSetAttributeValue(const FString& AttributeName, float NewValue);

    UFUNCTION(Server, Reliable, WithValidation)
    void ServerApplyItemEffects(const FName& ItemID);

    UFUNCTION(Server, Reliable, WithValidation)
    void ServerRemoveItemEffects();

private:
    //================================================
    // Attribute Management
    //================================================

    /** Primary attribute set for the equipment */
    UPROPERTY()
    UAttributeSet* CurrentAttributeSet;

    /** Weapon-specific attribute set (damage, fire rate, etc.) */
    UPROPERTY()
    UAttributeSet* WeaponAttributeSet;

    /** Armor-specific attribute set (defense, resistances, etc.) */
    UPROPERTY()
    UAttributeSet* ArmorAttributeSet;

    /** Ammo management attribute set (current/max ammo) */
    UPROPERTY()
    UAttributeSet* AmmoAttributeSet;

    /** Active effect handles for cleanup */
    UPROPERTY()
    TArray<FActiveGameplayEffectHandle> AppliedEffectHandles;

    /** Granted ability handles for cleanup */
    UPROPERTY()
    TArray<FGameplayAbilitySpecHandle> GrantedAbilityHandles;

    /** Map of attribute sets by type for quick access */
    UPROPERTY()
    TMap<FGameplayTag, UAttributeSet*> AttributeSetsByType;

    //================================================
    // Replication State
    //================================================

    /** Replicated attribute values for client synchronization */
    UPROPERTY(ReplicatedUsing=OnRep_ReplicatedAttributes)
    TArray<FSuspenseCoreReplicatedAttributeData> ReplicatedAttributes;

    /** Classes of attribute sets for client spawning */
    UPROPERTY(ReplicatedUsing=OnRep_AttributeSetClasses)
    TArray<TSubclassOf<UAttributeSet>> ReplicatedAttributeSetClasses;

    /** Version counter for forcing updates */
    UPROPERTY(Replicated)
    uint8 AttributeReplicationVersion;

    //================================================
    // Client Prediction State
    //================================================

    /** Active attribute predictions */
    UPROPERTY()
    TArray<FSuspenseCoreAttributePredictionData> ActiveAttributePredictions;

    /** Counter for prediction keys */
    UPROPERTY()
    int32 NextAttributePredictionKey;

    /** Map for quick attribute lookup */
    mutable TMap<FString, TPair<UAttributeSet*, FProperty*>> AttributePropertyCache;

    /** Critical section for thread safety */
    mutable FCriticalSection AttributeCacheCriticalSection;
};
