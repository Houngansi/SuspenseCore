// SuspenseCoreUIController.h
// SuspenseCore - Clean Architecture Bridge Layer
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SuspenseCoreUIController.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreUIController : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISuspenseCoreUIController
 *
 * Interface for centralized cursor/UI mode management.
 * Implemented by PlayerController to provide Push/Pop UI mode pattern.
 *
 * Usage in widgets:
 *   if (ISuspenseCoreUIController* UIController = Cast<ISuspenseCoreUIController>(GetOwningPlayer()))
 *   {
 *       UIController->PushUIMode(TEXT("MyMenu"));
 *   }
 */
class BRIDGESYSTEM_API ISuspenseCoreUIController
{
	GENERATED_BODY()

public:
	/**
	 * Push UI mode - shows cursor and enables UI input.
	 * Call when opening any UI widget that needs cursor.
	 * @param Reason - Debug identifier for this UI push (e.g., "PauseMenu", "Inventory")
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|UI")
	void PushUIMode(const FString& Reason);

	/**
	 * Pop UI mode - potentially hides cursor if no UI is active.
	 * Call when closing UI widget.
	 * @param Reason - Must match the reason used in PushUIMode
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|UI")
	void PopUIMode(const FString& Reason);

	/**
	 * Force set cursor visibility (use sparingly, prefer Push/Pop).
	 * @param bShowCursor - Whether to show cursor
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|UI")
	void SetCursorVisible(bool bShowCursor);

	/**
	 * Check if any UI is currently active.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|UI")
	bool IsUIActive() const;
};
