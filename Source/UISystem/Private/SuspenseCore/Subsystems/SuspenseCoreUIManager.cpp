// SuspenseCoreUIManager.cpp
// SuspenseCore - UI Manager Subsystem Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Subsystems/SuspenseCoreUIManager.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreUIDataProvider.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/UI/SuspenseCoreUIEvents.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "Components/ActorComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Blueprint/UserWidget.h"

//==================================================================
// Static Access
//==================================================================

USuspenseCoreUIManager* USuspenseCoreUIManager::Get(const UObject* WorldContext)
{
	if (!WorldContext)
	{
		return nullptr;
	}

	const UWorld* World = WorldContext->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		return nullptr;
	}

	return GameInstance->GetSubsystem<USuspenseCoreUIManager>();
}

//==================================================================
// Lifecycle
//==================================================================

void USuspenseCoreUIManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	bIsContainerScreenVisible = false;

	// Setup default screen config
	SetupDefaultScreenConfig();

	// Subscribe to EventBus
	SubscribeToEvents();

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreUIManager initialized"));
}

void USuspenseCoreUIManager::Deinitialize()
{
	UnsubscribeFromEvents();

	// Clean up widgets
	if (ContainerScreen)
	{
		ContainerScreen->RemoveFromParent();
		ContainerScreen = nullptr;
	}

	if (TooltipWidget)
	{
		TooltipWidget->RemoveFromParent();
		TooltipWidget = nullptr;
	}

	RegisteredProviders.Empty();

	Super::Deinitialize();
}

bool USuspenseCoreUIManager::ShouldCreateSubsystem(UObject* Outer) const
{
	// Create for all game instances
	return true;
}

void USuspenseCoreUIManager::SetupDefaultScreenConfig()
{
	// Inventory panel
	FSuspenseCorePanelConfig InventoryPanel;
	InventoryPanel.PanelTag = TAG_SuspenseCore_Event_UIPanel_Inventory;
	InventoryPanel.DisplayName = NSLOCTEXT("SuspenseCore", "Panel_Inventory", "INVENTORY");
	InventoryPanel.ContainerTypes.Add(ESuspenseCoreContainerType::Inventory);
	InventoryPanel.ContainerTypes.Add(ESuspenseCoreContainerType::Equipment);
	InventoryPanel.SortOrder = 0;
	InventoryPanel.bIsEnabled = true;
	ScreenConfig.Panels.Add(InventoryPanel);

	// Stash panel
	FSuspenseCorePanelConfig StashPanel;
	StashPanel.PanelTag = TAG_SuspenseCore_Event_UIPanel_Stash;
	StashPanel.DisplayName = NSLOCTEXT("SuspenseCore", "Panel_Stash", "STASH");
	StashPanel.ContainerTypes.Add(ESuspenseCoreContainerType::Inventory);
	StashPanel.ContainerTypes.Add(ESuspenseCoreContainerType::Stash);
	StashPanel.SortOrder = 1;
	StashPanel.bIsEnabled = true;
	ScreenConfig.Panels.Add(StashPanel);

	// Trader panel
	FSuspenseCorePanelConfig TraderPanel;
	TraderPanel.PanelTag = TAG_SuspenseCore_Event_UIPanel_Trader;
	TraderPanel.DisplayName = NSLOCTEXT("SuspenseCore", "Panel_Trader", "TRADER");
	TraderPanel.ContainerTypes.Add(ESuspenseCoreContainerType::Inventory);
	TraderPanel.ContainerTypes.Add(ESuspenseCoreContainerType::Trader);
	TraderPanel.SortOrder = 2;
	TraderPanel.bIsEnabled = true;
	ScreenConfig.Panels.Add(TraderPanel);

	ScreenConfig.DefaultPanelTag = TAG_SuspenseCore_Event_UIPanel_Inventory;
	ScreenConfig.bAllowCrossPanelDrag = true;
	ScreenConfig.bShowWeight = true;
	ScreenConfig.bShowCurrency = true;
}

//==================================================================
// Container Screen Management
//==================================================================

bool USuspenseCoreUIManager::ShowContainerScreen(APlayerController* PC, const FGameplayTag& PanelTag)
{
	TArray<FGameplayTag> Panels;
	Panels.Add(PanelTag);
	return ShowContainerScreenMulti(PC, Panels, PanelTag);
}

bool USuspenseCoreUIManager::ShowContainerScreenMulti(
	APlayerController* PC,
	const TArray<FGameplayTag>& PanelTags,
	const FGameplayTag& DefaultPanel)
{
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("ShowContainerScreen: No PlayerController"));
		return false;
	}

	// Create screen if needed
	if (!ContainerScreen)
	{
		ContainerScreen = CreateContainerScreen(PC);
		if (!ContainerScreen)
		{
			UE_LOG(LogTemp, Warning, TEXT("ShowContainerScreen: Failed to create screen widget"));
			return false;
		}
	}

	OwningPC = PC;

	// Find player's inventory provider
	TScriptInterface<ISuspenseCoreUIDataProvider> InventoryProvider = GetPlayerInventoryProvider(PC);

	// Configure and show screen
	// ContainerScreen->SetEnabledPanels(PanelTags);
	// ContainerScreen->SetActivePanel(DefaultPanel);
	// ContainerScreen->BindToProvider(InventoryProvider);

	ContainerScreen->AddToViewport(100);
	bIsContainerScreenVisible = true;

	// Update input mode
	UpdateInputMode(PC, true);

	// Broadcast event
	OnContainerScreenVisibilityChanged.Broadcast(true);

	// Broadcast via EventBus
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		FSuspenseCoreEventData EventData;
		EventData.Source = this;
		EventBus->Publish(TAG_SuspenseCore_Event_UIContainer_Opened, EventData);
	}

	UE_LOG(LogTemp, Log, TEXT("Container screen shown"));
	return true;
}

void USuspenseCoreUIManager::HideContainerScreen()
{
	if (!ContainerScreen || !bIsContainerScreenVisible)
	{
		return;
	}

	ContainerScreen->RemoveFromParent();
	bIsContainerScreenVisible = false;

	// Update input mode
	if (APlayerController* PC = OwningPC.Get())
	{
		UpdateInputMode(PC, false);
	}

	// Cancel any drag
	CancelDragOperation();

	// Hide tooltip
	HideTooltip();

	// Broadcast event
	OnContainerScreenVisibilityChanged.Broadcast(false);

	// Broadcast via EventBus
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		FSuspenseCoreEventData EventData;
		EventData.Source = this;
		EventBus->Publish(TAG_SuspenseCore_Event_UIContainer_Closed, EventData);
	}

	UE_LOG(LogTemp, Log, TEXT("Container screen hidden"));
}

void USuspenseCoreUIManager::CloseContainerScreen(APlayerController* PC)
{
	// Verify this is the owning player
	if (PC && OwningPC.Get() == PC)
	{
		HideContainerScreen();
	}
}

bool USuspenseCoreUIManager::ToggleContainerScreen(APlayerController* PC, const FGameplayTag& PanelTag)
{
	if (bIsContainerScreenVisible)
	{
		HideContainerScreen();
		return false;
	}
	else
	{
		ShowContainerScreen(PC, PanelTag);
		return true;
	}
}

bool USuspenseCoreUIManager::IsContainerScreenVisible() const
{
	return bIsContainerScreenVisible && ContainerScreen && ContainerScreen->IsInViewport();
}

//==================================================================
// Provider Discovery
//==================================================================

TScriptInterface<ISuspenseCoreUIDataProvider> USuspenseCoreUIManager::FindProviderOnActor(
	AActor* Actor,
	ESuspenseCoreContainerType ContainerType)
{
	if (!Actor)
	{
		return nullptr;
	}

	// Get all components
	TArray<UActorComponent*> Components;
	Actor->GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (!Component)
		{
			continue;
		}

		// Check if implements interface
		if (Component->Implements<USuspenseCoreUIDataProvider>())
		{
			ISuspenseCoreUIDataProvider* Provider = Cast<ISuspenseCoreUIDataProvider>(Component);
			if (Provider && Provider->GetContainerType() == ContainerType)
			{
				TScriptInterface<ISuspenseCoreUIDataProvider> Result;
				Result.SetInterface(Provider);
				Result.SetObject(Component);

				// Register for lookup
				RegisteredProviders.Add(Provider->GetProviderID(), Component);

				return Result;
			}
		}
	}

	return nullptr;
}

TArray<TScriptInterface<ISuspenseCoreUIDataProvider>> USuspenseCoreUIManager::FindAllProvidersOnActor(AActor* Actor)
{
	TArray<TScriptInterface<ISuspenseCoreUIDataProvider>> Result;

	if (!Actor)
	{
		return Result;
	}

	TArray<UActorComponent*> Components;
	Actor->GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (!Component)
		{
			continue;
		}

		if (Component->Implements<USuspenseCoreUIDataProvider>())
		{
			ISuspenseCoreUIDataProvider* Provider = Cast<ISuspenseCoreUIDataProvider>(Component);
			if (Provider)
			{
				TScriptInterface<ISuspenseCoreUIDataProvider> Interface;
				Interface.SetInterface(Provider);
				Interface.SetObject(Component);
				Result.Add(Interface);

				// Register for lookup
				RegisteredProviders.Add(Provider->GetProviderID(), Component);
			}
		}
	}

	return Result;
}

TScriptInterface<ISuspenseCoreUIDataProvider> USuspenseCoreUIManager::FindProviderByID(const FGuid& ProviderID)
{
	TWeakObjectPtr<UObject>* Found = RegisteredProviders.Find(ProviderID);
	if (Found && Found->IsValid())
	{
		UObject* Object = Found->Get();
		if (Object && Object->Implements<USuspenseCoreUIDataProvider>())
		{
			ISuspenseCoreUIDataProvider* Provider = Cast<ISuspenseCoreUIDataProvider>(Object);
			TScriptInterface<ISuspenseCoreUIDataProvider> Result;
			Result.SetInterface(Provider);
			Result.SetObject(Object);
			return Result;
		}
	}

	return nullptr;
}

TScriptInterface<ISuspenseCoreUIDataProvider> USuspenseCoreUIManager::GetPlayerInventoryProvider(APlayerController* PC)
{
	if (!PC)
	{
		return nullptr;
	}

	// Try PlayerState first
	if (APlayerState* PS = PC->GetPlayerState<APlayerState>())
	{
		TScriptInterface<ISuspenseCoreUIDataProvider> Provider = FindProviderOnActor(
			PS, ESuspenseCoreContainerType::Inventory);
		if (Provider)
		{
			return Provider;
		}
	}

	// Try Pawn
	if (APawn* Pawn = PC->GetPawn())
	{
		return FindProviderOnActor(Pawn, ESuspenseCoreContainerType::Inventory);
	}

	return nullptr;
}

TScriptInterface<ISuspenseCoreUIDataProvider> USuspenseCoreUIManager::GetPlayerEquipmentProvider(APlayerController* PC)
{
	if (!PC)
	{
		return nullptr;
	}

	// Try Pawn first for equipment (typically on character)
	if (APawn* Pawn = PC->GetPawn())
	{
		TScriptInterface<ISuspenseCoreUIDataProvider> Provider = FindProviderOnActor(
			Pawn, ESuspenseCoreContainerType::Equipment);
		if (Provider)
		{
			return Provider;
		}
	}

	// Try PlayerState
	if (APlayerState* PS = PC->GetPlayerState<APlayerState>())
	{
		return FindProviderOnActor(PS, ESuspenseCoreContainerType::Equipment);
	}

	return nullptr;
}

//==================================================================
// Notifications
//==================================================================

void USuspenseCoreUIManager::ShowNotification(const FSuspenseCoreUINotification& Notification)
{
	// Broadcast delegate for UI to handle
	OnUINotification.Broadcast(Notification);

	// Also broadcast via EventBus
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		FGameplayTag FeedbackTag = USuspenseCoreUIEventHelpers::GetFeedbackTypeTag(Notification.Type);

		FSuspenseCoreEventData EventData;
		EventData.Source = this;
		EventData.SetString(FName("Message"), Notification.Message.ToString());
		EventData.SetInt(FName("FeedbackType"), static_cast<int32>(Notification.Type));

		EventBus->Publish(FeedbackTag, EventData);
	}
}

void USuspenseCoreUIManager::ShowSimpleNotification(ESuspenseCoreUIFeedbackType Type, const FText& Message)
{
	FSuspenseCoreUINotification Notification;
	Notification.Type = Type;
	Notification.Message = Message;
	Notification.Duration = 3.0f;

	ShowNotification(Notification);
}

void USuspenseCoreUIManager::ShowItemPickupNotification(const FSuspenseCoreItemUIData& Item, int32 Quantity)
{
	FSuspenseCoreUINotification Notification = FSuspenseCoreUINotification::CreateItemPickup(Item, Quantity);
	ShowNotification(Notification);
}

//==================================================================
// Tooltip Management
//==================================================================

void USuspenseCoreUIManager::ShowItemTooltip(const FSuspenseCoreItemUIData& Item, const FVector2D& ScreenPosition)
{
	// TODO: Create/show tooltip widget
	// For now just log
	UE_LOG(LogTemp, Verbose, TEXT("ShowItemTooltip: %s at (%.0f, %.0f)"),
		*Item.DisplayName.ToString(), ScreenPosition.X, ScreenPosition.Y);
}

void USuspenseCoreUIManager::HideTooltip()
{
	if (TooltipWidget && TooltipWidget->IsInViewport())
	{
		TooltipWidget->RemoveFromParent();
	}
}

bool USuspenseCoreUIManager::IsTooltipVisible() const
{
	return TooltipWidget && TooltipWidget->IsInViewport();
}

//==================================================================
// Drag-Drop Support
//==================================================================

bool USuspenseCoreUIManager::StartDragOperation(const FSuspenseCoreDragData& DragData)
{
	if (!DragData.bIsValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("StartDragOperation: Invalid drag data"));
		return false;
	}

	CurrentDragData = DragData;

	// Broadcast drag started event
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		FSuspenseCoreEventData EventData;
		EventData.Source = this;
		EventData.SetString(FName("ItemInstanceID"), DragData.Item.InstanceID.ToString());
		EventData.SetInt(FName("SourceSlot"), DragData.SourceSlot);

		EventBus->Publish(TAG_SuspenseCore_Event_UIContainer_DragStarted, EventData);
	}

	UE_LOG(LogTemp, Verbose, TEXT("Drag started: %s from slot %d"),
		*DragData.Item.DisplayName.ToString(), DragData.SourceSlot);

	return true;
}

void USuspenseCoreUIManager::CancelDragOperation()
{
	if (!CurrentDragData.bIsValid)
	{
		return;
	}

	// Broadcast drag ended event
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		FSuspenseCoreEventData EventData;
		EventData.Source = this;
		EventData.SetBool(FName("Cancelled"), true);

		EventBus->Publish(TAG_SuspenseCore_Event_UIContainer_DragEnded, EventData);
	}

	CurrentDragData = FSuspenseCoreDragData();

	UE_LOG(LogTemp, Verbose, TEXT("Drag cancelled"));
}

//==================================================================
// Configuration
//==================================================================

void USuspenseCoreUIManager::SetScreenConfig(const FSuspenseCoreScreenConfig& NewConfig)
{
	ScreenConfig = NewConfig;
}

//==================================================================
// EventBus Integration
//==================================================================

void USuspenseCoreUIManager::SubscribeToEvents()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	// Subscribe to UI feedback events
	// Note: Actual subscription mechanism depends on EventBus implementation
}

void USuspenseCoreUIManager::UnsubscribeFromEvents()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	for (FDelegateHandle& Handle : EventSubscriptions)
	{
		// Unsubscribe logic
	}
	EventSubscriptions.Empty();
}

void USuspenseCoreUIManager::OnUIFeedbackEvent(const FSuspenseCoreEventData& EventData)
{
	// Handle feedback from game systems
	FString Message = EventData.GetString(FName("Message"));
	int32 FeedbackType = EventData.GetInt(FName("FeedbackType"));

	ShowSimpleNotification(
		static_cast<ESuspenseCoreUIFeedbackType>(FeedbackType),
		FText::FromString(Message));
}

void USuspenseCoreUIManager::OnContainerOpenedEvent(const FSuspenseCoreEventData& EventData)
{
	// External request to open container
}

void USuspenseCoreUIManager::OnContainerClosedEvent(const FSuspenseCoreEventData& EventData)
{
	// External request to close container
}

USuspenseCoreEventBus* USuspenseCoreUIManager::GetEventBus() const
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return nullptr;
	}

	USuspenseCoreEventManager* EventManager = GI->GetSubsystem<USuspenseCoreEventManager>();
	if (!EventManager)
	{
		return nullptr;
	}

	const_cast<USuspenseCoreUIManager*>(this)->CachedEventBus = EventManager->GetEventBus();
	return CachedEventBus.Get();
}

//==================================================================
// Internal
//==================================================================

USuspenseCoreContainerScreenWidget* USuspenseCoreUIManager::CreateContainerScreen(APlayerController* PC)
{
	if (!PC)
	{
		return nullptr;
	}

	// If we have a configured class, create it
	if (ContainerScreenClass)
	{
		USuspenseCoreContainerScreenWidget* Widget = CreateWidget<USuspenseCoreContainerScreenWidget>(PC, ContainerScreenClass);
		return Widget;
	}

	// TODO: Create default screen widget
	// For now return nullptr - will be implemented in Phase 7.4
	UE_LOG(LogTemp, Warning, TEXT("CreateContainerScreen: No ContainerScreenClass configured"));
	return nullptr;
}

USuspenseCoreTooltipWidget* USuspenseCoreUIManager::CreateTooltipWidget(APlayerController* PC)
{
	if (!PC)
	{
		return nullptr;
	}

	if (TooltipWidgetClass)
	{
		return CreateWidget<USuspenseCoreTooltipWidget>(PC, TooltipWidgetClass);
	}

	return nullptr;
}

void USuspenseCoreUIManager::UpdateInputMode(APlayerController* PC, bool bShowingUI)
{
	if (!PC)
	{
		return;
	}

	if (bShowingUI)
	{
		// Game and UI input
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(true);
	}
	else
	{
		// Game only
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(false);
	}
}
