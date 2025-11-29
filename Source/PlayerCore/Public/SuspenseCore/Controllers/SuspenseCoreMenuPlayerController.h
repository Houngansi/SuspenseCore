// SuspenseCoreMenuPlayerController.h
// SuspenseCore - Clean Architecture PlayerCore
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SuspenseCoreMenuPlayerController.generated.h"

/**
 * ASuspenseCoreMenuPlayerController
 *
 * PlayerController for menu screens.
 *
 * Features:
 * - Shows mouse cursor by default
 * - UI-only input mode
 * - No pawn control
 * - Handles Escape key for back/quit actions
 */
UCLASS()
class PLAYERCORE_API ASuspenseCoreMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASuspenseCoreMenuPlayerController();

	// ═══════════════════════════════════════════════════════════════════════════
	// APlayerController Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Set to UI only input mode with mouse cursor.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Menu")
	void SetUIOnlyMode();

	/**
	 * Set to game and UI input mode.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Menu")
	void SetGameAndUIMode();

	/**
	 * Return to main menu from any map.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Menu")
	void ReturnToMainMenu();

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Main menu map name */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Config")
	FName MainMenuMapName = TEXT("MainMenuMap");

	/** Should show cursor on begin play? */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Config")
	bool bShowCursorOnStart = true;

	/** Should set UI only mode on begin play? */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Config")
	bool bUIOnlyModeOnStart = true;

	// ═══════════════════════════════════════════════════════════════════════════
	// INPUT HANDLERS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Handle escape key press */
	void OnEscapePressed();

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when escape is pressed */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Menu")
	void OnEscapePressedEvent();

	/** Called when returning to main menu */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Menu")
	void OnReturnToMainMenu();
};
