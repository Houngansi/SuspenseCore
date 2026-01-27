// SuspenseCoreEnemyAnimInstance.h
// Simple AnimInstance for enemy characters that provides locomotion data.
// Use Speed, GroundSpeed, MovementDirection in your AnimBP BlendSpaces.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "SuspenseCoreEnemyAnimInstance.generated.h"

class UCharacterMovementComponent;

/**
 * USuspenseCoreEnemyAnimInstance
 *
 * Simple AnimInstance for enemy AI characters.
 * Provides locomotion data from CharacterMovementComponent.
 *
 * Use in AnimBP:
 * - Speed (for BlendSpace axis)
 * - GroundSpeed (horizontal only)
 * - MovementDirection (-180 to 180)
 * - bIsMoving (for state transitions)
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
    // LOCOMOTION DATA (use in BlendSpaces)
    // ═══════════════════════════════════════════════════════════════════════════════

    /** Total velocity magnitude (includes vertical) */
    UPROPERTY(BlueprintReadOnly, Category = "Animation|Locomotion")
    float Speed = 0.0f;

    /** Horizontal velocity magnitude (XY only, for ground movement) */
    UPROPERTY(BlueprintReadOnly, Category = "Animation|Locomotion")
    float GroundSpeed = 0.0f;

    /** Normalized speed (0-1 based on MaxWalkSpeed) */
    UPROPERTY(BlueprintReadOnly, Category = "Animation|Locomotion")
    float NormalizedSpeed = 0.0f;

    /** Movement direction relative to actor forward (-180 to 180) */
    UPROPERTY(BlueprintReadOnly, Category = "Animation|Locomotion")
    float MovementDirection = 0.0f;

    /** Forward movement component (-1 to 1) */
    UPROPERTY(BlueprintReadOnly, Category = "Animation|Locomotion")
    float MoveForward = 0.0f;

    /** Right movement component (-1 to 1) */
    UPROPERTY(BlueprintReadOnly, Category = "Animation|Locomotion")
    float MoveRight = 0.0f;

    /** Is character currently moving (speed > threshold) */
    UPROPERTY(BlueprintReadOnly, Category = "Animation|Locomotion")
    bool bIsMoving = false;

    /** Is character in air */
    UPROPERTY(BlueprintReadOnly, Category = "Animation|Locomotion")
    bool bIsInAir = false;

    /** Is character falling */
    UPROPERTY(BlueprintReadOnly, Category = "Animation|Locomotion")
    bool bIsFalling = false;

    /** Vertical velocity (for jump/fall blending) */
    UPROPERTY(BlueprintReadOnly, Category = "Animation|Locomotion")
    float VerticalVelocity = 0.0f;

    // ═══════════════════════════════════════════════════════════════════════════════
    // COMBAT STATE (optional, for weapon stances)
    // ═══════════════════════════════════════════════════════════════════════════════

    /** Does enemy have a weapon equipped */
    UPROPERTY(BlueprintReadOnly, Category = "Animation|Combat")
    bool bHasWeapon = false;

    /** Is enemy currently attacking */
    UPROPERTY(BlueprintReadOnly, Category = "Animation|Combat")
    bool bIsAttacking = false;

    /** Is enemy dead (for death animation) */
    UPROPERTY(BlueprintReadOnly, Category = "Animation|Combat")
    bool bIsDead = false;

    // ═══════════════════════════════════════════════════════════════════════════════
    // CONFIGURATION
    // ═══════════════════════════════════════════════════════════════════════════════

    /** Speed threshold for bIsMoving (default: 10) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Animation|Config")
    float MovingThreshold = 10.0f;

protected:
    UPROPERTY(Transient)
    TWeakObjectPtr<ACharacter> CachedCharacter;

    UPROPERTY(Transient)
    TWeakObjectPtr<UCharacterMovementComponent> CachedMovementComponent;

    void UpdateLocomotionData(float DeltaSeconds);
};
