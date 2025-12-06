// SuspenseCorePanelSwitcherWidget.cpp
// SuspenseCore - Panel Switcher (Tab Bar) Widget Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/Layout/SuspenseCorePanelSwitcherWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"

//==================================================================
// Constructor
//==================================================================

USuspenseCorePanelSwitcherWidget::USuspenseCorePanelSwitcherWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, NextTabKey(EKeys::Tab)
	, PreviousTabKey(EKeys::LeftShift) // Shift+Tab handled separately
{
}

//==================================================================
// UUserWidget Interface
//==================================================================

void USuspenseCorePanelSwitcherWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Setup EventBus subscriptions
	SetupEventSubscriptions();

	// FALLBACK: If TabContainer is not bound in Blueprint, create it dynamically
	if (!TabContainer)
	{
		TabContainer = NewObject<UHorizontalBox>(this, TEXT("TabContainer"));
		if (TabContainer)
		{
			// Get root widget and add TabContainer to it
			UWidget* RootWidget = GetRootWidget();
			if (UPanelWidget* RootPanel = Cast<UPanelWidget>(RootWidget))
			{
				RootPanel->AddChild(TabContainer);
				UE_LOG(LogTemp, Warning, TEXT("PanelSwitcher: TabContainer was not bound! Created dynamically. Please bind TabContainer in Blueprint for proper styling."));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("PanelSwitcher: Could not create TabContainer - root widget is not a panel!"));
			}
		}
	}
}

void USuspenseCorePanelSwitcherWidget::NativeDestruct()
{
	// Teardown EventBus subscriptions
	TeardownEventSubscriptions();

	// Clear all tabs
	ClearTabs();

	Super::NativeDestruct();
}

FReply USuspenseCorePanelSwitcherWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Handle tab navigation
	if (InKeyEvent.GetKey() == NextTabKey)
	{
		SelectNextTab();
		return FReply::Handled();
	}
	else if (InKeyEvent.GetKey() == PreviousTabKey)
	{
		SelectPreviousTab();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

//==================================================================
// Tab Management
//==================================================================

void USuspenseCorePanelSwitcherWidget::AddTab(const FGameplayTag& PanelTag, const FText& DisplayName)
{
	if (!PanelTag.IsValid() || !TabContainer)
	{
		UE_LOG(LogTemp, Warning, TEXT("AddTab: Invalid tag or TabContainer is null"));
		return;
	}

	// Check if tab already exists
	if (TabIndexByTag.Contains(PanelTag))
	{
		UE_LOG(LogTemp, Warning, TEXT("AddTab: Tab '%s' already exists"), *PanelTag.ToString());
		return;
	}

	// Create tab data
	FSuspenseCorePanelTab TabData(PanelTag, DisplayName);

	// Create tab button
	TabData.TabButton = CreateTabButton(TabData);
	if (!TabData.TabButton)
	{
		UE_LOG(LogTemp, Warning, TEXT("AddTab: Failed to create button for '%s'"), *PanelTag.ToString());
		return;
	}

	// Add button-to-tag mapping for click handling
	ButtonToTagMap.Add(TabData.TabButton, PanelTag);

	// Add to container
	if (UHorizontalBoxSlot* TabSlot = TabContainer->AddChildToHorizontalBox(TabData.TabButton))
	{
		TabSlot->SetHorizontalAlignment(HAlign_Fill);
		TabSlot->SetVerticalAlignment(VAlign_Fill);
		TabSlot->SetPadding(FMargin(4.0f, 0.0f));
	}

	// Track tab
	int32 TabIndex = Tabs.Num();
	Tabs.Add(TabData);
	TabIndexByTag.Add(PanelTag, TabIndex);

	UE_LOG(LogTemp, Log, TEXT("AddTab: Created tab '%s' at index %d"), *PanelTag.ToString(), TabIndex);

	// Notify Blueprint
	K2_OnTabCreated(TabData);

	// Update visual (deselected by default)
	UpdateTabVisual(TabData, false);
}

void USuspenseCorePanelSwitcherWidget::RemoveTab(const FGameplayTag& PanelTag)
{
	int32* FoundIndex = TabIndexByTag.Find(PanelTag);
	if (!FoundIndex)
	{
		return;
	}

	int32 TabIndex = *FoundIndex;
	if (TabIndex >= 0 && TabIndex < Tabs.Num())
	{
		FSuspenseCorePanelTab& Tab = Tabs[TabIndex];

		// Remove button from container and clean up
		if (Tab.TabButton)
		{
			// Remove dynamic delegate binding
			Tab.TabButton->OnClicked.RemoveDynamic(this, &USuspenseCorePanelSwitcherWidget::HandleTabButtonClicked);
			ButtonToTagMap.Remove(Tab.TabButton);
			Tab.TabButton->RemoveFromParent();
		}

		// Remove from tracking
		Tabs.RemoveAt(TabIndex);
		TabIndexByTag.Remove(PanelTag);

		// Update indices for remaining tabs
		for (auto& Pair : TabIndexByTag)
		{
			if (Pair.Value > TabIndex)
			{
				Pair.Value--;
			}
		}
	}
}

void USuspenseCorePanelSwitcherWidget::ClearTabs()
{
	// Remove all tab buttons
	for (FSuspenseCorePanelTab& Tab : Tabs)
	{
		if (Tab.TabButton)
		{
			// Remove dynamic delegate binding
			Tab.TabButton->OnClicked.RemoveDynamic(this, &USuspenseCorePanelSwitcherWidget::HandleTabButtonClicked);
			Tab.TabButton->RemoveFromParent();
		}
	}

	Tabs.Empty();
	TabIndexByTag.Empty();
	ButtonToTagMap.Empty();
	ActivePanelTag = FGameplayTag();

	if (TabContainer)
	{
		TabContainer->ClearChildren();
	}
}

//==================================================================
// Selection
//==================================================================

void USuspenseCorePanelSwitcherWidget::SetActivePanel(const FGameplayTag& PanelTag)
{
	if (ActivePanelTag == PanelTag)
	{
		return;
	}

	// Update previous tab visual
	if (ActivePanelTag.IsValid())
	{
		int32* PrevIndex = TabIndexByTag.Find(ActivePanelTag);
		if (PrevIndex && *PrevIndex >= 0 && *PrevIndex < Tabs.Num())
		{
			UpdateTabVisual(Tabs[*PrevIndex], false);
		}
	}

	ActivePanelTag = PanelTag;

	// Update new tab visual
	if (ActivePanelTag.IsValid())
	{
		int32* NewIndex = TabIndexByTag.Find(ActivePanelTag);
		if (NewIndex && *NewIndex >= 0 && *NewIndex < Tabs.Num())
		{
			UpdateTabVisual(Tabs[*NewIndex], true);
		}
	}

	// Notify Blueprint
	K2_OnTabSelected(PanelTag);
}

void USuspenseCorePanelSwitcherWidget::SelectNextTab()
{
	if (Tabs.Num() == 0)
	{
		return;
	}

	int32 CurrentIndex = 0;
	if (ActivePanelTag.IsValid())
	{
		int32* FoundIndex = TabIndexByTag.Find(ActivePanelTag);
		if (FoundIndex)
		{
			CurrentIndex = *FoundIndex;
		}
	}

	// Move to next (wrap around)
	int32 NextIndex = (CurrentIndex + 1) % Tabs.Num();
	FGameplayTag NextTag = Tabs[NextIndex].PanelTag;

	// Publish via EventBus
	PublishPanelSelected(NextTag);
}

void USuspenseCorePanelSwitcherWidget::SelectPreviousTab()
{
	if (Tabs.Num() == 0)
	{
		return;
	}

	int32 CurrentIndex = 0;
	if (ActivePanelTag.IsValid())
	{
		int32* FoundIndex = TabIndexByTag.Find(ActivePanelTag);
		if (FoundIndex)
		{
			CurrentIndex = *FoundIndex;
		}
	}

	// Move to previous (wrap around)
	int32 PrevIndex = (CurrentIndex - 1 + Tabs.Num()) % Tabs.Num();
	FGameplayTag PrevTag = Tabs[PrevIndex].PanelTag;

	// Publish via EventBus
	PublishPanelSelected(PrevTag);
}

//==================================================================
// Tab Creation
//==================================================================

UButton* USuspenseCorePanelSwitcherWidget::CreateTabButton_Implementation(const FSuspenseCorePanelTab& TabData)
{
	// Create button
	UButton* Button = NewObject<UButton>(this);
	if (!Button)
	{
		return nullptr;
	}

	// Create text inside button
	UTextBlock* Text = NewObject<UTextBlock>(Button);
	if (Text)
	{
		Text->SetText(TabData.DisplayName);
		Text->SetJustification(ETextJustify::Center);
		Button->AddChild(Text);
	}

	// CRITICAL: Store button->tag mapping BEFORE binding click event
	// This is used in HandleTabButtonClicked to identify which button was clicked
	ButtonToTagMap.Add(Button, TabData.PanelTag);

	// Bind click event - HandleTabButtonClicked will use ButtonToTagMap to find the tag
	Button->OnClicked.AddDynamic(this, &USuspenseCorePanelSwitcherWidget::HandleTabButtonClicked);

	return Button;
}

void USuspenseCorePanelSwitcherWidget::UpdateTabVisual_Implementation(const FSuspenseCorePanelTab& Tab, bool bIsSelected)
{
	if (!Tab.TabButton)
	{
		return;
	}

	// Apply appropriate style
	if (bIsSelected)
	{
		Tab.TabButton->SetStyle(SelectedTabStyle);
	}
	else
	{
		Tab.TabButton->SetStyle(NormalTabStyle);
	}

	// Update text if available
	if (Tab.TabText)
	{
		// Could change text color, font, etc. for selected state
	}
}

void USuspenseCorePanelSwitcherWidget::HandleTabButtonClicked()
{
	// Find which button was clicked using FSlateApplication focused widget
	if (FSlateApplication::IsInitialized())
	{
		TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetUserFocusedWidget(0);

		for (const auto& Pair : ButtonToTagMap)
		{
			UButton* Button = Pair.Key.Get();
			if (!Button)
			{
				continue;
			}

			TSharedPtr<SWidget> ButtonSlateWidget = Button->GetCachedWidget();
			if (!ButtonSlateWidget.IsValid())
			{
				continue;
			}

			// Check if focused widget is this button or its child
			TSharedPtr<SWidget> CurrentWidget = FocusedWidget;
			while (CurrentWidget.IsValid())
			{
				if (CurrentWidget == ButtonSlateWidget)
				{
					UE_LOG(LogTemp, Log, TEXT("PanelSwitcher: Tab clicked - %s"), *Pair.Value.ToString());
					PublishPanelSelected(Pair.Value);
					return;
				}
				CurrentWidget = CurrentWidget->GetParentWidget();
			}
		}
	}

	// Fallback: Check which button has keyboard focus (UMG level)
	for (const auto& Pair : ButtonToTagMap)
	{
		UButton* Button = Pair.Key.Get();
		if (Button && Button->HasKeyboardFocus())
		{
			UE_LOG(LogTemp, Log, TEXT("PanelSwitcher: Tab clicked (fallback) - %s"), *Pair.Value.ToString());
			PublishPanelSelected(Pair.Value);
			return;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("PanelSwitcher: HandleTabButtonClicked - could not identify which button"));
}

//==================================================================
// EventBus
//==================================================================

void USuspenseCorePanelSwitcherWidget::SetupEventSubscriptions()
{
	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GetWorld());
	if (!Manager)
	{
		UE_LOG(LogTemp, Warning, TEXT("PanelSwitcher: EventManager not available"));
		return;
	}

	CachedEventBus = Manager->GetEventBus();
	if (!CachedEventBus.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("PanelSwitcher: EventBus not available"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("PanelSwitcher: EventBus subscriptions established"));
}

void USuspenseCorePanelSwitcherWidget::TeardownEventSubscriptions()
{
	CachedEventBus.Reset();
}

void USuspenseCorePanelSwitcherWidget::PublishPanelSelected(const FGameplayTag& PanelTag)
{
	if (!CachedEventBus.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("PanelSwitcher: Cannot publish - EventBus not available"));
		return;
	}

	// Create event data with panel tag
	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
	EventData.SetTag(FName("PanelTag"), PanelTag);

	// Publish event via EventBus (SuspenseCore.Event.UI.Panel.Selected)
	CachedEventBus->Publish(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.Panel.Selected")),
		EventData
	);

	UE_LOG(LogTemp, Log, TEXT("PanelSwitcher: Published panel selected event - %s"), *PanelTag.ToString());
}
