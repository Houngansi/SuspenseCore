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

	UE_LOG(LogTemp, Log, TEXT("MasterHUD::NativeConstruct - Checking bound widgets:"));
	UE_LOG(LogTemp, Log, TEXT("  VitalsWidget: %s"), VitalsWidget ? TEXT("BOUND") : TEXT("NULL"));
	UE_LOG(LogTemp, Log, TEXT("  AmmoCounterWidget: %s"), AmmoCounterWidget ? TEXT("BOUND") : TEXT("NULL"));
	UE_LOG(LogTemp, Log, TEXT("  CrosshairWidget: %s"), CrosshairWidget ? TEXT("BOUND") : TEXT("NULL"));
	UE_LOG(LogTemp, Log, TEXT("  QuickSlotsWidget: %s"), QuickSlotsWidget ? TEXT("BOUND") : TEXT("NULL"));
	UE_LOG(LogTemp, Log, TEXT("  ReloadProgressWidget: %s"), ReloadProgressWidget ? TEXT("BOUND") : TEXT("NULL"));

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

	UE_LOG(LogTemp, Log, TEXT("MasterHUD::InitializeWeaponHUD - WeaponActor: %s, bHasWeaponEquipped: %d"),
		WeaponActor ? *WeaponActor->GetName() : TEXT("nullptr"), bHasWeaponEquipped);

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
	CachedWeaponActor.Reset();
	bHasWeaponEquipped = false;

	// Clear ammo counter (has interface)
	if (AmmoCounterWidget)
	{
		AmmoCounterWidget->Execute_ClearWeapon(AmmoCounterWidget);
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
		CrosshairWidget->SetCrosshairVisibility(bVisible);
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
	UE_LOG(LogTemp, Log, TEXT("MasterHUD::UpdateWeaponWidgetsVisibility - bHasWeaponEquipped: %d, bAutoHideWeaponHUD: %d, bCrosshairRequiresWeapon: %d"),
		bHasWeaponEquipped, bAutoHideWeaponHUD, bCrosshairRequiresWeapon);

	// Ammo counter visibility
	if (bAutoHideWeaponHUD)
	{
		UE_LOG(LogTemp, Log, TEXT("  Setting WeaponInfo visible: %d"), bHasWeaponEquipped);
		SetWeaponInfoVisible(bHasWeaponEquipped);
	}

	// Crosshair visibility
	if (bCrosshairRequiresWeapon)
	{
		UE_LOG(LogTemp, Log, TEXT("  Setting Crosshair visible: %d"), bHasWeaponEquipped);
		SetCrosshairVisible(bHasWeaponEquipped);
	}
}
