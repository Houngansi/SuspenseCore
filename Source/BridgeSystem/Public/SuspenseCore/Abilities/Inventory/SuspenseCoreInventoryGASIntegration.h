// MedComCore/Abilities/Inventory/SuspenseInventoryGASIntegration.h
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayEffectTypes.h"
#include "SuspenseCoreInventoryGASIntegration.generated.h"

// Forward declarations
class UAbilitySystemComponent;
class UGameplayEffect;
class UGameplayAbility;
class UAttributeSet;

/**
 * Class for integrating inventory system with Gameplay Ability System
 */
UCLASS(BlueprintType)
class BRIDGESYSTEM_API USuspenseInventoryGASIntegration : public UObject
{
    GENERATED_BODY()
    
public:
    /**
     * Initializes with an ability system component
     * @param InASC Ability system component
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|GAS")
    void Initialize(UAbilitySystemComponent* InASC);
    
    /**
     * Applies an effect from an inventory item
     * @param ItemID ID of the item
     * @param EffectClass Effect class to apply
     * @param Level Effect level
     * @return Handle to the applied effect
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|GAS")
    FActiveGameplayEffectHandle ApplyItemEffect(FName ItemID, TSubclassOf<UGameplayEffect> EffectClass, float Level = 1.0f);
    
    /**
     * Gives an ability from an inventory item
     * @param ItemID ID of the item
     * @param AbilityClass Ability class to give
     * @param Level Ability level
     * @return Spec handle
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|GAS")
    FGameplayAbilitySpecHandle GiveItemAbility(FName ItemID, TSubclassOf<UGameplayAbility> AbilityClass, int32 Level = 1);
    
    /**
     * Removes an effect from an inventory item
     * @param ItemID ID of the item
     * @param EffectClass Effect class to remove
     * @return True if removed
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|GAS")
    bool RemoveItemEffect(FName ItemID, TSubclassOf<UGameplayEffect> EffectClass);
    
    /**
     * Removes an ability from an inventory item
     * @param ItemID ID of the item
     * @param AbilityClass Ability class to remove
     * @return True if removed
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|GAS")
    bool RemoveItemAbility(FName ItemID, TSubclassOf<UGameplayAbility> AbilityClass);
    
    /**
     * Gets active effects from a specific item
     * @param ItemID ID of the item
     * @return Array of active effect handles
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|GAS")
    TArray<FActiveGameplayEffectHandle> GetActiveItemEffects(FName ItemID) const;
    
    /**
     * Gets active abilities from a specific item
     * @param ItemID ID of the item
     * @return Array of ability spec handles
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|GAS")
    TArray<FGameplayAbilitySpecHandle> GetActiveItemAbilities(FName ItemID) const;
    
    /**
     * Applies weight limit as a gameplay effect
     * @param MaxWeight Maximum weight
     * @param CurrentWeight Current weight
     * @return Effect handle
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|GAS")
    FActiveGameplayEffectHandle ApplyWeightEffect(float MaxWeight, float CurrentWeight);
    
    /**
     * Updates weight effect
     * @param NewCurrentWeight New current weight
     * @return True if successful
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|GAS")
    bool UpdateWeightEffect(float NewCurrentWeight);
    
    /**
     * Clears all item effects and abilities
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|GAS")
    void ClearAllItemEffects();
    
private:
    /** Reference to ability system component */
    UPROPERTY()
    UAbilitySystemComponent* ASC;
    
    /** Map of item IDs to effect handles */
    TMap<FName, TArray<FActiveGameplayEffectHandle>> ItemEffectMap;
    
    /** Map of item IDs to ability handles */
    TMap<FName, TArray<FGameplayAbilitySpecHandle>> ItemAbilityMap;
    
    /** Handle to weight effect */
    FActiveGameplayEffectHandle WeightEffectHandle;
};