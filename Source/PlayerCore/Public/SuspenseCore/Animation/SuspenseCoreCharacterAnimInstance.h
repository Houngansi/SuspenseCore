// SuspenseCoreCharacterAnimInstance.h
// Copyright Suspense Team. All Rights Reserved.
//
// SINGLE SOURCE OF TRUTH (SSOT) для всех анимационных данных персонажа.
// Данные автоматически загружаются из DataTable, настроенного в Project Settings.
//
// НАСТРОЙКА:
// 1. Создайте DataTable с Row Structure: FSuspenseCoreAnimationData
// 2. Добавьте строки: SMG, Pistol, Shotgun, Sniper, Knife, Special, Frag
// 3. Project Settings → Game → SuspenseCore → Weapon Animations Table
//
// ИСПОЛЬЗОВАНИЕ В ANIMGRAPH:
// - CurrentAnimationData.Stance (BlendSpace)
// - CurrentAnimationData.Locomotion (BlendSpace1D)
// - CurrentAnimationData.Idle, AimIn, AimOut, etc. (AnimSequence)
// - CurrentAnimationData.Draw, Holster, Reload, etc. (AnimMontage)
// - CurrentAnimationData.RHTransform, LHTransform, WTransform (Transform)

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Characters/SuspenseCoreCharacter.h"
#include "SuspenseCore/Types/Animation/SuspenseCoreAnimationState.h"
#include "SuspenseCoreCharacterAnimInstance.generated.h"

// Forward declarations
class USuspenseCoreWeaponStanceComponent;
class USuspenseCoreMovementAttributeSet;
class UAbilitySystemComponent;
class UBlendSpace;
class UBlendSpace1D;
class UAnimMontage;
class UAnimSequence;
class UAnimComposite;
class UDataTable;

/**
 * USuspenseCoreCharacterAnimInstance
 *
 * SINGLE SOURCE OF TRUTH для Animation Blueprint.
 * Все данные обновляются автоматически в NativeUpdateAnimation.
 *
 * АРХИТЕКТУРА:
 * ┌─────────────────────────────────────────────────────────────────┐
 * │  Project Settings → WeaponAnimationsTable (DataTable)          │
 * │  StanceComponent  → WeaponType, CombatState                    │
 * │  Character        → MovementState, Speed                       │
 * │  GAS Attributes   → SprintSpeed, CrouchSpeed                   │
 * │                    ↓                                            │
 * │           AnimInstance (автоматическая загрузка)               │
 * │                    ↓                                            │
 * │           CurrentAnimationData.* (используй в AnimGraph)       │
 * └─────────────────────────────────────────────────────────────────┘
 */
UCLASS()
class PLAYERCORE_API USuspenseCoreCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	USuspenseCoreCharacterAnimInstance();

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

	// ═══════════════════════════════════════════════════════════════════════════════
	// MOVEMENT STATE
	// ═══════════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Movement")
	ESuspenseCoreMovementState MovementState = ESuspenseCoreMovementState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Movement")
	bool bIsSprinting = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Movement")
	bool bIsCrouching = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Movement")
	bool bIsInAir = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Movement")
	bool bIsFalling = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Movement")
	bool bIsJumping = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Movement")
	bool bHasMovementInput = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Movement")
	bool bIsOnGround = true;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Movement")
	bool bIsSliding = false;

	// ═══════════════════════════════════════════════════════════════════════════════
	// POSE STATES / TURN IN PLACE
	// ═══════════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadOnly, Category = "Animation|PoseStates")
	float Lean = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|PoseStates")
	float BodyPitch = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|PoseStates")
	float YawOffset = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|PoseStates")
	float RotationCurve = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|PoseStates")
	float IsTurningCurve = 0.0f;

	// ═══════════════════════════════════════════════════════════════════════════════
	// VELOCITY & DIRECTION (для BlendSpaces)
	// ═══════════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Velocity")
	float Speed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Velocity")
	float GroundSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Velocity")
	float NormalizedSpeed = 0.0f;

	/** Направление движения вперёд (-1..1) - для BlendSpace Axis */
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Direction")
	float MoveForward = 0.0f;

	/** Направление движения вправо (-1..1) - для BlendSpace Axis */
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Direction")
	float MoveRight = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Direction")
	float MovementDirection = 0.0f;

	/** Величина движения (0-2) для State Machine переходов */
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Direction")
	float Movement = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Velocity")
	float VerticalVelocity = 0.0f;

	// ═══════════════════════════════════════════════════════════════════════════════
	// WEAPON STATE
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Тип текущего оружия (GameplayTag) */
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Weapon")
	FGameplayTag CurrentWeaponType;

	/** True когда тип оружия изменился (используй для отладки) */
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Weapon")
	bool bWeaponTypeChanged = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Weapon")
	bool bHasWeaponEquipped = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Weapon")
	bool bIsWeaponDrawn = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Weapon")
	bool bIsAiming = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Weapon")
	float AimingAlpha = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Weapon")
	bool bIsFiring = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Weapon")
	bool bIsReloading = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Weapon")
	bool bIsHoldingBreath = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Weapon")
	bool bIsWeaponMontageActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Weapon")
	float GripModifier = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Weapon")
	float WeaponLoweredAlpha = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Weapon")
	float RecoilAlpha = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Weapon")
	float WeaponSwayMultiplier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Weapon")
	float StoredRecoil = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Weapon")
	float AdditivePitch = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Weapon")
	float BlockDistance = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Weapon")
	bool bIsHolstered = true;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Weapon")
	bool bModifyGrip = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Weapon")
	bool bCreateAimPose = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Weapon")
	float SightDistance = 200.0f;

	/** Pose indices (from WeaponActor) */
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Weapon")
	int32 AimPose = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Weapon")
	int32 StoredPose = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Weapon")
	int32 GripID = 0;

	// ═══════════════════════════════════════════════════════════════════════════════
	// ANIMATION DATA (SSOT - автоматически загружается из DataTable)
	// ═══════════════════════════════════════════════════════════════════════════════

	/**
	 * ВСЕ АНИМАЦИОННЫЕ ДАННЫЕ ДЛЯ ТЕКУЩЕГО ОРУЖИЯ
	 *
	 * Используй в AnimGraph:
	 * - CurrentAnimationData.Stance (BlendSpace для стойки)
	 * - CurrentAnimationData.Locomotion (BlendSpace1D для движения)
	 * - CurrentAnimationData.Idle (AnimSequence)
	 * - CurrentAnimationData.AimPose (AnimComposite)
	 * - CurrentAnimationData.AimIn, AimIdle, AimOut (AnimSequence)
	 * - CurrentAnimationData.Slide, Blocked, GripBlocked (AnimSequence)
	 * - CurrentAnimationData.LeftHandGrip (AnimSequence)
	 * - CurrentAnimationData.GripPoses (AnimComposite)
	 * - CurrentAnimationData.FirstDraw, Draw, Holster (AnimMontage)
	 * - CurrentAnimationData.Firemode, Shoot, AimShoot (AnimMontage)
	 * - CurrentAnimationData.ReloadShort, ReloadLong (AnimMontage)
	 * - CurrentAnimationData.Melee, Throw (AnimMontage)
	 * - CurrentAnimationData.RHTransform, LHTransform, WTransform (FTransform)
	 * - CurrentAnimationData.LHGripTransform (TArray<FTransform>)
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Data")
	FSuspenseCoreAnimationData CurrentAnimationData;

	/** DataTable reference (для отладки) */
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Data")
	TObjectPtr<UDataTable> WeaponAnimationsTable = nullptr;

	// ═══════════════════════════════════════════════════════════════════════════════
	// IK TRANSFORMS (интерполированные)
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Left Hand IK Transform (интерполированный) */
	UPROPERTY(BlueprintReadOnly, Category = "Animation|IK")
	FTransform LeftHandIKTransform = FTransform::Identity;

	/** Right Hand IK Transform (интерполированный) */
	UPROPERTY(BlueprintReadOnly, Category = "Animation|IK")
	FTransform RightHandIKTransform = FTransform::Identity;

	/** Weapon Transform (вычисленный) */
	UPROPERTY(BlueprintReadOnly, Category = "Animation|IK")
	FTransform WeaponTransform = FTransform::Identity;

	/** Left Hand IK Alpha (0 = выкл, 1 = вкл) */
	UPROPERTY(BlueprintReadOnly, Category = "Animation|IK")
	float LeftHandIKAlpha = 0.0f;

	/** Right Hand IK Alpha */
	UPROPERTY(BlueprintReadOnly, Category = "Animation|IK")
	float RightHandIKAlpha = 0.0f;

	// ═══════════════════════════════════════════════════════════════════════════════
	// AIM OFFSET
	// ═══════════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadOnly, Category = "Animation|AimOffset")
	float AimYaw = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|AimOffset")
	float AimPitch = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|AimOffset")
	float TurnInPlaceAngle = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|AimOffset")
	bool bShouldTurnInPlace = false;

	// ═══════════════════════════════════════════════════════════════════════════════
	// PROCEDURAL ANIMATION
	// ═══════════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Procedural")
	float RecoilPitch = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Procedural")
	float RecoilYaw = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Procedural")
	float BreathingAlpha = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Procedural")
	float WeaponSwayAlpha = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Animation|Procedural")
	FVector CameraShake = FVector::ZeroVector;

	// ═══════════════════════════════════════════════════════════════════════════════
	// GAS ATTRIBUTES
	// ═══════════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Attributes")
	float MaxWalkSpeed = 400.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Attributes")
	float MaxSprintSpeed = 600.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Attributes")
	float MaxCrouchSpeed = 200.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Attributes")
	float MaxAimSpeed = 250.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Attributes")
	float JumpHeight = 420.0f;

	// ═══════════════════════════════════════════════════════════════════════════════
	// HELPER METHODS
	// ═══════════════════════════════════════════════════════════════════════════════

	UFUNCTION(BlueprintPure, Category = "Animation")
	ASuspenseCoreCharacter* GetSuspenseCoreCharacter() const;

	UFUNCTION(BlueprintPure, Category = "Animation")
	bool HasValidAnimationData() const;

	UFUNCTION(BlueprintPure, Category = "Animation")
	UAnimMontage* GetDrawMontage(bool bFirstDraw = false) const;

	UFUNCTION(BlueprintPure, Category = "Animation")
	UAnimMontage* GetHolsterMontage() const;

	UFUNCTION(BlueprintPure, Category = "Animation")
	UAnimMontage* GetReloadMontage(bool bTactical = true) const;

	UFUNCTION(BlueprintPure, Category = "Animation")
	UAnimMontage* GetFireMontage(bool bAiming = false) const;

	/**
	 * Get grip pose by index from GripPoses AnimComposite.
	 * Index 0 = Base, 1 = Aim, 2 = Reload, 3+ = Custom
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	UAnimSequenceBase* GetGripPoseByIndex(int32 PoseIndex) const;

	UFUNCTION(BlueprintCallable, Category = "Animation")
	UAnimSequenceBase* GetActiveGripPose() const;

protected:
	// ═══════════════════════════════════════════════════════════════════════════════
	// CACHED REFERENCES
	// ═══════════════════════════════════════════════════════════════════════════════

	UPROPERTY(Transient)
	TWeakObjectPtr<ASuspenseCoreCharacter> CachedCharacter;

	UPROPERTY(Transient)
	TWeakObjectPtr<USuspenseCoreWeaponStanceComponent> CachedStanceComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<UCharacterMovementComponent> CachedMovementComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

	UPROPERTY(Transient)
	TWeakObjectPtr<const USuspenseCoreMovementAttributeSet> CachedMovementAttributes;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CachedWeaponActor;

	// ═══════════════════════════════════════════════════════════════════════════════
	// INTERNAL UPDATE METHODS
	// ═══════════════════════════════════════════════════════════════════════════════

	void UpdateCachedReferences();
	void UpdateMovementData(float DeltaSeconds);
	void UpdateVelocityData(float DeltaSeconds);
	void UpdateWeaponData(float DeltaSeconds);
	void UpdateAnimationAssets();
	void UpdateIKData(float DeltaSeconds);
	void UpdateAimOffsetData(float DeltaSeconds);
	void UpdatePoseStates(float DeltaSeconds);
	void UpdateGASAttributes();
	void LoadWeaponAnimationsTable();

	const FSuspenseCoreAnimationData* GetAnimationDataForWeaponType(const FGameplayTag& WeaponType) const;

private:
	float LastCacheUpdateTime = 0.0f;
	static constexpr float CacheUpdateInterval = 1.0f;
	float PreviousAimingAlpha = 0.0f;
	static constexpr float AimInterpSpeed = 10.0f;

	// Pose States internal
	float LastTickYaw = 0.0f;
	float CurrentYaw = 0.0f;
	float TargetLean = 0.0f;
	static constexpr float LeanInterpSpeed = 10.0f;
	static constexpr float YawOffsetInterpSpeed = 4.0f;
	static constexpr float YawOffsetCurveInterpSpeed = 2.0f;
	static constexpr float MaxYawOffset = 120.0f;

	// IK Interpolation
	FTransform InterpolatedRHTransform = FTransform::Identity;
	FTransform InterpolatedLHTransform = FTransform::Identity;
	float InterpolatedBlockDistance = 0.0f;
	float InterpolatedAdditivePitch = 0.0f;
	static constexpr float TransformInterpSpeed = 8.0f;
	static constexpr float BlockDistanceInterpSpeed = 10.0f;
	static constexpr float AdditivePitchBlendExp = 6.0f;

	static const FName LHTargetSocketName;

	bool GetWeaponLHTargetTransform(FTransform& OutTransform) const;
	FTransform ComputeLHOffsetTransform() const;

	// Track previous weapon type for change detection
	FGameplayTag PreviousWeaponType;
};
