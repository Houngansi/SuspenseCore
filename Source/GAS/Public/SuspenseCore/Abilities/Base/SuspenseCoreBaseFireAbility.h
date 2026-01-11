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
 * Recoil configuration parameters.
 */
USTRUCT(BlueprintType)
struct GAS_API FSuspenseCoreRecoilConfig
{
	GENERATED_BODY()

	/** Base recoil multiplier increase per shot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil")
	float ProgressiveMultiplier;

	/** Maximum recoil multiplier cap */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil")
	float MaximumMultiplier;

	/** Time after last shot to reset recoil counter */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil")
	float ResetTime;

	/** Recoil multiplier when aiming down sights */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil")
	float ADSMultiplier;

	/** Delay before recoil recovery starts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil")
	float RecoveryDelay;

	/** Rate of recoil recovery (per update) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil")
	float RecoveryRate;

	FSuspenseCoreRecoilConfig()
		: ProgressiveMultiplier(1.2f)
		, MaximumMultiplier(3.0f)
		, ResetTime(0.5f)
		, ADSMultiplier(0.5f)
		, RecoveryDelay(0.2f)
		, RecoveryRate(0.9f)
	{
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
	// Recoil System
	//========================================================================

	/**
	 * Apply recoil to player view.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Recoil")
	void ApplyRecoil();

	/**
	 * Get current recoil multiplier based on consecutive shots.
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Recoil")
	float GetCurrentRecoilMultiplier() const;

	/**
	 * Increment consecutive shot counter.
	 */
	void IncrementShotCounter();

	/**
	 * Reset shot counter (called after recoil reset time).
	 */
	void ResetShotCounter();

	/**
	 * Start recoil recovery timer.
	 */
	void StartRecoilRecovery();

	/**
	 * Recoil recovery tick.
	 */
	void RecoverRecoil();

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

	/** Recoil recovery timer handle */
	FTimerHandle RecoilRecoveryTimerHandle;

	/** Recoil reset timer handle */
	FTimerHandle RecoilResetTimerHandle;

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
