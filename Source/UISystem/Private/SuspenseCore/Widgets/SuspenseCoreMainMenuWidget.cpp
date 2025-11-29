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
#include "SuspenseCore/SuspenseCoreInterfaces.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Image.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

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
		// Show character select screen (player can choose existing or create new)
		ShowCharacterSelectScreen();
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
	// Refresh the character list if widget exists
	if (CharacterSelectWidget)
	{
		CharacterSelectWidget->RefreshCharacterList();
	}

	if (ScreenSwitcher)
	{
		ScreenSwitcher->SetActiveWidgetIndex(CharacterSelectScreenIndex);
		OnScreenChanged(CharacterSelectScreenIndex);
	}

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMainMenu: Showing character select screen"));
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

	// Switch to main menu screen
	if (ScreenSwitcher)
	{
		ScreenSwitcher->SetActiveWidgetIndex(MainMenuScreenIndex);
		OnScreenChanged(MainMenuScreenIndex);
	}

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMainMenu: Showing main menu for player %s"), *PlayerId);
}

void USuspenseCoreMainMenuWidget::TransitionToGame()
{
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

	// Call Blueprint event
	OnTransitionToGame();

	// Open the game map
	UWorld* World = GetWorld();
	if (World)
	{
		UGameplayStatics::OpenLevel(World, GameMapName);
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
