// SuspenseCoreMainMenuWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/SuspenseCoreMainMenuWidget.h"
#include "SuspenseCore/Widgets/SuspenseCoreRegistrationWidget.h"
#include "SuspenseCore/Widgets/SuspenseCorePlayerInfoWidget.h"
#include "SuspenseCore/Widgets/SuspenseCoreCharacterSelectWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Services/SuspenseCoreServiceLocator.h"
#include "SuspenseCore/Repository/SuspenseCoreFilePlayerRepository.h"
#include "SuspenseCore/Subsystems/SuspenseCoreMapTransitionSubsystem.h"
#include "SuspenseCore/Save/SuspenseCoreSaveManager.h"
#include "SuspenseCore/SuspenseCoreInterfaces.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Image.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
	/**
	 * Converts a Blueprint SoftObjectPath to a proper class path for ?game= URL parameter.
	 */
	FString NormalizeGameModeClassPath(const FString& InputPath)
	{
		if (InputPath.IsEmpty())
		{
			return InputPath;
		}

		FString Result = InputPath;

		// Check if it's in SoftObjectPath format: /Script/Engine.Blueprint'/Game/...'
		if (Result.Contains(TEXT("'/")) && Result.EndsWith(TEXT("'")))
		{
			int32 StartIndex = Result.Find(TEXT("'/"));
			if (StartIndex != INDEX_NONE)
			{
				Result = Result.Mid(StartIndex + 1);
				Result = Result.Left(Result.Len() - 1);
			}
		}

		// Ensure it ends with _C (class suffix for Blueprints)
		if (!Result.EndsWith(TEXT("_C")))
		{
			Result += TEXT("_C");
		}

		return Result;
	}
}

USuspenseCoreMainMenuWidget::USuspenseCoreMainMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USuspenseCoreMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Setup UI
	UpdateUIDisplay();
	SetupButtonBindings();
	SetupEventSubscriptions();
	SetupCharacterSelectBindings();

	// Initialize menu flow
	InitializeMenu();
}

void USuspenseCoreMainMenuWidget::NativeDestruct()
{
	TeardownEventSubscriptions();
	Super::NativeDestruct();
}

void USuspenseCoreMainMenuWidget::InitializeMenu()
{
	// Initialize repository first to ensure directory exists
	GetOrCreateRepository();

	// Check if we have existing players
	if (HasExistingPlayer())
	{
		// Show main menu panel (CharacterSelect is embedded there)
		ShowMainMenuPanel();
	}
	else
	{
		// No existing players - go directly to registration
		ShowRegistrationScreen();
	}
}

bool USuspenseCoreMainMenuWidget::HasExistingPlayer() const
{
	// Get repository and check for existing players
	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GetWorld());
	if (Manager)
	{
		USuspenseCoreServiceLocator* ServiceLocator = Manager->GetServiceLocator();
		if (ServiceLocator && ServiceLocator->HasService(TEXT("PlayerRepository")))
		{
			UObject* RepoObj = ServiceLocator->GetServiceByName(TEXT("PlayerRepository"));
			if (RepoObj && RepoObj->Implements<USuspenseCorePlayerRepository>())
			{
				ISuspenseCorePlayerRepository* Repo = Cast<ISuspenseCorePlayerRepository>(RepoObj);
				if (Repo)
				{
					TArray<FString> PlayerIds;
					Repo->GetAllPlayerIds(PlayerIds);
					return PlayerIds.Num() > 0;
				}
			}
		}
	}

	// Try to check directly via file repository
	USuspenseCoreFilePlayerRepository* FileRepo = NewObject<USuspenseCoreFilePlayerRepository>();
	if (FileRepo)
	{
		FileRepo->Initialize(TEXT("")); // Initialize with default path
		TArray<FString> PlayerIds;
		FileRepo->GetAllPlayerIds(PlayerIds);
		return PlayerIds.Num() > 0;
	}

	return false;
}

void USuspenseCoreMainMenuWidget::ShowCharacterSelectScreen()
{
	// CharacterSelect is now embedded in MainMenuPanel - just show that panel
	ShowMainMenuPanel();
}

void USuspenseCoreMainMenuWidget::ShowMainMenuPanel()
{
	// Refresh the character list if widget exists
	if (CharacterSelectWidget)
	{
		CharacterSelectWidget->RefreshCharacterList();
	}

	if (ScreenSwitcher)
	{
		ScreenSwitcher->SetActiveWidgetIndex(MainMenuScreenIndex);
		OnScreenChanged(MainMenuScreenIndex);
	}

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMainMenu: Showing main menu panel (with CharacterSelect)"));
}

void USuspenseCoreMainMenuWidget::ShowRegistrationScreen()
{
	if (ScreenSwitcher)
	{
		ScreenSwitcher->SetActiveWidgetIndex(RegistrationScreenIndex);
		OnScreenChanged(RegistrationScreenIndex);
	}

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMainMenu: Showing registration screen"));
}

void USuspenseCoreMainMenuWidget::ShowMainMenuScreen(const FString& PlayerId)
{
	// Update current player and display data (screen switch handled separately)
	SelectPlayer(PlayerId);

	// Ensure we're on the main menu panel
	if (ScreenSwitcher && ScreenSwitcher->GetActiveWidgetIndex() != MainMenuScreenIndex)
	{
		ScreenSwitcher->SetActiveWidgetIndex(MainMenuScreenIndex);
		OnScreenChanged(MainMenuScreenIndex);
	}

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMainMenu: Selected player %s"), *PlayerId);
}

void USuspenseCoreMainMenuWidget::SelectPlayer(const FString& PlayerId)
{
	CurrentPlayerId = PlayerId;

	// Load player data
	ISuspenseCorePlayerRepository* Repo = GetOrCreateRepository();
	if (Repo)
	{
		if (Repo->LoadPlayer(PlayerId, CachedPlayerData))
		{
			// Update PlayerInfoWidget
			if (PlayerInfoWidget)
			{
				PlayerInfoWidget->DisplayPlayerData(CachedPlayerData);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreMainMenu: Failed to load player %s"), *PlayerId);
		}
	}
}

void USuspenseCoreMainMenuWidget::TransitionToGame()
{
	if (CurrentPlayerId.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreMainMenu: Cannot transition to game - no player selected"));
		return;
	}

	// Publish event
	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GetWorld());
	if (Manager && Manager->GetEventBus())
	{
		FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
		EventData.SetString(FName("PlayerId"), CurrentPlayerId);
		EventData.SetString(FName("MapName"), GameMapName.ToString());

		Manager->GetEventBus()->Publish(
			FGameplayTag::RequestGameplayTag(FName("Event.UI.MainMenu.PlayClicked")),
			EventData
		);
	}

	// Setup SaveManager with current player before transition
	if (USuspenseCoreSaveManager* SaveManager = USuspenseCoreSaveManager::Get(this))
	{
		SaveManager->SetCurrentPlayer(CurrentPlayerId);
		SaveManager->SetProfileData(CachedPlayerData);
	}

	// Call Blueprint event
	OnTransitionToGame();

	// Use transition subsystem for proper state persistence
	if (USuspenseCoreMapTransitionSubsystem* TransitionSubsystem = USuspenseCoreMapTransitionSubsystem::Get(this))
	{
		// Configure GameMode paths for proper switching
		if (!GameGameModePath.IsEmpty())
		{
			TransitionSubsystem->SetGameGameModePath(GameGameModePath);
			UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMainMenu: Set GameGameModePath: %s"), *GameGameModePath);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreMainMenu: GameGameModePath not set! Configure it in Blueprint."));
		}

		if (!MenuGameModePath.IsEmpty())
		{
			TransitionSubsystem->SetMenuGameModePath(MenuGameModePath);
			UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMainMenu: Set MenuGameModePath: %s"), *MenuGameModePath);
		}

		TransitionSubsystem->TransitionToGameMap(CurrentPlayerId, GameMapName);
	}
	else
	{
		// Fallback: direct level open with forced GameMode
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreMainMenu: TransitionSubsystem not found, using direct OpenLevel"));
		UWorld* World = GetWorld();
		if (World)
		{
			FString Options = FString::Printf(TEXT("?PlayerId=%s"), *CurrentPlayerId);
			if (!GameGameModePath.IsEmpty())
			{
				FString NormalizedPath = NormalizeGameModeClassPath(GameGameModePath);
				Options += FString::Printf(TEXT("?game=%s"), *NormalizedPath);
			}
			UGameplayStatics::OpenLevel(World, GameMapName, true, Options);
		}
	}
}

void USuspenseCoreMainMenuWidget::SetupButtonBindings()
{
	if (PlayButton)
	{
		PlayButton->OnClicked.AddDynamic(this, &USuspenseCoreMainMenuWidget::OnPlayButtonClicked);
	}

	if (OperatorsButton)
	{
		OperatorsButton->OnClicked.AddDynamic(this, &USuspenseCoreMainMenuWidget::OnOperatorsButtonClicked);
		// Disable for now - future feature
		OperatorsButton->SetIsEnabled(false);
	}

	if (SettingsButton)
	{
		SettingsButton->OnClicked.AddDynamic(this, &USuspenseCoreMainMenuWidget::OnSettingsButtonClicked);
		// Disable for now - future feature
		SettingsButton->SetIsEnabled(false);
	}

	if (QuitButton)
	{
		QuitButton->OnClicked.AddDynamic(this, &USuspenseCoreMainMenuWidget::OnQuitButtonClicked);
	}
}

void USuspenseCoreMainMenuWidget::SetupEventSubscriptions()
{
	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GetWorld());
	if (!Manager)
	{
		return;
	}

	CachedEventBus = Manager->GetEventBus();
	if (!CachedEventBus.IsValid())
	{
		return;
	}

	// Subscribe to registration success events
	RegistrationEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("Event.UI.Registration.Success")),
		const_cast<USuspenseCoreMainMenuWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreMainMenuWidget::OnRegistrationSuccess),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to character select events
	CharacterSelectEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("Event.UI.CharacterSelect.Selected")),
		const_cast<USuspenseCoreMainMenuWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreMainMenuWidget::OnCharacterSelected),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to create new character events
	CreateNewCharacterEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("Event.UI.CharacterSelect.CreateNew")),
		const_cast<USuspenseCoreMainMenuWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreMainMenuWidget::OnCreateNewCharacter),
		ESuspenseCoreEventPriority::Normal
	);
}

void USuspenseCoreMainMenuWidget::TeardownEventSubscriptions()
{
	if (CachedEventBus.IsValid())
	{
		if (RegistrationEventHandle.IsValid())
		{
			CachedEventBus->Unsubscribe(RegistrationEventHandle);
		}
		if (CharacterSelectEventHandle.IsValid())
		{
			CachedEventBus->Unsubscribe(CharacterSelectEventHandle);
		}
		if (CreateNewCharacterEventHandle.IsValid())
		{
			CachedEventBus->Unsubscribe(CreateNewCharacterEventHandle);
		}
	}
}

void USuspenseCoreMainMenuWidget::SetupCharacterSelectBindings()
{
	// Direct delegate bindings - more reliable than EventBus for widget communication
	if (CharacterSelectWidget)
	{
		// Subscribe to character selection (Play button clicked)
		CharacterSelectWidget->OnCharacterSelectedDelegate.AddDynamic(
			this, &USuspenseCoreMainMenuWidget::OnCharacterSelectedDirect);

		// Subscribe to character highlight (entry clicked - updates PlayerInfo immediately)
		CharacterSelectWidget->OnCharacterHighlightedDelegate.AddDynamic(
			this, &USuspenseCoreMainMenuWidget::OnCharacterHighlightedDirect);

		// Subscribe to create new request
		CharacterSelectWidget->OnCreateNewRequestedDelegate.AddDynamic(
			this, &USuspenseCoreMainMenuWidget::OnCreateNewCharacterDirect);

		// Subscribe to delete events
		CharacterSelectWidget->OnCharacterDeletedDelegate.AddDynamic(
			this, &USuspenseCoreMainMenuWidget::OnCharacterDeletedDirect);

		UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMainMenu: Bound to CharacterSelectWidget delegates"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreMainMenu: CharacterSelectWidget not found for direct binding"));
	}
}

void USuspenseCoreMainMenuWidget::OnCharacterSelectedDirect(const FString& PlayerId, const FSuspenseCoreCharacterEntry& Entry)
{
	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMainMenu: Character selected (direct): %s"), *PlayerId);

	if (!PlayerId.IsEmpty())
	{
		ShowMainMenuScreen(PlayerId);
	}
}

void USuspenseCoreMainMenuWidget::OnCreateNewCharacterDirect()
{
	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMainMenu: Create new character (direct)"));
	ShowRegistrationScreen();
}

void USuspenseCoreMainMenuWidget::OnCharacterDeletedDirect(const FString& PlayerId)
{
	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMainMenu: Character deleted (direct): %s"), *PlayerId);

	// If deleted the current player, clear the selection and PlayerInfo
	if (CurrentPlayerId == PlayerId)
	{
		CurrentPlayerId.Empty();
		if (PlayerInfoWidget)
		{
			PlayerInfoWidget->ClearDisplay();
		}
	}

	// CharacterSelectWidget already refreshes its list, so nothing else needed here
}

void USuspenseCoreMainMenuWidget::OnCharacterHighlightedDirect(const FString& PlayerId, const FSuspenseCoreCharacterEntry& Entry)
{
	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMainMenu: Character highlighted (direct): %s (%s)"),
		*Entry.DisplayName, *PlayerId);

	// Update PlayerInfo immediately when character is highlighted
	if (!PlayerId.IsEmpty())
	{
		SelectPlayer(PlayerId);
	}
}

ISuspenseCorePlayerRepository* USuspenseCoreMainMenuWidget::GetRepository()
{
	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GetWorld());
	if (Manager)
	{
		USuspenseCoreServiceLocator* ServiceLocator = Manager->GetServiceLocator();
		if (ServiceLocator && ServiceLocator->HasService(TEXT("PlayerRepository")))
		{
			UObject* RepoObj = ServiceLocator->GetServiceByName(TEXT("PlayerRepository"));
			if (RepoObj && RepoObj->Implements<USuspenseCorePlayerRepository>())
			{
				return Cast<ISuspenseCorePlayerRepository>(RepoObj);
			}
		}
	}
	return nullptr;
}

ISuspenseCorePlayerRepository* USuspenseCoreMainMenuWidget::GetOrCreateRepository()
{
	ISuspenseCorePlayerRepository* Repo = GetRepository();
	if (Repo)
	{
		return Repo;
	}

	// Create and register file repository
	USuspenseCoreFilePlayerRepository* FileRepo = NewObject<USuspenseCoreFilePlayerRepository>(this);

	// IMPORTANT: Initialize repository with default path ([Project]/Saved/Players/)
	FileRepo->Initialize(TEXT(""));

	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GetWorld());
	if (Manager)
	{
		USuspenseCoreServiceLocator* ServiceLocator = Manager->GetServiceLocator();
		if (ServiceLocator)
		{
			ServiceLocator->RegisterServiceByName(TEXT("PlayerRepository"), FileRepo);
		}
	}

	return FileRepo;
}

void USuspenseCoreMainMenuWidget::UpdateUIDisplay()
{
	if (GameTitleText)
	{
		GameTitleText->SetText(GameTitle);
	}

	if (VersionText)
	{
		VersionText->SetText(VersionString);
	}

	if (PlayButtonText)
	{
		PlayButtonText->SetText(FText::FromString(TEXT("PLAY")));
	}
}

void USuspenseCoreMainMenuWidget::OnRegistrationSuccess(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// Get player ID from event data
	FString PlayerId = EventData.GetString(FName("PlayerId"));

	if (!PlayerId.IsEmpty())
	{
		// Notify Blueprint
		OnRegistrationComplete(PlayerId);

		// Transition to main menu
		ShowMainMenuScreen(PlayerId);

		UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMainMenu: Registration successful for %s, transitioning to main menu"), *PlayerId);
	}
}

void USuspenseCoreMainMenuWidget::OnCharacterSelected(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// Get player ID from event data
	FString PlayerId = EventData.GetString(FName("PlayerId"));

	if (!PlayerId.IsEmpty())
	{
		// Transition to main menu with selected player
		ShowMainMenuScreen(PlayerId);

		UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMainMenu: Character selected %s, transitioning to main menu"), *PlayerId);
	}
}

void USuspenseCoreMainMenuWidget::OnCreateNewCharacter(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// Navigate to registration screen
	ShowRegistrationScreen();

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMainMenu: Create new character requested, showing registration"));
}

void USuspenseCoreMainMenuWidget::OnPlayButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMainMenu: Play button clicked"));
	TransitionToGame();
}

void USuspenseCoreMainMenuWidget::OnOperatorsButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMainMenu: Operators button clicked (not implemented)"));

	// Future: Open character select map
	// UGameplayStatics::OpenLevel(GetWorld(), CharacterSelectMapName);
}

void USuspenseCoreMainMenuWidget::OnSettingsButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMainMenu: Settings button clicked (not implemented)"));

	// Future: Show settings panel
}

void USuspenseCoreMainMenuWidget::OnQuitButtonClicked()
{
	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMainMenu: Quit button clicked"));

	// Quit the game
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		UKismetSystemLibrary::QuitGame(GetWorld(), PC, EQuitPreference::Quit, false);
	}
}
