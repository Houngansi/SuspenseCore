// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "ISuspenseScreen.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class USuspenseScreenInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for screen/panel widgets
 * Provides standardized lifecycle for content panels
 */
class BRIDGESYSTEM_API ISuspenseScreen
{
	GENERATED_BODY()

public:
	/**
	 * Gets the screen tag identifier
	 * @return Screen tag
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Screen")
	FGameplayTag GetScreenTag() const;

	/**
	 * Called when screen becomes active
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Screen")
	void OnScreenActivated();

	/**
	 * Called when screen becomes inactive
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Screen")
	void OnScreenDeactivated();

	/**
	 * Checks if screen is currently active
	 * @return True if active
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Screen")
	bool IsScreenActive() const;

	/**
	 * Updates screen content
	 * @param DeltaTime Time since last update
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Screen")
	void UpdateScreen(float DeltaTime);

	/**
	 * Refreshes screen content from data sources
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Screen")
	void RefreshScreenContent();

	/**
	 * Gets whether screen requires tick updates
	 * @return True if screen should tick
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Screen")
	bool RequiresTick() const;
};