// SuspenseCoreDoTService.h
// Service for tracking and managing Damage-over-Time effects
// Copyright Suspense Team. All Rights Reserved.
//
// ARCHITECTURE:
// - GameInstanceSubsystem for global lifecycle
// - Integrates with EventBus for decoupled communication
// - Tracks active DoT effects per character
// - Provides query API for UI widgets
//
// DESIGN PATTERN:
// - Service Locator via GameInstanceSubsystem
// - Observer pattern via EventBus subscription
// - DI-friendly (injectable dependencies)
//
// USAGE:
// 1. Get service: USuspenseCoreDoTService* Service = USuspenseCoreDoTService::Get(WorldContext);
// 2. Query DoTs: TArray<FSuspenseCoreActiveDoT> DoTs = Service->GetActiveDoTs(TargetActor);
// 3. Subscribe to events via EventBus (SuspenseCore.Event.DoT.Applied, etc.)
//
// FLOW:
// GrenadeProjectile → Apply GE → ASC → Tag Changed → DoTService → EventBus → UI Widget

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCoreDoTService.generated.h"

// Forward declarations
class USuspenseCoreEventBus;
class UAbilitySystemComponent;

/**
 * Active DoT effect information
 * Used for UI display and queries
 */
USTRUCT(BlueprintType)
struct GAS_API FSuspenseCoreActiveDoT
{
	GENERATED_BODY()

	/** Type of DoT effect (State.Health.Bleeding.Light, State.Burning, etc.) */
	UPROPERTY(BlueprintReadOnly, Category = "DoT")
	FGameplayTag DoTType;

	/** Damage per tick (negative = healing) */
	UPROPERTY(BlueprintReadOnly, Category = "DoT")
	float DamagePerTick = 0.0f;

	/** Tick interval in seconds */
	UPROPERTY(BlueprintReadOnly, Category = "DoT")
	float TickInterval = 1.0f;

	/** Remaining duration (-1 = infinite, e.g., bleeding) */
	UPROPERTY(BlueprintReadOnly, Category = "DoT")
	float RemainingDuration = -1.0f;

	/** Number of stacks (for stackable effects) */
	UPROPERTY(BlueprintReadOnly, Category = "DoT")
	int32 StackCount = 1;

	/** Time when effect was applied */
	UPROPERTY(BlueprintReadOnly, Category = "DoT")
	float ApplicationTime = 0.0f;

	/** Source actor (grenade owner, etc.) */
	UPROPERTY(BlueprintReadOnly, Category = "DoT")
	TWeakObjectPtr<AActor> SourceActor;

	/** Check if this is an infinite duration effect */
	bool IsInfinite() const { return RemainingDuration < 0.0f; }

	/** Check if effect is a bleeding type */
	bool IsBleeding() const;

	/** Check if effect is a burning type */
	bool IsBurning() const;

	/** Get display name for UI */
	FText GetDisplayName() const;

	/** Get icon texture path for UI */
	FString GetIconPath() const;
};

/**
 * Event data for DoT events published via EventBus
 */
USTRUCT(BlueprintType)
struct GAS_API FSuspenseCoreDoTEventPayload
{
	GENERATED_BODY()

	/** Actor affected by DoT */
	UPROPERTY(BlueprintReadOnly, Category = "DoT")
	TWeakObjectPtr<AActor> AffectedActor;

	/** DoT type tag */
	UPROPERTY(BlueprintReadOnly, Category = "DoT")
	FGameplayTag DoTType;

	/** Current DoT data */
	UPROPERTY(BlueprintReadOnly, Category = "DoT")
	FSuspenseCoreActiveDoT DoTData;

	/** Damage dealt this tick (for Tick events) */
	UPROPERTY(BlueprintReadOnly, Category = "DoT")
	float DamageDealt = 0.0f;

	/** Create event data from this payload */
	FSuspenseCoreEventData ToEventData() const;

	/** Parse from event data */
	static FSuspenseCoreDoTEventPayload FromEventData(const FSuspenseCoreEventData& EventData);
};

/**
 * USuspenseCoreDoTService
 *
 * Central service for DoT (Damage-over-Time) effect management.
 * Tracks active effects, publishes events, provides query API for UI.
 *
 * KEY FEATURES:
 * - Automatic tracking of GAS DoT effects via tag monitoring
 * - EventBus integration for decoupled UI updates
 * - Query API for widget data binding
 * - Support for bleeding (infinite) and burning (timed) effects
 *
 * EVENTS PUBLISHED:
 * - SuspenseCore.Event.DoT.Applied  - When DoT first applied
 * - SuspenseCore.Event.DoT.Tick     - Each damage tick
 * - SuspenseCore.Event.DoT.Expired  - When timed DoT expires
 * - SuspenseCore.Event.DoT.Removed  - When DoT is healed/removed
 *
 * THREAD SAFETY:
 * - All public methods are game-thread only
 * - Internal caches protected by critical sections
 *
 * @see USuspenseCoreEventBus
 * @see UGE_BleedingEffect
 * @see UGE_IncendiaryEffect_ArmorBypass
 */
UCLASS()
class GAS_API USuspenseCoreDoTService : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ═══════════════════════════════════════════════════════════════════
	// SUBSYSTEM LIFECYCLE
	// ═══════════════════════════════════════════════════════════════════

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

	// ═══════════════════════════════════════════════════════════════════
	// STATIC ACCESS
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Get DoT Service instance from any world context
	 * @param WorldContextObject Any UObject with world context
	 * @return DoT Service or nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DoT", meta = (WorldContext = "WorldContextObject"))
	static USuspenseCoreDoTService* Get(const UObject* WorldContextObject);

	// ═══════════════════════════════════════════════════════════════════
	// QUERY API - For UI Widgets
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Get all active DoT effects on a target
	 * @param Target Actor to query
	 * @return Array of active DoT effects (empty if none)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DoT")
	TArray<FSuspenseCoreActiveDoT> GetActiveDoTs(AActor* Target) const;

	/**
	 * Check if target has any active bleeding effect
	 * @param Target Actor to check
	 * @return True if bleeding (light or heavy)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DoT")
	bool HasActiveBleeding(AActor* Target) const;

	/**
	 * Check if target has any active burning effect
	 * @param Target Actor to check
	 * @return True if burning
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DoT")
	bool HasActiveBurning(AActor* Target) const;

	/**
	 * Get total bleed damage per second on target
	 * @param Target Actor to query
	 * @return Total DPS from all bleed effects
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DoT")
	float GetBleedDamagePerSecond(AActor* Target) const;

	/**
	 * Get remaining burn duration (shortest if multiple)
	 * @param Target Actor to query
	 * @return Remaining seconds (-1 if no burn or infinite)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DoT")
	float GetBurnTimeRemaining(AActor* Target) const;

	/**
	 * Get number of active DoT effects on target
	 * @param Target Actor to query
	 * @return Count of active effects
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DoT")
	int32 GetActiveDoTCount(AActor* Target) const;

	// ═══════════════════════════════════════════════════════════════════
	// REGISTRATION API - Called by GrenadeProjectile/Effects
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Register a new DoT effect application
	 * Called by GrenadeProjectile after applying GE
	 *
	 * @param Target Affected actor
	 * @param DoTType Type tag (State.Health.Bleeding.Light, etc.)
	 * @param DamagePerTick Damage per tick
	 * @param TickInterval Seconds between ticks
	 * @param Duration Total duration (-1 for infinite)
	 * @param Source Actor that caused the DoT
	 */
	void RegisterDoTApplied(
		AActor* Target,
		FGameplayTag DoTType,
		float DamagePerTick,
		float TickInterval,
		float Duration,
		AActor* Source
	);

	/**
	 * Register a DoT tick (damage dealt)
	 * Called by GameplayEffect periodic execution
	 */
	void RegisterDoTTick(
		AActor* Target,
		FGameplayTag DoTType,
		float DamageDealt
	);

	/**
	 * Register DoT removal (healed or expired)
	 * Called when effect ends
	 */
	void RegisterDoTRemoved(
		AActor* Target,
		FGameplayTag DoTType,
		bool bExpired  // true = naturally expired, false = healed
	);

protected:
	// ═══════════════════════════════════════════════════════════════════
	// INTERNAL STATE
	// ═══════════════════════════════════════════════════════════════════

	/** Map of Target Actor -> Active DoT effects */
	TMap<TWeakObjectPtr<AActor>, TArray<FSuspenseCoreActiveDoT>> ActiveDoTs;

	/** Cached EventBus reference */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> EventBus;

	/** Critical section for thread safety */
	mutable FCriticalSection DoTLock;

	// ═══════════════════════════════════════════════════════════════════
	// INTERNAL METHODS
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Initialize EventBus connection
	 */
	void InitializeEventBus();

	/**
	 * Cleanup stale entries (destroyed actors)
	 */
	void CleanupStaleEntries();

	/**
	 * Publish DoT event to EventBus
	 */
	void PublishDoTEvent(FGameplayTag EventTag, const FSuspenseCoreDoTEventPayload& Payload);

	/**
	 * Find existing DoT entry for actor/type
	 * @return Pointer to entry or nullptr
	 */
	FSuspenseCoreActiveDoT* FindDoTEntry(AActor* Target, FGameplayTag DoTType);

	/**
	 * Update remaining duration for timed effects
	 * Called from Tick or timer
	 */
	void UpdateDurations(float DeltaTime);

	// ═══════════════════════════════════════════════════════════════════
	// TIMER HANDLE
	// ═══════════════════════════════════════════════════════════════════

	/** Timer for duration updates */
	FTimerHandle DurationUpdateTimerHandle;

	/** Timer callback */
	void OnDurationUpdateTimer();
};
