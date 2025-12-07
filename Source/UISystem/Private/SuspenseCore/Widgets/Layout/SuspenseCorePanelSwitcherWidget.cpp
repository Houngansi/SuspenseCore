// SuspenseCorePanelSwitcherWidget.cpp
// SuspenseCore - Panel Switcher (Tab Bar) Widget Implementation
// AAA Pattern: EventBus communication, SuspenseCoreButtonWidget for tabs
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/Layout/SuspenseCorePanelSwitcherWidget.h"
#include "SuspenseCore/Widgets/Layout/SuspenseCorePanelWidget.h"
#include "SuspenseCore/Widgets/Common/SuspenseCoreButtonWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"

//==================================================================
// Constructor
//==================================================================

USuspenseCorePanelSwitcherWidget::USuspenseCorePanelSwitcherWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, DefaultTabIndex(0)
	, NextTabKey(EKeys::Tab)
	, CurrentTabIndex(-1)
{
}

//==================================================================
// UUserWidget Interface
//==================================================================

void USuspenseCorePanelSwitcherWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Preview tabs in designer (like legacy SuspenseUpperTabBar)
	if (IsDesignTime() && TabContainer && WidgetTree)
	{
		TabContainer->ClearChildren();

		for (int32 i = 0; i < TabConfigs.Num(); i++)
		{
			// Create preview text for design time
			UTextBlock* PreviewText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			if (PreviewText)
			{
				PreviewText->SetText(TabConfigs[i].DisplayName);

				// Create preview button
				UButton* PreviewButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
				if (PreviewButton)
				{
					PreviewButton->AddChild(PreviewText);

					UHorizontalBoxSlot* TabSlot = TabContainer->AddChildToHorizontalBox(PreviewButton);
					if (TabSlot)
					{
						TabSlot->SetPadding(FMargin(4.0f, 0.0f));
						TabSlot->SetHorizontalAlignment(HAlign_Left);
					}
				}
			}
		}
	}
}

void USuspenseCorePanelSwitcherWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Skip runtime initialization in design mode
	if (IsDesignTime())
	{
		return;
	}

	// Setup EventBus first
	SetupEventSubscriptions();

	// Validate required bindings
	if (!TabContainer)
	{
		UE_LOG(LogTemp, Error, TEXT("PanelSwitcher: TabContainer not bound! Bind it in Blueprint."));
		return;
	}

	// Clear design-time content
	TabContainer->ClearChildren();

	if (PanelContainer)
	{
		PanelContainer->ClearChildren();
	}

	// Initialize tabs from config
	InitializeTabs();

	UE_LOG(LogTemp, Log, TEXT("PanelSwitcher: Initialized with %d tabs"), RuntimeTabs.Num());
}

void USuspenseCorePanelSwitcherWidget::NativeDestruct()
{
	// Teardown EventBus subscriptions
	TeardownEventSubscriptions();

	// Clear all tabs (unbinds delegates)
	ClearTabs();

	Super::NativeDestruct();
}

FReply USuspenseCorePanelSwitcherWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Handle tab navigation
	if (InKeyEvent.GetKey() == NextTabKey && !InKeyEvent.IsShiftDown())
	{
		SelectNextTab();
		return FReply::Handled();
	}
	else if (InKeyEvent.GetKey() == NextTabKey && InKeyEvent.IsShiftDown())
	{
		// Shift+Tab = previous tab
		SelectPreviousTab();
		return FReply::Handled();
	}
	else if (PreviousTabKey.IsValid() && InKeyEvent.GetKey() == PreviousTabKey)
	{
		SelectPreviousTab();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

//==================================================================
// Tab Management API
//==================================================================

void USuspenseCorePanelSwitcherWidget::InitializeTabs()
{
	// Clear any existing runtime tabs
	ClearTabs();

	if (TabConfigs.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("PanelSwitcher: No TabConfigs defined!"));
		return;
	}

	// Create tabs from configs
	for (int32 i = 0; i < TabConfigs.Num(); i++)
	{
		const FSuspenseCorePanelTabConfig& Config = TabConfigs[i];

		if (!Config.PanelTag.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("PanelSwitcher: TabConfig[%d] has invalid PanelTag, skipping"), i);
			continue;
		}

		FSuspenseCorePanelTabRuntime RuntimeTab;
		RuntimeTab.PanelTag = Config.PanelTag;
		RuntimeTab.bEnabled = Config.bEnabled;

		// Create tab button
		RuntimeTab.TabButton = CreateTabButton(Config, i);
		if (!RuntimeTab.TabButton)
		{
			UE_LOG(LogTemp, Warning, TEXT("PanelSwitcher: Failed to create button for tab %d"), i);
			continue;
		}

		// Add button to container
		if (UHorizontalBoxSlot* TabSlot = TabContainer->AddChildToHorizontalBox(RuntimeTab.TabButton))
		{
			TabSlot->SetPadding(FMargin(4.0f, 0.0f));
			TabSlot->SetHorizontalAlignment(HAlign_Fill);
			TabSlot->SetVerticalAlignment(VAlign_Fill);
		}

		// Create panel widget if PanelContainer is bound
		if (PanelContainer && Config.PanelWidgetClass)
		{
			RuntimeTab.PanelWidget = CreatePanelWidget(Config, i);
			if (RuntimeTab.PanelWidget)
			{
				PanelContainer->AddChild(RuntimeTab.PanelWidget);
			}
		}

		// Track button->index mapping for click handler
		ButtonToIndexMap.Add(RuntimeTab.TabButton, RuntimeTabs.Num());

		// Store runtime tab
		RuntimeTabs.Add(RuntimeTab);

		// Notify Blueprint
		K2_OnTabCreated(i, RuntimeTab.TabButton);

		UE_LOG(LogTemp, Log, TEXT("PanelSwitcher: Created tab %d - %s"), i, *Config.PanelTag.ToString());
	}

	// Select default tab
	if (RuntimeTabs.Num() > 0)
	{
		int32 IndexToSelect = FMath::Clamp(DefaultTabIndex, 0, RuntimeTabs.Num() - 1);
		SelectTabByIndex(IndexToSelect);
	}
}

void USuspenseCorePanelSwitcherWidget::ClearTabs()
{
	// Unbind button delegates and remove from parent
	for (FSuspenseCorePanelTabRuntime& Tab : RuntimeTabs)
	{
		if (Tab.TabButton)
		{
			// Unbind from SuspenseCoreButtonWidget delegate
			Tab.TabButton->OnButtonClicked.RemoveDynamic(this, &USuspenseCorePanelSwitcherWidget::OnTabButtonClicked);
			Tab.TabButton->RemoveFromParent();
		}

		if (Tab.PanelWidget)
		{
			Tab.PanelWidget->RemoveFromParent();
		}
	}

	RuntimeTabs.Empty();
	ButtonToIndexMap.Empty();
	CurrentTabIndex = -1;

	if (TabContainer)
	{
		TabContainer->ClearChildren();
	}

	if (PanelContainer)
	{
		PanelContainer->ClearChildren();
	}
}

bool USuspenseCorePanelSwitcherWidget::SelectTabByIndex(int32 TabIndex)
{
	if (TabIndex < 0 || TabIndex >= RuntimeTabs.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("PanelSwitcher: Invalid tab index %d"), TabIndex);
		return false;
	}

	FSuspenseCorePanelTabRuntime& Tab = RuntimeTabs[TabIndex];

	// Check if tab is enabled
	if (!Tab.bEnabled)
	{
		UE_LOG(LogTemp, Warning, TEXT("PanelSwitcher: Tab %d is disabled"), TabIndex);
		return false;
	}

	int32 OldIndex = CurrentTabIndex;
	CurrentTabIndex = TabIndex;

	// Update all tab visuals
	UpdateAllTabVisuals();

	// Switch panel container if available
	if (PanelContainer && Tab.PanelWidget)
	{
		PanelContainer->SetActiveWidget(Tab.PanelWidget);
	}

	// Notify Blueprint
	if (OldIndex != CurrentTabIndex)
	{
		K2_OnTabSelected(OldIndex, CurrentTabIndex);
	}

	// Publish via EventBus
	PublishPanelSelected(Tab.PanelTag);

	UE_LOG(LogTemp, Log, TEXT("PanelSwitcher: Selected tab %d (%s)"), TabIndex, *Tab.PanelTag.ToString());
	return true;
}

bool USuspenseCorePanelSwitcherWidget::SelectTabByTag(const FGameplayTag& PanelTag)
{
	for (int32 i = 0; i < RuntimeTabs.Num(); i++)
	{
		if (RuntimeTabs[i].PanelTag.MatchesTagExact(PanelTag))
		{
			return SelectTabByIndex(i);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("PanelSwitcher: Tab with tag %s not found"), *PanelTag.ToString());
	return false;
}

FGameplayTag USuspenseCorePanelSwitcherWidget::GetActivePanel() const
{
	if (CurrentTabIndex >= 0 && CurrentTabIndex < RuntimeTabs.Num())
	{
		return RuntimeTabs[CurrentTabIndex].PanelTag;
	}
	return FGameplayTag();
}

void USuspenseCorePanelSwitcherWidget::SelectNextTab()
{
	if (RuntimeTabs.Num() == 0)
	{
		return;
	}

	// Find next enabled tab
	int32 StartIndex = FMath::Max(0, CurrentTabIndex);
	for (int32 i = 1; i <= RuntimeTabs.Num(); i++)
	{
		int32 NextIndex = (StartIndex + i) % RuntimeTabs.Num();
		if (RuntimeTabs[NextIndex].bEnabled)
		{
			SelectTabByIndex(NextIndex);
			return;
		}
	}
}

void USuspenseCorePanelSwitcherWidget::SelectPreviousTab()
{
	if (RuntimeTabs.Num() == 0)
	{
		return;
	}

	// Find previous enabled tab
	int32 StartIndex = FMath::Max(0, CurrentTabIndex);
	for (int32 i = 1; i <= RuntimeTabs.Num(); i++)
	{
		int32 PrevIndex = (StartIndex - i + RuntimeTabs.Num()) % RuntimeTabs.Num();
		if (RuntimeTabs[PrevIndex].bEnabled)
		{
			SelectTabByIndex(PrevIndex);
			return;
		}
	}
}

void USuspenseCorePanelSwitcherWidget::SetTabEnabled(int32 TabIndex, bool bEnabled)
{
	if (TabIndex < 0 || TabIndex >= RuntimeTabs.Num())
	{
		return;
	}

	FSuspenseCorePanelTabRuntime& Tab = RuntimeTabs[TabIndex];
	Tab.bEnabled = bEnabled;

	// Update button enabled state
	if (Tab.TabButton)
	{
		Tab.TabButton->SetButtonEnabled(bEnabled);
	}

	// If disabling current tab, select another
	if (!bEnabled && CurrentTabIndex == TabIndex)
	{
		SelectNextTab();
	}
}

void USuspenseCorePanelSwitcherWidget::RefreshActiveTabContent()
{
	if (CurrentTabIndex >= 0 && CurrentTabIndex < RuntimeTabs.Num())
	{
		USuspenseCorePanelWidget* Panel = RuntimeTabs[CurrentTabIndex].PanelWidget;
		if (Panel)
		{
			// TODO: Call panel refresh method if available
			UE_LOG(LogTemp, Log, TEXT("PanelSwitcher: Refreshing active tab content"));
		}
	}
}

//==================================================================
// Backward Compatibility API
//==================================================================

void USuspenseCorePanelSwitcherWidget::SetActivePanel(const FGameplayTag& PanelTag)
{
	// Wrapper for SelectTabByTag (backward compatibility with ContainerScreen)
	SelectTabByTag(PanelTag);
}

void USuspenseCorePanelSwitcherWidget::AddTab(const FGameplayTag& PanelTag, const FText& DisplayName)
{
	if (!PanelTag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("PanelSwitcher::AddTab - Invalid PanelTag"));
		return;
	}

	if (!TabContainer)
	{
		UE_LOG(LogTemp, Warning, TEXT("PanelSwitcher::AddTab - TabContainer not bound"));
		return;
	}

	// Check if tab already exists
	for (const FSuspenseCorePanelTabRuntime& Existing : RuntimeTabs)
	{
		if (Existing.PanelTag.MatchesTagExact(PanelTag))
		{
			UE_LOG(LogTemp, Warning, TEXT("PanelSwitcher::AddTab - Tab '%s' already exists"), *PanelTag.ToString());
			return;
		}
	}

	// Create config for this tab
	FSuspenseCorePanelTabConfig Config;
	Config.PanelTag = PanelTag;
	Config.DisplayName = DisplayName;
	Config.bEnabled = true;

	// Create runtime tab
	FSuspenseCorePanelTabRuntime RuntimeTab;
	RuntimeTab.PanelTag = PanelTag;
	RuntimeTab.bEnabled = true;

	// Create tab button
	int32 TabIndex = RuntimeTabs.Num();
	RuntimeTab.TabButton = CreateTabButton(Config, TabIndex);
	if (!RuntimeTab.TabButton)
	{
		UE_LOG(LogTemp, Warning, TEXT("PanelSwitcher::AddTab - Failed to create button for '%s'"), *PanelTag.ToString());
		return;
	}

	// Add button to container
	if (UHorizontalBoxSlot* TabSlot = TabContainer->AddChildToHorizontalBox(RuntimeTab.TabButton))
	{
		TabSlot->SetPadding(FMargin(4.0f, 0.0f));
		TabSlot->SetHorizontalAlignment(HAlign_Fill);
		TabSlot->SetVerticalAlignment(VAlign_Fill);
	}

	// Track button->index mapping
	ButtonToIndexMap.Add(RuntimeTab.TabButton, TabIndex);

	// Store runtime tab
	RuntimeTabs.Add(RuntimeTab);

	// Notify Blueprint
	K2_OnTabCreated(TabIndex, RuntimeTab.TabButton);

	// Update visuals (new tab is not selected)
	UpdateTabVisual(TabIndex, false);

	UE_LOG(LogTemp, Log, TEXT("PanelSwitcher::AddTab - Created tab '%s' at index %d"), *PanelTag.ToString(), TabIndex);
}

//==================================================================
// Tab Creation
//==================================================================

USuspenseCoreButtonWidget* USuspenseCorePanelSwitcherWidget::CreateTabButton_Implementation(
	const FSuspenseCorePanelTabConfig& Config, int32 TabIndex)
{
	// Determine button class to use
	TSubclassOf<USuspenseCoreButtonWidget> ButtonClass = TabButtonClass;
	if (!ButtonClass)
	{
		ButtonClass = USuspenseCoreButtonWidget::StaticClass();
	}

	// Create button widget
	USuspenseCoreButtonWidget* Button = CreateWidget<USuspenseCoreButtonWidget>(this, ButtonClass);
	if (!Button)
	{
		UE_LOG(LogTemp, Error, TEXT("PanelSwitcher: Failed to create SuspenseCoreButtonWidget"));
		return nullptr;
	}

	// Configure button
	Button->SetButtonText(Config.DisplayName);
	Button->SetButtonEnabled(Config.bEnabled);

	// Set ActionTag for identification (per BestPractices.md)
	// Format: SuspenseCore.UI.Tab.<PanelTag leaf>
	Button->SetActionTag(Config.PanelTag);

	// Set icon if provided
	if (Config.TabIcon)
	{
		Button->SetButtonIcon(Config.TabIcon);
	}

	// CRITICAL: Bind to button's OnButtonClicked delegate
	// SuspenseCoreButtonWidget broadcasts this when clicked
	Button->OnButtonClicked.AddDynamic(this, &USuspenseCorePanelSwitcherWidget::OnTabButtonClicked);

	return Button;
}

USuspenseCorePanelWidget* USuspenseCorePanelSwitcherWidget::CreatePanelWidget_Implementation(
	const FSuspenseCorePanelTabConfig& Config, int32 TabIndex)
{
	if (!Config.PanelWidgetClass)
	{
		return nullptr;
	}

	// Create panel widget
	USuspenseCorePanelWidget* Panel = CreateWidget<USuspenseCorePanelWidget>(this, Config.PanelWidgetClass);
	if (Panel)
	{
		UE_LOG(LogTemp, Log, TEXT("PanelSwitcher: Created panel widget for tab %d"), TabIndex);
	}

	return Panel;
}

void USuspenseCorePanelSwitcherWidget::UpdateTabVisual_Implementation(int32 TabIndex, bool bIsSelected)
{
	if (TabIndex < 0 || TabIndex >= RuntimeTabs.Num())
	{
		return;
	}

	USuspenseCoreButtonWidget* Button = RuntimeTabs[TabIndex].TabButton;
	if (!Button)
	{
		return;
	}

	// Update button style based on selection
	// SuspenseCoreButtonWidget handles its own styling via SetButtonStyle
	if (bIsSelected)
	{
		Button->SetButtonStyle(ESuspenseCoreButtonStyle::Primary);
	}
	else
	{
		Button->SetButtonStyle(ESuspenseCoreButtonStyle::Secondary);
	}
}

void USuspenseCorePanelSwitcherWidget::UpdateAllTabVisuals()
{
	for (int32 i = 0; i < RuntimeTabs.Num(); i++)
	{
		UpdateTabVisual(i, i == CurrentTabIndex);
	}
}

//==================================================================
// Internal Handlers
//==================================================================

void USuspenseCorePanelSwitcherWidget::OnTabButtonClicked(USuspenseCoreButtonWidget* Button)
{
	if (!Button)
	{
		return;
	}

	// Find tab index from ButtonToIndexMap
	int32* FoundIndex = ButtonToIndexMap.Find(Button);
	if (FoundIndex)
	{
		UE_LOG(LogTemp, Log, TEXT("PanelSwitcher: Tab button clicked - index %d"), *FoundIndex);
		SelectTabByIndex(*FoundIndex);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PanelSwitcher: Unknown button clicked"));
	}
}

void USuspenseCorePanelSwitcherWidget::PublishPanelSelected(const FGameplayTag& PanelTag)
{
	if (!CachedEventBus.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("PanelSwitcher: Cannot publish - EventBus not available"));
		return;
	}

	// Create event data (per BestPractices.md - use EventBus, not direct delegates)
	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
	EventData.SetString(FName("PanelTag"), PanelTag.ToString());
	EventData.SetInt(FName("TabIndex"), CurrentTabIndex);

	// Publish event: SuspenseCore.Event.UI.Panel.Selected
	CachedEventBus->Publish(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.Panel.Selected")),
		EventData
	);

	UE_LOG(LogTemp, Log, TEXT("PanelSwitcher: Published panel selected - %s"), *PanelTag.ToString());
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

	UE_LOG(LogTemp, Log, TEXT("PanelSwitcher: EventBus ready"));
}

void USuspenseCorePanelSwitcherWidget::TeardownEventSubscriptions()
{
	CachedEventBus.Reset();
}
