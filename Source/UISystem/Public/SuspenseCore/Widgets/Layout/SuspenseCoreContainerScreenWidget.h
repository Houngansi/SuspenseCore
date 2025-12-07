// SuspenseCoreContainerScreenWidget.h
// SuspenseCore - Main Container Screen Widget
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUIContainerTypes.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCoreContainerScreenWidget.generated.h"

// Forward declarations
class USuspenseCorePanelWidget;
class USuspenseCorePanelSwitcherWidget;
class USuspenseCoreTooltipWidget;
class USuspenseCoreContextMenuWidget;
class USuspenseCoreUIManager;
class UWidgetSwitcher;
class UOverlay;

/**
 * USuspenseCoreContainerScreenWidget
 *
 * Main screen widget for container UI (Inventory, Equipment, Stash, Trader, etc.)
 * Manages panel switching, tooltips, context menus, and drag-drop visualization.
 *
 * LAYOUT STRUCTURE:
 * - Header: Title, close button
 * - Panel Switcher: Tab bar for switching panels
 * - Panel Container: Widget switcher containing panels
 * - Footer: Weight/slots info, action buttons
 * - Overlay: Tooltips, context menus, drag ghost
 *
 * PANELS:
 * - Each panel can contain multiple container widgets
 * - Panels are configured via FSuspenseCorePanelConfig
 * - Common presets: Inventory, Inventory+Equipment, Inventory+Stash, etc.
 *
 * @see USuspenseCorePanelWidget
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
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	//==================================================================
	// Screen Configuration
	//==================================================================

	/**
	 * Initialize screen with configuration
	 * @param InScreenConfig Configuration defining available panels
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Screen")
	void InitializeScreen(const FSuspenseCoreScreenConfig& InScreenConfig);

	/**
	 * Get current screen configuration
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Screen")
	const FSuspenseCoreScreenConfig& GetScreenConfig() const { return ScreenConfig; }

	//==================================================================
	// Panel Management
	//==================================================================

	/**
	 * Switch to panel by tag
	 * @param PanelTag Gameplay tag identifying the panel
	 * @return True if panel was switched successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Screen")
	bool SwitchToPanel(const FGameplayTag& PanelTag);

	/**
	 * Switch to panel by index
	 * @param PanelIndex Index in the panels array
	 * @return True if panel was switched successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Screen")
	bool SwitchToPanelByIndex(int32 PanelIndex);

	/**
	 * Get current active panel
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Screen")
	USuspenseCorePanelWidget* GetActivePanel() const;

	/**
	 * Get current active panel tag
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Screen")
	FGameplayTag GetActivePanelTag() const { return ActivePanelTag; }

	/**
	 * Get panel by tag
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Screen")
	USuspenseCorePanelWidget* GetPanelByTag(const FGameplayTag& PanelTag) const;

	/**
	 * Get all panels
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Screen")
	TArray<USuspenseCorePanelWidget*> GetAllPanels() const { return Panels; }

	//==================================================================
	// Tooltip Management
	//==================================================================

	/**
	 * Show tooltip for item
	 * @param ItemData Item data to display
	 * @param ScreenPosition Position to show tooltip
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Screen")
	void ShowTooltip(const FSuspenseCoreItemUIData& ItemData, const FVector2D& ScreenPosition);

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
	 * @param ItemData Item data
	 * @param ContainerID Container containing the item
	 * @param SlotIndex Slot index
	 * @param ScreenPosition Position to show menu
	 * @param AvailableActions Actions to display
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Screen")
	void ShowContextMenu(
		const FSuspenseCoreItemUIData& ItemData,
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

	/**
	 * Show drag ghost for item
	 * @param ItemData Item being dragged
	 * @param DragOffset Offset from cursor
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Screen")
	void ShowDragGhost(const FSuspenseCoreItemUIData& ItemData, const FVector2D& DragOffset);

	/**
	 * Update drag ghost position
	 * @param ScreenPosition Current cursor position
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Screen")
	void UpdateDragGhostPosition(const FVector2D& ScreenPosition);

	/**
	 * Hide drag ghost
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Screen")
	void HideDragGhost();

	//==================================================================
	// Screen Actions
	//==================================================================

	/**
	 * Close the container screen
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Screen")
	void CloseScreen();

	/**
	 * Handle close button clicked
	 */
	UFUNCTION()
	void OnCloseButtonClicked();

	//==================================================================
	// Blueprint Events
	//==================================================================

	/** Called when screen is initialized */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Screen", meta = (DisplayName = "On Screen Initialized"))
	void K2_OnScreenInitialized();

	/** Called when panel is switched */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Screen", meta = (DisplayName = "On Panel Switched"))
	void K2_OnPanelSwitched(const FGameplayTag& NewPanelTag);

	/** Called when screen is about to close */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Screen", meta = (DisplayName = "On Screen Closing"))
	void K2_OnScreenClosing();

protected:
	//==================================================================
	// Setup Methods
	//==================================================================

	/** Create panel widgets from config */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|UI|Screen")
	void CreatePanels();
	virtual void CreatePanels_Implementation();

	/** Setup panel switcher tabs */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|UI|Screen")
	void SetupPanelSwitcher();
	virtual void SetupPanelSwitcher_Implementation();

	/** Bind to UIManager events */
	void BindToUIManager();

	/** Unbind from UIManager events */
	void UnbindFromUIManager();

	/** Create default layout structure if widgets are not bound in Blueprint */
	void CreateDefaultLayoutIfNeeded();

	//==================================================================
	// Widget References (Bind in Blueprint)
	//==================================================================

	/** Panel switcher widget (tab bar) */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<USuspenseCorePanelSwitcherWidget> PanelSwitcher;

	/** Widget switcher containing panels */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UWidgetSwitcher> PanelContainer;

	/** Overlay for tooltips and context menus */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UOverlay> OverlayLayer;

	/** Tooltip widget */
	UPROPERTY(BlueprintReadWrite, Category = "Widgets")
	TObjectPtr<USuspenseCoreTooltipWidget> ItemTooltipWidget;

	/** Context menu widget */
	UPROPERTY(BlueprintReadWrite, Category = "Widgets")
	TObjectPtr<USuspenseCoreContextMenuWidget> ContextMenuWidget;

	/** Drag ghost widget */
	UPROPERTY(BlueprintReadWrite, Category = "Widgets")
	TObjectPtr<UUserWidget> DragGhostWidget;

	//==================================================================
	// Configuration
	//==================================================================

	/** Widget class to use for panels */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
	TSubclassOf<USuspenseCorePanelWidget> PanelWidgetClass;

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

private:
	//==================================================================
	// State
	//==================================================================

	/** Current screen configuration */
	FSuspenseCoreScreenConfig ScreenConfig;

	/** Created panel widgets */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USuspenseCorePanelWidget>> Panels;

	/** Map of panel tags to panel widgets */
	TMap<FGameplayTag, TObjectPtr<USuspenseCorePanelWidget>> PanelsByTag;

	/** Currently active panel tag */
	FGameplayTag ActivePanelTag;

	/** Cached UIManager reference */
	UPROPERTY(Transient)
	TWeakObjectPtr<USuspenseCoreUIManager> CachedUIManager;

	/** Cached EventBus reference */
	UPROPERTY(Transient)
	TWeakObjectPtr<class USuspenseCoreEventBus> CachedEventBus;

	/** EventBus subscription handle for panel selection */
	FSuspenseCoreSubscriptionHandle PanelSelectedEventHandle;

	/** Drag ghost offset from cursor */
	FVector2D DragGhostOffset;

	/** Handle panel selected event from EventBus */
	void OnPanelSelectedEvent(const FGameplayTag& EventTag, const struct FSuspenseCoreEventData& EventData);
};
