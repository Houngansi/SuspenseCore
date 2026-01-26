// SuspenseCoreDoTService.h
// ASC-driven DoT tracking service with EventBus integration
// Copyright Suspense Team. All Rights Reserved.
//
// ARCHITECTURE:
// - GameInstanceSubsystem for global lifecycle
// - ASC-DRIVEN: Subscribes to ASC delegates (no manual caching!)
// - EventBus integration for decoupled UI updates
// - Implements IISuspenseCoreDoTService for DI/testing
//
// KEY CHANGE (v2.0):
// Previous version manually cached DoT state (RegisterDoTApplied/Removed).
// New version subscribes to ASC delegates for automatic sync:
// - OnActiveGameplayEffectAddedDelegateToSelf
// - OnAnyGameplayEffectRemovedDelegate
// This eliminates state duplication and ensures consistency.
//
// USAGE:
// 1. Character calls BindToASC() when ASC is initialized
// 2. Service auto-tracks all DoT effects via ASC delegates
// 3. Query via interface: Service->HasActiveBleeding(Actor)
// 4. UI subscribes to EventBus (Event.DoT.Applied, etc.)
//
// FLOW:
// ASC::ApplyGE → ASC Delegate → DoTService::OnEffectAdded → EventBus → UI

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "ActiveGameplayEffectHandle.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Interfaces/ISuspenseCoreDoTService.h"
#include "SuspenseCoreDoTService.generated.h"

// Forward declarations
class USuspenseCoreEventBus;
class UAbilitySystemComponent;
struct FActiveGameplayEffect;

/**
 * Active DoT effect information
 * Derived from ASC ActiveGameplayEffect data
 */
USTRUCT(BlueprintType)
struct GAS_API FSuspenseCoreActiveDoT
{
	GENERATED_BODY()

	/** Type of DoT effect (State.Health.Bleeding.Light, etc.) */
	UPROPERTY(BlueprintReadOnly, Category = "DoT")
	FGameplayTag DoTType;

	/** Damage per tick */
	UPROPERTY(BlueprintReadOnly, Category = "DoT")
	float DamagePerTick = 0.0f;

	/** Tick interval in seconds */
	UPROPERTY(BlueprintReadOnly, Category = "DoT")
	float TickInterval = 1.0f;

	/** Remaining duration (-1 = infinite) */
	UPROPERTY(BlueprintReadOnly, Category = "DoT")
	float RemainingDuration = -1.0f;

	/** Stack count */
	UPROPERTY(BlueprintReadOnly, Category = "DoT")
	int32 StackCount = 1;

	/** Application time (world time) */
	UPROPERTY(BlueprintReadOnly, Category = "DoT")
	float ApplicationTime = 0.0f;

	/** Source actor (grenade owner) */
	UPROPERTY(BlueprintReadOnly, Category = "DoT")
	TWeakObjectPtr<AActor> SourceActor;

	/** ASC handle for this effect (internal use) */
	FActiveGameplayEffectHandle EffectHandle;

	bool IsInfinite() const { return RemainingDuration < 0.0f; }
	bool IsBleeding() const;
	bool IsBurning() const;
	FText GetDisplayName() const;
	FString GetIconPath() const;
};

/**
 * DoT event payload for EventBus
 */
USTRUCT(BlueprintType)
struct GAS_API FSuspenseCoreDoTEventPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DoT")
	TWeakObjectPtr<AActor> AffectedActor;

	UPROPERTY(BlueprintReadOnly, Category = "DoT")
	FGameplayTag DoTType;

	UPROPERTY(BlueprintReadOnly, Category = "DoT")
	FSuspenseCoreActiveDoT DoTData;

	UPROPERTY(BlueprintReadOnly, Category = "DoT")
	float DamageDealt = 0.0f;

	FSuspenseCoreEventData ToEventData() const;
	static FSuspenseCoreDoTEventPayload FromEventData(const FSuspenseCoreEventData& EventData);
};

/**
 * ASC binding info for tracking delegate handles
 */
USTRUCT()
struct FSuspenseCoreASCBinding
{
	GENERATED_BODY()

	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	FDelegateHandle OnEffectAddedHandle;
	FDelegateHandle OnEffectRemovedHandle;
	FDelegateHandle OnEffectStackChangedHandle;
};

/**
 * USuspenseCoreDoTService
 *
 * ASC-driven Damage-over-Time tracking service.
 * Implements IISuspenseCoreDoTService for dependency injection.
 *
 * KEY FEATURES:
 * - ASC delegate subscription (no manual state sync!)
 * - EventBus integration for UI updates
 * - Query API for widgets
 * - Interface-based for testability
 *
 * NETWORK:
 * - Server-authoritative: DoT effects replicate via GAS
 * - Service tracks local ASC state
 * - EventBus events are local (UI only)
 *
 * @see IISuspenseCoreDoTService
 * @see USuspenseCoreEventBus
 */
UCLASS()
class GAS_API USuspenseCoreDoTService : public UGameInstanceSubsystem, public IISuspenseCoreDoTService
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
	 * Get DoT Service instance
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DoT", meta = (WorldContext = "WorldContextObject"))
	static USuspenseCoreDoTService* Get(const UObject* WorldContextObject);

	// ═══════════════════════════════════════════════════════════════════
	// IISuspenseCoreDoTService INTERFACE IMPLEMENTATION
	// ═══════════════════════════════════════════════════════════════════

	virtual TArray<FSuspenseCoreActiveDoT> GetActiveDoTs(AActor* Target) const override;
	virtual bool HasActiveBleeding(AActor* Target) const override;
	virtual bool HasActiveBurning(AActor* Target) const override;
	virtual float GetBleedDamagePerSecond(AActor* Target) const override;
	virtual float GetBurnTimeRemaining(AActor* Target) const override;
	virtual int32 GetActiveDoTCount(AActor* Target) const override;
	virtual bool HasActiveDoTOfType(AActor* Target, FGameplayTag DoTType) const override;

	virtual void NotifyDoTApplied(
		AActor* Target,
		FGameplayTag DoTType,
		float DamagePerTick,
		float TickInterval,
		float Duration,
		AActor* Source
	) override;

	virtual void NotifyDoTRemoved(
		AActor* Target,
		FGameplayTag DoTType,
		bool bExpired
	) override;

	virtual void BindToASC(UAbilitySystemComponent* ASC) override;
	virtual void UnbindFromASC(UAbilitySystemComponent* ASC) override;

	// ═══════════════════════════════════════════════════════════════════
	// BLUEPRINT QUERY API (wrappers for interface)
	// ═══════════════════════════════════════════════════════════════════

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DoT")
	TArray<FSuspenseCoreActiveDoT> BP_GetActiveDoTs(AActor* Target) const { return GetActiveDoTs(Target); }

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DoT")
	bool BP_HasActiveBleeding(AActor* Target) const { return HasActiveBleeding(Target); }

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DoT")
	bool BP_HasActiveBurning(AActor* Target) const { return HasActiveBurning(Target); }

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DoT")
	float BP_GetBleedDamagePerSecond(AActor* Target) const { return GetBleedDamagePerSecond(Target); }

	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DoT")
	float BP_GetBurnTimeRemaining(AActor* Target) const { return GetBurnTimeRemaining(Target); }

protected:
	// ═══════════════════════════════════════════════════════════════════
	// ASC DELEGATE HANDLERS
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Called when any GameplayEffect is added to bound ASC
	 * Filters for DoT effects and publishes EventBus events
	 */
	void OnActiveGameplayEffectAdded(
		UAbilitySystemComponent* ASC,
		const FGameplayEffectSpec& Spec,
		FActiveGameplayEffectHandle Handle
	);

	/**
	 * Called when any GameplayEffect is removed from bound ASC
	 * Filters for DoT effects and publishes EventBus events
	 * @deprecated Use OnActiveGameplayEffectRemovedWithASC instead
	 */
	void OnActiveGameplayEffectRemoved(
		const FActiveGameplayEffect& Effect
	);

	/**
	 * Called when any GameplayEffect is removed from bound ASC (with ASC reference)
	 * This version correctly identifies the target actor from the source ASC
	 */
	void OnActiveGameplayEffectRemovedWithASC(
		const FActiveGameplayEffect& Effect,
		UAbilitySystemComponent* SourceASC
	);

	/**
	 * Called when effect stack count changes
	 */
	void OnGameplayEffectStackChanged(
		FActiveGameplayEffectHandle Handle,
		int32 NewStackCount,
		int32 OldStackCount
	);

	// ═══════════════════════════════════════════════════════════════════
	// INTERNAL HELPERS
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Check if GameplayEffect is a DoT effect
	 * Uses Effect's GrantedTags to identify DoT types
	 */
	bool IsDoTEffect(const FGameplayEffectSpec& Spec) const;
	bool IsDoTEffect(const FActiveGameplayEffect& Effect) const;

	/**
	 * Extract DoT type tag from effect
	 */
	FGameplayTag GetDoTTypeFromEffect(const FGameplayEffectSpec& Spec) const;
	FGameplayTag GetDoTTypeFromEffect(const FActiveGameplayEffect& Effect) const;

	/**
	 * Build FSuspenseCoreActiveDoT from ASC effect data
	 */
	FSuspenseCoreActiveDoT BuildDoTDataFromEffect(
		const FActiveGameplayEffect& Effect,
		AActor* TargetActor
	) const;

	/**
	 * Publish DoT event to EventBus (async-safe)
	 */
	void PublishDoTEvent(FGameplayTag EventTag, const FSuspenseCoreDoTEventPayload& Payload);

	/**
	 * Get ASC from actor
	 */
	UAbilitySystemComponent* GetASCFromActor(AActor* Actor) const;

	/**
	 * Initialize EventBus connection
	 */
	void InitializeEventBus();

private:
	// ═══════════════════════════════════════════════════════════════════
	// STATE
	// ═══════════════════════════════════════════════════════════════════

	/** Bound ASCs with delegate handles */
	TArray<FSuspenseCoreASCBinding> BoundASCs;

	/** EventBus reference */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> EventBus;

	/** Critical section for thread safety */
	mutable FCriticalSection ServiceLock;

	/** DoT effect tag for filtering */
	FGameplayTag DoTRootTag;
	FGameplayTag BleedingRootTag;
	FGameplayTag BurningRootTag;

	// ═══════════════════════════════════════════════════════════════════
	// LEGACY SUPPORT (for manual notification during transition)
	// ═══════════════════════════════════════════════════════════════════

	/** Manual DoT cache (legacy, for non-ASC systems) */
	TMap<TWeakObjectPtr<AActor>, TArray<FSuspenseCoreActiveDoT>> ManualDoTCache;

	/** Timer for manual cache duration updates */
	FTimerHandle DurationUpdateTimerHandle;
	void OnDurationUpdateTimer();
	void CleanupStaleEntries();
};
