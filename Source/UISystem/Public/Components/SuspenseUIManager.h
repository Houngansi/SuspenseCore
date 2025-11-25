// Copyright Suspense Team Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "Types/UI/ContainerUITypes.h"
#include "SuspenseUIManager.generated.h"

// Forward declarations
class UUserWidget;
class USuspenseBaseWidget;
class USuspenseBaseLayoutWidget;
class UEventDelegateManager;
class USuspenseUpperTabBar;
class USuspenseInventoryUIBridge;
class USuspenseEquipmentUIBridge;
struct FSuspenseUnifiedItemData;

/**
 * Widget configuration structure
 * Contains all necessary data for widget lifecycle management
 */
USTRUCT(BlueprintType)
struct FSuspenseWidgetInfo
{
    GENERATED_BODY()

    // Widget class to create
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TSubclassOf<UUserWidget> WidgetClass;

    // Unique tag for identification
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FGameplayTag WidgetTag;

    // Z-order for display (higher = on top)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    int32 ZOrder = 0;

    // Should widget be created automatically
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    bool bAutoCreate = false;

    // Should widget persist between levels
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    bool bPersistent = false;
    
    // Should widget be added to viewport immediately when created
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    bool bAutoAddToViewport = true;

    FSuspenseWidgetInfo()
    {
        ZOrder = 0;
        bAutoCreate = false;
        bPersistent = false;
        bAutoAddToViewport = true;
    }
};

/**
 * Central UI management system for all widgets in the game
 * 
 * ARCHITECTURE:
 * - UIManager creates ONLY root widgets (HUD, CharacterScreen, Dialogs, Menus)
 * - All child widgets (Inventory, Equipment, etc.) are created through Layout system
 * - Layout widgets manage their own children and report to UIManager for registration
 * - Bridges always connect to widgets through Layout system, never directly
 * 
 * KEY FEATURES:
 * - Unified widget registry for both root and layout-created widgets
 * - Automatic registration of layout-created widgets through event system
 * - Smart widget search that looks in both direct registry and inside layouts
 * - On-demand bridge initialization when tabs are opened
 * - No widget duplication - each widget created exactly once
 * 
 * USAGE:
 * - Use CreateWidget() ONLY for root widgets
 * - Use ShowCharacterScreen() for complex UI with tabs
 * - Use FindWidgetInLayouts() to find any widget regardless of creation method
 * - Bridges are initialized automatically when their tabs are opened
 */
UCLASS()
class UISYSTEM_API USuspenseUIManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    USuspenseUIManager();
    
    // Lifecycle
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /**
     * Get singleton instance
     * @param WorldContext Any object with valid world context
     * @return Manager instance or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Manager", meta = (CallInEditor = "true"))
    static USuspenseUIManager* Get(const UObject* WorldContext);

    //================================================
    // Core Widget Management
    //================================================

    /**
     * Creates and registers ROOT widget only
     * DO NOT use this for child widgets - they must be created through Layout system
     * 
     * @param WidgetClass Widget class to create
     * @param WidgetTag Unique tag for identification (must be a root widget tag)
     * @param OwningObject Object that owns the widget (usually PlayerController)
     * @param bForceAddToViewport Override configuration and add to viewport immediately
     * @return Created widget or nullptr on error
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Manager")
    UUserWidget* CreateWidget(
        TSubclassOf<UUserWidget> WidgetClass, 
        FGameplayTag WidgetTag, 
        UObject* OwningObject,
        bool bForceAddToViewport = false);

    /**
     * Shows widget by tag (searches in both root registry and layouts)
     * @param WidgetTag Widget tag
     * @param bAddToViewport Add to viewport if not already added (only for root widgets)
     * @return Success of operation
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Manager")
    bool ShowWidget(FGameplayTag WidgetTag, bool bAddToViewport = true);

    /**
     * Hides widget by tag
     * @param WidgetTag Widget tag
     * @param bRemoveFromParent Remove from viewport
     * @return Success of operation
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Manager")
    bool HideWidget(FGameplayTag WidgetTag, bool bRemoveFromParent = false);

    /**
     * Destroys widget by tag
     * @param WidgetTag Widget tag
     * @return Success of operation
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Manager")
    bool DestroyWidget(FGameplayTag WidgetTag);

    /**
     * Gets widget by tag (only searches root registry, use FindWidgetInLayouts for full search)
     * @param WidgetTag Widget tag
     * @return Found widget or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Manager")
    UUserWidget* GetWidget(FGameplayTag WidgetTag) const;

    /**
     * Checks if widget exists
     * @param WidgetTag Widget tag
     * @return true if widget exists
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Manager")
    bool WidgetExists(FGameplayTag WidgetTag) const;

    //================================================
    // Layout Widget Support
    //================================================

    /**
     * Register a widget created inside a layout
     * Called automatically by layout system through events
     * @param Widget Widget to register
     * @param WidgetTag Tag for the widget
     * @param ParentLayout Parent layout widget
     * @return Success of registration
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Manager")
    bool RegisterLayoutWidget(UUserWidget* Widget, FGameplayTag WidgetTag, UUserWidget* ParentLayout = nullptr);

    /**
     * Unregister a layout widget
     * @param WidgetTag Widget tag
     * @return Success of unregistration
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Manager")
    bool UnregisterLayoutWidget(FGameplayTag WidgetTag);

    /**
     * Find widget in all registered layouts (comprehensive search)
     * This is the preferred method to find any widget
     * @param WidgetTag Widget tag to search for
     * @return Found widget or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Manager")
    UUserWidget* FindWidgetInLayouts(FGameplayTag WidgetTag) const;

    /**
     * Get widget from specific layout
     * @param LayoutWidget Layout to search in
     * @param WidgetTag Widget tag to find
     * @return Found widget or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Manager")
    UUserWidget* GetWidgetFromLayout(USuspenseBaseLayoutWidget* LayoutWidget, FGameplayTag WidgetTag) const;

    /**
     * Get all registered widget tags
     * @return Array of all widget tags
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Manager")
    TArray<FGameplayTag> GetAllWidgetTags() const;

    //================================================
    // Character Screen Management
    //================================================
    
    /**
     * Show character screen with specified tab
     * Handles bridge initialization for the selected tab
     * @param OwningObject Owner object (usually PlayerController)
     * @param TabTag Tab tag to open (e.g., UI.Tab.Inventory)
     * @return Created character screen widget
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Manager")
    UUserWidget* ShowCharacterScreen(UObject* OwningObject, FGameplayTag TabTag);
    
    /**
     * Hide character screen
     * @return Success of operation
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Manager")
    bool HideCharacterScreen();
    
    /**
     * Check if character screen is visible
     * @return true if screen is shown
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Manager")
    bool IsCharacterScreenVisible() const;

    //================================================
    // Batch Operations
    //================================================

    /**
     * Creates all widgets with AutoCreate flag (root widgets only)
     * @param OwningObject Object that owns the widgets
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Manager")
    void CreateAutoCreateWidgets(UObject* OwningObject);

    /**
     * Destroys all non-persistent widgets
     * Used when changing levels
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Manager")
    void DestroyNonPersistentWidgets();

    /**
     * Destroys all widgets
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Manager")
    void DestroyAllWidgets();

    //================================================
    // HUD Management
    //================================================

    /**
     * Initializes main HUD widget
     * @param OwningObject Object that owns the HUD
     * @return Created HUD widget
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Manager")
    UUserWidget* InitializeMainHUD(UObject* OwningObject);

    /**
     * Notifies HUD to update from owner
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Manager")
    void RequestHUDUpdate();

    //================================================
    // Widget Registration
    //================================================

    /**
     * Registers externally created widget (use only for special cases)
     * @param Widget Widget to register
     * @param WidgetTag Tag for the widget
     * @return Success of registration
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Manager")
    bool RegisterExternalWidget(UUserWidget* Widget, FGameplayTag WidgetTag);

    /**
     * Unregisters widget without destroying it
     * @param WidgetTag Widget tag
     * @return Unregistered widget or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Manager")
    UUserWidget* UnregisterWidget(FGameplayTag WidgetTag);

    //================================================
    // Bridge Management
    //================================================

    /**
     * Create and initialize inventory bridge (legacy method)
     * Prefer automatic initialization through ShowCharacterScreen
     * @param PlayerController Player controller for initialization
     * @return Created bridge or nullptr on error
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Manager")
    USuspenseInventoryUIBridge* CreateInventoryUIBridge(APlayerController* PlayerController);

    /**
     * Get inventory UI bridge
     * @return Inventory bridge or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Manager")
    USuspenseInventoryUIBridge* GetInventoryUIBridge() const;

    /**
     * Get equipment UI bridge
     * @return Equipment UI bridge or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Equipment")
    USuspenseEquipmentUIBridge* GetEquipmentUIBridge() const;

    /**
     * Create and initialize equipment UI bridge (legacy method)
     * Prefer automatic initialization through ShowCharacterScreen
     * @param PlayerController Player controller to associate with
     * @return Created bridge or nullptr on failure
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Equipment")
    USuspenseEquipmentUIBridge* CreateEquipmentUIBridge(APlayerController* PlayerController);

    /**
     * Convert unified item data to UI data for display
     * @param UnifiedData Data from DataTable
     * @param Quantity Item quantity
     * @return UI data for display
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Utility")
    FItemUIData ConvertUnifiedItemDataToUI(const FSuspenseUnifiedItemData& UnifiedData, int32 Quantity = 1) const;

    /**
     * Shows a notification message to the player
     * @param Message Text to display
     * @param Duration How long to show the notification (in seconds)
     * @param Color Color of the notification (optional, used for importance)
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Notifications")
    void ShowNotification(const FText& Message, float Duration = 3.0f, FLinearColor Color = FLinearColor::White);

    /**
     * Shows a notification with icon
     * @param Message Text to display
     * @param Icon Icon to show with notification
     * @param Duration How long to show the notification
     * @param Color Color tint for the notification
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Notifications")
    void ShowNotificationWithIcon(const FText& Message, UTexture2D* Icon, float Duration = 3.0f, FLinearColor Color = FLinearColor::White);

    /**
     * Clears all active notifications
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Notifications")
    void ClearAllNotifications();
 /**
      * Connects Equipment Bridge to game equipment component
      * Searches for component on Pawn or PlayerState and establishes connection
      * @param Bridge Bridge to connect
      * @param PlayerController Controller to search for component
      */
 void ConnectEquipmentBridgeToGameComponent(USuspenseEquipmentUIBridge* Bridge, APlayerController* PlayerController);

protected:
    // Configuration of all widgets in the game
    UPROPERTY(EditDefaultsOnly, Category = "UI|Configuration")
    TArray<FSuspenseWidgetInfo> WidgetConfigurations;

    // Main HUD widget class
    UPROPERTY(EditDefaultsOnly, Category = "UI|Configuration")
    TSubclassOf<UUserWidget> MainHUDClass;

    // Tag for main HUD
    UPROPERTY(EditDefaultsOnly, Category = "UI|Configuration")
    FGameplayTag MainHUDTag = FGameplayTag::RequestGameplayTag("UI.HUD.Main");
    
    // Character Screen widget class
    UPROPERTY(EditDefaultsOnly, Category = "UI|Configuration")
    TSubclassOf<UUserWidget> CharacterScreenClass;
    
    // Tag for Character Screen
    UPROPERTY(EditDefaultsOnly, Category = "UI|Configuration")
    FGameplayTag CharacterScreenTag = FGameplayTag::RequestGameplayTag("UI.Screen.Character");

    // Notification widget class
    UPROPERTY(EditDefaultsOnly, Category = "UI|Configuration")
    TSubclassOf<UUserWidget> NotificationWidgetClass;

    // Tag for notification widgets
    UPROPERTY(EditDefaultsOnly, Category = "UI|Configuration")
    FGameplayTag NotificationTag = FGameplayTag::RequestGameplayTag("UI.Notification");

    /**
     * Initialize inventory bridge for specific layout
     * Called internally when inventory tab is opened
     * @param PlayerController Player controller for bridge
     * @param LayoutWidget Layout containing inventory widget
     */
    void InitializeInventoryBridgeForLayout(APlayerController* PlayerController, USuspenseBaseLayoutWidget* LayoutWidget);

    /**
     * Initialize equipment bridge for specific layout
     * Called internally when equipment tab is opened
     * @param PlayerController Player controller for bridge
     * @param LayoutWidget Layout containing equipment widget
     */
    void InitializeEquipmentBridgeForLayout(APlayerController* PlayerController, USuspenseBaseLayoutWidget* LayoutWidget);

    /**
     * Analyzes layout widget contents and creates necessary bridges
     * This is key method for layout approach - intelligently determines which bridges are needed
     * @param PlayerController Player controller for bridge initialization
     * @param LayoutWidget Layout widget to analyze
     */
    void AnalyzeLayoutAndCreateBridges(APlayerController* PlayerController, USuspenseBaseLayoutWidget* LayoutWidget);
    
    /**
     * Fallback method for legacy tab-based bridge initialization
     * Used when layout analysis is not possible
     * @param PlayerController Player controller
     * @param TabTag Opening tab tag
     */
    void InitializeBridgesByTabTag(APlayerController* PlayerController, FGameplayTag TabTag);
    
    /**
     * Finds TabBar widget inside Character Screen
     * Supports both direct cast and widget tree search
     * @param CharacterScreen Character screen to search
     * @return Found TabBar or nullptr
     */
    USuspenseUpperTabBar* FindTabBarInCharacterScreen(UUserWidget* CharacterScreen) const;
 
    /**
     * Connects Inventory Bridge to game inventory component
     * Searches for component on PlayerState and establishes connection
     * @param Bridge Bridge to connect
     * @param PlayerController Controller to search for component
     */
    void ConnectInventoryBridgeToGameComponent(USuspenseInventoryUIBridge* Bridge, APlayerController* PlayerController);

    /**
     * Настраивает универсальную систему обновления для всех виджетов в layout
     * Подписывается на события инвентаря и экипировки для автоматического обновления
     * @param LayoutWidget Layout виджет для настройки обновлений
     */
    void SetupUniversalLayoutRefresh(USuspenseBaseLayoutWidget* LayoutWidget);

    /**
     * Обновляет все виджеты в указанном layout
     * Вызывает RefreshUI для каждого bridge и RefreshScreenContent для виджетов
     * @param LayoutWidget Layout виджет с виджетами для обновления
     */
    void RefreshAllWidgetsInLayout(USuspenseBaseLayoutWidget* LayoutWidget);

private:
    // Active widgets storage (root widgets only)
    UPROPERTY()
    TMap<FGameplayTag, UUserWidget*> ActiveWidgets;

    // Parent-child relationship map for layout widgets
    UPROPERTY()
    TMap<UUserWidget*, UUserWidget*> WidgetParentMap;

    // Configuration cache for quick access
    TMap<FGameplayTag, FSuspenseWidgetInfo> ConfigurationCache;

    // Cached event manager reference
    UPROPERTY()
    UEventDelegateManager* CachedEventManager;

    // Cached inventory bridge
    UPROPERTY()
    USuspenseInventoryUIBridge* InventoryUIBridge;
    
    // Equipment UI Bridge
    UPROPERTY()
    USuspenseEquipmentUIBridge* EquipmentUIBridge;
    
    // Layout event handles
    FDelegateHandle LayoutWidgetCreatedHandle;
    FDelegateHandle LayoutWidgetDestroyedHandle;
    
    // Internal methods
    void BuildConfigurationCache();
    bool IsWidgetValid(UUserWidget* Widget) const;
    void CleanupWidget(UUserWidget* Widget);
    void NotifyWidgetCreated(UUserWidget* Widget, FGameplayTag WidgetTag);
    void NotifyWidgetDestroyed(FGameplayTag WidgetTag);
    UEventDelegateManager* GetEventManager() const;
    APlayerController* GetPlayerControllerFromObject(UObject* Object) const;
    int32 GetZOrderForWidget(FGameplayTag WidgetTag) const;
    bool ShouldAutoAddToViewport(FGameplayTag WidgetTag) const;
    bool IsRootWidgetTag(FGameplayTag WidgetTag) const;
    
    // Initialization methods (now empty - bridges created on demand)
    void InitializeInventoryBridge();
    void InitializeEquipmentBridge();
    
    // Layout event handling
    void SubscribeToLayoutEvents();
    void UnsubscribeFromLayoutEvents();
    void OnLayoutWidgetCreated(UObject* Source, const FString& EventData);
    void OnLayoutWidgetDestroyed(const FString& EventData);

    // Utility methods
    FText GameplayTagToDisplayText(const FGameplayTag& Tag) const;
    void CleanupPreviousSession();
};