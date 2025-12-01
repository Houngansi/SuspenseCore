// SuspenseCoreCharacterSelectWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/SuspenseCoreCharacterSelectWidget.h"
#include "SuspenseCore/Widgets/SuspenseCoreCharacterEntryWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Services/SuspenseCoreServiceLocator.h"
#include "SuspenseCore/Repository/SuspenseCoreFilePlayerRepository.h"
#include "SuspenseCore/SuspenseCoreInterfaces.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreCharacterSelect, Log, All);

USuspenseCoreCharacterSelectWidget::USuspenseCoreCharacterSelectWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USuspenseCoreCharacterSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Setup button bindings
	if (CreateNewButton)
	{
		CreateNewButton->OnClicked.AddDynamic(this, &USuspenseCoreCharacterSelectWidget::OnCreateNewButtonClicked);
	}

	if (PlayButton)
	{
		PlayButton->OnClicked.AddDynamic(this, &USuspenseCoreCharacterSelectWidget::OnPlayButtonClicked);
	}

	if (DeleteButton)
	{
		DeleteButton->OnClicked.AddDynamic(this, &USuspenseCoreCharacterSelectWidget::OnDeleteButtonClicked);
	}

	// Update UI display (text, etc.)
	UpdateUIDisplay();

	// Initial play button state
	UpdatePlayButtonState();

	// NOTE: RefreshCharacterList() is NOT called here!
	// Parent widget (MainMenuWidget) should call RefreshCharacterList() AFTER
	// setting up delegate bindings to ensure proper event flow.
}

void USuspenseCoreCharacterSelectWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreCharacterSelectWidget::RefreshCharacterList()
{
	CharacterEntries.Empty();

	ISuspenseCorePlayerRepository* Repo = GetOrCreateRepository();
	if (!Repo)
	{
		UE_LOG(LogSuspenseCoreCharacterSelect, Warning, TEXT("Failed to get player repository"));
		BuildCharacterListUI();
		return;
	}

	// Get all player IDs
	TArray<FString> PlayerIds;
	Repo->GetAllPlayerIds(PlayerIds);

	// Load each player's data
	for (const FString& PlayerId : PlayerIds)
	{
		FSuspenseCorePlayerData PlayerData;
		if (Repo->LoadPlayer(PlayerId, PlayerData))
		{
			CharacterEntries.Add(FSuspenseCoreCharacterEntry(PlayerData));
		}
	}

	// Sort by last played (most recent first)
	CharacterEntries.Sort([](const FSuspenseCoreCharacterEntry& A, const FSuspenseCoreCharacterEntry& B)
	{
		return A.LastPlayed > B.LastPlayed;
	});

	UE_LOG(LogSuspenseCoreCharacterSelect, Log, TEXT("Loaded %d characters"), CharacterEntries.Num());

	// Rebuild UI
	BuildCharacterListUI();

	// Notify
	OnCharacterListRefreshed(CharacterEntries.Num());
}

void USuspenseCoreCharacterSelectWidget::SelectCharacter(const FString& PlayerId)
{
	// Find the entry
	FSuspenseCoreCharacterEntry* FoundEntry = CharacterEntries.FindByPredicate(
		[&PlayerId](const FSuspenseCoreCharacterEntry& Entry) { return Entry.PlayerId == PlayerId; }
	);

	if (FoundEntry)
	{
		UE_LOG(LogSuspenseCoreCharacterSelect, Log, TEXT("Selected character: %s (%s)"),
			*FoundEntry->DisplayName, *PlayerId);

		// Publish event via EventBus (primary inter-widget communication)
		PublishCharacterSelectEvent(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.CharacterSelect.Selected")),
			PlayerId
		);

		// Call delegates
		OnCharacterSelected(PlayerId, *FoundEntry);
		OnCharacterSelectedDelegate.Broadcast(PlayerId, *FoundEntry);
	}
	else
	{
		UE_LOG(LogSuspenseCoreCharacterSelect, Warning, TEXT("Character not found: %s"), *PlayerId);
	}
}

void USuspenseCoreCharacterSelectWidget::DeleteCharacter(const FString& PlayerId)
{
	ISuspenseCorePlayerRepository* Repo = GetOrCreateRepository();
	if (!Repo)
	{
		return;
	}

	if (Repo->DeletePlayer(PlayerId))
	{
		UE_LOG(LogSuspenseCoreCharacterSelect, Log, TEXT("Deleted character: %s"), *PlayerId);

		// Publish event via EventBus (primary inter-widget communication)
		PublishCharacterSelectEvent(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.CharacterSelect.Deleted")),
			PlayerId
		);

		// Notify
		OnCharacterDeleted(PlayerId);
		OnCharacterDeletedDelegate.Broadcast(PlayerId);

		// Refresh list
		RefreshCharacterList();
	}
	else
	{
		UE_LOG(LogSuspenseCoreCharacterSelect, Error, TEXT("Failed to delete character: %s"), *PlayerId);
	}
}

void USuspenseCoreCharacterSelectWidget::RequestCreateNewCharacter()
{
	UE_LOG(LogSuspenseCoreCharacterSelect, Log, TEXT("Create new character requested"));

	// Publish event via EventBus (primary inter-widget communication)
	PublishCharacterSelectEvent(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.CharacterSelect.CreateNew")),
		TEXT("")
	);

	// Notify
	OnCreateNewRequested();
	OnCreateNewRequestedDelegate.Broadcast();
}

void USuspenseCoreCharacterSelectWidget::HighlightCharacter(const FString& PlayerId)
{
	// Clear previous highlight
	FString PreviousHighlight = HighlightedPlayerId;

	// Find the entry
	FSuspenseCoreCharacterEntry* FoundEntry = CharacterEntries.FindByPredicate(
		[&PlayerId](const FSuspenseCoreCharacterEntry& Entry) { return Entry.PlayerId == PlayerId; }
	);

	if (FoundEntry)
	{
		HighlightedPlayerId = PlayerId;
		HighlightedEntry = *FoundEntry;

		UE_LOG(LogSuspenseCoreCharacterSelect, Log, TEXT("Highlighted character: %s (%s)"),
			*FoundEntry->DisplayName, *PlayerId);

		// Update entry widgets visual state
		for (const auto& Pair : EntryWidgetMap)
		{
			USuspenseCoreCharacterEntryWidget* EntryWidget = Pair.Key;
			if (EntryWidget)
			{
				EntryWidget->SetSelected(Pair.Value == PlayerId);
			}
		}

		// Publish highlight event via EventBus (primary inter-widget communication)
		PublishCharacterSelectEvent(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.CharacterSelect.Highlighted")),
			PlayerId
		);

		// Broadcast delegate for Blueprint events only
		OnCharacterHighlightedDelegate.Broadcast(PlayerId, *FoundEntry);
	}
	else
	{
		// Clear highlight if not found
		HighlightedPlayerId = TEXT("");
		HighlightedEntry = FSuspenseCoreCharacterEntry();
	}

	// Update play button
	UpdatePlayButtonState();
}

void USuspenseCoreCharacterSelectWidget::PlayWithHighlightedCharacter()
{
	if (!HighlightedPlayerId.IsEmpty())
	{
		UE_LOG(LogSuspenseCoreCharacterSelect, Log, TEXT("Playing with highlighted character: %s"),
			*HighlightedPlayerId);

		// This will trigger the transition to main menu
		SelectCharacter(HighlightedPlayerId);
	}
	else
	{
		UE_LOG(LogSuspenseCoreCharacterSelect, Warning, TEXT("No character highlighted to play with"));
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL
// ═══════════════════════════════════════════════════════════════════════════════

ISuspenseCorePlayerRepository* USuspenseCoreCharacterSelectWidget::GetOrCreateRepository()
{
	// Try to get from ServiceLocator first
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

	// Create default file repository
	USuspenseCoreFilePlayerRepository* FileRepo = NewObject<USuspenseCoreFilePlayerRepository>(this);

	// IMPORTANT: Initialize repository with default path ([Project]/Saved/Players/)
	FileRepo->Initialize(TEXT(""));

	// Register with ServiceLocator for future use
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

USuspenseCoreEventBus* USuspenseCoreCharacterSelectWidget::GetEventBus() const
{
	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GetWorld());
	if (Manager)
	{
		return Manager->GetEventBus();
	}
	return nullptr;
}

void USuspenseCoreCharacterSelectWidget::UpdateUIDisplay()
{
	if (TitleText)
	{
		TitleText->SetText(Title);
	}

	if (CreateNewButtonText)
	{
		CreateNewButtonText->SetText(CreateNewText);
	}

	if (PlayButtonText)
	{
		PlayButtonText->SetText(PlayText);
	}

	if (DeleteButtonText)
	{
		DeleteButtonText->SetText(DeleteText);
	}
}

void USuspenseCoreCharacterSelectWidget::BuildCharacterListUI()
{
	// Clear existing items and maps
	ButtonToPlayerIdMap.Empty();
	EntryWidgetMap.Empty();
	HighlightedPlayerId = TEXT("");

	if (CharacterListScrollBox)
	{
		CharacterListScrollBox->ClearChildren();
	}
	if (CharacterListBox)
	{
		CharacterListBox->ClearChildren();
	}

	// Update status text
	if (StatusText)
	{
		if (CharacterEntries.Num() == 0)
		{
			StatusText->SetText(NoCharactersText);
			StatusText->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			StatusText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Use custom entry widget if available
	if (CharacterEntryWidgetClass && CharacterEntryWidgetClass->IsChildOf(USuspenseCoreCharacterEntryWidget::StaticClass()))
	{
		for (const FSuspenseCoreCharacterEntry& Entry : CharacterEntries)
		{
			USuspenseCoreCharacterEntryWidget* EntryWidget = CreateWidget<USuspenseCoreCharacterEntryWidget>(GetWorld(), CharacterEntryWidgetClass);
			if (EntryWidget)
			{
				// Set character data
				EntryWidget->SetCharacterData(Entry.PlayerId, Entry.DisplayName, Entry.CharacterClassId, Entry.Level, nullptr);

				// Bind click event
				EntryWidget->OnEntryClicked.AddDynamic(this, &USuspenseCoreCharacterSelectWidget::OnEntryWidgetClicked);

				// Store mapping
				EntryWidgetMap.Add(EntryWidget, Entry.PlayerId);

				// Add to container
				if (CharacterListScrollBox)
				{
					CharacterListScrollBox->AddChild(EntryWidget);
				}
				else if (CharacterListBox)
				{
					CharacterListBox->AddChild(EntryWidget);
				}

				UE_LOG(LogSuspenseCoreCharacterSelect, Log,
					TEXT("Created entry widget for character: %s (Lv.%d)"),
					*Entry.DisplayName, Entry.Level);
			}
		}
	}
	else
	{
		// Fallback: Create simple buttons for each character
		for (const FSuspenseCoreCharacterEntry& Entry : CharacterEntries)
		{
			UButton* CharButton = CreateCharacterButton(Entry);
			if (CharButton)
			{
				// Store mapping for click handler
				ButtonToPlayerIdMap.Add(CharButton, Entry.PlayerId);

				// Add to container
				if (CharacterListScrollBox)
				{
					CharacterListScrollBox->AddChild(CharButton);
				}
				else if (CharacterListBox)
				{
					CharacterListBox->AddChild(CharButton);
				}

				UE_LOG(LogSuspenseCoreCharacterSelect, Log,
					TEXT("Created button for character: %s (Lv.%d)"),
					*Entry.DisplayName, Entry.Level);
			}
		}
	}

	// Auto-highlight first character if we have any
	if (CharacterEntries.Num() > 0)
	{
		HighlightCharacter(CharacterEntries[0].PlayerId);
	}

	// Update play button state
	UpdatePlayButtonState();
}

UButton* USuspenseCoreCharacterSelectWidget::CreateCharacterButton(const FSuspenseCoreCharacterEntry& Entry)
{
	// Create button
	UButton* Button = NewObject<UButton>(this);
	if (!Button)
	{
		return nullptr;
	}

	// Create text block for button content
	UTextBlock* ButtonText = NewObject<UTextBlock>(Button);
	if (ButtonText)
	{
		// Format: "DisplayName (Lv.X)"
		FString ButtonLabel = FString::Printf(TEXT("%s (Lv.%d)"), *Entry.DisplayName, Entry.Level);
		ButtonText->SetText(FText::FromString(ButtonLabel));

		// Style the text
		FSlateFontInfo FontInfo = ButtonText->GetFont();
		FontInfo.Size = 18;
		ButtonText->SetFont(FontInfo);

		Button->AddChild(ButtonText);
	}

	// Bind click event
	Button->OnClicked.AddDynamic(this, &USuspenseCoreCharacterSelectWidget::OnCharacterButtonClicked);

	UE_LOG(LogSuspenseCoreCharacterSelect, Verbose,
		TEXT("Character button created: %s (Lv.%d) - %s"),
		*Entry.DisplayName, Entry.Level, *Entry.PlayerId);

	return Button;
}

void USuspenseCoreCharacterSelectWidget::PublishCharacterSelectEvent(const FGameplayTag& EventTag, const FString& PlayerId)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
	EventData.SetString(FName("PlayerId"), PlayerId);

	EventBus->Publish(EventTag, EventData);
}

void USuspenseCoreCharacterSelectWidget::OnCreateNewButtonClicked()
{
	RequestCreateNewCharacter();
}

void USuspenseCoreCharacterSelectWidget::OnCharacterButtonClicked()
{
	// Find which button was clicked by checking hovered state
	for (const auto& Pair : ButtonToPlayerIdMap)
	{
		UButton* Button = Pair.Key;
		if (Button && Button->IsHovered())
		{
			// Highlight instead of select - user needs to click Play to confirm
			HighlightCharacter(Pair.Value);
			return;
		}
	}

	// Fallback: if no hovered button found
	UE_LOG(LogSuspenseCoreCharacterSelect, Warning, TEXT("OnCharacterButtonClicked: Could not determine which button was clicked"));
}

void USuspenseCoreCharacterSelectWidget::OnPlayButtonClicked()
{
	UE_LOG(LogSuspenseCoreCharacterSelect, Log, TEXT("Play button clicked"));
	PlayWithHighlightedCharacter();
}

void USuspenseCoreCharacterSelectWidget::OnDeleteButtonClicked()
{
	UE_LOG(LogSuspenseCoreCharacterSelect, Log, TEXT("Delete button clicked"));

	if (!HighlightedPlayerId.IsEmpty())
	{
		FString PlayerIdToDelete = HighlightedPlayerId;

		// Clear highlight before delete
		HighlightedPlayerId.Empty();
		HighlightedEntry = FSuspenseCoreCharacterEntry();

		// Delete the character (this will refresh list and broadcast delegate)
		DeleteCharacter(PlayerIdToDelete);
	}
	else
	{
		UE_LOG(LogSuspenseCoreCharacterSelect, Warning, TEXT("No character highlighted to delete"));
	}
}

void USuspenseCoreCharacterSelectWidget::OnEntryWidgetClicked(const FString& PlayerId)
{
	UE_LOG(LogSuspenseCoreCharacterSelect, Log, TEXT("Entry widget clicked: %s"), *PlayerId);
	HighlightCharacter(PlayerId);
}

void USuspenseCoreCharacterSelectWidget::UpdatePlayButtonState()
{
	bool bHasSelection = !HighlightedPlayerId.IsEmpty();

	if (PlayButton)
	{
		PlayButton->SetIsEnabled(bHasSelection);

		// Update button text
		if (PlayButtonText)
		{
			if (bHasSelection)
			{
				PlayButtonText->SetText(PlayText);
			}
			else
			{
				PlayButtonText->SetText(SelectCharacterText);
			}
		}
	}

	// Delete button also requires selection
	if (DeleteButton)
	{
		DeleteButton->SetIsEnabled(bHasSelection);
	}
}
