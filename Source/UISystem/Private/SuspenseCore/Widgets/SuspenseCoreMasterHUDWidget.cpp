// SuspenseCoreMasterHUDWidget.cpp
// SuspenseCore - Master HUD Widget
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/SuspenseCoreMasterHUDWidget.h"
#include "SuspenseCore/Widgets/SuspenseCoreGameHUDWidget.h"
#include "SuspenseCore/Widgets/HUD/SuspenseCoreAmmoCounterWidget.h"
#include "SuspenseCore/Widgets/HUD/SuspenseCoreCrosshairWidget.h"
#include "SuspenseCore/Widgets/HUD/SuspenseCoreQuickSlotHUDWidget.h"
#include "SuspenseCore/Widgets/HUD/SuspenseCoreReloadProgressWidget.h"
#include "Components/CanvasPanel.h"
#include "Widgets/SWidget.h"

USuspenseCoreMasterHUDWidget::USuspenseCoreMasterHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ═══════════════════════════════════════════════════════════════════════════════
// UUserWidget Interface
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreMasterHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UE_LOG(LogTemp, Warning, TEXT("════════════════════════════════════════════════════════════"));
	UE_LOG(LogTemp, Warning, TEXT("MasterHUD::NativeConstruct - Widget: %s (%p)"), *GetName(), this);
	UE_LOG(LogTemp, Warning, TEXT("  CrosshairWidget: %s (%p)"),
		CrosshairWidget ? *CrosshairWidget->GetName() : TEXT("NULL"),
		CrosshairWidget.Get());
	UE_LOG(LogTemp, Warning, TEXT("  AmmoCounterWidget: %s (%p)"),
		AmmoCounterWidget ? *AmmoCounterWidget->GetName() : TEXT("NULL"),
		AmmoCounterWidget.Get());
	UE_LOG(LogTemp, Warning, TEXT("════════════════════════════════════════════════════════════"));

	// Apply initial visibility based on configuration
	ApplyInitialVisibility();
}

void USuspenseCoreMasterHUDWidget::NativeDestruct()
{
	CachedOwningPawn.Reset();
	CachedWeaponActor.Reset();

	Super::NativeDestruct();
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreMasterHUDWidget::InitializeHUD(APawn* OwningPawn)
{
	CachedOwningPawn = OwningPawn;
	bIsInitialized = true;

	// Sub-widgets auto-initialize via EventBus subscriptions in their NativeConstruct
	// No additional initialization needed here

	OnHUDInitialized();
}

void USuspenseCoreMasterHUDWidget::InitializeWeaponHUD(AActor* WeaponActor)
{
	UE_LOG(LogTemp, Warning, TEXT("▶▶▶ MasterHUD::InitializeWeaponHUD - Frame=%llu, WeaponActor=%s, CachedActor=%s"),
		GFrameCounter,
		WeaponActor ? *WeaponActor->GetName() : TEXT("nullptr"),
		CachedWeaponActor.IsValid() ? *CachedWeaponActor->GetName() : TEXT("nullptr"));

	// Guard against duplicate initialization for same weapon
	if (CachedWeaponActor.Get() == WeaponActor && bHasWeaponEquipped)
	{
		UE_LOG(LogTemp, Warning, TEXT("  SKIPPING - Already initialized with this weapon"));
		return;
	}

	CachedWeaponActor = WeaponActor;
	bHasWeaponEquipped = WeaponActor != nullptr;

	// Initialize ammo counter with weapon (has interface)
	if (AmmoCounterWidget && WeaponActor)
	{
		UE_LOG(LogTemp, Log, TEXT("MasterHUD::InitializeWeaponHUD - Initializing AmmoCounterWidget"));
		AmmoCounterWidget->Execute_InitializeWithWeapon(AmmoCounterWidget, WeaponActor);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("MasterHUD::InitializeWeaponHUD - AmmoCounterWidget is %s, WeaponActor is %s"),
			AmmoCounterWidget ? TEXT("valid") : TEXT("NULL"),
			WeaponActor ? TEXT("valid") : TEXT("NULL"));
	}

	// Crosshair doesn't need weapon initialization - it subscribes to EventBus
	// Just update visibility based on weapon state
	UE_LOG(LogTemp, Log, TEXT("MasterHUD::InitializeWeaponHUD - CrosshairWidget is %s"),
		CrosshairWidget ? TEXT("valid") : TEXT("NULL"));

	UpdateWeaponWidgetsVisibility();

	if (WeaponActor)
	{
		OnWeaponHUDInitialized(WeaponActor);
	}
}

void USuspenseCoreMasterHUDWidget::ClearWeaponHUD()
{
	UE_LOG(LogTemp, Warning, TEXT("◀◀◀ MasterHUD::ClearWeaponHUD - Frame=%llu"), GFrameCounter);

	CachedWeaponActor.Reset();
	bHasWeaponEquipped = false;

	// Clear ammo counter (has interface)
	if (AmmoCounterWidget)
	{
		UE_LOG(LogTemp, Log, TEXT("MasterHUD::ClearWeaponHUD - Clearing AmmoCounterWidget"));
		AmmoCounterWidget->Execute_ClearWeapon(AmmoCounterWidget);
	}

	// Update visibility
	UE_LOG(LogTemp, Log, TEXT("MasterHUD::ClearWeaponHUD - Updating visibility, bHasWeaponEquipped: %d"), bHasWeaponEquipped);
	UpdateWeaponWidgetsVisibility();

	OnWeaponHUDCleared();
}

void USuspenseCoreMasterHUDWidget::SetHUDVisible(bool bVisible)
{
	SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	OnHUDVisibilityChanged(bVisible);
}

bool USuspenseCoreMasterHUDWidget::IsHUDVisible() const
{
	return GetVisibility() != ESlateVisibility::Collapsed && GetVisibility() != ESlateVisibility::Hidden;
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION VISIBILITY
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreMasterHUDWidget::SetVitalsVisible(bool bVisible)
{
	if (VitalsWidget)
	{
		VitalsWidget->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void USuspenseCoreMasterHUDWidget::SetWeaponInfoVisible(bool bVisible)
{
	UE_LOG(LogTemp, Warning, TEXT("MasterHUD::SetWeaponInfoVisible: bVisible=%d, Frame=%llu, InViewport=%d, AmmoWidget=%s"),
		bVisible, GFrameCounter, IsInViewport(),
		AmmoCounterWidget ? TEXT("VALID") : TEXT("NULL!!!"));

	if (AmmoCounterWidget)
	{
		ESlateVisibility CurrentVis = AmmoCounterWidget->GetVisibility();
		ESlateVisibility NewVis = bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
		UE_LOG(LogTemp, Warning, TEXT("  AmmoCounter (%p): Vis %d -> %d, Parent=%s"),
			AmmoCounterWidget.Get(), static_cast<int32>(CurrentVis), static_cast<int32>(NewVis),
			AmmoCounterWidget->GetParent() ? *AmmoCounterWidget->GetParent()->GetName() : TEXT("NULL"));

		// 1. UMG Level: Set visibility on the UWidget
		AmmoCounterWidget->SetVisibility(NewVis);
		AmmoCounterWidget->SetRenderOpacity(bVisible ? 1.0f : 0.0f);

		// 2. Slate Level: Set visibility directly on underlying SWidget
		TSharedPtr<SWidget> SlateWidget = AmmoCounterWidget->GetCachedWidget();
		if (SlateWidget.IsValid())
		{
			SlateWidget->SetVisibility(bVisible ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
			UE_LOG(LogTemp, Warning, TEXT("  Slate widget visibility set (bVisible=%d)"), bVisible);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("  GetCachedWidget() returned NULL!"));
		}

		// 3. Interface: Call interface method to set visibility on internal elements
		AmmoCounterWidget->Execute_SetAmmoCounterVisible(AmmoCounterWidget, bVisible);

		// 4. Force layout rebuild
		AmmoCounterWidget->ForceLayoutPrepass();
		AmmoCounterWidget->InvalidateLayoutAndVolatility();

		UE_LOG(LogTemp, Warning, TEXT("  Applied: UMG Vis=%d, Opacity=%.1f"),
			static_cast<int32>(AmmoCounterWidget->GetVisibility()), AmmoCounterWidget->GetRenderOpacity());
	}
}

void USuspenseCoreMasterHUDWidget::SetCrosshairVisible(bool bVisible)
{
	UE_LOG(LogTemp, Warning, TEXT("MasterHUD::SetCrosshairVisible: bVisible=%d, Frame=%llu, InViewport=%d, CrosshairWidget=%s"),
		bVisible, GFrameCounter, IsInViewport(),
		CrosshairWidget ? TEXT("VALID") : TEXT("NULL!!!"));

	if (CrosshairWidget)
	{
		ESlateVisibility CurrentVis = CrosshairWidget->GetVisibility();
		ESlateVisibility NewVis = bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
		UE_LOG(LogTemp, Warning, TEXT("  Crosshair (%p): Vis %d -> %d, Parent=%s"),
			CrosshairWidget.Get(), static_cast<int32>(CurrentVis), static_cast<int32>(NewVis),
			CrosshairWidget->GetParent() ? *CrosshairWidget->GetParent()->GetName() : TEXT("NULL"));

		// 1. UMG Level: Set visibility on the UWidget
		CrosshairWidget->SetVisibility(NewVis);
		CrosshairWidget->SetRenderOpacity(bVisible ? 1.0f : 0.0f);

		// 2. Slate Level: Set visibility directly on underlying SWidget
		TSharedPtr<SWidget> SlateWidget = CrosshairWidget->GetCachedWidget();
		if (SlateWidget.IsValid())
		{
			SlateWidget->SetVisibility(bVisible ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
			UE_LOG(LogTemp, Warning, TEXT("  Slate widget visibility set (bVisible=%d)"), bVisible);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("  GetCachedWidget() returned NULL!"));
		}

		// 3. Internal elements: Set visibility on CenterDot, directional crosshairs, etc.
		CrosshairWidget->SetCrosshairVisibility(bVisible);

		// 4. Force layout rebuild
		CrosshairWidget->ForceLayoutPrepass();
		CrosshairWidget->InvalidateLayoutAndVolatility();

		UE_LOG(LogTemp, Warning, TEXT("  Applied: UMG Vis=%d, Opacity=%.1f"),
			static_cast<int32>(CrosshairWidget->GetVisibility()), CrosshairWidget->GetRenderOpacity());
	}
}

void USuspenseCoreMasterHUDWidget::SetQuickSlotsVisible(bool bVisible)
{
	if (QuickSlotsWidget)
	{
		QuickSlotsWidget->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void USuspenseCoreMasterHUDWidget::SetReloadProgressVisible(bool bVisible)
{
	if (ReloadProgressWidget)
	{
		ReloadProgressWidget->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL METHODS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreMasterHUDWidget::ApplyInitialVisibility()
{
	// Vitals always visible by default
	SetVitalsVisible(true);

	// Quick slots always visible by default
	SetQuickSlotsVisible(true);

	// Weapon-related widgets depend on configuration
	UpdateWeaponWidgetsVisibility();

	// Reload progress hidden by default (shown during reload)
	if (bAutoHideReloadProgress)
	{
		SetReloadProgressVisible(false);
	}
}

void USuspenseCoreMasterHUDWidget::UpdateWeaponWidgetsVisibility()
{
	UE_LOG(LogTemp, Warning, TEXT("MasterHUD::UpdateWeaponWidgetsVisibility - Frame=%llu, bHasWeaponEquipped=%d"),
		GFrameCounter, bHasWeaponEquipped);

	// Ammo counter visibility
	if (bAutoHideWeaponHUD)
	{
		SetWeaponInfoVisible(bHasWeaponEquipped);
	}

	// Crosshair visibility
	if (bCrosshairRequiresWeapon)
	{
		SetCrosshairVisible(bHasWeaponEquipped);
	}
}
