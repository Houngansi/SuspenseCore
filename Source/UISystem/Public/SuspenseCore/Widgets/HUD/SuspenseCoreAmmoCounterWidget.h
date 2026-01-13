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
 * Tarkov-style weapon HUD widget displaying:
 * - Weapon name and icon
 * - Magazine rounds / capacity (e.g., "30/30")
 * - Chamber indicator (+1 when chambered)
 * - Reserve ammo count and available magazines
 * - Loaded ammo type (e.g., "5.45 PS")
 * - Fire mode indicator (AUTO/SEMI/BURST)
 *
 * Features:
 * - Real-time updates via EventBus (NO direct component polling!)
 * - Low/Critical ammo warnings (visual feedback from materials)
 * - Magazine swap animation support
 * - ALL components are MANDATORY (BindWidget)
 * - NO programmatic color changes - ALL colors from materials in Editor!
 *
 * Layout:
 * ┌──────────────────────────────────────┐
 * │  [ICON]  AK-74M                      │  ← Weapon icon + name
 * │  30+1 / 30    [5.45 PS]              │  ← Mag+Chamber / Capacity [AmmoType]
 * │  ░░░░░░░░░░   AUTO                   │  ← MagFill bar, FireMode
 * │  Reserve: 90  Mags: 3                │  ← Reserve info
 * └──────────────────────────────────────┘
 *
 * EventBus Events (subscribed):
 * - TAG_Equipment_Event_Magazine_Inserted
 * - TAG_Equipment_Event_Magazine_Ejected
 * - TAG_Equipment_Event_Magazine_RoundsChanged
 * - TAG_Equipment_Event_Weapon_AmmoChanged
 * - TAG_Equipment_Event_Weapon_FireModeChanged
 * - TAG_Equipment_Event_ItemEquipped (for active weapon change)
 */
UCLASS(Blueprintable, BlueprintType)
class UISYSTEM_API USuspenseCoreAmmoCounterWidget : public UUserWidget, public ISuspenseCoreAmmoCounterWidgetInterface
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

	/** Get current weapon actor */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|AmmoCounter")
	AActor* GetWeaponActor() const { return CachedWeaponActor.Get(); }

	/** Get ammo percentage (0-1) */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|AmmoCounter")
	float GetAmmoPercentage() const;

	/** Is currently reloading? */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|AmmoCounter")
	bool IsReloading() const { return bIsReloading; }

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

	/** Called when weapon changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|AmmoCounter|Events")
	void OnWeaponChanged(AActor* NewWeapon);

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - ALL MANDATORY (BindWidget)
	// ═══════════════════════════════════════════════════════════════════════════

	// --- Weapon Info ---

	/** Weapon name text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> WeaponNameText;

	/** Weapon icon */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> WeaponIcon;

	// --- Magazine Display ---

	/** Current magazine rounds text (e.g., "30") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> MagazineRoundsText;

	/** Chamber indicator text (e.g., "+1") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> ChamberIndicatorText;

	/** Magazine capacity text (e.g., "/30") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> MagazineCapacityText;

	/** Magazine fill progress bar */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UProgressBar> MagazineFillBar;

	// --- Ammo Type ---

	/** Ammo type display text (e.g., "5.45 PS") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> AmmoTypeText;

	/** Ammo type icon */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> AmmoTypeIcon;

	// --- Reserve Info ---

	/** Reserve rounds text (e.g., "90") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> ReserveRoundsText;

	/** Available magazines count text (e.g., "3") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> AvailableMagazinesText;

	// --- Fire Mode ---

	/** Fire mode text (e.g., "AUTO", "SEMI") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> FireModeText;

	/** Fire mode icon */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> FireModeIcon;

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

	/** Empty chamber text */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|AmmoCounter|Config")
	FText EmptyChamberText = NSLOCTEXT("AmmoCounter", "EmptyChamber", "+0");

	/** Show chamber indicator (+1 when round chambered) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|AmmoCounter|Config")
	bool bShowChamberIndicator = true;

	/** Show reserve ammo information section */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|AmmoCounter|Config")
	bool bShowReserveInfo = true;

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
	void OnMagazineSwappedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnMagazineRoundsChangedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnWeaponAmmoChangedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnFireModeChangedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnActiveWeaponChangedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnReloadStartEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnReloadEndEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL HELPERS
	// ═══════════════════════════════════════════════════════════════════════════

	void UpdateWeaponUI();
	void UpdateMagazineUI();
	void UpdateReserveUI();
	void UpdateFireModeUI();
	void UpdateAmmoTypeUI();
	void UpdateFillBar(float DeltaTime);
	void CheckAmmoWarnings();
	void ResetToEmptyState();

	// ═══════════════════════════════════════════════════════════════════════════
	// STATE
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	FSuspenseCoreSubscriptionHandle MagazineInsertedHandle;
	FSuspenseCoreSubscriptionHandle MagazineEjectedHandle;
	FSuspenseCoreSubscriptionHandle MagazineSwappedHandle;
	FSuspenseCoreSubscriptionHandle MagazineRoundsChangedHandle;
	FSuspenseCoreSubscriptionHandle WeaponAmmoChangedHandle;
	FSuspenseCoreSubscriptionHandle WeaponAmmoChangedGASHandle;  // BridgeSystem tag subscription (from GAS fire abilities)
	FSuspenseCoreSubscriptionHandle FireModeChangedHandle;
	FSuspenseCoreSubscriptionHandle ActiveWeaponChangedHandle;
	FSuspenseCoreSubscriptionHandle ReloadStartHandle;
	FSuspenseCoreSubscriptionHandle ReloadEndHandle;

	FSuspenseCoreAmmoCounterData CachedAmmoData;

	UPROPERTY()
	TWeakObjectPtr<AActor> CachedWeaponActor;

	float DisplayedFillPercent = 1.0f;
	float TargetFillPercent = 1.0f;

	bool bIsLowAmmo = false;
	bool bIsCriticalAmmo = false;
	bool bHasMagazine = true;
	bool bIsReloading = false;
	bool bIsInitialized = false;
};
