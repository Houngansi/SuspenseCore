// SuspenseCoreAmmoCounterWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.
//
// ARCHITECTURE:
// - EventBus-driven UI updates (push model, NO polling!)
// - Subscribes to weapon switch, magazine, and ammo events
// - Uses native tags from SuspenseCoreEquipmentTags namespace
// - Tarkov-style ammo display with magazine fill bar
//
// KEY EVENTS:
// - TAG_Equipment_Event_WeaponSlot_Switched: Active weapon changed
// - TAG_Equipment_Event_Magazine_*: Magazine operations
// - TAG_Equipment_Event_Weapon_*: Weapon state changes

#include "SuspenseCore/Widgets/HUD/SuspenseCoreAmmoCounterWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeapon.h"
#include "SuspenseCore/Interfaces/Equipment/ISuspenseCoreEquipmentDataProvider.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreItemDataTable.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "SuspenseCore/Components/SuspenseCoreMagazineComponent.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "Engine/Texture2D.h"

DEFINE_LOG_CATEGORY_STATIC(LogAmmoCounterWidget, Log, All);

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
	// Teardown old subscriptions first
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
			}
			else
			{
				// Fallback to actor name
				WeaponNameText->SetText(FText::FromString(WeaponActor->GetName()));
			}
		}
		else
		{
			// Actor doesn't implement ISuspenseCoreWeapon - use actor name
			WeaponNameText->SetText(FText::FromString(WeaponActor->GetName()));
		}
	}

	// CRITICAL FIX: Read initial ammo state from MagazineComponent
	// Without this, CachedAmmoData stays at 0/0 until an EventBus event arrives
	// @see TarkovStyle_Ammo_System_Design.md - WeaponAmmoState persistence
	if (WeaponActor)
	{
		if (USuspenseCoreMagazineComponent* MagComp = WeaponActor->FindComponentByClass<USuspenseCoreMagazineComponent>())
		{
			const FSuspenseCoreWeaponAmmoState& AmmoState = MagComp->GetWeaponAmmoState();

			CachedAmmoData.bHasMagazine = AmmoState.bHasMagazine;
			CachedAmmoData.MagazineRounds = AmmoState.InsertedMagazine.CurrentRoundCount;
			CachedAmmoData.MagazineCapacity = AmmoState.InsertedMagazine.MaxCapacity;
			CachedAmmoData.bHasChamberedRound = AmmoState.ChamberedRound.IsChambered();
			CachedAmmoData.LoadedAmmoType = AmmoState.InsertedMagazine.LoadedAmmoID;

			bHasMagazine = AmmoState.bHasMagazine;
			TargetFillPercent = CachedAmmoData.GetMagazineFillPercent();

			UE_LOG(LogTemp, Log, TEXT("AmmoCounterWidget: Initialized from MagazineComponent - Rounds=%d/%d, Chambered=%s"),
				CachedAmmoData.MagazineRounds,
				CachedAmmoData.MagazineCapacity,
				CachedAmmoData.bHasChamberedRound ? TEXT("Yes") : TEXT("No"));
		}
	}

	// Subscribe to events now that widget is ready
	SetupEventSubscriptions();
	RefreshDisplay();
}

void USuspenseCoreAmmoCounterWidget::ClearWeapon_Implementation()
{
	// Unsubscribe from all events immediately
	TeardownEventSubscriptions();

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
	// Standard UMG visibility is sufficient now that Retainer is removed
	ESlateVisibility NewVisibility = bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	SetVisibility(NewVisibility);
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
	UpdateWeaponUI();      // Weapon name and icon
	UpdateMagazineUI();    // Magazine rounds, capacity, fill bar
	UpdateReserveUI();     // Reserve ammo and available magazines
	UpdateFireModeUI();    // Fire mode (AUTO/SEMI/BURST)
	UpdateAmmoTypeUI();    // Loaded ammo type
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
		UE_LOG(LogAmmoCounterWidget, Warning, TEXT("SetupEventSubscriptions: No EventBus available!"));
		return;
	}

	using namespace SuspenseCoreEquipmentTags::Event;
	using namespace SuspenseCoreEquipmentTags::Magazine;

	// ═══════════════════════════════════════════════════════════════════════════
	// WEAPON SWITCH EVENT - Critical for UI update when active weapon changes
	// This is published by GA_WeaponSwitch ability via EventBus
	// ═══════════════════════════════════════════════════════════════════════════
	ActiveWeaponChangedHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_WeaponSlot_Switched,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreAmmoCounterWidget::OnActiveWeaponChangedEvent),
		ESuspenseCoreEventPriority::High  // High priority - UI should update immediately
	);

	UE_LOG(LogAmmoCounterWidget, Verbose, TEXT("Subscribed to TAG_Equipment_Event_WeaponSlot_Switched"));

	// ═══════════════════════════════════════════════════════════════════════════
	// MAGAZINE EVENTS - Tarkov-style magazine operations
	// ═══════════════════════════════════════════════════════════════════════════
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

	MagazineSwappedHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_Magazine_Swapped,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreAmmoCounterWidget::OnMagazineSwappedEvent),
		ESuspenseCoreEventPriority::Normal
	);

	MagazineRoundsChangedHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_Magazine_RoundsChanged,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreAmmoCounterWidget::OnMagazineRoundsChangedEvent),
		ESuspenseCoreEventPriority::Normal
	);

	// ═══════════════════════════════════════════════════════════════════════════
	// WEAPON STATE EVENTS
	// ═══════════════════════════════════════════════════════════════════════════
	WeaponAmmoChangedHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_Weapon_AmmoChanged,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreAmmoCounterWidget::OnWeaponAmmoChangedEvent),
		ESuspenseCoreEventPriority::Normal
	);

	FireModeChangedHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_Weapon_FireModeChanged,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreAmmoCounterWidget::OnFireModeChangedEvent),
		ESuspenseCoreEventPriority::Normal
	);

	UE_LOG(LogAmmoCounterWidget, Log, TEXT("Event subscriptions setup complete"));
}

void USuspenseCoreAmmoCounterWidget::TeardownEventSubscriptions()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	// Weapon switch event
	EventBus->Unsubscribe(ActiveWeaponChangedHandle);

	// Magazine events
	EventBus->Unsubscribe(MagazineInsertedHandle);
	EventBus->Unsubscribe(MagazineEjectedHandle);
	EventBus->Unsubscribe(MagazineSwappedHandle);
	EventBus->Unsubscribe(MagazineRoundsChangedHandle);

	// Weapon state events
	EventBus->Unsubscribe(WeaponAmmoChangedHandle);
	EventBus->Unsubscribe(FireModeChangedHandle);

	UE_LOG(LogAmmoCounterWidget, Verbose, TEXT("Event subscriptions torn down"));
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
	if (!bIsInitialized)
	{
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
	if (!bIsInitialized)
	{
		return;
	}

	CachedAmmoData.bHasMagazine = false;
	SetNoMagazineState_Implementation(true);
}

void USuspenseCoreAmmoCounterWidget::OnMagazineSwappedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	if (!bIsInitialized)
	{
		return;
	}

	// Magazine swap event contains full data for UI update
	int32 Rounds = EventData.GetInt(TEXT("CurrentRounds"), 0);
	int32 Capacity = EventData.GetInt(TEXT("MaxCapacity"), 30);
	bool bChambered = EventData.GetBool(TEXT("HasChamberedRound"));

	CachedAmmoData.MagazineRounds = Rounds;
	CachedAmmoData.MagazineCapacity = Capacity;
	CachedAmmoData.bHasChamberedRound = bChambered;
	CachedAmmoData.bHasMagazine = true;

	TargetFillPercent = CachedAmmoData.GetMagazineFillPercent();

	SetNoMagazineState_Implementation(false);
	UpdateMagazineUI();
	PlayMagazineSwapAnimation_Implementation();
}

void USuspenseCoreAmmoCounterWidget::OnMagazineRoundsChangedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	if (!bIsInitialized)
	{
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
	if (!bIsInitialized)
	{
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
	if (!bIsInitialized)
	{
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

void USuspenseCoreAmmoCounterWidget::OnActiveWeaponChangedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// This event is fired when player switches active weapon slot via GA_WeaponSwitch
	// We need to update the entire UI with the new weapon's data

	const int32 PreviousSlot = EventData.GetInt(TEXT("PreviousSlot"), INDEX_NONE);
	const int32 NewSlot = EventData.GetInt(TEXT("NewSlot"), INDEX_NONE);

	UE_LOG(LogAmmoCounterWidget, Log, TEXT("OnActiveWeaponChangedEvent: Slot %d → %d"), PreviousSlot, NewSlot);

	// Get the weapon actor from the new slot
	// We need to find the Equipment DataProvider to get the weapon actor
	AActor* NewWeaponActor = nullptr;

	// Try to get weapon actor from event data first (if provided)
	NewWeaponActor = Cast<AActor>(EventData.GetObject(TEXT("WeaponActor")));

	if (!NewWeaponActor)
	{
		// Fallback: Get weapon from DataProvider
		// The Target should be the owning pawn
		AActor* TargetActor = Cast<AActor>(EventData.GetObject(TEXT("Target")));
		if (APawn* Pawn = Cast<APawn>(TargetActor))
		{
			if (APlayerState* PS = Pawn->GetPlayerState())
			{
				// Find DataProvider on PlayerState
				TArray<UActorComponent*> Components;
				PS->GetComponents(Components);

				for (UActorComponent* Comp : Components)
				{
					if (Comp && Comp->GetClass()->ImplementsInterface(USuspenseCoreEquipmentDataProvider::StaticClass()))
					{
						ISuspenseCoreEquipmentDataProvider* Provider = Cast<ISuspenseCoreEquipmentDataProvider>(Comp);
						if (Provider && NewSlot != INDEX_NONE)
						{
							FSuspenseCoreInventoryItemInstance ItemInstance = Provider->GetSlotItem(NewSlot);
							// WeaponActor would be spawned by VisualizationService
							// For now, we get the weapon data from the item instance
							// The actual weapon actor reference needs to come from another source

							UE_LOG(LogAmmoCounterWidget, Verbose, TEXT("Got item from slot %d: %s"),
								NewSlot, *ItemInstance.ItemID.ToString());
						}
						break;
					}
				}
			}
		}
	}

	// If we have a new weapon actor, reinitialize the widget with it
	if (NewWeaponActor)
	{
		// This will teardown old subscriptions and set up new ones for the new weapon
		InitializeWithWeapon_Implementation(NewWeaponActor);
	}
	else
	{
		// No weapon actor available - update UI based on event data
		// This happens when VisualizationService hasn't spawned the weapon yet

		// Reset cached data
		CachedAmmoData = FSuspenseCoreAmmoCounterData();
		bIsInitialized = true;

		// Try to get weapon name from event data
		FString WeaponName = EventData.GetString(TEXT("WeaponName"));
		if (!WeaponName.IsEmpty() && WeaponNameText)
		{
			WeaponNameText->SetText(FText::FromString(WeaponName));
		}

		// Update weapon icon if provided
		UpdateWeaponUI();

		// Refresh all displays
		RefreshDisplay();

		// Fire Blueprint event for custom handling
		OnWeaponChanged(nullptr);
	}

	UE_LOG(LogAmmoCounterWidget, Log, TEXT("Weapon switch UI update complete for slot %d"), NewSlot);
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreAmmoCounterWidget::UpdateWeaponUI()
{
	// Update weapon icon and name based on cached weapon actor
	AActor* WeaponActor = CachedWeaponActor.Get();

	if (!WeaponActor)
	{
		// No weapon - show placeholder or hide
		if (WeaponNameText)
		{
			WeaponNameText->SetText(NSLOCTEXT("AmmoCounter", "NoWeapon", "---"));
		}

		if (WeaponIcon)
		{
			WeaponIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	// Get weapon data from ISuspenseCoreWeapon interface
	if (WeaponActor->Implements<USuspenseCoreWeapon>())
	{
		FSuspenseCoreUnifiedItemData WeaponData;
		if (ISuspenseCoreWeapon::Execute_GetWeaponItemData(WeaponActor, WeaponData))
		{
			// Update weapon name
			if (WeaponNameText)
			{
				WeaponNameText->SetText(WeaponData.DisplayName);
			}

			// Update weapon icon
			if (WeaponIcon)
			{
				WeaponIcon->SetVisibility(ESlateVisibility::HitTestInvisible);

				// Load icon texture if path is valid
				if (!WeaponData.IconTexturePath.IsNull())
				{
					UTexture2D* IconTexture = Cast<UTexture2D>(WeaponData.IconTexturePath.TryLoad());
					if (IconTexture)
					{
						WeaponIcon->SetBrushFromTexture(IconTexture);

						UE_LOG(LogAmmoCounterWidget, Verbose, TEXT("Updated weapon icon: %s"),
							*WeaponData.IconTexturePath.ToString());
					}
				}
			}

			UE_LOG(LogAmmoCounterWidget, Log, TEXT("Updated weapon UI: %s"),
				*WeaponData.DisplayName.ToString());
		}
	}
	else
	{
		// Weapon doesn't implement interface - use actor name as fallback
		if (WeaponNameText)
		{
			WeaponNameText->SetText(FText::FromString(WeaponActor->GetName()));
		}
	}
}

void USuspenseCoreAmmoCounterWidget::UpdateMagazineUI()
{
	// 1. Обработка случая, когда МАГАЗИНА НЕТ
	if (!bHasMagazine)
	{
		// FIX: Явно убираем "999", ставим текст для отсутствующего магазина (например "--" или "0")
		if (MagazineRoundsText)
		{
			MagazineRoundsText->SetText(NoMagazineText);
		}

		// Скрываем вместимость (чтобы не было видно "/999")
		if (MagazineCapacityText)
		{
			MagazineCapacityText->SetVisibility(ESlateVisibility::Collapsed);
		}

		// Скрываем индикатор патрона в патроннике
		if (ChamberIndicatorText)
		{
			ChamberIndicatorText->SetVisibility(ESlateVisibility::Collapsed);
		}

		// Обнуляем и скрываем прогресс бар
		if (MagazineFillBar)
		{
			MagazineFillBar->SetPercent(0.0f);
			MagazineFillBar->SetVisibility(ESlateVisibility::Collapsed);
		}

		// На этом всё, выходим, так как отображать цифры нечего
		return;
	}

	// 2. Обработка случая, когда МАГАЗИН ЕСТЬ
	// Возвращаем видимость элементам, которые могли быть скрыты
	if (MagazineCapacityText)
	{
		MagazineCapacityText->SetVisibility(ESlateVisibility::HitTestInvisible);

		MagazineCapacityText->SetText(FText::Format(
			NSLOCTEXT("AmmoCounter", "CapacityFormat", "/{0}"),
			FText::AsNumber(CachedAmmoData.MagazineCapacity)
		));
	}

	if (MagazineFillBar)
	{
		MagazineFillBar->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	// Обновляем текущее количество патронов
	if (MagazineRoundsText)
	{
		MagazineRoundsText->SetText(FText::AsNumber(CachedAmmoData.MagazineRounds));
	}

	// Обновляем индикатор патрона в патроннике
	if (ChamberIndicatorText && bShowChamberIndicator)
	{
		if (CachedAmmoData.bHasChamberedRound)
		{
			ChamberIndicatorText->SetText(ChamberFormat);
			ChamberIndicatorText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			// Если патронник пуст, скрываем индикатор (или делаем серым, в зависимости от дизайна)
			ChamberIndicatorText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Обновляем полоску (если не используется плавная интерполяция в Tick)
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
