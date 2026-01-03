// SuspenseCoreCrosshairWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/HUD/SuspenseCoreCrosshairWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
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

	SetupEventSubscriptions();
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

	// Interpolate spread
	float InterpSpeed = bCurrentlyFiring ? SpreadInterpSpeed : RecoveryInterpSpeed;

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

	bWasFiring = bCurrentlyFiring;
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
	bCrosshairVisible = bVisible;

	ESlateVisibility NewVisibility = bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;

	UE_LOG(LogTemp, Warning, TEXT("[Crosshair] SetCrosshairVisibility: bVisible=%d, CenterDot=%s, TopCrosshair=%s"),
		bVisible,
		CenterDot ? TEXT("valid") : TEXT("NULL"),
		TopCrosshair ? TEXT("valid") : TEXT("NULL"));

	if (CenterDot) CenterDot->SetVisibility(NewVisibility);
	if (TopCrosshair) TopCrosshair->SetVisibility(NewVisibility);
	if (BottomCrosshair) BottomCrosshair->SetVisibility(NewVisibility);
	if (LeftCrosshair) LeftCrosshair->SetVisibility(NewVisibility);
	if (RightCrosshair) RightCrosshair->SetVisibility(NewVisibility);

	UE_LOG(LogTemp, Warning, TEXT("[Crosshair] Visibility SET to: %s"),
		bVisible ? TEXT("HitTestInvisible") : TEXT("Collapsed"));
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

	SpreadUpdatedHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_Weapon_SpreadUpdated,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreCrosshairWidget::OnSpreadUpdatedEvent),
		ESuspenseCoreEventPriority::Normal
	);

	WeaponFiredHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_Weapon_Fired,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreCrosshairWidget::OnWeaponFiredEvent),
		ESuspenseCoreEventPriority::Normal
	);

	HitConfirmedHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_Visual_Effect,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreCrosshairWidget::OnHitConfirmedEvent),
		ESuspenseCoreEventPriority::Normal
	);
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
	float Spread = EventData.GetFloat(TEXT("Spread"), 0.0f);
	float Recoil = EventData.GetFloat(TEXT("Recoil"), 0.0f);
	bool bIsFiring = EventData.GetBool(TEXT("IsFiring"), false);

	UpdateCrosshair(Spread, Recoil, bIsFiring);
}

void USuspenseCoreCrosshairWidget::OnWeaponFiredEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	bCurrentlyFiring = true;

	// Add recoil kick
	float RecoilKick = EventData.GetFloat(TEXT("RecoilKick"), 5.0f);
	TargetSpreadRadius = FMath::Min(CurrentSpreadRadius + RecoilKick, MaximumSpread);
}

void USuspenseCoreCrosshairWidget::OnHitConfirmedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	FString EffectType = EventData.GetString(TEXT("EffectType"));

	if (EffectType == TEXT("HitMarker"))
	{
		bool bHeadshot = EventData.GetBool(TEXT("Headshot"), false);
		bool bKill = EventData.GetBool(TEXT("Kill"), false);

		DisplayHitMarker(bHeadshot, bKill);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreCrosshairWidget::UpdateCrosshairPositions()
{
	float Radius = CurrentSpreadRadius;

	// Update top crosshair
	if (TopCrosshair)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(TopCrosshair->Slot))
		{
			CanvasSlot->SetPosition(FVector2D(0.0f, -Radius - CrosshairLength / 2.0f));
		}
	}

	// Update bottom crosshair
	if (BottomCrosshair)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(BottomCrosshair->Slot))
		{
			CanvasSlot->SetPosition(FVector2D(0.0f, Radius + CrosshairLength / 2.0f));
		}
	}

	// Update left crosshair
	if (LeftCrosshair)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(LeftCrosshair->Slot))
		{
			CanvasSlot->SetPosition(FVector2D(-Radius - CrosshairLength / 2.0f, 0.0f));
		}
	}

	// Update right crosshair
	if (RightCrosshair)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(RightCrosshair->Slot))
		{
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
