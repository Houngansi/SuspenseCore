// W_DebuffIcon.h
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.
//
// ARCHITECTURE:
// - Individual debuff icon widget for HUD display
// - Created and managed by W_DebuffContainer
// - Data-driven visual updates based on DoT type
// - Animation support for pulse/fade effects
//
// USAGE:
// Do NOT place directly in HUD. W_DebuffContainer manages pooling and lifecycle.
//
// @see W_DebuffContainer.h
// @see USuspenseCoreDoTService
// @see Documentation/Plans/DebuffWidget_System_Plan.md

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Engine/StreamableManager.h"
#include "W_DebuffIcon.generated.h"

// Forward declarations
class UImage;
class UTextBlock;
class UProgressBar;
class UWidgetAnimation;
class UTexture2D;
struct FSuspenseCoreStatusEffectVisualRow;

/**
 * Individual debuff icon widget
 *
 * FEATURES:
 * - Displays debuff icon with appropriate visual
 * - Shows timer text (∞ for infinite, seconds for timed)
 * - Optional duration progress bar
 * - Pulse animation when critical
 * - Stack count display
 *
 * VISUALS:
 * - Bleeding: Red blood drop icons
 * - Burning: Orange flame icon
 * - Duration bar hidden for infinite effects (bleeding)
 *
 * USAGE:
 * Created dynamically by W_DebuffContainer.
 * Not intended for direct placement in HUD.
 */
UCLASS(Blueprintable, BlueprintType)
class UISYSTEM_API UW_DebuffIcon : public UUserWidget
{
	GENERATED_BODY()

public:
	UW_DebuffIcon(const FObjectInitializer& ObjectInitializer);

	// ═══════════════════════════════════════════════════════════════════
	// UUserWidget Interface
	// ═══════════════════════════════════════════════════════════════════

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ═══════════════════════════════════════════════════════════════════
	// UI BINDINGS - All components must exist in Blueprint
	// ═══════════════════════════════════════════════════════════════════

	/** Debuff icon image */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> DebuffImage;

	/** Timer text (∞ or seconds remaining) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> TimerText;

	/** Duration progress bar (hidden for infinite effects) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> DurationBar;

	/** Stack count text (hidden if StackCount <= 1) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StackText;

	// ═══════════════════════════════════════════════════════════════════
	// ANIMATIONS (Optional - set in Blueprint)
	// ═══════════════════════════════════════════════════════════════════

	/** Pulse animation for critical state */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> PulseAnimation;

	/** Fade in animation when debuff applied */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> FadeInAnimation;

	/** Fade out animation when debuff removed */
	UPROPERTY(BlueprintReadOnly, Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> FadeOutAnimation;

	// ═══════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════

	/** Icon textures mapped by debuff type tag */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Debuff|Config")
	TMap<FGameplayTag, TSoftObjectPtr<UTexture2D>> DebuffIcons;

	/** Tint color for critical state (low health + bleeding) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Debuff|Config")
	FLinearColor CriticalTintColor = FLinearColor(1.0f, 0.3f, 0.3f, 1.0f);

	/** Normal tint color */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Debuff|Config")
	FLinearColor NormalTintColor = FLinearColor::White;

	/** Duration threshold for "critical" warning (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Debuff|Config", meta = (ClampMin = "0.0"))
	float CriticalDurationThreshold = 3.0f;

	/** Infinite duration symbol (UTF-8 infinity) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Debuff|Config")
	FText InfiniteSymbol = NSLOCTEXT("Debuff", "Infinite", "∞");

	/**
	 * Use SSOT DataTable for visual data instead of hardcoded DebuffIcons map
	 * When enabled, icons and colors are loaded from StatusEffectVisualsDataTable
	 * @see FSuspenseCoreStatusEffectVisualRow (v2.0 simplified visual-only structure)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Debuff|Config")
	bool bUseSSOTData = true;

	// ═══════════════════════════════════════════════════════════════════
	// PUBLIC API
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Initialize debuff display with data
	 * @param InDoTType Type tag (State.Health.Bleeding.Light, etc.)
	 * @param InDuration Duration in seconds (-1 for infinite)
	 * @param InStackCount Number of stacks (default 1)
	 */
	UFUNCTION(BlueprintCallable, Category = "Debuff")
	void SetDebuffData(FGameplayTag InDoTType, float InDuration, int32 InStackCount = 1);

	/**
	 * Initialize debuff display using SSOT data
	 * Automatically queries DataManager for effect data by EffectID
	 * @param EffectID Effect ID (e.g., "BleedingLight")
	 * @param InDuration Duration override (-1 for infinite, 0 for SSOT default)
	 * @param InStackCount Number of stacks
	 */
	UFUNCTION(BlueprintCallable, Category = "Debuff")
	void SetDebuffDataFromSSOT(FName EffectID, float InDuration = 0.0f, int32 InStackCount = 1);

	/**
	 * Update timer display
	 * Called by container during NativeTick
	 * @param RemainingDuration Remaining seconds (-1 for infinite)
	 */
	UFUNCTION(BlueprintCallable, Category = "Debuff")
	void UpdateTimer(float RemainingDuration);

	/**
	 * Update stack count
	 * @param NewStackCount New stack count
	 */
	UFUNCTION(BlueprintCallable, Category = "Debuff")
	void UpdateStackCount(int32 NewStackCount);

	/**
	 * Play removal animation and notify when complete
	 * Container will release icon back to pool after animation
	 */
	UFUNCTION(BlueprintCallable, Category = "Debuff")
	void PlayRemovalAnimation();

	/**
	 * Reset icon to default state (for pooling)
	 */
	UFUNCTION(BlueprintCallable, Category = "Debuff")
	void ResetToDefault();

	/**
	 * Get current debuff type tag
	 */
	UFUNCTION(BlueprintPure, Category = "Debuff")
	FGameplayTag GetDebuffType() const { return DoTType; }

	/**
	 * Check if this is an infinite duration debuff
	 */
	UFUNCTION(BlueprintPure, Category = "Debuff")
	bool IsInfinite() const { return TotalDuration < 0.0f; }

	/**
	 * Check if icon is currently active (displaying a debuff)
	 */
	UFUNCTION(BlueprintPure, Category = "Debuff")
	bool IsActive() const { return bIsActive; }

	/**
	 * Get remaining duration
	 */
	UFUNCTION(BlueprintPure, Category = "Debuff")
	float GetRemainingDuration() const { return RemainingDuration; }

	// ═══════════════════════════════════════════════════════════════════
	// DELEGATES
	// ═══════════════════════════════════════════════════════════════════

	/** Called when removal animation completes (for pooling) */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRemovalComplete, UW_DebuffIcon*, Icon);

	UPROPERTY(BlueprintAssignable, Category = "Debuff|Events")
	FOnRemovalComplete OnRemovalComplete;

	/** Called when duration timer reaches 0 (for auto-removal of timed effects) */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDurationExpired, UW_DebuffIcon*, Icon, FGameplayTag, EffectType);

	UPROPERTY(BlueprintAssignable, Category = "Debuff|Events")
	FOnDurationExpired OnDurationExpired;

	// ═══════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════

	/** Called when debuff data is set */
	UFUNCTION(BlueprintImplementableEvent, Category = "Debuff|Events")
	void OnDebuffApplied(FGameplayTag DebuffType);

	/** Called when timer enters critical state */
	UFUNCTION(BlueprintImplementableEvent, Category = "Debuff|Events")
	void OnCriticalState(bool bIsCritical);

	/** Called when stack count changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "Debuff|Events")
	void OnStackCountChanged(int32 NewCount);

protected:
	// ═══════════════════════════════════════════════════════════════════
	// INTERNAL HELPERS
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Update visual state (icon, colors)
	 */
	void UpdateVisuals();

	/**
	 * Format duration for display
	 * @param Duration Seconds (-1 for infinite)
	 * @return Formatted string ("∞" or "5.2s" or "1:30")
	 */
	FText FormatDuration(float Duration) const;

	/**
	 * Load icon texture for current debuff type
	 * Uses SSOT data if bUseSSOTData is true, otherwise uses DebuffIcons map
	 */
	void LoadIconForType();

	/**
	 * Load icon and visual data from SSOT DataTable
	 * Called when bUseSSOTData is enabled
	 * @return true if SSOT data was found and applied
	 */
	bool LoadIconFromSSOT();

	/**
	 * Called when icon texture finishes async loading
	 */
	void OnIconLoaded();

	/**
	 * Update critical state and visuals
	 */
	void UpdateCriticalState();

	/**
	 * Called when fade out animation finishes
	 */
	UFUNCTION()
	void OnFadeOutFinished();

private:
	// ═══════════════════════════════════════════════════════════════════
	// STATE
	// ═══════════════════════════════════════════════════════════════════

	/** Current debuff type tag */
	UPROPERTY()
	FGameplayTag DoTType;

	/** Total duration (-1 = infinite) */
	float TotalDuration = -1.0f;

	/** Current remaining duration */
	float RemainingDuration = -1.0f;

	/** Current stack count */
	int32 StackCount = 1;

	/** Is in critical state */
	bool bIsInCriticalState = false;

	/** Is icon currently active (displaying a debuff) */
	bool bIsActive = false;

	/** Is removal animation playing */
	bool bIsRemoving = false;

	/** Async load handle for icon texture */
	TSharedPtr<FStreamableHandle> IconLoadHandle;

	/** Cached effect ID from SSOT (if using SSOT data) */
	FName CachedEffectID;

	/** Cached SSOT icon path (for async loading) */
	TSoftObjectPtr<UTexture2D> SSOTIconPath;

	/** Cached SSOT normal tint color */
	FLinearColor SSOTNormalTint = FLinearColor::White;

	/** Cached SSOT critical tint color */
	FLinearColor SSOTCriticalTint = FLinearColor(1.0f, 0.3f, 0.3f, 1.0f);
};
