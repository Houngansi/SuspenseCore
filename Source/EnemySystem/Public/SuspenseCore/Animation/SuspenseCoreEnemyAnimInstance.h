// SuspenseCoreEnemyAnimInstance.h
// AnimInstance для вражеских персонажей с идентичными переменными как в USuspenseCoreCharacterAnimInstance.
// Позволяет использовать тот же AnimBP что и для игрока.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "SuspenseCore/Characters/SuspenseCoreCharacter.h"
#include "SuspenseCoreEnemyAnimInstance.generated.h"

class UCharacterMovementComponent;

/**
 * USuspenseCoreEnemyAnimInstance
 *
 * AnimInstance для AI персонажей с теми же переменными что и USuspenseCoreCharacterAnimInstance.
 * Можно использовать тот же AnimBP без изменений.
 */
UCLASS()
class ENEMYSYSTEM_API USuspenseCoreEnemyAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    USuspenseCoreEnemyAnimInstance();

    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    // ═══════════════════════════════════════════════════════════════════════════════
    // MOVEMENT STATE (идентично USuspenseCoreCharacterAnimInstance)
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
    // VELOCITY & DIRECTION (идентично USuspenseCoreCharacterAnimInstance)
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
    // POSE STATES (упрощённые для AI)
    // ═══════════════════════════════════════════════════════════════════════════════

    UPROPERTY(BlueprintReadOnly, Category = "Animation|PoseStates")
    float Lean = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Animation|PoseStates")
    float BodyPitch = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Animation|PoseStates")
    float YawOffset = 0.0f;

    // ═══════════════════════════════════════════════════════════════════════════════
    // AIM OFFSET (для AI прицеливания)
    // ═══════════════════════════════════════════════════════════════════════════════

    UPROPERTY(BlueprintReadOnly, Category = "Animation|AimOffset")
    float AimYaw = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Animation|AimOffset")
    float AimPitch = 0.0f;

    // ═══════════════════════════════════════════════════════════════════════════════
    // WEAPON STATE (для AI с оружием)
    // ═══════════════════════════════════════════════════════════════════════════════

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

    // ═══════════════════════════════════════════════════════════════════════════════
    // GAS ATTRIBUTES (для совместимости)
    // ═══════════════════════════════════════════════════════════════════════════════

    UPROPERTY(BlueprintReadOnly, Category = "Animation|Attributes")
    float MaxWalkSpeed = 400.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Animation|Attributes")
    float MaxSprintSpeed = 600.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Animation|Attributes")
    float MaxCrouchSpeed = 200.0f;

protected:
    UPROPERTY(Transient)
    TWeakObjectPtr<ACharacter> CachedCharacter;

    UPROPERTY(Transient)
    TWeakObjectPtr<UCharacterMovementComponent> CachedMovementComponent;

    void UpdateMovementData(float DeltaSeconds);
    void UpdateVelocityData(float DeltaSeconds);
    void UpdateAimData(float DeltaSeconds);
};
