// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameFramework/PlayerState.h"
#include "ISuspenseCoreHUDWidget.generated.h"

// Forward declarations
class APawn;
class UUserWidget;
class USuspenseCoreEventManager;

UINTERFACE(MinimalAPI, BlueprintType)
class USuspenseCoreHUDWidget : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for main HUD widget
 * Manages and coordinates all HUD sub-widgets
 */
class BRIDGESYSTEM_API ISuspenseCoreHUDWidget
{
    GENERATED_BODY()

public:
    //================================================
    // HUD Setup
    //================================================
    
    /**
     * Sets up HUD for specific player character
     * @param Character Character to display HUD for
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|HUD")
    void SetupForPlayer(APawn* Character);

    /**
     * Cleans up HUD when player disconnects
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|HUD")
    void CleanupHUD();

    //================================================
    // Sub-Widget Access
    //================================================
    
    /**
     * Gets health/stamina widget
     * @return Health/stamina widget or nullptr
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|HUD")
    UUserWidget* GetHealthStaminaWidget() const;

    /**
     * Gets crosshair widget
     * @return Crosshair widget or nullptr
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|HUD")
    UUserWidget* GetCrosshairWidget() const;

    /**
     * Gets weapon info widget
     * @return Weapon info widget or nullptr
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|HUD")
    UUserWidget* GetWeaponInfoWidget() const;

    /**
     * Gets inventory widget
     * @return Inventory widget or nullptr
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|HUD")
    UUserWidget* GetInventoryWidget() const;

    //================================================
    // HUD Visibility
    //================================================
    
    /**
     * Shows or hides combat-related UI elements
     * @param bShow Whether to show combat elements
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|HUD")
    void ShowCombatElements(bool bShow);

    /**
     * Shows or hides non-combat UI elements
     * @param bShow Whether to show non-combat elements
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|HUD")
    void ShowNonCombatElements(bool bShow);

    /**
     * Sets overall HUD opacity
     * @param Opacity Opacity value (0-1)
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|HUD")
    void SetHUDOpacity(float Opacity);

    //================================================
    // Inventory Management
    //================================================
    
    /**
     * Shows inventory UI
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|HUD")
    void ShowInventory();

    /**
     * Hides inventory UI
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|HUD")
    void HideInventory();

    /**
     * Toggles inventory visibility
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|HUD")
    void ToggleInventory();

    /**
     * Checks if inventory is visible
     * @return True if inventory is visible
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|HUD")
    bool IsInventoryVisible() const;

    /**
     * Requests inventory initialization through event system
     * 
     * This method triggers inventory setup without direct dependencies.
     * The actual connection to game data happens through event listeners
     * that have access to game-specific classes.
     * 
     * This approach maintains clean module boundaries while allowing
     * flexible communication between UI and game logic.
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|HUD")
    void RequestInventoryInitialization();

    //================================================
    // Interaction Prompts
    //================================================
    
    /**
     * Shows interaction prompt
     * @param PromptText Text to display
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|HUD")
    void ShowInteractionPrompt(const FText& PromptText);

    /**
     * Hides interaction prompt
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|HUD")
    void HideInteractionPrompt();

    //================================================
    // Event System Access
    //================================================

    /**
     * Static helper to get event manager
     * @param WorldContextObject Any object with valid world context
     * @return Event manager or nullptr
     */
    static USuspenseCoreEventManager* GetDelegateManagerStatic(const UObject* WorldContextObject);
};

