// SuspenseCoreEquipmentWidget.cpp
// SuspenseCore - Equipment Container Widget Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/Equipment/SuspenseCoreEquipmentWidget.h"
#include "SuspenseCore/Widgets/Equipment/SuspenseCoreEquipmentSlotWidget.h"
#include "SuspenseCore/Widgets/DragDrop/SuspenseCoreDragDropOperation.h"
#include "InputCoreTypes.h"
#include "SuspenseCore/Actors/SuspenseCoreCharacterPreviewActor.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "SuspenseCore/Data/SuspenseCoreEquipmentSlotPresets.h"
#include "SuspenseCore/Settings/SuspenseCoreSettings.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
	/**
	 * Helper to get native tag for equipment slot type
	 * Use native tags instead of RequestGameplayTag per EventBus architecture documentation
	 */
	FGameplayTag GetNativeTagForSlotType(EEquipmentSlotType SlotType)
	{
		using namespace SuspenseCoreEquipmentTags::Slot;

		switch (SlotType)
		{
		case EEquipmentSlotType::PrimaryWeapon:    return TAG_Equipment_Slot_PrimaryWeapon;
		case EEquipmentSlotType::SecondaryWeapon:  return TAG_Equipment_Slot_SecondaryWeapon;
		case EEquipmentSlotType::Holster:          return TAG_Equipment_Slot_Holster;
		case EEquipmentSlotType::Scabbard:         return TAG_Equipment_Slot_Scabbard;
		case EEquipmentSlotType::Headwear:         return TAG_Equipment_Slot_Headwear;
		case EEquipmentSlotType::Earpiece:         return TAG_Equipment_Slot_Earpiece;
		case EEquipmentSlotType::Eyewear:          return TAG_Equipment_Slot_Eyewear;
		case EEquipmentSlotType::FaceCover:        return TAG_Equipment_Slot_FaceCover;
		case EEquipmentSlotType::BodyArmor:        return TAG_Equipment_Slot_BodyArmor;
		case EEquipmentSlotType::TacticalRig:      return TAG_Equipment_Slot_TacticalRig;
		case EEquipmentSlotType::Backpack:         return TAG_Equipment_Slot_Backpack;
		case EEquipmentSlotType::SecureContainer:  return TAG_Equipment_Slot_SecureContainer;
		case EEquipmentSlotType::QuickSlot1:       return TAG_Equipment_Slot_QuickSlot1;
		case EEquipmentSlotType::QuickSlot2:       return TAG_Equipment_Slot_QuickSlot2;
		case EEquipmentSlotType::QuickSlot3:       return TAG_Equipment_Slot_QuickSlot3;
		case EEquipmentSlotType::QuickSlot4:       return TAG_Equipment_Slot_QuickSlot4;
		case EEquipmentSlotType::Armband:          return TAG_Equipment_Slot_Armband;
		default:                                   return TAG_Equipment_Slot_None;
		}
	}
}

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

	// CRITICAL: Validate required BindWidget components
	checkf(SlotContainer, TEXT("%s: SlotContainer is REQUIRED! Add UCanvasPanel named 'SlotContainer' to your Blueprint."), *GetClass()->GetName());

	// Setup EventBus subscriptions (REQUIRED per documentation)
	SetupEventSubscriptions();

	// Auto-initialize from SSOT if enabled
	if (bAutoInitializeFromSSoT && SlotConfigs.Num() == 0)
	{
		AutoInitializeFromLoadoutManager();
	}

	// If slot configs were set (either manually or from SSOT), create widgets now
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

		// Create slot widget using UIPosition and UISize from Config
		USuspenseCoreEquipmentSlotWidget* SlotWidget = CreateSlotWidget(Config, nullptr);
		if (SlotWidget)
		{
			// Store references
			SlotWidgetsByType.Add(Config.SlotType, SlotWidget);
			SlotWidgetsByTag.Add(Config.SlotTag, SlotWidget);
			SlotWidgetsArray.Add(SlotWidget);

			// Position slot using UIPosition and UISize from FEquipmentSlotConfig
			PositionSlotWidgetFromConfig(SlotWidget, Config);

			UE_LOG(LogTemp, Log, TEXT("EquipmentWidget: Created slot %s at (%.0f, %.0f) size (%.0f, %.0f)"),
				*Config.DisplayName.ToString(),
				Config.UIPosition.X, Config.UIPosition.Y,
				Config.UISize.X, Config.UISize.Y);
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

void USuspenseCoreEquipmentWidget::PositionSlotWidgetFromConfig(USuspenseCoreEquipmentSlotWidget* SlotWidget, const FEquipmentSlotConfig& Config)
{
	if (!SlotWidget || !SlotContainer)
	{
		return;
	}

	// Get canvas slot
	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(SlotWidget->Slot);
	if (CanvasSlot)
	{
		// Set position directly from FEquipmentSlotConfig (SSOT)
		CanvasSlot->SetPosition(Config.UIPosition);

		// Set size - use UISize from config, fallback to DefaultSlotSize
		FVector2D SlotSize = Config.UISize;
		if (SlotSize.X <= 0 || SlotSize.Y <= 0)
		{
			SlotSize = DefaultSlotSize;
		}
		CanvasSlot->SetSize(SlotSize);

		// Update slot widget's internal size
		SlotWidget->SetSlotSize(SlotSize);

		// Set empty slot icon if provided
		if (!Config.EmptySlotIcon.IsNull())
		{
			SlotWidget->SetEmptySlotIconPath(Config.EmptySlotIcon.ToSoftObjectPath());
		}

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
	// Use native tags instead of RequestGameplayTag per EventBus architecture documentation
	FGameplayTag ExpectedTag = GetNativeTagForSlotType(SlotType);
	if (!ExpectedTag.IsValid())
	{
		return nullptr;
	}

	for (const FSuspenseCoreEquipmentSlotUIConfig& Config : SlotUIConfigs)
	{
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

	// Use FGameplayTag::RequestGameplayTag for cross-module compatibility
	// UI Request tags - for handling drag-drop equip/unequip requests
	static const FGameplayTag EquipItemTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UIRequest.EquipItem"));
	static const FGameplayTag UnequipItemTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UIRequest.UnequipItem"));

	// CRITICAL FIX: Subscribe to actual equipment change events published by DataStore
	// DataStore publishes Equipment.Event.SlotUpdated and Equipment.Event.Updated
	// NOT SuspenseCore.Event.UIProvider.DataChanged!
	static const FGameplayTag SlotUpdatedTag = FGameplayTag::RequestGameplayTag(FName("Equipment.Event.SlotUpdated"));
	static const FGameplayTag EquipmentUpdatedTag = FGameplayTag::RequestGameplayTag(FName("Equipment.Event.Updated"));

	// Subscribe to equipment item equipped events (UI Request)
	FSuspenseCoreSubscriptionHandle EquipHandle = EventBus->SubscribeNative(
		EquipItemTag,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(
			this, &USuspenseCoreEquipmentWidget::OnEquipmentItemEquipped),
		ESuspenseCoreEventPriority::Normal
	);
	SubscriptionHandles.Add(EquipHandle);

	// Subscribe to equipment item unequipped events (UI Request)
	FSuspenseCoreSubscriptionHandle UnequipHandle = EventBus->SubscribeNative(
		UnequipItemTag,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(
			this, &USuspenseCoreEquipmentWidget::OnEquipmentItemUnequipped),
		ESuspenseCoreEventPriority::Normal
	);
	SubscriptionHandles.Add(UnequipHandle);

	// Subscribe to slot updated events (actual data changes from DataStore)
	FSuspenseCoreSubscriptionHandle SlotUpdatedHandle = EventBus->SubscribeNative(
		SlotUpdatedTag,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(
			this, &USuspenseCoreEquipmentWidget::OnProviderDataChanged),
		ESuspenseCoreEventPriority::Normal
	);
	SubscriptionHandles.Add(SlotUpdatedHandle);

	// Subscribe to equipment updated events (general equipment state changes)
	FSuspenseCoreSubscriptionHandle EquipmentUpdatedHandle = EventBus->SubscribeNative(
		EquipmentUpdatedTag,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(
			this, &USuspenseCoreEquipmentWidget::OnProviderDataChanged),
		ESuspenseCoreEventPriority::Normal
	);
	SubscriptionHandles.Add(EquipmentUpdatedHandle);

	UE_LOG(LogTemp, Log, TEXT("EquipmentWidget: Setup %d EventBus subscriptions (SlotUpdated + EquipmentUpdated)"), SubscriptionHandles.Num());
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

void USuspenseCoreEquipmentWidget::OnEquipmentItemEquipped(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// Extract slot index from event data if available
	int32 TargetSlot = INDEX_NONE;
	if (const int32* SlotPtr = EventData.IntPayload.Find(FName("TargetSlot")))
	{
		TargetSlot = *SlotPtr;
	}

	// Refresh from provider to update all slots
	RefreshFromProvider();

	// Notify Blueprint if we have a valid slot
	if (TargetSlot != INDEX_NONE && SlotWidgetsArray.IsValidIndex(TargetSlot))
	{
		USuspenseCoreEquipmentSlotWidget* SlotWidget = SlotWidgetsArray[TargetSlot];
		if (SlotWidget)
		{
			FSuspenseCoreItemUIData ItemData;
			// Extract item info from event data if available
			if (const FString* ItemIDStr = EventData.StringPayload.Find(FName("ItemID")))
			{
				ItemData.ItemID = FName(**ItemIDStr);
			}
			K2_OnEquipRequested(SlotWidget->GetSlotType(), ItemData);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("EquipmentWidget: Item equipped event received (slot %d)"), TargetSlot);
}

void USuspenseCoreEquipmentWidget::OnEquipmentItemUnequipped(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// Extract slot index from event data if available
	int32 SourceSlot = INDEX_NONE;
	if (const int32* SlotPtr = EventData.IntPayload.Find(FName("SourceSlot")))
	{
		SourceSlot = *SlotPtr;
	}

	// Refresh from provider to update all slots
	RefreshFromProvider();

	// Notify Blueprint if we have a valid slot
	if (SourceSlot != INDEX_NONE && SlotWidgetsArray.IsValidIndex(SourceSlot))
	{
		USuspenseCoreEquipmentSlotWidget* SlotWidget = SlotWidgetsArray[SourceSlot];
		if (SlotWidget)
		{
			K2_OnUnequipRequested(SlotWidget->GetSlotType());
		}
	}

	UE_LOG(LogTemp, Log, TEXT("EquipmentWidget: Item unequipped event received (slot %d)"), SourceSlot);
}

void USuspenseCoreEquipmentWidget::OnProviderDataChanged(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// Refresh all slots from provider when data changes
	RefreshFromProvider();

	// Also refresh character preview if available
	RefreshCharacterPreview();

	UE_LOG(LogTemp, Log, TEXT("EquipmentWidget: Provider data changed, refreshed from provider"));
}

//==================================================================
// Character Preview
//==================================================================

ASuspenseCoreCharacterPreviewActor* USuspenseCoreEquipmentWidget::SpawnCharacterPreview(
	TSubclassOf<ASuspenseCoreCharacterPreviewActor> PreviewActorClass)
{
	if (!PreviewActorClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipmentWidget: SpawnCharacterPreview - No PreviewActorClass specified"));
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipmentWidget: SpawnCharacterPreview - No World context"));
		return nullptr;
	}

	// Spawn the preview actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ASuspenseCoreCharacterPreviewActor* SpawnedActor = World->SpawnActor<ASuspenseCoreCharacterPreviewActor>(
		PreviewActorClass,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);

	if (SpawnedActor)
	{
		CharacterPreview = SpawnedActor;
		RefreshCharacterPreview();
		UE_LOG(LogTemp, Log, TEXT("EquipmentWidget: Spawned character preview actor"));
	}

	return SpawnedActor;
}

void USuspenseCoreEquipmentWidget::RefreshCharacterPreview()
{
	if (!CharacterPreview.IsValid())
	{
		return;
	}

	// Get equipped items from provider and update preview
	if (!GetBoundProvider())
	{
		return;
	}

	// TODO: Implement actual character preview update
	// This should iterate through equipped items and update the preview actor's mesh/attachments
	UE_LOG(LogTemp, Verbose, TEXT("EquipmentWidget: RefreshCharacterPreview called"));
}

//==================================================================
// SSOT Auto-Initialization
//==================================================================

void USuspenseCoreEquipmentWidget::AutoInitializeFromLoadoutManager()
{
	// Get settings
	const USuspenseCoreSettings* Settings = GetDefault<USuspenseCoreSettings>();
	if (!Settings)
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipmentWidget: AutoInit - SuspenseCoreSettings not found"));
		return;
	}

	// Try to load from EquipmentSlotPresetsAsset (SSOT)
	if (!Settings->EquipmentSlotPresetsAsset.IsNull())
	{
		USuspenseCoreEquipmentSlotPresets* Presets = Cast<USuspenseCoreEquipmentSlotPresets>(
			Settings->EquipmentSlotPresetsAsset.LoadSynchronous());

		if (Presets && Presets->GetSlotCount() > 0)
		{
			SlotConfigs = Presets->GetAllPresets();
			UE_LOG(LogTemp, Log, TEXT("EquipmentWidget: AutoInit - Loaded %d slots from EquipmentSlotPresetsAsset"),
				SlotConfigs.Num());
			return;
		}
	}

	// Fallback: Use programmatic defaults with Native Tags
	SlotConfigs = USuspenseCoreEquipmentSlotPresets::CreateDefaultPresets();

	if (SlotConfigs.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("EquipmentWidget: AutoInit - Using %d default slots (no DataAsset configured)"),
			SlotConfigs.Num());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipmentWidget: AutoInit - No slot configs available!"));
	}
}

//==================================================================
// Drag-Drop Handling
//==================================================================

FReply USuspenseCoreEquipmentWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Get screen space position for accurate slot detection
	FVector2D ScreenSpacePos = InMouseEvent.GetScreenSpacePosition();
	USuspenseCoreEquipmentSlotWidget* SlotWidget = GetSlotWidgetAtScreenPosition(ScreenSpacePos);

	if (SlotWidget)
	{
		int32 SlotIndex = SlotWidgetsArray.IndexOfByKey(SlotWidget);
		bool bLeftClick = InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
		bool bRightClick = InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton;

		// Right-click could open context menu (handled elsewhere)
		if (bRightClick)
		{
			// TODO: Fire context menu event if needed
			return FReply::Handled();
		}

		// Left click on slot - check if item exists for potential drag
		if (bLeftClick)
		{
			// Check if slot has an item that can be dragged
			if (SlotWidget->Execute_CanBeDragged(SlotWidget))
			{
				// Store drag source info for NativeOnDragDetected
				DragSourceSlot = SlotIndex;
				DragStartMousePosition = ScreenSpacePos;

				UE_LOG(LogTemp, Log, TEXT("EquipmentWidget: MouseDown on slot %d (%s), starting drag detection"),
					SlotIndex, *SlotWidget->GetSlotTypeTag().ToString());

				// Return with DetectDrag to enable drag detection
				return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
			}
		}

		return FReply::Handled();
	}

	DragSourceSlot = INDEX_NONE;
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void USuspenseCoreEquipmentWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

	// Check if we have a valid drag source slot
	if (DragSourceSlot == INDEX_NONE)
	{
		UE_LOG(LogTemp, Verbose, TEXT("EquipmentWidget: NativeOnDragDetected - No drag source slot"));
		return;
	}

	// Validate slot index
	if (!SlotWidgetsArray.IsValidIndex(DragSourceSlot))
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipmentWidget: NativeOnDragDetected - Invalid slot index %d"), DragSourceSlot);
		DragSourceSlot = INDEX_NONE;
		return;
	}

	USuspenseCoreEquipmentSlotWidget* SlotWidget = SlotWidgetsArray[DragSourceSlot];
	if (!SlotWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipmentWidget: NativeOnDragDetected - Null slot widget at index %d"), DragSourceSlot);
		DragSourceSlot = INDEX_NONE;
		return;
	}

	// Get item data from slot widget's cached data
	const FSuspenseCoreItemUIData& CachedItemData = SlotWidget->GetItemData();
	if (!CachedItemData.InstanceID.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipmentWidget: NativeOnDragDetected - No item in slot %d"), DragSourceSlot);
		DragSourceSlot = INDEX_NONE;
		return;
	}

	// Create drag data
	FSuspenseCoreDragData DragData;
	DragData.Item = CachedItemData;
	DragData.SourceContainerType = ESuspenseCoreContainerType::Equipment;
	DragData.SourceContainerTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Container.Equipment"));
	DragData.SourceSlot = DragSourceSlot;
	DragData.bIsValid = true;

	// Calculate DragOffset: difference between slot's top-left and initial click position
	FGeometry SlotGeometry = SlotWidget->GetCachedGeometry();
	FVector2D SlotAbsolutePos = SlotGeometry.GetAbsolutePosition();
	FVector2D SlotLocalSize = SlotGeometry.GetLocalSize();

	// Check if geometry is valid
	const bool bGeometryValid = !SlotAbsolutePos.IsNearlyZero() || !SlotLocalSize.IsNearlyZero();
	if (bGeometryValid)
	{
		DragData.DragOffset = SlotAbsolutePos - DragStartMousePosition;

		UE_LOG(LogTemp, Log, TEXT("EquipmentWidget: DragOffset calculation: SlotPos=(%.1f, %.1f), ClickPos=(%.1f, %.1f), Offset=(%.1f, %.1f)"),
			SlotAbsolutePos.X, SlotAbsolutePos.Y,
			DragStartMousePosition.X, DragStartMousePosition.Y,
			DragData.DragOffset.X, DragData.DragOffset.Y);
	}
	else
	{
		// Geometry not cached yet - use zero offset
		DragData.DragOffset = FVector2D::ZeroVector;
		UE_LOG(LogTemp, Log, TEXT("EquipmentWidget: Slot geometry not cached, using zero DragOffset"));
	}

	// Get player controller for drag operation
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipmentWidget: NativeOnDragDetected - No owning player controller"));
		DragSourceSlot = INDEX_NONE;
		return;
	}

	// Create drag operation
	OutOperation = USuspenseCoreDragDropOperation::CreateDrag(PC, DragData);
	if (OutOperation)
	{
		// Notify slot that drag started
		SlotWidget->Execute_OnDragStarted(SlotWidget);

		UE_LOG(LogTemp, Log, TEXT("EquipmentWidget: NativeOnDragDetected - Started drag for item '%s' (InstanceID=%s) from slot %d"),
			*CachedItemData.DisplayName.ToString(),
			*CachedItemData.InstanceID.ToString(),
			DragSourceSlot);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipmentWidget: NativeOnDragDetected - Failed to create drag operation"));
	}

	// Clear drag source (operation will handle the rest)
	DragSourceSlot = INDEX_NONE;
}

void USuspenseCoreEquipmentWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);

	USuspenseCoreDragDropOperation* DragOp = Cast<USuspenseCoreDragDropOperation>(InOperation);
	if (!DragOp)
	{
		return;
	}

	UE_LOG(LogTemp, Verbose, TEXT("EquipmentWidget: DragEnter"));
}

void USuspenseCoreEquipmentWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	// Clear all highlights when leaving
	ClearSlotHighlights();
	HoveredSlotIndex = INDEX_NONE;

	Super::NativeOnDragLeave(InDragDropEvent, InOperation);

	UE_LOG(LogTemp, Verbose, TEXT("EquipmentWidget: DragLeave"));
}

bool USuspenseCoreEquipmentWidget::NativeOnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent, UDragDropOperation* Operation)
{
	USuspenseCoreDragDropOperation* DragOp = Cast<USuspenseCoreDragDropOperation>(Operation);
	if (!DragOp)
	{
		return false;
	}

	const FSuspenseCoreDragData& DragData = DragOp->GetDragData();

	// Find which slot is under the cursor using screen space position
	FVector2D ScreenSpacePos = DragDropEvent.GetScreenSpacePosition();
	USuspenseCoreEquipmentSlotWidget* SlotWidget = GetSlotWidgetAtScreenPosition(ScreenSpacePos);

	// Clear previous highlights
	if (HoveredSlotIndex != INDEX_NONE && SlotWidgetsArray.IsValidIndex(HoveredSlotIndex))
	{
		SlotWidgetsArray[HoveredSlotIndex]->SetHighlightState(ESuspenseCoreUISlotState::Empty);
	}

	if (SlotWidget)
	{
		int32 SlotIndex = SlotWidgetsArray.IndexOfByKey(SlotWidget);
		HoveredSlotIndex = SlotIndex;

		// Check if this slot can accept the item
		bool bCanAccept = SlotWidget->CanAcceptItemType(DragData.Item.ItemType);

		UE_LOG(LogTemp, Log, TEXT("EquipmentWidget: DragOver slot %d (%s), ItemType=%s, CanAccept=%d"),
			SlotIndex,
			*SlotWidget->GetSlotTypeTag().ToString(),
			*DragData.Item.ItemType.ToString(),
			bCanAccept ? 1 : 0);

		// Highlight the slot
		if (bCanAccept)
		{
			SlotWidget->SetHighlightState(ESuspenseCoreUISlotState::DropTargetValid);
		}
		else
		{
			SlotWidget->SetHighlightState(ESuspenseCoreUISlotState::DropTargetInvalid);
		}
	}
	else
	{
		HoveredSlotIndex = INDEX_NONE;
	}

	return true; // We handle the drag
}

bool USuspenseCoreEquipmentWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	// Clear highlights
	ClearSlotHighlights();

	USuspenseCoreDragDropOperation* DragOp = Cast<USuspenseCoreDragDropOperation>(InOperation);
	if (!DragOp)
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipmentWidget: NativeOnDrop - Invalid drag operation type"));
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}

	const FSuspenseCoreDragData& DragData = DragOp->GetDragData();

	// Find which slot is under the cursor using screen space position
	FVector2D ScreenSpacePos = InDragDropEvent.GetScreenSpacePosition();
	USuspenseCoreEquipmentSlotWidget* SlotWidget = GetSlotWidgetAtScreenPosition(ScreenSpacePos);

	if (!SlotWidget)
	{
		UE_LOG(LogTemp, Log, TEXT("EquipmentWidget: NativeOnDrop - No slot at position"));
		return false;
	}

	int32 SlotIndex = SlotWidgetsArray.IndexOfByKey(SlotWidget);

	// Validate drop using slot widget
	if (!SlotWidget->CanAcceptItemType(DragData.Item.ItemType))
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipmentWidget: NativeOnDrop - Slot %d cannot accept ItemType %s"),
			SlotIndex, *DragData.Item.ItemType.ToString());
		return false;
	}

	// Request equip through EventBus
	// Use RequestGameplayTag - tag is registered in DefaultGameplayTags.ini
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (EventBus)
	{
		FSuspenseCoreEventData EventData;
		EventData.Source = this;

		// Standard UI request payload format
		EventData.SetInt(FName("SourceSlot"), DragData.SourceSlot);
		EventData.SetInt(FName("TargetSlot"), SlotIndex);
		EventData.SetString(FName("ItemID"), DragData.Item.ItemID.ToString());
		EventData.SetString(FName("InstanceID"), DragData.Item.InstanceID.ToString());
		EventData.SetInt(FName("SourceContainerType"), static_cast<int32>(DragData.SourceContainerType));
		EventData.SetInt(FName("TargetContainerType"), static_cast<int32>(ESuspenseCoreContainerType::Equipment));

		// Tag is registered in DefaultGameplayTags.ini line 639
		static const FGameplayTag EquipItemTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UIRequest.EquipItem"));
		EventBus->Publish(EquipItemTag, EventData);

		UE_LOG(LogTemp, Log, TEXT("EquipmentWidget: NativeOnDrop - UIRequest.EquipItem sent for item %s (InstanceID=%s) to slot %d"),
			*DragData.Item.ItemID.ToString(),
			*DragData.Item.InstanceID.ToString(),
			SlotIndex);
	}

	HoveredSlotIndex = INDEX_NONE;
	return true;
}

USuspenseCoreEquipmentSlotWidget* USuspenseCoreEquipmentWidget::GetSlotWidgetAtScreenPosition(const FVector2D& ScreenSpacePos) const
{
	// Iterate through slot widgets and check which one contains the screen position
	// Use each slot's actual cached geometry for accurate hit-testing
	for (int32 Index = 0; Index < SlotWidgetsArray.Num(); ++Index)
	{
		USuspenseCoreEquipmentSlotWidget* SlotWidget = SlotWidgetsArray[Index];
		if (!SlotWidget)
		{
			continue;
		}

		// Get the slot's rendered geometry
		FGeometry SlotGeometry = SlotWidget->GetCachedGeometry();

		// Transform screen space position to slot's local space
		FVector2D SlotLocalPos = SlotGeometry.AbsoluteToLocal(ScreenSpacePos);
		FVector2D SlotSize = SlotGeometry.GetLocalSize();

		UE_LOG(LogTemp, Verbose, TEXT("GetSlotWidgetAtScreenPosition: Slot[%d] %s - LocalPos=(%.1f, %.1f), Size=(%.1f, %.1f)"),
			Index, *SlotWidget->GetSlotTypeTag().ToString(),
			SlotLocalPos.X, SlotLocalPos.Y, SlotSize.X, SlotSize.Y);

		// Check if position is within this slot's bounds (0,0 to SlotSize)
		if (SlotLocalPos.X >= 0.0f && SlotLocalPos.X <= SlotSize.X &&
			SlotLocalPos.Y >= 0.0f && SlotLocalPos.Y <= SlotSize.Y)
		{
			UE_LOG(LogTemp, Log, TEXT("GetSlotWidgetAtScreenPosition: HIT slot[%d] %s at local (%.1f, %.1f)"),
				Index, *SlotWidget->GetSlotTypeTag().ToString(), SlotLocalPos.X, SlotLocalPos.Y);
			return SlotWidget;
		}
	}

	UE_LOG(LogTemp, Verbose, TEXT("GetSlotWidgetAtScreenPosition: No slot found at screen position (%.1f, %.1f)"),
		ScreenSpacePos.X, ScreenSpacePos.Y);
	return nullptr;
}

// Keep old function for backward compatibility but implement using new method
USuspenseCoreEquipmentSlotWidget* USuspenseCoreEquipmentWidget::GetSlotWidgetAtPosition(const FVector2D& LocalPos) const
{
	// Convert local position back to screen space using our geometry
	FGeometry MyGeometry = GetCachedGeometry();
	FVector2D ScreenPos = MyGeometry.LocalToAbsolute(LocalPos);
	return GetSlotWidgetAtScreenPosition(ScreenPos);
}

void USuspenseCoreEquipmentWidget::ClearSlotHighlights()
{
	for (USuspenseCoreEquipmentSlotWidget* SlotWidget : SlotWidgetsArray)
	{
		if (SlotWidget)
		{
			SlotWidget->SetHighlightState(ESuspenseCoreUISlotState::Empty);
		}
	}
}
