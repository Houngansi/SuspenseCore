// SuspenseCoreGameHUDWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/SuspenseCoreGameHUDWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Styling/SlateBrush.h"

USuspenseCoreGameHUDWidget::USuspenseCoreGameHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// Helper function to create dynamic material instance from progress bar's background brush
static UMaterialInstanceDynamic* CreateDynamicMaterialFromProgressBar(UProgressBar* ProgressBar, UObject* Outer)
{
	if (!ProgressBar)
	{
		return nullptr;
	}

	const FProgressBarStyle& Style = ProgressBar->GetWidgetStyle();
	const FSlateBrush& BackgroundBrush = Style.BackgroundImage;

	UObject* ResourceObject = BackgroundBrush.GetResourceObject();
	if (!ResourceObject)
	{
		return nullptr;
	}

	UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(ResourceObject);
	if (!MaterialInterface)
	{
		return nullptr;
	}

	UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(MaterialInterface, Outer);
	if (DynamicMaterial)
	{
		FProgressBarStyle NewStyle = Style;
		NewStyle.BackgroundImage.SetResourceObject(DynamicMaterial);
		ProgressBar->SetWidgetStyle(NewStyle);
	}

	return DynamicMaterial;
}

void USuspenseCoreGameHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Reset FillColorAndOpacity to White so materials display correctly
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

	// Create dynamic material instances for material-based progress bars
	if (bUseMaterialProgress)
	{
		HealthProgressMaterial = CreateDynamicMaterialFromProgressBar(HealthProgressBar.Get(), this);
		ShieldProgressMaterial = CreateDynamicMaterialFromProgressBar(ShieldProgressBar.Get(), this);
		StaminaProgressMaterial = CreateDynamicMaterialFromProgressBar(StaminaProgressBar.Get(), this);
	}

	SetupEventSubscriptions();

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

	if (bSmoothProgressBars)
	{
		if (HealthProgressBar)
		{
			UpdateProgressBar(HealthProgressBar.Get(), HealthProgressMaterial.Get(), DisplayedHealthPercent, TargetHealthPercent, InDeltaTime);
		}

		if (ShieldProgressBar)
		{
			UpdateProgressBar(ShieldProgressBar.Get(), ShieldProgressMaterial.Get(), DisplayedShieldPercent, TargetShieldPercent, InDeltaTime);
		}

		if (StaminaProgressBar)
		{
			UpdateProgressBar(StaminaProgressBar.Get(), StaminaProgressMaterial.Get(), DisplayedStaminaPercent, TargetStaminaPercent, InDeltaTime);
		}
	}
}

void USuspenseCoreGameHUDWidget::SetupEventSubscriptions()
{
	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GetWorld());
	if (!Manager)
	{
		return;
	}

	CachedEventBus = Manager->GetEventBus();
	if (!CachedEventBus.IsValid())
	{
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
	if (StaminaTag.IsValid())
	{
		StaminaEventHandle = CachedEventBus->SubscribeNative(
			StaminaTag,
			const_cast<USuspenseCoreGameHUDWidget*>(this),
			FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreGameHUDWidget::OnStaminaEvent),
			ESuspenseCoreEventPriority::Normal
		);
	}

	// Subscribe to MaxStamina attribute events
	FGameplayTag MaxStaminaTag = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.GAS.Attribute.MaxStamina"), false);
	if (MaxStaminaTag.IsValid())
	{
		MaxStaminaEventHandle = CachedEventBus->SubscribeNative(
			MaxStaminaTag,
			const_cast<USuspenseCoreGameHUDWidget*>(this),
			FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreGameHUDWidget::OnMaxStaminaEvent),
			ESuspenseCoreEventPriority::Normal
		);
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
	CachedHealth = EventData.GetFloat(FName("Value"), CachedHealth);

	TargetHealthPercent = (CachedMaxHealth > 0.0f) ? (CachedHealth / CachedMaxHealth) : 0.0f;
	UpdateHealthUI();

	OnHealthChanged(CachedHealth, CachedMaxHealth, OldHealth);

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

	OnShieldChanged(CachedShield, CachedMaxShield, OldShield);

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
	float OldStamina = CachedStamina;
	CachedStamina = EventData.GetFloat(FName("Value"), CachedStamina);
	TargetStaminaPercent = (CachedMaxStamina > 0.0f) ? (CachedStamina / CachedMaxStamina) : 0.0f;
	UpdateStaminaUI();
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
	if (HealthProgressBar && !bSmoothProgressBars)
	{
		if (bUseMaterialProgress && HealthProgressMaterial)
		{
			HealthProgressMaterial->SetScalarParameterValue(MaterialProgressParameterName, TargetHealthPercent);
		}
		else
		{
			HealthProgressBar->SetPercent(TargetHealthPercent);
		}
	}

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
	if (ShieldProgressBar && !bSmoothProgressBars)
	{
		if (bUseMaterialProgress && ShieldProgressMaterial)
		{
			ShieldProgressMaterial->SetScalarParameterValue(MaterialProgressParameterName, TargetShieldPercent);
		}
		else
		{
			ShieldProgressBar->SetPercent(TargetShieldPercent);
		}
	}

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
	if (StaminaProgressBar && !bSmoothProgressBars)
	{
		if (bUseMaterialProgress && StaminaProgressMaterial)
		{
			StaminaProgressMaterial->SetScalarParameterValue(MaterialProgressParameterName, TargetStaminaPercent);
		}
		else
		{
			StaminaProgressBar->SetPercent(TargetStaminaPercent);
		}
	}

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

void USuspenseCoreGameHUDWidget::UpdateProgressBar(UProgressBar* ProgressBar, UMaterialInstanceDynamic* Material, float& DisplayedPercent, float TargetPercent, float DeltaTime)
{
	if (!ProgressBar)
	{
		return;
	}

	DisplayedPercent = FMath::FInterpTo(DisplayedPercent, TargetPercent, DeltaTime, ProgressBarInterpSpeed);

	if (bUseMaterialProgress && Material)
	{
		Material->SetScalarParameterValue(MaterialProgressParameterName, DisplayedPercent);
	}
	else
	{
		ProgressBar->SetPercent(DisplayedPercent);
	}
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
