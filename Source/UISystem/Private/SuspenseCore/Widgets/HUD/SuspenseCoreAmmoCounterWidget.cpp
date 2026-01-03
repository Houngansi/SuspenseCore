// SuspenseCoreAmmoCounterWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/HUD/SuspenseCoreAmmoCounterWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeapon.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreItemDataTable.h"
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
	// НЕ подписываемся на события здесь!
	// Подписки создаются только в InitializeWithWeapon когда реально есть оружие
}

void USuspenseCoreAmmoCounterWidget::NativeDestruct()
{
	TeardownEventSubscriptions();
	Super::NativeDestruct();
}

void USuspenseCoreAmmoCounterWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// GUARD: Не обрабатываем Tick если виджет не инициализирован (оружия нет)
	if (!bIsInitialized)
	{
		return;
	}

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
	UE_LOG(LogTemp, Warning, TEXT("[AmmoCounter] InitializeWithWeapon: WeaponActor=%s, Frame=%llu"),
		WeaponActor ? *WeaponActor->GetName() : TEXT("NULL"), GFrameCounter);

	// Сначала отписываемся от старых подписок (если были)
	TeardownEventSubscriptions();

	CachedWeaponActor = WeaponActor;
	bIsInitialized = true;
	bHasMagazine = true;

	// Reset display
	CachedAmmoData = FSuspenseCoreAmmoCounterData();

	// Get weapon display name from ISuspenseCoreWeapon interface
	if (WeaponActor && WeaponNameText)
	{
		if (WeaponActor->Implements<USuspenseCoreWeapon>())
		{
			FSuspenseCoreUnifiedItemData WeaponData;
			if (ISuspenseCoreWeapon::Execute_GetWeaponItemData(WeaponActor, WeaponData))
			{
				WeaponNameText->SetText(WeaponData.DisplayName);
				UE_LOG(LogTemp, Log, TEXT("AmmoCounter: Weapon DisplayName set to: %s"), *WeaponData.DisplayName.ToString());
			}
			else
			{
				// Fallback to actor name
				WeaponNameText->SetText(FText::FromString(WeaponActor->GetName()));
				UE_LOG(LogTemp, Warning, TEXT("AmmoCounter: GetWeaponItemData failed, using actor name"));
			}
		}
		else
		{
			// Actor doesn't implement ISuspenseCoreWeapon - use actor name
			WeaponNameText->SetText(FText::FromString(WeaponActor->GetName()));
			UE_LOG(LogTemp, Warning, TEXT("AmmoCounter: Weapon doesn't implement ISuspenseCoreWeapon"));
		}
	}

	// ТЕПЕРЬ подписываемся на события - виджет готов слушать
	SetupEventSubscriptions();

	RefreshDisplay();

	UE_LOG(LogTemp, Warning, TEXT("[AmmoCounter] InitializeWithWeapon DONE - subscribed to events"));
}

void USuspenseCoreAmmoCounterWidget::ClearWeapon_Implementation()
{
	UE_LOG(LogTemp, Warning, TEXT("[AmmoCounter] ClearWeapon: Frame=%llu - UNSUBSCRIBING from events"), GFrameCounter);

	// СРАЗУ отписываемся от всех событий - виджет больше не должен реагировать
	TeardownEventSubscriptions();

	CachedWeaponActor = nullptr;
	bIsInitialized = false;

	// Clear display
	CachedAmmoData = FSuspenseCoreAmmoCounterData();
	SetNoMagazineState_Implementation(true);

	UE_LOG(LogTemp, Warning, TEXT("[AmmoCounter] ClearWeapon DONE - unsubscribed, bIsInitialized=false"));
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
	UE_LOG(LogTemp, Warning, TEXT("[AmmoCounter] SetAmmoCounterVisible: bVisible=%d, Frame=%llu, CurrentVis=%d"),
		bVisible, GFrameCounter, static_cast<int32>(GetVisibility()));

	ESlateVisibility NewVisibility = bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	float NewOpacity = bVisible ? 1.0f : 0.0f;

	// УРОВЕНЬ 1: Visibility
	SetVisibility(NewVisibility);

	// УРОВЕНЬ 2: Render Opacity
	SetRenderOpacity(NewOpacity);

	// УРОВЕНЬ 3: Color and Opacity (Alpha = 0 делает виджет полностью прозрачным)
	SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, NewOpacity));

	// Force layout refresh
	ForceLayoutPrepass();

	UE_LOG(LogTemp, Warning, TEXT("[AmmoCounter] Applied: Visibility=%d, Opacity=%.1f, ColorAlpha=%.1f"),
		static_cast<int32>(GetVisibility()), GetRenderOpacity(), NewOpacity);
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

float USuspenseCoreAmmoCounterWidget::GetAmmoPercentage() const
{
	return CachedAmmoData.GetMagazineFillPercent();
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
	using namespace SuspenseCoreEquipmentTags::Magazine;

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
	// GUARD: Игнорируем события если виджет не инициализирован
	if (!bIsInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AmmoCounter] OnMagazineInsertedEvent IGNORED - not initialized"));
		return;
	}

	int32 Rounds = EventData.GetInt(TEXT("CurrentRounds"), 0);
	int32 Capacity = EventData.GetInt(TEXT("MaxCapacity"), 30);
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
	// GUARD: Игнорируем события если виджет не инициализирован
	if (!bIsInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AmmoCounter] OnMagazineEjectedEvent IGNORED - not initialized"));
		return;
	}

	CachedAmmoData.bHasMagazine = false;
	SetNoMagazineState_Implementation(true);
}

void USuspenseCoreAmmoCounterWidget::OnMagazineRoundsChangedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// GUARD: Игнорируем события если виджет не инициализирован
	if (!bIsInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AmmoCounter] OnMagazineRoundsChangedEvent IGNORED - not initialized"));
		return;
	}

	int32 CurrentRounds = EventData.GetInt(TEXT("CurrentRounds"), 0);
	int32 MaxRounds = EventData.GetInt(TEXT("MaxCapacity"), CachedAmmoData.MagazineCapacity);

	CachedAmmoData.MagazineRounds = CurrentRounds;
	CachedAmmoData.MagazineCapacity = MaxRounds;

	TargetFillPercent = CachedAmmoData.GetMagazineFillPercent();

	UpdateMagazineUI();
	CheckAmmoWarnings();
}

void USuspenseCoreAmmoCounterWidget::OnWeaponAmmoChangedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// GUARD: Игнорируем события если виджет не инициализирован
	if (!bIsInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AmmoCounter] OnWeaponAmmoChangedEvent IGNORED - not initialized"));
		return;
	}

	// This is fired when ammo is consumed (shot fired)
	int32 CurrentRounds = EventData.GetInt(TEXT("CurrentRounds"), CachedAmmoData.MagazineRounds);
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
	// GUARD: Игнорируем события если виджет не инициализирован
	if (!bIsInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AmmoCounter] OnFireModeChangedEvent IGNORED - not initialized"));
		return;
	}

	FString FireModeStr = EventData.GetString(TEXT("FireMode"));

	// Get fire mode tag from string (GetTag doesn't exist, use FGameplayTag::RequestGameplayTag)
	FString FireModeTagStr = EventData.GetString(TEXT("FireModeTag"));
	FGameplayTag FireModeTag = FireModeTagStr.IsEmpty()
		? FGameplayTag()
		: FGameplayTag::RequestGameplayTag(FName(*FireModeTagStr), false);

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
