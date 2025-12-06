// SuspenseCoreMenuWidget.h
// SuspenseCore - Base Menu Widget with Procedural Buttons
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SuspenseCoreMenuButtonConfig.h"
#include "SuspenseCoreMenuWidget.generated.h"

class UVerticalBox;
class UPanelWidget;
class USuspenseCoreButtonWidget;

/**
 * USuspenseCoreMenuWidget
 *
 * Base class for menus with procedural button creation.
 * Instead of hardcoding button bindings, buttons are created
 * from configuration array.
 *
 * USAGE IN BLUEPRINT:
 * 1. Create WBP derived from USuspenseCoreMenuWidget
 * 2. Add a VerticalBox named "ButtonContainer"
 * 3. Set ButtonConfigs array in Details panel
 * 4. Set ButtonWidgetClass to your button widget BP
 * 5. Bind to OnMenuButtonClicked delegate
 *
 * USAGE IN C++:
 * ```cpp
 * // Override GetDefaultButtonConfigs() to provide buttons
 * virtual TArray<FSuspenseCoreMenuButtonConfig> GetDefaultButtonConfigs() const override
 * {
 *     return {
 *         { TAG_Play, LOCTEXT("Play", "PLAY"), ESuspenseCoreButtonStyle::Primary },
 *         { TAG_Settings, LOCTEXT("Settings", "SETTINGS") },
 *         { TAG_Quit, LOCTEXT("Quit", "QUIT"), ESuspenseCoreButtonStyle::Danger }
 *     };
 * }
 * ```
 */
UCLASS(Blueprintable, BlueprintType)
class UISYSTEM_API USuspenseCoreMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreMenuWidget(const FObjectInitializer& ObjectInitializer);

	//==================================================================
	// UUserWidget
	//==================================================================

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//==================================================================
	// Public API
	//==================================================================

	/**
	 * Rebuild all buttons from configuration
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Menu")
	void RebuildButtons();

	/**
	 * Get button by action tag
	 * @param ActionTag Tag to find
	 * @return Button widget or nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Menu")
	USuspenseCoreButtonWidget* GetButtonByTag(FGameplayTag ActionTag) const;

	/**
	 * Set button enabled state by tag
	 * @param ActionTag Button tag
	 * @param bEnabled Enabled state
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Menu")
	void SetButtonEnabled(FGameplayTag ActionTag, bool bEnabled);

	/**
	 * Add button at runtime
	 * @param Config Button configuration
	 * @return Created button or nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Menu")
	USuspenseCoreButtonWidget* AddButton(const FSuspenseCoreMenuButtonConfig& Config);

	/**
	 * Remove button by tag
	 * @param ActionTag Button to remove
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Menu")
	void RemoveButton(FGameplayTag ActionTag);

	//==================================================================
	// Delegates
	//==================================================================

	/** Fired when any menu button is clicked */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Menu")
	FOnMenuButtonAction OnMenuButtonClicked;

protected:
	//==================================================================
	// Configuration
	//==================================================================

	/** Container for buttons (VerticalBox or HorizontalBox) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> ButtonContainer;

	/** Button widget class to spawn */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Menu")
	TSubclassOf<USuspenseCoreButtonWidget> ButtonWidgetClass;

	/** Button configurations */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Menu")
	TArray<FSuspenseCoreMenuButtonConfig> ButtonConfigs;

	/** Space between buttons */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Menu")
	float ButtonSpacing = 10.0f;

	//==================================================================
	// Override Points
	//==================================================================

	/**
	 * Override to provide default button configs in C++
	 * Called if ButtonConfigs array is empty
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|Menu")
	TArray<FSuspenseCoreMenuButtonConfig> GetDefaultButtonConfigs() const;
	virtual TArray<FSuspenseCoreMenuButtonConfig> GetDefaultButtonConfigs_Implementation() const;

	/**
	 * Called when a button is clicked (before delegate)
	 * Override to handle actions directly
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|Menu")
	void HandleButtonAction(FGameplayTag ActionTag, USuspenseCoreButtonWidget* Button);
	virtual void HandleButtonAction_Implementation(FGameplayTag ActionTag, USuspenseCoreButtonWidget* Button);

	/**
	 * Called after button is created, before adding to container
	 * Override to customize button appearance
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|Menu")
	void OnButtonCreated(USuspenseCoreButtonWidget* Button, const FSuspenseCoreMenuButtonConfig& Config);
	virtual void OnButtonCreated_Implementation(USuspenseCoreButtonWidget* Button, const FSuspenseCoreMenuButtonConfig& Config);

	//==================================================================
	// Internal
	//==================================================================

	/** Created button widgets mapped by tag */
	UPROPERTY()
	TMap<FGameplayTag, TObjectPtr<USuspenseCoreButtonWidget>> ButtonMap;

	/** Create single button from config */
	USuspenseCoreButtonWidget* CreateButton(const FSuspenseCoreMenuButtonConfig& Config);

	/** Clear all buttons */
	void ClearButtons();

	/** Button click handler */
	UFUNCTION()
	void OnButtonClicked(USuspenseCoreButtonWidget* Button);
};
