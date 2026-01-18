// SuspenseCoreReloadProgressWidget.h
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreReloadProgressWidget.h"
#include "SuspenseCoreReloadProgressWidget.generated.h"

// Forward declarations
class UTextBlock;
class UImage;
class UProgressBar;
class USuspenseCoreEventBus;

/**
 * USuspenseCoreReloadProgressWidget
 *
 * Tarkov-style reload progress HUD widget displaying:
 * - Reload progress bar (auto-fills based on elapsed time)
 * - Reload type text (Tactical/Empty/Emergency/Chamber)
 * - Phase indicators (Eject/Insert/Chamber) with opacity animation
 * - Time remaining countdown
 * - Cancel hint (if applicable)
 *
 * Features:
 * - Real-time updates via EventBus (NO direct component polling!)
 * - Automatic progress calculation based on elapsed time
 * - Smooth progress interpolation
 * - Phase-based visual feedback with opacity transitions
 * - ALL components are MANDATORY (BindWidget)
 * - Widget starts Collapsed, shown only during reload
 * - NO programmatic color changes - ALL colors from materials in Editor!
 *
 * Layout (UMG Designer Hierarchy):
 * ┌────────────────────────────────────────┐
 * │           TACTICAL RELOAD              │  ← ReloadTypeText
 * │              1.5s                      │  ← TimeRemainingText
 * │  ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  │  ← ReloadProgressBar
 * │  [●] EJECT → [●] INSERT → [●] CHAMBER  │  ← Phase indicators
 * │         Press [R] to cancel            │  ← CancelHintText
 * └────────────────────────────────────────┘
 *
 * EventBus Events (subscribed):
 * - TAG_Equipment_Event_Weapon_ReloadStart  → Show widget, start progress
 * - TAG_Equipment_Event_Weapon_ReloadEnd    → Hide widget
 * - TAG_Equipment_Event_Magazine_Ejected    → Phase 1 active
 * - TAG_Equipment_Event_Magazine_Inserted   → Phase 2 active
 * - TAG_Equipment_Event_Chamber_Chambered   → Phase 3 active
 */
UCLASS(Blueprintable, BlueprintType)
class UISYSTEM_API USuspenseCoreReloadProgressWidget : public UUserWidget, public ISuspenseCoreReloadProgressWidgetInterface
{
	GENERATED_BODY()

public:
	USuspenseCoreReloadProgressWidget(const FObjectInitializer& ObjectInitializer);

	// ═══════════════════════════════════════════════════════════════════════════
	// UUserWidget Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ═══════════════════════════════════════════════════════════════════════════
	// ISuspenseCoreReloadProgressWidget Implementation
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void ShowReloadProgress_Implementation(const FSuspenseCoreReloadProgressData& ReloadData) override;
	virtual void UpdateReloadProgress_Implementation(float Progress, float RemainingTime) override;
	virtual void HideReloadProgress_Implementation(bool bCompleted) override;
	virtual void OnMagazineEjected_Implementation() override;
	virtual void OnMagazineInserted_Implementation() override;
	virtual void OnChambering_Implementation() override;
	virtual void OnReloadCancelled_Implementation() override;
	virtual void SetReloadTypeDisplay_Implementation(ESuspenseCoreReloadType ReloadType, const FText& DisplayText) override;
	virtual void SetCanCancelReload_Implementation(bool bCanCancel) override;
	virtual bool IsReloadProgressVisible_Implementation() const override;
	virtual float GetCurrentReloadProgress_Implementation() const override;

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when reload starts */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Reload|Events")
	void OnReloadStarted(ESuspenseCoreReloadType ReloadType);

	/** Called when reload phase changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Reload|Events")
	void OnPhaseChanged(int32 NewPhase);

	/** Called when reload completes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Reload|Events")
	void OnReloadCompleted();

	/** Called when reload is cancelled */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Reload|Events")
	void OnReloadCancelledBP();

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - ALL MANDATORY (BindWidget) - Start Collapsed!
	// ═══════════════════════════════════════════════════════════════════════════

	// --- Main Display ---

	/** Reload type text (e.g., "TACTICAL RELOAD") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> ReloadTypeText;

	/** Reload progress bar */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UProgressBar> ReloadProgressBar;

	/** Time remaining text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> TimeRemainingText;

	// --- Phase Indicators ---

	/** Eject phase indicator */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> EjectPhaseIndicator;

	/** Insert phase indicator */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> InsertPhaseIndicator;

	/** Chamber phase indicator */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> ChamberPhaseIndicator;

	/** Eject phase text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> EjectPhaseText;

	/** Insert phase text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> InsertPhaseText;

	/** Chamber phase text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> ChamberPhaseText;

	// --- Cancel Hint ---

	/** Cancel hint text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> CancelHintText;

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Enable smooth progress interpolation */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Reload|Config")
	bool bSmoothProgress = true;

	/** Progress bar interpolation speed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Reload|Config", meta = (EditCondition = "bSmoothProgress"))
	float ProgressInterpSpeed = 20.0f;

	/** Show phase indicators (Eject/Insert/Chamber) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Reload|Config")
	bool bShowPhaseIndicators = true;

	/** Show time remaining text */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Reload|Config")
	bool bShowTimeRemaining = true;

	/** Cancel hint text format */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Reload|Config")
	FText CancelHintFormat = NSLOCTEXT("Reload", "CancelHint", "Press [R] to cancel");

	/** Reload type display texts */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Reload|Config")
	FText TacticalReloadText = NSLOCTEXT("Reload", "Tactical", "TACTICAL RELOAD");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Reload|Config")
	FText EmptyReloadText = NSLOCTEXT("Reload", "Empty", "FULL RELOAD");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Reload|Config")
	FText EmergencyReloadText = NSLOCTEXT("Reload", "Emergency", "EMERGENCY RELOAD");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Reload|Config")
	FText ChamberOnlyText = NSLOCTEXT("Reload", "Chamber", "CHAMBERING");

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
	void OnMagazineEjectedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnMagazineInsertedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnChamberEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL HELPERS
	// ═══════════════════════════════════════════════════════════════════════════

	void UpdateProgressUI();
	void UpdatePhaseIndicators(int32 CurrentPhase);
	void UpdateTimeRemainingUI();
	FText GetReloadTypeDisplayText(ESuspenseCoreReloadType ReloadType) const;

	// ═══════════════════════════════════════════════════════════════════════════
	// STATE
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	FSuspenseCoreSubscriptionHandle ReloadStartHandle;
	FSuspenseCoreSubscriptionHandle ReloadEndHandle;
	FSuspenseCoreSubscriptionHandle MagazineEjectedHandle;
	FSuspenseCoreSubscriptionHandle MagazineInsertedHandle;
	FSuspenseCoreSubscriptionHandle ChamberHandle;

	FSuspenseCoreReloadProgressData CachedReloadData;

	/** Current displayed progress (interpolated) */
	float DisplayedProgress = 0.0f;

	/** Target progress based on elapsed time */
	float TargetProgress = 0.0f;

	/** Total reload duration in seconds */
	float TotalReloadDuration = 0.0f;

	/** Elapsed time since reload started */
	float ElapsedReloadTime = 0.0f;

	/** Current phase: 0=None, 1=Eject, 2=Insert, 3=Chamber */
	int32 CurrentPhase = 0;

	/** Is reload currently in progress? */
	bool bIsReloading = false;

	/** Can this reload be cancelled? */
	bool bCanCancel = true;
};
