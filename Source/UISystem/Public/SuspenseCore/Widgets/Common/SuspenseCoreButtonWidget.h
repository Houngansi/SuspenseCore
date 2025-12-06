// SuspenseCoreButtonWidget.h
// SuspenseCore - Universal Button Widget
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreButtonWidget.generated.h"

// Forward declarations
class UButton;
class UTextBlock;
class UImage;
class USoundBase;

/**
 * Button click delegate
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSuspenseCoreButtonClicked, USuspenseCoreButtonWidget*, Button);

/**
 * Button hover delegate
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSuspenseCoreButtonHovered, USuspenseCoreButtonWidget*, Button, bool, bIsHovered);

/**
 * ESuspenseCoreButtonStyle
 * Visual style presets for buttons
 */
UENUM(BlueprintType)
enum class ESuspenseCoreButtonStyle : uint8
{
	Primary		UMETA(DisplayName = "Primary"),		// Main action buttons
	Secondary	UMETA(DisplayName = "Secondary"),	// Secondary actions
	Tertiary	UMETA(DisplayName = "Tertiary"),	// Subtle actions
	Danger		UMETA(DisplayName = "Danger"),		// Destructive actions (delete, quit)
	Success		UMETA(DisplayName = "Success"),		// Confirm actions
	Ghost		UMETA(DisplayName = "Ghost"),		// Minimal/transparent
	Custom		UMETA(DisplayName = "Custom")		// Use custom colors
};

/**
 * FSuspenseCoreButtonColors
 * Color configuration for button states
 */
USTRUCT(BlueprintType)
struct UISYSTEM_API FSuspenseCoreButtonColors
{
	GENERATED_BODY()

	/** Normal state background color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor NormalBackground = FLinearColor(0.1f, 0.1f, 0.1f, 0.9f);

	/** Hovered state background color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor HoveredBackground = FLinearColor(0.2f, 0.2f, 0.2f, 1.0f);

	/** Pressed state background color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor PressedBackground = FLinearColor(0.05f, 0.05f, 0.05f, 1.0f);

	/** Disabled state background color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor DisabledBackground = FLinearColor(0.1f, 0.1f, 0.1f, 0.5f);

	/** Text color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor TextColor = FLinearColor::White;

	/** Disabled text color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor DisabledTextColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);

	/** Border/accent color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colors")
	FLinearColor AccentColor = FLinearColor(0.8f, 0.6f, 0.2f, 1.0f);
};

/**
 * USuspenseCoreButtonWidget
 *
 * Universal button widget for SuspenseCore UI.
 * Provides consistent styling, sounds, and behavior across all UI.
 *
 * FEATURES:
 * - Text + optional icon
 * - Multiple style presets (Primary, Secondary, Danger, etc.)
 * - Hover/press animations
 * - Sound effects
 * - Keyboard focus support
 * - GameplayTag-based action identification
 *
 * USAGE:
 * 1. Add to any widget as a child
 * 2. Bind ButtonText and optionally Icon
 * 3. Set Style or CustomColors
 * 4. Connect OnButtonClicked delegate
 *
 * IN BLUEPRINT:
 * - Override K2_OnClicked for custom behavior
 * - Use ActionTag to identify button purpose
 *
 * @see ESuspenseCoreButtonStyle
 */
UCLASS(BlueprintType, Blueprintable)
class UISYSTEM_API USuspenseCoreButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreButtonWidget(const FObjectInitializer& ObjectInitializer);

	//==================================================================
	// UUserWidget Interface
	//==================================================================

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnFocusLost(const FFocusEvent& InFocusEvent) override;

	//==================================================================
	// Button Configuration
	//==================================================================

	/**
	 * Set button text
	 * @param InText Text to display
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Button")
	void SetButtonText(const FText& InText);

	/**
	 * Get button text
	 * @return Current button text
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Button")
	FText GetButtonText() const { return ButtonText; }

	/**
	 * Set button icon (optional)
	 * @param InIcon Icon texture/brush
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Button")
	void SetButtonIcon(UTexture2D* InIcon);

	/**
	 * Set button enabled state
	 * @param bEnabled Whether button is enabled
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Button")
	void SetButtonEnabled(bool bEnabled);

	/**
	 * Check if button is enabled
	 * @return true if enabled
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Button")
	bool IsButtonEnabled() const { return bIsEnabled; }

	/**
	 * Set button style preset
	 * @param NewStyle Style preset to apply
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Button")
	void SetButtonStyle(ESuspenseCoreButtonStyle NewStyle);

	/**
	 * Set custom colors (only works when Style is Custom)
	 * @param NewColors Custom color configuration
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Button")
	void SetCustomColors(const FSuspenseCoreButtonColors& NewColors);

	/**
	 * Simulate button click programmatically
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Button")
	void SimulateClick();

	//==================================================================
	// Events
	//==================================================================

	/** Called when button is clicked */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Button|Events")
	FOnSuspenseCoreButtonClicked OnButtonClicked;

	/** Called when button hover state changes */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Button|Events")
	FOnSuspenseCoreButtonHovered OnButtonHovered;

	//==================================================================
	// Blueprint Events
	//==================================================================

	/** Blueprint event for click handling */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Button", meta = (DisplayName = "On Clicked"))
	void K2_OnClicked();

	/** Blueprint event for hover handling */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Button", meta = (DisplayName = "On Hovered"))
	void K2_OnHovered(bool bIsHovered);

	/** Blueprint event for focus handling */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Button", meta = (DisplayName = "On Focus Changed"))
	void K2_OnFocusChanged(bool bHasFocus);

	/**
	 * Set action tag for identification
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Button")
	void SetActionTag(FGameplayTag NewTag) { ActionTag = NewTag; }

	/**
	 * Get action tag
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Button")
	FGameplayTag GetActionTag() const { return ActionTag; }

	/**
	 * Set tooltip text
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Button")
	void SetTooltipText(const FText& NewTooltip) { ButtonTooltipText = NewTooltip; }

protected:
	//==================================================================
	// Widget Bindings (set in Blueprint)
	//==================================================================

	/** Main button widget */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Widgets")
	TObjectPtr<UButton> MainButton;

	/** Text block for button label */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UTextBlock> ButtonTextBlock;

	/** Optional icon image */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UImage> ButtonIcon;

	//==================================================================
	// Configuration
	//==================================================================

	/** Button display text */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
	FText ButtonText;

	/** Button style preset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
	ESuspenseCoreButtonStyle Style = ESuspenseCoreButtonStyle::Primary;

	/** Custom colors (used when Style is Custom) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration",
		meta = (EditCondition = "Style == ESuspenseCoreButtonStyle::Custom"))
	FSuspenseCoreButtonColors CustomColors;

	/** Action tag for identification (e.g., SuspenseCore.UIAction.Play) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
	FGameplayTag ActionTag;

	/** Optional button tooltip text */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
	FText ButtonTooltipText;

	//==================================================================
	// Audio
	//==================================================================

	/** Sound to play on click */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundBase> ClickSound;

	/** Sound to play on hover */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundBase> HoverSound;

	//==================================================================
	// Internal Handlers
	//==================================================================

	/** Handle button clicked */
	UFUNCTION()
	void OnMainButtonClicked();

	/** Handle button hovered */
	UFUNCTION()
	void OnMainButtonHovered();

	/** Handle button unhovered */
	UFUNCTION()
	void OnMainButtonUnhovered();

	/** Handle button pressed */
	UFUNCTION()
	void OnMainButtonPressed();

	/** Handle button released */
	UFUNCTION()
	void OnMainButtonReleased();

	/** Apply style colors to button */
	void ApplyStyle();

	/** Get colors for current style */
	FSuspenseCoreButtonColors GetStyleColors() const;

	/** Update visual state based on current properties */
	void UpdateVisualState();

	/** Play sound effect */
	void PlaySound(USoundBase* Sound);

private:
	//==================================================================
	// State
	//==================================================================

	/** Is button currently enabled */
	bool bIsEnabled = true;

	/** Is button currently hovered */
	bool bIsHovered = false;

	/** Is button currently pressed */
	bool bIsPressed = false;

	/** Is button currently focused */
	bool bIsFocused = false;
};
