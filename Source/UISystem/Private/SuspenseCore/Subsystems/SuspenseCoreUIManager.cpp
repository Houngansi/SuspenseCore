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
#include "SuspenseCore/Widgets/HUD/SuspenseCoreMagazineTooltipWidget.h"
#include "SuspenseCore/Widgets/HUD/SuspenseCoreMagazineInspectionWidget.h"
#include "SuspenseCore/Widgets/SuspenseCoreMasterHUDWidget.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreMagazineTooltipWidget.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreMagazineInspectionWidget.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "SuspenseCore/Components/Presentation/SuspenseCoreEquipmentActorFactory.h"
#include "Components/ActorComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

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

	// Try to auto-load widget classes from common Blueprint paths
	LoadDefaultWidgetClasses();

	// Subscribe to EventBus
	SubscribeToEvents();

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreUIManager initialized"));
}

void USuspenseCoreUIManager::LoadDefaultWidgetClasses()
{
	// Try to load Magazine Tooltip Widget Blueprint
	if (!MagazineTooltipWidgetClass)
	{
		// Common paths where the Blueprint might be located
		static const TCHAR* MagazineTooltipPaths[] = {
			TEXT("/Game/UI/Widgets/WBP_MagazineTooltip.WBP_MagazineTooltip_C"),
			TEXT("/Game/UI/HUD/WBP_MagazineTooltip.WBP_MagazineTooltip_C"),
			TEXT("/Game/Blueprints/UI/WBP_MagazineTooltip.WBP_MagazineTooltip_C"),
			TEXT("/Game/SuspenseCore/UI/WBP_MagazineTooltip.WBP_MagazineTooltip_C"),
		};

		for (const TCHAR* Path : MagazineTooltipPaths)
		{
			UClass* FoundClass = LoadClass<USuspenseCoreMagazineTooltipWidget>(nullptr, Path);
			if (FoundClass)
			{
				MagazineTooltipWidgetClass = FoundClass;
				UE_LOG(LogTemp, Log, TEXT("UIManager: Auto-loaded MagazineTooltipWidgetClass from %s"), Path);
				break;
			}
		}
	}

	// Try to load Magazine Inspection Widget Blueprint
	if (!MagazineInspectionWidgetClass)
	{
		static const TCHAR* MagazineInspectionPaths[] = {
			TEXT("/Game/UI/Widgets/WBP_MagazineInspector.WBP_MagazineInspector_C"),
			TEXT("/Game/UI/HUD/WBP_MagazineInspector.WBP_MagazineInspector_C"),
			TEXT("/Game/Blueprints/UI/WBP_MagazineInspector.WBP_MagazineInspector_C"),
			TEXT("/Game/SuspenseCore/UI/WBP_MagazineInspector.WBP_MagazineInspector_C"),
		};

		for (const TCHAR* Path : MagazineInspectionPaths)
		{
			UClass* FoundClass = LoadClass<USuspenseCoreMagazineInspectionWidget>(nullptr, Path);
			if (FoundClass)
			{
				MagazineInspectionWidgetClass = FoundClass;
				UE_LOG(LogTemp, Log, TEXT("UIManager: Auto-loaded MagazineInspectionWidgetClass from %s"), Path);
				break;
			}
		}
	}

	// Try to load standard Tooltip Widget Blueprint
	if (!TooltipWidgetClass)
	{
		static const TCHAR* TooltipPaths[] = {
			TEXT("/Game/UI/Widgets/WBP_Tooltip.WBP_Tooltip_C"),
			TEXT("/Game/UI/HUD/WBP_Tooltip.WBP_Tooltip_C"),
			TEXT("/Game/Blueprints/UI/WBP_Tooltip.WBP_Tooltip_C"),
			TEXT("/Game/SuspenseCore/UI/WBP_Tooltip.WBP_Tooltip_C"),
		};

		for (const TCHAR* Path : TooltipPaths)
		{
			UClass* FoundClass = LoadClass<USuspenseCoreTooltipWidget>(nullptr, Path);
			if (FoundClass)
			{
				TooltipWidgetClass = FoundClass;
				UE_LOG(LogTemp, Log, TEXT("UIManager: Auto-loaded TooltipWidgetClass from %s"), Path);
				break;
			}
		}
	}

	// Log warnings for missing widget classes
	if (!MagazineTooltipWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: MagazineTooltipWidgetClass not configured. Create WBP_MagazineTooltip Blueprint or call ConfigureWidgetClasses()."));
	}
	if (!MagazineInspectionWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager: MagazineInspectionWidgetClass not configured. Create WBP_MagazineInspector Blueprint or call ConfigureWidgetClasses()."));
	}
}

void USuspenseCoreUIManager::ConfigureWidgetClasses(
	TSubclassOf<USuspenseCoreMagazineTooltipWidget> InMagazineTooltipClass,
	TSubclassOf<USuspenseCoreMagazineInspectionWidget> InMagazineInspectionClass)
{
	if (InMagazineTooltipClass)
	{
		MagazineTooltipWidgetClass = InMagazineTooltipClass;
		UE_LOG(LogTemp, Log, TEXT("UIManager: MagazineTooltipWidgetClass configured to %s"), *InMagazineTooltipClass->GetName());
	}

	if (InMagazineInspectionClass)
	{
		MagazineInspectionWidgetClass = InMagazineInspectionClass;
		UE_LOG(LogTemp, Log, TEXT("UIManager: MagazineInspectionWidgetClass configured to %s"), *InMagazineInspectionClass->GetName());
	}
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

	if (MagazineTooltipWidget)
	{
		MagazineTooltipWidget->RemoveFromParent();
		MagazineTooltipWidget = nullptr;
	}

	if (MagazineInspectionWidget)
	{
		MagazineInspectionWidget->RemoveFromParent();
		MagazineInspectionWidget = nullptr;
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

	// Check if this is a magazine item - use specialized tooltip
	bool bIsMagazine = IsMagazineItem(Item);
	UE_LOG(LogTemp, Log, TEXT("ShowItemTooltip: Item=%s, IsMagazine=%s, MagazineTooltipClass=%s"),
		*Item.ItemID.ToString(),
		bIsMagazine ? TEXT("YES") : TEXT("NO"),
		MagazineTooltipWidgetClass ? TEXT("SET") : TEXT("NOT SET"));

	if (bIsMagazine && MagazineTooltipWidgetClass)
	{
		// Convert to magazine tooltip data and show magazine tooltip
		FSuspenseCoreMagazineTooltipData MagazineData;
		MagazineData.MagazineID = Item.ItemID;
		MagazineData.DisplayName = Item.DisplayName;
		MagazineData.RarityTag = Item.RarityTag;
		// Note: Additional magazine data (CurrentRounds, MaxCapacity, Icon, etc.) should be
		// retrieved from MagazineComponent or stored in item payload
		// For now show basic magazine tooltip

		UE_LOG(LogTemp, Log, TEXT("ShowItemTooltip: Showing MAGAZINE tooltip for %s"), *Item.ItemID.ToString());
		ShowMagazineTooltip(MagazineData, ScreenPosition);
		return;
	}
	else if (bIsMagazine && !MagazineTooltipWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("ShowItemTooltip: Item %s IS a magazine but MagazineTooltipWidgetClass is NOT configured!"), *Item.ItemID.ToString());
	}

	// Standard item tooltip
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

void USuspenseCoreUIManager::ShowMagazineTooltip(const FSuspenseCoreMagazineTooltipData& MagazineData, const FVector2D& ScreenPosition)
{
	// Get player controller
	APlayerController* PC = OwningPC.Get();
	if (!PC)
	{
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

	// Hide standard tooltip if visible
	if (TooltipWidget && TooltipWidget->IsTooltipVisible())
	{
		TooltipWidget->Hide();
	}

	// Create magazine tooltip widget if needed
	if (!MagazineTooltipWidget)
	{
		MagazineTooltipWidget = CreateMagazineTooltipWidget(PC);
		if (!MagazineTooltipWidget)
		{
			UE_LOG(LogTemp, Warning, TEXT("ShowMagazineTooltip: Failed to create magazine tooltip widget"));
			return;
		}
	}

	// Ensure tooltip is in viewport with very high Z-order
	if (!MagazineTooltipWidget->IsInViewport())
	{
		MagazineTooltipWidget->AddToViewport(10000);
	}

	// Show magazine tooltip using interface
	ISuspenseCoreMagazineTooltipWidgetInterface::Execute_ShowMagazineTooltip(MagazineTooltipWidget, MagazineData, ScreenPosition);
}

void USuspenseCoreUIManager::HideTooltip()
{
	// Hide standard tooltip
	if (TooltipWidget)
	{
		TooltipWidget->Hide();
	}

	// Hide magazine tooltip
	if (MagazineTooltipWidget)
	{
		ISuspenseCoreMagazineTooltipWidgetInterface::Execute_HideMagazineTooltip(MagazineTooltipWidget);
	}
}

bool USuspenseCoreUIManager::IsTooltipVisible() const
{
	bool bStandardVisible = TooltipWidget && TooltipWidget->IsTooltipVisible();
	bool bMagazineVisible = MagazineTooltipWidget && ISuspenseCoreMagazineTooltipWidgetInterface::Execute_IsMagazineTooltipVisible(MagazineTooltipWidget);

	return bStandardVisible || bMagazineVisible;
}

bool USuspenseCoreUIManager::IsMagazineTooltipVisible() const
{
	return MagazineTooltipWidget && ISuspenseCoreMagazineTooltipWidgetInterface::Execute_IsMagazineTooltipVisible(MagazineTooltipWidget);
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
	if (MasterHUD.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("CreateMasterHUD: Destroying existing MasterHUD"));
		DestroyMasterHUD();
	}

	// Check if we have a class configured, fallback to base class
	if (!MasterHUDWidgetClass)
	{
		UE_LOG(LogTemp, Log, TEXT("CreateMasterHUD: MasterHUDWidgetClass not configured, using default USuspenseCoreMasterHUDWidget"));
		MasterHUDWidgetClass = USuspenseCoreMasterHUDWidget::StaticClass();
	}

	UE_LOG(LogTemp, Log, TEXT("CreateMasterHUD: Using widget class: %s"), *MasterHUDWidgetClass->GetName());

	// Create the master HUD widget
	USuspenseCoreMasterHUDWidget* NewHUD = CreateWidget<USuspenseCoreMasterHUDWidget>(PC, MasterHUDWidgetClass);
	MasterHUD = NewHUD;
	if (!MasterHUD.IsValid())
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
	return MasterHUD.Get();
}

void USuspenseCoreUIManager::DestroyMasterHUD()
{
	if (MasterHUD.IsValid())
	{
		MasterHUD->RemoveFromParent();
		MasterHUD.Reset();
	}
}

void USuspenseCoreUIManager::InitializeWeaponHUD(AActor* WeaponActor)
{
	UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════════════════════════════"));
	UE_LOG(LogTemp, Warning, TEXT("UIManager::InitializeWeaponHUD CALLED - Frame: %llu"), GFrameCounter);
	UE_LOG(LogTemp, Warning, TEXT("  MasterHUD: %s, WeaponActor: %s"),
		MasterHUD.IsValid() ? TEXT("valid") : TEXT("NULL/stale"),
		WeaponActor ? *WeaponActor->GetName() : TEXT("nullptr"));
	UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════════════════════════════"));

	// Try to find existing MasterHUD in viewport if not cached or stale
	if (!MasterHUD.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("UIManager::InitializeWeaponHUD - Searching for existing MasterHUD in viewport..."));

		UWorld* World = GetWorld();
		if (World)
		{
			// Search for existing MasterHUD widget on viewport
			TArray<UUserWidget*> FoundWidgets;
			UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, FoundWidgets, USuspenseCoreMasterHUDWidget::StaticClass(), false);

			UE_LOG(LogTemp, Log, TEXT("UIManager::InitializeWeaponHUD - Found %d MasterHUD widgets"), FoundWidgets.Num());

			for (UUserWidget* Widget : FoundWidgets)
			{
				if (USuspenseCoreMasterHUDWidget* FoundHUD = Cast<USuspenseCoreMasterHUDWidget>(Widget))
				{
					MasterHUD = FoundHUD;
					UE_LOG(LogTemp, Log, TEXT("UIManager::InitializeWeaponHUD - Cached existing MasterHUD: %s"), *MasterHUD->GetName());
					break;
				}
			}
		}
	}

	if (MasterHUD.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("UIManager::InitializeWeaponHUD - Calling MasterHUD->InitializeWeaponHUD"));
		MasterHUD->InitializeWeaponHUD(WeaponActor);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager::InitializeWeaponHUD - No MasterHUD found! Make sure PlayerController creates WBP_MasterHUD."));
	}
}

void USuspenseCoreUIManager::ClearWeaponHUD()
{
	UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════════════════════════════"));
	UE_LOG(LogTemp, Warning, TEXT("UIManager::ClearWeaponHUD CALLED - Frame: %llu"), GFrameCounter);
	UE_LOG(LogTemp, Warning, TEXT("  MasterHUD: %s"),
		MasterHUD.IsValid() ? TEXT("valid") : TEXT("NULL/stale"));
	UE_LOG(LogTemp, Warning, TEXT("═══════════════════════════════════════════════════════════════"));

	// Try to find existing MasterHUD if not cached or stale
	if (!MasterHUD.IsValid())
	{
		UWorld* World = GetWorld();
		if (World)
		{
			TArray<UUserWidget*> FoundWidgets;
			UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, FoundWidgets, USuspenseCoreMasterHUDWidget::StaticClass(), false);

			for (UUserWidget* Widget : FoundWidgets)
			{
				if (USuspenseCoreMasterHUDWidget* FoundHUD = Cast<USuspenseCoreMasterHUDWidget>(Widget))
				{
					MasterHUD = FoundHUD;
					UE_LOG(LogTemp, Log, TEXT("UIManager::ClearWeaponHUD - Found MasterHUD: %s"), *MasterHUD->GetName());
					break;
				}
			}
		}
	}

	if (MasterHUD.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("UIManager::ClearWeaponHUD - Calling MasterHUD->ClearWeaponHUD"));
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

	// Subscribe to ItemEquipped event - this fires when item is equipped
	// We get the weapon actor from ActorFactory using the slot index
	ItemEquippedHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_ItemEquipped,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreUIManager::OnItemEquippedEvent),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to ItemUnequipped event
	ItemUnequippedHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_ItemUnequipped,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreUIManager::OnItemUnequippedEvent),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to Visual_Detached event - this fires when weapon actor is detached/hidden
	VisualDetachedHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_Visual_Detached,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreUIManager::OnVisualDetachedEvent),
		ESuspenseCoreEventPriority::Normal
	);

	UE_LOG(LogTemp, Log, TEXT("UIManager::SubscribeToEvents - Subscribed to ItemEquipped/Unequipped/VisualDetached events"));
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
	EventBus->Unsubscribe(VisualDetachedHandle);

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
	int32 SlotIndex = EventData.GetInt(TEXT("Slot"), -1);

	UE_LOG(LogTemp, Warning, TEXT("▶▶▶ OnItemEquippedEvent - Frame: %llu, Slot: %d"),
		GFrameCounter, SlotIndex);

	// Check if this is a weapon slot (slots 0/1 are primary/secondary weapons)
	bool bIsWeaponSlot = (SlotIndex == 0 || SlotIndex == 1);

	// Check if item has weapon tag in event data
	if (!bIsWeaponSlot)
	{
		FString SlotType = EventData.GetString(TEXT("SlotType"));
		bIsWeaponSlot = SlotType.Contains(TEXT("Weapon")) || SlotType.Contains(TEXT("Primary")) || SlotType.Contains(TEXT("Secondary"));
	}

	if (!bIsWeaponSlot)
	{
		UE_LOG(LogTemp, Log, TEXT("UIManager::OnItemEquippedEvent - Not a weapon slot, skipping"));
		return;
	}

	// Try to get weapon actor from event data
	// First try "Target" which may contain the weapon actor
	AActor* WeaponActor = EventData.GetObject<AActor>(FName("Target"));
	if (!WeaponActor)
	{
		WeaponActor = Cast<AActor>(EventData.Source.Get());
	}

	// If we have a weapon actor directly, use it
	if (WeaponActor)
	{
		UE_LOG(LogTemp, Log, TEXT("UIManager::OnItemEquippedEvent - Using weapon actor from event: %s"), *WeaponActor->GetName());
		InitializeWeaponHUD(WeaponActor);
		return;
	}

	// Fallback: try to get weapon from player's ActorFactory
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager::OnItemEquippedEvent - No world"));
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager::OnItemEquippedEvent - No player controller"));
		return;
	}

	APawn* PlayerPawn = PC->GetPawn();
	if (!PlayerPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager::OnItemEquippedEvent - No player pawn"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("UIManager::OnItemEquippedEvent - Player pawn: %s"), *PlayerPawn->GetName());

	USuspenseCoreEquipmentActorFactory* ActorFactory = PlayerPawn->FindComponentByClass<USuspenseCoreEquipmentActorFactory>();
	if (!ActorFactory)
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager::OnItemEquippedEvent - No ActorFactory on player pawn"));
		return;
	}

	TMap<int32, AActor*> SpawnedActors = ActorFactory->GetAllSpawnedActors();
	WeaponActor = SpawnedActors.FindRef(SlotIndex);

	if (WeaponActor)
	{
		UE_LOG(LogTemp, Log, TEXT("UIManager::OnItemEquippedEvent - Found weapon actor from factory: %s"), *WeaponActor->GetName());
		InitializeWeaponHUD(WeaponActor);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UIManager::OnItemEquippedEvent - No weapon actor found for slot %d"), SlotIndex);
	}
}

void USuspenseCoreUIManager::OnItemUnequippedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	int32 SlotIndex = EventData.GetInt(TEXT("Slot"), -1);
	FString SlotType = EventData.GetString(TEXT("SlotType"));

	UE_LOG(LogTemp, Warning, TEXT("◀◀◀ OnItemUnequippedEvent - Frame: %llu, Slot: %d, SlotType: %s"),
		GFrameCounter, SlotIndex, *SlotType);

	// Check if this was a weapon slot
	bool bIsWeaponSlot = (SlotIndex == 0 || SlotIndex == 1);

	// Also check by slot type string
	if (!bIsWeaponSlot)
	{
		bIsWeaponSlot = SlotType.Contains(TEXT("Weapon")) || SlotType.Contains(TEXT("Primary")) || SlotType.Contains(TEXT("Secondary"));
	}

	if (bIsWeaponSlot)
	{
		UE_LOG(LogTemp, Log, TEXT("UIManager::OnItemUnequippedEvent - Clearing weapon HUD"));
		ClearWeaponHUD();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("UIManager::OnItemUnequippedEvent - Not a weapon slot, ignoring"));
	}
}

void USuspenseCoreUIManager::OnVisualDetachedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	int32 SlotIndex = EventData.GetInt(TEXT("Slot"), -1);
	FString SlotType = EventData.GetString(TEXT("SlotType"));

	UE_LOG(LogTemp, Warning, TEXT("◀◀◀ OnVisualDetachedEvent - Frame: %llu, Slot: %d, SlotType: %s"),
		GFrameCounter, SlotIndex, *SlotType);

	// Check if this was a weapon slot
	bool bIsWeaponSlot = (SlotIndex == 0 || SlotIndex == 1);

	// Also check by slot type string
	if (!bIsWeaponSlot)
	{
		bIsWeaponSlot = SlotType.Contains(TEXT("Weapon")) || SlotType.Contains(TEXT("Primary")) || SlotType.Contains(TEXT("Secondary"));
	}

	if (bIsWeaponSlot)
	{
		UE_LOG(LogTemp, Log, TEXT("UIManager::OnVisualDetachedEvent - Clearing weapon HUD (visual detached)"));
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

USuspenseCoreMagazineTooltipWidget* USuspenseCoreUIManager::CreateMagazineTooltipWidget(APlayerController* PC)
{
	if (!PC)
	{
		return nullptr;
	}

	if (MagazineTooltipWidgetClass)
	{
		return CreateWidget<USuspenseCoreMagazineTooltipWidget>(PC, MagazineTooltipWidgetClass);
	}

	// Fallback to default class if not configured
	UE_LOG(LogTemp, Warning, TEXT("CreateMagazineTooltipWidget: MagazineTooltipWidgetClass not configured"));
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

//==================================================================
// Magazine Inspection (Tarkov-style)
//==================================================================

bool USuspenseCoreUIManager::OpenMagazineInspection(const FSuspenseCoreMagazineInspectionData& InspectionData)
{
	// Get player controller
	APlayerController* PC = OwningPC.Get();
	if (!PC)
	{
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
		UE_LOG(LogTemp, Warning, TEXT("OpenMagazineInspection: No player controller available"));
		return false;
	}

	// Create inspection widget if needed
	if (!MagazineInspectionWidget)
	{
		MagazineInspectionWidget = CreateMagazineInspectionWidget(PC);
		if (!MagazineInspectionWidget)
		{
			UE_LOG(LogTemp, Warning, TEXT("OpenMagazineInspection: Failed to create widget"));
			return false;
		}
	}

	// Ensure widget is in viewport
	if (!MagazineInspectionWidget->IsInViewport())
	{
		MagazineInspectionWidget->AddToViewport(5000); // High Z-order but below tooltips
	}

	// Open inspection using interface
	ISuspenseCoreMagazineInspectionWidgetInterface::Execute_OpenInspection(MagazineInspectionWidget, InspectionData);

	UE_LOG(LogTemp, Log, TEXT("OpenMagazineInspection: Opened for magazine %s"), *InspectionData.DisplayName.ToString());
	return true;
}

void USuspenseCoreUIManager::CloseMagazineInspection()
{
	if (MagazineInspectionWidget)
	{
		ISuspenseCoreMagazineInspectionWidgetInterface::Execute_CloseInspection(MagazineInspectionWidget);
	}
}

bool USuspenseCoreUIManager::IsMagazineInspectionOpen() const
{
	if (!MagazineInspectionWidget)
	{
		return false;
	}

	// Check if widget is in viewport and visible
	return MagazineInspectionWidget->IsInViewport() && MagazineInspectionWidget->IsVisible();
}

USuspenseCoreMagazineInspectionWidget* USuspenseCoreUIManager::CreateMagazineInspectionWidget(APlayerController* PC)
{
	if (!PC)
	{
		return nullptr;
	}

	if (MagazineInspectionWidgetClass)
	{
		return CreateWidget<USuspenseCoreMagazineInspectionWidget>(PC, MagazineInspectionWidgetClass);
	}

	UE_LOG(LogTemp, Warning, TEXT("CreateMagazineInspectionWidget: MagazineInspectionWidgetClass not configured"));
	return nullptr;
}

bool USuspenseCoreUIManager::IsMagazineItem(const FSuspenseCoreItemUIData& ItemData) const
{
	// Check if item has magazine type tag in ItemType
	// Looking for tags like: Item.Magazine, Item.Category.Magazine, Item.Weapon.Magazine, Item.Equipment.Magazine
	static const FGameplayTag MagazineTag = FGameplayTag::RequestGameplayTag(FName("Item.Magazine"), false);
	static const FGameplayTag MagazineCategoryTag = FGameplayTag::RequestGameplayTag(FName("Item.Category.Magazine"), false);
	static const FGameplayTag WeaponMagazineTag = FGameplayTag::RequestGameplayTag(FName("Item.Weapon.Magazine"), false);
	static const FGameplayTag EquipmentMagazineTag = FGameplayTag::RequestGameplayTag(FName("Item.Equipment.Magazine"), false);

	// Log for debugging
	UE_LOG(LogTemp, Verbose, TEXT("IsMagazineItem: Checking item %s with ItemType: %s"),
		*ItemData.ItemID.ToString(),
		*ItemData.ItemType.ToString());

	// Check ItemType - it contains the item's type tag (Item.Weapon.Rifle, Item.Magazine.AR, etc.)
	if (MagazineTag.IsValid() && ItemData.ItemType.MatchesTag(MagazineTag))
	{
		UE_LOG(LogTemp, Log, TEXT("IsMagazineItem: %s matched Item.Magazine tag"), *ItemData.ItemID.ToString());
		return true;
	}

	if (MagazineCategoryTag.IsValid() && ItemData.ItemType.MatchesTag(MagazineCategoryTag))
	{
		UE_LOG(LogTemp, Log, TEXT("IsMagazineItem: %s matched Item.Category.Magazine tag"), *ItemData.ItemID.ToString());
		return true;
	}

	if (WeaponMagazineTag.IsValid() && ItemData.ItemType.MatchesTag(WeaponMagazineTag))
	{
		UE_LOG(LogTemp, Log, TEXT("IsMagazineItem: %s matched Item.Weapon.Magazine tag"), *ItemData.ItemID.ToString());
		return true;
	}

	if (EquipmentMagazineTag.IsValid() && ItemData.ItemType.MatchesTag(EquipmentMagazineTag))
	{
		UE_LOG(LogTemp, Log, TEXT("IsMagazineItem: %s matched Item.Equipment.Magazine tag"), *ItemData.ItemID.ToString());
		return true;
	}

	// Also check if the tag string contains "Magazine" anywhere (fallback)
	FString TagString = ItemData.ItemType.ToString();
	if (TagString.Contains(TEXT("Magazine"), ESearchCase::IgnoreCase))
	{
		UE_LOG(LogTemp, Log, TEXT("IsMagazineItem: %s matched via string contains 'Magazine'"), *ItemData.ItemID.ToString());
		return true;
	}

	return false;
}
