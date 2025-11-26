// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "ISuspenseUIWidget.generated.h"

// Forward declarations - только базовые UE типы
class USuspenseEventManager;

UINTERFACE(MinimalAPI, BlueprintType)
class USuspenseUIWidget : public UInterface
{
    GENERATED_BODY()
};

/**
 * Base interface for all UI widgets in the game
 * Provides standardized lifecycle and event handling
 */
class BRIDGESYSTEM_API ISuspenseUIWidget
{
    GENERATED_BODY()

public:
    //================================================
    // Widget Lifecycle
    //================================================
    
    /**
     * Called when widget is initialized
     * Use this instead of NativeConstruct for initialization
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Widget")
    void InitializeWidget();

    /**
     * Called when widget is being destroyed
     * Use this for cleanup
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Widget")
    void UninitializeWidget();

    /**
     * Called to update widget state
     * @param DeltaTime Time since last update
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Widget")
    void UpdateWidget(float DeltaTime);

    //================================================
    // Widget Properties
    //================================================
    
    /**
     * Gets the widget's unique tag
     * @return Widget tag
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Widget")
    FGameplayTag GetWidgetTag() const;

    /**
     * Sets the widget's tag
     * @param NewTag New tag to set
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Widget")
    void SetWidgetTag(const FGameplayTag& NewTag);

    /**
     * Checks if widget is initialized
     * @return true if initialized
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Widget")
    bool IsInitialized() const;

    //================================================
    // Visibility Management
    //================================================
    
    /**
     * Shows the widget with optional animation
     * @param bAnimate Whether to animate the show
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Widget")
    void ShowWidget(bool bAnimate = true);

    /**
     * Hides the widget with optional animation
     * @param bAnimate Whether to animate the hide
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Widget")
    void HideWidget(bool bAnimate = true);

    /**
     * Called when visibility changes
     * @param bIsVisible New visibility state
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Widget")
    void OnVisibilityChanged(bool bIsVisible);

    //================================================
    // Event System Access
    //================================================
    
    /**
     * Gets the event delegate manager
     * @return Event manager or nullptr
     */
    virtual USuspenseEventManager* GetDelegateManager() const = 0;

    /**
     * Static helper to get event manager
     * @param WorldContextObject Any object with valid world context
     * @return Event manager or nullptr
     */
    static USuspenseEventManager* GetDelegateManagerStatic(const UObject* WorldContextObject);
    
    /**
     * Helper to broadcast widget created event
     * @param Widget Widget that was created
     */
    static void BroadcastWidgetCreated(const UObject* Widget);
    
    /**
     * Helper to broadcast widget destroyed event
     * @param Widget Widget that was destroyed
     */
    static void BroadcastWidgetDestroyed(const UObject* Widget);
    
    /**
     * Helper to broadcast visibility changed event
     * @param Widget Widget whose visibility changed
     * @param bIsVisible New visibility state
     */
    static void BroadcastVisibilityChanged(const UObject* Widget, bool bIsVisible);
};