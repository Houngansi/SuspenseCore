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
 * - Reload progress bar
 * - Reload type text (Tactical/Empty/Emergency/Chamber)
 * - Phase indicators (Eject/Insert/Chamber)
 * - Cancel hint (if applicable)
 *
 * Features:
 * - Real-time updates via EventBus (NO direct component polling!)
 * - Smooth progress interpolation
 * - Phase-based visual feedback
 * - NO programmatic color changes - ALL colors from materials in Editor!
 *
 * Layout:
 * ┌────────────────────────────────────────┐
 * │           TACTICAL RELOAD              │  ← Reload type text
 * │  ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  │  ← Progress bar
 * │  [EJECT] → [INSERT] → [CHAMBER]        │  ← Phase indicators
 * │           Press [R] to cancel          │  ← Cancel hint
 * └────────────────────────────────────────┘
 *
 * EventBus Events (subscribed):
 * - TAG_Equipment_Event_Weapon_ReloadStart
 * - TAG_Equipment_Event_Weapon_ReloadEnd
 * - TAG_Equipment_Event_Reload_Tactical
 * - TAG_Equipment_Event_Reload_Empty
 * - TAG_Equipment_Event_Reload_Emergency
 * - TAG_Equipment_Event_Magazine_Ejected
 * - TAG_Equipment_Event_Magazine_Inserted
 * - TAG_Equipment_Event_Chamber_Chambered
 */
UCLASS(Blueprintable, BlueprintType)
class UISYSTEM_API USuspenseCoreReloadProgressWidget : public UUserWidget, public ISuspenseCoreReloadProgressWidget
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
	// UI BINDINGS - Main Display
	// ═══════════════════════════════════════════════════════════════════════════

	/** Reload type text (e.g., "TACTICAL RELOAD") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> ReloadTypeText;

	/** Reload progress bar */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UProgressBar> ReloadProgressBar;

	/** Time remaining text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TimeRemainingText;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Phase Indicators
	// ═══════════════════════════════════════════════════════════════════════════

	/** Eject phase indicator */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> EjectPhaseIndicator;

	/** Insert phase indicator */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> InsertPhaseIndicator;

	/** Chamber phase indicator */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> ChamberPhaseIndicator;

	/** Eject phase text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EjectPhaseText;

	/** Insert phase text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> InsertPhaseText;

	/** Chamber phase text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ChamberPhaseText;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Cancel Hint
	// ═══════════════════════════════════════════════════════════════════════════

	/** Cancel hint text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
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

	/** Show time remaining? */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Reload|Config")
	bool bShowTimeRemaining = true;

	/** Show phase indicators? */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Reload|Config")
	bool bShowPhaseIndicators = true;

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
	void OnReloadTacticalEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnReloadEmptyEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnReloadEmergencyEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
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
	FSuspenseCoreSubscriptionHandle ReloadTacticalHandle;
	FSuspenseCoreSubscriptionHandle ReloadEmptyHandle;
	FSuspenseCoreSubscriptionHandle ReloadEmergencyHandle;
	FSuspenseCoreSubscriptionHandle MagazineEjectedHandle;
	FSuspenseCoreSubscriptionHandle MagazineInsertedHandle;
	FSuspenseCoreSubscriptionHandle ChamberHandle;

	FSuspenseCoreReloadProgressData CachedReloadData;

	float DisplayedProgress = 0.0f;
	float TargetProgress = 0.0f;
	float CachedRemainingTime = 0.0f;

	int32 CurrentPhase = 0; // 0=None, 1=Eject, 2=Insert, 3=Chamber

	bool bIsReloading = false;
	bool bCanCancel = true;
};
