// SuspenseCoreButtonWidget.cpp
// SuspenseCore - Universal Button Widget Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/Common/SuspenseCoreButtonWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

USuspenseCoreButtonWidget::USuspenseCoreButtonWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsEnabled = true;
	bIsHovered = false;
	bIsFocused = false;
}

void USuspenseCoreButtonWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Apply text in editor preview so designers can see the text immediately
	if (ButtonTextBlock && !ButtonText.IsEmpty())
	{
		ButtonTextBlock->SetText(ButtonText);
	}
}

void USuspenseCoreButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind button events if MainButton is valid
	if (MainButton)
	{
		MainButton->OnClicked.AddDynamic(this, &USuspenseCoreButtonWidget::OnMainButtonClicked);
		MainButton->OnHovered.AddDynamic(this, &USuspenseCoreButtonWidget::OnMainButtonHovered);
		MainButton->OnUnhovered.AddDynamic(this, &USuspenseCoreButtonWidget::OnMainButtonUnhovered);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreButton[%s]: MainButton not bound - Blueprint widget must have UButton named 'MainButton'!"), *GetName());
	}

	// Apply initial text at runtime
	if (ButtonTextBlock && !ButtonText.IsEmpty())
	{
		ButtonTextBlock->SetText(ButtonText);
	}
}

void USuspenseCoreButtonWidget::NativeDestruct()
{
	// Unbind button events
	if (MainButton)
	{
		MainButton->OnClicked.RemoveDynamic(this, &USuspenseCoreButtonWidget::OnMainButtonClicked);
		MainButton->OnHovered.RemoveDynamic(this, &USuspenseCoreButtonWidget::OnMainButtonHovered);
		MainButton->OnUnhovered.RemoveDynamic(this, &USuspenseCoreButtonWidget::OnMainButtonUnhovered);
	}

	Super::NativeDestruct();
}

FReply USuspenseCoreButtonWidget::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
	bIsFocused = true;
	K2_OnFocusChanged(true);
	return Super::NativeOnFocusReceived(InGeometry, InFocusEvent);
}

void USuspenseCoreButtonWidget::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
{
	bIsFocused = false;
	K2_OnFocusChanged(false);
	Super::NativeOnFocusLost(InFocusEvent);
}

void USuspenseCoreButtonWidget::SetButtonText(const FText& InText)
{
	ButtonText = InText;
	if (ButtonTextBlock)
	{
		ButtonTextBlock->SetText(ButtonText);
	}
}

void USuspenseCoreButtonWidget::SetButtonIcon(UTexture2D* InIcon)
{
	if (ButtonIcon)
	{
		if (InIcon)
		{
			ButtonIcon->SetBrushFromTexture(InIcon);
			ButtonIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			ButtonIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void USuspenseCoreButtonWidget::SetButtonEnabled(bool bEnabled)
{
	bIsEnabled = bEnabled;

	if (MainButton)
	{
		MainButton->SetIsEnabled(bEnabled);
	}
}

void USuspenseCoreButtonWidget::SimulateClick()
{
	if (bIsEnabled)
	{
		OnMainButtonClicked();
	}
}

void USuspenseCoreButtonWidget::OnMainButtonClicked()
{
	if (!bIsEnabled)
	{
		return;
	}

	// Play click sound
	PlaySound(ClickSound);

	// Broadcast delegate
	OnButtonClicked.Broadcast(this);

	// Call Blueprint event
	K2_OnClicked();
}

void USuspenseCoreButtonWidget::OnMainButtonHovered()
{
	bIsHovered = true;

	// Play hover sound
	PlaySound(HoverSound);

	// Broadcast delegate
	OnButtonHovered.Broadcast(this, true);

	// Call Blueprint event
	K2_OnHovered(true);
}

void USuspenseCoreButtonWidget::OnMainButtonUnhovered()
{
	bIsHovered = false;

	// Broadcast delegate
	OnButtonHovered.Broadcast(this, false);

	// Call Blueprint event
	K2_OnHovered(false);
}

void USuspenseCoreButtonWidget::PlaySound(USoundBase* Sound)
{
	if (Sound)
	{
		UGameplayStatics::PlaySound2D(this, Sound);
	}
}
