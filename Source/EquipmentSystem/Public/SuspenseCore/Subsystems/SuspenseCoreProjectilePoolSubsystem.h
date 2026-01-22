// SuspenseCoreProjectilePoolSubsystem.h
// Object pooling for grenade projectiles to reduce GC pressure
// Copyright Suspense Team. All Rights Reserved.
//
// ARCHITECTURE:
// - WorldSubsystem for per-world pool lifecycle
// - Pre-allocates projectiles on world start
// - Reuses projectiles instead of Spawn/Destroy
// - Automatic pool expansion when needed
//
// PERFORMANCE:
// - Eliminates GC spikes from frequent projectile spawning
// - O(1) acquire/release from pool
// - Thread-safe for async operations
//
// USAGE:
// 1. Get pool: auto* Pool = USuspenseCoreProjectilePoolSubsystem::Get(World);
// 2. Acquire: auto* Proj = Pool->AcquireProjectile(ProjectileClass);
// 3. Release: Pool->ReleaseProjectile(Proj); // Called automatically on explosion
//
// NETWORK:
// - Server manages pool, clients receive replicated projectiles
// - Pool only active on server/standalone

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SuspenseCoreProjectilePoolSubsystem.generated.h"

class ASuspenseCoreGrenadeProjectile;

/**
 * Pool entry for tracking projectile state
 */
USTRUCT()
struct FSuspenseCorePooledProjectile
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<ASuspenseCoreGrenadeProjectile> Projectile = nullptr;

	/** Is currently in use (not in pool) */
	bool bInUse = false;

	/** Time when last returned to pool (for cleanup) */
	float ReturnTime = 0.0f;

	/** Class of this pooled projectile */
	UPROPERTY()
	TSubclassOf<ASuspenseCoreGrenadeProjectile> ProjectileClass;
};

/**
 * USuspenseCoreProjectilePoolSubsystem
 *
 * Object pooling system for grenade projectiles.
 * Reduces GC pressure by reusing actor instances.
 *
 * POOL MANAGEMENT:
 * - Initial pool size configurable per class
 * - Auto-expands when pool exhausted
 * - Periodic cleanup of excess pooled actors
 *
 * LIFECYCLE:
 * 1. Acquire: Get projectile from pool (or spawn if empty)
 * 2. Initialize: Caller sets up projectile (velocity, type, etc.)
 * 3. Release: Projectile returns to pool after explosion
 *
 * @see ASuspenseCoreGrenadeProjectile
 */
UCLASS()
class EQUIPMENTSYSTEM_API USuspenseCoreProjectilePoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ═══════════════════════════════════════════════════════════════════
	// SUBSYSTEM LIFECYCLE
	// ═══════════════════════════════════════════════════════════════════

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// ═══════════════════════════════════════════════════════════════════
	// STATIC ACCESS
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Get pool subsystem for world
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Pool", meta = (WorldContext = "WorldContextObject"))
	static USuspenseCoreProjectilePoolSubsystem* Get(const UObject* WorldContextObject);

	// ═══════════════════════════════════════════════════════════════════
	// POOL API
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Acquire projectile from pool
	 * Returns existing pooled projectile or spawns new one if pool empty
	 *
	 * @param ProjectileClass Class to acquire
	 * @param SpawnTransform Initial transform
	 * @return Ready-to-use projectile (never null)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Pool")
	ASuspenseCoreGrenadeProjectile* AcquireProjectile(
		TSubclassOf<ASuspenseCoreGrenadeProjectile> ProjectileClass,
		const FTransform& SpawnTransform
	);

	/**
	 * Release projectile back to pool
	 * Resets projectile state and hides it
	 *
	 * @param Projectile Projectile to return
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Pool")
	void ReleaseProjectile(ASuspenseCoreGrenadeProjectile* Projectile);

	/**
	 * Pre-warm pool with specified number of projectiles
	 *
	 * @param ProjectileClass Class to pre-allocate
	 * @param Count Number to pre-allocate
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Pool")
	void PreWarmPool(TSubclassOf<ASuspenseCoreGrenadeProjectile> ProjectileClass, int32 Count);

	/**
	 * Get current pool statistics
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Pool")
	void GetPoolStats(int32& OutTotalPooled, int32& OutInUse, int32& OutAvailable) const;

	/**
	 * Clear all pooled projectiles (for level transition)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Pool")
	void ClearPool();

	// ═══════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════

	/** Default pool size per class */
	UPROPERTY(EditDefaultsOnly, Category = "Pool")
	int32 DefaultPoolSize = 10;

	/** Maximum pool size per class (prevents memory bloat) */
	UPROPERTY(EditDefaultsOnly, Category = "Pool")
	int32 MaxPoolSize = 50;

	/** Time before excess pooled actors are destroyed (seconds) */
	UPROPERTY(EditDefaultsOnly, Category = "Pool")
	float CleanupDelay = 30.0f;

protected:
	// ═══════════════════════════════════════════════════════════════════
	// INTERNAL
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Spawn new projectile for pool
	 */
	ASuspenseCoreGrenadeProjectile* SpawnPooledProjectile(
		TSubclassOf<ASuspenseCoreGrenadeProjectile> ProjectileClass,
		const FTransform& SpawnTransform
	);

	/**
	 * Reset projectile state for reuse
	 */
	void ResetProjectile(ASuspenseCoreGrenadeProjectile* Projectile);

	/**
	 * Deactivate projectile (hide, disable tick, etc.)
	 */
	void DeactivateProjectile(ASuspenseCoreGrenadeProjectile* Projectile);

	/**
	 * Activate projectile for use
	 */
	void ActivateProjectile(ASuspenseCoreGrenadeProjectile* Projectile, const FTransform& Transform);

	/**
	 * Periodic cleanup of excess pooled actors
	 */
	void CleanupExcessPooled();

private:
	/** Pool storage: Class -> Array of pooled entries */
	TMap<TSubclassOf<ASuspenseCoreGrenadeProjectile>, TArray<FSuspenseCorePooledProjectile>> ProjectilePool;

	/** Critical section for thread safety */
	mutable FCriticalSection PoolLock;

	/** Cleanup timer */
	FTimerHandle CleanupTimerHandle;

	/** Is pool active (server/standalone only) */
	bool bPoolActive = false;
};
