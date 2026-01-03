// SuspenseCoreQuickSlotHUDWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/HUD/SuspenseCoreQuickSlotHUDWidget.h"
#include "SuspenseCore/Widgets/HUD/SuspenseCoreQuickSlotEntry.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Engine/Texture2D.h"

USuspenseCoreQuickSlotHUDWidget::USuspenseCoreQuickSlotHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ═══════════════════════════════════════════════════════════════════════════════
// UUserWidget Interface
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreQuickSlotHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	GenerateSlotEntries();
	SetupEventSubscriptions();
}

void USuspenseCoreQuickSlotHUDWidget::NativeDestruct()
{
	TeardownEventSubscriptions();
	ClearGeneratedSlots();

	Super::NativeDestruct();
}

// ═══════════════════════════════════════════════════════════════════════════════
// ISuspenseCoreQuickSlotHUDWidget Implementation
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreQuickSlotHUDWidget::InitializeQuickSlots_Implementation(AActor* InOwnerActor)
{
	OwnerActor = InOwnerActor;
	bIsInitialized = true;

	RefreshAllSlots();
}

void USuspenseCoreQuickSlotHUDWidget::CleanupQuickSlots_Implementation()
{
	OwnerActor = nullptr;
	bIsInitialized = false;

	// Clear all slots
	for (USuspenseCoreQuickSlotEntry* Entry : SlotEntries)
	{
		if (Entry)
		{
			Entry->ClearSlot();
		}
	}
}

void USuspenseCoreQuickSlotHUDWidget::UpdateSlot_Implementation(const FSuspenseCoreQuickSlotHUDData& SlotData)
{
	if (IsValidSlotIndex(SlotData.SlotIndex))
	{
		if (USuspenseCoreQuickSlotEntry* Entry = SlotEntries[SlotData.SlotIndex])
		{
			Entry->UpdateSlotData(SlotData);
			OnSlotUpdated(SlotData.SlotIndex, SlotData);
		}
	}
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
	if (IsValidSlotIndex(SlotIndex))
	{
		if (USuspenseCoreQuickSlotEntry* Entry = SlotEntries[SlotIndex])
		{
			Entry->ClearSlot();
		}
	}
}

void USuspenseCoreQuickSlotHUDWidget::PlaySlotUseAnimation_Implementation(int32 SlotIndex)
{
	if (IsValidSlotIndex(SlotIndex))
	{
		if (USuspenseCoreQuickSlotEntry* Entry = SlotEntries[SlotIndex])
		{
			Entry->PlayUseAnimation();
			OnSlotUsed(SlotIndex);
		}
	}
}

void USuspenseCoreQuickSlotHUDWidget::UpdateSlotCooldown_Implementation(int32 SlotIndex, float RemainingTime, float TotalTime)
{
	if (IsValidSlotIndex(SlotIndex))
	{
		if (USuspenseCoreQuickSlotEntry* Entry = SlotEntries[SlotIndex])
		{
			Entry->SetCooldownTarget(RemainingTime, TotalTime);
		}
	}
}

void USuspenseCoreQuickSlotHUDWidget::HighlightSlot_Implementation(int32 SlotIndex)
{
	// Remove previous highlight
	if (HighlightedSlotIndex != INDEX_NONE && IsValidSlotIndex(HighlightedSlotIndex))
	{
		if (USuspenseCoreQuickSlotEntry* Entry = SlotEntries[HighlightedSlotIndex])
		{
			Entry->SetHighlighted(false);
		}
	}

	HighlightedSlotIndex = SlotIndex;

	// Apply new highlight
	if (IsValidSlotIndex(SlotIndex))
	{
		if (USuspenseCoreQuickSlotEntry* Entry = SlotEntries[SlotIndex])
		{
			Entry->SetHighlighted(true);
		}
	}
}

void USuspenseCoreQuickSlotHUDWidget::SetSlotAvailability_Implementation(int32 SlotIndex, bool bAvailable)
{
	if (IsValidSlotIndex(SlotIndex))
	{
		if (USuspenseCoreQuickSlotEntry* Entry = SlotEntries[SlotIndex])
		{
			Entry->SetAvailable(bAvailable);
		}
	}
}

void USuspenseCoreQuickSlotHUDWidget::UpdateMagazineRounds_Implementation(int32 SlotIndex, int32 CurrentRounds, int32 MaxRounds)
{
	if (IsValidSlotIndex(SlotIndex))
	{
		if (USuspenseCoreQuickSlotEntry* Entry = SlotEntries[SlotIndex])
		{
			Entry->UpdateMagazineRounds(CurrentRounds, MaxRounds);
		}
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
	// Request data from EventBus - components will respond
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (EventBus && OwnerActor.IsValid())
	{
		FSuspenseCoreEventData RequestData;
		RequestData.SetObject(TEXT("Owner"), OwnerActor.Get());

		using namespace SuspenseCoreEquipmentTags::QuickSlot;
		EventBus->Publish(TAG_Equipment_Event_QuickSlot, RequestData);
	}
}

USuspenseCoreQuickSlotEntry* USuspenseCoreQuickSlotHUDWidget::GetSlotEntry(int32 SlotIndex) const
{
	if (IsValidSlotIndex(SlotIndex))
	{
		return SlotEntries[SlotIndex];
	}
	return nullptr;
}

// ═══════════════════════════════════════════════════════════════════════════════
// PROCEDURAL GENERATION
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreQuickSlotHUDWidget::GenerateSlotEntries()
{
	if (!SlotContainer || !SlotEntryClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreQuickSlotHUDWidget: SlotContainer or SlotEntryClass is null!"));
		return;
	}

	ClearGeneratedSlots();

	SlotEntries.Reserve(SlotCount);

	for (int32 i = 0; i < SlotCount; ++i)
	{
		USuspenseCoreQuickSlotEntry* Entry = CreateWidget<USuspenseCoreQuickSlotEntry>(this, SlotEntryClass);
		if (Entry)
		{
			// Initialize with index and hotkey
			FText HotkeyText = HotkeyTexts.IsValidIndex(i) ? HotkeyTexts[i] : FText::AsNumber(i + 4);
			Entry->InitializeSlot(i, HotkeyText);

			// Add to container
			UHorizontalBoxSlot* BoxSlot = SlotContainer->AddChildToHorizontalBox(Entry);
			if (BoxSlot && i > 0)
			{
				BoxSlot->SetPadding(FMargin(SlotSpacing, 0.0f, 0.0f, 0.0f));
			}

			SlotEntries.Add(Entry);
		}
	}

	OnSlotsGenerated(SlotEntries.Num());
}

void USuspenseCoreQuickSlotHUDWidget::ClearGeneratedSlots()
{
	if (SlotContainer)
	{
		SlotContainer->ClearChildren();
	}

	SlotEntries.Empty();
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

	using namespace SuspenseCoreEquipmentTags::QuickSlot;
	using namespace SuspenseCoreEquipmentTags::Magazine;

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
	int32 SlotIndex = EventData.GetInt(TEXT("SlotIndex"), INDEX_NONE);
	if (!IsValidSlotIndex(SlotIndex))
	{
		return;
	}

	FSuspenseCoreQuickSlotHUDData SlotData;
	SlotData.SlotIndex = SlotIndex;
	SlotData.ItemID = FName(*EventData.GetString(TEXT("ItemID")));
	SlotData.DisplayName = FText::FromString(EventData.GetString(TEXT("DisplayName")));
	SlotData.Quantity = EventData.GetInt(TEXT("Quantity"), 1);
	SlotData.bIsMagazine = EventData.GetBool(TEXT("IsMagazine"), false);
	SlotData.MagazineRounds = EventData.GetInt(TEXT("MagazineRounds"), 0);
	SlotData.MagazineCapacity = EventData.GetInt(TEXT("MagazineCapacity"), 0);
	SlotData.bIsAvailable = EventData.GetBool(TEXT("IsAvailable"), true);

	// Get item icon from event data
	SlotData.Icon = EventData.GetObject<UTexture2D>(TEXT("Icon"));

	UpdateSlot_Implementation(SlotData);
}

void USuspenseCoreQuickSlotHUDWidget::OnQuickSlotClearedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	int32 SlotIndex = EventData.GetInt(TEXT("SlotIndex"), INDEX_NONE);
	ClearSlot_Implementation(SlotIndex);
}

void USuspenseCoreQuickSlotHUDWidget::OnQuickSlotUsedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	int32 SlotIndex = EventData.GetInt(TEXT("SlotIndex"), INDEX_NONE);
	PlaySlotUseAnimation_Implementation(SlotIndex);
}

void USuspenseCoreQuickSlotHUDWidget::OnQuickSlotCooldownStartedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	int32 SlotIndex = EventData.GetInt(TEXT("SlotIndex"), INDEX_NONE);
	float Duration = EventData.GetFloat(TEXT("Duration"), 1.0f);

	UpdateSlotCooldown_Implementation(SlotIndex, Duration, Duration);
}

void USuspenseCoreQuickSlotHUDWidget::OnQuickSlotCooldownEndedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	int32 SlotIndex = EventData.GetInt(TEXT("SlotIndex"), INDEX_NONE);
	UpdateSlotCooldown_Implementation(SlotIndex, 0.0f, 0.0f);
}

void USuspenseCoreQuickSlotHUDWidget::OnMagazineRoundsChangedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	int32 SlotIndex = EventData.GetInt(TEXT("QuickSlotIndex"), INDEX_NONE);
	if (SlotIndex == INDEX_NONE)
	{
		return;
	}

	int32 CurrentRounds = EventData.GetInt(TEXT("CurrentRounds"), 0);
	int32 MaxRounds = EventData.GetInt(TEXT("MaxRounds"), 0);

	UpdateMagazineRounds_Implementation(SlotIndex, CurrentRounds, MaxRounds);
}
