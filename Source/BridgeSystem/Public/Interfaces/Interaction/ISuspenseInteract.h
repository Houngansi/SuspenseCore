// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "ISuspenseInteract.generated.h"

// Forward declarations
class APlayerController;
class USuspenseEventManager;

/**
 * Interface for all interactable objects in the world
 * Provides standardized interaction methods for both direct calls
 * and usage through Gameplay Ability System
 * 
 * ARCHITECTURAL IMPROVEMENTS:
 * - Integrated with centralized delegate system
 * - Provides static helper methods for event broadcasting
 * - Thread-safe event notifications
 */
UINTERFACE(MinimalAPI, BlueprintType, meta=(NotBlueprintable))
class USuspenseInteract : public UInterface
{
    GENERATED_BODY()
};

class BRIDGESYSTEM_API ISuspenseInteract
{
    GENERATED_BODY()

public:
    //================================================
    // Core Interaction Methods
    //================================================
    
    /**
     * Performs interaction with the object
     * @param InstigatingController Player controller initiating the interaction
     * @return true if interaction was successful
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interaction")
    bool Interact(APlayerController* InstigatingController);
    
    /**
     * Checks if interaction is possible
     * @param InstigatingController Player controller checking interaction
     * @return true if interaction is possible
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interaction")
    bool CanInteract(APlayerController* InstigatingController) const;
    
    /**
     * Returns interaction type for UI visualization
     * @return Interaction type tag
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interaction")
    FGameplayTag GetInteractionType() const;
    
    /**
     * Gets text for UI display
     * @return Interaction text
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interaction")
    FText GetInteractionText() const;
    
    //================================================
    // Extended Interaction Methods
    //================================================
    
    /**
     * Gets interaction priority when multiple objects are available
     * @return Priority value (higher = more priority)
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interaction")
    int32 GetInteractionPriority() const;
    
    /**
     * Gets required distance for interaction
     * @return Maximum interaction distance in units
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interaction")
    float GetInteractionDistance() const;
    
    /**
     * Called when object becomes focused for interaction
     * @param InstigatingController Controller that focused on object
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interaction")
    void OnInteractionFocusGained(APlayerController* InstigatingController);
    
    /**
     * Called when object loses interaction focus
     * @param InstigatingController Controller that lost focus
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interaction")
    void OnInteractionFocusLost(APlayerController* InstigatingController);
    
    //================================================
    // Centralized Event System Access
    //================================================
    
    /**
     * Gets the central delegate manager for interaction events
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
     * Helper to broadcast interaction started events
     * @param Interactable Object being interacted with
     * @param Instigator Controller initiating interaction
     * @param InteractionType Type of interaction
     */
    static void BroadcastInteractionStarted(
        const UObject* Interactable,
        APlayerController* Instigator,
        const FGameplayTag& InteractionType);
    
    /**
     * Helper to broadcast interaction completed events
     * @param Interactable Object that was interacted with
     * @param Instigator Controller that completed interaction
     * @param bSuccess Whether interaction succeeded
     */
    static void BroadcastInteractionCompleted(
        const UObject* Interactable,
        APlayerController* Instigator,
        bool bSuccess);
    
    /**
     * Helper to broadcast interaction focus events
     * @param Interactable Object gaining/losing focus
     * @param Instigator Controller changing focus
     * @param bGainedFocus True if gained, false if lost
     */
    static void BroadcastInteractionFocusChanged(
        const UObject* Interactable,
        APlayerController* Instigator,
        bool bGainedFocus);
};