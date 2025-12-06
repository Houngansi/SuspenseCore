// SuspenseCorePanelSwitcherWidget.cpp
// SuspenseCore - Panel Switcher (Tab Bar) Widget Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/Layout/SuspenseCorePanelSwitcherWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

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
}

void USuspenseCorePanelSwitcherWidget::NativeDestruct()
{
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
		return;
	}

	// Check if tab already exists
	if (TabIndexByTag.Contains(PanelTag))
	{
		return;
	}

	// Create tab data
	FSuspenseCorePanelTab TabData(PanelTag, DisplayName);

	// Create tab button
	TabData.TabButton = CreateTabButton(TabData);
	if (!TabData.TabButton)
	{
		return;
	}

	// Add to container
	if (UHorizontalBoxSlot* TabSlot = TabContainer->AddChildToHorizontalBox(TabData.TabButton))
	{
		TabSlot->SetHorizontalAlignment(HAlign_Fill);
		TabSlot->SetVerticalAlignment(VAlign_Fill);
		TabSlot->SetPadding(FMargin(2.0f, 0.0f));
	}

	// Track tab
	int32 TabIndex = Tabs.Num();
	Tabs.Add(TabData);
	TabIndexByTag.Add(PanelTag, TabIndex);

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

		// Remove button from container
		if (Tab.TabButton)
		{
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
			Tab.TabButton->RemoveFromParent();
		}
	}

	Tabs.Empty();
	TabIndexByTag.Empty();
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

	// Broadcast selection
	PanelSelectedDelegate.Broadcast(NextTag);
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

	// Broadcast selection
	PanelSelectedDelegate.Broadcast(PrevTag);
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

	// Store text reference in tab data (we'll update it in the array after adding)
	// This is a bit awkward due to const, but we handle it

	// Bind click event - using a simple click handler that finds the button
	Button->OnClicked.AddDynamic(this, &USuspenseCorePanelSwitcherWidget::HandleTabButtonClicked);

	// We need a way to pass the tag to the click handler
	// Since we can't easily pass parameters with dynamic delegates,
	// we'll use a lambda with the widget switcher approach
	// For now, we'll use a workaround with the button's user data

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
	// Find which button was clicked by checking pressed/focused state
	for (const FSuspenseCorePanelTab& Tab : Tabs)
	{
		if (Tab.TabButton && (Tab.TabButton->HasKeyboardFocus() || Tab.TabButton->IsPressed()))
		{
			PanelSelectedDelegate.Broadcast(Tab.PanelTag);
			return;
		}
	}
}

void USuspenseCorePanelSwitcherWidget::OnTabButtonClicked(FGameplayTag PanelTag)
{
	PanelSelectedDelegate.Broadcast(PanelTag);
}
