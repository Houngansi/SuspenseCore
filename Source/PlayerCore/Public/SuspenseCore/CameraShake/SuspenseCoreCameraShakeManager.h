// SuspenseCoreCameraShakeManager.h
// SuspenseCore - Layered Camera Shake Manager
// Copyright Suspense Team. All Rights Reserved.
//
// Manages multiple concurrent camera shakes with priority-based blending.
// AAA-standard approach used in Battlefield, Call of Duty, etc.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SuspenseCoreCameraShakeTypes.h"
#include "SuspenseCoreCameraShakeManager.generated.h"

class APlayerController;
class APlayerCameraManager;
class UCameraShakeBase;

/**
 * USuspenseCoreCameraShakeManager
 *
 * Manages layered camera shakes with priority-based blending.
 * Attach to PlayerController or use as singleton per player.
 *
 * Features:
 * - Priority-based shake management
 * - Category-based grouping
 * - Concurrent shake limiting
 * - Automatic cleanup of finished shakes
 *
 * Usage:
 *   Manager->PlayShake(ShakeConfig);
 *   Manager->StopShakesByCategory("Weapon");
 *   Manager->StopShakesByPriority(ESuspenseCoreShakePriority::Movement);
 */
UCLASS(BlueprintType)
class PLAYERCORE_API USuspenseCoreCameraShakeManager : public UObject
{
	GENERATED_BODY()

public:
	USuspenseCoreCameraShakeManager();

	// =========================================================================
	// Initialization
	// =========================================================================

	/** Initialize manager with player controller */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CameraShake")
	void Initialize(APlayerController* InPlayerController);

	/** Check if manager is initialized and ready */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|CameraShake")
	bool IsInitialized() const { return PlayerController.IsValid(); }

	// =========================================================================
	// Shake Playback
	// =========================================================================

	/**
	 * Play a camera shake with full configuration.
	 * @param Config Shake configuration
	 * @return The active shake instance (can be null if blocked)
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CameraShake")
	UCameraShakeBase* PlayShake(const FSuspenseCoreShakeConfig& Config);

	/**
	 * Play a shake with simple parameters.
	 * @param ShakeClass Shake class to play
	 * @param Scale Scale multiplier
	 * @param Priority Priority level
	 * @param Category Optional category name
	 * @return The active shake instance
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CameraShake")
	UCameraShakeBase* PlayShakeSimple(
		TSubclassOf<UCameraShakeBase> ShakeClass,
		float Scale = 1.0f,
		ESuspenseCoreShakePriority Priority = ESuspenseCoreShakePriority::Movement,
		FName Category = NAME_None);

	// =========================================================================
	// Shake Control
	// =========================================================================

	/**
	 * Stop all shakes of a specific category.
	 * @param Category Category to stop
	 * @param bImmediately If true, stop immediately without blend out
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CameraShake")
	void StopShakesByCategory(FName Category, bool bImmediately = false);

	/**
	 * Stop all shakes at or below a priority level.
	 * @param Priority Maximum priority to stop
	 * @param bImmediately If true, stop immediately without blend out
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CameraShake")
	void StopShakesByPriority(ESuspenseCoreShakePriority Priority, bool bImmediately = false);

	/**
	 * Stop all active shakes.
	 * @param bImmediately If true, stop immediately without blend out
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CameraShake")
	void StopAllShakes(bool bImmediately = false);

	/**
	 * Stop a specific shake class.
	 * @param ShakeClass Class to stop
	 * @param bImmediately If true, stop immediately
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|CameraShake")
	void StopShakeClass(TSubclassOf<UCameraShakeBase> ShakeClass, bool bImmediately = false);

	// =========================================================================
	// Query
	// =========================================================================

	/** Get number of active shakes */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|CameraShake")
	int32 GetActiveShakeCount() const;

	/** Get number of active shakes in category */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|CameraShake")
	int32 GetActiveShakeCountByCategory(FName Category) const;

	/** Check if any shake is active at or above priority */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|CameraShake")
	bool HasActiveShakeAtPriority(ESuspenseCoreShakePriority Priority) const;

	/** Get highest active priority */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|CameraShake")
	ESuspenseCoreShakePriority GetHighestActivePriority() const;

	/** Is a specific shake class currently playing? */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|CameraShake")
	bool IsShakeClassPlaying(TSubclassOf<UCameraShakeBase> ShakeClass) const;

	// =========================================================================
	// Configuration
	// =========================================================================

	/** Global shake scale multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float GlobalShakeScale = 1.0f;

	/** Enable priority-based weight reduction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	bool bEnablePriorityBlending = true;

	/** Auto-cleanup interval in seconds (0 = every frame) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (ClampMin = "0.0"))
	float CleanupInterval = 0.5f;

protected:
	/** Update manager state (cleanup finished shakes) */
	void Update();

	/** Remove finished shake layers */
	void CleanupFinishedLayers();

	/** Apply blend mode logic before starting shake */
	void ApplyBlendMode(const FSuspenseCoreShakeConfig& Config);

	/** Get camera manager from player controller */
	APlayerCameraManager* GetCameraManager() const;

private:
	/** Owning player controller */
	UPROPERTY()
	TWeakObjectPtr<APlayerController> PlayerController;

	/** Active shake layers */
	UPROPERTY()
	TArray<FSuspenseCoreShakeLayer> ActiveLayers;

	/** Timer for periodic cleanup */
	FTimerHandle CleanupTimerHandle;

	/** Last cleanup time */
	float LastCleanupTime = 0.0f;
};
