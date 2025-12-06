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
	// Set default values
	bIsEnabled = true;
	bIsHovered = false;
	bIsPressed = false;
	bIsFocused = false;
}

void USuspenseCoreButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Debug: Log binding status
	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreButton[%s]: MainButton=%s, ButtonTextBlock=%s, ButtonIcon=%s, ButtonText='%s'"),
		*GetName(),
		MainButton ? TEXT("BOUND") : TEXT("NULL"),
		ButtonTextBlock ? TEXT("BOUND") : TEXT("NULL"),
		ButtonIcon ? TEXT("BOUND") : TEXT("NULL"),
		*ButtonText.ToString());

	// Bind button events if MainButton is valid
	if (MainButton)
	{
		MainButton->OnClicked.AddDynamic(this, &USuspenseCoreButtonWidget::OnMainButtonClicked);
		MainButton->OnHovered.AddDynamic(this, &USuspenseCoreButtonWidget::OnMainButtonHovered);
		MainButton->OnUnhovered.AddDynamic(this, &USuspenseCoreButtonWidget::OnMainButtonUnhovered);
		MainButton->OnPressed.AddDynamic(this, &USuspenseCoreButtonWidget::OnMainButtonPressed);
		MainButton->OnReleased.AddDynamic(this, &USuspenseCoreButtonWidget::OnMainButtonReleased);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreButton[%s]: MainButton not bound - Blueprint widget must have UButton named 'MainButton'!"), *GetName());
	}

	// Apply initial text
	if (ButtonTextBlock)
	{
		if (!ButtonText.IsEmpty())
		{
			ButtonTextBlock->SetText(ButtonText);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreButton[%s]: ButtonTextBlock not bound - Blueprint widget must have UTextBlock named 'ButtonTextBlock'!"), *GetName());
	}

	// Apply initial style
	ApplyStyle();
}

void USuspenseCoreButtonWidget::NativeDestruct()
{
	// Unbind button events
	if (MainButton)
	{
		MainButton->OnClicked.RemoveDynamic(this, &USuspenseCoreButtonWidget::OnMainButtonClicked);
		MainButton->OnHovered.RemoveDynamic(this, &USuspenseCoreButtonWidget::OnMainButtonHovered);
		MainButton->OnUnhovered.RemoveDynamic(this, &USuspenseCoreButtonWidget::OnMainButtonUnhovered);
		MainButton->OnPressed.RemoveDynamic(this, &USuspenseCoreButtonWidget::OnMainButtonPressed);
		MainButton->OnReleased.RemoveDynamic(this, &USuspenseCoreButtonWidget::OnMainButtonReleased);
	}

	Super::NativeDestruct();
}

FReply USuspenseCoreButtonWidget::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
	bIsFocused = true;
	UpdateVisualState();
	K2_OnFocusChanged(true);
	return Super::NativeOnFocusReceived(InGeometry, InFocusEvent);
}

void USuspenseCoreButtonWidget::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
{
	bIsFocused = false;
	UpdateVisualState();
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

	UpdateVisualState();
}

void USuspenseCoreButtonWidget::SetButtonStyle(ESuspenseCoreButtonStyle NewStyle)
{
	Style = NewStyle;
	ApplyStyle();
}

void USuspenseCoreButtonWidget::SetCustomColors(const FSuspenseCoreButtonColors& NewColors)
{
	CustomColors = NewColors;
	if (Style == ESuspenseCoreButtonStyle::Custom)
	{
		ApplyStyle();
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

	UpdateVisualState();

	// Broadcast delegate
	OnButtonHovered.Broadcast(this, true);

	// Call Blueprint event
	K2_OnHovered(true);
}

void USuspenseCoreButtonWidget::OnMainButtonUnhovered()
{
	bIsHovered = false;
	UpdateVisualState();

	// Broadcast delegate
	OnButtonHovered.Broadcast(this, false);

	// Call Blueprint event
	K2_OnHovered(false);
}

void USuspenseCoreButtonWidget::OnMainButtonPressed()
{
	bIsPressed = true;
	UpdateVisualState();
}

void USuspenseCoreButtonWidget::OnMainButtonReleased()
{
	bIsPressed = false;
	UpdateVisualState();
}

void USuspenseCoreButtonWidget::ApplyStyle()
{
	FSuspenseCoreButtonColors Colors = GetStyleColors();

	// Apply colors to button background
	if (MainButton)
	{
		FButtonStyle ButtonStyle = MainButton->GetStyle();

		// Normal state
		ButtonStyle.Normal.TintColor = FSlateColor(Colors.NormalBackground);

		// Hovered state
		ButtonStyle.Hovered.TintColor = FSlateColor(Colors.HoveredBackground);

		// Pressed state
		ButtonStyle.Pressed.TintColor = FSlateColor(Colors.PressedBackground);

		// Disabled state
		ButtonStyle.Disabled.TintColor = FSlateColor(Colors.DisabledBackground);

		MainButton->SetStyle(ButtonStyle);
	}

	// Apply text color
	UpdateVisualState();
}

FSuspenseCoreButtonColors USuspenseCoreButtonWidget::GetStyleColors() const
{
	switch (Style)
	{
	case ESuspenseCoreButtonStyle::Primary:
		{
			FSuspenseCoreButtonColors Colors;
			Colors.NormalBackground = FLinearColor(0.8f, 0.6f, 0.2f, 0.9f);  // Gold
			Colors.HoveredBackground = FLinearColor(0.9f, 0.7f, 0.3f, 1.0f);
			Colors.PressedBackground = FLinearColor(0.6f, 0.45f, 0.15f, 1.0f);
			Colors.DisabledBackground = FLinearColor(0.4f, 0.3f, 0.1f, 0.5f);
			Colors.TextColor = FLinearColor::Black;
			Colors.DisabledTextColor = FLinearColor(0.3f, 0.3f, 0.3f, 1.0f);
			Colors.AccentColor = FLinearColor(1.0f, 0.8f, 0.4f, 1.0f);
			return Colors;
		}

	case ESuspenseCoreButtonStyle::Secondary:
		{
			FSuspenseCoreButtonColors Colors;
			Colors.NormalBackground = FLinearColor(0.2f, 0.2f, 0.25f, 0.9f);  // Dark gray
			Colors.HoveredBackground = FLinearColor(0.3f, 0.3f, 0.35f, 1.0f);
			Colors.PressedBackground = FLinearColor(0.15f, 0.15f, 0.18f, 1.0f);
			Colors.DisabledBackground = FLinearColor(0.15f, 0.15f, 0.15f, 0.5f);
			Colors.TextColor = FLinearColor::White;
			Colors.DisabledTextColor = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);
			Colors.AccentColor = FLinearColor(0.5f, 0.5f, 0.55f, 1.0f);
			return Colors;
		}

	case ESuspenseCoreButtonStyle::Tertiary:
		{
			FSuspenseCoreButtonColors Colors;
			Colors.NormalBackground = FLinearColor(0.1f, 0.1f, 0.1f, 0.5f);  // Subtle
			Colors.HoveredBackground = FLinearColor(0.15f, 0.15f, 0.15f, 0.7f);
			Colors.PressedBackground = FLinearColor(0.08f, 0.08f, 0.08f, 0.8f);
			Colors.DisabledBackground = FLinearColor(0.1f, 0.1f, 0.1f, 0.3f);
			Colors.TextColor = FLinearColor(0.8f, 0.8f, 0.8f, 1.0f);
			Colors.DisabledTextColor = FLinearColor(0.4f, 0.4f, 0.4f, 1.0f);
			Colors.AccentColor = FLinearColor(0.6f, 0.6f, 0.6f, 1.0f);
			return Colors;
		}

	case ESuspenseCoreButtonStyle::Danger:
		{
			FSuspenseCoreButtonColors Colors;
			Colors.NormalBackground = FLinearColor(0.7f, 0.15f, 0.15f, 0.9f);  // Red
			Colors.HoveredBackground = FLinearColor(0.85f, 0.2f, 0.2f, 1.0f);
			Colors.PressedBackground = FLinearColor(0.5f, 0.1f, 0.1f, 1.0f);
			Colors.DisabledBackground = FLinearColor(0.35f, 0.1f, 0.1f, 0.5f);
			Colors.TextColor = FLinearColor::White;
			Colors.DisabledTextColor = FLinearColor(0.7f, 0.7f, 0.7f, 1.0f);
			Colors.AccentColor = FLinearColor(1.0f, 0.3f, 0.3f, 1.0f);
			return Colors;
		}

	case ESuspenseCoreButtonStyle::Success:
		{
			FSuspenseCoreButtonColors Colors;
			Colors.NormalBackground = FLinearColor(0.15f, 0.6f, 0.25f, 0.9f);  // Green
			Colors.HoveredBackground = FLinearColor(0.2f, 0.7f, 0.3f, 1.0f);
			Colors.PressedBackground = FLinearColor(0.1f, 0.45f, 0.18f, 1.0f);
			Colors.DisabledBackground = FLinearColor(0.1f, 0.3f, 0.12f, 0.5f);
			Colors.TextColor = FLinearColor::White;
			Colors.DisabledTextColor = FLinearColor(0.7f, 0.7f, 0.7f, 1.0f);
			Colors.AccentColor = FLinearColor(0.3f, 0.9f, 0.4f, 1.0f);
			return Colors;
		}

	case ESuspenseCoreButtonStyle::Ghost:
		{
			FSuspenseCoreButtonColors Colors;
			Colors.NormalBackground = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);  // Transparent
			Colors.HoveredBackground = FLinearColor(0.2f, 0.2f, 0.2f, 0.3f);
			Colors.PressedBackground = FLinearColor(0.15f, 0.15f, 0.15f, 0.5f);
			Colors.DisabledBackground = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
			Colors.TextColor = FLinearColor(0.8f, 0.6f, 0.2f, 1.0f);  // Gold text
			Colors.DisabledTextColor = FLinearColor(0.4f, 0.4f, 0.4f, 1.0f);
			Colors.AccentColor = FLinearColor(0.8f, 0.6f, 0.2f, 1.0f);
			return Colors;
		}

	case ESuspenseCoreButtonStyle::Custom:
	default:
		return CustomColors;
	}
}

void USuspenseCoreButtonWidget::UpdateVisualState()
{
	FSuspenseCoreButtonColors Colors = GetStyleColors();
	FLinearColor TextColor;

	if (!bIsEnabled)
	{
		TextColor = Colors.DisabledTextColor;
	}
	else
	{
		TextColor = Colors.TextColor;
	}

	// Update text color
	if (ButtonTextBlock)
	{
		ButtonTextBlock->SetColorAndOpacity(FSlateColor(TextColor));
	}

	// Update icon color (tint)
	if (ButtonIcon)
	{
		ButtonIcon->SetColorAndOpacity(TextColor);
	}
}

void USuspenseCoreButtonWidget::PlaySound(USoundBase* Sound)
{
	if (Sound)
	{
		UGameplayStatics::PlaySound2D(this, Sound);
	}
}
