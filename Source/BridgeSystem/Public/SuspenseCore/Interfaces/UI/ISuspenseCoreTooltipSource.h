// Copyright Suspense Team. All Rights Reserved.

#ifndef SUSPENSECORE_INTERFACES_UI_ISUSPENSECORETOOLTIPSOURCE_H
#define SUSPENSECORE_INTERFACES_UI_ISUSPENSECORETOOLTIPSOURCE_H

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SuspenseCore/Types/UI/SuspenseCoreContainerUITypes.h"
#include "ISuspenseTooltipSource.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseTooltipSource : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for widgets that can trigger tooltip display
 * Follows Interface Segregation Principle - separate from tooltip widget interface
 */
class BRIDGESYSTEM_API ISuspenseTooltipSource
{
	GENERATED_BODY()

public:
	/**
	 * Get item data for tooltip display
	 * @return Item data to show in tooltip
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Tooltip|Source")
	FItemUIData GetTooltipData() const;
    
	/**
	 * Check if tooltip should be shown for this widget
	 * @return True if tooltip can be displayed
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Tooltip|Source")
	bool CanShowTooltip() const;
    
	/**
	 * Get tooltip display delay in seconds
	 * @return Delay before showing tooltip
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Tooltip|Source")
	float GetTooltipDelay() const;
    
	/**
	 * Called when tooltip is shown for this widget
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Tooltip|Source")
	void OnTooltipShown();
    
	/**
	 * Called when tooltip is hidden
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Tooltip|Source")
	void OnTooltipHidden();
};

#endif // SUSPENSECORE_INTERFACES_UI_ISUSPENSECORETOOLTIPSOURCE_H