// SuspenseCoreGameHUDWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/SuspenseCoreGameHUDWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"

USuspenseCoreGameHUDWidget::USuspenseCoreGameHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USuspenseCoreGameHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// CRITICAL: Reset FillColorAndOpacity to White so materials display correctly!
	// UE5 ProgressBar default colors TINT the material - we want pure material colors
	if (HealthProgressBar)
	{
		HealthProgressBar->SetFillColorAndOpacity(FLinearColor::White);
	}
	if (ShieldProgressBar)
	{
		ShieldProgressBar->SetFillColorAndOpacity(FLinearColor::White);
	}
	if (StaminaProgressBar)
	{
		StaminaProgressBar->SetFillColorAndOpacity(FLinearColor::White);
	}

	// Setup EventBus subscriptions - the ONLY way to receive attribute updates!
	SetupEventSubscriptions();

	// Initial UI update
	UpdateHealthUI();
	UpdateShieldUI();
	UpdateStaminaUI();
}

void USuspenseCoreGameHUDWidget::NativeDestruct()
{
	TeardownEventSubscriptions();
	Super::NativeDestruct();
}

void USuspenseCoreGameHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
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

void USuspenseCoreGameHUDWidget::SetupEventSubscriptions()
{
	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GetWorld());
	if (!Manager)
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreGameHUDWidget: EventManager not found"));
		return;
	}

	CachedEventBus = Manager->GetEventBus();
	if (!CachedEventBus.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreGameHUDWidget: EventBus not found"));
		return;
	}

	// Subscribe to Health attribute events
	HealthEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.GAS.Attribute.Health")),
		const_cast<USuspenseCoreGameHUDWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreGameHUDWidget::OnHealthEvent),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to MaxHealth attribute events
	MaxHealthEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.GAS.Attribute.MaxHealth")),
		const_cast<USuspenseCoreGameHUDWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreGameHUDWidget::OnMaxHealthEvent),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to Shield attribute events
	ShieldEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.GAS.Attribute.Shield")),
		const_cast<USuspenseCoreGameHUDWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreGameHUDWidget::OnShieldEvent),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to MaxShield attribute events
	MaxShieldEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.GAS.Attribute.MaxShield")),
		const_cast<USuspenseCoreGameHUDWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreGameHUDWidget::OnMaxShieldEvent),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to Stamina attribute events
	FGameplayTag StaminaTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.GAS.Attribute.Stamina"), false);
	UE_LOG(LogTemp, Warning, TEXT("[HUD Widget] Subscribing to Stamina tag: %s (Valid: %s)"),
		*StaminaTag.ToString(), StaminaTag.IsValid() ? TEXT("YES") : TEXT("NO"));

	if (StaminaTag.IsValid())
	{
		StaminaEventHandle = CachedEventBus->SubscribeNative(
			StaminaTag,
			const_cast<USuspenseCoreGameHUDWidget*>(this),
			FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreGameHUDWidget::OnStaminaEvent),
			ESuspenseCoreEventPriority::Normal
		);
		UE_LOG(LogTemp, Warning, TEXT("[HUD Widget] Stamina subscription handle valid: %s"),
			StaminaEventHandle.IsValid() ? TEXT("YES") : TEXT("NO"));
	}

	// Subscribe to MaxStamina attribute events
	FGameplayTag MaxStaminaTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.GAS.Attribute.MaxStamina"), false);
	UE_LOG(LogTemp, Warning, TEXT("[HUD Widget] Subscribing to MaxStamina tag: %s (Valid: %s)"),
		*MaxStaminaTag.ToString(), MaxStaminaTag.IsValid() ? TEXT("YES") : TEXT("NO"));

	if (MaxStaminaTag.IsValid())
	{
		MaxStaminaEventHandle = CachedEventBus->SubscribeNative(
			MaxStaminaTag,
			const_cast<USuspenseCoreGameHUDWidget*>(this),
			FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreGameHUDWidget::OnMaxStaminaEvent),
			ESuspenseCoreEventPriority::Normal
		);
		UE_LOG(LogTemp, Warning, TEXT("[HUD Widget] MaxStamina subscription handle valid: %s"),
			MaxStaminaEventHandle.IsValid() ? TEXT("YES") : TEXT("NO"));
	}

	// Subscribe to LowHealth event
	LowHealthEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.LowHealth")),
		const_cast<USuspenseCoreGameHUDWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreGameHUDWidget::OnLowHealthEvent),
		ESuspenseCoreEventPriority::Normal
	);

	// Subscribe to ShieldBroken event
	ShieldBrokenEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.GAS.Shield.Broken")),
		const_cast<USuspenseCoreGameHUDWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreGameHUDWidget::OnShieldBrokenEvent),
		ESuspenseCoreEventPriority::Normal
	);

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreGameHUDWidget: EventBus subscriptions setup complete"));
}

void USuspenseCoreGameHUDWidget::TeardownEventSubscriptions()
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

void USuspenseCoreGameHUDWidget::OnHealthEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
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

void USuspenseCoreGameHUDWidget::OnMaxHealthEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	CachedMaxHealth = EventData.GetFloat(FName("Value"), CachedMaxHealth);

	TargetHealthPercent = (CachedMaxHealth > 0.0f) ? (CachedHealth / CachedMaxHealth) : 0.0f;
	UpdateHealthUI();
}

void USuspenseCoreGameHUDWidget::OnShieldEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
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

void USuspenseCoreGameHUDWidget::OnMaxShieldEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	CachedMaxShield = EventData.GetFloat(FName("Value"), CachedMaxShield);

	TargetShieldPercent = (CachedMaxShield > 0.0f) ? (CachedShield / CachedMaxShield) : 0.0f;
	UpdateShieldUI();
}

void USuspenseCoreGameHUDWidget::OnStaminaEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	UE_LOG(LogTemp, Warning, TEXT("[HUD Widget] OnStaminaEvent received! Tag: %s"), *EventTag.ToString());

	float OldStamina = CachedStamina;

	CachedStamina = EventData.GetFloat(FName("Value"), CachedStamina);

	UE_LOG(LogTemp, Warning, TEXT("[HUD Widget] Stamina: %.2f -> %.2f, MaxStamina: %.2f"),
		OldStamina, CachedStamina, CachedMaxStamina);

	TargetStaminaPercent = (CachedMaxStamina > 0.0f) ? (CachedStamina / CachedMaxStamina) : 0.0f;

	UE_LOG(LogTemp, Warning, TEXT("[HUD Widget] TargetStaminaPercent: %.2f"), TargetStaminaPercent);

	UpdateStaminaUI();

	// Broadcast Blueprint event
	OnStaminaChanged(CachedStamina, CachedMaxStamina, OldStamina);
}

void USuspenseCoreGameHUDWidget::OnMaxStaminaEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	CachedMaxStamina = EventData.GetFloat(FName("Value"), CachedMaxStamina);

	TargetStaminaPercent = (CachedMaxStamina > 0.0f) ? (CachedStamina / CachedMaxStamina) : 0.0f;
	UpdateStaminaUI();
}

void USuspenseCoreGameHUDWidget::OnLowHealthEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	OnHealthCritical();
}

void USuspenseCoreGameHUDWidget::OnShieldBrokenEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	bWasShieldBroken = true;
	OnShieldBroken();
}

// ═══════════════════════════════════════════════════════════════════════════
// PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════

void USuspenseCoreGameHUDWidget::RefreshAllValues()
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

void USuspenseCoreGameHUDWidget::SetHealthValues(float Current, float Max)
{
	float OldHealth = CachedHealth;
	CachedHealth = Current;
	CachedMaxHealth = Max;
	TargetHealthPercent = (CachedMaxHealth > 0.0f) ? (CachedHealth / CachedMaxHealth) : 0.0f;
	UpdateHealthUI();
	OnHealthChanged(CachedHealth, CachedMaxHealth, OldHealth);
}

void USuspenseCoreGameHUDWidget::SetShieldValues(float Current, float Max)
{
	float OldShield = CachedShield;
	CachedShield = Current;
	CachedMaxShield = Max;
	TargetShieldPercent = (CachedMaxShield > 0.0f) ? (CachedShield / CachedMaxShield) : 0.0f;
	UpdateShieldUI();
	OnShieldChanged(CachedShield, CachedMaxShield, OldShield);
}

void USuspenseCoreGameHUDWidget::SetStaminaValues(float Current, float Max)
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

void USuspenseCoreGameHUDWidget::UpdateHealthUI()
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
}

void USuspenseCoreGameHUDWidget::UpdateShieldUI()
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

void USuspenseCoreGameHUDWidget::UpdateStaminaUI()
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

void USuspenseCoreGameHUDWidget::UpdateProgressBar(UProgressBar* ProgressBar, float& DisplayedPercent, float TargetPercent, float DeltaTime)
{
	if (!ProgressBar)
	{
		return;
	}

	// Smooth interpolation for fluid fill/drain animation
	// NOTE: Only SetPercent is called - NO color changes! Colors from material in Editor
	DisplayedPercent = FMath::FInterpTo(DisplayedPercent, TargetPercent, DeltaTime, ProgressBarInterpSpeed);
	ProgressBar->SetPercent(DisplayedPercent);
}

FString USuspenseCoreGameHUDWidget::FormatValueText(float Current, float Max) const
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
