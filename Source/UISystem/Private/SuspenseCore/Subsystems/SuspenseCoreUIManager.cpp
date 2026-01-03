// SuspenseCoreUIManager.cpp
// SuspenseCore - UI Manager Subsystem Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Subsystems/SuspenseCoreUIManager.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreUIDataProvider.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreUIContainer.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/UI/SuspenseCoreUIEvents.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Widgets/Layout/SuspenseCoreContainerScreenWidget.h"
#include "SuspenseCore/Widgets/Layout/SuspenseCorePanelSwitcherWidget.h"
#include "SuspenseCore/Widgets/Layout/SuspenseCorePanelWidget.h"
#include "SuspenseCore/Widgets/Base/SuspenseCoreBaseContainerWidget.h"
#include "SuspenseCore/Widgets/Layout/SuspenseCoreContainerPairLayoutWidget.h"
#include "SuspenseCore/Widgets/Tooltip/SuspenseCoreTooltipWidget.h"
#include "SuspenseCore/Widgets/SuspenseCoreMasterHUDWidget.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
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
	// Panel identification tags - use SuspenseCore.UI.Panel.* namespace
	// These are for panel identification, not EventBus events
	static const FGameplayTag PanelEquipmentTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.UI.Panel.Equipment"));
	static const FGameplayTag PanelStashTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.UI.Panel.Stash"));
	static const FGameplayTag PanelTradingTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.UI.Panel.Trading"));

	// Equipment panel - shows Equipment (left) + Inventory (right) side by side
	// This is the default/main panel for character management
	FSuspenseCorePanelConfig EquipmentPanel;
	EquipmentPanel.PanelTag = PanelEquipmentTag;
	EquipmentPanel.DisplayName = NSLOCTEXT("SuspenseCore", "Panel_Equipment", "EQUIPMENT");
	EquipmentPanel.ContainerTypes.Add(ESuspenseCoreContainerType::Equipment);
	EquipmentPanel.ContainerTypes.Add(ESuspenseCoreContainerType::Inventory);
	EquipmentPanel.bHorizontalLayout = true;
	EquipmentPanel.SortOrder = 0;
	EquipmentPanel.bIsEnabled = true;
	ScreenConfig.Panels.Add(EquipmentPanel);

	// Stash panel - shows Stash (left) + Inventory (right) side by side
	// Enabled when near a stash container
	FSuspenseCorePanelConfig StashPanel;
	StashPanel.PanelTag = PanelStashTag;
	StashPanel.DisplayName = NSLOCTEXT("SuspenseCore", "Panel_Stash", "STASH");
	StashPanel.ContainerTypes.Add(ESuspenseCoreContainerType::Stash);
	StashPanel.ContainerTypes.Add(ESuspenseCoreContainerType::Inventory);
	StashPanel.bHorizontalLayout = true;
	StashPanel.SortOrder = 1;
	StashPanel.bIsEnabled = false; // Disabled by default, enabled when near stash
	ScreenConfig.Panels.Add(StashPanel);

	// Trader panel - shows Trader (left) + Inventory (right) side by side
	// Enabled when interacting with a trader NPC
	FSuspenseCorePanelConfig TraderPanel;
	TraderPanel.PanelTag = PanelTradingTag;
	TraderPanel.DisplayName = NSLOCTEXT("SuspenseCore", "Panel_Trader", "TRADER");
	TraderPanel.ContainerTypes.Add(ESuspenseCoreContainerType::Trader);
	TraderPanel.ContainerTypes.Add(ESuspenseCoreContainerType::Inventory);
	TraderPanel.bHorizontalLayout = true;
	TraderPanel.SortOrder = 2;
	TraderPanel.bIsEnabled = false; // Disabled by default, enabled when trading
	ScreenConfig.Panels.Add(TraderPanel);

	// Default to Equipment panel (combined Equipment + Inventory view)
	ScreenConfig.DefaultPanelTag = PanelEquipmentTag;
	ScreenConfig.bAllowCrossPanelDrag = true;
	ScreenConfig.bShowWeight = true;
	ScreenConfig.bShowCurrency = true;
}

void USuspenseCoreUIManager::BindProvidersToScreen(APlayerController* PC)
{
	if (!ContainerScreen || !PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("BindProvidersToScreen: No ContainerScreen or PC"));
		return;
	}

	// Get PanelSwitcher from ContainerScreen
	USuspenseCorePanelSwitcherWidget* PanelSwitcher = ContainerScreen->GetPanelSwitcher();
	if (!PanelSwitcher)
	{
		UE_LOG(LogTemp, Warning, TEXT("BindProvidersToScreen: No PanelSwitcher on ContainerScreen"));
		return;
	}

	// Get player's pawn
	APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("BindProvidersToScreen: No pawn"));
		return;
	}

	// Find all providers on player (Pawn and PlayerState)
	TArray<TScriptInterface<ISuspenseCoreUIDataProvider>> AllProviders = FindAllProvidersOnActor(Pawn);

	if (APlayerState* PS = PC->GetPlayerState<APlayerState>())
	{
		TArray<TScriptInterface<ISuspenseCoreUIDataProvider>> StateProviders = FindAllProvidersOnActor(PS);
		AllProviders.Append(StateProviders);
	}

	UE_LOG(LogTemp, Log, TEXT("BindProvidersToScreen: Found %d providers"), AllProviders.Num());

	// Create provider lookup by container type
	TMap<ESuspenseCoreContainerType, TScriptInterface<ISuspenseCoreUIDataProvider>> ProvidersByType;
	for (const TScriptInterface<ISuspenseCoreUIDataProvider>& Provider : AllProviders)
	{
		if (ISuspenseCoreUIDataProvider* ProviderInterface = Provider.GetInterface())
		{
			ESuspenseCoreContainerType Type = ProviderInterface->GetContainerType();
			ProvidersByType.Add(Type, Provider);
			UE_LOG(LogTemp, Log, TEXT("  Provider type: %d"), static_cast<int32>(Type));
		}
	}

	// Iterate all content widgets in PanelSwitcher
	int32 TabCount = PanelSwitcher->GetTabCount();
	int32 BoundCount = 0;

	for (int32 TabIndex = 0; TabIndex < TabCount; ++TabIndex)
	{
		UUserWidget* ContentWidget = PanelSwitcher->GetTabContent(TabIndex);
		if (!ContentWidget)
		{
			continue;
		}

		// Check if this widget implements ISuspenseCoreUIContainer
		// First try cast to base container widget (which has ExpectedContainerType)
		USuspenseCoreBaseContainerWidget* ContainerWidget = Cast<USuspenseCoreBaseContainerWidget>(ContentWidget);
		if (ContainerWidget)
		{
			// Get expected container type
			ESuspenseCoreContainerType ExpectedType = ContainerWidget->GetExpectedContainerType();

			// Find matching provider
			if (TScriptInterface<ISuspenseCoreUIDataProvider>* FoundProvider = ProvidersByType.Find(ExpectedType))
			{
				// Bind provider to widget
				ContainerWidget->BindToProvider(*FoundProvider);
				BoundCount++;

				UE_LOG(LogTemp, Log, TEXT("BindProvidersToScreen: Bound provider (type=%d) to tab %d (%s)"),
					static_cast<int32>(ExpectedType), TabIndex, *ContentWidget->GetClass()->GetName());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("BindProvidersToScreen: No provider for container type %d (tab %d)"),
					static_cast<int32>(ExpectedType), TabIndex);
			}
		}
		else
		{
			// Try ContainerPairLayoutWidget (holds two containers like Equipment+Inventory)
			USuspenseCoreContainerPairLayoutWidget* PairLayout = Cast<USuspenseCoreContainerPairLayoutWidget>(ContentWidget);
			if (PairLayout)
			{
				// ContainerPairLayoutWidget handles its own provider binding
				// Pass the provider map so it can bind both containers
				for (USuspenseCoreBaseContainerWidget* ChildContainer : PairLayout->GetAllContainers())
				{
					if (ChildContainer)
					{
						ESuspenseCoreContainerType ExpectedType = ChildContainer->GetExpectedContainerType();
						if (TScriptInterface<ISuspenseCoreUIDataProvider>* FoundProvider = ProvidersByType.Find(ExpectedType))
						{
							ChildContainer->BindToProvider(*FoundProvider);
							BoundCount++;
							UE_LOG(LogTemp, Log, TEXT("BindProvidersToScreen: Bound provider (type=%d) to PairLayout child container (%s)"),
								static_cast<int32>(ExpectedType), *ChildContainer->GetClass()->GetName());
						}
						else
						{
							UE_LOG(LogTemp, Warning, TEXT("BindProvidersToScreen: No provider for PairLayout child type %d"),
								static_cast<int32>(ExpectedType));
						}
					}
				}
			}
			else
			{
				// Widget doesn't implement base container - might be custom widget
				UE_LOG(LogTemp, Verbose, TEXT("BindProvidersToScreen: Tab %d (%s) is not a container widget"),
					TabIndex, *ContentWidget->GetClass()->GetName());
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("BindProvidersToScreen: Bound %d/%d container widgets"), BoundCount, TabCount);
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

	// NEW ARCHITECTURE: Screen is configured via Blueprint (PanelSwitcher.TabConfigs)
	// No need to call InitializeScreen() - PanelSwitcher handles everything

	// Add to viewport FIRST (NativeConstruct creates content widgets in PanelSwitcher)
	ContainerScreen->AddToViewport(100);

	// Find providers and bind to content widgets AFTER widgets are created
	BindProvidersToScreen(PC);

	// Activate screen (sets input mode, opens default/remembered panel)
	ContainerScreen->ActivateScreen();

	// Override with requested panel if specified
	if (DefaultPanel.IsValid())
	{
		ContainerScreen->OpenPanelByTag(DefaultPanel);
	}

	bIsContainerScreenVisible = true;

	// Broadcast event
	OnContainerScreenVisibilityChanged.Broadcast(true);

	// Broadcast via EventBus
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		static const FGameplayTag ContainerOpenedTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UIContainer.Opened"));
		FSuspenseCoreEventData EventData;
		EventData.Source = this;
		EventBus->Publish(ContainerOpenedTag, EventData);
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
		static const FGameplayTag ContainerClosedTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UIContainer.Closed"));
		FSuspenseCoreEventData EventData;
		EventData.Source = this;
		EventBus->Publish(ContainerClosedTag, EventData);
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
	// Get player controller
	APlayerController* PC = OwningPC.Get();
	if (!PC)
	{
		// Try to get from game instance
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UWorld* World = GI->GetWorld())
			{
				PC = World->GetFirstPlayerController();
			}
		}
	}

	if (!PC)
	{
		return;
	}

	// Create tooltip widget if needed
	if (!TooltipWidget)
	{
		TooltipWidget = CreateTooltipWidget(PC);
		if (!TooltipWidget)
		{
			return;
		}
	}

	// Ensure tooltip is in viewport with very high Z-order (above everything including drag visuals)
	if (!TooltipWidget->IsInViewport())
	{
		TooltipWidget->AddToViewport(10000);
	}

	// Show tooltip for item
	TooltipWidget->ShowForItem(Item, ScreenPosition);
}

void USuspenseCoreUIManager::HideTooltip()
{
	if (TooltipWidget)
	{
		TooltipWidget->Hide();
	}
}

bool USuspenseCoreUIManager::IsTooltipVisible() const
{
	return TooltipWidget && TooltipWidget->IsVisible();
}

//==================================================================
// Master HUD Management
//==================================================================

USuspenseCoreMasterHUDWidget* USuspenseCoreUIManager::CreateMasterHUD(APlayerController* PC)
{
	UE_LOG(LogTemp, Log, TEXT("UIManager::CreateMasterHUD - Starting..."));

	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateMasterHUD: Invalid PlayerController"));
		return nullptr;
	}

	// Destroy existing HUD if any
	if (MasterHUD)
	{
		UE_LOG(LogTemp, Log, TEXT("CreateMasterHUD: Destroying existing MasterHUD"));
		DestroyMasterHUD();
	}

	// Check if we have a class configured
	if (!MasterHUDWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateMasterHUD: MasterHUDWidgetClass not configured in UIManager"));
		return nullptr;
	}

	UE_LOG(LogTemp, Log, TEXT("CreateMasterHUD: Using widget class: %s"), *MasterHUDWidgetClass->GetName());

	// Create the master HUD widget
	MasterHUD = CreateWidget<USuspenseCoreMasterHUDWidget>(PC, MasterHUDWidgetClass);
	if (!MasterHUD)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateMasterHUD: Failed to create MasterHUD widget"));
		return nullptr;
	}

	UE_LOG(LogTemp, Log, TEXT("CreateMasterHUD: Widget created successfully, adding to viewport"));

	// Add to viewport with standard HUD Z-order
	MasterHUD->AddToViewport(0);

	// Initialize with pawn if available
	if (APawn* Pawn = PC->GetPawn())
	{
		UE_LOG(LogTemp, Log, TEXT("CreateMasterHUD: Initializing with pawn: %s"), *Pawn->GetName());
		MasterHUD->InitializeHUD(Pawn);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateMasterHUD: No pawn available for initialization"));
	}

	UE_LOG(LogTemp, Log, TEXT("CreateMasterHUD: Complete!"));
	return MasterHUD;
}

void USuspenseCoreUIManager::DestroyMasterHUD()
{
	if (MasterHUD)
	{
		MasterHUD->RemoveFromParent();
		MasterHUD = nullptr;
	}
}

void USuspenseCoreUIManager::InitializeWeaponHUD(AActor* WeaponActor)
{
	UE_LOG(LogTemp, Log, TEXT("UIManager::InitializeWeaponHUD - MasterHUD: %s, WeaponActor: %s"),
		MasterHUD ? TEXT("valid") : TEXT("NULL"),
		WeaponActor ? *WeaponActor->GetName() : TEXT("nullptr"));

	if (MasterHUD)
	{
		MasterHUD->InitializeWeaponHUD(WeaponActor);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager::InitializeWeaponHUD - MasterHUD is NULL! Call CreateMasterHUD first."));
	}
}

void USuspenseCoreUIManager::ClearWeaponHUD()
{
	if (MasterHUD)
	{
		MasterHUD->ClearWeaponHUD();
	}
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
		static const FGameplayTag DragStartedTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UIContainer.DragStarted"));
		FSuspenseCoreEventData EventData;
		EventData.Source = this;
		EventData.SetString(FName("ItemInstanceID"), DragData.Item.InstanceID.ToString());
		EventData.SetInt(FName("SourceSlot"), DragData.SourceSlot);

		EventBus->Publish(DragStartedTag, EventData);
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
		static const FGameplayTag DragEndedTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UIContainer.DragEnded"));
		FSuspenseCoreEventData EventData;
		EventData.Source = this;
		EventData.SetBool(FName("Cancelled"), true);

		EventBus->Publish(DragEndedTag, EventData);
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
		UE_LOG(LogTemp, Warning, TEXT("UIManager::SubscribeToEvents - EventBus not available"));
		return;
	}

	using namespace SuspenseCoreEquipmentTags::Event;

	// Subscribe to item equipped event
	ItemEquippedHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_ItemEquipped,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreUIManager::OnItemEquippedEvent),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to item unequipped event
	ItemUnequippedHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_ItemUnequipped,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreUIManager::OnItemUnequippedEvent),
		ESuspenseCoreEventPriority::Normal
	);

	UE_LOG(LogTemp, Log, TEXT("UIManager::SubscribeToEvents - Subscribed to equipment events"));
}

void USuspenseCoreUIManager::UnsubscribeFromEvents()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	// Unsubscribe from equipment events
	EventBus->Unsubscribe(ItemEquippedHandle);
	EventBus->Unsubscribe(ItemUnequippedHandle);

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

void USuspenseCoreUIManager::OnItemEquippedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// Get the equipped item actor from event data (Source is the item actor)
	AActor* ItemActor = Cast<AActor>(EventData.Source.Get());

	UE_LOG(LogTemp, Log, TEXT("UIManager::OnItemEquippedEvent - ItemActor: %s"),
		ItemActor ? *ItemActor->GetName() : TEXT("nullptr"));

	// Check if this is a weapon
	bool bIsWeapon = false;

	// Check if item has weapon tag in event data
	FGameplayTag ItemTypeTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Item.Type.Weapon"), false);
	bIsWeapon = EventData.Tags.HasTag(ItemTypeTag);

	// Fallback: check event string data
	if (!bIsWeapon)
	{
		FString ItemType = EventData.GetString(TEXT("ItemType"));
		bIsWeapon = ItemType.Contains(TEXT("Weapon"));
	}

	// Fallback: check slot index (slots 0/1 are weapon slots)
	if (!bIsWeapon)
	{
		int32 SlotIndex = EventData.GetInt(TEXT("SlotIndex"), -1);
		if (SlotIndex == 0 || SlotIndex == 1) // Primary/Secondary weapon slots
		{
			bIsWeapon = true;
		}
	}

	if (bIsWeapon && ItemActor)
	{
		UE_LOG(LogTemp, Log, TEXT("UIManager::OnItemEquippedEvent - Initializing weapon HUD for: %s"), *ItemActor->GetName());
		InitializeWeaponHUD(ItemActor);
	}
}

void USuspenseCoreUIManager::OnItemUnequippedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	UE_LOG(LogTemp, Log, TEXT("UIManager::OnItemUnequippedEvent - Clearing weapon HUD"));

	// Check if this was a weapon slot
	int32 SlotIndex = EventData.GetInt(TEXT("SlotIndex"), -1);
	if (SlotIndex == 0 || SlotIndex == 1) // Primary/Secondary weapon slots
	{
		ClearWeaponHUD();
	}
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
