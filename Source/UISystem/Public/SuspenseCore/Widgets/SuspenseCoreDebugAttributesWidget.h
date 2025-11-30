// SuspenseCoreDebugAttributesWidget.h
// SuspenseCore - Debug Attribute Display
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SuspenseCoreDebugAttributesWidget.generated.h"

class UTextBlock;
class UVerticalBox;
class UButton;
class UComboBoxString;
class ASuspenseCorePlayerState;
class USuspenseCoreCharacterClassData;
class USuspenseCoreCharacterClassSubsystem;

/**
 * USuspenseCoreDebugAttributesWidget
 *
 * Debug widget for testing and visualizing character attributes in PIE.
 *
 * Features:
 * - Real-time attribute display (Health, Stamina, Shield, etc.)
 * - Class selection dropdown
 * - Attribute modification buttons
 * - Event log
 *
 * Usage:
 * 1. Create BP_DebugAttributes inheriting this class
 * 2. Bind UI elements in Blueprint
 * 3. Add to viewport via console command or dev menu
 *
 * Console command to show: SuspenseCore.Debug.ShowAttributes
 */
UCLASS()
class UISYSTEM_API USuspenseCoreDebugAttributesWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreDebugAttributesWidget(const FObjectInitializer& ObjectInitializer);

	// ═══════════════════════════════════════════════════════════════════════════
	// USERWIDGET INTERFACE
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API
	// ═══════════════════════════════════════════════════════════════════════════

	/** Refresh all displayed values */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Debug")
	void RefreshDisplay();

	/** Apply selected class to player */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Debug")
	void ApplySelectedClass();

	/** Apply damage for testing */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Debug")
	void TestDamage(float Amount = 25.0f);

	/** Apply healing for testing */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Debug")
	void TestHealing(float Amount = 25.0f);

	/** Consume stamina for testing */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Debug")
	void TestStaminaConsume(float Amount = 20.0f);

	/** Reset all attributes to defaults */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Debug")
	void ResetAttributes();

	/** Add log message */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Debug")
	void AddLogMessage(const FString& Message);

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Header
	// ═══════════════════════════════════════════════════════════════════════════

	/** Title text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* TitleText;

	/** Current class display */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* CurrentClassText;

	/** Class selection dropdown */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UComboBoxString* ClassSelector;

	/** Apply class button */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* ApplyClassButton;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Health Section
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* HealthValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* MaxHealthValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* HealthRegenValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* ArmorValueText;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Stamina Section
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* StaminaValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* MaxStaminaValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* StaminaRegenValueText;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Shield Section
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* ShieldValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* MaxShieldValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* ShieldRegenValueText;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Combat Section
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* AttackPowerValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* MovementSpeedValueText;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Movement Section
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* WalkSpeedValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* SprintSpeedValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* JumpHeightValueText;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Progression Section
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* LevelValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* ExperienceValueText;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Test Buttons
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* DamageButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* HealButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* StaminaConsumeButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* ResetButton;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Log Section
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UVerticalBox* LogContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* ClearLogButton;

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Update interval in seconds */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
	float UpdateInterval = 0.1f;

	/** Max log entries */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
	int32 MaxLogEntries = 20;

	// ═══════════════════════════════════════════════════════════════════════════
	// STATE
	// ═══════════════════════════════════════════════════════════════════════════

	/** Cached player state */
	UPROPERTY()
	TWeakObjectPtr<ASuspenseCorePlayerState> CachedPlayerState;

	/** Cached class subsystem */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreCharacterClassSubsystem> CachedClassSubsystem;

	/** Selected class ID */
	FName SelectedClassId;

	/** Update timer */
	float UpdateTimer = 0.0f;

	/** Log entries */
	TArray<FString> LogEntries;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL METHODS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Setup button bindings */
	void SetupButtonBindings();

	/** Populate class selector */
	void PopulateClassSelector();

	/** Get local player state */
	ASuspenseCorePlayerState* GetLocalPlayerState() const;

	/** Update health section */
	void UpdateHealthDisplay();

	/** Update stamina section */
	void UpdateStaminaDisplay();

	/** Update shield section */
	void UpdateShieldDisplay();

	/** Update combat section */
	void UpdateCombatDisplay();

	/** Update movement section */
	void UpdateMovementDisplay();

	/** Update progression section */
	void UpdateProgressionDisplay();

	/** Update current class display */
	void UpdateClassDisplay();

	/** Format value for display */
	FString FormatValue(float Value, int32 Decimals = 1) const;

	/** Format percentage for display */
	FString FormatPercent(float Current, float Max) const;

	// ═══════════════════════════════════════════════════════════════════════════
	// BUTTON HANDLERS
	// ═══════════════════════════════════════════════════════════════════════════

	UFUNCTION()
	void OnApplyClassClicked();

	UFUNCTION()
	void OnDamageClicked();

	UFUNCTION()
	void OnHealClicked();

	UFUNCTION()
	void OnStaminaConsumeClicked();

	UFUNCTION()
	void OnResetClicked();

	UFUNCTION()
	void OnClearLogClicked();

	UFUNCTION()
	void OnClassSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
};
