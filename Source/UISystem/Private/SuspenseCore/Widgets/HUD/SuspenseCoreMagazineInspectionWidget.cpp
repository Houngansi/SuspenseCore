// SuspenseCoreMagazineInspectionWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/HUD/SuspenseCoreMagazineInspectionWidget.h"
#include "SuspenseCore/Widgets/DragDrop/SuspenseCoreDragDropOperation.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUIContainerTypes.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/WrapBox.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"

USuspenseCoreMagazineInspectionWidget::USuspenseCoreMagazineInspectionWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ═══════════════════════════════════════════════════════════════════════════════
// UUserWidget Interface
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreMagazineInspectionWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Validate required BindWidget components
	checkf(CloseButton, TEXT("USuspenseCoreMagazineInspectionWidget: CloseButton is REQUIRED!"));
	checkf(MagazineNameText, TEXT("USuspenseCoreMagazineInspectionWidget: MagazineNameText is REQUIRED!"));
	checkf(RoundSlotsContainer, TEXT("USuspenseCoreMagazineInspectionWidget: RoundSlotsContainer is REQUIRED!"));
	checkf(RoundsCountText, TEXT("USuspenseCoreMagazineInspectionWidget: RoundsCountText is REQUIRED!"));
	checkf(FillProgressBar, TEXT("USuspenseCoreMagazineInspectionWidget: FillProgressBar is REQUIRED!"));
	checkf(DropZoneBorder, TEXT("USuspenseCoreMagazineInspectionWidget: DropZoneBorder is REQUIRED!"));
	checkf(LoadingProgressBar, TEXT("USuspenseCoreMagazineInspectionWidget: LoadingProgressBar is REQUIRED!"));
	checkf(LoadingStatusText, TEXT("USuspenseCoreMagazineInspectionWidget: LoadingStatusText is REQUIRED!"));

	// Bind close button
	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &USuspenseCoreMagazineInspectionWidget::OnCloseButtonClicked);
	}

	// Start hidden
	SetVisibility(ESlateVisibility::Collapsed);

	// Hide loading indicator initially
	if (LoadingProgressBar)
	{
		LoadingProgressBar->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (LoadingStatusText)
	{
		LoadingStatusText->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Subscribe to events
	SetupEventSubscriptions();
}

void USuspenseCoreMagazineInspectionWidget::NativeDestruct()
{
	TeardownEventSubscriptions();
	ClearRoundSlots();

	Super::NativeDestruct();
}

void USuspenseCoreMagazineInspectionWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Update loading progress if active
	if (bIsLoadingInProgress && LoadingTotalTime > 0.0f)
	{
		LoadingProgress += InDeltaTime / LoadingTotalTime;
		LoadingProgress = FMath::Clamp(LoadingProgress, 0.0f, 1.0f);

		if (LoadingProgressBar)
		{
			LoadingProgressBar->SetPercent(LoadingProgress);
		}
	}
}

FReply USuspenseCoreMagazineInspectionWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Consume mouse clicks to prevent clicking through
	return FReply::Handled();
}

bool USuspenseCoreMagazineInspectionWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	// Clear drop highlight
	SetDropHighlight_Implementation(false, false);

	// Get our custom drag operation
	USuspenseCoreDragDropOperation* DragOp = Cast<USuspenseCoreDragDropOperation>(InOperation);
	if (!DragOp)
	{
		return false;
	}

	const FSuspenseCoreDragData& DragData = DragOp->GetDragData();

	// Check if it's compatible ammo
	if (!IsCompatibleAmmo(DragData.Item))
	{
		OnAmmoDropResult(ESuspenseCoreMagazineDropResult::IncompatibleCaliber);
		return false;
	}

	// Check if magazine is full
	if (CachedInspectionData.IsFull())
	{
		OnAmmoDropResult(ESuspenseCoreMagazineDropResult::MagazineFull);
		return false;
	}

	// Check if already loading
	if (bIsLoadingInProgress)
	{
		OnAmmoDropResult(ESuspenseCoreMagazineDropResult::Busy);
		return false;
	}

	// Request ammo loading through EventBus
	int32 AmmoQuantity = DragData.Item.Quantity > 0 ? DragData.Item.Quantity : 1;
	RequestAmmoLoad(DragData.Item.ItemID, AmmoQuantity);

	OnAmmoDropResult(ESuspenseCoreMagazineDropResult::Loaded);
	return true;
}

bool USuspenseCoreMagazineInspectionWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	// Get our custom drag operation
	USuspenseCoreDragDropOperation* DragOp = Cast<USuspenseCoreDragDropOperation>(InOperation);
	if (!DragOp)
	{
		return false;
	}

	const FSuspenseCoreDragData& DragData = DragOp->GetDragData();

	// Check if it's compatible ammo
	bool bIsCompatible = IsCompatibleAmmo(DragData.Item) && !CachedInspectionData.IsFull() && !bIsLoadingInProgress;

	// Update drop highlight
	SetDropHighlight_Implementation(true, bIsCompatible);

	// Update drag visual validity
	DragOp->UpdateDropValidity(bIsCompatible);

	return true;
}

void USuspenseCoreMagazineInspectionWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	// Clear drop highlight when drag leaves
	SetDropHighlight_Implementation(false, false);
}

// ═══════════════════════════════════════════════════════════════════════════════
// ISuspenseCoreMagazineInspectionWidget Implementation
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreMagazineInspectionWidget::OpenInspection_Implementation(const FSuspenseCoreMagazineInspectionData& InspectionData)
{
	CachedInspectionData = InspectionData;
	bIsVisible = true;

	// Update header
	if (MagazineNameText)
	{
		MagazineNameText->SetText(InspectionData.DisplayName);
	}

	if (CaliberText)
	{
		CaliberText->SetText(InspectionData.CaliberDisplayName);
	}

	if (MagazineIcon && InspectionData.Icon)
	{
		MagazineIcon->SetBrushFromTexture(InspectionData.Icon);
		MagazineIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	// Build round slots
	RebuildRoundSlots();

	// Update footer
	UpdateFooterUI();

	// Show widget
	SetVisibility(ESlateVisibility::Visible);

	// Notify Blueprint
	OnInspectionOpened();
}

void USuspenseCoreMagazineInspectionWidget::CloseInspection_Implementation()
{
	bIsVisible = false;
	bIsLoadingInProgress = false;

	SetVisibility(ESlateVisibility::Collapsed);

	// Notify Blueprint
	OnInspectionClosed();
}

void USuspenseCoreMagazineInspectionWidget::UpdateInspection_Implementation(const FSuspenseCoreMagazineInspectionData& InspectionData)
{
	CachedInspectionData = InspectionData;

	// Update each round slot
	for (int32 i = 0; i < InspectionData.RoundSlots.Num(); ++i)
	{
		UpdateRoundSlot(i, InspectionData.RoundSlots[i]);
	}

	// Update footer
	UpdateFooterUI();
}

void USuspenseCoreMagazineInspectionWidget::StartLoadingSlot_Implementation(int32 SlotIndex, float LoadTime)
{
	bIsLoadingInProgress = true;
	bIsUnloading = false;
	LoadingSlotIndex = SlotIndex;
	LoadingProgress = 0.0f;
	LoadingTotalTime = LoadTime;

	// Show loading UI
	UpdateLoadingUI();

	// Notify Blueprint
	OnRoundLoadingStarted(SlotIndex);
}

void USuspenseCoreMagazineInspectionWidget::CompleteLoadingSlot_Implementation(int32 SlotIndex, const FSuspenseCoreRoundSlotData& RoundData)
{
	bIsLoadingInProgress = false;
	LoadingSlotIndex = -1;

	// Update slot visual
	UpdateRoundSlot(SlotIndex, RoundData);

	// Update cached data
	if (SlotIndex >= 0 && SlotIndex < CachedInspectionData.RoundSlots.Num())
	{
		CachedInspectionData.RoundSlots[SlotIndex] = RoundData;
		CachedInspectionData.CurrentRounds++;
	}

	// Hide loading UI
	UpdateLoadingUI();
	UpdateFooterUI();

	// Notify Blueprint
	OnRoundLoadingCompleted(SlotIndex);
}

void USuspenseCoreMagazineInspectionWidget::StartUnloadingSlot_Implementation(int32 SlotIndex, float UnloadTime)
{
	bIsLoadingInProgress = true;
	bIsUnloading = true;
	LoadingSlotIndex = SlotIndex;
	LoadingProgress = 0.0f;
	LoadingTotalTime = UnloadTime;

	// Show loading UI
	UpdateLoadingUI();
}

void USuspenseCoreMagazineInspectionWidget::CompleteUnloadingSlot_Implementation(int32 SlotIndex)
{
	bIsLoadingInProgress = false;
	LoadingSlotIndex = -1;

	// Update slot to empty
	if (SlotIndex >= 0 && SlotIndex < CachedInspectionData.RoundSlots.Num())
	{
		FSuspenseCoreRoundSlotData EmptySlot;
		EmptySlot.SlotIndex = SlotIndex;
		EmptySlot.bIsOccupied = false;

		CachedInspectionData.RoundSlots[SlotIndex] = EmptySlot;
		CachedInspectionData.CurrentRounds = FMath::Max(0, CachedInspectionData.CurrentRounds - 1);

		UpdateRoundSlot(SlotIndex, EmptySlot);
	}

	// Hide loading UI
	UpdateLoadingUI();
	UpdateFooterUI();
}

void USuspenseCoreMagazineInspectionWidget::CancelLoadingOperation_Implementation()
{
	bIsLoadingInProgress = false;
	LoadingSlotIndex = -1;
	LoadingProgress = 0.0f;

	// Hide loading UI
	UpdateLoadingUI();
}

ESuspenseCoreMagazineDropResult USuspenseCoreMagazineInspectionWidget::OnAmmoDropped_Implementation(FName AmmoID, int32 Quantity)
{
	// This will be handled by AmmoLoadingService through EventBus
	// For now, just validate basic compatibility

	if (CachedInspectionData.IsFull())
	{
		OnAmmoDropResult(ESuspenseCoreMagazineDropResult::MagazineFull);
		return ESuspenseCoreMagazineDropResult::MagazineFull;
	}

	if (bIsLoadingInProgress)
	{
		OnAmmoDropResult(ESuspenseCoreMagazineDropResult::Busy);
		return ESuspenseCoreMagazineDropResult::Busy;
	}

	// TODO: Publish event to AmmoLoadingService
	// For now return Loaded to indicate acceptance
	OnAmmoDropResult(ESuspenseCoreMagazineDropResult::Loaded);
	return ESuspenseCoreMagazineDropResult::Loaded;
}

void USuspenseCoreMagazineInspectionWidget::SetDropHighlight_Implementation(bool bHighlight, bool bIsCompatible)
{
	if (!DropZoneBorder)
	{
		return;
	}

	// Drop zone highlighting is handled in Blueprint via material parameters
	// Here we just set the visibility/state

	if (bHighlight)
	{
		DropZoneBorder->SetVisibility(ESlateVisibility::Visible);
		// Blueprint should bind to a HighlightState variable to change color
	}
	else
	{
		DropZoneBorder->SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool USuspenseCoreMagazineInspectionWidget::IsInspectionVisible_Implementation() const
{
	return bIsVisible;
}

FSuspenseCoreMagazineInspectionData USuspenseCoreMagazineInspectionWidget::GetCurrentInspectionData_Implementation() const
{
	return CachedInspectionData;
}

FGuid USuspenseCoreMagazineInspectionWidget::GetInspectedMagazineID_Implementation() const
{
	return CachedInspectionData.MagazineInstanceID;
}

// ═══════════════════════════════════════════════════════════════════════════════
// EVENTBUS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreMagazineInspectionWidget::SetupEventSubscriptions()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	using namespace SuspenseCoreEquipmentTags::Magazine;

	// Subscribe to ammo loading events (Tarkov-style)
	LoadingStartedHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_Ammo_LoadStarted,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreMagazineInspectionWidget::OnAmmoLoadingStartedEvent),
		ESuspenseCoreEventPriority::Normal
	);

	LoadingProgressHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_Ammo_RoundLoaded,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreMagazineInspectionWidget::OnAmmoLoadingProgressEvent),
		ESuspenseCoreEventPriority::Normal
	);

	LoadingCompletedHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_Ammo_LoadCompleted,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreMagazineInspectionWidget::OnAmmoLoadingCompletedEvent),
		ESuspenseCoreEventPriority::Normal
	);

	LoadingCancelledHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_Ammo_LoadCancelled,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreMagazineInspectionWidget::OnAmmoLoadingCancelledEvent),
		ESuspenseCoreEventPriority::Normal
	);
}

void USuspenseCoreMagazineInspectionWidget::TeardownEventSubscriptions()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	EventBus->Unsubscribe(LoadingStartedHandle);
	EventBus->Unsubscribe(LoadingProgressHandle);
	EventBus->Unsubscribe(LoadingCompletedHandle);
	EventBus->Unsubscribe(LoadingCancelledHandle);
}

USuspenseCoreEventBus* USuspenseCoreMagazineInspectionWidget::GetEventBus() const
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

	USuspenseCoreEventManager* EventManager = GI->GetSubsystem<USuspenseCoreEventManager>();
	if (!EventManager)
	{
		return nullptr;
	}

	const_cast<USuspenseCoreMagazineInspectionWidget*>(this)->CachedEventBus = EventManager->GetEventBus();
	return CachedEventBus.Get();
}

void USuspenseCoreMagazineInspectionWidget::OnAmmoLoadingStartedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// Check if this event is for our magazine
	FGuid MagazineID;
	FGuid::Parse(EventData.GetString(TEXT("MagazineInstanceID")), MagazineID);
	if (MagazineID != CachedInspectionData.MagazineInstanceID)
	{
		return;
	}

	int32 SlotIndex = EventData.GetInt(TEXT("SlotIndex"), 0);
	float LoadTime = EventData.GetFloat(TEXT("LoadTime"), 1.0f);

	StartLoadingSlot_Implementation(SlotIndex, LoadTime);
}

void USuspenseCoreMagazineInspectionWidget::OnAmmoLoadingProgressEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// Check if this event is for our magazine
	FGuid MagazineID;
	FGuid::Parse(EventData.GetString(TEXT("MagazineInstanceID")), MagazineID);
	if (MagazineID != CachedInspectionData.MagazineInstanceID)
	{
		return;
	}

	int32 SlotIndex = EventData.GetInt(TEXT("SlotIndex"), 0);
	int32 CurrentRound = EventData.GetInt(TEXT("CurrentRound"), 0);
	int32 TotalRounds = EventData.GetInt(TEXT("TotalRounds"), 0);

	// Update the slot that was just loaded
	if (SlotIndex >= 0 && SlotIndex < CachedInspectionData.RoundSlots.Num())
	{
		FSuspenseCoreRoundSlotData LoadedSlot;
		LoadedSlot.SlotIndex = SlotIndex;
		LoadedSlot.bIsOccupied = true;
		LoadedSlot.AmmoID = *EventData.GetString(TEXT("AmmoID"));
		LoadedSlot.AmmoDisplayName = FText::FromString(EventData.GetString(TEXT("AmmoName")));

		CompleteLoadingSlot_Implementation(SlotIndex, LoadedSlot);

		// If more rounds to load, start next
		if (CurrentRound < TotalRounds)
		{
			int32 NextSlotIndex = CachedInspectionData.GetFirstEmptySlot();
			if (NextSlotIndex >= 0)
			{
				float LoadTime = EventData.GetFloat(TEXT("LoadTime"), 1.0f);
				StartLoadingSlot_Implementation(NextSlotIndex, LoadTime);
			}
		}
	}
}

void USuspenseCoreMagazineInspectionWidget::OnAmmoLoadingCompletedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// Check if this event is for our magazine
	FGuid MagazineID;
	FGuid::Parse(EventData.GetString(TEXT("MagazineInstanceID")), MagazineID);
	if (MagazineID != CachedInspectionData.MagazineInstanceID)
	{
		return;
	}

	bIsLoadingInProgress = false;
	LoadingSlotIndex = -1;
	UpdateLoadingUI();
}

void USuspenseCoreMagazineInspectionWidget::OnAmmoLoadingCancelledEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// Check if this event is for our magazine
	FGuid MagazineID;
	FGuid::Parse(EventData.GetString(TEXT("MagazineInstanceID")), MagazineID);
	if (MagazineID != CachedInspectionData.MagazineInstanceID)
	{
		return;
	}

	CancelLoadingOperation_Implementation();
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreMagazineInspectionWidget::RebuildRoundSlots()
{
	ClearRoundSlots();

	if (!RoundSlotsContainer || !RoundSlotWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("MagazineInspection: RoundSlotsContainer or RoundSlotWidgetClass not set"));
		return;
	}

	// Create slot widgets for each round position
	for (int32 i = 0; i < CachedInspectionData.MaxCapacity; ++i)
	{
		UUserWidget* SlotWidget = CreateWidget<UUserWidget>(this, RoundSlotWidgetClass);
		if (SlotWidget)
		{
			RoundSlotsContainer->AddChild(SlotWidget);
			RoundSlotWidgets.Add(SlotWidget);

			// Update slot visual
			if (i < CachedInspectionData.RoundSlots.Num())
			{
				UpdateRoundSlot(i, CachedInspectionData.RoundSlots[i]);
			}
			else
			{
				// Empty slot
				FSuspenseCoreRoundSlotData EmptySlot;
				EmptySlot.SlotIndex = i;
				EmptySlot.bIsOccupied = false;
				UpdateRoundSlot(i, EmptySlot);
			}
		}
	}
}

void USuspenseCoreMagazineInspectionWidget::UpdateRoundSlot(int32 SlotIndex, const FSuspenseCoreRoundSlotData& SlotData)
{
	if (SlotIndex < 0 || SlotIndex >= RoundSlotWidgets.Num())
	{
		return;
	}

	UUserWidget* SlotWidget = RoundSlotWidgets[SlotIndex];
	if (!SlotWidget)
	{
		return;
	}

	// Update slot widget via Blueprint interface
	// The slot widget should implement a SetSlotData function
	// For now, we just set visibility

	// Find image component in slot widget and update
	if (UImage* RoundImage = Cast<UImage>(SlotWidget->GetWidgetFromName(TEXT("RoundImage"))))
	{
		if (SlotData.bIsOccupied && SlotData.AmmoIcon)
		{
			RoundImage->SetBrushFromTexture(SlotData.AmmoIcon);
			RoundImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			RoundImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Update occupied state
	if (UBorder* SlotBorder = Cast<UBorder>(SlotWidget->GetWidgetFromName(TEXT("SlotBorder"))))
	{
		// Occupied/empty styling handled in Blueprint
	}
}

void USuspenseCoreMagazineInspectionWidget::UpdateFooterUI()
{
	// Update rounds count text
	if (RoundsCountText)
	{
		RoundsCountText->SetText(FText::Format(
			NSLOCTEXT("MagInspection", "RoundsFormat", "{0}/{1}"),
			FText::AsNumber(CachedInspectionData.CurrentRounds),
			FText::AsNumber(CachedInspectionData.MaxCapacity)
		));
	}

	// Update fill progress bar
	if (FillProgressBar)
	{
		FillProgressBar->SetPercent(CachedInspectionData.GetFillPercent());
	}

	// Update hint text
	if (HintText)
	{
		if (CachedInspectionData.IsFull())
		{
			HintText->SetText(FullHintText);
		}
		else
		{
			HintText->SetText(DropHintText);
		}
	}
}

void USuspenseCoreMagazineInspectionWidget::UpdateLoadingUI()
{
	if (bIsLoadingInProgress)
	{
		if (LoadingProgressBar)
		{
			LoadingProgressBar->SetVisibility(ESlateVisibility::HitTestInvisible);
			LoadingProgressBar->SetPercent(LoadingProgress);
		}

		if (LoadingStatusText)
		{
			LoadingStatusText->SetVisibility(ESlateVisibility::HitTestInvisible);

			FText StatusFormat = bIsUnloading ? UnloadingStatusFormat : LoadingStatusFormat;
			LoadingStatusText->SetText(FText::Format(StatusFormat, FText::AsNumber(LoadingSlotIndex + 1)));
		}
	}
	else
	{
		if (LoadingProgressBar)
		{
			LoadingProgressBar->SetVisibility(ESlateVisibility::Collapsed);
		}

		if (LoadingStatusText)
		{
			LoadingStatusText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void USuspenseCoreMagazineInspectionWidget::ClearRoundSlots()
{
	for (UUserWidget* SlotWidget : RoundSlotWidgets)
	{
		if (SlotWidget)
		{
			SlotWidget->RemoveFromParent();
		}
	}
	RoundSlotWidgets.Empty();
}

void USuspenseCoreMagazineInspectionWidget::OnCloseButtonClicked()
{
	CloseInspection_Implementation();
}

void USuspenseCoreMagazineInspectionWidget::HandleRoundSlotClicked(int32 SlotIndex)
{
	// Check if slot is occupied
	if (SlotIndex >= 0 && SlotIndex < CachedInspectionData.RoundSlots.Num())
	{
		const FSuspenseCoreRoundSlotData& SlotData = CachedInspectionData.RoundSlots[SlotIndex];

		if (SlotData.bIsOccupied && SlotData.bCanUnload && !bIsLoadingInProgress)
		{
			// Notify Blueprint - it can then call AmmoLoadingService to start unload
			OnRoundClicked(SlotIndex);
		}
	}
}

bool USuspenseCoreMagazineInspectionWidget::IsCompatibleAmmo(const FSuspenseCoreItemUIData& ItemData) const
{
	// Check if item has ammo type tag
	static const FGameplayTag AmmoTag = FGameplayTag::RequestGameplayTag(FName("Item.Ammo"), false);
	static const FGameplayTag AmmoCategoryTag = FGameplayTag::RequestGameplayTag(FName("Item.Category.Ammo"), false);

	// Must be an ammo item
	if (!ItemData.ItemType.MatchesTag(AmmoTag) && !ItemData.ItemType.MatchesTag(AmmoCategoryTag))
	{
		return false;
	}

	// Check caliber compatibility if we have caliber data
	if (CachedInspectionData.CaliberTag.IsValid())
	{
		// Look for caliber tag in item's type tag hierarchy
		// Item.Ammo.556x45 should match Magazine caliber Item.Caliber.556x45
		// For now, accept all ammo - caliber validation should be done by AmmoLoadingService
	}

	return true;
}

void USuspenseCoreMagazineInspectionWidget::RequestAmmoLoad(FName AmmoID, int32 Quantity)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		UE_LOG(LogTemp, Warning, TEXT("MagazineInspection: Cannot request ammo load - EventBus not available"));
		return;
	}

	using namespace SuspenseCoreEquipmentTags::Magazine;

	// Create event data for ammo loading request
	FSuspenseCoreEventData EventData;
	EventData.SetString(TEXT("MagazineInstanceID"), CachedInspectionData.MagazineInstanceID.ToString());
	EventData.SetString(TEXT("MagazineID"), CachedInspectionData.MagazineID.ToString());
	EventData.SetString(TEXT("AmmoID"), AmmoID.ToString());
	EventData.SetInt(TEXT("Quantity"), Quantity);
	EventData.SetInt(TEXT("FirstEmptySlot"), CachedInspectionData.GetFirstEmptySlot());

	// Publish load request event
	EventBus->Publish(TAG_Equipment_Event_Ammo_LoadRequested, EventData);

	UE_LOG(LogTemp, Log, TEXT("MagazineInspection: Requested load of %d x %s into magazine %s"),
		Quantity, *AmmoID.ToString(), *CachedInspectionData.MagazineInstanceID.ToString());
}
