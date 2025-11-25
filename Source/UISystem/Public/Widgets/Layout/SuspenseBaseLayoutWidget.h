// MedComBaseLayoutWidget.h
// Copyright Suspense Team Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Base/SuspenseBaseWidget.h"
#include "Interfaces/UI/ISuspenseLayoutInterface.h"
#include "SuspenseBaseLayoutWidget.generated.h"

/**
 * Configuration for a widget in layout
 * Each widget must have explicit tags for proper identification
 */
USTRUCT(BlueprintType)
struct FLayoutWidgetConfig
{
    GENERATED_BODY()

    /** Widget class to create */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSubclassOf<UUserWidget> WidgetClass;

    /** 
     * Tag for identifying widget in layout - REQUIRED
     * This tag must be unique within the layout
     * Example: "UI.Widget.Inventory", "UI.Widget.Equipment"
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGameplayTag WidgetTag;

    /** 
     * Layout-specific slot tag (e.g., "Layout.Slot.Left", "Layout.Slot.Right", "Layout.Slot.Center")
     * Used for positioning within specific layout types
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGameplayTag SlotTag;

    /** 
     * Size weight for flexible layouts (0 = auto, >0 = flex)
     * Higher values take more space in flex layouts
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
    float SizeWeight = 1.0f;

    /** Padding around widget */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FMargin Padding;

    /** 
     * Whether this widget should be created immediately
     * If false, widget will be created on demand
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bCreateImmediately = true;

    /** 
     * Whether to initialize widget after creation
     * Set to false if you need custom initialization
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAutoInitialize = true;

    /**
     * Whether to register widget in UIManager
     * Set to true for widgets that need global access
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bRegisterInUIManager = false;

    FLayoutWidgetConfig()
    {
        SizeWeight = 1.0f;
        Padding = FMargin(4.0f);
        bCreateImmediately = true;
        bAutoInitialize = true;
        bRegisterInUIManager = false;
    }

    /** Validation helper */
    bool IsValid() const
    {
        return WidgetClass != nullptr && WidgetTag.IsValid();
    }
};

/**
 * Base class for layout widgets that can contain multiple child widgets
 * Provides foundation for different layout types (horizontal, vertical, grid, etc.)
 * 
 * Key features:
 * - Requires explicit tags for all widgets (no dynamic generation)
 * - Supports lazy creation of widgets
 * - Provides consistent initialization pipeline
 * - Handles proper cleanup and lifecycle management
 * - Integrates with UIManager for widget registration
 */
UCLASS(Abstract)
class UISYSTEM_API USuspenseBaseLayoutWidget : public USuspenseBaseWidget, public ISuspenseLayoutInterface
{
    GENERATED_BODY()

public:
    USuspenseBaseLayoutWidget(const FObjectInitializer& ObjectInitializer);

    //~ Begin UUserWidget Interface
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    //~ End UUserWidget Interface

    //~ Begin ISuspenseUIWidgetInterface Interface
    virtual void InitializeWidget_Implementation() override;
    virtual void UninitializeWidget_Implementation() override;
    //~ End ISuspenseUIWidgetInterface Interface

    //~ Begin ISuspenseLayoutInterface Interface
    virtual bool AddWidgetToLayout_Implementation(UUserWidget* Widget, FGameplayTag SlotTag) override;
    virtual bool RemoveWidgetFromLayout_Implementation(UUserWidget* Widget) override;
    virtual TArray<UUserWidget*> GetLayoutWidgets_Implementation() const override;
    virtual void ClearLayout_Implementation() override;
    virtual void RefreshLayout_Implementation() override;
    //~ End ISuspenseLayoutInterface Interface

    /**
     * Initialize layout from configuration
     * Creates and arranges all configured widgets marked for immediate creation
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Layout")
    virtual void InitializeFromConfig();

    /**
     * Get widget by tag
     * @param InWidgetTag Tag to search for
     * @return Found widget or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Layout")
    UUserWidget* GetWidgetByTag(FGameplayTag InWidgetTag) const;

    /**
     * Create widget from configuration on demand
     * @param InWidgetTag Tag of widget to create
     * @return Created widget or nullptr if already exists or config not found
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Layout")
    UUserWidget* CreateWidgetByTag(FGameplayTag InWidgetTag);

    /**
     * Check if widget with tag exists in layout
     * @param InWidgetTag Tag to check
     * @return True if widget exists
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Layout")
    bool HasWidget(FGameplayTag InWidgetTag) const;

    /**
     * Get configuration for widget by tag
     * @param InWidgetTag Tag to search for
     * @return Pointer to config or nullptr
     */
    const FLayoutWidgetConfig* GetWidgetConfig(FGameplayTag InWidgetTag) const;

    /**
     * Validate all widget configurations
     * @return True if all configs are valid
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Layout")
    bool ValidateConfiguration() const;

    /**
     * Get all widget tags in this layout
     * @return Array of widget tags
     */
    UFUNCTION(BlueprintCallable, Category = "UI|Layout")
    TArray<FGameplayTag> GetAllWidgetTags() const;

protected:
    /** 
     * Configuration for widgets in this layout
     * All widgets must have unique WidgetTag values
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout|Config", meta = (TitleProperty = "WidgetTag"))
    TArray<FLayoutWidgetConfig> WidgetConfigurations;

    /** Whether to create widgets automatically on initialization */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout|Config")
    bool bAutoCreateWidgets = true;

    /** Whether to validate configuration on initialization */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout|Config")
    bool bValidateOnInit = true;

    /** Whether to register this layout in UIManager */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout|Config")
    bool bRegisterLayoutInUIManager = false;

    /** Created widget instances mapped by their tags */
    UPROPERTY(BlueprintReadOnly, Category = "Layout")
    TMap<FGameplayTag, UUserWidget*> LayoutWidgets;

    /**
     * Get the panel widget that will contain child widgets
     * Must be overridden by derived classes
     */
    virtual UPanelWidget* GetLayoutPanel() const PURE_VIRTUAL(USuspenseBaseLayoutWidget::GetLayoutPanel, return nullptr;);

    /**
     * Add widget to the layout panel
     * Must be overridden by derived classes for specific layout logic
     */
    virtual bool AddWidgetToPanel(UUserWidget* Widget, const FLayoutWidgetConfig* Config) PURE_VIRTUAL(USuspenseBaseLayoutWidget::AddWidgetToPanel, return false;);

    /**
     * Create widget from configuration
     * Can be overridden for custom creation logic
     */
    virtual UUserWidget* CreateLayoutWidget(const FLayoutWidgetConfig& Config);

    /**
     * Initialize a created widget
     * Can be overridden for custom initialization
     */
    virtual void InitializeLayoutWidget(UUserWidget* Widget, const FLayoutWidgetConfig& Config);

    /**
     * Called when configuration validation fails
     * @param InvalidWidgetTag Tag of the invalid widget config
     * @param ValidationError Description of the validation failure
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "UI|Layout", DisplayName = "On Validation Failed")
    void K2_OnValidationFailed(FGameplayTag InvalidWidgetTag, const FString& ValidationError);

    /**
     * Called when a widget is added to layout
     * @param Widget The added widget
     * @param InWidgetTag Tag of the added widget
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "UI|Layout", DisplayName = "On Widget Added")
    void K2_OnWidgetAdded(UUserWidget* Widget, FGameplayTag InWidgetTag);

    /**
     * Called when a widget is removed from layout
     * @param Widget The removed widget
     * @param InWidgetTag Tag of the removed widget
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "UI|Layout", DisplayName = "On Widget Removed")
    void K2_OnWidgetRemoved(UUserWidget* Widget, FGameplayTag InWidgetTag);

    /**
     * Called when layout is about to be cleared
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "UI|Layout", DisplayName = "On Layout Clearing")
    void K2_OnLayoutClearing();

    /**
     * Called after layout has been refreshed
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "UI|Layout", DisplayName = "On Layout Refreshed")
    void K2_OnLayoutRefreshed();

private:
    /** Create all configured widgets marked for immediate creation */
    void CreateConfiguredWidgets();

    /** Clear all created widgets */
    void ClearCreatedWidgets();

    /** Internal validation implementation */
    bool ValidateConfigurationInternal() const;

    /** Check for duplicate tags in configuration */
    bool HasDuplicateTags() const;

    /** Get config by tag - internal helper */
    const FLayoutWidgetConfig* FindConfigByTag(FGameplayTag InTag) const;

    /** Register widget in UIManager if needed */
    void RegisterWidgetInUIManager(UUserWidget* Widget, const FGameplayTag& WidgetTag);

    /** Unregister widget from UIManager */
    void UnregisterWidgetFromUIManager(const FGameplayTag& WidgetTag);

    /** Notify about widget creation */
    void NotifyWidgetCreated(UUserWidget* Widget, const FGameplayTag& WidgetTag);

    /** Notify about widget destruction */
    void NotifyWidgetDestroyed(const FGameplayTag& WidgetTag);

    /** Get UIManager instance */
    class USuspenseUIManager* GetUIManager() const;

    /** Get EventDelegateManager instance */
    class UEventDelegateManager* GetEventManager() const;
};