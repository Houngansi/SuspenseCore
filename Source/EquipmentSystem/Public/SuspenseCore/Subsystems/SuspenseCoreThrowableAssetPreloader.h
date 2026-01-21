// SuspenseCoreThrowableAssetPreloader.h
// Async preloader for throwable assets to eliminate microfreeze on first grenade use
// Copyright Suspense Team. All Rights Reserved.
//
// PURPOSE:
// Preloads all throwable assets (VFX, Audio, Effects) asynchronously at game start
// to prevent LoadSynchronous() hitches during gameplay.
//
// ARCHITECTURE:
// - GameInstanceSubsystem for global lifecycle
// - Uses FStreamableManager for async loading
// - Integrates with DataManager for SSOT throwable data
// - Publishes events via EventBus when preload completes
//
// FLOW:
// 1. Initialize() called by GameInstance
// 2. Subscribes to DataManager ready event
// 3. When DataManager ready → LoadAllThrowableAssets()
// 4. Async loads all soft references from FSuspenseCoreThrowableAttributeRow
// 5. Caches loaded assets for instant access
// 6. Publishes SuspenseCore.Event.Throwable.AssetsLoaded
//
// USAGE:
// - GrenadeProjectile::InitializeFromSSOT() calls GetPreloadedAsset() instead of LoadSynchronous()
// - GrenadeHandler calls GetPreloadedActorClass() instead of LoadSynchronous()

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/StreamableManager.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreThrowableAssetPreloader.generated.h"

// Forward declarations
class USuspenseCoreDataManager;
class USuspenseCoreEventBus;
class UNiagaraSystem;
class UParticleSystem;
class USoundBase;
class UCameraShakeBase;
class UGameplayEffect;

/**
 * Cached throwable assets for a single throwable item
 */
USTRUCT()
struct FSuspenseCoreThrowableAssetCache
{
	GENERATED_BODY()

	/** Throwable ID this cache belongs to */
	UPROPERTY()
	FName ThrowableID;

	/** Actor class for spawning */
	UPROPERTY()
	TSubclassOf<AActor> ActorClass;

	// ═══════════════════════════════════════════════════════════════════
	// VFX
	// ═══════════════════════════════════════════════════════════════════

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> ExplosionEffect;

	UPROPERTY()
	TObjectPtr<UParticleSystem> ExplosionEffectLegacy;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> SmokeEffect;

	UPROPERTY()
	TObjectPtr<UParticleSystem> SmokeEffectLegacy;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> TrailEffect;

	// ═══════════════════════════════════════════════════════════════════
	// Audio
	// ═══════════════════════════════════════════════════════════════════

	UPROPERTY()
	TObjectPtr<USoundBase> ExplosionSound;

	UPROPERTY()
	TObjectPtr<USoundBase> PinPullSound;

	UPROPERTY()
	TObjectPtr<USoundBase> BounceSound;

	// ═══════════════════════════════════════════════════════════════════
	// Camera Shake
	// ═══════════════════════════════════════════════════════════════════

	UPROPERTY()
	TSubclassOf<UCameraShakeBase> ExplosionCameraShake;

	// ═══════════════════════════════════════════════════════════════════
	// Damage Effects
	// ═══════════════════════════════════════════════════════════════════

	UPROPERTY()
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY()
	TSubclassOf<UGameplayEffect> FlashbangEffectClass;

	UPROPERTY()
	TSubclassOf<UGameplayEffect> IncendiaryEffectClass;

	/** Check if cache has loaded assets */
	bool IsLoaded() const
	{
		return ActorClass != nullptr;
	}
};

/**
 * USuspenseCoreThrowableAssetPreloader
 *
 * GameInstance subsystem that preloads all throwable assets asynchronously
 * to eliminate micro-freezes when grenades are first used.
 *
 * CRITICAL FOR AAA QUALITY:
 * - LoadSynchronous() in gameplay thread causes 50-200ms freezes
 * - This preloader loads all assets at game start (loading screen)
 * - Result: Zero hitches during combat
 *
 * @see FSuspenseCoreThrowableAttributeRow - SSOT for throwable attributes
 * @see USuspenseCoreGrenadeHandler - Uses preloaded classes
 * @see ASuspenseCoreGrenadeProjectile - Uses preloaded assets
 */
UCLASS()
class EQUIPMENTSYSTEM_API USuspenseCoreThrowableAssetPreloader : public UGameInstanceSubsystem
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
	 * Get subsystem from any world context
	 * @param WorldContextObject Any UObject with world context
	 * @return Preloader subsystem or nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Throwable", meta = (WorldContext = "WorldContextObject"))
	static USuspenseCoreThrowableAssetPreloader* Get(const UObject* WorldContextObject);

	// ═══════════════════════════════════════════════════════════════════
	// PUBLIC API - ASSET ACCESS
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Get preloaded actor class for a throwable
	 * Returns immediately if preloaded, falls back to sync load if not
	 *
	 * @param ThrowableID Item ID of the throwable
	 * @return Actor class or nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Throwable")
	TSubclassOf<AActor> GetPreloadedActorClass(FName ThrowableID) const;

	/**
	 * Get full preloaded asset cache for a throwable
	 * Used by GrenadeProjectile::InitializeFromSSOT()
	 *
	 * @param ThrowableID Item ID of the throwable
	 * @param OutCache Output cache structure
	 * @return true if found and loaded
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Throwable")
	bool GetPreloadedAssets(FName ThrowableID, FSuspenseCoreThrowableAssetCache& OutCache) const;

	/**
	 * Check if assets are preloaded for a throwable
	 *
	 * @param ThrowableID Item ID
	 * @return true if preloaded and ready
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Throwable")
	bool AreAssetsPreloaded(FName ThrowableID) const;

	/**
	 * Check if all throwable assets have been preloaded
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Throwable")
	bool IsPreloadComplete() const { return bPreloadComplete; }

	/**
	 * Get number of throwables preloaded
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Throwable")
	int32 GetPreloadedCount() const { return PreloadedAssets.Num(); }

	// ═══════════════════════════════════════════════════════════════════
	// PUBLIC API - MANUAL CONTROL
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Manually trigger asset preload
	 * Usually called automatically, but can be triggered manually
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Throwable")
	void StartPreload();

	/**
	 * Preload a specific throwable by ID
	 * Useful for dynamically added throwables
	 *
	 * @param ThrowableID Item ID to preload
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Throwable")
	void PreloadThrowable(FName ThrowableID);

protected:
	// ═══════════════════════════════════════════════════════════════════
	// INTERNAL STATE
	// ═══════════════════════════════════════════════════════════════════

	/** Cached preloaded assets by throwable ID */
	UPROPERTY()
	TMap<FName, FSuspenseCoreThrowableAssetCache> PreloadedAssets;

	/** Streamable manager for async loading */
	FStreamableManager StreamableManager;

	/** Active loading handles (kept alive until load completes) */
	TArray<TSharedPtr<FStreamableHandle>> ActiveLoadHandles;

	/** Flag indicating preload is complete */
	bool bPreloadComplete = false;

	/** Flag indicating preload has started */
	bool bPreloadStarted = false;

	/** Number of pending loads */
	int32 PendingLoadCount = 0;

	/** Cached DataManager reference */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreDataManager> DataManager;

	/** Cached EventBus reference */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> EventBus;

	// ═══════════════════════════════════════════════════════════════════
	// INTERNAL METHODS
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Load all throwable assets from DataManager
	 * Called when DataManager is ready
	 */
	void LoadAllThrowableAssets();

	/**
	 * Load assets for a single throwable
	 * @param ThrowableID ID of the throwable
	 * @param ItemData Unified item data containing actor class
	 */
	void LoadThrowableAssets(FName ThrowableID, const struct FSuspenseCoreUnifiedItemData& ItemData);

	/**
	 * Called when a single throwable's assets finish loading
	 * @param ThrowableID ID of the loaded throwable
	 */
	void OnThrowableAssetsLoaded(FName ThrowableID);

	/**
	 * Called when all throwable assets finish loading
	 */
	void OnAllAssetsLoaded();

	/**
	 * Callback for DataManager ready event
	 */
	void OnDataManagerReady(FGameplayTag EventTag, const struct FSuspenseCoreEventData& EventData);

	/**
	 * Publish preload complete event via EventBus
	 */
	void PublishPreloadCompleteEvent();
};
