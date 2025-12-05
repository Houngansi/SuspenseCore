// SuspenseCoreGameGameMode.cpp
// SuspenseCore - Clean Architecture PlayerCore
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/GameModes/SuspenseCoreGameGameMode.h"
#include "SuspenseCore/Core/SuspenseCorePlayerController.h"
#include "SuspenseCore/Core/SuspenseCorePlayerState.h"
#include "SuspenseCore/Characters/SuspenseCoreCharacter.h"
#include "SuspenseCore/Save/SuspenseCoreSaveManager.h"
#include "SuspenseCore/Subsystems/SuspenseCoreMapTransitionSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameStateBase.h"

ASuspenseCoreGameGameMode::ASuspenseCoreGameGameMode()
{
	// Use SuspenseCore player controller with Enhanced Input
	PlayerControllerClass = ASuspenseCorePlayerController::StaticClass();

	// Use SuspenseCore player state with GAS integration
	PlayerStateClass = ASuspenseCorePlayerState::StaticClass();

	// Use SuspenseCore character
	DefaultPawnClass = ASuspenseCoreCharacter::StaticClass();

	// Enable seamless travel for smoother transitions
	bUseSeamlessTravel = false; // Disabled for simpler map loading

	UE_LOG(LogTemp, Warning, TEXT("=== SuspenseCoreGameGameMode CONSTRUCTOR ==="));
	UE_LOG(LogTemp, Warning, TEXT("  PlayerControllerClass: %s"), *PlayerControllerClass->GetName());
	UE_LOG(LogTemp, Warning, TEXT("  PlayerStateClass: %s"), *PlayerStateClass->GetName());
	UE_LOG(LogTemp, Warning, TEXT("  DefaultPawnClass: %s"), DefaultPawnClass ? *DefaultPawnClass->GetName() : TEXT("null"));
}

void ASuspenseCoreGameGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreGameGameMode::InitGame - Map: %s, Options: %s"), *MapName, *Options);

	// Try to get player ID from options first
	CurrentPlayerId = ParsePlayerIdFromOptions(Options);

	// If not in options, try to get from transition subsystem
	if (CurrentPlayerId.IsEmpty())
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (USuspenseCoreMapTransitionSubsystem* TransitionSubsystem = GI->GetSubsystem<USuspenseCoreMapTransitionSubsystem>())
			{
				CurrentPlayerId = TransitionSubsystem->GetCurrentPlayerId();
				UE_LOG(LogTemp, Log, TEXT("SuspenseCoreGameGameMode: Got PlayerId from TransitionSubsystem: %s"), *CurrentPlayerId);
			}
		}
	}

	if (!CurrentPlayerId.IsEmpty())
	{
		OnPlayerIdResolved(CurrentPlayerId);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreGameGameMode: No PlayerId found - save system may not work correctly"));
	}
}

void ASuspenseCoreGameGameMode::StartPlay()
{
	Super::StartPlay();

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreGameGameMode::StartPlay"));
}

void ASuspenseCoreGameGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Initialize save system
	if (bAutoStartSaveManager && !CurrentPlayerId.IsEmpty())
	{
		InitializeSaveSystem();
	}

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreGameGameMode::BeginPlay - Player: %s"), *CurrentPlayerId);
}

FString ASuspenseCoreGameGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	FString Result = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreGameGameMode::InitNewPlayer - Controller: %s"),
		NewPlayerController ? *NewPlayerController->GetName() : TEXT("null"));

	return Result;
}

void ASuspenseCoreGameGameMode::ReturnToMainMenu()
{
	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreGameGameMode: Returning to main menu"));

	// Save before leaving
	if (USuspenseCoreSaveManager* SaveManager = USuspenseCoreSaveManager::Get(this))
	{
		// Trigger auto-save before leaving
		SaveManager->TriggerAutoSave();
	}

	// Call Blueprint event
	OnReturnToMainMenu();

	// Clear transition data (we're going back to menu)
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USuspenseCoreMapTransitionSubsystem* TransitionSubsystem = GI->GetSubsystem<USuspenseCoreMapTransitionSubsystem>())
		{
			// Keep the player ID for re-selection in menu
			// TransitionSubsystem->ClearTransitionData();
		}
	}

	// Open main menu level
	UGameplayStatics::OpenLevel(GetWorld(), MainMenuMapName);
}

void ASuspenseCoreGameGameMode::InitializeSaveSystem()
{
	if (USuspenseCoreSaveManager* SaveManager = USuspenseCoreSaveManager::Get(this))
	{
		// Set current player
		SaveManager->SetCurrentPlayer(CurrentPlayerId);

		// Configure auto-save
		if (AutoSaveInterval > 0.0f)
		{
			SaveManager->SetAutoSaveEnabled(true);
			SaveManager->SetAutoSaveInterval(AutoSaveInterval);
		}

		UE_LOG(LogTemp, Log, TEXT("SuspenseCoreGameGameMode: SaveManager initialized for player %s"), *CurrentPlayerId);
	}
}

FString ASuspenseCoreGameGameMode::ParsePlayerIdFromOptions(const FString& Options)
{
	// Parse "?PlayerId=XXX" from options string
	FString PlayerId;

	// Try to extract PlayerId option
	if (UGameplayStatics::HasOption(Options, TEXT("PlayerId")))
	{
		PlayerId = UGameplayStatics::ParseOption(Options, TEXT("PlayerId"));
	}

	return PlayerId;
}
