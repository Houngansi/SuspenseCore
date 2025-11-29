// SuspenseCoreMenuPlayerController.cpp
// SuspenseCore - Clean Architecture PlayerCore
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Controllers/SuspenseCoreMenuPlayerController.h"
#include "Kismet/GameplayStatics.h"

ASuspenseCoreMenuPlayerController::ASuspenseCoreMenuPlayerController()
{
	// Show cursor by default for menu
	bShowMouseCursor = true;

	// Enable click events
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void ASuspenseCoreMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (bShowCursorOnStart)
	{
		bShowMouseCursor = true;
	}

	if (bUIOnlyModeOnStart)
	{
		SetUIOnlyMode();
	}
}

void ASuspenseCoreMenuPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent)
	{
		// Bind Escape key
		InputComponent->BindAction("Escape", IE_Pressed, this, &ASuspenseCoreMenuPlayerController::OnEscapePressed);

		// Also bind to Gamepad back button
		InputComponent->BindAction("Back", IE_Pressed, this, &ASuspenseCoreMenuPlayerController::OnEscapePressed);
	}
}

void ASuspenseCoreMenuPlayerController::SetUIOnlyMode()
{
	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

void ASuspenseCoreMenuPlayerController::SetGameAndUIMode()
{
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

void ASuspenseCoreMenuPlayerController::ReturnToMainMenu()
{
	OnReturnToMainMenu();
	UGameplayStatics::OpenLevel(GetWorld(), MainMenuMapName);
}

void ASuspenseCoreMenuPlayerController::OnEscapePressed()
{
	// Call Blueprint event first
	OnEscapePressedEvent();

	// Default behavior could be to go back or quit
	// This is typically overridden in Blueprint
}
