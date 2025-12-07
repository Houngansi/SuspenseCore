// SuspenseCorePanelSwitcherWidget.h
// SuspenseCore - Panel Switcher (Tab Bar) Widget
// AAA Pattern: Copied from legacy SuspenseUpperTabBar with EventBus
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

/**
 * ESuspenseCorePanelLayoutType
 * Layout type for tab content (copied from legacy ETabContentLayoutType)
 */
UENUM(BlueprintType)
enum class ESuspenseCorePanelLayoutType : uint8
{
	/** Single widget - any UUserWidget */
	Single UMETA(DisplayName = "Single Widget"),

	/** Multiple widgets with layout - NOT IMPLEMENTED YET */
	Layout UMETA(DisplayName = "Layout Widget"),

	/** Custom composite widget */
	Custom UMETA(DisplayName = "Custom")
};

/**
 * FSuspenseCorePanelTabConfig
 * Tab configuration (copied from legacy FSuspenseTabConfig)
 * Supports any UUserWidget as content - inventory, equipment, combined screens, etc.
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

	/** Layout type for this tab */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tab")
	ESuspenseCorePanelLayoutType LayoutType = ESuspenseCorePanelLayoutType::Single;

	/** Content widget class - ANY UUserWidget! (for Single and Custom types) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tab",
		meta = (EditCondition = "LayoutType != ESuspenseCorePanelLayoutType::Layout"))
	TSubclassOf<UUserWidget> ContentWidgetClass;

	/** Optional icon texture for tab button */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tab")
	TObjectPtr<UTexture2D> TabIcon;

	/** Is this tab enabled by default */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tab")
	bool bEnabled = true;

	FSuspenseCorePanelTabConfig()
		: DisplayName(FText::FromString(TEXT("Tab")))
		, LayoutType(ESuspenseCorePanelLayoutType::Single)
		, bEnabled(true)
	{
	}
};

/**
 * FSuspenseCorePanelTabRuntime
 * Runtime data for active tab
 */
USTRUCT(BlueprintType)
struct UISYSTEM_API FSuspenseCorePanelTabRuntime
{
	GENERATED_BODY()

	/** Panel tag identifier */
	UPROPERTY(BlueprintReadOnly, Category = "Tab")
	FGameplayTag PanelTag;

	/** Tab button widget instance (SuspenseCoreButtonWidget) */
	UPROPERTY(BlueprintReadOnly, Category = "Tab")
	TObjectPtr<USuspenseCoreButtonWidget> TabButton;

	/** Content widget instance - ANY UUserWidget */
	UPROPERTY(BlueprintReadOnly, Category = "Tab")
	TObjectPtr<UUserWidget> ContentWidget;

	/** Is tab currently enabled */
	bool bEnabled = true;

	FSuspenseCorePanelTabRuntime() = default;
};

/**
 * USuspenseCorePanelSwitcherWidget
 *
 * AAA-grade tab bar widget for switching between panels.
 * COPIED FROM LEGACY SuspenseUpperTabBar with EventBus integration.
 *
 * FEATURES (same as legacy):
 * - TabConfigs defined in Blueprint
 * - ContentWidgetClass = ANY UUserWidget (inventory, equipment, combined screens)
 * - Creates SuspenseCoreButtonWidget for tab buttons
 * - Switches content in WidgetSwitcher (ContentSwitcher)
 * - Uses EventBus for panel selection events (SuspenseCore.Event.UI.Panel.Selected)
 *
 * USAGE:
 * 1. In Blueprint, add TabConfigs with:
 *    - DisplayName = "Inventory"
 *    - PanelTag = SuspenseCore.UI.Panel.Inventory
 *    - ContentWidgetClass = WBP_InventoryEquipmentScreen (your combined widget)
 * 2. Bind TabContainer (HorizontalBox) and ContentSwitcher (WidgetSwitcher)
 * 3. Widget auto-creates everything on NativeConstruct
 *
 * @see USuspenseUpperTabBar (legacy)
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
	// Tab Management API (same as legacy)
	//==================================================================

	/** Initialize tabs from TabConfigs (called automatically in NativeConstruct) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|PanelSwitcher")
	void InitializeTabs();

	/** Clear all runtime tabs */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|PanelSwitcher")
	void ClearTabs();

	/** Get tab count */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|PanelSwitcher")
	int32 GetTabCount() const { return RuntimeTabs.Num(); }

	/** Select tab by index */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|PanelSwitcher")
	bool SelectTabByIndex(int32 TabIndex);

	/** Select tab by tag */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|PanelSwitcher")
	bool SelectTabByTag(const FGameplayTag& PanelTag);

	/** Get currently selected tab index */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|PanelSwitcher")
	int32 GetSelectedTabIndex() const { return CurrentTabIndex; }

	/** Get currently selected tab tag (alias for GetActivePanel) */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|PanelSwitcher")
	FGameplayTag GetSelectedTabTag() const { return GetActivePanel(); }

	/** Get currently active panel tag */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|PanelSwitcher")
	FGameplayTag GetActivePanel() const;

	/** Get content widget for tab index */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|PanelSwitcher")
	UUserWidget* GetTabContent(int32 TabIndex) const;

	/** Select next tab (wraps around) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|PanelSwitcher")
	void SelectNextTab();

	/** Select previous tab (wraps around) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|PanelSwitcher")
	void SelectPreviousTab();

	/** Set tab enabled state */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|PanelSwitcher")
	void SetTabEnabled(int32 TabIndex, bool bEnabled);

	/** Refresh content of currently selected tab */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|PanelSwitcher")
	void RefreshActiveTabContent();

	//==================================================================
	// Backward Compatibility API (for ContainerScreen integration)
	//==================================================================

	/** Set active panel by tag (wrapper for SelectTabByTag) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|PanelSwitcher")
	void SetActivePanel(const FGameplayTag& PanelTag);

	/** Add a tab dynamically at runtime */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|PanelSwitcher")
	void AddTab(const FGameplayTag& PanelTag, const FText& DisplayName);

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
	// Configuration (set in Blueprint - SAME AS LEGACY)
	//==================================================================

	/** Tab configurations - define your screens here! */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration")
	TArray<FSuspenseCorePanelTabConfig> TabConfigs;

	/** Default tab index to select on initialization */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration", meta = (ClampMin = "0"))
	int32 DefaultTabIndex = 0;

	/** Tab button widget class (SuspenseCoreButtonWidget or Blueprint subclass) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration")
	TSubclassOf<USuspenseCoreButtonWidget> TabButtonClass;

	/** Key to switch to next tab */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration")
	FKey NextTabKey = EKeys::Tab;

	/** Key to switch to previous tab */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configuration")
	FKey PreviousTabKey;

	//==================================================================
	// Widget Bindings (bind in Blueprint - SAME AS LEGACY)
	//==================================================================

	/** Container for tab buttons (HorizontalBox) */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Widgets")
	TObjectPtr<UHorizontalBox> TabContainer;

	/** Content switcher - switches between content widgets */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Widgets")
	TObjectPtr<UWidgetSwitcher> ContentSwitcher;

	//==================================================================
	// Tab Creation (override for custom behavior)
	//==================================================================

	/** Create tab button from config */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|UI|PanelSwitcher")
	USuspenseCoreButtonWidget* CreateTabButton(const FSuspenseCorePanelTabConfig& Config, int32 TabIndex);
	virtual USuspenseCoreButtonWidget* CreateTabButton_Implementation(const FSuspenseCorePanelTabConfig& Config, int32 TabIndex);

	/** Create content widget from config (like legacy CreateTabContent) */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|UI|PanelSwitcher")
	UUserWidget* CreateContentWidget(const FSuspenseCorePanelTabConfig& Config, int32 TabIndex);
	virtual UUserWidget* CreateContentWidget_Implementation(const FSuspenseCorePanelTabConfig& Config, int32 TabIndex);

	/** Update tab visual state (selected/normal) */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|UI|PanelSwitcher")
	void UpdateTabVisual(int32 TabIndex, bool bIsSelected);
	virtual void UpdateTabVisual_Implementation(int32 TabIndex, bool bIsSelected);

	/** Update all tab visuals */
	void UpdateAllTabVisuals();

	//==================================================================
	// Internal Handlers
	//==================================================================

	/** Handle tab button clicked */
	UFUNCTION()
	void OnTabButtonClicked(USuspenseCoreButtonWidget* Button);

	/** Publish panel selected event via EventBus */
	void PublishPanelSelected(const FGameplayTag& PanelTag);

	//==================================================================
	// EventBus
	//==================================================================

	void SetupEventSubscriptions();
	void TeardownEventSubscriptions();

private:
	//==================================================================
	// Runtime State
	//==================================================================

	/** Runtime tab data */
	UPROPERTY(Transient)
	TArray<FSuspenseCorePanelTabRuntime> RuntimeTabs;

	/** Current selected tab index (-1 if none) */
	int32 CurrentTabIndex = -1;

	/** Map of buttons to tab indices */
	UPROPERTY(Transient)
	TMap<TObjectPtr<USuspenseCoreButtonWidget>, int32> ButtonToIndexMap;

	/** Cached EventBus reference */
	UPROPERTY(Transient)
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;
};
