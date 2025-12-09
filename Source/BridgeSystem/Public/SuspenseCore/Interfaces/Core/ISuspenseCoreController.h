// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "ISuspenseCoreController.generated.h"

// Forward declarations
class AActor;
class APawn;
class USuspenseCoreEventManager;

UINTERFACE(MinimalAPI, BlueprintType)
class USuspenseCoreController : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for controllers that manage characters and their weapons
 * Provides methods for communication between different systems and controller
 */
class BRIDGESYSTEM_API ISuspenseCoreController
{
    GENERATED_BODY()

public:
    //================================================
    // Weapon Management
    //================================================
    
    /**
     * Notifies controller about weapon change
     * @param NewWeapon New active weapon or nullptr when unequipping
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Controller|Weapon")
    void NotifyWeaponChanged(AActor* NewWeapon);
    
    /**
     * Returns current active weapon
     * @return Pointer to weapon or nullptr
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Controller|Weapon")
    AActor* GetCurrentWeapon() const;
    
    /**
     * Notifies controller about weapon state change
     * @param WeaponState New weapon state
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Controller|Weapon")
    void NotifyWeaponStateChanged(FGameplayTag WeaponState);
    
    /**
     * Checks if controller can use weapon at this moment
     * @return true if controller can use weapon
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Controller|Weapon")
    bool CanUseWeapon() const;
    
    //================================================
    // Pawn Management
    //================================================
    
    /**
     * Returns controlled pawn
     * @return Pointer to controlled pawn or nullptr
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Controller|Pawn")
    APawn* GetControlledPawn() const;
    
    /**
     * Checks if controller has valid pawn
     * @return true if controller has valid pawn
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Controller|Pawn")
    bool HasValidPawn() const;
    
    //================================================
    // Input Management
    //================================================
    
    /**
     * Notifies controller to update input bindings
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Controller|Input")
    void UpdateInputBindings();
    
    /**
     * Gets input priority for ability system
     * @return Input priority value
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Controller|Input")
    int32 GetInputPriority() const;
    
    //================================================
    // Event System Access
    //================================================
    
    /**
     * Gets the central delegate manager for controller events
     * @return Pointer to delegate manager or nullptr if not available
     */
    virtual USuspenseCoreEventManager* GetDelegateManager() const = 0;
    
    /**
     * Static helper to get delegate manager
     * @param WorldContextObject Any object with valid world context
     * @return Delegate manager or nullptr
     */
    static USuspenseCoreEventManager* GetDelegateManagerStatic(const UObject* WorldContextObject);
    
    /**
     * Helper to broadcast controller weapon events
     * @param Controller Controller object
     * @param NewWeapon New weapon actor
     */
    static void BroadcastControllerWeaponChanged(const UObject* Controller, AActor* NewWeapon);
};