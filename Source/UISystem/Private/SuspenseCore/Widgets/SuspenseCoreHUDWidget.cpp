// SuspenseCoreHUDWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/SuspenseCoreHUDWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "SuspenseCore/Attributes/SuspenseCoreShieldAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

USuspenseCoreHUDWidget::USuspenseCoreHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USuspenseCoreHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Auto-bind to local player if configured
	if (bAutoBindToLocalPlayer)
	{
		BindToLocalPlayer();
	}

	// Setup EventBus subscriptions
	SetupEventSubscriptions();

	// Initial refresh
	RefreshAllValues();
}

void USuspenseCoreHUDWidget::NativeDestruct()
{
	UnbindFromActor();
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
			UpdateProgressBar(HealthProgressBar, DisplayedHealthPercent, TargetHealthPercent, InDeltaTime);
		}

		if (ShieldProgressBar)
		{
			UpdateProgressBar(ShieldProgressBar, DisplayedShieldPercent, TargetShieldPercent, InDeltaTime);
		}

		if (StaminaProgressBar)
		{
			UpdateProgressBar(StaminaProgressBar, DisplayedStaminaPercent, TargetStaminaPercent, InDeltaTime);
		}
	}
}

void USuspenseCoreHUDWidget::BindToActor(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	// Unbind from previous
	UnbindFromActor();

	// Get ASC from actor
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	if (!ASC)
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreHUDWidget: Actor %s has no ASC"), *Actor->GetName());
		return;
	}

	BoundActor = Actor;
	BoundASC = ASC;

	// Setup attribute change callbacks
	SetupAttributeCallbacks();

	// Initial refresh
	RefreshAllValues();

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreHUDWidget: Bound to %s"), *Actor->GetName());
}

void USuspenseCoreHUDWidget::UnbindFromActor()
{
	TeardownAttributeCallbacks();
	BoundActor.Reset();
	BoundASC.Reset();
}

void USuspenseCoreHUDWidget::BindToLocalPlayer()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	APawn* Pawn = PC->GetPawn();
	if (Pawn)
	{
		BindToActor(Pawn);
	}
}

void USuspenseCoreHUDWidget::RefreshAllValues()
{
	if (!BoundASC.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* ASC = BoundASC.Get();

	// Get Health AttributeSet values
	const USuspenseCoreAttributeSet* HealthSet = ASC->GetSet<USuspenseCoreAttributeSet>();
	if (HealthSet)
	{
		CachedHealth = HealthSet->GetHealth();
		CachedMaxHealth = HealthSet->GetMaxHealth();
		CachedStamina = HealthSet->GetStamina();
		CachedMaxStamina = HealthSet->GetMaxStamina();
	}

	// Get Shield AttributeSet values
	const USuspenseCoreShieldAttributeSet* ShieldSet = ASC->GetSet<USuspenseCoreShieldAttributeSet>();
	if (ShieldSet)
	{
		CachedShield = ShieldSet->GetShield();
		CachedMaxShield = ShieldSet->GetMaxShield();
	}

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

void USuspenseCoreHUDWidget::SetupAttributeCallbacks()
{
	if (!BoundASC.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* ASC = BoundASC.Get();

	// Health attribute callbacks
	HealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
		USuspenseCoreAttributeSet::GetHealthAttribute()).AddUObject(
			this, &USuspenseCoreHUDWidget::OnAttributeValueChanged);

	MaxHealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
		USuspenseCoreAttributeSet::GetMaxHealthAttribute()).AddUObject(
			this, &USuspenseCoreHUDWidget::OnAttributeValueChanged);

	// Stamina attribute callbacks
	StaminaChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
		USuspenseCoreAttributeSet::GetStaminaAttribute()).AddUObject(
			this, &USuspenseCoreHUDWidget::OnAttributeValueChanged);

	MaxStaminaChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
		USuspenseCoreAttributeSet::GetMaxStaminaAttribute()).AddUObject(
			this, &USuspenseCoreHUDWidget::OnAttributeValueChanged);

	// Shield attribute callbacks
	ShieldChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
		USuspenseCoreShieldAttributeSet::GetShieldAttribute()).AddUObject(
			this, &USuspenseCoreHUDWidget::OnAttributeValueChanged);

	MaxShieldChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
		USuspenseCoreShieldAttributeSet::GetMaxShieldAttribute()).AddUObject(
			this, &USuspenseCoreHUDWidget::OnAttributeValueChanged);
}

void USuspenseCoreHUDWidget::TeardownAttributeCallbacks()
{
	if (!BoundASC.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* ASC = BoundASC.Get();

	// Remove Health callbacks
	if (HealthChangedHandle.IsValid())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(
			USuspenseCoreAttributeSet::GetHealthAttribute()).Remove(HealthChangedHandle);
		HealthChangedHandle.Reset();
	}

	if (MaxHealthChangedHandle.IsValid())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(
			USuspenseCoreAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthChangedHandle);
		MaxHealthChangedHandle.Reset();
	}

	// Remove Stamina callbacks
	if (StaminaChangedHandle.IsValid())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(
			USuspenseCoreAttributeSet::GetStaminaAttribute()).Remove(StaminaChangedHandle);
		StaminaChangedHandle.Reset();
	}

	if (MaxStaminaChangedHandle.IsValid())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(
			USuspenseCoreAttributeSet::GetMaxStaminaAttribute()).Remove(MaxStaminaChangedHandle);
		MaxStaminaChangedHandle.Reset();
	}

	// Remove Shield callbacks
	if (ShieldChangedHandle.IsValid())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(
			USuspenseCoreShieldAttributeSet::GetShieldAttribute()).Remove(ShieldChangedHandle);
		ShieldChangedHandle.Reset();
	}

	if (MaxShieldChangedHandle.IsValid())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(
			USuspenseCoreShieldAttributeSet::GetMaxShieldAttribute()).Remove(MaxShieldChangedHandle);
		MaxShieldChangedHandle.Reset();
	}
}

void USuspenseCoreHUDWidget::SetupEventSubscriptions()
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

	// Subscribe to GAS attribute events
	HealthEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.GAS.Attribute.Health")),
		const_cast<USuspenseCoreHUDWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreHUDWidget::OnAttributeEvent),
		ESuspenseCoreEventPriority::Normal
	);

	ShieldEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.GAS.Attribute")),
		const_cast<USuspenseCoreHUDWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCoreHUDWidget::OnAttributeEvent),
		ESuspenseCoreEventPriority::Normal
	);
}

void USuspenseCoreHUDWidget::TeardownEventSubscriptions()
{
	if (CachedEventBus.IsValid())
	{
		if (HealthEventHandle.IsValid())
		{
			CachedEventBus->Unsubscribe(HealthEventHandle);
		}
		if (ShieldEventHandle.IsValid())
		{
			CachedEventBus->Unsubscribe(ShieldEventHandle);
		}
		if (StaminaEventHandle.IsValid())
		{
			CachedEventBus->Unsubscribe(StaminaEventHandle);
		}
	}
}

USuspenseCoreEventBus* USuspenseCoreHUDWidget::GetEventBus() const
{
	return CachedEventBus.Get();
}

void USuspenseCoreHUDWidget::OnAttributeValueChanged(const FOnAttributeChangeData& Data)
{
	// Determine which attribute changed and update accordingly
	if (Data.Attribute == USuspenseCoreAttributeSet::GetHealthAttribute())
	{
		HandleHealthChanged(Data.NewValue);
	}
	else if (Data.Attribute == USuspenseCoreAttributeSet::GetMaxHealthAttribute())
	{
		HandleMaxHealthChanged(Data.NewValue);
	}
	else if (Data.Attribute == USuspenseCoreAttributeSet::GetStaminaAttribute())
	{
		HandleStaminaChanged(Data.NewValue);
	}
	else if (Data.Attribute == USuspenseCoreAttributeSet::GetMaxStaminaAttribute())
	{
		HandleMaxStaminaChanged(Data.NewValue);
	}
	else if (Data.Attribute == USuspenseCoreShieldAttributeSet::GetShieldAttribute())
	{
		HandleShieldChanged(Data.NewValue);
	}
	else if (Data.Attribute == USuspenseCoreShieldAttributeSet::GetMaxShieldAttribute())
	{
		HandleMaxShieldChanged(Data.NewValue);
	}
}

void USuspenseCoreHUDWidget::HandleHealthChanged(float NewValue)
{
	float OldHealth = CachedHealth;
	CachedHealth = NewValue;

	TargetHealthPercent = (CachedMaxHealth > 0.0f) ? (CachedHealth / CachedMaxHealth) : 0.0f;

	UpdateHealthUI();

	// Broadcast events
	OnHealthChanged(NewValue, CachedMaxHealth, OldHealth);

	// Check for critical health
	bool bIsCritical = TargetHealthPercent <= CriticalHealthThreshold && CachedHealth > 0.0f;
	if (bIsCritical && !bWasHealthCritical)
	{
		OnHealthCritical();
	}
	bWasHealthCritical = bIsCritical;
}

void USuspenseCoreHUDWidget::HandleMaxHealthChanged(float NewValue)
{
	CachedMaxHealth = NewValue;
	TargetHealthPercent = (CachedMaxHealth > 0.0f) ? (CachedHealth / CachedMaxHealth) : 0.0f;
	UpdateHealthUI();
}

void USuspenseCoreHUDWidget::HandleShieldChanged(float NewValue)
{
	float OldShield = CachedShield;
	CachedShield = NewValue;

	TargetShieldPercent = (CachedMaxShield > 0.0f) ? (CachedShield / CachedMaxShield) : 0.0f;

	UpdateShieldUI();

	// Broadcast events
	OnShieldChanged(NewValue, CachedMaxShield, OldShield);

	// Check for shield broken
	bool bIsBroken = CachedShield <= 0.0f && CachedMaxShield > 0.0f;
	if (bIsBroken && !bWasShieldBroken)
	{
		OnShieldBroken();
	}
	bWasShieldBroken = bIsBroken;
}

void USuspenseCoreHUDWidget::HandleMaxShieldChanged(float NewValue)
{
	CachedMaxShield = NewValue;
	TargetShieldPercent = (CachedMaxShield > 0.0f) ? (CachedShield / CachedMaxShield) : 0.0f;
	UpdateShieldUI();
}

void USuspenseCoreHUDWidget::HandleStaminaChanged(float NewValue)
{
	float OldStamina = CachedStamina;
	CachedStamina = NewValue;

	TargetStaminaPercent = (CachedMaxStamina > 0.0f) ? (CachedStamina / CachedMaxStamina) : 0.0f;

	UpdateStaminaUI();

	// Broadcast events
	OnStaminaChanged(NewValue, CachedMaxStamina, OldStamina);
}

void USuspenseCoreHUDWidget::HandleMaxStaminaChanged(float NewValue)
{
	CachedMaxStamina = NewValue;
	TargetStaminaPercent = (CachedMaxStamina > 0.0f) ? (CachedStamina / CachedMaxStamina) : 0.0f;
	UpdateStaminaUI();
}

void USuspenseCoreHUDWidget::OnAttributeEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// Refresh all values when we receive attribute events
	RefreshAllValues();
}

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

// ═══════════════════════════════════════════════════════════════════════════
// PUBLIC GETTERS
// ═══════════════════════════════════════════════════════════════════════════

float USuspenseCoreHUDWidget::GetCurrentHealth() const
{
	return CachedHealth;
}

float USuspenseCoreHUDWidget::GetMaxHealth() const
{
	return CachedMaxHealth;
}

float USuspenseCoreHUDWidget::GetHealthPercent() const
{
	return TargetHealthPercent;
}

float USuspenseCoreHUDWidget::GetCurrentShield() const
{
	return CachedShield;
}

float USuspenseCoreHUDWidget::GetMaxShield() const
{
	return CachedMaxShield;
}

float USuspenseCoreHUDWidget::GetShieldPercent() const
{
	return TargetShieldPercent;
}

float USuspenseCoreHUDWidget::GetCurrentStamina() const
{
	return CachedStamina;
}

float USuspenseCoreHUDWidget::GetMaxStamina() const
{
	return CachedMaxStamina;
}

float USuspenseCoreHUDWidget::GetStaminaPercent() const
{
	return TargetStaminaPercent;
}
