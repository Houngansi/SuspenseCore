// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ISuspenseHealthStaminaWidget.generated.h"

// Forward declarations - только базовые UE типы
class UAbilitySystemComponent;
class USuspenseEventManager;

UINTERFACE(MinimalAPI, BlueprintType)
class USuspenseHealthStaminaWidgetInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for health and stamina display widgets
 * Provides standardized methods for updating character vitals
 */
class BRIDGESYSTEM_API ISuspenseHealthStaminaWidgetInterface
{
    GENERATED_BODY()

public:
    //================================================
    // Initialization
    //================================================
    
    /**
     * Initializes widget with ability system component
     * @param ASC Ability system component to track
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|HealthStamina")
    void InitializeWithASC(UAbilitySystemComponent* ASC);

    //================================================
    // Health Management
    //================================================
    
    /**
     * Updates health display
     * @param CurrentHealth Current health value
     * @param MaxHealth Maximum health value
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Health")
    void UpdateHealth(float CurrentHealth, float MaxHealth);

    /**
     * Gets current health percentage
     * @return Health percentage (0-1)
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Health")
    float GetHealthPercentage() const;

    /**
     * Sets whether to animate health changes
     * @param bAnimate Whether to animate
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Health")
    void SetAnimateHealthChanges(bool bAnimate);

    //================================================
    // Stamina Management
    //================================================
    
    /**
     * Updates stamina display
     * @param CurrentStamina Current stamina value
     * @param MaxStamina Maximum stamina value
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Stamina")
    void UpdateStamina(float CurrentStamina, float MaxStamina);

    /**
     * Gets current stamina percentage
     * @return Stamina percentage (0-1)
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Stamina")
    float GetStaminaPercentage() const;

    /**
     * Sets whether to animate stamina changes
     * @param bAnimate Whether to animate
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Stamina")
    void SetAnimateStaminaChanges(bool bAnimate);

    //================================================
    // Animation Settings
    //================================================
    
    /**
     * Sets animation speeds for interpolation
     * @param HealthSpeed Health interpolation speed
     * @param StaminaSpeed Stamina interpolation speed
     * @param BarSpeed Progress bar interpolation speed
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Animation")
    void SetInterpolationSpeeds(float HealthSpeed, float StaminaSpeed, float BarSpeed);

    /**
     * Forces immediate update without animation
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Animation")
    void ForceImmediateUpdate();

    //================================================
    // Event System Access
    //================================================

    /**
     * Static helper to get event manager
     * @param WorldContextObject Any object with valid world context
     * @return Event manager or nullptr
     */
    static USuspenseEventManager* GetDelegateManagerStatic(const UObject* WorldContextObject);
    
    /**
     * Helper to broadcast health update event
     * @param Widget Widget broadcasting the event
     * @param CurrentHealth Current health value
     * @param MaxHealth Maximum health value
     */
    static void BroadcastHealthUpdated(const UObject* Widget, float CurrentHealth, float MaxHealth);
    
    /**
     * Helper to broadcast stamina update event
     * @param Widget Widget broadcasting the event
     * @param CurrentStamina Current stamina value
     * @param MaxStamina Maximum stamina value
     */
    static void BroadcastStaminaUpdated(const UObject* Widget, float CurrentStamina, float MaxStamina);
};