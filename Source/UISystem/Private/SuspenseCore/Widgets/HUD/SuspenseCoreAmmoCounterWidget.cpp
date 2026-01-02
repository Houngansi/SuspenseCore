// SuspenseCoreAmmoCounterWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/HUD/SuspenseCoreAmmoCounterWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"

USuspenseCoreAmmoCounterWidget::USuspenseCoreAmmoCounterWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ═══════════════════════════════════════════════════════════════════════════════
// UUserWidget Interface
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreAmmoCounterWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetupEventSubscriptions();
}

void USuspenseCoreAmmoCounterWidget::NativeDestruct()
{
	TeardownEventSubscriptions();
	Super::NativeDestruct();
}

void USuspenseCoreAmmoCounterWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bSmoothFillBar)
	{
		UpdateFillBar(InDeltaTime);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// ISuspenseCoreAmmoCounterWidget Implementation
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreAmmoCounterWidget::InitializeWithWeapon_Implementation(AActor* WeaponActor)
{
	CachedWeaponActor = WeaponActor;
	bIsInitialized = true;
	bHasMagazine = true;

	// Reset display
	CachedAmmoData = FSuspenseCoreAmmoCounterData();
	RefreshDisplay();
}

void USuspenseCoreAmmoCounterWidget::ClearWeapon_Implementation()
{
	CachedWeaponActor = nullptr;
	bIsInitialized = false;

	// Clear display
	CachedAmmoData = FSuspenseCoreAmmoCounterData();
	SetNoMagazineState_Implementation(true);
}

void USuspenseCoreAmmoCounterWidget::UpdateAmmoCounter_Implementation(const FSuspenseCoreAmmoCounterData& AmmoData)
{
	CachedAmmoData = AmmoData;
	bHasMagazine = AmmoData.bHasMagazine;

	TargetFillPercent = AmmoData.GetMagazineFillPercent();

	UpdateMagazineUI();
	UpdateReserveUI();
	UpdateFireModeUI();
	UpdateAmmoTypeUI();
	CheckAmmoWarnings();
}

void USuspenseCoreAmmoCounterWidget::UpdateMagazineState_Implementation(int32 CurrentRounds, int32 MaxRounds, bool bChambered)
{
	CachedAmmoData.MagazineRounds = CurrentRounds;
	CachedAmmoData.MagazineCapacity = MaxRounds;
	CachedAmmoData.bHasChamberedRound = bChambered;
	CachedAmmoData.bHasMagazine = true;
	bHasMagazine = true;

	TargetFillPercent = CachedAmmoData.GetMagazineFillPercent();

	UpdateMagazineUI();
	CheckAmmoWarnings();
}

void USuspenseCoreAmmoCounterWidget::UpdateReserveAmmo_Implementation(int32 ReserveRounds, int32 AvailableMags)
{
	CachedAmmoData.ReserveRounds = ReserveRounds;
	CachedAmmoData.AvailableMagazines = AvailableMags;

	UpdateReserveUI();
}

void USuspenseCoreAmmoCounterWidget::UpdateFireMode_Implementation(FGameplayTag FireModeTag, const FText& DisplayText)
{
	CachedAmmoData.FireModeTag = FireModeTag;
	CachedAmmoData.FireModeText = DisplayText;

	UpdateFireModeUI();
	OnFireModeChanged(DisplayText);
}

void USuspenseCoreAmmoCounterWidget::UpdateAmmoType_Implementation(FName AmmoID, const FText& DisplayName)
{
	CachedAmmoData.LoadedAmmoType = AmmoID;
	CachedAmmoData.AmmoDisplayName = DisplayName;

	UpdateAmmoTypeUI();
}

void USuspenseCoreAmmoCounterWidget::SetLowAmmoWarning_Implementation(bool bLowAmmo, bool bCritical)
{
	bIsLowAmmo = bLowAmmo;
	bIsCriticalAmmo = bCritical;

	// Visual feedback handled by materials - no programmatic color changes!
	OnLowAmmo(bCritical);
}

void USuspenseCoreAmmoCounterWidget::SetNoMagazineState_Implementation(bool bNoMagazine)
{
	bHasMagazine = !bNoMagazine;

	if (MagazineRoundsText)
	{
		MagazineRoundsText->SetText(bNoMagazine ? NoMagazineText : FText::AsNumber(CachedAmmoData.MagazineRounds));
	}

	if (ChamberIndicatorText)
	{
		ChamberIndicatorText->SetVisibility(bNoMagazine ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (MagazineCapacityText)
	{
		MagazineCapacityText->SetVisibility(bNoMagazine ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (MagazineFillBar)
	{
		MagazineFillBar->SetVisibility(bNoMagazine ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void USuspenseCoreAmmoCounterWidget::PlayAmmoConsumedAnimation_Implementation()
{
	OnAmmoConsumed(CachedAmmoData.MagazineRounds);
}

void USuspenseCoreAmmoCounterWidget::PlayMagazineSwapAnimation_Implementation()
{
	OnMagazineSwapped(CachedAmmoData.MagazineRounds, CachedAmmoData.MagazineCapacity);
}

void USuspenseCoreAmmoCounterWidget::SetAmmoCounterVisible_Implementation(bool bVisible)
{
	SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

bool USuspenseCoreAmmoCounterWidget::IsAmmoCounterVisible_Implementation() const
{
	return GetVisibility() != ESlateVisibility::Collapsed && GetVisibility() != ESlateVisibility::Hidden;
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreAmmoCounterWidget::RefreshDisplay()
{
	UpdateMagazineUI();
	UpdateReserveUI();
	UpdateFireModeUI();
	UpdateAmmoTypeUI();
}

// ═══════════════════════════════════════════════════════════════════════════════
// EVENTBUS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreAmmoCounterWidget::SetupEventSubscriptions()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	using namespace SuspenseCoreEquipmentTags::Event;

	MagazineInsertedHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_Magazine_Inserted,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreAmmoCounterWidget::OnMagazineInsertedEvent),
		ESuspenseCoreEventPriority::Normal
	);

	MagazineEjectedHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_Magazine_Ejected,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreAmmoCounterWidget::OnMagazineEjectedEvent),
		ESuspenseCoreEventPriority::Normal
	);

	MagazineRoundsChangedHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_Magazine_RoundsChanged,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreAmmoCounterWidget::OnMagazineRoundsChangedEvent),
		ESuspenseCoreEventPriority::Normal
	);

	WeaponAmmoChangedHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_Weapon_AmmoChanged,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreAmmoCounterWidget::OnWeaponAmmoChangedEvent),
		ESuspenseCoreEventPriority::Normal
	);

	// Note: FireModeChanged tag would need to be added to native tags
	// For now, fire mode updates come through UpdateFireMode_Implementation
}

void USuspenseCoreAmmoCounterWidget::TeardownEventSubscriptions()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	EventBus->Unsubscribe(MagazineInsertedHandle);
	EventBus->Unsubscribe(MagazineEjectedHandle);
	EventBus->Unsubscribe(MagazineRoundsChangedHandle);
	EventBus->Unsubscribe(WeaponAmmoChangedHandle);
	EventBus->Unsubscribe(FireModeChangedHandle);
}

USuspenseCoreEventBus* USuspenseCoreAmmoCounterWidget::GetEventBus() const
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	USuspenseCoreEventManager* EventManager = USuspenseCoreEventManager::Get(this);
	if (EventManager)
	{
		const_cast<USuspenseCoreAmmoCounterWidget*>(this)->CachedEventBus = EventManager->GetEventBus();
		return CachedEventBus.Get();
	}

	return nullptr;
}

void USuspenseCoreAmmoCounterWidget::OnMagazineInsertedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	int32 Rounds = EventData.GetInt32(TEXT("CurrentRounds"), 0);
	int32 Capacity = EventData.GetInt32(TEXT("MaxCapacity"), 30);
	FName AmmoType = FName(*EventData.GetString(TEXT("LoadedAmmoType")));

	CachedAmmoData.MagazineRounds = Rounds;
	CachedAmmoData.MagazineCapacity = Capacity;
	CachedAmmoData.LoadedAmmoType = AmmoType;
	CachedAmmoData.bHasMagazine = true;

	TargetFillPercent = CachedAmmoData.GetMagazineFillPercent();

	SetNoMagazineState_Implementation(false);
	UpdateMagazineUI();
	PlayMagazineSwapAnimation_Implementation();
}

void USuspenseCoreAmmoCounterWidget::OnMagazineEjectedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	CachedAmmoData.bHasMagazine = false;
	SetNoMagazineState_Implementation(true);
}

void USuspenseCoreAmmoCounterWidget::OnMagazineRoundsChangedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	int32 CurrentRounds = EventData.GetInt32(TEXT("CurrentRounds"), 0);
	int32 MaxRounds = EventData.GetInt32(TEXT("MaxCapacity"), CachedAmmoData.MagazineCapacity);

	CachedAmmoData.MagazineRounds = CurrentRounds;
	CachedAmmoData.MagazineCapacity = MaxRounds;

	TargetFillPercent = CachedAmmoData.GetMagazineFillPercent();

	UpdateMagazineUI();
	CheckAmmoWarnings();
}

void USuspenseCoreAmmoCounterWidget::OnWeaponAmmoChangedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// This is fired when ammo is consumed (shot fired)
	int32 CurrentRounds = EventData.GetInt32(TEXT("CurrentRounds"), CachedAmmoData.MagazineRounds);
	bool bChambered = EventData.GetBool(TEXT("HasChamberedRound"));

	CachedAmmoData.MagazineRounds = CurrentRounds;
	CachedAmmoData.bHasChamberedRound = bChambered;

	TargetFillPercent = CachedAmmoData.GetMagazineFillPercent();

	UpdateMagazineUI();
	PlayAmmoConsumedAnimation_Implementation();
	CheckAmmoWarnings();
}

void USuspenseCoreAmmoCounterWidget::OnFireModeChangedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	FString FireModeStr = EventData.GetString(TEXT("FireMode"));
	FGameplayTag FireModeTag = EventData.GetTag(TEXT("FireModeTag"));

	CachedAmmoData.FireModeTag = FireModeTag;
	CachedAmmoData.FireModeText = FText::FromString(FireModeStr);

	UpdateFireModeUI();
	OnFireModeChanged(CachedAmmoData.FireModeText);
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreAmmoCounterWidget::UpdateMagazineUI()
{
	if (!bHasMagazine)
	{
		return;
	}

	// Update magazine rounds
	if (MagazineRoundsText)
	{
		MagazineRoundsText->SetText(FText::AsNumber(CachedAmmoData.MagazineRounds));
	}

	// Update chamber indicator
	if (ChamberIndicatorText && bShowChamberIndicator)
	{
		if (CachedAmmoData.bHasChamberedRound)
		{
			ChamberIndicatorText->SetText(ChamberFormat);
			ChamberIndicatorText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			ChamberIndicatorText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Update capacity
	if (MagazineCapacityText)
	{
		MagazineCapacityText->SetText(FText::Format(
			NSLOCTEXT("AmmoCounter", "CapacityFormat", "/{0}"),
			FText::AsNumber(CachedAmmoData.MagazineCapacity)
		));
	}

	// Update fill bar (if not using smooth interpolation)
	if (MagazineFillBar && !bSmoothFillBar)
	{
		MagazineFillBar->SetPercent(CachedAmmoData.GetMagazineFillPercent());
	}
}

void USuspenseCoreAmmoCounterWidget::UpdateReserveUI()
{
	if (!bShowReserveInfo)
	{
		return;
	}

	if (ReserveRoundsText)
	{
		ReserveRoundsText->SetText(FText::AsNumber(CachedAmmoData.ReserveRounds));
	}

	if (AvailableMagazinesText)
	{
		AvailableMagazinesText->SetText(FText::AsNumber(CachedAmmoData.AvailableMagazines));
	}
}

void USuspenseCoreAmmoCounterWidget::UpdateFireModeUI()
{
	if (FireModeText)
	{
		FireModeText->SetText(CachedAmmoData.FireModeText);
	}
}

void USuspenseCoreAmmoCounterWidget::UpdateAmmoTypeUI()
{
	if (AmmoTypeText)
	{
		AmmoTypeText->SetText(CachedAmmoData.AmmoDisplayName);
	}
}

void USuspenseCoreAmmoCounterWidget::UpdateFillBar(float DeltaTime)
{
	if (!MagazineFillBar)
	{
		return;
	}

	if (FMath::Abs(DisplayedFillPercent - TargetFillPercent) > KINDA_SMALL_NUMBER)
	{
		DisplayedFillPercent = FMath::FInterpTo(
			DisplayedFillPercent,
			TargetFillPercent,
			DeltaTime,
			FillBarInterpSpeed
		);

		MagazineFillBar->SetPercent(DisplayedFillPercent);
	}
}

void USuspenseCoreAmmoCounterWidget::CheckAmmoWarnings()
{
	bool bNewLowAmmo = CachedAmmoData.IsAmmoLow();
	bool bNewCritical = CachedAmmoData.IsAmmoCritical();

	if (bNewLowAmmo != bIsLowAmmo || bNewCritical != bIsCriticalAmmo)
	{
		SetLowAmmoWarning_Implementation(bNewLowAmmo, bNewCritical);
	}
}
