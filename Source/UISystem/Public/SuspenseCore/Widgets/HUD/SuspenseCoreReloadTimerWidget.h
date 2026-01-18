// SuspenseCoreReloadTimerWidget.h
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCoreReloadTimerWidget.generated.h"

// Forward declarations
class UTextBlock;
class UProgressBar;
class USuspenseCoreEventBus;

/**
 * USuspenseCoreReloadTimerWidget
 *
 * Simple HUD widget displaying reload timer:
 * - Circular or linear progress bar
 * - Time remaining text
 * - Reload type indicator
 *
 * Features:
 * - Real-time updates via EventBus (NO direct component polling!)
 * - Smooth progress interpolation
 * - Auto show/hide on reload start/end
 * - ALL components are MANDATORY (BindWidget)
 * - Widget starts Collapsed, shown only during reload
 * - NO programmatic color changes - ALL colors from materials in Editor!
 *
 * Layout:
 * ┌─────────────────────┐
 * │   RELOADING...      │  ← Status text
 * │      2.3s           │  ← Time remaining
 * │  ████████░░░░░░░░   │  ← Progress bar
 * └─────────────────────┘
 *
 * EventBus Events (subscribed):
 * - TAG_Equipment_Event_Weapon_ReloadStart
 * - TAG_Equipment_Event_Weapon_ReloadEnd
 *
 * @see USuspenseCoreReloadProgressWidget for phase-based reload display
 */
UCLASS(Blueprintable, BlueprintType)
class UISYSTEM_API USuspenseCoreReloadTimerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreReloadTimerWidget(const FObjectInitializer& ObjectInitializer);

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
	 * Show the reload timer
	 * @param Duration Total reload duration in seconds
	 * @param ReloadType Type of reload (Tactical/Empty/Emergency/ChamberOnly)
	 * @param bCanCancel Whether reload can be cancelled
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|ReloadTimer")
	void ShowReloadTimer(float Duration, ESuspenseCoreReloadType ReloadType, bool bCanCancel = true);

	/**
	 * Hide the reload timer
	 * @param bCompleted true if reload completed successfully, false if cancelled
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|ReloadTimer")
	void HideReloadTimer(bool bCompleted);

	/**
	 * Update progress manually (if not using tick-based interpolation)
	 * @param Progress 0.0 to 1.0
	 * @param RemainingTime Time remaining in seconds
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|ReloadTimer")
	void UpdateProgress(float Progress, float RemainingTime);

	/**
	 * Check if timer is currently visible
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|ReloadTimer")
	bool IsTimerVisible() const;

	/**
	 * Get current progress (0.0 to 1.0)
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|ReloadTimer")
	float GetCurrentProgress() const { return DisplayedProgress; }

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when reload timer starts */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|ReloadTimer|Events")
	void OnTimerStarted(ESuspenseCoreReloadType ReloadType);

	/** Called when reload timer completes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|ReloadTimer|Events")
	void OnTimerCompleted();

	/** Called when reload is cancelled */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|ReloadTimer|Events")
	void OnTimerCancelled();

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - ALL MANDATORY (BindWidget) - Start Collapsed!
	// ═══════════════════════════════════════════════════════════════════════════

	/** Status text (e.g., "RELOADING..." or "TACTICAL RELOAD") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusText;

	/** Time remaining text (e.g., "2.3s") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> TimeRemainingText;

	/** Reload progress bar */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UProgressBar> ReloadProgressBar;

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Enable smooth progress interpolation */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|ReloadTimer|Config")
	bool bSmoothProgress = true;

	/** Progress bar interpolation speed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|ReloadTimer|Config", meta = (EditCondition = "bSmoothProgress"))
	float ProgressInterpSpeed = 15.0f;

	/** Show time remaining text */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|ReloadTimer|Config")
	bool bShowTimeRemaining = true;

	/** Status text for different reload types */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|ReloadTimer|Config")
	FText TacticalReloadText = NSLOCTEXT("ReloadTimer", "Tactical", "TACTICAL RELOAD");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|ReloadTimer|Config")
	FText EmptyReloadText = NSLOCTEXT("ReloadTimer", "Empty", "RELOADING...");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|ReloadTimer|Config")
	FText EmergencyReloadText = NSLOCTEXT("ReloadTimer", "Emergency", "EMERGENCY RELOAD");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|ReloadTimer|Config")
	FText ChamberOnlyText = NSLOCTEXT("ReloadTimer", "Chamber", "CHAMBERING");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|ReloadTimer|Config")
	FText DefaultReloadText = NSLOCTEXT("ReloadTimer", "Default", "RELOADING...");

private:
	// ═══════════════════════════════════════════════════════════════════════════
	// EVENTBUS
	// ═══════════════════════════════════════════════════════════════════════════

	void SetupEventSubscriptions();
	void TeardownEventSubscriptions();
	USuspenseCoreEventBus* GetEventBus() const;

	// EventBus handlers
	void OnReloadStartEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnReloadEndEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL HELPERS
	// ═══════════════════════════════════════════════════════════════════════════

	void UpdateUI();
	FText GetStatusTextForReloadType(ESuspenseCoreReloadType ReloadType) const;

	// ═══════════════════════════════════════════════════════════════════════════
	// STATE
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	FSuspenseCoreSubscriptionHandle ReloadStartHandle;
	FSuspenseCoreSubscriptionHandle ReloadEndHandle;

	float TotalDuration = 0.0f;
	float ElapsedTime = 0.0f;
	float DisplayedProgress = 0.0f;
	float TargetProgress = 0.0f;

	ESuspenseCoreReloadType CurrentReloadType = ESuspenseCoreReloadType::None;

	bool bIsReloading = false;
	bool bCanCancel = true;
};
