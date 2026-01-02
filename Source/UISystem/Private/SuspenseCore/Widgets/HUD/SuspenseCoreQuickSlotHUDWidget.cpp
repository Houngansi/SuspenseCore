// SuspenseCoreQuickSlotHUDWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/HUD/SuspenseCoreQuickSlotHUDWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Border.h"

USuspenseCoreQuickSlotHUDWidget::USuspenseCoreQuickSlotHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Initialize arrays
	CachedSlotData.SetNum(SUSPENSECORE_HUD_QUICKSLOT_COUNT);
	DisplayedCooldownProgress.Init(1.0f, SUSPENSECORE_HUD_QUICKSLOT_COUNT);
	TargetCooldownProgress.Init(1.0f, SUSPENSECORE_HUD_QUICKSLOT_COUNT);
}

// ═══════════════════════════════════════════════════════════════════════════════
// UUserWidget Interface
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreQuickSlotHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Setup default hotkey texts
	if (Slot0HotkeyText && DefaultHotkeyTexts.IsValidIndex(0))
	{
		Slot0HotkeyText->SetText(DefaultHotkeyTexts[0]);
	}
	if (Slot1HotkeyText && DefaultHotkeyTexts.IsValidIndex(1))
	{
		Slot1HotkeyText->SetText(DefaultHotkeyTexts[1]);
	}
	if (Slot2HotkeyText && DefaultHotkeyTexts.IsValidIndex(2))
	{
		Slot2HotkeyText->SetText(DefaultHotkeyTexts[2]);
	}
	if (Slot3HotkeyText && DefaultHotkeyTexts.IsValidIndex(3))
	{
		Slot3HotkeyText->SetText(DefaultHotkeyTexts[3]);
	}

	// Setup EventBus subscriptions
	SetupEventSubscriptions();
}

void USuspenseCoreQuickSlotHUDWidget::NativeDestruct()
{
	TeardownEventSubscriptions();
	Super::NativeDestruct();
}

void USuspenseCoreQuickSlotHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Update cooldown interpolation
	if (bSmoothCooldown)
	{
		UpdateCooldowns(InDeltaTime);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// ISuspenseCoreQuickSlotHUDWidget Implementation
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreQuickSlotHUDWidget::InitializeQuickSlots_Implementation(AActor* InOwnerActor)
{
	OwnerActor = InOwnerActor;
	bIsInitialized = true;

	// Clear all slots to initial state
	for (int32 i = 0; i < SUSPENSECORE_HUD_QUICKSLOT_COUNT; ++i)
	{
		ClearSlot_Implementation(i);
	}
}

void USuspenseCoreQuickSlotHUDWidget::CleanupQuickSlots_Implementation()
{
	OwnerActor = nullptr;
	bIsInitialized = false;
}

void USuspenseCoreQuickSlotHUDWidget::UpdateSlot_Implementation(const FSuspenseCoreQuickSlotHUDData& SlotData)
{
	if (!IsValidSlotIndex(SlotData.SlotIndex))
	{
		return;
	}

	CachedSlotData[SlotData.SlotIndex] = SlotData;
	UpdateSlotUI(SlotData.SlotIndex, SlotData);

	// Notify Blueprint
	OnSlotUpdated(SlotData.SlotIndex, SlotData);
}

void USuspenseCoreQuickSlotHUDWidget::UpdateAllSlots_Implementation(const TArray<FSuspenseCoreQuickSlotHUDData>& AllSlots)
{
	for (const FSuspenseCoreQuickSlotHUDData& SlotData : AllSlots)
	{
		UpdateSlot_Implementation(SlotData);
	}
}

void USuspenseCoreQuickSlotHUDWidget::ClearSlot_Implementation(int32 SlotIndex)
{
	if (!IsValidSlotIndex(SlotIndex))
	{
		return;
	}

	FSuspenseCoreQuickSlotHUDData EmptyData;
	EmptyData.SlotIndex = SlotIndex;
	EmptyData.bIsAvailable = true;

	if (DefaultHotkeyTexts.IsValidIndex(SlotIndex))
	{
		EmptyData.HotkeyText = DefaultHotkeyTexts[SlotIndex];
	}

	CachedSlotData[SlotIndex] = EmptyData;
	UpdateSlotUI(SlotIndex, EmptyData);
}

void USuspenseCoreQuickSlotHUDWidget::PlaySlotUseAnimation_Implementation(int32 SlotIndex)
{
	if (!IsValidSlotIndex(SlotIndex))
	{
		return;
	}

	// Notify Blueprint to play animation
	OnSlotUsed(SlotIndex);
}

void USuspenseCoreQuickSlotHUDWidget::UpdateSlotCooldown_Implementation(int32 SlotIndex, float RemainingTime, float TotalTime)
{
	if (!IsValidSlotIndex(SlotIndex))
	{
		return;
	}

	CachedSlotData[SlotIndex].CooldownRemaining = RemainingTime;
	CachedSlotData[SlotIndex].CooldownDuration = TotalTime;

	// Calculate target progress (1 = ready, 0 = just started)
	float Progress = (TotalTime > 0.0f) ? (1.0f - (RemainingTime / TotalTime)) : 1.0f;
	TargetCooldownProgress[SlotIndex] = Progress;

	// If not using smooth cooldown, update immediately
	if (!bSmoothCooldown)
	{
		DisplayedCooldownProgress[SlotIndex] = Progress;

		UBorder* Border = nullptr;
		UImage* Icon = nullptr;
		UTextBlock* Quantity = nullptr;
		UTextBlock* Hotkey = nullptr;
		UProgressBar* CooldownBar = nullptr;
		GetSlotElements(SlotIndex, Border, Icon, Quantity, Hotkey, CooldownBar);

		if (CooldownBar)
		{
			CooldownBar->SetPercent(Progress);
		}
	}
}

void USuspenseCoreQuickSlotHUDWidget::HighlightSlot_Implementation(int32 SlotIndex)
{
	HighlightedSlotIndex = SlotIndex;

	// Highlighting is handled via Blueprint events or material parameters
	// This just stores the state - visual feedback should come from materials
}

void USuspenseCoreQuickSlotHUDWidget::SetSlotAvailability_Implementation(int32 SlotIndex, bool bAvailable)
{
	if (!IsValidSlotIndex(SlotIndex))
	{
		return;
	}

	CachedSlotData[SlotIndex].bIsAvailable = bAvailable;

	// Visual feedback handled by materials in Editor
}

void USuspenseCoreQuickSlotHUDWidget::UpdateMagazineRounds_Implementation(int32 SlotIndex, int32 CurrentRounds, int32 MaxRounds)
{
	if (!IsValidSlotIndex(SlotIndex))
	{
		return;
	}

	CachedSlotData[SlotIndex].MagazineRounds = CurrentRounds;
	CachedSlotData[SlotIndex].MagazineCapacity = MaxRounds;

	// Update quantity text
	UBorder* Border = nullptr;
	UImage* Icon = nullptr;
	UTextBlock* Quantity = nullptr;
	UTextBlock* Hotkey = nullptr;
	UProgressBar* CooldownBar = nullptr;
	GetSlotElements(SlotIndex, Border, Icon, Quantity, Hotkey, CooldownBar);

	if (Quantity)
	{
		Quantity->SetText(FText::AsNumber(CurrentRounds));
	}
}

void USuspenseCoreQuickSlotHUDWidget::SetQuickSlotHUDVisible_Implementation(bool bVisible)
{
	SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

bool USuspenseCoreQuickSlotHUDWidget::IsQuickSlotHUDVisible_Implementation() const
{
	return GetVisibility() != ESlateVisibility::Collapsed && GetVisibility() != ESlateVisibility::Hidden;
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreQuickSlotHUDWidget::RefreshAllSlots()
{
	for (int32 i = 0; i < SUSPENSECORE_HUD_QUICKSLOT_COUNT; ++i)
	{
		if (CachedSlotData.IsValidIndex(i))
		{
			UpdateSlotUI(i, CachedSlotData[i]);
		}
	}
}

FSuspenseCoreQuickSlotHUDData USuspenseCoreQuickSlotHUDWidget::GetSlotData(int32 SlotIndex) const
{
	if (CachedSlotData.IsValidIndex(SlotIndex))
	{
		return CachedSlotData[SlotIndex];
	}
	return FSuspenseCoreQuickSlotHUDData();
}

// ═══════════════════════════════════════════════════════════════════════════════
// EVENTBUS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreQuickSlotHUDWidget::SetupEventSubscriptions()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	using namespace SuspenseCoreEquipmentTags::Event;

	// Subscribe to QuickSlot events - use NATIVE TAGS!
	QuickSlotAssignedHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_QuickSlot_Assigned,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreQuickSlotHUDWidget::OnQuickSlotAssignedEvent),
		ESuspenseCoreEventPriority::Normal
	);

	QuickSlotClearedHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_QuickSlot_Cleared,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreQuickSlotHUDWidget::OnQuickSlotClearedEvent),
		ESuspenseCoreEventPriority::Normal
	);

	QuickSlotUsedHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_QuickSlot_Used,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreQuickSlotHUDWidget::OnQuickSlotUsedEvent),
		ESuspenseCoreEventPriority::Normal
	);

	QuickSlotCooldownStartedHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_QuickSlot_CooldownStarted,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreQuickSlotHUDWidget::OnQuickSlotCooldownStartedEvent),
		ESuspenseCoreEventPriority::Normal
	);

	QuickSlotCooldownEndedHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_QuickSlot_CooldownEnded,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreQuickSlotHUDWidget::OnQuickSlotCooldownEndedEvent),
		ESuspenseCoreEventPriority::Normal
	);

	MagazineRoundsChangedHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_Magazine_RoundsChanged,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreQuickSlotHUDWidget::OnMagazineRoundsChangedEvent),
		ESuspenseCoreEventPriority::Normal
	);
}

void USuspenseCoreQuickSlotHUDWidget::TeardownEventSubscriptions()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	EventBus->Unsubscribe(QuickSlotAssignedHandle);
	EventBus->Unsubscribe(QuickSlotClearedHandle);
	EventBus->Unsubscribe(QuickSlotUsedHandle);
	EventBus->Unsubscribe(QuickSlotCooldownStartedHandle);
	EventBus->Unsubscribe(QuickSlotCooldownEndedHandle);
	EventBus->Unsubscribe(MagazineRoundsChangedHandle);
}

USuspenseCoreEventBus* USuspenseCoreQuickSlotHUDWidget::GetEventBus() const
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	USuspenseCoreEventManager* EventManager = USuspenseCoreEventManager::Get(this);
	if (EventManager)
	{
		const_cast<USuspenseCoreQuickSlotHUDWidget*>(this)->CachedEventBus = EventManager->GetEventBus();
		return CachedEventBus.Get();
	}

	return nullptr;
}

void USuspenseCoreQuickSlotHUDWidget::OnQuickSlotAssignedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	int32 SlotIndex = EventData.GetInt32(TEXT("SlotIndex"), INDEX_NONE);
	if (!IsValidSlotIndex(SlotIndex))
	{
		return;
	}

	FSuspenseCoreQuickSlotHUDData SlotData;
	SlotData.SlotIndex = SlotIndex;
	SlotData.ItemID = FName(*EventData.GetString(TEXT("ItemID")));
	SlotData.DisplayName = FText::FromString(EventData.GetString(TEXT("DisplayName")));
	SlotData.Quantity = EventData.GetInt32(TEXT("Quantity"), 1);
	SlotData.bIsMagazine = EventData.GetBool(TEXT("IsMagazine"));
	SlotData.MagazineRounds = EventData.GetInt32(TEXT("MagazineRounds"), 0);
	SlotData.MagazineCapacity = EventData.GetInt32(TEXT("MagazineCapacity"), 0);
	SlotData.bIsAvailable = true;

	if (DefaultHotkeyTexts.IsValidIndex(SlotIndex))
	{
		SlotData.HotkeyText = DefaultHotkeyTexts[SlotIndex];
	}

	UpdateSlot_Implementation(SlotData);
}

void USuspenseCoreQuickSlotHUDWidget::OnQuickSlotClearedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	int32 SlotIndex = EventData.GetInt32(TEXT("SlotIndex"), INDEX_NONE);
	ClearSlot_Implementation(SlotIndex);
}

void USuspenseCoreQuickSlotHUDWidget::OnQuickSlotUsedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	int32 SlotIndex = EventData.GetInt32(TEXT("SlotIndex"), INDEX_NONE);
	PlaySlotUseAnimation_Implementation(SlotIndex);
}

void USuspenseCoreQuickSlotHUDWidget::OnQuickSlotCooldownStartedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	int32 SlotIndex = EventData.GetInt32(TEXT("SlotIndex"), INDEX_NONE);
	float Duration = EventData.GetFloat(TEXT("Duration"), 0.0f);

	UpdateSlotCooldown_Implementation(SlotIndex, Duration, Duration);
	OnSlotCooldownStarted(SlotIndex, Duration);
}

void USuspenseCoreQuickSlotHUDWidget::OnQuickSlotCooldownEndedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	int32 SlotIndex = EventData.GetInt32(TEXT("SlotIndex"), INDEX_NONE);

	UpdateSlotCooldown_Implementation(SlotIndex, 0.0f, 0.0f);
	OnSlotCooldownEnded(SlotIndex);
}

void USuspenseCoreQuickSlotHUDWidget::OnMagazineRoundsChangedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	int32 SlotIndex = EventData.GetInt32(TEXT("SlotIndex"), INDEX_NONE);
	int32 CurrentRounds = EventData.GetInt32(TEXT("CurrentRounds"), 0);
	int32 MaxRounds = EventData.GetInt32(TEXT("MaxRounds"), 0);

	UpdateMagazineRounds_Implementation(SlotIndex, CurrentRounds, MaxRounds);
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreQuickSlotHUDWidget::GetSlotElements(int32 SlotIndex, UBorder*& OutBorder, UImage*& OutIcon,
	UTextBlock*& OutQuantity, UTextBlock*& OutHotkey, UProgressBar*& OutCooldown) const
{
	switch (SlotIndex)
	{
	case 0:
		OutBorder = Slot0Border;
		OutIcon = Slot0Icon;
		OutQuantity = Slot0QuantityText;
		OutHotkey = Slot0HotkeyText;
		OutCooldown = Slot0CooldownBar;
		break;
	case 1:
		OutBorder = Slot1Border;
		OutIcon = Slot1Icon;
		OutQuantity = Slot1QuantityText;
		OutHotkey = Slot1HotkeyText;
		OutCooldown = Slot1CooldownBar;
		break;
	case 2:
		OutBorder = Slot2Border;
		OutIcon = Slot2Icon;
		OutQuantity = Slot2QuantityText;
		OutHotkey = Slot2HotkeyText;
		OutCooldown = Slot2CooldownBar;
		break;
	case 3:
		OutBorder = Slot3Border;
		OutIcon = Slot3Icon;
		OutQuantity = Slot3QuantityText;
		OutHotkey = Slot3HotkeyText;
		OutCooldown = Slot3CooldownBar;
		break;
	default:
		OutBorder = nullptr;
		OutIcon = nullptr;
		OutQuantity = nullptr;
		OutHotkey = nullptr;
		OutCooldown = nullptr;
		break;
	}
}

void USuspenseCoreQuickSlotHUDWidget::UpdateSlotUI(int32 SlotIndex, const FSuspenseCoreQuickSlotHUDData& SlotData)
{
	UBorder* Border = nullptr;
	UImage* Icon = nullptr;
	UTextBlock* Quantity = nullptr;
	UTextBlock* Hotkey = nullptr;
	UProgressBar* CooldownBar = nullptr;
	GetSlotElements(SlotIndex, Border, Icon, Quantity, Hotkey, CooldownBar);

	// Update icon
	if (Icon)
	{
		if (SlotData.Icon)
		{
			Icon->SetBrushFromTexture(SlotData.Icon);
			Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else if (EmptySlotTexture && SlotData.IsEmpty())
		{
			Icon->SetBrushFromTexture(EmptySlotTexture);
			Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Icon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Update quantity text
	if (Quantity)
	{
		if (SlotData.IsEmpty())
		{
			Quantity->SetVisibility(ESlateVisibility::Collapsed);
		}
		else if (SlotData.bIsMagazine)
		{
			Quantity->SetText(FText::AsNumber(SlotData.MagazineRounds));
			Quantity->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else if (SlotData.Quantity > 1)
		{
			Quantity->SetText(FText::Format(NSLOCTEXT("QuickSlot", "QuantityFormat", "x{0}"), FText::AsNumber(SlotData.Quantity)));
			Quantity->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Quantity->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Update hotkey
	if (Hotkey)
	{
		Hotkey->SetText(SlotData.HotkeyText);
	}

	// Update cooldown bar
	if (CooldownBar)
	{
		float Progress = SlotData.GetCooldownProgress();
		CooldownBar->SetPercent(Progress);

		// Show bar only if on cooldown
		CooldownBar->SetVisibility(Progress < 1.0f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void USuspenseCoreQuickSlotHUDWidget::UpdateCooldowns(float DeltaTime)
{
	for (int32 i = 0; i < SUSPENSECORE_HUD_QUICKSLOT_COUNT; ++i)
	{
		if (FMath::Abs(DisplayedCooldownProgress[i] - TargetCooldownProgress[i]) > KINDA_SMALL_NUMBER)
		{
			DisplayedCooldownProgress[i] = FMath::FInterpTo(
				DisplayedCooldownProgress[i],
				TargetCooldownProgress[i],
				DeltaTime,
				CooldownInterpSpeed
			);

			UBorder* Border = nullptr;
			UImage* Icon = nullptr;
			UTextBlock* Quantity = nullptr;
			UTextBlock* Hotkey = nullptr;
			UProgressBar* CooldownBar = nullptr;
			GetSlotElements(i, Border, Icon, Quantity, Hotkey, CooldownBar);

			if (CooldownBar)
			{
				CooldownBar->SetPercent(DisplayedCooldownProgress[i]);
				CooldownBar->SetVisibility(DisplayedCooldownProgress[i] < 0.999f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			}
		}
	}
}
