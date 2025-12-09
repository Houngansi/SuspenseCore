// SuspenseCoreContainerScreenWidget.h
// SuspenseCore - Main Container Screen Widget
// AAA Pattern: Copied from legacy SuspenseCharacterScreen with EventBus
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCoreContainerScreenWidget.generated.h"

// Forward declarations
class USuspenseCorePanelSwitcherWidget;
class USuspenseCoreTooltipWidget;
class USuspenseCoreContextMenuWidget;
class USuspenseCoreUIManager;
class USuspenseCoreEventBus;
class UOverlay;

/**
 * USuspenseCoreContainerScreenWidget
 *
 * Main screen widget for container UI (Inventory, Equipment, Stash, Trader, etc.)
 * Uses PanelSwitcher for panel management (like legacy CharacterScreen uses UpperTabBar).
 *
 * IMPORTANT: Panel content is managed by PanelSwitcher.TabConfigs.ContentWidgetClass
 * ContainerScreen only handles: overlay (tooltips, context menu), input mode, EventBus
 *
 * ARCHITECTURE (like legacy CharacterScreen):
 * - PanelSwitcher = UpperTabBar (manages tabs + content)
 * - OverlayLayer = tooltips, context menus, drag ghost
 * - Screen activation/deactivation via EventBus
 *
 * USAGE:
 * 1. In Blueprint, bind PanelSwitcher (configure TabConfigs there)
 * 2. Configure ScreenTag and DefaultPanelTag
 * 3. Call ActivateScreen() to show, DeactivateScreen() to hide
 *
 * @see USuspenseCharacterScreen (legacy)
 * @see USuspenseCorePanelSwitcherWidget
 */
UCLASS(BlueprintType, Blueprintable)
class UISYSTEM_API USuspenseCoreContainerScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreContainerScreenWidget(const FObjectInitializer& ObjectInitializer);

	//==================================================================
	// UUserWidget Interface
	//==================================================================

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	//==================================================================
	// Screen Activation (from legacy CharacterScreen)
	//==================================================================

	/**
	 * Activate the container screen
	 * Opens default or last remembered panel, sets input mode, publishes events
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Screen")
	void ActivateScreen();

	/**
	 * Deactivate the container screen
	 * Remembers current panel, restores input mode, publishes events
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Screen")
	void DeactivateScreen();

	/**
	 * Is screen currently active
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Screen")
	bool IsScreenActive() const { return bIsActive; }

	/**
	 * Get screen tag (unique identifier)
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Screen")
	FGameplayTag GetScreenTag() const { return ScreenTag; }

	//==================================================================
	// Panel Management (delegates to PanelSwitcher)
	//==================================================================

	/**
	 * Open panel by tag (delegates to PanelSwitcher->SelectTabByTag)
	 * @param PanelTag Tag of the panel to open
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Screen")
	void OpenPanelByTag(const FGameplayTag& PanelTag);

	/**
	 * Open panel by index (delegates to PanelSwitcher->SelectTabByIndex)
	 * @param PanelIndex Index of the panel to open
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Screen")
	void OpenPanelByIndex(int32 PanelIndex);

	/**
	 * Get the panel switcher widget (like legacy GetTabBar)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Screen")
	USuspenseCorePanelSwitcherWidget* GetPanelSwitcher() const { return PanelSwitcher; }

	/**
	 * Get current active panel tag
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Screen")
	FGameplayTag GetActivePanelTag() const { return ActivePanelTag; }

	/**
	 * Get current active panel content widget
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Screen")
	UUserWidget* GetActivePanelContent() const;

	/**
	 * Refresh content of the currently active panel
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Screen")
	void RefreshScreenContent();

	//==================================================================
	// Tooltip Management
	//==================================================================

	/**
	 * Show tooltip for item
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Screen")
	void ShowTooltip(const struct FSuspenseCoreItemUIData& ItemData, const FVector2D& ScreenPosition);

	/**
	 * Hide current tooltip
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Screen")
	void HideTooltip();

	/**
	 * Is tooltip currently visible
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Screen")
	bool IsTooltipVisible() const;

	//==================================================================
	// Context Menu Management
	//==================================================================

	/**
	 * Show context menu for item
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Screen")
	void ShowContextMenu(
		const struct FSuspenseCoreItemUIData& ItemData,
		const FGuid& ContainerID,
		int32 SlotIndex,
		const FVector2D& ScreenPosition,
		const TArray<FGameplayTag>& AvailableActions);

	/**
	 * Hide current context menu
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Screen")
	void HideContextMenu();

	/**
	 * Is context menu currently visible
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Screen")
	bool IsContextMenuVisible() const;

	//==================================================================
	// Drag-Drop Visual
	//==================================================================

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Screen")
	void ShowDragGhost(const struct FSuspenseCoreItemUIData& ItemData, const FVector2D& DragOffset);

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Screen")
	void UpdateDragGhostPosition(const FVector2D& ScreenPosition);

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Screen")
	void HideDragGhost();

	//==================================================================
	// Screen Actions
	//==================================================================

	/**
	 * Close the container screen (calls DeactivateScreen + RemoveFromParent)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Screen")
	void CloseScreen();

	//==================================================================
	// Blueprint Events
	//==================================================================

	/** Called when panel is switched */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Screen", meta = (DisplayName = "On Panel Switched"))
	void K2_OnPanelSwitched(const FGameplayTag& NewPanelTag);

	/** Called when screen is about to close */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Screen", meta = (DisplayName = "On Screen Closing"))
	void K2_OnScreenClosing();

	/** Called when screen is activated */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Screen", meta = (DisplayName = "On Container Screen Opened"))
	void K2_OnContainerScreenOpened();

	/** Called when screen is deactivated */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Screen", meta = (DisplayName = "On Container Screen Closed"))
	void K2_OnContainerScreenClosed();

protected:
	//==================================================================
	// Setup Methods
	//==================================================================

	/** Bind to EventBus events */
	void BindToUIManager();

	/** Unbind from EventBus events */
	void UnbindFromUIManager();

	//==================================================================
	// Widget References (Bind in Blueprint)
	//==================================================================

	/** Panel switcher widget - manages tabs AND content (like legacy UpperTabBar) */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Widgets")
	TObjectPtr<USuspenseCorePanelSwitcherWidget> PanelSwitcher;

	/** Overlay for tooltips and context menus */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UOverlay> OverlayLayer;

	/** Tooltip widget (created on demand) */
	UPROPERTY(BlueprintReadWrite, Category = "Widgets")
	TObjectPtr<USuspenseCoreTooltipWidget> ItemTooltipWidget;

	/** Context menu widget (created on demand) */
	UPROPERTY(BlueprintReadWrite, Category = "Widgets")
	TObjectPtr<USuspenseCoreContextMenuWidget> ContextMenuWidget;

	/** Drag ghost widget (created on demand) */
	UPROPERTY(BlueprintReadWrite, Category = "Widgets")
	TObjectPtr<UUserWidget> DragGhostWidget;

	//==================================================================
	// Configuration
	//==================================================================

	/** Widget class to use for tooltip */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
	TSubclassOf<USuspenseCoreTooltipWidget> ItemTooltipWidgetClass;

	/** Widget class to use for context menu */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
	TSubclassOf<USuspenseCoreContextMenuWidget> ContextMenuWidgetClass;

	/** Widget class to use for drag ghost */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
	TSubclassOf<UUserWidget> DragGhostWidgetClass;

	/** Key to close screen */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
	FKey CloseKey;

	//==================================================================
	// Screen State Configuration (from legacy CharacterScreen)
	//==================================================================

	/** Screen identifier tag (e.g., SuspenseCore.UI.Screen.Container) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screen")
	FGameplayTag ScreenTag;

	/** Default panel to open when screen activates */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screen")
	FGameplayTag DefaultPanelTag;

	/** Whether to remember last opened panel */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screen")
	bool bRememberLastPanel = true;

	/** Whether screen is currently active */
	UPROPERTY(BlueprintReadOnly, Category = "Screen")
	bool bIsActive = false;

	/** Last opened panel tag (for remember feature) */
	UPROPERTY(BlueprintReadOnly, Category = "Screen")
	FGameplayTag LastOpenedPanel;

private:
	//==================================================================
	// State
	//==================================================================

	/** Currently active panel tag */
	FGameplayTag ActivePanelTag;

	/** Cached UIManager reference */
	UPROPERTY(Transient)
	TWeakObjectPtr<USuspenseCoreUIManager> CachedUIManager;

	/** Cached EventBus reference */
	UPROPERTY(Transient)
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	/** EventBus subscription handle for panel selection */
	FSuspenseCoreSubscriptionHandle PanelSelectedEventHandle;

	/** Drag ghost offset from cursor */
	FVector2D DragGhostOffset;

	/** Handle panel selected event from EventBus */
	void OnPanelSelectedEvent(FGameplayTag EventTag, const struct FSuspenseCoreEventData& EventData);

	/** Update input mode based on screen state */
	void UpdateInputMode();

	/** Publish screen event via EventBus */
	void PublishScreenEvent(const FGameplayTag& EventTag);

	/** Refresh timer handle */
	FTimerHandle RefreshTimerHandle;
};
