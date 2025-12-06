// SuspenseCoreMenuWidget.cpp
// SuspenseCore - Base Menu Widget with Procedural Buttons
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/Common/SuspenseCoreMenuWidget.h"
#include "SuspenseCore/Widgets/Common/SuspenseCoreButtonWidget.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"

USuspenseCoreMenuWidget::USuspenseCoreMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ButtonSpacing(10.0f)
{
}

void USuspenseCoreMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Build buttons on construct
	RebuildButtons();
}

void USuspenseCoreMenuWidget::NativeDestruct()
{
	ClearButtons();
	Super::NativeDestruct();
}

void USuspenseCoreMenuWidget::RebuildButtons()
{
	ClearButtons();

	// Use provided configs or get defaults
	TArray<FSuspenseCoreMenuButtonConfig> ConfigsToUse = ButtonConfigs;
	if (ConfigsToUse.Num() == 0)
	{
		ConfigsToUse = GetDefaultButtonConfigs();
	}

	// Sort by SortOrder
	ConfigsToUse.Sort([](const FSuspenseCoreMenuButtonConfig& A, const FSuspenseCoreMenuButtonConfig& B)
	{
		return A.SortOrder < B.SortOrder;
	});

	// Create buttons
	for (const FSuspenseCoreMenuButtonConfig& Config : ConfigsToUse)
	{
		CreateButton(Config);
	}
}

USuspenseCoreButtonWidget* USuspenseCoreMenuWidget::GetButtonByTag(FGameplayTag ActionTag) const
{
	const TObjectPtr<USuspenseCoreButtonWidget>* Found = ButtonMap.Find(ActionTag);
	return Found ? Found->Get() : nullptr;
}

void USuspenseCoreMenuWidget::SetButtonEnabled(FGameplayTag ActionTag, bool bEnabled)
{
	if (USuspenseCoreButtonWidget* Button = GetButtonByTag(ActionTag))
	{
		Button->SetButtonEnabled(bEnabled);
	}
}

USuspenseCoreButtonWidget* USuspenseCoreMenuWidget::AddButton(const FSuspenseCoreMenuButtonConfig& Config)
{
	// Remove existing if present
	RemoveButton(Config.ActionTag);

	return CreateButton(Config);
}

void USuspenseCoreMenuWidget::RemoveButton(FGameplayTag ActionTag)
{
	if (TObjectPtr<USuspenseCoreButtonWidget>* Found = ButtonMap.Find(ActionTag))
	{
		if (USuspenseCoreButtonWidget* Button = Found->Get())
		{
			Button->OnButtonClicked.RemoveDynamic(this, &USuspenseCoreMenuWidget::OnButtonClicked);
			Button->RemoveFromParent();
		}
		ButtonMap.Remove(ActionTag);
	}
}

USuspenseCoreButtonWidget* USuspenseCoreMenuWidget::CreateButton(const FSuspenseCoreMenuButtonConfig& Config)
{
	if (!ButtonWidgetClass || !ButtonContainer)
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreMenuWidget: ButtonWidgetClass or ButtonContainer not set!"));
		return nullptr;
	}

	if (!Config.ActionTag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreMenuWidget: Button config has invalid ActionTag!"));
		return nullptr;
	}

	// Create button widget
	USuspenseCoreButtonWidget* Button = CreateWidget<USuspenseCoreButtonWidget>(GetOwningPlayer(), ButtonWidgetClass);
	if (!Button)
	{
		return nullptr;
	}

	// Configure button
	Button->SetButtonText(Config.ButtonText);
	Button->SetButtonStyle(Config.Style);
	Button->SetButtonEnabled(Config.bEnabled);
	Button->SetActionTag(Config.ActionTag);

	if (Config.Icon)
	{
		Button->SetButtonIcon(Config.Icon);
	}

	if (!Config.Tooltip.IsEmpty())
	{
		Button->SetTooltipText(Config.Tooltip);
	}

	// Bind click event
	Button->OnButtonClicked.AddDynamic(this, &USuspenseCoreMenuWidget::OnButtonClicked);

	// Allow customization before adding
	OnButtonCreated(Button, Config);

	// Add to container with spacing
	if (UVerticalBox* VBox = Cast<UVerticalBox>(ButtonContainer))
	{
		UVerticalBoxSlot* ButtonSlot = VBox->AddChildToVerticalBox(Button);
		if (ButtonSlot)
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, ButtonSpacing));
		}
	}
	else if (UHorizontalBox* HBox = Cast<UHorizontalBox>(ButtonContainer))
	{
		UHorizontalBoxSlot* ButtonSlot = HBox->AddChildToHorizontalBox(Button);
		if (ButtonSlot)
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, ButtonSpacing, 0.0f));
		}
	}
	else
	{
		ButtonContainer->AddChild(Button);
	}

	// Store reference
	ButtonMap.Add(Config.ActionTag, Button);

	return Button;
}

void USuspenseCoreMenuWidget::ClearButtons()
{
	for (auto& Pair : ButtonMap)
	{
		if (USuspenseCoreButtonWidget* Button = Pair.Value.Get())
		{
			Button->OnButtonClicked.RemoveDynamic(this, &USuspenseCoreMenuWidget::OnButtonClicked);
			Button->RemoveFromParent();
		}
	}
	ButtonMap.Empty();
}

void USuspenseCoreMenuWidget::OnButtonClicked(USuspenseCoreButtonWidget* Button)
{
	if (!Button)
	{
		return;
	}

	FGameplayTag ActionTag = Button->GetActionTag();

	// Call handler (can be overridden)
	HandleButtonAction(ActionTag, Button);

	// Broadcast delegate
	OnMenuButtonClicked.Broadcast(ActionTag, Button);
}

TArray<FSuspenseCoreMenuButtonConfig> USuspenseCoreMenuWidget::GetDefaultButtonConfigs_Implementation() const
{
	// Override in derived classes to provide default buttons
	return TArray<FSuspenseCoreMenuButtonConfig>();
}

void USuspenseCoreMenuWidget::HandleButtonAction_Implementation(FGameplayTag ActionTag, USuspenseCoreButtonWidget* Button)
{
	// Override in derived classes to handle button actions
	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMenuWidget: Button action '%s' clicked"), *ActionTag.ToString());
}

void USuspenseCoreMenuWidget::OnButtonCreated_Implementation(USuspenseCoreButtonWidget* Button, const FSuspenseCoreMenuButtonConfig& Config)
{
	// Override in derived classes to customize buttons after creation
}
