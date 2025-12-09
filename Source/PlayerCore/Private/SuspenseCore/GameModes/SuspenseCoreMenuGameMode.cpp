// SuspenseCoreMenuGameMode.cpp
// SuspenseCore - Clean Architecture PlayerCore
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/GameModes/SuspenseCoreMenuGameMode.h"
#include "SuspenseCore/Controllers/SuspenseCoreMenuPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

ASuspenseCoreMenuGameMode::ASuspenseCoreMenuGameMode()
{
	// No default pawn - this is a menu
	DefaultPawnClass = nullptr;

	// Use menu player controller
	PlayerControllerClass = ASuspenseCoreMenuPlayerController::StaticClass();

	// No HUD needed
	HUDClass = nullptr;

	// No player state needed for menu
	PlayerStateClass = nullptr;

	// Don't start players
	bStartPlayersAsSpectators = true;

	UE_LOG(LogTemp, Warning, TEXT("=== SuspenseCoreMenuGameMode CONSTRUCTOR ==="));
}

void ASuspenseCoreMenuGameMode::StartPlay()
{
	Super::StartPlay();

	UE_LOG(LogTemp, Warning, TEXT("=== SuspenseCoreMenuGameMode::StartPlay on map: %s ==="), *GetWorld()->GetMapName());

	// Widget creation to be added when UISystem module is enabled
}

void ASuspenseCoreMenuGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Setup menu input mode for the first player controller
	SetupMenuInputMode();
}

APawn* ASuspenseCoreMenuGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	// Don't spawn any pawn for menu
	return nullptr;
}

AActor* ASuspenseCoreMenuGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// No player start needed for menu
	return nullptr;
}

// Widget creation to be added when UISystem module is enabled

void ASuspenseCoreMenuGameMode::SetupMenuInputMode()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (PC)
	{
		// UI only input mode
		FInputModeUIOnly InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);

		// Show mouse cursor
		PC->bShowMouseCursor = true;
	}
}

void ASuspenseCoreMenuGameMode::ShowMainMenu()
{
	// Stub: Widget creation to be added when UISystem module is enabled
	SetupMenuInputMode();
	UE_LOG(LogTemp, Warning, TEXT("ShowMainMenu: UISystem not enabled, widget not available"));
}

void ASuspenseCoreMenuGameMode::HideMainMenu()
{
	// Stub: UISystem not enabled
}

void ASuspenseCoreMenuGameMode::TransitionToGameMap(FName MapName)
{
	FName TargetMap = MapName.IsNone() ? DefaultGameMapName : MapName;

	OnTransitionToGame(TargetMap);

	UGameplayStatics::OpenLevel(GetWorld(), TargetMap);
}

void ASuspenseCoreMenuGameMode::ReturnToMainMenu(UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return;
	}

	// Get the main menu map name from game instance or use default
	FName MainMenuMap = TEXT("MainMenuMap");

	// Open main menu level
	UGameplayStatics::OpenLevel(World, MainMenuMap);
}
