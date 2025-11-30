// SuspenseCoreHUDWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/SuspenseCoreHUDWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"

USuspenseCoreHUDWidget::USuspenseCoreHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USuspenseCoreHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Setup EventBus subscriptions - the ONLY way to receive attribute updates!
	SetupEventSubscriptions();

	// Initial UI update
	UpdateHealthUI();
	UpdateShieldUI();
	UpdateStaminaUI();
}

void USuspenseCoreHUDWidget::NativeDestruct()
{
	TeardownEventSubscriptions();
	Super::NativeDestruct();
}

void USuspenseCoreHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Smooth progress bar interpolation
	if (bSmoothProgressBars)
	{
		if (HealthProgressBar)
		{
			UpdateProgressBar(HealthProgressBar.Get(), DisplayedHealthPercent, TargetHealthPercent, InDeltaTime);
		}

		if (ShieldProgressBar)
		{
			UpdateProgressBar(ShieldProgressBar.Get(), DisplayedShieldPercent, TargetShieldPercent, InDeltaTime);
		}

		if (StaminaProgressBar)
		{
			UpdateProgressBar(StaminaProgressBar.Get(), DisplayedStaminaPercent, TargetStaminaPercent, InDeltaTime);
		}
	}
}

void USuspenseCoreHUDWidget::SetupEventSubscriptions()
{
	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GetWorld());
	if (!Manager)
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreHUDWidget: EventManager not found"));
		return;
	}

	CachedEventBus = Manager->GetEventBus();
	if (!CachedEventBus.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreHUDWidget: EventBus not found"));
		return;
	}

	// Subscribe to Health attribute events
	HealthEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.GAS.Attribute.Health")),
		const_cast<USuspenseCoreHUDWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreHUDWidget::OnHealthEvent),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to MaxHealth attribute events
	MaxHealthEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.GAS.Attribute.MaxHealth")),
		const_cast<USuspenseCoreHUDWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreHUDWidget::OnMaxHealthEvent),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to Shield attribute events
	ShieldEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.GAS.Attribute.Shield")),
		const_cast<USuspenseCoreHUDWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreHUDWidget::OnShieldEvent),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to MaxShield attribute events
	MaxShieldEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.GAS.Attribute.MaxShield")),
		const_cast<USuspenseCoreHUDWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreHUDWidget::OnMaxShieldEvent),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to Stamina attribute events
	StaminaEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.GAS.Attribute.Stamina")),
		const_cast<USuspenseCoreHUDWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreHUDWidget::OnStaminaEvent),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to MaxStamina attribute events
	MaxStaminaEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.GAS.Attribute.MaxStamina")),
		const_cast<USuspenseCoreHUDWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreHUDWidget::OnMaxStaminaEvent),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to LowHealth event
	LowHealthEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.LowHealth")),
		const_cast<USuspenseCoreHUDWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreHUDWidget::OnLowHealthEvent),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to ShieldBroken event
	ShieldBrokenEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.GAS.Shield.Broken")),
		const_cast<USuspenseCoreHUDWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreHUDWidget::OnShieldBrokenEvent),
		ESuspenseCoreEventPriority::Normal
	);

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreHUDWidget: EventBus subscriptions setup complete"));
}

void USuspenseCoreHUDWidget::TeardownEventSubscriptions()
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
	if (LowHealthEventHandle.IsValid())
	{
		CachedEventBus->Unsubscribe(LowHealthEventHandle);
	}
	if (ShieldBrokenEventHandle.IsValid())
	{
		CachedEventBus->Unsubscribe(ShieldBrokenEventHandle);
	}
}

// ═══════════════════════════════════════════════════════════════════════════
// EVENTBUS HANDLERS
// ═══════════════════════════════════════════════════════════════════════════

void USuspenseCoreHUDWidget::OnHealthEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	float OldHealth = CachedHealth;

	// Extract new value from EventData using proper API
	CachedHealth = EventData.GetFloat(FName("Value"), CachedHealth);

	TargetHealthPercent = (CachedMaxHealth > 0.0f) ? (CachedHealth / CachedMaxHealth) : 0.0f;
	UpdateHealthUI();

	// Broadcast Blueprint event
	OnHealthChanged(CachedHealth, CachedMaxHealth, OldHealth);

	// Check for critical health
	bool bIsCritical = TargetHealthPercent <= CriticalHealthThreshold && CachedHealth > 0.0f;
	if (bIsCritical && !bWasHealthCritical)
	{
		OnHealthCritical();
	}
	bWasHealthCritical = bIsCritical;
}

void USuspenseCoreHUDWidget::OnMaxHealthEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	CachedMaxHealth = EventData.GetFloat(FName("Value"), CachedMaxHealth);

	TargetHealthPercent = (CachedMaxHealth > 0.0f) ? (CachedHealth / CachedMaxHealth) : 0.0f;
	UpdateHealthUI();
}

void USuspenseCoreHUDWidget::OnShieldEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	float OldShield = CachedShield;

	CachedShield = EventData.GetFloat(FName("Value"), CachedShield);

	TargetShieldPercent = (CachedMaxShield > 0.0f) ? (CachedShield / CachedMaxShield) : 0.0f;
	UpdateShieldUI();

	// Broadcast Blueprint event
	OnShieldChanged(CachedShield, CachedMaxShield, OldShield);

	// Check for shield broken
	bool bIsBroken = CachedShield <= 0.0f && CachedMaxShield > 0.0f;
	if (bIsBroken && !bWasShieldBroken)
	{
		OnShieldBroken();
	}
	bWasShieldBroken = bIsBroken;
}

void USuspenseCoreHUDWidget::OnMaxShieldEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	CachedMaxShield = EventData.GetFloat(FName("Value"), CachedMaxShield);

	TargetShieldPercent = (CachedMaxShield > 0.0f) ? (CachedShield / CachedMaxShield) : 0.0f;
	UpdateShieldUI();
}

void USuspenseCoreHUDWidget::OnStaminaEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	float OldStamina = CachedStamina;

	CachedStamina = EventData.GetFloat(FName("Value"), CachedStamina);

	TargetStaminaPercent = (CachedMaxStamina > 0.0f) ? (CachedStamina / CachedMaxStamina) : 0.0f;
	UpdateStaminaUI();

	// Broadcast Blueprint event
	OnStaminaChanged(CachedStamina, CachedMaxStamina, OldStamina);
}

void USuspenseCoreHUDWidget::OnMaxStaminaEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	CachedMaxStamina = EventData.GetFloat(FName("Value"), CachedMaxStamina);

	TargetStaminaPercent = (CachedMaxStamina > 0.0f) ? (CachedStamina / CachedMaxStamina) : 0.0f;
	UpdateStaminaUI();
}

void USuspenseCoreHUDWidget::OnLowHealthEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	OnHealthCritical();
}

void USuspenseCoreHUDWidget::OnShieldBrokenEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	bWasShieldBroken = true;
	OnShieldBroken();
}

// ═══════════════════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════

void USuspenseCoreHUDWidget::RefreshAllValues()
{
	// Calculate target percentages
	TargetHealthPercent = (CachedMaxHealth > 0.0f) ? (CachedHealth / CachedMaxHealth) : 0.0f;
	TargetShieldPercent = (CachedMaxShield > 0.0f) ? (CachedShield / CachedMaxShield) : 0.0f;
	TargetStaminaPercent = (CachedMaxStamina > 0.0f) ? (CachedStamina / CachedMaxStamina) : 0.0f;

	// If not smooth, set displayed values directly
	if (!bSmoothProgressBars)
	{
		DisplayedHealthPercent = TargetHealthPercent;
		DisplayedShieldPercent = TargetShieldPercent;
		DisplayedStaminaPercent = TargetStaminaPercent;
	}

	// Update UI
	UpdateHealthUI();
	UpdateShieldUI();
	UpdateStaminaUI();
}

void USuspenseCoreHUDWidget::SetHealthValues(float Current, float Max)
{
	float OldHealth = CachedHealth;
	CachedHealth = Current;
	CachedMaxHealth = Max;
	TargetHealthPercent = (CachedMaxHealth > 0.0f) ? (CachedHealth / CachedMaxHealth) : 0.0f;
	UpdateHealthUI();
	OnHealthChanged(CachedHealth, CachedMaxHealth, OldHealth);
}

void USuspenseCoreHUDWidget::SetShieldValues(float Current, float Max)
{
	float OldShield = CachedShield;
	CachedShield = Current;
	CachedMaxShield = Max;
	TargetShieldPercent = (CachedMaxShield > 0.0f) ? (CachedShield / CachedMaxShield) : 0.0f;
	UpdateShieldUI();
	OnShieldChanged(CachedShield, CachedMaxShield, OldShield);
}

void USuspenseCoreHUDWidget::SetStaminaValues(float Current, float Max)
{
	float OldStamina = CachedStamina;
	CachedStamina = Current;
	CachedMaxStamina = Max;
	TargetStaminaPercent = (CachedMaxStamina > 0.0f) ? (CachedStamina / CachedMaxStamina) : 0.0f;
	UpdateStaminaUI();
	OnStaminaChanged(CachedStamina, CachedMaxStamina, OldStamina);
}

// ═══════════════════════════════════════════════════════════════════════════
// UI UPDATE METHODS
// ═══════════════════════════════════════════════════════════════════════════

void USuspenseCoreHUDWidget::UpdateHealthUI()
{
	// Update progress bar
	if (HealthProgressBar && !bSmoothProgressBars)
	{
		HealthProgressBar->SetPercent(TargetHealthPercent);
	}

	// Update text values
	if (HealthValueText)
	{
		if (bShowDecimals)
		{
			HealthValueText->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), CachedHealth)));
		}
		else
		{
			HealthValueText->SetText(FText::FromString(FString::Printf(TEXT("%d"), FMath::RoundToInt(CachedHealth))));
		}
	}

	if (MaxHealthValueText)
	{
		if (bShowDecimals)
		{
			MaxHealthValueText->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), CachedMaxHealth)));
		}
		else
		{
			MaxHealthValueText->SetText(FText::FromString(FString::Printf(TEXT("%d"), FMath::RoundToInt(CachedMaxHealth))));
		}
	}

	if (HealthText)
	{
		HealthText->SetText(FText::FromString(FormatValueText(CachedHealth, CachedMaxHealth)));
	}

	// Apply color based on health level
	ApplyHealthBarColor();
}

void USuspenseCoreHUDWidget::UpdateShieldUI()
{
	// Update progress bar
	if (ShieldProgressBar && !bSmoothProgressBars)
	{
		ShieldProgressBar->SetPercent(TargetShieldPercent);
	}

	// Update text values
	if (ShieldValueText)
	{
		if (bShowDecimals)
		{
			ShieldValueText->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), CachedShield)));
		}
		else
		{
			ShieldValueText->SetText(FText::FromString(FString::Printf(TEXT("%d"), FMath::RoundToInt(CachedShield))));
		}
	}

	if (MaxShieldValueText)
	{
		if (bShowDecimals)
		{
			MaxShieldValueText->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), CachedMaxShield)));
		}
		else
		{
			MaxShieldValueText->SetText(FText::FromString(FString::Printf(TEXT("%d"), FMath::RoundToInt(CachedMaxShield))));
		}
	}

	if (ShieldText)
	{
		ShieldText->SetText(FText::FromString(FormatValueText(CachedShield, CachedMaxShield)));
	}
}

void USuspenseCoreHUDWidget::UpdateStaminaUI()
{
	// Update progress bar
	if (StaminaProgressBar && !bSmoothProgressBars)
	{
		StaminaProgressBar->SetPercent(TargetStaminaPercent);
	}

	// Update text values
	if (StaminaValueText)
	{
		if (bShowDecimals)
		{
			StaminaValueText->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), CachedStamina)));
		}
		else
		{
			StaminaValueText->SetText(FText::FromString(FString::Printf(TEXT("%d"), FMath::RoundToInt(CachedStamina))));
		}
	}

	if (MaxStaminaValueText)
	{
		if (bShowDecimals)
		{
			MaxStaminaValueText->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), CachedMaxStamina)));
		}
		else
		{
			MaxStaminaValueText->SetText(FText::FromString(FString::Printf(TEXT("%d"), FMath::RoundToInt(CachedMaxStamina))));
		}
	}

	if (StaminaText)
	{
		StaminaText->SetText(FText::FromString(FormatValueText(CachedStamina, CachedMaxStamina)));
	}
}

void USuspenseCoreHUDWidget::UpdateProgressBar(UProgressBar* ProgressBar, float& DisplayedPercent, float TargetPercent, float DeltaTime)
{
	if (!ProgressBar)
	{
		return;
	}

	// Smooth interpolation
	DisplayedPercent = FMath::FInterpTo(DisplayedPercent, TargetPercent, DeltaTime, ProgressBarInterpSpeed);
	ProgressBar->SetPercent(DisplayedPercent);
}

void USuspenseCoreHUDWidget::ApplyHealthBarColor()
{
	if (!HealthProgressBar)
	{
		return;
	}

	// Determine color based on health percentage
	FLinearColor BarColor;
	if (TargetHealthPercent <= CriticalHealthThreshold)
	{
		BarColor = HealthColorCritical;
	}
	else
	{
		// Interpolate between critical and normal based on health
		float Alpha = FMath::Clamp((TargetHealthPercent - CriticalHealthThreshold) / (1.0f - CriticalHealthThreshold), 0.0f, 1.0f);
		BarColor = FLinearColor::LerpUsingHSV(HealthColorCritical, HealthColorNormal, Alpha);
	}

	HealthProgressBar->SetFillColorAndOpacity(BarColor);
}

FString USuspenseCoreHUDWidget::FormatValueText(float Current, float Max) const
{
	FString Result = ValueFormatPattern;

	FString CurrentStr;
	FString MaxStr;

	if (bShowDecimals)
	{
		CurrentStr = FString::Printf(TEXT("%.1f"), Current);
		MaxStr = FString::Printf(TEXT("%.1f"), Max);
	}
	else
	{
		CurrentStr = FString::Printf(TEXT("%d"), FMath::RoundToInt(Current));
		MaxStr = FString::Printf(TEXT("%d"), FMath::RoundToInt(Max));
	}

	Result = Result.Replace(TEXT("{0}"), *CurrentStr);
	Result = Result.Replace(TEXT("{1}"), *MaxStr);

	return Result;
}
