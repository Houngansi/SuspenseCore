// SuspenseCoreRegistrationWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/SuspenseCoreRegistrationWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Services/SuspenseCoreServiceLocator.h"
#include "SuspenseCore/Repository/SuspenseCoreFilePlayerRepository.h"
#include "SuspenseCore/SuspenseCoreInterfaces.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "TimerManager.h"

USuspenseCoreRegistrationWidget::USuspenseCoreRegistrationWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USuspenseCoreRegistrationWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetupButtonBindings();
	UpdateUIState();

	// Set initial status
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(TEXT("Enter your display name to create an account.")));
	}

	if (TitleText)
	{
		TitleText->SetText(FText::FromString(TEXT("Create New Character")));
	}
}

void USuspenseCoreRegistrationWidget::NativeDestruct()
{
	// Clear timer if active
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(AutoCloseTimerHandle);
	}

	Super::NativeDestruct();
}

void USuspenseCoreRegistrationWidget::SetupButtonBindings()
{
	if (CreateButton)
	{
		CreateButton->OnClicked.AddDynamic(this, &USuspenseCoreRegistrationWidget::OnCreateButtonClicked);
	}

	if (DisplayNameInput)
	{
		DisplayNameInput->OnTextChanged.AddDynamic(this, &USuspenseCoreRegistrationWidget::OnDisplayNameChanged);
	}
}

void USuspenseCoreRegistrationWidget::OnCreateButtonClicked()
{
	AttemptCreatePlayer();
}

void USuspenseCoreRegistrationWidget::OnDisplayNameChanged(const FText& Text)
{
	UpdateUIState();
}

void USuspenseCoreRegistrationWidget::AttemptCreatePlayer()
{
	if (bIsProcessing)
	{
		ShowError(TEXT("Please wait, registration in progress..."));
		return;
	}

	if (!ValidateInput())
	{
		return;
	}

	bIsProcessing = true;
	UpdateUIState();

	// Get repository
	ISuspenseCorePlayerRepository* Repository = GetOrCreateRepository();
	if (!Repository)
	{
		ShowError(TEXT("Failed to initialize player repository. Please try again."));
		bIsProcessing = false;
		UpdateUIState();
		return;
	}

	// Create player
	FString DisplayName = GetEnteredDisplayName();
	FSuspenseCorePlayerData NewPlayerData;

	bool bSuccess = Repository->CreatePlayer(DisplayName, NewPlayerData);

	if (bSuccess)
	{
		// Save to file
		bSuccess = Repository->SavePlayer(NewPlayerData);

		if (bSuccess)
		{
			ShowSuccess(FString::Printf(TEXT("Character '%s' created successfully! ID: %s"),
				*NewPlayerData.DisplayName, *NewPlayerData.PlayerId));

			// Broadcast event
			OnRegistrationComplete.Broadcast(NewPlayerData);
			PublishRegistrationEvent(true, NewPlayerData, TEXT(""));

			// Auto-close if enabled
			if (bAutoCloseOnSuccess && GetWorld())
			{
				GetWorld()->GetTimerManager().SetTimer(
					AutoCloseTimerHandle,
					this,
					&USuspenseCoreRegistrationWidget::HandleAutoClose,
					AutoCloseDelay,
					false
				);
			}
		}
		else
		{
			ShowError(TEXT("Failed to save player data. Please try again."));
			OnRegistrationError.Broadcast(TEXT("Save failed"));
			PublishRegistrationEvent(false, FSuspenseCorePlayerData(), TEXT("Save failed"));
		}
	}
	else
	{
		ShowError(TEXT("Failed to create player. Please try again."));
		OnRegistrationError.Broadcast(TEXT("Creation failed"));
		PublishRegistrationEvent(false, FSuspenseCorePlayerData(), TEXT("Creation failed"));
	}

	bIsProcessing = false;
	UpdateUIState();
}

bool USuspenseCoreRegistrationWidget::ValidateInput() const
{
	FString DisplayName = GetEnteredDisplayName();

	if (DisplayName.Len() < MinDisplayNameLength)
	{
		const_cast<USuspenseCoreRegistrationWidget*>(this)->ShowError(
			FString::Printf(TEXT("Display name must be at least %d characters."), MinDisplayNameLength));
		return false;
	}

	if (DisplayName.Len() > MaxDisplayNameLength)
	{
		const_cast<USuspenseCoreRegistrationWidget*>(this)->ShowError(
			FString::Printf(TEXT("Display name cannot exceed %d characters."), MaxDisplayNameLength));
		return false;
	}

	// Check for invalid characters (basic validation)
	for (TCHAR Char : DisplayName)
	{
		if (!FChar::IsAlnum(Char) && Char != '_' && Char != '-' && Char != ' ')
		{
			const_cast<USuspenseCoreRegistrationWidget*>(this)->ShowError(
				TEXT("Display name contains invalid characters. Use letters, numbers, spaces, _ or -."));
			return false;
		}
	}

	return true;
}

FString USuspenseCoreRegistrationWidget::GetEnteredDisplayName() const
{
	if (DisplayNameInput)
	{
		return DisplayNameInput->GetText().ToString().TrimStartAndEnd();
	}
	return TEXT("");
}

void USuspenseCoreRegistrationWidget::ClearInputFields()
{
	if (DisplayNameInput)
	{
		DisplayNameInput->SetText(FText::GetEmpty());
	}

	if (StatusText)
	{
		StatusText->SetText(FText::FromString(TEXT("Enter your display name to create an account.")));
	}
}

void USuspenseCoreRegistrationWidget::ShowError(const FString& Message)
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Message));
		StatusText->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
	}

	UE_LOG(LogTemp, Warning, TEXT("SuspenseCore Registration Error: %s"), *Message);
}

void USuspenseCoreRegistrationWidget::ShowSuccess(const FString& Message)
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Message));
		StatusText->SetColorAndOpacity(FSlateColor(FLinearColor::Green));
	}

	UE_LOG(LogTemp, Log, TEXT("SuspenseCore Registration Success: %s"), *Message);
}

void USuspenseCoreRegistrationWidget::SetPlayerRepository(TScriptInterface<ISuspenseCorePlayerRepository> InRepository)
{
	PlayerRepository = InRepository;
}

USuspenseCoreEventBus* USuspenseCoreRegistrationWidget::GetEventBus() const
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GetWorld());
	if (Manager)
	{
		const_cast<USuspenseCoreRegistrationWidget*>(this)->CachedEventBus = Manager->GetEventBus();
		return CachedEventBus.Get();
	}

	return nullptr;
}

ISuspenseCorePlayerRepository* USuspenseCoreRegistrationWidget::GetOrCreateRepository()
{
	// If already set, return it
	if (PlayerRepository.GetObject())
	{
		return Cast<ISuspenseCorePlayerRepository>(PlayerRepository.GetObject());
	}

	// Try to get from ServiceLocator
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

void USuspenseCoreRegistrationWidget::PublishRegistrationEvent(bool bSuccess, const FSuspenseCorePlayerData& PlayerData, const FString& ErrorMessage)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
	EventData.SetBool(FName("Success"), bSuccess);
	if (bSuccess)
	{
		EventData.SetString(FName("PlayerId"), PlayerData.PlayerId);
		EventData.SetString(FName("DisplayName"), PlayerData.DisplayName);
	}
	else
	{
		EventData.SetString(FName("ErrorMessage"), ErrorMessage);
	}

	FGameplayTag EventTag = bSuccess
		? FGameplayTag::RequestGameplayTag(FName("Event.UI.Registration.Success"))
		: FGameplayTag::RequestGameplayTag(FName("Event.UI.Registration.Failed"));

	EventBus->Publish(EventTag, EventData);
}

void USuspenseCoreRegistrationWidget::UpdateUIState()
{
	if (CreateButton)
	{
		bool bCanCreate = !bIsProcessing && GetEnteredDisplayName().Len() >= MinDisplayNameLength;
		CreateButton->SetIsEnabled(bCanCreate);
	}

	if (DisplayNameInput)
	{
		DisplayNameInput->SetIsEnabled(!bIsProcessing);
	}
}

void USuspenseCoreRegistrationWidget::HandleAutoClose()
{
	SetVisibility(ESlateVisibility::Collapsed);
	RemoveFromParent();
}
