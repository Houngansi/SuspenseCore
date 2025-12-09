// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ISuspenseCoreCrosshairWidget.generated.h"

// Forward declarations - только базовые UE типы
class USuspenseCoreEventManager;

UINTERFACE(MinimalAPI, BlueprintType)
class USuspenseCoreCrosshairWidgetInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for crosshair widgets
 * Provides standardized methods for dynamic crosshair updates
 */
class BRIDGESYSTEM_API ISuspenseCoreCrosshairWidgetInterface
{
    GENERATED_BODY()

public:
    //================================================
    // Crosshair State
    //================================================
    
    /**
     * Updates crosshair spread and recoil
     * @param Spread Current weapon spread
     * @param Recoil Current recoil amount
     * @param bIsFiring Whether weapon is currently firing
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Crosshair")
    void UpdateCrosshair(float Spread, float Recoil, bool bIsFiring);

    /**
     * Sets crosshair visibility
     * @param bVisible Whether crosshair should be visible
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Crosshair")
    void SetCrosshairVisibility(bool bVisible);

    /**
     * Gets current crosshair visibility
     * @return true if crosshair is visible
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Crosshair")
    bool IsCrosshairVisible() const;

    //================================================
    // Crosshair Appearance
    //================================================
    
    /**
     * Sets crosshair color
     * @param NewColor New crosshair color
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Crosshair")
    void SetCrosshairColor(const FLinearColor& NewColor);

    /**
     * Gets current crosshair color
     * @return Current crosshair color
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Crosshair")
    FLinearColor GetCrosshairColor() const;

    /**
     * Sets crosshair style/type
     * @param CrosshairType Type of crosshair to display
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Crosshair")
    void SetCrosshairType(const FName& CrosshairType);

    //================================================
    // Crosshair Parameters
    //================================================
    
    /**
     * Sets minimum spread value
     * @param MinSpread Minimum spread when not firing
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Crosshair")
    void SetMinimumSpread(float MinSpread);

    /**
     * Sets maximum spread value
     * @param MaxSpread Maximum spread when firing
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Crosshair")
    void SetMaximumSpread(float MaxSpread);

    /**
     * Sets crosshair interpolation speed
     * @param Speed Interpolation speed for smooth transitions
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Crosshair")
    void SetInterpolationSpeed(float Speed);

    //================================================
    // Hit Indication
    //================================================
    
    /**
     * Shows hit marker
     * @param bHeadshot Whether hit was a headshot
     * @param bKill Whether hit resulted in kill
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Crosshair")
    void ShowHitMarker(bool bHeadshot, bool bKill);

    //================================================
    // Event System Access
    //================================================

    /**
     * Static helper to get event manager
     * @param WorldContextObject Any object with valid world context
     * @return Event manager or nullptr
     */
    static USuspenseCoreEventManager* GetDelegateManagerStatic(const UObject* WorldContextObject);
    
    /**
     * Helper to broadcast crosshair update event
     * @param Widget Widget broadcasting the event
     * @param Spread Current spread value
     * @param Recoil Current recoil value
     */
    static void BroadcastCrosshairUpdated(const UObject* Widget, float Spread, float Recoil);
    
    /**
     * Helper to broadcast crosshair color change event
     * @param Widget Widget broadcasting the event
     * @param NewColor New crosshair color
     */
    static void BroadcastCrosshairColorChanged(const UObject* Widget, const FLinearColor& NewColor);
};

