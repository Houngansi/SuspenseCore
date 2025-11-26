// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ISuspenseCharacter.generated.h"

// Forward declarations
class AActor;
class UAbilitySystemComponent;
class USuspenseEventManager;

UINTERFACE(MinimalAPI, BlueprintType)
class USuspenseCharacter : public UInterface
{
    GENERATED_BODY()
};

/**
 * Extended interface for character interaction
 * Includes methods for weapon management and AbilitySystemComponent access
 */
class BRIDGESYSTEM_API ISuspenseCharacter
{
    GENERATED_BODY()

public:
    //================================================
    // Weapon Management
    //================================================
    
    /**
     * Sets whether the character has a weapon
     * @param bHasWeapon true if character has weapon
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Character|Weapon")
    void SetHasWeapon(bool bHasWeapon);
    
    /**
     * Sets the current weapon actor
     * @param WeaponActor Weapon actor
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Character|Weapon")
    void SetCurrentWeaponActor(AActor* WeaponActor);
    
    /**
     * Gets the current weapon actor
     * @return Weapon actor or nullptr if no weapon
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Character|Weapon")
    AActor* GetCurrentWeaponActor() const;
    
    /**
     * Checks if character has a weapon
     * @return true if character has weapon
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Character|Weapon")
    bool HasWeapon() const;
    
    //================================================
    // Ability System Access
    //================================================
    
    /**
     * Gets the character's AbilitySystemComponent
     * @return Pointer to AbilitySystemComponent or nullptr
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Character|Abilities")
    UAbilitySystemComponent* GetASC() const;
    
    /**
     * Gets the character's level
     * @return Character level
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Character|Stats")
    float GetCharacterLevel() const;
    
    //================================================
    // Character State
    //================================================
    
    /**
     * Checks if character is alive
     * @return true if character is alive
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Character|State")
    bool IsAlive() const;
    
    /**
     * Gets character's team ID
     * @return Team ID
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Character|Team")
    int32 GetTeamId() const;
    
    //================================================
    // Event System Access
    //================================================
    
    /**
     * Gets the central delegate manager for character events
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
     * Helper to broadcast weapon change events
     * @param Character Character object
     * @param NewWeapon New weapon actor
     * @param bHasWeapon Whether character has weapon
     */
    static void BroadcastWeaponChanged(const UObject* Character, AActor* NewWeapon, bool bHasWeapon);
};