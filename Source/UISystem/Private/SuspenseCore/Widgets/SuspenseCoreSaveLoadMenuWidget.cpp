// SuspenseCoreSaveLoadMenuWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/SuspenseCoreSaveLoadMenuWidget.h"
#include "SuspenseCore/Widgets/SuspenseCoreSaveSlotWidget.h"
#include "SuspenseCore/Save/SuspenseCoreSaveManager.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreSaveLoadMenu, Log, All);

// Special slot indices (must match SuspenseCoreSaveManager constants)
static constexpr int32 AUTOSAVE_SLOT = 100;
static constexpr int32 QUICKSAVE_SLOT = 101;

USuspenseCoreSaveLoadMenuWidget::USuspenseCoreSaveLoadMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USuspenseCoreSaveLoadMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetupButtonBindings();

	// Start hidden
	SetVisibility(ESlateVisibility::Collapsed);
	bIsVisible = false;

	// Hide confirmation overlay
	if (ConfirmationOverlay)
	{
		ConfirmationOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Subscribe to save manager events
	if (USuspenseCoreSaveManager* SaveMgr = GetSaveManager())
	{
		SaveMgr->OnSaveCompleted.AddDynamic(this, &USuspenseCoreSaveLoadMenuWidget::OnSaveCompleted);
		SaveMgr->OnLoadCompleted.AddDynamic(this, &USuspenseCoreSaveLoadMenuWidget::OnLoadCompleted);
	}

	// Make focusable
	SetIsFocusable(true);
}

void USuspenseCoreSaveLoadMenuWidget::NativeDestruct()
{
	// Clear timer
	if (StatusTimerHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(StatusTimerHandle);
		}
	}

	// Unsubscribe from save manager
	if (USuspenseCoreSaveManager* SaveMgr = GetSaveManager())
	{
		SaveMgr->OnSaveCompleted.RemoveDynamic(this, &USuspenseCoreSaveLoadMenuWidget::OnSaveCompleted);
		SaveMgr->OnLoadCompleted.RemoveDynamic(this, &USuspenseCoreSaveLoadMenuWidget::OnLoadCompleted);
	}

	ClearSlotWidgets();

	Super::NativeDestruct();
}

FReply USuspenseCoreSaveLoadMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// ESC to close
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (PendingOperation != EPendingOperation::None)
		{
			HideConfirmation();
		}
		else
		{
			HideMenu();
		}
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void USuspenseCoreSaveLoadMenuWidget::ShowMenu(ESuspenseCoreSaveLoadMode Mode)
{
	if (bIsVisible && CurrentMode == Mode)
	{
		return;
	}

	CurrentMode = Mode;
	bIsVisible = true;

	SetVisibility(ESlateVisibility::Visible);
	SetUIInputMode();

	// Reset selection
	SelectedSlotIndex = -1;
	bSelectedSlotEmpty = true;

	// Update display
	UpdateModeDisplay();
	RefreshSlots();
	UpdateActionButtonState();

	// Focus
	SetFocus();

	OnMenuShown(Mode);

	UE_LOG(LogSuspenseCoreSaveLoadMenu, Log, TEXT("Save/Load menu shown in %s mode"),
		Mode == ESuspenseCoreSaveLoadMode::Save ? TEXT("Save") : TEXT("Load"));
}

void USuspenseCoreSaveLoadMenuWidget::HideMenu()
{
	if (!bIsVisible)
	{
		return;
	}

	bIsVisible = false;
	SetVisibility(ESlateVisibility::Collapsed);

	RestoreGameInputMode();
	HideConfirmation();

	OnMenuHidden();
	OnMenuClosed.Broadcast();

	UE_LOG(LogSuspenseCoreSaveLoadMenu, Log, TEXT("Save/Load menu hidden"));
}

void USuspenseCoreSaveLoadMenuWidget::ToggleMenu(ESuspenseCoreSaveLoadMode Mode)
{
	if (bIsVisible && CurrentMode == Mode)
	{
		HideMenu();
	}
	else
	{
		ShowMenu(Mode);
	}
}

void USuspenseCoreSaveLoadMenuWidget::RefreshSlots()
{
	USuspenseCoreSaveManager* SaveMgr = GetSaveManager();
	if (!SaveMgr)
	{
		UE_LOG(LogSuspenseCoreSaveLoadMenu, Warning, TEXT("Cannot refresh slots: SaveManager not available"));
		return;
	}

	// Get all save headers
	TArray<FSuspenseCoreSaveHeader> Headers = SaveMgr->GetAllSlotHeaders();

	// Create a map for quick lookup
	TMap<int32, FSuspenseCoreSaveHeader> HeaderMap;
	for (const FSuspenseCoreSaveHeader& Header : Headers)
	{
		HeaderMap.Add(Header.SlotIndex, Header);
	}

	// Clear and recreate widgets
	ClearSlotWidgets();
	CreateSlotWidgets();

	// Update each widget
	int32 WidgetIndex = 0;

	// Quick Save slot
	if (bShowQuickSaveSlot && SlotWidgets.IsValidIndex(WidgetIndex))
	{
		if (FSuspenseCoreSaveHeader* Header = HeaderMap.Find(QUICKSAVE_SLOT))
		{
			SlotWidgets[WidgetIndex]->InitializeSlot(QUICKSAVE_SLOT, *Header, false);
		}
		else
		{
			SlotWidgets[WidgetIndex]->SetEmpty(QUICKSAVE_SLOT);
		}
		WidgetIndex++;
	}

	// Auto Save slot
	if (bShowAutoSaveSlot && SlotWidgets.IsValidIndex(WidgetIndex))
	{
		if (FSuspenseCoreSaveHeader* Header = HeaderMap.Find(AUTOSAVE_SLOT))
		{
			SlotWidgets[WidgetIndex]->InitializeSlot(AUTOSAVE_SLOT, *Header, false);
		}
		else
		{
			SlotWidgets[WidgetIndex]->SetEmpty(AUTOSAVE_SLOT);
		}
		WidgetIndex++;
	}

	// Manual slots
	for (int32 i = 0; i < NumManualSlots && SlotWidgets.IsValidIndex(WidgetIndex); i++, WidgetIndex++)
	{
		if (FSuspenseCoreSaveHeader* Header = HeaderMap.Find(i))
		{
			SlotWidgets[WidgetIndex]->InitializeSlot(i, *Header, false);
		}
		else
		{
			SlotWidgets[WidgetIndex]->SetEmpty(i);
		}
	}

	UE_LOG(LogSuspenseCoreSaveLoadMenu, Log, TEXT("Refreshed %d slot widgets"), SlotWidgets.Num());
}

void USuspenseCoreSaveLoadMenuWidget::SelectSlot(int32 SlotIndex)
{
	// Deselect previous
	for (USuspenseCoreSaveSlotWidget* Widget : SlotWidgets)
	{
		if (Widget && Widget->GetSlotIndex() == SelectedSlotIndex)
		{
			Widget->SetSelected(false);
		}
	}

	SelectedSlotIndex = SlotIndex;

	// Find and select new slot
	for (USuspenseCoreSaveSlotWidget* Widget : SlotWidgets)
	{
		if (Widget && Widget->GetSlotIndex() == SlotIndex)
		{
			Widget->SetSelected(true);
			bSelectedSlotEmpty = Widget->IsSlotEmpty();
			break;
		}
	}

	UpdateActionButtonState();
	OnSlotSelectedEvent(SlotIndex, bSelectedSlotEmpty);
}

void USuspenseCoreSaveLoadMenuWidget::SetupButtonBindings()
{
	if (ActionButton)
	{
		ActionButton->OnClicked.AddDynamic(this, &USuspenseCoreSaveLoadMenuWidget::OnActionButtonClicked);
	}

	if (DeleteButton)
	{
		DeleteButton->OnClicked.AddDynamic(this, &USuspenseCoreSaveLoadMenuWidget::OnDeleteButtonClicked);
	}

	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &USuspenseCoreSaveLoadMenuWidget::OnCloseButtonClicked);
	}

	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.AddDynamic(this, &USuspenseCoreSaveLoadMenuWidget::OnConfirmButtonClicked);
	}

	if (CancelButton)
	{
		CancelButton->OnClicked.AddDynamic(this, &USuspenseCoreSaveLoadMenuWidget::OnCancelButtonClicked);
	}
}

USuspenseCoreSaveManager* USuspenseCoreSaveLoadMenuWidget::GetSaveManager()
{
	if (!CachedSaveManager.IsValid())
	{
		CachedSaveManager = USuspenseCoreSaveManager::Get(this);
	}
	return CachedSaveManager.Get();
}

void USuspenseCoreSaveLoadMenuWidget::CreateSlotWidgets()
{
	if (!SlotsContainer || !SaveSlotWidgetClass)
	{
		UE_LOG(LogSuspenseCoreSaveLoadMenu, Warning, TEXT("Cannot create slot widgets: missing container or widget class"));
		return;
	}

	int32 TotalSlots = NumManualSlots;
	if (bShowQuickSaveSlot) TotalSlots++;
	if (bShowAutoSaveSlot) TotalSlots++;

	for (int32 i = 0; i < TotalSlots; i++)
	{
		USuspenseCoreSaveSlotWidget* SlotWidget = CreateWidget<USuspenseCoreSaveSlotWidget>(this, SaveSlotWidgetClass);
		if (SlotWidget)
		{
			// Bind callbacks
			SlotWidget->OnSlotSelected.AddDynamic(this, &USuspenseCoreSaveLoadMenuWidget::OnSlotSelected);
			SlotWidget->OnDeleteRequested.AddDynamic(this, &USuspenseCoreSaveLoadMenuWidget::OnSlotDeleteRequested);

			SlotsContainer->AddChild(SlotWidget);
			SlotWidgets.Add(SlotWidget);
		}
	}
}

void USuspenseCoreSaveLoadMenuWidget::ClearSlotWidgets()
{
	for (USuspenseCoreSaveSlotWidget* Widget : SlotWidgets)
	{
		if (Widget)
		{
			Widget->OnSlotSelected.RemoveAll(this);
			Widget->OnDeleteRequested.RemoveAll(this);
			Widget->RemoveFromParent();
		}
	}
	SlotWidgets.Empty();
}

void USuspenseCoreSaveLoadMenuWidget::UpdateModeDisplay()
{
	if (TitleText)
	{
		TitleText->SetText(CurrentMode == ESuspenseCoreSaveLoadMode::Save ? SaveModeTitle : LoadModeTitle);
	}

	if (ActionButtonText)
	{
		ActionButtonText->SetText(CurrentMode == ESuspenseCoreSaveLoadMode::Save ? SaveButtonText : LoadButtonText);
	}

	// Delete button only in Load mode or when slot has data
	if (DeleteButton)
	{
		bool bCanDelete = SelectedSlotIndex >= 0 && !bSelectedSlotEmpty && SelectedSlotIndex != AUTOSAVE_SLOT;
		DeleteButton->SetVisibility(bCanDelete ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void USuspenseCoreSaveLoadMenuWidget::UpdateActionButtonState()
{
	if (!ActionButton)
	{
		return;
	}

	bool bCanAct = false;

	if (CurrentMode == ESuspenseCoreSaveLoadMode::Save)
	{
		// Can save to any selected slot (except AutoSave which is automatic)
		bCanAct = SelectedSlotIndex >= 0 && SelectedSlotIndex != AUTOSAVE_SLOT;
	}
	else // Load mode
	{
		// Can only load from non-empty slots
		bCanAct = SelectedSlotIndex >= 0 && !bSelectedSlotEmpty;
	}

	ActionButton->SetIsEnabled(bCanAct);

	// Update delete button
	if (DeleteButton)
	{
		bool bCanDelete = SelectedSlotIndex >= 0 && !bSelectedSlotEmpty && SelectedSlotIndex != AUTOSAVE_SLOT;
		DeleteButton->SetVisibility(bCanDelete ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		DeleteButton->SetIsEnabled(bCanDelete);
	}
}

void USuspenseCoreSaveLoadMenuWidget::ShowConfirmation(EPendingOperation Operation, int32 SlotIndex)
{
	PendingOperation = Operation;
	PendingOperationSlot = SlotIndex;

	if (ConfirmationOverlay)
	{
		ConfirmationOverlay->SetVisibility(ESlateVisibility::Visible);
	}

	if (ConfirmationText)
	{
		switch (Operation)
		{
		case EPendingOperation::Save:
			ConfirmationText->SetText(ConfirmOverwriteText);
			break;
		case EPendingOperation::Load:
			ConfirmationText->SetText(ConfirmLoadText);
			break;
		case EPendingOperation::Delete:
			ConfirmationText->SetText(ConfirmDeleteText);
			break;
		default:
			break;
		}
	}
}

void USuspenseCoreSaveLoadMenuWidget::HideConfirmation()
{
	PendingOperation = EPendingOperation::None;
	PendingOperationSlot = -1;

	if (ConfirmationOverlay)
	{
		ConfirmationOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void USuspenseCoreSaveLoadMenuWidget::ExecutePendingOperation()
{
	switch (PendingOperation)
	{
	case EPendingOperation::Save:
		PerformSave(PendingOperationSlot);
		break;
	case EPendingOperation::Load:
		PerformLoad(PendingOperationSlot);
		break;
	case EPendingOperation::Delete:
		PerformDelete(PendingOperationSlot);
		break;
	default:
		break;
	}

	HideConfirmation();
}

void USuspenseCoreSaveLoadMenuWidget::ShowStatus(const FText& Message)
{
	if (StatusText)
	{
		StatusText->SetText(Message);
		StatusText->SetVisibility(ESlateVisibility::Visible);
	}

	// Auto-hide after delay
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			StatusTimerHandle,
			this,
			&USuspenseCoreSaveLoadMenuWidget::HideStatus,
			StatusMessageDuration,
			false
		);
	}
}

void USuspenseCoreSaveLoadMenuWidget::HideStatus()
{
	if (StatusText)
	{
		StatusText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void USuspenseCoreSaveLoadMenuWidget::PerformSave(int32 SlotIndex)
{
	USuspenseCoreSaveManager* SaveMgr = GetSaveManager();
	if (!SaveMgr)
	{
		ShowStatus(OperationFailedText);
		OnOperationFailed(CurrentMode, SlotIndex, TEXT("SaveManager not available"));
		return;
	}

	FString SlotName = FString::Printf(TEXT("Manual Save %d"), SlotIndex + 1);
	SaveMgr->SaveToSlot(SlotIndex, SlotName);

	UE_LOG(LogSuspenseCoreSaveLoadMenu, Log, TEXT("Saving to slot %d"), SlotIndex);
}

void USuspenseCoreSaveLoadMenuWidget::PerformLoad(int32 SlotIndex)
{
	USuspenseCoreSaveManager* SaveMgr = GetSaveManager();
	if (!SaveMgr)
	{
		ShowStatus(OperationFailedText);
		OnOperationFailed(CurrentMode, SlotIndex, TEXT("SaveManager not available"));
		return;
	}

	SaveMgr->LoadFromSlot(SlotIndex);

	UE_LOG(LogSuspenseCoreSaveLoadMenu, Log, TEXT("Loading from slot %d"), SlotIndex);
}

void USuspenseCoreSaveLoadMenuWidget::PerformDelete(int32 SlotIndex)
{
	USuspenseCoreSaveManager* SaveMgr = GetSaveManager();
	if (!SaveMgr)
	{
		ShowStatus(OperationFailedText);
		return;
	}

	SaveMgr->DeleteSlot(SlotIndex);
	RefreshSlots();
	ShowStatus(FText::FromString(TEXT("Save deleted.")));
	UE_LOG(LogSuspenseCoreSaveLoadMenu, Log, TEXT("Deleted slot %d"), SlotIndex);
}

void USuspenseCoreSaveLoadMenuWidget::SetUIInputMode()
{
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(true);
	}
}

void USuspenseCoreSaveLoadMenuWidget::RestoreGameInputMode()
{
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(false);
	}
}

// ═══════════════════════════════════════════════════════════════════════════
// BUTTON HANDLERS
// ═══════════════════════════════════════════════════════════════════════════

void USuspenseCoreSaveLoadMenuWidget::OnActionButtonClicked()
{
	if (SelectedSlotIndex < 0)
	{
		return;
	}

	if (CurrentMode == ESuspenseCoreSaveLoadMode::Save)
	{
		// If slot is not empty, confirm overwrite
		if (!bSelectedSlotEmpty)
		{
			ShowConfirmation(EPendingOperation::Save, SelectedSlotIndex);
		}
		else
		{
			PerformSave(SelectedSlotIndex);
		}
	}
	else // Load mode
	{
		ShowConfirmation(EPendingOperation::Load, SelectedSlotIndex);
	}
}

void USuspenseCoreSaveLoadMenuWidget::OnDeleteButtonClicked()
{
	if (SelectedSlotIndex >= 0 && !bSelectedSlotEmpty)
	{
		ShowConfirmation(EPendingOperation::Delete, SelectedSlotIndex);
	}
}

void USuspenseCoreSaveLoadMenuWidget::OnCloseButtonClicked()
{
	HideMenu();
}

void USuspenseCoreSaveLoadMenuWidget::OnConfirmButtonClicked()
{
	ExecutePendingOperation();
}

void USuspenseCoreSaveLoadMenuWidget::OnCancelButtonClicked()
{
	HideConfirmation();
}

// ═══════════════════════════════════════════════════════════════════════════
// SLOT WIDGET CALLBACKS
// ═══════════════════════════════════════════════════════════════════════════

void USuspenseCoreSaveLoadMenuWidget::OnSlotSelected(int32 SlotIndex, bool bIsEmpty)
{
	SelectSlot(SlotIndex);
}

void USuspenseCoreSaveLoadMenuWidget::OnSlotDeleteRequested(int32 SlotIndex)
{
	ShowConfirmation(EPendingOperation::Delete, SlotIndex);
}

// ═══════════════════════════════════════════════════════════════════════════
// SAVE MANAGER CALLBACKS
// ═══════════════════════════════════════════════════════════════════════════

void USuspenseCoreSaveLoadMenuWidget::OnSaveCompleted(bool bSuccess, const FString& ErrorMessage)
{
	if (bSuccess)
	{
		ShowStatus(SaveSuccessText);
		RefreshSlots();
		OnOperationSucceeded(ESuspenseCoreSaveLoadMode::Save, SelectedSlotIndex);
		OnOperationCompleted.Broadcast(true, SaveSuccessText.ToString());
	}
	else
	{
		ShowStatus(OperationFailedText);
		OnOperationFailed(ESuspenseCoreSaveLoadMode::Save, SelectedSlotIndex, ErrorMessage);
		OnOperationCompleted.Broadcast(false, ErrorMessage);
	}
}

void USuspenseCoreSaveLoadMenuWidget::OnLoadCompleted(bool bSuccess, const FString& ErrorMessage)
{
	if (bSuccess)
	{
		ShowStatus(LoadSuccessText);
		OnOperationSucceeded(ESuspenseCoreSaveLoadMode::Load, SelectedSlotIndex);
		OnOperationCompleted.Broadcast(true, LoadSuccessText.ToString());

		// Hide menu after successful load
		HideMenu();
	}
	else
	{
		ShowStatus(OperationFailedText);
		OnOperationFailed(ESuspenseCoreSaveLoadMode::Load, SelectedSlotIndex, ErrorMessage);
		OnOperationCompleted.Broadcast(false, ErrorMessage);
	}
}
