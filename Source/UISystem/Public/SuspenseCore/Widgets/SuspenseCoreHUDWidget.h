// SuspenseCoreHUDWidget.h
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCoreHUDWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UImage;
class UAbilitySystemComponent;
class USuspenseCoreEventBus;
class USuspenseCoreAttributeSet;
class USuspenseCoreShieldAttributeSet;

/**
 * USuspenseCoreHUDWidget
 *
 * In-game HUD widget displaying player vital stats:
 * - Health (HP) with progress bar and numeric value
 * - Shield with progress bar and numeric value
 * - Stamina with progress bar and numeric value
 *
 * Features:
 * - Real-time attribute updates via GAS AttributeSet binding
 * - EventBus integration for attribute change notifications
 * - Smooth progress bar animations
 * - Color coding based on values (critical health = red, etc.)
 * - Supports both local player and spectating modes
 *
 * Usage in Blueprint:
 * 1. Create Widget BP inheriting from this class
 * 2. Bind UI elements (ProgressBar, TextBlock)
 * 3. Add to PlayerController's HUD
 */
UCLASS(Blueprintable, BlueprintType)
class UISYSTEM_API USuspenseCoreHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreHUDWidget(const FObjectInitializer& ObjectInitializer);

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
	 * Bind to a specific actor's ASC for attribute display.
	 * @param Actor - Actor with AbilitySystemComponent
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|HUD")
	void BindToActor(AActor* Actor);

	/**
	 * Unbind from current actor.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|HUD")
	void UnbindFromActor();

	/**
	 * Bind to local player's pawn automatically.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|HUD")
	void BindToLocalPlayer();

	/**
	 * Force refresh all displayed values.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|HUD")
	void RefreshAllValues();

	/**
	 * Is currently bound to an actor?
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|HUD")
	bool IsBound() const { return BoundASC.IsValid(); }

	// ═══════════════════════════════════════════════════════════════════════════
	// HEALTH API
	// ═══════════════════════════════════════════════════════════════════════════

	/** Get current health value */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|HUD|Health")
	float GetCurrentHealth() const;

	/** Get max health value */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|HUD|Health")
	float GetMaxHealth() const;

	/** Get health as percentage (0-1) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|HUD|Health")
	float GetHealthPercent() const;

	// ═══════════════════════════════════════════════════════════════════════════
	// SHIELD API
	// ═══════════════════════════════════════════════════════════════════════════

	/** Get current shield value */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|HUD|Shield")
	float GetCurrentShield() const;

	/** Get max shield value */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|HUD|Shield")
	float GetMaxShield() const;

	/** Get shield as percentage (0-1) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|HUD|Shield")
	float GetShieldPercent() const;

	// ═══════════════════════════════════════════════════════════════════════════
	// STAMINA API
	// ═══════════════════════════════════════════════════════════════════════════

	/** Get current stamina value */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|HUD|Stamina")
	float GetCurrentStamina() const;

	/** Get max stamina value */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|HUD|Stamina")
	float GetMaxStamina() const;

	/** Get stamina as percentage (0-1) */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|HUD|Stamina")
	float GetStaminaPercent() const;

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
	// UI BINDINGS - HEALTH
	// ═══════════════════════════════════════════════════════════════════════════

	/** Health progress bar */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UProgressBar* HealthProgressBar;

	/** Current health text (e.g., "75") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* HealthValueText;

	/** Max health text (e.g., "100") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* MaxHealthValueText;

	/** Combined health text (e.g., "75 / 100") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* HealthText;

	/** Health icon/image */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UImage* HealthIcon;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - SHIELD
	// ═══════════════════════════════════════════════════════════════════════════

	/** Shield progress bar */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UProgressBar* ShieldProgressBar;

	/** Current shield text (e.g., "50") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* ShieldValueText;

	/** Max shield text (e.g., "100") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* MaxShieldValueText;

	/** Combined shield text (e.g., "50 / 100") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* ShieldText;

	/** Shield icon/image */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UImage* ShieldIcon;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - STAMINA
	// ═══════════════════════════════════════════════════════════════════════════

	/** Stamina progress bar */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UProgressBar* StaminaProgressBar;

	/** Current stamina text (e.g., "80") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* StaminaValueText;

	/** Max stamina text (e.g., "100") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* MaxStaminaValueText;

	/** Combined stamina text (e.g., "80 / 100") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* StaminaText;

	/** Stamina icon/image */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UImage* StaminaIcon;

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Auto-bind to local player on construct */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|HUD|Config")
	bool bAutoBindToLocalPlayer = true;

	/** Enable smooth progress bar interpolation */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|HUD|Config")
	bool bSmoothProgressBars = true;

	/** Progress bar interpolation speed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|HUD|Config", meta = (EditCondition = "bSmoothProgressBars"))
	float ProgressBarInterpSpeed = 10.0f;

	/** Threshold for critical health (0-1) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|HUD|Config")
	float CriticalHealthThreshold = 0.25f;

	/** Color for normal health bar */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|HUD|Config|Colors")
	FLinearColor HealthColorNormal = FLinearColor(0.2f, 0.8f, 0.2f, 1.0f);

	/** Color for critical health bar */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|HUD|Config|Colors")
	FLinearColor HealthColorCritical = FLinearColor(0.9f, 0.1f, 0.1f, 1.0f);

	/** Color for shield bar */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|HUD|Config|Colors")
	FLinearColor ShieldColor = FLinearColor(0.2f, 0.6f, 1.0f, 1.0f);

	/** Color for stamina bar */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|HUD|Config|Colors")
	FLinearColor StaminaColor = FLinearColor(1.0f, 0.8f, 0.2f, 1.0f);

	/** Number format pattern (e.g., "{0} / {1}" for "75 / 100") */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|HUD|Config|Format")
	FString ValueFormatPattern = TEXT("{0} / {1}");

	/** Show decimals in numeric values */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|HUD|Config|Format")
	bool bShowDecimals = false;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL STATE
	// ═══════════════════════════════════════════════════════════════════════════

	/** Cached bound ASC */
	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;

	/** Cached bound actor */
	UPROPERTY()
	TWeakObjectPtr<AActor> BoundActor;

	/** Cached EventBus */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	/** EventBus subscription handles */
	FSuspenseCoreSubscriptionHandle HealthEventHandle;
	FSuspenseCoreSubscriptionHandle ShieldEventHandle;
	FSuspenseCoreSubscriptionHandle StaminaEventHandle;

	/** Target values for smooth interpolation */
	float TargetHealthPercent = 1.0f;
	float TargetShieldPercent = 1.0f;
	float TargetStaminaPercent = 1.0f;

	/** Current displayed values for interpolation */
	float DisplayedHealthPercent = 1.0f;
	float DisplayedShieldPercent = 1.0f;
	float DisplayedStaminaPercent = 1.0f;

	/** Cached attribute values */
	float CachedHealth = 0.0f;
	float CachedMaxHealth = 100.0f;
	float CachedShield = 0.0f;
	float CachedMaxShield = 100.0f;
	float CachedStamina = 0.0f;
	float CachedMaxStamina = 100.0f;

	/** Was health critical in previous frame? */
	bool bWasHealthCritical = false;

	/** Was shield broken in previous frame? */
	bool bWasShieldBroken = false;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL METHODS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Setup GAS attribute change callbacks */
	void SetupAttributeCallbacks();

	/** Teardown GAS attribute callbacks */
	void TeardownAttributeCallbacks();

	/** Setup EventBus subscriptions */
	void SetupEventSubscriptions();

	/** Teardown EventBus subscriptions */
	void TeardownEventSubscriptions();

	/** Get EventBus instance */
	USuspenseCoreEventBus* GetEventBus() const;

	/** Update health UI elements */
	void UpdateHealthUI();

	/** Update shield UI elements */
	void UpdateShieldUI();

	/** Update stamina UI elements */
	void UpdateStaminaUI();

	/** Update progress bar with smooth interpolation */
	void UpdateProgressBar(UProgressBar* ProgressBar, float& DisplayedPercent, float TargetPercent, float DeltaTime);

	/** Apply color to progress bar based on value */
	void ApplyHealthBarColor();

	/** Format value text */
	FString FormatValueText(float Current, float Max) const;

	/** Handle attribute value changed from GAS */
	void OnAttributeValueChanged(const FOnAttributeChangeData& Data);

	/** Handle health attribute changed */
	void HandleHealthChanged(float NewValue);

	/** Handle max health attribute changed */
	void HandleMaxHealthChanged(float NewValue);

	/** Handle shield attribute changed */
	void HandleShieldChanged(float NewValue);

	/** Handle max shield attribute changed */
	void HandleMaxShieldChanged(float NewValue);

	/** Handle stamina attribute changed */
	void HandleStaminaChanged(float NewValue);

	/** Handle max stamina attribute changed */
	void HandleMaxStaminaChanged(float NewValue);

	/** Handle EventBus attribute event */
	void OnAttributeEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Delegate handles for attribute changes */
	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;
	FDelegateHandle ShieldChangedHandle;
	FDelegateHandle MaxShieldChangedHandle;
	FDelegateHandle StaminaChangedHandle;
	FDelegateHandle MaxStaminaChangedHandle;
};
