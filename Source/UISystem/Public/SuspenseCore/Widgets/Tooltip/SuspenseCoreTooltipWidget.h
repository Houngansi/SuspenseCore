// SuspenseCoreTooltipWidget.h
// SuspenseCore - Item Tooltip Widget
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUITypes.h"
#include "SuspenseCoreTooltipWidget.generated.h"

// Forward declarations
class UImage;
class UTextBlock;
class UVerticalBox;

/**
 * USuspenseCoreTooltipWidget
 *
 * Tooltip widget for displaying item information.
 * Positioned near cursor and follows mouse movement.
 *
 * FEATURES:
 * - Item name, icon, description
 * - Stats display (damage, armor, weight, etc.)
 * - Comparison mode (show equipped item stats)
 * - Smart positioning to stay on screen
 *
 * @see USuspenseCoreContainerScreenWidget
 */
UCLASS(BlueprintType, Blueprintable)
class UISYSTEM_API USuspenseCoreTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreTooltipWidget(const FObjectInitializer& ObjectInitializer);

	//==================================================================
	// UUserWidget Interface
	//==================================================================

	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	//==================================================================
	// Tooltip Control
	//==================================================================

	/**
	 * Show tooltip for item at position
	 * @param ItemData Item data to display
	 * @param ScreenPosition Position to show tooltip
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Tooltip")
	void ShowForItem(const FSuspenseCoreItemUIData& ItemData, const FVector2D& ScreenPosition);

	/**
	 * Update tooltip position
	 * @param ScreenPosition New position
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Tooltip")
	void UpdatePosition(const FVector2D& ScreenPosition);

	/**
	 * Hide tooltip
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Tooltip")
	void Hide();

	/**
	 * Enable comparison mode with another item
	 * @param CompareItemData Item to compare against (usually equipped)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Tooltip")
	void SetComparisonItem(const FSuspenseCoreItemUIData& CompareItemData);

	/**
	 * Clear comparison mode
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Tooltip")
	void ClearComparison();

	//==================================================================
	// Blueprint Events
	//==================================================================

	/** Called to populate tooltip content - override in Blueprint */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Tooltip", meta = (DisplayName = "On Populate Tooltip"))
	void K2_OnPopulateTooltip(const FSuspenseCoreItemUIData& ItemData);

	/** Called when comparison mode changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Tooltip", meta = (DisplayName = "On Comparison Changed"))
	void K2_OnComparisonChanged(bool bInHasComparison, const FSuspenseCoreItemUIData& CompareItemData);

protected:
	//==================================================================
	// Content Population
	//==================================================================

	/** Populate tooltip content from item data */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|UI|Tooltip")
	void PopulateContent(const FSuspenseCoreItemUIData& ItemData);
	virtual void PopulateContent_Implementation(const FSuspenseCoreItemUIData& ItemData);

	/** Calculate best position to keep tooltip on screen */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|UI|Tooltip")
	FVector2D CalculateBestPosition(const FVector2D& DesiredPosition);
	virtual FVector2D CalculateBestPosition_Implementation(const FVector2D& DesiredPosition);

	/**
	 * Main positioning method - handles DPI scaling and viewport bounds
	 * @param ScreenPosition Raw screen position from mouse
	 */
	void RepositionTooltip(const FVector2D& ScreenPosition);

	//==================================================================
	// Widget References (Bind in Blueprint)
	//==================================================================

	/** Item icon */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UImage> ItemIcon;

	/** Item name text */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UTextBlock> ItemNameText;

	/** Item description text */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UTextBlock> DescriptionText;

	/** Container for stats */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets")
	TObjectPtr<UVerticalBox> StatsContainer;

	//==================================================================
	// Configuration
	//==================================================================

	/** Offset from cursor position */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
	FVector2D CursorOffset;

	/** Padding from screen edge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
	float ScreenEdgePadding;

	/** Delay before showing tooltip */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
	float ShowDelay;

private:
	//==================================================================
	// State
	//==================================================================

	/** Current item data */
	FSuspenseCoreItemUIData CurrentItemData;

	/** Comparison item data */
	FSuspenseCoreItemUIData ComparisonItemData;

	/** Is comparison mode active */
	bool bHasComparison;

	/** Target position */
	FVector2D TargetPosition;

	/** Time until tooltip should show */
	float ShowDelayTimer;
};
