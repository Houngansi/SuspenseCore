// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "ISuspenseEnemy.generated.h"

// Forward declarations
class AActor;
class USuspenseEventManager;

UINTERFACE(MinimalAPI, BlueprintType)
class USuspenseEnemyInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for working with enemy characters (AI)
 * Provides access to basic AI functions
 */
class BRIDGESYSTEM_API ISuspenseEnemy
{
    GENERATED_BODY()

public:
    //================================================
    // Weapon Management
    //================================================
    
    /**
     * Sets current AI weapon
     * @param WeaponActor Weapon actor
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Enemy|Weapon")
    void SetCurrentWeapon(AActor* WeaponActor);
    
    /**
     * Gets current AI weapon
     * @return Weapon actor or nullptr if no weapon
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Enemy|Weapon")
    AActor* GetCurrentWeapon() const;
    
    /**
     * Checks if AI has weapon
     * @return true if AI has weapon
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Enemy|Weapon")
    bool HasWeapon() const;
    
    //================================================
    // AI State Management
    //================================================
    
    /**
     * Gets current AI state
     * @return Current AI state tag
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Enemy|State")
    FGameplayTag GetAIState() const;
    
    /**
     * Sets AI state
     * @param NewState New AI state tag
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Enemy|State")
    void SetAIState(const FGameplayTag& NewState);
    
    /**
     * Gets AI threat level
     * @return Threat level (0.0 - 1.0)
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Enemy|AI")
    float GetThreatLevel() const;
    
    /**
     * Gets AI awareness radius
     * @return Awareness radius in units
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Enemy|AI")
    float GetAwarenessRadius() const;
    
    //================================================
    // Combat Behavior
    //================================================
    
    /**
     * Checks if AI can attack
     * @return true if AI can attack
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Enemy|Combat")
    bool CanAttack() const;
    
    /**
     * Gets preferred combat range
     * @return Preferred combat distance in units
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Enemy|Combat")
    float GetPreferredCombatRange() const;
    
    //================================================
    // Event System Access
    //================================================
    
    /**
     * Gets the central delegate manager for enemy events
     * @return Pointer to delegate manager or nullptr if not available
     */
    virtual USuspenseEventManager* GetDelegateManager() const = 0;
    
    /**
     * Static helper to get delegate manager
     * @param WorldContextObject Any object with valid world context
     * @return Delegate manager or nullptr
     */
    static USuspenseEventManager* GetDelegateManagerStatic(const UObject* WorldContextObject);
    
    /**
     * Helper to broadcast enemy weapon events
     * @param Enemy Enemy object
     * @param NewWeapon New weapon actor
     */
    static void BroadcastEnemyWeaponChanged(const UObject* Enemy, AActor* NewWeapon);
};