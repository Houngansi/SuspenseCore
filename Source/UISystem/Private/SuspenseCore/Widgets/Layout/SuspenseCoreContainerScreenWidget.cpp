// SuspenseCoreContainerScreenWidget.cpp
// SuspenseCore - Main Container Screen Widget Implementation
// AAA Pattern: Copied from legacy SuspenseCharacterScreen with EventBus
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/Layout/SuspenseCoreContainerScreenWidget.h"
#include "SuspenseCore/Widgets/Layout/SuspenseCorePanelSwitcherWidget.h"
#include "SuspenseCore/Widgets/Tooltip/SuspenseCoreTooltipWidget.h"
#include "SuspenseCore/Widgets/ContextMenu/SuspenseCoreContextMenuWidget.h"
#include "SuspenseCore/Subsystems/SuspenseCoreUIManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUIContainerTypes.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

//==================================================================
// Constructor
//==================================================================

USuspenseCoreContainerScreenWidget::USuspenseCoreContainerScreenWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CloseKey(EKeys::Escape)
	, bRememberLastPanel(true)
	, bIsActive(false)
	, DragGhostOffset(FVector2D::ZeroVector)
{
	// Set default tags
	ScreenTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.UI.Screen.Container"), false);
	DefaultPanelTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.UI.Panel.Inventory"), false);
}

//==================================================================
// UUserWidget Interface
//==================================================================

void USuspenseCoreContainerScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);

	// Cache UIManager reference
	CachedUIManager = USuspenseCoreUIManager::Get(this);

	// Bind to EventBus
	BindToUIManager();

	// Validate PanelSwitcher binding
	if (!PanelSwitcher)
	{
		UE_LOG(LogTemp, Error, TEXT("[ContainerScreen] PanelSwitcher is NOT bound! Bind it in Blueprint."));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[ContainerScreen] NativeConstruct completed. PanelSwitcher OK"));
	}
}

void USuspenseCoreContainerScreenWidget::NativeDestruct()
{
	// Clear refresh timer
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}

	// Unbind from EventBus
	UnbindFromUIManager();

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

	UE_LOG(LogTemp, Log, TEXT("[ContainerScreen] NativeDestruct completed"));

	Super::NativeDestruct();
}

FReply USuspenseCoreContainerScreenWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == CloseKey)
	{
		CloseScreen();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

//==================================================================
// Screen Activation (from legacy CharacterScreen)
//==================================================================

void USuspenseCoreContainerScreenWidget::ActivateScreen()
{
	if (bIsActive)
	{
		return;
	}

	bIsActive = true;

	// Determine which panel to open
	FGameplayTag PanelToOpen = DefaultPanelTag;
	if (bRememberLastPanel && LastOpenedPanel.IsValid())
	{
		PanelToOpen = LastOpenedPanel;
	}

	// Open the panel via PanelSwitcher
	if (PanelToOpen.IsValid())
	{
		OpenPanelByTag(PanelToOpen);
	}

	// Set input mode
	UpdateInputMode();

	// Call Blueprint event
	K2_OnContainerScreenOpened();

	// Publish screen opened event via EventBus
	PublishScreenEvent(FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.Screen.Opened")));

	// Force refresh after short delay (like legacy)
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(RefreshTimerHandle, [this]()
		{
			RefreshScreenContent();
		}, 0.2f, false);
	}

	UE_LOG(LogTemp, Log, TEXT("[ContainerScreen] Activated - Panel: %s"),
		PanelToOpen.IsValid() ? *PanelToOpen.ToString() : TEXT("None"));
}

void USuspenseCoreContainerScreenWidget::DeactivateScreen()
{
	if (!bIsActive)
	{
		return;
	}

	bIsActive = false;

	// Remember current panel
	if (bRememberLastPanel && ActivePanelTag.IsValid())
	{
		LastOpenedPanel = ActivePanelTag;
	}

	// Restore input mode
	UpdateInputMode();

	// Call Blueprint event
	K2_OnContainerScreenClosed();

	// Publish screen closed event via EventBus
	PublishScreenEvent(FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.Screen.Closed")));

	UE_LOG(LogTemp, Log, TEXT("[ContainerScreen] Deactivated - Last Panel: %s"),
		LastOpenedPanel.IsValid() ? *LastOpenedPanel.ToString() : TEXT("None"));
}

//==================================================================
// Panel Management (delegates to PanelSwitcher)
//==================================================================

void USuspenseCoreContainerScreenWidget::OpenPanelByTag(const FGameplayTag& PanelTag)
{
	if (!PanelSwitcher)
	{
		UE_LOG(LogTemp, Error, TEXT("[ContainerScreen] No PanelSwitcher - cannot open panel"));
		return;
	}

	bool bSuccess = PanelSwitcher->SelectTabByTag(PanelTag);

	if (bSuccess)
	{
		ActivePanelTag = PanelTag;
		HideTooltip();
		HideContextMenu();
		K2_OnPanelSwitched(PanelTag);
		UE_LOG(LogTemp, Log, TEXT("[ContainerScreen] Opened panel: %s"), *PanelTag.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ContainerScreen] Failed to open panel: %s"), *PanelTag.ToString());

		// Fallback: select first tab
		if (PanelSwitcher->GetTabCount() > 0)
		{
			PanelSwitcher->SelectTabByIndex(0);
			ActivePanelTag = PanelSwitcher->GetSelectedTabTag();
			UE_LOG(LogTemp, Log, TEXT("[ContainerScreen] Selected first panel as fallback"));
		}
	}
}

void USuspenseCoreContainerScreenWidget::OpenPanelByIndex(int32 PanelIndex)
{
	if (!PanelSwitcher)
	{
		return;
	}

	PanelSwitcher->SelectTabByIndex(PanelIndex);
	ActivePanelTag = PanelSwitcher->GetSelectedTabTag();
	HideTooltip();
	HideContextMenu();
	K2_OnPanelSwitched(ActivePanelTag);
}

UUserWidget* USuspenseCoreContainerScreenWidget::GetActivePanelContent() const
{
	if (!PanelSwitcher)
	{
		return nullptr;
	}

	int32 CurrentIndex = PanelSwitcher->GetSelectedTabIndex();
	return PanelSwitcher->GetTabContent(CurrentIndex);
}

void USuspenseCoreContainerScreenWidget::RefreshScreenContent()
{
	if (PanelSwitcher)
	{
		PanelSwitcher->RefreshActiveTabContent();
		UE_LOG(LogTemp, Log, TEXT("[ContainerScreen] Refreshed panel content"));
	}
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
	K2_OnScreenClosing();

	// Notify UIManager
	if (USuspenseCoreUIManager* UIManager = CachedUIManager.Get())
	{
		UIManager->CloseContainerScreen(GetOwningPlayer());
	}

	// Deactivate screen (restores input mode, publishes event)
	DeactivateScreen();

	// Remove from viewport
	RemoveFromParent();
}

//==================================================================
// EventBus Binding
//==================================================================

void USuspenseCoreContainerScreenWidget::BindToUIManager()
{
	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GetWorld());
	if (!Manager)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ContainerScreen] EventManager not available"));
		return;
	}

	CachedEventBus = Manager->GetEventBus();
	if (!CachedEventBus.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ContainerScreen] EventBus not available"));
		return;
	}

	// Subscribe to panel selected events
	PanelSelectedEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.Panel.Selected")),
		const_cast<USuspenseCoreContainerScreenWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreContainerScreenWidget::OnPanelSelectedEvent),
		ESuspenseCoreEventPriority::Normal
	);

	UE_LOG(LogTemp, Log, TEXT("[ContainerScreen] EventBus subscriptions established"));
}

void USuspenseCoreContainerScreenWidget::UnbindFromUIManager()
{
	if (CachedEventBus.IsValid() && PanelSelectedEventHandle.IsValid())
	{
		CachedEventBus->Unsubscribe(PanelSelectedEventHandle);
		PanelSelectedEventHandle = FSuspenseCoreSubscriptionHandle();
	}

	CachedEventBus.Reset();
}

void USuspenseCoreContainerScreenWidget::OnPanelSelectedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	FString PanelTagString = EventData.GetString(FName("PanelTag"));
	FGameplayTag PanelTag = FGameplayTag::RequestGameplayTag(FName(*PanelTagString), false);

	if (PanelTag.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("[ContainerScreen] Received panel selected event - %s"), *PanelTag.ToString());

		// Update active panel tag (PanelSwitcher already switched the content)
		ActivePanelTag = PanelTag;
		HideTooltip();
		HideContextMenu();
		K2_OnPanelSwitched(PanelTag);
	}
}

//==================================================================
// Helper Methods
//==================================================================

void USuspenseCoreContainerScreenWidget::UpdateInputMode()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (bIsActive)
		{
			FInputModeGameAndUI InputMode;
			InputMode.SetWidgetToFocus(TakeWidget());
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PC->SetInputMode(InputMode);
			PC->bShowMouseCursor = true;
		}
		else
		{
			FInputModeGameOnly InputMode;
			PC->SetInputMode(InputMode);
			PC->bShowMouseCursor = false;
		}
	}
}

void USuspenseCoreContainerScreenWidget::PublishScreenEvent(const FGameplayTag& EventTag)
{
	if (!CachedEventBus.IsValid())
	{
		return;
	}

	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
	EventData.SetString(FName("ScreenTag"), ScreenTag.ToString());
	EventData.SetString(FName("ActivePanelTag"), ActivePanelTag.ToString());

	CachedEventBus->Publish(EventTag, EventData);

	UE_LOG(LogTemp, Log, TEXT("[ContainerScreen] Published event: %s"), *EventTag.ToString());
}
