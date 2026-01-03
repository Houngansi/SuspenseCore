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
	CachedWeaponActor = WeaponActor;
	bHasWeaponEquipped = WeaponActor != nullptr;

	// Initialize ammo counter with weapon
	if (AmmoCounterWidget && WeaponActor)
	{
		AmmoCounterWidget->Execute_InitializeWithWeapon(AmmoCounterWidget, WeaponActor);
	}

	// Initialize crosshair with weapon
	if (CrosshairWidget && WeaponActor)
	{
		CrosshairWidget->Execute_InitializeWithWeapon(CrosshairWidget, WeaponActor);
	}

	// Update visibility based on weapon state
	UpdateWeaponWidgetsVisibility();

	if (WeaponActor)
	{
		OnWeaponHUDInitialized(WeaponActor);
	}
}

void USuspenseCoreMasterHUDWidget::ClearWeaponHUD()
{
	CachedWeaponActor.Reset();
	bHasWeaponEquipped = false;

	// Clear ammo counter
	if (AmmoCounterWidget)
	{
		AmmoCounterWidget->Execute_ClearWeapon(AmmoCounterWidget);
	}

	// Clear crosshair
	if (CrosshairWidget)
	{
		CrosshairWidget->Execute_ClearWeapon(CrosshairWidget);
	}

	// Update visibility
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
	if (AmmoCounterWidget)
	{
		AmmoCounterWidget->Execute_SetAmmoCounterVisible(AmmoCounterWidget, bVisible);
	}
}

void USuspenseCoreMasterHUDWidget::SetCrosshairVisible(bool bVisible)
{
	if (CrosshairWidget)
	{
		CrosshairWidget->Execute_SetCrosshairVisible(CrosshairWidget, bVisible);
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
		ReloadProgressWidget->Execute_SetReloadProgressVisible(ReloadProgressWidget, bVisible);
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
