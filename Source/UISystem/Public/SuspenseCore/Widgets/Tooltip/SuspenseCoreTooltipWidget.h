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
class UBorder;
class USizeBox;

/**
 * USuspenseCoreTooltipWidget
 *
 * Tooltip widget for displaying item information with smooth animations.
 * Positioned near cursor and follows mouse movement.
 *
 * BINDWIDGET COMPONENTS (create in Blueprint with EXACT names):
 * ============================================================
 * REQUIRED:
 * - RootBorder: UBorder - Main container for fade animation
 * - ItemNameText: UTextBlock - Item display name (colored by rarity)
 *
 * OPTIONAL (but recommended):
 * - ItemIcon: UImage - Item icon image
 * - ItemTypeText: UTextBlock - Item type (Weapon, Armor, etc.)
 * - DescriptionText: UTextBlock - Item description
 * - WeightText: UTextBlock - Item weight (e.g., "3.4 kg")
 * - ValueText: UTextBlock - Item value/price (e.g., "38,000")
 * - SizeText: UTextBlock - Grid size (e.g., "5x2")
 * - RarityBorder: UBorder - Border colored by rarity
 * - StatsContainer: UVerticalBox - Container for dynamic stat widgets
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
	 * Show tooltip for item at position with fade-in animation
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
	 * Hide tooltip with fade-out animation
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Tooltip")
	void Hide();

	/**
	 * Hide tooltip immediately without animation
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Tooltip")
	void HideImmediate();

	/**
	 * Check if tooltip is currently visible or animating
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Tooltip")
	bool IsTooltipVisible() const { return bIsShowing || CurrentOpacity > 0.0f; }

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

	/** Called to populate tooltip content - override in Blueprint for custom display */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Tooltip", meta = (DisplayName = "On Populate Tooltip"))
	void K2_OnPopulateTooltip(const FSuspenseCoreItemUIData& ItemData);

	/** Called when comparison mode changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Tooltip", meta = (DisplayName = "On Comparison Changed"))
	void K2_OnComparisonChanged(bool bInHasComparison, const FSuspenseCoreItemUIData& CompareItemData);

	/** Called when fade animation starts */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Tooltip", meta = (DisplayName = "On Fade Started"))
	void K2_OnFadeStarted(bool bInFadingIn);

	/** Called when fade animation completes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|UI|Tooltip", meta = (DisplayName = "On Fade Completed"))
	void K2_OnFadeCompleted(bool bInWasFadingIn);

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

	/** Update fade animation */
	void UpdateFadeAnimation(float DeltaTime);

	/** Get color for rarity tag */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Tooltip")
	FLinearColor GetRarityColor(const FGameplayTag& RarityTag) const;

	/** Format weight for display */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Tooltip")
	FText FormatWeight(float Weight) const;

	/** Format value for display */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Tooltip")
	FText FormatValue(int32 Value) const;

	/** Get readable item type from tag */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Tooltip")
	FText GetItemTypeDisplayName(const FGameplayTag& ItemTypeTag) const;

	//==================================================================
	// Widget References - REQUIRED (must exist in Blueprint)
	//==================================================================

	/** Root border - REQUIRED for fade animation. Name: "RootBorder" */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Widgets|Required")
	TObjectPtr<UBorder> RootBorder;

	/** Item name text - REQUIRED. Name: "ItemNameText" */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Widgets|Required")
	TObjectPtr<UTextBlock> ItemNameText;

	//==================================================================
	// Widget References - OPTIONAL (recommended for full display)
	//==================================================================

	/** Item icon image. Name: "ItemIcon" */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets|Optional")
	TObjectPtr<UImage> ItemIcon;

	/** Item type text (Assault Rifle, Helmet, etc.). Name: "ItemTypeText" */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets|Optional")
	TObjectPtr<UTextBlock> ItemTypeText;

	/** Item description text. Name: "DescriptionText" */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets|Optional")
	TObjectPtr<UTextBlock> DescriptionText;

	/** Weight text (e.g., "3.4 kg"). Name: "WeightText" */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets|Optional")
	TObjectPtr<UTextBlock> WeightText;

	/** Value/price text. Name: "ValueText" */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets|Optional")
	TObjectPtr<UTextBlock> ValueText;

	/** Grid size text (e.g., "5x2"). Name: "SizeText" */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets|Optional")
	TObjectPtr<UTextBlock> SizeText;

	/** Rarity border (colored by rarity). Name: "RarityBorder" */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets|Optional")
	TObjectPtr<UBorder> RarityBorder;

	/** Container for dynamic stats. Name: "StatsContainer" */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Widgets|Optional")
	TObjectPtr<UVerticalBox> StatsContainer;

	//==================================================================
	// Configuration - Positioning
	//==================================================================

	/** Offset from cursor position */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration|Position")
	FVector2D CursorOffset;

	/** Padding from screen edge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration|Position")
	float ScreenEdgePadding;

	//==================================================================
	// Configuration - Animation
	//==================================================================

	/** Duration of fade-in animation (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration|Animation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FadeInDuration;

	/** Duration of fade-out animation (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration|Animation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FadeOutDuration;

	//==================================================================
	// Configuration - Rarity Colors
	//==================================================================

	/** Color for Common rarity */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration|Colors")
	FLinearColor CommonColor;

	/** Color for Uncommon rarity */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration|Colors")
	FLinearColor UncommonColor;

	/** Color for Rare rarity */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration|Colors")
	FLinearColor RareColor;

	/** Color for Epic rarity */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration|Colors")
	FLinearColor EpicColor;

	/** Color for Legendary rarity */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration|Colors")
	FLinearColor LegendaryColor;

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

	/** Is tooltip currently showing (or fading in) */
	bool bIsShowing;

	/** Is currently fading (in or out) */
	bool bIsFading;

	/** Is fading in (true) or out (false) */
	bool bFadingIn;

	/** Current opacity (0.0 - 1.0) */
	float CurrentOpacity;

	//==================================================================
	// Animation State (AAA-quality easing)
	//==================================================================

	/** Current animation progress (0.0 -> 1.0) - allows seamless reverse */
	float AnimProgress;

	/** Starting scale for pop-in effect (93% for subtle growth) */
	static constexpr float StartScale = 0.93f;

	/** Vertical drift in pixels for float-up effect */
	static constexpr float VerticalDrift = 8.0f;
};
