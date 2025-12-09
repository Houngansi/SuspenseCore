// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "ISuspenseCoreAttributeProvider.generated.h"

// Forward declarations
class UAttributeSet;
class UAbilitySystemComponent;
class USuspenseCoreEventManager;

/**
 * Simple attribute data structure for UI and cross-module communication
 * Provides attribute information without GAS dependency
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseAttributeData
{
    GENERATED_BODY()

    /** Current value of the attribute */
    UPROPERTY(BlueprintReadWrite, Category = "Attribute")
    float CurrentValue = 0.0f;

    /** Maximum value of the attribute */
    UPROPERTY(BlueprintReadWrite, Category = "Attribute")
    float MaxValue = 0.0f;

    /** Percentage (0.0 to 1.0) */
    UPROPERTY(BlueprintReadWrite, Category = "Attribute")
    float Percentage = 0.0f;

    /** Attribute identification tag */
    UPROPERTY(BlueprintReadWrite, Category = "Attribute")
    FGameplayTag AttributeTag;

    /** Display name for UI */
    UPROPERTY(BlueprintReadWrite, Category = "Attribute")
    FText DisplayName;

    /** Whether this attribute data is valid */
    UPROPERTY(BlueprintReadWrite, Category = "Attribute")
    bool bIsValid = false;

    /** Helper to create attribute data */
    static FSuspenseAttributeData CreateAttributeData(float Current, float Max, const FGameplayTag& Tag, const FText& Name = FText::GetEmpty())
    {
        FSuspenseAttributeData Data;
        Data.CurrentValue = Current;
        Data.MaxValue = Max;
        Data.Percentage = (Max > 0.0f) ? (Current / Max) : 0.0f;
        Data.AttributeTag = Tag;
        Data.DisplayName = Name;
        Data.bIsValid = true;
        return Data;
    }
};

UINTERFACE(MinimalAPI, BlueprintType)
class USuspenseAttributeProvider : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for objects that provide access to attributes and related data
 * Used for unified attribute access across different modules
 * 
 * Architecture notes:
 * - Game modules use AttributeSet methods for full GAS functionality
 * - UI modules use simple data methods to avoid GAS dependency
 * - EventDelegateManager provides decoupled communication
 */
class BRIDGESYSTEM_API ISuspenseAttributeProvider
{
    GENERATED_BODY()

public:
    //================================================
    // Core Attribute Access (Existing Methods)
    //================================================
    
    /**
     * Returns the current AttributeSet of the object
     * @return Pointer to AttributeSet or nullptr if not available
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Attributes")
    UAttributeSet* GetAttributeSet() const;
    
    /**
     * Returns the AttributeSet class used by the object
     * @return AttributeSet class or nullptr
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Attributes")
    TSubclassOf<UAttributeSet> GetAttributeSetClass() const;
    
    /**
     * Returns the base effect applied to attributes
     * @return GameplayEffect class or nullptr
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Attributes|Effects")
    TSubclassOf<UGameplayEffect> GetBaseStatsEffect() const;
    
    /**
     * Initializes attributes in the specified AttributeSet
     * @param AttributeSet AttributeSet to initialize
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Attributes")
    void InitializeAttributes(UAttributeSet* AttributeSet) const;
    
    /**
     * Applies effects to the specified AbilitySystemComponent
     * @param ASC AbilitySystemComponent to apply effects to
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Attributes|Effects")
    void ApplyEffects(UAbilitySystemComponent* ASC) const;
    
    /**
     * Checks if attributes are available on the object
     * @return true if attributes are available
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Attributes")
    bool HasAttributes() const;
    
    /**
     * Sets the AttributeSet class for this provider
     * @param NewClass New AttributeSet class to use
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Attributes")
    void SetAttributeSetClass(TSubclassOf<UAttributeSet> NewClass);
    
    //================================================
    // Simple Data Access (New Methods for UI)
    //================================================
    
    /**
     * Get attribute data by tag - for UI and cross-module communication
     * @param AttributeTag Tag identifying the attribute
     * @return Attribute data structure
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Attributes|Data")
    FSuspenseAttributeData GetAttributeData(const FGameplayTag& AttributeTag) const;
    
    /**
     * Get health attribute data
     * @return Health data structure
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Attributes|Data")
    FSuspenseAttributeData GetHealthData() const;
    
    /**
     * Get stamina attribute data
     * @return Stamina data structure
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Attributes|Data")
    FSuspenseAttributeData GetStaminaData() const;
    
    /**
     * Get armor attribute data
     * @return Armor data structure
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Attributes|Data")
    FSuspenseAttributeData GetArmorData() const;
    
    /**
     * Get all available attribute data
     * @return Array of all attribute data
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Attributes|Data")
    TArray<FSuspenseAttributeData> GetAllAttributeData() const;
    
    /**
     * Get specific attribute value by tag
     * @param AttributeTag Tag identifying the attribute
     * @param OutCurrentValue Current value of the attribute
     * @param OutMaxValue Maximum value of the attribute
     * @return true if attribute was found
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Attributes|Data")
    bool GetAttributeValue(const FGameplayTag& AttributeTag, float& OutCurrentValue, float& OutMaxValue) const;
    
    //================================================
    // Attribute Modification Notifications
    //================================================
    
    /**
     * Notify that an attribute has changed
     * @param AttributeTag Tag of the changed attribute
     * @param NewValue New value of the attribute
     * @param OldValue Previous value of the attribute
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Attributes|Events")
    void NotifyAttributeChanged(const FGameplayTag& AttributeTag, float NewValue, float OldValue);
    
    //================================================
    // Event System Access
    //================================================
    
    /**
     * Gets the central delegate manager for attribute events
     * @return Pointer to delegate manager or nullptr if not available
     */
    virtual USuspenseCoreEventManager* GetDelegateManager() const = 0;
    
    /**
     * Static helper to get delegate manager
     * @param WorldContextObject Any object with valid world context
     * @return Delegate manager or nullptr
     */
    static USuspenseCoreEventManager* GetDelegateManagerStatic(const UObject* WorldContextObject);
    
    //================================================
    // Helper Methods for Common Attributes
    //================================================
    
    /**
     * Broadcast health update through event system
     * @param Provider Object implementing this interface
     * @param CurrentHealth Current health value
     * @param MaxHealth Maximum health value
     */
    static void BroadcastHealthUpdate(const UObject* Provider, float CurrentHealth, float MaxHealth);
    
    /**
     * Broadcast stamina update through event system
     * @param Provider Object implementing this interface
     * @param CurrentStamina Current stamina value
     * @param MaxStamina Maximum stamina value
     */
    static void BroadcastStaminaUpdate(const UObject* Provider, float CurrentStamina, float MaxStamina);
};