// SuspenseCoreCrosshairWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/HUD/SuspenseCoreCrosshairWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "TimerManager.h"

USuspenseCoreCrosshairWidget::USuspenseCoreCrosshairWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ═══════════════════════════════════════════════════════════════════════════════
// UUserWidget Interface
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreCrosshairWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Initialize spread
	CurrentSpreadRadius = MinimumSpread;
	TargetSpreadRadius = MinimumSpread;
	BaseSpreadRadius = MinimumSpread;

	// Hide hit markers initially
	if (HitMarkerTopLeft) HitMarkerTopLeft->SetVisibility(ESlateVisibility::Collapsed);
	if (HitMarkerTopRight) HitMarkerTopRight->SetVisibility(ESlateVisibility::Collapsed);
	if (HitMarkerBottomLeft) HitMarkerBottomLeft->SetVisibility(ESlateVisibility::Collapsed);
	if (HitMarkerBottomRight) HitMarkerBottomRight->SetVisibility(ESlateVisibility::Collapsed);

	// Update initial positions
	UpdateCrosshairPositions();

	// НЕ подписываемся на события здесь!
	// Подписки создаются только когда прицел становится видимым (SetCrosshairVisibility(true))
}

void USuspenseCoreCrosshairWidget::NativeDestruct()
{
	TeardownEventSubscriptions();

	// Clear timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitMarkerTimerHandle);
	}

	Super::NativeDestruct();
}

void USuspenseCoreCrosshairWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// GUARD: Не обрабатываем Tick если прицел скрыт
	if (!bCrosshairVisible)
	{
		return;
	}

	// Track time since last shot
	TimeSinceLastShot += InDeltaTime;

	// Detect firing state based on cooldown
	bool bWasFiringThisFrame = bCurrentlyFiring;
	bCurrentlyFiring = (TimeSinceLastShot < FireCooldown);

	// Start recovery when firing stops
	if (bWasFiringThisFrame && !bCurrentlyFiring)
	{
		TargetSpreadRadius = BaseSpreadRadius;
	}

	// Select interpolation speed
	float InterpSpeed = bCurrentlyFiring ? SpreadInterpSpeed : RecoveryInterpSpeed;

	// Interpolate current spread toward target
	if (FMath::Abs(CurrentSpreadRadius - TargetSpreadRadius) > KINDA_SMALL_NUMBER)
	{
		CurrentSpreadRadius = FMath::FInterpTo(
			CurrentSpreadRadius,
			TargetSpreadRadius,
			InDeltaTime,
			InterpSpeed
		);

		UpdateCrosshairPositions();
		OnSpreadChanged(CurrentSpreadRadius);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreCrosshairWidget::UpdateCrosshair(float Spread, float Recoil, bool bIsFiring)
{
	bCurrentlyFiring = bIsFiring;

	// Calculate target spread
	float NewSpread = Spread * SpreadMultiplier + Recoil;
	TargetSpreadRadius = FMath::Clamp(NewSpread, MinimumSpread, MaximumSpread);

	if (!bIsFiring)
	{
		TargetSpreadRadius = BaseSpreadRadius;
	}
}

void USuspenseCoreCrosshairWidget::SetCrosshairVisibility(bool bVisible)
{
	// Skip if no state change
	if (bCrosshairVisible == bVisible)
	{
		return;
	}

	// Manage event subscriptions based on visibility
	if (bVisible)
	{
		SetupEventSubscriptions();
	}
	else
	{
		TeardownEventSubscriptions();
		ResetToBaseSpread();
		bCurrentlyFiring = false;
	}

	bCrosshairVisible = bVisible;

	// Standard UMG visibility is sufficient now that Retainer is removed
	ESlateVisibility NewVisibility = bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	SetVisibility(NewVisibility);
}

void USuspenseCoreCrosshairWidget::SetCrosshairType(const FName& CrosshairType)
{
	// Type handling is done via materials in Editor
	// This is just for notification purposes
}

void USuspenseCoreCrosshairWidget::SetMinimumSpread(float MinSpread)
{
	MinimumSpread = MinSpread;
	BaseSpreadRadius = MinSpread;
}

void USuspenseCoreCrosshairWidget::SetMaximumSpread(float MaxSpread)
{
	MaximumSpread = MaxSpread;
}

void USuspenseCoreCrosshairWidget::SetInterpolationSpeed(float Speed)
{
	SpreadInterpSpeed = Speed;
}

void USuspenseCoreCrosshairWidget::ShowHitMarker(bool bHeadshot, bool bKill)
{
	DisplayHitMarker(bHeadshot, bKill);
}

void USuspenseCoreCrosshairWidget::ResetToBaseSpread()
{
	CurrentSpreadRadius = BaseSpreadRadius;
	TargetSpreadRadius = BaseSpreadRadius;
	UpdateCrosshairPositions();
}

// ═══════════════════════════════════════════════════════════════════════════════
// EVENTBUS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreCrosshairWidget::SetupEventSubscriptions()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	using namespace SuspenseCoreEquipmentTags::Event;

	// Subscribe to Equipment tags (legacy/alternative source)
	SpreadUpdatedHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_Weapon_SpreadUpdated,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreCrosshairWidget::OnSpreadUpdatedEvent),
		ESuspenseCoreEventPriority::Normal
	);

	HitConfirmedHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_Visual_Effect,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreCrosshairWidget::OnHitConfirmedEvent),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to GAS weapon events (primary source from fire ability)
	WeaponFiredHandle = EventBus->SubscribeNative(
		SuspenseCoreTags::Event::Weapon::Fired,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreCrosshairWidget::OnWeaponFiredEvent),
		ESuspenseCoreEventPriority::Normal
	);

	SpreadChangedHandle = EventBus->SubscribeNative(
		SuspenseCoreTags::Event::Weapon::SpreadChanged,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreCrosshairWidget::OnSpreadChangedEvent),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to ADS events
	AimStartedHandle = EventBus->SubscribeNative(
		SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Weapon_Stance_AimStarted,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreCrosshairWidget::OnAimStartedEvent),
		ESuspenseCoreEventPriority::Normal
	);

	AimEndedHandle = EventBus->SubscribeNative(
		SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Weapon_Stance_AimEnded,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreCrosshairWidget::OnAimEndedEvent),
		ESuspenseCoreEventPriority::Normal
	);

	UE_LOG(LogTemp, Log, TEXT("CrosshairWidget: Subscribed to weapon and ADS events"));
}

void USuspenseCoreCrosshairWidget::TeardownEventSubscriptions()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	EventBus->Unsubscribe(SpreadUpdatedHandle);
	EventBus->Unsubscribe(WeaponFiredHandle);
	EventBus->Unsubscribe(HitConfirmedHandle);
	EventBus->Unsubscribe(SpreadChangedHandle);
	EventBus->Unsubscribe(AimStartedHandle);
	EventBus->Unsubscribe(AimEndedHandle);
}

USuspenseCoreEventBus* USuspenseCoreCrosshairWidget::GetEventBus() const
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	USuspenseCoreEventManager* EventManager = USuspenseCoreEventManager::Get(this);
	if (EventManager)
	{
		const_cast<USuspenseCoreCrosshairWidget*>(this)->CachedEventBus = EventManager->GetEventBus();
		return CachedEventBus.Get();
	}

	return nullptr;
}

void USuspenseCoreCrosshairWidget::OnSpreadUpdatedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	if (!bCrosshairVisible)
	{
		return;
	}

	float Spread = EventData.GetFloat(TEXT("Spread"), 0.0f);
	float Recoil = EventData.GetFloat(TEXT("Recoil"), 0.0f);
	bool bIsFiring = EventData.GetBool(TEXT("IsFiring"), false);

	UpdateCrosshair(Spread, Recoil, bIsFiring);
}

void USuspenseCoreCrosshairWidget::OnSpreadChangedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	if (!bCrosshairVisible)
	{
		return;
	}

	// GAS publishes spread in degrees - convert to pixels
	float SpreadDegrees = EventData.GetFloat(TEXT("Spread"), 0.0f);

	// Apply aiming multiplier if in ADS
	float EffectiveMultiplier = bIsAiming ? (SpreadMultiplier * AimingSpreadMultiplier) : SpreadMultiplier;

	// Update target spread (with multiplier for visual scaling)
	TargetSpreadRadius = FMath::Clamp(
		SpreadDegrees * EffectiveMultiplier,
		MinimumSpread,
		MaximumSpread
	);

	UE_LOG(LogTemp, Verbose, TEXT("CrosshairWidget: SpreadChanged - Degrees=%.2f, TargetRadius=%.2f"),
		SpreadDegrees, TargetSpreadRadius);
}

void USuspenseCoreCrosshairWidget::OnWeaponFiredEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	if (!bCrosshairVisible)
	{
		return;
	}

	// Reset timer - we're actively firing
	TimeSinceLastShot = 0.0f;
	bCurrentlyFiring = true;

	// Get spread from event (in degrees)
	float SpreadDegrees = EventData.GetFloat(TEXT("Spread"), 0.0f);

	// Add recoil kick for visual feedback
	float RecoilKick = EventData.GetFloat(TEXT("RecoilKick"), 2.0f);

	// Apply aiming multiplier if in ADS
	float EffectiveMultiplier = bIsAiming ? (SpreadMultiplier * AimingSpreadMultiplier) : SpreadMultiplier;

	// Calculate new target spread
	float NewSpread = (SpreadDegrees * EffectiveMultiplier) + RecoilKick;
	TargetSpreadRadius = FMath::Clamp(NewSpread, MinimumSpread, MaximumSpread);

	UE_LOG(LogTemp, Verbose, TEXT("CrosshairWidget: Fired - Spread=%.2f°, Kick=%.2f, Target=%.2fpx, ADS=%s"),
		SpreadDegrees, RecoilKick, TargetSpreadRadius, bIsAiming ? TEXT("YES") : TEXT("NO"));
}

void USuspenseCoreCrosshairWidget::OnHitConfirmedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	if (!bCrosshairVisible)
	{
		return;
	}

	FString EffectType = EventData.GetString(TEXT("EffectType"));

	if (EffectType == TEXT("HitMarker"))
	{
		bool bHeadshot = EventData.GetBool(TEXT("Headshot"), false);
		bool bKill = EventData.GetBool(TEXT("Kill"), false);

		DisplayHitMarker(bHeadshot, bKill);
	}
}

void USuspenseCoreCrosshairWidget::OnAimStartedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	bIsAiming = true;

	UE_LOG(LogTemp, Verbose, TEXT("CrosshairWidget: ADS Started - bHideCrosshairWhenAiming=%s"),
		bHideCrosshairWhenAiming ? TEXT("true") : TEXT("false"));

	if (bHideCrosshairWhenAiming)
	{
		// Hide crosshair for scope-based aiming
		SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		// Tighten crosshair for ADS
		TargetSpreadRadius = BaseSpreadRadius * AimingSpreadMultiplier;
	}
}

void USuspenseCoreCrosshairWidget::OnAimEndedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	bIsAiming = false;

	UE_LOG(LogTemp, Verbose, TEXT("CrosshairWidget: ADS Ended"));

	if (bHideCrosshairWhenAiming)
	{
		// Show crosshair again
		SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	// Return to base spread
	TargetSpreadRadius = BaseSpreadRadius;
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreCrosshairWidget::UpdateCrosshairPositions()
{
	float Radius = CurrentSpreadRadius;

	// Center anchor for all crosshair elements
	const FAnchors CenterAnchor(0.5f, 0.5f);
	const FVector2D CenterAlignment(0.5f, 0.5f);

	// Update top crosshair
	if (TopCrosshair)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(TopCrosshair->Slot))
		{
			CanvasSlot->SetAnchors(CenterAnchor);
			CanvasSlot->SetAlignment(CenterAlignment);
			CanvasSlot->SetPosition(FVector2D(0.0f, -Radius - CrosshairLength / 2.0f));
		}
	}

	// Update bottom crosshair
	if (BottomCrosshair)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(BottomCrosshair->Slot))
		{
			CanvasSlot->SetAnchors(CenterAnchor);
			CanvasSlot->SetAlignment(CenterAlignment);
			CanvasSlot->SetPosition(FVector2D(0.0f, Radius + CrosshairLength / 2.0f));
		}
	}

	// Update left crosshair
	if (LeftCrosshair)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(LeftCrosshair->Slot))
		{
			CanvasSlot->SetAnchors(CenterAnchor);
			CanvasSlot->SetAlignment(CenterAlignment);
			CanvasSlot->SetPosition(FVector2D(-Radius - CrosshairLength / 2.0f, 0.0f));
		}
	}

	// Update right crosshair
	if (RightCrosshair)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(RightCrosshair->Slot))
		{
			CanvasSlot->SetAnchors(CenterAnchor);
			CanvasSlot->SetAlignment(CenterAlignment);
			CanvasSlot->SetPosition(FVector2D(Radius + CrosshairLength / 2.0f, 0.0f));
		}
	}
}

void USuspenseCoreCrosshairWidget::DisplayHitMarker(bool bHeadshot, bool bKill)
{
	// Show all hit marker parts
	ESlateVisibility Visible = ESlateVisibility::HitTestInvisible;

	if (HitMarkerTopLeft) HitMarkerTopLeft->SetVisibility(Visible);
	if (HitMarkerTopRight) HitMarkerTopRight->SetVisibility(Visible);
	if (HitMarkerBottomLeft) HitMarkerBottomLeft->SetVisibility(Visible);
	if (HitMarkerBottomRight) HitMarkerBottomRight->SetVisibility(Visible);

	OnHitMarkerShown(bHeadshot, bKill);

	// Set timer to hide
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitMarkerTimerHandle);
		World->GetTimerManager().SetTimer(
			HitMarkerTimerHandle,
			this,
			&USuspenseCoreCrosshairWidget::HideHitMarker,
			HitMarkerDuration,
			false
		);
	}
}

void USuspenseCoreCrosshairWidget::HideHitMarker()
{
	ESlateVisibility Collapsed = ESlateVisibility::Collapsed;

	if (HitMarkerTopLeft) HitMarkerTopLeft->SetVisibility(Collapsed);
	if (HitMarkerTopRight) HitMarkerTopRight->SetVisibility(Collapsed);
	if (HitMarkerBottomLeft) HitMarkerBottomLeft->SetVisibility(Collapsed);
	if (HitMarkerBottomRight) HitMarkerBottomRight->SetVisibility(Collapsed);

	OnHitMarkerHidden();
}
