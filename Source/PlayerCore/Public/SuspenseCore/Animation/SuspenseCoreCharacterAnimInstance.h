// SuspenseCoreCharacterAnimInstance.h
// Copyright Suspense Team. All Rights Reserved.
//
// AnimInstance с логикой определения оружия.
// Интерполяция и M LH Offset реализуются в Blueprint.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreCharacterAnimInstance.generated.h"

// Forward declarations
class UBlendSpace;
class UBlendSpace1D;
class UAnimMontage;
class UAnimSequence;
class UAnimSequenceBase;
class UAnimComposite;
class ASuspenseCoreCharacter;
class USuspenseCoreWeaponStanceComponent;

/**
 * USuspenseCoreCharacterAnimInstance
 *
 * AnimInstance с автоматическим определением оружия из WeaponStanceComponent.
 * Интерполяция и вычисление трансформов реализуются в Blueprint EventGraph.
 */
UCLASS()
class PLAYERCORE_API USuspenseCoreCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	USuspenseCoreCharacterAnimInstance();

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// ═══════════════════════════════════════════════════════════════════════════════
	// WEAPON IDENTIFICATION (автоматически из WeaponStanceComponent)
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Тип текущего оружия (GameplayTag) - определяется автоматически */
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	FGameplayTag CurrentWeaponType;

	/** Предыдущий тип оружия (для отслеживания смены) */
	UPROPERTY(BlueprintReadWrite, Category = "Weapon")
	FGameplayTag LastWeaponType;

	/** Оружие экипировано */
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	bool bHasWeaponEquipped = false;

	/** Оружие в руках (не в кобуре) */
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	bool bIsWeaponDrawn = false;

	/** Grip ID - индекс хвата из оружия */
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	int32 GripID = 0;

	/** Aim Pose индекс */
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	int32 AimPose = 0;

	/** Stored Pose индекс */
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	int32 StoredPose = 0;

	// ═══════════════════════════════════════════════════════════════════════════════
	// WEAPON STATE (автоматически из WeaponStanceComponent)
	// ═══════════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	bool bIsHolstered = true;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	bool bIsAiming = false;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	bool bIsHoldingBreath = false;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	bool bIsFiring = false;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	bool bIsReloading = false;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	bool bModifyGrip = false;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	bool bCreateAimPose = false;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	bool bIsWeaponMontageActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	float AimingAlpha = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	float GripModifier = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	float WeaponLoweredAlpha = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	float RecoilAlpha = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	float StoredRecoil = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	float AdditivePitch = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	float BlockDistance = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	float SightDistance = 200.0f;

	// ═══════════════════════════════════════════════════════════════════════════════
	// MOVEMENT (задаётся из Blueprint или автоматически)
	// ═══════════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadWrite, Category = "Movement")
	float Speed = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Movement")
	float GroundSpeed = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Movement")
	float MoveForward = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Movement")
	float MoveRight = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Movement")
	float Movement = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Movement")
	bool bIsSprinting = false;

	UPROPERTY(BlueprintReadWrite, Category = "Movement")
	bool bIsCrouching = false;

	UPROPERTY(BlueprintReadWrite, Category = "Movement")
	bool bIsInAir = false;

	UPROPERTY(BlueprintReadWrite, Category = "Movement")
	bool bIsFalling = false;

	UPROPERTY(BlueprintReadWrite, Category = "Movement")
	bool bIsJumping = false;

	UPROPERTY(BlueprintReadWrite, Category = "Movement")
	bool bIsSliding = false;

	// ═══════════════════════════════════════════════════════════════════════════════
	// POSE STATES (задаётся из Blueprint)
	// ═══════════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadWrite, Category = "PoseStates")
	float Lean = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "PoseStates")
	float Roll = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "PoseStates")
	float Pitch = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "PoseStates")
	float YawOffset = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "PoseStates")
	float Yaw = 0.0f;

	// ═══════════════════════════════════════════════════════════════════════════════
	// TRANSFORMS (задаются из Blueprint после интерполяции)
	// ═══════════════════════════════════════════════════════════════════════════════

	/** RH Transform - результат TInterp To для VB_Hand_R */
	UPROPERTY(BlueprintReadWrite, Category = "Transforms", meta = (DisplayName = "RH Transform"))
	FTransform RHTransform = FTransform::Identity;

	/** LH Transform - результат TInterp To для VB_Hand_L */
	UPROPERTY(BlueprintReadWrite, Category = "Transforms", meta = (DisplayName = "LH Transform"))
	FTransform LHTransform = FTransform::Identity;

	// ═══════════════════════════════════════════════════════════════════════════════
	// DT VARIABLES (из DataTable - задаются из Blueprint)
	// ═══════════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	TObjectPtr<UBlendSpace> DTStance = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	TObjectPtr<UBlendSpace> DTLocomotion = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	TObjectPtr<UAnimSequenceBase> DTIdle = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	TObjectPtr<UAnimSequenceBase> DTAimPose = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	TObjectPtr<UAnimSequenceBase> DTAimIn = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	TObjectPtr<UAnimSequenceBase> DTAimIdle = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	TObjectPtr<UAnimSequenceBase> DTAimOut = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	TObjectPtr<UAnimSequenceBase> DTSlide = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	TObjectPtr<UAnimSequenceBase> DTBlocked = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	TObjectPtr<UAnimSequenceBase> DTGripBlocked = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	TObjectPtr<UAnimComposite> DTGripPoses = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable", meta = (DisplayName = "DT RH Transform"))
	FTransform DTRHTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable", meta = (DisplayName = "DT LH Transform"))
	FTransform DTLHTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable", meta = (DisplayName = "DT W Transform"))
	FTransform DTWTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable", meta = (DisplayName = "DT LH Grip Transform"))
	TArray<FTransform> DTLHGripTransform;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	float DTAimPoseAlpha = 0.0f;

	// ═══════════════════════════════════════════════════════════════════════════════
	// AIM OFFSET
	// ═══════════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadWrite, Category = "AimOffset")
	float AimYaw = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "AimOffset")
	float AimPitch = 0.0f;

	// ═══════════════════════════════════════════════════════════════════════════════
	// CAMERA
	// ═══════════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadWrite, Category = "Camera")
	FVector CameraShake = FVector::ZeroVector;

	// ═══════════════════════════════════════════════════════════════════════════════
	// HELPER FUNCTIONS
	// ═══════════════════════════════════════════════════════════════════════════════

	/**
	 * Получить имя строки legacy DataTable из WeaponArchetype тега.
	 * Weapon.Rifle.* -> SMG, Weapon.Pistol.* -> Pistol, и т.д.
	 */
	UFUNCTION(BlueprintPure, Category = "Animation|Helpers")
	FName GetLegacyRowNameFromArchetype() const;

	/**
	 * Статическая версия - для любого WeaponArchetype тега.
	 */
	UFUNCTION(BlueprintPure, Category = "Animation|Helpers")
	static FName GetLegacyRowNameFromArchetypeTag(const FGameplayTag& WeaponArchetype);

	/** Получить владельца как SuspenseCoreCharacter */
	UFUNCTION(BlueprintPure, Category = "Animation")
	ASuspenseCoreCharacter* GetSuspenseCoreCharacter() const;

protected:
	// ═══════════════════════════════════════════════════════════════════════════════
	// CACHED REFERENCES
	// ═══════════════════════════════════════════════════════════════════════════════

	UPROPERTY(Transient)
	TWeakObjectPtr<ASuspenseCoreCharacter> CachedCharacter;

	UPROPERTY(Transient)
	TWeakObjectPtr<USuspenseCoreWeaponStanceComponent> CachedStanceComponent;

	/** Обновить данные об оружии из WeaponStanceComponent */
	void UpdateWeaponData();
};
