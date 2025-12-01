// SuspenseCoreLevelWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/SuspenseCoreLevelWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

USuspenseCoreLevelWidget::USuspenseCoreLevelWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USuspenseCoreLevelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Setup EventBus subscriptions
	SetupEventSubscriptions();

	// Initial UI update
	UpdateLevelUI();
	UpdateExperienceUI();
}

void USuspenseCoreLevelWidget::NativeDestruct()
{
	TeardownEventSubscriptions();
	Super::NativeDestruct();
}

void USuspenseCoreLevelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Smooth progress bar interpolation
	if (bSmoothProgressBar)
	{
		UpdateProgressBar(InDeltaTime);
	}
}

// ═══════════════════════════════════════════════════════════════════════════
// EVENTBUS SUBSCRIPTIONS
// ═══════════════════════════════════════════════════════════════════════════

void USuspenseCoreLevelWidget::SetupEventSubscriptions()
{
	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GetWorld());
	if (!Manager)
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreLevelWidget: EventManager not found"));
		return;
	}

	CachedEventBus = Manager->GetEventBus();
	if (!CachedEventBus.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreLevelWidget: EventBus not found"));
		return;
	}

	// Subscribe to Level change events
	LevelEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.LevelChanged")),
		const_cast<USuspenseCoreLevelWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreLevelWidget::OnLevelEvent),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to Experience change events
	ExperienceEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Progression.Experience.Changed")),
		const_cast<USuspenseCoreLevelWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreLevelWidget::OnExperienceEvent),
		ESuspenseCoreEventPriority::Normal
	);

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreLevelWidget: EventBus subscriptions setup complete"));
}

void USuspenseCoreLevelWidget::TeardownEventSubscriptions()
{
	if (!CachedEventBus.IsValid())
	{
		return;
	}

	if (LevelEventHandle.IsValid())
	{
		CachedEventBus->Unsubscribe(LevelEventHandle);
	}
	if (ExperienceEventHandle.IsValid())
	{
		CachedEventBus->Unsubscribe(ExperienceEventHandle);
	}
}

// ═══════════════════════════════════════════════════════════════════════════
// EVENTBUS HANDLERS
// ═══════════════════════════════════════════════════════════════════════════

void USuspenseCoreLevelWidget::OnLevelEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	int32 OldLevel = CachedLevel;
	CachedLevel = static_cast<int32>(EventData.GetFloat(FName("Level"), static_cast<float>(CachedLevel)));

	UpdateLevelUI();
	OnLevelChanged(CachedLevel, OldLevel);

	// Check for level up
	if (CachedLevel > OldLevel)
	{
		OnLevelUp(CachedLevel);
	}
}

void USuspenseCoreLevelWidget::OnExperienceEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	CachedCurrentExp = static_cast<int64>(EventData.GetFloat(FName("CurrentExp"), static_cast<float>(CachedCurrentExp)));
	CachedMaxExp = static_cast<int64>(EventData.GetFloat(FName("MaxExp"), static_cast<float>(CachedMaxExp)));

	TargetExpPercent = (CachedMaxExp > 0) ? (static_cast<float>(CachedCurrentExp) / static_cast<float>(CachedMaxExp)) : 0.0f;
	UpdateExperienceUI();

	OnExperienceChanged(CachedCurrentExp, CachedMaxExp, TargetExpPercent);
}

// ═══════════════════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════

void USuspenseCoreLevelWidget::SetLevel(int32 NewLevel)
{
	int32 OldLevel = CachedLevel;
	CachedLevel = NewLevel;
	UpdateLevelUI();
	OnLevelChanged(CachedLevel, OldLevel);
}

void USuspenseCoreLevelWidget::SetExperience(int64 CurrentExp, int64 MaxExp)
{
	CachedCurrentExp = CurrentExp;
	CachedMaxExp = MaxExp;
	TargetExpPercent = (CachedMaxExp > 0) ? (static_cast<float>(CachedCurrentExp) / static_cast<float>(CachedMaxExp)) : 0.0f;
	UpdateExperienceUI();
	OnExperienceChanged(CachedCurrentExp, CachedMaxExp, TargetExpPercent);
}

void USuspenseCoreLevelWidget::SetLevelAndExperience(int32 NewLevel, int64 CurrentExp, int64 MaxExp)
{
	int32 OldLevel = CachedLevel;
	CachedLevel = NewLevel;
	CachedCurrentExp = CurrentExp;
	CachedMaxExp = MaxExp;
	TargetExpPercent = (CachedMaxExp > 0) ? (static_cast<float>(CachedCurrentExp) / static_cast<float>(CachedMaxExp)) : 0.0f;

	UpdateLevelUI();
	UpdateExperienceUI();

	OnLevelChanged(CachedLevel, OldLevel);
	OnExperienceChanged(CachedCurrentExp, CachedMaxExp, TargetExpPercent);
}

void USuspenseCoreLevelWidget::RefreshDisplay()
{
	if (!bSmoothProgressBar)
	{
		DisplayedExpPercent = TargetExpPercent;
	}

	UpdateLevelUI();
	UpdateExperienceUI();
}

// ═══════════════════════════════════════════════════════════════════════════
// UI UPDATE
// ═══════════════════════════════════════════════════════════════════════════

void USuspenseCoreLevelWidget::UpdateLevelUI()
{
	if (LevelValueText)
	{
		FString LevelStr = LevelFormatPattern;
		LevelStr = LevelStr.Replace(TEXT("{0}"), *FString::Printf(TEXT("%d"), CachedLevel));
		LevelValueText->SetText(FText::FromString(LevelStr));
	}
}

void USuspenseCoreLevelWidget::UpdateExperienceUI()
{
	if (ExpProgressBar && !bSmoothProgressBar)
	{
		ExpProgressBar->SetPercent(TargetExpPercent);
	}

	// Update separate current/max texts
	if (ExpCurrentText)
	{
		ExpCurrentText->SetText(FText::FromString(FormatNumber(CachedCurrentExp)));
	}

	if (ExpMaxText)
	{
		ExpMaxText->SetText(FText::FromString(FormatNumber(CachedMaxExp)));
	}

	// Update combined text
	if (ExpText)
	{
		FString Result = ExpFormatPattern;
		Result = Result.Replace(TEXT("{0}"), *FormatNumber(CachedCurrentExp));
		Result = Result.Replace(TEXT("{1}"), *FormatNumber(CachedMaxExp));
		ExpText->SetText(FText::FromString(Result));
	}
}

void USuspenseCoreLevelWidget::UpdateProgressBar(float DeltaTime)
{
	if (!ExpProgressBar)
	{
		return;
	}

	DisplayedExpPercent = FMath::FInterpTo(DisplayedExpPercent, TargetExpPercent, DeltaTime, ProgressBarInterpSpeed);
	ExpProgressBar->SetPercent(DisplayedExpPercent);
}

FString USuspenseCoreLevelWidget::FormatNumber(int64 Value) const
{
	if (!bCompactNumbers)
	{
		return FString::Printf(TEXT("%lld"), Value);
	}

	// Compact format for large numbers
	if (Value >= 1000000000)
	{
		return FString::Printf(TEXT("%.1fB"), Value / 1000000000.0);
	}
	else if (Value >= 1000000)
	{
		return FString::Printf(TEXT("%.1fM"), Value / 1000000.0);
	}
	else if (Value >= 1000)
	{
		return FString::Printf(TEXT("%.1fK"), Value / 1000.0);
	}

	return FString::Printf(TEXT("%lld"), Value);
}
