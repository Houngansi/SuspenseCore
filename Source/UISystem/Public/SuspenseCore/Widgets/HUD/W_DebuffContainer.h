// W_DebuffContainer.h
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.
//
// ARCHITECTURE:
// - Container widget for procedural status effect icon management
// - EventBus-driven updates (NO polling!)
// - Object pooling for icon widgets
// - Supports local player and spectated targets
// - Displays BOTH debuffs (DoT) AND buffs (HoT, Regenerating, etc.)
//
// DATA FLOW:
// GrenadeProjectile::ApplyDoTEffects() → DoTService → EventBus → W_DebuffContainer → W_DebuffIcon
// MedicalHandler::ApplyHealOverTime() → EventBus → W_DebuffContainer → W_DebuffIcon
//
// EVENTBUS EVENTS:
// - SuspenseCoreTags::Event::DoT::Applied   - Debuff applied
// - SuspenseCoreTags::Event::DoT::Removed   - Debuff healed/cured
// - SuspenseCoreTags::Event::DoT::Expired   - Debuff expired naturally
// - SuspenseCoreTags::Event::DoT::Tick      - Damage tick occurred
// - SuspenseCoreMedicalTags::Event::HoTStarted     - Buff (HoT) applied
// - SuspenseCoreMedicalTags::Event::HoTInterrupted - Buff (HoT) interrupted
//
// @see W_DebuffIcon.h
// @see USuspenseCoreDoTService
// @see Documentation/Plans/DebuffWidget_System_Plan.md

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "W_DebuffContainer.generated.h"

// Forward declarations
class UHorizontalBox;
class UW_DebuffIcon;
class USuspenseCoreEventBus;
class USuspenseCoreDoTService;

/**
 * Container widget for procedural debuff icon management
 *
 * FEATURES:
 * - EventBus subscription for DoT events
 * - Object pooling for icon widgets (no GC churn!)
 * - Automatic add/remove/update of debuff icons
 * - Support for multiple debuff types
 * - Target actor switching (for spectator mode)
 *
 * LAYOUT:
 * ┌──────────────────────────────────────────────────────────────────────┐
 * │ W_DebuffContainer (HorizontalBox)                                    │
 * │ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐                     │
 * │ │W_Debuff │ │W_Debuff │ │W_Debuff │ │ (Pool)  │  ← Dynamic loading  │
 * │ │Icon #1  │ │Icon #2  │ │Icon #3  │ │ Hidden  │                     │
 * │ │[Bleed]  │ │[Burn]   │ │[Poison] │ │         │                     │
 * │ │Timer: ∞ │ │Timer:5s │ │Timer:10s│ │         │                     │
 * │ └─────────┘ └─────────┘ └─────────┘ └─────────┘                     │
 * └──────────────────────────────────────────────────────────────────────┘
 *
 * USAGE:
 * 1. Add to Master HUD Widget (as child of a Canvas/Overlay)
 * 2. Widget auto-subscribes to EventBus on NativeConstruct
 * 3. Icons appear/disappear as DoT effects are applied/removed
 * 4. Call SetTargetActor() to change tracked actor (spectator mode)
 *
 * PERFORMANCE:
 * - Widget pooling prevents allocation spikes
 * - Uses NativeTick for timer updates (batched)
 * - Lazy icon texture loading
 */
UCLASS(Blueprintable, BlueprintType)
class UISYSTEM_API UW_DebuffContainer : public UUserWidget
{
	GENERATED_BODY()

public:
	UW_DebuffContainer(const FObjectInitializer& ObjectInitializer);

	// ═══════════════════════════════════════════════════════════════════
	// UUserWidget Interface
	// ═══════════════════════════════════════════════════════════════════

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ═══════════════════════════════════════════════════════════════════
	// UI BINDINGS
	// ═══════════════════════════════════════════════════════════════════

	/** Container box for debuff icons */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UHorizontalBox> DebuffBox;

	// ═══════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════

	/** Widget class for debuff icons (must be Blueprint child of W_DebuffIcon) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Debuff|Config")
	TSubclassOf<UW_DebuffIcon> DebuffIconClass;

	/** Maximum number of visible debuff icons */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Debuff|Config", meta = (ClampMin = "1", ClampMax = "20"))
	int32 MaxVisibleDebuffs = 10;

	/** Pool size for widget recycling */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Debuff|Config", meta = (ClampMin = "1"))
	int32 IconPoolSize = 15;

	/** Update interval for duration timers (seconds) - NOT USED, tick-driven */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Debuff|Config", meta = (ClampMin = "0.016"))
	float UpdateInterval = 0.1f;

	/** Auto-set target to local player pawn on construct */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Debuff|Config")
	bool bAutoTargetLocalPlayer = true;

	// ═══════════════════════════════════════════════════════════════════
	// PUBLIC API
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Manually refresh all debuff icons from DoTService
	 * Useful for initialization or sync after reconnect
	 */
	UFUNCTION(BlueprintCallable, Category = "Debuff")
	void RefreshFromDoTService();

	/**
	 * Clear all active debuff icons
	 */
	UFUNCTION(BlueprintCallable, Category = "Debuff")
	void ClearAllDebuffs();

	/**
	 * Set target actor to display debuffs for
	 * @param NewTarget Actor to track (null = local player)
	 */
	UFUNCTION(BlueprintCallable, Category = "Debuff")
	void SetTargetActor(AActor* NewTarget);

	/**
	 * Get current target actor
	 */
	UFUNCTION(BlueprintPure, Category = "Debuff")
	AActor* GetTargetActor() const { return TargetActor.Get(); }

	/**
	 * Get count of currently active debuffs
	 */
	UFUNCTION(BlueprintPure, Category = "Debuff")
	int32 GetActiveDebuffCount() const { return ActiveDebuffs.Num(); }

	/**
	 * Check if a specific debuff type is active
	 */
	UFUNCTION(BlueprintPure, Category = "Debuff")
	bool HasDebuffOfType(FGameplayTag DoTType) const { return ActiveDebuffs.Contains(DoTType); }

	// ═══════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════

	/** Called when any debuff is added */
	UFUNCTION(BlueprintImplementableEvent, Category = "Debuff|Events")
	void OnDebuffAdded(FGameplayTag DebuffType);

	/** Called when any debuff is removed */
	UFUNCTION(BlueprintImplementableEvent, Category = "Debuff|Events")
	void OnDebuffRemoved(FGameplayTag DebuffType);

	/** Called when target actor changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "Debuff|Events")
	void OnTargetChanged(AActor* NewTarget);

protected:
	// ═══════════════════════════════════════════════════════════════════
	// EVENTBUS HANDLERS
	// ═══════════════════════════════════════════════════════════════════

	/** Called when DoT applied event received */
	void OnDoTApplied(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Called when DoT removed event received */
	void OnDoTRemoved(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Called when DoT expired event received */
	void OnDoTExpired(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Called when DoT tick event received (optional - for damage numbers) */
	void OnDoTTick(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Called when HoT (buff) started event received */
	void OnHoTStarted(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Called when HoT (buff) interrupted/ended event received */
	void OnHoTEnded(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	// ═══════════════════════════════════════════════════════════════════
	// EVENTBUS SETUP
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Subscribe to EventBus events
	 */
	void SetupEventSubscriptions();

	/**
	 * Unsubscribe from EventBus events
	 */
	void TeardownEventSubscriptions();

	/**
	 * Get EventBus instance
	 */
	USuspenseCoreEventBus* GetEventBus() const;

	// ═══════════════════════════════════════════════════════════════════
	// ICON MANAGEMENT
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Add or update debuff icon
	 * @param DoTType Debuff type tag
	 * @param Duration Duration (-1 for infinite)
	 * @param StackCount Stack count
	 */
	void AddOrUpdateDebuff(FGameplayTag DoTType, float Duration, int32 StackCount);

	/**
	 * Remove debuff icon (with animation)
	 * @param DoTType Debuff type to remove
	 */
	void RemoveDebuff(FGameplayTag DoTType);

	/**
	 * Get icon from pool or create new
	 * @return Available icon widget (may be null if pool exhausted and can't create)
	 */
	UW_DebuffIcon* AcquireIcon();

	/**
	 * Return icon to pool
	 * @param Icon Icon to release
	 */
	void ReleaseIcon(UW_DebuffIcon* Icon);

	/**
	 * Initialize widget pool
	 */
	void InitializePool();

	/**
	 * Check if event is for our target actor
	 * @param EventData Event data to check
	 * @return True if event is for our tracked target
	 */
	bool IsEventForTarget(const FSuspenseCoreEventData& EventData) const;

	/**
	 * Called when icon removal animation completes
	 */
	UFUNCTION()
	void OnIconRemovalComplete(UW_DebuffIcon* Icon);

	/**
	 * Called when icon duration timer expires (for auto-removal)
	 */
	UFUNCTION()
	void OnIconDurationExpired(UW_DebuffIcon* Icon, FGameplayTag EffectType);

private:
	// ═══════════════════════════════════════════════════════════════════
	// STATE
	// ═══════════════════════════════════════════════════════════════════

	/** Map of active debuffs: DoTType → Icon */
	UPROPERTY()
	TMap<FGameplayTag, TObjectPtr<UW_DebuffIcon>> ActiveDebuffs;

	/** Pool of available icon widgets */
	UPROPERTY()
	TArray<TObjectPtr<UW_DebuffIcon>> IconPool;

	/** EventBus subscription handles - DoT (Debuffs) */
	FSuspenseCoreSubscriptionHandle DoTAppliedHandle;
	FSuspenseCoreSubscriptionHandle DoTRemovedHandle;
	FSuspenseCoreSubscriptionHandle DoTExpiredHandle;
	FSuspenseCoreSubscriptionHandle DoTTickHandle;

	/** EventBus subscription handles - HoT (Buffs) */
	FSuspenseCoreSubscriptionHandle HoTStartedHandle;
	FSuspenseCoreSubscriptionHandle HoTEndedHandle;

	/** Target actor to display debuffs for */
	UPROPERTY()
	TWeakObjectPtr<AActor> TargetActor;

	/** Cached EventBus reference */
	UPROPERTY()
	mutable TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	/** Cached DoTService reference */
	UPROPERTY()
	mutable TWeakObjectPtr<USuspenseCoreDoTService> CachedDoTService;

	/** Is pool initialized? */
	bool bPoolInitialized = false;

	/** Timer accumulator for batched updates (unused, kept for potential future use) */
	float UpdateTimer = 0.0f;
};
