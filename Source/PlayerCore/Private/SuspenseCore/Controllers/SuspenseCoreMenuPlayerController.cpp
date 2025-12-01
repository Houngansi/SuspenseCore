// SuspenseCoreMenuPlayerController.cpp
// SuspenseCore - Clean Architecture PlayerCore
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Controllers/SuspenseCoreMenuPlayerController.h"
#include "Camera/CameraActor.h"
#include "EngineUtils.h"
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

	UE_LOG(LogTemp, Log, TEXT("[MenuPlayerController] BeginPlay on map: %s"), *GetWorld()->GetMapName());

	if (bShowCursorOnStart)
	{
		bShowMouseCursor = true;
	}

	if (bUIOnlyModeOnStart)
	{
		SetUIOnlyMode();
	}

	// Auto-set view to level camera
	if (bAutoSetLevelCamera)
	{
		SetViewToLevelCamera();
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

void ASuspenseCoreMenuPlayerController::SetViewToLevelCamera()
{
	ACameraActor* Camera = FindLevelCamera();
	if (Camera)
	{
		SetViewToCamera(Camera);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[MenuPlayerController] No CameraActor found in level. Add a CameraActor with tag '%s' or any CameraActor."), *MenuCameraTag.ToString());
	}
}

void ASuspenseCoreMenuPlayerController::SetViewToCamera(ACameraActor* CameraActor)
{
	if (!CameraActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MenuPlayerController] SetViewToCamera: CameraActor is null"));
		return;
	}

	// Set view target with blend
	SetViewTargetWithBlend(CameraActor, 0.0f);

	UE_LOG(LogTemp, Log, TEXT("[MenuPlayerController] View target set to camera: %s"), *CameraActor->GetName());

	// Notify Blueprint
	OnCameraSet(CameraActor);
}

ACameraActor* ASuspenseCoreMenuPlayerController::FindLevelCamera()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// First, try to find camera with specific tag
	if (!MenuCameraTag.IsNone())
	{
		TArray<AActor*> TaggedActors;
		UGameplayStatics::GetAllActorsWithTag(World, MenuCameraTag, TaggedActors);

		for (AActor* Actor : TaggedActors)
		{
			if (ACameraActor* Camera = Cast<ACameraActor>(Actor))
			{
				UE_LOG(LogTemp, Log, TEXT("[MenuPlayerController] Found tagged camera: %s"), *Camera->GetName());
				return Camera;
			}
		}
	}

	// Fallback: find any CameraActor in the level
	for (TActorIterator<ACameraActor> It(World); It; ++It)
	{
		ACameraActor* Camera = *It;
		if (Camera)
		{
			UE_LOG(LogTemp, Log, TEXT("[MenuPlayerController] Found camera (no tag): %s"), *Camera->GetName());
			return Camera;
		}
	}

	return nullptr;
}
