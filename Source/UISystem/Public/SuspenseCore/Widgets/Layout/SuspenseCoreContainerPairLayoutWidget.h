// SuspenseCoreContainerPairLayoutWidget.h
// SuspenseCore - Container Pair Layout Widget
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUIContainerTypes.h"
#include "SuspenseCoreContainerPairLayoutWidget.generated.h"

// Forward declarations
class USuspenseCoreBaseContainerWidget;
class ISuspenseCoreUIDataProvider;
class UCanvasPanel;

/**
 * USuspenseCoreContainerPairLayoutWidget
 *
 * A layout widget that holds a PAIR of container widgets (e.g., Equipment + Inventory).
 * Designed for Blueprint configuration - just drag existing widgets as children.
 *
 * USAGE:
 * 1. Create a Blueprint child (e.g., WBP_EquipmentInventoryLayout)
 * 2. Add CanvasPanel as root
 * 3. Add WBP_EquipmentWidget and WBP_InventoryWidget as children
 * 4. Position them freely on canvas
 * 5. Bind to PrimaryContainer and SecondaryContainer slots
 * 6. Set PrimaryContainerType and SecondaryContainerType
 *
 * ARCHITECTURE:
 * - Does NOT create widgets - uses existing Blueprints via BindWidget
 * - Handles provider binding for both containers automatically
 * - Designed for PanelSwitcher as ContentWidgetClass
 *
 * EXAMPLE BLUEPRINTS:
 * - WBP_EquipmentInventoryLayout (Equipment left + Inventory right)
 * - WBP_StashInventoryLayout (Stash left + Inventory right)
 * - WBP_TraderInventoryLayout (Trader left + Inventory right)
 *
 * @see USuspenseCoreBaseContainerWidget
 * @see USuspenseCorePanelSwitcherWidget
 */
UCLASS(BlueprintType, Blueprintable)
class UISYSTEM_API USuspenseCoreContainerPairLayoutWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreContainerPairLayoutWidget(const FObjectInitializer& ObjectInitializer);

	//==================================================================
	// UUserWidget Interface
	//==================================================================

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/**
	 * Override to detect WidgetSwitcher show/hide
	 * Called when WidgetSwitcher changes active index
	 */
	virtual void SetVisibility(ESlateVisibility InVisibility) override;

	//==================================================================
	// Container Access
	//==================================================================

	/**
	 * Get primary container widget (left/main)
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Layout")
	USuspenseCoreBaseContainerWidget* GetPrimaryContainer() const { return PrimaryContainer; }

	/**
	 * Get secondary container widget (right/auxiliary)
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Layout")
	USuspenseCoreBaseContainerWidget* GetSecondaryContainer() const { return SecondaryContainer; }

	/**
	 * Get both containers as array
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Layout")
	TArray<USuspenseCoreBaseContainerWidget*> GetAllContainers() const;

	/**
	 * Get container by type
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Layout")
	USuspenseCoreBaseContainerWidget* GetContainerByType(ESuspenseCoreContainerType ContainerType) const;

	//==================================================================
	// Provider Binding
	//==================================================================

	/**
	 * Bind both containers to player's providers automatically
	 * Finds matching providers based on configured container types
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Layout")
	void BindToPlayerProviders();

	/**
	 * Bind specific provider to container by type
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Layout")
	bool BindProviderToContainer(ESuspenseCoreContainerType ContainerType, TScriptInterface<ISuspenseCoreUIDataProvider> Provider);

	/**
	 * Unbind all providers
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Layout")
	void UnbindAllProviders();

	/**
	 * Refresh both containers
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Layout")
	void RefreshAllContainers();

	//==================================================================
	// Configuration
	//==================================================================

	/**
	 * Get layout tag (identifies this layout configuration)
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Layout")
	FGameplayTag GetLayoutTag() const { return LayoutTag; }

	/**
	 * Get primary container type
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Layout")
	ESuspenseCoreContainerType GetPrimaryContainerType() const { return PrimaryContainerType; }

	/**
	 * Get secondary container type
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Layout")
	ESuspenseCoreContainerType GetSecondaryContainerType() const { return SecondaryContainerType; }

	//==================================================================
	// Panel Lifecycle
	//==================================================================

	/**
	 * Called when this layout becomes visible (panel switched to it)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Layout")
	void OnLayoutShown();

	/**
	 * Called when this layout is hidden (panel switched away)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Layout")
	void OnLayoutHidden();

	/**
	 * Is layout currently active
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Layout")
	bool IsLayoutActive() const { return bIsActive; }

	//==================================================================
	// Blueprint Events
	//==================================================================

	/** Called after NativeConstruct - containers are ready */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Layout", meta = (DisplayName = "On Layout Constructed"))
	void K2_OnLayoutConstructed();

	/** Called when layout becomes visible */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Layout", meta = (DisplayName = "On Layout Shown"))
	void K2_OnLayoutShown();

	/** Called when layout is hidden */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Layout", meta = (DisplayName = "On Layout Hidden"))
	void K2_OnLayoutHidden();

	/** Called when providers are bound */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Layout", meta = (DisplayName = "On Providers Bound"))
	void K2_OnProvidersBound();

protected:
	//==================================================================
	// Widget References (Bind in Blueprint)
	//==================================================================

	/**
	 * Primary container widget (left side / main)
	 * Bind to your Equipment/Stash/Trader widget in Blueprint
	 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Containers")
	TObjectPtr<USuspenseCoreBaseContainerWidget> PrimaryContainer;

	/**
	 * Secondary container widget (right side / auxiliary)
	 * Usually binds to Inventory widget
	 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Containers")
	TObjectPtr<USuspenseCoreBaseContainerWidget> SecondaryContainer;

	/**
	 * Canvas panel for free positioning (optional root)
	 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Layout")
	TObjectPtr<UCanvasPanel> LayoutCanvas;

	//==================================================================
	// Configuration (Set in Blueprint Defaults)
	//==================================================================

	/**
	 * Tag identifying this layout pair
	 * Examples: SuspenseCore.UI.Layout.EquipmentInventory
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
	FGameplayTag LayoutTag;

	/**
	 * Expected type for primary (left) container
	 * Used for automatic provider matching
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
	ESuspenseCoreContainerType PrimaryContainerType = ESuspenseCoreContainerType::Equipment;

	/**
	 * Expected type for secondary (right) container
	 * Used for automatic provider matching
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
	ESuspenseCoreContainerType SecondaryContainerType = ESuspenseCoreContainerType::Inventory;

	/**
	 * Auto-bind to player providers on construct
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
	bool bAutoBindOnConstruct = true;

	//==================================================================
	// Helper Methods
	//==================================================================

	/** Find provider for container type on player */
	TScriptInterface<ISuspenseCoreUIDataProvider> FindProviderForType(ESuspenseCoreContainerType ContainerType) const;

	/** Validate container types match configuration */
	void ValidateContainerTypes();

private:
	//==================================================================
	// State
	//==================================================================

	/** Is layout currently active/visible */
	bool bIsActive = false;

	/** Has the layout been initialized */
	bool bIsInitialized = false;
};
