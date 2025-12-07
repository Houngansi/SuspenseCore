// SuspenseCorePanelSwitcherWidget.h
// SuspenseCore - Panel Switcher (Tab Bar) Widget
// AAA Pattern: EventBus communication, SuspenseCoreButtonWidget for tabs
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCorePanelSwitcherWidget.generated.h"

// Forward declarations
class UHorizontalBox;
class UWidgetSwitcher;
class USuspenseCoreButtonWidget;
class USuspenseCoreEventBus;
class USuspenseCorePanelWidget;

/**
 * FSuspenseCorePanelTabConfig
 * Configuration for a panel tab (like FSuspenseTabConfig in legacy)
 */
USTRUCT(BlueprintType)
struct UISYSTEM_API FSuspenseCorePanelTabConfig
{
	GENERATED_BODY()

	/** Display name shown on tab button */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tab")
	FText DisplayName;

	/** Panel tag for identification (e.g., SuspenseCore.UI.Panel.Inventory) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tab")
	FGameplayTag PanelTag;

	/** Panel widget class to create for this tab */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tab")
	TSubclassOf<USuspenseCorePanelWidget> PanelWidgetClass;

	/** Optional icon texture for tab button */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tab")
	TObjectPtr<UTexture2D> TabIcon;

	/** Is this tab enabled by default */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tab")
	bool bEnabled = true;

	FSuspenseCorePanelTabConfig()
		: DisplayName(FText::FromString(TEXT("Tab")))
		, bEnabled(true)
	{
	}

	FSuspenseCorePanelTabConfig(const FGameplayTag& InTag, const FText& InName)
		: DisplayName(InName)
		, PanelTag(InTag)
		, bEnabled(true)
	{
	}
};

/**
 * FSuspenseCorePanelTabRuntime
 * Runtime data for active tab (button reference, etc.)
 */
USTRUCT(BlueprintType)
struct UISYSTEM_API FSuspenseCorePanelTabRuntime
{
	GENERATED_BODY()

	/** Panel tag identifier */
	UPROPERTY(BlueprintReadOnly, Category = "Tab")
	FGameplayTag PanelTag;

	/** Tab button widget instance */
	UPROPERTY(BlueprintReadOnly, Category = "Tab")
	TObjectPtr<USuspenseCoreButtonWidget> TabButton;

	/** Panel widget instance (created from config) */
	UPROPERTY(BlueprintReadOnly, Category = "Tab")
	TObjectPtr<USuspenseCorePanelWidget> PanelWidget;

	/** Is tab currently enabled */
	bool bEnabled = true;

	FSuspenseCorePanelTabRuntime() = default;
};

/**
 * USuspenseCorePanelSwitcherWidget
 *
 * AAA-grade tab bar widget for switching between panels.
 * Uses SuspenseCoreButtonWidget for tab buttons with consistent styling.
 * Communicates ONLY via EventBus (no direct delegates between widgets).
 *
 * ARCHITECTURE (per BestPractices.md):
 * - TabConfigs defined in Blueprint (like legacy SuspenseUpperTabBar)
 * - Creates SuspenseCoreButtonWidget for each tab
 * - Uses EventBus for panel selection events
 * - ActionTag on each button for identification
 *
 * USAGE:
 * 1. Define TabConfigs in Blueprint
 * 2. Bind TabContainer (HorizontalBox) and PanelContainer (WidgetSwitcher)
 * 3. Widget auto-creates tab buttons and panel widgets
 *
 * @see USuspenseCoreContainerScreenWidget
 * @see USuspenseCoreButtonWidget
 */
UCLASS(BlueprintType, Blueprintable)
class UISYSTEM_API USuspenseCorePanelSwitcherWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCorePanelSwitcherWidget(const FObjectInitializer& ObjectInitializer);

	//==================================================================
	// UUserWidget Interface
	//==================================================================

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativePreConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	//==================================================================
	// Tab Management API
	//==================================================================

	/**
	 * Initialize tabs from config (called in NativeConstruct)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|PanelSwitcher")
	void InitializeTabs();

	/**
	 * Clear all runtime tabs
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|PanelSwitcher")
	void ClearTabs();

	/**
	 * Get tab count
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|PanelSwitcher")
	int32 GetTabCount() const { return RuntimeTabs.Num(); }

	/**
	 * Select tab by index
	 * @param TabIndex Index of tab to select
	 * @return Success
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|PanelSwitcher")
	bool SelectTabByIndex(int32 TabIndex);

	/**
	 * Select tab by tag
	 * @param PanelTag Tag of panel to select
	 * @return Success
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|PanelSwitcher")
	bool SelectTabByTag(const FGameplayTag& PanelTag);

	/**
	 * Get currently selected tab index
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|PanelSwitcher")
	int32 GetSelectedTabIndex() const { return CurrentTabIndex; }

	/**
	 * Get currently active panel tag
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|PanelSwitcher")
	FGameplayTag GetActivePanel() const;

	/**
	 * Select next tab (wraps around)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|PanelSwitcher")
	void SelectNextTab();

	/**
	 * Select previous tab (wraps around)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|PanelSwitcher")
	void SelectPreviousTab();

	/**
	 * Set tab enabled state
	 * @param TabIndex Index of tab
	 * @param bEnabled Whether to enable
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|PanelSwitcher")
	void SetTabEnabled(int32 TabIndex, bool bEnabled);

	/**
	 * Refresh content of currently selected tab
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|PanelSwitcher")
	void RefreshActiveTabContent();

	//==================================================================
	// Blueprint Events
	//==================================================================

	/** Called when a tab button is created - allows Blueprint customization */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|PanelSwitcher", meta = (DisplayName = "On Tab Created"))
	void K2_OnTabCreated(int32 TabIndex, USuspenseCoreButtonWidget* TabButton);

	/** Called when tab selection changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|PanelSwitcher", meta = (DisplayName = "On Tab Selected"))
	void K2_OnTabSelected(int32 OldIndex, int32 NewIndex);

protected:
	//==================================================================
	// Configuration (set in Blueprint)
	//==================================================================

	/** Tab configurations (define in Blueprint like legacy TabConfigs) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration")
	TArray<FSuspenseCorePanelTabConfig> TabConfigs;

	/** Default tab index to select on initialization */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration", meta = (ClampMin = "0"))
	int32 DefaultTabIndex = 0;

	/** Tab button widget class (must be SuspenseCoreButtonWidget or subclass) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration")
	TSubclassOf<USuspenseCoreButtonWidget> TabButtonClass;

	/** Key to switch to next tab */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration")
	FKey NextTabKey = EKeys::Tab;

	/** Key to switch to previous tab (typically Shift+Tab via modifiers) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration")
	FKey PreviousTabKey;

	//==================================================================
	// Widget Bindings (bind in Blueprint)
	//==================================================================

	/** Container for tab buttons (HorizontalBox) */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Widgets")
	TObjectPtr<UHorizontalBox> TabContainer;

	/** Container for panel content (WidgetSwitcher) */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UWidgetSwitcher> PanelContainer;

	//==================================================================
	// Tab Creation (override for custom behavior)
	//==================================================================

	/** Create tab button from config */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|UI|PanelSwitcher")
	USuspenseCoreButtonWidget* CreateTabButton(const FSuspenseCorePanelTabConfig& Config, int32 TabIndex);
	virtual USuspenseCoreButtonWidget* CreateTabButton_Implementation(const FSuspenseCorePanelTabConfig& Config, int32 TabIndex);

	/** Create panel widget from config */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|UI|PanelSwitcher")
	USuspenseCorePanelWidget* CreatePanelWidget(const FSuspenseCorePanelTabConfig& Config, int32 TabIndex);
	virtual USuspenseCorePanelWidget* CreatePanelWidget_Implementation(const FSuspenseCorePanelTabConfig& Config, int32 TabIndex);

	/** Update tab visual state (selected/normal) */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|UI|PanelSwitcher")
	void UpdateTabVisual(int32 TabIndex, bool bIsSelected);
	virtual void UpdateTabVisual_Implementation(int32 TabIndex, bool bIsSelected);

	/** Update all tab visuals based on CurrentTabIndex */
	void UpdateAllTabVisuals();

	//==================================================================
	// Internal Handlers
	//==================================================================

	/** Handle tab button clicked (via SuspenseCoreButtonWidget delegate) */
	UFUNCTION()
	void OnTabButtonClicked(USuspenseCoreButtonWidget* Button);

	/** Publish panel selected event via EventBus */
	void PublishPanelSelected(const FGameplayTag& PanelTag);

	//==================================================================
	// EventBus
	//==================================================================

	/** Setup EventBus subscriptions */
	void SetupEventSubscriptions();

	/** Teardown EventBus subscriptions */
	void TeardownEventSubscriptions();

private:
	//==================================================================
	// Runtime State
	//==================================================================

	/** Runtime tab data (buttons, panels) */
	UPROPERTY(Transient)
	TArray<FSuspenseCorePanelTabRuntime> RuntimeTabs;

	/** Current selected tab index (-1 if none) */
	int32 CurrentTabIndex = -1;

	/** Map of buttons to tab indices (for click handling) */
	UPROPERTY(Transient)
	TMap<TObjectPtr<USuspenseCoreButtonWidget>, int32> ButtonToIndexMap;

	/** Cached EventBus reference */
	UPROPERTY(Transient)
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;
};
