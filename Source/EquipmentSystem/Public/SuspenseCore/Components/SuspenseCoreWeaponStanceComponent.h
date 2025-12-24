// Copyright Suspense Team. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeaponAnimation.h"
#include "SuspenseCoreWeaponStanceComponent.generated.h"

/**
 * Snapshot of weapon stance state for animation system
 * Passed to AnimInstance each frame for efficient data access
 */
USTRUCT(BlueprintType)
struct EQUIPMENTSYSTEM_API FSuspenseCoreWeaponStanceSnapshot
{
	GENERATED_BODY()

	// -------- Weapon Identity --------
	/** Current weapon type tag (e.g., Weapon.Type.Rifle.AssaultRifle) */
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	FGameplayTag WeaponType;

	/** Is weapon currently drawn (not holstered) */
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	bool bIsDrawn = false;

	// -------- Combat States --------
	/** Is player currently aiming down sights */
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bIsAiming = false;

	/** Is weapon currently firing */
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bIsFiring = false;

	/** Is weapon currently reloading */
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bIsReloading = false;

	/** Is player holding breath for stability */
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bIsHoldingBreath = false;

	/** Is montage currently playing on weapon layer */
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bIsMontageActive = false;

	// -------- Pose Modifiers --------
	/** Aim pose alpha (0 = hip fire, 1 = full ADS) */
	UPROPERTY(BlueprintReadOnly, Category = "Pose")
	float AimPoseAlpha = 0.0f;

	/** Grip modifier (0 = default grip, 1 = modified/tactical grip) */
	UPROPERTY(BlueprintReadOnly, Category = "Pose")
	float GripModifier = 0.0f;

	/** Weapon lowered amount (0 = normal, 1 = fully lowered, e.g., near wall) */
	UPROPERTY(BlueprintReadOnly, Category = "Pose")
	float WeaponLoweredAlpha = 0.0f;

	// -------- Procedural Animation --------
	/** Weapon sway multiplier (affected by movement, stamina, etc.) */
	UPROPERTY(BlueprintReadOnly, Category = "Procedural")
	float SwayMultiplier = 1.0f;

	/** Recoil recovery alpha (0 = no recoil, 1 = max recoil) */
	UPROPERTY(BlueprintReadOnly, Category = "Procedural")
	float RecoilAlpha = 0.0f;
};

/**
 * Weapon Stance Component
 *
 * Single Source of Truth (SSOT) for weapon animation state on character.
 * Provides all weapon-related state needed by the animation system.
 *
 * Architecture:
 * - Replicated for multiplayer synchronization
 * - Combat states set by GAS abilities and weapon logic
 * - AnimInstance reads snapshot each frame via GetStanceSnapshot()
 * - ISuspenseCoreWeaponAnimation provides assets based on WeaponType
 *
 * Usage:
 * - Equipment system calls SetWeaponStance() when weapon changes
 * - GAS abilities call SetAiming(), SetFiring(), etc.
 * - Weapon actor calls SetRecoil(), SetWeaponLowered()
 * - AnimInstance calls GetStanceSnapshot() in NativeUpdateAnimation()
 */
UCLASS(ClassGroup=(Equipment), meta=(BlueprintSpawnableComponent), BlueprintType, Blueprintable)
class EQUIPMENTSYSTEM_API USuspenseCoreWeaponStanceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USuspenseCoreWeaponStanceComponent();

	//~ Begin UActorComponent Interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent Interface

	// ========================================================================
	// Equipment System API (called by USuspenseCoreEquipmentAttachmentComponent)
	// ========================================================================

	UFUNCTION(BlueprintCallable, Category="Weapon|Stance")
	void OnEquipmentChanged(AActor* NewEquipmentActor);

	UFUNCTION(BlueprintCallable, Category="Weapon|Stance")
	void SetWeaponStance(const FGameplayTag& WeaponTypeTag, bool bImmediate = false);

	UFUNCTION(BlueprintCallable, Category="Weapon|Stance")
	void ClearWeaponStance(bool bImmediate = false);

	UFUNCTION(BlueprintCallable, Category="Weapon|Stance")
	void SetWeaponDrawnState(bool bDrawn);

	// ========================================================================
	// Combat State API (called by GAS abilities and weapon logic)
	// ========================================================================

	/** Set aiming state (from ADS ability) */
	UFUNCTION(BlueprintCallable, Category="Weapon|Combat")
	void SetAiming(bool bNewAiming);

	/** Set firing state (from weapon fire logic) */
	UFUNCTION(BlueprintCallable, Category="Weapon|Combat")
	void SetFiring(bool bNewFiring);

	/** Set reloading state (from WeaponStateManager or reload ability) */
	UFUNCTION(BlueprintCallable, Category="Weapon|Combat")
	void SetReloading(bool bNewReloading);

	/** Set breath holding state (from sniper scope ability) */
	UFUNCTION(BlueprintCallable, Category="Weapon|Combat")
	void SetHoldingBreath(bool bNewHoldingBreath);

	/** Set montage active state (called by AnimInstance when montage plays/ends) */
	UFUNCTION(BlueprintCallable, Category="Weapon|Combat")
	void SetMontageActive(bool bNewMontageActive);

	// ========================================================================
	// Pose Modifier API (called by weapon/environment logic)
	// ========================================================================

	/** Set target aim pose alpha (interpolated in Tick) */
	UFUNCTION(BlueprintCallable, Category="Weapon|Pose")
	void SetTargetAimPose(float TargetAlpha);

	/** Set grip modifier (0 = default, 1 = tactical/modified grip) */
	UFUNCTION(BlueprintCallable, Category="Weapon|Pose")
	void SetGripModifier(float NewGripModifier);

	/** Set weapon lowered state (e.g., near wall) */
	UFUNCTION(BlueprintCallable, Category="Weapon|Pose")
	void SetWeaponLowered(float LoweredAlpha);

	// ========================================================================
	// Procedural Animation API (called by weapon during firing)
	// ========================================================================

	/** Add recoil impulse (decays over time) */
	UFUNCTION(BlueprintCallable, Category="Weapon|Procedural")
	void AddRecoil(float RecoilAmount);

	/** Set sway multiplier (affected by stamina, movement) */
	UFUNCTION(BlueprintCallable, Category="Weapon|Procedural")
	void SetSwayMultiplier(float NewMultiplier);

	// ========================================================================
	// Animation System API (called by AnimInstance)
	// ========================================================================

	/** Get complete stance snapshot for animation (call once per frame) */
	UFUNCTION(BlueprintPure, Category="Weapon|Animation")
	FSuspenseCoreWeaponStanceSnapshot GetStanceSnapshot() const;

	/** Get animation data interface for current weapon type */
	UFUNCTION(BlueprintCallable, Category="Weapon|Animation")
	TScriptInterface<ISuspenseCoreWeaponAnimation> GetAnimationInterface() const;

	// ========================================================================
	// Getters for individual states
	// ========================================================================

	UFUNCTION(BlueprintPure, Category="Weapon|Stance")
	FGameplayTag GetCurrentWeaponType() const { return CurrentWeaponType; }

	UFUNCTION(BlueprintPure, Category="Weapon|Stance")
	bool IsWeaponDrawn() const { return bWeaponDrawn; }

	UFUNCTION(BlueprintPure, Category="Weapon|Combat")
	bool IsAiming() const { return bIsAiming; }

	UFUNCTION(BlueprintPure, Category="Weapon|Combat")
	bool IsFiring() const { return bIsFiring; }

	UFUNCTION(BlueprintPure, Category="Weapon|Combat")
	bool IsReloading() const { return bIsReloading; }

	UFUNCTION(BlueprintPure, Category="Weapon|Combat")
	bool IsHoldingBreath() const { return bIsHoldingBreath; }

	UFUNCTION(BlueprintPure, Category="Weapon|Combat")
	bool IsMontageActive() const { return bIsMontageActive; }

	UFUNCTION(BlueprintPure, Category="Weapon|Pose")
	float GetAimPoseAlpha() const { return AimPoseAlpha; }

	UFUNCTION(BlueprintPure, Category="Weapon|Stance")
	AActor* GetTrackedEquipmentActor() const { return TrackedEquipmentActor.Get(); }

protected:
	// Replication callbacks
	UFUNCTION()
	void OnRep_WeaponType();

	UFUNCTION()
	void OnRep_DrawnState();

	UFUNCTION()
	void OnRep_CombatState();

	// Push current stance to animation layer
	void PushToAnimationLayer(bool bSkipIfNoInterface = true) const;

	// Update interpolated values
	void UpdateInterpolatedValues(float DeltaTime);

private:
	// ========================================================================
	// Replicated Weapon Identity
	// ========================================================================

	UPROPERTY(ReplicatedUsing=OnRep_WeaponType)
	FGameplayTag CurrentWeaponType;

	UPROPERTY(ReplicatedUsing=OnRep_DrawnState)
	bool bWeaponDrawn = false;

	// ========================================================================
	// Replicated Combat States
	// ========================================================================

	UPROPERTY(ReplicatedUsing=OnRep_CombatState)
	bool bIsAiming = false;

	UPROPERTY(ReplicatedUsing=OnRep_CombatState)
	bool bIsFiring = false;

	UPROPERTY(ReplicatedUsing=OnRep_CombatState)
	bool bIsReloading = false;

	UPROPERTY(ReplicatedUsing=OnRep_CombatState)
	bool bIsHoldingBreath = false;

	// ========================================================================
	// Non-replicated Local State (visual only)
	// ========================================================================

	/** Is montage currently playing (tracked locally) */
	bool bIsMontageActive = false;

	/** Current aim pose alpha (interpolated) */
	float AimPoseAlpha = 0.0f;

	/** Target aim pose alpha (set by SetTargetAimPose) */
	float TargetAimPoseAlpha = 0.0f;

	/** Current grip modifier */
	float GripModifier = 0.0f;

	/** Weapon lowered alpha (near wall, etc.) */
	float WeaponLoweredAlpha = 0.0f;

	/** Sway multiplier */
	float SwayMultiplier = 1.0f;

	/** Current recoil alpha (decays over time) */
	float RecoilAlpha = 0.0f;

	// ========================================================================
	// Configuration
	// ========================================================================

	/** Speed of aim pose interpolation */
	UPROPERTY(EditDefaultsOnly, Category="Weapon|Config")
	float AimInterpSpeed = 15.0f;

	/** Speed of recoil recovery */
	UPROPERTY(EditDefaultsOnly, Category="Weapon|Config")
	float RecoilRecoverySpeed = 8.0f;

	// ========================================================================
	// Internal State
	// ========================================================================

	/** Tracked equipment actor (weak reference) */
	UPROPERTY()
	TWeakObjectPtr<AActor> TrackedEquipmentActor;

	/** Cached animation interface */
	UPROPERTY(Transient)
	mutable TScriptInterface<ISuspenseCoreWeaponAnimation> CachedAnimationInterface;

	/** Animation interface cache lifetime */
	UPROPERTY(EditDefaultsOnly, Category="Weapon|Config")
	float AnimationInterfaceCacheLifetime = 0.25f;

	mutable float LastAnimationInterfaceCacheTime = -1000.0f;
};
