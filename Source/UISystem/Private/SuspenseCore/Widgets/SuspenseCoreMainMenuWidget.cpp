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
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
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

	// Setup EventBus subscriptions (primary communication method per architecture docs)
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
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.MainMenu.PlayClicked")),
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
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.Registration.Success")),
		const_cast<USuspenseCoreMainMenuWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreMainMenuWidget::OnRegistrationSuccess),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to character select events
	CharacterSelectEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.CharacterSelect.Selected")),
		const_cast<USuspenseCoreMainMenuWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreMainMenuWidget::OnCharacterSelected),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to create new character events
	CreateNewCharacterEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.CharacterSelect.CreateNew")),
		const_cast<USuspenseCoreMainMenuWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreMainMenuWidget::OnCreateNewCharacter),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to character highlighted events (for PlayerInfo updates)
	CharacterHighlightedEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.CharacterSelect.Highlighted")),
		const_cast<USuspenseCoreMainMenuWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreMainMenuWidget::OnCharacterHighlighted),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to character deleted events
	CharacterDeletedEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.CharacterSelect.Deleted")),
		const_cast<USuspenseCoreMainMenuWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreMainMenuWidget::OnCharacterDeleted),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to render target ready events for character preview
	RenderTargetReadyEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.RenderTargetReady")),
		const_cast<USuspenseCoreMainMenuWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreMainMenuWidget::OnRenderTargetReady),
		ESuspenseCoreEventPriority::Normal
	);

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMainMenu: EventBus subscriptions established"));
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
		if (CharacterHighlightedEventHandle.IsValid())
		{
			CachedEventBus->Unsubscribe(CharacterHighlightedEventHandle);
		}
		if (CharacterDeletedEventHandle.IsValid())
		{
			CachedEventBus->Unsubscribe(CharacterDeletedEventHandle);
		}
		if (RenderTargetReadyEventHandle.IsValid())
		{
			CachedEventBus->Unsubscribe(RenderTargetReadyEventHandle);
		}
	}

	// Request capture disable via EventBus (no direct dependency on character)
	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GetWorld());
	if (Manager && Manager->GetEventBus())
	{
		FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
		EventData.SetBool(FName("Enabled"), false);
		Manager->GetEventBus()->Publish(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.CharacterPreview.RequestCapture")),
			EventData
		);
	}
}

// NOTE: Direct delegate bindings removed - all widget communication via EventBus per architecture docs
// See: Source/SuspenseCore/Documentation/Architecture/Planning/EventSystemMigration.md

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

		// Refresh character list to include the newly created character
		if (CharacterSelectWidget)
		{
			CharacterSelectWidget->RefreshCharacterList();
			// Explicitly highlight the new character in the list
			CharacterSelectWidget->HighlightCharacter(PlayerId);
		}

		// Transition to main menu and select the new player
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

void USuspenseCoreMainMenuWidget::OnCharacterHighlighted(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// Get player ID from event data
	FString PlayerId = EventData.GetString(FName("PlayerId"));

	if (!PlayerId.IsEmpty())
	{
		// Update PlayerInfo immediately when character is highlighted
		SelectPlayer(PlayerId);

		UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMainMenu: Character highlighted %s, updating PlayerInfo"), *PlayerId);
	}
}

void USuspenseCoreMainMenuWidget::OnCharacterDeleted(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// Get player ID from event data
	FString PlayerId = EventData.GetString(FName("PlayerId"));

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMainMenu: Character deleted %s"), *PlayerId);

	// If deleted the current player, clear the selection and PlayerInfo
	if (CurrentPlayerId == PlayerId)
	{
		CurrentPlayerId.Empty();
		if (PlayerInfoWidget)
		{
			PlayerInfoWidget->ClearDisplay();
		}
	}

	// CharacterSelectWidget already refreshes its list via EventBus
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

// ═══════════════════════════════════════════════════════════════════════════════
// CHARACTER PREVIEW (Render Target via EventBus - no direct module dependency)
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreMainMenuWidget::UpdateCharacterPreviewImage(UTextureRenderTarget2D* RenderTarget)
{
	if (!CharacterPreviewImage || !RenderTarget)
	{
		return;
	}

	// Cache render target
	CachedRenderTarget = RenderTarget;

	// Create or update dynamic material instance
	if (CharacterPreviewBaseMaterial)
	{
		// Use provided base material
		if (!CharacterPreviewMaterial || CharacterPreviewMaterial->Parent != CharacterPreviewBaseMaterial)
		{
			CharacterPreviewMaterial = UMaterialInstanceDynamic::Create(CharacterPreviewBaseMaterial, this);
		}

		if (CharacterPreviewMaterial)
		{
			CharacterPreviewMaterial->SetTextureParameterValue(FName("RenderTargetTexture"), RenderTarget);
		}
	}

	// Apply material or render target directly to image widget
	FSlateBrush Brush;
	if (CharacterPreviewMaterial)
	{
		Brush.SetResourceObject(CharacterPreviewMaterial);
	}
	else
	{
		// Fallback: Use render target directly as texture
		Brush.SetResourceObject(RenderTarget);
	}

	Brush.ImageSize = FVector2D(512.0f, 512.0f);
	Brush.DrawAs = ESlateBrushDrawType::Image;

	CharacterPreviewImage->SetBrush(Brush);
	CharacterPreviewImage->SetVisibility(ESlateVisibility::Visible);

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMainMenu: Character preview updated with render target"));
}

void USuspenseCoreMainMenuWidget::ClearCharacterPreview()
{
	if (CharacterPreviewImage)
	{
		CharacterPreviewImage->SetVisibility(ESlateVisibility::Hidden);
	}

	// Request capture disable via EventBus
	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GetWorld());
	if (Manager && Manager->GetEventBus())
	{
		FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
		EventData.SetBool(FName("Enabled"), false);
		Manager->GetEventBus()->Publish(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.CharacterPreview.RequestCapture")),
			EventData
		);
	}

	CharacterPreviewMaterial = nullptr;
	CachedRenderTarget = nullptr;

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMainMenu: Character preview cleared"));
}

void USuspenseCoreMainMenuWidget::OnRenderTargetReady(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMainMenu: Render target ready event received"));

	// Get render target from event data (passed via EventBus, no direct character dependency)
	UTextureRenderTarget2D* RenderTarget = EventData.GetObject<UTextureRenderTarget2D>(FName("RenderTarget"));

	if (RenderTarget)
	{
		UpdateCharacterPreviewImage(RenderTarget);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreMainMenu: RenderTarget not found in event data"));
	}
}
