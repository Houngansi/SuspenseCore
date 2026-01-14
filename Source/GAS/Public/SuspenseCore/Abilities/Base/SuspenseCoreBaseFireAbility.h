// SuspenseCoreBaseFireAbility.h
// SuspenseCore - Base Fire Ability
// Copyright Suspense Team. All Rights Reserved.
//
// Abstract base class for all weapon fire abilities (Single, Auto, Burst).
// Handles shot generation, server validation, damage application, and visual effects.
//
// ARCHITECTURE:
// - Uses ISuspenseCoreWeaponCombatState for state (DI compliance)
// - Uses ISuspenseCoreWeapon for weapon operations
// - Uses ISuspenseCoreMagazineProvider for ammo
// - EventBus for all event publishing
//
// Usage:
//   Inherit from this class and implement FireNextShot()
//   See USuspenseCoreSingleShotAbility, USuspenseCoreAutoFireAbility, etc.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Abilities/Base/SuspenseCoreAbility.h"
#include "SuspenseCore/Types/Equipment/SuspenseCoreWeaponTypes.h"
#include "SuspenseCoreBaseFireAbility.generated.h"

// Forward declarations
class ISuspenseCoreWeaponCombatState;
class ISuspenseCoreWeapon;
class ISuspenseCoreMagazineProvider;
class USuspenseCoreWeaponAttributeSet;
class UNiagaraSystem;
class USoundBase;
class UAnimMontage;
class UCameraShakeBase;

/**
 * Shot result for server validation and client notification.
 */
USTRUCT(BlueprintType)
struct GAS_API FSuspenseCoreShotResult
{
	GENERATED_BODY()

	/** All hit results from the shot */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	TArray<FHitResult> HitResults;

	/** Whether server validated the shot */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bWasValidated;

	/** Total damage dealt */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	float DamageDealt;

	/** Shot timestamp for ordering */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	float Timestamp;

	FSuspenseCoreShotResult()
		: bWasValidated(false)
		, DamageDealt(0.0f)
		, Timestamp(0.0f)
	{
	}
};

/**
 * FSuspenseCoreRecoilPatternPoint
 *
 * A single point in a recoil pattern sequence.
 * Defines how much the weapon kicks for a specific shot in a burst.
 *
 * @see Documentation/Plans/TarkovStyle_Recoil_System_Design.md Phase 6
 */
USTRUCT(BlueprintType)
struct GAS_API FSuspenseCoreRecoilPatternPoint
{
	GENERATED_BODY()

	/** Pitch offset for this shot (degrees, positive = upward kick) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern")
	float PitchOffset;

	/** Yaw offset for this shot (degrees, positive = right kick) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern")
	float YawOffset;

	FSuspenseCoreRecoilPatternPoint()
		: PitchOffset(0.0f)
		, YawOffset(0.0f)
	{
	}

	FSuspenseCoreRecoilPatternPoint(float InPitch, float InYaw)
		: PitchOffset(InPitch)
		, YawOffset(InYaw)
	{
	}
};

/**
 * FSuspenseCoreRecoilPattern
 *
 * Defines a learnable recoil pattern for a weapon.
 * The pattern is blended with random recoil based on RecoilPatternStrength.
 *
 * USAGE:
 * - PatternStrength 0.0: Pure random recoil
 * - PatternStrength 0.5: 50% pattern, 50% random (semi-predictable)
 * - PatternStrength 1.0: Pure pattern (fully learnable like CS:GO)
 *
 * The pattern loops after all points are used, scaled down for subsequent loops.
 *
 * @see Documentation/Plans/TarkovStyle_Recoil_System_Design.md Phase 6
 */
USTRUCT(BlueprintType)
struct GAS_API FSuspenseCoreRecoilPattern
{
	GENERATED_BODY()

	/** Ordered sequence of recoil pattern points */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern")
	TArray<FSuspenseCoreRecoilPatternPoint> Points;

	/** How much to scale pattern on subsequent loops (0.5 = 50% of original) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern",
		meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float LoopScaleFactor;

	FSuspenseCoreRecoilPattern()
		: LoopScaleFactor(0.7f)
	{
		// Default pattern: Initial strong kick, then alternating
		// This is a placeholder - real patterns should come from SSOT
		Points.Add(FSuspenseCoreRecoilPatternPoint(1.0f, 0.0f));   // Shot 1: Strong up
		Points.Add(FSuspenseCoreRecoilPatternPoint(0.8f, -0.1f));  // Shot 2: Up-left
		Points.Add(FSuspenseCoreRecoilPatternPoint(0.7f, 0.15f));  // Shot 3: Up-right
		Points.Add(FSuspenseCoreRecoilPatternPoint(0.6f, -0.05f)); // Shot 4: Up-slight left
		Points.Add(FSuspenseCoreRecoilPatternPoint(0.5f, 0.1f));   // Shot 5: Up-right
		Points.Add(FSuspenseCoreRecoilPatternPoint(0.4f, 0.0f));   // Shot 6: Stabilizing
		Points.Add(FSuspenseCoreRecoilPatternPoint(0.35f, -0.08f));// Shot 7: Left drift
		Points.Add(FSuspenseCoreRecoilPatternPoint(0.3f, 0.12f));  // Shot 8: Right drift
	}

	/** Get pattern point for a specific shot, with loop scaling */
	FSuspenseCoreRecoilPatternPoint GetPointForShot(int32 ShotIndex) const
	{
		if (Points.Num() == 0)
		{
			return FSuspenseCoreRecoilPatternPoint();
		}

		int32 LoopCount = ShotIndex / Points.Num();
		int32 IndexInLoop = ShotIndex % Points.Num();

		FSuspenseCoreRecoilPatternPoint Point = Points[IndexInLoop];

		// Scale down for subsequent loops
		if (LoopCount > 0)
		{
			float Scale = FMath::Pow(LoopScaleFactor, static_cast<float>(LoopCount));
			Point.PitchOffset *= Scale;
			Point.YawOffset *= Scale;
		}

		return Point;
	}
};

/**
 * Recoil configuration parameters.
 * Controls progressive recoil buildup, ADS modifiers, and visual/aim separation.
 *
 * @see Documentation/Plans/TarkovStyle_Recoil_System_Design.md Phase 5
 */
USTRUCT(BlueprintType)
struct GAS_API FSuspenseCoreRecoilConfig
{
	GENERATED_BODY()

	/** Base recoil multiplier increase per shot (e.g., 1.2 = +20% per shot) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil|Progressive")
	float ProgressiveMultiplier;

	/** Maximum recoil multiplier cap (e.g., 3.0 = max 3x recoil) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil|Progressive")
	float MaximumMultiplier;

	/** Time after last shot to reset recoil counter (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil|Progressive")
	float ResetTime;

	/** Recoil multiplier when aiming down sights (0.5 = 50% recoil when ADS) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil|ADS")
	float ADSMultiplier;

	//========================================================================
	// Visual vs Aim Recoil Separation (Phase 5)
	// @see Documentation/Plans/TarkovStyle_Recoil_System_Design.md Section 2.1
	//========================================================================

	/** Visual recoil multiplier - how much stronger camera kick feels vs actual aim
	 *  1.5 = Visual recoil is 50% stronger than aim recoil
	 *  This creates the Tarkov "feel" where weapon kicks visually but aim recovers faster */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil|Visual",
		meta = (ClampMin = "1.0", ClampMax = "3.0", ToolTip = "Visual kick multiplier (1.5 = 50% stronger camera kick)"))
	float VisualRecoilMultiplier;

	/** How quickly visual recoil converges relative to aim recoil
	 *  1.0 = same rate, 1.5 = visual converges 50% faster than aim
	 *  Faster visual convergence makes gun "feel" stable while aim still drifts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil|Visual",
		meta = (ClampMin = "0.5", ClampMax = "2.0", ToolTip = "Visual convergence speed multiplier"))
	float VisualConvergenceMultiplier;

	FSuspenseCoreRecoilConfig()
		: ProgressiveMultiplier(1.2f)
		, MaximumMultiplier(3.0f)
		, ResetTime(0.5f)
		, ADSMultiplier(0.5f)
		, VisualRecoilMultiplier(1.5f)
		, VisualConvergenceMultiplier(1.2f)
	{
	}
};


/**
 * FSuspenseCoreRecoilState
 *
 * Runtime state for Tarkov-style recoil with convergence and visual/aim separation.
 * Tracks accumulated camera offset and manages auto-return to aim point.
 *
 * CONVERGENCE SYSTEM:
 * After each shot, camera is displaced by recoil. Over time (after ConvergenceDelay),
 * camera automatically returns to original aim point at ConvergenceSpeed.
 * This creates the characteristic Tarkov "kick and return" feel.
 *
 * VISUAL VS AIM SEPARATION (Phase 5):
 * - VisualPitch/Yaw: What player SEES (camera kick, stronger for feel)
 * - AimPitch/Yaw: Where bullets GO (actual aim offset, more accurate)
 * - Visual recoil can be 50% stronger for dramatic effect
 * - Visual converges faster so gun "feels" stable while aim still drifts
 *
 * @see Documentation/Plans/TarkovStyle_Recoil_System_Design.md Section 2.4
 */
USTRUCT(BlueprintType)
struct GAS_API FSuspenseCoreRecoilState
{
	GENERATED_BODY()

	//========================================================================
	// Visual Recoil State (what player SEES)
	// Applied to camera, can be stronger for "feel"
	//========================================================================

	/** Current visual camera pitch offset (degrees, negative = up)
	 *  This is what the player sees - can be stronger than aim recoil */
	UPROPERTY(BlueprintReadOnly, Category = "Recoil|Visual")
	float VisualPitch;

	/** Current visual camera yaw offset (degrees) */
	UPROPERTY(BlueprintReadOnly, Category = "Recoil|Visual")
	float VisualYaw;

	//========================================================================
	// Aim Recoil State (where bullets GO)
	// Applied to shot direction, determines actual accuracy
	//========================================================================

	/** Current aim pitch offset (degrees, negative = up)
	 *  This affects where bullets actually go - more stable than visual */
	UPROPERTY(BlueprintReadOnly, Category = "Recoil|Aim")
	float AimPitch;

	/** Current aim yaw offset (degrees) */
	UPROPERTY(BlueprintReadOnly, Category = "Recoil|Aim")
	float AimYaw;

	//========================================================================
	// Legacy/Combined State (for backward compatibility)
	//========================================================================

	/** Current accumulated camera pitch offset (degrees, negative = up)
	 *  @deprecated Use VisualPitch for camera, AimPitch for bullets */
	UPROPERTY(BlueprintReadOnly, Category = "Recoil|State")
	float AccumulatedPitch;

	/** Current accumulated camera yaw offset (degrees)
	 *  @deprecated Use VisualYaw for camera, AimYaw for bullets */
	UPROPERTY(BlueprintReadOnly, Category = "Recoil|State")
	float AccumulatedYaw;

	/** Target pitch to converge to (usually 0 for return to original aim) */
	UPROPERTY(BlueprintReadOnly, Category = "Recoil|State")
	float TargetPitch;

	/** Target yaw to converge to (usually 0 for return to original aim) */
	UPROPERTY(BlueprintReadOnly, Category = "Recoil|State")
	float TargetYaw;

	//========================================================================
	// Timing State
	//========================================================================

	/** Time since last shot (seconds) */
	UPROPERTY(BlueprintReadOnly, Category = "Recoil|Timing")
	float TimeSinceLastShot;

	/** Whether convergence is currently active */
	UPROPERTY(BlueprintReadOnly, Category = "Recoil|State")
	bool bIsConverging;

	/** Whether we're waiting for convergence delay */
	UPROPERTY(BlueprintReadOnly, Category = "Recoil|State")
	bool bWaitingForConvergence;

	//========================================================================
	// Weapon Data Cache (from SSOT)
	//========================================================================

	/** Cached convergence speed from weapon data */
	float CachedConvergenceSpeed;

	/** Cached convergence delay from weapon data */
	float CachedConvergenceDelay;

	/** Cached ergonomics from weapon data */
	float CachedErgonomics;

	/** Cached recoil angle bias from weapon data */
	float CachedRecoilBias;

	/** Cached pattern strength from weapon data (0=random, 1=pure pattern) */
	float CachedPatternStrength;

	FSuspenseCoreRecoilState()
		: VisualPitch(0.0f)
		, VisualYaw(0.0f)
		, AimPitch(0.0f)
		, AimYaw(0.0f)
		, AccumulatedPitch(0.0f)
		, AccumulatedYaw(0.0f)
		, TargetPitch(0.0f)
		, TargetYaw(0.0f)
		, TimeSinceLastShot(0.0f)
		, bIsConverging(false)
		, bWaitingForConvergence(false)
		, CachedConvergenceSpeed(5.0f)
		, CachedConvergenceDelay(0.1f)
		, CachedErgonomics(42.0f)
		, CachedRecoilBias(0.0f)
		, CachedPatternStrength(0.3f)
	{
	}

	/** Reset all state (on weapon switch or death) */
	void Reset()
	{
		VisualPitch = 0.0f;
		VisualYaw = 0.0f;
		AimPitch = 0.0f;
		AimYaw = 0.0f;
		AccumulatedPitch = 0.0f;
		AccumulatedYaw = 0.0f;
		TargetPitch = 0.0f;
		TargetYaw = 0.0f;
		TimeSinceLastShot = 0.0f;
		bIsConverging = false;
		bWaitingForConvergence = false;
	}

	/** Check if camera has any accumulated offset (visual or aim) */
	bool HasOffset() const
	{
		return HasVisualOffset() || HasAimOffset();
	}

	/** Check if visual recoil has any offset */
	bool HasVisualOffset() const
	{
		return !FMath::IsNearlyZero(VisualPitch, 0.01f) ||
			   !FMath::IsNearlyZero(VisualYaw, 0.01f);
	}

	/** Check if aim recoil has any offset */
	bool HasAimOffset() const
	{
		return !FMath::IsNearlyZero(AimPitch, 0.01f) ||
			   !FMath::IsNearlyZero(AimYaw, 0.01f);
	}

	/** Get effective convergence speed with ergonomics bonus
	 *  Formula: Speed × (1 + Ergonomics/100)
	 *  42 ergo = 1.42× speed, 70 ergo = 1.70× speed */
	float GetEffectiveConvergenceSpeed() const
	{
		return CachedConvergenceSpeed * (1.0f + CachedErgonomics / 100.0f);
	}

	/** Get aim offset as FRotator (for applying to shot direction)
	 *  AimPitch stores CORRECTION (Aim - Visual), typically negative.
	 *  Applied to camera direction to get bullet direction.
	 *  Negative pitch = bullets go less up than camera = more accurate */
	FRotator GetAimOffsetRotator() const
	{
		return FRotator(AimPitch, AimYaw, 0.0f);
	}
};

/**
 * USuspenseCoreBaseFireAbility
 *
 * Abstract base class for all fire abilities.
 * Provides common functionality for shot generation, validation,
 * damage application, and visual effects.
 *
 * Child classes implement FireNextShot() to define firing behavior:
 * - Single: Fire once, end immediately
 * - Auto: Fire continuously while input held
 * - Burst: Fire N shots with delay, then end
 *
 * ARCHITECTURE COMPLIANCE:
 * - Uses interfaces from BridgeSystem (no direct EquipmentSystem deps)
 * - State management through ISuspenseCoreWeaponCombatState
 * - EventBus for all event publishing
 * - Native tags for compile-time safety
 */
UCLASS(Abstract)
class GAS_API USuspenseCoreBaseFireAbility : public USuspenseCoreAbility
{
	GENERATED_BODY()

public:
	USuspenseCoreBaseFireAbility();

	//========================================================================
	// Configuration
	//========================================================================

	/** Fire montage to play on character */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Animation")
	TObjectPtr<UAnimMontage> FireMontage;

	/** Fire sound to play */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Audio")
	TObjectPtr<USoundBase> FireSound;

	/** Muzzle flash effect */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Effects")
	TObjectPtr<UNiagaraSystem> MuzzleFlashEffect;

	/** Bullet tracer effect */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Effects")
	TObjectPtr<UNiagaraSystem> TracerEffect;

	/** Impact effect (generic) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Effects")
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	/** Camera shake class for recoil */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Effects")
	TSubclassOf<UCameraShakeBase> RecoilCameraShake;

	/** Recoil configuration */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Recoil")
	FSuspenseCoreRecoilConfig RecoilConfig;

	/** Recoil pattern for learnable spray patterns (Phase 6)
	 *  Blended with random recoil based on weapon's RecoilPatternStrength */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Recoil")
	FSuspenseCoreRecoilPattern RecoilPattern;

	/** Enable debug visualization */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Debug")
	bool bDebugTraces;

protected:
	//========================================================================
	// GameplayAbility Interface
	//========================================================================

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags
	) const override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled
	) override;

	virtual void InputPressed(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo
	) override;

	virtual void InputReleased(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo
	) override;

	//========================================================================
	// Core Fire Methods (implement in children)
	//========================================================================

	/**
	 * Execute the next shot. PURE VIRTUAL - must be implemented by children.
	 * Called by ActivateAbility and timer callbacks for auto/burst fire.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "SuspenseCore|Fire")
	void FireNextShot();
	virtual void FireNextShot_Implementation() PURE_VIRTUAL(USuspenseCoreBaseFireAbility::FireNextShot_Implementation, );

	//========================================================================
	// Shot Generation
	//========================================================================

	/**
	 * Generate shot request with current weapon state.
	 * Calculates muzzle location, aim direction, spread, etc.
	 *
	 * @return Shot parameters ready for trace
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Fire")
	FWeaponShotParams GenerateShotRequest();

	/**
	 * Execute a single shot (for use by child classes).
	 * Handles trace, damage, effects, and ammo consumption.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Fire")
	void ExecuteSingleShot();

	//========================================================================
	// Server Validation & Damage
	//========================================================================

	/**
	 * Server RPC to validate and process shot.
	 */
	UFUNCTION(Server, Reliable, WithValidation, Category = "SuspenseCore|Network")
	void ServerFireShot(const FWeaponShotParams& ShotRequest);
	bool ServerFireShot_Validate(const FWeaponShotParams& ShotRequest);
	void ServerFireShot_Implementation(const FWeaponShotParams& ShotRequest);

	/**
	 * Client RPC to receive confirmed shot result.
	 */
	UFUNCTION(Client, Reliable, Category = "SuspenseCore|Network")
	void ClientReceiveShotResult(const FSuspenseCoreShotResult& ShotResult);
	void ClientReceiveShotResult_Implementation(const FSuspenseCoreShotResult& ShotResult);

	/**
	 * Validate shot request (server-side).
	 */
	bool ValidateShotRequest(const FWeaponShotParams& ShotRequest) const;

	/**
	 * Process shot trace on server.
	 */
	void ServerProcessShotTrace(const FWeaponShotParams& ShotRequest, FSuspenseCoreShotResult& OutResult);

	/**
	 * Apply damage to hit targets.
	 */
	void ApplyDamageToTargets(const TArray<FHitResult>& HitResults, float BaseDamage);

	//========================================================================
	// Visual Effects
	//========================================================================

	/**
	 * Play local fire effects (montage, sound, muzzle flash).
	 * Called on owning client only.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Effects")
	void PlayLocalFireEffects();

	/**
	 * Play impact effects at hit locations.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Effects")
	void PlayImpactEffects(const TArray<FHitResult>& HitResults);

	/**
	 * Spawn tracer from muzzle to target.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Effects")
	void SpawnTracer(const FVector& Start, const FVector& End);

	//========================================================================
	// Recoil System (Tarkov-Style with Convergence)
	// @see Documentation/Plans/TarkovStyle_Recoil_System_Design.md
	//========================================================================

	/**
	 * Apply recoil impulse to player view and start convergence tracking.
	 * Called on each shot. Adds to AccumulatedPitch/Yaw in RecoilState.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Recoil")
	void ApplyRecoil();

	/**
	 * Tick convergence system - call every frame while RecoilState has offset.
	 * Gradually returns camera to original aim point after ConvergenceDelay.
	 * @param DeltaTime Time since last tick
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Recoil")
	void TickConvergence(float DeltaTime);

	/**
	 * Initialize recoil state from weapon attributes (SSOT).
	 * Caches ConvergenceSpeed, ConvergenceDelay, Ergonomics, RecoilBias.
	 */
	void InitializeRecoilStateFromWeapon();

	/**
	 * Calculate total attachment recoil modifier.
	 * Multiplies all equipped attachment RecoilModifiers.
	 * @return Combined modifier (e.g., 0.727 = 27% reduction)
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Recoil")
	float CalculateAttachmentRecoilModifier() const;

	/**
	 * Get current recoil multiplier based on consecutive shots.
	 * Formula: 1.0 + (ShotCount-1) × (ProgressiveMultiplier-1), capped at MaximumMultiplier
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Recoil")
	float GetCurrentRecoilMultiplier() const;

	/**
	 * Get current recoil state (for debugging/UI).
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Recoil")
	const FSuspenseCoreRecoilState& GetRecoilState() const { return RecoilState; }

	/**
	 * Increment consecutive shot counter.
	 */
	void IncrementShotCounter();

	/**
	 * Reset shot counter (called after recoil reset time).
	 */
	void ResetShotCounter();

	/**
	 * Start convergence timer after shot.
	 */
	void StartConvergenceTimer();

	/**
	 * Bind to world tick for convergence updates.
	 */
	void BindToWorldTick();

	/**
	 * Unbind from world tick.
	 */
	void UnbindFromWorldTick();

	/**
	 * Timer callback for convergence updates.
	 * Called at 60Hz rate to update recoil recovery.
	 */
	void OnConvergenceTick();

	//========================================================================
	// Ammunition
	//========================================================================

	/**
	 * Consume ammo from magazine.
	 * @param Amount Ammo to consume (default 1)
	 * @return True if ammo was consumed
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Ammo")
	bool ConsumeAmmo(int32 Amount = 1);

	/**
	 * Check if weapon has ammo.
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Ammo")
	bool HasAmmo() const;

	//========================================================================
	// Interface Access (DI compliant)
	//========================================================================

	/**
	 * Get weapon combat state interface.
	 * @return Combat state interface or nullptr
	 */
	ISuspenseCoreWeaponCombatState* GetWeaponCombatState() const;

	/**
	 * Get weapon interface from current weapon actor.
	 * @return Weapon interface or nullptr
	 */
	ISuspenseCoreWeapon* GetWeaponInterface() const;

	/**
	 * Get magazine provider interface.
	 * @return Magazine provider or nullptr
	 */
	ISuspenseCoreMagazineProvider* GetMagazineProvider() const;

	/**
	 * Get weapon attribute set.
	 * @return Attribute set or nullptr
	 */
	const USuspenseCoreWeaponAttributeSet* GetWeaponAttributes() const;

	/**
	 * Get muzzle world location.
	 */
	FVector GetMuzzleLocation() const;

	/**
	 * Get aim direction (screen center).
	 */
	FVector GetAimDirection() const;

	//========================================================================
	// EventBus Publishing
	//========================================================================

	/** Publish weapon fired event */
	void PublishWeaponFiredEvent(const FWeaponShotParams& ShotParams, bool bSuccess);

	/** Publish ammo changed event */
	void PublishAmmoChangedEvent();

	/** Publish spread changed event */
	void PublishSpreadChangedEvent(float NewSpread);

	//========================================================================
	// State
	//========================================================================

	/** Current consecutive shot count (for recoil) */
	int32 ConsecutiveShotsCount;

	/** Time of last shot (for recoil reset) */
	float LastShotTime;

	/** Pending shots awaiting server confirmation */
	TArray<FWeaponShotParams> PendingShots;

	/** Recoil reset timer handle */
	FTimerHandle RecoilResetTimerHandle;

	/** Convergence delay timer handle */
	FTimerHandle ConvergenceDelayTimerHandle;

	//========================================================================
	// Recoil State (Tarkov-Style Convergence)
	//========================================================================

	/** Runtime recoil state with convergence tracking */
	FSuspenseCoreRecoilState RecoilState;

	/** Timer handle for convergence tick updates */
	FTimerHandle ConvergenceTickTimerHandle;

	/** Whether we're currently bound to convergence tick timer */
	bool bBoundToWorldTick;

	//========================================================================
	// Validation Constants
	//========================================================================

	/** Max allowed distance between claimed origin and actual muzzle */
	static constexpr float MaxAllowedOriginDistance = 300.0f;

	/** Max allowed time difference between client and server */
	static constexpr float MaxTimeDifference = 2.0f;

	/** Max allowed angle difference for direction validation */
	static constexpr float MaxAngleDifference = 45.0f;
};
