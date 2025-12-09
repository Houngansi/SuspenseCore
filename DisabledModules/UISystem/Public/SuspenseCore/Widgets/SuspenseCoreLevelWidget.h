// SuspenseCoreLevelWidget.h
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCoreLevelWidget.generated.h"

class UTextBlock;
class UProgressBar;
class USuspenseCoreEventBus;

/**
 * USuspenseCoreLevelWidget
 *
 * Character Level Panel widget displaying:
 * - Level label and value
 * - Experience progress bar
 * - Experience current/max values
 *
 * Modular widget that can be embedded in HUD, PlayerInfo, or other widgets.
 * Uses EventBus ONLY for updates (per BestPractices.md).
 *
 * EventBus Events (subscribed):
 * - SuspenseCore.Event.Player.LevelChanged
 * - SuspenseCore.Event.Progression.Experience.Changed
 *
 * Blueprint Usage:
 * 1. Create WBP_CharacterLevelPanel from this class
 * 2. Add TextBlocks: LevelValueText (required), LevelLabelText (optional)
 * 3. Add ProgressBar: ExpProgressBar (required)
 * 4. Add TextBlocks: ExpCurrentText, ExpMaxText, or combined ExpText
 * 5. Embed in HUD or PlayerInfo widget
 */
UCLASS(Blueprintable, BlueprintType)
class UISYSTEM_API USuspenseCoreLevelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreLevelWidget(const FObjectInitializer& ObjectInitializer);

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
	 * Set level value directly (for testing/initialization).
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Level")
	void SetLevel(int32 NewLevel);

	/**
	 * Set experience values directly (for testing/initialization).
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Level")
	void SetExperience(int64 CurrentExp, int64 MaxExp);

	/**
	 * Set both level and experience values.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Level")
	void SetLevelAndExperience(int32 NewLevel, int64 CurrentExp, int64 MaxExp);

	/**
	 * Force refresh UI display.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Level")
	void RefreshDisplay();

	// ═══════════════════════════════════════════════════════════════════════════
	// GETTERS
	// ═══════════════════════════════════════════════════════════════════════════

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Level")
	int32 GetCurrentLevel() const { return CachedLevel; }

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Level")
	int64 GetCurrentExperience() const { return CachedCurrentExp; }

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Level")
	int64 GetMaxExperience() const { return CachedMaxExp; }

	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Level")
	float GetExperiencePercent() const { return TargetExpPercent; }

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when level changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Level|Events")
	void OnLevelChanged(int32 NewLevel, int32 OldLevel);

	/** Called when experience changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Level|Events")
	void OnExperienceChanged(int64 CurrentExp, int64 MaxExp, float Percent);

	/** Called when level up occurs */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Level|Events")
	void OnLevelUp(int32 NewLevel);

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// WIDGET BINDINGS - LEVEL
	// ═══════════════════════════════════════════════════════════════════════════

	/** "LEVEL" label text (optional) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LevelLabelText;

	/** Level value text (the number) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> LevelValueText;

	// ═══════════════════════════════════════════════════════════════════════════
	// WIDGET BINDINGS - EXPERIENCE
	// ═══════════════════════════════════════════════════════════════════════════

	/** Experience progress bar */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UProgressBar> ExpProgressBar;

	/** "exp" label text (optional) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ExpLabelText;

	/** Current experience text (e.g., "1500") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ExpCurrentText;

	/** Max experience text (e.g., "2000") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ExpMaxText;

	/** Combined experience text (e.g., "1500 / 2000") - alternative to separate texts */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ExpText;

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Enable smooth progress bar interpolation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Level|Animation")
	bool bSmoothProgressBar = true;

	/** Progress bar interpolation speed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Level|Animation", meta = (ClampMin = "1.0", ClampMax = "50.0", EditCondition = "bSmoothProgressBar"))
	float ProgressBarInterpSpeed = 10.0f;

	/** Show large numbers in compact format (1.5K, 2.3M) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Level|Display")
	bool bCompactNumbers = false;

	/** Format pattern for combined exp text (e.g., "{0} / {1}") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Level|Display")
	FString ExpFormatPattern = TEXT("{0} / {1}");

	/** Level format pattern (e.g., "Lv. {0}" or just "{0}") */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SuspenseCore|Level|Display")
	FString LevelFormatPattern = TEXT("{0}");

private:
	// ═══════════════════════════════════════════════════════════════════════════
	// EVENTBUS SUBSCRIPTIONS
	// ═══════════════════════════════════════════════════════════════════════════

	void SetupEventSubscriptions();
	void TeardownEventSubscriptions();

	// EventBus handlers
	void OnLevelEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnExperienceEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	// ═══════════════════════════════════════════════════════════════════════════
	// UI UPDATE
	// ═══════════════════════════════════════════════════════════════════════════

	void UpdateLevelUI();
	void UpdateExperienceUI();
	void UpdateProgressBar(float DeltaTime);
	FString FormatNumber(int64 Value) const;

	// ═══════════════════════════════════════════════════════════════════════════
	// CACHED VALUES
	// ═══════════════════════════════════════════════════════════════════════════

	int32 CachedLevel = 1;
	int64 CachedCurrentExp = 0;
	int64 CachedMaxExp = 100;

	// Display values (for smooth interpolation)
	float DisplayedExpPercent = 0.0f;
	float TargetExpPercent = 0.0f;

	// ═══════════════════════════════════════════════════════════════════════════
	// EVENTBUS
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	FSuspenseCoreSubscriptionHandle LevelEventHandle;
	FSuspenseCoreSubscriptionHandle ExperienceEventHandle;
};
