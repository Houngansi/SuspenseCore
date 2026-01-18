// SuspenseCoreReloadTimerWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/HUD/SuspenseCoreReloadTimerWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Engine/GameInstance.h"

USuspenseCoreReloadTimerWidget::USuspenseCoreReloadTimerWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ═══════════════════════════════════════════════════════════════════════════════
// UUserWidget Interface
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreReloadTimerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Validate required BindWidget components
	checkf(StatusText, TEXT("USuspenseCoreReloadTimerWidget: StatusText is REQUIRED!"));
	checkf(TimeRemainingText, TEXT("USuspenseCoreReloadTimerWidget: TimeRemainingText is REQUIRED!"));
	checkf(ReloadProgressBar, TEXT("USuspenseCoreReloadTimerWidget: ReloadProgressBar is REQUIRED!"));

	// Start hidden
	SetVisibility(ESlateVisibility::Collapsed);

	// Subscribe to EventBus events
	SetupEventSubscriptions();
}

void USuspenseCoreReloadTimerWidget::NativeDestruct()
{
	TeardownEventSubscriptions();
	Super::NativeDestruct();
}

void USuspenseCoreReloadTimerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bIsReloading)
	{
		return;
	}

	// Update elapsed time
	ElapsedTime += InDeltaTime;

	// Calculate target progress
	if (TotalDuration > 0.0f)
	{
		TargetProgress = FMath::Clamp(ElapsedTime / TotalDuration, 0.0f, 1.0f);
	}

	// Smooth interpolation
	if (bSmoothProgress)
	{
		DisplayedProgress = FMath::FInterpTo(DisplayedProgress, TargetProgress, InDeltaTime, ProgressInterpSpeed);
	}
	else
	{
		DisplayedProgress = TargetProgress;
	}

	// Update UI
	UpdateUI();
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreReloadTimerWidget::ShowReloadTimer(float Duration, ESuspenseCoreReloadType ReloadType, bool bInCanCancel)
{
	TotalDuration = Duration;
	ElapsedTime = 0.0f;
	DisplayedProgress = 0.0f;
	TargetProgress = 0.0f;
	CurrentReloadType = ReloadType;
	bCanCancel = bInCanCancel;
	bIsReloading = true;

	// Update status text
	if (StatusText)
	{
		StatusText->SetText(GetStatusTextForReloadType(ReloadType));
	}

	// Reset progress bar
	if (ReloadProgressBar)
	{
		ReloadProgressBar->SetPercent(0.0f);
	}

	// Show widget
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// Notify Blueprint
	OnTimerStarted(ReloadType);
}

void USuspenseCoreReloadTimerWidget::HideReloadTimer(bool bCompleted)
{
	bIsReloading = false;

	// Hide widget
	SetVisibility(ESlateVisibility::Collapsed);

	// Reset state
	TotalDuration = 0.0f;
	ElapsedTime = 0.0f;
	DisplayedProgress = 0.0f;
	TargetProgress = 0.0f;
	CurrentReloadType = ESuspenseCoreReloadType::None;

	// Notify Blueprint
	if (bCompleted)
	{
		OnTimerCompleted();
	}
	else
	{
		OnTimerCancelled();
	}
}

void USuspenseCoreReloadTimerWidget::UpdateProgress(float Progress, float RemainingTime)
{
	TargetProgress = FMath::Clamp(Progress, 0.0f, 1.0f);

	// Update elapsed time based on remaining
	if (TotalDuration > 0.0f)
	{
		ElapsedTime = TotalDuration - RemainingTime;
	}

	if (!bSmoothProgress)
	{
		DisplayedProgress = TargetProgress;
		UpdateUI();
	}
}

bool USuspenseCoreReloadTimerWidget::IsTimerVisible() const
{
	return bIsReloading && GetVisibility() != ESlateVisibility::Collapsed;
}

// ═══════════════════════════════════════════════════════════════════════════════
// EVENTBUS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreReloadTimerWidget::SetupEventSubscriptions()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	using namespace SuspenseCoreEquipmentTags::Event;

	// Subscribe to reload events
	ReloadStartHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_Weapon_ReloadStart,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreReloadTimerWidget::OnReloadStartEvent),
		ESuspenseCoreEventPriority::Normal
	);

	ReloadEndHandle = EventBus->SubscribeNative(
		TAG_Equipment_Event_Weapon_ReloadEnd,
		this,
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreReloadTimerWidget::OnReloadEndEvent),
		ESuspenseCoreEventPriority::Normal
	);
}

void USuspenseCoreReloadTimerWidget::TeardownEventSubscriptions()
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	EventBus->Unsubscribe(ReloadStartHandle);
	EventBus->Unsubscribe(ReloadEndHandle);
}

USuspenseCoreEventBus* USuspenseCoreReloadTimerWidget::GetEventBus() const
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		return nullptr;
	}

	USuspenseCoreEventManager* EventManager = GI->GetSubsystem<USuspenseCoreEventManager>();
	if (!EventManager)
	{
		return nullptr;
	}

	const_cast<USuspenseCoreReloadTimerWidget*>(this)->CachedEventBus = EventManager->GetEventBus();
	return CachedEventBus.Get();
}

void USuspenseCoreReloadTimerWidget::OnReloadStartEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// Extract reload data
	float Duration = EventData.GetFloat(TEXT("Duration"), 2.0f);
	bool bCanCancelReload = EventData.GetBool(TEXT("CanCancel"), true);

	// Parse reload type from string
	FString ReloadTypeStr = EventData.GetString(TEXT("ReloadType"));
	ESuspenseCoreReloadType ReloadType = ESuspenseCoreReloadType::None;

	if (ReloadTypeStr == TEXT("Tactical"))
	{
		ReloadType = ESuspenseCoreReloadType::Tactical;
	}
	else if (ReloadTypeStr == TEXT("Empty"))
	{
		ReloadType = ESuspenseCoreReloadType::Empty;
	}
	else if (ReloadTypeStr == TEXT("Emergency"))
	{
		ReloadType = ESuspenseCoreReloadType::Emergency;
	}
	else if (ReloadTypeStr == TEXT("ChamberOnly"))
	{
		ReloadType = ESuspenseCoreReloadType::ChamberOnly;
	}

	// Show the timer
	ShowReloadTimer(Duration, ReloadType, bCanCancelReload);
}

void USuspenseCoreReloadTimerWidget::OnReloadEndEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// Check if reload completed or was cancelled
	bool bCompleted = EventData.GetBool(TEXT("Completed"), true);

	// Hide the timer
	HideReloadTimer(bCompleted);
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreReloadTimerWidget::UpdateUI()
{
	// Update progress bar
	if (ReloadProgressBar)
	{
		ReloadProgressBar->SetPercent(DisplayedProgress);
	}

	// Update time remaining text
	if (TimeRemainingText && bShowTimeRemaining)
	{
		float RemainingTime = FMath::Max(0.0f, TotalDuration - ElapsedTime);
		FText TimeText = FText::Format(
			NSLOCTEXT("ReloadTimer", "TimeFormat", "{0}s"),
			FText::AsNumber(RemainingTime, &FNumberFormattingOptions::DefaultNoGrouping().SetMaximumFractionalDigits(1))
		);
		TimeRemainingText->SetText(TimeText);
	}
}

FText USuspenseCoreReloadTimerWidget::GetStatusTextForReloadType(ESuspenseCoreReloadType ReloadType) const
{
	switch (ReloadType)
	{
	case ESuspenseCoreReloadType::Tactical:
		return TacticalReloadText;
	case ESuspenseCoreReloadType::Empty:
		return EmptyReloadText;
	case ESuspenseCoreReloadType::Emergency:
		return EmergencyReloadText;
	case ESuspenseCoreReloadType::ChamberOnly:
		return ChamberOnlyText;
	default:
		return DefaultReloadText;
	}
}
