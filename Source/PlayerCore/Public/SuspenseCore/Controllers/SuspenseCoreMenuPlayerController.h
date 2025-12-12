// SuspenseCoreMenuPlayerController.h
// SuspenseCore - Clean Architecture PlayerCore
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SuspenseCoreMenuPlayerController.generated.h"

class ACameraActor;
class UUserWidget;

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
 * - Auto-finds and uses CameraActor in level (tag: "MenuCamera")
 * - Creates and manages MainMenuWidget
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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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

	/**
	 * Find and set view target to camera actor in level.
	 * Searches for CameraActor with tag "MenuCamera" or first CameraActor found.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Menu")
	void SetViewToLevelCamera();

	/**
	 * Set view target to specific camera actor.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Menu")
	void SetViewToCamera(ACameraActor* CameraActor);

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API - MAIN MENU WIDGET
	// ═══════════════════════════════════════════════════════════════════════════

	/** Get main menu widget */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Menu|UI")
	UUserWidget* GetMainMenuWidget() const { return MainMenuWidget; }

	/** Show main menu widget */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Menu|UI")
	void ShowMainMenu();

	/** Hide main menu widget */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Menu|UI")
	void HideMainMenu();

	/** Check if main menu is visible */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Menu|UI")
	bool IsMainMenuVisible() const;

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

	/** Should auto-find and use level camera on begin play? */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Config")
	bool bAutoSetLevelCamera = true;

	/** Tag to search for when finding menu camera (default: "MenuCamera") */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Config")
	FName MenuCameraTag = TEXT("MenuCamera");

	// ═══════════════════════════════════════════════════════════════════════════
	// UI CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Main menu widget class to spawn */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|UI")
	TSubclassOf<UUserWidget> MainMenuWidgetClass;

	/** Spawned main menu widget */
	UPROPERTY()
	UUserWidget* MainMenuWidget = nullptr;

	/** Should auto-create main menu widget on begin play? */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|UI")
	bool bAutoCreateMainMenu = true;

	// ═══════════════════════════════════════════════════════════════════════════
	// INPUT HANDLERS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Handle escape key press */
	void OnEscapePressed();

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL METHODS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Find camera actor in level by tag or first available */
	ACameraActor* FindLevelCamera();

	/** Create main menu widget */
	void CreateMainMenuWidget();

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when escape is pressed */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Menu")
	void OnEscapePressedEvent();

	/** Called when returning to main menu */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Menu")
	void OnReturnToMainMenu();

	/** Called when camera view target is set */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Menu")
	void OnCameraSet(ACameraActor* Camera);

	/** Called when main menu widget is created */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Menu|UI")
	void OnMainMenuCreated();
};
