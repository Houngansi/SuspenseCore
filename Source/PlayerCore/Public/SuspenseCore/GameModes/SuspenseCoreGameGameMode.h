// SuspenseCoreGameGameMode.h
// SuspenseCore - Clean Architecture PlayerCore
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SuspenseCoreGameGameMode.generated.h"

class ASuspenseCorePlayerController;
class ASuspenseCorePlayerState;
class ASuspenseCoreCharacter;

/**
 * ASuspenseCoreGameGameMode
 *
 * GameMode for actual gameplay (not menus).
 * Uses SuspenseCore Clean Architecture classes.
 *
 * Features:
 * - Uses SuspenseCorePlayerController with Enhanced Input
 * - Uses SuspenseCorePlayerState with GAS integration
 * - Spawns SuspenseCoreCharacter
 * - Supports pause menu and save system
 * - Handles return to main menu
 */
UCLASS()
class PLAYERCORE_API ASuspenseCoreGameGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASuspenseCoreGameGameMode();

	// ═══════════════════════════════════════════════════════════════════════════
	// AGameModeBase Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void BeginPlay() override;
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal = TEXT("")) override;

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Return to main menu map.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Game")
	void ReturnToMainMenu();

	/**
	 * Get the current player ID (from transition data).
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Game")
	FString GetCurrentPlayerId() const { return CurrentPlayerId; }

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Name of the main menu map */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Config")
	FName MainMenuMapName = TEXT("MainMenuMap");

	/** Auto-start save manager when game begins */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Config")
	bool bAutoStartSaveManager = true;

	/** Auto-save interval in seconds (0 = disabled) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Config")
	float AutoSaveInterval = 300.0f;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL
	// ═══════════════════════════════════════════════════════════════════════════

	/** Current player ID from map transition */
	FString CurrentPlayerId;

	/** Initialize save system for current player */
	void InitializeSaveSystem();

	/** Parse player ID from level options */
	FString ParsePlayerIdFromOptions(const FString& Options);

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when player ID is resolved */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Game")
	void OnPlayerIdResolved(const FString& PlayerId);

	/** Called when returning to main menu */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Game")
	void OnReturnToMainMenu();
};
