// SuspenseCoreRegistrationWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/SuspenseCoreRegistrationWidget.h"
#include "SuspenseCore/Widgets/SuspenseCoreClassSelectionButtonWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Services/SuspenseCoreServiceLocator.h"
#include "SuspenseCore/Repository/SuspenseCoreFilePlayerRepository.h"
#include "SuspenseCore/SuspenseCoreInterfaces.h"
#include "SuspenseCore/Subsystems/SuspenseCoreCharacterClassSubsystem.h"
#include "SuspenseCore/Subsystems/SuspenseCoreCharacterSelectionSubsystem.h"
#include "SuspenseCore/Data/SuspenseCoreCharacterClassData.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Image.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreRegistration, Log, All);

USuspenseCoreRegistrationWidget::USuspenseCoreRegistrationWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USuspenseCoreRegistrationWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetupButtonBindings();
	SetupClassSelectionBindings();

	// Set default class selection (also updates CharacterSelectionSubsystem)
	SelectClass(TEXT("Assault"));

	// Set initial status
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(TEXT("Select a class and enter your character name.")));
	}

	if (TitleText)
	{
		TitleText->SetText(FText::FromString(TEXT("Create Your Character")));
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

	if (BackButton)
	{
		BackButton->OnClicked.AddDynamic(this, &USuspenseCoreRegistrationWidget::OnBackButtonClicked);
		UE_LOG(LogSuspenseCoreRegistration, Log, TEXT("[RegistrationWidget] BackButton bound"));
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
		ShowSuccess(FString::Printf(TEXT("Character '%s' created! Class: %s"),
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
		ShowError(TEXT("Failed to save data. Please try again."));
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
			TEXT("Please select a character class."));
		return false;
	}

	FString DisplayName = GetEnteredDisplayName();

	if (DisplayName.Len() < MinDisplayNameLength)
	{
		const_cast<USuspenseCoreRegistrationWidget*>(this)->ShowError(
			FString::Printf(TEXT("Name must be at least %d characters."), MinDisplayNameLength));
		return false;
	}

	if (DisplayName.Len() > MaxDisplayNameLength)
	{
		const_cast<USuspenseCoreRegistrationWidget*>(this)->ShowError(
			FString::Printf(TEXT("Name must not exceed %d characters."), MaxDisplayNameLength));
		return false;
	}

	// Check for invalid characters (basic validation)
	for (TCHAR Char : DisplayName)
	{
		if (!FChar::IsAlnum(Char) && Char != '_' && Char != '-' && Char != ' ')
		{
			const_cast<USuspenseCoreRegistrationWidget*>(this)->ShowError(
				TEXT("Name contains invalid characters. Use letters, numbers, spaces, _ or -."));
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

	UE_LOG(LogSuspenseCoreRegistration, Warning, TEXT("Registration Error: %s"), *Message);
}

void USuspenseCoreRegistrationWidget::ShowSuccess(const FString& Message)
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Message));
		StatusText->SetColorAndOpacity(FSlateColor(FLinearColor::Green));
	}

	UE_LOG(LogSuspenseCoreRegistration, Log, TEXT("Registration Success: %s"), *Message);
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
	UE_LOG(LogSuspenseCoreRegistration, Log, TEXT("[RegistrationWidget] SelectClass('%s') called"), *ClassId);

	SelectedClassId = ClassId;
	UpdateClassSelectionUI();
	UpdateUIState();

	// Update CharacterSelectionSubsystem (persists across maps and notifies PreviewActor)
	USuspenseCoreCharacterSelectionSubsystem* SelectionSubsystem = USuspenseCoreCharacterSelectionSubsystem::Get(this);
	if (SelectionSubsystem)
	{
		UE_LOG(LogSuspenseCoreRegistration, Verbose, TEXT("[RegistrationWidget] CharacterSelectionSubsystem found"));
		FName ClassIdName = FName(*ClassId);

		// Get class data from CharacterClassSubsystem and register it
		USuspenseCoreCharacterClassSubsystem* ClassSubsystem = USuspenseCoreCharacterClassSubsystem::Get(this);
		if (ClassSubsystem)
		{
			UE_LOG(LogSuspenseCoreRegistration, Verbose, TEXT("[RegistrationWidget] CharacterClassSubsystem found"));
			USuspenseCoreCharacterClassData* ClassData = ClassSubsystem->GetClassById(ClassIdName);
			if (ClassData)
			{
				UE_LOG(LogSuspenseCoreRegistration, Verbose, TEXT("[RegistrationWidget] ClassData found for '%s', registering and selecting"), *ClassId);
				// Register class data if not already registered
				SelectionSubsystem->RegisterClassData(ClassData, ClassIdName);
				// Select the class (publishes CharacterClass.Changed event)
				SelectionSubsystem->SelectCharacterClass(ClassData, ClassIdName);
			}
			else
			{
				UE_LOG(LogSuspenseCoreRegistration, Warning, TEXT("[RegistrationWidget] ClassData NOT found for '%s', selecting by ID only"), *ClassId);
				// Fallback: select by ID only
				SelectionSubsystem->SelectCharacterClassById(ClassIdName);
			}
		}
		else
		{
			UE_LOG(LogSuspenseCoreRegistration, Warning, TEXT("[RegistrationWidget] CharacterClassSubsystem NOT found! Selecting by ID only"));
			// No class subsystem, just select by ID
			SelectionSubsystem->SelectCharacterClassById(ClassIdName);
		}
	}
	else
	{
		UE_LOG(LogSuspenseCoreRegistration, Error, TEXT("[RegistrationWidget] CharacterSelectionSubsystem NOT found! Check GameInstance setup."));
	}

	// Also publish legacy ClassPreview event for backwards compatibility
	PublishClassPreviewEvent(ClassId);

	UE_LOG(LogSuspenseCoreRegistration, Verbose, TEXT("[RegistrationWidget] SelectClass complete for '%s'"), *ClassId);
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
	UE_LOG(LogSuspenseCoreRegistration, Log, TEXT("[RegistrationWidget] SetupClassSelectionBindings called"));

	// Try procedural class button creation first
	if (ClassButtonContainer && ClassButtonWidgetClass)
	{
		UE_LOG(LogSuspenseCoreRegistration, Log, TEXT("[RegistrationWidget] Using procedural class button creation"));
		CreateProceduralClassButtons();
	}
	else
	{
		// Fall back to legacy hardcoded buttons
		UE_LOG(LogSuspenseCoreRegistration, Log, TEXT("[RegistrationWidget] Falling back to legacy class buttons"));
		SetupLegacyClassBindings();
	}
}

void USuspenseCoreRegistrationWidget::CreateProceduralClassButtons()
{
	if (!ClassButtonContainer || !ClassButtonWidgetClass)
	{
		UE_LOG(LogSuspenseCoreRegistration, Warning, TEXT("[RegistrationWidget] Cannot create procedural buttons - missing container or widget class"));
		return;
	}

	// Clear existing buttons
	ClassButtonContainer->ClearChildren();
	CreatedClassButtons.Empty();

	// Get all available classes from subsystem
	USuspenseCoreCharacterClassSubsystem* ClassSubsystem = USuspenseCoreCharacterClassSubsystem::Get(this);
	if (!ClassSubsystem)
	{
		UE_LOG(LogSuspenseCoreRegistration, Warning, TEXT("[RegistrationWidget] CharacterClassSubsystem not available"));
		return;
	}

	TArray<USuspenseCoreCharacterClassData*> AllClasses = ClassSubsystem->GetAllClasses();
	UE_LOG(LogSuspenseCoreRegistration, Log, TEXT("[RegistrationWidget] Found %d classes to create buttons for"), AllClasses.Num());

	// Debug: log container info
	FVector2D ContainerSize = ClassButtonContainer->GetDesiredSize();
	UE_LOG(LogSuspenseCoreRegistration, Log, TEXT("[RegistrationWidget] ClassButtonContainer: Type=%s, Size=%.1f x %.1f, Visibility=%d, ChildCount=%d"),
		*ClassButtonContainer->GetClass()->GetName(),
		ContainerSize.X, ContainerSize.Y,
		(int32)ClassButtonContainer->GetVisibility(),
		ClassButtonContainer->GetChildrenCount());

	for (USuspenseCoreCharacterClassData* ClassData : AllClasses)
	{
		if (!ClassData)
		{
			continue;
		}

		// Create button widget
		USuspenseCoreClassSelectionButtonWidget* ButtonWidget = CreateWidget<USuspenseCoreClassSelectionButtonWidget>(this, ClassButtonWidgetClass);
		if (!ButtonWidget)
		{
			UE_LOG(LogSuspenseCoreRegistration, Warning, TEXT("[RegistrationWidget] Failed to create class button widget"));
			continue;
		}

		// Configure button with class data
		ButtonWidget->SetClassData(ClassData);

		// Bind click event
		ButtonWidget->OnClassButtonClicked.AddDynamic(this, &USuspenseCoreRegistrationWidget::OnClassButtonClicked);

		// Ensure widget is visible
		ButtonWidget->SetVisibility(ESlateVisibility::Visible);

		// Add to container and configure slot
		UPanelSlot* Slot = ClassButtonContainer->AddChild(ButtonWidget);

		// Configure slot based on container type
		if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(Slot))
		{
			HSlot->SetPadding(FMargin(8.0f, 4.0f, 8.0f, 4.0f));
			HSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);
			HSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Fill);
			HSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
		else if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(Slot))
		{
			VSlot->SetPadding(FMargin(4.0f, 8.0f, 4.0f, 8.0f));
			VSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
			VSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Center);
			VSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}

		CreatedClassButtons.Add(ButtonWidget);

		// Debug: log widget details
		FVector2D DesiredSize = ButtonWidget->GetDesiredSize();
		UE_LOG(LogSuspenseCoreRegistration, Log, TEXT("[RegistrationWidget] Created class button for: %s (DesiredSize: %.1f x %.1f, Visibility: %d)"),
			*ClassData->ClassID.ToString(), DesiredSize.X, DesiredSize.Y, (int32)ButtonWidget->GetVisibility());
	}

	UE_LOG(LogSuspenseCoreRegistration, Log, TEXT("[RegistrationWidget] Created %d class buttons"), CreatedClassButtons.Num());
}

void USuspenseCoreRegistrationWidget::OnClassButtonClicked(const FString& ClassId)
{
	UE_LOG(LogSuspenseCoreRegistration, Log, TEXT("[RegistrationWidget] Class button clicked: %s"), *ClassId);
	SelectClass(ClassId);
}

void USuspenseCoreRegistrationWidget::UpdateClassButtonSelectionStates()
{
	for (USuspenseCoreClassSelectionButtonWidget* ButtonWidget : CreatedClassButtons)
	{
		if (ButtonWidget)
		{
			bool bIsSelected = (ButtonWidget->GetClassId() == SelectedClassId);
			ButtonWidget->SetSelected(bIsSelected);
		}
	}
}

void USuspenseCoreRegistrationWidget::SetupLegacyClassBindings()
{
	UE_LOG(LogSuspenseCoreRegistration, Log, TEXT("[RegistrationWidget] SetupLegacyClassBindings called (DEPRECATED)"));
	UE_LOG(LogSuspenseCoreRegistration, Log, TEXT("[RegistrationWidget] AssaultClassButton: %s"), AssaultClassButton ? TEXT("BOUND") : TEXT("NULL"));
	UE_LOG(LogSuspenseCoreRegistration, Log, TEXT("[RegistrationWidget] MedicClassButton: %s"), MedicClassButton ? TEXT("BOUND") : TEXT("NULL"));
	UE_LOG(LogSuspenseCoreRegistration, Log, TEXT("[RegistrationWidget] SniperClassButton: %s"), SniperClassButton ? TEXT("BOUND") : TEXT("NULL"));

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
	UE_LOG(LogSuspenseCoreRegistration, Log, TEXT("[RegistrationWidget] OnAssaultClassClicked (legacy)"));
	SelectClass(TEXT("Assault"));
}

void USuspenseCoreRegistrationWidget::OnMedicClassClicked()
{
	UE_LOG(LogSuspenseCoreRegistration, Log, TEXT("[RegistrationWidget] OnMedicClassClicked (legacy)"));
	SelectClass(TEXT("Medic"));
}

void USuspenseCoreRegistrationWidget::OnSniperClassClicked()
{
	UE_LOG(LogSuspenseCoreRegistration, Log, TEXT("[RegistrationWidget] OnSniperClassClicked (legacy)"));
	SelectClass(TEXT("Sniper"));
}

void USuspenseCoreRegistrationWidget::OnBackButtonClicked()
{
	UE_LOG(LogSuspenseCoreRegistration, Log, TEXT("[RegistrationWidget] Back button clicked - returning to character select"));

	// Publish navigation event via EventBus
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (EventBus)
	{
		FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
		EventBus->Publish(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.Registration.BackToSelect")),
			EventData
		);
	}
}

void USuspenseCoreRegistrationWidget::UpdateClassSelectionUI()
{
	// Update procedural button selection states
	UpdateClassButtonSelectionStates();

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
				DisplayName = FText::FromString(TEXT("Assault"));
			}
			else if (SelectedClassId == TEXT("Medic"))
			{
				DisplayName = FText::FromString(TEXT("Medic"));
			}
			else if (SelectedClassId == TEXT("Sniper"))
			{
				DisplayName = FText::FromString(TEXT("Sniper"));
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
				Description = FText::FromString(TEXT("Balanced frontline fighter. Increased damage and reload speed."));
			}
			else if (SelectedClassId == TEXT("Medic"))
			{
				Description = FText::FromString(TEXT("Team support specialist. Fast health and shield regeneration."));
			}
			else if (SelectedClassId == TEXT("Sniper"))
			{
				Description = FText::FromString(TEXT("Long-range marksman. High damage and accuracy."));
			}
			SelectedClassDescriptionText->SetText(Description);
		}
	}

	// Update class icon from ClassData (for selected class display panel)
	if (ClassIconImage && SelectedClass)
	{
		if (UTexture2D* IconTexture = SelectedClass->ClassIcon.LoadSynchronous())
		{
			ClassIconImage->SetBrushFromTexture(IconTexture);
			ClassIconImage->SetVisibility(ESlateVisibility::Visible);
			UE_LOG(LogSuspenseCoreRegistration, Verbose, TEXT("[RegistrationWidget] ClassIcon set for %s"), *SelectedClassId);
		}
		else
		{
			ClassIconImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Update class portrait from ClassData (for selected class display panel)
	if (ClassPortraitImage && SelectedClass)
	{
		if (UTexture2D* PortraitTexture = SelectedClass->ClassPortrait.LoadSynchronous())
		{
			ClassPortraitImage->SetBrushFromTexture(PortraitTexture);
			ClassPortraitImage->SetVisibility(ESlateVisibility::Visible);
			UE_LOG(LogSuspenseCoreRegistration, Verbose, TEXT("[RegistrationWidget] ClassPortrait set for %s"), *SelectedClassId);
		}
		else
		{
			ClassPortraitImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// LEGACY: Update button visual states (highlight selected) - for backwards compatibility
	// New code should use ClassButtonContainer with procedural class buttons
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
