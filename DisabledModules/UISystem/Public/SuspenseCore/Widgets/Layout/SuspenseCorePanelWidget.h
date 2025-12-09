// SuspenseCorePanelWidget.h
// SuspenseCore - Panel Widget containing multiple containers
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUIContainerTypes.h"
#include "SuspenseCorePanelWidget.generated.h"

// Forward declarations
class USuspenseCoreBaseContainerWidget;
class ISuspenseCoreUIDataProvider;
class UHorizontalBox;
class UVerticalBox;

/**
 * USuspenseCorePanelWidget
 *
 * A panel containing one or more container widgets arranged in a layout.
 * Used within USuspenseCoreContainerScreenWidget for panel switching.
 *
 * LAYOUT OPTIONS:
 * - Horizontal: Containers side by side (e.g., Inventory | Equipment)
 * - Vertical: Containers stacked (less common)
 * - Custom: Blueprint-defined layout
 *
 * CONTAINER BINDING:
 * - Automatically finds and binds to providers based on container types
 * - Supports player inventory, external containers, etc.
 *
 * @see USuspenseCoreContainerScreenWidget
 * @see USuspenseCoreBaseContainerWidget
 */
UCLASS(BlueprintType, Blueprintable)
class UISYSTEM_API USuspenseCorePanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCorePanelWidget(const FObjectInitializer& ObjectInitializer);

	//==================================================================
	// UUserWidget Interface
	//==================================================================

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//==================================================================
	// Initialization
	//==================================================================

	/**
	 * Initialize panel with configuration
	 * @param InPanelConfig Configuration defining container layout
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Panel")
	void InitializePanel(const FSuspenseCorePanelConfig& InPanelConfig);

	/**
	 * Get panel configuration
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Panel")
	const FSuspenseCorePanelConfig& GetPanelConfig() const { return PanelConfig; }

	/**
	 * Get panel tag
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Panel")
	FGameplayTag GetPanelTag() const { return PanelConfig.PanelTag; }

	//==================================================================
	// Container Management
	//==================================================================

	/**
	 * Get all container widgets in this panel
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Panel")
	TArray<USuspenseCoreBaseContainerWidget*> GetContainerWidgets() const { return ContainerWidgets; }

	/**
	 * Get container widget by type
	 * @param ContainerType Type of container to find
	 * @return First container matching the type, or nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Panel")
	USuspenseCoreBaseContainerWidget* GetContainerByType(ESuspenseCoreContainerType ContainerType) const;

	/**
	 * Get container widget at index
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Panel")
	USuspenseCoreBaseContainerWidget* GetContainerAtIndex(int32 Index) const;

	/**
	 * Bind container to provider
	 * @param ContainerType Type of container to bind
	 * @param Provider Provider to bind to
	 * @return True if bound successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Panel")
	bool BindContainerToProvider(ESuspenseCoreContainerType ContainerType, TScriptInterface<ISuspenseCoreUIDataProvider> Provider);

	/**
	 * Bind all containers to appropriate providers for player
	 * Finds providers automatically based on container types
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Panel")
	void BindToPlayerProviders();

	/**
	 * Bind secondary container (for two-container panels like Inventory+Stash)
	 * @param Provider External provider (stash, trader, loot, etc.)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Panel")
	void BindSecondaryProvider(TScriptInterface<ISuspenseCoreUIDataProvider> Provider);

	/**
	 * Refresh all containers
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Panel")
	void RefreshAllContainers();

	//==================================================================
	// Panel State
	//==================================================================

	/**
	 * Called when panel becomes visible (switched to)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Panel")
	void OnPanelShown();

	/**
	 * Called when panel is hidden (switched away)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Panel")
	void OnPanelHidden();

	/**
	 * Is this panel currently active/visible
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Panel")
	bool IsPanelActive() const { return bIsActive; }

	//==================================================================
	// Blueprint Events
	//==================================================================

	/** Called when panel is initialized */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Panel", meta = (DisplayName = "On Panel Initialized"))
	void K2_OnPanelInitialized();

	/** Called when panel is shown */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Panel", meta = (DisplayName = "On Panel Shown"))
	void K2_OnPanelShown();

	/** Called when panel is hidden */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Panel", meta = (DisplayName = "On Panel Hidden"))
	void K2_OnPanelHidden();

	/** Called when container is created - allows Blueprint customization */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Panel", meta = (DisplayName = "On Container Created"))
	void K2_OnContainerCreated(USuspenseCoreBaseContainerWidget* Container, ESuspenseCoreContainerType ContainerType);

protected:
	//==================================================================
	// Container Creation
	//==================================================================

	/** Create container widgets based on config */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|UI|Panel")
	void CreateContainers();
	virtual void CreateContainers_Implementation();

	/** Get widget class for container type */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|UI|Panel")
	TSubclassOf<USuspenseCoreBaseContainerWidget> GetWidgetClassForContainerType(ESuspenseCoreContainerType ContainerType);
	virtual TSubclassOf<USuspenseCoreBaseContainerWidget> GetWidgetClassForContainerType_Implementation(ESuspenseCoreContainerType ContainerType);

	//==================================================================
	// Widget References (Bind in Blueprint)
	//==================================================================

	/** Container for horizontally arranged containers */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UHorizontalBox> HorizontalContainerBox;

	/** Container for vertically arranged containers */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UVerticalBox> VerticalContainerBox;

	//==================================================================
	// Configuration
	//==================================================================

	/** Default widget class for inventory containers */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
	TSubclassOf<USuspenseCoreBaseContainerWidget> InventoryWidgetClass;

	/** Default widget class for equipment containers */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
	TSubclassOf<USuspenseCoreBaseContainerWidget> EquipmentWidgetClass;

	/** Default widget class for stash containers */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
	TSubclassOf<USuspenseCoreBaseContainerWidget> StashWidgetClass;

	/** Default widget class for trader containers */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
	TSubclassOf<USuspenseCoreBaseContainerWidget> TraderWidgetClass;

	/** Default widget class for loot containers */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
	TSubclassOf<USuspenseCoreBaseContainerWidget> LootWidgetClass;

private:
	//==================================================================
	// State
	//==================================================================

	/** Panel configuration */
	FSuspenseCorePanelConfig PanelConfig;

	/** Created container widgets */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USuspenseCoreBaseContainerWidget>> ContainerWidgets;

	/** Map container types to widgets */
	TMap<ESuspenseCoreContainerType, TObjectPtr<USuspenseCoreBaseContainerWidget>> ContainersByType;

	/** Is panel currently active */
	bool bIsActive;
};
