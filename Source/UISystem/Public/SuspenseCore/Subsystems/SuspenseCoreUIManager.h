// SuspenseCoreUIManager.h
// SuspenseCore - UI Manager Subsystem
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUITypes.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUIContainerTypes.h"
#include "SuspenseCoreUIManager.generated.h"

// Forward declarations
class ISuspenseCoreUIDataProvider;
class USuspenseCoreEventBus;
class USuspenseCoreContainerScreenWidget;
class USuspenseCoreInventoryWidget;
class USuspenseCoreTooltipWidget;
class USuspenseCoreMagazineInspectionWidget;
class USuspenseCoreMasterHUDWidget;
class APlayerController;
struct FSuspenseCoreEventData;
struct FSuspenseCoreMagazineInspectionData;

/**
 * Delegate for container screen visibility changes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnContainerScreenVisibilityChanged, bool, bIsVisible);

/**
 * Delegate for notification events
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUINotification, const FSuspenseCoreUINotification&, Notification);

/**
 * USuspenseCoreUIManager
 *
 * Central manager for SuspenseCore UI system.
 * GameInstanceSubsystem that handles:
 * - Container screen management (show/hide inventory, equipment, etc.)
 * - Provider discovery and registration
 * - UI notifications and feedback
 * - EventBus integration for UI events
 *
 * ARCHITECTURE:
 * - UISystem depends ONLY on BridgeSystem
 * - Finds providers at runtime via interfaces
 * - All communication through EventBus
 * - Configurable widget classes
 *
 * USAGE:
 * ```cpp
 * // Get manager
 * USuspenseCoreUIManager* UIManager = USuspenseCoreUIManager::Get(WorldContext);
 *
 * // Show inventory screen
 * UIManager->ShowContainerScreen(PC, TAG_Panel_Inventory);
 *
 * // Find provider on actor
 * auto Provider = UIManager->FindProviderOnActor(Actor, ESuspenseCoreContainerType::Inventory);
 * ```
 *
 * @see ISuspenseCoreUIDataProvider
 * @see USuspenseCoreContainerScreenWidget
 */
UCLASS()
class UISYSTEM_API USuspenseCoreUIManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//==================================================================
	// Static Access
	//==================================================================

	/**
	 * Get UI Manager from world context
	 * @param WorldContext Object with world context
	 * @return UI Manager or nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI", meta = (WorldContext = "WorldContext"))
	static USuspenseCoreUIManager* Get(const UObject* WorldContext);

	//==================================================================
	// Lifecycle
	//==================================================================

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	//==================================================================
	// Container Screen Management
	//==================================================================

	/**
	 * Show container screen with specified panels
	 * @param PC Owning player controller
	 * @param PanelTag Panel to show (SuspenseCore.UI.Panel.*)
	 * @return true if screen shown
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	bool ShowContainerScreen(APlayerController* PC, const FGameplayTag& PanelTag);

	/**
	 * Show container screen with multiple panels
	 * @param PC Owning player controller
	 * @param PanelTags Panels to enable
	 * @param DefaultPanel Panel to show by default
	 * @return true if screen shown
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	bool ShowContainerScreenMulti(
		APlayerController* PC,
		const TArray<FGameplayTag>& PanelTags,
		const FGameplayTag& DefaultPanel);

	/**
	 * Hide container screen
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	void HideContainerScreen();

	/**
	 * Close container screen for player (called from widget)
	 * @param PC Player controller
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	void CloseContainerScreen(APlayerController* PC);

	/**
	 * Toggle container screen
	 * @param PC Owning player controller
	 * @param PanelTag Panel to toggle
	 * @return New visibility state
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	bool ToggleContainerScreen(APlayerController* PC, const FGameplayTag& PanelTag);

	/**
	 * Check if container screen is visible
	 * @return true if visible
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI")
	bool IsContainerScreenVisible() const;

	/**
	 * Get current container screen widget
	 * @return Container screen or nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI")
	USuspenseCoreContainerScreenWidget* GetContainerScreen() const { return ContainerScreen; }

	//==================================================================
	// Provider Discovery
	//==================================================================

	/**
	 * Find UI data provider on actor by type
	 * @param Actor Actor to search
	 * @param ContainerType Type to find
	 * @return Provider or nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	TScriptInterface<ISuspenseCoreUIDataProvider> FindProviderOnActor(
		AActor* Actor,
		ESuspenseCoreContainerType ContainerType);

	/**
	 * Find all providers on actor
	 * @param Actor Actor to search
	 * @return Array of providers
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	TArray<TScriptInterface<ISuspenseCoreUIDataProvider>> FindAllProvidersOnActor(AActor* Actor);

	/**
	 * Find provider by ID
	 * @param ProviderID Provider GUID
	 * @return Provider or nullptr
	 */
	TScriptInterface<ISuspenseCoreUIDataProvider> FindProviderByID(const FGuid& ProviderID);

	/**
	 * Get player's inventory provider
	 * Convenience method to get inventory from local player's PlayerState
	 * @param PC Player controller
	 * @return Inventory provider or nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	TScriptInterface<ISuspenseCoreUIDataProvider> GetPlayerInventoryProvider(APlayerController* PC);

	/**
	 * Get player's equipment provider
	 * Convenience method to get equipment from local player's Pawn
	 * @param PC Player controller
	 * @return Equipment provider or nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	TScriptInterface<ISuspenseCoreUIDataProvider> GetPlayerEquipmentProvider(APlayerController* PC);

	//==================================================================
	// Notifications
	//==================================================================

	/**
	 * Show UI notification
	 * @param Notification Notification data
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	void ShowNotification(const FSuspenseCoreUINotification& Notification);

	/**
	 * Show simple notification
	 * @param Type Notification type
	 * @param Message Message text
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	void ShowSimpleNotification(ESuspenseCoreUIFeedbackType Type, const FText& Message);

	/**
	 * Show item pickup notification
	 * @param Item Item data
	 * @param Quantity Amount picked up
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	void ShowItemPickupNotification(const FSuspenseCoreItemUIData& Item, int32 Quantity);

	//==================================================================
	// Tooltip Management
	//==================================================================

	/**
	 * Show tooltip for item
	 * @param Item Item to show tooltip for
	 * @param ScreenPosition Screen position
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	void ShowItemTooltip(const FSuspenseCoreItemUIData& Item, const FVector2D& ScreenPosition);

	/**
	 * Hide current tooltip
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	void HideTooltip();

	/**
	 * Check if tooltip is visible
	 * @return true if tooltip showing
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI")
	bool IsTooltipVisible() const;

	//==================================================================
	// Magazine Inspection (Tarkov-style)
	//==================================================================

	/**
	 * Open magazine inspection widget for detailed magazine view
	 * Shows per-round visualization with loading/unloading support
	 * @param InspectionData Magazine inspection data
	 * @return true if inspection opened
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Magazine")
	bool OpenMagazineInspection(const FSuspenseCoreMagazineInspectionData& InspectionData);

	/**
	 * Close magazine inspection widget
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Magazine")
	void CloseMagazineInspection();

	/**
	 * Check if magazine inspection is currently open
	 * @return true if inspection is open
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Magazine")
	bool IsMagazineInspectionOpen() const;

	/**
	 * Get magazine inspection widget
	 * @return Magazine inspection widget or nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Magazine")
	USuspenseCoreMagazineInspectionWidget* GetMagazineInspectionWidget() const { return MagazineInspectionWidget; }

	/**
	 * Check if item is a magazine by its tags
	 * @param ItemData Item to check
	 * @return true if item has Magazine tag
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Magazine")
	bool IsMagazineItem(const FSuspenseCoreItemUIData& ItemData) const;

	//==================================================================
	// Drag-Drop Support
	//==================================================================

	/**
	 * Start drag operation
	 * @param DragData Drag data
	 * @return true if drag started
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	bool StartDragOperation(const FSuspenseCoreDragData& DragData);

	/**
	 * Cancel current drag operation
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI")
	void CancelDragOperation();

	/**
	 * Check if drag is in progress
	 * @return true if dragging
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI")
	bool IsDragging() const { return CurrentDragData.bIsValid; }

	/**
	 * Get current drag data
	 * @return Current drag data
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI")
	const FSuspenseCoreDragData& GetCurrentDragData() const { return CurrentDragData; }

	//==================================================================
	// Configuration
	//==================================================================

	/** Container screen widget class - set in Blueprint or Data Asset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|UI|Config")
	TSubclassOf<USuspenseCoreContainerScreenWidget> ContainerScreenClass;

	/** Tooltip widget class - standard item tooltip */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|UI|Config")
	TSubclassOf<USuspenseCoreTooltipWidget> TooltipWidgetClass;

	/**
	 * Magazine inspection widget class - Tarkov-style detailed magazine view
	 * Shows per-round slot visualization with loading/unloading
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|UI|Config")
	TSubclassOf<USuspenseCoreMagazineInspectionWidget> MagazineInspectionWidgetClass;

	/** Master HUD widget class - contains all in-game HUD elements */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|UI|Config")
	TSubclassOf<USuspenseCoreMasterHUDWidget> MasterHUDWidgetClass;

	//==================================================================
	// Master HUD Management
	//==================================================================

	/**
	 * Create and show master HUD for player
	 * @param PC Player controller
	 * @return Created HUD widget or nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|HUD")
	USuspenseCoreMasterHUDWidget* CreateMasterHUD(APlayerController* PC);

	/**
	 * Get current master HUD widget
	 * @return Master HUD or nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|HUD")
	USuspenseCoreMasterHUDWidget* GetMasterHUD() const { return MasterHUD.Get(); }

	/**
	 * Destroy master HUD
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|HUD")
	void DestroyMasterHUD();

	/**
	 * Initialize weapon HUD with equipped weapon
	 * Convenience method that calls MasterHUD->InitializeWeaponHUD
	 * @param WeaponActor Equipped weapon
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|HUD")
	void InitializeWeaponHUD(AActor* WeaponActor);

	/**
	 * Clear weapon HUD
	 * Convenience method that calls MasterHUD->ClearWeaponHUD
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|HUD")
	void ClearWeaponHUD();

	/**
	 * Get screen configuration
	 * @return Current screen config
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Config")
	const FSuspenseCoreScreenConfig& GetScreenConfig() const { return ScreenConfig; }

	/**
	 * Set screen configuration
	 * @param NewConfig New configuration
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Config")
	void SetScreenConfig(const FSuspenseCoreScreenConfig& NewConfig);

	/**
	 * Configure widget classes manually from Blueprint
	 * Call this from GameMode BeginPlay or HUD Blueprint if auto-detection fails
	 * @param InMagazineInspectionClass Widget class for magazine inspection panel
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Config")
	void ConfigureWidgetClasses(TSubclassOf<USuspenseCoreMagazineInspectionWidget> InMagazineInspectionClass);

	//==================================================================
	// Events
	//==================================================================

	/** Broadcast when container screen visibility changes */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|UI|Events")
	FOnContainerScreenVisibilityChanged OnContainerScreenVisibilityChanged;

	/** Broadcast when notification should be shown */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|UI|Events")
	FOnUINotification OnUINotification;

protected:
	//==================================================================
	// Initialization Helpers
	//==================================================================

	/** Load default widget classes from common Blueprint paths */
	void LoadDefaultWidgetClasses();

	//==================================================================
	// EventBus Integration
	//==================================================================

	/** Subscribe to EventBus events */
	void SubscribeToEvents();

	/** Unsubscribe from EventBus events */
	void UnsubscribeFromEvents();

	/** Handle UI feedback event */
	void OnUIFeedbackEvent(const FSuspenseCoreEventData& EventData);

	/** Handle container opened event */
	void OnContainerOpenedEvent(const FSuspenseCoreEventData& EventData);

	/** Handle container closed event */
	void OnContainerClosedEvent(const FSuspenseCoreEventData& EventData);

	/** Handle item equipped event - auto-initialize weapon HUD */
	void OnItemEquippedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Handle item unequipped event - auto-clear weapon HUD */
	void OnItemUnequippedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Handle visual detached event - clear weapon HUD when weapon actor is hidden */
	void OnVisualDetachedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Get EventBus */
	USuspenseCoreEventBus* GetEventBus() const;

	//==================================================================
	// Internal
	//==================================================================

	/** Create container screen widget */
	USuspenseCoreContainerScreenWidget* CreateContainerScreen(APlayerController* PC);

	/** Create tooltip widget */
	USuspenseCoreTooltipWidget* CreateTooltipWidget(APlayerController* PC);

	/** Create magazine inspection widget */
	USuspenseCoreMagazineInspectionWidget* CreateMagazineInspectionWidget(APlayerController* PC);

	/** Update input mode for container screen */
	void UpdateInputMode(APlayerController* PC, bool bShowingUI);

	/** Setup default screen configuration */
	void SetupDefaultScreenConfig();

	/** Bind providers to container screen panels */
	void BindProvidersToScreen(APlayerController* PC);

private:
	//==================================================================
	// Widget Instances
	//==================================================================

	/** Current container screen widget */
	UPROPERTY(Transient)
	USuspenseCoreContainerScreenWidget* ContainerScreen;

	/** Current tooltip widget (standard) */
	UPROPERTY(Transient)
	USuspenseCoreTooltipWidget* TooltipWidget;

	/** Current magazine inspection widget (Tarkov-style) */
	UPROPERTY(Transient)
	USuspenseCoreMagazineInspectionWidget* MagazineInspectionWidget;

	/** Current master HUD widget - use TWeakObjectPtr to auto-invalidate when widget is destroyed (e.g. level transition) */
	UPROPERTY(Transient)
	TWeakObjectPtr<USuspenseCoreMasterHUDWidget> MasterHUD;

	/** Owning player controller for current screen */
	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerController> OwningPC;

	//==================================================================
	// Configuration
	//==================================================================

	/** Screen configuration */
	UPROPERTY()
	FSuspenseCoreScreenConfig ScreenConfig;

	//==================================================================
	// State
	//==================================================================

	/** Current drag data */
	FSuspenseCoreDragData CurrentDragData;

	/** Registered provider IDs for lookup */
	TMap<FGuid, TWeakObjectPtr<UObject>> RegisteredProviders;

	/** Cached EventBus reference */
	UPROPERTY(Transient)
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	/** Event subscription handles */
	TArray<FDelegateHandle> EventSubscriptions;

	/** Item equipped event handle */
	FSuspenseCoreSubscriptionHandle ItemEquippedHandle;

	/** Item unequipped event handle */
	FSuspenseCoreSubscriptionHandle ItemUnequippedHandle;

	/** Visual detached event handle (for weapon unequip) */
	FSuspenseCoreSubscriptionHandle VisualDetachedHandle;

	/** Is container screen currently visible */
	bool bIsContainerScreenVisible;
};
