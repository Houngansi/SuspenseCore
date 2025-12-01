// SuspenseCoreClassSelectionButtonWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/SuspenseCoreClassSelectionButtonWidget.h"
#include "SuspenseCore/Subsystems/SuspenseCoreCharacterClassSubsystem.h"
#include "SuspenseCore/Data/SuspenseCoreCharacterClassData.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Border.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreClassButton, Log, All);

USuspenseCoreClassSelectionButtonWidget::USuspenseCoreClassSelectionButtonWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USuspenseCoreClassSelectionButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind button click event
	if (SelectButton)
	{
		SelectButton->OnClicked.AddDynamic(this, &USuspenseCoreClassSelectionButtonWidget::OnButtonClicked);
	}

	// Initial visual state
	UpdateVisualState();
}

void USuspenseCoreClassSelectionButtonWidget::NativeDestruct()
{
	// Unbind button event to prevent dangling delegates
	if (SelectButton)
	{
		SelectButton->OnClicked.RemoveDynamic(this, &USuspenseCoreClassSelectionButtonWidget::OnButtonClicked);
	}

	Super::NativeDestruct();
}

void USuspenseCoreClassSelectionButtonWidget::SetClassData(USuspenseCoreCharacterClassData* InClassData)
{
	if (!InClassData)
	{
		UE_LOG(LogSuspenseCoreClassButton, Warning, TEXT("SetClassData called with null ClassData"));
		return;
	}

	CachedClassData = InClassData;
	ClassId = InClassData->ClassID.ToString();

	// Update class name text
	if (ClassNameText)
	{
		ClassNameText->SetText(InClassData->DisplayName);
		ClassNameText->SetColorAndOpacity(FSlateColor(InClassData->PrimaryColor));
	}

	// Update class icon
	if (ClassIconImage)
	{
		if (UTexture2D* IconTexture = InClassData->ClassIcon.LoadSynchronous())
		{
			ClassIconImage->SetBrushFromTexture(IconTexture);
			ClassIconImage->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			ClassIconImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	UE_LOG(LogSuspenseCoreClassButton, Verbose,
		TEXT("Class button configured: %s (%s)"),
		*InClassData->DisplayName.ToString(), *ClassId);
}

void USuspenseCoreClassSelectionButtonWidget::SetClassById(const FString& InClassId)
{
	ClassId = InClassId;

	USuspenseCoreCharacterClassSubsystem* ClassSubsystem = USuspenseCoreCharacterClassSubsystem::Get(this);
	if (ClassSubsystem)
	{
		USuspenseCoreCharacterClassData* ClassData = ClassSubsystem->GetClassById(FName(*InClassId));
		if (ClassData)
		{
			SetClassData(ClassData);
			return;
		}
	}

	// Fallback if class data not found
	if (ClassNameText)
	{
		ClassNameText->SetText(FText::FromString(InClassId));
	}

	if (ClassIconImage)
	{
		ClassIconImage->SetVisibility(ESlateVisibility::Collapsed);
	}

	UE_LOG(LogSuspenseCoreClassButton, Warning,
		TEXT("ClassData not found for ID: %s - using fallback display"),
		*InClassId);
}

void USuspenseCoreClassSelectionButtonWidget::SetSelected(bool bInSelected)
{
	if (bIsSelected != bInSelected)
	{
		bIsSelected = bInSelected;
		UpdateVisualState();
		OnSelectionChanged(bIsSelected);

		UE_LOG(LogSuspenseCoreClassButton, Verbose,
			TEXT("Class button %s selection: %s"),
			*ClassId, bIsSelected ? TEXT("Selected") : TEXT("Deselected"));
	}
}

void USuspenseCoreClassSelectionButtonWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	bIsHovered = true;
	UpdateVisualState();
}

void USuspenseCoreClassSelectionButtonWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bIsHovered = false;
	UpdateVisualState();
}

void USuspenseCoreClassSelectionButtonWidget::UpdateVisualState()
{
	// Update border color based on state
	if (ButtonBorder)
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

		ButtonBorder->SetBrushColor(BorderColor);
	}

	// Update button style based on selection
	if (SelectButton)
	{
		FButtonStyle Style = SelectButton->GetStyle();
		if (bIsSelected)
		{
			Style.Normal.TintColor = FSlateColor(SelectedBorderColor);
		}
		else
		{
			Style.Normal.TintColor = FSlateColor(NormalBorderColor);
		}
		SelectButton->SetStyle(Style);
	}
}

void USuspenseCoreClassSelectionButtonWidget::OnButtonClicked()
{
	UE_LOG(LogSuspenseCoreClassButton, Log,
		TEXT("Class button clicked: %s"),
		*ClassId);

	// Broadcast event with class ID
	OnClassButtonClicked.Broadcast(ClassId);
}
