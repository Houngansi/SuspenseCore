// SuspenseCoreMasterHUDWidget.h
// SuspenseCore - Master HUD Widget
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreMasterHUDWidget.generated.h"

// Forward declarations
class USuspenseCoreGameHUDWidget;
class USuspenseCoreAmmoCounterWidget;
class USuspenseCoreCrosshairWidget;
class USuspenseCoreQuickSlotHUDWidget;
class USuspenseCoreReloadProgressWidget;
class UCanvasPanel;
class UOverlay;

/**
 * USuspenseCoreMasterHUDWidget
 *
 * Master HUD widget that aggregates all in-game HUD elements:
 * - Vitals (Health, Shield, Stamina) via GameHUDWidget
 * - Ammo Counter (Magazine, Reserve, Fire Mode)
 * - Crosshair (Dynamic spread indicator)
 * - Quick Slots (Equipment shortcuts)
 * - Reload Progress (Tarkov-style reload phases)
 *
 * ARCHITECTURE:
 * - Container widget that holds all HUD sub-widgets
 * - Each sub-widget is autonomous and subscribes to EventBus
 * - Master HUD manages visibility and layout
 * - All bindings are MANDATORY (BindWidget)
 *
 * USAGE:
 * 1. Create Blueprint Widget inheriting from this class
 * 2. Layout all sub-widgets in UMG Designer
 * 3. Bind each sub-widget to corresponding property
 * 4. Spawn via PlayerController and add to viewport
 *
 * ```cpp
 * // In PlayerController
 * MasterHUD = CreateWidget<USuspenseCoreMasterHUDWidget>(this, MasterHUDClass);
 * MasterHUD->AddToViewport();
 * MasterHUD->InitializeHUD(GetPawn());
 * ```
 *
 * IMPORTANT:
 * - All colors come from materials - NO programmatic color changes!
 * - Sub-widgets auto-subscribe to EventBus in NativeConstruct
 * - Call InitializeHUD after pawn is available
 *
 * @see USuspenseCoreGameHUDWidget
 * @see USuspenseCoreAmmoCounterWidget
 * @see USuspenseCoreCrosshairWidget
 * @see USuspenseCoreQuickSlotHUDWidget
 * @see USuspenseCoreReloadProgressWidget
 */
UCLASS(Blueprintable, BlueprintType)
class UISYSTEM_API USuspenseCoreMasterHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreMasterHUDWidget(const FObjectInitializer& ObjectInitializer);

	// ═══════════════════════════════════════════════════════════════════════════
	// UUserWidget Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Initialize HUD with owning pawn
	 * Call this after pawn is available (e.g., after possession)
	 * @param OwningPawn The pawn this HUD represents
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|MasterHUD")
	void InitializeHUD(APawn* OwningPawn);

	/**
	 * Initialize weapon HUD elements with weapon actor
	 * Call when weapon is equipped
	 * @param WeaponActor The equipped weapon
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|MasterHUD")
	void InitializeWeaponHUD(AActor* WeaponActor);

	/**
	 * Clear weapon HUD elements
	 * Call when weapon is unequipped
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|MasterHUD")
	void ClearWeaponHUD();

	/**
	 * Show/hide entire HUD
	 * @param bVisible New visibility state
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|MasterHUD")
	void SetHUDVisible(bool bVisible);

	/**
	 * Check if HUD is visible
	 * @return true if HUD is visible
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|MasterHUD")
	bool IsHUDVisible() const;

	// ═══════════════════════════════════════════════════════════════════════════
	// SECTION VISIBILITY
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Show/hide vitals section (HP, Shield, Stamina)
	 * @param bVisible New visibility state
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|MasterHUD|Sections")
	void SetVitalsVisible(bool bVisible);

	/**
	 * Show/hide weapon section (Ammo, Fire Mode)
	 * @param bVisible New visibility state
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|MasterHUD|Sections")
	void SetWeaponInfoVisible(bool bVisible);

	/**
	 * Show/hide crosshair
	 * @param bVisible New visibility state
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|MasterHUD|Sections")
	void SetCrosshairVisible(bool bVisible);

	/**
	 * Show/hide quick slots
	 * @param bVisible New visibility state
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|MasterHUD|Sections")
	void SetQuickSlotsVisible(bool bVisible);

	/**
	 * Show/hide reload progress
	 * @param bVisible New visibility state
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|MasterHUD|Sections")
	void SetReloadProgressVisible(bool bVisible);

	// ═══════════════════════════════════════════════════════════════════════════
	// GETTERS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Get vitals widget */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|MasterHUD")
	USuspenseCoreGameHUDWidget* GetVitalsWidget() const { return VitalsWidget; }

	/** Get ammo counter widget */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|MasterHUD")
	USuspenseCoreAmmoCounterWidget* GetAmmoCounterWidget() const { return AmmoCounterWidget; }

	/** Get crosshair widget */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|MasterHUD")
	USuspenseCoreCrosshairWidget* GetCrosshairWidget() const { return CrosshairWidget; }

	/** Get quick slots widget */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|MasterHUD")
	USuspenseCoreQuickSlotHUDWidget* GetQuickSlotsWidget() const { return QuickSlotsWidget; }

	/** Get reload progress widget */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|MasterHUD")
	USuspenseCoreReloadProgressWidget* GetReloadProgressWidget() const { return ReloadProgressWidget; }

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when HUD is fully initialized */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|MasterHUD|Events")
	void OnHUDInitialized();

	/** Called when weapon HUD is initialized */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|MasterHUD|Events")
	void OnWeaponHUDInitialized(AActor* WeaponActor);

	/** Called when weapon HUD is cleared */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|MasterHUD|Events")
	void OnWeaponHUDCleared();

	/** Called when HUD visibility changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|MasterHUD|Events")
	void OnHUDVisibilityChanged(bool bNewVisible);

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - ALL MANDATORY (BindWidget)
	// ═══════════════════════════════════════════════════════════════════════════

	/** Root canvas panel containing all HUD elements */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCanvasPanel> RootCanvas;

	/** Vitals widget (Health, Shield, Stamina) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<USuspenseCoreGameHUDWidget> VitalsWidget;

	/** Ammo counter widget (Magazine, Reserve, Fire Mode) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<USuspenseCoreAmmoCounterWidget> AmmoCounterWidget;

	/** Crosshair widget (Dynamic spread) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<USuspenseCoreCrosshairWidget> CrosshairWidget;

	/** Quick slots widget (Equipment shortcuts) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<USuspenseCoreQuickSlotHUDWidget> QuickSlotsWidget;

	/** Reload progress widget (Tarkov-style phases) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<USuspenseCoreReloadProgressWidget> ReloadProgressWidget;

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Auto-hide weapon HUD when no weapon equipped */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|MasterHUD|Config")
	bool bAutoHideWeaponHUD = true;

	/** Auto-hide reload progress when not reloading */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|MasterHUD|Config")
	bool bAutoHideReloadProgress = true;

	/** Show crosshair only when weapon equipped */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|MasterHUD|Config")
	bool bCrosshairRequiresWeapon = true;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL STATE
	// ═══════════════════════════════════════════════════════════════════════════

	/** Cached owning pawn */
	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> CachedOwningPawn;

	/** Cached weapon actor */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CachedWeaponActor;

	/** Is HUD initialized */
	bool bIsInitialized = false;

	/** Has weapon equipped */
	bool bHasWeaponEquipped = false;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL METHODS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Apply initial visibility states based on configuration */
	void ApplyInitialVisibility();

	/** Update weapon-related widgets visibility */
	void UpdateWeaponWidgetsVisibility();
};
