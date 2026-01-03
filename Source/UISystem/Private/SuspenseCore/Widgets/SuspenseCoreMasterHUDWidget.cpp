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
	UE_LOG(LogTemp, Warning, TEXT("MasterHUD::SetWeaponInfoVisible: bVisible=%d, Frame=%llu"), bVisible, GFrameCounter);

	if (AmmoCounterWidget)
	{
		ESlateVisibility NewVis = bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
		float NewOpacity = bVisible ? 1.0f : 0.0f;
		FVector2D NewScale = bVisible ? FVector2D(1.0f, 1.0f) : FVector2D(0.0f, 0.0f);

		// ═══════════════════════════════════════════════════════════════════════
		// УБИЙСТВЕННАЯ КОМБИНАЦИЯ - все способы сразу!
		// ═══════════════════════════════════════════════════════════════════════

		// 1. Disable widget completely
		AmmoCounterWidget->SetIsEnabled(bVisible);

		// 2. UMG Visibility
		AmmoCounterWidget->SetVisibility(NewVis);

		// 3. Render Opacity
		AmmoCounterWidget->SetRenderOpacity(NewOpacity);

		// 4. Color and Opacity (Alpha = 0)
		AmmoCounterWidget->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, NewOpacity));

		// 5. Render Scale = 0 (делает виджет невидимым независимо от других настроек!)
		AmmoCounterWidget->SetRenderScale(NewScale);

		// 6. Slate Level
		TSharedPtr<SWidget> SlateWidget = AmmoCounterWidget->GetCachedWidget();
		if (SlateWidget.IsValid())
		{
			SlateWidget->SetVisibility(bVisible ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
		}

		// 7. Interface method
		AmmoCounterWidget->Execute_SetAmmoCounterVisible(AmmoCounterWidget, bVisible);

		// 8. Force rebuild
		AmmoCounterWidget->ForceLayoutPrepass();
		AmmoCounterWidget->InvalidateLayoutAndVolatility();

		UE_LOG(LogTemp, Warning, TEXT("  AmmoCounter: Vis=%d, Opacity=%.1f, Scale=(%.1f,%.1f), Enabled=%d"),
			static_cast<int32>(AmmoCounterWidget->GetVisibility()), AmmoCounterWidget->GetRenderOpacity(),
			NewScale.X, NewScale.Y, AmmoCounterWidget->GetIsEnabled());
	}
}

void USuspenseCoreMasterHUDWidget::SetCrosshairVisible(bool bVisible)
{
	UE_LOG(LogTemp, Warning, TEXT("════════════════════════════════════════════════════════════════════"));
	UE_LOG(LogTemp, Warning, TEXT("MasterHUD::SetCrosshairVisible: bVisible=%d, Frame=%llu"), bVisible, GFrameCounter);
	UE_LOG(LogTemp, Warning, TEXT("  CrosshairWidget pointer: %p"), CrosshairWidget.Get());

	if (CrosshairWidget)
	{
		ESlateVisibility NewVis = bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
		float NewOpacity = bVisible ? 1.0f : 0.0f;
		FVector2D NewScale = bVisible ? FVector2D(1.0f, 1.0f) : FVector2D(0.0f, 0.0f);

		UE_LOG(LogTemp, Warning, TEXT("  BEFORE: Vis=%d, Opacity=%.2f, Scale=(%.2f,%.2f)"),
			static_cast<int32>(CrosshairWidget->GetVisibility()),
			CrosshairWidget->GetRenderOpacity(),
			CrosshairWidget->GetRenderTransform().Scale.X,
			CrosshairWidget->GetRenderTransform().Scale.Y);

		// 1. Disable widget completely
		CrosshairWidget->SetIsEnabled(bVisible);

		// 2. UMG Visibility
		CrosshairWidget->SetVisibility(NewVis);

		// 3. Render Opacity
		CrosshairWidget->SetRenderOpacity(NewOpacity);

		// 4. Color and Opacity (Alpha = 0)
		CrosshairWidget->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, NewOpacity));

		// 5. Render Scale = 0
		CrosshairWidget->SetRenderScale(NewScale);

		// 6. Slate Level
		TSharedPtr<SWidget> SlateWidget = CrosshairWidget->GetCachedWidget();
		if (SlateWidget.IsValid())
		{
			SlateWidget->SetVisibility(bVisible ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
		}

		// 7. Internal widget method
		CrosshairWidget->SetCrosshairVisibility(bVisible);

		// 8. Force rebuild
		CrosshairWidget->ForceLayoutPrepass();
		CrosshairWidget->InvalidateLayoutAndVolatility();

		UE_LOG(LogTemp, Warning, TEXT("  AFTER: Vis=%d, Opacity=%.2f, Scale=(%.2f,%.2f), Enabled=%d"),
			static_cast<int32>(CrosshairWidget->GetVisibility()),
			CrosshairWidget->GetRenderOpacity(),
			CrosshairWidget->GetRenderTransform().Scale.X,
			CrosshairWidget->GetRenderTransform().Scale.Y,
			CrosshairWidget->GetIsEnabled());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("  !!! CrosshairWidget is NULL - CANNOT HIDE !!!"));
	}
	UE_LOG(LogTemp, Warning, TEXT("════════════════════════════════════════════════════════════════════"));
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
	UE_LOG(LogTemp, Warning, TEXT("MasterHUD::UpdateWeaponWidgetsVisibility - Frame=%llu, bHasWeaponEquipped=%d, bAutoHideWeaponHUD=%d, bCrosshairRequiresWeapon=%d"),
		GFrameCounter, bHasWeaponEquipped, bAutoHideWeaponHUD, bCrosshairRequiresWeapon);

	// Ammo counter visibility
	if (bAutoHideWeaponHUD)
	{
		UE_LOG(LogTemp, Warning, TEXT("  -> Calling SetWeaponInfoVisible(%d)"), bHasWeaponEquipped);
		SetWeaponInfoVisible(bHasWeaponEquipped);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("  -> SKIPPING SetWeaponInfoVisible because bAutoHideWeaponHUD=false!"));
	}

	// Crosshair visibility
	if (bCrosshairRequiresWeapon)
	{
		UE_LOG(LogTemp, Warning, TEXT("  -> Calling SetCrosshairVisible(%d)"), bHasWeaponEquipped);
		SetCrosshairVisible(bHasWeaponEquipped);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("  -> SKIPPING SetCrosshairVisible because bCrosshairRequiresWeapon=false!"));
	}
}
