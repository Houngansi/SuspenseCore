// Copyright Suspense Team. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeaponAnimation.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeaponCombatState.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCoreWeaponStanceComponent.generated.h"

class USuspenseCoreEventBus;

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

	/** Is weapon blocked by wall/obstacle */
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bIsWeaponBlocked = false;

	/** Is montage currently playing on weapon layer */
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bIsMontageActive = false;

	// -------- Legacy Compatibility --------
	/** Is weapon holstered (inverse of bIsDrawn, for legacy ABP) */
	UPROPERTY(BlueprintReadOnly, Category = "Legacy")
	bool bIsHolstered = true;

	/** Should modify grip (legacy: LegacyModifyGrip) */
	UPROPERTY(BlueprintReadOnly, Category = "Legacy")
	bool bModifyGrip = false;

	/** Should create aim pose (legacy: LegacyCreateAimPose) */
	UPROPERTY(BlueprintReadOnly, Category = "Legacy")
	bool bCreateAimPose = false;

	// -------- Pose Indices (from Weapon Data Table) --------
	/** Current aim pose index */
	UPROPERTY(BlueprintReadOnly, Category = "Pose")
	int32 AimPose = 0;

	/** Stored pose index (for transitions) */
	UPROPERTY(BlueprintReadOnly, Category = "Pose")
	int32 StoredPose = 0;

	/** Current grip ID (for hand placement) */
	UPROPERTY(BlueprintReadOnly, Category = "Pose")
	int32 GripID = 0;

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

	// -------- Weapon IK Transforms --------
	/** Aim transform for weapon positioning */
	UPROPERTY(BlueprintReadOnly, Category = "IK")
	FTransform AimTransform;

	/** Right hand IK transform */
	UPROPERTY(BlueprintReadOnly, Category = "IK")
	FTransform RightHandTransform;

	/** Left hand IK transform */
	UPROPERTY(BlueprintReadOnly, Category = "IK")
	FTransform LeftHandTransform;

	// -------- Procedural Animation --------
	/** Weapon sway multiplier (affected by movement, stamina, etc.) */
	UPROPERTY(BlueprintReadOnly, Category = "Procedural")
	float SwayMultiplier = 1.0f;

	/** Recoil recovery alpha (0 = no recoil, 1 = max recoil) */
	UPROPERTY(BlueprintReadOnly, Category = "Procedural")
	float RecoilAlpha = 0.0f;

	/** Stored recoil value (for procedural animation) */
	UPROPERTY(BlueprintReadOnly, Category = "Procedural")
	float StoredRecoil = 0.0f;

	/** Additive pitch for weapon (recoil, breathing) */
	UPROPERTY(BlueprintReadOnly, Category = "Procedural")
	float AdditivePitch = 0.0f;

	/** Block/wall detection distance */
	UPROPERTY(BlueprintReadOnly, Category = "Procedural")
	float BlockDistance = 0.0f;

	// -------- Aim Target --------
	/** Sight distance - расстояние до цели прицеливания */
	UPROPERTY(BlueprintReadOnly, Category = "AimTarget")
	float SightDistance = 200.0f;
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
class EQUIPMENTSYSTEM_API USuspenseCoreWeaponStanceComponent : public UActorComponent, public ISuspenseCoreWeaponCombatState
{
	GENERATED_BODY()

public:
	USuspenseCoreWeaponStanceComponent();

	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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
	// Implements ISuspenseCoreWeaponCombatState interface
	// ========================================================================

	/** Set aiming state (from ADS ability) */
	UFUNCTION(BlueprintCallable, Category="Weapon|Combat")
	virtual void SetAiming(bool bNewAiming) override;

	/** Set firing state (from weapon fire logic) */
	UFUNCTION(BlueprintCallable, Category="Weapon|Combat")
	virtual void SetFiring(bool bNewFiring) override;

	/** Set reloading state (from WeaponStateManager or reload ability) */
	UFUNCTION(BlueprintCallable, Category="Weapon|Combat")
	virtual void SetReloading(bool bNewReloading) override;

	/** Set breath holding state (from sniper scope ability) */
	UFUNCTION(BlueprintCallable, Category="Weapon|Combat")
	virtual void SetHoldingBreath(bool bNewHoldingBreath) override;

	/** Set montage active state (called by AnimInstance when montage plays/ends) */
	UFUNCTION(BlueprintCallable, Category="Weapon|Combat")
	virtual void SetMontageActive(bool bNewMontageActive) override;

	/** Set weapon blocked state (from WeaponActor wall detection) */
	UFUNCTION(BlueprintCallable, Category="Weapon|Combat")
	void SetWeaponBlocked(bool bNewBlocked);

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

	/** Set aim pose index (from weapon data) */
	UFUNCTION(BlueprintCallable, Category="Weapon|Pose")
	void SetAimPose(int32 NewAimPose);

	/** Set stored pose index (for transitions) */
	UFUNCTION(BlueprintCallable, Category="Weapon|Pose")
	void SetStoredPose(int32 NewStoredPose);

	/** Set grip ID (for hand placement variations) */
	UFUNCTION(BlueprintCallable, Category="Weapon|Pose")
	void SetGripID(int32 NewGripID);

	/** Set modify grip flag (legacy compatibility) */
	UFUNCTION(BlueprintCallable, Category="Weapon|Pose")
	void SetModifyGrip(bool bNewModifyGrip);

	/** Set create aim pose flag (legacy compatibility) */
	UFUNCTION(BlueprintCallable, Category="Weapon|Pose")
	void SetCreateAimPose(bool bNewCreateAimPose);

	// ========================================================================
	// IK Transform API (called by weapon actor)
	// ========================================================================

	/** Set aim transform for weapon positioning */
	UFUNCTION(BlueprintCallable, Category="Weapon|IK")
	void SetAimTransform(const FTransform& NewTransform);

	/** Set right hand IK transform */
	UFUNCTION(BlueprintCallable, Category="Weapon|IK")
	void SetRightHandTransform(const FTransform& NewTransform);

	/** Set left hand IK transform */
	UFUNCTION(BlueprintCallable, Category="Weapon|IK")
	void SetLeftHandTransform(const FTransform& NewTransform);

	/** Set all IK transforms at once (more efficient) */
	UFUNCTION(BlueprintCallable, Category="Weapon|IK")
	void SetWeaponTransforms(const FTransform& InAimTransform, const FTransform& InRightHand, const FTransform& InLeftHand);

	// ========================================================================
	// Procedural Animation API (called by weapon during firing)
	// ========================================================================

	/** Add recoil impulse (decays over time) */
	UFUNCTION(BlueprintCallable, Category="Weapon|Procedural")
	void AddRecoil(float RecoilAmount);

	/** Set sway multiplier (affected by stamina, movement) */
	UFUNCTION(BlueprintCallable, Category="Weapon|Procedural")
	void SetSwayMultiplier(float NewMultiplier);

	/** Set stored recoil value */
	UFUNCTION(BlueprintCallable, Category="Weapon|Procedural")
	void SetStoredRecoil(float NewStoredRecoil);

	/** Set additive pitch (recoil, breathing effects) */
	UFUNCTION(BlueprintCallable, Category="Weapon|Procedural")
	void SetAdditivePitch(float NewAdditivePitch);

	/** Set block distance (wall detection) */
	UFUNCTION(BlueprintCallable, Category="Weapon|Procedural")
	void SetBlockDistance(float NewBlockDistance);

	/** Set sight distance (aim target distance for FABRIK) */
	UFUNCTION(BlueprintCallable, Category="Weapon|AimTarget")
	void SetSightDistance(float NewSightDistance);

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
	// ISuspenseCoreWeaponCombatState interface implementation
	// ========================================================================

	UFUNCTION(BlueprintPure, Category="Weapon|Stance")
	FGameplayTag GetCurrentWeaponType() const { return CurrentWeaponType; }

	UFUNCTION(BlueprintPure, Category="Weapon|Stance")
	virtual bool IsWeaponDrawn() const override { return bWeaponDrawn; }

	UFUNCTION(BlueprintPure, Category="Weapon|Combat")
	virtual bool IsAiming() const override { return bIsAiming; }

	UFUNCTION(BlueprintPure, Category="Weapon|Combat")
	virtual bool IsFiring() const override { return bIsFiring; }

	UFUNCTION(BlueprintPure, Category="Weapon|Combat")
	virtual bool IsReloading() const override { return bIsReloading; }

	UFUNCTION(BlueprintPure, Category="Weapon|Combat")
	virtual bool IsHoldingBreath() const override { return bIsHoldingBreath; }

	UFUNCTION(BlueprintPure, Category="Weapon|Combat")
	virtual bool IsMontageActive() const override { return bIsMontageActive; }

	UFUNCTION(BlueprintPure, Category="Weapon|Combat")
	bool IsWeaponBlocked() const { return bIsWeaponBlocked; }

	UFUNCTION(BlueprintPure, Category="Weapon|Pose")
	virtual float GetAimPoseAlpha() const override { return AimPoseAlpha; }

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

	// ========================================================================
	// EventBus Integration
	// ========================================================================

	/** Get EventBus from owner (cached) */
	USuspenseCoreEventBus* GetEventBus() const;

	/** Broadcast combat state event to EventBus */
	void BroadcastCombatStateEvent(FGameplayTag EventTag) const;

	/** Subscribe to stance change events from EventBus */
	void SubscribeToStanceEvents();

	/** Unsubscribe from stance change events */
	void UnsubscribeFromStanceEvents();

	/** Handler for stance change requested (grenade equip, etc.) */
	void OnStanceChangeRequested(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Handler for stance restore requested (grenade unequip, etc.) */
	void OnStanceRestoreRequested(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

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

	UPROPERTY(ReplicatedUsing=OnRep_CombatState)
	bool bIsWeaponBlocked = false;

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
	// Legacy Compatibility State
	// ========================================================================

	/** Modify grip flag (legacy: LegacyModifyGrip) */
	bool bModifyGrip = false;

	/** Create aim pose flag (legacy: LegacyCreateAimPose) */
	bool bCreateAimPose = false;

	// ========================================================================
	// Pose Indices (from Weapon Data Table)
	// ========================================================================

	/** Current aim pose index */
	int32 AimPose = 0;

	/** Stored pose index (for transitions) */
	int32 StoredPose = 0;

	/** Current grip ID */
	int32 GripID = 0;

	// ========================================================================
	// IK Transforms (from Weapon Actor)
	// ========================================================================

	/** Aim transform for weapon positioning */
	FTransform AimTransform;

	/** Right hand IK transform */
	FTransform RightHandTransform;

	/** Left hand IK transform */
	FTransform LeftHandTransform;

	// ========================================================================
	// Procedural Animation State
	// ========================================================================

	/** Stored recoil value */
	float StoredRecoil = 0.0f;

	/** Additive pitch (recoil, breathing) */
	float AdditivePitch = 0.0f;

	/** Block/wall detection distance (interpolated) */
	float BlockDistance = 0.0f;

	/** Target block distance (set by wall detection, interpolated to BlockDistance) */
	float TargetBlockDistance = 0.0f;

	/** Sight distance for aim target (FABRIK) */
	float SightDistance = 200.0f;

	// ========================================================================
	// Configuration
	// ========================================================================

	/** Speed of aim pose interpolation */
	UPROPERTY(EditDefaultsOnly, Category="Weapon|Config")
	float AimInterpSpeed = 15.0f;

	/** Speed of recoil recovery */
	UPROPERTY(EditDefaultsOnly, Category="Weapon|Config")
	float RecoilRecoverySpeed = 8.0f;

	/** Speed of block distance interpolation (weapon lowering near walls) */
	UPROPERTY(EditDefaultsOnly, Category="Weapon|Config")
	float BlockDistanceInterpSpeed = 8.0f;

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

	/** Cached EventBus reference */
	UPROPERTY(Transient)
	mutable TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	/** Subscription handles for EventBus events */
	FSuspenseCoreSubscriptionHandle StanceChangeHandle;
	FSuspenseCoreSubscriptionHandle StanceRestoreHandle;

	/** Stored previous weapon type for restoration */
	FGameplayTag StoredPreviousWeaponType;
	bool bStoredPreviousDrawnState = false;
};
