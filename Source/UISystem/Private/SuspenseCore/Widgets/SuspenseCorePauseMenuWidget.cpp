// SuspenseCorePauseMenuWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/SuspenseCorePauseMenuWidget.h"
#include "SuspenseCore/Save/SuspenseCoreSaveManager.h"
#include "SuspenseCore/Subsystems/SuspenseCoreMapTransitionSubsystem.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCorePauseMenu, Log, All);

USuspenseCorePauseMenuWidget::USuspenseCorePauseMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ═══════════════════════════════════════════════════════════════════════════════
// UUserWidget
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCorePauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetupButtonBindings();
	UpdateUIDisplay();

	// Start hidden
	SetVisibility(ESlateVisibility::Collapsed);
	bIsVisible = false;

	// Subscribe to save events
	if (USuspenseCoreSaveManager* SaveMgr = GetSaveManager())
	{
		SaveMgr->OnSaveCompleted.AddDynamic(this, &USuspenseCorePauseMenuWidget::OnSaveCompleted);
		SaveMgr->OnLoadCompleted.AddDynamic(this, &USuspenseCorePauseMenuWidget::OnLoadCompleted);
	}

	// Make focusable for key input
	SetIsFocusable(true);
}

void USuspenseCorePauseMenuWidget::NativeDestruct()
{
	// Unsubscribe from save events
	if (USuspenseCoreSaveManager* SaveMgr = GetSaveManager())
	{
		SaveMgr->OnSaveCompleted.RemoveDynamic(this, &USuspenseCorePauseMenuWidget::OnSaveCompleted);
		SaveMgr->OnLoadCompleted.RemoveDynamic(this, &USuspenseCorePauseMenuWidget::OnLoadCompleted);
	}

	// Clear timer
	if (StatusHideTimer.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(StatusHideTimer);
		}
	}

	Super::NativeDestruct();
}

FReply USuspenseCorePauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// ESC to close menu
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (bIsVisible)
		{
			HidePauseMenu();
			return FReply::Handled();
		}
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCorePauseMenuWidget::ShowPauseMenu()
{
	if (bIsVisible)
	{
		return;
	}

	bIsVisible = true;
	SetVisibility(ESlateVisibility::Visible);
	SetGamePaused(true);

	// Focus the widget
	SetFocus();

	// Set input mode
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(true);
	}

	OnMenuShown();
	OnPauseMenuShown.Broadcast();

	UE_LOG(LogSuspenseCorePauseMenu, Log, TEXT("Pause menu shown"));
}

void USuspenseCorePauseMenuWidget::HidePauseMenu()
{
	if (!bIsVisible)
	{
		return;
	}

	bIsVisible = false;
	SetVisibility(ESlateVisibility::Collapsed);
	SetGamePaused(false);

	// Restore input mode
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(false);
	}

	OnMenuHidden();
	OnPauseMenuHidden.Broadcast();

	UE_LOG(LogSuspenseCorePauseMenu, Log, TEXT("Pause menu hidden"));
}

void USuspenseCorePauseMenuWidget::TogglePauseMenu()
{
	if (bIsVisible)
	{
		HidePauseMenu();
	}
	else
	{
		ShowPauseMenu();
	}
}

void USuspenseCorePauseMenuWidget::QuickSave()
{
	if (USuspenseCoreSaveManager* SaveMgr = GetSaveManager())
	{
		ShowSaveStatus(true);
		SaveMgr->QuickSave();
	}
}

void USuspenseCorePauseMenuWidget::QuickLoad()
{
	if (USuspenseCoreSaveManager* SaveMgr = GetSaveManager())
	{
		SaveMgr->QuickLoad();
		HidePauseMenu(); // Hide menu after load
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCorePauseMenuWidget::SetupButtonBindings()
{
	if (ContinueButton)
	{
		ContinueButton->OnClicked.AddDynamic(this, &USuspenseCorePauseMenuWidget::OnContinueButtonClicked);
	}

	if (SaveButton)
	{
		SaveButton->OnClicked.AddDynamic(this, &USuspenseCorePauseMenuWidget::OnSaveButtonClicked);
	}

	if (LoadButton)
	{
		LoadButton->OnClicked.AddDynamic(this, &USuspenseCorePauseMenuWidget::OnLoadButtonClicked);
	}

	if (ExitToLobbyButton)
	{
		ExitToLobbyButton->OnClicked.AddDynamic(this, &USuspenseCorePauseMenuWidget::OnExitToLobbyButtonClicked);
	}

	if (QuitButton)
	{
		QuitButton->OnClicked.AddDynamic(this, &USuspenseCorePauseMenuWidget::OnQuitButtonClicked);
	}
}

void USuspenseCorePauseMenuWidget::UpdateUIDisplay()
{
	if (TitleText)
	{
		TitleText->SetText(Title);
	}

	if (ContinueButtonText)
	{
		ContinueButtonText->SetText(ContinueText);
	}

	if (SaveButtonText)
	{
		SaveButtonText->SetText(SaveText);
	}

	if (LoadButtonText)
	{
		LoadButtonText->SetText(LoadText);
	}

	if (ExitToLobbyButtonText)
	{
		ExitToLobbyButtonText->SetText(ExitToLobbyText);
	}

	if (QuitButtonText)
	{
		QuitButtonText->SetText(QuitText);
	}

	// Hide status initially
	if (SaveStatusText)
	{
		SaveStatusText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void USuspenseCorePauseMenuWidget::SetGamePaused(bool bPaused)
{
	UGameplayStatics::SetGamePaused(GetWorld(), bPaused);
}

void USuspenseCorePauseMenuWidget::ShowSaveStatus(bool bSaving)
{
	if (SaveStatusText)
	{
		SaveStatusText->SetText(bSaving ? SavingText : SavedText);
		SaveStatusText->SetVisibility(ESlateVisibility::Visible);
	}
}

void USuspenseCorePauseMenuWidget::HideSaveStatusAfterDelay()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			StatusHideTimer,
			[this]()
			{
				if (SaveStatusText)
				{
					SaveStatusText->SetVisibility(ESlateVisibility::Collapsed);
				}
			},
			2.0f, // 2 second delay
			false
		);
	}
}

USuspenseCoreSaveManager* USuspenseCorePauseMenuWidget::GetSaveManager()
{
	if (!CachedSaveManager.IsValid())
	{
		CachedSaveManager = USuspenseCoreSaveManager::Get(this);
	}
	return CachedSaveManager.Get();
}

// ═══════════════════════════════════════════════════════════════════════════════
// BUTTON HANDLERS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCorePauseMenuWidget::OnContinueButtonClicked()
{
	UE_LOG(LogSuspenseCorePauseMenu, Log, TEXT("Continue clicked"));
	HidePauseMenu();
}

void USuspenseCorePauseMenuWidget::OnSaveButtonClicked()
{
	UE_LOG(LogSuspenseCorePauseMenu, Log, TEXT("Save clicked"));
	QuickSave();
}

void USuspenseCorePauseMenuWidget::OnLoadButtonClicked()
{
	UE_LOG(LogSuspenseCorePauseMenu, Log, TEXT("Load clicked"));

	if (USuspenseCoreSaveManager* SaveMgr = GetSaveManager())
	{
		if (SaveMgr->HasQuickSave())
		{
			QuickLoad();
		}
		else
		{
			UE_LOG(LogSuspenseCorePauseMenu, Warning, TEXT("No quick save to load"));
			// TODO: Show "No save found" message
		}
	}
}

void USuspenseCorePauseMenuWidget::OnExitToLobbyButtonClicked()
{
	UE_LOG(LogSuspenseCorePauseMenu, Log, TEXT("Exit to lobby clicked"));

	// Auto-save before leaving
	if (USuspenseCoreSaveManager* SaveMgr = GetSaveManager())
	{
		SaveMgr->TriggerAutoSave();
	}

	OnExitToLobby();

	// Unpause before loading new level
	SetGamePaused(false);

	// Use transition subsystem for proper state handling
	if (USuspenseCoreMapTransitionSubsystem* TransitionSubsystem = USuspenseCoreMapTransitionSubsystem::Get(this))
	{
		// Configure GameMode paths for proper switching
		if (!MenuGameModePath.IsEmpty())
		{
			TransitionSubsystem->SetMenuGameModePath(MenuGameModePath);
			UE_LOG(LogSuspenseCorePauseMenu, Log, TEXT("Set MenuGameModePath: %s"), *MenuGameModePath);
		}
		else
		{
			UE_LOG(LogSuspenseCorePauseMenu, Warning, TEXT("MenuGameModePath not set! Configure it in Blueprint."));
		}

		if (!GameGameModePath.IsEmpty())
		{
			TransitionSubsystem->SetGameGameModePath(GameGameModePath);
		}

		TransitionSubsystem->TransitionToMainMenu(LobbyMapName);
	}
	else
	{
		// Fallback: direct level open with forced GameMode
		UE_LOG(LogSuspenseCorePauseMenu, Warning, TEXT("TransitionSubsystem not found, using direct OpenLevel"));
		FString Options;
		if (!MenuGameModePath.IsEmpty())
		{
			Options = FString::Printf(TEXT("?game=%s"), *MenuGameModePath);
		}
		UGameplayStatics::OpenLevel(GetWorld(), LobbyMapName, true, Options);
	}
}

void USuspenseCorePauseMenuWidget::OnQuitButtonClicked()
{
	UE_LOG(LogSuspenseCorePauseMenu, Log, TEXT("Quit clicked"));

	OnQuitGame();

	// Quit the game
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		UKismetSystemLibrary::QuitGame(GetWorld(), PC, EQuitPreference::Quit, false);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// SAVE CALLBACKS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCorePauseMenuWidget::OnSaveCompleted(bool bSuccess, const FString& ErrorMessage)
{
	if (bSuccess)
	{
		ShowSaveStatus(false); // Show "Saved!"
		HideSaveStatusAfterDelay();
	}
	else
	{
		UE_LOG(LogSuspenseCorePauseMenu, Error, TEXT("Save failed: %s"), *ErrorMessage);
		if (SaveStatusText)
		{
			SaveStatusText->SetText(FText::FromString(TEXT("Save Failed!")));
			HideSaveStatusAfterDelay();
		}
	}
}

void USuspenseCorePauseMenuWidget::OnLoadCompleted(bool bSuccess, const FString& ErrorMessage)
{
	if (!bSuccess)
	{
		UE_LOG(LogSuspenseCorePauseMenu, Error, TEXT("Load failed: %s"), *ErrorMessage);
		// TODO: Show error message to user
	}
}
