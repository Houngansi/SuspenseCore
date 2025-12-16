// SuspenseCoreEquipmentWidget.cpp
// SuspenseCore - Equipment Container Widget Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/Equipment/SuspenseCoreEquipmentWidget.h"
#include "SuspenseCore/Widgets/Equipment/SuspenseCoreEquipmentSlotWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

//==================================================================
// Constructor
//==================================================================

USuspenseCoreEquipmentWidget::USuspenseCoreEquipmentWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, DefaultSlotSize(80.0f, 80.0f)
{
	// Set expected container type for equipment
	ExpectedContainerType = ESuspenseCoreContainerType::Equipment;
}

//==================================================================
// UUserWidget Interface
//==================================================================

void USuspenseCoreEquipmentWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Setup EventBus subscriptions (REQUIRED per documentation)
	SetupEventSubscriptions();

	// If slot configs were set before construction, create widgets now
	if (SlotConfigs.Num() > 0 && SlotWidgetsByType.Num() == 0)
	{
		CreateSlotWidgets();
	}
}

void USuspenseCoreEquipmentWidget::NativeDestruct()
{
	// Teardown EventBus subscriptions (REQUIRED per documentation)
	TeardownEventSubscriptions();

	// Clear slot widgets
	ClearSlotWidgets();

	// Clear cached references
	CachedEventBus.Reset();

	Super::NativeDestruct();
}

//==================================================================
// ISuspenseCoreUIContainer Overrides
//==================================================================

UWidget* USuspenseCoreEquipmentWidget::GetSlotWidget(int32 SlotIndex) const
{
	if (SlotWidgetsArray.IsValidIndex(SlotIndex))
	{
		return SlotWidgetsArray[SlotIndex];
	}
	return nullptr;
}

TArray<UWidget*> USuspenseCoreEquipmentWidget::GetAllSlotWidgets() const
{
	TArray<UWidget*> Result;
	Result.Reserve(SlotWidgetsArray.Num());
	for (const auto& SlotWidget : SlotWidgetsArray)
	{
		if (SlotWidget)
		{
			Result.Add(SlotWidget);
		}
	}
	return Result;
}

//==================================================================
// Equipment-Specific API
//==================================================================

void USuspenseCoreEquipmentWidget::InitializeFromSlotConfigs(const TArray<FEquipmentSlotConfig>& InSlotConfigs)
{
	SlotConfigs = InSlotConfigs;

	// Clear existing slots
	ClearSlotWidgets();

	// Create new slots
	CreateSlotWidgets();
}

void USuspenseCoreEquipmentWidget::InitializeWithUIConfig(
	const TArray<FEquipmentSlotConfig>& InSlotConfigs,
	const TArray<FSuspenseCoreEquipmentSlotUIConfig>& InUIConfigs)
{
	SlotConfigs = InSlotConfigs;
	SlotUIConfigs = InUIConfigs;

	// Clear existing slots
	ClearSlotWidgets();

	// Create new slots with UI config
	CreateSlotWidgets();
}

USuspenseCoreEquipmentSlotWidget* USuspenseCoreEquipmentWidget::GetSlotByType(EEquipmentSlotType SlotType) const
{
	if (const TObjectPtr<USuspenseCoreEquipmentSlotWidget>* FoundSlot = SlotWidgetsByType.Find(SlotType))
	{
		return *FoundSlot;
	}
	return nullptr;
}

USuspenseCoreEquipmentSlotWidget* USuspenseCoreEquipmentWidget::GetSlotByTag(const FGameplayTag& SlotTag) const
{
	if (const TWeakObjectPtr<USuspenseCoreEquipmentSlotWidget>* FoundSlot = SlotWidgetsByTag.Find(SlotTag))
	{
		return FoundSlot->Get();
	}
	return nullptr;
}

void USuspenseCoreEquipmentWidget::UpdateSlotByType(EEquipmentSlotType SlotType, const FSuspenseCoreSlotUIData& SlotData, const FSuspenseCoreItemUIData& ItemData)
{
	if (USuspenseCoreEquipmentSlotWidget* SlotWidget = GetSlotByType(SlotType))
	{
		SlotWidget->UpdateSlotData(SlotData, ItemData);
	}
}

void USuspenseCoreEquipmentWidget::UpdateSlotByTag(const FGameplayTag& SlotTag, const FSuspenseCoreSlotUIData& SlotData, const FSuspenseCoreItemUIData& ItemData)
{
	if (USuspenseCoreEquipmentSlotWidget* SlotWidget = GetSlotByTag(SlotTag))
	{
		SlotWidget->UpdateSlotData(SlotData, ItemData);
	}
}

TArray<EEquipmentSlotType> USuspenseCoreEquipmentWidget::GetAllSlotTypes() const
{
	TArray<EEquipmentSlotType> SlotTypes;
	SlotWidgetsByType.GetKeys(SlotTypes);
	return SlotTypes;
}

bool USuspenseCoreEquipmentWidget::HasSlot(EEquipmentSlotType SlotType) const
{
	return SlotWidgetsByType.Contains(SlotType);
}

//==================================================================
// Override Points from Base
//==================================================================

void USuspenseCoreEquipmentWidget::CreateSlotWidgets_Implementation()
{
	if (SlotConfigs.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipmentWidget: No slot configs provided"));
		return;
	}

	if (!SlotContainer)
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipmentWidget: SlotContainer not bound - create CanvasPanel named 'SlotContainer' in Blueprint"));
		return;
	}

	// Get or use default widget class
	TSubclassOf<USuspenseCoreEquipmentSlotWidget> WidgetClassToUse = SlotWidgetClass;
	if (!WidgetClassToUse)
	{
		WidgetClassToUse = USuspenseCoreEquipmentSlotWidget::StaticClass();
	}

	// Create slot widgets
	for (int32 Index = 0; Index < SlotConfigs.Num(); ++Index)
	{
		const FEquipmentSlotConfig& Config = SlotConfigs[Index];

		if (Config.SlotType == EEquipmentSlotType::None)
		{
			continue;
		}

		// Find UI config for this slot
		const FSuspenseCoreEquipmentSlotUIConfig* UIConfig = FindUIConfigForSlot(Config.SlotType);

		// Create slot widget
		USuspenseCoreEquipmentSlotWidget* SlotWidget = CreateSlotWidget(Config, UIConfig);
		if (SlotWidget)
		{
			// Store references
			SlotWidgetsByType.Add(Config.SlotType, SlotWidget);
			SlotWidgetsByTag.Add(Config.SlotTag, SlotWidget);
			SlotWidgetsArray.Add(SlotWidget);

			// Position if UI config available
			if (UIConfig)
			{
				PositionSlotWidget(SlotWidget, *UIConfig);
			}

			UE_LOG(LogTemp, Log, TEXT("EquipmentWidget: Created slot %s (Index: %d)"),
				*Config.DisplayName.ToString(), Index);
		}
	}

	// Update container data
	CachedContainerData.TotalSlots = SlotWidgetsArray.Num();
	CachedContainerData.LayoutType = ESuspenseCoreSlotLayoutType::Named;

	// Notify Blueprint
	K2_OnSlotsInitialized(SlotWidgetsArray.Num());

	UE_LOG(LogTemp, Log, TEXT("EquipmentWidget: Initialized %d equipment slots"), SlotWidgetsArray.Num());
}

void USuspenseCoreEquipmentWidget::UpdateSlotWidget_Implementation(int32 SlotIndex, const FSuspenseCoreSlotUIData& SlotData, const FSuspenseCoreItemUIData& ItemData)
{
	if (SlotWidgetsArray.IsValidIndex(SlotIndex))
	{
		SlotWidgetsArray[SlotIndex]->UpdateSlotData(SlotData, ItemData);
	}
}

void USuspenseCoreEquipmentWidget::ClearSlotWidgets_Implementation()
{
	// Remove all slot widgets
	for (auto& SlotWidget : SlotWidgetsArray)
	{
		if (SlotWidget)
		{
			SlotWidget->RemoveFromParent();
		}
	}

	SlotWidgetsByType.Empty();
	SlotWidgetsByTag.Empty();
	SlotWidgetsArray.Empty();
}

//==================================================================
// Slot Creation
//==================================================================

USuspenseCoreEquipmentSlotWidget* USuspenseCoreEquipmentWidget::CreateSlotWidget(
	const FEquipmentSlotConfig& SlotConfig,
	const FSuspenseCoreEquipmentSlotUIConfig* UIConfig)
{
	if (!SlotContainer)
	{
		return nullptr;
	}

	// Determine widget class
	TSubclassOf<USuspenseCoreEquipmentSlotWidget> WidgetClassToUse = SlotWidgetClass;
	if (!WidgetClassToUse)
	{
		WidgetClassToUse = USuspenseCoreEquipmentSlotWidget::StaticClass();
	}

	// Create widget
	USuspenseCoreEquipmentSlotWidget* SlotWidget = CreateWidget<USuspenseCoreEquipmentSlotWidget>(this, WidgetClassToUse);
	if (!SlotWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("EquipmentWidget: Failed to create slot widget for %s"), *SlotConfig.DisplayName.ToString());
		return nullptr;
	}

	// Initialize from config
	SlotWidget->InitializeFromConfig(SlotConfig);

	// Set slot size
	FVector2D SlotSize = DefaultSlotSize;
	if (UIConfig && UIConfig->SlotSize.X > 0 && UIConfig->SlotSize.Y > 0)
	{
		SlotSize.X = UIConfig->SlotSize.X * DefaultSlotSize.X;
		SlotSize.Y = UIConfig->SlotSize.Y * DefaultSlotSize.Y;
	}
	SlotWidget->SetSlotSize(SlotSize);

	// Set empty slot icon if provided in UI config
	if (UIConfig && UIConfig->EmptySlotIcon.IsValid())
	{
		SlotWidget->SetEmptySlotIconPath(UIConfig->EmptySlotIcon);
	}

	// Add to container
	SlotContainer->AddChild(SlotWidget);

	return SlotWidget;
}

void USuspenseCoreEquipmentWidget::PositionSlotWidget(USuspenseCoreEquipmentSlotWidget* SlotWidget, const FSuspenseCoreEquipmentSlotUIConfig& UIConfig)
{
	if (!SlotWidget || !SlotContainer)
	{
		return;
	}

	// Get canvas slot
	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(SlotWidget->Slot);
	if (CanvasSlot)
	{
		// Set position from UI config
		CanvasSlot->SetPosition(UIConfig.LayoutPosition);

		// Set size
		FVector2D SlotSize = DefaultSlotSize;
		if (UIConfig.SlotSize.X > 0 && UIConfig.SlotSize.Y > 0)
		{
			SlotSize.X = UIConfig.SlotSize.X * DefaultSlotSize.X;
			SlotSize.Y = UIConfig.SlotSize.Y * DefaultSlotSize.Y;
		}
		CanvasSlot->SetSize(SlotSize);

		// Anchor to top-left for absolute positioning
		CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
		CanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));
	}
}

//==================================================================
// Private Helpers
//==================================================================

const FSuspenseCoreEquipmentSlotUIConfig* USuspenseCoreEquipmentWidget::FindUIConfigForSlot(EEquipmentSlotType SlotType) const
{
	for (const FSuspenseCoreEquipmentSlotUIConfig& Config : SlotUIConfigs)
	{
		// Match by slot type tag
		FString ExpectedTagName = FString::Printf(TEXT("Equipment.Slot.%s"),
			*UEnum::GetValueAsString(SlotType).RightChop(FString(TEXT("EEquipmentSlotType::")).Len()));
		FGameplayTag ExpectedTag = FGameplayTag::RequestGameplayTag(*ExpectedTagName, false);

		if (Config.SlotTypeTag == ExpectedTag)
		{
			return &Config;
		}
	}
	return nullptr;
}

int32 USuspenseCoreEquipmentWidget::GetSlotIndexForType(EEquipmentSlotType SlotType) const
{
	for (int32 Index = 0; Index < SlotWidgetsArray.Num(); ++Index)
	{
		if (SlotWidgetsArray[Index] && SlotWidgetsArray[Index]->GetSlotType() == SlotType)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

//==================================================================
// EventBus Integration (REQUIRED pattern per documentation)
//==================================================================

void USuspenseCoreEquipmentWidget::SetupEventSubscriptions()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipmentWidget: EventBus not available for subscriptions"));
		return;
	}

	// Subscribe to equipment item equipped events (using Native Tags)
	FSuspenseCoreSubscriptionHandle EquipHandle = EventBus->SubscribeNative(
		TAG_SuspenseCore_Event_UIRequest_EquipItem,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(
			this, &USuspenseCoreEquipmentWidget::OnEquipmentItemEquipped),
		ESuspenseCoreEventPriority::Normal
	);
	SubscriptionHandles.Add(EquipHandle);

	// Subscribe to equipment item unequipped events
	FSuspenseCoreSubscriptionHandle UnequipHandle = EventBus->SubscribeNative(
		TAG_SuspenseCore_Event_UIRequest_UnequipItem,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(
			this, &USuspenseCoreEquipmentWidget::OnEquipmentItemUnequipped),
		ESuspenseCoreEventPriority::Normal
	);
	SubscriptionHandles.Add(UnequipHandle);

	// Subscribe to provider data changed events
	FSuspenseCoreSubscriptionHandle DataChangedHandle = EventBus->SubscribeNative(
		TAG_SuspenseCore_Event_UIProvider_DataChanged,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(
			this, &USuspenseCoreEquipmentWidget::OnProviderDataChanged),
		ESuspenseCoreEventPriority::Normal
	);
	SubscriptionHandles.Add(DataChangedHandle);

	UE_LOG(LogTemp, Log, TEXT("EquipmentWidget: Setup %d EventBus subscriptions"), SubscriptionHandles.Num());
}

void USuspenseCoreEquipmentWidget::TeardownEventSubscriptions()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (EventBus)
	{
		for (const FSuspenseCoreSubscriptionHandle& Handle : SubscriptionHandles)
		{
			EventBus->Unsubscribe(Handle);
		}
	}

	SubscriptionHandles.Empty();
	UE_LOG(LogTemp, Log, TEXT("EquipmentWidget: Teardown EventBus subscriptions"));
}

USuspenseCoreEventBus* USuspenseCoreEquipmentWidget::GetEventBus() const
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		return nullptr;
	}

	if (USuspenseCoreEventManager* EventManager = GI->GetSubsystem<USuspenseCoreEventManager>())
	{
		CachedEventBus = EventManager->GetEventBus();
		return CachedEventBus.Get();
	}

	return nullptr;
}

void USuspenseCoreEquipmentWidget::OnEquipmentItemEquipped(const FSuspenseCoreUIEventPayload& Payload)
{
	// Check if this event is for our container
	if (Payload.TargetContainerType != ESuspenseCoreContainerType::Equipment)
	{
		return;
	}

	// Find the slot by index and refresh
	if (Payload.TargetSlot != INDEX_NONE && SlotWidgetsArray.IsValidIndex(Payload.TargetSlot))
	{
		RefreshFromProvider();

		// Notify Blueprint
		if (SlotWidgetsArray.IsValidIndex(Payload.TargetSlot))
		{
			USuspenseCoreEquipmentSlotWidget* SlotWidget = SlotWidgetsArray[Payload.TargetSlot];
			if (SlotWidget)
			{
				FSuspenseCoreItemUIData ItemData;
				ItemData.InstanceID = Payload.ItemInstanceID;
				ItemData.ItemID = Payload.ItemID;
				K2_OnEquipRequested(SlotWidget->GetSlotType(), ItemData);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("EquipmentWidget: Item equipped to slot %d"), Payload.TargetSlot);
}

void USuspenseCoreEquipmentWidget::OnEquipmentItemUnequipped(const FSuspenseCoreUIEventPayload& Payload)
{
	// Check if this event is for our container
	if (Payload.SourceContainerType != ESuspenseCoreContainerType::Equipment)
	{
		return;
	}

	// Find the slot by index and refresh
	if (Payload.SourceSlot != INDEX_NONE && SlotWidgetsArray.IsValidIndex(Payload.SourceSlot))
	{
		RefreshFromProvider();

		// Notify Blueprint
		USuspenseCoreEquipmentSlotWidget* SlotWidget = SlotWidgetsArray[Payload.SourceSlot];
		if (SlotWidget)
		{
			K2_OnUnequipRequested(SlotWidget->GetSlotType());
		}
	}

	UE_LOG(LogTemp, Log, TEXT("EquipmentWidget: Item unequipped from slot %d"), Payload.SourceSlot);
}

void USuspenseCoreEquipmentWidget::OnProviderDataChanged(const FSuspenseCoreUIEventPayload& Payload)
{
	// Check if this event is for our container type
	if (Payload.TargetContainerType != ESuspenseCoreContainerType::Equipment &&
		Payload.SourceContainerType != ESuspenseCoreContainerType::Equipment)
	{
		return;
	}

	// Refresh all slots from provider
	RefreshFromProvider();

	UE_LOG(LogTemp, Log, TEXT("EquipmentWidget: Provider data changed, refreshed from provider"));
}
