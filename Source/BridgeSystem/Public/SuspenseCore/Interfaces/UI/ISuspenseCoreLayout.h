// ISuspenseCoreLayout.h
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "ISuspenseCoreLayout.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreLayout : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for layout widgets that can contain multiple child widgets
 * Allows creating complex UI compositions while keeping widgets independent
 */
class BRIDGESYSTEM_API ISuspenseCoreLayout
{
	GENERATED_BODY()

public:
	/**
	 * Add widget to layout
	 * @param Widget Widget to add
	 * @param SlotTag Optional tag for specific slot placement
	 * @return True if successfully added
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Layout")
	bool AddWidgetToLayout(UUserWidget* Widget, FGameplayTag SlotTag);

	/**
	 * Remove widget from layout
	 * @param Widget Widget to remove
	 * @return True if successfully removed
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Layout")
	bool RemoveWidgetFromLayout(UUserWidget* Widget);

	/**
	 * Get all widgets in layout
	 * @return Array of widgets
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Layout")
	TArray<UUserWidget*> GetLayoutWidgets() const;

	/**
	 * Clear all widgets from layout
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Layout")
	void ClearLayout();

	/**
	 * Refresh layout arrangement
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Layout")
	void RefreshLayout();
};

