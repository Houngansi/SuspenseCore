// SuspenseCoreAttributesWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/SuspenseCoreAttributesWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

USuspenseCoreAttributesWidget::USuspenseCoreAttributesWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USuspenseCoreAttributesWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Setup EventBus subscriptions
	SetupEventSubscriptions();

	// Initial UI update
	UpdateHealthUI();
	UpdateShieldUI();
	UpdateStaminaUI();
}

void USuspenseCoreAttributesWidget::NativeDestruct()
{
	TeardownEventSubscriptions();
	Super::NativeDestruct();
}

void USuspenseCoreAttributesWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Smooth progress bar interpolation
	if (bSmoothProgressBars)
	{
		if (HealthBar)
		{
			UpdateProgressBar(HealthBar.Get(), DisplayedHealthPercent, TargetHealthPercent, InDeltaTime);
		}

		if (ShieldBar)
		{
			UpdateProgressBar(ShieldBar.Get(), DisplayedShieldPercent, TargetShieldPercent, InDeltaTime);
		}

		if (StaminaBar)
		{
			UpdateProgressBar(StaminaBar.Get(), DisplayedStaminaPercent, TargetStaminaPercent, InDeltaTime);
		}
	}
}

// ═══════════════════════════════════════════════════════════════════════════
// EVENTBUS SUBSCRIPTIONS
// ═══════════════════════════════════════════════════════════════════════════

void USuspenseCoreAttributesWidget::SetupEventSubscriptions()
{
	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GetWorld());
	if (!Manager)
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreAttributesWidget: EventManager not found"));
		return;
	}

	CachedEventBus = Manager->GetEventBus();
	if (!CachedEventBus.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreAttributesWidget: EventBus not found"));
		return;
	}

	// Subscribe to Health attribute events
	HealthEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.GAS.Attribute.Health")),
		const_cast<USuspenseCoreAttributesWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreAttributesWidget::OnHealthEvent),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to MaxHealth attribute events
	MaxHealthEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.GAS.Attribute.MaxHealth")),
		const_cast<USuspenseCoreAttributesWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreAttributesWidget::OnMaxHealthEvent),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to Shield attribute events
	ShieldEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.GAS.Attribute.Shield")),
		const_cast<USuspenseCoreAttributesWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreAttributesWidget::OnShieldEvent),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to MaxShield attribute events
	MaxShieldEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.GAS.Attribute.MaxShield")),
		const_cast<USuspenseCoreAttributesWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreAttributesWidget::OnMaxShieldEvent),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to Stamina attribute events
	StaminaEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.GAS.Attribute.Stamina")),
		const_cast<USuspenseCoreAttributesWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreAttributesWidget::OnStaminaEvent),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to MaxStamina attribute events
	MaxStaminaEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.GAS.Attribute.MaxStamina")),
		const_cast<USuspenseCoreAttributesWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreAttributesWidget::OnMaxStaminaEvent),
		ESuspenseCoreEventPriority::Normal
	);

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreAttributesWidget: EventBus subscriptions setup complete"));
}

void USuspenseCoreAttributesWidget::TeardownEventSubscriptions()
{
	if (!CachedEventBus.IsValid())
	{
		return;
	}

	if (HealthEventHandle.IsValid())
	{
		CachedEventBus->Unsubscribe(HealthEventHandle);
	}
	if (MaxHealthEventHandle.IsValid())
	{
		CachedEventBus->Unsubscribe(MaxHealthEventHandle);
	}
	if (ShieldEventHandle.IsValid())
	{
		CachedEventBus->Unsubscribe(ShieldEventHandle);
	}
	if (MaxShieldEventHandle.IsValid())
	{
		CachedEventBus->Unsubscribe(MaxShieldEventHandle);
	}
	if (StaminaEventHandle.IsValid())
	{
		CachedEventBus->Unsubscribe(StaminaEventHandle);
	}
	if (MaxStaminaEventHandle.IsValid())
	{
		CachedEventBus->Unsubscribe(MaxStaminaEventHandle);
	}
}

// ═══════════════════════════════════════════════════════════════════════════
// EVENTBUS HANDLERS
// ═══════════════════════════════════════════════════════════════════════════

void USuspenseCoreAttributesWidget::OnHealthEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	float OldHealth = CachedHealth;
	CachedHealth = EventData.GetFloat(FName("Value"), CachedHealth);

	TargetHealthPercent = (CachedMaxHealth > 0.0f) ? (CachedHealth / CachedMaxHealth) : 0.0f;
	UpdateHealthUI();

	OnHealthChanged(CachedHealth, CachedMaxHealth, OldHealth);

	// Check for critical health
	bool bIsCritical = TargetHealthPercent <= CriticalHealthThreshold && CachedHealth > 0.0f;
	if (bIsCritical && !bWasHealthCritical)
	{
		OnHealthCritical();
	}
	bWasHealthCritical = bIsCritical;
}

void USuspenseCoreAttributesWidget::OnMaxHealthEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	CachedMaxHealth = EventData.GetFloat(FName("Value"), CachedMaxHealth);

	TargetHealthPercent = (CachedMaxHealth > 0.0f) ? (CachedHealth / CachedMaxHealth) : 0.0f;
	UpdateHealthUI();
}

void USuspenseCoreAttributesWidget::OnShieldEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	float OldShield = CachedShield;
	CachedShield = EventData.GetFloat(FName("Value"), CachedShield);

	TargetShieldPercent = (CachedMaxShield > 0.0f) ? (CachedShield / CachedMaxShield) : 0.0f;
	UpdateShieldUI();

	OnShieldChanged(CachedShield, CachedMaxShield, OldShield);

	// Check for shield broken
	bool bIsBroken = CachedShield <= 0.0f && CachedMaxShield > 0.0f;
	if (bIsBroken && !bWasShieldBroken)
	{
		OnShieldBroken();
	}
	bWasShieldBroken = bIsBroken;
}

void USuspenseCoreAttributesWidget::OnMaxShieldEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	CachedMaxShield = EventData.GetFloat(FName("Value"), CachedMaxShield);

	TargetShieldPercent = (CachedMaxShield > 0.0f) ? (CachedShield / CachedMaxShield) : 0.0f;
	UpdateShieldUI();
}

void USuspenseCoreAttributesWidget::OnStaminaEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	float OldStamina = CachedStamina;
	CachedStamina = EventData.GetFloat(FName("Value"), CachedStamina);

	TargetStaminaPercent = (CachedMaxStamina > 0.0f) ? (CachedStamina / CachedMaxStamina) : 0.0f;
	UpdateStaminaUI();

	OnStaminaChanged(CachedStamina, CachedMaxStamina, OldStamina);
}

void USuspenseCoreAttributesWidget::OnMaxStaminaEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	CachedMaxStamina = EventData.GetFloat(FName("Value"), CachedMaxStamina);

	TargetStaminaPercent = (CachedMaxStamina > 0.0f) ? (CachedStamina / CachedMaxStamina) : 0.0f;
	UpdateStaminaUI();
}

// ═══════════════════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════

void USuspenseCoreAttributesWidget::RefreshAllValues()
{
	TargetHealthPercent = (CachedMaxHealth > 0.0f) ? (CachedHealth / CachedMaxHealth) : 0.0f;
	TargetShieldPercent = (CachedMaxShield > 0.0f) ? (CachedShield / CachedMaxShield) : 0.0f;
	TargetStaminaPercent = (CachedMaxStamina > 0.0f) ? (CachedStamina / CachedMaxStamina) : 0.0f;

	if (!bSmoothProgressBars)
	{
		DisplayedHealthPercent = TargetHealthPercent;
		DisplayedShieldPercent = TargetShieldPercent;
		DisplayedStaminaPercent = TargetStaminaPercent;
	}

	UpdateHealthUI();
	UpdateShieldUI();
	UpdateStaminaUI();
}

void USuspenseCoreAttributesWidget::SetHealthValues(float Current, float Max)
{
	float OldHealth = CachedHealth;
	CachedHealth = Current;
	CachedMaxHealth = Max;
	TargetHealthPercent = (CachedMaxHealth > 0.0f) ? (CachedHealth / CachedMaxHealth) : 0.0f;
	UpdateHealthUI();
	OnHealthChanged(CachedHealth, CachedMaxHealth, OldHealth);
}

void USuspenseCoreAttributesWidget::SetShieldValues(float Current, float Max)
{
	float OldShield = CachedShield;
	CachedShield = Current;
	CachedMaxShield = Max;
	TargetShieldPercent = (CachedMaxShield > 0.0f) ? (CachedShield / CachedMaxShield) : 0.0f;
	UpdateShieldUI();
	OnShieldChanged(CachedShield, CachedMaxShield, OldShield);
}

void USuspenseCoreAttributesWidget::SetStaminaValues(float Current, float Max)
{
	float OldStamina = CachedStamina;
	CachedStamina = Current;
	CachedMaxStamina = Max;
	TargetStaminaPercent = (CachedMaxStamina > 0.0f) ? (CachedStamina / CachedMaxStamina) : 0.0f;
	UpdateStaminaUI();
	OnStaminaChanged(CachedStamina, CachedMaxStamina, OldStamina);
}

// ═══════════════════════════════════════════════════════════════════════════
// UI UPDATE
// ═══════════════════════════════════════════════════════════════════════════

void USuspenseCoreAttributesWidget::UpdateHealthUI()
{
	if (HealthBar && !bSmoothProgressBars)
	{
		HealthBar->SetPercent(TargetHealthPercent);
	}

	if (HealthValueText)
	{
		FString ValueStr = bShowDecimals
			? FString::Printf(TEXT("%.1f"), CachedHealth)
			: FString::Printf(TEXT("%d"), FMath::RoundToInt(CachedHealth));
		HealthValueText->SetText(FText::FromString(ValueStr));
	}

	if (MaxHealthValueText)
	{
		FString ValueStr = bShowDecimals
			? FString::Printf(TEXT("%.1f"), CachedMaxHealth)
			: FString::Printf(TEXT("%d"), FMath::RoundToInt(CachedMaxHealth));
		MaxHealthValueText->SetText(FText::FromString(ValueStr));
	}

	if (HealthText)
	{
		HealthText->SetText(FText::FromString(FormatValueText(CachedHealth, CachedMaxHealth)));
	}

	ApplyHealthBarColor();
}

void USuspenseCoreAttributesWidget::UpdateShieldUI()
{
	if (ShieldBar && !bSmoothProgressBars)
	{
		ShieldBar->SetPercent(TargetShieldPercent);
	}

	if (ShieldValueText)
	{
		FString ValueStr = bShowDecimals
			? FString::Printf(TEXT("%.1f"), CachedShield)
			: FString::Printf(TEXT("%d"), FMath::RoundToInt(CachedShield));
		ShieldValueText->SetText(FText::FromString(ValueStr));
	}

	if (MaxShieldValueText)
	{
		FString ValueStr = bShowDecimals
			? FString::Printf(TEXT("%.1f"), CachedMaxShield)
			: FString::Printf(TEXT("%d"), FMath::RoundToInt(CachedMaxShield));
		MaxShieldValueText->SetText(FText::FromString(ValueStr));
	}

	if (ShieldText)
	{
		ShieldText->SetText(FText::FromString(FormatValueText(CachedShield, CachedMaxShield)));
	}

	if (ShieldBar)
	{
		ShieldBar->SetFillColorAndOpacity(ShieldColor);
	}
}

void USuspenseCoreAttributesWidget::UpdateStaminaUI()
{
	if (StaminaBar && !bSmoothProgressBars)
	{
		StaminaBar->SetPercent(TargetStaminaPercent);
	}

	if (StaminaValueText)
	{
		FString ValueStr = bShowDecimals
			? FString::Printf(TEXT("%.1f"), CachedStamina)
			: FString::Printf(TEXT("%d"), FMath::RoundToInt(CachedStamina));
		StaminaValueText->SetText(FText::FromString(ValueStr));
	}

	if (MaxStaminaValueText)
	{
		FString ValueStr = bShowDecimals
			? FString::Printf(TEXT("%.1f"), CachedMaxStamina)
			: FString::Printf(TEXT("%d"), FMath::RoundToInt(CachedMaxStamina));
		MaxStaminaValueText->SetText(FText::FromString(ValueStr));
	}

	if (StaminaText)
	{
		StaminaText->SetText(FText::FromString(FormatValueText(CachedStamina, CachedMaxStamina)));
	}

	if (StaminaBar)
	{
		StaminaBar->SetFillColorAndOpacity(StaminaColor);
	}
}

void USuspenseCoreAttributesWidget::UpdateProgressBar(UProgressBar* Bar, float& DisplayedPercent, float TargetPercent, float DeltaTime)
{
	if (!Bar)
	{
		return;
	}

	DisplayedPercent = FMath::FInterpTo(DisplayedPercent, TargetPercent, DeltaTime, ProgressBarInterpSpeed);
	Bar->SetPercent(DisplayedPercent);
}

void USuspenseCoreAttributesWidget::ApplyHealthBarColor()
{
	if (!HealthBar)
	{
		return;
	}

	FLinearColor BarColor;
	if (TargetHealthPercent <= CriticalHealthThreshold)
	{
		BarColor = HealthColorCritical;
	}
	else
	{
		float Alpha = FMath::Clamp((TargetHealthPercent - CriticalHealthThreshold) / (1.0f - CriticalHealthThreshold), 0.0f, 1.0f);
		BarColor = FLinearColor::LerpUsingHSV(HealthColorCritical, HealthColorNormal, Alpha);
	}

	HealthBar->SetFillColorAndOpacity(BarColor);
}

FString USuspenseCoreAttributesWidget::FormatValueText(float Current, float Max) const
{
	FString Result = ValueFormatPattern;

	FString CurrentStr = bShowDecimals
		? FString::Printf(TEXT("%.1f"), Current)
		: FString::Printf(TEXT("%d"), FMath::RoundToInt(Current));

	FString MaxStr = bShowDecimals
		? FString::Printf(TEXT("%.1f"), Max)
		: FString::Printf(TEXT("%d"), FMath::RoundToInt(Max));

	Result = Result.Replace(TEXT("{0}"), *CurrentStr);
	Result = Result.Replace(TEXT("{1}"), *MaxStr);

	return Result;
}
