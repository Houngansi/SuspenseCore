// SuspenseCorePanelSwitcherWidget.h
// SuspenseCore - Panel Switcher (Tab Bar) Widget
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "SuspenseCorePanelSwitcherWidget.generated.h"

// Forward declarations
class UHorizontalBox;
class UButton;
class UTextBlock;

/**
 * Delegate for panel selection
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSuspenseCorePanelSelected, const FGameplayTag&);

/**
 * FSuspenseCorePanelTab
 * Data for a single tab in the panel switcher
 */
USTRUCT(BlueprintType)
struct UISYSTEM_API FSuspenseCorePanelTab
{
	GENERATED_BODY()

	/** Panel tag identifier */
	UPROPERTY(BlueprintReadWrite, Category = "Tab")
	FGameplayTag PanelTag;

	/** Display name shown on tab */
	UPROPERTY(BlueprintReadWrite, Category = "Tab")
	FText DisplayName;

	/** Tab button widget */
	UPROPERTY(BlueprintReadWrite, Category = "Tab")
	TObjectPtr<UButton> TabButton;

	/** Tab text widget */
	UPROPERTY(BlueprintReadWrite, Category = "Tab")
	TObjectPtr<UTextBlock> TabText;

	FSuspenseCorePanelTab()
		: DisplayName(FText::GetEmpty())
	{
	}

	FSuspenseCorePanelTab(const FGameplayTag& InTag, const FText& InName)
		: PanelTag(InTag)
		, DisplayName(InName)
	{
	}
};

/**
 * USuspenseCorePanelSwitcherWidget
 *
 * Tab bar widget for switching between panels in a container screen.
 * Creates tab buttons for each panel and manages selection state.
 *
 * FEATURES:
 * - Dynamic tab creation from panel configs
 * - Visual feedback for selected/hovered tabs
 * - Blueprint-customizable tab appearance
 * - Keyboard navigation support
 *
 * @see USuspenseCoreContainerScreenWidget
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
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	//==================================================================
	// Tab Management
	//==================================================================

	/**
	 * Add a tab for a panel
	 * @param PanelTag Tag identifying the panel
	 * @param DisplayName Text to show on tab
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|PanelSwitcher")
	void AddTab(const FGameplayTag& PanelTag, const FText& DisplayName);

	/**
	 * Remove a tab
	 * @param PanelTag Tag of panel to remove
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|PanelSwitcher")
	void RemoveTab(const FGameplayTag& PanelTag);

	/**
	 * Clear all tabs
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|PanelSwitcher")
	void ClearTabs();

	/**
	 * Get all tabs
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|PanelSwitcher")
	TArray<FSuspenseCorePanelTab> GetAllTabs() const { return Tabs; }

	/**
	 * Get tab count
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|PanelSwitcher")
	int32 GetTabCount() const { return Tabs.Num(); }

	//==================================================================
	// Selection
	//==================================================================

	/**
	 * Set active panel (updates visual state)
	 * @param PanelTag Tag of panel to activate
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|PanelSwitcher")
	void SetActivePanel(const FGameplayTag& PanelTag);

	/**
	 * Get currently active panel tag
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|PanelSwitcher")
	FGameplayTag GetActivePanel() const { return ActivePanelTag; }

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

	//==================================================================
	// Events
	//==================================================================

	/**
	 * Get panel selected delegate
	 */
	FOnSuspenseCorePanelSelected& OnPanelSelected() { return PanelSelectedDelegate; }

	//==================================================================
	// Blueprint Events
	//==================================================================

	/** Called when a tab is created - allows Blueprint customization */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|PanelSwitcher", meta = (DisplayName = "On Tab Created"))
	void K2_OnTabCreated(const FSuspenseCorePanelTab& Tab);

	/** Called when tab selection changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|PanelSwitcher", meta = (DisplayName = "On Tab Selected"))
	void K2_OnTabSelected(const FGameplayTag& PanelTag);

protected:
	//==================================================================
	// Tab Creation
	//==================================================================

	/** Create tab button widget */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|UI|PanelSwitcher")
	UButton* CreateTabButton(const FSuspenseCorePanelTab& TabData);
	virtual UButton* CreateTabButton_Implementation(const FSuspenseCorePanelTab& TabData);

	/** Update tab visual state */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|UI|PanelSwitcher")
	void UpdateTabVisual(const FSuspenseCorePanelTab& Tab, bool bIsSelected);
	virtual void UpdateTabVisual_Implementation(const FSuspenseCorePanelTab& Tab, bool bIsSelected);

	/** Handle tab button clicked with index - allows reliable identification */
	void HandleTabClickedByIndex(int32 TabIndex);

	/** Handle tab button clicked with tag */
	void OnTabButtonClicked(FGameplayTag PanelTag);

	//==================================================================
	// Widget References (Bind in Blueprint)
	//==================================================================

	/** Container for tab buttons */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UHorizontalBox> TabContainer;

	//==================================================================
	// Configuration
	//==================================================================

	/** Style for normal tabs */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
	FButtonStyle NormalTabStyle;

	/** Style for selected tabs */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
	FButtonStyle SelectedTabStyle;

	/** Key to switch to next tab */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
	FKey NextTabKey;

	/** Key to switch to previous tab */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
	FKey PreviousTabKey;

private:
	//==================================================================
	// State
	//==================================================================

	/** All tabs */
	UPROPERTY(Transient)
	TArray<FSuspenseCorePanelTab> Tabs;

	/** Currently active panel tag */
	FGameplayTag ActivePanelTag;

	/** Panel selected delegate */
	FOnSuspenseCorePanelSelected PanelSelectedDelegate;

	/** Map of panel tags to tab indices */
	TMap<FGameplayTag, int32> TabIndexByTag;

	/** Map of buttons to panel tags (for click handling) */
	TMap<TObjectPtr<UButton>, FGameplayTag> ButtonToTagMap;
};
