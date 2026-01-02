// SuspenseCoreAmmoCounterWidget.h
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreAmmoCounterWidget.h"
#include "SuspenseCoreAmmoCounterWidget.generated.h"

// Forward declarations
class UTextBlock;
class UImage;
class UProgressBar;
class USuspenseCoreEventBus;

/**
 * USuspenseCoreAmmoCounterWidget
 *
 * Tarkov-style ammo counter HUD widget displaying:
 * - Magazine rounds / capacity (e.g., "30/30")
 * - Chamber indicator (+1 when chambered)
 * - Reserve ammo count
 * - Loaded ammo type (e.g., "5.45 PS")
 * - Fire mode indicator (AUTO/SEMI/BURST)
 *
 * Features:
 * - Real-time updates via EventBus (NO direct component polling!)
 * - Low/Critical ammo warnings (visual feedback from materials)
 * - Magazine swap animation support
 * - Fire mode display
 * - NO programmatic color changes - ALL colors from materials in Editor!
 *
 * Layout:
 * ┌──────────────────────────────┐
 * │  30+1 / 30    [5.45 PS]     │  ← Mag+Chamber / Capacity [AmmoType]
 * │  ░░░░░░░░░░   AUTO          │  ← MagFill bar, FireMode
 * │  Reserve: 90  Mags: 3       │  ← Reserve info
 * └──────────────────────────────┘
 *
 * EventBus Events (subscribed):
 * - TAG_Equipment_Event_Magazine_Inserted
 * - TAG_Equipment_Event_Magazine_Ejected
 * - TAG_Equipment_Event_Magazine_RoundsChanged
 * - TAG_Equipment_Event_Weapon_AmmoChanged
 * - TAG_Equipment_Event_Weapon_FireModeChanged
 */
UCLASS(Blueprintable, BlueprintType)
class UISYSTEM_API USuspenseCoreAmmoCounterWidget : public UUserWidget, public ISuspenseCoreAmmoCounterWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreAmmoCounterWidget(const FObjectInitializer& ObjectInitializer);

	// ═══════════════════════════════════════════════════════════════════════════
	// UUserWidget Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ═══════════════════════════════════════════════════════════════════════════
	// ISuspenseCoreAmmoCounterWidget Implementation
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void InitializeWithWeapon_Implementation(AActor* WeaponActor) override;
	virtual void ClearWeapon_Implementation() override;
	virtual void UpdateAmmoCounter_Implementation(const FSuspenseCoreAmmoCounterData& AmmoData) override;
	virtual void UpdateMagazineState_Implementation(int32 CurrentRounds, int32 MaxRounds, bool bChambered) override;
	virtual void UpdateReserveAmmo_Implementation(int32 ReserveRounds, int32 AvailableMags) override;
	virtual void UpdateFireMode_Implementation(FGameplayTag FireModeTag, const FText& DisplayText) override;
	virtual void UpdateAmmoType_Implementation(FName AmmoID, const FText& DisplayName) override;
	virtual void SetLowAmmoWarning_Implementation(bool bLowAmmo, bool bCritical) override;
	virtual void SetNoMagazineState_Implementation(bool bNoMagazine) override;
	virtual void PlayAmmoConsumedAnimation_Implementation() override;
	virtual void PlayMagazineSwapAnimation_Implementation() override;
	virtual void SetAmmoCounterVisible_Implementation(bool bVisible) override;
	virtual bool IsAmmoCounterVisible_Implementation() const override;

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API
	// ═══════════════════════════════════════════════════════════════════════════

	/** Get current ammo data */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|AmmoCounter")
	FSuspenseCoreAmmoCounterData GetAmmoData() const { return CachedAmmoData; }

	/** Force refresh display */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|AmmoCounter")
	void RefreshDisplay();

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when ammo is consumed (fire) */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|AmmoCounter|Events")
	void OnAmmoConsumed(int32 RemainingRounds);

	/** Called when magazine is swapped */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|AmmoCounter|Events")
	void OnMagazineSwapped(int32 NewRounds, int32 NewCapacity);

	/** Called when ammo becomes low */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|AmmoCounter|Events")
	void OnLowAmmo(bool bCritical);

	/** Called when fire mode changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|AmmoCounter|Events")
	void OnFireModeChanged(const FText& NewFireMode);

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Main Display
	// ═══════════════════════════════════════════════════════════════════════════

	/** Current magazine rounds text (e.g., "30") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> MagazineRoundsText;

	/** Chamber indicator text (e.g., "+1") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ChamberIndicatorText;

	/** Magazine capacity text (e.g., "/30") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> MagazineCapacityText;

	/** Magazine fill progress bar */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> MagazineFillBar;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Ammo Type
	// ═══════════════════════════════════════════════════════════════════════════

	/** Ammo type display text (e.g., "5.45 PS") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> AmmoTypeText;

	/** Ammo type icon */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> AmmoTypeIcon;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Reserve
	// ═══════════════════════════════════════════════════════════════════════════

	/** Reserve rounds text (e.g., "90") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReserveRoundsText;

	/** Available magazines count text (e.g., "3") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AvailableMagazinesText;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Fire Mode
	// ═══════════════════════════════════════════════════════════════════════════

	/** Fire mode text (e.g., "AUTO", "SEMI") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> FireModeText;

	/** Fire mode icon */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> FireModeIcon;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Weapon Info
	// ═══════════════════════════════════════════════════════════════════════════

	/** Weapon name text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WeaponNameText;

	/** Weapon icon */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> WeaponIcon;

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Low ammo threshold percentage (0-1) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|AmmoCounter|Config")
	float LowAmmoThreshold = 0.25f;

	/** Critical ammo threshold percentage (0-1) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|AmmoCounter|Config")
	float CriticalAmmoThreshold = 0.1f;

	/** Critical ammo absolute threshold (rounds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|AmmoCounter|Config")
	int32 CriticalAmmoAbsolute = 3;

	/** Show chamber indicator? */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|AmmoCounter|Config")
	bool bShowChamberIndicator = true;

	/** Show reserve info? */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|AmmoCounter|Config")
	bool bShowReserveInfo = true;

	/** Enable smooth fill bar interpolation */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|AmmoCounter|Config")
	bool bSmoothFillBar = true;

	/** Fill bar interpolation speed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|AmmoCounter|Config", meta = (EditCondition = "bSmoothFillBar"))
	float FillBarInterpSpeed = 15.0f;

	/** No magazine text */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|AmmoCounter|Config")
	FText NoMagazineText = NSLOCTEXT("AmmoCounter", "NoMag", "---");

	/** Chamber indicator format */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|AmmoCounter|Config")
	FText ChamberFormat = NSLOCTEXT("AmmoCounter", "Chamber", "+1");

private:
	// ═══════════════════════════════════════════════════════════════════════════
	// EVENTBUS
	// ═══════════════════════════════════════════════════════════════════════════

	void SetupEventSubscriptions();
	void TeardownEventSubscriptions();
	USuspenseCoreEventBus* GetEventBus() const;

	// EventBus handlers
	void OnMagazineInsertedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnMagazineEjectedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnMagazineRoundsChangedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnWeaponAmmoChangedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnFireModeChangedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL HELPERS
	// ═══════════════════════════════════════════════════════════════════════════

	void UpdateMagazineUI();
	void UpdateReserveUI();
	void UpdateFireModeUI();
	void UpdateAmmoTypeUI();
	void UpdateFillBar(float DeltaTime);
	void CheckAmmoWarnings();

	// ═══════════════════════════════════════════════════════════════════════════
	// STATE
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	FSuspenseCoreSubscriptionHandle MagazineInsertedHandle;
	FSuspenseCoreSubscriptionHandle MagazineEjectedHandle;
	FSuspenseCoreSubscriptionHandle MagazineRoundsChangedHandle;
	FSuspenseCoreSubscriptionHandle WeaponAmmoChangedHandle;
	FSuspenseCoreSubscriptionHandle FireModeChangedHandle;

	FSuspenseCoreAmmoCounterData CachedAmmoData;

	UPROPERTY()
	TWeakObjectPtr<AActor> CachedWeaponActor;

	float DisplayedFillPercent = 1.0f;
	float TargetFillPercent = 1.0f;

	bool bIsLowAmmo = false;
	bool bIsCriticalAmmo = false;
	bool bHasMagazine = true;
	bool bIsInitialized = false;
};
