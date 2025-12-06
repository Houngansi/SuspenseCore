// SuspenseCoreContainerScreenWidget.cpp
// SuspenseCore - Main Container Screen Widget Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/Layout/SuspenseCoreContainerScreenWidget.h"
#include "SuspenseCore/Widgets/Layout/SuspenseCorePanelWidget.h"
#include "SuspenseCore/Widgets/Layout/SuspenseCorePanelSwitcherWidget.h"
#include "SuspenseCore/Widgets/Tooltip/SuspenseCoreTooltipWidget.h"
#include "SuspenseCore/Widgets/ContextMenu/SuspenseCoreContextMenuWidget.h"
#include "SuspenseCore/Subsystems/SuspenseCoreUIManager.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"

//==================================================================
// Constructor
//==================================================================

USuspenseCoreContainerScreenWidget::USuspenseCoreContainerScreenWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CloseKey(EKeys::Escape)
	, DragGhostOffset(FVector2D::ZeroVector)
{
}

//==================================================================
// UUserWidget Interface
//==================================================================

void USuspenseCoreContainerScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Cache UIManager reference
	CachedUIManager = USuspenseCoreUIManager::Get(this);

	// Bind to UIManager events
	BindToUIManager();

	// Set up input mode for UI
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
	}
}

void USuspenseCoreContainerScreenWidget::NativeDestruct()
{
	// Unbind from UIManager events
	UnbindFromUIManager();

	// Clean up panels
	for (USuspenseCorePanelWidget* Panel : Panels)
	{
		if (Panel)
		{
			Panel->RemoveFromParent();
		}
	}
	Panels.Empty();
	PanelsByTag.Empty();

	// Clean up overlay widgets
	if (ItemTooltipWidget)
	{
		ItemTooltipWidget->RemoveFromParent();
		ItemTooltipWidget = nullptr;
	}
	if (ContextMenuWidget)
	{
		ContextMenuWidget->RemoveFromParent();
		ContextMenuWidget = nullptr;
	}
	if (DragGhostWidget)
	{
		DragGhostWidget->RemoveFromParent();
		DragGhostWidget = nullptr;
	}

	Super::NativeDestruct();
}

void USuspenseCoreContainerScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
}

FReply USuspenseCoreContainerScreenWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Handle close key
	if (InKeyEvent.GetKey() == CloseKey)
	{
		CloseScreen();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

//==================================================================
// Screen Configuration
//==================================================================

void USuspenseCoreContainerScreenWidget::InitializeScreen(const FSuspenseCoreScreenConfig& InScreenConfig)
{
	ScreenConfig = InScreenConfig;

	// Create panels from config
	CreatePanels();

	// Setup panel switcher
	SetupPanelSwitcher();

	// Switch to first panel if available
	if (ScreenConfig.Panels.Num() > 0)
	{
		SwitchToPanel(ScreenConfig.Panels[0].PanelTag);
	}

	// Notify Blueprint
	K2_OnScreenInitialized();
}

//==================================================================
// Panel Management
//==================================================================

bool USuspenseCoreContainerScreenWidget::SwitchToPanel(const FGameplayTag& PanelTag)
{
	if (!PanelTag.IsValid())
	{
		return false;
	}

	TObjectPtr<USuspenseCorePanelWidget>* FoundPanel = PanelsByTag.Find(PanelTag);
	if (!FoundPanel || !*FoundPanel)
	{
		return false;
	}

	// Find panel index in widget switcher
	int32 PanelIndex = Panels.Find(*FoundPanel);
	if (PanelIndex == INDEX_NONE)
	{
		return false;
	}

	// Switch in widget switcher
	if (PanelContainer)
	{
		PanelContainer->SetActiveWidgetIndex(PanelIndex);
	}

	// Update panel switcher selection
	if (PanelSwitcher)
	{
		PanelSwitcher->SetActivePanel(PanelTag);
	}

	ActivePanelTag = PanelTag;

	// Hide tooltip and context menu when switching panels
	HideTooltip();
	HideContextMenu();

	// Notify Blueprint
	K2_OnPanelSwitched(PanelTag);

	return true;
}

bool USuspenseCoreContainerScreenWidget::SwitchToPanelByIndex(int32 PanelIndex)
{
	if (PanelIndex < 0 || PanelIndex >= Panels.Num())
	{
		return false;
	}

	USuspenseCorePanelWidget* Panel = Panels[PanelIndex];
	if (!Panel)
	{
		return false;
	}

	// Find the panel tag
	for (const auto& Pair : PanelsByTag)
	{
		if (Pair.Value == Panel)
		{
			return SwitchToPanel(Pair.Key);
		}
	}

	return false;
}

USuspenseCorePanelWidget* USuspenseCoreContainerScreenWidget::GetActivePanel() const
{
	TObjectPtr<USuspenseCorePanelWidget> const* FoundPanel = PanelsByTag.Find(ActivePanelTag);
	return FoundPanel ? *FoundPanel : nullptr;
}

USuspenseCorePanelWidget* USuspenseCoreContainerScreenWidget::GetPanelByTag(const FGameplayTag& PanelTag) const
{
	TObjectPtr<USuspenseCorePanelWidget> const* FoundPanel = PanelsByTag.Find(PanelTag);
	return FoundPanel ? *FoundPanel : nullptr;
}

//==================================================================
// Tooltip Management
//==================================================================

void USuspenseCoreContainerScreenWidget::ShowTooltip(const FSuspenseCoreItemUIData& ItemData, const FVector2D& ScreenPosition)
{
	// Create tooltip widget if needed
	if (!ItemTooltipWidget && ItemTooltipWidgetClass)
	{
		ItemTooltipWidget = CreateWidget<USuspenseCoreTooltipWidget>(GetOwningPlayer(), ItemTooltipWidgetClass);
		if (ItemTooltipWidget && OverlayLayer)
		{
			UOverlaySlot* OverlaySlotRef = OverlayLayer->AddChildToOverlay(ItemTooltipWidget);
			if (OverlaySlotRef)
			{
				OverlaySlotRef->SetHorizontalAlignment(HAlign_Left);
				OverlaySlotRef->SetVerticalAlignment(VAlign_Top);
			}
		}
	}

	if (ItemTooltipWidget)
	{
		ItemTooltipWidget->ShowForItem(ItemData, ScreenPosition);
	}
}

void USuspenseCoreContainerScreenWidget::HideTooltip()
{
	if (ItemTooltipWidget)
	{
		ItemTooltipWidget->Hide();
	}
}

bool USuspenseCoreContainerScreenWidget::IsTooltipVisible() const
{
	return ItemTooltipWidget && ItemTooltipWidget->IsVisible();
}

//==================================================================
// Context Menu Management
//==================================================================

void USuspenseCoreContainerScreenWidget::ShowContextMenu(
	const FSuspenseCoreItemUIData& ItemData,
	const FGuid& ContainerID,
	int32 SlotIndex,
	const FVector2D& ScreenPosition,
	const TArray<FGameplayTag>& AvailableActions)
{
	// Hide tooltip when showing context menu
	HideTooltip();

	// Create context menu widget if needed
	if (!ContextMenuWidget && ContextMenuWidgetClass)
	{
		ContextMenuWidget = CreateWidget<USuspenseCoreContextMenuWidget>(GetOwningPlayer(), ContextMenuWidgetClass);
		if (ContextMenuWidget && OverlayLayer)
		{
			UOverlaySlot* OverlaySlotRef = OverlayLayer->AddChildToOverlay(ContextMenuWidget);
			if (OverlaySlotRef)
			{
				OverlaySlotRef->SetHorizontalAlignment(HAlign_Left);
				OverlaySlotRef->SetVerticalAlignment(VAlign_Top);
			}
		}
	}

	if (ContextMenuWidget)
	{
		ContextMenuWidget->ShowForItem(ItemData, ContainerID, SlotIndex, ScreenPosition, AvailableActions);
	}
}

void USuspenseCoreContainerScreenWidget::HideContextMenu()
{
	if (ContextMenuWidget)
	{
		ContextMenuWidget->Hide();
	}
}

bool USuspenseCoreContainerScreenWidget::IsContextMenuVisible() const
{
	return ContextMenuWidget && ContextMenuWidget->IsVisible();
}

//==================================================================
// Drag-Drop Visual
//==================================================================

void USuspenseCoreContainerScreenWidget::ShowDragGhost(const FSuspenseCoreItemUIData& ItemData, const FVector2D& DragOffset)
{
	DragGhostOffset = DragOffset;

	// Create drag ghost widget if needed
	if (!DragGhostWidget && DragGhostWidgetClass)
	{
		DragGhostWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), DragGhostWidgetClass);
		if (DragGhostWidget && OverlayLayer)
		{
			UOverlaySlot* OverlaySlotRef = OverlayLayer->AddChildToOverlay(DragGhostWidget);
			if (OverlaySlotRef)
			{
				OverlaySlotRef->SetHorizontalAlignment(HAlign_Left);
				OverlaySlotRef->SetVerticalAlignment(VAlign_Top);
			}
		}
	}

	if (DragGhostWidget)
	{
		// Set item icon on ghost widget
		// This would be implemented based on your drag ghost widget structure
		DragGhostWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void USuspenseCoreContainerScreenWidget::UpdateDragGhostPosition(const FVector2D& ScreenPosition)
{
	if (DragGhostWidget)
	{
		FVector2D Position = ScreenPosition + DragGhostOffset;
		DragGhostWidget->SetRenderTranslation(Position);
	}
}

void USuspenseCoreContainerScreenWidget::HideDragGhost()
{
	if (DragGhostWidget)
	{
		DragGhostWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

//==================================================================
// Screen Actions
//==================================================================

void USuspenseCoreContainerScreenWidget::CloseScreen()
{
	// Notify Blueprint
	K2_OnScreenClosing();

	// Notify UIManager
	if (USuspenseCoreUIManager* UIManager = CachedUIManager.Get())
	{
		UIManager->CloseContainerScreen(GetOwningPlayer());
	}

	// Restore input mode
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = false;
	}

	// Remove from viewport
	RemoveFromParent();
}

void USuspenseCoreContainerScreenWidget::OnCloseButtonClicked()
{
	CloseScreen();
}

//==================================================================
// Setup Methods
//==================================================================

void USuspenseCoreContainerScreenWidget::CreatePanels_Implementation()
{
	if (!PanelWidgetClass || !PanelContainer)
	{
		return;
	}

	// Clear existing panels
	PanelContainer->ClearChildren();
	Panels.Empty();
	PanelsByTag.Empty();

	// Create panel for each ENABLED config
	for (const FSuspenseCorePanelConfig& PanelConfig : ScreenConfig.Panels)
	{
		// Skip disabled panels
		if (!PanelConfig.bIsEnabled)
		{
			continue;
		}

		USuspenseCorePanelWidget* Panel = CreateWidget<USuspenseCorePanelWidget>(GetOwningPlayer(), PanelWidgetClass);
		if (!Panel)
		{
			continue;
		}

		// Initialize panel with config
		Panel->InitializePanel(PanelConfig);

		// Add to widget switcher
		PanelContainer->AddChild(Panel);

		// Track panel
		Panels.Add(Panel);
		PanelsByTag.Add(PanelConfig.PanelTag, Panel);
	}
}

void USuspenseCoreContainerScreenWidget::SetupPanelSwitcher_Implementation()
{
	if (!PanelSwitcher)
	{
		return;
	}

	// Clear existing tabs
	PanelSwitcher->ClearTabs();

	// Add tab for each ENABLED panel
	for (const FSuspenseCorePanelConfig& PanelConfig : ScreenConfig.Panels)
	{
		if (!PanelConfig.bIsEnabled)
		{
			continue;
		}
		PanelSwitcher->AddTab(PanelConfig.PanelTag, PanelConfig.DisplayName);
	}

	// Bind panel switcher selection event
	PanelSwitcher->OnPanelSelected().AddWeakLambda(this, [this](const FGameplayTag& PanelTag)
	{
		SwitchToPanel(PanelTag);
	});
}

void USuspenseCoreContainerScreenWidget::BindToUIManager()
{
	// UIManager binding would be done here if needed
}

void USuspenseCoreContainerScreenWidget::UnbindFromUIManager()
{
	// UIManager unbinding would be done here if needed
}
