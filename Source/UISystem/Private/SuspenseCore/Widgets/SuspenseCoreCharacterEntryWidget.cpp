// SuspenseCoreCharacterEntryWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/SuspenseCoreCharacterEntryWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Border.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreCharacterEntry, Log, All);

USuspenseCoreCharacterEntryWidget::USuspenseCoreCharacterEntryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USuspenseCoreCharacterEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind button click events
	if (SelectButton)
	{
		SelectButton->OnClicked.AddDynamic(this, &USuspenseCoreCharacterEntryWidget::OnButtonClicked);
	}

	if (EntryButton)
	{
		EntryButton->OnClicked.AddDynamic(this, &USuspenseCoreCharacterEntryWidget::OnButtonClicked);
	}

	// Initial visual state
	UpdateVisualState();
}

void USuspenseCoreCharacterEntryWidget::NativeDestruct()
{
	// Unbind button events to prevent dangling delegates
	if (SelectButton)
	{
		SelectButton->OnClicked.RemoveDynamic(this, &USuspenseCoreCharacterEntryWidget::OnButtonClicked);
	}

	if (EntryButton)
	{
		EntryButton->OnClicked.RemoveDynamic(this, &USuspenseCoreCharacterEntryWidget::OnButtonClicked);
	}

	Super::NativeDestruct();
}

void USuspenseCoreCharacterEntryWidget::SetCharacterData(const FString& InPlayerId, const FString& InDisplayName, int32 InLevel, UTexture2D* InAvatarTexture)
{
	PlayerId = InPlayerId;
	DisplayName = InDisplayName;
	Level = InLevel;

	// Update display name
	if (DisplayNameText)
	{
		DisplayNameText->SetText(FText::FromString(InDisplayName));
	}

	// Update level text
	if (LevelText)
	{
		FString LevelString = FString::Printf(TEXT("Level %d"), InLevel);
		LevelText->SetText(FText::FromString(LevelString));
	}

	// Update avatar
	if (AvatarImage)
	{
		UTexture2D* TextureToUse = InAvatarTexture ? InAvatarTexture : DefaultAvatarTexture;
		if (TextureToUse)
		{
			AvatarImage->SetBrushFromTexture(TextureToUse);
		}
	}

	UE_LOG(LogSuspenseCoreCharacterEntry, Verbose,
		TEXT("Character entry set: %s (Lv.%d) - %s"),
		*InDisplayName, InLevel, *InPlayerId);
}

void USuspenseCoreCharacterEntryWidget::SetSelected(bool bInSelected)
{
	if (bIsSelected != bInSelected)
	{
		bIsSelected = bInSelected;
		UpdateVisualState();
		OnSelectionChanged(bIsSelected);

		UE_LOG(LogSuspenseCoreCharacterEntry, Verbose,
			TEXT("Character entry %s selection: %s"),
			*PlayerId, bIsSelected ? TEXT("Selected") : TEXT("Deselected"));
	}
}

void USuspenseCoreCharacterEntryWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	bIsHovered = true;
	UpdateVisualState();
}

void USuspenseCoreCharacterEntryWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bIsHovered = false;
	UpdateVisualState();
}

FReply USuspenseCoreCharacterEntryWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnButtonClicked();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void USuspenseCoreCharacterEntryWidget::UpdateVisualState()
{
	if (EntryBorder)
	{
		FLinearColor BorderColor;
		if (bIsSelected)
		{
			BorderColor = SelectedBorderColor;
		}
		else if (bIsHovered)
		{
			BorderColor = HoveredBorderColor;
		}
		else
		{
			BorderColor = NormalBorderColor;
		}

		EntryBorder->SetBrushColor(BorderColor);
	}
}

void USuspenseCoreCharacterEntryWidget::OnButtonClicked()
{
	UE_LOG(LogSuspenseCoreCharacterEntry, Log,
		TEXT("Character entry clicked: %s (%s)"),
		*DisplayName, *PlayerId);

	// Broadcast event
	OnEntryClicked.Broadcast(PlayerId);
}
