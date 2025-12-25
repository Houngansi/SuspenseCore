// SuspenseCoreCharacterAnimInstance.h
// Copyright Suspense Team. All Rights Reserved.
//
// Минимальный AnimInstance - только переменные для Blueprint.
// Вся логика реализуется в Blueprint EventGraph.

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

/**
 * USuspenseCoreCharacterAnimInstance
 *
 * Контейнер переменных для Animation Blueprint.
 * Вся логика (интерполяция, M LH Offset и т.д.) реализуется в Blueprint EventGraph.
 */
UCLASS()
class PLAYERCORE_API USuspenseCoreCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// ═══════════════════════════════════════════════════════════════════════════════
	// MOVEMENT (задаётся из Blueprint)
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
	float MovementDirection = 0.0f;

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

	UPROPERTY(BlueprintReadWrite, Category = "Movement")
	bool bHasMovementInput = false;

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
	// WEAPON STATE (задаётся из Blueprint)
	// ═══════════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadWrite, Category = "Weapon")
	bool bIsHolstered = true;

	UPROPERTY(BlueprintReadWrite, Category = "Weapon")
	bool bIsAiming = false;

	UPROPERTY(BlueprintReadWrite, Category = "Weapon")
	bool bIsHoldingBreath = false;

	UPROPERTY(BlueprintReadWrite, Category = "Weapon")
	bool bModifyGrip = false;

	UPROPERTY(BlueprintReadWrite, Category = "Weapon")
	bool bIsMontageActive = false;

	UPROPERTY(BlueprintReadWrite, Category = "Weapon")
	int32 GripID = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Weapon")
	float AdditivePitch = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Weapon")
	float StoredRecoil = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Weapon")
	float BlockDistance = 0.0f;

	// ═══════════════════════════════════════════════════════════════════════════════
	// TRANSFORMS (результаты - задаются из Blueprint после интерполяции)
	// ═══════════════════════════════════════════════════════════════════════════════

	/** RH Transform - для VB_Hand_R */
	UPROPERTY(BlueprintReadWrite, Category = "Transforms", meta = (DisplayName = "RH Transform"))
	FTransform RHTransform = FTransform::Identity;

	/** LH Transform - для VB_Hand_L */
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

	/** DT RH Transform - из DataTable */
	UPROPERTY(BlueprintReadWrite, Category = "DataTable", meta = (DisplayName = "DT RH Transform"))
	FTransform DTRHTransform = FTransform::Identity;

	/** DT LH Transform - из DataTable */
	UPROPERTY(BlueprintReadWrite, Category = "DataTable", meta = (DisplayName = "DT LH Transform"))
	FTransform DTLHTransform = FTransform::Identity;

	/** DT W Transform - из DataTable для Weapon bone */
	UPROPERTY(BlueprintReadWrite, Category = "DataTable", meta = (DisplayName = "DT W Transform"))
	FTransform DTWTransform = FTransform::Identity;

	/** DT LH Grip Transform - массив из DataTable */
	UPROPERTY(BlueprintReadWrite, Category = "DataTable", meta = (DisplayName = "DT LH Grip Transform"))
	TArray<FTransform> DTLHGripTransform;

	UPROPERTY(BlueprintReadWrite, Category = "DataTable")
	float DTAimPoseAlpha = 0.0f;

	// ═══════════════════════════════════════════════════════════════════════════════
	// AIM OFFSET (задаётся из Blueprint)
	// ═══════════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadWrite, Category = "AimOffset")
	float AimYaw = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "AimOffset")
	float AimPitch = 0.0f;

	// ═══════════════════════════════════════════════════════════════════════════════
	// CAMERA (задаётся из Blueprint)
	// ═══════════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadWrite, Category = "Camera")
	FVector CameraShake = FVector::ZeroVector;
};
