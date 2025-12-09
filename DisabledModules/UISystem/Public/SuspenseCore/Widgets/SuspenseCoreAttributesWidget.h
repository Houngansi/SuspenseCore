// SuspenseCoreAttributesWidget.h
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCoreAttributesWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UImage;
class USuspenseCoreEventBus;

/**
 * USuspenseCoreAttributesWidget
 *
 * Displays Health, Shield, and Stamina with progress bars and text.
 * Uses EventBus ONLY for attribute updates (per BestPractices.md).
 *
 * EventBus Events (subscribed):
 * - SuspenseCore.Event.GAS.Attribute.Health
 * - SuspenseCore.Event.GAS.Attribute.MaxHealth
 * - SuspenseCore.Event.GAS.Attribute.Shield
 * - SuspenseCore.Event.GAS.Attribute.MaxShield
 * - SuspenseCore.Event.GAS.Attribute.Stamina
 * - SuspenseCore.Event.GAS.Attribute.MaxStamina
 *
 * Blueprint Usage:
 * 1. Create WBP_Attributes from this class
 * 2. Add ProgressBars named: HealthBar, ShieldBar, StaminaBar
 * 3. Add TextBlocks named: HealthText, ShieldText, StaminaText
 * 4. Add to HUD widget or viewport
 */
UCLASS()
class UISYSTEM_API USuspenseCoreAttributesWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreAttributesWidget(const FObjectInitializer& ObjectInitializer);

	// ═══════════════════════════════════════════════════════════════════════════
	// WIDGET LIFECYCLE
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API
	// ═══════════════════════════════════════════════════════════════════════════

	/** Force refresh all values */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Attributes")
	void RefreshAllValues();

	/** Manually set health values */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Attributes")
	void SetHealthValues(float Current, float Max);

	/** Manually set shield values */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Attributes")
	void SetShieldValues(float Current, float Max);

	/** Manually set stamina values */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Attributes")
	void SetStaminaValues(float Current, float Max);

	// Getters
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Attributes")
	float GetCurrentHealth() const { return CachedHealth; }

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Attributes")
	float GetMaxHealth() const { return CachedMaxHealth; }

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Attributes")
	float GetHealthPercent() const { return (CachedMaxHealth > 0.f) ? (CachedHealth / CachedMaxHealth) : 0.f; }

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Attributes")
	float GetCurrentShield() const { return CachedShield; }

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Attributes")
	float GetMaxShield() const { return CachedMaxShield; }

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Attributes")
	float GetShieldPercent() const { return (CachedMaxShield > 0.f) ? (CachedShield / CachedMaxShield) : 0.f; }

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Attributes")
	float GetCurrentStamina() const { return CachedStamina; }

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Attributes")
	float GetMaxStamina() const { return CachedMaxStamina; }

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Attributes")
	float GetStaminaPercent() const { return (CachedMaxStamina > 0.f) ? (CachedStamina / CachedMaxStamina) : 0.f; }

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when health changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Attributes|Events")
	void OnHealthChanged(float NewHealth, float MaxHealth, float OldHealth);

	/** Called when shield changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Attributes|Events")
	void OnShieldChanged(float NewShield, float MaxShield, float OldShield);

	/** Called when stamina changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Attributes|Events")
	void OnStaminaChanged(float NewStamina, float MaxStamina, float OldStamina);

	/** Called when health becomes critical (<25%) */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Attributes|Events")
	void OnHealthCritical();

	/** Called when shield is broken (reaches 0) */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Attributes|Events")
	void OnShieldBroken();

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// WIDGET BINDINGS - HEALTH
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MaxHealthValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> HealthIcon;

	// ═══════════════════════════════════════════════════════════════════════════
	// WIDGET BINDINGS - SHIELD
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ShieldBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ShieldText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ShieldValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MaxShieldValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> ShieldIcon;

	// ═══════════════════════════════════════════════════════════════════════════
	// WIDGET BINDINGS - STAMINA
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> StaminaBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StaminaText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StaminaValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MaxStaminaValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> StaminaIcon;

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Enable smooth progress bar interpolation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Attributes|Animation")
	bool bSmoothProgressBars = true;

	/** Progress bar interpolation speed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Attributes|Animation", meta = (ClampMin = "1.0", ClampMax = "50.0"))
	float ProgressBarInterpSpeed = 10.0f;

	/** Show decimal values in text */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Attributes|Display")
	bool bShowDecimals = false;

	/** Format pattern for combined text (e.g., "{0} / {1}") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Attributes|Display")
	FString ValueFormatPattern = TEXT("{0} / {1}");

	/** Threshold for critical health (default: 0.25 = 25%) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Attributes|Thresholds", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CriticalHealthThreshold = 0.25f;

private:
	// ═══════════════════════════════════════════════════════════════════════════
	// EVENTBUS SUBSCRIPTIONS
	// ═══════════════════════════════════════════════════════════════════════════

	void SetupEventSubscriptions();
	void TeardownEventSubscriptions();

	// EventBus handlers
	void OnHealthEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnMaxHealthEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnShieldEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnMaxShieldEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnStaminaEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnMaxStaminaEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	// ═══════════════════════════════════════════════════════════════════════════
	// UI UPDATE
	// ═══════════════════════════════════════════════════════════════════════════

	void UpdateHealthUI();
	void UpdateShieldUI();
	void UpdateStaminaUI();
	void UpdateProgressBar(UProgressBar* Bar, float& DisplayedPercent, float TargetPercent, float DeltaTime);
	FString FormatValueText(float Current, float Max) const;

	// ═══════════════════════════════════════════════════════════════════════════
	// CACHED VALUES
	// ═══════════════════════════════════════════════════════════════════════════

	float CachedHealth = 100.0f;
	float CachedMaxHealth = 100.0f;
	float CachedShield = 0.0f;
	float CachedMaxShield = 100.0f;
	float CachedStamina = 100.0f;
	float CachedMaxStamina = 100.0f;

	// Display values (for smooth interpolation)
	float DisplayedHealthPercent = 1.0f;
	float DisplayedShieldPercent = 0.0f;
	float DisplayedStaminaPercent = 1.0f;

	float TargetHealthPercent = 1.0f;
	float TargetShieldPercent = 0.0f;
	float TargetStaminaPercent = 1.0f;

	// State tracking
	bool bWasHealthCritical = false;
	bool bWasShieldBroken = true;

	// ═══════════════════════════════════════════════════════════════════════════
	// EVENTBUS
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	FSuspenseCoreSubscriptionHandle HealthEventHandle;
	FSuspenseCoreSubscriptionHandle MaxHealthEventHandle;
	FSuspenseCoreSubscriptionHandle ShieldEventHandle;
	FSuspenseCoreSubscriptionHandle MaxShieldEventHandle;
	FSuspenseCoreSubscriptionHandle StaminaEventHandle;
	FSuspenseCoreSubscriptionHandle MaxStaminaEventHandle;
};
