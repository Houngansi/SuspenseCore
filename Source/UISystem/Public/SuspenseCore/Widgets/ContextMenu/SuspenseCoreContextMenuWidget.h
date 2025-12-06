// SuspenseCoreContextMenuWidget.h
// SuspenseCore - Item Context Menu Widget
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUITypes.h"
#include "SuspenseCoreContextMenuWidget.generated.h"

// Forward declarations
class UVerticalBox;
class UButton;

/**
 * Delegate for context menu action selection
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSuspenseCoreContextAction, FGameplayTag, ActionTag, FGuid, ContainerID, int32, SlotIndex);

/**
 * FSuspenseCoreContextMenuAction
 * Data for a context menu action
 */
USTRUCT(BlueprintType)
struct UISYSTEM_API FSuspenseCoreContextMenuAction
{
	GENERATED_BODY()

	/** Action tag */
	UPROPERTY(BlueprintReadWrite, Category = "Action")
	FGameplayTag ActionTag;

	/** Display text */
	UPROPERTY(BlueprintReadWrite, Category = "Action")
	FText DisplayText;

	/** Icon (optional) */
	UPROPERTY(BlueprintReadWrite, Category = "Action")
	TObjectPtr<UTexture2D> Icon;

	/** Is action enabled */
	UPROPERTY(BlueprintReadWrite, Category = "Action")
	bool bEnabled;

	/** Button widget */
	UPROPERTY(BlueprintReadWrite, Category = "Action")
	TObjectPtr<UButton> ActionButton;

	FSuspenseCoreContextMenuAction()
		: bEnabled(true)
	{
	}

	FSuspenseCoreContextMenuAction(const FGameplayTag& InTag, const FText& InText, bool bInEnabled = true)
		: ActionTag(InTag)
		, DisplayText(InText)
		, bEnabled(bInEnabled)
	{
	}
};

/**
 * USuspenseCoreContextMenuWidget
 *
 * Context menu for item actions (Use, Equip, Drop, etc.)
 * Shows available actions based on item type and context.
 *
 * FEATURES:
 * - Dynamic action list from provider
 * - Keyboard navigation
 * - Action execution via EventBus
 * - Auto-close on action or click outside
 *
 * @see USuspenseCoreContainerScreenWidget
 */
UCLASS(BlueprintType, Blueprintable)
class UISYSTEM_API USuspenseCoreContextMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreContextMenuWidget(const FObjectInitializer& ObjectInitializer);

	//==================================================================
	// UUserWidget Interface
	//==================================================================

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	//==================================================================
	// Context Menu Control
	//==================================================================

	/**
	 * Show context menu for item
	 * @param ItemData Item data
	 * @param InContainerID Container ID
	 * @param InSlotIndex Slot index
	 * @param ScreenPosition Position to show menu
	 * @param AvailableActions Actions to display
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|ContextMenu")
	void ShowForItem(
		const FSuspenseCoreItemUIData& ItemData,
		const FGuid& InContainerID,
		int32 InSlotIndex,
		const FVector2D& ScreenPosition,
		const TArray<FGameplayTag>& AvailableActions);

	/**
	 * Hide context menu
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|ContextMenu")
	void Hide();

	/**
	 * Execute selected action
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|ContextMenu")
	void ExecuteSelectedAction();

	/**
	 * Navigate selection up
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|ContextMenu")
	void NavigateUp();

	/**
	 * Navigate selection down
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|ContextMenu")
	void NavigateDown();

	//==================================================================
	// Events
	//==================================================================

	/** Fired when an action is selected */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|UI|ContextMenu")
	FOnSuspenseCoreContextAction OnActionSelected;

	//==================================================================
	// Blueprint Events
	//==================================================================

	/** Called to create action button - override in Blueprint for custom styling */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|ContextMenu", meta = (DisplayName = "On Create Action Button"))
	UButton* K2_OnCreateActionButton(const FSuspenseCoreContextMenuAction& Action);

	/** Called when action is executed */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|ContextMenu", meta = (DisplayName = "On Action Executed"))
	void K2_OnActionExecuted(const FGameplayTag& ActionTag);

protected:
	//==================================================================
	// Action Management
	//==================================================================

	/** Create action buttons */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|UI|ContextMenu")
	void CreateActionButtons(const TArray<FGameplayTag>& ActionTags);
	virtual void CreateActionButtons_Implementation(const TArray<FGameplayTag>& ActionTags);

	/** Get display text for action tag */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|UI|ContextMenu")
	FText GetActionDisplayText(const FGameplayTag& ActionTag);
	virtual FText GetActionDisplayText_Implementation(const FGameplayTag& ActionTag);

	/** Handle action button clicked */
	UFUNCTION()
	void OnActionButtonClicked(FGameplayTag ActionTag);

	/** Update selection visual */
	void UpdateSelectionVisual();

	/** Calculate best position to stay on screen */
	FVector2D CalculateBestPosition(const FVector2D& DesiredPosition);

	//==================================================================
	// Widget References (Bind in Blueprint)
	//==================================================================

	/** Container for action buttons */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UVerticalBox> ActionContainer;

	//==================================================================
	// Configuration
	//==================================================================

	/** Padding from screen edge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
	float ScreenEdgePadding;

private:
	//==================================================================
	// State
	//==================================================================

	/** Current item data */
	FSuspenseCoreItemUIData CurrentItemData;

	/** Container ID for current item */
	FGuid ContainerID;

	/** Slot index for current item */
	int32 SlotIndex;

	/** Actions in menu */
	UPROPERTY(Transient)
	TArray<FSuspenseCoreContextMenuAction> Actions;

	/** Currently selected action index */
	int32 SelectedActionIndex;
};
