// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Types/UI/SuspenseContainerUITypes.h"
#include "ISuspenseTooltip.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class USuspenseTooltipInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for tooltip widgets
 * Provides standardized methods for displaying item information
 */
class BRIDGESYSTEM_API ISuspenseTooltip
{
	GENERATED_BODY()

public:
	/**
	 * Show tooltip with item data
	 * @param ItemData Item data to display
	 * @param ScreenPosition Screen position for tooltip
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Tooltip")
	void ShowTooltip(const FItemUIData& ItemData, const FVector2D& ScreenPosition);

	/**
	 * Hide the tooltip
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Tooltip")
	void HideTooltip();

	/**
	 * Update tooltip position
	 * @param ScreenPosition New screen position
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Tooltip")
	void UpdateTooltipPosition(const FVector2D& ScreenPosition);

	/**
	 * Check if tooltip is currently visible
	 * @return True if visible
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Tooltip")
	bool IsTooltipVisible() const;

	/**
	 * Set tooltip anchor and pivot for positioning
	 * @param Anchor Anchor point (0,0 = top-left, 1,1 = bottom-right)
	 * @param Pivot Pivot point for rotation/positioning
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Tooltip")
	void SetTooltipAnchor(const FVector2D& Anchor, const FVector2D& Pivot);
};