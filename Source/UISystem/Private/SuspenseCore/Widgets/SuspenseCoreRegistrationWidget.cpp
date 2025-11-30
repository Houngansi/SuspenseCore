// SuspenseCoreRegistrationWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/SuspenseCoreRegistrationWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Services/SuspenseCoreServiceLocator.h"
#include "SuspenseCore/Repository/SuspenseCoreFilePlayerRepository.h"
#include "SuspenseCore/SuspenseCoreInterfaces.h"
#include "SuspenseCore/Subsystems/SuspenseCoreCharacterClassSubsystem.h"
#include "SuspenseCore/Data/SuspenseCoreCharacterClassData.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "TimerManager.h"

USuspenseCoreRegistrationWidget::USuspenseCoreRegistrationWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USuspenseCoreRegistrationWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetupButtonBindings();
	SetupClassSelectionBindings();

	// Set default class selection
	SelectedClassId = TEXT("Assault");
	UpdateClassSelectionUI();
	UpdateUIState();

	// Set initial status
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(TEXT("Выберите класс и введите имя персонажа.")));
	}

	if (TitleText)
	{
		TitleText->SetText(FText::FromString(TEXT("Создание персонажа")));
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

	// Create player with selected class
	FString DisplayName = GetEnteredDisplayName();
	FSuspenseCorePlayerData NewPlayerData = FSuspenseCorePlayerData::CreateNew(DisplayName, SelectedClassId);

	// Save to repository
	bool bSuccess = Repository->SavePlayer(NewPlayerData);

	if (bSuccess)
	{
		ShowSuccess(FString::Printf(TEXT("Персонаж '%s' создан! Класс: %s"),
			*NewPlayerData.DisplayName, *SelectedClassId));

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
		ShowError(TEXT("Не удалось сохранить данные. Попробуйте снова."));
		OnRegistrationError.Broadcast(TEXT("Save failed"));
		PublishRegistrationEvent(false, FSuspenseCorePlayerData(), TEXT("Save failed"));
	}

	bIsProcessing = false;
	UpdateUIState();
}

bool USuspenseCoreRegistrationWidget::ValidateInput() const
{
	// Validate class selection
	if (SelectedClassId.IsEmpty())
	{
		const_cast<USuspenseCoreRegistrationWidget*>(this)->ShowError(
			TEXT("Пожалуйста, выберите класс персонажа."));
		return false;
	}

	FString DisplayName = GetEnteredDisplayName();

	if (DisplayName.Len() < MinDisplayNameLength)
	{
		const_cast<USuspenseCoreRegistrationWidget*>(this)->ShowError(
			FString::Printf(TEXT("Имя должно содержать минимум %d символа."), MinDisplayNameLength));
		return false;
	}

	if (DisplayName.Len() > MaxDisplayNameLength)
	{
		const_cast<USuspenseCoreRegistrationWidget*>(this)->ShowError(
			FString::Printf(TEXT("Имя не должно превышать %d символов."), MaxDisplayNameLength));
		return false;
	}

	// Check for invalid characters (basic validation)
	for (TCHAR Char : DisplayName)
	{
		if (!FChar::IsAlnum(Char) && Char != '_' && Char != '-' && Char != ' ')
		{
			const_cast<USuspenseCoreRegistrationWidget*>(this)->ShowError(
				TEXT("Имя содержит недопустимые символы. Используйте буквы, цифры, пробелы, _ или -."));
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
		? FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.Registration.Success"))
		: FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.Registration.Failed"));

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

// ═══════════════════════════════════════════════════════════════════════════════
// CLASS SELECTION
// ═══════════════════════════════════════════════════════════════════════════════

FString USuspenseCoreRegistrationWidget::GetSelectedClassId() const
{
	return SelectedClassId;
}

void USuspenseCoreRegistrationWidget::SelectClass(const FString& ClassId)
{
	SelectedClassId = ClassId;
	UpdateClassSelectionUI();
	UpdateUIState();

	// Publish ClassPreview event via EventBus for character preview components
	PublishClassPreviewEvent(ClassId);

	UE_LOG(LogTemp, Log, TEXT("SuspenseCore Registration: Selected class '%s'"), *ClassId);
}

void USuspenseCoreRegistrationWidget::PublishClassPreviewEvent(const FString& ClassId)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
	EventData.SetString(FName("ClassId"), ClassId);

	EventBus->Publish(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.ClassPreview.Selected")),
		EventData
	);
}

void USuspenseCoreRegistrationWidget::SetupClassSelectionBindings()
{
	if (AssaultClassButton)
	{
		AssaultClassButton->OnClicked.AddDynamic(this, &USuspenseCoreRegistrationWidget::OnAssaultClassClicked);
	}

	if (MedicClassButton)
	{
		MedicClassButton->OnClicked.AddDynamic(this, &USuspenseCoreRegistrationWidget::OnMedicClassClicked);
	}

	if (SniperClassButton)
	{
		SniperClassButton->OnClicked.AddDynamic(this, &USuspenseCoreRegistrationWidget::OnSniperClassClicked);
	}
}

void USuspenseCoreRegistrationWidget::OnAssaultClassClicked()
{
	SelectClass(TEXT("Assault"));
}

void USuspenseCoreRegistrationWidget::OnMedicClassClicked()
{
	SelectClass(TEXT("Medic"));
}

void USuspenseCoreRegistrationWidget::OnSniperClassClicked()
{
	SelectClass(TEXT("Sniper"));
}

void USuspenseCoreRegistrationWidget::UpdateClassSelectionUI()
{
	// Get class data from subsystem
	USuspenseCoreCharacterClassSubsystem* ClassSubsystem = USuspenseCoreCharacterClassSubsystem::Get(this);

	USuspenseCoreCharacterClassData* SelectedClass = nullptr;
	if (ClassSubsystem)
	{
		SelectedClass = ClassSubsystem->GetClassById(FName(*SelectedClassId));
	}

	// Update class info display
	if (SelectedClassNameText)
	{
		if (SelectedClass)
		{
			SelectedClassNameText->SetText(SelectedClass->DisplayName);
			SelectedClassNameText->SetColorAndOpacity(FSlateColor(SelectedClass->PrimaryColor));
		}
		else
		{
			// Fallback display names if subsystem not ready
			FText DisplayName;
			if (SelectedClassId == TEXT("Assault"))
			{
				DisplayName = FText::FromString(TEXT("Штурмовик"));
			}
			else if (SelectedClassId == TEXT("Medic"))
			{
				DisplayName = FText::FromString(TEXT("Медик"));
			}
			else if (SelectedClassId == TEXT("Sniper"))
			{
				DisplayName = FText::FromString(TEXT("Снайпер"));
			}
			else
			{
				DisplayName = FText::FromString(SelectedClassId);
			}
			SelectedClassNameText->SetText(DisplayName);
		}
	}

	if (SelectedClassDescriptionText)
	{
		if (SelectedClass)
		{
			SelectedClassDescriptionText->SetText(SelectedClass->ShortDescription);
		}
		else
		{
			// Fallback descriptions
			FText Description;
			if (SelectedClassId == TEXT("Assault"))
			{
				Description = FText::FromString(TEXT("Сбалансированный боец передовой линии. Повышенный урон и скорость перезарядки."));
			}
			else if (SelectedClassId == TEXT("Medic"))
			{
				Description = FText::FromString(TEXT("Поддержка команды. Быстрая регенерация здоровья и щита."));
			}
			else if (SelectedClassId == TEXT("Sniper"))
			{
				Description = FText::FromString(TEXT("Дальнобойный стрелок. Высокий урон и точность."));
			}
			SelectedClassDescriptionText->SetText(Description);
		}
	}

	// Update button visual states (highlight selected)
	auto UpdateButtonStyle = [this](UButton* Button, const FString& ButtonClassId)
	{
		if (!Button) return;

		bool bIsSelected = (ButtonClassId == SelectedClassId);

		// Set button style based on selection
		FButtonStyle Style = Button->GetStyle();
		if (bIsSelected)
		{
			// Highlight selected button
			Style.Normal.TintColor = FSlateColor(FLinearColor(0.3f, 0.6f, 1.0f, 1.0f));
		}
		else
		{
			// Default style
			Style.Normal.TintColor = FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f));
		}
		Button->SetStyle(Style);
	};

	UpdateButtonStyle(AssaultClassButton, TEXT("Assault"));
	UpdateButtonStyle(MedicClassButton, TEXT("Medic"));
	UpdateButtonStyle(SniperClassButton, TEXT("Sniper"));
}
