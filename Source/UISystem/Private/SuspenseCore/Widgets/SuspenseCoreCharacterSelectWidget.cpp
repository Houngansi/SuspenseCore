// SuspenseCoreCharacterSelectWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/SuspenseCoreCharacterSelectWidget.h"
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

	// Update UI
	UpdateUIDisplay();

	// Load and display characters
	RefreshCharacterList();
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

		// Publish event
		PublishCharacterSelectEvent(
			FGameplayTag::RequestGameplayTag(FName("Event.UI.CharacterSelect.Selected")),
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

		// Publish event
		PublishCharacterSelectEvent(
			FGameplayTag::RequestGameplayTag(FName("Event.UI.CharacterSelect.Deleted")),
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

	// Publish event
	PublishCharacterSelectEvent(
		FGameplayTag::RequestGameplayTag(FName("Event.UI.CharacterSelect.CreateNew")),
		TEXT("")
	);

	// Notify
	OnCreateNewRequested();
	OnCreateNewRequestedDelegate.Broadcast();
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
}

void USuspenseCoreCharacterSelectWidget::BuildCharacterListUI()
{
	// Clear existing items
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

	// If no custom entry widget class, create simple buttons
	if (!CharacterEntryWidgetClass)
	{
		for (const FSuspenseCoreCharacterEntry& Entry : CharacterEntries)
		{
			UButton* CharButton = CreateCharacterButton(Entry);
			if (CharButton)
			{
				if (CharacterListScrollBox)
				{
					CharacterListScrollBox->AddChild(CharButton);
				}
				else if (CharacterListBox)
				{
					CharacterListBox->AddChild(CharButton);
				}
			}
		}
	}
	else
	{
		// Create custom entry widgets
		for (const FSuspenseCoreCharacterEntry& Entry : CharacterEntries)
		{
			UUserWidget* EntryWidget = CreateWidget<UUserWidget>(GetWorld(), CharacterEntryWidgetClass);
			if (EntryWidget)
			{
				// Try to set data via interface or properties
				// The Blueprint widget should handle its own display

				if (CharacterListScrollBox)
				{
					CharacterListScrollBox->AddChild(EntryWidget);
				}
				else if (CharacterListBox)
				{
					CharacterListBox->AddChild(EntryWidget);
				}
			}
		}
	}
}

UButton* USuspenseCoreCharacterSelectWidget::CreateCharacterButton(const FSuspenseCoreCharacterEntry& Entry)
{
	// Note: This is a simple fallback. For better UI, use CharacterEntryWidgetClass
	// In UMG, we can't easily create buttons with text dynamically without a widget class
	// This method is mainly for documentation - actual implementation should use Blueprint widgets

	UE_LOG(LogSuspenseCoreCharacterSelect, Verbose,
		TEXT("Character: %s (Lv.%d) - %s"),
		*Entry.DisplayName, Entry.Level, *Entry.PlayerId);

	return nullptr; // Blueprint should handle this
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

void USuspenseCoreCharacterSelectWidget::OnCharacterButtonClicked(const FString& PlayerId)
{
	SelectCharacter(PlayerId);
}
