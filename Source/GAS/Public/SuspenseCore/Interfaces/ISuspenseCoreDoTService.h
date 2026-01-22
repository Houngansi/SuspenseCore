// ISuspenseCoreDoTService.h
// Interface for DoT Service - enables dependency injection and testing
// Copyright Suspense Team. All Rights Reserved.
//
// ARCHITECTURE:
// Interface-based service pattern for AAA-quality testability.
// Allows mock implementations for unit tests without live GAS/EventBus.
//
// USAGE:
// 1. Get interface: ISuspenseCoreDoTService* Service = GetDoTService();
// 2. Query via interface: Service->HasActiveBleeding(Actor);
// 3. Mock in tests: TestDoTService implements ISuspenseCoreDoTService

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "ISuspenseCoreDoTService.generated.h"

// Forward declarations
struct FSuspenseCoreActiveDoT;

/**
 * Interface for DoT Service
 * Enables dependency injection and mock implementations for testing
 */
UINTERFACE(MinimalAPI, Blueprintable, meta = (CannotImplementInterfaceInBlueprint))
class UISuspenseCoreDoTService : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISuspenseCoreDoTService
 *
 * Pure interface for Damage-over-Time service operations.
 * Implement this interface for:
 * - Production: USuspenseCoreDoTService (GameInstanceSubsystem)
 * - Testing: UMockDoTService (returns predictable values)
 *
 * THREAD SAFETY:
 * All implementations must be game-thread safe.
 *
 * @see USuspenseCoreDoTService - Production implementation
 */
class GAS_API IISuspenseCoreDoTService
{
	GENERATED_BODY()

public:
	// ═══════════════════════════════════════════════════════════════════
	// QUERY API
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Get all active DoT effects on target
	 * @param Target Actor to query
	 * @return Array of active DoT data (empty if none)
	 */
	virtual TArray<FSuspenseCoreActiveDoT> GetActiveDoTs(AActor* Target) const = 0;

	/**
	 * Check if target has any active bleeding
	 */
	virtual bool HasActiveBleeding(AActor* Target) const = 0;

	/**
	 * Check if target has any active burning
	 */
	virtual bool HasActiveBurning(AActor* Target) const = 0;

	/**
	 * Get total bleed DPS on target
	 */
	virtual float GetBleedDamagePerSecond(AActor* Target) const = 0;

	/**
	 * Get remaining burn duration (shortest if multiple)
	 */
	virtual float GetBurnTimeRemaining(AActor* Target) const = 0;

	/**
	 * Get count of active DoT effects
	 */
	virtual int32 GetActiveDoTCount(AActor* Target) const = 0;

	/**
	 * Check if target has specific DoT type active
	 * @param Target Actor to check
	 * @param DoTType Tag to check (e.g., State.Health.Bleeding.Light)
	 */
	virtual bool HasActiveDoTOfType(AActor* Target, FGameplayTag DoTType) const = 0;

	// ═══════════════════════════════════════════════════════════════════
	// REGISTRATION API (called by GE or Projectile)
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Notify service of new DoT application
	 * Called after GE is applied to target
	 */
	virtual void NotifyDoTApplied(
		AActor* Target,
		FGameplayTag DoTType,
		float DamagePerTick,
		float TickInterval,
		float Duration,
		AActor* Source
	) = 0;

	/**
	 * Notify service of DoT removal
	 * @param bExpired true if naturally expired, false if healed
	 */
	virtual void NotifyDoTRemoved(
		AActor* Target,
		FGameplayTag DoTType,
		bool bExpired
	) = 0;

	// ═══════════════════════════════════════════════════════════════════
	// ASC BINDING (for ASC-driven tracking)
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Bind to ASC delegates for automatic DoT tracking
	 * Called when character initializes ASC
	 * @param ASC AbilitySystemComponent to monitor
	 */
	virtual void BindToASC(class UAbilitySystemComponent* ASC) = 0;

	/**
	 * Unbind from ASC (on character death/destroy)
	 */
	virtual void UnbindFromASC(class UAbilitySystemComponent* ASC) = 0;
};
