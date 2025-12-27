// SuspenseCoreGameHUDWidget.h
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCoreGameHUDWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UImage;
class USuspenseCoreEventBus;
class UMaterialInstanceDynamic;

/**
 * USuspenseCoreGameHUDWidget
 *
 * In-game HUD widget displaying player vital stats:
 * - Health (HP) with progress bar and numeric value
 * - Shield with progress bar and numeric value (optional)
 * - Stamina with progress bar and numeric value (optional)
 *
 * Features:
 * - Real-time updates via EventBus (NO direct GAS delegates!)
 * - Smooth progress bar animations with configurable interpolation speed
 * - Numeric value display showing current/max values
 * - NO programmatic color changes - ALL colors come from materials assigned in Editor!
 *
 * IMPORTANT - Material-Based Colors:
 * This widget does NOT set any colors programmatically. All visual styling
 * (colors, gradients, effects) must be configured via materials assigned to
 * the ProgressBar widgets in the UMG Editor. This allows designers full control
 * over the visual appearance without code changes.
 *
 * EventBus Events (subscribed):
 * - SuspenseCore.Event.GAS.Attribute.Health
 * - SuspenseCore.Event.GAS.Attribute.MaxHealth
 * - SuspenseCore.Event.GAS.Attribute.Shield
 * - SuspenseCore.Event.GAS.Attribute.MaxShield
 * - SuspenseCore.Event.GAS.Attribute.Stamina
 * - SuspenseCore.Event.GAS.Attribute.MaxStamina
 * - SuspenseCore.Event.Player.LowHealth
 * - SuspenseCore.Event.GAS.Shield.Broken
 *
 * Usage in Blueprint:
 * 1. Create Widget BP inheriting from this class
 * 2. Bind UI elements (ProgressBar, TextBlock)
 * 3. Assign materials to ProgressBars for desired colors/effects
 * 4. Add to PlayerController's HUD
 */
UCLASS(Blueprintable, BlueprintType)
class UISYSTEM_API USuspenseCoreGameHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreGameHUDWidget(const FObjectInitializer& ObjectInitializer);

	// ═══════════════════════════════════════════════════════════════════════════
	// UUserWidget Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Force refresh all displayed values by requesting current state.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|HUD")
	void RefreshAllValues();

	/**
	 * Set health values directly (for testing/initialization).
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|HUD")
	void SetHealthValues(float Current, float Max);

	/**
	 * Set shield values directly (for testing/initialization).
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|HUD")
	void SetShieldValues(float Current, float Max);

	/**
	 * Set stamina values directly (for testing/initialization).
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|HUD")
	void SetStaminaValues(float Current, float Max);

	// ═══════════════════════════════════════════════════════════════════════════
	// GETTERS
	// ═══════════════════════════════════════════════════════════════════════════

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|HUD|Health")
	float GetCurrentHealth() const { return CachedHealth; }

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|HUD|Health")
	float GetMaxHealth() const { return CachedMaxHealth; }

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|HUD|Health")
	float GetHealthPercent() const { return TargetHealthPercent; }

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|HUD|Shield")
	float GetCurrentShield() const { return CachedShield; }

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|HUD|Shield")
	float GetMaxShield() const { return CachedMaxShield; }

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|HUD|Shield")
	float GetShieldPercent() const { return TargetShieldPercent; }

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|HUD|Stamina")
	float GetCurrentStamina() const { return CachedStamina; }

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|HUD|Stamina")
	float GetMaxStamina() const { return CachedMaxStamina; }

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|HUD|Stamina")
	float GetStaminaPercent() const { return TargetStaminaPercent; }

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when health changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|HUD|Events")
	void OnHealthChanged(float NewHealth, float MaxHealth, float OldHealth);

	/** Called when shield changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|HUD|Events")
	void OnShieldChanged(float NewShield, float MaxShield, float OldShield);

	/** Called when stamina changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|HUD|Events")
	void OnStaminaChanged(float NewStamina, float MaxStamina, float OldStamina);

	/** Called when health becomes critical (<25%) */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|HUD|Events")
	void OnHealthCritical();

	/** Called when shield is broken (reaches 0) */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|HUD|Events")
	void OnShieldBroken();

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - HEALTH (HealthProgressBar is REQUIRED!)
	// NOTE: All colors come from materials assigned in Editor - no code color changes!
	// ═══════════════════════════════════════════════════════════════════════════

	/** Health progress bar - REQUIRED! Colors from material assigned in Editor */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthProgressBar;

	/** Current health text (e.g., "75") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthValueText;

	/** Max health text (e.g., "100") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MaxHealthValueText;

	/** Combined health text (e.g., "75 / 100") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText;

	/** Health icon/image */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> HealthIcon;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - SHIELD (Optional)
	// NOTE: All colors come from materials assigned in Editor - no code color changes!
	// ═══════════════════════════════════════════════════════════════════════════

	/** Shield progress bar - colors from material assigned in Editor */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ShieldProgressBar;

	/** Current shield text (e.g., "50") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ShieldValueText;

	/** Max shield text (e.g., "100") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MaxShieldValueText;

	/** Combined shield text (e.g., "50 / 100") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ShieldText;

	/** Shield icon/image */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> ShieldIcon;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - STAMINA (Optional)
	// NOTE: All colors come from materials assigned in Editor - no code color changes!
	// ═══════════════════════════════════════════════════════════════════════════

	/** Stamina progress bar - colors from material assigned in Editor */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> StaminaProgressBar;

	/** Current stamina text (e.g., "80") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StaminaValueText;

	/** Max stamina text (e.g., "100") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MaxStaminaValueText;

	/** Combined stamina text (e.g., "80 / 100") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StaminaText;

	/** Stamina icon/image */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> StaminaIcon;

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Enable smooth progress bar interpolation */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|HUD|Config")
	bool bSmoothProgressBars = true;

	/** Progress bar interpolation speed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|HUD|Config", meta = (EditCondition = "bSmoothProgressBars"))
	float ProgressBarInterpSpeed = 10.0f;

	/** Use material parameter for progress bars instead of SetPercent() */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|HUD|Config")
	bool bUseMaterialProgress = true;

	/** Material parameter name for progress value (0-1) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|HUD|Config", meta = (EditCondition = "bUseMaterialProgress"))
	FName MaterialProgressParameterName = FName("Progress");

	/** Threshold for critical health (0-1) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|HUD|Config")
	float CriticalHealthThreshold = 0.25f;

	/** Number format pattern (e.g., "{0} / {1}" for "75 / 100") */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|HUD|Config|Format")
	FString ValueFormatPattern = TEXT("{0} / {1}");

	/** Show decimals in numeric values */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|HUD|Config|Format")
	bool bShowDecimals = false;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL STATE
	// ═══════════════════════════════════════════════════════════════════════════

	/** Cached EventBus */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	/** EventBus subscription handles */
	FSuspenseCoreSubscriptionHandle HealthEventHandle;
	FSuspenseCoreSubscriptionHandle MaxHealthEventHandle;
	FSuspenseCoreSubscriptionHandle ShieldEventHandle;
	FSuspenseCoreSubscriptionHandle MaxShieldEventHandle;
	FSuspenseCoreSubscriptionHandle StaminaEventHandle;
	FSuspenseCoreSubscriptionHandle MaxStaminaEventHandle;
	FSuspenseCoreSubscriptionHandle LowHealthEventHandle;
	FSuspenseCoreSubscriptionHandle ShieldBrokenEventHandle;

	/** Target values for smooth interpolation */
	float TargetHealthPercent = 1.0f;
	float TargetShieldPercent = 1.0f;
	float TargetStaminaPercent = 1.0f;

	/** Current displayed values for interpolation */
	float DisplayedHealthPercent = 1.0f;
	float DisplayedShieldPercent = 1.0f;
	float DisplayedStaminaPercent = 1.0f;

	/** Cached attribute values */
	float CachedHealth = 100.0f;
	float CachedMaxHealth = 100.0f;
	float CachedShield = 100.0f;
	float CachedMaxShield = 100.0f;
	float CachedStamina = 100.0f;
	float CachedMaxStamina = 100.0f;

	/** Was health critical in previous frame? */
	bool bWasHealthCritical = false;

	/** Was shield broken in previous frame? */
	bool bWasShieldBroken = false;

	/** Dynamic material instances for progress bars (when using material-based progress) */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> HealthProgressMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ShieldProgressMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> StaminaProgressMaterial;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL METHODS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Setup EventBus subscriptions - the ONLY way to receive attribute updates! */
	void SetupEventSubscriptions();

	/** Teardown EventBus subscriptions */
	void TeardownEventSubscriptions();

	/** Update health UI elements */
	void UpdateHealthUI();

	/** Update shield UI elements */
	void UpdateShieldUI();

	/** Update stamina UI elements */
	void UpdateStaminaUI();

	/** Update progress bar with smooth interpolation (supports material-based progress) */
	void UpdateProgressBar(UProgressBar* ProgressBar, UMaterialInstanceDynamic* Material, float& DisplayedPercent, float TargetPercent, float DeltaTime);

	/** Format value text */
	FString FormatValueText(float Current, float Max) const;

	// ═══════════════════════════════════════════════════════════════════════════
	// EVENTBUS HANDLERS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Handle Health attribute event from EventBus */
	void OnHealthEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Handle MaxHealth attribute event from EventBus */
	void OnMaxHealthEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Handle Shield attribute event from EventBus */
	void OnShieldEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Handle MaxShield attribute event from EventBus */
	void OnMaxShieldEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Handle Stamina attribute event from EventBus */
	void OnStaminaEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Handle MaxStamina attribute event from EventBus */
	void OnMaxStaminaEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Handle LowHealth event from EventBus */
	void OnLowHealthEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Handle ShieldBroken event from EventBus */
	void OnShieldBrokenEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
};
