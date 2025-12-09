// SuspenseCoreSaveSlotWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/SuspenseCoreSaveSlotWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Border.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreSaveSlot, Log, All);

// Special slot indices (must match SuspenseCoreSaveManager constants)
static constexpr int32 AUTOSAVE_SLOT = 100;
static constexpr int32 QUICKSAVE_SLOT = 101;

USuspenseCoreSaveSlotWidget::USuspenseCoreSaveSlotWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USuspenseCoreSaveSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind button events
	if (SlotButton)
	{
		SlotButton->OnClicked.AddDynamic(this, &USuspenseCoreSaveSlotWidget::OnSlotButtonClicked);
		SlotButton->OnHovered.AddDynamic(this, &USuspenseCoreSaveSlotWidget::OnSlotButtonHovered);
		SlotButton->OnUnhovered.AddDynamic(this, &USuspenseCoreSaveSlotWidget::OnSlotButtonUnhovered);
	}

	if (DeleteButton)
	{
		DeleteButton->OnClicked.AddDynamic(this, &USuspenseCoreSaveSlotWidget::OnDeleteButtonClicked);
	}

	// Initial display
	UpdateDisplay();
}

void USuspenseCoreSaveSlotWidget::NativeDestruct()
{
	// Unbind events
	if (SlotButton)
	{
		SlotButton->OnClicked.RemoveAll(this);
		SlotButton->OnHovered.RemoveAll(this);
		SlotButton->OnUnhovered.RemoveAll(this);
	}

	if (DeleteButton)
	{
		DeleteButton->OnClicked.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void USuspenseCoreSaveSlotWidget::InitializeSlot(int32 InSlotIndex, const FSuspenseCoreSaveHeader& InHeader, bool bInIsEmpty)
{
	SlotIndex = InSlotIndex;
	bIsEmpty = bInIsEmpty;
	CachedHeader = InHeader;

	UpdateDisplay();

	UE_LOG(LogSuspenseCoreSaveSlot, Verbose, TEXT("Initialized slot %d: %s"),
		SlotIndex, bIsEmpty ? TEXT("Empty") : *CachedHeader.SlotName);
}

void USuspenseCoreSaveSlotWidget::SetEmpty(int32 InSlotIndex)
{
	SlotIndex = InSlotIndex;
	bIsEmpty = true;
	CachedHeader = FSuspenseCoreSaveHeader();

	UpdateDisplay();
}

void USuspenseCoreSaveSlotWidget::SetSlotData(int32 InSlotIndex, const FSuspenseCoreSaveHeader& InHeader)
{
	SlotIndex = InSlotIndex;
	bIsEmpty = false;
	CachedHeader = InHeader;

	UpdateDisplay();
}

void USuspenseCoreSaveSlotWidget::SetSelected(bool bSelected)
{
	bIsSelected = bSelected;

	if (SlotBorder)
	{
		SlotBorder->SetBrushColor(bIsSelected ? SelectedColor : NormalColor);
	}
}

void USuspenseCoreSaveSlotWidget::SetDeleteEnabled(bool bEnabled)
{
	if (DeleteButton)
	{
		DeleteButton->SetVisibility(bEnabled && !bIsEmpty ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void USuspenseCoreSaveSlotWidget::UpdateDisplay()
{
	// Slot name
	if (SlotNameText)
	{
		SlotNameText->SetText(FText::FromString(GetSlotDisplayName(SlotIndex)));
	}

	if (bIsEmpty)
	{
		// Show empty state
		if (EmptyText)
		{
			EmptyText->SetVisibility(ESlateVisibility::Visible);
			EmptyText->SetText(EmptySaveSlot);
		}

		// Hide data fields
		if (CharacterNameText) CharacterNameText->SetVisibility(ESlateVisibility::Collapsed);
		if (LevelText) LevelText->SetVisibility(ESlateVisibility::Collapsed);
		if (LocationText) LocationText->SetVisibility(ESlateVisibility::Collapsed);
		if (TimestampText) TimestampText->SetVisibility(ESlateVisibility::Collapsed);
		if (PlaytimeText) PlaytimeText->SetVisibility(ESlateVisibility::Collapsed);
		if (DeleteButton) DeleteButton->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		// Hide empty indicator
		if (EmptyText)
		{
			EmptyText->SetVisibility(ESlateVisibility::Collapsed);
		}

		// Show data
		if (CharacterNameText)
		{
			CharacterNameText->SetVisibility(ESlateVisibility::Visible);
			CharacterNameText->SetText(FText::FromString(CachedHeader.CharacterName));
		}

		if (LevelText)
		{
			LevelText->SetVisibility(ESlateVisibility::Visible);
			LevelText->SetText(FText::FromString(FString::Printf(TEXT("Lv. %d"), CachedHeader.CharacterLevel)));
		}

		if (LocationText)
		{
			LocationText->SetVisibility(ESlateVisibility::Visible);
			LocationText->SetText(FText::FromString(CachedHeader.LocationName));
		}

		if (TimestampText)
		{
			TimestampText->SetVisibility(ESlateVisibility::Visible);
			TimestampText->SetText(FText::FromString(FormatTimestamp(CachedHeader.SaveTimestamp)));
		}

		if (PlaytimeText)
		{
			PlaytimeText->SetVisibility(ESlateVisibility::Visible);
			PlaytimeText->SetText(FText::FromString(FormatPlaytime(CachedHeader.TotalPlayTimeSeconds)));
		}

		// Show delete button for non-special slots
		if (DeleteButton && SlotIndex != AUTOSAVE_SLOT)
		{
			DeleteButton->SetVisibility(ESlateVisibility::Visible);
		}
	}

	// Update border color
	if (SlotBorder)
	{
		SlotBorder->SetBrushColor(bIsSelected ? SelectedColor : NormalColor);
	}
}

FString USuspenseCoreSaveSlotWidget::FormatTimestamp(const FDateTime& Timestamp) const
{
	// Format: "Nov 29, 2025 15:30"
	return Timestamp.ToString(TEXT("%b %d, %Y %H:%M"));
}

FString USuspenseCoreSaveSlotWidget::FormatPlaytime(int64 TotalSeconds) const
{
	int32 Hours = TotalSeconds / 3600;
	int32 Minutes = (TotalSeconds % 3600) / 60;
	int32 Seconds = TotalSeconds % 60;

	if (Hours > 0)
	{
		return FString::Printf(TEXT("%dh %dm"), Hours, Minutes);
	}
	else if (Minutes > 0)
	{
		return FString::Printf(TEXT("%dm %ds"), Minutes, Seconds);
	}
	else
	{
		return FString::Printf(TEXT("%ds"), Seconds);
	}
}

FString USuspenseCoreSaveSlotWidget::GetSlotDisplayName(int32 Index) const
{
	switch (Index)
	{
	case QUICKSAVE_SLOT:
		return QuickSaveText.ToString();
	case AUTOSAVE_SLOT:
		return AutoSaveText.ToString();
	default:
		return FString::Printf(TEXT("Slot %d"), Index + 1);
	}
}

void USuspenseCoreSaveSlotWidget::OnSlotButtonClicked()
{
	UE_LOG(LogSuspenseCoreSaveSlot, Log, TEXT("Slot %d clicked (empty: %s)"),
		SlotIndex, bIsEmpty ? TEXT("true") : TEXT("false"));

	OnSlotSelectedEvent(SlotIndex, bIsEmpty);
	OnSlotSelected.Broadcast(SlotIndex, bIsEmpty);
}

void USuspenseCoreSaveSlotWidget::OnDeleteButtonClicked()
{
	UE_LOG(LogSuspenseCoreSaveSlot, Log, TEXT("Delete requested for slot %d"), SlotIndex);

	OnDeleteRequestedEvent(SlotIndex);
	OnDeleteRequested.Broadcast(SlotIndex);
}

void USuspenseCoreSaveSlotWidget::OnSlotButtonHovered()
{
	if (!bIsSelected && SlotBorder)
	{
		SlotBorder->SetBrushColor(HoveredColor);
	}
}

void USuspenseCoreSaveSlotWidget::OnSlotButtonUnhovered()
{
	if (!bIsSelected && SlotBorder)
	{
		SlotBorder->SetBrushColor(NormalColor);
	}
}
