// SuspenseCoreMenuGameMode.h
// SuspenseCore - Clean Architecture PlayerCore
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SuspenseCoreMenuGameMode.generated.h"

/**
 * ASuspenseCoreMenuGameMode
 *
 * GameMode for menu screens (Main Menu, Character Select, etc.)
 *
 * Features:
 * - No pawn spawning
 * - UI-only input mode
 * - Creates and manages main menu widget
 * - Handles transitions between menu screens
 */
UCLASS()
class PLAYERCORE_API ASuspenseCoreMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASuspenseCoreMenuGameMode();

	// ═══════════════════════════════════════════════════════════════════════════
	// AGameModeBase Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void StartPlay() override;
	virtual void BeginPlay() override;

	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Show the main menu (widget creation to be added when UISystem enabled).
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Menu")
	void ShowMainMenu();

	/**
	 * Hide the main menu.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Menu")
	void HideMainMenu();

	/**
	 * Transition to game map.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Menu")
	void TransitionToGameMap(FName MapName);

	/**
	 * Return to main menu from game.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Menu")
	static void ReturnToMainMenu(UObject* WorldContextObject);

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Name of the main menu map (for returning from game) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Config")
	FName MainMenuMapName = TEXT("MainMenuMap");

	/** Name of the default game map */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Config")
	FName DefaultGameMapName = TEXT("GameMap");

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL
	// ═══════════════════════════════════════════════════════════════════════════

	/** Setup input mode for menu */
	void SetupMenuInputMode();

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when menu is shown */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Menu")
	void OnMenuShown();

	/** Called when transitioning to game */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Menu")
	void OnTransitionToGame(FName MapName);
};
