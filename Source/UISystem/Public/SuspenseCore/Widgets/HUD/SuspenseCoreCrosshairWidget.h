// SuspenseCoreCrosshairWidget.h
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCoreCrosshairWidget.generated.h"

// Forward declarations
class UImage;
class UCanvasPanel;
class USuspenseCoreEventBus;

/**
 * USuspenseCoreCrosshairWidget
 *
 * Dynamic crosshair HUD widget that adjusts based on weapon spread and recoil.
 *
 * Features:
 * - Real-time updates via EventBus (NO direct component polling!)
 * - Dynamic spread adjustment
 * - Hit marker display
 * - ALL components are MANDATORY (BindWidget)
 * - NO programmatic color changes - ALL colors from materials in Editor!
 *
 * Layout:
 *          ┌───┐
 *          │ T │  ← TopCrosshair
 *          └───┘
 *   ┌───┐   ·   ┌───┐
 *   │ L │   ·   │ R │  ← Left/Right Crosshair
 *   └───┘   ·   └───┘
 *          ┌───┐
 *          │ B │  ← BottomCrosshair
 *          └───┘
 *
 *      ╲       ╱
 *       ╲     ╱    ← Hit marker (shown on hit)
 *       ╱     ╲
 *      ╱       ╲
 *
 * EventBus Events (subscribed):
 * - TAG_Equipment_Event_Weapon_SpreadUpdated
 * - TAG_Equipment_Event_Weapon_Fired
 * - TAG_Equipment_Event_Visual_Effect (for hit markers)
 */
UCLASS(Blueprintable, BlueprintType)
class UISYSTEM_API USuspenseCoreCrosshairWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreCrosshairWidget(const FObjectInitializer& ObjectInitializer);

	// ═══════════════════════════════════════════════════════════════════════════
	// UUserWidget Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API
	// ═══════════════════════════════════════════════════════════════════════════

	/** Update crosshair spread */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Crosshair")
	void UpdateCrosshair(float Spread, float Recoil, bool bIsFiring);

	/** Set crosshair visibility */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Crosshair")
	void SetCrosshairVisibility(bool bVisible);

	/** Is crosshair visible? */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Crosshair")
	bool IsCrosshairVisible() const { return bCrosshairVisible; }

	/** Set crosshair type */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Crosshair")
	void SetCrosshairType(const FName& CrosshairType);

	/** Set minimum spread */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Crosshair")
	void SetMinimumSpread(float MinSpread);

	/** Set maximum spread */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Crosshair")
	void SetMaximumSpread(float MaxSpread);

	/** Set interpolation speed */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Crosshair")
	void SetInterpolationSpeed(float Speed);

	/** Show hit marker */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Crosshair")
	void ShowHitMarker(bool bHeadshot, bool bKill);

	/** Reset crosshair to base spread */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Crosshair")
	void ResetToBaseSpread();

	/** Get current spread */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Crosshair")
	float GetCurrentSpread() const { return CurrentSpreadRadius; }

	/** Is currently firing? */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Crosshair")
	bool IsFiring() const { return bCurrentlyFiring; }

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when spread changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Crosshair|Events")
	void OnSpreadChanged(float NewSpread);

	/** Called when hit marker is shown */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Crosshair|Events")
	void OnHitMarkerShown(bool bHeadshot, bool bKill);

	/** Called when hit marker is hidden */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Crosshair|Events")
	void OnHitMarkerHidden();

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - ALL MANDATORY (BindWidget)
	// ═══════════════════════════════════════════════════════════════════════════

	/** Main container for crosshair elements */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCanvasPanel> CrosshairCanvas;

	/** Center dot */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> CenterDot;

	/** Top crosshair line */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> TopCrosshair;

	/** Bottom crosshair line */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> BottomCrosshair;

	/** Left crosshair line */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> LeftCrosshair;

	/** Right crosshair line */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> RightCrosshair;

	// --- Hit Marker ---

	/** Hit marker top-left */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> HitMarkerTopLeft;

	/** Hit marker top-right */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> HitMarkerTopRight;

	/** Hit marker bottom-left */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> HitMarkerBottomLeft;

	/** Hit marker bottom-right */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> HitMarkerBottomRight;

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Length of crosshair lines in pixels */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Crosshair|Appearance")
	float CrosshairLength = 10.0f;

	/** Thickness of crosshair lines in pixels */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Crosshair|Appearance")
	float CrosshairThickness = 2.0f;

	/** Multiplier applied to spread values for UI scaling */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Crosshair|Spread")
	float SpreadMultiplier = 20.0f;

	/** Minimum spread radius in pixels */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Crosshair|Spread")
	float MinimumSpread = 5.0f;

	/** Maximum spread radius in pixels */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Crosshair|Spread")
	float MaximumSpread = 100.0f;

	/** Speed of spread increase when firing */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Crosshair|Animation")
	float SpreadInterpSpeed = 10.0f;

	/** Speed of spread recovery when not firing */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Crosshair|Animation")
	float RecoveryInterpSpeed = 15.0f;

	/** Hit marker display duration */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Crosshair|HitMarker")
	float HitMarkerDuration = 0.2f;

	/** Hit marker size */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Crosshair|HitMarker")
	float HitMarkerSize = 10.0f;

	/** Hit marker offset from center */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Crosshair|HitMarker")
	float HitMarkerOffset = 8.0f;

private:
	// ═══════════════════════════════════════════════════════════════════════════
	// EVENTBUS
	// ═══════════════════════════════════════════════════════════════════════════

	void SetupEventSubscriptions();
	void TeardownEventSubscriptions();
	USuspenseCoreEventBus* GetEventBus() const;

	// EventBus handlers
	void OnSpreadUpdatedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnWeaponFiredEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnHitConfirmedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL HELPERS
	// ═══════════════════════════════════════════════════════════════════════════

	void UpdateCrosshairPositions();
	void DisplayHitMarker(bool bHeadshot, bool bKill);
	void HideHitMarker();

	// ═══════════════════════════════════════════════════════════════════════════
	// STATE
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	FSuspenseCoreSubscriptionHandle SpreadUpdatedHandle;
	FSuspenseCoreSubscriptionHandle WeaponFiredHandle;
	FSuspenseCoreSubscriptionHandle HitConfirmedHandle;

	float CurrentSpreadRadius = 0.0f;
	float TargetSpreadRadius = 0.0f;
	float BaseSpreadRadius = 0.0f;

	bool bCurrentlyFiring = false;
	bool bWasFiring = false;
	bool bCrosshairVisible = true;

	FTimerHandle HitMarkerTimerHandle;
};
