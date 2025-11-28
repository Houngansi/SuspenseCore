// Copyright Suspense Team Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Base/SuspenseBaseWidget.h"
#include "Interfaces/UI/ISuspenseHUDWidget.h"
#include "Widgets/Screens/SuspenseCharacterScreen.h"
#include "SuspenseMainHUDWidget.generated.h"

// Forward declarations
class USuspenseHealthStaminaWidget;
class USuspenseCrosshairWidget;
class USuspenseWeaponUIWidget;
class USuspenseInventoryWidget;
class UTextBlock;
class ISuspenseAttributeProvider;

/**
 * Main HUD widget that serves as a container for all UI elements
 *
 * This widget follows the composition pattern where sub-widgets are
 * bound through the Blueprint editor. Each sub-widget (health, crosshair,
 * inventory, etc.) is a separate widget that can be designed independently.
 *
 * Blueprint Setup:
 * 1. Create a Blueprint based on this class
 * 2. In the Designer, add child widgets for each UI element
 * 3. Bind them to the corresponding properties (they'll appear in the Bind dropdown)
 * 4. Set the MainHUDClass in your PlayerController to this Blueprint
 */
UCLASS()
class UISYSTEM_API USuspenseMainHUDWidget : public USuspenseBaseWidget, public ISuspenseHUDWidget
{
    GENERATED_BODY()

public:
    USuspenseMainHUDWidget(const FObjectInitializer& ObjectInitializer);

    //~ Begin USuspenseBaseWidget Interface
    virtual void InitializeWidget_Implementation() override;
    virtual void UninitializeWidget_Implementation() override;
    virtual void UpdateWidget_Implementation(float DeltaTime) override;
    //~ End USuspenseBaseWidget Interface

    //~ Begin ISuspenseHUDWidgetInterface Interface
    virtual void SetupForPlayer_Implementation(APawn* Character) override;
    virtual void CleanupHUD_Implementation() override;
    virtual UUserWidget* GetHealthStaminaWidget_Implementation() const override;
    virtual UUserWidget* GetCrosshairWidget_Implementation() const override;
    virtual UUserWidget* GetWeaponInfoWidget_Implementation() const override;
    virtual UUserWidget* GetInventoryWidget_Implementation() const override;
    virtual void ShowCombatElements_Implementation(bool bShow) override;
    virtual void ShowNonCombatElements_Implementation(bool bShow) override;
    virtual void SetHUDOpacity_Implementation(float Opacity) override;
    virtual void ShowInteractionPrompt_Implementation(const FText& PromptText) override;
    virtual void HideInteractionPrompt_Implementation() override;
    virtual void ShowInventory_Implementation() override;
    virtual void HideInventory_Implementation() override;
    virtual void ToggleInventory_Implementation() override;
    virtual bool IsInventoryVisible_Implementation() const override;
    virtual void RequestInventoryInitialization_Implementation() override;
    //~ End ISuspenseHUDWidgetInterface Interface

    /**
     * Sets up the HUD with an attribute provider for health/stamina updates
     * This is typically called when the HUD is initialized with a character
     * @param Provider Interface to the object providing attribute data
     */
    UFUNCTION(BlueprintCallable, Category = "UI|HUD")
    void SetupWithProvider(TScriptInterface<ISuspenseAttributeProvider> Provider);

    /**
     * Opens the character screen with a specific tab
     * This is the primary method for showing any character-related UI
     * @param TabTag The tag of the tab to open (e.g., UI.Tab.Inventory, UI.Tab.Stats)
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Character")
    void ShowCharacterScreenWithTab(const FGameplayTag& TabTag);

    /**
     * Closes the character screen
     * This hides the entire character menu system
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Character")
    void HideCharacterScreen();

    /**
     * Toggles the character screen visibility
     * Opens with the last selected tab or default tab
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Character")
    void ToggleCharacterScreen();

    /**
     * Checks if the character screen is currently visible
     * @return true if the character screen is open
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Character")
    bool IsCharacterScreenVisible() const;

    // Backward compatibility methods for direct value setting
    // These bypass the provider system and directly update the widgets

    UFUNCTION(BlueprintCallable, Category = "UI|Health")
    void SetCurrentHealth_UI(float CurrentHealth);

    UFUNCTION(BlueprintCallable, Category = "UI|Health")
    void SetMaxHealth_UI(float MaxHealth);

    UFUNCTION(BlueprintCallable, Category = "UI|Health")
    void SetHealthPercentage_UI(float HealthPercentage);

    UFUNCTION(BlueprintCallable, Category = "UI|Health")
    float GetHealthPercentage() const;

    UFUNCTION(BlueprintCallable, Category = "UI|Stamina")
    void SetCurrentStamina_UI(float CurrentStamina);

    UFUNCTION(BlueprintCallable, Category = "UI|Stamina")
    void SetMaxStamina_UI(float MaxStamina);

    UFUNCTION(BlueprintCallable, Category = "UI|Stamina")
    void SetStaminaPercentage_UI(float StaminaPercentage);

    UFUNCTION(BlueprintCallable, Category = "UI|Stamina")
    float GetStaminaPercentage() const;

    UFUNCTION(BlueprintCallable, Category = "UI|Crosshair")
    void SetCrosshairVisibility(bool bVisible);

protected:
    //================================================
    // Bindable Widget Properties
    //================================================

    /**
     * Health and stamina display widget
     * REQUIRED: Must be bound in Blueprint
     */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    USuspenseHealthStaminaWidget* HealthStaminaWidget;

    /**
     * Dynamic crosshair widget
     * Optional: Only bind if your game uses a crosshair
     */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    USuspenseCrosshairWidget* DynamicCrosshair;

    /**
     * Weapon information display widget
     * Optional: Only needed for games with weapons
     */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    USuspenseWeaponUIWidget* WeaponInfoWidget;

    /**
     * Character screen widget
     * Optional: Contains the full character menu with tabs
     * This is the preferred way to handle inventory and other character screens
     */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    USuspenseCharacterScreen* CharacterScreen;

    /**
     * Legacy inventory widget
     * Optional: Direct inventory widget for backward compatibility
     * Prefer using CharacterScreen instead for new implementations
     */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    USuspenseInventoryWidget* InventoryWidget;

    /**
     * Text block for interaction prompts
     * Optional: Used to display contextual interaction hints
     */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* InteractionPrompt;

    //================================================
    // Configuration Properties
    //================================================

    /**
     * Whether to automatically hide combat UI when no weapon is equipped
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI|HUD")
    bool bAutoHideCombatElements = true;

    /**
     * Duration for interaction prompt fade animations
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI|HUD")
    float InteractionPromptFadeDuration = 0.2f;

    /**
     * Default opacity for the entire HUD
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "UI|HUD", meta = (ClampMin = 0.0, ClampMax = 1.0))
    float DefaultHUDOpacity = 1.0f;

    /**
     * Called when inventory visibility changes
     * Override in Blueprint to add custom logic
     * @param bIsVisible New visibility state
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "UI|HUD", DisplayName = "On Inventory Visibility Changed")
    void K2_OnInventoryVisibilityChanged(bool bIsVisible);

    /**
     * Called when character screen visibility changes
     * Override in Blueprint to add custom logic
     * @param bIsVisible New visibility state
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "UI|HUD", DisplayName = "On Character Screen Visibility Changed")
    void K2_OnCharacterScreenVisibilityChanged(bool bIsVisible);

    /**
     * Native implementation of inventory visibility change
     * Calls the Blueprint event and handles any C++ logic
     */
    virtual void OnInventoryVisibilityChanged(bool bIsVisible);

    /**
     * Native implementation of character screen visibility change
     * Calls the Blueprint event and handles any C++ logic
     */
    virtual void OnCharacterScreenVisibilityChanged(bool bIsVisible);

private:
    /**
     * Initializes all child widgets with proper tags and settings
     */
    void InitializeChildWidgets();

    /**
     * Subscribes to relevant events from the EventDelegateManager
     */
    void SetupEventSubscriptions();

    /**
     * Cleans up event subscriptions
     */
    void ClearEventSubscriptions();

    /**
     * Validates that required widgets are properly bound
     * @return true if all required widgets are valid
     */
    bool ValidateWidgetBindings() const;

    /**
     * Initializes inventory bridge on first request
     * Отложенная инициализация - вызывается только при первом открытии инвентаря
     */
    void EnsureInventoryBridgeInitialized();

    // Event Handlers
    void OnActiveWeaponChanged(AActor* NewWeapon);
    void OnCrosshairUpdateRequested(float Spread, float Recoil);
    void OnCrosshairColorChanged(FLinearColor NewColor);
    void OnNotificationReceived(const FString& Message, float Duration);
    void OnInventoryCloseRequested();

    // Internal state
    UPROPERTY()
    TScriptInterface<ISuspenseAttributeProvider> AttributeProvider;

    UPROPERTY()
    APawn* OwningPawn;

    bool bCombatElementsVisible = true;
    bool bNonCombatElementsVisible = true;
    bool bInventoryBridgeInitialized = false; // Флаг инициализации Bridge

    // Event subscription handles
    FDelegateHandle WeaponChangedHandle;
    FDelegateHandle CrosshairUpdateHandle;
    FDelegateHandle CrosshairColorHandle;
    FDelegateHandle NotificationHandle;

    bool bIsSetup = false;
};
