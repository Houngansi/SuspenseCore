// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpec.h"
#include "ActiveGameplayEffectHandle.h"
#include "IMedComAbilityProvider.generated.h"

// Forward declarations
class UGameplayAbility;
class UGameplayEffect;
class UAbilitySystemComponent;
class UEventDelegateManager;

UINTERFACE(MinimalAPI, BlueprintType)
class UMedComAbilityProvider : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for ability providers
 * Replaces AbilitySystemBridge functionality
 */
class MEDCOMSHARED_API IMedComAbilityProvider
{
    GENERATED_BODY()

public:
    //================================================
    // Ability Management
    //================================================
    
    /**
     * Grants ability with specified parameters
     * @param AbilityClass Ability class to grant
     * @param Level Ability level
     * @param InputID Input ID for ability activation
     * @return Handle to granted ability
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "AbilitySystem|Abilities")
    FGameplayAbilitySpecHandle GrantAbility(TSubclassOf<UGameplayAbility> AbilityClass, int32 Level, int32 InputID);
    
    /**
     * Removes ability by handle
     * @param AbilityHandle Handle of ability to remove
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "AbilitySystem|Abilities")
    void RemoveAbility(FGameplayAbilitySpecHandle AbilityHandle);
    
    /**
     * Removes all granted abilities
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "AbilitySystem|Abilities")
    void RemoveAllAbilities();
    
    /**
     * Checks if ability can be granted
     * @param AbilityClass Ability class to check
     * @return true if ability can be granted
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "AbilitySystem|Abilities")
    bool CanGrantAbility(TSubclassOf<UGameplayAbility> AbilityClass) const;
    
    //================================================
    // Effect Management
    //================================================
    
    /**
     * Applies effect to self
     * @param EffectClass Effect class to apply
     * @param Level Effect level
     * @return Handle to applied effect
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "AbilitySystem|Effects")
    FActiveGameplayEffectHandle ApplyEffectToSelf(TSubclassOf<UGameplayEffect> EffectClass, float Level);
    
    /**
     * Removes effect by handle
     * @param EffectHandle Handle of effect to remove
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "AbilitySystem|Effects")
    void RemoveEffect(FActiveGameplayEffectHandle EffectHandle);
    
    /**
     * Removes all effects with specified tags
     * @param Tags Tags to match for removal
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "AbilitySystem|Effects")
    void RemoveEffectsWithTags(const FGameplayTagContainer& Tags);
    
    /**
     * Checks if effect is active
     * @param EffectHandle Effect handle to check
     * @return true if effect is active
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "AbilitySystem|Effects")
    bool IsEffectActive(FActiveGameplayEffectHandle EffectHandle) const;
    
    //================================================
    // Ability System Access
    //================================================
    
    /**
     * Gets ability system component
     * @return Ability system component
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "AbilitySystem")
    UAbilitySystemComponent* GetAbilitySystemComponent() const;
    
    /**
     * Initializes ability provider
     * @param InASC Ability system component to use
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "AbilitySystem")
    void InitializeAbilityProvider(UAbilitySystemComponent* InASC);
    
    /**
     * Checks if ability provider is initialized
     * @return true if initialized
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "AbilitySystem")
    bool IsInitialized() const;
    
    //================================================
    // Tag Management
    //================================================
    
    /**
     * Adds gameplay tags to owner
     * @param Tags Tags to add
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "AbilitySystem|Tags")
    void AddGameplayTags(const FGameplayTagContainer& Tags);
    
    /**
     * Removes gameplay tags from owner
     * @param Tags Tags to remove
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "AbilitySystem|Tags")
    void RemoveGameplayTags(const FGameplayTagContainer& Tags);
    
    /**
     * Checks if owner has all specified tags
     * @param Tags Tags to check
     * @return true if owner has all tags
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "AbilitySystem|Tags")
    bool HasAllGameplayTags(const FGameplayTagContainer& Tags) const;
    
    /**
     * Checks if owner has any specified tags
     * @param Tags Tags to check
     * @return true if owner has any of the tags
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "AbilitySystem|Tags")
    bool HasAnyGameplayTags(const FGameplayTagContainer& Tags) const;
    
    //================================================
    // Event System Access
    //================================================
    
    /**
     * Gets the central delegate manager for ability events
     * @return Pointer to delegate manager or nullptr if not available
     */
    virtual UEventDelegateManager* GetDelegateManager() const = 0;
    
    /**
     * Static helper to get delegate manager
     * @param WorldContextObject Any object with valid world context
     * @return Delegate manager or nullptr
     */
    static UEventDelegateManager* GetDelegateManagerStatic(const UObject* WorldContextObject);
    
    /**
     * Helper to broadcast ability granted events
     * @param Provider Provider object
     * @param AbilityHandle Granted ability handle
     * @param AbilityClass Ability class that was granted
     */
    static void BroadcastAbilityGranted(const UObject* Provider, FGameplayAbilitySpecHandle AbilityHandle, TSubclassOf<UGameplayAbility> AbilityClass);
    
    /**
     * Helper to broadcast effect applied events
     * @param Provider Provider object
     * @param EffectHandle Applied effect handle
     * @param EffectClass Effect class that was applied
     */
    static void BroadcastEffectApplied(const UObject* Provider, FActiveGameplayEffectHandle EffectHandle, TSubclassOf<UGameplayEffect> EffectClass);
};